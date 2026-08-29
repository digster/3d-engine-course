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
    if (!white_.create_sampled(dev, cb, one_texel, true, "white 1x1")
        || !SDL_SubmitGPUCommandBuffer(cb))
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_scene: the fallback white texture was not created");
        destroy();
        return false;
    }

    return true;
}

void gpu_scene_renderer::destroy()
{
    depth_.destroy();
    white_.destroy();
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
                                      SDL_GPUSampler* sampler, frame_log* log) const
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

    bool style_present[k_styles] = {};

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
        if (tex != bound_texture)
        {
            SDL_GPUTextureSamplerBinding bind{};
            bind.texture = tex;
            bind.sampler = sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &bind, 1);
            bound_texture = tex;
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
