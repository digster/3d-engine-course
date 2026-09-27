// scratch/verify_614.cpp — Lesson 6.14's harness: two problems, two prefilters.
//
//   §A  THE LOBE — GGX's half-width in closed form, against a bisection
//   §B  THE CROSSOVER — where shading aliasing begins, and why MSAA cannot help
//   §C  THE RESOLVE — averaging light, not bytes, for the fourth time
//   §D  SUPERSAMPLING — against a ground truth that is analytic, not rendered
//   §E  SPECULAR AA — the measurement that reframes what antialiasing IS
//   §F  MSAA vs SSAA — what they differ by, which is exactly the shading term
//   §G  THE GOLDEN — a new capability must not move the old picture
//   §H  THE GPU — MSAA targets, pipelines, and the resolve
//
// §E IS THE ONE THAT MATTERS, and it is the most surprising result in the module.
// Filtering the NDF barely improves ACCURACY — at roughness 0.20 it makes the
// per-pixel error slightly worse — while reducing the frame-to-frame SWING by
// three orders of magnitude. Antialiasing is not the pursuit of a more correct
// pixel. It is the removal of frequencies that cannot be represented, and the
// price is detail you agree to lose.
//
// Build and run:  sh scratch/build_verify_614.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/antialias.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_mesh.hpp>
#include <engine/gfx/gpu_post.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/gpu_uniform.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/microfacet.hpp>
#include <engine/math/vec3.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
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

using engine::linear_rgb;
using engine::microsurface;
using engine::vec3;

constexpr double k_pi = 3.14159265358979323846;

std::string read_file(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// GGX, for the bisection in §A. Written out rather than reused so the check
/// compares two INDEPENDENT routes to the same number.
double ggx_raw(double cos_h, double alpha)
{
    const double a2 = alpha * alpha;
    const double d = cos_h * cos_h * (a2 - 1.0) + 1.0;
    return a2 / (k_pi * d * d);
}

double bisect_half_angle(double alpha)
{
    const double peak = ggx_raw(1.0, alpha);
    double lo = 0.0;
    double hi = k_pi / 2.0;
    for (int i = 0; i < 200; ++i)
    {
        const double mid = 0.5 * (lo + hi);
        if (ggx_raw(std::cos(mid), alpha) > peak * 0.5) { lo = mid; } else { hi = mid; }
    }
    return hi;
}

engine::lighting probe_lighting()
{
    engine::lighting lights;
    const double ce = std::cos(0.70);
    const vec3 to_light{static_cast<float>(ce * std::sin(0.85)),
                        static_cast<float>(std::sin(0.70)),
                        static_cast<float>(ce * std::cos(0.85))};
    lights.key.direction = to_light * -1.0f;
    lights.key.colour = {1.0f, 1.0f, 1.0f};
    lights.key.irradiance = engine::k_reference_irradiance;
    lights.ambient = {0.0f, 0.0f, 0.0f};
    return lights;
}

// ===========================================================================
//  §A — the lobe
// ===========================================================================

void section_a_lobe()
{
    section("A  THE SPECULAR LOBE, IN CLOSED FORM");

    std::printf("    roughness      bisected        closed form       abs error\n");
    double worst = 0.0;
    for (double r : {0.90, 0.50, 0.30, 0.10, 0.05, 0.02})
    {
        const double alpha = r * r;
        const double bisected = bisect_half_angle(alpha);
        const double closed = engine::ggx_lobe_half_angle(static_cast<float>(alpha));
        const double err = std::fabs(bisected - closed);
        worst = (err > worst) ? err : worst;
        std::printf("    %8.2f  %14.9f  %16.9f  %13.2e\n", r, bisected, closed, err);
    }
    checkf(worst < 1e-6,
           "the closed form agrees with a bisection of the real NDF to %.2e across "
           "roughness 0.02 to 0.90 — EXACT, not a small-angle fit. Two independent "
           "routes to one number is what makes this a check rather than a "
           "restatement", worst);

    // AND THE FOLKLORE, CORRECTED.
    const double coeff = engine::ggx_lobe_half_angle(0.01f) / 0.01;
    checkf(std::fabs(coeff - std::sqrt(std::sqrt(2.0) - 1.0)) < 1e-4,
           "the coefficient is sqrt(sqrt(2) - 1) = %.6f, not 1. 'The lobe is about "
           "alpha wide' is the usual hand-wave and it overstates the width by "
           "%.0f%%", coeff, 100.0 * (1.0 / coeff - 1.0));
}

// ===========================================================================
//  §B — the crossover
// ===========================================================================

void section_b_crossover()
{
    section("B  WHERE SHADING ALIASING BEGINS");

    // A sphere 40 px in radius. A closed surface sweeps its normal through a
    // quarter turn from the centre of its silhouette to the rim, so the average
    // turn per pixel is (pi/2) / radius.
    const float turn = static_cast<float>(k_pi / 2.0) / 40.0f;
    std::printf("    a sphere 40 px in radius turns its normal %.6f rad per pixel\n",
                static_cast<double>(turn));
    std::printf("    roughness   lobe half-width   lobe / turn   verdict\n");
    for (float r : {0.50f, 0.30f, 0.20f, 0.10f, 0.05f})
    {
        const float ratio = engine::lobe_to_variation_ratio(r, turn);
        std::printf("    %8.2f   %14.6f   %11.3f   %s\n", static_cast<double>(r),
                    static_cast<double>(engine::ggx_lobe_half_angle(engine::alpha_from_roughness(r))),
                    static_cast<double>(ratio), (ratio > 1.0f) ? "resolved" : "ALIASES");
    }

    // Bisect the crossover.
    float lo = 0.001f;
    float hi = 1.0f;
    for (int i = 0; i < 100; ++i)
    {
        const float mid = 0.5f * (lo + hi);
        if (engine::lobe_to_variation_ratio(mid, turn) > 1.0f) { hi = mid; } else { lo = mid; }
    }
    checkf(hi > 0.2f && hi < 0.3f,
           "the crossover is at roughness %.4f. Above it the lobe is wider than a "
           "pixel's worth of normal variation and the highlight is resolved; below "
           "it the highlight can fall BETWEEN sample points entirely, which is a "
           "sparkle rather than a staircase", static_cast<double>(hi));

    checkf(hi < 0.49f,
           "which places the demo's own roughness of 0.49 SAFELY ABOVE the "
           "crossover — and every polished material Lesson 6.12 introduced "
           "(0.20, 0.10, 0.05) safely below it. The module has been walking "
           "toward this number since 6.12 put a 55,917 in the buffer");
}

// ===========================================================================
//  §C — the resolve
// ===========================================================================

void section_c_resolve()
{
    section("C  AVERAGING LIGHT, NOT BYTES — FOR THE FOURTH TIME");

    // A 2x2 supersampled pixel, half white and half black. This is the canonical
    // half-covered edge, built by hand so the answer has an arithmetic truth
    // rather than a rendered one.
    engine::framebuffer hi(2, 2);
    hi.put_pixel(0, 0, engine::pack_argb(255, 255, 255));
    hi.put_pixel(1, 0, engine::pack_argb(255, 255, 255));
    hi.put_pixel(0, 1, engine::pack_argb(0, 0, 0));
    hi.put_pixel(1, 1, engine::pack_argb(0, 0, 0));

    engine::framebuffer lo(1, 1);
    engine::resolve_supersampled(hi, lo, 2, /*encoded_average=*/false);
    const int linear_code = static_cast<int>(lo.pixel_at(0, 0) & 0xFFu);

    engine::framebuffer lo2(1, 1);
    engine::resolve_supersampled(hi, lo2, 2, /*encoded_average=*/true);
    const int encoded_code = static_cast<int>(lo2.pixel_at(0, 0) & 0xFFu);

    checkf(linear_code == 188,
           "a half-covered edge resolves to code %d in linear light. Half the "
           "light is half of 1.0, and the sRGB encode of 0.5 is 0.7354, which is "
           "code 188 — derived, then measured", linear_code);

    checkf(encoded_code == 127 || encoded_code == 128,
           "averaging the stored BYTES instead gives code %d, which is 6.1's "
           "mistake in its fourth costume — after 6.10's mip chains and 6.11's "
           "compositing", encoded_code);

    const float delivered = engine::to_linear(engine::pack_argb(
        static_cast<Uint8>(encoded_code), static_cast<Uint8>(encoded_code),
        static_cast<Uint8>(encoded_code))).r;
    checkf(delivered > 0.2f && delivered < 0.23f,
           "and code %d is a linear %.4f — so the wrong resolve delivers %.1f%% of "
           "the light instead of 50%%. The symptom is specific: antialiased edges "
           "come out TOO DARK, every silhouette grows a thin dark outline, and the "
           "usual guess is that edges are being blended twice",
           encoded_code, static_cast<double>(delivered),
           static_cast<double>(100.0f * delivered));
}

// ===========================================================================
//  §D — supersampling, against an analytic truth
// ===========================================================================

void section_d_supersampling()
{
    section("D  SUPERSAMPLING A SIGNAL ABOVE NYQUIST");

    // A CHECKERBOARD FINER THAN THE PIXEL GRID. Its true mean is exactly 0.5
    // whatever the phase — which is what makes this a ground truth rather than
    // another rendering. A point sample gets 0 or 1 depending on where it lands;
    // that is aliasing, in one line.
    const auto checker = [](double x, double y, double period) {
        const int cx = static_cast<int>(std::floor(x / period));
        const int cy = static_cast<int>(std::floor(y / period));
        return ((cx + cy) & 1) != 0;
    };

    const double period = 0.37;      // well under one pixel
    const int w = 64;
    const int h = 64;

    // THE STATISTIC HAS TO BE PER PIXEL. The first version of this measurement
    // took the mean over the whole image and found it already correct at 1x —
    // because errors of opposite sign cancel across 4,096 pixels whose phases
    // differ. That is not the absence of aliasing, it is the mean being blind to
    // it. What aliases is each PIXEL, so the statistic is the RMS deviation of
    // individual pixels from the true local mean of 0.5.
    std::printf("    a checker of period %.2f px — far above Nyquist. True local mean: 0.500\n",
                period);
    std::printf("    factor   samples/px   per-pixel RMS deviation   worst pixel\n");

    double prev_err = 1.0;
    bool monotone = true;
    for (int factor : {1, 2, 4, 8})
    {
        engine::framebuffer hi(w * factor, h * factor);
        for (int y = 0; y < hi.height(); ++y)
        {
            for (int x = 0; x < hi.width(); ++x)
            {
                const double sx = (x + 0.5) / factor;
                const double sy = (y + 0.5) / factor;
                const bool on = checker(sx, sy, period);
                hi.put_pixel(x, y, on ? engine::pack_argb(255, 255, 255)
                                      : engine::pack_argb(0, 0, 0));
            }
        }

        engine::framebuffer lo(w, h);
        engine::resolve_supersampled(hi, lo, factor);

        // Back in LINEAR light — the space the truth lives in.
        double sq = 0.0;
        double worst = 0.0;
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const double d = static_cast<double>(engine::to_linear(lo.pixel_at(x, y)).r) - 0.5;
                sq += d * d;
                worst = std::fmax(worst, std::fabs(d));
            }
        }
        const double err = std::sqrt(sq / (w * h));
        std::printf("    %6d   %10d   %22.4f   %11.4f\n",
                    factor, engine::aa_samples(factor), err, worst);
        if (factor > 1 && err > prev_err) { monotone = false; }
        prev_err = err;
    }

    checkf(monotone,
           "the per-pixel error falls monotonically with the sample count, toward a "
           "local mean the sample grid cannot otherwise find. NOTE WHAT THIS DOES "
           "NOT SAY: the checker is still not RESOLVED at any factor — it cannot "
           "be, it is above Nyquist — it is merely no longer LYING about being "
           "something else. That distinction is the whole of §E as well");

    checkf(true,
           "and note the statistic. The MEAN over the whole image is already "
           "correct at 1x, because errors of opposite sign cancel across pixels "
           "whose phases differ — which is a measurement being blind to the thing "
           "it was pointed at, and is how the first draft of this check passed "
           "while measuring nothing");
}

// ===========================================================================
//  §E — specular AA, and what antialiasing actually is
// ===========================================================================

void section_e_specular()
{
    section("E  FILTERING THE NDF: ACCURACY vs STABILITY");

    const engine::lighting lights = probe_lighting();
    const vec3 n0{0.0f, 1.0f, 0.0f};
    const vec3 to_light = lights.key.to_light();
    const vec3 mirror = engine::normalised(to_light * -1.0f + n0 * (2.0f * dot(n0, to_light)));
    const float turn = static_cast<float>(k_pi / 2.0) / 40.0f;

    const auto shade_at = [&](float roughness, double angle) {
        const microsurface m{.roughness = roughness, .metallic = 1.0f};
        const vec3 nn{static_cast<float>(std::sin(angle)), static_cast<float>(std::cos(angle)), 0.0f};
        const linear_rgb c = engine::shade({1.0f, 1.0f, 1.0f}, nn, mirror, lights, m);
        return static_cast<double>(std::fmax(c.r, std::fmax(c.g, c.b)));
    };

    std::printf("    roughness   filtered   naive RMS   filtered RMS   naive swing   filt swing\n");

    double best_swing_gain = 0.0;
    double best_rms_gain = 0.0;
    for (float r : {0.30f, 0.20f, 0.10f, 0.05f})
    {
        const float fr = engine::filtered_roughness(r, vec3{turn, 0.0f, 0.0f},
                                                       vec3{0.0f, turn, 0.0f});

        double nsq = 0.0;
        double fsq = 0.0;
        double nlo = 1e30;
        double nhi = 0.0;
        double flo = 1e30;
        double fhi = 0.0;
        const int phases = 64;
        for (int p = 0; p < phases; ++p)
        {
            const double phase = (p / double(phases) - 0.5) * turn;

            // GROUND TRUTH FOR THIS PIXEL: the BRDF integrated over its own
            // footprint, by brute force. Per phase, NOT averaged over phases —
            // averaging over phases IS antialiasing and would flatter the naive
            // sample, which is exactly the mistake the first draft of this
            // measurement made.
            double truth = 0.0;
            for (int k = 0; k < 256; ++k)
            {
                truth += shade_at(r, phase + (k / 256.0 - 0.5) * turn);
            }
            truth /= 256.0;

            const double nv = shade_at(r, phase);
            const double fv = shade_at(fr, phase);
            nsq += ((nv - truth) / truth) * ((nv - truth) / truth);
            fsq += ((fv - truth) / truth) * ((fv - truth) / truth);
            nlo = std::fmin(nlo, nv); nhi = std::fmax(nhi, nv);
            flo = std::fmin(flo, fv); fhi = std::fmax(fhi, fv);
        }
        const double nrms = 100.0 * std::sqrt(nsq / phases);
        const double frms = 100.0 * std::sqrt(fsq / phases);
        const double nswing = (nlo > 0.0) ? nhi / nlo : 0.0;
        const double fswing = (flo > 0.0) ? fhi / flo : 0.0;

        std::printf("    %8.2f   %8.4f   %8.1f%%   %11.1f%%   %10.1fx   %9.1fx\n",
                    static_cast<double>(r), static_cast<double>(fr),
                    nrms, frms, nswing, fswing);

        if (r == 0.05f)
        {
            best_swing_gain = nswing / fswing;
            best_rms_gain = nrms / frms;
        }
    }

    checkf(best_swing_gain > 100.0,
           "at roughness 0.05 the filter reduces the sub-pixel SWING by %.0fx",
           best_swing_gain);

    checkf(best_rms_gain < 10.0,
           "and it improves the per-pixel ACCURACY by only %.1fx. THAT CONTRAST IS "
           "THE LESSON: antialiasing does not make a pixel correct, it makes it "
           "STABLE. The artefact was never 'this pixel has the wrong value' — it "
           "was 'this pixel changes violently when nothing in the scene did'",
           best_rms_gain);

    checkf(true,
           "which is why the right question about an AA technique is never 'how "
           "accurate is it' but 'what frequencies does it remove, and can the grid "
           "carry what is left'");
}

// ===========================================================================
//  §F — MSAA vs SSAA
// ===========================================================================

void section_f_msaa_vs_ssaa()
{
    section("F  WHAT MSAA AND SSAA DIFFER BY");

    const engine::lighting lights = probe_lighting();
    const vec3 n0{0.0f, 1.0f, 0.0f};
    const vec3 to_light = lights.key.to_light();
    const vec3 mirror = engine::normalised(to_light * -1.0f + n0 * (2.0f * dot(n0, to_light)));
    const float turn = static_cast<float>(k_pi / 2.0) / 40.0f;

    const auto shade_at = [&](float roughness, double angle) {
        const microsurface m{.roughness = roughness, .metallic = 1.0f};
        const vec3 nn{static_cast<float>(std::sin(angle)), static_cast<float>(std::cos(angle)), 0.0f};
        const linear_rgb c = engine::shade({1.0f, 1.0f, 1.0f}, nn, mirror, lights, m);
        return static_cast<double>(std::fmax(c.r, std::fmax(c.g, c.b)));
    };

    std::printf("    ON AN INTERIOR PIXEL — no silhouette, one primitive covering all\n");
    std::printf("    four sample positions — MSAA shades ONCE and replicates.\n\n");
    std::printf("    roughness   MSAA (1 shade)   SSAA (4 shades)   ratio\n");
    int disagreements = 0;
    for (float r : {0.50f, 0.20f, 0.10f, 0.05f})
    {
        const double one = shade_at(r, 0.37 * turn);
        double four = 0.0;
        for (int i = 0; i < 4; ++i) { four += shade_at(r, 0.37 * turn + (i - 1.5) * 0.25 * turn); }
        four *= 0.25;
        const double ratio = (four > 0.0) ? one / four : 0.0;
        std::printf("    %8.2f   %14.2f   %15.2f   %6.3fx\n",
                    static_cast<double>(r), one, four, ratio);
        if (ratio < 0.5 || ratio > 2.0) { ++disagreements; }
    }

    checkf(disagreements >= 2,
           "%d of 4 roughnesses disagree by more than 2x. THE GAP IS THE SHADING "
           "ALIASING, and MSAA cannot reach it BY CONSTRUCTION: multisampling "
           "coverage while shading once per primitive per pixel is the "
           "optimisation that makes MSAA cost ~1.3x rather than 4x. You cannot buy "
           "shading samples with a coverage feature", disagreements);

    // AND WHAT A RESOLVE DOES TO 6.13's FIREFLY.
    const double resolved = 55917.0 / 4.0;
    checkf(resolved / 6.3774 > 1000.0,
           "one sample of 6.12's 55,917 in a 4x pixel resolves to %.0f, which is "
           "still %.0fx above ACES's lid of 6.3774. MSAA divides a firefly by at "
           "most 4 and the lid is 8,769x below it — which is 6.13's clamp argument "
           "arriving from a different direction",
           resolved, resolved / 6.3774);
}

// ===========================================================================
//  §G — the golden
// ===========================================================================

void section_g_golden()
{
    section("G  THE GOLDEN");

    const int rc = demo::write_reference_shot("scratch/verify614.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify614.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL, hash E917C06C — TWENTY-THIRD lesson at this hash, and "
          "the argument was checked BEFORE any code was written rather than "
          "afterwards. It is structural three times over: supersampling is a "
          "resolve between two `framebuffer`s the fixture does not allocate; "
          "specular AA is a function that produces a ROUGHNESS, applied by the "
          "caller, so `shade()` and `fill_style` never change; and MSAA is a "
          "property of a GPU target in a fixture that is CPU-only");
}

// ===========================================================================
//  §H — the GPU
// ===========================================================================

void section_h_gpu()
{
    section("H  THE GPU: MSAA TARGETS, PIPELINES AND THE RESOLVE");

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::printf("  ---- SDL_Init(VIDEO): %s\n", SDL_GetError());
    }

    // Nothing below calls destroy(): `gpu` is declared first so it is destroyed
    // LAST, which is the order every object after it needs. 6.11's harness
    // segfaulted on exit for exactly this reason.
    engine::gpu_device gpu;
    if (!gpu.create(nullptr, false).ok())
    {
        std::printf("  ---- no GPU device on this machine; §H skipped\n");
        return;
    }

    // ---- What this machine can actually do ---------------------------------
    //
    // ASKED, NOT ASSUMED. `SDL_GPUTextureSupportsSampleCount` exists because MSAA
    // support is per FORMAT, and 4x on a float target is common but not
    // guaranteed. A lesson that hard-coded 4x would be teaching a bug.
    std::printf("    sample-count support on this device:\n");
    SDL_GPUSampleCount best = SDL_GPU_SAMPLECOUNT_1;
    for (auto [n, name] : {std::pair<SDL_GPUSampleCount, const char*>{SDL_GPU_SAMPLECOUNT_2, "2x"},
                           {SDL_GPU_SAMPLECOUNT_4, "4x"},
                           {SDL_GPU_SAMPLECOUNT_8, "8x"}})
    {
        const bool hdr_ok = SDL_GPUTextureSupportsSampleCount(gpu.handle(), engine::k_hdr_format, n);
        const bool ldr_ok = SDL_GPUTextureSupportsSampleCount(
            gpu.handle(), SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, n);
        std::printf("      %-3s  R16G16B16A16_FLOAT %-3s   R8G8B8A8_UNORM %-3s\n",
                    name, hdr_ok ? "yes" : "NO", ldr_ok ? "yes" : "NO");
        if (hdr_ok) { best = n; }
    }
    check(true, "the device was asked rather than assumed — support is per format");

    if (best == SDL_GPU_SAMPLECOUNT_1)
    {
        std::printf("  ---- this device supports no MSAA on the HDR format; rest of §H skipped\n");
        return;
    }

    engine::gpu_shader scene_vs;
    engine::gpu_shader scene_fs;
    engine::gpu_shader post_vs;
    engine::gpu_shader tone_fs;
    engine::gpu_shader bright_fs;
    engine::gpu_shader down_fs;
    engine::gpu_shader up_fs;
    if (!scene_vs.load(gpu, "scene.vert", engine::shader_stage::vertex)
        || !scene_fs.load(gpu, "scene.frag", engine::shader_stage::fragment)
        || !post_vs.load(gpu, "fullscreen.vert", engine::shader_stage::vertex)
        || !tone_fs.load(gpu, "tonemap.frag", engine::shader_stage::fragment)
        || !bright_fs.load(gpu, "bloom_bright.frag", engine::shader_stage::fragment)
        || !down_fs.load(gpu, "bloom_down.frag", engine::shader_stage::fragment)
        || !up_fs.load(gpu, "bloom_up.frag", engine::shader_stage::fragment))
    {
        std::printf("  ---- shaders did not load; §H skipped\n");
        return;
    }

    // ---- A multisample colour target ---------------------------------------
    engine::gpu_texture ms;
    check(ms.create_colour_target(gpu, engine::k_hdr_format, 64, 64, "verify614 msaa",
                                  /*sampled=*/true, best),
          "a multisample colour target was created");
    checkf(ms.multisampled(),
           "and it reports %d samples — note that `sampled` was requested and "
           "SILENTLY DROPPED, because a multisample texture cannot be sampled at "
           "all. It is resolved into a single-sample texture and THAT is what a "
           "later pass reads", 1 << static_cast<int>(ms.samples()));

    // ---- The stack, at that sample count ------------------------------------
    engine::gpu_post_stack stack;
    check(stack.create(gpu, post_vs.handle(), tone_fs.handle(),
                       bright_fs.handle(), down_fs.handle(), up_fs.handle(),
                       SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
          "the post stack");

    engine::bloom_settings bs;
    check(stack.resize(gpu, 64, 64, bs, best),
          "the stack sized with MSAA: a multisample scene target AND the "
          "single-sample one it resolves into. 6.13's ownership rule needed no "
          "rewrite — the RESOLVED target is what crosses between stages, so the "
          "stack owns it; the multisample target is an intermediate of the scene "
          "pass that nothing downstream ever sees");

    checkf(stack.scene_target() != stack.resolved_target(),
           "and `scene_target()` and `resolved_target()` are now DIFFERENT "
           "textures — which is the whole shape of an MSAA frame, and the thing a "
           "caller must not get backwards");

    const SDL_GPUColorTargetInfo ci = stack.scene_target_info(SDL_FColor{0, 0, 0, 1});
    check(ci.store_op == SDL_GPU_STOREOP_RESOLVE && ci.resolve_texture == stack.resolved_target(),
          "the scene pass's target info carries STOREOP_RESOLVE and points at the "
          "resolve texture. RESOLVE rather than RESOLVE_AND_STORE: the SDL3 header "
          "says the first lets the driver DISCARD the multisample memory and is "
          "'the most performant method', and nothing here reads per-sample data");

    // ---- Pipelines are a different set at a different sample count ----------
    engine::gpu_scene_renderer scene_1x;
    engine::gpu_scene_renderer scene_4x;
    check(scene_1x.create(gpu, scene_vs.handle(), scene_fs.handle(),
                          SDL_GPU_TEXTUREFORMAT_D32_FLOAT, engine::k_hdr_format),
          "nine scene pipelines at 1x");
    check(scene_4x.create(gpu, scene_vs.handle(), scene_fs.handle(),
                          SDL_GPU_TEXTUREFORMAT_D32_FLOAT, engine::k_hdr_format, best),
          "and NINE MORE at the MSAA sample count — a pipeline's multisample state "
          "is baked in at creation exactly as its colour format is (4.4), so MSAA "
          "is not a different argument to the same nine pipelines, it is a "
          "different nine pipelines. The count this lesson leaves behind is 22");

    check(scene_4x.ensure_depth(gpu, 64, 64, best),
          "and a depth target at the MATCHING sample count — a hard requirement, "
          "not a convention: every attachment in a pass shares its sample "
          "positions, so a 4x colour target beside a 1x depth target is a pass "
          "that cannot be begun");
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    std::printf("verify_614 — Lesson 6.14: Antialiasing\n");

    section_a_lobe();
    section_b_crossover();
    section_c_resolve();
    section_d_supersampling();
    section_e_specular();
    section_f_msaa_vs_ssaa();

    // The golden runs BEFORE the GPU section, which 6.11 established: §H
    // initialises the video subsystem and creates a device, and a CPU reference
    // render has no business being downstream of either.
    section_g_golden();
    section_h_gpu();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
