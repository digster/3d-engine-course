// engine/src/gfx/shadow.cpp — see shadow.hpp for why this file exists.

#include <engine/gfx/shadow.hpp>

#include <engine/core/log.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/transform.hpp>

#include <SDL3/SDL.h>

#include <cmath>

namespace engine {

const char* name_of(shadow_bias b)
{
    switch (b)
    {
    case shadow_bias::none:          return "NONE (the acne)";
    case shadow_bias::constant:      return "CONSTANT (peter-pans)";
    case shadow_bias::slope_scaled:  return "SLOPE-SCALED (derived)";
    case shadow_bias::normal_offset: return "NORMAL-OFFSET (geometric N)";
    }
    return "?";
}

float quantisation_bias(depth_format format)
{
    // Half of one code, in device units. `depth_buffer::quantise` rounds to the
    // nearest of `2^bits - 1` evenly spaced codes, so the stored value can be
    // wrong by half a gap before any geometry is involved.
    switch (format)
    {
    // A float's grid is not uniform, so "half a code" is not one number — but
    // near the values a depth buffer actually holds it is around 6e-8 at the far
    // plane and unmeasurably smaller near zero, which is two orders of magnitude
    // below the slope term at every angle this engine can rasterise. Reporting 0
    // is the honest simplification: the term exists and does not matter.
    case depth_format::f32:     return 0.0f;
    case depth_format::unorm24: return 0.5f / 16777215.0f;
    case depth_format::unorm16: return 0.5f / 65535.0f;
    }
    return 0.0f;
}

light_camera fit_directional(const directional_light& key, const aabb& scene,
                             int resolution)
{
    light_camera cam;
    if (resolution <= 0 || scene.empty()) { return cam; }

    // ---- The light's axes ---------------------------------------------------
    //
    // `direction` is the direction of TRAVEL (light.hpp), so it is the light
    // camera's forward axis directly — no negation. The up hint only has to be
    // non-parallel to it; which non-parallel vector it is decides how the map's
    // texel grid is rotated about the light axis and nothing else.
    const vec3 fwd = normalised_or(key.direction, vec3{0.0f, -1.0f, 0.0f});
    vec3 up{0.0f, 1.0f, 0.0f};
    if (std::fabs(dot(fwd, up)) > 0.99f) { up = vec3{0.0f, 0.0f, 1.0f}; }

    // THE EYE IS THE SCENE'S CENTRE, which sounds wrong and is exactly right. A
    // directional light has no position, so any point on the axis gives the same
    // rays; putting the eye in the middle means the scene straddles z = 0 in
    // light view space and the near plane comes out NEGATIVE. `perspective`
    // could not accept that — a negative near plane is a divide by a sign
    // change — and `orthographic` can, because it has no divide.
    const vec3 centre = scene.centre();
    cam.view_from_world = look_at(centre, centre + fwd, up);

    // ---- The box, measured in the light's own space -------------------------
    const aabb lv = transformed(scene, cam.view_from_world);
    const vec3 ext = lv.extent();
    const vec3 mid = lv.centre();

    // SQUARE IN X AND Y. Every formula in §4 says "one texel" as though a texel
    // had a single size; making the box square is what earns that sentence. One
    // texel of margin on each side keeps geometry that sits exactly on the
    // boundary from being clipped by the fill rule.
    float half = 0.5f * std::max(ext.x, ext.y);
    if (half <= 0.0f) { half = 1e-3f; }             // a scene with no extent
    half *= 1.0f + 2.0f / static_cast<float>(resolution);

    // The near and far planes, from the extent ALONG the light. View space looks
    // down -z, so the nearest surface has the LARGEST z and the furthest the
    // smallest — the sign flip that turns a view-space z into a distance.
    float near_z = -lv.max.z;
    float far_z  = -lv.min.z;

    // Pad both, by 1% of the range or a millimetre, whichever is larger. Without
    // it the nearest vertex sits exactly on the near plane, where `near_distance`
    // returns 0 and the clipper's `>= 0` test is decided by the last bit of a
    // float — a caster flickering in and out of its own shadow map.
    const float pad = std::max(1e-3f, 0.01f * (far_z - near_z));
    near_z -= pad;
    far_z  += pad;

    cam.clip_from_view = orthographic(mid.x - half, mid.x + half,
                                      mid.y - half, mid.y + half,
                                      near_z, far_z);
    cam.clip_from_world = cam.clip_from_view * cam.view_from_world;

    cam.vp.x = 0.0f;
    cam.vp.y = 0.0f;
    cam.vp.w = static_cast<float>(resolution);
    cam.vp.h = static_cast<float>(resolution);
    cam.vp.min_depth = 0.0f;
    cam.vp.max_depth = 1.0f;

    // The two numbers §4 runs on: how much world one texel covers, and how much
    // world one unit of device depth is worth.
    cam.world_per_texel = (2.0f * half) / static_cast<float>(resolution);
    cam.depth_range = far_z - near_z;
    return cam;
}

aabb shadow_map::bounds_of(std::span<const scene_object> objects, const mesh_pool& meshes)
{
    aabb box;
    for (const scene_object& obj : objects)
    {
        const mesh_data* geometry = meshes.get(obj.geometry);
        if (geometry == nullptr) { continue; }

        // EVERY VERTEX, not the object-space box transformed. `transformed()`
        // returns the box around the transformed box, which under a rotation is
        // up to sqrt(3) too large — and a shadow map's texel density is inversely
        // proportional to the box's side, so slack here is resolution thrown
        // away. This is the one place in the engine where the tight answer is
        // worth a pass over the vertices.
        const mat4 world_from_model = model_matrix(obj.xform, trs_order::trs);
        for (const vec3& v : geometry->vertices)
        {
            const vec4 w = world_from_model * point(v);
            box.expand(vec3{w.x, w.y, w.z});
        }
    }
    return box;
}

bool shadow_map::create(int resolution, depth_format format)
{
    if (resolution <= 0) { return false; }

    depth_ = std::make_unique<depth_buffer>(resolution, resolution, format);
    unused_colour_ = std::make_unique<framebuffer>(resolution, resolution);
    resolution_ = resolution;
    cam_ = light_camera{};
    bounds_ = aabb{};

    ENGINE_LOG_INFO(engine::log_gfx, "shadow_map: %dx%d %s (%d KB depth, %d KB unused colour)",
                    resolution, resolution, name_of(format),
                    static_cast<int>(sizeof(float) * static_cast<std::size_t>(resolution)
                                     * static_cast<std::size_t>(resolution) / 1024),
                    static_cast<int>(sizeof(Uint32) * static_cast<std::size_t>(resolution)
                                     * static_cast<std::size_t>(resolution) / 1024));
    return true;
}

void shadow_map::render(std::span<const scene_object> objects, const mesh_pool& meshes,
                        const directional_light& key, shadow_stats* stats_out)
{
    if (!valid()) { return; }

    const Uint64 t0 = SDL_GetPerformanceCounter();

    bounds_ = bounds_of(objects, meshes);
    cam_ = fit_directional(key, bounds_, resolution_);

    // A cleared buffer is full of `k_far`, which reads back as "the light sees
    // all the way to the far plane here" — nothing occludes, everything is lit.
    // That is the right identity for an empty map, and it is why forgetting the
    // clear turns last frame's shadows into this frame's (Lesson 3.1 §7).
    depth_->clear();

    if (!cam_.valid())
    {
        if (stats_out != nullptr) { *stats_out = shadow_stats{}; }
        return;
    }

    // ---- The same vertex stage, pointed somewhere else ----------------------
    //
    // THIS IS THE LESSON'S CHEAPEST CLAIM AND ITS BIGGEST ONE. Nothing below is
    // new: `collect_triangles` is Module 2's pipeline and `draw_triangles` is
    // Module 3's rasterizer, unmodified. A shadow map is not a new renderer, it
    // is the renderer you already have with a different camera and no colour.
    const camera_view cv{cam_.view_from_world, bounds_.centre()};
    const projector pr{cam_.clip_from_view, cam_.vp, near_mode::clip};

    render_options opts;
    opts.compose = trs_order::trs;
    opts.normals = normal_source::vertex;

    // `palette` is the "off" position of the shading cycle — no light, no normal,
    // no BRDF. A depth pass has no use for a colour and computing one would be
    // per-vertex work thrown away; this is the CPU's equivalent of the GPU's
    // depth-only pipeline having no fragment shader worth speaking of.
    opts.shading = shade_eval::palette;
    opts.specular = specular_model::none;

    opts.cull = (set_.cull == cull_mode::back)  ? cull_choice::back
              : (set_.cull == cull_mode::front) ? cull_choice::front
                                                : cull_choice::none;

    const lighting unused{};
    collect_stats cstats;
    collect_triangles(tris_, scratch_, objects, meshes, cv, pr, unused, opts, &cstats);

    fill_style style;
    style.cull = set_.cull;

    // DEPTH ONLY. The fragment function is never called and no colour is stored;
    // `unused_colour_` exists solely to tell the rasterizer how big the pass is
    // (shadow.hpp says why, at length).
    style.depth_only = true;

    draw_triangles(*unused_colour_, depth_.get(), tris_, false, style);

    if (stats_out != nullptr)
    {
        shadow_stats s;
        s.objects = static_cast<int>(objects.size());
        s.triangles = static_cast<int>(tris_.size());
        s.texels = resolution_ * resolution_;
        s.render_ms = 1000.0 * static_cast<double>(SDL_GetPerformanceCounter() - t0)
                    / static_cast<double>(SDL_GetPerformanceFrequency());
        *stats_out = s;
    }
}

float shadow_map::visibility(vec3 world_pos, vec3 geometric_normal, float n_dot_l) const
{
    // Not fitted, or the surface faces away from the light. In the second case
    // the direct term is already zero (Lesson 3.6's clamp), so the shadow map has
    // nothing to add and consulting it would only risk a wrong answer at a
    // silhouette. `1` means "fully lit" and multiplying by it is a no-op.
    if (!valid() || !cam_.valid() || n_dot_l <= 0.0f) { return 1.0f; }

    const float slope = slope_from_cosine(n_dot_l, set_.max_slope);

    // ---- Where to sample ----------------------------------------------------
    //
    // NORMAL-OFFSET MOVES THE POINT, EVERY OTHER POLICY MOVES THE DEPTH — and
    // that is the entire structural difference between them. The offset is
    // proportional to sin(theta): nothing at all when the surface faces the
    // light and one full texel when it is edge-on, which is precisely where the
    // depth-side bias is diverging.
    vec3 p = world_pos;
    if (set_.bias == shadow_bias::normal_offset)
    {
        // Scaled by the same kernel reach, and for the same reason: the taps a
        // 3x3 kernel reads are further away, so the point has to move further
        // off the surface to clear them.
        const float sin_theta = std::sqrt(std::max(0.0f, 1.0f - n_dot_l * n_dot_l));
        const float offset = set_.normal_scale * cam_.world_per_texel * sin_theta
                           * pcf_reach_texels(set_.pcf_radius) * 1.41421356f;
        p = p + normalised_or(geometric_normal, vec3{0.0f, 1.0f, 0.0f}) * offset;
    }

    // ---- Into the light's pixels -------------------------------------------
    //
    // `w` is 1 — an orthographic projection has no perspective divide — so this
    // is an affine map and `perspective_divide` is the identity. It is still
    // written out, because the day this becomes a spot light it stops being.
    const vec4 clip = cam_.clip_from_world * point(p);
    if (clip.w <= 0.0f) { return 1.0f; }
    const vec3 ndc = perspective_divide(clip);

    // The SAME viewport that wrote the map, y-flip and all. Two spellings of one
    // mapping is how a shadow ends up half a texel from its caster.
    const vec3 s = cam_.vp.to_screen(ndc);
    const float depth = s.z;

    // Outside the box the light was fitted to. `depth_at` already answers
    // out-of-bounds reads with `k_far`, which behaves as "nothing in front of
    // you"; returning early says the same thing without doing the taps.
    if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f
        || depth < 0.0f || depth > 1.0f)
    {
        return 1.0f;
    }

    // ---- The bias, in device depth units ------------------------------------
    float bias = 0.0f;
    switch (set_.bias)
    {
    case shadow_bias::none:
        break;
    case shadow_bias::constant:
        bias = set_.constant_bias;
        break;
    case shadow_bias::slope_scaled:
        // THE REACH DEPENDS ON THE KERNEL, and forgetting that is the bug this
        // lesson found by rendering. A 3x3 kernel reads a texel that is 2.12
        // texel-diagonals away, not 0.71, so a bias sized for one tap leaves
        // two thirds of the error uncovered — and the acne it lets back in
        // arrives at the same moment PCF does, which makes it look like a
        // filtering bug.
        bias = set_.constant_bias
             + set_.slope_scale * slope_scaled_bias(cam_.world_per_texel, slope,
                                                    cam_.depth_range,
                                                    pcf_reach_texels(set_.pcf_radius));
        break;
    case shadow_bias::normal_offset:
        // The point has already moved; what is left is the FORMAT's half-code,
        // which no amount of geometry can remove.
        bias = set_.constant_bias;
        break;
    }
    bias += quantisation_bias(depth_->format());

    const float reference = depth - bias;

    // ---- PCF: compare, THEN average ----------------------------------------
    //
    // §6, and the order is not a preference. Averaging the stored DEPTHS of an
    // occluder at 0.3 and one at 0.9 gives 0.6, which a receiver at 0.5 passes —
    // "fully lit", when half its taps are occluded. Comparing first gives 0 and
    // 1, whose average is 0.5. The values being averaged have to be visibilities,
    // because visibility is what we want the average of.
    const int r = (set_.pcf_radius < 0) ? 0 : set_.pcf_radius;

    // ROUND, NOT FLOOR, AND THE DIFFERENCE IS WHERE THIS RASTERIZER PUTS ITS
    // SAMPLES. `fill_triangle` evaluates every attribute — depth included — at
    // INTEGER pixel coordinates (raster.cpp's `weights`, taken at `x`, not
    // `x + 0.5`), so the depth stored "for texel i" is the depth of the surface
    // at exactly `x = i`. The nearest stored sample to a fragment at `x` is
    // therefore `round(x)`, and the offset between them lands in [-0.5, +0.5).
    //
    // Take `floor` instead and every lookup is biased half a texel in one
    // direction: the reach doubles to a full texel diagonal, the derived bias
    // is half of what is needed, and the acne comes back on one side of every
    // slope. It also stops matching the GPU, where hardware samples at pixel
    // CENTRES and `SampleCmp` selects the texel containing the uv — a different
    // spelling of the same [-0.5, +0.5) offset. §4.4.
    const int cx = static_cast<int>(std::lround(s.x));
    const int cy = static_cast<int>(std::lround(s.y));

    int lit = 0;
    int taps = 0;
    for (int dy = -r; dy <= r; ++dy)
    {
        for (int dx = -r; dx <= r; ++dx)
        {
            const float stored = depth_->depth_at(cx + dx, cy + dy);
            lit += (reference <= stored) ? 1 : 0;
            ++taps;
        }
    }

    const float fraction = static_cast<float>(lit) / static_cast<float>(taps);

    // `strength` fades the whole effect. At 1 a fully occluded surface keeps
    // only its ambient term, which is what a real shadow is; below 1 it is a
    // dial for the lesson's before/after pictures and nothing more.
    return 1.0f - set_.strength * (1.0f - fraction);
}

} // namespace engine
