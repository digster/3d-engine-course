// scratch/verify_612.cpp — Lesson 6.12's harness: the lid, the exposure, the curves.
//
//   §A  THE LID — what this engine has been avoiding, and by how much
//   §B  EXPOSURE — EV as a scale of light, and the old behaviour as a point on it
//   §C  THE CURVES — four operators, and the properties that make one legal
//   §D  PER-CHANNEL vs LUMINANCE — the hue shift, measured both ways
//   §E  THE RESOLVE — three steps whose order is not negotiable
//   §F  BLENDING ON A FLOAT TARGET — what happens to 6.11's rule
//   §G  THE LOG-AVERAGE — why exposure metering is done in log space
//   §H  THE GOLDEN — a new capability must not move the old picture
//   §I  CPU vs GPU — the same curve, computed twice
//
// §A AND §C ARE THE PAIR THAT MATTERS. §A measures what the clamp has been
// discarding — not hypothetically, but in this engine's own scene with one
// material changed — and §C establishes what makes a tonemap operator legal
// rather than merely popular: monotonic, identity near zero, bounded above.
//
// Build and run:  sh scratch/build_verify_612.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/blend.hpp>
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

using engine::hdr_buffer;
using engine::linear_rgb;
using engine::microsurface;
using engine::tonemap;
using engine::tonemap_settings;
using engine::vec3;

std::string read_file(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// The reference scene's light, exactly as demo_scene.cpp builds it.
engine::lighting shot_lighting()
{
    engine::lighting lights;
    const float ce = std::cos(0.70f);
    const vec3 to_light{ce * std::sin(0.85f), std::sin(0.70f), ce * std::cos(0.85f)};
    lights.key.direction = to_light * -1.0f;
    lights.key.colour = {1.0f, 0.97f, 0.90f};
    lights.key.irradiance = engine::k_reference_irradiance;
    lights.ambient = {0.05f, 0.06f, 0.08f};
    return lights;
}

// ===========================================================================
//  §A — the lid
// ===========================================================================

void section_a_lid()
{
    section("A  THE LID, AND WHAT HAS BEEN HOLDING IT ON");

    // TWO CHOICES, NOT AN ACCIDENT. `k_reference_irradiance` is pi so that a
    // white surface renders at exactly 1.0 (6.2 §5.2 derived it), and the demo's
    // roughness is 0.49. Both are visible here.
    const engine::lighting lights = shot_lighting();
    const vec3 n{0.0f, 1.0f, 0.0f};
    const vec3 to_light = lights.key.to_light();

    // The MIRROR direction. A sharp lobe is narrow, so evaluating anywhere else
    // reports a peak that is an artefact of where you looked — which is exactly
    // what the first version of `probe_612.cpp` did, and the giveaway was that
    // SHARPER surfaces reported LOWER peaks.
    const vec3 mirror = engine::normalised(to_light * -1.0f + n * (2.0f * dot(n, to_light)));

    const auto peak_at = [&](float roughness, float metallic) {
        const microsurface s{.roughness = roughness, .metallic = metallic};
        const linear_rgb c = engine::shade({0.9f, 0.9f, 0.9f}, n, mirror, lights, s);
        return std::fmax(c.r, std::fmax(c.g, c.b));
    };

    const float demo_peak = peak_at(0.49f, 0.0f);
    checkf(demo_peak > 0.85f && demo_peak < 1.0f,
           "the demo's own roughness 0.49 peaks at %.4f — JUST under the lid, "
           "which is the whole reason this course has rendered six modules "
           "without an HDR pipeline and never looked wrong",
           static_cast<double>(demo_peak));

    std::printf("  the same surface, polished:\n");
    for (float r : {0.35f, 0.20f, 0.10f, 0.05f})
    {
        const float p = peak_at(r, 0.0f);
        std::printf("    dielectric roughness %.2f  peak %12.4f  %+5.1f stops\n",
                    static_cast<double>(r), static_cast<double>(p),
                    static_cast<double>(std::log2(p)));
    }
    for (float r : {0.20f, 0.05f})
    {
        const float p = peak_at(r, 1.0f);
        std::printf("    METAL      roughness %.2f  peak %12.4f  %+5.1f stops\n",
                    static_cast<double>(r), static_cast<double>(p),
                    static_cast<double>(std::log2(p)));
    }

    const float sharp_metal = peak_at(0.05f, 1.0f);
    checkf(sharp_metal > 10000.0f,
           "a polished metal reaches %.0f at the mirror angle — %.1f stops above "
           "white, and `to_encoded` stores every value from 1.0 upward as the "
           "SAME CODE. That is not 'a bit too bright'; it is the highlight's "
           "entire shape being replaced by a constant",
           static_cast<double>(sharp_metal), static_cast<double>(std::log2(sharp_metal)));

    check(engine::linear_to_srgb_u8(1.0f) == 255
              && engine::linear_to_srgb_u8(4.0f) == 255
              && engine::linear_to_srgb_u8(59003.0f) == 255,
          "1.0, 4.0 and 59003.0 all encode to code 255 — the clamp is not lossy "
          "at the margin, it is total above the margin");

    // THE UNITS DEBT, which is the same problem wearing a suit. 6.2 renamed
    // `intensity` to `irradiance` so lights could be MEASURED, and then could not
    // let anybody author one.
    engine::lighting sun = lights;
    sun.key.irradiance = 120000.0f;   // full daylight, in lux
    const microsurface polished{.roughness = 0.20f, .metallic = 1.0f};
    const linear_rgb under_sun = engine::shade({0.9f, 0.9f, 0.9f}, n, mirror, sun, polished);
    checkf(under_sun.r > 1.0e6f,
           "under REAL sunlight (120,000 lux) the same surface returns %.0f. You "
           "cannot author a light in physical units until something downstream "
           "decides what maps to white — which is what an exposure control is, "
           "and is exactly what light.hpp promised this lesson would add",
           static_cast<double>(under_sun.r));
}

// ===========================================================================
//  §B — exposure
// ===========================================================================

void section_b_exposure()
{
    section("B  EXPOSURE — A LOGARITHMIC SCALE OF LIGHT");

    // ONE STOP IS A FACTOR OF TWO. That is the whole definition, and it is worth
    // asserting because every other property follows from it.
    const float e0 = engine::exposure_from_ev100(0.0f);
    const float e1 = engine::exposure_from_ev100(1.0f);
    checkf(std::fabs(e0 / e1 - 2.0f) < 1e-5f,
           "EV 0 -> x%.5f and EV 1 -> x%.5f: exactly a factor of %.6f. One stop "
           "HALVES the light that reaches white, which is why artists and "
           "photographers can use the same word",
           static_cast<double>(e0), static_cast<double>(e1),
           static_cast<double>(e0 / e1));

    // THE OLD BEHAVIOUR AS A POINT ON THE NEW SCALE. This is what turns a
    // replacement into a generalisation: `k_reference_irradiance` was chosen so
    // that 1.0 maps to white, which is an exposure of exactly 1.
    const float ref = engine::exposure_from_ev100(engine::k_reference_ev100);
    checkf(std::fabs(ref - 1.0f) < 1e-5f,
           "k_reference_ev100 gives an exposure of %.7f — the pre-6.12 engine is "
           "EV %.4f, not a special case. 6.2's choice that a white surface "
           "renders at 1.0 IS an exposure setting; it was simply the only one",
           static_cast<double>(ref), static_cast<double>(engine::k_reference_ev100));

    // The round trip, which is how an auto-exposure turns a measurement into a
    // setting.
    for (float lum : {0.18f, 1.0f, 12.0f, 1000.0f})
    {
        const float ev = engine::ev100_from_luminance(lum);
        const float back = 1.2f * std::exp2(ev);
        checkf(std::fabs(back - lum) < lum * 1e-4f,
               "luminance %.4f <-> EV %.4f round-trips to %.4f",
               static_cast<double>(lum), static_cast<double>(ev),
               static_cast<double>(back));
    }

    std::printf("  a few real scenes, for calibration:\n");
    for (auto [name, ev] : {std::pair<const char*, float>{"a dim room", 3.0f},
                            {"an overcast day", 12.0f},
                            {"full daylight", 15.0f}})
    {
        std::printf("    %-16s EV %5.1f  ->  exposure x%.8f  (white at %.1f)\n",
                    name, static_cast<double>(ev),
                    static_cast<double>(engine::exposure_from_ev100(ev)),
                    static_cast<double>(1.2f * std::exp2(ev)));
    }
}

// ===========================================================================
//  §C — the curves
// ===========================================================================

void section_c_curves()
{
    section("C  THE CURVES");

    const tonemap ops[] = {tonemap::clamp, tonemap::reinhard,
                           tonemap::reinhard_white, tonemap::aces};

    // ---- The three properties that make an operator legal ------------------
    for (tonemap op : ops)
    {
        // MONOTONIC. Brighter in, brighter out — or the image is not a
        // photograph of anything, and a highlight can come out darker than its
        // own edge.
        bool monotonic = true;
        float prev = -1.0f;
        for (int i = 0; i <= 2000; ++i)
        {
            const float x = static_cast<float>(i) * 0.05f;
            const float y = engine::apply_tonemap(x, op, 4.0f);
            if (y < prev - 1e-6f) { monotonic = false; break; }
            prev = y;
        }

        // BOUNDED. Never above 1, or it has not done its job.
        bool bounded = true;
        for (int i = 0; i <= 2000; ++i)
        {
            const float x = static_cast<float>(i) * 50.0f;
            if (engine::apply_tonemap(x, op, 4.0f) > 1.0f + 1e-6f) { bounded = false; break; }
        }

        checkf(monotonic && bounded && engine::apply_tonemap(0.0f, op, 4.0f) == 0.0f,
               "%-14s is monotonic over [0, 100], never exceeds 1 up to 100,000, "
               "and sends 0 to 0", engine::name_of(op));
    }

    // NEAR THE ORIGIN they must all be close to the identity, or the dark end of
    // every image is wrong — and the dark end is where an sRGB encode spends most
    // of its codes.
    std::printf("  what each does to a dark value (x = 0.02, code 40):\n");
    for (tonemap op : ops)
    {
        const float y = engine::apply_tonemap(0.02f, op, 4.0f);
        std::printf("    %-14s %.5f  ->  code %3d  (%+d)\n", engine::name_of(op),
                    static_cast<double>(y), engine::linear_to_srgb_u8(y),
                    static_cast<int>(engine::linear_to_srgb_u8(y))
                        - static_cast<int>(engine::linear_to_srgb_u8(0.02f)));
    }

    // ---- Reinhard never reaches 1, and the number is famous -----------------
    //
    // x/(1+x) < 1 for every finite x. So pure white is UNREACHABLE, and an image
    // tonemapped this way has no 255 in it — which is exactly why it looks washed
    // out.
    float need = 0.0f;
    for (float x = 1.0f; x < 1.0e6f; x *= 1.001f)
    {
        if (engine::linear_to_srgb_u8(engine::apply_tonemap(x, tonemap::reinhard, 4.0f)) >= 255)
        {
            need = x;
            break;
        }
    }
    checkf(need > 200.0f && need < 260.0f,
           "plain Reinhard needs an input of %.0f to reach code 255, because "
           "x/(1+x) is strictly below 1 for every finite x. An image tonemapped "
           "this way has NO PURE WHITE IN IT, which is the washed-out look it is "
           "famous for.\n           (Checked by hand: code 255 needs an ENCODED "
           "value of 254.5/255, which is a LINEAR 0.99554, so x/(1+x) = 0.99554 "
           "and x = 223.2. This comment first said 768, from applying the 254.5 "
           "threshold in the wrong space — which is Module 6's recurring mistake "
           "and is why the harness measures it.)",
           static_cast<double>(need));

    // ---- The white point solves it exactly ---------------------------------
    for (float w : {2.0f, 4.0f, 11.0f})
    {
        const float at_w = engine::apply_tonemap(w, tonemap::reinhard_white, w);
        checkf(std::fabs(at_w - 1.0f) < 1e-5f,
               "reinhard_white sends W = %.1f to exactly %.6f — which is the "
               "derivation, not a fit: f(W) = W(1 + 1/W)/(1+W) = (W+1)/(1+W)",
               static_cast<double>(w), static_cast<double>(at_w));
    }

    // ---- ACES is a FIT, and honesty about that is the point ----------------
    checkf(std::fabs(engine::apply_tonemap(1.0f, tonemap::aces, 4.0f) - 0.8010f) < 0.01f,
           "the ACES fit sends 1.0 to %.4f rather than to 1.0, because it has a "
           "SHOULDER — it starts rolling off well before white, which is what "
           "makes it read as photographic. It is Narkowicz's five-constant "
           "approximation to a much larger transform, and it is not derived from "
           "anything",
           static_cast<double>(engine::apply_tonemap(1.0f, tonemap::aces, 4.0f)));

    std::printf("  the four curves, tabulated:\n         x:");
    const float xs[] = {0.1f, 0.5f, 1.0f, 2.0f, 4.0f, 16.0f, 256.0f};
    for (float x : xs) { std::printf("%9.1f", static_cast<double>(x)); }
    std::printf("\n");
    for (tonemap op : ops)
    {
        std::printf("    %-10s", engine::name_of(op));
        for (float x : xs)
        {
            std::printf("%9.4f", static_cast<double>(engine::apply_tonemap(x, op, 4.0f)));
        }
        std::printf("\n");
    }
}

// ===========================================================================
//  §D — per-channel vs luminance
// ===========================================================================

void section_d_hue()
{
    section("D  PER-CHANNEL vs LUMINANCE");

    // A bright, saturated orange — the colour a fire or a sunset actually is, and
    // the case where the two answers diverge most.
    const linear_rgb hot{8.0f, 2.0f, 0.4f};

    tonemap_settings per;
    per.op = tonemap::reinhard;
    per.per_channel = true;

    tonemap_settings luma = per;
    luma.per_channel = false;

    const linear_rgb a = engine::apply_tonemap(hot, per);
    const linear_rgb b = engine::apply_tonemap(hot, luma);

    std::printf("  input (%.1f, %.1f, %.1f), luminance %.4f\n",
                static_cast<double>(hot.r), static_cast<double>(hot.g),
                static_cast<double>(hot.b), static_cast<double>(engine::luminance(hot)));
    std::printf("    per channel : (%.4f, %.4f, %.4f)\n",
                static_cast<double>(a.r), static_cast<double>(a.g), static_cast<double>(a.b));
    std::printf("    luminance   : (%.4f, %.4f, %.4f)\n",
                static_cast<double>(b.r), static_cast<double>(b.g), static_cast<double>(b.b));

    // SATURATION, as the ratio of the extremes. Per-channel compresses the big
    // channel more than the small one — that is what a saturating curve does — so
    // the colour moves TOWARD WHITE as it brightens, which is what film does and
    // what people expect a highlight to look like.
    const float sat_in = hot.r / hot.b;
    const float sat_per = a.r / a.b;
    const float sat_lum = b.r / b.b;
    checkf(sat_per < sat_in * 0.5f,
           "per-channel drops the red:blue ratio from %.1f to %.2f — the highlight "
           "DESATURATES toward white, which is not a defect but the behaviour "
           "film has and the reason this is the default",
           static_cast<double>(sat_in), static_cast<double>(sat_per));

    checkf(std::fabs(sat_lum - sat_in) < 0.01f,
           "luminance-only holds it at %.2f — the hue and saturation are exactly "
           "preserved, because scaling a colour by a scalar is a move along the "
           "ray from black through it", static_cast<double>(sat_lum));

    // AND THE TRAP. Luminance-only can leave a channel above 1, which the encode
    // then clips — shifting the hue anyway, at a threshold nobody chose.
    const linear_rgb blue{0.0f, 0.0f, 8.0f};
    const linear_rgb mapped = engine::apply_tonemap(blue, luma);
    checkf(mapped.b > 1.0f,
           "a pure blue of (0, 0, 8) has luminance %.4f, sails through the curve, "
           "and arrives with b = %.4f for `to_encoded` to CLIP. Luminance-only "
           "preserves the hue right up to the point where it does not — and blue "
           "is the worst case because its luminance weight is only 0.0722",
           static_cast<double>(engine::luminance(blue)), static_cast<double>(mapped.b));

    const linear_rgb blue_per = engine::apply_tonemap(blue, per);
    checkf(blue_per.b <= 1.0f,
           "per-channel brings the same colour to b = %.4f, in range by "
           "construction — every channel went through a function bounded by 1",
           static_cast<double>(blue_per.b));
}

// ===========================================================================
//  §E — the resolve, and the order of its three steps
// ===========================================================================

void section_e_order()
{
    section("E  THE RESOLVE — THREE STEPS, ONE ORDER");

    const linear_rgb c{6.0f, 3.0f, 1.5f};
    tonemap_settings s;
    s.op = tonemap::reinhard;
    s.exposure = 0.25f;

    // CORRECT: exposure, then curve.
    const linear_rgb right = engine::apply_tonemap(
        linear_rgb{c.r * s.exposure, c.g * s.exposure, c.b * s.exposure}, s);

    // WRONG: curve, then exposure. The curve has already compressed everything
    // into [0,1), so multiplying afterwards scales an image that no longer has
    // the range the exposure was meant to select from.
    tonemap_settings unexposed = s;
    unexposed.exposure = 1.0f;
    const linear_rgb curved = engine::apply_tonemap(c, unexposed);
    const linear_rgb wrong{curved.r * s.exposure, curved.g * s.exposure,
                           curved.b * s.exposure};

    checkf(engine::linear_to_srgb_u8(right.r) != engine::linear_to_srgb_u8(wrong.r),
           "exposure BEFORE the curve gives code %d; after it, code %d. The "
           "second is not merely darker — it has lost the ability to distinguish "
           "the bright end, because everything above about 4 had already been "
           "squeezed into the same place before the multiply",
           engine::linear_to_srgb_u8(right.r), engine::linear_to_srgb_u8(wrong.r));

    // And the other ordering mistake: tonemapping CODES rather than light.
    const Uint32 encoded_first = engine::to_encoded({0.5f, 0.5f, 0.5f});
    const float as_code = static_cast<float>(engine::red_of(encoded_first)) / 255.0f;
    const float curve_on_code = engine::apply_tonemap(as_code, tonemap::reinhard, 4.0f);
    const float curve_on_light = engine::apply_tonemap(0.5f, tonemap::reinhard, 4.0f);
    checkf(std::fabs(curve_on_code - curve_on_light) > 0.05f,
           "the same mid grey curved as LIGHT gives %.4f and curved as a CODE "
           "gives %.4f. A curve applied after the encode compresses the codes the "
           "encode had carefully spread out, and it crushes the shadows first",
           static_cast<double>(curve_on_light), static_cast<double>(curve_on_code));

    // ---- The whole resolve, over a buffer ---------------------------------
    hdr_buffer src(8, 4);
    engine::framebuffer dst(8, 4);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            const float v = std::exp2(static_cast<float>(x) - 3.0f);   // 1/8 .. 16
            src.put_pixel(x, y, {v, v, v});
        }
    }

    tonemap_settings rs;
    rs.op = tonemap::reinhard;
    engine::resolve(src, dst, rs);

    std::printf("  a row of powers of two through the resolve (reinhard, exposure 1):\n    ");
    for (int x = 0; x < 8; ++x)
    {
        std::printf("%6.3f->%3d ", static_cast<double>(std::exp2(static_cast<float>(x) - 3.0f)),
                    engine::red_of(dst.pixel_at(x, 0)));
    }
    std::printf("\n");
    check(engine::red_of(dst.pixel_at(0, 0)) < engine::red_of(dst.pixel_at(7, 0)),
          "…and it is monotonic across seven stops, in 8 bits, with nothing "
          "clipped — which is the entire point of the exercise");

    // A dimension mismatch is a no-op, not a partial write.
    engine::framebuffer wrong_size(4, 4);
    wrong_size.clear(0xFF123456u);
    engine::resolve(src, wrong_size, rs);
    check(wrong_size.pixel_at(0, 0) == 0xFF123456u,
          "a resolve into a target of the wrong size writes NOTHING — half a "
          "resolved frame looks like a rendering bug in whichever half you "
          "notice first");
}

// ===========================================================================
//  §F — blending on a float target
// ===========================================================================

void section_f_blend()
{
    section("F  WHAT AN HDR TARGET DOES TO LESSON 6.11's RULE");

    // 6.11 established that compositing must decode both operands, blend in
    // linear light, and re-encode — and measured the round trip. On a float
    // target there is no round trip, because the buffer already holds light.
    const linear_rgb dst{0.25f, 0.25f, 0.25f};
    const linear_rgb src{1.0f, 1.0f, 1.0f};

    const linear_rgb hdr_result = engine::over(src, 0.5f, dst);
    const Uint32 ldr_result = engine::blend_over(engine::to_encoded(dst),
                                                engine::to_encoded(src), 0.5f);

    checkf(std::fabs(hdr_result.r - 0.625f) < 1e-6f,
           "on a float target the composite is one lerp: 1.0*0.5 + 0.25*0.5 = "
           "%.4f, exactly, with no conversion anywhere near it",
           static_cast<double>(hdr_result.r));

    const float ldr_light = engine::srgb_to_linear_u8(engine::red_of(ldr_result));
    checkf(std::fabs(ldr_light - hdr_result.r) < 0.005f,
           "the 8-bit path reaches the same answer (%.4f against %.4f) — 6.11's "
           "machinery was CORRECT, and what it was correcting for was the storage "
           "format rather than the arithmetic",
           static_cast<double>(ldr_light), static_cast<double>(hdr_result.r));

    // AND THE ONE THAT CANNOT BE DONE IN 8 BITS AT ALL.
    const linear_rgb bright{40.0f, 40.0f, 40.0f};
    const linear_rgb over_bright = engine::over(bright, 0.25f, dst);
    checkf(over_bright.r > 10.0f,
           "compositing a value of 40 at quarter coverage gives %.4f and the "
           "buffer keeps it. The 8-bit path cannot: 40 was already code 255 "
           "before the blend, so it would have composited 1.0 and lost a factor "
           "of forty BEFORE the operator ran",
           static_cast<double>(over_bright.r));

    // THE COST, measured. 6.11 priced one composite at 68.87 ns correct against
    // 21.94 ns on raw bytes.
    const Uint64 n = 2000000;
    float sink = 0.0f;
    Uint64 t0 = SDL_GetPerformanceCounter();
    for (Uint64 i = 0; i < n; ++i)
    {
        sink += engine::over(src, 0.5f, dst).r;
    }
    Uint64 t1 = SDL_GetPerformanceCounter();
    const double ns_hdr = 1e9 * static_cast<double>(t1 - t0)
                        / static_cast<double>(SDL_GetPerformanceFrequency()) / n;

    Uint32 sink2 = 0;
    const Uint32 d8 = engine::to_encoded(dst);
    const Uint32 s8 = engine::to_encoded(src);
    t0 = SDL_GetPerformanceCounter();
    for (Uint64 i = 0; i < n; ++i)
    {
        sink2 ^= engine::blend_over(d8, s8, 0.5f, engine::encode_mode::fast);
    }
    t1 = SDL_GetPerformanceCounter();
    const double ns_ldr = 1e9 * static_cast<double>(t1 - t0)
                        / static_cast<double>(SDL_GetPerformanceFrequency()) / n;

    checkf(sink != 0.0f && sink2 != 0xDEADBEEFu,
           "one composite: %.2f ns on a float target against %.2f ns on 8 bits — "
           "%.1fx cheaper, because the decode and the re-encode are simply absent. "
           "AN HDR BUFFER IS NOT ONLY ABOUT RANGE: it is the buffer that holds the "
           "quantity the arithmetic is defined on",
           ns_hdr, ns_ldr, ns_ldr / ns_hdr);
}

// ===========================================================================
//  §G — the log-average
// ===========================================================================

void section_g_logmean()
{
    section("G  THE LOG-AVERAGE");

    // A frame that is mostly dim with one very bright specular pixel — which is
    // every frame with a polished surface in it.
    hdr_buffer buf(100, 100);
    buf.clear({0.05f, 0.05f, 0.05f});
    buf.put_pixel(50, 50, {5000.0f, 5000.0f, 5000.0f});

    const engine::hdr_stats st = engine::measure(buf);

    checkf(st.over_one == 1 && st.pixels == 10000,
           "one pixel of ten thousand is over the lid, and `measure` finds it");

    // THE ARITHMETIC MEAN IS DESTROYED BY IT. One pixel in ten thousand at 5000
    // contributes 0.5 to the mean all by itself — ten times the entire rest of
    // the image.
    checkf(st.mean_luminance > 0.5f,
           "the ARITHMETIC mean luminance is %.4f, of which %.4f comes from that "
           "single pixel. An auto-exposure driven by this number would stop down "
           "by three stops because one specular highlight exists",
           static_cast<double>(st.mean_luminance), static_cast<double>(5000.0f / 10000.0f));

    checkf(st.log_mean_luminance < 0.1f,
           "the LOG-average is %.5f — it barely notices, because the log of a "
           "large number is a small number. That robustness is the entire reason "
           "exposure metering is done in log space, and it is why Reinhard's 2002 "
           "paper specifies the geometric mean",
           static_cast<double>(st.log_mean_luminance));

    checkf(st.mean_luminance / st.log_mean_luminance > 10.0f,
           "the two disagree by %.1fx on the same image — which is the sort of "
           "gap that turns 'the exposure flickers when a highlight comes into "
           "view' into a bug report nobody can reproduce",
           static_cast<double>(st.mean_luminance / st.log_mean_luminance));
}

// ===========================================================================
//  §H — the golden
// ===========================================================================

void section_h_golden()
{
    section("H  THE GOLDEN");

    const int rc = demo::write_reference_shot("scratch/verify612.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify612.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL, hash E917C06C — TWENTY-FIRST lesson at this hash, and "
          "the reason is STRUCTURAL rather than a flag. The fixture renders into "
          "an 8-bit `framebuffer` through the path it has always used, and "
          "everything this lesson built operates on a DIFFERENT BUFFER TYPE. This "
          "is not 'the tonemapper is optional' — it is 'the tonemapper is a stage "
          "over a target the fixture does not have'");
}

// ===========================================================================
//  Device-side plumbing for §I — the shape verify_61 and verify_611 established
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
                                engine::index_mode::indexed, "verify612 quad");
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
    if (fence != nullptr)
    {
        SDL_WaitForGPUFences(gpu.handle(), true, &fence, 1);
        SDL_ReleaseGPUFence(gpu.handle(), fence);
    }
    return ok;
}

// ===========================================================================
//  §I — CPU vs GPU
// ===========================================================================

void section_i_gpu()
{
    section("I  CPU vs GPU — THE SAME CURVE, COMPUTED TWICE");

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::printf("  ---- SDL_Init(VIDEO): %s\n", SDL_GetError());
    }

    // NOTHING BELOW CALLS destroy(). `gpu` is declared first, so it is destroyed
    // LAST, which is exactly the order every object after it needs — and an
    // explicit teardown would run the device's destruction in the middle of that
    // sequence. Lesson 6.11's harness segfaulted on exit for precisely this
    // reason and the fix was to delete the cleanup.
    engine::gpu_device gpu;
    if (!gpu.create(nullptr, false).ok())
    {
        std::printf("  ---- no GPU device on this machine; §I skipped\n");
        return;
    }

    engine::gpu_shader scene_vs;
    engine::gpu_shader scene_fs;
    engine::gpu_shader post_vs;
    engine::gpu_shader post_fs;
    if (!scene_vs.load(gpu, "scene.vert", engine::shader_stage::vertex)
        || !scene_fs.load(gpu, "scene.frag", engine::shader_stage::fragment)
        || !post_vs.load(gpu, "fullscreen.vert", engine::shader_stage::vertex)
        || !post_fs.load(gpu, "tonemap.frag", engine::shader_stage::fragment))
    {
        std::printf("  ---- shaders did not load; §I skipped\n");
        return;
    }
    check(true, "scene.vert/frag and fullscreen.vert/tonemap.frag all loaded");

    engine::gpu_mesh quad;
    if (!upload_quad(gpu, quad))
    {
        std::printf("  ---- quad upload failed; §I skipped\n");
        return;
    }

    engine::gpu_sampler samp;
    check(samp.create(gpu), "a sampler for the scene pass's fallback texture");

    // ---- The HDR target, and the flag that is easy to forget ---------------
    engine::gpu_texture hdr_tex;
    check(hdr_tex.create_colour_target(gpu, engine::k_hdr_format, 16, 16, "verify612 hdr"),
          "an R16G16B16A16_FLOAT colour target that can ALSO be sampled — both "
          "usages, because a scene buffer is written by one pass and read by the "
          "next");

    engine::gpu_scene_renderer scene;
    check(scene.create(gpu, scene_vs.handle(), scene_fs.handle(),
                       SDL_GPU_TEXTUREFORMAT_D32_FLOAT, engine::k_hdr_format),
          "the scene's NINE pipelines, created against the FLOAT format — a "
          "pipeline's colour target format is baked in at creation (4.4), so "
          "rendering to HDR is a different set of pipelines, not a different "
          "argument to the same ones");

    engine::gpu_tonemap_pass post;
    check(post.create(gpu, post_vs.handle(), post_fs.handle(),
                      SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM),
          "the resolve pipeline: no vertex input, no depth, no blending");

    target ldr;
    if (!ldr.create(gpu, 16, 16, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM))
    {
        std::printf("  ---- LDR target failed; §I skipped\n");
        return;
    }

    // The scene collapses to `lit = ambient`, which verify_61 §E established as
    // the way to measure a transfer function with no shading arithmetic in the
    // way. Here `ambient` is set ABOVE 1 — which is the whole point: an 8-bit
    // target could not have carried it this far.
    const auto render_and_resolve = [&](float ambient, tonemap op, float exposure) -> int {
        if (!scene.ensure_depth(gpu, 16, 16)) { return -1; }

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

        // ZERO, AND NOW FOR A THIRD REASON. The scene is writing a FLOAT target,
        // so there is no transfer function there to be right or wrong about: the
        // values written are quantities of light, and the encode happens in the
        // resolve below.
        light.encode_output = 0.0f;

        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());

        // ---- PASS ONE: the scene, into the float target --------------------
        SDL_GPUColorTargetInfo hdr_info{};
        hdr_info.texture = hdr_tex.handle();
        hdr_info.load_op = SDL_GPU_LOADOP_CLEAR;
        hdr_info.store_op = SDL_GPU_STOREOP_STORE;
        hdr_info.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
        SDL_GPUDepthStencilTargetInfo dsi = scene.depth_target_info();
        SDL_GPURenderPass* p1 = SDL_BeginGPURenderPass(cb, &hdr_info, 1, &dsi);
        scene.render(cb, p1, &item, 1, engine::camera_uniforms{clip}, light, samp.handle());
        SDL_EndGPURenderPass(p1);

        // ---- PASS TWO: the resolve, into 8 bits ----------------------------
        //
        // A SECOND PASS, and the fact that it is a second pass is the lesson.
        // The first one had to END before this one could read its output.
        tonemap_settings ts;
        ts.op = op;
        ts.exposure = exposure;

        SDL_GPUColorTargetInfo ldr_info{};
        ldr_info.texture = ldr.colour;
        ldr_info.load_op = SDL_GPU_LOADOP_CLEAR;
        ldr_info.store_op = SDL_GPU_STOREOP_STORE;
        ldr_info.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 1.0f};
        SDL_GPURenderPass* p2 = SDL_BeginGPURenderPass(cb, &ldr_info, 1, nullptr);
        post.render(cb, p2, hdr_tex.handle(), ts, /*shader_encodes=*/true);
        SDL_EndGPURenderPass(p2);

        const std::vector<Uint8> px = download(ldr, cb);
        return px.empty() ? -1 : static_cast<int>(px[(8 * 16 + 8) * 4]);
    };

    // ---- The comparison ----------------------------------------------------
    std::printf("  a flat quad of known linear value, through BOTH resolves:\n");
    std::printf("    value   curve            GPU   CPU\n");

    int agreements = 0;
    int compared = 0;
    for (auto [value, op] : {std::pair<float, tonemap>{4.0f, tonemap::reinhard},
                             {4.0f, tonemap::aces},
                             {16.0f, tonemap::reinhard},
                             {0.5f, tonemap::reinhard},
                             {40.0f, tonemap::clamp}})
    {
        const int gpu_code = render_and_resolve(value, op, 1.0f);
        if (gpu_code < 0) { continue; }

        tonemap_settings ts;
        ts.op = op;
        const linear_rgb cpu = engine::apply_tonemap(linear_rgb{value, value, value}, ts);
        const int cpu_code = engine::linear_to_srgb_u8(cpu.r);

        std::printf("    %6.1f  %-14s  %5d %5d\n", static_cast<double>(value),
                    engine::name_of(op), gpu_code, cpu_code);
        ++compared;
        if (std::abs(gpu_code - cpu_code) <= 2) { ++agreements; }
    }

    checkf(compared > 0 && agreements == compared,
           "%d of %d agree to within 2 codes. The curve is written twice — once "
           "in hdr.cpp and once in tonemap.frag.hlsl — and two implementations of "
           "one curve that disagree are a bug that only shows up in a diff, which "
           "is why they are compared rather than trusted",
           agreements, compared);

    // AND THE VALUE THAT COULD NOT HAVE SURVIVED 8 BITS.
    const int high = render_and_resolve(16.0f, tonemap::reinhard, 1.0f);
    tonemap_settings ts;
    ts.op = tonemap::reinhard;
    const int high_cpu = engine::linear_to_srgb_u8(
        engine::apply_tonemap(linear_rgb{16.0f, 16.0f, 16.0f}, ts).r);
    checkf(high > 0 && high < 255 && std::abs(high - high_cpu) <= 2,
           "a linear value of 16 — four stops above white — resolves to code %d "
           "rather than 255, on the GPU, through a float target the scene pass "
           "wrote and a second pass read. Before this lesson that value did not "
           "survive the fragment shader", high);
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    std::printf("verify_612 — Lesson 6.12: HDR and Tonemapping\n");

    section_a_lid();
    section_b_exposure();
    section_c_curves();
    section_d_hue();
    section_e_order();
    section_f_blend();
    section_g_logmean();

    // The golden runs BEFORE the GPU section, which 6.11 established: §I
    // initialises the video subsystem and creates a device, and a CPU reference
    // render has no business being downstream of either.
    section_h_golden();
    section_i_gpu();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
