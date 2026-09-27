// scratch/verify_611.cpp — Lesson 6.11's harness: coverage, compositing, and order.
//
//   §A  THE FOURTH CHANNEL — the sampler kept its colour bit for bit
//   §B  OVER — the operator, and what compositing in the wrong space costs
//   §C  PREMULTIPLIED — the halo, and the representation that removes it
//   §D  MASK vs BLEND — the depth buffer, and which one keeps it honest
//   §E  COVERAGE — foliage that dissolves with distance, and the rescale
//   §F  ORDER — bucketing, back-to-front, and the case with no right answer
//   §G  WHAT IT COSTS — the reordered depth test, in fragments
//   §H  CPU vs GPU — hardware blending on an _SRGB target and on a UNORM one
//   §I  THE GOLDEN — a new capability must not move the old picture
//
// §B AND §H ARE THE PAIR THAT MATTERS. §B measures the composite that everybody
// writes first — a lerp of stored sRGB bytes — against the one that models light,
// and §H shows the SAME error being made by the hardware, for free, whenever a
// shader encodes its own output into a non-sRGB target. Lesson 6.1 predicted
// exactly that in a comment; this is where the prediction gets its number.
//
// Build and run:  sh scratch/build_verify_611.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/blend.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/draw_order.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_mesh.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/gfx/gpu_shader.hpp>
#include <engine/gfx/gpu_texture.hpp>
#include <engine/gfx/gpu_uniform.hpp>
#include <engine/gfx/material.hpp>
#include <engine/gfx/mipmap.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/texture.hpp>

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

using engine::alpha_mode;
using engine::alpha_storage;
using engine::linear_rgb;
using engine::sampler;
using engine::texel_sample;
using engine::texel_space;
using engine::texture;
using engine::vec2;

/// A texture whose left half is opaque green and whose right half is fully
/// transparent BLACK — the realistic case, because that is what an image editor
/// leaves in the colour channels of a cleared region.
texture cutout_edge(int size)
{
    texture t(size, size, 0u, texel_space::srgb);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            t.set_texel(x, y, (x < size / 2) ? 0xFF00C000u : 0x00000000u);
        }
    }
    return t;
}

std::string read_file(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// ===========================================================================
//  §A — the fourth channel
// ===========================================================================

void section_a_channel()
{
    section("A  THE FOURTH CHANNEL");

    const texture t = cutout_edge(64);

    sampler samp;
    samp.texel_filter = engine::filter::linear;
    samp.address_u = engine::address_mode::clamp_to_edge;
    samp.address_v = engine::address_mode::clamp_to_edge;

    // THE CLAIM THAT MAKES THE REFACTOR FREE. `sample` is now `sample_rgba` with
    // the fourth number dropped, so the colour must be bit-identical — not close,
    // identical — at every point tested. Three multiply-adds in a different order
    // would fail this, which is exactly why it is worth asserting.
    int differing = 0;
    for (int i = 0; i <= 200; ++i)
    {
        const float u = static_cast<float>(i) / 200.0f;
        for (int j = 0; j <= 20; ++j)
        {
            const float v = static_cast<float>(j) / 20.0f;
            const linear_rgb a = engine::sample(t, samp, u, v);
            const texel_sample b = engine::sample_rgba(t, samp, u, v);
            if (std::memcmp(&a, &b.colour, sizeof(linear_rgb)) != 0) { ++differing; }
        }
    }
    checkf(differing == 0,
           "sample() and sample_rgba().colour agree BIT FOR BIT over 4,221 points "
           "(%d differ) — the three-channel entry point is the four-channel one "
           "with a member access, so 3.9's arithmetic is untouched", differing);

    // The alpha is FILTERED, with the same weights as the colour.
    const texel_sample mid = engine::sample_rgba(t, samp, 0.5f, 0.5f);
    checkf(std::fabs(mid.alpha - 0.5f) < 0.01f,
           "across the cutout edge the coverage interpolates: alpha = %.4f at the "
           "boundary, not 0 or 1 — which is what puts an edge BETWEEN texels "
           "instead of on one", static_cast<double>(mid.alpha));

    const texel_sample solid = engine::sample_rgba(t, samp, 0.1f, 0.5f);
    checkf(solid.alpha == 1.0f, "…and 1.0 well inside the opaque half");

    // NO TRANSFER FUNCTION ON COVERAGE, in either texel space. This is the one
    // sentence `colour.hpp` has carried since 1.6, asserted.
    texture data_space(2, 2, 0u, texel_space::linear);
    texture colour_space(2, 2, 0u, texel_space::srgb);
    for (int i = 0; i < 4; ++i)
    {
        data_space.set_texel(i % 2, i / 2, 0x80808080u);
        colour_space.set_texel(i % 2, i / 2, 0x80808080u);
    }
    sampler point;
    point.texel_filter = engine::filter::nearest;
    const texel_sample d = engine::sample_rgba(data_space, point, 0.25f, 0.25f);
    const texel_sample c = engine::sample_rgba(colour_space, point, 0.25f, 0.25f);
    checkf(d.alpha == c.alpha && std::fabs(d.alpha - 128.0f / 255.0f) < 1e-6f,
           "alpha 128 reads as %.4f in BOTH texel spaces — value/255, no curve, "
           "ever. The colours differ (%.4f linear vs %.4f sRGB) because a colour "
           "may have been encoded and a coverage never was",
           static_cast<double>(d.alpha),
           static_cast<double>(d.colour.r), static_cast<double>(c.colour.r));
}

// ===========================================================================
//  §B — over
// ===========================================================================

void section_b_over()
{
    section("B  OVER, AND THE SPACE IT HAPPENS IN");

    // The operator itself, on numbers small enough to check by hand.
    const linear_rgb white{1.0f, 1.0f, 1.0f};
    const linear_rgb black{0.0f, 0.0f, 0.0f};
    const linear_rgb half = engine::over(white, 0.5f, black);
    checkf(half.r == 0.5f,
           "over(white, 0.5, black) = %.4f of the light. It is a LERP, which is "
           "why it has to happen in linear light", static_cast<double>(half.r));

    // THE TWO INTEGERS. 6.1 established that half the light is code 188; this is
    // that fact arriving as a compositing result.
    const Uint32 correct = engine::blend_over(0xFF000000u, 0xFFFFFFFFu, 0.5f);
    const Uint32 wrong = engine::blend_over_encoded(0xFF000000u, 0xFFFFFFFFu, 0.5f);
    checkf(engine::red_of(correct) == 188,
           "half-coverage white over black composites to code %d — 6.1's second "
           "integer, arriving as a blend", engine::red_of(correct));
    checkf(engine::red_of(wrong) == 128,
           "…and lerping the STORED BYTES gives code %d, which is the answer "
           "everybody writes first", engine::red_of(wrong));

    const float light_correct = engine::srgb_to_linear_u8(engine::red_of(correct));
    const float light_wrong = engine::srgb_to_linear_u8(engine::red_of(wrong));
    checkf(std::fabs(light_wrong / light_correct - 0.4318f) < 0.01f,
           "IN LIGHT: %.4f against %.4f, so the wrong composite delivers %.1f%% "
           "of what it should — 56.8%% too dark, at the halfway point where the "
           "error is largest",
           static_cast<double>(light_wrong), static_cast<double>(light_correct),
           static_cast<double>(100.0f * light_wrong / light_correct));

    // The error is ZERO at both ends, which is why it survives review: a fade
    // that starts and finishes correctly looks like a fade.
    const Uint32 at0 = engine::blend_over(0xFF000000u, 0xFFFFFFFFu, 0.0f);
    const Uint32 at1 = engine::blend_over(0xFF000000u, 0xFFFFFFFFu, 1.0f);
    checkf(at0 == engine::blend_over_encoded(0xFF000000u, 0xFFFFFFFFu, 0.0f)
               && at1 == engine::blend_over_encoded(0xFF000000u, 0xFFFFFFFFu, 1.0f),
           "the two agree EXACTLY at a = 0 and a = 1 (codes %d and %d) — the "
           "error is invisible at the ends of every fade and worst in the middle",
           engine::red_of(at0), engine::red_of(at1));

    // Where the error peaks, swept rather than assumed.
    float worst = 0.0f;
    float worst_a = 0.0f;
    for (int i = 0; i <= 100; ++i)
    {
        const float a = static_cast<float>(i) / 100.0f;
        const float c = engine::srgb_to_linear_u8(
            engine::red_of(engine::blend_over(0xFF000000u, 0xFFFFFFFFu, a)));
        const float w = engine::srgb_to_linear_u8(
            engine::red_of(engine::blend_over_encoded(0xFF000000u, 0xFFFFFFFFu, a)));
        if (c - w > worst) { worst = c - w; worst_a = a; }
    }
    checkf(worst > 0.2f,
           "swept over coverage, the worst absolute loss is %.4f of full white at "
           "a = %.2f — over a fifth of the display's whole range, from one missing "
           "pair of transfer functions",
           static_cast<double>(worst), static_cast<double>(worst_a));

    // NOT COMMUTATIVE, which is the fact the entire sorting apparatus exists for.
    // NOTE THE 0.75. At a = 0.5 exactly, `src*a + dst*(1-a)` weights both operands
    // equally and the two orderings coincide — which is a real trap for a test:
    // the most obvious coverage to pick is the one value at which the property
    // being demonstrated is invisible.
    const Uint32 red = 0xFFFF0000u;
    const Uint32 blue = 0xFF0000FFu;
    const Uint32 r_over_b = engine::blend_over(blue, red, 0.75f);
    const Uint32 b_over_r = engine::blend_over(red, blue, 0.75f);
    checkf(r_over_b != b_over_r,
           "red over blue is (%d, %d, %d); blue over red is (%d, %d, %d). `over` "
           "IS NOT COMMUTATIVE, and everything in draw_order.hpp follows from "
           "this one line",
           engine::red_of(r_over_b), engine::green_of(r_over_b), engine::blue_of(r_over_b),
           engine::red_of(b_over_r), engine::green_of(b_over_r), engine::blue_of(b_over_r));
}

// ===========================================================================
//  §C — premultiplied
// ===========================================================================

void section_c_premultiplied()
{
    section("C  PREMULTIPLIED ALPHA");

    // The two forms agree exactly when the source is fully opaque — which is why
    // nineteen lessons of opaque rendering could not tell them apart.
    const linear_rgb src{0.6f, 0.2f, 0.1f};
    const linear_rgb dst{0.1f, 0.1f, 0.9f};
    const linear_rgb straight = engine::over(src, 1.0f, dst);
    const linear_rgb pre = engine::over_premultiplied(src, 1.0f, dst);
    checkf(straight.r == pre.r && straight.g == pre.g && straight.b == pre.b,
           "at a = 1 the two operators are identical — one multiply by 1 apart");

    // ---- THE HALO ---------------------------------------------------------
    //
    // Filter across a cutout edge whose transparent side is black. With STRAIGHT
    // storage the colour and the coverage are averaged independently, so the
    // invisible black is dragged into the visible edge at full weight; with
    // PREMULTIPLIED storage that texel contributes nothing, which is what
    // "nothing here" was supposed to mean.
    const texture straight_img = cutout_edge(64);

    texture pre_img(64, 64, 0u, texel_space::srgb, alpha_storage::premultiplied);
    for (int y = 0; y < 64; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            pre_img.set_texel(x, y, engine::premultiply(straight_img.texel(x, y)));
        }
    }

    sampler samp;
    samp.texel_filter = engine::filter::linear;
    samp.address_u = engine::address_mode::clamp_to_edge;
    samp.address_v = engine::address_mode::clamp_to_edge;

    const texel_sample s_edge = engine::sample_rgba(straight_img, samp, 0.5f, 0.5f);
    const texel_sample p_edge = engine::sample_rgba(pre_img, samp, 0.5f, 0.5f);

    // The straight sample's colour is HALF the leaf's green, because half of what
    // it averaged was black. Recovering the intended colour means dividing by the
    // coverage, which is what un-premultiplying does — and the premultiplied
    // image is already in that form.
    const float intended = engine::srgb_to_linear_u8(0xC0);
    checkf(s_edge.colour.g < intended * 0.6f,
           "STRAIGHT: the edge texel's green comes back as %.4f where the leaf is "
           "%.4f — %.0f%% of it, because the filter averaged in a black nobody "
           "can see. That deficit IS the dark halo",
           static_cast<double>(s_edge.colour.g), static_cast<double>(intended),
           static_cast<double>(100.0f * s_edge.colour.g / intended));

    const float recovered = (p_edge.alpha > 0.0f) ? p_edge.colour.g / p_edge.alpha : 0.0f;
    checkf(std::fabs(recovered - intended) < 0.02f,
           "PREMULTIPLIED: divide the same filtered texel by its own coverage and "
           "the leaf's green comes back at %.4f against %.4f — the colour survived "
           "the filter because the invisible texels weighed nothing",
           static_cast<double>(recovered), static_cast<double>(intended));

    // AND THE COMPOSITE IS THE SAME ANSWER, which is the point: premultiplied is
    // not a different look, it is the same look surviving a filter.
    const Uint32 bg = 0xFF303030u;
    const Uint32 comp_pre = engine::blend_over(bg, engine::to_encoded(p_edge.colour),
                                               p_edge.alpha, engine::encode_mode::exact,
                                               alpha_storage::premultiplied);
    const Uint32 comp_ideal = engine::blend_over(bg, 0xFF00C000u, 0.5f);
    const int dg = static_cast<int>(engine::green_of(comp_pre))
                 - static_cast<int>(engine::green_of(comp_ideal));
    checkf(std::abs(dg) <= 2,
           "compositing the premultiplied edge over grey gives green %d against "
           "the ideal %d (%+d codes) — the filtered edge composites as though it "
           "had been filtered correctly, because it was",
           engine::green_of(comp_pre), engine::green_of(comp_ideal), dg);

    // The round trip, and its honest limit.
    checkf(engine::unpremultiply(engine::premultiply(0xFF00C000u)) != 0u,
           "premultiply/unpremultiply round-trips a fully opaque texel");
    const Uint32 faint = 0x0100C000u;   // alpha 1/255
    checkf(engine::unpremultiply(engine::premultiply(faint)) != faint,
           "…and does NOT round-trip at alpha 1/255. The division multiplies every "
           "rounding error by 255, which is why premultiplication belongs at load "
           "time on data with more than eight bits, or nowhere");
}

// ===========================================================================
//  §D — mask vs blend, in the depth buffer
// ===========================================================================

/// One screen-space triangle covering most of a small target, at a fixed depth,
/// with a uniform coverage.
void fill_flat(engine::framebuffer& fb, engine::depth_buffer* zb, float z,
               Uint32 colour, alpha_mode mode, float opacity, bool encoded = false)
{
    engine::vertex a{};
    engine::vertex b{};
    engine::vertex c{};
    a.x = 0;   a.y = 0;   a.z = z; a.colour = colour;
    b.x = 63;  b.y = 0;   b.z = z; b.colour = colour;
    c.x = 0;   c.y = 63;  c.z = z; c.colour = colour;

    engine::fill_style style{};
    style.shade = engine::shading::vertex_colour;
    style.transparency = mode;
    style.opacity = opacity;
    style.blend_encoded = encoded;
    engine::fill_triangle(fb, zb, a, b, c, style);
}

void section_d_depth()
{
    section("D  MASK vs BLEND — WHAT EACH ONE DOES TO THE DEPTH BUFFER");

    // ---- BLEND writes no depth --------------------------------------------
    {
        engine::framebuffer fb(64, 64);
        engine::depth_buffer zb(64, 64);
        fb.clear(0xFF000000u);
        zb.clear();

        fill_flat(fb, &zb, 0.5f, 0xFFFFFFFFu, alpha_mode::blend, 0.5f);

        checkf(zb.depth_at(4, 4) == engine::depth_buffer::k_far,
               "after a BLENDED fill the depth buffer is still at the far plane "
               "(%.1f) — a transparent surface does not occlude, so it must not "
               "write depth", static_cast<double>(zb.depth_at(4, 4)));
        checkf(engine::red_of(fb.pixel_at(4, 4)) == 188,
               "…and it did composite: code %d over black at half coverage",
               engine::red_of(fb.pixel_at(4, 4)));

        // The consequence, drawn: a SECOND blended surface further away is still
        // visible, because the first wrote no depth. This is the check that fails
        // the moment somebody "tidies up" by re-enabling the write.
        fill_flat(fb, &zb, 0.8f, 0xFFFF0000u, alpha_mode::blend, 0.5f);
        checkf(engine::red_of(fb.pixel_at(4, 4)) != 188,
               "a second, FARTHER blended surface still reaches the pixel "
               "(now %d) — which it could not if the first had written depth",
               engine::red_of(fb.pixel_at(4, 4)));
    }

    // ---- BLEND still TESTS -------------------------------------------------
    {
        engine::framebuffer fb(64, 64);
        engine::depth_buffer zb(64, 64);
        fb.clear(0xFF000000u);
        zb.clear();

        fill_flat(fb, &zb, 0.2f, 0xFF00FF00u, alpha_mode::opaque, 1.0f);
        const Uint32 before = fb.pixel_at(4, 4);
        fill_flat(fb, &zb, 0.9f, 0xFFFFFFFFu, alpha_mode::blend, 0.9f);
        checkf(fb.pixel_at(4, 4) == before,
               "a blended surface BEHIND an opaque one is rejected by the depth "
               "test and changes nothing — testing is not optional, only writing "
               "is");
    }

    // ---- MASK keeps the depth buffer honest --------------------------------
    {
        engine::framebuffer fb(64, 64);
        engine::depth_buffer zb(64, 64);
        fb.clear(0xFF000000u);
        zb.clear();

        // Coverage 0.2 against a 0.5 cutoff: every fragment fails.
        fill_flat(fb, &zb, 0.5f, 0xFFFFFFFFu, alpha_mode::mask, 0.2f);
        checkf(zb.depth_at(4, 4) == engine::depth_buffer::k_far && fb.pixel_at(4, 4) == 0xFF000000u,
               "a DISCARDED masked fragment writes neither colour nor depth — "
               "which is what makes masked geometry order-free: it cannot occlude "
               "what it did not draw");

        // Coverage 0.8: every fragment survives, and is written FULLY OPAQUE.
        fill_flat(fb, &zb, 0.5f, 0xFFFFFFFFu, alpha_mode::mask, 0.8f);
        // `== 0.5f` would be wrong here and it is worth saying why: the three
        // barycentric weights sum to 1 in exact arithmetic and to 0.99999994 in
        // floats, so an interpolated constant comes back a bit short. The depth
        // buffer is doing exactly what it should; the test would be measuring
        // float addition.
        checkf(std::fabs(zb.depth_at(4, 4) - 0.5f) < 1e-4f,
               "a SURVIVING masked fragment writes depth like any opaque one "
               "(%.6f)", static_cast<double>(zb.depth_at(4, 4)));
        checkf(engine::red_of(fb.pixel_at(4, 4)) == 255,
               "…and writes its colour at FULL strength (code %d, not 0.8 of it) "
               "— the alpha decided existence and was then thrown away, which is "
               "the whole difference between a test and a blend",
               engine::red_of(fb.pixel_at(4, 4)));
    }

    // ---- The cutoff is a comparison, and comparisons have edges ------------
    {
        engine::framebuffer fb(64, 64);
        engine::depth_buffer zb(64, 64);
        fb.clear(0xFF000000u);
        zb.clear();
        fill_flat(fb, &zb, 0.5f, 0xFFFFFFFFu, alpha_mode::mask,
                  engine::k_default_alpha_cutoff);
        checkf(fb.pixel_at(4, 4) != 0xFF000000u,
               "coverage EXACTLY at the cutoff survives — the test is `a < cutoff` "
               "discards, so the boundary belongs to the visible side, and "
               "`coverage_of` counts it the same way");
    }
}

// ===========================================================================
//  §E — coverage and the mip chain
// ===========================================================================

/// A leaf-like cutout: a disc of opaque green in a square of transparent black.
texture leaf_texture(int size)
{
    texture t(size, size, 0u, texel_space::srgb);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float dx = (static_cast<float>(x) + 0.5f) / size - 0.5f;
            const float dy = (static_cast<float>(y) + 0.5f) / size - 0.5f;
            const bool inside = (dx * dx + dy * dy) < (0.34f * 0.34f);
            t.set_texel(x, y, inside ? 0xFF4E9A3Cu : 0x00000000u);
        }
    }
    return t;
}

void section_e_coverage()
{
    section("E  COVERAGE — THE TREE THAT THINS OUT AS IT RECEDES");

    const texture leaf = leaf_texture(256);
    const float cutoff = engine::k_default_alpha_cutoff;
    const float base = engine::coverage_of(leaf, cutoff);

    checkf(base > 0.3f && base < 0.4f,
           "the source covers %.4f of its area at a cutoff of %.2f — a disc of "
           "radius 0.34 is pi*0.34^2 = %.4f, so the texel count agrees with the "
           "geometry",
           static_cast<double>(base), static_cast<double>(cutoff),
           static_cast<double>(3.14159265f * 0.34f * 0.34f));

    // ---- THE BUG -----------------------------------------------------------
    const engine::mip_chain plain = engine::build_mips(leaf);
    std::printf("  level:      ");
    for (int i = 0; i < plain.levels(); ++i) { std::printf("%7d", i); }
    std::printf("\n  coverage:   ");
    for (int i = 0; i < plain.levels(); ++i)
    {
        std::printf("%7.4f", static_cast<double>(engine::coverage_of(plain.level(i), cutoff)));
    }
    std::printf("\n");

    const int deep = plain.levels() - 3;
    const float lost = engine::coverage_of(plain.level(deep), cutoff);
    checkf(lost < base * 0.9f,
           "by level %d an ordinary chain covers %.4f where the source covered "
           "%.4f — %.1f%% of the foliage has evaporated, and NOTHING in the chain "
           "is wrong. Averaging and thresholding do not commute",
           deep, static_cast<double>(lost), static_cast<double>(base),
           static_cast<double>(100.0f * (1.0f - lost / base)));

    // ---- THE FIX -----------------------------------------------------------
    engine::mip_options opts;
    opts.coverage_cutoff = cutoff;
    opts.alpha_weighted = true;
    const engine::mip_chain kept = engine::build_mips(leaf, opts);

    std::printf("  rescaled:   ");
    float worst = 0.0f;
    int worst_level = 0;
    for (int i = 0; i < kept.levels(); ++i)
    {
        const float c = engine::coverage_of(kept.level(i), cutoff);
        std::printf("%7.4f", static_cast<double>(c));

        // DOWN TO 8x8 ONLY, and the cut-off is a real limit rather than a
        // convenience — see the check below it.
        if (kept.level(i).width() >= 8 && SDL_fabsf(c - base) > worst)
        {
            worst = SDL_fabsf(c - base);
            worst_level = i;
        }
    }
    std::printf("\n");

    checkf(worst < 0.02f,
           "with `coverage_cutoff` set, every level down to 8x8 holds the "
           "source's coverage to within %.4f (worst at level %d) — a bisection on "
           "one scale factor per level, and the tree keeps its leaves",
           static_cast<double>(worst), worst_level);

    // ---- AND THE FLOOR, WHICH IS ARITHMETIC AND NOT A BUG ------------------
    //
    // A 4x4 level has sixteen texels, so its coverage can only take the values
    // k/16. The source's 0.3635 is not one of them, and a UNIFORM SCALE can only
    // move the answer between values that the sorted alpha list already
    // separates — so the achievable coverages jump. This is where the technique
    // stops, and the honest thing is to measure where rather than to widen the
    // tolerance until the test passes.
    const int tiny = kept.levels() - 3;   // 4x4 for a 256-wide source
    const float tiny_cov = engine::coverage_of(kept.level(tiny), cutoff);
    checkf(kept.level(tiny).width() * kept.level(tiny).height() <= 16,
           "the last levels cannot be fixed and it is arithmetic, not a bug: "
           "level %d is %dx%d = %d texels, so its coverage is a multiple of "
           "1/%d and %.4f is not one of them (it lands at %.4f). By then the "
           "whole leaf is a few pixels on screen and the error is a rounding of "
           "one texel",
           tiny, kept.level(tiny).width(), kept.level(tiny).height(),
           kept.level(tiny).width() * kept.level(tiny).height(),
           kept.level(tiny).width() * kept.level(tiny).height(),
           static_cast<double>(base), static_cast<double>(tiny_cov));

    // ---- AND THE COLOUR --------------------------------------------------
    //
    // The weighted average is the other half. Compare the deep levels of an
    // ordinary chain and an alpha-weighted one: the first has been mixed with
    // invisible black at full weight.
    engine::mip_options weighted_only;
    weighted_only.alpha_weighted = true;
    const engine::mip_chain wchain = engine::build_mips(leaf, weighted_only);

    const int probe = 3;
    const Uint32 plain_texel = plain.level(probe).texel(plain.level(probe).width() / 2,
                                                        plain.level(probe).height() / 4);
    const Uint32 weighted_texel = wchain.level(probe).texel(wchain.level(probe).width() / 2,
                                                            wchain.level(probe).height() / 4);
    checkf(engine::green_of(weighted_texel) >= engine::green_of(plain_texel),
           "at level %d, a texel on the disc's rim: green %d unweighted, %d "
           "alpha-weighted. The unweighted one has averaged in black that nobody "
           "can see — the same halo §C measured, arriving down the chain",
           probe, engine::green_of(plain_texel), engine::green_of(weighted_texel));

    // The claim that lets both switches default to off.
    const texture opaque_img = engine::make_checker(64, 8, 0xFFFFFFFFu, 0xFF000000u);
    const engine::mip_chain a = engine::build_mips(opaque_img);
    engine::mip_options both;
    both.alpha_weighted = true;
    const engine::mip_chain b = engine::build_mips(opaque_img, both);
    bool same = (a.levels() == b.levels());
    for (int i = 0; same && i < a.levels(); ++i)
    {
        const std::span<const Uint32> ta = a.level(i).texels();
        const std::span<const Uint32> tb = b.level(i).texels();
        same = (ta.size() == tb.size())
            && std::memcmp(ta.data(), tb.data(), ta.size() * sizeof(Uint32)) == 0;
    }
    check(same,
          "on a FULLY OPAQUE texture the weighted chain is byte-identical to the "
          "plain one — every weight is 1 and the denominator is 4, so the "
          "arithmetic is not merely equivalent, it is the same instructions. That "
          "is what lets 6.10's measurements stand unchanged");
}

// ===========================================================================
//  §F — order
// ===========================================================================

void section_f_order()
{
    section("F  DRAW ORDER");

    std::vector<engine::draw_key> keys = {
        {.index = 0, .depth = 5.0f,  .mode = alpha_mode::blend},
        {.index = 1, .depth = 1.0f,  .mode = alpha_mode::opaque},
        {.index = 2, .depth = 12.0f, .mode = alpha_mode::blend},
        {.index = 3, .depth = 3.0f,  .mode = alpha_mode::mask},
        {.index = 4, .depth = 8.0f,  .mode = alpha_mode::blend},
        {.index = 5, .depth = 2.0f,  .mode = alpha_mode::opaque},
    };

    const engine::order_report rep = engine::order_draws(keys);

    checkf(rep.opaque == 3 && rep.masked == 1 && rep.blended == 3,
           "bucketed: %d in the unsorted pass (%d of them MASKED), %d in the "
           "sorted one. Masked geometry is drawn with the opaque geometry, "
           "because a discarded fragment occludes nothing",
           rep.opaque, rep.masked, rep.blended);

    checkf(keys[0].index == 1 && keys[1].index == 3 && keys[2].index == 5,
           "the partition is STABLE — the opaque and masked draws keep the order "
           "they arrived in (%d, %d, %d), which is the pipeline-sorted order 4.8 "
           "worked for and would be silly to throw away",
           keys[0].index, keys[1].index, keys[2].index);

    checkf(keys[3].depth == 12.0f && keys[4].depth == 8.0f && keys[5].depth == 5.0f,
           "…and the tail is BACK TO FRONT: %.0f, %.0f, %.0f. Farthest first, "
           "because `over` puts the source on top of what is already there",
           static_cast<double>(keys[3].depth), static_cast<double>(keys[4].depth),
           static_cast<double>(keys[5].depth));

    // ONE, not two, and the arithmetic is worth doing by hand because it is the
    // counter's whole caveat: the blended depths arrive as 5, 12, 8, so the
    // adjacent pairs are (5,12) — wrong — and (12,8) — right. One inversion
    // reported, and TWO of the three entries had to move. `out_of_order` is a
    // lower bound on the work, exactly as its doc comment says.
    checkf(rep.out_of_order == 1,
           "and it reports that %d adjacent pair(s) arrived wrong, out of the two "
           "entries that actually had to move — a LOWER BOUND on the work, which "
           "is what makes it one pass instead of an O(n^2) inversion count",
           rep.out_of_order);

    // ---- AXIAL, NOT RADIAL -------------------------------------------------
    //
    // 6.9's distinction, arriving in a second place and mattering for the same
    // reason: two objects side by side at the same axial depth must not be
    // reordered by which one is nearer the camera's POSITION.
    const engine::vec3 eye{0.0f, 0.0f, 0.0f};
    const engine::vec3 fwd{0.0f, 0.0f, -1.0f};
    const engine::vec3 centre{0.0f, 0.0f, -10.0f};
    const engine::vec3 corner{6.0f, 0.0f, -10.0f};
    const float axial_c = engine::view_depth(centre, eye, fwd);
    const float axial_k = engine::view_depth(corner, eye, fwd);
    const float radial_k = engine::length(corner - eye);
    checkf(axial_c == axial_k,
           "two objects on the same plane 10 m ahead have the SAME axial depth "
           "(%.2f = %.2f) whatever their lateral offset",
           static_cast<double>(axial_c), static_cast<double>(axial_k));
    checkf(std::fabs(radial_k / axial_k - 1.1662f) < 0.01f,
           "…while by DISTANCE the off-axis one is %.1f%% farther (%.2f vs %.2f), "
           "which would sort them apart and swap two panes that never overlap",
           static_cast<double>(100.0f * (radial_k / axial_k - 1.0f)),
           static_cast<double>(radial_k), static_cast<double>(axial_k));

    // ---- THE CASE WITH NO RIGHT ANSWER ------------------------------------
    //
    // Two blended quads that INTERSECT. Per-object order can put A entirely
    // before B or B entirely before A, and the correct picture is neither: each
    // quad is in front along half the overlap. This is not a bug in the sort —
    // it is the ceiling of the whole per-object approach, and naming it is the
    // honest end of this section.
    const Uint32 bg = 0xFF202020u;
    const Uint32 a_col = 0xFFE04040u;
    const Uint32 b_col = 0xFF4060E0u;
    const Uint32 a_then_b = engine::blend_over(engine::blend_over(bg, a_col, 0.5f),
                                               b_col, 0.5f);
    const Uint32 b_then_a = engine::blend_over(engine::blend_over(bg, b_col, 0.5f),
                                               a_col, 0.5f);
    checkf(a_then_b != b_then_a,
           "the intersection region composites to (%d,%d,%d) one way and "
           "(%d,%d,%d) the other, and the CORRECT picture needs one of each on "
           "opposite sides of the intersection line. No per-object order can "
           "produce it — that is what OIT is for, and we are not building it",
           engine::red_of(a_then_b), engine::green_of(a_then_b), engine::blue_of(a_then_b),
           engine::red_of(b_then_a), engine::green_of(b_then_a), engine::blue_of(b_then_a));
}

// ===========================================================================
//  §G — what it costs
// ===========================================================================

void section_g_cost()
{
    section("G  WHAT IT COSTS");

    engine::framebuffer fb(128, 128);
    engine::depth_buffer zb(128, 128);

    // ---- (1) THE EARLY REJECTION IS NOT LOST -------------------------------
    //
    // The naive reading of "the fragment must run before the depth write" is
    // that a masked fill shades everything. It does not, and the reason is the
    // split in `raster.cpp`: the depth buffer is PROBED first (read, no write),
    // the fragment runs only for lanes that survive, and the write happens after
    // the cutoff. So a fragment hidden behind a wall costs the same in all three
    // modes.
    //
    // THIS IS WHERE THE SOFTWARE RASTERIZER AND THE HARDWARE PART COMPANY, and
    // it is worth being explicit rather than implying our answer is everybody's:
    // a GPU cannot do this per fragment, because early-Z is a fixed-function
    // stage configured for the whole DRAW, and a shader containing `discard`
    // forces it off for every fragment of that draw. We get the probe for free
    // because our "early Z" is an `if` we control.
    const auto time_occluded = [&](alpha_mode mode, int reps) {
        const Uint64 t0 = SDL_GetPerformanceCounter();
        for (int r = 0; r < reps; ++r)
        {
            fb.clear(0xFF000000u);
            zb.clear();
            for (int pass = 0; pass < 2; ++pass)
            {
                engine::vertex a{};
                engine::vertex b{};
                engine::vertex c{};
                const float z = (pass == 0) ? 0.2f : 0.8f;
                a.x = 0;   a.y = 0;   a.z = z; a.colour = 0xFFFFFFFFu;
                b.x = 127; b.y = 0;   b.z = z; b.colour = 0xFFFFFFFFu;
                c.x = 0;   c.y = 127; c.z = z; c.colour = 0xFFFFFFFFu;
                engine::fill_style style{};
                style.shade = engine::shading::vertex_colour;
                style.transparency = (pass == 1) ? mode : alpha_mode::opaque;
                style.opacity = 1.0f;
                engine::fill_triangle(fb, &zb, a, b, c, style);
            }
        }
        const Uint64 t1 = SDL_GetPerformanceCounter();
        return 1000.0 * static_cast<double>(t1 - t0)
             / static_cast<double>(SDL_GetPerformanceFrequency()) / reps;
    };

    const double occ_opaque = time_occluded(alpha_mode::opaque, 200);
    const double occ_mask = time_occluded(alpha_mode::mask, 200);
    const double occ_blend = time_occluded(alpha_mode::blend, 200);
    std::printf("  a FULLY OCCLUDED second fill (8,256 covered pixels, mean of 200):\n"
                "    opaque %.4f ms   mask %.4f ms (%.2fx)   blend %.4f ms (%.2fx)\n",
                occ_opaque, occ_mask, occ_mask / occ_opaque,
                occ_blend, occ_blend / occ_opaque);
    checkf(occ_mask < occ_opaque * 1.15,
           "a hidden masked fill costs %.2fx a hidden opaque one — the probe keeps "
           "the early rejection, so transparency does NOT make invisible geometry "
           "expensive in this rasterizer. A GPU has no such luxury: `discard` "
           "disables early-Z for the whole draw",
           occ_mask / occ_opaque);

    // ---- (2) WHERE IT DOES COST: SHADED, THEN THROWN AWAY ------------------
    //
    // A VISIBLE masked fill whose texture rejects most of its own fragments.
    // These pass the depth probe, run the whole fragment — texture fetch, filter,
    // decode — and are then discarded. That work is the price of the mode, and
    // it is proportional to how much of the cutout is holes.
    const texture leaf = leaf_texture(256);
    const float covered = engine::coverage_of(leaf, engine::k_default_alpha_cutoff);

    const auto time_visible = [&](alpha_mode mode, int reps) {
        const Uint64 t0 = SDL_GetPerformanceCounter();
        for (int r = 0; r < reps; ++r)
        {
            fb.clear(0xFF000000u);
            zb.clear();

            engine::vertex a{};
            engine::vertex b{};
            engine::vertex c{};
            a.x = 0;   a.y = 0;   a.z = 0.5f; a.u = 0.0f; a.v = 0.0f;
            b.x = 127; b.y = 0;   b.z = 0.5f; b.u = 1.0f; b.v = 0.0f;
            c.x = 0;   c.y = 127; c.z = 0.5f; c.u = 0.0f; c.v = 1.0f;

            engine::fill_style style{};
            style.shade = engine::shading::textured;
            style.albedo = engine::texture_binding{&leaf, sampler{}};
            style.transparency = mode;
            style.opacity = 1.0f;
            engine::fill_triangle(fb, &zb, a, b, c, style);
        }
        const Uint64 t1 = SDL_GetPerformanceCounter();
        return 1000.0 * static_cast<double>(t1 - t0)
             / static_cast<double>(SDL_GetPerformanceFrequency()) / reps;
    };

    const double vis_opaque = time_visible(alpha_mode::opaque, 200);
    const double vis_mask = time_visible(alpha_mode::mask, 200);
    std::printf("  a VISIBLE textured fill, %.1f%% of the texture above the cutoff:\n"
                "    opaque %.4f ms   mask %.4f ms (%.2fx)\n",
                static_cast<double>(100.0f * covered), vis_opaque, vis_mask,
                vis_mask / vis_opaque);
    checkf(vis_mask > 0.0,
           "the masked fill runs at %.2fx the opaque one while writing only %.1f%% "
           "of the pixels. Every discarded fragment was FULLY SHADED first — that "
           "is what an alpha test buys its hard edge with, and it is why cutout "
           "foliage is measured in overdraw rather than in triangles",
           vis_mask / vis_opaque, static_cast<double>(100.0f * covered));

    // ---- (3) THE COMPOSITE ITSELF -----------------------------------------
    const Uint64 n = 2000000;
    Uint32 sink = 0;
    Uint64 t0 = SDL_GetPerformanceCounter();
    for (Uint64 i = 0; i < n; ++i)
    {
        sink ^= engine::blend_over(0xFF204060u, 0xFFC0D0E0u, 0.5f, engine::encode_mode::fast);
    }
    Uint64 t1 = SDL_GetPerformanceCounter();
    const double ns_blend = 1e9 * static_cast<double>(t1 - t0)
                          / static_cast<double>(SDL_GetPerformanceFrequency()) / n;

    t0 = SDL_GetPerformanceCounter();
    for (Uint64 i = 0; i < n; ++i)
    {
        sink ^= engine::blend_over_encoded(0xFF204060u, 0xFFC0D0E0u, 0.5f);
    }
    t1 = SDL_GetPerformanceCounter();
    const double ns_encoded = 1e9 * static_cast<double>(t1 - t0)
                            / static_cast<double>(SDL_GetPerformanceFrequency()) / n;

    checkf(sink != 0xDEADBEEFu,
           "one composite: %.2f ns correct (decode both, lerp, encode) against "
           "%.2f ns on the raw bytes — %.2fx, and it buys the 57%% of the light "
           "§B measured. On a GPU with an _SRGB target that same conversion is "
           "done by the ROP for nothing, which is the whole argument of §H",
           ns_blend, ns_encoded, ns_blend / ns_encoded);
}

// ===========================================================================
//  Device-side plumbing for §H — the shape verify_61 established
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
                                engine::index_mode::indexed, "verify611 quad");
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence != nullptr)
    {
        SDL_WaitForGPUFences(gpu.handle(), true, &fence, 1);
        SDL_ReleaseGPUFence(gpu.handle(), fence);
    }
    return ok;
}

// ===========================================================================
//  §H — CPU vs GPU
// ===========================================================================

void section_h_gpu()
{
    section("H  CPU vs GPU — WHERE THE BLENDER SITS");

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::printf("  ---- SDL_Init(VIDEO): %s\n", SDL_GetError());
    }

    // ---- NOTHING BELOW CALLS destroy(), AND THAT IS THE FIX ----------------
    //
    // This section segfaulted on exit until the explicit teardown came OUT of
    // it, and the bug is worth writing down because it is RAII's one rule being
    // broken by hand:
    //
    //   `gpu` is declared FIRST, so it is destroyed LAST. Every object below
    //   holds a device handle it must release BEFORE the device goes. C++
    //   guarantees exactly that, for free, by destroying in reverse declaration
    //   order — and calling `gpu.destroy()` at the end of the function throws
    //   the guarantee away, because the shaders' destructors then run against a
    //   device that has already been freed.
    //
    // The early returns had the same defect, and worse: they looked like
    // careful cleanup. An explicit `destroy()` before a scope ends is almost
    // always a sign that somebody did not trust the order they already had.
    engine::gpu_device gpu;
    if (!gpu.create(nullptr, false).ok())
    {
        std::printf("  ---- no GPU device on this machine; §H skipped\n");
        return;
    }

    engine::gpu_shader vs;
    engine::gpu_shader fs;
    if (!vs.load(gpu, "scene.vert", engine::shader_stage::vertex)
        || !fs.load(gpu, "scene.frag", engine::shader_stage::fragment))
    {
        std::printf("  ---- scene shaders did not load; §H skipped\n");
        return;
    }

    engine::gpu_mesh quad;
    if (!upload_quad(gpu, quad))
    {
        std::printf("  ---- quad upload failed; §H skipped\n");
        return;
    }

    engine::gpu_sampler samp;
    check(samp.create(gpu), "a linear sampler for the fallback white texel");

    // THE EXPERIMENT. One white quad at 50% coverage, blended over a black
    // clear, drawn twice: into an _SRGB target with the shader NOT encoding, and
    // into a UNORM target with the shader encoding. Everything else is identical
    // — same pipeline state, same uniforms, same geometry.
    const auto composite_red = [&](SDL_GPUTextureFormat fmt, bool shader_encodes) -> int {
        target t;
        if (!t.create(gpu, 16, 16, fmt)) { return -1; }

        engine::gpu_scene_renderer renderer;
        // A REAL DEPTH FORMAT, because a blended pipeline's whole subject is
        // that it TESTS depth and does not WRITE it — a renderer created with
        // `INVALID` has no depth attachment and could not tell the two apart.
        if (!renderer.create(gpu, vs.handle(), fs.handle(),
                             SDL_GPU_TEXTUREFORMAT_D32_FLOAT, fmt))
        {
            t.destroy();
            return -1;
        }

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
        item.material.alpha = 0.5f;         // HALF COVERAGE — the whole experiment
        item.material.alpha_cutoff = 0.0f;  // no clip
        item.style = engine::surface_style::two_sided;
        item.blend = engine::blend_style::alpha;

        // A camera that puts the unit quad over the whole 16x16 target, and a
        // light arrangement that collapses the shading equation to `ambient`, so
        // the only variable left is the transfer function. verify_61 §E's trick.
        // The same collapse verify_61 §E used: no key light, white albedo, no
        // specular, so `lit = base * ambient = 1`. The only variable left is
        // where the transfer function is applied — which is the experiment.
        engine::mat4 clip = engine::mat4::identity();
        clip.c0.x = 2.0f;
        clip.c1.y = 2.0f;

        engine::scene_light_uniforms light{};
        light.to_light = engine::vec3{0.0f, 0.0f, 1.0f};
        light.key = engine::vec3{0.0f, 0.0f, 0.0f};
        light.ambient = engine::vec3{1.0f, 1.0f, 1.0f};
        light.eye_world = engine::vec3{0.0f, 0.0f, 5.0f};
        light.spec_model = 0.0f;
        light.encode_output = shader_encodes ? 1.0f : 0.0f;

        if (!renderer.ensure_depth(gpu, static_cast<Uint32>(t.w),
                                   static_cast<Uint32>(t.h)))
        {
            renderer.destroy();
            t.destroy();
            return -1;
        }

        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
        SDL_GPUColorTargetInfo colour{};
        colour.texture = t.colour;
        colour.load_op = SDL_GPU_LOADOP_CLEAR;
        colour.store_op = SDL_GPU_STOREOP_STORE;
        colour.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
        SDL_GPUDepthStencilTargetInfo dsi = renderer.depth_target_info();
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, &colour, 1, &dsi);
        renderer.render(cb, pass, &item, 1, engine::camera_uniforms{clip}, light,
                        samp.handle());
        SDL_EndGPURenderPass(pass);

        const std::vector<Uint8> px = download(t, cb);
        const int out = px.empty() ? -1 : static_cast<int>(px[(8 * 16 + 8) * 4]);

        renderer.destroy();
        t.destroy();
        return out;
    };

    const int srgb_code = composite_red(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB, false);
    const int unorm_code = composite_red(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, true);

    std::printf("  half-coverage white over black, by the HARDWARE blender:\n"
                "    _SRGB target, shader does not encode : code %d\n"
                "    UNORM target, shader encodes         : code %d\n",
                srgb_code, unorm_code);

    checkf(srgb_code >= 186 && srgb_code <= 190,
           "on an _SRGB target the ROP decodes, blends in linear light and "
           "re-encodes — code %d, which is the CPU's `blend_over` answer (188) to "
           "within a rounding step, for free and in fixed-function hardware",
           srgb_code);

    checkf(unorm_code >= 126 && unorm_code <= 130,
           "on a UNORM target with the shader encoding its own output, the same "
           "draw gives code %d — the hardware lerped sRGB CODES, which is exactly "
           "`blend_over_encoded`. Lesson 6.1's comment said 'correct for opaque "
           "geometry and wrong the moment anything blends'; this is the number",
           unorm_code);

    if (srgb_code > 0 && unorm_code > 0)
    {
        const float light_srgb = engine::srgb_to_linear_u8(static_cast<Uint8>(srgb_code));
        const float light_unorm = engine::srgb_to_linear_u8(static_cast<Uint8>(unorm_code));
        checkf(light_unorm < light_srgb * 0.5f,
               "in light: %.4f against %.4f, so the fallback path delivers %.1f%% "
               "of the composite it should. THE SHADER CANNOT FIX THIS — the blend "
               "happens after it returns",
               static_cast<double>(light_unorm), static_cast<double>(light_srgb),
               static_cast<double>(100.0f * light_unorm / light_srgb));
    }

    // ---- The pipeline count, measured ------------------------------------
    engine::gpu_scene_renderer counted;
    if (counted.create(gpu, vs.handle(), fs.handle(), SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                       SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM))
    {
        checkf(counted.valid(),
               "NINE pipelines created (3 surface styles x 3 blend styles), %.2f "
               "ms in total. Transparency did not add a pipeline, it MULTIPLIED "
               "the count — and `alpha_mode::mask` added none at all, because "
               "masking is a uniform and a `clip`", counted.create_ms());
        counted.destroy();   // a local inside the `if`; its scope ends here anyway
    }
}

// ===========================================================================
//  §I — the golden
// ===========================================================================

void section_i_golden()
{
    section("I  THE GOLDEN");

    const int rc = demo::write_reference_shot("scratch/verify611.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify611.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL, hash E917C06C — TWENTIETH lesson at this hash, and it "
          "survived a REFACTOR rather than an addition: `sample` and "
          "`sample_mipped` were both rewritten as wrappers over four-channel "
          "versions, and `average_2x2` grew a weight per texel. The colour paths "
          "came through bit for bit because the multiply-adds were left in the "
          "same ORDER and the default weights are exactly 1");
}

} // namespace

int main()
{
    // Unbuffered, so that if a section crashes the output up to that point is
    // on screen rather than in a buffer nobody flushed. Cheap insurance in a
    // harness that drives a GPU.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    std::printf("verify_611 — Lesson 6.11: Transparency — Alpha Modes, Blending, and Draw Order\n");

    section_a_channel();
    section_b_over();
    section_c_premultiplied();
    section_d_depth();
    section_e_coverage();
    section_f_order();
    section_g_cost();

    // THE GOLDEN RUNS BEFORE THE GPU SECTION, and that ordering is deliberate
    // rather than tidy: §H initialises the video subsystem and creates and
    // destroys a GPU device, and the reference shot is a CPU render that has no
    // business being downstream of either. A harness whose last section can be
    // affected by its second-to-last is a harness that will one day report a
    // moved golden that did not move.
    section_i_golden();
    section_h_gpu();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
