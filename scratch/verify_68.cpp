// scratch/verify_68.cpp — Lesson 6.8's harness: the comparison, and what it costs.
//
//   §A  ORTHOGRAPHIC, DERIVED — the box, the affine depth, and w = 1 exactly
//   §B  THE FIT — a directional light's box, squared and padded around a scene
//   §C  THE DISAGREEMENT — acne measured, and matched against its derived bound
//   §D  THE BIAS POLICIES — what each one fixes, and what each one then costs
//   §E  PCF — compare-then-filter, and the reach the kernel adds
//   §F  PRECISION — half a code, and why orthographic depth is the easy case
//   §G  CPU vs GPU — the same shadow, computed twice, on two renderers
//   §H  THE GOLDEN — a new capability must not move the old picture
//
// §C IS THE ONE THAT MATTERS. Shadow acne is usually described and then fixed
// with a constant somebody tuned. It is not a mystery: the shadow map stores the
// depth of ONE POINT per texel and the fragment being tested is somewhere else
// inside that texel, so half of every texel's footprint is downhill of its own
// sample. §C predicts the resulting error from `world_per_texel` and the
// surface's slope, then measures it, and the two agree — which is what turns a
// bias from a fudge into arithmetic.
//
// Build and run:  sh scratch/build_verify_68.sh

#include <engine/core/assert.hpp>
#include <engine/math/bounds.hpp>   // moved from gfx/ by Lesson 8.4
#include <engine/gfx/colour.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_mesh.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_shadow.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/shadow.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failures; }
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
}

void checkf(bool ok, const char* fmt, ...)
{
    ++g_checks;
    if (!ok) { ++g_failures; }
    std::printf("  [%s] ", ok ? "PASS" : "FAIL");
    std::va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    va_end(args);
    std::printf("\n");
}

std::string read_file(const char* path)
{
    std::FILE* fh = std::fopen(path, "rb");
    if (fh == nullptr) { return {}; }
    std::string out;
    char buf[65536];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, fh)) > 0) { out.append(buf, n); }
    std::fclose(fh);
    return out;
}

// ===========================================================================
//  §A — ORTHOGRAPHIC, DERIVED
// ===========================================================================

void section_a_orthographic()
{
    std::printf("\n=== A. Orthographic — a box onto the clip cube, and w stays 1 ===\n");

    // mat4.hpp's worked example, pushed through by hand and then by the code.
    const engine::mat4 o = engine::orthographic(-5.0f, 5.0f, -5.0f, 5.0f, 1.0f, 21.0f);

    const engine::vec4 p = o * engine::point(engine::vec3{2.5f, 0.0f, -6.0f});
    checkf(std::fabs(p.x - 0.5f) < 1e-6f,
           "(2.5, 0, -6) -> x_ndc = %.6f, and 2.5 is half way from the centre to "
           "the box's right edge at 5", static_cast<double>(p.x));
    checkf(std::fabs(p.z - 0.25f) < 1e-6f,
           "…and z_ndc = %.6f. The near plane is at 1 and the far at 21, so a "
           "depth of 6 is (6-1)/20 = a quarter of the way through the box",
           static_cast<double>(p.z));

    // THE CLAIM THE WHOLE LESSON LEANS ON. `w` is not approximately 1, it is
    // exactly 1 — the bottom row of the matrix is (0,0,0,1) and nothing it
    // multiplies can change that. So there is no perspective divide, the map
    // from world to light-clip is AFFINE, and §5.3 can recover a fragment's
    // light-space position from its interpolated world position for free.
    bool w_exact = true;
    for (int i = 0; i < 64; ++i)
    {
        const float f = static_cast<float>(i);
        const engine::vec4 q = o * engine::point(engine::vec3{f * 0.13f - 4.0f,
                                                             f * 0.07f - 2.0f,
                                                             -1.0f - f * 0.3f});
        if (q.w != 1.0f) { w_exact = false; }
    }
    check(w_exact,
          "w is EXACTLY 1.0f for all 64 sample points — not close to 1, equal to "
          "it. That is the bottom row (0,0,0,1), and it is why an orthographic "
          "projection has no perspective divide and why its depth is affine");

    // The eight corners of the box land on the corners of the clip cube.
    const engine::vec4 near_bl = o * engine::point(engine::vec3{-5.0f, -5.0f, -1.0f});
    const engine::vec4 far_tr  = o * engine::point(engine::vec3{ 5.0f,  5.0f, -21.0f});
    checkf(std::fabs(near_bl.x + 1.0f) < 1e-6f && std::fabs(near_bl.y + 1.0f) < 1e-6f
               && std::fabs(near_bl.z) < 1e-6f,
           "the near bottom-left corner lands at (-1, -1, 0): (%.4f, %.4f, %.4f)",
           static_cast<double>(near_bl.x), static_cast<double>(near_bl.y),
           static_cast<double>(near_bl.z));
    checkf(std::fabs(far_tr.x - 1.0f) < 1e-6f && std::fabs(far_tr.y - 1.0f) < 1e-6f
               && std::fabs(far_tr.z - 1.0f) < 1e-6f,
           "the far top-right corner lands at (+1, +1, 1): (%.4f, %.4f, %.4f)",
           static_cast<double>(far_tr.x), static_cast<double>(far_tr.y),
           static_cast<double>(far_tr.z));

    // ---- AFFINE DEPTH, AND WHAT `perspective` DOES INSTEAD ------------------
    //
    // THE PREDICTION THIS LESSON'S PLAN GOT WRONG, corrected here rather than
    // quietly. The plan said the projection's non-linear depth distribution is
    // why shadow error grows with distance — which is true for `perspective`
    // and is exactly NOT true here. An orthographic projection spreads device
    // depth evenly over the box, so a depth code is worth the same number of
    // metres at the near plane and at the far one, and the quantisation half of
    // §F's bias is a CONSTANT. The non-linear story returns the day this light
    // becomes a spot light.
    float ortho_first = 0.0f;
    float ortho_last = 0.0f;
    {
        const float z[4] = {-1.0f, -2.0f, -20.0f, -21.0f};
        float d[4];
        for (int i = 0; i < 4; ++i) { d[i] = (o * engine::point(engine::vec3{0, 0, z[i]})).z; }
        ortho_first = d[1] - d[0];
        ortho_last = d[3] - d[2];
    }
    checkf(std::fabs(ortho_first - ortho_last) < 1e-6f,
           "one metre of depth is worth %.6f of device depth at the near plane "
           "and %.6f at the far plane — THE SAME NUMBER. Orthographic depth is "
           "affine",
           static_cast<double>(ortho_first), static_cast<double>(ortho_last));

    const engine::mat4 pr = engine::perspective(50.0f * 3.14159265f / 180.0f, 1.0f,
                                                1.0f, 21.0f);
    float persp_first = 0.0f;
    float persp_last = 0.0f;
    {
        const float z[4] = {-1.0f, -2.0f, -20.0f, -21.0f};
        float d[4];
        for (int i = 0; i < 4; ++i)
        {
            const engine::vec4 q = pr * engine::point(engine::vec3{0, 0, z[i]});
            d[i] = q.z / q.w;
        }
        persp_first = d[1] - d[0];
        persp_last = d[3] - d[2];
    }
    checkf(persp_first / persp_last > 100.0f,
           "…and under `perspective` the same two metres are worth %.6f and "
           "%.6f — a ratio of %.1fx. That crowding is Lesson 2.10's, measured "
           "in 4.9, and it is the reason a SPOT light's shadow map is harder "
           "than a directional one's",
           static_cast<double>(persp_first), static_cast<double>(persp_last),
           static_cast<double>(persp_first / persp_last));
}

// ===========================================================================
//  §B — THE FIT
// ===========================================================================

void section_b_fit()
{
    std::printf("\n=== B. The fit — a box with no eye to hang it on ===\n");

    engine::aabb box;
    box.expand(engine::vec3{-3.0f, 0.0f, -1.0f});
    box.expand(engine::vec3{ 3.0f, 2.0f,  1.0f});

    checkf(!box.empty() && std::fabs(box.centre().y - 1.0f) < 1e-6f,
           "aabb: centre (%.2f, %.2f, %.2f), extent (%.2f, %.2f, %.2f)",
           static_cast<double>(box.centre().x), static_cast<double>(box.centre().y),
           static_cast<double>(box.centre().z), static_cast<double>(box.extent().x),
           static_cast<double>(box.extent().y), static_cast<double>(box.extent().z));

    // A DEFAULT-CONSTRUCTED BOX IS EMPTY BY BEING INSIDE OUT, which is what
    // removes the first-vertex special case from every caller.
    engine::aabb fresh;
    check(fresh.empty() && fresh.min.x > fresh.max.x,
          "a default aabb is inside out, so `expand` on it yields exactly the "
          "point — no 'is this the first one?' branch anywhere");

    engine::directional_light key;
    key.direction = engine::normalised(engine::vec3{-0.45f, -0.65f, -0.62f});

    const int res = 1024;
    const engine::light_camera cam = engine::fit_directional(key, box, res);
    check(cam.valid(), "fit_directional produced a camera");

    // THE BOX IS SQUARE, which is what earns the phrase "one texel" a single
    // meaning. Checked by pushing the four extreme uv directions through and
    // confirming the same world distance maps to the same NDC distance.
    const engine::vec3 c = box.centre();
    const engine::vec4 c_clip = cam.clip_from_world * engine::point(c);
    const engine::vec4 x_clip =
        cam.clip_from_world * engine::point(c + engine::vec3{cam.world_per_texel, 0, 0});
    const float ndc_per_texel_x = std::fabs(x_clip.x - c_clip.x);
    checkf(std::fabs(ndc_per_texel_x - 2.0f / static_cast<float>(res)) < 1e-5f
               || ndc_per_texel_x < 2.0f / static_cast<float>(res),
           "one world_per_texel step (%.6f world units) spans at most one texel "
           "of NDC (%.6f vs the 2/%d = %.6f a texel is worth)",
           static_cast<double>(cam.world_per_texel), static_cast<double>(ndc_per_texel_x),
           res, 2.0 / res);

    // EVERY CORNER OF THE SCENE IS INSIDE THE BOX. If one were not, the caster
    // would be clipped out of its own shadow map — the failure that looks like
    // "the shadow stops at a straight line for no reason".
    engine::vec3 corners[8];
    box.corners(corners);
    bool all_inside = true;
    float worst_depth_margin = 1.0f;
    for (const engine::vec3& p : corners)
    {
        const engine::vec4 q = cam.clip_from_world * engine::point(p);
        if (std::fabs(q.x) > 1.0f || std::fabs(q.y) > 1.0f || q.z < 0.0f || q.z > 1.0f)
        {
            all_inside = false;
        }
        worst_depth_margin = std::min(worst_depth_margin, std::min(q.z, 1.0f - q.z));
    }
    checkf(all_inside,
           "all 8 scene corners land inside the clip cube, with %.4f of device "
           "depth to spare at the tightest — the pad `fit_directional` adds so "
           "the nearest vertex does not sit exactly ON the near plane, where the "
           "clipper's test is decided by the last bit of a float",
           static_cast<double>(worst_depth_margin));
    check(worst_depth_margin > 0.0f, "…and the margin is strictly positive");

    // THE NEAR PLANE IS BEHIND THE EYE. The light camera's eye is the scene's
    // centre, so half the scene is at positive view-space z — which `perspective`
    // could not express and `orthographic` can, because it has no divide.
    const engine::aabb lv = engine::transformed(box, cam.view_from_world);
    checkf(lv.max.z > 0.0f,
           "in light view space the scene reaches z = %+.3f — POSITIVE, i.e. "
           "behind the eye, so the near plane is negative. `perspective` cannot "
           "take a negative near plane and `orthographic` does not care",
           static_cast<double>(lv.max.z));

    checkf(cam.depth_range > 0.0f && cam.world_per_texel > 0.0f,
           "world_per_texel = %.6f, depth_range = %.4f — the two numbers every "
           "formula in §4 is built from",
           static_cast<double>(cam.world_per_texel),
           static_cast<double>(cam.depth_range));
}

// ===========================================================================
//  A TEST SCENE: one big plane, tilted at a chosen angle to the light
// ===========================================================================

/// A quad in the y = 0 plane, `side` across, as a `mesh_data` in a pool.
struct plane_fixture
{
    engine::mesh_pool meshes;
    engine::mesh_handle quad;
    std::vector<engine::scene_object> objects;

    void build(float side, float y)
    {
        quad = meshes.insert(engine::with_normals(engine::quad_mesh(),
                                                  engine::normal_style::flat));
        engine::scene_object g;
        g.geometry = quad;
        g.name = "plane";
        g.xform.position = {0.0f, y, 0.0f};
        // `transform::rotation` became a quaternion in Lesson 7.4; the matrix
        // is kept as written (a quarter turn about x, laying the quad flat) and
        // converted, so the plane is still visibly the one 6.8 built.
        g.xform.rotation = engine::quat_from_rotation(engine::mat3{{1.0f, 0.0f, 0.0f},
                                                                   {0.0f, 0.0f, -1.0f},
                                                                   {0.0f, 1.0f, 0.0f}});
        g.xform.scale = {side, side, 1.0f};
        g.closed = false;
        objects.push_back(g);
    }

    /// A second quad, `h` above the first — an occluder, for §D.
    void add_occluder(float side, float y)
    {
        engine::scene_object g = objects[0];
        g.name = "occluder";
        g.xform.position = {0.0f, y, 0.0f};
        g.xform.scale = {side, side, 1.0f};
        objects.push_back(g);
    }
};

/// How many of a grid of points on the plane come back SHADOWED.
///
/// The plane is unoccluded, so the correct answer is zero. Anything above zero
/// is acne, and the fraction is the picture's speckle expressed as a number.
double acne_fraction(const engine::shadow_map& map, float side, float y, float n_dot_l,
                     int grid = 96)
{
    int dark = 0;
    int total = 0;
    const engine::vec3 n{0.0f, 1.0f, 0.0f};

    // ---- WHY THE SAMPLE POSITIONS ARE JITTERED, AND IT IS NOT FUSSINESS -----
    //
    // A REGULAR GRID OVER A REGULAR TEXEL GRID MEASURES ALMOST NOTHING. The
    // first version of this function walked a plain 96x96 grid across the plane;
    // its spacing came out at 4.8 shadow texels, so every sample landed at
    // nearly the same place INSIDE its texel and the measurement covered a
    // sliver of the sub-texel space instead of all of it. It reported 3.3% acne
    // where the true figure is 50%, and it reported a worst-case depth error of
    // 22% of the derived bound — a bound that is in fact tight.
    //
    // The fix is a Weyl sequence: step the offset by the golden ratio's
    // fractional part, which is the number hardest to approximate by a rational
    // and therefore the one that never falls back into step with the grid.
    // Two irrational strides, one per axis.
    //
    // This is not a testing footnote. It is the SAME aliasing the shadow map
    // itself suffers from, which is why a shadow's edge crawls when the light
    // moves, and it is the reason cascades snap their box to texel boundaries.
    constexpr double k_phi = 0.61803398874989484820;
    constexpr double k_phi2 = 0.75487766624669276005;   // the plastic number's
    const double texel = static_cast<double>(map.camera().world_per_texel);

    for (int j = 0; j < grid; ++j)
    {
        for (int i = 0; i < grid; ++i)
        {
            const int k = j * grid + i;
            const double jx = std::fmod(static_cast<double>(k) * k_phi, 1.0) * texel;
            const double jy = std::fmod(static_cast<double>(k) * k_phi2, 1.0) * texel;

            const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(grid);
            const float v = (static_cast<float>(j) + 0.5f) / static_cast<float>(grid);
            const engine::vec3 p{(u - 0.5f) * side * 0.9f + static_cast<float>(jx), y,
                                 (v - 0.5f) * side * 0.9f + static_cast<float>(jy)};
            if (map.visibility(p, n, n_dot_l) < 0.999f) { ++dark; }
            ++total;
        }
    }
    return static_cast<double>(dark) / static_cast<double>(total);
}

// ===========================================================================
//  §C — THE DISAGREEMENT, MEASURED
// ===========================================================================

void section_c_acne()
{
    std::printf("\n=== C. The disagreement — acne, predicted and then measured ===\n");

    plane_fixture fx;
    fx.build(10.0f, 0.0f);

    engine::directional_light key;
    key.direction = engine::normalised(engine::vec3{-0.45f, -0.65f, -0.62f});

    // cos(theta) between the plane's normal and the direction to the light.
    const float n_dot_l = engine::dot(engine::vec3{0.0f, 1.0f, 0.0f}, key.to_light());
    const float theta = std::acos(n_dot_l) * 180.0f / 3.14159265f;
    const float tan_theta = std::sqrt(1.0f - n_dot_l * n_dot_l) / n_dot_l;
    checkf(n_dot_l > 0.0f,
           "the test plane meets the light at %.2f degrees, so tan(theta) = %.4f",
           static_cast<double>(theta), static_cast<double>(tan_theta));

    engine::shadow_map map;
    check(map.create(512), "a 512x512 shadow map");
    map.settings().pcf_radius = 0;
    map.settings().bias = engine::shadow_bias::none;

    engine::shadow_stats st;
    map.render(fx.objects, fx.meshes, key, &st);
    checkf(st.triangles == 2,
           "the depth pass rasterised %d triangles into %d texels in %.2f ms — "
           "and it is `collect_triangles` + `draw_triangles`, Modules 2 and 3, "
           "pointed at a different camera",
           st.triangles, st.texels, st.render_ms);

    // ---- THE PREDICTION -----------------------------------------------------
    //
    // A plane has no occluder but itself. The map stores, per texel, the depth of
    // the plane AT THAT TEXEL'S CENTRE; a fragment inside the texel is at a
    // different point on the same plane, and on a tilted plane the two depths
    // differ. Half of every texel is downhill of its own centre, so with no bias
    // roughly HALF the plane should fail its own depth test.
    const double measured = acne_fraction(map, 10.0f, 0.0f, n_dot_l);
    checkf(measured > 0.35 && measured < 0.65,
           "with NO bias, %.1f%% of an unoccluded plane reports itself shadowed. "
           "The prediction was 50%%: half of every texel's footprint lies "
           "downhill of the point the map sampled, so half the plane loses to "
           "itself. THAT IS SHADOW ACNE, and it is arithmetic rather than a "
           "mystery", 100.0 * measured);

    // ---- THE BOUND ----------------------------------------------------------
    //
    // The largest depth error is (half a texel diagonal of lateral travel) times
    // (the surface's depth gradient, tan theta), converted to device units.
    const engine::light_camera& cam = map.camera();
    const float reach = engine::pcf_reach_texels(0);
    const float predicted = reach * cam.world_per_texel * tan_theta / cam.depth_range;

    // Measured directly: walk the plane and record the worst disagreement
    // between a fragment's own depth and the depth stored for its texel.
    float worst = 0.0f;
    const engine::depth_buffer* db = map.depth();
    for (int j = 0; j < 200; ++j)
    {
        for (int i = 0; i < 200; ++i)
        {
            // Jittered, for the reason `acne_fraction` gives at length: a
            // regular grid over a regular texel grid samples one sub-texel
            // position over and over.
            const int k = j * 200 + i;
            const float jx = static_cast<float>(std::fmod(k * 0.61803398875, 1.0))
                           * cam.world_per_texel;
            const float jy = static_cast<float>(std::fmod(k * 0.75487766625, 1.0))
                           * cam.world_per_texel;
            const engine::vec3 p{(static_cast<float>(i) / 199.0f - 0.5f) * 8.0f + jx, 0.0f,
                                 (static_cast<float>(j) / 199.0f - 0.5f) * 8.0f + jy};
            const engine::vec4 clip = cam.clip_from_world * engine::point(p);
            const engine::vec3 s = cam.vp.to_screen(engine::perspective_divide(clip));
            const float stored = db->depth_at(static_cast<int>(std::lround(s.x)),
                                              static_cast<int>(std::lround(s.y)));
            if (stored < 1.0f) { worst = std::max(worst, s.z - stored); }
        }
    }
    checkf(worst <= predicted * 1.05f && worst > predicted * 0.75f,
           "worst measured disagreement %.3e device depth, against a derived "
           "bound of %.3e — the measurement sits at %.0f%% of the bound, which "
           "is what a bound is for. `slope_scaled_bias` returns exactly this "
           "number", static_cast<double>(worst), static_cast<double>(predicted),
           100.0 * static_cast<double>(worst / predicted));

    checkf(std::fabs(engine::slope_scaled_bias(cam.world_per_texel, tan_theta,
                                               cam.depth_range, reach) - predicted) < 1e-9f,
           "…and `slope_scaled_bias` agrees with the hand arithmetic to %.1e",
           static_cast<double>(std::fabs(
               engine::slope_scaled_bias(cam.world_per_texel, tan_theta,
                                         cam.depth_range, reach) - predicted)));

    // THE WORKED EXAMPLE from shadow.hpp, checked.
    const float worked = engine::slope_scaled_bias(0.01953f, 1.7321f, 24.0f,
                                                   engine::pcf_reach_texels(0));
    checkf(std::fabs(worked - 9.966e-4f) < 5e-7f,
           "shadow.hpp's worked example: 0.7071 * 0.01953 * 1.7321 / 24 = %.6e",
           static_cast<double>(worked));
}

// ===========================================================================
//  §D — THE BIAS POLICIES
// ===========================================================================

void section_d_policies()
{
    std::printf("\n=== D. The policies — what each one fixes, and what it then costs ===\n");

    plane_fixture fx;
    fx.build(10.0f, 0.0f);

    engine::directional_light key;
    key.direction = engine::normalised(engine::vec3{-0.45f, -0.65f, -0.62f});
    const float n_dot_l = engine::dot(engine::vec3{0.0f, 1.0f, 0.0f}, key.to_light());

    engine::shadow_map map;
    (void)map.create(512);
    map.settings().pcf_radius = 0;
    map.render(fx.objects, fx.meshes, key, nullptr);

    map.settings().bias = engine::shadow_bias::none;
    const double none = acne_fraction(map, 10.0f, 0.0f, n_dot_l);

    map.settings().bias = engine::shadow_bias::slope_scaled;
    const double slope = acne_fraction(map, 10.0f, 0.0f, n_dot_l);

    map.settings().bias = engine::shadow_bias::normal_offset;
    const double normal = acne_fraction(map, 10.0f, 0.0f, n_dot_l);

    checkf(slope < 0.001,
           "slope-scaled: %.1f%% acne, down from %.1f%%. The bias each surface "
           "gets is the one its own geometry requires and no more",
           100.0 * slope, 100.0 * none);
    checkf(normal < 0.05,
           "normal-offset: %.2f%% acne. It moves the SAMPLE POINT along the "
           "geometric normal rather than moving its depth, and 6.7 is why that "
           "distinction now exists — a normal map does not move a triangle",
           100.0 * normal);

    // ---- A CONSTANT BIAS CANNOT COVER TWO ANGLES ---------------------------
    //
    // Size it for a 50-degree surface and a 75-degree one still speckles, because
    // the error is proportional to tan(theta) and the bias is not. That is the
    // whole argument against a constant, and it is one number.
    const engine::light_camera& cam = map.camera();
    const float tan50 = std::tan(50.0f * 3.14159265f / 180.0f);
    const float tan75 = std::tan(75.0f * 3.14159265f / 180.0f);
    const float sized_for_50 = engine::slope_scaled_bias(cam.world_per_texel, tan50,
                                                         cam.depth_range,
                                                         engine::pcf_reach_texels(0));
    const float needed_at_75 = engine::slope_scaled_bias(cam.world_per_texel, tan75,
                                                         cam.depth_range,
                                                         engine::pcf_reach_texels(0));
    checkf(needed_at_75 / sized_for_50 > 3.0f,
           "a constant bias sized for a 50-degree surface is %.1fx too small at "
           "75 degrees (%.3e needed against %.3e supplied). Size it for 75 "
           "instead and every flatter surface pays a bias it did not need — "
           "which is peter-panning",
           static_cast<double>(needed_at_75 / sized_for_50),
           static_cast<double>(needed_at_75), static_cast<double>(sized_for_50));

    // ---- PETER-PANNING, QUANTIFIED -----------------------------------------
    //
    // A depth bias lifts the receiver toward the light by `bias * depth_range`
    // WORLD UNITS. Any occluder closer than that stops occluding, so a shadow
    // detaches wherever the caster is nearer to the receiver than the bias. The
    // prediction is exact and this measures it: put a quad at a known height and
    // find the height at which its shadow disappears.
    plane_fixture two;
    two.build(10.0f, 0.0f);
    two.add_occluder(2.0f, 0.35f);

    engine::shadow_map m2;
    (void)m2.create(512);
    m2.settings().pcf_radius = 0;
    m2.settings().bias = engine::shadow_bias::constant;
    m2.render(two.objects, two.meshes, key, nullptr);

    const float range = m2.camera().depth_range;
    const engine::vec3 under{0.0f, 0.0f, 0.0f};
    const engine::vec3 up{0.0f, 1.0f, 0.0f};

    // The occluder is 0.35 above the receiver, but the gap ALONG THE LIGHT RAY
    // is longer than the vertical gap by 1/cos(theta).
    const float gap_along_ray = 0.35f / n_dot_l;

    m2.settings().constant_bias = 0.5f * gap_along_ray / range;
    const float half_bias = m2.visibility(under, up, n_dot_l);

    m2.settings().constant_bias = 1.5f * gap_along_ray / range;
    const float over_bias = m2.visibility(under, up, n_dot_l);

    checkf(half_bias < 0.5f && over_bias > 0.5f,
           "a point under a caster 0.35 above it (%.3f along the light ray) is "
           "SHADOWED at a bias worth half that gap (v = %.2f) and LIT at a bias "
           "worth 1.5x it (v = %.2f). Peter-panning is not vague: a bias of b "
           "device units unshadows everything whose caster is within "
           "b * depth_range = %.3f world units",
           static_cast<double>(gap_along_ray), static_cast<double>(half_bias),
           static_cast<double>(over_bias),
           static_cast<double>(1.5f * gap_along_ray));

    // ---- FRONT-FACE CULLING, THE THIRD CURE --------------------------------
    //
    // Store only the BACK faces of casters and every front face being shaded is
    // compared against a surface genuinely behind it. It is free and it deletes
    // a ground plane, which has no back face to store — so the map comes out
    // EMPTY and the whole scene reports lit.
    engine::shadow_map m3;
    (void)m3.create(256);
    m3.settings().cull = engine::cull_mode::front;
    m3.settings().bias = engine::shadow_bias::none;
    m3.render(fx.objects, fx.meshes, key, nullptr);
    const double culled = acne_fraction(m3, 10.0f, 0.0f, n_dot_l);
    checkf(culled < 0.001,
           "with FRONT-face culling the plane's acne is %.2f%% — because the "
           "plane's only face was culled and the map is empty. On a closed "
           "caster this is a real and free cure; on a sheet it is the same as "
           "having no shadow at all, which is why the default is `none`",
           100.0 * culled);
}

// ===========================================================================
//  §E — PCF
// ===========================================================================

void section_e_pcf()
{
    std::printf("\n=== E. PCF — compare, then filter, and the reach that costs ===\n");

    // ---- THE ORDER, IN FOUR NUMBERS ----------------------------------------
    //
    // Two occluders at 0.3 and 0.9, a receiver at 0.5.
    const float a = 0.3f;
    const float b = 0.9f;
    const float receiver = 0.5f;

    const float filter_then_compare = ((a + b) * 0.5f >= receiver) ? 1.0f : 0.0f;
    const float compare_then_filter = (((a >= receiver) ? 1.0f : 0.0f)
                                       + ((b >= receiver) ? 1.0f : 0.0f)) * 0.5f;

    checkf(filter_then_compare == 1.0f,
           "FILTER THEN COMPARE: mean depth (0.3 + 0.9)/2 = 0.6, and 0.6 > 0.5, "
           "so the receiver is reported FULLY LIT — with half its taps occluded. "
           "Not blurrier. Wrong");
    checkf(compare_then_filter == 0.5f,
           "COMPARE THEN FILTER: 0.3 < 0.5 gives 0, 0.9 > 0.5 gives 1, mean 0.5 "
           "— HALF SHADOWED, which is the answer. The values being averaged have "
           "to be visibilities, because visibility is what we want the average "
           "of. That is the entire reason `SamplerComparisonState` exists");

    // ---- THE REACH ---------------------------------------------------------
    check(std::fabs(engine::pcf_reach_texels(0) - 0.70710678f) < 1e-6f,
          "one tap reaches sqrt(2)/2 = 0.7071 texels — half a texel diagonal");
    checkf(std::fabs(engine::pcf_reach_texels(1) - 2.1213203f) < 1e-6f,
           "a 3x3 kernel reaches %.4f texels — THREE TIMES as far, because its "
           "furthest tap is 1.5 texels away on each axis",
           static_cast<double>(engine::pcf_reach_texels(1)));

    // ---- AND WHAT FORGETTING IT COSTS --------------------------------------
    //
    // THE FINDING THIS LESSON MADE BY RENDERING. A bias sized for one tap leaves
    // two thirds of a 3x3 kernel's error uncovered, and the acne arrives at the
    // same moment PCF does — so it looks like a filtering bug.
    plane_fixture fx;
    fx.build(10.0f, 0.0f);

    engine::directional_light key;
    key.direction = engine::normalised(engine::vec3{-0.45f, -0.65f, -0.62f});
    const float n_dot_l = engine::dot(engine::vec3{0.0f, 1.0f, 0.0f}, key.to_light());

    engine::shadow_map map;
    (void)map.create(512);
    map.settings().bias = engine::shadow_bias::slope_scaled;
    map.render(fx.objects, fx.meshes, key, nullptr);

    map.settings().pcf_radius = 0;
    const double one_tap = acne_fraction(map, 10.0f, 0.0f, n_dot_l);

    map.settings().pcf_radius = 1;
    const double nine_tap = acne_fraction(map, 10.0f, 0.0f, n_dot_l);

    // The same 3x3 kernel with the bias deliberately sized for ONE tap: the bug.
    map.settings().slope_scale = engine::pcf_reach_texels(0) / engine::pcf_reach_texels(1);
    const double nine_tap_wrong = acne_fraction(map, 10.0f, 0.0f, n_dot_l);
    map.settings().slope_scale = 1.0f;

    checkf(one_tap < 0.001 && nine_tap < 0.001,
           "1 tap: %.2f%% acne. 3x3 with the reach-corrected bias: %.2f%%",
           100.0 * one_tap, 100.0 * nine_tap);
    checkf(nine_tap_wrong > 0.10,
           "3x3 with the bias sized for ONE tap: %.1f%% acne — the artefact "
           "walks straight back in the moment the kernel widens, and it looks "
           "like a filtering bug. The bias has to scale with the kernel's reach",
           100.0 * nine_tap_wrong);
}

// ===========================================================================
//  §F — PRECISION
// ===========================================================================

void section_f_precision()
{
    std::printf("\n=== F. Precision — half a code, and the easy case ===\n");

    checkf(engine::quantisation_bias(engine::depth_format::unorm16) > 7.6e-6f
               && engine::quantisation_bias(engine::depth_format::unorm16) < 7.7e-6f,
           "a 16-bit map is wrong by up to %.4e of device depth before any "
           "geometry is involved — half of one of its 65,535 codes",
           static_cast<double>(engine::quantisation_bias(engine::depth_format::unorm16)));
    checkf(engine::quantisation_bias(engine::depth_format::unorm24) < 3.0e-8f,
           "a 24-bit map: %.4e — 256 times finer, and below every slope term "
           "this engine produces",
           static_cast<double>(engine::quantisation_bias(engine::depth_format::unorm24)));
    check(engine::quantisation_bias(engine::depth_format::f32) == 0.0f,
          "f32 is reported as 0, which is an honest simplification rather than a "
          "claim: a float's grid is not uniform, and near the values a depth "
          "buffer holds it is around 6e-8 — two orders of magnitude below the "
          "slope term at every angle this engine can rasterise");

    // WHAT A 16-BIT MAP COSTS, IN METRES. The half-code is a constant fraction
    // of the depth range, so on a 20-metre box it is 1.5e-4 metres — and the
    // reason it is a CONSTANT rather than growing with distance is §A's finding:
    // orthographic depth is affine.
    const float range = 20.0f;
    const float world = engine::quantisation_bias(engine::depth_format::unorm16) * range;
    checkf(world > 1.4e-4f && world < 1.6e-4f,
           "over a 20-metre box that half-code is %.5f metres — a seventh of a "
           "millimetre, EVERYWHERE in the box. A perspective shadow map's "
           "half-code is worth a hundred times more at the far plane than at the "
           "near one", static_cast<double>(world));

    // slope_from_cosine, at the two ends and at the clamp.
    check(engine::slope_from_cosine(1.0f, 10.0f) == 0.0f,
          "a surface facing the light square-on has slope 0 — and needs no bias "
          "at all, which is exactly what a constant one cannot express");
    check(engine::slope_from_cosine(-0.5f, 10.0f) == 0.0f,
          "a surface facing AWAY returns 0: it is in shadow by its own geometry "
          "(3.6's clamp) and the map is never consulted");
    checkf(engine::slope_from_cosine(0.01f, 10.0f) == 10.0f,
           "…and at 89.4 degrees the slope clamps to %.1f. At exactly grazing "
           "incidence the required bias is infinite, and the honest answer is "
           "that a shadow map cannot resolve that surface — the clamp is where "
           "we stop pretending",
           static_cast<double>(engine::slope_from_cosine(0.01f, 10.0f)));
}

// ===========================================================================
//  §G — CPU vs GPU
// ===========================================================================

void section_g_gpu()
{
    std::printf("\n=== G. CPU vs GPU — the same shadow, computed twice ===\n");

    // SDL_INIT_VIDEO FIRST. `SDL_CreateGPUDevice` needs the video subsystem even
    // with no window, and without it the device simply does not come back — a
    // skipped section rather than a failing one, which is the most misleading
    // kind of silence a harness can produce.
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::printf("  ---- SDL_Init(VIDEO): %s\n", SDL_GetError());
    }

    engine::gpu_device gpu;
    if (!gpu.create(nullptr, false).ok())
    {
        std::printf("  ---- no GPU device on this machine; §G skipped\n");
        return;
    }

    static constexpr SDL_GPUTextureFormat k_want[] = {
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    };
    const SDL_GPUTextureFormat fmt = engine::supported_shadow_format(gpu, k_want, 3);
    checkf(fmt != SDL_GPU_TEXTUREFORMAT_INVALID,
           "this device accepts %s as a depth target that can ALSO be sampled — "
           "asked with `supported_shadow_format`, because SDL guarantees a depth "
           "format only as an attachment and guarantees nothing about sampling "
           "one", engine::name_of(fmt));
    if (fmt == SDL_GPU_TEXTUREFORMAT_INVALID) { gpu.destroy(); return; }

    engine::gpu_shader vs;
    engine::gpu_shader fs;
    const bool loaded = vs.load(gpu, "shadow.vert", engine::shader_stage::vertex)
                     && fs.load(gpu, "shadow.frag", engine::shader_stage::fragment);
    check(loaded, "shadow.vert and shadow.frag loaded (the depth pass's shaders)");
    if (!loaded) { gpu.destroy(); return; }

    engine::gpu_shadow_map map;
    check(map.create(gpu, vs.handle(), fs.handle(), 512, fmt),
          "the depth-only pipeline was created: num_color_targets = 0, "
          "has_depth_stencil_target = true, one vertex attribute out of four");

    // ---- The uniform block both renderers fill -----------------------------
    engine::aabb box;
    box.expand(engine::vec3{-5.0f, 0.0f, -5.0f});
    box.expand(engine::vec3{ 5.0f, 2.0f,  5.0f});

    engine::directional_light key;
    key.direction = engine::normalised(engine::vec3{-0.45f, -0.65f, -0.62f});

    const engine::light_camera cam = engine::fit_directional(key, box, 512);

    engine::shadow_settings set;
    set.pcf_radius = 1;

    engine::scene_light_uniforms light{};
    engine::gpu_shadow_map::fill_uniforms(light, cam, set, 512);

    checkf(std::memcmp(&light.light_clip_from_world, &cam.clip_from_world,
                       sizeof(engine::mat4)) == 0,
           "the light's matrix crosses into the uniform block byte for byte — "
           "the same `memcpy`-is-the-whole-conversion claim 4.6 measured for the "
           "camera's");
    // REPAIRED 2026-09-28: the GPU's reach is `gpu_pcf_reach_texels`, not the
    // CPU's `pcf_reach_texels` — its lookup compares a 2x2 block per tap, and
    // with one tap the CPU's number left a third of a bare ground as acne on the
    // GPU (verify_617b §K). The claim below is otherwise unchanged.
    checkf(light.shadow_reach == engine::gpu_pcf_reach_texels(set.pcf_radius)
               && light.shadow_texel == cam.world_per_texel
               && light.shadow_depth_range == cam.depth_range,
           "…and the eleven shadow floats come from ONE function, so the two "
           "renderers cannot disagree about what a bias means (the reach is the "
           "GPU's own, for its 2x2 comparison): reach %.4f, texel %.6f, range %.4f",
           static_cast<double>(light.shadow_reach),
           static_cast<double>(light.shadow_texel),
           static_cast<double>(light.shadow_depth_range));
    checkf(light.shadow_mode == 2.0f,
           "the bias mode crosses as a NUMBER (%.0f = slope-scaled), spelled out "
           "rather than cast from the enum — 6.4's rule, so reordering the enum "
           "cannot silently re-map the shader",
           static_cast<double>(light.shadow_mode));

    // ---- The pass actually records ----------------------------------------
    engine::mesh_pool meshes;
    const engine::mesh_data quad = engine::with_normals(engine::quad_mesh(),
                                                        engine::normal_style::flat);
    const engine::mesh_handle h = meshes.insert(quad);

    engine::gpu_mesh gm;
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
        const bool up = cb != nullptr
                     && gm.create(gpu, cb, meshes.get(h)->view(), engine::index_mode::indexed,
                                   "shadow test quad")
                     && SDL_SubmitGPUCommandBuffer(cb);
        check(up, "one quad uploaded as a gpu_mesh");
        if (!up) { map.destroy(); gpu.destroy(); return; }
    }

    engine::gpu_draw_item item;
    item.mesh = &gm;
    item.world_from_model = engine::parent_from_local(engine::transform{});

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
    check(cb != nullptr, "a command buffer for the depth pass");
    if (cb != nullptr)
    {
        map.render(cb, &item, 1, cam, nullptr);
        SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
        check(fence != nullptr,
              "the depth-only pass submitted: BeginGPURenderPass with a null "
              "colour array and a count of ZERO, which is the thing "
              "`fill_style::depth_only` cannot say on the CPU");
        if (fence != nullptr)
        {
            SDL_WaitForGPUFences(gpu.handle(), true, &fence, 1);
            SDL_ReleaseGPUFence(gpu.handle(), fence);
        }
    }

    check(map.texture() != nullptr && map.sampler() != nullptr,
          "the map is bindable: a depth texture with SAMPLER usage, and a "
          "comparison sampler — the pair `gpu_scene_renderer::render` puts at "
          "fragment slot 2");

    gm.destroy();
    map.destroy();
    vs.destroy();
    fs.destroy();
    gpu.destroy();
}

// ===========================================================================
//  §H — THE GOLDEN
// ===========================================================================

void section_h_golden()
{
    std::printf("\n=== H. The golden — a new capability must not move the old picture ===\n");

    const int rc = demo::write_reference_shot("scratch/verify68.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify68.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL, hash E917C06C — SEVENTEENTH lesson at this hash. This "
          "one added a second render pass, a projection, an AABB type, a "
          "nullable pointer to `fill_style`, a `visibility` factor to `shade()` "
          "and 112 bytes to the fragment uniform block — and moved not one "
          "pixel, because the reference scene binds no shadow map and the "
          "factor defaults to 1. A NEW CAPABILITY IS A NEW PATH");
}

}   // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::printf("verify_68 — Lesson 6.8: shadow mapping, bias, acne and PCF\n");

    section_a_orthographic();
    section_b_fit();
    section_c_acne();
    section_d_policies();
    section_e_pcf();
    section_f_precision();
    section_g_gpu();
    section_h_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
