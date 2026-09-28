// engine/src/gfx/shadow.cpp — see shadow.hpp for why this file exists.

#include <engine/gfx/shadow.hpp>

#include <engine/core/log.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/transform.hpp>

#include <SDL3/SDL.h>

#include <algorithm>   // 6.17b: std::clamp in fit_spot
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

// ---------------------------------------------------------------------------
// Lesson 6.17b — cameras for lights that are somewhere
// ---------------------------------------------------------------------------

namespace {

/// Fill in the parts every local-light camera shares: the viewport, the
/// per-metre texel footprint, and the planes the bias conversion needs.
void finish_perspective(light_camera& cam, int resolution, float half_angle,
                        float near_plane, float far_plane, vec3 eye, vec3 axis)
{
    cam.eye = eye;
    cam.axis = axis;
    cam.clip_from_world = cam.clip_from_view * cam.view_from_world;

    // The SAME viewport convention `fit_directional` uses, y-flip and all, so
    // `shadow_map::render` and `visibility` agree on which texel is which.
    cam.vp.x = 0.0f;
    cam.vp.y = 0.0f;
    cam.vp.w = static_cast<float>(resolution);
    cam.vp.h = static_cast<float>(resolution);
    cam.vp.min_depth = 0.0f;
    cam.vp.max_depth = 1.0f;

    // One texel at ONE METRE: the map spans `2 tan(half_angle)` metres there,
    // over `resolution` texels. For a cube face `tan(45 deg) = 1` and this is
    // simply `2 / resolution`.
    cam.world_per_texel = 2.0f * std::tan(half_angle) / static_cast<float>(resolution);
    cam.depth_range = far_plane - near_plane;   // informational under perspective
    cam.perspective = true;
    cam.near_plane = near_plane;
    cam.far_plane = far_plane;
}

/// Far plane for a local light: its range, or 50 m when the range is infinite.
[[nodiscard]] float far_for(const local_light& light)
{
    return (light.range > 0.0f) ? light.range : 50.0f;
}

} // namespace

light_camera fit_spot(const local_light& light, int resolution, float near_plane)
{
    light_camera cam;
    if (resolution <= 0 || near_plane <= 0.0f) { return cam; }

    const float far_plane = far_for(light);
    if (far_plane <= near_plane) { return cam; }

    const vec3 fwd = normalised_or(light.direction, vec3{0.0f, -1.0f, 0.0f});
    // The same parallel-up guard `fit_directional` needs, for the same reason:
    // a spot pointing straight down is the COMMONEST spot there is, and
    // `look_at` with an up vector parallel to the view axis has no right-hand
    // side to build.
    vec3 up{0.0f, 1.0f, 0.0f};
    if (std::fabs(dot(fwd, up)) > 0.99f) { up = vec3{0.0f, 0.0f, 1.0f}; }

    const float half = std::clamp(light.outer_angle, 1.0e-3f, k_max_spot_shadow_angle);

    cam.view_from_world = look_at(light.position, light.position + fwd, up);
    cam.clip_from_view = perspective(2.0f * half, 1.0f, near_plane, far_plane);
    finish_perspective(cam, resolution, half, near_plane, far_plane, light.position, fwd);
    return cam;
}

light_camera fit_cube_face(const local_light& light, cube_face face, int resolution,
                           float near_plane)
{
    light_camera cam;
    if (resolution <= 0 || near_plane <= 0.0f) { return cam; }

    const float far_plane = far_for(light);
    if (far_plane <= near_plane) { return cam; }

    // ---- The view matrix, written as its own definition ----------------------
    //
    // A view matrix's ROWS are the camera's right, up and back axes (Lesson 2.9:
    // `look_at` builds exactly this, from a forward and an up hint). Here all
    // three are DICTATED by the face table rather than chosen:
    //
    //     right =  u        so the image's x grows the way the face's u does
    //     up    = -v        because the face's v grows DOWN the image
    //     back  = -major    because a view looks down its own -z
    //
    // `look_at(p, p + major, -v)` would compute the same up and back and then
    // DERIVE right as `forward x up` — which for every face is `-u`. The
    // determinant of these rows is -1: this is a reflection, not a rotation,
    // and that sign is the whole of 6.15's handedness warning.
    const cube_axes ax = cube_face_axes(face);
    const vec3 r = ax.u;
    const vec3 upv = -ax.v;
    const vec3 back = -ax.major;
    const vec3 p = light.position;

    // Column-major storage (conventions §3): each braced group is a COLUMN.
    cam.view_from_world = mat4{
        {r.x, upv.x, back.x, 0.0f},
        {r.y, upv.y, back.y, 0.0f},
        {r.z, upv.z, back.z, 0.0f},
        {-dot(r, p), -dot(upv, p), -dot(back, p), 1.0f}};

    // 90 degrees, square: six of these tile the sphere with no gap and no
    // overlap, which is what makes a cube a cube.
    constexpr float k_quarter_turn = 1.5707963f;
    cam.clip_from_view = perspective(k_quarter_turn, 1.0f, near_plane, far_plane);
    finish_perspective(cam, resolution, 0.5f * k_quarter_turn, near_plane, far_plane,
                       light.position, ax.major);
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

    // The one-map case fits the box itself, from the same meshes it is about to
    // draw. Lesson 6.9's cascades fit elsewhere and call the overload below.
    const aabb scene = bounds_of(objects, meshes);
    render(objects, meshes, fit_directional(key, scene, resolution_), scene, stats_out);
}

void shadow_map::render(std::span<const scene_object> objects, const mesh_pool& meshes,
                        const light_camera& cam, const aabb& bounds,
                        shadow_stats* stats_out)
{
    if (!valid()) { return; }

    const Uint64 t0 = SDL_GetPerformanceCounter();

    bounds_ = bounds;
    cam_ = cam;

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

    // ---- LESSON 6.17b: HOW BIG IS A TEXEL, HERE? ----------------------------
    //
    // Under an orthographic box the answer is `world_per_texel`, everywhere —
    // which is why 6.8 could derive one bias per surface slope and stop. Under
    // a perspective camera (a spot light, a cube face) the pyramid widens as it
    // goes, and a texel `d` metres along the axis is `d` times the footprint
    // `world_per_texel` records at one metre. So the fragment's AXIAL distance
    // is needed before any bias can be sized, and `w` is exactly that: for
    // `perspective()`, clip-space `w` is the distance along the view axis, and
    // for a cube face it is the major-axis component — the same number.
    //
    // The ORTHOGRAPHIC path computes nothing new here and multiplies by the
    // same value it always did, so every shadow drawn before this lesson —
    // 6.8's, 6.9's cascades — is bit-for-bit what it was.
    float texel = cam_.world_per_texel;
    float bias_slope = slope;
    if (cam_.perspective)
    {
        const vec4 at = cam_.clip_from_world * point(world_pos);
        if (at.w <= 0.0f) { return 1.0f; }   // behind the lamp: not in this map
        texel *= at.w;

        // AND THE SLOPE IS NOT tan(theta) ANY MORE — `perspective_slope`
        // derives why. `slope` above stays tan(theta) for the normal offset,
        // which moves the point across the SURFACE and is still about the ray.
        const vec3 n = normalised_or(geometric_normal, vec3{0.0f, 1.0f, 0.0f});
        const float dist = length(world_pos - cam_.eye);
        const float cos_phi = (dist > 0.0f) ? at.w / dist : 1.0f;
        bias_slope = perspective_slope(dot(n, cam_.axis), cos_phi, n_dot_l, set_.max_slope);
    }

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
        const float offset = set_.normal_scale * texel * sin_theta
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
    vec3 ndc = perspective_divide(clip);

    // LESSON 6.17b: A PERSPECTIVE MAP CLAMPS TO ITS EDGE INSTEAD OF REJECTING.
    // For a cube face that is a statement about ownership: `local_shadow_set`
    // chose this face with `direction_to_cube`, so the direction belongs here
    // even when float rounding projects it a hair past -1 — which is exactly
    // what happens on the 45-degree seams between faces, where the choice was
    // a tie. Rejecting would call those points lit; verify_617b §F found a line
    // of them leaking through a crate's shadow. For a spot the clamp can only
    // touch points outside the fitted square, where the cone is already zero.
    // The orthographic path keeps 6.8's rejection, unchanged.
    if (cam_.perspective)
    {
        ndc.x = std::clamp(ndc.x, -1.0f, 1.0f);
        ndc.y = std::clamp(ndc.y, -1.0f, 1.0f);
    }

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
        if (cam_.perspective)
        {
            // LESSON 6.17b. 6.8 §4.4's derivation, unchanged, up to the last
            // step: the furthest tap is `reach` texels away, a texel HERE is
            // `texel` metres, and walking that far across a surface tilted at
            // theta changes its depth by that distance times tan(theta). That
            // is a bias in METRES, measured along the light's axis.
            //
            // The last step is the one that changed. 6.8 divided by
            // `depth_range` because an orthographic projection makes device
            // depth AFFINE in distance — one scale factor converts metres
            // everywhere. `perspective()` makes it a hyperbola, so the honest
            // conversion is to move the point back by the bias in metres and
            // ask the curve how far that moved it in device units. Exact,
            // with no derivative taken and nothing assumed small.
            //
            // AND THE REACH IS HALF A TEXEL LONGER THAN 6.8's, for a reason the
            // orthographic map never made visible: this rasterizer snaps every
            // vertex to a whole pixel (Lesson 2.2's integer edge functions), and
            // snapping can slide a triangle's depth PLANE sideways by up to half
            // a texel. A floor clipped to the guard band is thousands of pixels
            // wide and slides by the full amount — verify_617b measured a stored
            // depth 0.36 of a texel's depth step nearer than the analytic floor,
            // and 1,895 points of acne along one cube seam in gltf_view's scene.
            // Half a texel from the nearest sample, half from the snap: (r + 1)
            // sqrt(2) — which is also the GPU's reach, for its own reason (its
            // one tap is a 2x2 bilinear comparison; scene.frag.hlsl says why).
            const float snap_reach = pcf_reach_texels(set_.pcf_radius) + 0.70710678f;
            const float metres = set_.slope_scale * (snap_reach * texel * bias_slope);
            const float w = clip.w;
            const float back = std::max(cam_.near_plane, w - metres);
            bias = set_.constant_bias + k_perspective_depth_floor
                 + (perspective_depth(w, cam_.near_plane, cam_.far_plane)
                    - perspective_depth(back, cam_.near_plane, cam_.far_plane));
        }
        else
        {
            bias = set_.constant_bias
                 + set_.slope_scale * slope_scaled_bias(cam_.world_per_texel, slope,
                                                        cam_.depth_range,
                                                        pcf_reach_texels(set_.pcf_radius));
        }
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
    int cx = static_cast<int>(std::lround(s.x));
    int cy = static_cast<int>(std::lround(s.y));

    // 6.17b, the same argument one step later: a point in a face's last half
    // texel rounds to a sample one past the edge, which `depth_at` answers with
    // "far". Its nearest sample IN THIS MAP is the edge texel.
    if (cam_.perspective)
    {
        cx = std::clamp(cx, 0, resolution_ - 1);
        cy = std::clamp(cy, 0, resolution_ - 1);
    }

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

// ---------------------------------------------------------------------------
// Lesson 6.17b — local_shadow_set
// ---------------------------------------------------------------------------

void local_shadow_set::render(std::span<const scene_object> objects,
                              const mesh_pool& meshes,
                              std::span<const local_light> lights,
                              shadow_stats* stats)
{
    first_.assign(lights.size(), -1);
    kinds_.resize(lights.size());
    positions_.resize(lights.size());

    // ---- Lay the maps out: one per spot, a block of six per point -----------
    int needed = 0;
    for (std::size_t i = 0; i < lights.size(); ++i)
    {
        kinds_[i] = lights[i].kind;
        positions_[i] = lights[i].position;
        if (!lights[i].casts_shadow) { continue; }
        first_[i] = needed;
        needed += (lights[i].kind == local_light_kind::spot) ? 1 : k_cube_faces;
    }

    // Grow, never shrink below what is in use; a map is re-created only when
    // the resolution it must have changed, which is the steady-state promise
    // `shadow_map` itself makes (its `render` reuses its triangle list).
    if (static_cast<int>(maps_.size()) < needed) { maps_.resize(static_cast<std::size_t>(needed)); }

    const aabb bounds = shadow_map::bounds_of(objects, meshes);
    shadow_stats total;

    for (std::size_t i = 0; i < lights.size(); ++i)
    {
        if (first_[i] < 0) { continue; }
        const local_light& light = lights[i];
        const bool spot = (light.kind == local_light_kind::spot);
        const int res = spot ? set_.spot_resolution : set_.point_resolution;
        const int faces = spot ? 1 : k_cube_faces;

        for (int f = 0; f < faces; ++f)
        {
            shadow_map& m = maps_[static_cast<std::size_t>(first_[i] + f)];
            if (!m.valid() || m.resolution() != res)
            {
                if (!m.create(res)) { continue; }
            }
            m.settings() = set_.bias;

            const light_camera cam = spot
                ? fit_spot(light, res, set_.near_plane)
                : fit_cube_face(light, static_cast<cube_face>(f), res, set_.near_plane);

            shadow_stats s;
            m.render(objects, meshes, cam, bounds, &s);
            total.objects += s.objects;
            total.triangles += s.triangles;
            total.texels += s.texels;
            total.render_ms += s.render_ms;
        }
    }

    if (stats != nullptr) { *stats = total; }
}

int local_shadow_set::first_map(std::size_t light_index) const
{
    return (light_index < first_.size()) ? first_[light_index] : -1;
}

float local_shadow_set::visibility(std::size_t light_index, vec3 world_pos,
                                   vec3 geometric_normal, float n_dot_l) const
{
    const int first = first_map(light_index);
    if (first < 0) { return 1.0f; }

    if (kinds_[light_index] == local_light_kind::spot)
    {
        return maps_[static_cast<std::size_t>(first)].visibility(world_pos, geometric_normal,
                                                                 n_dot_l);
    }

    // THE FACE IS THE ONE THE HARDWARE WOULD PICK. `direction_to_cube` is the
    // largest-component rule a `TextureCube` lookup applies, on the vector from
    // the lamp to the point — so the CPU reads the face the GPU reads, and the
    // face's own camera (whose `w` is that same largest component) sizes the
    // bias from the same distance.
    const cube_texel t = direction_to_cube(world_pos - positions_[light_index]);
    const int face = static_cast<int>(t.face);
    return maps_[static_cast<std::size_t>(first + face)].visibility(world_pos,
                                                                    geometric_normal,
                                                                    n_dot_l);
}

} // namespace engine
