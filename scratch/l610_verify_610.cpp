// scratch/verify_610.cpp — Lesson 6.10's harness: the chain, the level, and the
// bug that makes a texture darker as it recedes.
//
//   §A  THE CHAIN — sizes, count, and the 33% everyone quotes
//   §B  LINEAR LIGHT — what averaging sRGB bytes actually costs
//   §C  THE LEVEL — log2 of a footprint, against hand arithmetic
//   §D  THE ANALYTIC GRADIENT — exact, checked against a finite difference
//   §E  TRILINEAR — the level seam, and what blending costs
//   §F  ANISOTROPY — a grazing footprint, and what isotropic throws away
//   §G  WHAT IT COSTS — blurring a surface that was never undersampled
//   §H  THE GOLDEN — a new capability must not move the old picture
//
// §B AND §D ARE THE TWO THAT MATTER. §B measures the famous bug: a mip chain
// built by averaging sRGB bytes gets darker at every level, and the symptom is a
// surface that dims as it recedes — usually diagnosed as "the lighting falls off
// too fast". §D is the CPU's replacement for `ddx`/`ddy`: a scanline rasterizer
// has no neighbouring fragment to subtract, so the derivative has to come from
// the triangle analytically, and the test is whether the closed form agrees with
// a numerical difference of the real interpolation.
//
// Build and run:  sh scratch/build_verify_610.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/mipmap.hpp>
#include <engine/gfx/texture.hpp>
#include <engine/math/vec2.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
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

using engine::filter;
using engine::linear_rgb;
using engine::mip_chain;
using engine::sampler;
using engine::texel_space;
using engine::texture;
using engine::uv_footprint;
using engine::vec2;

/// A black-and-white checker: the harshest case for an averaging bug, because
/// every 2x2 block averages two extremes.
texture checker(int size, int cell, texel_space space = texel_space::srgb)
{
    texture t(size, size, 0xFF000000u, space);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const bool on = ((x / cell) + (y / cell)) % 2 == 0;
            t.set_texel(x, y, on ? 0xFFFFFFFFu : 0xFF000000u);
        }
    }
    return t;
}

// ---------------------------------------------------------------------------
void section_a_chain()
{
    section("A  THE CHAIN");

    const texture base = checker(256, 8);
    const mip_chain chain = engine::build_mips(base);

    std::printf("  level  size        texels\n");
    for (int i = 0; i < chain.levels(); ++i)
    {
        std::printf("  %5d  %4dx%-4d  %8d\n", i, chain.level(i).width(),
                    chain.level(i).height(),
                    chain.level(i).width() * chain.level(i).height());
    }

    checkf(chain.levels() == 9, "256x256 gives %d levels (1 + log2(256))", chain.levels());
    checkf(chain.level(8).width() == 1 && chain.level(8).height() == 1,
           "the last level is 1x1");

    // THE 33%, derived: 1/4 + 1/16 + 1/64 + ... = 1/3.
    const std::size_t base_texels = 256u * 256u;
    const double extra = static_cast<double>(chain.texels() - base_texels)
                       / static_cast<double>(base_texels);
    checkf(std::fabs(extra - 1.0 / 3.0) < 0.01,
           "the chain costs %.2f%% more than the image, against the "
           "1/4+1/16+1/64+... = 33.33%% the series predicts", extra * 100.0);

    // A non-square texture must not stop when ONE axis hits 1.
    const texture wide = checker(64, 4);
    texture strip(64, 4, 0xFF808080u);
    const mip_chain schain = engine::build_mips(strip);
    checkf(schain.level(schain.levels() - 1).width() == 1
           && schain.level(schain.levels() - 1).height() == 1,
           "a 64x4 strip reaches 1x1 in %d levels rather than stopping at 16x1",
           schain.levels());
    (void)wide;
}

// ---------------------------------------------------------------------------
// The famous bug, measured.
void section_b_linear()
{
    section("B  LINEAR LIGHT");

    // A 2x2 checker: every block averages one black and one white texel, so the
    // right answer is exactly half the LIGHT, which is sRGB code 188 (Lesson
    // 6.1's second integer).
    texture t(2, 2, 0xFF000000u, texel_space::srgb);
    t.set_texel(0, 0, 0xFFFFFFFFu);
    t.set_texel(1, 1, 0xFFFFFFFFu);
    t.set_texel(1, 0, 0xFF000000u);
    t.set_texel(0, 1, 0xFF000000u);

    const mip_chain chain = engine::build_mips(t);
    const Uint32 got = chain.level(1).texel(0, 0);
    const int code = static_cast<int>(got & 0xFFu);

    // What the naive version would have produced: (255 + 0 + 0 + 255) / 4 = 127.
    const int naive = 127;

    const float lit_correct = engine::srgb_to_linear_u8(static_cast<Uint8>(code));
    const float lit_naive = engine::srgb_to_linear_u8(static_cast<Uint8>(naive));

    std::printf("  half the LIGHT of black and white:\n");
    std::printf("    averaged in linear light : code %3d  -> %.4f of white\n",
                code, static_cast<double>(lit_correct));
    std::printf("    averaged as sRGB bytes   : code %3d  -> %.4f of white\n",
                naive, static_cast<double>(lit_naive));

    checkf(code >= 187 && code <= 189,
           "linear-light averaging gives code %d, and 6.1's second integer says "
           "half the light is code 188", code);

    const float darker = 1.0f - lit_naive / lit_correct;
    checkf(darker > 0.55f && darker < 0.60f,
           "the naive chain delivers %.1f%% of the light it should — %.1f%% TOO "
           "DARK at level 1 alone, on the harshest case. It compounds down the "
           "chain, so a surface dims as it recedes and the usual diagnosis is "
           "\"the lighting falls off too fast\"",
           static_cast<double>(lit_naive / lit_correct * 100.0f),
           static_cast<double>(darker * 100.0f));

    // DATA IS NOT COLOUR. A linear-space texture must be averaged as bytes, or
    // 6.7's normal maps would be decoded as though they were pictures.
    texture d(2, 2, 0xFF000000u, texel_space::linear);
    d.set_texel(0, 0, 0xFFFFFFFFu);
    d.set_texel(1, 1, 0xFFFFFFFFu);
    d.set_texel(1, 0, 0xFF000000u);
    d.set_texel(0, 1, 0xFF000000u);
    const mip_chain dchain = engine::build_mips(d);
    const int dcode = static_cast<int>(dchain.level(1).texel(0, 0) & 0xFFu);
    checkf(dcode == 127 || dcode == 128,
           "a texel_space::linear texture averages to %d — the BYTES, correctly, "
           "because a normal map was never sRGB-encoded", dcode);
}

// ---------------------------------------------------------------------------
void section_c_level()
{
    section("C  THE LEVEL");

    // A footprint of exactly 1 texel is level 0: the texture is already the
    // right size and no averaging is wanted.
    uv_footprint one{};
    one.d_dx = vec2{1.0f / 256.0f, 0.0f};
    one.d_dy = vec2{0.0f, 1.0f / 256.0f};
    checkf(engine::mip_level_for(one, 256, 256) == 0.0f,
           "a one-texel footprint selects level 0 exactly");

    // Powers of two land on whole levels, which is the whole point of log2.
    struct { float texels; float level; } cases[] = {
        {2.0f, 1.0f}, {4.0f, 2.0f}, {8.0f, 3.0f}, {64.0f, 6.0f},
    };
    for (const auto& c : cases)
    {
        uv_footprint fp{};
        fp.d_dx = vec2{c.texels / 256.0f, 0.0f};
        fp.d_dy = vec2{0.0f, 1.0f / 256.0f};
        const float got = engine::mip_level_for(fp, 256, 256);
        checkf(std::fabs(got - c.level) < 1e-5f,
               "a %.0f-texel footprint selects level %.4f, predicted %.1f",
               static_cast<double>(c.texels), static_cast<double>(got),
               static_cast<double>(c.level));
    }

    // 3.9's OWN MEASUREMENT, run through the new formula. That lesson reported
    // 62.46 texels per pixel two rows below the horizon.
    uv_footprint horizon{};
    horizon.d_dx = vec2{62.46f / 256.0f, 0.0f};
    horizon.d_dy = vec2{0.0f, 1.0f / 256.0f};
    const float lvl = engine::mip_level_for(horizon, 256, 256);
    checkf(std::fabs(lvl - std::log2(62.46f)) < 1e-4f,
           "Lesson 3.9's measured 62.46 texels/pixel at the horizon selects "
           "level %.3f — which is why four bilinear taps could only ever see "
           "6.4%% of the truth there", static_cast<double>(lvl));

    // Magnification asks for level 0 and must not go negative: there is no
    // level -1, and a negative index is a crash rather than a blur.
    uv_footprint mag{};
    mag.d_dx = vec2{0.25f / 256.0f, 0.0f};
    mag.d_dy = vec2{0.0f, 0.25f / 256.0f};
    checkf(engine::mip_level_for(mag, 256, 256) == 0.0f,
           "a quarter-texel footprint (magnification) clamps to level 0");
}

// ---------------------------------------------------------------------------
// The CPU's replacement for ddx/ddy, checked against the real interpolation.
void section_d_gradient()
{
    section("D  THE ANALYTIC GRADIENT");

    // A triangle in screen space with genuinely different w per vertex, so the
    // perspective divide actually bends the interpolation. Anything with equal
    // w would let an affine (wrong) derivative pass.
    const float x0 = 40.0f, y0 = 30.0f, iw0 = 1.0f / 2.0f;
    const float x1 = 300.0f, y1 = 60.0f, iw1 = 1.0f / 9.0f;
    const float x2 = 90.0f, y2 = 220.0f, iw2 = 1.0f / 5.0f;

    const float u0 = 0.0f, v0 = 0.0f;
    const float u1 = 1.0f, v1 = 0.2f;
    const float u2 = 0.1f, v2 = 1.0f;

    // Premultiplied attributes, exactly as raster.cpp stores them.
    const float pu0 = u0 * iw0, pu1 = u1 * iw1, pu2 = u2 * iw2;
    const float pv0 = v0 * iw0, pv1 = v1 * iw1, pv2 = v2 * iw2;

    const float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    const float inv_area = 1.0f / area;

    // df_i/dx and df_i/dy — the edge steps over the area, which is what
    // raster.cpp's step_x0/step_y0 already are.
    const float dfx[3] = {(y1 - y2) * inv_area, (y2 - y0) * inv_area, (y0 - y1) * inv_area};
    const float dfy[3] = {(x2 - x1) * inv_area, (x0 - x2) * inv_area, (x1 - x0) * inv_area};

    // The affine numerator gradients, constant over the triangle.
    const vec2 dnum_dx{dfx[0] * pu0 + dfx[1] * pu1 + dfx[2] * pu2,
                       dfx[0] * pv0 + dfx[1] * pv1 + dfx[2] * pv2};
    const vec2 dnum_dy{dfy[0] * pu0 + dfy[1] * pu1 + dfy[2] * pu2,
                       dfy[0] * pv0 + dfy[1] * pv1 + dfy[2] * pv2};
    const float dw_dx = dfx[0] * iw0 + dfx[1] * iw1 + dfx[2] * iw2;
    const float dw_dy = dfy[0] * iw0 + dfy[1] * iw1 + dfy[2] * iw2;

    // The real interpolation, so a finite difference has something to difference.
    auto uv_at = [&](float x, float y) -> vec2 {
        const float f0 = ((x1 - x) * (y2 - y) - (x2 - x) * (y1 - y)) * inv_area;
        const float f1 = ((x2 - x) * (y0 - y) - (x0 - x) * (y2 - y)) * inv_area;
        const float f2 = ((x0 - x) * (y1 - y) - (x1 - x) * (y0 - y)) * inv_area;
        const float wr = 1.0f / (f0 * iw0 + f1 * iw1 + f2 * iw2);
        return vec2{(f0 * pu0 + f1 * pu1 + f2 * pu2) * wr,
                    (f0 * pv0 + f1 * pv1 + f2 * pv2) * wr};
    };

    double worst = 0.0;
    for (const auto& pt : {vec2{110.0f, 80.0f}, vec2{150.0f, 70.0f}, vec2{80.0f, 150.0f}})
    {
        const float f0 = ((x1 - pt.x) * (y2 - pt.y) - (x2 - pt.x) * (y1 - pt.y)) * inv_area;
        const float f1 = ((x2 - pt.x) * (y0 - pt.y) - (x0 - pt.x) * (y2 - pt.y)) * inv_area;
        const float f2 = ((x0 - pt.x) * (y1 - pt.y) - (x1 - pt.x) * (y0 - pt.y)) * inv_area;
        const float wr = 1.0f / (f0 * iw0 + f1 * iw1 + f2 * iw2);
        const vec2 uv = uv_at(pt.x, pt.y);

        const uv_footprint got = engine::uv_gradients(dnum_dx, dnum_dy, dw_dx, dw_dy, uv, wr);

        // Central difference of the actual interpolation.
        const float e = 0.01f;
        const vec2 px = uv_at(pt.x + e, pt.y), mx = uv_at(pt.x - e, pt.y);
        const vec2 py = uv_at(pt.x, pt.y + e), my = uv_at(pt.x, pt.y - e);
        const vec2 num_dx{(px.x - mx.x) / (2 * e), (px.y - mx.y) / (2 * e)};
        const vec2 num_dy{(py.x - my.x) / (2 * e), (py.y - my.y) / (2 * e)};

        const double err = std::max(
            std::max(std::fabs(got.d_dx.x - num_dx.x), std::fabs(got.d_dx.y - num_dx.y)),
            std::max(std::fabs(got.d_dy.x - num_dy.x), std::fabs(got.d_dy.y - num_dy.y)));
        worst = std::max(worst, err);

        std::printf("  at (%5.1f,%5.1f)  du/dx analytic %+.6f  numerical %+.6f\n",
                    static_cast<double>(pt.x), static_cast<double>(pt.y),
                    static_cast<double>(got.d_dx.x), static_cast<double>(num_dx.x));
    }

    checkf(worst < 2e-5,
           "the closed form agrees with a central difference of the real "
           "perspective-correct interpolation to %.2e — it is EXACT, not an "
           "approximation, because U and W are both affine in screen space",
           worst);
}

// ---------------------------------------------------------------------------
void section_e_trilinear()
{
    section("E  TRILINEAR");

    const texture base = checker(256, 8);
    const mip_chain chain = engine::build_mips(base);

    sampler nearest_mip;
    nearest_mip.mip_filter = filter::nearest;
    sampler linear_mip;
    linear_mip.mip_filter = filter::linear;

    // Sweep the footprint across a level boundary and watch for a STEP. Nearest
    // must jump; trilinear must not.
    float worst_nearest = 0.0f;
    float worst_linear = 0.0f;
    linear_rgb prev_n{}, prev_l{};
    bool first = true;

    for (int i = 0; i <= 40; ++i)
    {
        // Level 4.0 -> 8.0 texels, level 3.0 -> 8, so sweep 4..12 texels:
        // that spans levels 2.0 to 3.58 and CROSSES the nearest-rounding
        // boundary at level 2.5 (5.657 texels) and 3.5 (11.3 texels).
        const float texels = 4.0f + 8.0f * static_cast<float>(i) / 40.0f;
        uv_footprint fp{};
        fp.d_dx = vec2{texels / 256.0f, 0.0f};
        fp.d_dy = vec2{0.0f, texels / 256.0f};

        const linear_rgb n = engine::sample_mipped(chain, nearest_mip, vec2{0.137f, 0.611f}, fp);
        const linear_rgb l = engine::sample_mipped(chain, linear_mip, vec2{0.137f, 0.611f}, fp);
        if (!first)
        {
            worst_nearest = std::max(worst_nearest, std::fabs(n.r - prev_n.r));
            worst_linear = std::max(worst_linear, std::fabs(l.r - prev_l.r));
        }
        prev_n = n;
        prev_l = l;
        first = false;
    }

    checkf(worst_nearest > worst_linear * 2.0f,
           "sweeping across a level boundary, NEAREST jumps by %.4f in one step "
           "and TRILINEAR by %.4f — the jump is the visible line across a floor, "
           "and it is the same artefact 6.9 met as the cascade seam",
           static_cast<double>(worst_nearest), static_cast<double>(worst_linear));
}

// ---------------------------------------------------------------------------
void section_f_aniso()
{
    section("F  ANISOTROPY");

    const texture base = checker(256, 8);
    const mip_chain chain = engine::build_mips(base);

    // A grazing floor: 32 texels along one screen axis, 2 along the other. That
    // is a 16:1 footprint, which is exactly the case a square average cannot
    // represent.
    uv_footprint graze{};
    graze.d_dx = vec2{32.0f / 256.0f, 0.0f};
    graze.d_dy = vec2{0.0f, 2.0f / 256.0f};

    const float iso_level = engine::mip_level_for(graze, 256, 256);
    checkf(std::fabs(iso_level - 5.0f) < 1e-4f,
           "isotropic picks level %.2f from the LONG axis (32 texels) — "
           "so the short axis, which only needed level 1, is blurred by a "
           "factor of %.0f", static_cast<double>(iso_level),
           std::pow(2.0, static_cast<double>(iso_level) - 1.0));

    sampler iso;
    iso.max_anisotropy = 1;
    sampler aniso;
    aniso.max_anisotropy = 16;

    // Contrast on a texture with real detail: the anisotropic sample keeps more
    // of it, so it should differ from the heavily blurred isotropic one.
    const linear_rgb a = engine::sample_mipped(chain, iso, vec2{0.31f, 0.52f}, graze);
    const linear_rgb b = engine::sample_mipped(chain, aniso, vec2{0.31f, 0.52f}, graze);

    std::printf("  isotropic  (level %.2f, 1 tap ) -> %.4f\n",
                static_cast<double>(iso_level), static_cast<double>(a.r));
    std::printf("  anisotropic(level %.2f, 16 taps) -> %.4f\n",
                std::log2(2.0), static_cast<double>(b.r));

    checkf(std::fabs(a.r - b.r) > 1e-4f,
           "the two disagree by %.4f, which is the detail an isotropic average "
           "threw away", static_cast<double>(std::fabs(a.r - b.r)));

    // A SQUARE footprint must not trigger anisotropy: 1:1 has no long axis, and
    // taking 16 taps of the same place would be pure cost.
    uv_footprint square{};
    square.d_dx = vec2{8.0f / 256.0f, 0.0f};
    square.d_dy = vec2{0.0f, 8.0f / 256.0f};
    const linear_rgb s1 = engine::sample_mipped(chain, iso, vec2{0.31f, 0.52f}, square);
    const linear_rgb s16 = engine::sample_mipped(chain, aniso, vec2{0.31f, 0.52f}, square);
    checkf(std::fabs(s1.r - s16.r) < 1e-6f,
           "on a SQUARE footprint anisotropy changes nothing (%.6f vs %.6f) — "
           "it costs only where it buys", static_cast<double>(s1.r),
           static_cast<double>(s16.r));
}

// ---------------------------------------------------------------------------
// 6.9's habit: measure the case where the feature loses.
void section_g_cost()
{
    section("G  WHAT IT COSTS");

    const texture base = checker(256, 8);
    const mip_chain chain = engine::build_mips(base);

    // A surface that was never undersampled — one texel per pixel — must come
    // back IDENTICAL to an unmipped fetch, or mipmapping is blurring things it
    // was never asked to touch.
    uv_footprint fine{};
    fine.d_dx = vec2{1.0f / 256.0f, 0.0f};
    fine.d_dy = vec2{0.0f, 1.0f / 256.0f};

    sampler s;
    const linear_rgb mipped = engine::sample_mipped(chain, s, vec2{0.3123f, 0.7071f}, fine);
    const linear_rgb plain = engine::sample(base, s, 0.3123f, 0.7071f);

    checkf(std::fabs(mipped.r - plain.r) < 1e-6f
           && std::fabs(mipped.g - plain.g) < 1e-6f,
           "at one texel per pixel the mipped fetch is IDENTICAL to the plain "
           "one (%.6f vs %.6f) — level 0 is the original image, so a surface "
           "that was never undersampled pays nothing but the memory",
           static_cast<double>(mipped.r), static_cast<double>(plain.r));

    // And the memory, stated plainly.
    const std::size_t base_texels = 256u * 256u;
    std::printf("  memory: %zu texels -> %zu with the chain (+%.1f%%)\n",
                base_texels, chain.texels(),
                100.0 * (static_cast<double>(chain.texels()) / static_cast<double>(base_texels) - 1.0));

    // THE HONEST COST. Force a level with mip_bias and measure the blur: this is
    // what a too-eager LOD does to a surface that did not need it.
    sampler blurred;
    blurred.mip_bias = 2.0f;
    const linear_rgb over = engine::sample_mipped(chain, blurred, vec2{0.3123f, 0.7071f}, fine);
    checkf(std::fabs(over.r - plain.r) > 1e-3f,
           "a +2 level bias moves the same fetch from %.4f to %.4f — which is "
           "what over-eager level selection costs, and why mip_bias is exposed "
           "rather than hidden", static_cast<double>(plain.r),
           static_cast<double>(over.r));
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

void section_h_golden()
{
    section("H  THE GOLDEN");

    const int rc = demo::write_reference_shot("scratch/verify610.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify610.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL, hash E917C06C — NINETEENTH lesson at this hash, and "
          "this one was NOT automatic: the reference scene samples textures, so "
          "a chain built by default would have moved it. THE CHAIN IS OPT-IN — "
          "`texture` is untouched, `mip_chain` is a separate type, and a call "
          "site written before 6.10 samples exactly what it did in 3.9. A NEW "
          "CAPABILITY IS A NEW PATH");
}

} // namespace

int main()
{
    std::printf("verify_610 — Lesson 6.10: Mipmaps, LOD, and Anisotropic Filtering\n");

    section_a_chain();
    section_b_linear();
    section_c_level();
    section_d_gradient();
    section_e_trilinear();
    section_f_aniso();
    section_g_cost();
    section_h_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
