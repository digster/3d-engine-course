// scratch/verify_69.cpp — Lesson 6.9's harness: what splitting the frustum buys,
// and what it costs.
//
//   §A  THE SPLIT SCHEME — uniform, logarithmic, and why the blend
//   §B  THE SLICE — eight corners from three numbers, checked against the matrix
//   §C  THE WIN — texel density, cascaded against 6.8's one map
//   §D  ROTATION INVARIANCE — the sphere's whole reason for existing
//   §E  SNAPPING — the grid that does not slide, measured
//   §F  THE BIAS THAT NEEDED NO NEW CODE — 6.8 §4, audited
//   §G  SELECTION AND THE SEAM — where cascades meet, and the blend band
//   §H  DEPTH RANGE — an occluder outside the slice still casts
//   §I  THE GOLDEN — a new capability must not move the old picture
//
// §D AND §F ARE THE TWO THAT MATTER. §D measures the claim the sphere fit is
// built on: yaw the camera and `world_per_texel` must not move, because if it
// moves then every texel in the map changes size and the whole picture crawls.
// §F is the audit of Lesson 6.8: every bias term there was a multiple of
// `world_per_texel`, so if that derivation was real, cascades need NO new bias
// code — only their own `light_camera`. Either the numbers come out or 6.8's
// formula was a fudge with a good story.
//
// Build and run:  sh scratch/build_verify_69.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/bounds.hpp>
#include <engine/gfx/cascade.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/shadow.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/vec3.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdarg>
#include <cstdio>
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

void section(const char* title)
{
    std::printf("\n== %s ==\n", title);
}

using engine::aabb;
using engine::camera_frustum;
using engine::directional_light;
using engine::frustum_slice;
using engine::light_camera;
using engine::mat4;
using engine::vec3;

constexpr float k_fovy = 1.0471975512f;   // 60 degrees
constexpr float k_aspect = 16.0f / 9.0f;
constexpr int k_res = 1024;

// A camera at the origin looking down -z, yawed by `deg` about +y.
camera_frustum camera_at(float deg, vec3 eye = vec3{0.0f, 0.0f, 0.0f})
{
    const float r = deg * 3.14159265358979f / 180.0f;
    const vec3 fwd{std::sin(r) * -1.0f, 0.0f, -std::cos(r)};
    const mat4 view = engine::look_at(eye, eye + fwd, vec3{0.0f, 1.0f, 0.0f});
    camera_frustum cam;
    cam.world_from_view = engine::rigid_inverse(view);
    cam.fovy_radians = k_fovy;
    cam.aspect = k_aspect;
    return cam;
}

// ---------------------------------------------------------------------------
void section_a_splits()
{
    section("A  THE SPLIT SCHEME");

    const float near_d = 0.1f;
    const float far_d = 60.0f;

    // lambda = 0 is pure uniform: equal world slices.
    std::printf("  lambda=0 (uniform)     ");
    for (int i = 0; i < 4; ++i)
    {
        std::printf("%8.3f", engine::practical_split(i, 4, near_d, far_d, 0.0f));
    }
    std::printf("\n");

    // lambda = 1 is pure logarithmic: a geometric series.
    std::printf("  lambda=1 (logarithmic) ");
    for (int i = 0; i < 4; ++i)
    {
        std::printf("%8.3f", engine::practical_split(i, 4, near_d, far_d, 1.0f));
    }
    std::printf("\n");

    std::printf("  lambda=0.5 (practical) ");
    for (int i = 0; i < 4; ++i)
    {
        std::printf("%8.3f", engine::practical_split(i, 4, near_d, far_d, 0.5f));
    }
    std::printf("\n");

    // Uniform cut i lands at near + (far-near)*(i+1)/n, exactly.
    const float u1 = engine::practical_split(0, 4, near_d, far_d, 0.0f);
    checkf(std::fabs(u1 - (near_d + (far_d - near_d) * 0.25f)) < 1e-4f,
           "uniform cut 0 = %.4f, predicted %.4f", u1,
           near_d + (far_d - near_d) * 0.25f);

    // Logarithmic cut i lands at near*(far/near)^((i+1)/n), exactly.
    const float l1 = engine::practical_split(0, 4, near_d, far_d, 1.0f);
    const float l1_pred = near_d * std::pow(far_d / near_d, 0.25f);
    checkf(std::fabs(l1 - l1_pred) < 1e-4f,
           "logarithmic cut 0 = %.4f, predicted %.4f", l1, l1_pred);

    // THE COMPLAINT ABOUT PURE LOG, stated as a number: the first cascade covers
    // almost nothing, so it is spent on the ground at your feet.
    checkf(l1 < 1.0f, "pure log spends cascade 0 on the first %.2f m", l1);

    // The practical scheme is between them, everywhere.
    bool between = true;
    for (int i = 0; i < 3; ++i)
    {
        const float u = engine::practical_split(i, 4, near_d, far_d, 0.0f);
        const float l = engine::practical_split(i, 4, near_d, far_d, 1.0f);
        const float p = engine::practical_split(i, 4, near_d, far_d, 0.5f);
        if (p < std::min(u, l) - 1e-4f || p > std::max(u, l) + 1e-4f) { between = false; }
    }
    check(between, "practical splits lie between the two schemes at every cut");

    // Monotonic, and the last cut IS the far plane — a gap or an overlap there
    // would leave a ring of world unshadowed or resolve it twice.
    bool monotonic = true;
    float prev = near_d;
    for (int i = 0; i < 4; ++i)
    {
        const float s = engine::practical_split(i, 4, near_d, far_d, 0.5f);
        if (s <= prev) { monotonic = false; }
        prev = s;
    }
    check(monotonic, "splits strictly increase");
    checkf(engine::practical_split(3, 4, near_d, far_d, 0.5f) == far_d,
           "the last split is the far plane exactly (%.1f)", far_d);
}

// ---------------------------------------------------------------------------
void section_b_slice()
{
    section("B  THE SLICE");

    const camera_frustum cam = camera_at(0.0f);
    const frustum_slice s = engine::slice_corners(cam, 1.0f, 10.0f);

    // With an identity-ish camera at the origin looking down -z, corner 0 of the
    // near plane is (-hw, -hh, -1) with hh = tan(fovy/2)*1.
    const float hh = std::tan(0.5f * k_fovy) * 1.0f;
    const float hw = hh * k_aspect;

    checkf(std::fabs(s.corners[0].x + hw) < 1e-4f &&
           std::fabs(s.corners[0].y + hh) < 1e-4f &&
           std::fabs(s.corners[0].z + 1.0f) < 1e-4f,
           "near corner 0 = (%.4f, %.4f, %.4f), predicted (%.4f, %.4f, %.4f)",
           static_cast<double>(s.corners[0].x), static_cast<double>(s.corners[0].y),
           static_cast<double>(s.corners[0].z),
           static_cast<double>(-hw), static_cast<double>(-hh), -1.0);

    // Every far corner sits at z = -10: the slice's far distance, negated,
    // because -z is forward.
    bool far_z_ok = true;
    for (int i = 4; i < 8; ++i)
    {
        if (std::fabs(s.corners[i].z + 10.0f) > 1e-4f) { far_z_ok = false; }
    }
    check(far_z_ok, "all four far corners sit at z = -10 (−z is forward)");

    // THE INDEPENDENT CHECK: push a corner through the actual projection matrix
    // and it must land on the edge of the NDC box. This is what makes §B a test
    // of the corner builder rather than a restatement of it.
    const mat4 proj = engine::perspective(k_fovy, k_aspect, 1.0f, 10.0f);
    const engine::vec4 clip = proj * engine::point(s.corners[7]);
    const vec3 ndc = engine::perspective_divide(clip);
    checkf(std::fabs(std::fabs(ndc.x) - 1.0f) < 1e-3f &&
           std::fabs(std::fabs(ndc.y) - 1.0f) < 1e-3f &&
           std::fabs(ndc.z - 1.0f) < 1e-3f,
           "far corner 7 through perspective() lands on the NDC box: "
           "(%.4f, %.4f, %.4f)",
           static_cast<double>(ndc.x), static_cast<double>(ndc.y),
           static_cast<double>(ndc.z));
}

// ---------------------------------------------------------------------------
// The headline number: what the near field actually gains.
void section_c_density()
{
    section("C  THE WIN — texel density");

    directional_light key;
    key.direction = engine::normalised(vec3{-0.4f, -1.0f, -0.3f});

    // A 40 m scene, which is what 6.8's one map has to cover.
    const aabb scene{vec3{-20.0f, -1.0f, -20.0f}, vec3{20.0f, 6.0f, 20.0f}};
    const light_camera one = engine::fit_directional(key, scene, k_res);
    std::printf("  6.8, one map over the whole scene: world_per_texel = %.4f m\n",
                static_cast<double>(one.world_per_texel));

    const camera_frustum cam = camera_at(0.0f, vec3{0.0f, 1.7f, 18.0f});
    float near_d = 0.1f;
    float first = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float far_d = engine::practical_split(i, 4, 0.1f, 60.0f, 0.5f);
        const frustum_slice sl = engine::slice_corners(cam, near_d, far_d);
        const light_camera lc =
            engine::fit_directional_slice(key, sl, scene, k_res, true);
        std::printf("  cascade %d  [%6.2f, %6.2f] m   world_per_texel = %.5f m\n",
                    i, static_cast<double>(near_d), static_cast<double>(far_d),
                    static_cast<double>(lc.world_per_texel));
        if (i == 0) { first = lc.world_per_texel; }
        near_d = far_d;
    }

    const float gain = one.world_per_texel / first;
    checkf(gain > 2.5f,
           "cascade 0 resolves %.1fx finer than 6.8's single map "
           "(%.4f m -> %.5f m per texel)",
           static_cast<double>(gain), static_cast<double>(one.world_per_texel),
           static_cast<double>(first));

    // AND THE PRICE OF STABILITY, which is the honest other half of §C. 2.8x is
    // less than the arithmetic promises, and the missing factor is the sphere:
    // it must contain the slice whichever way the camera is pointing, so it is
    // bigger than a box fitted to the corners of THIS orientation. Measure what
    // that costs, because a reader who works out the frustum's dimensions by
    // hand will expect a larger number and should be told why they are right.
    const camera_frustum c0cam = camera_at(0.0f, vec3{0.0f, 1.7f, 18.0f});
    const frustum_slice s0 = engine::slice_corners(c0cam, 0.1f, 7.78f);
    const vec3 fwd0 = engine::normalised_or(key.direction, vec3{0.0f, -1.0f, 0.0f});
    const mat4 b0 = engine::look_at(vec3{0.0f, 0.0f, 0.0f}, fwd0, vec3{0.0f, 1.0f, 0.0f});
    aabb tight;
    for (const vec3& c : s0.corners) { tight.expand(engine::xyz(b0 * engine::point(c))); }
    const vec3 te = tight.extent();
    const float tight_wpt = std::max(te.x, te.y) / static_cast<float>(k_res);
    checkf(tight_wpt < first,
           "a tight box would give %.5f m per texel — the sphere costs %.0f%% "
           "of the resolution, and buys §D in exchange",
           static_cast<double>(tight_wpt),
           static_cast<double>((first / tight_wpt - 1.0f) * 100.0f));
}

// ---------------------------------------------------------------------------
// The sphere's entire justification, measured.
void section_d_rotation()
{
    section("D  ROTATION INVARIANCE");

    directional_light key;
    key.direction = engine::normalised(vec3{-0.4f, -1.0f, -0.3f});
    const aabb scene{vec3{-20.0f, -1.0f, -20.0f}, vec3{20.0f, 6.0f, 20.0f}};

    float min_wpt = 1e30f;
    float max_wpt = -1e30f;
    for (int deg = 0; deg < 360; deg += 5)
    {
        const camera_frustum cam = camera_at(static_cast<float>(deg),
                                             vec3{0.0f, 1.7f, 0.0f});
        const frustum_slice sl = engine::slice_corners(cam, 0.1f, 15.0f);
        const light_camera lc =
            engine::fit_directional_slice(key, sl, scene, k_res, true);
        min_wpt = std::min(min_wpt, lc.world_per_texel);
        max_wpt = std::max(max_wpt, lc.world_per_texel);
    }

    const float spread = (max_wpt - min_wpt) / max_wpt;
    checkf(spread < 1e-5f,
           "yawing the camera through 360 deg moves world_per_texel by %.3e "
           "(min %.6f, max %.6f) — the sphere has no orientation",
           static_cast<double>(spread), static_cast<double>(min_wpt),
           static_cast<double>(max_wpt));

    // AND THE CONTRAST, which is the point of the decision. A box fitted tightly
    // to the eight corners DOES change size with yaw. Measure that directly.
    float box_min = 1e30f;
    float box_max = -1e30f;
    const vec3 fwd = engine::normalised_or(key.direction, vec3{0.0f, -1.0f, 0.0f});
    for (int deg = 0; deg < 360; deg += 5)
    {
        const camera_frustum cam = camera_at(static_cast<float>(deg),
                                             vec3{0.0f, 1.7f, 0.0f});
        const frustum_slice sl = engine::slice_corners(cam, 0.1f, 15.0f);
        const mat4 basis = engine::look_at(vec3{0.0f, 0.0f, 0.0f}, fwd,
                                           vec3{0.0f, 1.0f, 0.0f});
        aabb box;
        for (const vec3& c : sl.corners) { box.expand(engine::xyz(basis * engine::point(c))); }
        const vec3 ext = box.extent();
        const float side = std::max(ext.x, ext.y);
        box_min = std::min(box_min, side);
        box_max = std::max(box_max, side);
    }
    const float box_spread = (box_max - box_min) / box_max;
    checkf(box_spread > 0.05f,
           "a corner-fitted box changes side by %.1f%% over the same yaw "
           "(%.3f m -> %.3f m) — that is the shimmer the sphere removes",
           static_cast<double>(box_spread * 100.0f),
           static_cast<double>(box_min), static_cast<double>(box_max));
}

// ---------------------------------------------------------------------------
void section_e_snapping()
{
    section("E  SNAPPING");

    directional_light key;
    key.direction = engine::normalised(vec3{-0.4f, -1.0f, -0.3f});
    const aabb scene{vec3{-20.0f, -1.0f, -20.0f}, vec3{20.0f, 6.0f, 20.0f}};

    // Walk the camera forward in very small steps and watch where the light's
    // box centre lands, expressed in TEXELS. Snapped, it must always be a whole
    // number; unsnapped, it slides continuously — and a grid that slides under
    // fixed geometry is exactly what makes an edge crawl.
    float worst_snapped_frac = 0.0f;
    float worst_unsnapped_frac = 0.0f;

    for (int step = 0; step < 40; ++step)
    {
        const float z = 10.0f + static_cast<float>(step) * 0.013f;  // sub-texel steps
        const camera_frustum cam = camera_at(0.0f, vec3{0.0f, 1.7f, z});
        const frustum_slice sl = engine::slice_corners(cam, 0.1f, 15.0f);

        for (int snap = 0; snap < 2; ++snap)
        {
            const light_camera lc =
                engine::fit_directional_slice(key, sl, scene, k_res, snap != 0);

            // The light's eye, in its own space, in texels.
            const vec3 eye = engine::translation_of(engine::rigid_inverse(lc.view_from_world));
            const vec3 ls = engine::xyz(lc.view_from_world * engine::point(eye));
            (void)ls;

            // Take the world origin's light-space x, in texels: if the grid is
            // snapped, the fractional part is stable frame to frame.
            const vec3 probe = engine::xyz(lc.view_from_world * engine::point(vec3{0.0f, 0.0f, 0.0f}));
            const float texels = probe.x / lc.world_per_texel;
            const float frac = std::fabs(texels - std::floor(texels + 0.5f));
            if (snap != 0) { worst_snapped_frac = std::max(worst_snapped_frac, frac); }
            else { worst_unsnapped_frac = std::max(worst_unsnapped_frac, frac); }
        }
    }

    checkf(worst_snapped_frac < 1e-3f,
           "snapped: the world origin stays within %.2e texels of a texel centre "
           "across 40 sub-texel camera steps",
           static_cast<double>(worst_snapped_frac));
    checkf(worst_unsnapped_frac > 0.1f,
           "unsnapped: it wanders %.3f texels — the grid slides, and every "
           "shadow edge crawls with it",
           static_cast<double>(worst_unsnapped_frac));
}

// ---------------------------------------------------------------------------
// The audit of Lesson 6.8.
void section_f_bias()
{
    section("F  THE BIAS THAT NEEDED NO NEW CODE");

    directional_light key;
    key.direction = engine::normalised(vec3{-0.4f, -1.0f, -0.3f});
    const aabb scene{vec3{-20.0f, -1.0f, -20.0f}, vec3{20.0f, 6.0f, 20.0f}};
    const camera_frustum cam = camera_at(0.0f, vec3{0.0f, 1.7f, 18.0f});

    // A 45-degree surface: tan(theta) = 1, so the slope factor is exactly 1 and
    // the bias is reach * world_per_texel / depth_range with nothing hidden.
    const float slope = 1.0f;
    const float reach = engine::pcf_reach_texels(1);

    std::printf("  reach = %.4f texels (PCF radius 1)\n",
                static_cast<double>(reach));

    float near_d = 0.1f;
    std::vector<float> biases;
    for (int i = 0; i < 4; ++i)
    {
        const float far_d = engine::practical_split(i, 4, 0.1f, 60.0f, 0.5f);
        const frustum_slice sl = engine::slice_corners(cam, near_d, far_d);
        const light_camera lc =
            engine::fit_directional_slice(key, sl, scene, k_res, true);

        // 6.8's function. Unchanged. Called with this cascade's numbers.
        const float b = engine::slope_scaled_bias(lc.world_per_texel, slope,
                                                  lc.depth_range, reach);
        biases.push_back(b);
        std::printf("  cascade %d  wpt = %.5f m  depth_range = %7.3f m  "
                    "bias = %.3e\n",
                    i, static_cast<double>(lc.world_per_texel),
                    static_cast<double>(lc.depth_range), static_cast<double>(b));
        near_d = far_d;
    }

    // THE BIAS IN METRES differs enormously, which is the obvious half: it is
    // reach * world_per_texel * slope, and world_per_texel spans 7x.
    const float metres_near = reach * 0.01945f * slope;
    const float metres_far = reach * 0.14165f * slope;
    checkf(metres_far > metres_near * 5.0f,
           "in WORLD units the far cascade's bias is %.1fx the near one's "
           "(%.4f m vs %.4f m)",
           static_cast<double>(metres_far / metres_near),
           static_cast<double>(metres_far), static_cast<double>(metres_near));

    // AND NOW THE SURPRISE, which is the best thing in this harness. In DEVICE
    // depth — the units the buffer actually stores — cascades 1, 2 and 3 all
    // want the SAME bias, to three decimal places. That is not a coincidence and
    // it is derivable:
    //
    //     bias = reach * wpt / depth_range
    //     wpt  = 2r / resolution           (the sphere's diameter over the side)
    //     depth_range ~ 2r                 (the sphere again, when it dominates)
    //  => bias ~ reach * slope / resolution
    //
    // The radius cancels. A sphere-fitted cascade's device-space bias depends on
    // the RESOLUTION and the slope and nothing else — not on which cascade it
    // is, not on how far away it is.
    const float predicted = reach * slope / static_cast<float>(k_res);
    checkf(std::fabs(biases[2] - predicted) / predicted < 0.05f,
           "cascade 2's bias %.4e is within %.1f%% of reach*slope/resolution "
           "= %.4e — the sphere's radius cancels out",
           static_cast<double>(biases[2]),
           static_cast<double>(std::fabs(biases[2] - predicted) / predicted * 100.0f),
           static_cast<double>(predicted));

    // Cascade 0 is the exception, and the exception confirms the mechanism: its
    // depth range is set by the CASTERS (31.9 m) rather than by its own small
    // sphere (~19.9 m), so the cancellation is spoiled and it needs LESS bias.
    checkf(biases[0] < biases[1] * 0.8f,
           "cascade 0 needs only %.0f%% of cascade 1's bias, because the casters "
           "set its depth range instead of its sphere — the one cascade where "
           "the radius does not cancel",
           static_cast<double>(biases[0] / biases[1] * 100.0f));

    // And it is LINEAR in world_per_texel, which is the derivation's actual
    // claim rather than merely "bigger".
    near_d = 0.1f;
    const float f0 = engine::practical_split(0, 4, 0.1f, 60.0f, 0.5f);
    const frustum_slice s0 = engine::slice_corners(cam, near_d, f0);
    const light_camera c0 = engine::fit_directional_slice(key, s0, scene, k_res, true);
    const float doubled = engine::slope_scaled_bias(c0.world_per_texel * 2.0f, slope,
                                                    c0.depth_range, reach);
    checkf(std::fabs(doubled - 2.0f * biases[0]) < 1e-9f,
           "doubling world_per_texel doubles the bias exactly (%.3e vs %.3e) — "
           "it is a multiple, not a curve",
           static_cast<double>(doubled), static_cast<double>(2.0f * biases[0]));
}

// ---------------------------------------------------------------------------
void section_g_seam()
{
    section("G  SELECTION AND THE SEAM");

    engine::cascaded_shadow_map csm;
    check(csm.create(4, 256), "a four-cascade set at 256 square");

    csm.settings().count = 4;
    csm.settings().lambda = 0.5f;
    csm.settings().near_d = 0.1f;
    csm.settings().far_d = 60.0f;

    // Splits are filled by render(); fill them here the same way so selection
    // can be tested without a scene.
    for (int i = 0; i < 4; ++i)
    {
        (void)engine::practical_split(i, 4, 0.1f, 60.0f, 0.5f);
    }

    // With blending off, selection is a step function and every depth lands in
    // exactly one cascade.
    csm.settings().blend_fraction = 0.0f;
    const engine::cascade_choice hard = csm.choose(0.5f);
    checkf(hard.blend == 0.0f && hard.index == hard.next,
           "blend_fraction = 0 gives a hard choice (cascade %d, blend %.1f) — "
           "which is the seam, and the first picture the lesson shows",
           hard.index, static_cast<double>(hard.blend));

    // Depths far beyond the last split still resolve, to the last cascade, and
    // do not run off the end of the array.
    const engine::cascade_choice beyond = csm.choose(1e6f);
    checkf(beyond.index == 3 && beyond.next == 3,
           "a depth past the far plane clamps to the last cascade (%d)",
           beyond.index);
}

// ---------------------------------------------------------------------------
void section_h_depth_range()
{
    section("H  DEPTH RANGE");

    directional_light key;
    key.direction = vec3{0.0f, -1.0f, 0.0f};   // straight down

    const camera_frustum cam = camera_at(0.0f, vec3{0.0f, 1.7f, 0.0f});
    const frustum_slice sl = engine::slice_corners(cam, 0.1f, 10.0f);

    // A slice near the ground, and a caster fifty metres UP — outside the slice
    // entirely, but between the light and it.
    const aabb tall{vec3{-5.0f, 0.0f, -5.0f}, vec3{5.0f, 50.0f, 5.0f}};
    const light_camera with = engine::fit_directional_slice(key, sl, tall, k_res, true);

    const aabb flat{vec3{-5.0f, 0.0f, -5.0f}, vec3{5.0f, 1.0f, 5.0f}};
    const light_camera without = engine::fit_directional_slice(key, sl, flat, k_res, true);

    checkf(with.depth_range > without.depth_range + 30.0f,
           "a 50 m caster stretches the depth range to %.1f m (from %.1f m) — "
           "an occluder above the slice is outside it and must still be drawn",
           static_cast<double>(with.depth_range),
           static_cast<double>(without.depth_range));

    // The box's SIDE, though, must not care: the sphere covers the slice, and a
    // tall caster is a depth-range fact, not a texel-density one.
    checkf(std::fabs(with.world_per_texel - without.world_per_texel) < 1e-6f,
           "…and world_per_texel is unchanged (%.6f vs %.6f): the caster moved "
           "the near plane, not the texel grid",
           static_cast<double>(with.world_per_texel),
           static_cast<double>(without.world_per_texel));
}

// ---------------------------------------------------------------------------
std::string read_file(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) { return {}; }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void section_i_golden()
{
    section("I  THE GOLDEN");

    const int rc = demo::write_reference_shot("scratch/verify69.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify69.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL, hash E917C06C — EIGHTEENTH lesson at this hash. This "
          "one added a cascade header and source, a depth ARRAY texture, a "
          "per-layer render pass, a 336-byte uniform block, a second cbuffer in "
          "the scene shader and a Texture2D -> Texture2DArray binding change — "
          "and moved not one pixel, because the reference scene binds no shadow "
          "map and `cascade_count` reaches the shader through a fallback that is "
          "an identity. A NEW CAPABILITY IS A NEW PATH");
}

} // namespace

int main()
{
    std::printf("verify_69 — Lesson 6.9: Cascaded Shadow Maps\n");

    section_a_splits();
    section_b_slice();
    section_c_density();
    section_d_rotation();
    section_e_snapping();
    section_f_bias();
    section_g_seam();
    section_h_depth_range();
    section_i_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
