// engine/src/gfx/gpu_post.cpp — one pipeline, one sampler, one draw call.
//
// Lesson 6.12. The argument is in gpu_post.hpp; this is the forty lines it
// produces, and their brevity is itself the point — a post pass is small because
// almost everything a scene pass needs is absent.

#include <engine/gfx/gpu_post.hpp>

#include <engine/core/log.hpp>

namespace engine {

gpu_tonemap_pass::~gpu_tonemap_pass()
{
    destroy();
}

bool gpu_tonemap_pass::create(const gpu_device& dev,
                              SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                              SDL_GPUTextureFormat target_format)
{
    destroy();
    if (vertex == nullptr || fragment == nullptr) { return false; }

    pipeline_desc desc(dev, vertex, fragment);

    // ---- Everything a scene pipeline has, that this one does not ------------
    //
    // NO VERTEX INPUT. `gpu_mesh::describe` is not called and no buffer or
    // attribute is declared, so `num_vertex_buffers` and `num_vertex_attributes`
    // stay zero. The vertex shader computes its three corners from
    // `SV_VertexID`, which needs nothing bound. This is legal, and it is the
    // whole reason `fullscreen.vert.hlsl` exists.
    //
    // NO DEPTH. `has_depth_stencil_target` stays false and no depth test or write
    // is enabled — a triangle covering the screen has nothing to be occluded by.
    // Note that this also means the pass can be begun with a null depth target,
    // which is what the caller does.
    //
    // NO BLENDING. 6.11's `blend()` is not called. The resolve REPLACES the
    // swapchain's contents; there is nothing underneath to composite with, and
    // blending a tonemapped value would be `f(a) over f(b)`, which is not
    // `f(a over b)` because a curve is not linear.
    //
    // NO CULLING, because a full-screen triangle's winding is an accident of how
    // `SV_VertexID` was turned into positions rather than a fact about a surface,
    // and there is no "back" of the screen.
    SDL_GPUGraphicsPipelineCreateInfo& raw = desc.raw();
    raw.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    raw.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

    desc.colour_target_format(target_format);

    if (!pipeline_.create(dev, desc.info()))
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_post: the tonemap pipeline was not created");
        destroy();
        return false;
    }

    // A NEAREST, CLAMPED sampler, and both halves are deliberate. The resolve is
    // 1:1 — one source texel per destination pixel — so a linear filter has
    // nothing to interpolate and would only risk a half-texel error if the two
    // targets ever disagreed in size. `clamp_to_edge` because a full-screen
    // triangle's uv reaches exactly 0 and 1 and `repeat` would wrap the edge
    // texel to the far side on the last row of pixels.
    if (!sampler_.create(dev, filter::nearest, address_mode::clamp_to_edge,
                         "tonemap resolve", filter::nearest, 1))
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_post: the resolve sampler was not created");
        destroy();
        return false;
    }

    ENGINE_LOG_INFO(engine::log_gpu,
                    "gpu_post: tonemap pipeline -> %s, %.2f ms (no vertex input, no depth, no blend)",
                    name_of(target_format), pipeline_.create_ms());
    return true;
}

void gpu_tonemap_pass::destroy()
{
    sampler_.destroy();
    pipeline_.destroy();
}

void gpu_tonemap_pass::render(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                              SDL_GPUTexture* hdr, const tonemap_settings& settings,
                              bool shader_encodes) const
{
    if (cb == nullptr || pass == nullptr || hdr == nullptr || !pipeline_.valid()) { return; }

    SDL_BindGPUGraphicsPipeline(pass, pipeline_.handle());

    SDL_GPUTextureSamplerBinding binding{};
    binding.texture = hdr;
    binding.sampler = sampler_.handle();
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);

    const tonemap_uniforms u = uniforms_of(settings, shader_encodes);
    SDL_PushGPUFragmentUniformData(cb, 0, &u, sizeof(u));

    // THREE VERTICES, ONE INSTANCE, AND NOTHING BOUND. No vertex buffer, no index
    // buffer, no `SDL_BindGPUVertexBuffers` at all — the corners come out of
    // `SV_VertexID`. It is worth pausing on how little a full-screen pass costs
    // to ISSUE compared to how much it costs to RUN: one draw call, and one
    // fragment for every pixel on the screen, every frame.
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
}

} // namespace engine
