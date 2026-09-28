// engine/src/gfx/gpu_shadow.cpp — see gpu_shadow.hpp for why this file exists.

#include <engine/gfx/gpu_shadow.hpp>

#include <engine/core/log.hpp>
#include <engine/gfx/gpu_mesh.hpp>

namespace engine {

// ---------------------------------------------------------------------------
// Shared by every shadow map in the engine — Lesson 6.17b
// ---------------------------------------------------------------------------
//
// Until 6.17b these two functions were the middle of `gpu_shadow_map::create`
// and the body of `gpu_shadow_map::render_into`. A second owner of depth maps —
// `gpu_local_shadows`, below — needs both unchanged, so they moved out rather
// than being copied: the pipeline and the draw loop are facts about "render
// depth from somewhere", and a spot light, a cube face and a cascade all do
// exactly that. Every statement, log line and frame-log event is the one 6.8
// wrote; `verify_617b` §H reruns the older harnesses against it.

namespace {

/// The depth-only pipeline: one position attribute, no colour target, depth
/// test and write, cull NONE.
[[nodiscard]] bool build_depth_only_pipeline(const gpu_device& dev,
                                             SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                                             SDL_GPUTextureFormat format, gpu_pipeline& out)
{
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
    //
    // LESSON 6.17b ADDS A SECOND REASON, and it is structural rather than
    // cautious: a point light's cube faces are rendered through MIRRORED
    // cameras (`fit_cube_face`), which reverses every triangle's winding. A
    // pipeline that culled by winding would need a second copy with the front
    // face flipped just for them; one that culls nothing serves both.
    raw.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    raw.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

    return out.create(dev, desc.info());
}

/// Set the viewport, bind the pipeline, push the light's camera, and draw.
/// The pass is already begun, with the attachment and layer chosen.
void record_depth_draws(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                        const gpu_pipeline& pipeline, int resolution,
                        const mat4& clip_from_world,
                        const gpu_draw_item* items, int count, frame_log* log)
{
    // The viewport is the whole map. Set explicitly rather than relying on the
    // default, because the swapchain-sized viewport left behind by a previous
    // pass would silently render the scene into one corner.
    SDL_GPUViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.w = static_cast<float>(resolution);
    vp.h = static_cast<float>(resolution);
    vp.min_depth = 0.0f;
    vp.max_depth = 1.0f;
    SDL_SetGPUViewport(pass, &vp);

    SDL_BindGPUGraphicsPipeline(pass, pipeline.handle());
    if (log != nullptr) { log->record(gpu_event_kind::bind_pipeline, "shadow depth-only"); }

    // ONE per-frame push, and it is a camera matrix in the camera's slot. "Render
    // from the light" is literally this: the same b0 the scene pass fills with
    // `projection * view`, filled with the light's box instead.
    const camera_uniforms light_camera_block{clip_from_world};
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
}

/// Begin a depth-only pass on one layer of `texture`, cleared to the far plane.
/// 6.8's `render`, parameterised by the target so both owners share it.
[[nodiscard]] SDL_GPURenderPass* begin_depth_pass(SDL_GPUCommandBuffer* cb,
                                                  SDL_GPUTexture* texture, Uint32 layer)
{
    SDL_GPUDepthStencilTargetInfo dsi{};
    dsi.texture = texture;

    // WHICH CASCADE THIS PASS WRITES. A `Uint8`, sitting beside `mip_level` —
    // both are "which slice of this texture is the target", and both default to
    // zero, which is why the one-cascade case needed no change here at all.
    // (6.17b: and for a cube array, which FACE of which cube — `6c + f`.)
    dsi.layer = static_cast<Uint8>(layer);

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

    // NO COLOUR TARGETS: a null array and a count of zero. The pass still knows
    // its own dimensions — they come from the attachment — which is the thing
    // `fill_style::depth_only` could not arrange on the CPU.
    return SDL_BeginGPURenderPass(cb, nullptr, 0, &dsi);
}

} // namespace

// ---------------------------------------------------------------------------
// gpu_shadow_map — Lessons 6.8 and 6.9
// ---------------------------------------------------------------------------

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
    //
    // `build_depth_only_pipeline`, above — shared with the local lights' maps
    // since Lesson 6.17b, because a pipeline is bound to a depth FORMAT and not
    // to which light is rendering into it.
    if (!build_depth_only_pipeline(dev, vertex, fragment, format, pipeline_))
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
    const int clamped = (layer < 0) ? 0 : (layer >= layers_ ? layers_ - 1 : layer);

    const debug_group g(cb, "shadow map (depth only)", log);

    SDL_GPURenderPass* pass = begin_depth_pass(cb, depth_.handle(),
                                               static_cast<Uint32>(clamped));
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

    render_into(cb, pass, items, count, cam, log);

    if (log != nullptr) { log->record(gpu_event_kind::pass_end, nullptr); }
    SDL_EndGPURenderPass(pass);
}

void gpu_shadow_map::render_into(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                                 const gpu_draw_item* items, int count,
                                 const light_camera& cam, frame_log* log) const
{
    if (!valid() || cb == nullptr || pass == nullptr || items == nullptr || count <= 0)
    {
        return;
    }

    record_depth_draws(cb, pass, pipeline_, resolution_, cam.clip_from_world,
                       items, count, log);
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
    // THE GPU'S REACH, NOT THE CPU'S: this lookup is a 2x2 comparison per tap
    // (gpu_shadow.hpp's `gpu_pcf_reach_texels`). Until the fix after 6.17b this
    // was `pcf_reach_texels`, and a one-tap sun left a third of a bare ground
    // as acne on the GPU while the CPU's picture was clean.
    out.shadow_reach = gpu_pcf_reach_texels(set.pcf_radius);
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
    // LESSON 6.15 RENAMED THIS SLOT and the compiler caught it, which is the
    // whole argument for filling padding with a named field: `pad2 = 0` and
    // `ibl_intensity = 0` are the same store, but only one of them is a
    // statement about the environment. This function fills the SHADOW half of
    // the block and must not touch the environment's switch — whoever owns the
    // environment sets it. Zeroing it here would silently disable IBL from
    // inside the shadow code, which is exactly the kind of long-range coupling
    // a `pad` name hides.
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

// ---------------------------------------------------------------------------
// Lesson 6.17b — local lights
// ---------------------------------------------------------------------------

namespace {

/// The shadow mode as the shader's number — `fill_uniforms`' mapping, for the
/// same reason: the contract is a number, not an enum's underlying value.
[[nodiscard]] float mode_number(shadow_bias b)
{
    return (b == shadow_bias::none)         ? 0.0f
         : (b == shadow_bias::constant)     ? 1.0f
         : (b == shadow_bias::slope_scaled) ? 2.0f
                                            : 3.0f;
}

/// Row `r` of a column-major matrix, as a `vec4`: element `r` of each column.
[[nodiscard]] vec4 row_of(const mat4& m, int r)
{
    return vec4{m.at(r, 0), m.at(r, 1), m.at(r, 2), m.at(r, 3)};
}

} // namespace

gpu_local_light pack_local_light(const local_light& light)
{
    gpu_local_light g{};
    g.position_range = vec4{light.position.x, light.position.y, light.position.z,
                            light.range};

    // `colour * intensity`, premultiplied once here so each fragment multiplies
    // once there. `shade_local` groups its product the same way.
    g.radiance = vec4{light.colour.r * light.intensity,
                      light.colour.g * light.intensity,
                      light.colour.b * light.intensity,
                      (light.kind == local_light_kind::spot) ? 1.0f : 0.0f};

    // The cone's cosines are computed HERE, on the CPU, once per light per
    // frame — glTF's reference code says "these two values can be calculated
    // on the CPU", and a `cos` per light per fragment would be the price of
    // not doing so. `cone_terms_of` is the function the CPU renderer calls, so
    // both renderers interpolate between the same two numbers.
    const cone_terms t = cone_terms_of(light);
    const vec3 axis = normalised_or(light.direction, vec3{0.0f, -1.0f, 0.0f});
    g.axis_cone = vec4{axis.x, axis.y, axis.z, t.scale};
    g.cone_shadow = vec4{t.offset, -1.0f, 0.0f, 0.0f};
    return g;
}

gpu_local_shadows::~gpu_local_shadows()
{
    destroy();
}

bool gpu_local_shadows::create(const gpu_device& dev,
                               SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                               SDL_GPUTextureFormat format,
                               const local_shadow_settings& settings,
                               int max_spots, int max_points)
{
    destroy();

    if (vertex == nullptr || fragment == nullptr || format == SDL_GPU_TEXTUREFORMAT_INVALID
        || settings.spot_resolution <= 0 || settings.point_resolution <= 0)
    {
        return false;
    }

    set_ = settings;

    // AT LEAST ONE OF EACH, EVEN WITH A BUDGET OF ZERO. The shader declares both
    // bindings, and a declared slot with nothing bound draws nothing at all —
    // the failure this engine has now paid for five times. A one-layer array
    // nobody writes to costs 1 MB at 512x512 and D32, and it is the difference
    // between "no spot shadows" and "no picture".
    max_spots_ = (max_spots < 1) ? 1 : max_spots;
    max_points_ = (max_points < 1) ? 1 : max_points;

    const bool ok =
        spots_.create_depth_array(dev, format, static_cast<Uint32>(set_.spot_resolution),
                                  static_cast<Uint32>(set_.spot_resolution),
                                  static_cast<Uint32>(max_spots_), "spot shadows", true)
        && points_.create_depth_cube_array(dev, format,
                                           static_cast<Uint32>(set_.point_resolution),
                                           static_cast<Uint32>(max_points_),
                                           "point shadows (cube array)", true)
        && compare_.create_comparison(dev, "local shadow comparison");
    if (!ok)
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "gpu_local_shadows: the shadow textures were not created");
        destroy();
        return false;
    }

    if (!build_depth_only_pipeline(dev, vertex, fragment, format, pipeline_))
    {
        ENGINE_LOG_ERROR(engine::log_gpu,
                         "gpu_local_shadows: the depth-only pipeline was not created");
        destroy();
        return false;
    }

    ENGINE_LOG_INFO(engine::log_gpu,
                    "gpu_local_shadows: %d spot layer(s) at %d, %d cube(s) at %d, %s",
                    max_spots_, set_.spot_resolution, max_points_, set_.point_resolution,
                    name_of(format));
    return true;
}

void gpu_local_shadows::destroy()
{
    pipeline_.destroy();
    compare_.destroy();
    spots_.destroy();
    points_.destroy();
    jobs_.clear();
    max_spots_ = 0;
    max_points_ = 0;
    dropped_ = 0;
}

int gpu_local_shadows::prepare(std::span<const local_light> lights,
                               std::span<gpu_local_light> out)
{
    jobs_.clear();
    dropped_ = 0;

    const std::size_t n = (lights.size() < out.size()) ? lights.size() : out.size();
    int spot_slot = 0;
    int point_slot = 0;

    for (std::size_t i = 0; i < n; ++i)
    {
        const local_light& light = lights[i];
        gpu_local_light g = pack_local_light(light);

        const bool spot = (light.kind == local_light_kind::spot);
        const bool budget = spot ? (spot_slot < max_spots_) : (point_slot < max_points_);
        if (light.casts_shadow && !budget) { ++dropped_; }

        if (light.casts_shadow && budget && valid())
        {
            const int slot = spot ? spot_slot++ : point_slot++;
            const int res = spot ? set_.spot_resolution : set_.point_resolution;
            const shadow_settings& b = set_.bias;

            // THE SHADOW HALF OF THE RECORD. The near and far planes, because a
            // perspective depth needs its own curve to convert a bias in metres
            // (`perspective_depth`); the texel footprint at one metre, which the
            // shader scales by the fragment's own distance; and 6.8's four bias
            // knobs, in the same order `fill_uniforms` pushes the sun's.
            light_camera first_cam = spot
                ? fit_spot(light, res, set_.near_plane)
                : fit_cube_face(light, cube_face::pos_x, res, set_.near_plane);
            g.cone_shadow.y = static_cast<float>(slot);
            g.cone_shadow.z = first_cam.near_plane;
            g.cone_shadow.w = first_cam.far_plane;
            g.shadow_texel = vec4{first_cam.world_per_texel,
                                  1.0f / static_cast<float>(res),
                                  static_cast<float>(b.pcf_radius), b.strength};
            g.shadow_bias = vec4{b.constant_bias + quantisation_bias(depth_format::f32),
                                 mode_number(b.bias), b.slope_scale, b.max_slope};
            g.shadow_normal = vec4{b.normal_scale, 0.0f, 0.0f, 0.0f};

            // A spot's lookup projects the fragment through the map's matrix;
            // a point's does not need one (the cube lookup takes a direction,
            // and the depth is the major axis), so its rows stay zero.
            if (spot)
            {
                for (int r = 0; r < 4; ++r) { g.shadow_row[r] = row_of(first_cam.clip_from_world, r); }

                local_shadow_job job;
                job.light = static_cast<int>(i);
                job.kind = light.kind;
                job.face = 0;
                job.texture = spots_.handle();
                job.layer = static_cast<Uint32>(slot);
                job.resolution = res;
                job.cam = first_cam;
                jobs_.push_back(job);
            }
            else
            {
                for (int f = 0; f < k_cube_faces; ++f)
                {
                    local_shadow_job job;
                    job.light = static_cast<int>(i);
                    job.kind = light.kind;
                    job.face = f;
                    job.texture = points_.handle();
                    job.layer = static_cast<Uint32>(k_cube_faces * slot + f);
                    job.resolution = res;
                    job.cam = (f == 0) ? first_cam
                                       : fit_cube_face(light, static_cast<cube_face>(f), res,
                                                       set_.near_plane);
                    jobs_.push_back(job);
                }
            }
        }

        out[i] = g;
    }

    if (dropped_ > 0)
    {
        ENGINE_LOG_WARN(engine::log_gpu,
                        "gpu_local_shadows: %d shadowed light(s) past the budget "
                        "(%d spot, %d point) are drawn without a shadow",
                        dropped_, max_spots_, max_points_);
    }
    return static_cast<int>(n);
}

void gpu_local_shadows::render(SDL_GPUCommandBuffer* cb, const gpu_draw_item* items,
                               int count, frame_log* log) const
{
    if (!valid() || cb == nullptr || items == nullptr || count <= 0) { return; }

    for (int j = 0; j < job_count(); ++j)
    {
        const local_shadow_job& job = jobs_[static_cast<std::size_t>(j)];
        const debug_group g(cb, (job.kind == local_light_kind::spot)
                                    ? "spot shadow (depth only)"
                                    : "point shadow face (depth only)", log);

        SDL_GPURenderPass* pass = begin_depth_pass(cb, job.texture, job.layer);
        if (pass == nullptr)
        {
            ENGINE_LOG_ERROR(engine::log_gpu,
                             "gpu_local_shadows: SDL_BeginGPURenderPass failed: %s",
                             SDL_GetError());
            return;
        }
        if (log != nullptr)
        {
            log->record(gpu_event_kind::pass_begin, "depth only (no colour target)");
        }

        render_into(cb, pass, j, items, count, log);

        if (log != nullptr) { log->record(gpu_event_kind::pass_end, nullptr); }
        SDL_EndGPURenderPass(pass);
    }
}

void gpu_local_shadows::render_into(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                                    int job_index, const gpu_draw_item* items, int count,
                                    frame_log* log) const
{
    if (!valid() || cb == nullptr || pass == nullptr || items == nullptr || count <= 0
        || job_index < 0 || job_index >= job_count())
    {
        return;
    }

    const local_shadow_job& job = jobs_[static_cast<std::size_t>(job_index)];
    record_depth_draws(cb, pass, pipeline_, job.resolution, job.cam.clip_from_world,
                       items, count, log);
}

} // namespace engine
