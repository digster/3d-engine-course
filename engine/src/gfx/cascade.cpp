// engine/src/gfx/cascade.cpp — Lesson 6.9.
//
// The header carries the argument; this file is the arithmetic.

#include <engine/gfx/cascade.hpp>

#include <algorithm>
#include <cmath>

namespace engine
{
namespace
{

/// Round `v` down to a whole multiple of `step`.
///
/// `std::floor` and not a cast: a cast truncates TOWARD ZERO, so it rounds the
/// wrong way for negative coordinates and the grid would have a discontinuity
/// at the origin. Light space straddles the origin by construction — the box is
/// centred on the slice — so half the world would snap one way and half the
/// other, and the seam would sit exactly where the camera usually is.
[[nodiscard]] float snap_down(float v, float step)
{
    if (step <= 0.0f) { return v; }
    return std::floor(v / step) * step;
}

} // namespace

float practical_split(int index, int count, float near_d, float far_d,
                      float lambda)
{
    if (count <= 0) { return far_d; }
    if (index >= count - 1) { return far_d; }   // the last cut IS the far plane
    if (index < 0) { return near_d; }

    // Guard the logarithm. `near_d` at or below zero has no log, and the ratio
    // far/near is what the scheme is built on.
    const float n = std::max(near_d, 1e-4f);
    const float f = std::max(far_d, n * (1.0f + 1e-4f));

    const float frac = static_cast<float>(index + 1) / static_cast<float>(count);

    const float uniform = n + (f - n) * frac;
    const float logarithmic = n * std::pow(f / n, frac);

    const float l = std::clamp(lambda, 0.0f, 1.0f);
    return l * logarithmic + (1.0f - l) * uniform;
}

frustum_slice slice_corners(const camera_frustum& cam, float near_d, float far_d)
{
    frustum_slice out;

    // At distance d the frustum's half-height is tan(fovy/2)·d. That IS the
    // definition of a vertical field of view — the projection matrix is built
    // from the same tangent (mat4.hpp `perspective`), so this is not a
    // re-derivation, it is the same number read forwards instead of backwards.
    const float t = std::tan(0.5f * cam.fovy_radians);

    const float d[2] = { near_d, far_d };
    for (int plane = 0; plane < 2; ++plane)
    {
        const float hh = t * d[plane];
        const float hw = hh * cam.aspect;

        for (int corner = 0; corner < 4; ++corner)
        {
            // Bit 0 is +x, bit 1 is +y, bit 2 (`plane`) is the far plane — the
            // same bit pattern `aabb::corners` uses, so an index means one thing
            // across the whole engine.
            const float sx = (corner & 1) ? hw : -hw;
            const float sy = (corner & 2) ? hh : -hh;

            // -Z IS FORWARD (conventions §2). A corner at distance d sits at
            // z = -d, and writing +d here builds the frustum behind the camera —
            // which looks correct exactly when you turn around.
            const vec3 view_pos{sx, sy, -d[plane]};
            out.corners[plane * 4 + corner] =
                xyz(cam.world_from_view * point(view_pos));
        }
    }
    return out;
}

light_camera fit_directional_slice(const directional_light& key,
                                   const frustum_slice& slice,
                                   const aabb& casters, int resolution,
                                   bool snap)
{
    light_camera cam;
    if (resolution <= 0) { return cam; }

    // ---- The light's orientation, exactly as 6.8 built it --------------------
    const vec3 fwd = normalised_or(key.direction, vec3{0.0f, -1.0f, 0.0f});
    vec3 up{0.0f, 1.0f, 0.0f};
    if (std::fabs(dot(fwd, up)) > 0.99f) { up = vec3{0.0f, 0.0f, 1.0f}; }

    // ---- The bounding SPHERE of the slice ------------------------------------
    //
    // THE REASON THIS IS A SPHERE AND NOT A BOX is the whole anti-shimmer
    // argument. A box fitted to the eight corners changes SIZE as the camera
    // yaws, because the corners rotate inside it — and `world_per_texel` is the
    // box side over the resolution, so every texel in the map would change size
    // while the camera merely turned on the spot. A sphere has no orientation.
    vec3 centre{0.0f, 0.0f, 0.0f};
    for (const vec3& c : slice.corners) { centre += c; }
    centre *= (1.0f / 8.0f);

    float radius = 0.0f;
    for (const vec3& c : slice.corners)
    {
        radius = std::max(radius, length(c - centre));
    }
    if (radius <= 0.0f) { radius = 1e-3f; }

    // Half a texel of slack on each side, so a sample exactly on the boundary
    // still has neighbours for PCF to read. 6.8's fit does the same thing.
    radius *= 1.0f + 2.0f / static_cast<float>(resolution);

    const float side = 2.0f * radius;
    const float world_per_texel = side / static_cast<float>(resolution);

    // ---- Snapping ------------------------------------------------------------
    //
    // Build the light's basis first, move the sphere centre into it, and round
    // THERE — the grid we are snapping to is the map's, and the map's axes are
    // the light's. Snapping in world space would round along the wrong axes and
    // achieve nothing.
    // THE BASIS IS ANCHORED AT THE WORLD ORIGIN, NOT AT THE SLICE CENTRE, and
    // that is the whole of it. Build it at the centre and `basis * point(centre)`
    // is (0,0,0) by construction — the centre is the eye — so there is nothing
    // left to round and snapping silently does nothing. The grid we are snapping
    // to has to be nailed to the WORLD, so its origin must be the world's.
    const mat4 basis = look_at(vec3{0.0f, 0.0f, 0.0f}, fwd, up);
    vec3 centre_ls = xyz(basis * point(centre));

    if (snap)
    {
        // Round DOWN to a whole texel, in x and y. z is the depth axis and its
        // grid is the depth buffer's, not the texel grid's, so it is left alone.
        //
        // The effect: as the camera walks, the box advances in texel-sized jumps
        // instead of sliding, so the grid lands on the same world positions it
        // landed on last frame. A shadow edge either stays put or moves exactly
        // one texel — which the eye reads as stable, where a continuous slide
        // reads as crawling.
        centre_ls.x = snap_down(centre_ls.x, world_per_texel);
        centre_ls.y = snap_down(centre_ls.y, world_per_texel);
    }

    // Put the (possibly snapped) centre back into world space and re-aim the
    // light there, so `view_from_world` and the box agree.
    const mat4 world_from_light = rigid_inverse(basis);
    const vec3 eye = xyz(world_from_light * point(centre_ls));
    cam.view_from_world = look_at(eye, eye + fwd, up);

    // ---- Depth range: the CASTERS, not the slice -----------------------------
    //
    // An occluder between the light and this slice is OUTSIDE the slice and must
    // still be drawn, or it stops casting the moment it leaves the camera's
    // view — shadows blinking out at the screen edge. So the near and far planes
    // come from the casters' extent along the light axis, not from the sphere.
    float near_z = -radius;
    float far_z = radius;
    if (!casters.empty())
    {
        const aabb cl = transformed(casters, cam.view_from_world);
        // Light space looks down -z, so the NEAREST caster has the largest z.
        near_z = std::min(near_z, -cl.max.z);
        far_z = std::max(far_z, -cl.min.z);
    }
    const float pad = std::max(1e-3f, 0.01f * (far_z - near_z));
    near_z -= pad;
    far_z += pad;

    cam.clip_from_view = orthographic(-radius, radius, -radius, radius,
                                      near_z, far_z);
    cam.clip_from_world = cam.clip_from_view * cam.view_from_world;

    cam.vp.x = 0.0f;
    cam.vp.y = 0.0f;
    cam.vp.w = static_cast<float>(resolution);
    cam.vp.h = static_cast<float>(resolution);
    cam.vp.min_depth = 0.0f;
    cam.vp.max_depth = 1.0f;

    // THE TWO NUMBERS 6.8'S BIAS IS MADE OF, and the only per-cascade part of
    // it. Nothing in `slope_scaled_bias` had to change.
    cam.world_per_texel = world_per_texel;
    cam.depth_range = far_z - near_z;
    return cam;
}

bool cascaded_shadow_map::create(int count, int resolution, depth_format format)
{
    maps_.clear();
    splits_.clear();
    resolution_ = 0;

    const int n = std::clamp(count, 1, k_max_cascades);
    if (resolution <= 0) { return false; }

    maps_.resize(static_cast<std::size_t>(n));
    for (shadow_map& m : maps_)
    {
        if (!m.create(resolution, format)) { maps_.clear(); return false; }
    }
    splits_.assign(static_cast<std::size_t>(n), 0.0f);
    resolution_ = resolution;
    set_.count = n;
    return true;
}

void cascaded_shadow_map::apply_shadow_settings(const shadow_settings& s)
{
    for (shadow_map& m : maps_) { m.settings() = s; }
}

void cascaded_shadow_map::render(std::span<const scene_object> objects,
                                 const mesh_pool& meshes,
                                 const directional_light& key,
                                 const camera_frustum& cam,
                                 shadow_stats* stats_out)
{
    if (!valid()) { return; }

    // Once, from the same meshes about to be drawn — 6.8's rule. Every cascade
    // shares these bounds, because "what can cast into this slice" is a question
    // about the whole world, not about the slice.
    casters_ = shadow_map::bounds_of(objects, meshes);

    const int n = count();
    shadow_stats total{};

    float near_d = std::max(set_.near_d, 1e-4f);
    for (int i = 0; i < n; ++i)
    {
        const float far_d = practical_split(i, n, set_.near_d, set_.far_d,
                                            set_.lambda);
        splits_[static_cast<std::size_t>(i)] = far_d;

        const frustum_slice slice = slice_corners(cam, near_d, far_d);
        const light_camera lc = fit_directional_slice(key, slice, casters_,
                                                      resolution_, set_.snap);

        shadow_stats s{};
        maps_[static_cast<std::size_t>(i)].render(objects, meshes, lc, casters_, &s);

        total.objects += s.objects;
        total.triangles += s.triangles;
        total.texels += s.texels;
        total.render_ms += s.render_ms;

        // Slices ABUT: this cascade's far distance is the next one's near. A gap
        // would leave a ring of unshadowed world; an overlap would waste texels
        // resolving the same metres twice.
        near_d = far_d;
    }

    if (stats_out != nullptr) { *stats_out = total; }
}

cascade_choice cascaded_shadow_map::choose(float view_depth) const
{
    cascade_choice out;
    const int n = count();
    if (n <= 0) { return out; }

    int i = 0;
    while (i < n - 1 && view_depth > splits_[static_cast<std::size_t>(i)]) { ++i; }
    out.index = i;
    out.next = i;
    out.blend = 0.0f;

    // ---- The seam, and the band that hides it --------------------------------
    //
    // Two cascades disagree along their shared distance because they have
    // different texel grids and different biases, and the disagreement draws a
    // straight line across the picture at exactly the split distance. Fading
    // from one to the other across a band turns a step into a ramp — the two
    // answers are both defensible, so anything between them is too.
    if (set_.blend_fraction > 0.0f && i < n - 1)
    {
        const float far_d = splits_[static_cast<std::size_t>(i)];
        const float near_d = (i == 0) ? set_.near_d
                                      : splits_[static_cast<std::size_t>(i - 1)];
        const float band = (far_d - near_d) * std::clamp(set_.blend_fraction, 0.0f, 1.0f);
        if (band > 0.0f && view_depth > far_d - band)
        {
            out.next = i + 1;
            out.blend = std::clamp((view_depth - (far_d - band)) / band, 0.0f, 1.0f);
        }
    }
    return out;
}

float cascaded_shadow_map::visibility(vec3 world_pos, vec3 geometric_normal,
                                      float n_dot_l, float view_depth) const
{
    if (!valid()) { return 1.0f; }

    const cascade_choice c = choose(view_depth);
    const float a = maps_[static_cast<std::size_t>(c.index)]
                        .visibility(world_pos, geometric_normal, n_dot_l);
    if (c.blend <= 0.0f || c.next == c.index) { return a; }

    const float b = maps_[static_cast<std::size_t>(c.next)]
                        .visibility(world_pos, geometric_normal, n_dot_l);
    return a + (b - a) * c.blend;
}

} // namespace engine
