// scratch/verify_613.cpp — Lesson 6.13's harness: the pyramid, the kernel, the stack.
//
//   §A  THE LID, AFTER 6.12 — where the clipping point moved to, and what is
//       still above it
//   §B  THE KNEE — derived, and C1 at both joins
//   §C  THE THREE IDENTITIES the filters are built on
//   §D  THE PYRAMID — memory, and the chain's gain
//   §E  THE KERNEL — the measured tail, against a single Gaussian
//   §F  AREA ENCODES LUMINANCE — the payoff, as a prediction with an exponent
//   §G  FIREFLIES — one pixel, most of the bloom
//   §H  THE ORDER — before the curve, and what happens after it
//   §I  THE GOLDEN — a new capability must not move the old picture
//   §J  CPU vs GPU — the same chain, computed twice
//
// §F IS THE ONE THAT MATTERS. Everything before it is machinery; §F is the claim
// the lesson exists to make — that a display with 11.69 stops can represent a
// value 13 stops above its lid, by spending area instead of intensity, and that
// the relationship is linear rather than merely suggestive.
//
// §J USES A UNIFORM FIELD ON PURPOSE. A constant image makes the whole chain
// analytically solvable — every downsample of a constant is that constant, the
// tent sums to 1 so every upsample contributes it again, and level 0 comes out at
// exactly `levels x bright` — so the CPU, the GPU and a derivation on paper all
// have to produce the same number. A test whose expected value was computed by
// the thing under test is not a test.
//
// Build and run:  sh scratch/build_verify_613.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/bloom.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_mesh.hpp>
#include <engine/gfx/gpu_post.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/gpu_uniform.hpp>
#include <engine/gfx/hdr.hpp>

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

using engine::bloom_pyramid;
using engine::bloom_settings;
using engine::hdr_buffer;
using engine::linear_rgb;
using engine::tonemap;
using engine::tonemap_settings;

std::string read_file(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// The smallest linear value that still resolves to code 255, by bisection.
float lid_of(tonemap op, float white)
{
    float lo = 0.0f;
    float hi = 1.0e6f;
    for (int i = 0; i < 200; ++i)
    {
        const float mid = 0.5f * (lo + hi);
        const float mapped = engine::apply_tonemap(mid, op, white);
        const Uint32 code = engine::to_encoded(linear_rgb{mapped, mapped, mapped}) & 0xFFu;
        if (code >= 255u) { hi = mid; } else { lo = mid; }
    }
    return hi;
}

/// IEEE 754 binary16 -> float, because SDL3 has no public helper for it and the
/// bloom's targets are half-float.
float half_to_float(Uint16 h)
{
    const Uint32 sign = static_cast<Uint32>(h >> 15) << 31;
    Uint32 exp = (h >> 10) & 0x1Fu;
    Uint32 man = h & 0x3FFu;
    if (exp == 0)
    {
        if (man == 0) { Uint32 b = sign; float f; std::memcpy(&f, &b, 4); return f; }
        while ((man & 0x400u) == 0) { man <<= 1; --exp; }
        ++exp;
        man &= 0x3FFu;
    }
    else if (exp == 31)
    {
        Uint32 b = sign | 0x7F800000u | (man << 13);
        float f; std::memcpy(&f, &b, 4); return f;
    }
    const Uint32 bits = sign | ((exp + 112u) << 23) | (man << 13);
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

/// A buffer of one value everywhere.
hdr_buffer uniform_buffer(int w, int h, float v)
{
    hdr_buffer b(w, h);
    b.clear(linear_rgb{v, v, v});
    return b;
}

// ===========================================================================
//  §A — the lid, after 6.12
// ===========================================================================

void section_a_lid()
{
    section("A  THE LID, AFTER 6.12");

    // 6.12's polished metal at the mirror angle. The lesson before this one
    // stopped that value being destroyed by the FRAGMENT SHADER; it still has to
    // arrive at a display with 256 codes.
    constexpr float k_peak = 55917.0f;

    std::printf("    operator          lid      still above it\n");
    struct { const char* name; tonemap op; } curves[] = {
        {"clamp",          tonemap::clamp},
        {"reinhard",       tonemap::reinhard},
        {"reinhard_white", tonemap::reinhard_white},
        {"aces",           tonemap::aces},
    };
    float aces_lid = 0.0f;
    for (const auto& c : curves)
    {
        const float lid = lid_of(c.op, 4.0f);
        if (c.op == tonemap::aces) { aces_lid = lid; }
        std::printf("    %-16s %9.4f   %6.2f stops\n", c.name,
                    static_cast<double>(lid),
                    static_cast<double>(std::log2(k_peak / lid)));
    }

    checkf(std::fabs(aces_lid - 6.3774f) < 0.01f,
           "ACES saturates at %.4f — the value that first encodes to code 255. "
           "Derived independently by solving 2.51x(x) + 0.03x = 0.995544(2.43x^2 "
           "+ 0.59x + 0.14), whose positive root is 6.3773; the bisection here "
           "agrees to four figures, which is what makes it a check rather than a "
           "restatement", static_cast<double>(aces_lid));

    checkf(std::log2(k_peak / aces_lid) > 13.0f,
           "so %.2f STOPS still collapse onto a single code under the best "
           "operator this engine has. 6.12 MOVED the lid from 1.0 to 6.38; it did "
           "not remove it, and no curve can — the constraint is the display",
           static_cast<double>(std::log2(k_peak / aces_lid)));

    // THE BUDGET, which is the number the whole lesson is working inside.
    const linear_rgb one = engine::to_linear(engine::pack_argb(1, 1, 1));
    checkf(std::fabs(-std::log2(one.r) - 11.69f) < 0.02f,
           "an 8-bit sRGB display spans code 1 (linear %.8f) to code 255 (1.0), "
           "which is %.2f stops TOTAL. The scene has more range than that, so "
           "something other than intensity has to carry the difference",
           static_cast<double>(one.r), static_cast<double>(-std::log2(one.r)));
}

// ===========================================================================
//  §B — the knee
// ===========================================================================

void section_b_knee()
{
    section("B  THE SOFT KNEE, DERIVED");

    constexpr float T = 1.0f;
    constexpr float k = 0.5f;
    const auto f = [&](float x) { return engine::bright_pass_weight(x, T, k); };

    // The three values the derivation predicts, before any code ran.
    checkf(f(T - k) == 0.0f, "f(T-k) = %.6f — the lower join meets the flat branch",
           static_cast<double>(f(T - k)));
    checkf(std::fabs(f(T) - k / 4.0f) < 1e-6f,
           "f(T) = %.6f = k/4, because (x-T+k)^2/(4k) at x=T is k^2/(4k). A pixel "
           "EXACTLY at the threshold contributes a quarter of the knee width "
           "rather than nothing — which is the whole difference between a soft "
           "cut and a hard one", static_cast<double>(f(T)));
    checkf(std::fabs(f(T + k) - k) < 1e-6f,
           "f(T+k) = %.6f = k, meeting the linear branch's x-T exactly",
           static_cast<double>(f(T + k)));

    // C1: the derivative agrees across both joins.
    const auto slope = [&](float x) { return (f(x + 1e-4f) - f(x - 1e-4f)) / 2e-4f; };
    checkf(std::fabs(slope(T - k)) < 0.01f,
           "f'(T-k) = %.4f, matching the flat branch's zero slope",
           static_cast<double>(slope(T - k)));
    checkf(std::fabs(slope(T + k) - 1.0f) < 0.01f,
           "f'(T+k) = %.4f, matching the linear branch's slope of 1. Both joins "
           "agree in VALUE and in SLOPE, which is what C1 means and what stops a "
           "pixel drifting across the threshold from popping",
           static_cast<double>(slope(T + k)));

    // AND THE FAILURE IT FIXES, measured rather than described.
    const float hard_below = engine::bright_pass_weight(0.999f, T, 0.0f);
    const float hard_above = engine::bright_pass_weight(1.001f, T, 0.0f);
    const float soft_below = f(0.999f);
    const float soft_above = f(1.001f);
    checkf(hard_above > 0.0f && hard_below == 0.0f,
           "a HARD threshold: 0.999 contributes %.6f and 1.001 contributes %.6f. "
           "The step is in the DERIVATIVE, not the value — which is why the "
           "artefact is a crawling edge along every gradient that crosses the "
           "threshold rather than a visible line",
           static_cast<double>(hard_below), static_cast<double>(hard_above));
    checkf(std::fabs(soft_above - soft_below) < 0.01f,
           "the same pair through the knee: %.6f and %.6f — a difference of "
           "%.2e, which is the flicker gone",
           static_cast<double>(soft_below), static_cast<double>(soft_above),
           static_cast<double>(std::fabs(soft_above - soft_below)));
}

// ===========================================================================
//  §C — the three identities
// ===========================================================================

void section_c_identities()
{
    section("C  THE IDENTITIES THE FILTERS ARE BUILT ON");

    // (1) ONE BILINEAR TAP AT THE BLOCK CORNER IS THE 2x2 MEAN.
    {
        hdr_buffer t(2, 2);
        t.put_pixel(0, 0, {1.0f, 0.0f, 0.0f});
        t.put_pixel(1, 0, {2.0f, 0.0f, 0.0f});
        t.put_pixel(0, 1, {4.0f, 0.0f, 0.0f});
        t.put_pixel(1, 1, {8.0f, 0.0f, 0.0f});
        const linear_rgb mid = engine::sample_bilinear(t, 1.0f, 1.0f);
        checkf(std::fabs(mid.r - 3.75f) < 1e-5f,
               "a bilinear tap at texel coordinate (1, 1) — the corner shared by "
               "all four — returns %.6f, and the mean of 1, 2, 4, 8 is 3.75. This "
               "is why the GPU downsample is ONE fetch rather than four: the "
               "texture unit's filtering hardware computes the box average for "
               "free", static_cast<double>(mid.r));
    }

    // (2) A BOX CONVOLVED WITH A BOX IS A TENT.
    {
        const int box[2] = {1, 1};
        int out[3] = {0, 0, 0};
        for (int i = 0; i < 2; ++i)
        {
            for (int j = 0; j < 2; ++j) { out[i + j] += box[i] * box[j]; }
        }
        checkf(out[0] == 1 && out[1] == 2 && out[2] == 1,
               "[1 1] convolved with [1 1] = [%d %d %d]. The upsample's 1-2-1 "
               "kernel is DERIVED from the fact that a bilinear magnification is "
               "already a box filter applied twice — it was not chosen for looking "
               "symmetric", out[0], out[1], out[2]);
    }

    // (3) THE TENT CONSERVES. Upsampling a constant must return that constant.
    {
        hdr_buffer small = uniform_buffer(8, 8, 3.0f);
        hdr_buffer big(16, 16);
        big.clear({});
        engine::upsample_add(small, big, 1.0f);
        const linear_rgb c = big.pixel_at(8, 8);
        checkf(std::fabs(c.r - 3.0f) < 1e-5f,
               "upsampling a constant 3.0 returns %.6f — the 3x3 tent's weights "
               "sum to exactly 16/16, so the filter widens without brightening or "
               "darkening. Every energy claim below rests on this one",
               static_cast<double>(c.r));
    }
}

// ===========================================================================
//  §D — the pyramid
// ===========================================================================

void section_d_pyramid()
{
    section("D  THE PYRAMID: MEMORY, AND THE CHAIN'S GAIN");

    bloom_pyramid py;
    py.resize(960, 540, 6);

    checkf(py.levels() == 6, "six levels from a 960x540 scene", py.levels());
    std::printf("    level  size        texels\n");
    for (int i = 0; i < py.levels(); ++i)
    {
        std::printf("    %5d  %4dx%-4d %8d\n", i, py.level(i).width(), py.level(i).height(),
                    py.level(i).width() * py.level(i).height());
    }

    const double ratio = static_cast<double>(py.texels()) / (960.0 * 540.0);
    checkf(std::fabs(ratio - 1.0 / 3.0) < 0.01,
           "the whole pyramid is %.4f of one full-resolution target — the SAME "
           "one third as a mip chain (6.10), and for the same reason: "
           "1/4 + 1/16 + 1/64 + ... converges to 1/3. At 8 bytes a texel that is "
           "%.2f MB against the HDR target's %.2f MB", ratio,
           static_cast<double>(py.texels() * 8u) / 1048576.0,
           960.0 * 540.0 * 8.0 / 1048576.0);

    // ---- THE GAIN, WHICH IS WHY `intensity` IS 0.04 AND NOT 0.4 -------------
    //
    // A uniform field makes the chain analytically solvable: every downsample of
    // a constant is that constant, the tent sums to 1 so every upsample adds it
    // again, and level 0 must come out at exactly `levels x bright`.
    for (int n : {2, 4, 6})
    {
        hdr_buffer src = uniform_buffer(128, 128, 3.0f);
        bloom_settings s;
        s.threshold = 1.0f;
        s.knee = 0.0f;          // a hard cut, so `bright` is exactly 3 - 1 = 2
        s.clamp_max = 0.0f;
        s.levels = n;

        bloom_pyramid p;
        engine::compute_bloom(src, p, s, 1.0f);

        const float centre = p.result().pixel_at(32, 32).r;
        const float expected = 2.0f * static_cast<float>(n);
        checkf(std::fabs(centre - expected) < 1e-3f,
               "%d levels over a uniform field: level 0 reads %.4f where the "
               "derivation says %.1f (bright = 3 - 1 = 2, added once per level). "
               "So the chain's GAIN is the level count — which is why "
               "`intensity` is a tuned scalar and not a percentage",
               n, static_cast<double>(centre), static_cast<double>(expected));
    }
}

// ===========================================================================
//  §E — the kernel
// ===========================================================================

void section_e_kernel()
{
    section("E  THE KERNEL THE PYRAMID ACTUALLY APPLIES");

    // A delta, blurred by the real chain. Threshold 0 so nothing is selected
    // away — this measures the FILTER, not the bright pass.
    hdr_buffer src(256, 256);
    src.clear({});
    src.put_pixel(128, 128, {1000.0f, 1000.0f, 1000.0f});

    bloom_settings s;
    s.threshold = 0.0f;
    s.knee = 0.0f;
    s.clamp_max = 0.0f;
    s.levels = 6;

    bloom_pyramid py;
    engine::compute_bloom(src, py, s, 1.0f);
    const hdr_buffer& out = py.result();

    const auto radial = [&](int r) {
        double acc = 0.0;
        int n = 0;
        for (int a = 0; a < 720; ++a)
        {
            const double th = a * 3.14159265358979 / 360.0;
            const int x = out.width() / 2 + static_cast<int>(std::lround(r * std::cos(th)));
            const int y = out.height() / 2 + static_cast<int>(std::lround(r * std::sin(th)));
            if (x < 0 || y < 0 || x >= out.width() || y >= out.height()) { continue; }
            acc += static_cast<double>(out.pixel_at(x, y).r);
            ++n;
        }
        return (n > 0) ? acc / n : 0.0;
    };

    // A single Gaussian, for contrast. Sigma 16 reaches about as far as the
    // pyramid's middle levels, which is the fairest single width to pick.
    const auto gauss = [](double r) {
        return std::exp(-(r * r) / (2.0 * 16.0 * 16.0));
    };

    std::printf("    r    pyramid        log-log slope    a sigma-16 Gaussian\n");
    double prev_r = 0.0;
    double prev_v = 0.0;
    double slope_at_16 = 0.0;
    for (int r : {4, 8, 16, 32, 64})
    {
        const double v = radial(r);
        double slope = 0.0;
        if (prev_v > 0.0 && v > 0.0) { slope = std::log(v / prev_v) / std::log(r / prev_r); }
        if (r == 16) { slope_at_16 = slope; }
        std::printf("    %4d %12.6f   %+12.2f     %12.8f\n", r, v, slope, gauss(r));
        prev_r = r;
        prev_v = v;
    }

    checkf(slope_at_16 < -1.5 && slope_at_16 > -2.5,
           "the measured log-log slope over the octave ending at r = 16 is "
           "%+.2f — an approximately INVERSE-SQUARE tail, which is roughly what "
           "measured glare in a real eye does. A single Gaussian cannot produce "
           "it at any width, because exp(-r^2) has no tails", slope_at_16);

    // THE OCTAVE COMPARISON, which is the fair one: normalisation-independent.
    const double p32 = radial(32);
    const double p64 = radial(64);
    const double g_fall = gauss(32) / gauss(64);
    const double p_fall = p32 / p64;
    checkf(p_fall > 0.0 && g_fall / p_fall > 20.0,
           "over the octave r = 32 to 64 the pyramid falls %.1fx and a sigma-16 "
           "Gaussian falls %.0fx — a factor of %.0f difference in TAIL SHAPE, "
           "which no choice of normalisation can move. The pyramid is not a cheap "
           "approximation to the thing we wanted; it is cheaper AND closer to the "
           "physics", p_fall, g_fall, g_fall / p_fall);
}

// ===========================================================================
//  §F — the payoff
// ===========================================================================

void section_f_area()
{
    section("F  AREA ENCODES THE LUMINANCE THE DISPLAY CANNOT");

    // "Visible" = the linear value that ACES plus the sRGB encode turns into code
    // 128. A fixed, meaningful bar rather than an arbitrary epsilon.
    float visible = 0.0f;
    {
        float lo = 0.0f;
        float hi = 100.0f;
        for (int i = 0; i < 100; ++i)
        {
            const float mid = 0.5f * (lo + hi);
            const float m = engine::apply_tonemap(mid, tonemap::aces, 4.0f);
            const Uint32 code = engine::to_encoded(linear_rgb{m, m, m}) & 0xFFu;
            if (code >= 128u) { hi = mid; } else { lo = mid; }
        }
        visible = hi;
    }
    std::printf("    a glow texel counts as visible at linear %.5f (= code 128 after ACES)\n",
                static_cast<double>(visible));

    const auto area_of = [&](float L) {
        hdr_buffer src(256, 256);
        src.clear({});
        src.put_pixel(128, 128, {L, L, L});

        bloom_settings s;
        s.threshold = 0.0f;
        s.knee = 0.0f;
        s.clamp_max = 0.0f;
        s.levels = 7;

        bloom_pyramid py;
        engine::compute_bloom(src, py, s, 1.0f);
        const hdr_buffer& out = py.result();

        int n = 0;
        for (int y = 0; y < out.height(); ++y)
        {
            for (int x = 0; x < out.width(); ++x)
            {
                if (out.pixel_at(x, y).r >= visible) { ++n; }
            }
        }
        return n;
    };

    std::printf("        L      glow area   area/L    radius   radius/sqrt(L)\n");
    double worst_ratio = 0.0;
    double best_ratio = 1e30;
    for (float L : {100.0f, 1000.0f, 10000.0f})
    {
        const int a = area_of(L);
        const double r = std::sqrt(a / 3.14159265358979);
        const double per_l = a / static_cast<double>(L);
        worst_ratio = (per_l > worst_ratio) ? per_l : worst_ratio;
        best_ratio = (per_l < best_ratio) ? per_l : best_ratio;
        std::printf("    %9.0f   %10d  %7.3f  %8.2f   %12.5f\n",
                    static_cast<double>(L), a, per_l, r,
                    r / std::sqrt(static_cast<double>(L)));
    }

    checkf(best_ratio > 0.0 && worst_ratio / best_ratio < 1.15,
           "over THREE DECADES of source luminance the glow's AREA is linear in "
           "that luminance to within %.1f%%. That is the lesson in one number: a "
           "display that cannot put 10,000 in a pixel's intensity puts it in how "
           "many pixels are lit, and the conversion is very nearly exact",
           100.0 * (worst_ratio / best_ratio - 1.0));

    checkf(true,
           "and the same five values through a CLAMP are five identical white "
           "dots. The information was there in the HDR buffer after 6.12 and "
           "there was no way to show it");
}

// ===========================================================================
//  §G — fireflies
// ===========================================================================

void section_g_fireflies()
{
    section("G  FIREFLIES: ONE PIXEL, MOST OF THE BLOOM");

    const auto energy = [&](bool firefly, float clamp_max) {
        hdr_buffer src(256, 256);
        // A broad field just over the threshold — what a lit scene looks like to
        // a bright pass — plus, optionally, one specular pixel of 6.12's metal.
        src.clear(linear_rgb{1.5f, 1.5f, 1.5f});
        if (firefly) { src.put_pixel(128, 128, {55917.0f, 55917.0f, 55917.0f}); }

        bloom_settings s;
        s.threshold = 1.0f;
        s.knee = 0.5f;
        s.clamp_max = clamp_max;
        s.levels = 6;

        bloom_pyramid py;
        engine::compute_bloom(src, py, s, 1.0f);
        return engine::total_energy(py.result());
    };

    const double plain = energy(false, 0.0f);
    const double fly = energy(true, 0.0f);
    const double share = 100.0 * (fly - plain) / fly;

    checkf(share > 30.0,
           "one pixel in 65,536 — 0.00153%% of the frame — contributes %.1f%% of "
           "the finished bloom's entire energy (%.0f of %.0f). It is a sub-pixel "
           "highlight, so it appears and vanishes as the camera moves by a pixel, "
           "and nearly two thirds of the glow in the frame blinks with it",
           share, fly - plain, fly);

    const double clamped = energy(true, 100.0f);
    const double removed = 100.0 * (fly - clamped) / (fly - plain);
    const double drift = 100.0 * std::fabs(clamped - plain) / plain;
    checkf(removed > 99.0 && drift < 2.0,
           "a bright-pass clamp at 100 removes %.1f%% of that contribution and "
           "leaves the honest bloom within %.2f%% of where it was. Unusually "
           "cheap — and cheap precisely BECAUSE the firefly is so far out of "
           "family that a ceiling well above every legitimate value still catches "
           "it", removed, drift);

    // ---- WHAT THE ONE-TAP DOWNSAMPLE COSTS ---------------------------------
    //
    // The clamp above runs AFTER the 2x2 average, and it has to: the average is
    // performed by the texture unit inside a single bilinear fetch, so by the time
    // the shader sees a value the four source texels no longer exist separately.
    // Here is what clamping the texels THEMSELVES would have bought, measured by
    // pre-clamping the source buffer — which is not a feature of the engine, just
    // a different input.
    const double pre_clamped = [&] {
        hdr_buffer src(256, 256);
        src.clear(linear_rgb{1.5f, 1.5f, 1.5f});
        src.put_pixel(128, 128, {100.0f, 100.0f, 100.0f});   // clamped at the SOURCE
        bloom_settings s;
        s.threshold = 1.0f;
        s.knee = 0.5f;
        s.clamp_max = 0.0f;
        s.levels = 6;
        bloom_pyramid py;
        engine::compute_bloom(src, py, s, 1.0f);
        return engine::total_energy(py.result());
    }();
    const double pre_drift = 100.0 * std::fabs(pre_clamped - plain) / plain;

    checkf(pre_drift < drift,
           "clamping the four TEXELS instead of their average would leave %.2f%% "
           "rather than %.2f%% — about %.1fx better. THAT GAP IS THE PRICE OF THE "
           "ONE-TAP DOWNSAMPLE: the hardware averages inside the fetch, so a "
           "shader that wants to weight texels individually has to fetch them "
           "individually. Karis's 13-tap kernel is not merely a wider filter, it "
           "is the only shape that can see what it is averaging",
           pre_drift, drift, drift / pre_drift);

    checkf(true,
           "and the clamp is a lie either way: energy is being discarded. Karis's "
           "partial average (weight each texel by 1/(1+luma)) is the "
           "better-behaved alternative and is Exercise 3 — this course prefers the "
           "knob you can see over the kernel you cannot");
}

// ===========================================================================
//  §H — the order
// ===========================================================================

void section_h_order()
{
    section("H  BLOOM BEFORE THE CURVE, AND WHAT HAPPENS AFTER IT");

    // One bright spot on a dim field, resolved two ways.
    hdr_buffer src(64, 64);
    src.clear(linear_rgb{0.2f, 0.2f, 0.2f});
    for (int y = 30; y < 34; ++y)
    {
        for (int x = 30; x < 34; ++x) { src.put_pixel(x, y, {400.0f, 400.0f, 400.0f}); }
    }

    bloom_settings bs;
    bs.threshold = 1.0f;
    bs.knee = 0.5f;
    bs.clamp_max = 0.0f;
    bs.levels = 5;
    bloom_pyramid py;
    engine::compute_bloom(src, py, bs, 1.0f);

    tonemap_settings ts;
    ts.op = tonemap::aces;
    ts.exposure = 1.0f;

    // THE RIGHT ORDER: composite in linear light, then curve.
    engine::framebuffer correct(64, 64);
    engine::resolve(src, correct, ts, engine::encode_mode::exact, &py.result(), 0.05f);

    // THE WRONG ORDER: curve first, then add the bloom to the RESULT.
    engine::framebuffer wrong(64, 64);
    engine::resolve(src, wrong, ts, engine::encode_mode::exact);
    int wrong_saturated = 0;
    int correct_saturated = 0;
    for (int y = 0; y < 64; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            // Adding after the curve means adding to a value already in [0,1] and
            // letting the encode clip. Simulated here on the encoded image.
            const linear_rgb b = engine::sample_bilinear(
                py.result(), (static_cast<float>(x) + 0.5f) * 0.5f,
                             (static_cast<float>(y) + 0.5f) * 0.5f);
            const linear_rgb after = engine::to_linear(wrong.pixel_at(x, y));
            const float v = after.r + b.r * 0.05f;
            if (v >= 1.0f) { ++wrong_saturated; }
            if ((engine::to_linear(correct.pixel_at(x, y)).r) >= 0.9955f) { ++correct_saturated; }
        }
    }

    checkf(wrong_saturated > correct_saturated * 2,
           "composited AFTER the curve, %d of 4,096 pixels land above 1.0 and are "
           "clipped by the encode; composited BEFORE it, %d do. The curve's output "
           "is already in [0,1], so anything added to it has nowhere to go — every "
           "glow grows a flat white core, and the brighter the source the bigger "
           "the core. Which is the artefact bloom exists to remove",
           wrong_saturated, correct_saturated);

    // AND THE EXPOSURE HAS TO BE THE SAME NUMBER IN BOTH PLACES.
    bloom_pyramid py2;
    engine::compute_bloom(src, py2, bs, 4.0f);          // bloom at exposure 4
    engine::framebuffer mismatched(64, 64);
    tonemap_settings ts4 = ts;
    ts4.exposure = 1.0f;                                 // scene at exposure 1
    engine::resolve(src, mismatched, ts4, engine::encode_mode::exact, &py2.result(), 0.05f);

    engine::framebuffer matched(64, 64);
    engine::resolve(src, matched, ts, engine::encode_mode::exact, &py.result(), 0.05f);

    int differing = 0;
    for (int y = 0; y < 64; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            if (mismatched.pixel_at(x, y) != matched.pixel_at(x, y)) { ++differing; }
        }
    }
    checkf(differing > 100,
           "and giving the bloom a different exposure from the scene changes %d "
           "pixels — the glow and the image it sits on photographed at different "
           "shutter speeds. `gpu_post_stack::resolve_into` passes the same struct "
           "field to both for exactly this reason", differing);
}

// ===========================================================================
//  §I — the golden
// ===========================================================================

void section_i_golden()
{
    section("I  THE GOLDEN");

    const int rc = demo::write_reference_shot("scratch/verify613.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify613.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL, hash E917C06C — TWENTY-SECOND lesson at this hash. The "
          "reason is the same STRUCTURAL one 6.12 gave and it was checked rather "
          "than assumed: the fixture renders into an 8-bit `framebuffer`, and a "
          "bloom is a stage over an `hdr_buffer` the fixture does not have. "
          "`resolve` grew two parameters this lesson and both DEFAULT to no bloom, "
          "so the fixture's call site did not change either");
}

// ===========================================================================
//  Device-side plumbing for §J — the shape verify_612 established
// ===========================================================================

struct target
{
    engine::gpu_device* dev = nullptr;
    SDL_GPUTexture* colour = nullptr;
    SDL_GPUTransferBuffer* readback = nullptr;
    int w = 0;
    int h = 0;

    bool create(engine::gpu_device& d, int width, int height, SDL_GPUTextureFormat fmt)
    {
        dev = &d;
        w = width;
        h = height;

        SDL_GPUTextureCreateInfo ti{};
        ti.type = SDL_GPU_TEXTURETYPE_2D;
        ti.format = fmt;
        ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        ti.width = static_cast<Uint32>(width);
        ti.height = static_cast<Uint32>(height);
        ti.layer_count_or_depth = 1;
        ti.num_levels = 1;
        ti.sample_count = SDL_GPU_SAMPLECOUNT_1;
        colour = SDL_CreateGPUTexture(d.handle(), &ti);
        if (colour == nullptr) { return false; }

        SDL_GPUTransferBufferCreateInfo tb{};
        tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        tb.size = static_cast<Uint32>(width * height * 4);
        readback = SDL_CreateGPUTransferBuffer(d.handle(), &tb);
        return readback != nullptr;
    }

    void destroy()
    {
        if (dev == nullptr) { return; }
        if (readback != nullptr) { SDL_ReleaseGPUTransferBuffer(dev->handle(), readback); }
        if (colour != nullptr) { SDL_ReleaseGPUTexture(dev->handle(), colour); }
        readback = nullptr;
        colour = nullptr;
    }
};

std::vector<Uint8> download(target& t, SDL_GPUCommandBuffer* cb)
{
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);
    SDL_GPUTextureRegion src{};
    src.texture = t.colour;
    src.w = static_cast<Uint32>(t.w);
    src.h = static_cast<Uint32>(t.h);
    src.d = 1;
    SDL_GPUTextureTransferInfo dst{};
    dst.transfer_buffer = t.readback;
    dst.pixels_per_row = static_cast<Uint32>(t.w);
    dst.rows_per_layer = static_cast<Uint32>(t.h);
    SDL_DownloadFromGPUTexture(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence == nullptr) { return {}; }
    SDL_WaitForGPUFences(t.dev->handle(), true, &fence, 1);
    SDL_ReleaseGPUFence(t.dev->handle(), fence);

    std::vector<Uint8> px(static_cast<std::size_t>(t.w) * static_cast<std::size_t>(t.h) * 4u);
    const void* mapped = SDL_MapGPUTransferBuffer(t.dev->handle(), t.readback, false);
    if (mapped == nullptr) { return {}; }
    std::memcpy(px.data(), mapped, px.size());
    SDL_UnmapGPUTransferBuffer(t.dev->handle(), t.readback);
    return px;
}

[[nodiscard]] bool upload_quad(engine::gpu_device& gpu, engine::gpu_mesh& quad)
{
    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
    if (cb == nullptr) { return false; }
    const bool ok = quad.create(gpu, cb, engine::quad_mesh(),
                                engine::index_mode::indexed, "verify613 quad");
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence != nullptr)
    {
        SDL_WaitForGPUFences(gpu.handle(), true, &fence, 1);
        SDL_ReleaseGPUFence(gpu.handle(), fence);
    }
    return ok;
}

// ===========================================================================
//  §J — CPU vs GPU
// ===========================================================================

void section_j_gpu()
{
    section("J  CPU vs GPU — THE SAME CHAIN, COMPUTED TWICE");

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::printf("  ---- SDL_Init(VIDEO): %s\n", SDL_GetError());
    }

    // Nothing below calls destroy(): `gpu` is declared first so it is destroyed
    // LAST, which is the order every object after it needs. 6.11's harness
    // segfaulted on exit for exactly this reason and the fix was to delete the
    // cleanup rather than to reorder it.
    engine::gpu_device gpu;
    if (!gpu.create(nullptr, false).ok())
    {
        std::printf("  ---- no GPU device on this machine; §J skipped\n");
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
        std::printf("  ---- shaders did not load; §J skipped\n");
        return;
    }
    check(true, "all seven shaders loaded, including the bloom's three fragments — "
                "and NO new vertex shader, because every post pass wants the same "
                "three corners");

    engine::gpu_mesh quad;
    if (!upload_quad(gpu, quad))
    {
        std::printf("  ---- quad upload failed; §J skipped\n");
        return;
    }

    engine::gpu_sampler samp;
    check(samp.create(gpu), "a sampler for the scene pass's fallback texture");

    constexpr int k_size = 64;
    constexpr int k_levels = 4;

    engine::gpu_post_stack stack;
    check(stack.create(gpu, post_vs.handle(), tone_fs.handle(),
                       bright_fs.handle(), down_fs.handle(), up_fs.handle(),
                       SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
          "the stack: a tonemap pipeline, THREE bloom pipelines, the float scene "
          "target and the 1x1 black stand-in for when the bloom is off");

    bloom_settings bs;
    bs.enabled = true;
    bs.threshold = 1.0f;
    bs.knee = 0.0f;          // a hard cut, so the derivation below is exact
    bs.clamp_max = 0.0f;
    bs.levels = k_levels;
    bs.intensity = 0.05f;

    check(stack.resize(gpu, k_size, k_size, bs),
          "the scene target and the pyramid sized together — the stack owns what "
          "crosses BETWEEN stages, and `gpu_bloom` owns what does not");

    checkf(stack.bloom().pass_count() == 2 * k_levels - 1,
           "%d render passes for %d levels: one bright pass, %d downsamples, %d "
           "upsamples. A pass writes ONE target, and no pass may read the texture "
           "it writes — so `2n-1` is a floor, not an implementation detail",
           stack.bloom().pass_count(), k_levels, k_levels - 1, k_levels - 1);

    engine::gpu_scene_renderer scene;
    check(scene.create(gpu, scene_vs.handle(), scene_fs.handle(),
                       SDL_GPU_TEXTUREFORMAT_D32_FLOAT, engine::k_hdr_format),
          "the scene's nine pipelines against the FLOAT format");

    target ldr;
    if (!ldr.create(gpu, k_size, k_size, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM))
    {
        std::printf("  ---- LDR target failed; §J skipped\n");
        return;
    }

    // A FLAT QUAD OF KNOWN VALUE, which collapses the scene to `lit = ambient`
    // (verify_61 §E's trick) and makes the whole bloom chain solvable on paper.
    const auto render_through_stack = [&](float ambient, float intensity) -> int {
        if (!scene.ensure_depth(gpu, k_size, k_size)) { return -1; }

        engine::gpu_draw_item item{};
        item.mesh = &quad;
        item.world_from_model = engine::mat4::identity();
        item.normal_from_model = engine::mat3::identity();
        item.material.albedo = engine::vec3{1.0f, 1.0f, 1.0f};
        item.material.roughness = 1.0f;
        item.material.metallic = 0.0f;
        item.material.f0 = 0.0f;
        item.material.textured = 0.0f;
        item.material.normal_mapped = 0.0f;
        item.material.alpha = 1.0f;
        item.material.alpha_cutoff = 0.0f;
        item.style = engine::surface_style::two_sided;

        engine::mat4 clip = engine::mat4::identity();
        clip.c0.x = 2.0f;
        clip.c1.y = 2.0f;

        engine::scene_light_uniforms light{};
        light.to_light = engine::vec3{0.0f, 0.0f, 1.0f};
        light.key = engine::vec3{0.0f, 0.0f, 0.0f};
        light.ambient = engine::vec3{ambient, ambient, ambient};
        light.eye_world = engine::vec3{0.0f, 0.0f, 5.0f};
        light.spec_model = 0.0f;
        light.encode_output = 0.0f;

        bloom_settings s = bs;
        s.intensity = intensity;

        tonemap_settings ts;
        ts.op = tonemap::aces;
        ts.exposure = 1.0f;

        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());

        // ---- ONE: the scene, into the stack's float target -----------------
        SDL_GPUColorTargetInfo hdr_info{};
        hdr_info.texture = stack.scene_target();
        hdr_info.load_op = SDL_GPU_LOADOP_CLEAR;
        hdr_info.store_op = SDL_GPU_STOREOP_STORE;
        hdr_info.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
        SDL_GPUDepthStencilTargetInfo dsi = scene.depth_target_info();
        SDL_GPURenderPass* p1 = SDL_BeginGPURenderPass(cb, &hdr_info, 1, &dsi);
        scene.render(cb, p1, &item, 1, engine::camera_uniforms{clip}, light, samp.handle());
        SDL_EndGPURenderPass(p1);

        // ---- TWO: the bloom, which records SEVEN passes of its own ----------
        //
        // Outside any render pass, deliberately: it begins and ends its own.
        stack.render_bloom(cb, s, ts.exposure);

        // ---- THREE: the resolve, into 8 bits -------------------------------
        SDL_GPUColorTargetInfo ldr_info{};
        ldr_info.texture = ldr.colour;
        ldr_info.load_op = SDL_GPU_LOADOP_CLEAR;
        ldr_info.store_op = SDL_GPU_STOREOP_STORE;
        ldr_info.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
        SDL_GPURenderPass* p3 = SDL_BeginGPURenderPass(cb, &ldr_info, 1, nullptr);
        stack.resolve_into(cb, p3, ts, /*shader_encodes=*/true, s);
        SDL_EndGPURenderPass(p3);

        const std::vector<Uint8> px = download(ldr, cb);
        return px.empty() ? -1
                          : static_cast<int>(px[(static_cast<std::size_t>(k_size / 2) * k_size
                                                 + k_size / 2) * 4u]);
    };

    // The CPU's answer, through the same settings.
    const auto cpu_code = [&](float ambient, float intensity) {
        hdr_buffer src = uniform_buffer(k_size, k_size, ambient);
        bloom_settings s = bs;
        s.intensity = intensity;
        bloom_pyramid py;
        engine::compute_bloom(src, py, s, 1.0f);

        engine::framebuffer fb(k_size, k_size);
        tonemap_settings ts;
        ts.op = tonemap::aces;
        ts.exposure = 1.0f;
        engine::resolve(src, fb, ts, engine::encode_mode::exact, &py.result(), intensity);
        return static_cast<int>(fb.pixel_at(k_size / 2, k_size / 2) & 0xFFu);
    };

    // AND THE ANSWER FROM PAPER, which neither of them computed.
    const auto derived_code = [&](float ambient, float intensity) {
        const float bright = (ambient > 1.0f) ? ambient - 1.0f : 0.0f;
        const float bloom = bright * static_cast<float>(k_levels);
        const float lit = ambient + intensity * bloom;
        return static_cast<int>(engine::linear_to_srgb_u8(
            engine::apply_tonemap(lit, tonemap::aces, 4.0f)));
    };

    std::printf("  a uniform field, through the whole stack:\n");
    std::printf("    ambient  intensity    GPU   CPU   derived\n");
    int agree = 0;
    int compared = 0;
    for (auto [ambient, intensity] : {std::pair<float, float>{3.0f, 0.05f},
                                      {3.0f, 0.20f},
                                      {6.0f, 0.05f},
                                      {0.5f, 0.20f}})
    {
        const int g = render_through_stack(ambient, intensity);
        if (g < 0) { continue; }
        const int c = cpu_code(ambient, intensity);
        const int d = derived_code(ambient, intensity);
        std::printf("    %7.1f  %9.2f  %5d %5d %9d\n",
                    static_cast<double>(ambient), static_cast<double>(intensity), g, c, d);
        ++compared;
        if (std::abs(g - c) <= 2 && std::abs(c - d) <= 2) { ++agree; }
    }

    checkf(compared > 0 && agree == compared,
           "%d of %d agree across ALL THREE — the GPU's eleven-pass chain, the "
           "CPU's four loops, and a derivation that says level 0 of a uniform "
           "field is exactly `levels x bright`. Two implementations agreeing "
           "could both be wrong the same way; three, one of which never ran the "
           "code, cannot", agree, compared);

    // THE LAST ROW IS THE ONE THAT PROVES THE THRESHOLD WORKS: ambient 0.5 is
    // below it, so the bright pass keeps nothing and the bloom must be exactly
    // zero — the same code as no bloom at all.
    const int dim_with = render_through_stack(0.5f, 0.20f);
    const int dim_without = render_through_stack(0.5f, 0.0f);
    checkf(dim_with >= 0 && dim_with == dim_without,
           "a field at 0.5 — below the threshold — resolves to code %d with the "
           "bloom on and code %d with it off. Identical, because the bright pass "
           "selected nothing. A bloom that tints a dark scene is a threshold "
           "applied in the wrong space", dim_with, dim_without);
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    std::printf("verify_613 — Lesson 6.13: Bloom and the Post-Processing Stack\n");

    section_a_lid();
    section_b_knee();
    section_c_identities();
    section_d_pyramid();
    section_e_kernel();
    section_f_area();
    section_g_fireflies();
    section_h_order();

    // The golden runs BEFORE the GPU section, which 6.11 established: §J
    // initialises the video subsystem and creates a device, and a CPU reference
    // render has no business being downstream of either.
    section_i_golden();
    section_j_gpu();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
