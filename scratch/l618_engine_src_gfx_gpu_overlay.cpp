// engine/src/gfx/gpu_overlay.cpp — one pipeline, one texture, one draw call.
//
// Lesson 6.18. Everything the overlay costs the GPU is in this file, and it is
// worth noticing how little there is: a pipeline with blending on and depth off,
// a one-channel texture, two stream buffers, and an indexed draw.

#include <engine/gfx/gpu_overlay.hpp>

#include <engine/core/log.hpp>

namespace engine {

gpu_overlay::~gpu_overlay()
{
    destroy();
}

bool gpu_overlay::create(const gpu_device& dev, SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                         SDL_GPUTextureFormat target_format, int max_quads)
{
    destroy();

    if (!dev.valid() || vertex == nullptr || fragment == nullptr || max_quads <= 0)
    {
        return false;
    }
    if (max_quads > k_max_overlay_quads) { max_quads = k_max_overlay_quads; }
    max_quads_ = max_quads;

    pipeline_desc desc(dev, vertex, fragment);
    desc.colour_target_format(target_format);

    // ---- The vertex layout, which IS `overlay_vertex`'s memory layout -------
    //
    // Three attributes at three offsets, and the offsets are the struct's. There
    // is no reflection on the C++ side to catch a disagreement — `check_layout`
    // compares this against the SHADER, which is the other half — so the
    // `static_assert` on `sizeof(overlay_vertex)` in the header is the guard on
    // this half.
    desc.vertex_buffer(0, static_cast<Uint32>(sizeof(overlay_vertex)));
    desc.attribute(0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0);    // position
    desc.attribute(1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 8);    // uv
    // UBYTE4_NORM: four bytes in MEMORY ORDER, each divided by 255 on the way
    // into the shader. `overlay_vertex` names its four colour bytes r, g, b, a
    // for exactly this reason — a packed ARGB word would arrive as (B, G, R, A)
    // on a little-endian machine, which is Lesson 6.15's BRDF-table bug.
    desc.attribute(2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, 16);

    // ---- The state that makes it an overlay --------------------------------
    //
    // `blend(false)` — straight alpha, `src*a + dst*(1-a)`. Straight rather than
    // premultiplied because the atlas holds COVERAGE, not premultiplied colour:
    // there is nothing in the texture to have been multiplied. The shader
    // produces `(tint, coverage * vertex_alpha)` and the ROP does the rest, in
    // linear light when the target is `_SRGB`.
    (void)desc.blend(false);

    // NO DEPTH STATE AT ALL, which is worth stating rather than leaving as an
    // absence. The overlay is drawn last, into a target with no depth attachment
    // bound, in declaration order — and "declaration order" is the sort. That is
    // the correct answer for a HUD and it is why `overlay_batch` has no sort key:
    // 6.11's `order_draws` sorts transparent GEOMETRY back to front because
    // depth decides what is in front; here the programmer decides.
    //
    // Culling is left at the course default (CCW front, back cull, conventions
    // §7) rather than disabled, so a wrongly-wound quad is invisible rather than
    // silently fine — which makes `push_quad`'s winding a thing the GPU checks.
    if (!pipeline_.create(dev, desc.info()))
    {
        ENGINE_LOG_ERROR(log_gpu, "gpu_overlay: the pipeline was not created");
        destroy();
        return false;
    }

    // NEAREST, CLAMPED. The blit is 1:1 — one atlas texel per screen pixel, both
    // on the integer grid after the layout's snap — so a linear tap at a pixel
    // centre returns exactly one texel anyway. Saying NEAREST makes that a fact
    // of the code rather than an accident of the arithmetic, and it makes the
    // software compositor's integer fetch and this sampler agree exactly, which
    // is what §9's channel-for-channel comparison needs.
    //
    // It is also the right default to CHANGE: text drawn at a scale, or from a
    // mipped atlas, wants linear, and §10 says what else has to move with it.
    if (!sampler_.create(dev, filter::nearest, address_mode::clamp_to_edge,
                         "glyph atlas", filter::nearest, 1))
    {
        ENGINE_LOG_ERROR(log_gpu, "gpu_overlay: the sampler was not created");
        destroy();
        return false;
    }

    const Uint32 vertex_bytes =
        static_cast<Uint32>(max_quads_) * 4u * static_cast<Uint32>(sizeof(overlay_vertex));
    const Uint32 index_bytes =
        static_cast<Uint32>(max_quads_) * 6u * static_cast<Uint32>(sizeof(Uint16));

    // STREAM buffers, not `gpu_buffer`. The contents change every frame, and
    // `gpu_stream_buffer::write` cycles both the device buffer and its transfer
    // buffer — so a write can never land on a read the GPU has not reached.
    // Lesson 4.5's argument, and the overlay is the case it was written for: a
    // few kilobytes, rewritten sixty times a second.
    if (!vertices_.create(dev, SDL_GPU_BUFFERUSAGE_VERTEX, vertex_bytes, "overlay vertices")
        || !indices_.create(dev, SDL_GPU_BUFFERUSAGE_INDEX, index_bytes, "overlay indices"))
    {
        ENGINE_LOG_ERROR(log_gpu, "gpu_overlay: the stream buffers were not created");
        destroy();
        return false;
    }

    ENGINE_LOG_INFO(log_gpu,
                    "gpu_overlay: %d quads max (%u B vertices + %u B indices), "
                    "pipeline -> %s, %.2f ms",
                    max_quads_, vertex_bytes, index_bytes,
                    name_of(target_format), pipeline_.create_ms());
    return true;
}

void gpu_overlay::destroy()
{
    indices_.destroy();
    vertices_.destroy();
    atlas_.destroy();
    sampler_.destroy();
    pipeline_.destroy();
    max_quads_ = 0;
    last_vertex_bytes_ = 0;
    last_index_bytes_ = 0;
    last_index_count_ = 0;
}

bool gpu_overlay::set_font(const gpu_device& dev, SDL_GPUCommandBuffer* cb,
                           const font_atlas& atlas)
{
    if (!atlas.valid())
    {
        ENGINE_LOG_ERROR(log_gpu, "gpu_overlay::set_font: the atlas is not valid");
        return false;
    }

    // NO MIPS. A 1:1 blit's footprint is exactly one texel, so level 0 is the
    // level a mip selector would choose at every pixel; a chain would be memory
    // the sampler never touches. It is the right call HERE and the wrong one the
    // moment text scales — see §10, and note that turning it on also means
    // raising `font_bake_options::padding` to `1 << levels`, because level N's
    // texel spans 2^N of level 0's and the bleed argument scales with it.
    if (!atlas_.create_coverage(dev, cb, atlas.coverage.data(), atlas.width, atlas.height,
                                "glyph atlas", /*mips*/ false))
    {
        ENGINE_LOG_ERROR(log_gpu, "gpu_overlay::set_font: the atlas texture was not created");
        return false;
    }

    ENGINE_LOG_INFO(log_gpu, "gpu_overlay: atlas %dx%d uploaded, %zu B (R8, one channel)",
                    atlas.width, atlas.height, atlas.byte_count());
    return true;
}

bool gpu_overlay::upload(SDL_GPUCommandBuffer* cb, const overlay_batch& batch)
{
    last_vertex_bytes_ = 0;
    last_index_bytes_ = 0;
    last_index_count_ = 0;

    if (cb == nullptr || batch.empty() || !vertices_.valid()) { return false; }

    if (batch.quad_count() > max_quads_)
    {
        // Refused rather than truncated. A partial upload draws a HUD with its
        // last line missing, which looks like a layout bug; this says what
        // happened and how to fix it.
        ENGINE_LOG_ERROR(log_gpu, "gpu_overlay::upload: %d quads exceeds the %d this was sized for",
                         batch.quad_count(), max_quads_);
        return false;
    }

    if (!vertices_.write(cb, batch.vertices().data(), batch.vertex_bytes())
        || !indices_.write(cb, batch.indices().data(), batch.index_bytes()))
    {
        return false;
    }

    last_vertex_bytes_ = batch.vertex_bytes();
    last_index_bytes_ = batch.index_bytes();
    last_index_count_ = static_cast<Uint32>(batch.indices().size());
    return true;
}

void gpu_overlay::record(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                         const overlay_batch& batch, bool shader_encodes,
                         float stem_darken) const
{
    if (cb == nullptr || pass == nullptr || !valid() || last_index_count_ == 0) { return; }
    if (!atlas_.valid()) { return; }

    SDL_BindGPUGraphicsPipeline(pass, pipeline_.handle());

    SDL_GPUBufferBinding vb{};
    vb.buffer = vertices_.handle();
    vb.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

    SDL_GPUBufferBinding ib{};
    ib.buffer = indices_.handle();
    ib.offset = 0;
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    SDL_GPUTextureSamplerBinding binding{};
    binding.texture = atlas_.handle();
    binding.sampler = sampler_.handle();
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);

    // THE VIEWPORT COMES FROM THE BATCH, NOT FROM THE DEVICE. The batch was laid
    // out against a particular size, and drawing it against a different one
    // stretches text that was carefully snapped to a pixel grid. Reading it from
    // the thing that was laid out is what makes "resize means re-lay-out"
    // enforceable rather than a convention.
    overlay_viewport_uniforms vu{};
    vu.inv_half_width = 2.0f / static_cast<float>(batch.viewport_width());
    vu.inv_half_height = 2.0f / static_cast<float>(batch.viewport_height());
    SDL_PushGPUVertexUniformData(cb, 0, &vu, sizeof(vu));

    overlay_shading_uniforms su{};
    su.encode = shader_encodes ? 1.0f : 0.0f;
    su.stem_darken = stem_darken;
    SDL_PushGPUFragmentUniformData(cb, 0, &su, sizeof(su));

    // ONE DRAW CALL FOR THE WHOLE OVERLAY — the panel, the labels and the
    // numbers, in the order they were asked for. That is the payoff for the
    // solid texel: a batch that mixes textured and untextured quads needs no
    // state change between them, so "how many draw calls is your UI" has the
    // answer "one" rather than "one per widget".
    SDL_DrawGPUIndexedPrimitives(pass, last_index_count_, 1, 0, 0, 0);
}

} // namespace engine
