// engine/src/gfx/gpu_shadow.cpp — see gpu_shadow.hpp for why this file exists.

#include <engine/gfx/gpu_shadow.hpp>

#include <engine/core/log.hpp>
#include <engine/gfx/gpu_mesh.hpp>

namespace engine {

gpu_shadow_map::~gpu_shadow_map()
{
    destroy();
}

bool gpu_shadow_map::create(const gpu_device& dev,
                            SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                            int resolution, SDL_GPUTextureFormat format,
                            int layers)
{
    destroy();

    if (vertex == nullptr || fragment == nullptr || resolution <= 0
        || format == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        return false;
    }

    // ---- The attachment, which is also a texture ---------------------------
    //
    // `sampled = true` is the whole difference between this and the scene's own
    // depth buffer. gpu_texture.hpp names the two costs; this is where they are
    // paid, deliberately, because a shadow map that cannot be read is a shadow
    // map that does nothing.
    // ALWAYS AN ARRAY, EVEN AT ONE LAYER (Lesson 6.9). The shader binds a
    // `Texture2DArray`, so a one-cascade map and a four-cascade map differ only
    // in `layer_count_or_depth` — and 6.8's single map becomes the degenerate
    // case of this one instead of a second code path to keep in step.
    layers_ = (layers < 1) ? 1 : (layers > k_max_cascades ? k_max_cascades : layers);
    if (!depth_.create_depth_array(dev, format, static_cast<Uint32>(resolution),
                                   static_cast<Uint32>(resolution),
                                   static_cast<Uint32>(layers_), "shadow map", true))
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_shadow: the %dx%d depth texture was not created",
                         resolution, resolution);
        return false;
    }

    if (!compare_.create_comparison(dev, "shadow comparison"))
    {
        destroy();
        return false;
    }

    // ---- The depth-only pipeline -------------------------------------------
    pipeline_desc desc(dev, vertex, fragment);

    // ONE ATTRIBUTE OUT OF FOUR, and the buffer is unchanged. `gpu_vertex_pnu` is
    // 48 bytes with position, normal, uv and tangent interleaved; this pipeline
    // declares the position and the hardware never fetches the rest. A vertex
    // layout is per-PIPELINE state, which Lesson 6.7 established when `describe`
    // grew its `with_tangent` parameter, and this is that fact at its limit.
    desc.vertex_buffer(0, static_cast<Uint32>(sizeof(gpu_vertex_pnu)));
    desc.attribute(0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0);

    SDL_GPUGraphicsPipelineCreateInfo& raw = desc.raw();

    // ---- NO COLOUR. This is the line the whole file is about. --------------
    //
    // `pipeline_desc`'s constructor fills in one colour target taken from the
    // device's swapchain, which is right for anything drawing into a window and
    // wrong here. Zeroing the count is what makes this a depth-only pipeline —
    // and the create-info's `color_target_descriptions` pointer is then never
    // read, which is why the array it points at may stay as it is.
    raw.target_info.num_color_targets = 0;

    raw.target_info.has_depth_stencil_target = true;
    raw.target_info.depth_stencil_format = format;
    raw.depth_stencil_state.enable_depth_test = true;
    raw.depth_stencil_state.enable_depth_write = true;

    // LESS, the same comparison the scene pass makes, for the same reason: 0 is
    // the near plane (conventions §4). The map records the NEAREST surface along
    // each of the light's rays, and "nearest" is a `<` under this convention.
    raw.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;

    // ---- Which faces to keep, and why the default is NONE -------------------
    //
    // Culling back faces here is correct for closed casters and deletes a ground
    // plane, exactly as it does in the camera pass (Lesson 3.4). Culling FRONT
    // faces is the third acne cure (§4.7) and is a real technique. Neither is
    // safe for arbitrary geometry, so the pipeline keeps everything and the
    // choice belongs to whoever knows what is in the scene.
    raw.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    raw.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

    if (!pipeline_.create(dev, desc.info()))
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_shadow: the depth-only pipeline was not created");
        destroy();
        return false;
    }

    resolution_ = resolution;
    ENGINE_LOG_INFO(engine::log_gpu, "gpu_shadow: %dx%d %s, sampled, comparison sampler",
                    resolution, resolution, name_of(format));
    return true;
}

void gpu_shadow_map::destroy()
{
    pipeline_.destroy();
    compare_.destroy();
    depth_.destroy();
    resolution_ = 0;
}

void gpu_shadow_map::render(SDL_GPUCommandBuffer* cb, const gpu_draw_item* items, int count,
                            const light_camera& cam, frame_log* log,
                            int layer) const
{
    if (!valid() || cb == nullptr || items == nullptr || count <= 0) { return; }

    // ---- A pass with a depth attachment and nothing else --------------------
    SDL_GPUDepthStencilTargetInfo dsi{};
    dsi.texture = depth_.handle();

    // WHICH CASCADE THIS PASS WRITES. A `Uint8`, sitting beside `mip_level` —
    // both are "which slice of this texture is the target", and both default to
    // zero, which is why the one-cascade case needed no change here at all.
    dsi.layer = static_cast<Uint8>((layer < 0) ? 0 : (layer >= layers_ ? layers_ - 1 : layer));

    // 1 is the far plane, so a cleared map says "the light sees all the way to
    // the back of its box here" — nothing occludes, everything is lit. The same
    // identity `depth_buffer::clear` has had since Lesson 3.1.
    dsi.clear_depth = 1.0f;
    dsi.load_op = SDL_GPU_LOADOP_CLEAR;

    // **STORE, NOT DONT_CARE**, and this is the line `gpu_scene.cpp` predicted in
    // Lesson 4.7. The scene's depth buffer is consumed by the pass that writes it
    // and may stay in tile memory forever; this one is read by a LATER pass, so
    // it has to be written out to memory in full. That is the cost of a shadow
    // map on tiled hardware, and it is not small.
    dsi.store_op = SDL_GPU_STOREOP_STORE;
    dsi.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    dsi.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    const debug_group g(cb, "shadow map (depth only)", log);

    // NO COLOUR TARGETS: a null array and a count of zero. The pass still knows
    // its own dimensions — they come from the attachment — which is the thing
    // `fill_style::depth_only` could not arrange on the CPU.
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, nullptr, 0, &dsi);
    if (pass == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_shadow: SDL_BeginGPURenderPass failed: %s",
                         SDL_GetError());
        return;
    }

    if (log != nullptr)
    {
        log->record(gpu_event_kind::pass_begin, "depth only (no colour target)");
    }

    // The viewport is the whole map. Set explicitly rather than relying on the
    // default, because the swapchain-sized viewport left behind by a previous
    // pass would silently render the scene into one corner.
    SDL_GPUViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.w = static_cast<float>(resolution_);
    vp.h = static_cast<float>(resolution_);
    vp.min_depth = 0.0f;
    vp.max_depth = 1.0f;
    SDL_SetGPUViewport(pass, &vp);

    SDL_BindGPUGraphicsPipeline(pass, pipeline_.handle());
    if (log != nullptr) { log->record(gpu_event_kind::bind_pipeline, "shadow depth-only"); }

    // ONE per-frame push, and it is a camera matrix in the camera's slot. "Render
    // from the light" is literally this: the same b0 the scene pass fills with
    // `projection * view`, filled with the light's box instead.
    const camera_uniforms light_camera_block{cam.clip_from_world};
    SDL_PushGPUVertexUniformData(cb, 0, &light_camera_block, sizeof(light_camera_block));
    if (log != nullptr)
    {
        log->record(gpu_event_kind::push_uniform, "light camera (vertex)", 0,
                    static_cast<Uint32>(sizeof(light_camera_block)));
    }

    for (int i = 0; i < count; ++i)
    {
        const gpu_draw_item& item = items[i];
        if (item.mesh == nullptr || !item.mesh->valid()) { continue; }

        // THE MODEL MATRIX ALONE. `object_uniforms` also carries the normal
        // matrix's three columns, and pushing it whole would be pushing 48 bytes
        // per draw into a shader that declares 64 — which is legal, wasteful, and
        // exactly the kind of quiet mismatch Lesson 4.4 spent a whole lesson on.
        // `shadow.vert.hlsl` declares one `float4x4` and this pushes one.
        const mat4 world_from_model = item.world_from_model;
        SDL_PushGPUVertexUniformData(cb, 1, &world_from_model, sizeof(world_from_model));

        item.mesh->bind(pass, 0);
        item.mesh->draw(pass, 1);

        if (log != nullptr)
        {
            log->record(gpu_event_kind::push_uniform, "object model matrix", 1,
                        static_cast<Uint32>(sizeof(world_from_model)));
            log->record(gpu_event_kind::draw, "shadow caster", item.mesh->index_count(), 1u,
                        (item.mesh->index_count() > 0u) ? item.mesh->index_count() / 3u
                                                        : item.mesh->vertex_count() / 3u);
        }
    }

    if (log != nullptr) { log->record(gpu_event_kind::pass_end, nullptr); }
    SDL_EndGPURenderPass(pass);
}

void gpu_shadow_map::fill_uniforms(scene_light_uniforms& out, const light_camera& cam,
                                   const shadow_settings& set, int resolution)
{
    out.light_clip_from_world = cam.clip_from_world;
    out.shadow_strength = set.strength;
    out.shadow_texel = cam.world_per_texel;
    out.shadow_depth_range = cam.depth_range;
    out.shadow_bias = set.constant_bias + quantisation_bias(depth_format::f32);
    out.shadow_slope_scale = set.slope_scale;
    out.shadow_max_slope = set.max_slope;
    out.shadow_reach = pcf_reach_texels(set.pcf_radius);
    out.shadow_pcf = static_cast<float>(set.pcf_radius);

    // The mode as a NUMBER, spelled out rather than cast from the enum. Lesson
    // 6.4 established the rule when `spec_model` gained a fourth value: an enum's
    // underlying value is a C++ detail and the shader's contract is a number, so
    // reordering the enum must not silently re-map the shader.
    out.shadow_mode = (set.bias == shadow_bias::none)          ? 0.0f
                    : (set.bias == shadow_bias::constant)      ? 1.0f
                    : (set.bias == shadow_bias::slope_scaled)  ? 2.0f
                                                               : 3.0f;
    out.shadow_normal_scale = set.normal_scale;
    out.shadow_texel_uv = (resolution > 0) ? 1.0f / static_cast<float>(resolution) : 0.0f;
    out.pad2 = 0.0f;
}

void gpu_shadow_map::fill_cascade_uniforms(cascade_uniforms& out,
                                           const cascaded_shadow_map& csm,
                                           vec3 view_forward)
{
    out = cascade_uniforms{};
    const vec3 f = normalised_or(view_forward, vec3{0.0f, 0.0f, -1.0f});
    out.view_forward = vec4{f.x, f.y, f.z, 0.0f};

    const int n = csm.count();
    out.cascade_count = static_cast<float>(n);
    out.blend_fraction = csm.settings().blend_fraction;

    float* splits = &out.splits.x;
    float* wpt = &out.world_per_texel.x;
    float* range = &out.depth_range.x;

    for (int i = 0; i < 4; ++i)
    {
        // CASCADES PAST THE END REPEAT THE LAST ONE rather than holding zero.
        // A zero split would make the shader select a cascade that was never
        // rendered, and a zero `world_per_texel` would make its bias zero —
        // which is acne, appearing only when the count is below four. Clamping
        // makes the unused slots harmless instead of merely unused.
        const int src = (i < n) ? i : (n - 1);
        out.light_clip_from_world[i] = csm.map(src).camera().clip_from_world;
        splits[i] = csm.splits()[static_cast<std::size_t>(src)];
        wpt[i] = csm.map(src).camera().world_per_texel;
        range[i] = csm.map(src).camera().depth_range;
    }
}

} // namespace engine
