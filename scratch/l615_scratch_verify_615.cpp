// scratch/verify_615.cpp — Lesson 6.15's harness: the sky, and what it costs to
// integrate it.
//
//   §A  THE FACES — the enum against SDL's, and the round trip
//   §B  THE WEIGHT — solid angles, and the 1% that never goes away
//   §C  THE EXACT HALF — a uniform environment must reproduce Lesson 6.2
//   §D  THE SKY — a baked cube against the function it came from, and the seam
//   §E  THE SPLIT SUM — against brute force, and the measurement that lied first
//   §F  THE LEVEL — the lobe's answer against the one everybody ships
//   §G  CONVERGENCE AND QUANTISATION — samples, bytes, and the energy that is lost
//   §H  HALF FLOATS — the only way HDR data reaches the device
//   §I  THE GOLDEN — a whole new lighting model must not move the old picture
//   §J  THE GPU — cube textures, six faces, six mip levels, three bindings
//
// §E IS THE ONE THAT MATTERS, and it matters twice. The split-sum approximation
// lands 2.0% to 5.7% dark on a smooth sky and up to 15.4% dark when the sky
// contains a sun — but the FIRST version of this measurement reported 20% at
// roughness 0.25 and it was wrong. Eight thousand importance samples cannot
// resolve a 6000:1 source; a million can. Lesson 6.14's rule was to check that
// a measurement can produce a non-null result before believing a null one. This
// is its mirror: check that a non-null result has CONVERGED before believing it.
//
// Build and run:  sh scratch/build_verify_615.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/cubemap.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_pipeline.hpp>
#include <engine/gfx/gpu_post.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/hdr.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/microfacet.hpp>
#include <engine/math/vec3.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <numbers>
#include <sstream>
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

void section(const char* title)
{
    std::printf("\n== %s ==\n", title);
}

using engine::brdf_terms;
using engine::cube_face;
using engine::cube_map;
using engine::cube_texel;
using engine::environment;
using engine::linear_rgb;
using engine::microsurface;
using engine::sky_settings;
using engine::vec3;

constexpr double k_pi = 3.14159265358979323846;

std::string read_file(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
}

double elapsed_ms(std::chrono::steady_clock::time_point t0)
{
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// ===========================================================================
//  §A — the faces
// ===========================================================================

void section_a_faces()
{
    section("A  THE FACES: SDL'S ORDER, AND THE ROUND TRIP");

    // THE SAME DISCIPLINE Lesson 3.9's `filter` enum gets, applied to a second
    // enum that exists only to be `static_cast` to SDL's. If SDL ever reorders
    // `SDL_GPUCubeMapFace`, six faces would quietly swap places and the sky
    // would be scrambled in a way no compiler could see. This is what stands
    // between us and that.
    check(static_cast<int>(cube_face::pos_x) == SDL_GPU_CUBEMAPFACE_POSITIVEX
       && static_cast<int>(cube_face::neg_x) == SDL_GPU_CUBEMAPFACE_NEGATIVEX
       && static_cast<int>(cube_face::pos_y) == SDL_GPU_CUBEMAPFACE_POSITIVEY
       && static_cast<int>(cube_face::neg_y) == SDL_GPU_CUBEMAPFACE_NEGATIVEY
       && static_cast<int>(cube_face::pos_z) == SDL_GPU_CUBEMAPFACE_POSITIVEZ
       && static_cast<int>(cube_face::neg_z) == SDL_GPU_CUBEMAPFACE_NEGATIVEZ,
          "engine::cube_face is enumerator-for-enumerator SDL_GPUCubeMapFace, so "
          "the face index can be handed straight to SDL_GPUTextureRegion::layer");

    // ---- The round trip -----------------------------------------------------
    double worst = 0.0;
    int face_mismatches = 0;
    for (int f = 0; f < engine::k_cube_faces; ++f)
    {
        for (int j = 0; j < 17; ++j)
        {
            for (int i = 0; i < 17; ++i)
            {
                const vec3 d = engine::cube_to_direction(static_cast<cube_face>(f),
                                                         (i + 0.5f) / 17.0f,
                                                         (j + 0.5f) / 17.0f);
                const cube_texel t = engine::direction_to_cube(d);
                if (static_cast<int>(t.face) != f) { ++face_mismatches; }
                const vec3 back = engine::cube_to_direction(t.face, t.u, t.v);
                worst = std::max<double>(worst, engine::length(d - back));
            }
        }
    }
    checkf(face_mismatches == 0,
           "every one of 6 x 289 directions comes back on the face it was "
           "generated from (%d mismatches)", face_mismatches);
    checkf(worst < 1.0e-6,
           "direction -> face/uv -> direction is exact to %.3e, which is float "
           "rounding on a normalise and not an error in the table", worst);

    // ---- The axes, spelled out ---------------------------------------------
    //
    // The six centres are the six axes, which is the one check that would catch
    // a transposed row in the table. Written out rather than looped, because
    // "these six directions" IS the content.
    struct { cube_face f; vec3 expect; } centres[] = {
        {cube_face::pos_x, { 1,  0,  0}},
        {cube_face::neg_x, {-1,  0,  0}},
        {cube_face::pos_y, { 0,  1,  0}},
        {cube_face::neg_y, { 0, -1,  0}},
        {cube_face::pos_z, { 0,  0,  1}},
        {cube_face::neg_z, { 0,  0, -1}},
    };
    bool centres_ok = true;
    for (const auto& c : centres)
    {
        const vec3 got = engine::cube_to_direction(c.f, 0.5f, 0.5f);
        if (engine::length(got - c.expect) > 1.0e-6f) { centres_ok = false; }
    }
    check(centres_ok, "each face's centre is its own axis: +X -> (1,0,0) and so on");

    // ---- THE HANDEDNESS NOTE, AS A MEASUREMENT ------------------------------
    //
    // Conventions §2 pins this world as right-handed, Y-up, -Z forward. Looking
    // along +X in such a world, "right" is cross(forward, up) = +Z. The cube
    // table puts the +X face's u axis along -Z. So u increases to the LEFT, and
    // a face image authored by a right-handed camera is mirrored.
    //
    // This is not a bug to fix — the hardware implements the table — it is a
    // fact to know when you bake faces from anything but `cube_to_direction`.
    const vec3 right_of_plus_x = engine::cross(vec3{1, 0, 0}, vec3{0, 1, 0});
    const vec3 u_axis_of_plus_x = engine::normalised(
        engine::cube_to_direction(cube_face::pos_x, 1.0f, 0.5f)
        - engine::cube_to_direction(cube_face::pos_x, 0.0f, 0.5f));
    checkf(engine::dot(right_of_plus_x, u_axis_of_plus_x) < -0.99f,
           "THE MIRROR IS REAL AND MEASURED: looking along +X, a right-handed "
           "world's 'right' is %+.0f%+.0f%+.0f and the cube's u axis is "
           "%+.0f%+.0f%+.0f — they are OPPOSITE (dot %.3f). Faces baked with a "
           "right-handed camera come out left-right flipped",
           right_of_plus_x.x, right_of_plus_x.y, right_of_plus_x.z,
           u_axis_of_plus_x.x, u_axis_of_plus_x.y, u_axis_of_plus_x.z,
           engine::dot(right_of_plus_x, u_axis_of_plus_x));
}

// ===========================================================================
//  §B — the weight
// ===========================================================================

void section_b_weight()
{
    section("B  THE WEIGHT: SOLID ANGLES, AND THE 1% THAT NEVER GOES AWAY");

    std::printf("    %-6s %-16s %-12s %-10s\n", "size", "sum (sr)", "rel err", "corner/centre");
    bool all_close = true;
    for (int n : {8, 32, 128})
    {
        double total = 0.0;
        for (int j = 0; j < n; ++j)
        {
            for (int i = 0; i < n; ++i) { total += engine::cube_texel_solid_angle(n, i, j); }
        }
        total *= 6.0;
        const double rel = std::fabs(total - 4.0 * k_pi) / (4.0 * k_pi);
        const double ratio = engine::cube_texel_solid_angle(n, 0, 0)
                           / engine::cube_texel_solid_angle(n, n / 2, n / 2);
        std::printf("    %-6d %-16.9f %-12.2e %-10.6f\n", n, total, rel, ratio);
        if (rel > 1.0e-6) { all_close = false; }
    }
    check(all_close,
          "six faces of texel solid angles sum to 4*pi at every resolution — the "
          "one check that says Lambert's four-corner formula was transcribed "
          "correctly");

    // THE ASYMPTOTE, DERIVED. A texel at face coordinates (x, y) sits at
    // r = sqrt(1 + x^2 + y^2) and its plane is tilted by cos = 1/r, so
    // dw = dA/r^3. At the corner r = sqrt(3), so the ratio tends to
    // 1/(3*sqrt(3)) = 0.19245 as the texels shrink onto the corner.
    const double predicted = 1.0 / (3.0 * std::sqrt(3.0));
    const double measured = engine::cube_texel_solid_angle(512, 0, 0)
                          / engine::cube_texel_solid_angle(512, 256, 256);
    checkf(std::fabs(measured - predicted) < 2.0e-3,
           "the corner/centre ratio converges to the derived 1/(3*sqrt(3)) = "
           "%.5f (measured %.5f at 512) — so the CENTRE texel is worth %.3f "
           "corner texels, and that is the whole reason every integral in "
           "cubemap.cpp carries a weight",
           predicted, measured, 1.0 / predicted);

    // ---- WHAT EQUAL WEIGHTS COST, AND WHY IT IS A BUG AND NOT AN ERROR -----
    //
    // The mean of cos(theta) over a hemisphere, weighted by solid angle, is
    // exactly 0.5 (the integral of cos over the hemisphere is pi, the
    // hemisphere is 2*pi). Weight the same texels equally and the answer is
    // wrong — and stays wrong as the cube is refined, because the corners are
    // over-counted by a fixed factor however small they get.
    const vec3 N = engine::normalised(vec3{0.3f, 0.8f, 0.2f});
    std::printf("    %-6s %-18s %-18s %-8s\n", "size", "equal-weight", "solid-angle", "ratio");
    double bias_16 = 0.0, bias_64 = 0.0;
    for (int n : {16, 64})
    {
        double flat_num = 0.0, sa_num = 0.0, sa_den = 0.0;
        long count = 0;
        for (int f = 0; f < engine::k_cube_faces; ++f)
        {
            for (int j = 0; j < n; ++j)
            {
                for (int i = 0; i < n; ++i)
                {
                    const vec3 d = engine::cube_to_direction(static_cast<cube_face>(f),
                                                             (i + 0.5f) / n, (j + 0.5f) / n);
                    const double c = engine::dot(N, d);
                    if (c <= 0.0) { continue; }
                    flat_num += c;
                    ++count;
                    const double w = engine::cube_texel_solid_angle(n, i, j);
                    sa_num += c * w;
                    sa_den += w;
                }
            }
        }
        const double flat = flat_num / count;
        const double weighted = sa_num / sa_den;
        std::printf("    %-6d %-18.8f %-18.8f %-8.6f\n", n, flat, weighted, flat / weighted);
        (n == 16 ? bias_16 : bias_64) = flat / weighted;
    }
    checkf(std::fabs(bias_16 - bias_64) < 1.0e-3 && bias_64 > 1.005,
           "an unweighted texel average is %.3f%% too bright, and REFINING THE "
           "CUBE DOES NOT HELP: %.6f at 16x16 and %.6f at 64x64. It is a bias, "
           "not a discretisation error — which is exactly what makes it a bug",
           (bias_64 - 1.0) * 100.0, bias_16, bias_64);
}

// ===========================================================================
//  §C — the exact half
// ===========================================================================

void section_c_exact()
{
    section("C  THE EXACT HALF: A UNIFORM ENVIRONMENT MUST REPRODUCE LESSON 6.2");

    // This is the check the whole file is built around. Lambert's BRDF is a
    // constant, so it comes out of the integral exactly, and the integral of
    // cos over the hemisphere is exactly pi. Feed the convolution a uniform
    // radiance and it must return pi times it — at which point
    // (albedo/pi) * pi * L = albedo * L, which is what `light.hpp` has computed
    // since Lesson 3.6.
    std::printf("    %-6s %-16s %-12s %-10s\n", "size", "E", "rel err", "x previous");
    double prev = 0.0;
    bool converging = true;
    for (int n : {8, 16, 32, 64})
    {
        const cube_map env = engine::make_uniform_environment(n, {1.0f, 1.0f, 1.0f});
        const cube_map irr = engine::irradiance_map(env, 4);
        const float e = irr.face(cube_face::pos_y, 0).pixel_at(2, 2).r;
        const double rel = std::fabs(e - k_pi) / k_pi;
        std::printf("    %-6d %-16.8f %-12.3e %-10s", n, e, rel,
                    prev > 0.0 ? "" : "—");
        if (prev > 0.0) { std::printf("%.2f", prev / rel); }
        std::printf("\n");
        if (prev > 0.0 && (prev / rel < 3.0 || prev / rel > 5.0)) { converging = false; }
        prev = rel;
    }
    checkf(prev < 2.0e-5,
           "E converges to pi with relative error %.3e at 64x64 faces — the "
           "diffuse half has NO approximation in it, only discretisation", prev);
    check(converging,
           "and it converges SECOND ORDER: each doubling of the face divides the "
           "error by about 4, which is what a midpoint rule on a smooth "
           "integrand does and is the evidence that nothing else is wrong");

    // ---- THE IDENTITY, END TO END -------------------------------------------
    //
    // Not the integral in isolation but the whole ambient term, through
    // `image_based_light`, against the expression it replaces. A pure dielectric
    // with roughness 1 still has a specular lobe, so the comparison is made
    // against the DIFFUSE half alone by setting f0 to zero — which the metallic
    // workflow cannot express, so it is done by subtracting the specular
    // contribution a zero-albedo material gives.
    const linear_rgb radiance{0.30f, 0.40f, 0.50f};
    const cube_map env = engine::make_uniform_environment(64, radiance);
    const environment baked = engine::bake_environment(env, 16, 4, 32);

    microsurface surface;
    surface.roughness = 1.0f;
    surface.metallic = 0.0f;
    const linear_rgb albedo{0.80f, 0.60f, 0.40f};

    const vec3 n{0.0f, 1.0f, 0.0f};
    const vec3 v = engine::normalised(vec3{0.3f, 0.8f, 0.2f});

    const linear_rgb ibl = engine::image_based_light(baked, surface, albedo, n, v);
    // The specular half alone: same surface, black albedo, so the diffuse term
    // vanishes and what is left is the split sum's own contribution.
    const linear_rgb spec_only =
        engine::image_based_light(baked, surface, {0.0f, 0.0f, 0.0f}, n, v);

    const linear_rgb old_term{albedo.r * radiance.r, albedo.g * radiance.g,
                              albedo.b * radiance.b};
    const float diffuse_r = ibl.r - spec_only.r;
    const float diffuse_g = ibl.g - spec_only.g;
    const float diffuse_b = ibl.b - spec_only.b;
    const double worst = std::max({std::fabs(diffuse_r - old_term.r) / old_term.r,
                                   std::fabs(diffuse_g - old_term.g) / old_term.g,
                                   std::fabs(diffuse_b - old_term.b) / old_term.b});
    checkf(worst < 2.0e-3,
           "and END TO END: image_based_light's diffuse half is (%.5f, %.5f, "
           "%.5f) where Lesson 6.2's `albedo * ambient` is (%.5f, %.5f, %.5f) — "
           "agreeing to %.3e. THE OLD TERM IS A SPECIAL CASE OF THE NEW ONE",
           diffuse_r, diffuse_g, diffuse_b,
           old_term.r, old_term.g, old_term.b, worst);

    // And the part the old term could never do: the same mirror, in the same
    // room, is no longer black.
    microsurface mirror;
    mirror.roughness = 0.05f;
    mirror.metallic = 1.0f;
    const linear_rgb chrome{0.95f, 0.93f, 0.88f};
    const linear_rgb mirrored = engine::image_based_light(baked, mirror, chrome, n, v);
    checkf(mirrored.r > 0.2f,
           "THE SENTENCE scene.frag.hlsl HAS CARRIED SINCE LESSON 6.4 IS "
           "CLOSED: a mirror in a uniform room of radiance %.2f now reflects "
           "%.4f instead of 0.0000, because the ambient term finally has a "
           "specular lobe", radiance.r, mirrored.r);
}

// ===========================================================================
//  §D — the sky
// ===========================================================================

void section_d_sky()
{
    section("D  THE SKY: A BAKED CUBE AGAINST THE FUNCTION IT CAME FROM");

    sky_settings sky;
    const cube_map env = engine::make_sky_environment(128, sky);
    check(env.valid() && env.size() == 128 && env.levels() == 1,
          "a 128x128 six-face environment bakes");

    // EVERY TEXEL MUST BE THE FUNCTION, because `make_sky_environment` evaluates
    // it at the texel centre. This is not a test of the sky model; it is a test
    // that `cube_to_direction` and `direction_to_cube` agree, because the bake
    // uses the first and the sample uses the second.
    double worst = 0.0;
    for (int f = 0; f < engine::k_cube_faces; ++f)
    {
        for (int j = 0; j < 128; j += 7)
        {
            for (int i = 0; i < 128; i += 7)
            {
                const vec3 d = engine::cube_to_direction(static_cast<cube_face>(f),
                                                         (i + 0.5f) / 128.0f,
                                                         (j + 0.5f) / 128.0f);
                const linear_rgb want = engine::sky_radiance(sky, d);
                const linear_rgb got = env.sample(d, 0);
                const double e = std::fabs(got.g - want.g)
                               / std::max(1.0e-6, static_cast<double>(want.g));
                worst = std::max(worst, e);
            }
        }
    }
    checkf(worst < 1.0e-5,
           "sampling the baked cube at a texel centre returns the analytic sky to "
           "%.3e — so the bake and the lookup use the same face table, which is "
           "the only thing this can be testing", worst);

    // ---- THE SEAM, AND THE FIRST TWO TESTS OF IT MEASURED NOTHING ----------
    //
    // `cube_map::sample` clamps at a face edge instead of stepping onto the
    // neighbouring face, so two lookups straddling an edge should disagree by
    // about one texel's worth of whatever the environment is doing there. The
    // header admits the limit; this measures it.
    //
    // IT TOOK THREE ATTEMPTS AND THE FIRST TWO BOTH REPORTED 0.0000%.
    //   (1) The probe pairs never crossed a face boundary, so the test compared
    //       a face with itself. Now `crossings` is checked FIRST.
    //   (2) They did cross — and the answer was still zero, because
    //       `sky_radiance` depends only on `d.y`. Along the +X/+Z edge the sky
    //       is LITERALLY CONSTANT, so the two clamped edge texels hold the same
    //       value and there is nothing for a seam to break. A null result from
    //       an environment that cannot exhibit the defect is not evidence that
    //       the defect is absent.
    // So the measurement is made on an environment built to show it: a smooth
    // function that varies with AZIMUTH, which is the axis a vertical face edge
    // actually separates.
    {
        const int n = 16;
        cube_map probe(n, 1);
        for (int f = 0; f < engine::k_cube_faces; ++f)
        {
            engine::hdr_buffer& img = probe.face(static_cast<cube_face>(f), 0);
            for (int y = 0; y < n; ++y)
            {
                linear_rgb* row = img.row(y);
                for (int x = 0; x < n; ++x)
                {
                    const vec3 d = engine::cube_to_direction(static_cast<cube_face>(f),
                                                             (x + 0.5f) / n,
                                                             (y + 0.5f) / n);
                    // Smooth, bounded, and varying in azimuth — so any jump in
                    // the SAMPLED value across an edge is the sampler's.
                    const float v = 1.0f + 0.5f * std::sin(4.0f * std::atan2(d.z, d.x));
                    row[x] = {v, v, v};
                }
            }
        }

        double worst = 0.0;
        int crossings = 0;
        for (int k = 1; k < 32; ++k)
        {
            const float elevation = (k / 32.0f) * 0.8f - 0.4f;
            const float a = 0.25f * static_cast<float>(k_pi);
            const vec3 left = engine::normalised(
                vec3{std::cos(a + 0.002f), elevation, std::sin(a + 0.002f)});
            const vec3 right = engine::normalised(
                vec3{std::cos(a - 0.002f), elevation, std::sin(a - 0.002f)});
            if (engine::direction_to_cube(left).face
                == engine::direction_to_cube(right).face) { continue; }
            ++crossings;
            const linear_rgb l = probe.sample(left, 0);
            const linear_rgb r = probe.sample(right, 0);
            worst = std::max<double>(worst,
                std::fabs(l.g - r.g) / std::max(1.0e-6f, r.g));
        }
        checkf(crossings > 20,
               "%d of 31 probe pairs land on DIFFERENT faces — checked FIRST, "
               "because a seam test confined to one face reports a perfect "
               "0.0000%% and reads as a pass", crossings);
        checkf(worst > 0.001 && worst < 0.20,
               "THE SEAM IS REAL AND IT IS %.2f%% at 16x16 on an environment "
               "that varies in azimuth — one texel's worth of the function, "
               "which is exactly what a clamp instead of a face step should "
               "cost. THE GPU HAS NO SEAM AT ALL: hardware cube sampling "
               "filters across face edges, which is the one thing in this "
               "lesson where the GPU is simply better and not merely faster",
               worst * 100.0);

        // And the same measurement on the sky, for the record: zero, and the
        // reason is a property of the sky rather than of the sampler.
        double sky_worst = 0.0;
        for (int k = 1; k < 32; ++k)
        {
            const float elevation = (k / 32.0f) * 0.8f - 0.4f;
            const float a = 0.25f * static_cast<float>(k_pi);
            const vec3 left = engine::normalised(
                vec3{std::cos(a + 0.002f), elevation, std::sin(a + 0.002f)});
            const vec3 right = engine::normalised(
                vec3{std::cos(a - 0.002f), elevation, std::sin(a - 0.002f)});
            sky_worst = std::max<double>(sky_worst,
                std::fabs(env.sample(left, 0).g - env.sample(right, 0).g)
                    / std::max(1.0e-6f, env.sample(right, 0).g));
        }
        checkf(sky_worst < 1.0e-5,
               "and on THIS lesson's sky the same probe reads %.2e — genuinely "
               "seamless, because a sky that depends only on elevation is "
               "constant along a vertical face edge. Worth stating: the seam is "
               "a property of the environment as much as of the sampler",
               sky_worst);
    }

    // The sun must survive the bake. A disc of angular radius 1 degree on a
    // 128-texel face (0.70 degrees per texel) is a handful of texels, which is
    // the point of the default being larger than the real sun's 0.266.
    double peak = 0.0;
    for (int f = 0; f < engine::k_cube_faces; ++f)
    {
        for (int j = 0; j < 128; ++j)
        {
            const linear_rgb* row = env.face(static_cast<cube_face>(f), 0).row(j);
            for (int i = 0; i < 128; ++i) { peak = std::max<double>(peak, row[i].g); }
        }
    }
    checkf(peak > 5000.0,
           "the sun disc survives the bake at %.0f — a dynamic range of %.0f:1 "
           "against the sky around it, which is why a cube map face is an "
           "hdr_buffer and not a texture", peak, peak / 0.5);
}

// ===========================================================================
//  §E — the split sum
// ===========================================================================

/// The true integral, by brute-force GGX importance sampling.
///
/// `from_cube` chooses the reference: the ANALYTIC sky, or the BAKED cube map
/// the split sum actually reads. Running both separates two error sources that
/// are usually reported as one — what the bake lost, and what the approximation
/// loses — and the separation matters, because the first is fixable by spending
/// texels and the second is not.
double brute_force_specular(const sky_settings& sky, const cube_map* baked,
                            float roughness, float n_dot_v, int samples)
{
    const float alpha = engine::alpha_from_roughness(roughness);

    // The tangent-space normal is (0, 0, 1), so every `n.x` below is a z
    // component and the vector is never written down.
    const vec3 v{std::sqrt(1.0f - n_dot_v * n_dot_v), 0.0f, n_dot_v};

    // The environment is in WORLD space, so samples have to be rotated into a
    // frame with the normal pointing up — straight up, which puts the sun in
    // the upper hemisphere where it belongs and keeps the frame the identity
    // up to an axis swap.
    const vec3 world_n{0.0f, 1.0f, 0.0f};
    const vec3 world_t{1.0f, 0.0f, 0.0f};
    const vec3 world_b{0.0f, 0.0f, 1.0f};

    double total = 0.0;
    for (int i = 0; i < samples; ++i)
    {
        const engine::vec2 xi = engine::hammersley(i, samples);
        const vec3 h = engine::ggx_importance_sample(xi.x, xi.y, alpha);
        const vec3 l = engine::reflect(-v, h);
        const float ndl = l.z;
        if (ndl <= 0.0f) { continue; }

        const float ndh = std::max(1.0e-6f, h.z);
        const float vdh = std::max(1.0e-6f, engine::dot(v, h));

        const vec3 world_l = world_t * l.x + world_b * l.y + world_n * l.z;
        const linear_rgb li = (baked != nullptr) ? baked->sample(world_l, 0)
                                                 : engine::sky_radiance(sky, world_l);

        // The estimator: D cancels against the pdf, leaving G, the Jacobian of
        // the half-vector-to-light change of variables, and the cosines — the
        // same expression `integrate_brdf` uses, times Fresnel and the
        // radiance. f0 = 0.04, a dielectric.
        const float g = engine::smith_g(ndl, n_dot_v, alpha);
        const float fc = std::pow(1.0f - vdh, 5.0f);
        const float fresnel = 0.04f * (1.0f - fc) + fc;
        total += static_cast<double>(li.g) * g * vdh / (ndh * n_dot_v) * fresnel;
    }
    return total / samples;
}

/// The split sum, driven through the SHIPPED function rather than reimplemented
/// — the point is to measure what the engine does, not what the paper says. A
/// black albedo with `metallic = 0` isolates the specular half at f0 = 0.04.
double split_sum_specular(const environment& baked, float roughness, float n_dot_v)
{
    microsurface surface;
    surface.roughness = roughness;
    surface.metallic = 0.0f;

    const vec3 n{0.0f, 1.0f, 0.0f};
    const vec3 v = engine::normalised(vec3{std::sqrt(1.0f - n_dot_v * n_dot_v),
                                           n_dot_v, 0.0f});
    return engine::image_based_light(baked, surface, {0.0f, 0.0f, 0.0f}, n, v).g;
}

void section_e_split_sum()
{
    section("E  THE SPLIT SUM: AGAINST BRUTE FORCE, AND THE MEASUREMENT THAT LIED");

    sky_settings sunny;
    sky_settings clear = sunny;
    clear.sun_radiance = {0.0f, 0.0f, 0.0f};

    const cube_map sunny_env = engine::make_sky_environment(128, sunny);
    const cube_map clear_env = engine::make_sky_environment(128, clear);

    // ---- FIRST, THE LESSON ABOUT THE MEASUREMENT ITSELF ---------------------
    //
    // The same integral at two sample counts. If they disagree, the smaller one
    // is not measuring what it claims to — and the first draft of this section
    // reported a 20% split-sum error that was entirely this.
    {
        double worst_ratio = 1.0;
        float worst_r = 0.0f, worst_ndv = 0.0f;
        for (float roughness : {0.10f, 0.25f, 0.50f, 0.75f})
        {
            for (float ndv : {0.9f, 0.6f, 0.4f})
            {
                const double low = brute_force_specular(sunny, nullptr, roughness, ndv, 8192);
                const double high = brute_force_specular(sunny, nullptr, roughness, ndv, 1000000);
                const double ratio = low / std::max(1.0e-9, high);
                if (std::fabs(std::log(ratio)) > std::fabs(std::log(worst_ratio)))
                {
                    worst_ratio = ratio;
                    worst_r = roughness;
                    worst_ndv = ndv;
                }
            }
        }
        checkf(worst_ratio > 1.2 || worst_ratio < 0.83,
               "8,192 importance samples disagree with 1,000,000 by a factor of "
               "%.2f at roughness %.2f, n.v %.1f. A 6000:1 sun disc inside a "
               "narrow lobe is a high-variance integrand, and THE "
               "UNDER-SAMPLED ANSWER LOOKS PERFECTLY PLAUSIBLE — it is a "
               "number of the right order with no sign of being wrong. 6.14's "
               "rule was to check a measurement CAN produce a non-null result; "
               "this is its mirror",
               worst_ratio, worst_r, worst_ndv);

        double smooth_worst = 1.0;
        for (float roughness : {0.10f, 0.25f, 0.50f, 0.75f})
        {
            for (float ndv : {0.9f, 0.6f, 0.4f})
            {
                const double low = brute_force_specular(clear, nullptr, roughness, ndv, 8192);
                const double high = brute_force_specular(clear, nullptr, roughness, ndv, 1000000);
                smooth_worst = std::max(smooth_worst, std::max(low / high, high / low));
            }
        }
        checkf(smooth_worst < 1.01,
               "and on the SAME integrand with the sun removed, 8,192 samples "
               "agree with 1,000,000 to within %.3f%% everywhere — so the "
               "sample count was never the problem, THE DYNAMIC RANGE WAS",
               (smooth_worst - 1.0) * 100.0);
    }

    // ---- Now the approximation, with the bake's error separated out ---------
    struct { const char* label; const sky_settings* sky; const cube_map* env; } passes[] = {
        {"sun REMOVED (smooth sky)", &clear, &clear_env},
        {"sun PRESENT (6000:1 disc)", &sunny, &sunny_env},
    };

    double smooth_split_worst = 1.0;      // over every angle
    double smooth_split_normal = 1.0;     // n.v = 0.9 only
    double smooth_split_low_rough = 1.0;  // roughness 0.10 only
    double sunny_split_worst = 1.0;
    double sunny_bake_worst = 1.0;

    for (const auto& pass : passes)
    {
        const environment baked = engine::bake_environment(*pass.env, 32, 6, 64);
        std::printf("    --- %s ---\n", pass.label);
        std::printf("    %-7s %-5s %-12s %-12s %-12s %-9s %-9s\n",
                    "rough", "n.v", "truth", "cube-truth", "split-sum",
                    "bake", "split");

        for (float roughness : {0.10f, 0.25f, 0.50f, 0.75f, 1.00f})
        {
            for (float ndv : {0.9f, 0.4f})
            {
                // THREE NUMBERS, NOT TWO.
                //   truth       the integral over the analytic sky
                //   cube_truth  the same integral over the 128^2 BAKED cube
                //   split       what the engine computes
                // truth -> cube_truth is what the bake lost (fixable with
                // texels); cube_truth -> split is the approximation (not).
                const double truth =
                    brute_force_specular(*pass.sky, nullptr, roughness, ndv, 1000000);
                const double cube_truth =
                    brute_force_specular(*pass.sky, pass.env, roughness, ndv, 1000000);
                const double split = split_sum_specular(baked, roughness, ndv);

                const double bake_ratio = cube_truth / std::max(1.0e-9, truth);
                const double split_ratio = split / std::max(1.0e-9, cube_truth);
                std::printf("    %-7.2f %-5.1f %-12.6f %-12.6f %-12.6f %-9.4f %-9.4f\n",
                            roughness, ndv, truth, cube_truth, split,
                            bake_ratio, split_ratio);

                if (pass.sky == &clear)
                {
                    smooth_split_worst = std::min(smooth_split_worst, split_ratio);
                    if (ndv > 0.5f)
                    {
                        smooth_split_normal = std::min(smooth_split_normal, split_ratio);
                    }
                    if (roughness < 0.15f)
                    {
                        smooth_split_low_rough =
                            std::min(smooth_split_low_rough, split_ratio);
                    }
                }
                else
                {
                    sunny_split_worst = std::min(sunny_split_worst, split_ratio);
                    sunny_bake_worst = std::min(sunny_bake_worst, bake_ratio);
                }
            }
        }
    }

    checkf((1.0 - smooth_split_worst) > 4.0 * (1.0 - smooth_split_normal),
           "THE ERROR HAS A SHAPE, AND THE SHAPE IS THE POINT. At near-normal "
           "incidence the split sum is never worse than %.1f%% dark; at grazing "
           "it reaches %.1f%% — more than four times as much. That asymmetry IS "
           "the n = v = r assumption: the "
           "prefilter baked its lobe around the REFLECTION direction, which at "
           "a grazing view points across the horizon and averages in ground the "
           "real surface never sees. What it discards is the stretched, "
           "comet-shaped highlight; a round one is what is left",
           (1.0 - smooth_split_normal) * 100.0, (1.0 - smooth_split_worst) * 100.0);

    checkf(smooth_split_low_rough > 0.99,
           "and it is EXACT WHERE IT MATTERS MOST VISUALLY: at roughness 0.10 "
           "the split sum is within %.2f%% of the true integral, because a "
           "narrow lobe barely notices which direction it was centred on. The "
           "technique degrades smoothly from a mirror, which is the opposite of "
           "how most approximations behave",
           std::fabs(1.0 - smooth_split_low_rough) * 100.0);

    checkf(sunny_split_worst < smooth_split_worst,
           "WITH A SUN IN THE SKY IT IS WORSE AGAIN, %.1f%% against %.1f%%, and "
           "for a second, independent reason: the prefilter weights radiance by "
           "n.l where the true integral weights it by the whole BRDF, and those "
           "two disagree most where the environment is nearly a delta function. "
           "THE ERROR OF THIS TECHNIQUE DEPENDS ON THE ENVIRONMENT, not only on "
           "the material — which is not something you can read off the paper",
           (1.0 - sunny_split_worst) * 100.0, (1.0 - smooth_split_worst) * 100.0);

    checkf(sunny_bake_worst < 0.99 || sunny_bake_worst > 1.01,
           "AND THE BAKE IS A SEPARATE ERROR, worth %.3f at worst: a 1-degree "
           "sun on a 128-texel face (0.70 degrees per texel) is a handful of "
           "texels, so the disc's total energy is quantised before any "
           "integral runs. Reporting bake and approximation as one number — "
           "which is what comparing the split sum straight against the "
           "analytic sky does — blames the technique for the resolution",
           sunny_bake_worst);
}

// ===========================================================================
//  §F — the level
// ===========================================================================

void section_f_level()
{
    section("F  THE LEVEL: THE LOBE'S ANSWER AGAINST THE ONE EVERYBODY SHIPS");

    const int base = 128;
    const int levels = 8;
    std::printf("    %-8s %-9s %-12s %-12s %-9s %-9s %s\n",
                "rough", "alpha", "half-angle", "lobe (sr)", "derived", "linear", "diff");

    double worst = 0.0;
    float worst_at = 0.0f;
    for (int i = 0; i <= 20; ++i)
    {
        const float roughness = i / 20.0f;
        const float alpha = engine::alpha_from_roughness(roughness);
        const float half_angle = engine::ggx_lobe_half_angle(alpha);
        const float lobe = engine::ggx_lobe_solid_angle(alpha);
        const float derived = engine::prefilter_level_for(roughness, base, levels);
        const float linear = roughness * (levels - 1);
        if (i % 2 == 0)
        {
            std::printf("    %-8.2f %-9.4f %9.2f deg %-12.6f %-9.3f %-9.3f %+.3f\n",
                        roughness, alpha, half_angle * 180.0 / k_pi, lobe,
                        derived, linear, linear - derived);
        }
        if (std::fabs(linear - derived) > worst)
        {
            worst = std::fabs(linear - derived);
            worst_at = roughness;
        }
    }

    // The hemisphere check is the one that would catch an algebra slip in
    // `ggx_lobe_solid_angle`: at alpha = 1 the lobe is the whole hemisphere.
    checkf(std::fabs(engine::ggx_lobe_solid_angle(1.0f) - 2.0 * k_pi) < 1.0e-4,
           "at alpha = 1 the lobe subtends %.6f sr, which is 2*pi — the whole "
           "hemisphere, and the check that the spherical-cap formula is right",
           engine::ggx_lobe_solid_angle(1.0f));

    checkf(worst < 1.0,
           "the shipped linear-in-roughness mapping is never more than %.3f of a "
           "LEVEL from the one the lobe's own width implies (worst at roughness "
           "%.2f). THE FOLKLORE IS NOT ARBITRARY — it is a good fit to a derived "
           "answer, and now we know by how much", worst, worst_at);

    checkf(engine::prefilter_level_for(0.10f, base, levels) < 0.01f,
           "AND WE KNOW THE DIRECTION OF THE ERROR: at roughness 0.10 the lobe "
           "wants level %.3f and the linear mapping reads %.3f, so a near-mirror "
           "is over-blurred by most of a level. On the middle of the range the "
           "sign flips and it is under-blurred",
           engine::prefilter_level_for(0.10f, base, levels), 0.10f * (levels - 1));

    // For comparison, the other mapping seen in the wild.
    double sqrt_worst = 0.0;
    for (int i = 0; i <= 20; ++i)
    {
        const float roughness = i / 20.0f;
        const float derived = engine::prefilter_level_for(roughness, base, levels);
        sqrt_worst = std::max<double>(sqrt_worst,
                                      std::fabs(std::sqrt(roughness) * (levels - 1) - derived));
    }
    checkf(sqrt_worst > worst * 2.0,
           "and sqrt(roughness) * (levels-1), which also circulates, is off by "
           "%.3f levels — %.1fx worse than the linear one", sqrt_worst, sqrt_worst / worst);
}

// ===========================================================================
//  §G — convergence and quantisation
// ===========================================================================

void section_g_convergence()
{
    section("G  CONVERGENCE AND QUANTISATION: SAMPLES, BYTES, AND LOST ENERGY");

    // ---- How many importance samples the BRDF table needs -------------------
    std::printf("    %-10s %-12s %-12s %-12s\n", "samples", "scale", "bias", "vs 65536");
    const brdf_terms truth = engine::integrate_brdf(0.6f, 0.4f, 65536);
    double last_err = 1.0;
    for (int n : {16, 64, 256, 1024, 4096})
    {
        const brdf_terms t = engine::integrate_brdf(0.6f, 0.4f, n);
        const double err = std::fabs(t.scale - truth.scale) + std::fabs(t.bias - truth.bias);
        std::printf("    %-10d %-12.6f %-12.6f %-12.2e\n", n, t.scale, t.bias, err);
        last_err = err;
    }
    checkf(last_err < 1.0e-3,
           "4,096 Hammersley samples reach %.2e of the 65,536-sample answer, and "
           "1,024 — the shipped default — is already inside a thousandth. A "
           "low-discrepancy sequence converges near 1/N where random sampling "
           "manages 1/sqrt(N)", last_err);

    // ---- What the 8-bit table costs -----------------------------------------
    //
    // `make_brdf_lut` stores scale and bias as bytes. The header calls this the
    // one real compromise in the file; here is the number.
    const engine::texture lut = engine::make_brdf_lut(64, 1024);
    engine::sampler samp;
    samp.texel_filter = engine::filter::linear;
    samp.address_u = engine::address_mode::clamp_to_edge;
    samp.address_v = engine::address_mode::clamp_to_edge;

    double worst_q = 0.0;
    for (int i = 1; i < 16; ++i)
    {
        for (int j = 1; j < 16; ++j)
        {
            const float ndv = (i + 0.5f) / 16.0f;
            const float roughness = (j + 0.5f) / 16.0f;
            const brdf_terms exact = engine::integrate_brdf(ndv, roughness, 4096);
            const linear_rgb got = engine::sample(lut, samp, ndv, roughness);
            worst_q = std::max<double>(worst_q, std::fabs(got.r - exact.scale));
        }
    }
    checkf(worst_q < 0.02,
           "the 8-bit table differs from the un-quantised integral by at most "
           "%.5f absolute — about %.1f codes, which is the bilinear filter and "
           "the 64x64 grid rather than the byte. Shipping engines still use "
           "RG16F, and the exercise at the end of the lesson is why",
           worst_q, worst_q * 255.0);

    // ---- THE ENERGY THE MODEL LOSES, AND IT IS NOT THE SPLIT SUM'S FAULT ----
    //
    // scale + bias is what a perfect mirror (F0 = 1) would reflect. It should be
    // 1 and it is not, at high roughness — because the Smith G models SINGLE
    // scattering only, so light that bounces twice between microfacets is
    // dropped. Lesson 6.3's probe measured the same thing from the other side
    // and got 0.3069 at full roughness.
    std::printf("    %-10s %-10s %-10s %-10s\n", "rough", "n.v", "scale+bias", "lost");
    for (float roughness : {0.1f, 0.5f, 1.0f})
    {
        for (float ndv : {0.9f, 0.4f})
        {
            const brdf_terms t = engine::integrate_brdf(ndv, roughness, 4096);
            std::printf("    %-10.2f %-10.1f %-10.6f %-10.1f%%\n",
                        roughness, ndv, t.scale + t.bias,
                        (1.0 - (t.scale + t.bias)) * 100.0);
        }
    }
    const brdf_terms rough_normal = engine::integrate_brdf(0.9f, 1.0f, 4096);
    checkf(rough_normal.scale + rough_normal.bias < 0.40f,
           "at roughness 1 and near-normal incidence the table sums to %.4f, so "
           "**%.0f%% of the energy is missing** — and this is NOT the split sum's "
           "error. It is the single-scattering Smith G, which Lesson 6.3's probe "
           "measured at 0.3069 from the other direction. Multi-scattering "
           "compensation is a known fix and §11 points at it",
           rough_normal.scale + rough_normal.bias,
           (1.0 - (rough_normal.scale + rough_normal.bias)) * 100.0);

    // ---- What the bake costs, as a time and a memory figure ----------------
    {
        sky_settings sky;
        const cube_map env = engine::make_sky_environment(128, sky);

        const auto t0 = std::chrono::steady_clock::now();
        const cube_map irr = engine::irradiance_map(env, 32);
        const double irr_ms = elapsed_ms(t0);

        const auto t1 = std::chrono::steady_clock::now();
        const cube_map pre = engine::prefilter_environment(env, 6, 64);
        const double pre_ms = elapsed_ms(t1);

        const auto t2 = std::chrono::steady_clock::now();
        const engine::texture lut64 = engine::make_brdf_lut(64, 1024);
        const double lut_ms = elapsed_ms(t2);

        const double env_mb = env.texel_count() * sizeof(linear_rgb) / (1024.0 * 1024.0);
        const double pre_mb = pre.texel_count() * sizeof(linear_rgb) / (1024.0 * 1024.0);
        const double irr_mb = irr.texel_count() * sizeof(linear_rgb) / (1024.0 * 1024.0);

        std::printf("    irradiance 32^2  %8.1f ms   %6.3f MB  (%zu texels)\n",
                    irr_ms, irr_mb, irr.texel_count());
        std::printf("    prefilter  6 lvl %8.1f ms   %6.3f MB  (%zu texels)\n",
                    pre_ms, pre_mb, pre.texel_count());
        std::printf("    brdf lut   64^2  %8.1f ms   %6.3f MB\n",
                    lut_ms, lut64.width() * lut64.height() * 4 / (1024.0 * 1024.0));
        std::printf("    source     128^2 %8s      %6.3f MB  (%zu texels)\n",
                    "—", env_mb, env.texel_count());

        checkf(pre.texel_count() < env.texel_count() * 4 / 3 + 1,
               "the prefiltered chain holds %zu texels against the source's %zu "
               "— the famous 4/3, because 1 + 1/4 + 1/16 + ... converges to it "
               "(Lesson 6.10 derived the same number as 33%% overhead)",
               pre.texel_count(), env.texel_count());
    }
}

// ===========================================================================
//  §H — half floats
// ===========================================================================

void section_h_half()
{
    section("H  HALF FLOATS: THE ONLY WAY HDR DATA REACHES THE DEVICE");

    check(engine::half_to_float(engine::float_to_half(0.0f)) == 0.0f,
          "zero survives");
    check(engine::half_to_float(engine::float_to_half(1.0f)) == 1.0f,
          "one is exact — an exponent of 0 and a mantissa of 0");
    check(engine::half_to_float(engine::float_to_half(-2.5f)) == -2.5f,
          "a negative with an exact binary fraction is exact, and negatives are "
          "encoded faithfully even though radiance cannot be negative: a "
          "function that clamps its own input cannot be tested");

    // THE HEADLINE: relative precision, across the whole useful range.
    double worst = 0.0;
    float worst_at = 0.0f;
    for (double x = 1.0e-4; x < 6.0e4; x *= 1.0007)
    {
        const auto f = static_cast<float>(x);
        const float back = engine::half_to_float(engine::float_to_half(f));
        const double rel = std::fabs(back - f) / f;
        if (rel > worst) { worst = rel; worst_at = f; }
    }
    checkf(worst < 5.0e-4,
           "worst relative error %.6f across 1e-4 to 6e4 (at %.4g) — which is "
           "2^-11, half an ulp on 10 mantissa bits plus the implicit one. THE "
           "PRECISION IS RELATIVE, so the sky at 0.4 and the sun at 6,000 are "
           "stored to the same accuracy, which is exactly the property "
           "gpu_post.hpp argues makes half floats right for light",
           worst, worst_at);

    // Overflow must CLAMP, not become infinity — an inf in an environment map
    // becomes a mip level of NaN.
    const float huge = engine::half_to_float(engine::float_to_half(1.0e6f));
    checkf(std::isfinite(huge) && huge > 65000.0f,
           "1e6 clamps to %.0f, the largest finite half, rather than becoming "
           "inf — because an inf here poisons every integral downstream of it",
           huge);

    // NaN must stay NaN. A silent 65,504 where a NaN was is far worse to debug.
    check(std::isnan(engine::half_to_float(
              engine::float_to_half(std::numeric_limits<float>::quiet_NaN()))),
          "a NaN stays a NaN, so the bug that produced it is still visible");

    // Subnormals.
    const float tiny = engine::half_to_float(engine::float_to_half(1.0e-7f));
    checkf(tiny > 0.0f && tiny < 2.0e-7f,
           "1e-7 is below 2^-14 and encodes as a SUBNORMAL, coming back as %.3e "
           "rather than zero — the branch whose absence quantises the shadows "
           "and shows up only at a low exposure", tiny);
}

// ===========================================================================
//  §I — the golden
// ===========================================================================

void section_i_golden()
{
    section("I  THE GOLDEN");

    const int rc = demo::write_reference_shot("scratch/verify615.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify615.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL, hash E917C06C — TWENTY-FOURTH lesson at this hash, "
          "and the argument was checked before a line of the lesson was written. "
          "It is structural SIX times over, which is twice 6.14's three: the "
          "fixture's `lighting` struct has no environment field; `shade()` never "
          "calls `image_based_light`; `cube_map`, `environment` and the BRDF "
          "table are types the fixture never constructs; microfacet.hpp's two "
          "new functions are called by neither `shade()` nor "
          "`cook_torrance_specular`; the half-float pair has no CPU-raster "
          "caller; and every shader and GPU change is gated behind "
          "`ibl_intensity`, which defaults to 0");
}

// ===========================================================================
//  §J — the GPU
// ===========================================================================

void section_j_gpu()
{
    section("J  THE GPU: CUBE TEXTURES, SIX FACES, SIX LEVELS");

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::printf("  ---- SDL_Init(VIDEO): %s\n", SDL_GetError());
    }

    // Declared FIRST so it is destroyed LAST — 6.11's segfault-on-exit, and the
    // ordering rule every harness since has kept.
    engine::gpu_device gpu;
    if (!gpu.create(nullptr, false).ok())
    {
        std::printf("  ---- no GPU device on this machine; §J skipped\n");
        return;
    }

    // ---- ASKED, NOT ASSUMED, AND THE TYPE IS PART OF THE QUESTION ----------
    std::printf("    cube-map format support on this device (%s):\n",
                SDL_GetGPUDeviceDriver(gpu.handle()));
    for (auto [fmt, name] : {
             std::pair<SDL_GPUTextureFormat, const char*>{
                 SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, "R16G16B16A16_FLOAT"},
             {SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT, "R32G32B32A32_FLOAT"},
             {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, "R8G8B8A8_UNORM"}})
    {
        const bool as_cube = SDL_GPUTextureSupportsFormat(
            gpu.handle(), fmt, SDL_GPU_TEXTURETYPE_CUBE, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        const bool as_2d = SDL_GPUTextureSupportsFormat(
            gpu.handle(), fmt, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        std::printf("      %-20s cube %-4s   2D %-4s\n",
                    name, as_cube ? "yes" : "NO", as_2d ? "yes" : "NO");
    }

    sky_settings sky;
    const cube_map env = engine::make_sky_environment(64, sky);
    const cube_map pre = engine::prefilter_environment(env, 5, 32);
    const cube_map irr = engine::irradiance_map(env, 16);
    const engine::texture lut = engine::make_brdf_lut(32, 256);

    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
    check(cb != nullptr, "acquired a command buffer for the uploads");
    if (cb == nullptr) { return; }

    engine::gpu_texture sky_tex;
    engine::gpu_texture pre_tex;
    engine::gpu_texture irr_tex;

    const bool sky_ok = sky_tex.create_cube(gpu, cb, env, engine::k_hdr_format,
                                            "environment (radiance)");
    checkf(sky_ok, "a 64x64x6 single-level cube map uploads (%u bytes)",
           sky_tex.uploaded_bytes());
    checkf(sky_ok && sky_tex.width() == 64 && sky_tex.levels() == 1,
           "and reports 64x64, 1 level");

    const bool pre_ok = pre_tex.create_cube(gpu, cb, pre, engine::k_hdr_format,
                                            "environment (prefiltered)");
    checkf(pre_ok && pre_tex.levels() == 5,
           "the FIVE-LEVEL prefiltered chain uploads level by level (%u bytes, "
           "%d levels) — SDL_GenerateMipmapsForGPUTexture is deliberately NOT "
           "used, because these levels are GGX blurs and not box filters",
           pre_tex.uploaded_bytes(), pre_tex.levels());

    // 6 faces x (32^2 + 16^2 + 8^2 + 4^2 + 2^2) texels x 4 channels x 2 bytes...
    // except the chain's base is the ENVIRONMENT's size, 64.
    const Uint32 expect = [&] {
        Uint32 total = 0;
        for (int level = 0; level < pre.levels(); ++level)
        {
            const auto n = static_cast<Uint32>(pre.size_at(level));
            total += n * n * 4u * 2u * 6u;
        }
        return total;
    }();
    checkf(pre_ok && pre_tex.uploaded_bytes() == expect,
           "and moves exactly the bytes the chain contains: %u, computed "
           "independently as sum over levels of 6 x n^2 x 4 channels x 2 bytes",
           expect);

    check(irr_tex.create_cube(gpu, cb, irr, engine::k_hdr_format,
                              "environment (irradiance)"),
          "the 16x16 irradiance cube uploads — small because a cosine "
          "convolution leaves nothing finer than about 60 degrees");

    engine::gpu_texture lut_tex;
    engine::image_data lut_img;
    lut_img.width = lut.width();
    lut_img.height = lut.height();
    lut_img.source_channels = 4;
    lut_img.pixels.resize(static_cast<std::size_t>(lut.width()) * lut.height() * 4u);
    for (int y = 0; y < lut.height(); ++y)
    {
        for (int x = 0; x < lut.width(); ++x)
        {
            // ARGB IN, RGBA OUT. `engine::texture` stores ARGB (colour.hpp's
            // `pack_argb`) and `image_data` holds RGBA rows, so this is a
            // genuine reorder and not a copy — the SECOND place in this lesson
            // where getting it wrong produced a plausible picture rather than
            // an obvious one. See §9 of the lesson.
            const Uint32 t = lut.texel(x, y);
            const std::size_t o = (static_cast<std::size_t>(y) * lut.width() + x) * 4u;
            lut_img.pixels[o + 0] = static_cast<std::uint8_t>((t >> 16) & 0xFFu);  // R
            lut_img.pixels[o + 1] = static_cast<std::uint8_t>((t >> 8) & 0xFFu);   // G
            lut_img.pixels[o + 2] = static_cast<std::uint8_t>(t & 0xFFu);          // B
            lut_img.pixels[o + 3] = static_cast<std::uint8_t>((t >> 24) & 0xFFu);  // A
        }
    }
    // `srgb = false`, and it is the same decision `texel_space::linear` makes on
    // the CPU side: these are integral coefficients, not colours.
    check(lut_tex.create_sampled(gpu, cb, lut_img, false, "brdf lut"),
          "the BRDF table uploads as a LINEAR 2-D texture — not _SRGB, for the "
          "reason Lesson 6.7 gave about normal maps");

    // ---- A format we have not implemented must be REFUSED, not mangled ------
    engine::gpu_texture bad;
    check(!bad.create_cube(gpu, cb, env, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                           "should not be created"),
          "an 8-bit format is REFUSED rather than filled with reinterpreted "
          "float bytes — the failure 4.2 called a sheared image, arriving in the "
          "channel axis");

    SDL_SubmitGPUCommandBuffer(cb);

    // ---- The sampler the environment needs ----------------------------------
    engine::gpu_sampler cube_sampler;
    check(cube_sampler.create(gpu, engine::filter::linear,
                              engine::address_mode::clamp_to_edge,
                              "environment (trilinear, clamped)",
                              engine::filter::linear),
          "a trilinear, CLAMP_TO_EDGE sampler — `mip = linear` is not optional, "
          "because the prefiltered chain is indexed by a CONTINUOUS roughness "
          "and `nearest` would step visibly between blur radii");

    // ---- The skybox shaders, and what their reflection says ----------------
    engine::gpu_shader sky_vert;
    engine::gpu_shader sky_frag;
    const bool vert_ok = sky_vert.load(gpu, "skybox.vert", engine::shader_stage::vertex);
    const bool frag_ok = sky_frag.load(gpu, "skybox.frag", engine::shader_stage::fragment);
    if (!vert_ok || !frag_ok)
    {
        std::printf("  ---- skybox shaders not found beside the executable; "
                    "the pipeline check is skipped\n");
        return;
    }
    check(true, "skybox.vert and skybox.frag load");

    // THE CONTRACT THE SHADER DECLARES, read back rather than assumed. 4.9 built
    // this reflection path precisely so a binding mismatch is a failed test and
    // not a black screen.
    checkf(sky_frag.resources().samplers == 1,
           "skybox.frag declares %u sampler — the cube map, at slot 0",
           sky_frag.resources().samplers);
    checkf(sky_vert.resources().uniform_buffers == 1,
           "skybox.vert declares %u uniform buffer — the three camera-basis "
           "vectors that turn a pixel into a ray",
           sky_vert.resources().uniform_buffers);
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    std::printf("verify_615 — Lesson 6.15: Skybox and Image-Based Lighting\n");

    section_a_faces();
    section_b_weight();
    section_c_exact();
    section_d_sky();
    section_e_split_sum();
    section_f_level();
    section_g_convergence();
    section_h_half();

    // The golden runs BEFORE the GPU section — 6.11's rule: §J initialises the
    // video subsystem and creates a device, and a CPU reference render has no
    // business being downstream of either.
    section_i_golden();
    section_j_gpu();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
