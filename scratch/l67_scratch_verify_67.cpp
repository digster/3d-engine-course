// scratch/verify_67.cpp — Lesson 6.7's harness: the frame, and what it holds.
//
//   §A  A TEXTURE THAT KNOWS WHAT IT HOLDS — the colour space, measured
//   §B  THE TBN DERIVATION — solved on a triangle whose answer is known by hand
//   §C  handedness: a mirrored uv chart, and the sign that records it
//   §D  THE ROUND TRIP — a flat map must change nothing, exactly
//   §E  the analytic bump map, against its own closed form
//   §F  a tangent is carried by the MODEL matrix, not the normal matrix
//   §G  THE GOLDEN — a new capability must not move the old picture
//
// §D IS THE ONE THAT MATTERS, and it is the cheapest test in the file. A normal
// map whose every texel is (0.5, 0.5, 1.0) encodes the direction (0, 0, 1) —
// "no change" — so `T*0 + B*0 + N*1` must return the geometric normal EXACTLY.
// Every mistake this lesson can make breaks it: a wrong colour space, a wrong
// decode range, a frame that is not orthonormal, a bitangent with the wrong
// sign, a basis change written in the wrong order. One assertion, six bugs.
//
// Build and run:  sh scratch/build_verify_67.sh

#include <engine/core/assert.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/gpu_uniform.hpp>
#include <engine/gfx/material.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/texture.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
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
    char line[900];
    va_list args;
    va_start(args, fmt);
    SDL_vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    check(ok, line);
}

[[nodiscard]] std::string read_file(const char* path)
{
    std::size_t size = 0;
    void* data = SDL_LoadFile(path, &size);
    if (data == nullptr) { return {}; }
    std::string out(static_cast<const char*>(data), size);
    SDL_free(data);
    return out;
}

/// The engine's own fragment-side frame construction, extracted so the test
/// drives exactly the arithmetic `raster.cpp` runs rather than a paraphrase.
///
/// It is a duplicate, and that is a real cost worth naming: if the fragment
/// changes and this does not, the test keeps passing while the renderer is
/// wrong. The honest fix is to lift the frame into a header both call — which is
/// Exercise 2, and is deliberately not done here so the lesson can show the
/// duplication and then price it.
[[nodiscard]] engine::vec3 perturb(engine::vec3 n, engine::vec4 tangent,
                                   engine::linear_rgb sampled)
{
    const engine::vec3 nn = engine::normalised_or(n, engine::vec3{0.0f, 0.0f, 1.0f});
    const engine::vec3 ti{tangent.x, tangent.y, tangent.z};
    const engine::vec3 tt =
        engine::normalised_or(ti - nn * engine::dot(nn, ti), engine::vec3{1.0f, 0.0f, 0.0f});
    const engine::vec3 bb = engine::cross(nn, tt) * ((tangent.w >= 0.0f) ? 1.0f : -1.0f);

    const engine::vec3 tn{sampled.r * 2.0f - 1.0f,
                          sampled.g * 2.0f - 1.0f,
                          sampled.b * 2.0f - 1.0f};
    return tt * tn.x + bb * tn.y + nn * tn.z;
}

// ===========================================================================
//  §A — A TEXTURE THAT KNOWS WHAT IT HOLDS
// ===========================================================================

void section_a_colour_space()
{
    std::printf("\n=== A. Colour or data? The field Lesson 6.6 had to defer ===\n");

    // The whole subject in one number. 128 is the byte a flat normal map stores
    // for x and y — "no tilt" — and it means 0.5020 as DATA and 0.2140 as a
    // COLOUR. Through the wrong space every surface tilts, in the same direction,
    // everywhere, by an amount that looks like an over-strong normal map.
    const float as_data = 128.0f / 255.0f;
    const float as_colour = engine::srgb_to_linear(as_data);

    // The tilt is DERIVED here rather than quoted, which is the difference
    // between a test and a note: decode 128 through the wrong space, map it from
    // [0,1] to [-1,1], and measure the angle from straight up.
    const float wrong_xy = as_colour * 2.0f - 1.0f;
    const float wrong_tilt =
        std::atan(std::sqrt(2.0f) * std::fabs(wrong_xy)) * 180.0f / 3.14159265f;

    checkf(as_data > 0.5f && as_colour < 0.25f && wrong_tilt > 30.0f,
           "the byte 128 means %.5f as DATA and %.5f as a COLOUR. That is the "
           "whole lesson in one number: read through the sRGB curve the flat "
           "direction becomes (%.3f, %.3f, 1), a surface tilted %.1f degrees — "
           "everywhere, in one direction, on every normal-mapped object",
           static_cast<double>(as_data), static_cast<double>(as_colour),
           static_cast<double>(wrong_xy), static_cast<double>(wrong_xy),
           static_cast<double>(wrong_tilt));

    // Same texels, two spaces, two answers — which is what makes the space part
    // of the asset's IDENTITY rather than a rendering option (§A of 5.5's rule).
    engine::texture as_srgb(4, 4, 0xFF8080FFu, engine::texel_space::srgb);
    engine::texture as_linear(4, 4, 0xFF8080FFu, engine::texel_space::linear);

    const engine::sampler samp{.texel_filter = engine::filter::nearest};
    const engine::linear_rgb s = engine::sample(as_srgb, samp, 0.5f, 0.5f);
    const engine::linear_rgb d = engine::sample(as_linear, samp, 0.5f, 0.5f);

    checkf(std::fabs(d.r - as_data) < 1e-4f && std::fabs(s.r - as_colour) < 1e-4f,
           "the SAME texel (0x8080FF) samples as %.4f through texel_space::linear "
           "and %.4f through ::srgb. One branch, in `fetch` — the one place every "
           "read already goes through, which is why no caller can forget to ask",
           static_cast<double>(d.r), static_cast<double>(s.r));

    checkf(as_srgb.space() == engine::texel_space::srgb
               && as_linear.space() == engine::texel_space::linear,
           "…and the space is a property of the TEXTURE, set at construction. "
           "That is where SDL_GPU puts it too: the decode is declared by the "
           "FORMAT (_UNORM against _UNORM_SRGB) and performed by the sampler, so "
           "one image cannot be sRGB in one binding and linear in another");

    checkf(engine::texture(2, 2).space() == engine::texel_space::srgb,
           "the default is sRGB, which keeps every texture written before this "
           "lesson meaning exactly what it meant. A default that changed the "
           "answer would have made this lesson a re-baseline");

    // ---- The space is part of the cache key --------------------------------
    engine::search_path paths;
    paths.add_root("assets");
    engine::asset_store store(paths);

    const engine::texture_load colour = store.load_texture("uv_grid.png");
    const engine::texture_load data =
        store.load_texture("uv_grid.png", engine::texel_space::linear);

    checkf(colour.ok() && data.ok() && colour.handle != data.handle,
           "the same PNG read as colour and read as data are TWO textures — "
           "`mesh_import`'s rule from 5.5 arriving for a second type. A store "
           "keyed on the filename alone would hand the second requester the "
           "first one's decode and be quietly wrong");
    checkf(colour.source == data.source && store.counters().files_read == 1,
           "…but ONE image behind them, decoded once (files_read = %d). The bytes "
           "are the same and only the reading differs, which is exactly what the "
           "two-level cache is for",
           store.counters().files_read);
    checkf(store.find_texture("uv_grid.png").valid()
               && store.find_texture("uv_grid.png", engine::texel_space::linear).valid(),
           "both are findable, under keys 'uv_grid.png' and 'uv_grid.png|linear' "
           "— 5.5's rule that THE DEFAULT SERIALISES TO NOTHING, so a generated "
           "texture inserted under a bare name still collides correctly with a "
           "loaded one requested with the default space");
}

// ===========================================================================
//  §B — THE DERIVATION
// ===========================================================================
//
// A triangle whose tangent frame can be worked out on paper in four lines, so
// the test is checking the ENGINE against arithmetic rather than against itself.
//
//   p0 = (0, 0,  0)   uv0 = (0, 0)
//   p1 = (2, 0,  0)   uv1 = (1, 0)
//   p2 = (0, 0, -2)   uv2 = (0, 1)
//
// It lies in the xz plane, so N = +y. e1 = (2,0,0), e2 = (0,0,-2); the uv deltas
// are (1,0) and (0,1), so the 2x2 is the identity and det = 1. Then
//
//   T = (dv2*e1 - dv1*e2) / det = (1*(2,0,0) - 0*(0,0,-2)) = (2,0,0)
//   B = (du1*e2 - du2*e1) / det = (1*(0,0,-2) - 0*(2,0,0)) = (0,0,-2)
//
// so T normalises to (1,0,0) and B to (0,0,-1). And cross(N,T) = (0,0,-1), which
// agrees with B, so the handedness is +1.

void section_b_derivation()
{
    std::printf("\n=== B. The derivation, against a triangle solved by hand ===\n");

    engine::mesh_data m;
    m.vertices = {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -2.0f}};
    m.uvs = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
    m.normals = {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    m.indices = {0, 1, 2};

    const engine::mesh_data framed = engine::with_tangents(m.view());

    checkf(framed.tangents.size() == 3,
           "three vertices in, three tangents out — %zu. The vertex count is "
           "UNCHANGED, unlike flat normals: a vertex already carries one uv, so "
           "it already has one frame, and wherever an artist needed two the uv "
           "seam had already forced two vertices",
           framed.tangents.size());
    if (framed.tangents.size() != 3) { return; }

    const engine::vec4 t = framed.tangents[0];
    checkf(std::fabs(t.x - 1.0f) < 1e-6f && std::fabs(t.y) < 1e-6f
               && std::fabs(t.z) < 1e-6f,
           "T = (%.4f, %.4f, %.4f) — the +u direction in world units, and the "
           "hand-solved answer is (1, 0, 0). The uv delta matrix here is the "
           "IDENTITY, so T is simply e1 normalised; a chart that stretched u "
           "would scale it and the normalisation is what removes that",
           static_cast<double>(t.x), static_cast<double>(t.y),
           static_cast<double>(t.z));

    checkf(std::fabs(t.w - 1.0f) < 1e-6f,
           "…and the handedness is %+.0f, because cross(N, T) = (0, 0, -1) agrees "
           "with the B the solve produced",
           static_cast<double>(t.w));

    // The bitangent is not stored, so this is the test that it is RECOVERABLE —
    // which is the whole argument for spending four bytes on a sign instead of
    // twelve on a vector.
    const engine::vec3 n{0.0f, 1.0f, 0.0f};
    const engine::vec3 tt{t.x, t.y, t.z};
    const engine::vec3 b = engine::cross(n, tt) * t.w;
    checkf(std::fabs(b.z + 1.0f) < 1e-6f && std::fabs(b.x) < 1e-6f
               && std::fabs(b.y) < 1e-6f,
           "B recovered as w*cross(N,T) = (%.4f, %.4f, %.4f), matching the solve's "
           "(0, 0, -1). Storing it instead would cost 12 bytes a vertex to save "
           "one cross product a fragment — and could DISAGREE with N and T, which "
           "a recomputed value cannot",
           static_cast<double>(b.x), static_cast<double>(b.y),
           static_cast<double>(b.z));

    // Orthonormality, which is what makes the basis change a rotation rather
    // than a general (and lossy) linear map.
    checkf(std::fabs(engine::dot(n, tt)) < 1e-6f
               && std::fabs(engine::dot(n, b)) < 1e-6f
               && std::fabs(engine::dot(tt, b)) < 1e-6f
               && std::fabs(engine::length(tt) - 1.0f) < 1e-6f,
           "the frame is ORTHONORMAL: all three pairwise dots below 1e-6 and |T| "
           "= 1. That is what makes the tangent-to-world step a rotation, so a "
           "unit normal in the map stays a unit normal in the world");

    // ---- Degenerate uv chart -----------------------------------------------
    //
    // Three corners on one line of the chart: det = 0, no frame exists, and the
    // face must contribute NOTHING rather than an infinity.
    engine::mesh_data flat_chart = m;
    flat_chart.uvs = {{0.0f, 0.0f}, {1.0f, 0.0f}, {2.0f, 0.0f}};
    const engine::mesh_data degen = engine::with_tangents(flat_chart.view());
    bool finite = true;
    for (const engine::vec4 v : degen.tangents)
    {
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
        {
            finite = false;
        }
    }
    checkf(finite,
           "a triangle with ZERO uv area produces finite tangents, not NaNs. "
           "det = 0 is a real case — an untextured face, a collapsed unwrap — so "
           "the face contributes nothing and the vertices fall back to an "
           "arbitrary but VALID frame. A NaN here would spread through every "
           "pixel the vertex touches and explain nothing");

    // ---- No uvs, no frame ---------------------------------------------------
    engine::mesh_data no_uvs = m;
    no_uvs.uvs.clear();
    checkf(engine::with_tangents(no_uvs.view()).tangents.empty(),
           "a mesh with no uvs gets NO tangents, and that is a definition rather "
           "than a failure: tangent space IS the uv parameterisation, so a mesh "
           "without one has no +u direction for a map to tilt toward");
}

// ===========================================================================
//  §C — HANDEDNESS
// ===========================================================================

void section_c_handedness()
{
    std::printf("\n=== C. The mirrored chart, and the sign that records it ===\n");

    // The same triangle as §B with `u` NEGATED — which is what an artist does to
    // half of every symmetric model: unwrap one side and reflect it, so both
    // halves share one region of the texture.
    //
    // By hand: det = (-1)(1) - (0)(0) = -1, so r = -1.
    //   T = r*(dv2*e1 - dv1*e2) = -(2,0,0) = (-2,0,0) -> (-1,0,0)
    //   B = r*(du1*e2 - du2*e1) = -((-1)*(0,0,-2)) = (0,0,-2) -> (0,0,-1)
    // and cross(N,T) = cross((0,1,0), (-1,0,0)) = (0,0,+1), which DISAGREES with
    // B — so the handedness is -1.
    engine::mesh_data m;
    m.vertices = {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -2.0f}};
    m.uvs = {{0.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}};
    m.normals = {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    m.indices = {0, 1, 2};

    const engine::mesh_data framed = engine::with_tangents(m.view());
    if (framed.tangents.empty()) { check(false, "no tangents"); return; }

    const engine::vec4 t = framed.tangents[0];
    checkf(std::fabs(t.x + 1.0f) < 1e-6f,
           "T = (%.4f, %.4f, %.4f) — the +u direction now points along -x, "
           "because that is where u increases on a mirrored chart",
           static_cast<double>(t.x), static_cast<double>(t.y),
           static_cast<double>(t.z));
    checkf(t.w < 0.0f,
           "…and the handedness is %+.0f. THIS IS WHY A TANGENT IS A vec4. Drop "
           "the sign and read the frame as right-handed everywhere, and one half "
           "of every symmetric model is lit as the mirror image of the other — "
           "which reads as a modelling error, not a shading one",
           static_cast<double>(t.w));

    // And the consequence, measured: the same map through the two handednesses
    // gives normals that differ, and differ in the BITANGENT direction only.
    const engine::vec3 n{0.0f, 1.0f, 0.0f};
    const engine::linear_rgb tilt{0.5f, 1.0f, 0.5f};   // strongly tilted along +v
    const engine::vec3 right = perturb(n, engine::vec4{1.0f, 0.0f, 0.0f, 1.0f}, tilt);
    const engine::vec3 left = perturb(n, engine::vec4{1.0f, 0.0f, 0.0f, -1.0f}, tilt);
    checkf(std::fabs(right.z + left.z) < 1e-6f && std::fabs(right.x - left.x) < 1e-6f,
           "the two handednesses give normals whose bitangent components are "
           "exact negatives ((%.3f, %.3f, %.3f) against (%.3f, %.3f, %.3f)) and "
           "whose tangent components agree — so the error is a REFLECTION of the "
           "lighting, which is the most plausible-looking wrong answer available",
           static_cast<double>(right.x), static_cast<double>(right.y),
           static_cast<double>(right.z), static_cast<double>(left.x),
           static_cast<double>(left.y), static_cast<double>(left.z));
}

// ===========================================================================
//  §D — THE ROUND TRIP
// ===========================================================================

void section_d_round_trip()
{
    std::printf("\n=== D. The round trip: a flat map must change NOTHING ===\n");

    // strength 0 -> every texel is the lavender (128, 128, 255).
    const engine::texture flat = engine::make_normal_bumps(16, 4, 0.0f);
    checkf(flat.texel(0, 0) == 0xFF8080FFu,
           "the flat map's texels are 0x%08X — (128, 128, 255), THE LAVENDER. "
           "That colour is not a convention somebody picked: it is what the "
           "direction (0, 0, 1) becomes when [-1,1] is packed into a byte, and it "
           "is why an unperturbed normal map looks like that in every tool you "
           "will ever open one in",
           flat.texel(0, 0));
    checkf(flat.space() == engine::texel_space::linear,
           "…and it arrives carrying texel_space::linear, set by the function "
           "that made it. An image whose meaning is fixed by its producer should "
           "not leave the caller to declare what it is");

    const engine::sampler samp{.texel_filter = engine::filter::linear};

    // The identity, over a spread of frames — including tilted normals, mirrored
    // handedness and a tangent that is NOT already perpendicular to the normal
    // (which is the state every interpolated frame is actually in).
    struct frame { engine::vec3 n; engine::vec4 t; const char* what; };
    const frame frames[] = {
        {{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, "axis-aligned"},
        {{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, -1.0f}, "mirrored"},
        {engine::normalised(engine::vec3{0.3f, 0.8f, -0.5f}),
         {0.9f, 0.1f, 0.2f, 1.0f}, "oblique, non-perpendicular"},
        {engine::normalised(engine::vec3{-0.6f, 0.2f, 0.77f}),
         {0.2f, 0.9f, -0.3f, -1.0f}, "oblique, mirrored"},
    };

    float worst = 0.0f;
    const char* worst_case = "";
    for (const frame& f : frames)
    {
        const engine::linear_rgb s = engine::sample(flat, samp, 0.37f, 0.61f);
        const engine::vec3 got = engine::normalised(perturb(f.n, f.t, s));
        const engine::vec3 want = engine::normalised(f.n);
        const float err = engine::length(got - want);
        if (err > worst) { worst = err; worst_case = f.what; }
    }

    // ---- AND THE FLOOR IS NOT ZERO, WHICH IS THE FINDING ------------------
    //
    // The first run of this section asserted `worst < 1e-6` and measured
    // 5.55e-03. The loader was right and the assertion was wrong, for a reason
    // worth more than the check: **0.5 IS NOT AN 8-BIT CODE.** The nearest is
    // 128, and `128/255 * 2 - 1` is `1/255`, not 0 — so the flat direction a
    // normal map can actually store is (0.0039, 0.0039, 1), and every flat
    // normal map in existence tilts the surface it describes very slightly.
    //
    // Predicted here from the encoding rather than read off the measurement,
    // which is what makes this a test of the round trip and not a record of it.
    const float flat_code = 128.0f / 255.0f * 2.0f - 1.0f;   // = 1/255 exactly
    const engine::vec3 storable =
        engine::normalised(engine::vec3{flat_code, flat_code, 1.0f});
    const float floor_err = engine::length(storable - engine::vec3{0.0f, 0.0f, 1.0f});
    const float floor_tilt =
        std::atan(std::sqrt(2.0f) * flat_code) * 180.0f / 3.14159265f;

    // The comparison is against ONE number even though `worst` ranges over four
    // different frames, and that is not sloppiness: the basis change is a
    // ROTATION (§B asserts the frame is orthonormal), and a rotation preserves
    // length — so the error has the same magnitude whichever frame it is
    // expressed in. If that stops being true, §B fails first.
    checkf(std::fabs(worst - floor_err) < 1e-6f,
           "a flat map returns the geometric normal to %.4e over four frames "
           "(worst: %s), which is EXACTLY the quantisation floor of %.4e — not "
           "approximately, to seven digits. ONE ASSERTION, SIX BUGS: a wrong "
           "colour space, a wrong decode range, a frame that is not orthonormal, "
           "a bitangent with the wrong sign, a basis change written in the wrong "
           "order, and a tangent that was never re-orthogonalised all break it",
           static_cast<double>(worst), worst_case, static_cast<double>(floor_err));

    checkf(floor_tilt > 0.3f && floor_tilt < 0.35f,
           "…and the floor is not zero because 0.5 IS NOT AN 8-BIT CODE. The "
           "nearest is 128, and 128/255*2-1 is 1/255 rather than 0 — so the "
           "flattest normal map that can be stored still tilts its surface by "
           "%.3f degrees, everywhere. That is why 16-bit normal maps exist, and "
           "why some pipelines argue about 127 against 128",
           static_cast<double>(floor_tilt));

    // And the counter-test, because an identity that holds for the wrong reason
    // is not an identity: read the SAME map as colour and the round trip fails.
    engine::texture wrong(16, 16, 0xFF8080FFu, engine::texel_space::srgb);
    const engine::linear_rgb bad = engine::sample(wrong, samp, 0.37f, 0.61f);
    const engine::vec3 n{0.0f, 1.0f, 0.0f};
    const engine::vec3 got = engine::normalised(
        perturb(n, engine::vec4{1.0f, 0.0f, 0.0f, 1.0f}, bad));
    const float tilt_deg =
        std::acos(std::fmin(1.0f, engine::dot(got, n))) * 180.0f / 3.14159265f;
    checkf(tilt_deg > 30.0f,
           "…and read as COLOUR the same flat map tilts the normal by %.1f "
           "degrees. Everywhere, in one direction, on every surface — which is "
           "not a subtle artefact, and is still the kind that gets shipped, "
           "because it looks like a normal map authored too strong",
           static_cast<double>(tilt_deg));
}

// ===========================================================================
//  §E — THE ANALYTIC MAP
// ===========================================================================

void section_e_analytic()
{
    std::printf("\n=== E. The bump map against its own closed form ===\n");

    constexpr int size = 256;
    constexpr int cells = 4;
    constexpr float strength = 1.0f;

    const engine::texture bumps = engine::make_normal_bumps(size, cells, strength);
    const engine::sampler samp{.texel_filter = engine::filter::nearest};

    const float k = 2.0f * 3.14159265358979f * static_cast<float>(cells);
    const float amplitude = strength / k;

    // The steepest tilt the map should contain, from the parameterisation
    // rather than from the image: peak |grad h| is amplitude*k = strength.
    checkf(std::fabs(amplitude * k - strength) < 1e-6f,
           "peak gradient = amplitude*k = %.4f = `strength`, at ANY cell count. "
           "That is the whole reason the 1/k is in there: with a fixed amplitude, "
           "six cells gives a slope of 37.7 — a surface tilted 88 degrees "
           "everywhere — and the picture goes dark in a way that reads as a bug "
           "in the shading rather than an absurd input. It did, for one build",
           static_cast<double>(amplitude * k));

    float worst = 0.0f;
    int samples = 0;
    for (int y = 0; y < size; y += 7)
    {
        for (int x = 0; x < size; x += 7)
        {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);

            const float dhdu = -amplitude * k * std::sin(k * u) * std::cos(k * v);
            const float dhdv = -amplitude * k * std::cos(k * u) * std::sin(k * v);
            const engine::vec3 want =
                engine::normalised(engine::vec3{-dhdu, -dhdv, 1.0f});

            const engine::linear_rgb s = engine::sample(bumps, samp, u, v);
            const engine::vec3 got = engine::normalised(
                engine::vec3{s.r * 2.0f - 1.0f, s.g * 2.0f - 1.0f, s.b * 2.0f - 1.0f});

            const float err = engine::length(got - want);
            if (err > worst) { worst = err; }
            ++samples;
        }
    }

    // The tolerance is DERIVED from the encoding, not chosen. Rounding to the
    // nearest of 256 codes is a half-code error, and a half code in the stored
    // [0,1] range is `0.5/255`, which the [-1,1] decode doubles to `1/255`.
    // Three channels of that, in the worst direction, is `sqrt(3)/255`.
    //
    // (The first draft of this line said 1/510 and 0.0034, which forgot the
    // doubling — and the measurement, 5.18e-03, is above that and below the
    // correct bound. A tolerance that is WRONG rather than merely loose fails a
    // correct implementation, which is the more expensive mistake of the two.)
    const float bound = std::sqrt(3.0f) / 255.0f;
    checkf(worst < bound,
           "the sampled normal matches the closed form to %.2e over %d samples, "
           "against a derived bound of sqrt(3)/255 = %.2e. The bound is the "
           "ENCODING: a half-code error is 0.5/255 stored, doubled to 1/255 by "
           "the [-1,1] decode, over three channels. A tolerance loose enough to "
           "admit a real error would be a test deleted slowly (6.3's rule)",
           static_cast<double>(worst), samples, static_cast<double>(bound));

    // The flat texel is exactly the identity, which the quantisation above does
    // NOT threaten — 0.5 is not representable in 8 bits and 128/255 is the
    // nearest code, which is why §D's assertion is about the DIRECTION.
    const engine::linear_rgb centre = engine::sample(bumps, samp, 0.0f, 0.0f);
    checkf(std::fabs(centre.b - 1.0f) < 1e-6f,
           "at a peak of the height field the gradient is zero and the normal is "
           "straight up: blue = %.4f = 255/255 exactly. Which is worth noticing — "
           "the z channel gets the FULL range and x and y only ever use half of "
           "it either side of 128, so a normal map spends a third of its bits on "
           "an axis that is almost always near 1. That is what two-channel normal "
           "maps exploit (Exercise 4)",
           static_cast<double>(centre.b));
}

// ===========================================================================
//  §F — WHICH MATRIX CARRIES A TANGENT
// ===========================================================================

void section_f_transform()
{
    std::printf("\n=== F. A tangent is carried by the MODEL matrix ===\n");

    // A non-uniform scale, which is the only case where the two answers differ —
    // and is exactly why the mistake survives: under a uniform scale, or none,
    // the two agree exactly and two thirds of a typical scene looks perfect.
    const engine::mat4 model = engine::to_mat4(engine::scale(2.0f, 1.0f, 1.0f));

    // A surface in the xy plane (N = +z) with a tangent at 45 degrees. Not
    // axis-aligned, because an axis-aligned tangent is scaled by the two matrices
    // in DIFFERENT amounts and the SAME direction, so normalising hides it.
    const engine::vec3 t_model = engine::normalised(engine::vec3{1.0f, 1.0f, 0.0f});

    const engine::vec3 by_model =
        engine::normalised(engine::linear_of(model) * t_model);
    const engine::vec3 by_normal =
        engine::normalised(engine::normal_matrix(model) * t_model);

    const float dot = engine::dot(by_model, by_normal);
    const float angle = std::acos(std::fmin(1.0f, dot)) * 180.0f / 3.14159265f;

    checkf(angle > 30.0f,
           "the two answers are %.1f degrees apart: model gives (%.3f, %.3f, "
           "%.3f), the normal matrix gives (%.3f, %.3f, %.3f). Both lie IN the "
           "surface, which is why the wrong one is not obviously wrong — the "
           "frame is merely SKEWED, and a skewed normal map reads as an asset "
           "problem",
           static_cast<double>(angle), static_cast<double>(by_model.x),
           static_cast<double>(by_model.y), static_cast<double>(by_model.z),
           static_cast<double>(by_normal.x), static_cast<double>(by_normal.y),
           static_cast<double>(by_normal.z));

    // WHICH ONE IS RIGHT, established rather than asserted. A tangent is a
    // difference of positions, so transform two nearby points and subtract.
    const engine::vec3 p0{0.0f, 0.0f, 0.0f};
    const engine::vec3 p1 = t_model * 0.001f;
    const engine::vec3 q0 = engine::xyz(model * engine::point(p0));
    const engine::vec3 q1 = engine::xyz(model * engine::point(p1));
    const engine::vec3 truth = engine::normalised(q1 - q0);

    checkf(engine::length(truth - by_model) < 1e-6f,
           "and the model matrix is the RIGHT one, established by transporting "
           "two nearby points and subtracting: (%.4f, %.4f, %.4f), matching to "
           "%.2e. A tangent lies IN the surface, so it is a difference of "
           "positions, and a difference of positions transforms the way positions "
           "do. A NORMAL is perpendicular to the surface, and perpendicularity is "
           "what the inverse transpose exists to preserve (3.6)",
           static_cast<double>(truth.x), static_cast<double>(truth.y),
           static_cast<double>(truth.z),
           static_cast<double>(engine::length(truth - by_model)));

    checkf(engine::length(truth - by_normal) > 0.4f,
           "…and the normal matrix is off by %.3f in the same test, which is the "
           "number that makes this an argument rather than a preference",
           static_cast<double>(engine::length(truth - by_normal)));

    // Under a UNIFORM scale they agree, which is the whole reason the bug ships.
    const engine::mat4 uniform = engine::to_mat4(engine::scale(3.0f, 3.0f, 3.0f));
    const engine::vec3 u_model =
        engine::normalised(engine::linear_of(uniform) * t_model);
    const engine::vec3 u_normal =
        engine::normalised(engine::normal_matrix(uniform) * t_model);
    checkf(engine::length(u_model - u_normal) < 1e-6f,
           "under a UNIFORM scale the two agree to %.2e — so an engine that gets "
           "this wrong looks perfect on every object nobody stretched, which is "
           "most of them. Identical in shape to the normal-matrix bug 3.6 put on "
           "a key for the same reason",
           static_cast<double>(engine::length(u_model - u_normal)));

    // ---- And the uniform block did not grow --------------------------------
    engine::material m;
    m.normal_map = engine::texture_handle{};
    const engine::material_uniforms flat_u = engine::uniforms_of(m);
    checkf(sizeof(engine::material_uniforms) == 32 && flat_u.normal_mapped == 0.0f,
           "material_uniforms is still exactly 32 bytes and `normal_mapped` is "
           "DERIVED from the handle. The flag cost zero bytes because it spent "
           "the `pad0` slot 6.4 added to fill the second register — so no binding "
           "code moved and no shader's register allocation changed");
}

// ===========================================================================
//  §G — THE GOLDEN
// ===========================================================================

void section_g_golden()
{
    std::printf("\n=== G. The golden — a new capability must not move the old picture ===\n");

    const int rc = demo::write_reference_shot("scratch/verify67.ppm");
    check(rc == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file("scratch/verify67.ppm");
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(),
           "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL, hash E917C06C — SIXTEENTH lesson at this hash. This "
          "one threaded a fourth varying through the vertex, the clipper, the "
          "interpolator and both shaders, grew the GPU vertex from 32 bytes to "
          "48, and added a branch to the one function every texture read goes "
          "through — and moved not one pixel, because no material in the "
          "reference scene has a normal map. A NEW CAPABILITY IS A NEW PATH");
}

}   // namespace

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    std::printf("verify_67 — Lesson 6.7: normal mapping and the TBN basis\n");

    section_a_colour_space();
    section_b_derivation();
    section_c_handedness();
    section_d_round_trip();
    section_e_analytic();
    section_f_transform();
    section_g_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
