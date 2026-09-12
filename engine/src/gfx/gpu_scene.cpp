// engine/src/gfx/gpu_scene.cpp — see gpu_scene.hpp for why this file exists.

#include <engine/gfx/gpu_scene.hpp>

#include <engine/gfx/instancing.hpp>  // 6.16: instance_batch, gpu_instance

#include <engine/gfx/cubemap.hpp>   // Lesson 6.15: the 1x1 black fallback cube
#include <engine/gfx/gpu_post.hpp>  // k_hdr_format

#include <engine/core/log.hpp>
#include <engine/gfx/image.hpp>

#include <cstring>

namespace engine {

const char* name_of(blend_style b)
{
    switch (b)
    {
    case blend_style::opaque:        return "opaque";
    case blend_style::alpha:         return "alpha";
    case blend_style::premultiplied: return "premultiplied";
    }
    return "?";
}

const char* name_of(surface_style s)
{
    switch (s)
    {
    case surface_style::solid:     return "solid (cull back)";
    case surface_style::two_sided: return "two-sided (cull none)";
    case surface_style::wireframe: return "wireframe (fill LINE)";
    }
    return "?";
}

gpu_scene_renderer::~gpu_scene_renderer()
{
    destroy();
}

bool gpu_scene_renderer::create(const gpu_device& dev,
                                SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                                SDL_GPUTextureFormat depth_format,
                                SDL_GPUTextureFormat colour_format,
                                SDL_GPUSampleCount samples,
                                SDL_GPUShader* instanced_vertex)
{
    if (vertex == nullptr || fragment == nullptr) { return false; }

    depth_format_ = depth_format;
    samples_ = samples;

    // ---- The three pipelines ------------------------------------------------
    //
    // Written as a loop over the enum rather than three near-identical blocks,
    // because the whole point being made is how LITTLE separates them: two enum
    // values out of the fifty-three fields in a create-info. Spelling that out
    // three times would bury it.
    static constexpr surface_style k_order[k_styles] = {
        surface_style::solid, surface_style::two_sided, surface_style::wireframe
    };

    // LESSON 6.11. A SECOND AXIS, and therefore a NESTED loop and a PRODUCT.
    // Three became nine, and the shape of the change is the lesson: transparency
    // is not a fourth surface style, it is an independent choice that every
    // surface style has to be crossed with. See `blend_style` in the header for
    // why that multiplication is where real engines stop enumerating and start
    // hashing.
    static constexpr blend_style k_blend_order[k_blends] = {
        blend_style::opaque, blend_style::alpha, blend_style::premultiplied
    };

    for (int i = 0; i < k_styles; ++i)
    {
    for (int j = 0; j < k_blends; ++j)
    {
        pipeline_desc desc(dev, vertex, fragment);

            // LESSON 6.14. EVERY pipeline gets the sample count, because a
            // pipeline's multisample state is baked in at creation exactly as its
            // colour format is (4.4) — so MSAA is not a different argument to the
            // same nine pipelines, it is a different nine pipelines.
            desc.samples(samples_);

        // Slot 0, `gpu_vertex_pnu`, three attributes — the layout Lesson 4.5
        // built and 4.7 last touched. Not re-derived here: `describe` is the one
        // place that knows the pitch and the offsets, which is what stops the
        // C++ half and the HLSL half drifting apart.
        gpu_mesh::describe(desc, 0);

        // No instance buffer. Lesson 4.5's ring of seven tori was one mesh drawn
        // seven times; a scene is different meshes drawn once each, and there is
        // nothing for a per-instance rate to do. Instancing comes back the moment
        // the scene has repeated geometry — foliage, crates, particles — and the
        // machinery for it is still exactly where 4.5 left it.

        if (colour_format != SDL_GPU_TEXTUREFORMAT_INVALID)
        {
            desc.colour_target_format(colour_format);
        }

        SDL_GPUGraphicsPipelineCreateInfo& raw = desc.raw();

        // ---- The two fields that differ ------------------------------------
        //
        // `front_face` is NOT one of them. It is COUNTER_CLOCKWISE in all three,
        // set by `pipeline_desc`'s constructor, and it is conventions §7 — the
        // same winding Lesson 3.4's `is_front_facing` tests by the sign of a
        // signed area. Nothing in the port had to negotiate it.
        switch (k_order[i])
        {
        case surface_style::solid:
            raw.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            raw.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
            break;
        case surface_style::two_sided:
            raw.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            raw.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
            break;
        case surface_style::wireframe:
            raw.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_LINE;
            raw.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
            break;
        }

        // ---- Depth ----------------------------------------------------------
        //
        // LESS, because SDL_GPU's NDC puts 0 at the near plane (conventions §4):
        // smaller is nearer. Lesson 3.1's `depth_buffer::test_and_write` makes
        // the identical comparison in software, which is not a coincidence —
        // Lesson 3.1 chose it to match, three modules before it mattered.
        if (depth_format_ != SDL_GPU_TEXTUREFORMAT_INVALID)
        {
            raw.target_info.has_depth_stencil_target = true;
            raw.target_info.depth_stencil_format = depth_format_;
            raw.depth_stencil_state.enable_depth_test = true;
            raw.depth_stencil_state.enable_depth_write = true;
            raw.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        }

        // ---- The blend axis (Lesson 6.11) -----------------------------------
        //
        // TWO CALLS, and the second one is the one people forget. Blending
        // without disabling the depth write gives a transparent surface that
        // occludes every transparent surface behind it — so a scene of glass
        // panes shows exactly one of them, whichever happened to be drawn first,
        // and the others are simply absent. It reads as a culling bug.
        if (k_blend_order[j] != blend_style::opaque)
        {
            desc.blend(k_blend_order[j] == blend_style::premultiplied);
            desc.depth_write(false);
        }

        if (!pipelines_[i][j].create(dev, desc.info()))
        {
            ENGINE_LOG_ERROR(engine::log_gpu,
                             "gpu_scene: pipeline '%s' / '%s' was not created",
                             name_of(k_order[i]), name_of(k_blend_order[j]));
            destroy();
            return false;
        }
        create_ms_ += pipelines_[i][j].create_ms();
    }
    }

    // ---- The tenth pipeline: instanced, solid, opaque — Lesson 6.16 ---------
    //
    // Built from the SAME fragment shader as the nine above. That is not a
    // saving, it is the statement: instancing changes where the vertex stage
    // gets its transform and the fragment stage never asked, so every Module 6
    // feature comes along unported. The pipeline is separate anyway because a
    // pipeline IS the (vertex, fragment, state) triple — Lesson 4.1 — and one
    // third of it changed.
    if (instanced_vertex != nullptr)
    {
        pipeline_desc desc(dev, instanced_vertex, fragment);
        desc.samples(samples_);

        // BOTH LAYOUTS, AND THE ORDER MATTERS FOR THE LOCATIONS, NOT THE SLOTS.
        // `gpu_mesh::describe` takes locations 0-3 from buffer slot 0;
        // `describe_instances` takes 4-10 from slot 1. Eleven attributes in one
        // pipeline, which is what raised `pipeline_desc::k_max_attributes` from
        // eight to sixteen this lesson — see that constant's comment for how the
        // three dropped attributes announced themselves.
        gpu_mesh::describe(desc, 0);
        describe_instances(desc, 1);

        if (colour_format != SDL_GPU_TEXTUREFORMAT_INVALID)
        {
            desc.colour_target_format(colour_format);
        }

        SDL_GPUGraphicsPipelineCreateInfo& raw = desc.raw();
        raw.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        raw.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        if (depth_format_ != SDL_GPU_TEXTUREFORMAT_INVALID)
        {
            raw.target_info.has_depth_stencil_target = true;
            raw.target_info.depth_stencil_format = depth_format_;
            raw.depth_stencil_state.enable_depth_test = true;
            raw.depth_stencil_state.enable_depth_write = true;
            raw.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        }

        if (!instanced_.create(dev, desc.info()))
        {
            // NOT FATAL, and that is deliberate. A device that cannot build this
            // pipeline can still draw the whole scene through `render`; losing
            // instancing costs draw calls, not pixels. Returning false here
            // would turn an optimisation into a hard requirement.
            ENGINE_LOG_ERROR(engine::log_gpu,
                             "gpu_scene: the instanced pipeline was not created; "
                             "render_batched will draw nothing");
        }
        else
        {
            create_ms_ += instanced_.create_ms();
        }
    }

    // ---- One white texel ----------------------------------------------------
    //
    // The identity element of sampling, and the third time this trick has been
    // needed: Lesson 4.6 gave an older harness the camera matrix its `static
    // const` floats encoded, Lesson 4.7 gave one a 1x1 white texture, and here it
    // is again for objects that carry no image. A draw with nothing bound at a
    // sampler slot the shader declares produces no pixels and no message.
    //
    // sRGB, so that a caller who does set `textured` gets exactly 1.0 in linear
    // light rather than something that depends on which decode ran.
    image_data one_texel;
    one_texel.width = 1;
    one_texel.height = 1;
    one_texel.source_channels = 4;
    one_texel.pixels = {255, 255, 255, 255};

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(dev.handle());
    if (cb == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_scene: could not acquire a command buffer: %s", SDL_GetError());
        destroy();
        return false;
    }
    // ---- And one flat normal texel — Lesson 6.7 -----------------------------
    //
    // (128, 128, 255): the lavender. It decodes to (0.5, 0.5, 1.0), maps to the
    // direction (0, 0, 1), and `T*0 + B*0 + N*1` is `N` — so binding it changes
    // nothing, which is exactly what an identity element is for.
    //
    // **NOT sRGB**, and that one boolean is this lesson's entire subject on the
    // GPU side. A normal map is data, so it wants an `_UNORM` format and a
    // sampler that hands back `byte/255`. Pass `true` here and 128 decodes to
    // 0.2140 rather than 0.5020, the flat direction becomes (-0.57, -0.57, 1)
    // normalised, and every unmapped surface in the scene tilts.
    image_data flat_texel;
    flat_texel.width = 1;
    flat_texel.height = 1;
    flat_texel.source_channels = 4;
    flat_texel.pixels = {128, 128, 255, 255};

    // ---- And one texel of "nothing occludes" — Lesson 6.8 ------------------
    //
    // THE THIRD IDENTITY ELEMENT, and the first one that cannot be uploaded.
    // `SDL_UploadToGPUTexture` cannot target a depth texture at all, so the only
    // way to put a value in one is to have a render pass clear it — a pass that
    // begins, clears to 1.0, and ends without drawing anything. That is a legal
    // and completely ordinary thing to do, and it is the shape every "clear a
    // render target" helper in every engine eventually takes.
    //
    // 1.0 is the far plane, so this map reports that the nearest surface along
    // every one of the light's rays is as far away as the box goes: nothing is
    // in front of anything, everything is lit. Bind it and the shadow term is
    // exactly 1.
    static constexpr SDL_GPUTextureFormat k_shadow_candidates[] = {
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    };
    const SDL_GPUTextureFormat shadow_fmt =
        supported_shadow_format(dev, k_shadow_candidates, 3);

    // ---- Lesson 6.15: the environment's identity elements -------------------
    //
    // A 1x1 BLACK cube map and a 1x1 black table, built through the same
    // `create_cube` path a real environment uses so that the fallback exercises
    // the code rather than bypassing it. Black is the identity for addition,
    // which is what the ambient term does with them — see the header.
    //
    // `k_hdr_format` and not an 8-bit format, because the shader's
    // `TextureCube<float4>` binding does not care but a format mismatch between
    // the fallback and the real thing is the sort of difference that makes a
    // bug appear only when the feature is switched off.
    cube_map black_env(1, 1);
    const bool cube_ok =
        black_cube_.create_cube(dev, cb, black_env, k_hdr_format, "black cube 1x1 (no env)")
        && cube_sampler_.create(dev, filter::linear, address_mode::clamp_to_edge,
                                "environment (trilinear, clamped)", filter::linear);

    image_data black_texel;
    black_texel.width = 1;
    black_texel.height = 1;
    black_texel.source_channels = 4;
    black_texel.pixels = {0, 0, 0, 255};

    if (!white_.create_sampled(dev, cb, one_texel, true, "white 1x1")
        || !flat_normal_.create_sampled(dev, cb, flat_texel, false, "flat normal 1x1")
        || !black_lut_.create_sampled(dev, cb, black_texel, false, "black brdf lut 1x1")
        || !cube_ok)
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_scene: the fallback textures were not created");
        SDL_SubmitGPUCommandBuffer(cb);
        destroy();
        return false;
    }

    // A device that supports no sampled depth format at all gets no shadows,
    // which is a capability report rather than an error — the same shape
    // `ensure_depth` already has for a device with no depth format.
    if (shadow_fmt != SDL_GPU_TEXTUREFORMAT_INVALID
        && far_depth_.create_depth(dev, shadow_fmt, 1, 1, "far 1x1 (no shadow)", true)
        && shadow_sampler_.create_comparison(dev, "shadow comparison (fallback)"))
    {
        SDL_GPUDepthStencilTargetInfo dsi{};
        dsi.texture = far_depth_.handle();
        dsi.clear_depth = 1.0f;
        dsi.load_op = SDL_GPU_LOADOP_CLEAR;
        dsi.store_op = SDL_GPU_STOREOP_STORE;
        dsi.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        dsi.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

        SDL_GPURenderPass* clear_pass = SDL_BeginGPURenderPass(cb, nullptr, 0, &dsi);
        if (clear_pass != nullptr) { SDL_EndGPURenderPass(clear_pass); }
    }
    else
    {
        ENGINE_LOG_INFO(engine::log_gpu,
                        "gpu_scene: no sampled depth format — this device gets no shadows");
        far_depth_.destroy();
        shadow_sampler_.destroy();
    }

    if (!SDL_SubmitGPUCommandBuffer(cb))
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_scene: the fallback upload was not submitted");
        destroy();
        return false;
    }

    return true;
}

void gpu_scene_renderer::destroy()
{
    depth_.destroy();
    white_.destroy();
    flat_normal_.destroy();
    far_depth_.destroy();
    shadow_sampler_.destroy();

    // 6.16 ADDED `instanced_` HERE AND FOUND THREE THAT WERE MISSING.
    //
    // `black_cube_`, `black_lut_` and `cube_sampler_` arrived in Lesson 6.15 and
    // were never added to this function. It is NOT a leak — every one of them is
    // an RAII member whose destructor runs when the renderer dies, and each
    // `create_*` calls its own `destroy()` first, so re-creating is clean too.
    // What it is, is a function that does not do what its name says: after
    // `destroy()` the object reported `valid() == false` while still holding
    // three live GPU objects, so "destroyed" and "empty" had quietly stopped
    // meaning the same thing.
    //
    // The transferable part is how it was found: not by a leak report, but by
    // ADDING A MEMBER AND READING THE LIST. A teardown function is a list that
    // must be kept in step with a declaration list by hand, and nothing checks
    // it — which is an argument for having as few members as possible, and, in
    // Module 9, for the arena that makes teardown one operation instead of eight.
    black_cube_.destroy();
    black_lut_.destroy();
    cube_sampler_.destroy();

    instanced_.destroy();
    for (auto& row : pipelines_) { for (gpu_pipeline& p : row) { p.destroy(); } }
    depth_w_ = 0;
    depth_h_ = 0;
    create_ms_ = 0.0;
}

bool gpu_scene_renderer::ensure_depth(const gpu_device& dev, Uint32 w, Uint32 h,
                                      SDL_GPUSampleCount samples)
{
    if (depth_format_ == SDL_GPU_TEXTUREFORMAT_INVALID) { return false; }
    if (w == 0 || h == 0) { return false; }
    // THE SAMPLE COUNT IS PART OF THE IDENTITY (6.14). Without it in this test, a
    // program that toggles MSAA keeps the depth buffer it already had and the
    // next pass fails to begin — with an error about the attachment, several
    // frames after the setting changed.
    if (depth_.valid() && depth_w_ == w && depth_h_ == h && depth_.samples() == samples)
    {
        return true;
    }

    depth_.destroy();
    if (!depth_.create_depth(dev, depth_format_, w, h, "scene depth", false, samples))
    {
        return false;
    }
    depth_w_ = w;
    depth_h_ = h;
    ENGINE_LOG_INFO(engine::log_gpu, "gpu_scene: depth target %ux%u %s", w, h, name_of(depth_format_));
    return true;
}

SDL_GPUDepthStencilTargetInfo gpu_scene_renderer::depth_target_info() const
{
    SDL_GPUDepthStencilTargetInfo info{};
    info.texture = depth_.handle();

    // 1 is the far plane. Conventions §4, and the reason the compare op above is
    // LESS rather than GREATER.
    info.clear_depth = 1.0f;
    info.load_op = SDL_GPU_LOADOP_CLEAR;

    // DONT_CARE, not STORE: nothing reads this buffer after the pass ends. On
    // tiled hardware — every phone, and Apple silicon — that difference is real
    // bandwidth, because a DONT_CARE depth buffer never leaves tile memory at
    // all. Module 6's shadow maps and depth-based post-processing are where this
    // has to become STORE, and where the cost shows up.
    info.store_op = SDL_GPU_STOREOP_DONT_CARE;
    info.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    return info;
}

gpu_scene_renderer::frame_setup
gpu_scene_renderer::begin_frame(SDL_GPUCommandBuffer* cb,
                                const camera_uniforms& camera,
                                const scene_light_uniforms& light,
                                const cascade_uniforms* cascades,
                                SDL_GPUTexture* shadow,
                                SDL_GPUSampler* shadow_sampler,
                                const scene_environment* environment,
                                frame_log* log,
                                draw_stats& stats) const
{

    // ---- Per FRAME, pushed once ---------------------------------------------
    //
    // Both blocks go onto the COMMAND BUFFER. They are not bound to this pass and
    // they survive every pipeline change below, which is exactly why the loop
    // does not have to re-push them and why "bind" is the wrong verb for a push.
    SDL_PushGPUVertexUniformData(cb, 0, &camera, sizeof(camera));
    SDL_PushGPUFragmentUniformData(cb, 0, &light, sizeof(light));
    stats.uniform_bytes += static_cast<Uint32>(sizeof(camera) + sizeof(light));

    // ---- The cascades — Lesson 6.9 ------------------------------------------
    //
    // ALWAYS PUSHED, EVEN WHEN THERE ARE NO SHADOWS. A cbuffer the shader
    // declares and nobody fills does not read as zero — it reads as whatever was
    // last in that slot, which is a garbage matrix and a `cascade_count` that
    // may be anything. The fallback below is a deliberate identity: one cascade,
    // a split past any reachable depth, and `shadow_strength` in the light block
    // is what actually disables the lookup.
    cascade_uniforms fallback{};
    if (cascades == nullptr)
    {
        fallback.cascade_count = 1.0f;
        fallback.splits = vec4{1e30f, 1e30f, 1e30f, 1e30f};
        fallback.world_per_texel = vec4{1.0f, 1.0f, 1.0f, 1.0f};
        fallback.depth_range = vec4{1.0f, 1.0f, 1.0f, 1.0f};
        fallback.view_forward = vec4{0.0f, 0.0f, -1.0f, 0.0f};
        for (mat4& m : fallback.light_clip_from_world) { m = light.light_clip_from_world; }
    }
    const cascade_uniforms& casc = (cascades != nullptr) ? *cascades : fallback;
    SDL_PushGPUFragmentUniformData(cb, 2, &casc, sizeof(casc));
    stats.uniform_bytes += static_cast<Uint32>(sizeof(casc));

    if (log != nullptr)
    {
        log->record(gpu_event_kind::push_uniform, "camera (vertex)", 0,
                    static_cast<Uint32>(sizeof(camera)));
        log->record(gpu_event_kind::push_uniform, "lighting (fragment)", 0,
                    static_cast<Uint32>(sizeof(light)));
    }

    // ---- The shadow map, resolved once for the whole frame — Lesson 6.8 -----
    //
    // It does not change between draws, so it is not part of the per-item
    // change detection below; it rides along in the same three-binding call
    // because `SDL_BindGPUFragmentSamplers` takes an array and re-binding two
    // slots costs the same as re-binding three.
    SDL_GPUTexture* const shadow_tex = (shadow != nullptr) ? shadow : far_depth_.handle();
    SDL_GPUSampler* const shadow_samp =
        (shadow_sampler != nullptr) ? shadow_sampler : shadow_sampler_.handle();

    // ---- The environment, resolved once for the whole frame — Lesson 6.15 ---
    //
    // Same shape as the shadow map above and for the same reason: three
    // resources that do not change between draws, folded into the one
    // `SDL_BindGPUFragmentSamplers` call the albedo already forces. The array
    // goes from three entries to six and the call count does not move.
    //
    // AN INCOMPLETE ENVIRONMENT IS TREATED AS NO ENVIRONMENT, not as a partial
    // one. Binding two of the three would leave the shader reading an undefined
    // slot, and `complete()` says so in one place rather than three.
    const bool have_env = (environment != nullptr) && environment->complete();
    SDL_GPUTexture* const irr_tex = have_env ? environment->irradiance : black_cube_.handle();
    SDL_GPUTexture* const pre_tex = have_env ? environment->prefiltered : black_cube_.handle();
    SDL_GPUTexture* const lut_tex = have_env ? environment->brdf_lut : black_lut_.handle();
    SDL_GPUSampler* const cube_samp = have_env ? environment->cube_sampler : cube_sampler_.handle();
    SDL_GPUSampler* const lut_samp = have_env ? environment->lut_sampler : cube_sampler_.handle();

    frame_setup out;
    out.shadow = shadow_tex;
    out.shadow_sampler = shadow_samp;
    out.irradiance = irr_tex;
    out.prefiltered = pre_tex;
    out.brdf_lut = lut_tex;
    out.cube_sampler = cube_samp;
    out.lut_sampler = lut_samp;
    return out;
}

draw_stats gpu_scene_renderer::render(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                                      const gpu_draw_item* items, int count,
                                      const camera_uniforms& camera,
                                      const scene_light_uniforms& light,
                                      SDL_GPUSampler* sampler, frame_log* log,
                                      SDL_GPUTexture* shadow,
                                      SDL_GPUSampler* shadow_sampler,
                                      const cascade_uniforms* cascades,
                                      const scene_environment* environment) const
{
    draw_stats stats;
    if (cb == nullptr || pass == nullptr || items == nullptr || count <= 0) { return stats; }

    const frame_setup frame = begin_frame(cb, camera, light, cascades, shadow,
                                          shadow_sampler, environment, log, stats);

    // Track what is currently bound so a redundant bind can be skipped AND
    // counted. The skipping is a small real saving; the counting is the lesson.
    const gpu_pipeline* bound_pipeline = nullptr;
    SDL_GPUTexture* bound_texture = nullptr;
    SDL_GPUTexture* bound_normal = nullptr;   // 6.7

    // 6.11: a matrix now, because the ideal bind count is one per distinct
    // (surface style, blend style) PAIR present — a solid opaque draw and a solid
    // blended draw are two pipelines however well the list is sorted.
    bool style_present[k_styles][k_blends] = {};

    for (int i = 0; i < count; ++i)
    {
        const gpu_draw_item& item = items[i];
        ++stats.items;

        // A scene whose model failed to load should draw the rest of itself.
        if (item.mesh == nullptr || !item.mesh->valid()) { continue; }

        style_present[static_cast<int>(item.style)][static_cast<int>(item.blend)] = true;

        const gpu_pipeline& want =
            pipelines_[static_cast<int>(item.style)][static_cast<int>(item.blend)];
        if (!want.valid()) { continue; }

        if (&want != bound_pipeline)
        {
            SDL_BindGPUGraphicsPipeline(pass, want.handle());
            bound_pipeline = &want;
            ++stats.pipeline_binds;
            if (log != nullptr)
            {
                log->record(gpu_event_kind::bind_pipeline, name_of(item.style));
            }
        }

        SDL_GPUTexture* tex = (item.texture != nullptr) ? item.texture : white_.handle();
        SDL_GPUTexture* nrm = (item.normal_map != nullptr) ? item.normal_map
                                                           : flat_normal_.handle();
        if (tex != bound_texture || nrm != bound_normal)
        {
            // BOTH SLOTS IN ONE CALL — Lesson 6.7. `SDL_BindGPUFragmentSamplers`
            // takes an array and a count, so two bindings starting at slot 0 is
            // one call rather than two, and the pair travels together for the
            // same reason `SDL_GPUTextureSamplerBinding` is a pair at all.
            //
            // The change-detection is now on the PAIR, which is the honest
            // version: rebinding because the albedo changed while the normal map
            // did not still costs a bind, and counting it as one is what keeps
            // `texture_binds` comparable with the number 4.8 measured.
            // THREE SLOTS NOW, and the third is a different KIND of binding: a
            // depth texture read through a COMPARISON sampler (6.8). It never
            // changes within a frame, so binding it here rather than once at the
            // top is redundant work — but a partial `SDL_BindGPUFragmentSamplers`
            // does not merge with an earlier one, it REPLACES the range it names,
            // so slot 2 would be unbound the moment slot 0 changed. One call for
            // the whole set is the only spelling that is correct.
            // SIX SLOTS NOW, and the comment above about a partial bind
            // REPLACING the range it names is the reason all six travel
            // together: slots 3 to 5 do not change within a frame, but rebinding
            // slot 0 alone would leave them unbound. The rule that made three
            // correct makes six correct and costs the same call.
            SDL_GPUTextureSamplerBinding binds[6]{};
            binds[0].texture = tex;
            binds[0].sampler = sampler;
            binds[1].texture = nrm;
            binds[1].sampler = sampler;
            binds[2].texture = frame.shadow;
            binds[2].sampler = frame.shadow_sampler;
            binds[3].texture = frame.irradiance;
            binds[3].sampler = frame.cube_sampler;
            binds[4].texture = frame.prefiltered;
            binds[4].sampler = frame.cube_sampler;
            binds[5].texture = frame.brdf_lut;
            binds[5].sampler = frame.lut_sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, binds, 6);
            bound_texture = tex;
            bound_normal = nrm;
            ++stats.texture_binds;
            if (log != nullptr)
            {
                log->record(gpu_event_kind::bind_sampler,
                            (item.texture != nullptr) ? "albedo" : "white 1x1", 0);
            }
        }

        // ---- Per DRAW ------------------------------------------------------
        //
        // 112 bytes to the vertex stage and 32 to the fragment stage, per object,
        // per frame. Four objects is 576 bytes of uniform traffic a frame — which
        // is nothing, and is nothing precisely because a `mat4` per object is
        // small. Module 7's skinned characters push a bone palette instead, at
        // which point this mechanism runs out and a STORAGE buffer takes over.
        object_uniforms obj{};
        obj.world_from_model = item.world_from_model;
        obj.normal_from_model[0] = vec4{item.normal_from_model.c0.x,
                                        item.normal_from_model.c0.y,
                                        item.normal_from_model.c0.z, 0.0f};
        obj.normal_from_model[1] = vec4{item.normal_from_model.c1.x,
                                        item.normal_from_model.c1.y,
                                        item.normal_from_model.c1.z, 0.0f};
        obj.normal_from_model[2] = vec4{item.normal_from_model.c2.x,
                                        item.normal_from_model.c2.y,
                                        item.normal_from_model.c2.z, 0.0f};

        SDL_PushGPUVertexUniformData(cb, 1, &obj, sizeof(obj));
        SDL_PushGPUFragmentUniformData(cb, 1, &item.material, sizeof(item.material));
        stats.uniform_bytes += static_cast<Uint32>(sizeof(obj) + sizeof(item.material));

        item.mesh->bind(pass, 0);

        const Uint32 tris = (item.mesh->index_count() > 0u)
            ? item.mesh->index_count() / 3u
            : item.mesh->vertex_count() / 3u;

        if (log != nullptr)
        {
            log->record(gpu_event_kind::push_uniform, "object matrices", 1,
                        static_cast<Uint32>(sizeof(obj)));
            log->record(gpu_event_kind::push_uniform, "material", 1,
                        static_cast<Uint32>(sizeof(item.material)));
            log->record(gpu_event_kind::bind_vertex, "mesh vertices", 0);
            if (item.mesh->index_count() > 0u)
            {
                log->record(gpu_event_kind::bind_index, "mesh indices", 0);
            }
        }

        item.mesh->draw(pass, 1);
        ++stats.draws;

        if (log != nullptr)
        {
            log->record(gpu_event_kind::draw, "scene object",
                        item.mesh->index_count(), 1u, tris);
        }

        // An expanded mesh (Lesson 4.5's control) keeps no index buffer, so the
        // triangle count came from whichever of the two the draw actually used.
        stats.triangles += tris;
    }

    for (const auto& row : style_present)
    {
        for (bool present : row)
        {
            if (present) { ++stats.ideal_pipeline_binds; }
        }
    }

    return stats;
}

// ---------------------------------------------------------------------------
// Lesson 6.16 — the same frame, submitted as batches
// ---------------------------------------------------------------------------

draw_stats gpu_scene_renderer::render_batched(SDL_GPUCommandBuffer* cb,
                                              SDL_GPURenderPass* pass,
                                              const instance_batch* batches, int count,
                                              SDL_GPUBuffer* instances,
                                              const camera_uniforms& camera,
                                              const scene_light_uniforms& light,
                                              SDL_GPUSampler* sampler, frame_log* log,
                                              SDL_GPUTexture* shadow,
                                              SDL_GPUSampler* shadow_sampler,
                                              const cascade_uniforms* cascades,
                                              const scene_environment* environment) const
{
    draw_stats stats;
    if (cb == nullptr || pass == nullptr || batches == nullptr || count <= 0) { return stats; }
    if (instances == nullptr || !instanced_.valid()) { return stats; }

    // THE IDENTICAL PROLOGUE, BY CONSTRUCTION AND NOT BY DISCIPLINE. One call,
    // one implementation; `render` above makes the same one. See `frame_setup`.
    const frame_setup frame = begin_frame(cb, camera, light, cascades, shadow,
                                          shadow_sampler, environment, log, stats);

    // ONE PIPELINE FOR THE WHOLE LOOP. `render`'s change-detection over nine
    // pipelines has nothing to detect here: `create` builds exactly one instanced
    // pipeline (solid, opaque) and `batch_instances` only ever produces opaque
    // multi-instance batches, so the bind happens once, outside the loop.
    //
    // That is a real and slightly surprising saving. A perfectly sorted
    // non-instanced frame still pays one pipeline bind per distinct style present
    // — `ideal_pipeline_binds` has measured exactly that since Lesson 4.8 — and
    // here the ideal and the actual are both 1 by construction.
    SDL_BindGPUGraphicsPipeline(pass, instanced_.handle());
    stats.pipeline_binds = 1;
    stats.ideal_pipeline_binds = 1;
    if (log != nullptr) { log->record(gpu_event_kind::bind_pipeline, "instanced solid"); }

    SDL_GPUTexture* bound_texture = nullptr;
    SDL_GPUTexture* bound_normal = nullptr;

    for (int b = 0; b < count; ++b)
    {
        const instance_batch& batch = batches[b];
        stats.items += batch.count;

        if (batch.key.mesh == nullptr || !batch.key.mesh->valid()) { continue; }

        // A batch this pipeline cannot express is SKIPPED, not drawn wrongly.
        // `batch_instances` never produces a blended batch with more than one
        // instance, but it does hand back single-instance blended and two-sided
        // batches so the caller has one list; those belong in `render`.
        if (batch.key.style != surface_style::solid
            || batch.key.blend != blend_style::opaque)
        {
            continue;
        }

        SDL_GPUTexture* tex = (batch.key.texture != nullptr) ? batch.key.texture
                                                             : white_.handle();
        SDL_GPUTexture* nrm = (batch.key.normal_map != nullptr) ? batch.key.normal_map
                                                                : flat_normal_.handle();
        if (tex != bound_texture || nrm != bound_normal)
        {
            // The same six-slot bind `render` performs, for the same reason: a
            // partial `SDL_BindGPUFragmentSamplers` REPLACES the range it names
            // rather than merging, so slots 2-5 must ride along with 0 and 1.
            SDL_GPUTextureSamplerBinding binds[6]{};
            binds[0].texture = tex;
            binds[0].sampler = sampler;
            binds[1].texture = nrm;
            binds[1].sampler = sampler;
            binds[2].texture = frame.shadow;
            binds[2].sampler = frame.shadow_sampler;
            binds[3].texture = frame.irradiance;
            binds[3].sampler = frame.cube_sampler;
            binds[4].texture = frame.prefiltered;
            binds[4].sampler = frame.cube_sampler;
            binds[5].texture = frame.brdf_lut;
            binds[5].sampler = frame.lut_sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, binds, 6);
            bound_texture = tex;
            bound_normal = nrm;
            ++stats.texture_binds;
        }

        // ---- The material, still a push ------------------------------------
        //
        // PER BATCH, not per instance, and that is the batch key earning its
        // keep: every instance in this run was required to have byte-identical
        // material bytes, so one push serves all of them. A hundred objects that
        // differ only in placement push 32 bytes once here against 3,200 bytes
        // through `render`.
        SDL_PushGPUFragmentUniformData(cb, 1, &batch.key.material,
                                       sizeof(batch.key.material));
        stats.uniform_bytes += static_cast<Uint32>(sizeof(batch.key.material));

        // ---- The mesh at slot 0, the instances at slot 1 --------------------
        batch.key.mesh->bind(pass, 0);

        // ONE BUFFER, MANY BATCHES, ADDRESSED BY OFFSET. `first` is an index into
        // the instance array, so the byte offset is `first * sizeof(gpu_instance)`
        // and the hardware's instance index counts from zero inside the binding.
        // Binding a separate buffer per batch would work and would cost an
        // allocation per batch per frame; this costs a different `offset` field.
        SDL_GPUBufferBinding instance_binding{};
        instance_binding.buffer = instances;
        instance_binding.offset =
            static_cast<Uint32>(batch.first) * static_cast<Uint32>(sizeof(gpu_instance));
        SDL_BindGPUVertexBuffers(pass, 1, &instance_binding, 1);

        const Uint32 tris = (batch.key.mesh->index_count() > 0u)
            ? batch.key.mesh->index_count() / 3u
            : batch.key.mesh->vertex_count() / 3u;

        if (log != nullptr)
        {
            log->record(gpu_event_kind::push_uniform, "material (per batch)", 1,
                        static_cast<Uint32>(sizeof(batch.key.material)));
            log->record(gpu_event_kind::bind_vertex, "instance placements", 1);
        }

        batch.key.mesh->draw(pass, static_cast<Uint32>(batch.count));
        ++stats.draws;
        stats.triangles += tris * static_cast<Uint32>(batch.count);

        if (log != nullptr)
        {
            log->record(gpu_event_kind::draw, "instanced batch",
                        batch.key.mesh->index_count(),
                        static_cast<Uint32>(batch.count), tris);
        }
    }

    return stats;
}

} // namespace engine
