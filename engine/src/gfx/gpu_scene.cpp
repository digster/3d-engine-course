// engine/src/gfx/gpu_scene.cpp — see gpu_scene.hpp for why this file exists.

#include <engine/gfx/gpu_scene.hpp>

#include <engine/core/log.hpp>
#include <engine/gfx/image.hpp>

#include <cstring>

namespace engine {

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
                                SDL_GPUTextureFormat colour_format)
{
    if (vertex == nullptr || fragment == nullptr) { return false; }

    depth_format_ = depth_format;

    // ---- The three pipelines ------------------------------------------------
    //
    // Written as a loop over the enum rather than three near-identical blocks,
    // because the whole point being made is how LITTLE separates them: two enum
    // values out of the fifty-three fields in a create-info. Spelling that out
    // three times would bury it.
    static constexpr surface_style k_order[k_styles] = {
        surface_style::solid, surface_style::two_sided, surface_style::wireframe
    };

    for (int i = 0; i < k_styles; ++i)
    {
        pipeline_desc desc(dev, vertex, fragment);

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

        if (!pipelines_[i].create(dev, desc.info()))
        {
            ENGINE_LOG_ERROR(engine::log_gpu, "gpu_scene: pipeline '%s' was not created", name_of(k_order[i]));
            destroy();
            return false;
        }
        create_ms_ += pipelines_[i].create_ms();
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

    if (!white_.create_sampled(dev, cb, one_texel, true, "white 1x1")
        || !flat_normal_.create_sampled(dev, cb, flat_texel, false, "flat normal 1x1"))
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
    for (gpu_pipeline& p : pipelines_) { p.destroy(); }
    depth_w_ = 0;
    depth_h_ = 0;
    create_ms_ = 0.0;
}

bool gpu_scene_renderer::ensure_depth(const gpu_device& dev, Uint32 w, Uint32 h)
{
    if (depth_format_ == SDL_GPU_TEXTUREFORMAT_INVALID) { return false; }
    if (w == 0 || h == 0) { return false; }
    if (depth_.valid() && depth_w_ == w && depth_h_ == h) { return true; }

    if (!depth_.create_depth(dev, depth_format_, w, h, "scene depth")) { return false; }
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

draw_stats gpu_scene_renderer::render(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                                      const gpu_draw_item* items, int count,
                                      const camera_uniforms& camera,
                                      const scene_light_uniforms& light,
                                      SDL_GPUSampler* sampler, frame_log* log,
                                      SDL_GPUTexture* shadow,
                                      SDL_GPUSampler* shadow_sampler,
                                      const cascade_uniforms* cascades) const
{
    draw_stats stats;
    if (cb == nullptr || pass == nullptr || items == nullptr || count <= 0) { return stats; }

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

    // Track what is currently bound so a redundant bind can be skipped AND
    // counted. The skipping is a small real saving; the counting is the lesson.
    const gpu_pipeline* bound_pipeline = nullptr;
    SDL_GPUTexture* bound_texture = nullptr;

    SDL_GPUTexture* bound_normal = nullptr;   // 6.7
    bool style_present[k_styles] = {};

    // ---- The shadow map, resolved once for the whole frame — Lesson 6.8 -----
    //
    // It does not change between draws, so it is not part of the per-item
    // change detection below; it rides along in the same three-binding call
    // because `SDL_BindGPUFragmentSamplers` takes an array and re-binding two
    // slots costs the same as re-binding three.
    SDL_GPUTexture* const shadow_tex = (shadow != nullptr) ? shadow : far_depth_.handle();
    SDL_GPUSampler* const shadow_samp =
        (shadow_sampler != nullptr) ? shadow_sampler : shadow_sampler_.handle();

    for (int i = 0; i < count; ++i)
    {
        const gpu_draw_item& item = items[i];
        ++stats.items;

        // A scene whose model failed to load should draw the rest of itself.
        if (item.mesh == nullptr || !item.mesh->valid()) { continue; }

        style_present[static_cast<int>(item.style)] = true;

        const gpu_pipeline& want = pipelines_[static_cast<int>(item.style)];
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
            SDL_GPUTextureSamplerBinding binds[3]{};
            binds[0].texture = tex;
            binds[0].sampler = sampler;
            binds[1].texture = nrm;
            binds[1].sampler = sampler;
            binds[2].texture = shadow_tex;
            binds[2].sampler = shadow_samp;
            SDL_BindGPUFragmentSamplers(pass, 0, binds, 3);
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

    for (bool present : style_present)
    {
        if (present) { ++stats.ideal_pipeline_binds; }
    }

    return stats;
}

} // namespace engine
