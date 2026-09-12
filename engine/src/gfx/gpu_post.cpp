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

    // A SECOND SAMPLER, AND IT IS LINEAR — Lesson 6.13. The argument that made
    // the first one NEAREST was that the resolve is 1:1, and that argument does
    // not transfer: the bloom is HALF resolution, so this fetch is a genuine
    // magnification and a nearest tap would make every glow blocky at exactly the
    // scale the pyramid worked to smooth. Two inputs of different sizes need two
    // samplers, which is a small, concrete instance of why a post-processing
    // stage's resources belong to the stage.
    if (!bloom_sampler_.create(dev, filter::linear, address_mode::clamp_to_edge,
                               "bloom composite", filter::linear, 1))
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_post: the bloom sampler was not created");
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
    bloom_sampler_.destroy();
    sampler_.destroy();
    pipeline_.destroy();
}

void gpu_tonemap_pass::render(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                              SDL_GPUTexture* hdr, const tonemap_settings& settings,
                              bool shader_encodes,
                              SDL_GPUTexture* bloom, float bloom_intensity) const
{
    if (cb == nullptr || pass == nullptr || hdr == nullptr || !pipeline_.valid()) { return; }

    SDL_BindGPUGraphicsPipeline(pass, pipeline_.handle());

    // TWO BINDINGS IN ONE CALL, and the array order IS the register order: index
    // 0 lands on `t0/s0`, index 1 on `t1/s1`. SDL takes a contiguous array with a
    // first slot and a count, so there is no way to bind t1 without binding t0 —
    // which is the API making the "must not be null" rule in the header into
    // something you cannot get wrong by accident, only by passing a null.
    SDL_GPUTextureSamplerBinding bindings[2]{};
    bindings[0].texture = hdr;
    bindings[0].sampler = sampler_.handle();
    bindings[1].texture = (bloom != nullptr) ? bloom : hdr;
    bindings[1].sampler = bloom_sampler_.handle();
    SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);

    // The fallback above binds the HDR target to slot 1 when no bloom is given,
    // which is safe only because `bloom_intensity` is then zero — the shader
    // fetches and multiplies by nothing. It exists so that a caller who has not
    // built a bloom cannot produce a validation error; `gpu_post_stack` passes a
    // 1x1 black texture instead, which is the honest version.
    const tonemap_uniforms u = uniforms_of(settings, shader_encodes,
                                           (bloom != nullptr) ? bloom_intensity : 0.0f);
    SDL_PushGPUFragmentUniformData(cb, 0, &u, sizeof(u));

    // THREE VERTICES, ONE INSTANCE, AND NOTHING BOUND. No vertex buffer, no index
    // buffer, no `SDL_BindGPUVertexBuffers` at all — the corners come out of
    // `SV_VertexID`. It is worth pausing on how little a full-screen pass costs
    // to ISSUE compared to how much it costs to RUN: one draw call, and one
    // fragment for every pixel on the screen, every frame.
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
}

// ===========================================================================
//  Lesson 6.13 — the bloom
// ===========================================================================

namespace {

/// Begin a colour-only render pass against one texture. Every stage of the bloom
/// wants exactly this and differs only in the load op, so it is written once.
///
/// `clear_colour` is never used — `LOAD` preserves and `DONT_CARE` discards — but
/// SDL requires the field and leaving it uninitialised in a struct it reads is
/// the kind of thing that works until a driver decides to read it.
SDL_GPURenderPass* begin_target_pass(SDL_GPUCommandBuffer* cb, SDL_GPUTexture* target,
                                     SDL_GPULoadOp load)
{
    SDL_GPUColorTargetInfo ci{};
    ci.texture = target;
    ci.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 0.0f};
    ci.load_op = load;
    ci.store_op = SDL_GPU_STOREOP_STORE;
    ci.mip_level = 0;
    ci.layer_or_depth_plane = 0;
    ci.cycle = false;
    return SDL_BeginGPURenderPass(cb, &ci, 1, nullptr);
}

/// Bind one texture, push one uniform block, draw the full-screen triangle.
template <typename Uniforms>
void full_screen_draw(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                      const gpu_pipeline& pipe, SDL_GPUTexture* source,
                      SDL_GPUSampler* sampler, const Uniforms& u)
{
    SDL_BindGPUGraphicsPipeline(pass, pipe.handle());

    SDL_GPUTextureSamplerBinding binding{};
    binding.texture = source;
    binding.sampler = sampler;
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);

    SDL_PushGPUFragmentUniformData(cb, 0, &u, sizeof(u));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
}

/// The three rasterizer settings every post pipeline shares.
void configure_post_pipeline(pipeline_desc& desc, SDL_GPUTextureFormat format)
{
    SDL_GPUGraphicsPipelineCreateInfo& raw = desc.raw();
    raw.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    raw.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    desc.colour_target_format(format);
}

} // namespace

gpu_bloom::~gpu_bloom()
{
    destroy();
}

bool gpu_bloom::create(const gpu_device& dev,
                       SDL_GPUShader* fullscreen_vertex,
                       SDL_GPUShader* bright_fragment,
                       SDL_GPUShader* down_fragment,
                       SDL_GPUShader* up_fragment)
{
    destroy();
    if (fullscreen_vertex == nullptr || bright_fragment == nullptr
        || down_fragment == nullptr || up_fragment == nullptr)
    {
        return false;
    }

    // THREE PIPELINES, AND THE ONLY ONE THAT DIFFERS IN STATE IS THE UPSAMPLE.
    // Bright and down are byte-identical in everything but their fragment shader
    // — same format, same rasterizer, no blend, no depth — which is worth
    // noticing because it is exactly the situation that makes a pipeline CACHE
    // keyed on a hash of the state pay for itself. We still enumerate, because
    // the count is countable; see the lesson's §6 for where that stops being true.
    {
        pipeline_desc desc(dev, fullscreen_vertex, bright_fragment);
        configure_post_pipeline(desc, k_hdr_format);
        if (!bright_.create(dev, desc.info()))
        {
            ENGINE_LOG_ERROR(engine::log_gpu, "gpu_bloom: the bright-pass pipeline was not created");
            destroy();
            return false;
        }
    }
    {
        pipeline_desc desc(dev, fullscreen_vertex, down_fragment);
        configure_post_pipeline(desc, k_hdr_format);
        if (!down_.create(dev, desc.info()))
        {
            ENGINE_LOG_ERROR(engine::log_gpu, "gpu_bloom: the downsample pipeline was not created");
            destroy();
            return false;
        }
    }
    {
        pipeline_desc desc(dev, fullscreen_vertex, up_fragment);
        configure_post_pipeline(desc, k_hdr_format);

        // THE ONE LINE THAT MAKES THE PYRAMID A SUM. `src*1 + dst*1` — Lesson
        // 6.11's blending machinery in a pass with no geometry in it, and the
        // reason the upsample never has to read the target it writes.
        desc.blend_add();

        if (!up_.create(dev, desc.info()))
        {
            ENGINE_LOG_ERROR(engine::log_gpu, "gpu_bloom: the upsample pipeline was not created");
            destroy();
            return false;
        }
    }

    // LINEAR, and here the filter mode is carrying arithmetic rather than taste:
    // the downsample's single tap IS the 2x2 average only because the hardware
    // interpolates. Set this to NEAREST and every stage silently becomes a point
    // decimation — the bloom still looks blurry (five more levels of it are
    // coming), so nothing obviously breaks, and the result aliases.
    if (!sampler_.create(dev, filter::linear, address_mode::clamp_to_edge,
                         "bloom pyramid", filter::linear, 1))
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_bloom: the pyramid sampler was not created");
        destroy();
        return false;
    }

    ENGINE_LOG_INFO(engine::log_gpu,
                    "gpu_bloom: 3 pipelines (bright %.2f ms, down %.2f ms, up %.2f ms, additive)",
                    bright_.create_ms(), down_.create_ms(), up_.create_ms());
    return true;
}

void gpu_bloom::destroy()
{
    for (gpu_texture& t : levels_) { t.destroy(); }
    level_count_ = 0;
    sampler_.destroy();
    up_.destroy();
    down_.destroy();
    bright_.destroy();
}

bool gpu_bloom::resize(const gpu_device& dev, Uint32 full_width, Uint32 full_height, int levels)
{
    if (levels < 1) { levels = 1; }
    if (levels > k_max_bloom_levels) { levels = k_max_bloom_levels; }
    if (full_width < 2u) { full_width = 2u; }
    if (full_height < 2u) { full_height = 2u; }

    // IDEMPOTENT, and it matters more here than on the CPU: a texture allocation
    // is a driver call that may stall the pipeline, not a `malloc`. Level 0's
    // dimensions plus the count identify the pyramid completely, because every
    // other level is derived from them.
    const Uint32 want_w = full_width / 2u;
    const Uint32 want_h = full_height / 2u;
    if (level_count_ == levels && levels_[0].valid()
        && levels_[0].width() == want_w && levels_[0].height() == want_h)
    {
        return true;
    }

    for (gpu_texture& t : levels_) { t.destroy(); }
    level_count_ = 0;

    Uint32 w = want_w;
    Uint32 h = want_h;
    for (int i = 0; i < levels; ++i)
    {
        char name[32];
        SDL_snprintf(name, sizeof(name), "bloom level %d", i);

        // BOTH USAGES, EVERY LEVEL. Each level is written by one pass and read by
        // the next, and the level above it is written by a pass that reads it —
        // so unlike the scene target, where the pairing is obvious, here EVERY
        // texture is genuinely both. Omitting SAMPLER produces a texture that
        // renders perfectly and reads as undefined.
        if (!levels_[i].create_colour_target(dev, k_hdr_format, w, h, name, true))
        {
            ENGINE_LOG_ERROR(engine::log_gpu, "gpu_bloom: level %d (%ux%u) was not created", i, w, h);
            destroy();
            return false;
        }
        ++level_count_;

        if (w <= 1u && h <= 1u) { break; }
        w = (w > 1u) ? w / 2u : 1u;
        h = (h > 1u) ? h / 2u : 1u;
    }

    ENGINE_LOG_INFO(engine::log_gpu,
                    "gpu_bloom: %d levels from %ux%u, %zu texels (%.2f MB at 8 B/texel), %d render passes",
                    level_count_, want_w, want_h, texels(),
                    static_cast<double>(texels() * 8u) / (1024.0 * 1024.0), pass_count());
    return true;
}

SDL_GPUTexture* gpu_bloom::result() const
{
    return (level_count_ > 0) ? levels_[0].handle() : nullptr;
}

std::size_t gpu_bloom::texels() const
{
    std::size_t n = 0;
    for (int i = 0; i < level_count_; ++i)
    {
        n += static_cast<std::size_t>(levels_[i].width()) * static_cast<std::size_t>(levels_[i].height());
    }
    return n;
}

void gpu_bloom::render(SDL_GPUCommandBuffer* cb, SDL_GPUTexture* scene,
                       const bloom_settings& s, float exposure) const
{
    if (cb == nullptr || scene == nullptr || level_count_ <= 0 || !valid()) { return; }

    // ---- Down: the bright pass, then four halvings --------------------------
    //
    // The bright pass reads the SCENE at full resolution and writes level 0 at
    // half, so it replaces what would otherwise be a separate first downsample.
    // Its `texel_size` is therefore the scene's, and every downsample's is the
    // level it reads — a distinction the uniform packing makes explicit precisely
    // because getting it wrong is invisible until the glow is the wrong size.
    {
        SDL_GPURenderPass* pass = begin_target_pass(cb, levels_[0].handle(),
                                                    SDL_GPU_LOADOP_DONT_CARE);
        if (pass == nullptr) { return; }

        // DONT_CARE, not CLEAR: the bright pass writes every texel of its target,
        // so clearing first is a full-target write thrown away. On a tiler that
        // is real bandwidth; on a desktop GPU it is merely free to avoid.
        const bloom_bright_uniforms u = uniforms_of(
            s, exposure,
            static_cast<int>(levels_[0].width() * 2u),
            static_cast<int>(levels_[0].height() * 2u));
        full_screen_draw(cb, pass, bright_, scene, sampler_.handle(), u);
        SDL_EndGPURenderPass(pass);
    }

    for (int i = 1; i < level_count_; ++i)
    {
        SDL_GPURenderPass* pass = begin_target_pass(cb, levels_[i].handle(),
                                                    SDL_GPU_LOADOP_DONT_CARE);
        if (pass == nullptr) { return; }

        const bloom_filter_uniforms u = filter_uniforms_of(
            static_cast<int>(levels_[i - 1].width()),
            static_cast<int>(levels_[i - 1].height()), s.radius);
        full_screen_draw(cb, pass, down_, levels_[i - 1].handle(), sampler_.handle(), u);
        SDL_EndGPURenderPass(pass);
    }

    // ---- Up: from the smallest level back to level 0, adding --------------
    //
    // LOAD, NOT DONT_CARE, and this is the one load op in the chain that is
    // load-bearing. The upsample blends `src + dst`, so the destination's existing
    // contents ARE an operand; discard them and the additive blend has nothing to
    // add to, which turns the pyramid from a sum of every level into just the
    // widest one. The symptom is a bloom that is far too soft and far too dim,
    // and it looks like a tuning problem rather than a load op.
    for (int i = level_count_ - 1; i > 0; --i)
    {
        SDL_GPURenderPass* pass = begin_target_pass(cb, levels_[i - 1].handle(),
                                                    SDL_GPU_LOADOP_LOAD);
        if (pass == nullptr) { return; }

        const bloom_filter_uniforms u = filter_uniforms_of(
            static_cast<int>(levels_[i].width()),
            static_cast<int>(levels_[i].height()), s.radius);
        full_screen_draw(cb, pass, up_, levels_[i].handle(), sampler_.handle(), u);
        SDL_EndGPURenderPass(pass);
    }
}

// ===========================================================================
//  The stack
// ===========================================================================

gpu_post_stack::~gpu_post_stack()
{
    destroy();
}

bool gpu_post_stack::create(const gpu_device& dev,
                            SDL_GPUShader* fullscreen_vertex,
                            SDL_GPUShader* tonemap_fragment,
                            SDL_GPUShader* bright_fragment,
                            SDL_GPUShader* down_fragment,
                            SDL_GPUShader* up_fragment,
                            SDL_GPUTextureFormat target_format)
{
    destroy();

    if (!tonemap_.create(dev, fullscreen_vertex, tonemap_fragment, target_format)) { return false; }
    if (!bloom_.create(dev, fullscreen_vertex, bright_fragment, down_fragment, up_fragment))
    {
        destroy();
        return false;
    }

    // A 1x1 BLACK TEXTURE, AND IT IS NOT A HACK — it is what "no bloom" has to be
    // spelled as. SDL_GPU offers no way to unbind a sampler, and a pipeline whose
    // fragment shader declares `t1` must have something bound there on every draw
    // that uses it. The alternatives are a second tonemap pipeline compiled
    // without the bloom fetch (doubling this pass's pipeline count to save one
    // texel of bandwidth) or a branch in the shader (a divergent fetch on every
    // fragment to save a multiply on some). One texel wins.
    if (!no_bloom_.create_colour_target(dev, k_hdr_format, 1u, 1u, "no bloom (1x1 black)", true))
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_post_stack: the 1x1 bloom stand-in was not created");
        destroy();
        return false;
    }

    return true;
}

void gpu_post_stack::destroy()
{
    no_bloom_.destroy();
    scene_.destroy();
    bloom_.destroy();
    tonemap_.destroy();
}

bool gpu_post_stack::resize(const gpu_device& dev, Uint32 width, Uint32 height,
                            const bloom_settings& s)
{
    if (width < 1u) { width = 1u; }
    if (height < 1u) { height = 1u; }

    if (!scene_.valid() || scene_.width() != width || scene_.height() != height)
    {
        scene_.destroy();
        if (!scene_.create_colour_target(dev, k_hdr_format, width, height, "hdr scene", true))
        {
            ENGINE_LOG_ERROR(engine::log_gpu, "gpu_post_stack: the %ux%u scene target failed", width, height);
            return false;
        }
    }

    // THE PYRAMID IS ALLOCATED EVEN WHEN THE BLOOM IS OFF, and that is a decision
    // rather than an oversight: toggling bloom mid-frame would otherwise stall on
    // six texture creations, which is exactly when a user is A/B-ing the effect
    // and least wants a hitch. 1.32 MB at 960x540 is the price. A shipping engine
    // with a fixed quality setting would free them; one with a debug menu would
    // not.
    return bloom_.resize(dev, width, height, s.levels);
}

void gpu_post_stack::render_bloom(SDL_GPUCommandBuffer* cb, const bloom_settings& s,
                                  float exposure) const
{
    if (!s.enabled) { return; }
    bloom_.render(cb, scene_.handle(), s, exposure);
}

void gpu_post_stack::resolve_into(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                                  const tonemap_settings& tone, bool shader_encodes,
                                  const bloom_settings& s) const
{
    // THE ONE PLACE THE EXPOSURE HAS TO AGREE. `render_bloom` was handed
    // `tone.exposure` by the caller and `tonemap_` applies it here; they are the
    // same field of the same struct precisely so that they cannot drift. If the
    // bloom is off, the stand-in texture is bound and the intensity is zero.
    const bool on = s.enabled && bloom_.result() != nullptr;
    tonemap_.render(cb, pass, scene_.handle(), tone, shader_encodes,
                    on ? bloom_.result() : no_bloom_.handle(),
                    on ? s.intensity : 0.0f);
}

} // namespace engine
