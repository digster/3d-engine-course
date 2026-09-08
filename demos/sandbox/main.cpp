// src/main.cpp — the engine's entry point.
//
// This file hosts the current lesson's demo. As of Lesson 2.12 — the Module 2
// MILESTONE — that is a real indexed MESH spinning in perspective: a regular
// icosahedron (12 vertices, 20 triangles, built from the golden ratio) plus two
// cubes, on a ground plane, carried through the COMPLETE coordinate chain
// model -> world (2.8) -> view (2.9) -> clip (2.10) -> NDC -> screen (2.11).
// [P] swaps perspective/orthographic; the HUD narrates vertex 0 of the selected
// mesh through every one of those spaces, so you can watch w stop being 1 at clip
// and the +y axis flip at screen. Not one line of it is unexplained.
//
// [Tab] CYCLES five demos — scene (2.6-2.12), basis transforms (2.5), triangles
// (2.2-2.4), lines (2.1), and Pong (1.8) — because deleting a working demo to
// make room would be a regression. Five is far too many for one executable, and
// the next one will be worse. That is not an oversight: Module 5 opens by
// splitting the tree into a static library and a demos/ directory, and this file
// is where the argument for doing so accumulates until it is impossible to ignore.
//
// The loop is the one settled in Lesson 1.4 and does not change again:
//
//     drain events -> tick clock -> update input -> N fixed steps -> render

// Our own, most-local first. The order is not aesthetic: a demo's own headers
// coming first means that if one of them forgot an include, THIS translation
// unit is where it fails, rather than silently working because <engine/...>
// happened to pull in what it needed (cpp-style §3).
//
// Spelled short rather than as "../common/demo_scene.hpp", because demos/common
// exports its directory: `target_include_directories(demo_common PUBLIC common)`.
// A relative path would work too and would be a lie about where the header comes
// from — it comes from a LIBRARY this target links, not from a sibling folder.
#include "demo_scene.hpp"

// The engine's public API. Angle brackets, because that is what an installed
// library's headers look like from outside it — and after Lesson 5.1 there is no
// other way to spell them: the demo's include path contains engine/include and
// nothing else, so `#include "gfx/raster.hpp"` is now a compile error rather
// than a style violation. The boundary is enforced by the build, not by taste.
#include <engine/core/clock.hpp>
#include <engine/core/fixed_step.hpp>
#include <engine/core/input.hpp>
#include <engine/core/profile.hpp>        // Lesson 3.10: the frame budget
#include <engine/gfx/clip.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/debug_draw.hpp>      // Lesson 5.1: wireframes, axes, the depth view
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/gpu_buffer.hpp>      // Lesson 4.4: three vertices, on the device
#include <engine/gfx/gpu_debug.hpp>       // Lesson 4.9: names, groups, and a frame log
#include <engine/gfx/gpu_device.hpp>      // Lesson 4.2: the device, and the window claim
#include <engine/gfx/gpu_mesh.hpp>        // Lesson 4.5: a real mesh, interleaved and indexed
#include <engine/gfx/gpu_pipeline.hpp>    // Lesson 4.4: every piece of render state, in one object
#include <engine/gfx/gpu_present.hpp>     // Lesson 4.2: a framebuffer, carried by the GPU
#include <engine/gfx/gpu_scene.hpp>       // Lesson 4.8: a scene, rather than a thing
#include <engine/gfx/gpu_shader.hpp>      // Lesson 4.3: HLSL, compiled and on the device
#include <engine/gfx/gpu_texture.hpp>     // Lesson 4.7: an image on the device, and the depth target
#include <engine/gfx/gpu_uniform.hpp>     // Lesson 4.6: data that is the same for every vertex
#include <engine/gfx/image.hpp>           // Lesson 4.7: stb_image, behind our own interface
#include <engine/gfx/light.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/obj.hpp>
#include <engine/gfx/projector.hpp>       // Lesson 5.1: view space -> pixels, gathered
#include <engine/gfx/raster.hpp>
#include <engine/gfx/scene.hpp>           // Lesson 5.1: a thing in the world
#include <engine/gfx/soft_renderer.hpp>   // Lesson 5.1: the CPU pipeline, now the engine's
#include <engine/gfx/texture.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat2.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec2.hpp>
#include <engine/core/log.hpp>            // Lesson 5.3: categories and levels
#include <engine/platform/platform.hpp>   // Lesson 5.2: SDL's lifecycle, once

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Lesson 5.1 — what did NOT go into the engine
// ---------------------------------------------------------------------------
//
// These are the functions that cycle an enum to its next value on a keypress,
// plus the two label helpers for engine enums and the rule that says which
// cells of the shading grid are degenerate. Every one of them is about a
// KEYBOARD, and a keyboard is not the renderer's business — an engine that
// shipped `next_cull()` would be shipping this demo's [U] key to everybody who
// linked it.
//
// They are the cheapest possible illustration of the question the split forces:
// not "is this code good" but "whose is it". They compile and behave exactly as
// they did; the only thing that changed is which side of a line they sit on.

[[nodiscard]] engine::near_mode next_near(engine::near_mode m)
{
    switch (m)
    {
    case engine::near_mode::clip: return engine::near_mode::drop;
    case engine::near_mode::drop: return engine::near_mode::none;
    case engine::near_mode::none: return engine::near_mode::clip;
    }
    return engine::near_mode::clip;
}

[[nodiscard]] engine::trs_order next_order(engine::trs_order o)
{
    switch (o)
    {
    case engine::trs_order::trs: return engine::trs_order::tsr;
    case engine::trs_order::tsr: return engine::trs_order::rts;
    case engine::trs_order::rts: return engine::trs_order::trs;
    }
    return engine::trs_order::trs;
}

[[nodiscard]] engine::normal_source next_normal_source(engine::normal_source n)
{
    return (n == engine::normal_source::face) ? engine::normal_source::vertex : engine::normal_source::face;
}

[[nodiscard]] engine::shade_eval next_eval(engine::shade_eval e)
{
    switch (e)
    {
    case engine::shade_eval::palette:   return engine::shade_eval::flat;
    case engine::shade_eval::flat:      return engine::shade_eval::gouraud;
    case engine::shade_eval::gouraud:   return engine::shade_eval::per_pixel;
    case engine::shade_eval::per_pixel: return engine::shade_eval::palette;
    }
    return engine::shade_eval::palette;
}

/// Does this cell of the 2x3 grid produce the same picture as plain flat shading?
///
/// **With a face normal, ALL THREE evaluation points agree exactly — but only
/// while the shading is view-independent.** That one sentence is the test of
/// whether you have the two axes straight, and it is worth working through:
///
///   - A face normal is constant over the triangle, so the diffuse term depends on
///     nothing that varies across it. Evaluate it once at the centroid, three times
///     at the corners, or once per fragment: the same number comes out, and
///     interpolating equal numbers gives that number back.
///   - A SPECULAR term breaks it, because `to_eye = eye - position` varies across a
///     face even when the normal does not. Now the corners genuinely differ, and
///     flat / Gouraud / per-pixel are three different pictures.
///
/// **The first draft of this function got that wrong**, claiming `face x gouraud`
/// was degenerate unconditionally. `verify_38` §A measured 761 differing pixels
/// with the highlight on and settled it. The distinction is not pedantic: it is the
/// difference between "the normal is the only input that varies" and "the normal is
/// the only input", and 3.7 is exactly where the second stopped being true.
[[nodiscard]] bool is_degenerate(engine::normal_source n, engine::shade_eval e, engine::specular_model m)
{
    if (n != engine::normal_source::face) { return false; }
    if (e == engine::shade_eval::flat) { return false; }   // flat IS the thing compared against
    return m == engine::specular_model::none;
}

[[nodiscard]] engine::cull_choice next_cull(engine::cull_choice c)
{
    switch (c)
    {
    case engine::cull_choice::none:            return engine::cull_choice::back;
    case engine::cull_choice::back:            return engine::cull_choice::front;
    case engine::cull_choice::front:           return engine::cull_choice::back_by_forward;
    case engine::cull_choice::back_by_forward: return engine::cull_choice::none;
    }
    return engine::cull_choice::none;
}

[[nodiscard]] engine::address_mode next_address(engine::address_mode m)
{
    switch (m)
    {
    case engine::address_mode::repeat:          return engine::address_mode::mirrored_repeat;
    case engine::address_mode::mirrored_repeat: return engine::address_mode::clamp_to_edge;
    case engine::address_mode::clamp_to_edge:   return engine::address_mode::repeat;
    }
    return engine::address_mode::repeat;
}

[[nodiscard]] const char* name_of(engine::filter f)
{
    switch (f)
    {
    case engine::filter::nearest: return "NEAREST";
    case engine::filter::linear:  return "BILINEAR";
    }
    return "?";
}

[[nodiscard]] const char* name_of(engine::address_mode m)
{
    switch (m)
    {
    case engine::address_mode::repeat:          return "repeat";
    case engine::address_mode::mirrored_repeat: return "mirrored";
    case engine::address_mode::clamp_to_edge:   return "clamp";
    }
    return "?";
}

constexpr Uint32 k_throttle_ms = 50;

// The framebuffer's size moved to demos/common/demo_scene.hpp in Lesson 5.1,
// because `k_full_viewport` is written in terms of it and the viewport is demo
// content. Aliased here so the 200-odd uses below did not all have to change.
constexpr int k_fb_width = demo::k_fb_width;
constexpr int k_fb_height = demo::k_fb_height;

/// Which lesson's demo is on screen. [Tab] cycles.
///
/// Renamed from `demo` in Lesson 5.1, because `demo` is now the namespace
/// the shared demo content lives in and an enum in an anonymous namespace
/// would hide it. The smallest possible cost of drawing a boundary, and
/// worth noticing: names only collide once they can see each other.
enum class screen
{
    scene,       ///< Lessons 2.6 – 2.8 — this lesson
    basis,       ///< Lesson 2.5
    triangles,   ///< Lessons 2.2 – 2.4
    lines        ///< Lesson 2.1
    // Lesson 1.8's Pong used to be a fifth value here. Lesson 5.2 gave it its
    // own executable — `./build/demos/pong` — because `engine::app` means a
    // demo no longer has to borrow somebody else's loop to exist.
};

[[nodiscard]] screen next_demo(screen d)
{
    switch (d)
    {
    case screen::scene:     return screen::basis;
    case screen::basis:     return screen::triangles;
    case screen::triangles: return screen::lines;
    case screen::lines:     return screen::scene;
    }
    return screen::scene;
}

// ===========================================================================
// Lesson 2.2 — triangles
// ===========================================================================

/// How the left panel draws its triangle.
enum class tri_mode
{
    filled,      ///< fill_triangle: the finished article
    wireframe,   ///< draw_triangle: three lines, for comparison
    halfplanes,  ///< colour every pixel by WHICH edge tests it passes
    weights,     ///< Lesson 2.3: w0 as a ramp, with a live probe
    isolines,    ///< Lesson 2.3: contours of constant weight
    gouraud,     ///< Lesson 2.4: three corner colours, interpolated
    checker      ///< Lesson 2.4: the same loop carrying (u,v) instead
};

[[nodiscard]] const char* name_of(tri_mode m)
{
    switch (m)
    {
    case tri_mode::filled:     return "filled";
    case tri_mode::wireframe:  return "wireframe";
    case tri_mode::halfplanes: return "half-planes";
    case tri_mode::weights:    return "weights (w0)";
    case tri_mode::isolines:   return "iso-lines";
    case tri_mode::gouraud:    return "gouraud";
    case tri_mode::checker:    return "uv checker";
    }
    return "?";
}

/// True for the two Lesson 2.4 views, which share a right-hand panel.
[[nodiscard]] bool is_attribute_view(tri_mode m)
{
    return m == tri_mode::gouraud || m == tri_mode::checker;
}

/// Paint the three edge functions' signs across the triangle's bounding box.
///
/// This is the lesson's central claim made visible: a triangle is the region
/// where three half-planes overlap. A pixel passing all three tests is inside;
/// one passing two is in the "wrong" part of two half-planes, and so on. Written
/// with direct edge_function calls rather than the incremental form, because
/// here the point is what is being computed, not how cheaply.
void draw_halfplanes(engine::framebuffer& fb,
                     int x0, int y0, int x1, int y1, int x2, int y2)
{
    int area = engine::edge_function(x0, y0, x1, y1, x2, y2);
    if (area == 0) { return; }
    if (area < 0) { std::swap(x1, x2); std::swap(y1, y2); }

    const int min_x = std::max(0, std::min({x0, x1, x2}) - 14);
    const int min_y = std::max(0, std::min({y0, y1, y2}) - 14);
    const int max_x = std::min(fb.width() - 1, std::max({x0, x1, x2}) + 14);
    const int max_y = std::min(fb.height() - 1, std::max({y0, y1, y2}) + 14);

    for (int y = min_y; y <= max_y; ++y)
    {
        for (int x = min_x; x <= max_x; ++x)
        {
            const int w0 = engine::edge_function(x1, y1, x2, y2, x, y);
            const int w1 = engine::edge_function(x2, y2, x0, y0, x, y);
            const int w2 = engine::edge_function(x0, y0, x1, y1, x, y);

            const int passes = (w0 >= 0 ? 1 : 0) + (w1 >= 0 ? 1 : 0) + (w2 >= 0 ? 1 : 0);

            // Three shades outside, one bright fill inside — so "inside" reads
            // as a region rather than as a colour among colours.
            Uint32 c = 0;
            switch (passes)
            {
            case 3: c = engine::pack_argb(226, 196, 110); break;   // inside
            case 2: c = engine::pack_argb(56, 62, 84); break;
            case 1: c = engine::pack_argb(34, 38, 54); break;
            default: c = engine::pack_argb(20, 22, 32); break;
            }
            fb.put_pixel(x, y, c);
        }
    }
}

/// Paint each pixel by its w0 — "how much of vertex 0 is here".
///
/// A ramp rather than three colours, because this view is about a single
/// COORDINATE. Blending three vertex colours is interpolating an attribute,
/// which is Lesson 2.4's subject and needs the linear-light care of Lesson 1.6.
///
/// Pixels outside the triangle are drawn too, in red, because w0 goes negative
/// out there and that is worth seeing rather than hiding: the weights describe
/// the whole plane, not just the interior.
void draw_weights(engine::framebuffer& fb,
                  int x0, int y0, int x1, int y1, int x2, int y2)
{
    const int pad = 20;
    const int min_x = std::max(0, std::min({x0, x1, x2}) - pad);
    const int min_y = std::max(0, std::min({y0, y1, y2}) - pad);
    const int max_x = std::min(fb.width() - 1, std::max({x0, x1, x2}) + pad);
    const int max_y = std::min(fb.height() - 1, std::max({y0, y1, y2}) + pad);

    for (int y = min_y; y <= max_y; ++y)
    {
        for (int x = min_x; x <= max_x; ++x)
        {
            const engine::barycentric b =
                engine::barycentric_at(x0, y0, x1, y1, x2, y2, x, y);

            if (b.w0 < 0.0f)
            {
                // Negative: past the edge opposite v0. Deepen with distance.
                const float m = std::min(1.0f, -b.w0);
                const Uint8 r = static_cast<Uint8>(40.0f + 150.0f * m);
                fb.put_pixel(x, y, engine::pack_argb(r, 30, 38));
            }
            else
            {
                const float m = std::min(1.0f, b.w0);
                const Uint8 v = static_cast<Uint8>(24.0f + 220.0f * m);
                fb.put_pixel(x, y, engine::pack_argb(v, v, static_cast<Uint8>(v * 0.72f)));
            }
        }
    }
}

/// Draw contour lines where any weight crosses a multiple of 0.1.
///
/// The point of the picture: every contour of w0 is PARALLEL to the edge
/// opposite v0, and they are evenly spaced. That is what "the triangle's own
/// coordinate system" looks like — three families of parallel lines, one per
/// vertex, and the weights are just how far along each family you are.
void draw_isolines(engine::framebuffer& fb,
                   int x0, int y0, int x1, int y1, int x2, int y2)
{
    const int pad = 12;
    const int min_x = std::max(0, std::min({x0, x1, x2}) - pad);
    const int min_y = std::max(0, std::min({y0, y1, y2}) - pad);
    const int max_x = std::min(fb.width() - 1, std::max({x0, x1, x2}) + pad);
    const int max_y = std::min(fb.height() - 1, std::max({y0, y1, y2}) + pad);

    const Uint32 tint[3] = {engine::pack_argb(236, 122, 92),
                            engine::pack_argb(122, 196, 152),
                            engine::pack_argb(126, 162, 236)};

    for (int y = min_y; y <= max_y; ++y)
    {
        for (int x = min_x; x <= max_x; ++x)
        {
            const engine::barycentric b =
                engine::barycentric_at(x0, y0, x1, y1, x2, y2, x, y);
            const engine::barycentric br =
                engine::barycentric_at(x0, y0, x1, y1, x2, y2, x + 1, y);
            const engine::barycentric bd =
                engine::barycentric_at(x0, y0, x1, y1, x2, y2, x, y + 1);

            const float w[3]  = {b.w0, b.w1, b.w2};
            const float wr[3] = {br.w0, br.w1, br.w2};
            const float wd[3] = {bd.w0, bd.w1, bd.w2};

            // A contour is "this pixel and its right/below neighbour fall either
            // side of a multiple of 0.1". Comparing floor()s asks that without
            // hunting for exact equality, which floats would never satisfy —
            // §3.6 measured the sum landing on 1.0f only 85% of the time.
            constexpr float step = 0.1f;
            for (int i = 0; i < 3; ++i)
            {
                const bool crosses =
                    std::floor(w[i] / step) != std::floor(wr[i] / step) ||
                    std::floor(w[i] / step) != std::floor(wd[i] / step);

                // Only inside the triangle: the contours continue over the
                // whole plane (they must — the weights are defined everywhere),
                // but drawn unbounded they bury the shape they describe.
                const bool inside = w[0] >= 0.0f && w[1] >= 0.0f && w[2] >= 0.0f;
                if (crosses && inside)
                {
                    fb.put_pixel(x, y, tint[i]);
                    break;
                }
            }
        }
    }
}

/// The mouse probe: the three sub-triangles whose areas ARE the weights.
///
/// This is the derivation drawn live. P is the cursor; each sub-triangle is
/// P with one edge of the original, and each is tinted to match the vertex it
/// is opposite — the vertex whose weight it supplies.
void draw_probe(engine::framebuffer& fb,
                int x0, int y0, int x1, int y1, int x2, int y2,
                int px, int py, engine::barycentric b)
{
    const Uint32 tint[3] = {engine::pack_argb(236, 122, 92),
                            engine::pack_argb(122, 196, 152),
                            engine::pack_argb(126, 162, 236)};

    // Sub-triangle i is P with the edge OPPOSITE vertex i — the same pairing
    // barycentric_at uses, drawn so the two cannot drift apart.
    engine::draw_triangle(fb, px, py, x1, y1, x2, y2, tint[0]);
    engine::draw_triangle(fb, px, py, x2, y2, x0, y0, tint[1]);
    engine::draw_triangle(fb, px, py, x0, y0, x1, y1, tint[2]);

    // Three bars, lengths proportional to the weights, in the same tints.
    const int bar_x = 6;
    const int bar_y = 150;
    const float w[3] = {b.w0, b.w1, b.w2};
    for (int i = 0; i < 3; ++i)
    {
        const int len = static_cast<int>(std::lround(std::max(0.0f, w[i]) * 100.0f));
        fb.fill_rect(bar_x, bar_y + i * 6, 100, 4, engine::pack_argb(30, 32, 42));
        fb.fill_rect(bar_x, bar_y + i * 6, std::min(len, 100), 4, tint[i]);
    }

    fb.fill_rect(px - 1, py - 1, 3, 3, engine::pack_argb(250, 250, 240));
}

// ---------------------------------------------------------------------------
// Lesson 2.4 — a second attribute, and the bias made visible
// ---------------------------------------------------------------------------

/// The engine's shaded fill, with `(u, v)` carried instead of a colour.
///
/// Written out longhand on purpose. Compare it line for line against
/// `fill_triangle` in raster.cpp: the setup is the same, the walk is the same,
/// the unbias-and-divide is the same. The *only* difference is what the three
/// weighted sums add up — two floats here instead of three, and a procedural
/// checker instead of a colour blend at the end.
///
/// That is the lesson's claim made concrete: the machinery does not care what it
/// carries. It is also, honestly, the argument for not writing it this way. A
/// third copy of this loop is where a rasterizer starts to rot, and the engine's
/// answer arrives in stages — Module 3 grows `vertex` as each attribute earns
/// its place, and Module 4 hands the whole problem to the GPU, where these are
/// called *varyings* and the hardware interpolates them for you.
void fill_triangle_uv(engine::framebuffer& fb,
                      int x0, int y0, int x1, int y1, int x2, int y2,
                      bool biased_weights)
{
    // (u,v) at the three corners: a unit right-triangle's worth of texture
    // space, which is what an OBJ file will hand us in Lesson 3.5.
    float u[3] = {0.0f, 1.0f, 0.0f};
    float v[3] = {0.0f, 0.0f, 1.0f};

    int area = engine::edge_function(x0, y0, x1, y1, x2, y2);
    if (area == 0) { return; }
    if (area < 0)
    {
        // Reorienting moves vertex 1 to slot 2 — so the attributes move too.
        // Exactly the bug `struct vertex` exists to prevent, here where it has
        // to be done by hand.
        std::swap(x1, x2); std::swap(y1, y2);
        std::swap(u[1], u[2]); std::swap(v[1], v[2]);
        area = -area;
    }

    const int min_x = std::max(0, std::min({x0, x1, x2}));
    const int min_y = std::max(0, std::min({y0, y1, y2}));
    const int max_x = std::min(fb.width() - 1, std::max({x0, x1, x2}));
    const int max_y = std::min(fb.height() - 1, std::max({y0, y1, y2}));
    if (min_x > max_x || min_y > max_y) { return; }

    const int bias0 = engine::is_top_left(x1, y1, x2, y2) ? 0 : -1;
    const int bias1 = engine::is_top_left(x2, y2, x0, y0) ? 0 : -1;
    const int bias2 = engine::is_top_left(x0, y0, x1, y1) ? 0 : -1;

    const int step_x0 = y1 - y2, step_y0 = x2 - x1;
    const int step_x1 = y2 - y0, step_y1 = x0 - x2;
    const int step_x2 = y0 - y1, step_y2 = x1 - x0;

    int row_w0 = engine::edge_function(x1, y1, x2, y2, min_x, min_y) + bias0;
    int row_w1 = engine::edge_function(x2, y2, x0, y0, min_x, min_y) + bias1;
    int row_w2 = engine::edge_function(x0, y0, x1, y1, min_x, min_y) + bias2;

    const float inv_area = 1.0f / static_cast<float>(area);

    for (int y = min_y; y <= max_y; ++y)
    {
        int w0 = row_w0, w1 = row_w1, w2 = row_w2;
        Uint32* const row = fb.row(y);

        for (int x = min_x; x <= max_x; ++x)
        {
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                const float f0 = static_cast<float>(biased_weights ? w0 : w0 - bias0) * inv_area;
                const float f1 = static_cast<float>(biased_weights ? w1 : w1 - bias1) * inv_area;
                const float f2 = static_cast<float>(biased_weights ? w2 : w2 - bias2) * inv_area;

                const float uu = f0 * u[0] + f1 * u[1] + f2 * u[2];
                const float vv = f0 * v[0] + f1 * v[1] + f2 * v[2];

                // A procedural texture: no image, no sampler, no filtering —
                // just a rule evaluated at (u,v). Lesson 3.7 replaces the rule
                // with a lookup, and nothing else about this loop changes.
                constexpr float cells = 8.0f;
                const int cu = static_cast<int>(uu * cells);
                const int cv = static_cast<int>(vv * cells);
                row[x] = ((cu + cv) & 1) ? engine::pack_argb(232, 226, 214)
                                         : engine::pack_argb(58, 64, 88);
            }

            w0 += step_x0; w1 += step_x1; w2 += step_x2;
        }

        row_w0 += step_y0; row_w1 += step_y1; row_w2 += step_y2;
    }
}

// The bias magnifier. A triangle small enough that the fill rule's -1 is a
// meaningful fraction of its area, drawn twice — once with the weights
// unbiased, once with the bias left in — and blown up so single pixels are
// legible. Numbers verified by the Lesson 2.4 harness: 2A = 112, the edge
// opposite v0 is 11.314 px long, so the attribute field is displaced by
// 1/11.314 = 0.088 px, perpendicular to that edge.
constexpr int k_mag_w = 14;      ///< the small triangle's own grid
constexpr int k_mag_h = 14;
constexpr int k_mag_zoom = 5;
constexpr int k_mag_x = 172;     ///< where the pair lands in the framebuffer
constexpr int k_mag_y = 40;

constexpr int k_mag_x0 = 1,  k_mag_y0 = 1;
constexpr int k_mag_x1 = 12, k_mag_y1 = 4;
constexpr int k_mag_x2 = 4,  k_mag_y2 = 12;

/// Fill the small triangle with a **striped scalar attribute**, into a grid.
///
/// Stripes rather than a smooth ramp, because that is the whole point. The bias
/// displaces the attribute field by a tenth of a pixel, which on a smooth ramp
/// changes a colour by a fraction of a level and is invisible. Quantise the
/// attribute — a stripe, a checker cell, a texel index — and a tenth of a pixel
/// at a threshold flips whole pixels to the wrong side.
///
/// The band count is adjustable at runtime ( `[` and `]` ) and that is the most
/// important thing about this demo. The displacement is a fixed 0.088 px, but
/// how many *pixels* it ruins depends entirely on where the band thresholds
/// happen to fall relative to it. Sweep the frequency and the count jumps
/// around — 0, then 15, then 4, then 0 again — which is the honest lesson: a
/// sub-pixel error in a quantised attribute cannot be tested away by trying one
/// setting and seeing nothing.
///
/// @return -1 for uncovered cells, 0 or 1 for the stripe parity.
void fill_striped(std::vector<int>& out, bool biased_weights, float bands)
{
    // s = 0 at v0 and 1 at both other corners, so s = 1 - w0: a scalar that
    // sweeps the triangle from one vertex to the opposite edge.
    const float s_at[3] = {0.0f, 1.0f, 1.0f};

    int x0 = k_mag_x0, y0 = k_mag_y0;
    int x1 = k_mag_x1, y1 = k_mag_y1;
    int x2 = k_mag_x2, y2 = k_mag_y2;

    int area = engine::edge_function(x0, y0, x1, y1, x2, y2);
    if (area == 0) { return; }
    if (area < 0) { std::swap(x1, x2); std::swap(y1, y2); area = -area; }

    const int bias0 = engine::is_top_left(x1, y1, x2, y2) ? 0 : -1;
    const int bias1 = engine::is_top_left(x2, y2, x0, y0) ? 0 : -1;
    const int bias2 = engine::is_top_left(x0, y0, x1, y1) ? 0 : -1;

    const float inv_area = 1.0f / static_cast<float>(area);

    for (int y = 0; y < k_mag_h; ++y)
    {
        for (int x = 0; x < k_mag_w; ++x)
        {
            const int w0 = engine::edge_function(x1, y1, x2, y2, x, y) + bias0;
            const int w1 = engine::edge_function(x2, y2, x0, y0, x, y) + bias1;
            const int w2 = engine::edge_function(x0, y0, x1, y1, x, y) + bias2;
            if (w0 < 0 || w1 < 0 || w2 < 0) { continue; }

            // Coverage used the biased values above — that part is correct and
            // is identical in both passes. Only the INTERPOLATION differs.
            const float f0 = static_cast<float>(biased_weights ? w0 : w0 - bias0) * inv_area;
            const float f1 = static_cast<float>(biased_weights ? w1 : w1 - bias1) * inv_area;
            const float f2 = static_cast<float>(biased_weights ? w2 : w2 - bias2) * inv_area;

            const float s = f0 * s_at[0] + f1 * s_at[1] + f2 * s_at[2];
            out[static_cast<std::size_t>(y * k_mag_w + x)] =
                static_cast<int>(std::floor(s * bands)) & 1;
        }
    }
}

/// Draw both versions side by side, ringing every cell where they disagree.
///
/// @return the number of covered cells whose stripe came out different.
int draw_bias_magnifier(engine::framebuffer& fb, float bands)
{
    std::vector<int> good(static_cast<std::size_t>(k_mag_w * k_mag_h), -1);
    std::vector<int> bad(static_cast<std::size_t>(k_mag_w * k_mag_h), -1);
    fill_striped(good, false, bands);
    fill_striped(bad, true, bands);

    const Uint32 k_empty = engine::pack_argb(18, 20, 28);
    const Uint32 k_dark = engine::pack_argb(58, 64, 88);
    const Uint32 k_light = engine::pack_argb(232, 226, 214);
    const Uint32 k_wrong = engine::pack_argb(236, 92, 92);

    int differ = 0;
    for (int panel = 0; panel < 2; ++panel)
    {
        const std::vector<int>& src = (panel == 0) ? good : bad;
        const int ox = k_mag_x + panel * (k_mag_w * k_mag_zoom + 6);

        for (int y = 0; y < k_mag_h; ++y)
        {
            for (int x = 0; x < k_mag_w; ++x)
            {
                const std::size_t i = static_cast<std::size_t>(y * k_mag_w + x);
                const int cell = src[i];
                const Uint32 c = (cell < 0) ? k_empty : (cell == 0 ? k_dark : k_light);

                fb.fill_rect(ox + x * k_mag_zoom, k_mag_y + y * k_mag_zoom,
                             k_mag_zoom, k_mag_zoom, c);

                if (good[i] != bad[i])
                {
                    if (panel == 0) { ++differ; }
                    // A ring, not a fill: the wrong VALUE still needs to be
                    // readable underneath the mark that says it is wrong.
                    engine::draw_triangle(fb,
                        ox + x * k_mag_zoom, k_mag_y + y * k_mag_zoom,
                        ox + x * k_mag_zoom + k_mag_zoom - 1, k_mag_y + y * k_mag_zoom,
                        ox + x * k_mag_zoom, k_mag_y + y * k_mag_zoom + k_mag_zoom - 1,
                        k_wrong);
                }
            }
        }
    }
    return differ;
}

/// The fill rule, disabled — every boundary pixel claimed by every triangle.
///
/// Kept local to the demo rather than added to the engine, because it is not a
/// thing the engine should be able to do. Written the direct way so the
/// comparison against fill_triangle is a comparison of *rules*, not of loops.
void fill_no_rule(std::vector<int>& coverage, int w, int h,
                  int x0, int y0, int x1, int y1, int x2, int y2)
{
    int area = engine::edge_function(x0, y0, x1, y1, x2, y2);
    if (area == 0) { return; }
    if (area < 0) { std::swap(x1, x2); std::swap(y1, y2); }

    const int min_x = std::max(0, std::min({x0, x1, x2}));
    const int min_y = std::max(0, std::min({y0, y1, y2}));
    const int max_x = std::min(w - 1, std::max({x0, x1, x2}));
    const int max_y = std::min(h - 1, std::max({y0, y1, y2}));

    for (int y = min_y; y <= max_y; ++y)
    {
        for (int x = min_x; x <= max_x; ++x)
        {
            const int w0 = engine::edge_function(x1, y1, x2, y2, x, y);
            const int w1 = engine::edge_function(x2, y2, x0, y0, x, y);
            const int w2 = engine::edge_function(x0, y0, x1, y1, x, y);
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                coverage[static_cast<std::size_t>(y * w + x)] += 1;
            }
        }
    }
}

constexpr int k_cov_w = 52;    ///< the coverage grid, in its own small pixels
constexpr int k_cov_h = 52;
constexpr int k_cov_zoom = 2;
constexpr int k_cov_x = 196;   ///< where it lands in the framebuffer
constexpr int k_cov_y = 44;

/// Draw two triangles that share an edge, counting how many times each pixel is
/// written, then colour by that count.
///
/// This is the only honest way to show a fill rule working. With opaque colours
/// a double-drawn pixel looks exactly like a correctly-drawn one — the defect is
/// invisible precisely because the second write lands on top of the first. Count
/// instead, and the seam lights up.
///
/// @return the number of pixels written more than once.
int draw_coverage(engine::framebuffer& fb, bool use_rule)
{
    std::vector<int> coverage(static_cast<std::size_t>(k_cov_w * k_cov_h), 0);

    // An axis-aligned square split corner to corner — the single most common
    // piece of geometry there is, and deliberately not a rotated one.
    //
    // The reason matters. A pixel is only ever at risk of being drawn twice if
    // its centre lies EXACTLY on the shared edge, and for an edge of arbitrary
    // slope almost no pixel centres do. Rotate this quad and the defect shrinks
    // to two or three stray pixels — still wrong, but easy to dismiss as noise.
    // A 45-degree diagonal passes through every pixel centre along it, so the
    // whole seam doubles at once and the failure is impossible to miss.
    //
    // That is not a rigged demo, it is the realistic one: quads split into
    // triangle pairs, terrain grids and UI rectangles are overwhelmingly
    // axis-aligned, so the catastrophic case is also the common case.
    const int lo = 6;
    const int hi = k_cov_w - 7;

    const int ax = lo, ay = lo;    // shared edge runs from here…
    const int bx = hi, by = hi;    // …to here: slope exactly 1

    const int rx = hi, ry = lo;    // upper-right corner
    const int lx = lo, ly = hi;    // lower-left corner

    if (use_rule)
    {
        // Count through the real engine routine, by drawing each triangle into
        // its own buffer and reading it back. Slow, and it means the demo is
        // measuring the shipped code rather than a copy of it.
        for (int t = 0; t < 2; ++t)
        {
            engine::framebuffer scratch(k_cov_w, k_cov_h);
            scratch.clear(0u);
            if (t == 0) { engine::fill_triangle(scratch, ax, ay, bx, by, lx, ly, 0xFFFFFFFFu); }
            else        { engine::fill_triangle(scratch, ax, ay, rx, ry, bx, by, 0xFFFFFFFFu); }

            for (int y = 0; y < k_cov_h; ++y)
            {
                for (int x = 0; x < k_cov_w; ++x)
                {
                    if (scratch.pixel_at(x, y) != 0u)
                    {
                        coverage[static_cast<std::size_t>(y * k_cov_w + x)] += 1;
                    }
                }
            }
        }
    }
    else
    {
        fill_no_rule(coverage, k_cov_w, k_cov_h, ax, ay, bx, by, lx, ly);
        fill_no_rule(coverage, k_cov_w, k_cov_h, ax, ay, rx, ry, bx, by);
    }

    const Uint32 k_once = engine::pack_argb(122, 196, 152);   // correct
    const Uint32 k_twice = engine::pack_argb(236, 92, 92);    // drawn twice
    const Uint32 k_empty = engine::pack_argb(18, 20, 28);

    int doubled = 0;
    for (int y = 0; y < k_cov_h; ++y)
    {
        for (int x = 0; x < k_cov_w; ++x)
        {
            const int n = coverage[static_cast<std::size_t>(y * k_cov_w + x)];
            if (n > 1) { doubled += 1; }
            const Uint32 c = (n == 0) ? k_empty : (n == 1 ? k_once : k_twice);
            fb.fill_rect(k_cov_x + x * k_cov_zoom, k_cov_y + y * k_cov_zoom,
                         k_cov_zoom, k_cov_zoom, c);
        }
    }
    return doubled;
}

// ---------------------------------------------------------------------------
// Lesson 3.1 — hidden surfaces: sort them, or test them per pixel
// ---------------------------------------------------------------------------

/// How the demo decides what is in front of what. [F] cycles.
enum class hidden_surface
{
    wireframe,    ///< 2.12's picture: no surfaces at all, so nothing to hide
    painter,      ///< sort triangles back-to-front and paint over. Fails, on purpose.
    zbuffer,      ///< one depth per pixel. The answer.
    depth_view    ///< show the depth buffer itself, stretched to its own range
};

[[nodiscard]] const char* name_of(hidden_surface h)
{
    switch (h)
    {
    case hidden_surface::wireframe:  return "wireframe (2.12)";
    case hidden_surface::painter:    return "PAINTER'S (sorted)";
    case hidden_surface::zbuffer:    return "Z-BUFFER";
    case hidden_surface::depth_view: return "depth buffer";
    }
    return "?";
}

[[nodiscard]] hidden_surface next_hidden(hidden_surface h)
{
    switch (h)
    {
    case hidden_surface::wireframe:  return hidden_surface::painter;
    case hidden_surface::painter:    return hidden_surface::zbuffer;
    case hidden_surface::zbuffer:    return hidden_surface::depth_view;
    case hidden_surface::depth_view: return hidden_surface::wireframe;
    }
    return hidden_surface::zbuffer;
}

[[nodiscard]] engine::depth_format next_depth_format(engine::depth_format f)
{
    switch (f)
    {
    case engine::depth_format::f32:     return engine::depth_format::unorm24;
    case engine::depth_format::unorm24: return engine::depth_format::unorm16;
    case engine::depth_format::unorm16: return engine::depth_format::f32;
    }
    return engine::depth_format::f32;
}

// ---------------------------------------------------------------------------
// Lesson 3.7 — the highlight
// ---------------------------------------------------------------------------

[[nodiscard]] const char* name_of(engine::specular_model m)
{
    switch (m)
    {
    case engine::specular_model::none:  return "none (3.6, diffuse only)";
    case engine::specular_model::phong: return "PHONG   dot(R,v)^p";
    case engine::specular_model::blinn: return "BLINN   dot(n,h)^p";
    case engine::specular_model::cook_torrance: return "COOK-TORRANCE  D G F / 4(n.l)(n.v)";
    }
    return "?";
}

[[nodiscard]] engine::specular_model next_specular(engine::specular_model m)
{
    switch (m)
    {
    case engine::specular_model::none:  return engine::specular_model::phong;
    case engine::specular_model::phong: return engine::specular_model::blinn;
    case engine::specular_model::blinn: return engine::specular_model::cook_torrance;
    case engine::specular_model::cook_torrance: return engine::specular_model::none;
    }
    return engine::specular_model::none;
}

/// The exponent that makes the *other* model look about as tight as this one.
///
/// Lesson 3.7 §3.4: the halfway vector moves at half the rate the mirror direction
/// does, so `dot(n,h)` falls off half as fast as `dot(R,v)` and needs roughly four
/// times the exponent to match. Measured by fitting: 4.38x at p=4, converging to
/// 4.01x by p=128.
///
/// This exists so the demo's Phong-vs-Blinn pixel count means something. Comparing
/// them at the SAME exponent mostly measures that one lobe is wider than the other,
/// which is not interesting; comparing them at the same visual tightness measures
/// the thing that actually differs — the SHAPE of the tail, and the cut-off.
[[nodiscard]] float matched_roughness(engine::specular_model from, float roughness)
{
    // Lesson 3.7's 4x rule, restated in Lesson 6.4's parameter. The rule was
    // always about the EXPONENT, so the honest way to apply it now is to convert
    // out to an exponent, scale, and convert back — rather than inventing a
    // factor for roughness, which is not linearly related to it.
    const float s = engine::blinn_exponent_from_alpha(engine::alpha_from_roughness(roughness));
    float target = s;
    if (from == engine::specular_model::phong) { target = s * 4.0f; }    // -> blinn
    if (from == engine::specular_model::blinn) { target = s * 0.25f; }   // -> phong
    return engine::roughness_from_alpha(engine::alpha_from_blinn_exponent(target));
}

/// The length of the model's x axis after `m`, in world units.
///
/// Under T*R*S this is `scale.x` at every orientation, because rotation does not
/// change a length. Under T*S*R it is whatever the world-axis scale happens to do
/// to whichever direction the model's x axis is currently pointing — so it
/// breathes as the object turns. Measured from the matrix rather than predicted,
/// so the HUD reports what was drawn.
[[nodiscard]] float axis_length(const engine::mat4& m, engine::vec3 axis)
{
    return engine::length(engine::xyz(m * engine::direction(axis)));
}

/// The angle, in degrees, between two of the model's axes after `m`.
///
/// The model's x and y axes are at 90 degrees to each other by construction. A
/// transform that keeps them there has moved the object; one that does not has
/// DEFORMED it, and this number says which happened. 90.000 under T*R*S always;
/// under T*S*R it opens as far as 157.99 degrees for this lesson's slab.
[[nodiscard]] float axis_angle_deg(const engine::mat4& m, engine::vec3 a, engine::vec3 b)
{
    const engine::vec3 ta = engine::xyz(m * engine::direction(a));
    const engine::vec3 tb = engine::xyz(m * engine::direction(b));
    const float lengths = engine::length(ta) * engine::length(tb);
    if (lengths <= 0.0f) { return 0.0f; }

    // Clamped before acos: dot/(|a||b|) is mathematically in [-1, 1], but floats
    // round, and acos(1.0000001) is NaN rather than 0. Cheap insurance in exactly
    // the place — an exact axis alignment — the demo hits every few seconds.
    const float c = std::clamp(engine::dot(ta, tb) / lengths, -1.0f, 1.0f);
    return std::acos(c) * 180.0f / 3.14159265358979f;
}

// ===========================================================================
// Lesson 2.5 — matrices as basis transforms
// ===========================================================================

/// Which transformation the basis view is showing. [Z] cycles.
enum class xform
{
    identity,
    rotate,
    scale_xy,
    shear_x,
    scale_then_rotate,   ///< R * S — scale is applied FIRST
    rotate_then_scale    ///< S * R — rotate is applied FIRST
};

[[nodiscard]] const char* name_of(xform x)
{
    switch (x)
    {
    case xform::identity:          return "identity";
    case xform::rotate:            return "rotation(t)";
    case xform::scale_xy:          return "scale(1+t, 1-0.6t)";
    case xform::shear_x:           return "shear(t, 0)";
    case xform::scale_then_rotate: return "R(t) * S   [scale first]";
    case xform::rotate_then_scale: return "S * R(t)   [rotate first]";
    }
    return "?";
}

[[nodiscard]] xform next_xform(xform x)
{
    switch (x)
    {
    case xform::identity:          return xform::rotate;
    case xform::rotate:            return xform::scale_xy;
    case xform::scale_xy:          return xform::shear_x;
    case xform::shear_x:           return xform::scale_then_rotate;
    case xform::scale_then_rotate: return xform::rotate_then_scale;
    case xform::rotate_then_scale: return xform::identity;
    }
    return xform::identity;
}

/// The fixed scale used by both composition modes, so the only difference
/// between them is the ORDER.
constexpr engine::mat2 k_compose_scale = engine::scale(1.6f, 0.7f);

[[nodiscard]] engine::mat2 build(xform x, float t)
{
    switch (x)
    {
    case xform::identity:          return engine::mat2::identity();
    case xform::rotate:            return engine::rotation(t);
    case xform::scale_xy:          return engine::scale(1.0f + t, 1.0f - 0.6f * t);
    case xform::shear_x:           return engine::shear(t, 0.0f);

    // Read these the way operator* is documented: the RIGHT-hand factor is
    // applied first. Both modes use the same two ingredients and differ only in
    // which one goes first — which is the whole point of showing them.
    case xform::scale_then_rotate: return engine::rotation(t) * k_compose_scale;
    case xform::rotate_then_scale: return k_compose_scale * engine::rotation(t);
    }
    return engine::mat2::identity();
}

/// The other order, for the modes where order is the subject. Returns identity
/// when the mode is not a composition, and the caller then draws no ghost.
[[nodiscard]] engine::mat2 build_reversed(xform x, float t)
{
    if (x == xform::scale_then_rotate) { return k_compose_scale * engine::rotation(t); }
    if (x == xform::rotate_then_scale) { return engine::rotation(t) * k_compose_scale; }
    return engine::mat2::identity();
}

constexpr engine::vec2 k_basis_origin{100.0f, 118.0f};
constexpr float k_basis_unit = 44.0f;   ///< framebuffer pixels per unit of maths space

/// Maths space -> framebuffer pixels.
///
/// The y NEGATION is the whole of it, and it is worth naming because it is the
/// first appearance of something Lesson 2.11 will formalise. mat2 works in the
/// mathematical convention, +y up, where rotation(t) turns counter-clockwise.
/// The framebuffer has +y DOWN. Nothing about the matrix changes; the picture is
/// flipped at the moment of drawing, exactly here, in one place.
[[nodiscard]] engine::vec2 to_screen(engine::vec2 v)
{
    return {k_basis_origin.x + v.x * k_basis_unit,
            k_basis_origin.y - v.y * k_basis_unit};
}

void line_maths(engine::framebuffer& fb, engine::vec2 a, engine::vec2 b, Uint32 colour)
{
    const engine::vec2 pa = to_screen(a);
    const engine::vec2 pb = to_screen(b);
    engine::draw_line(fb,
        static_cast<int>(std::lround(pa.x)), static_cast<int>(std::lround(pa.y)),
        static_cast<int>(std::lround(pb.x)), static_cast<int>(std::lround(pb.y)), colour);
}

/// Draw the image of the integer lattice under `m`.
///
/// This is the picture that makes "linear" mean something you can see. The
/// original grid is square; the transformed one is generally not — but its lines
/// are still straight, still parallel within each family, and still evenly
/// spaced. Any transformation that bent a line or bunched the spacing would not
/// be a matrix, and no 2x2 can produce one.
void draw_lattice(engine::framebuffer& fb, const engine::mat2& m)
{
    constexpr int k_reach = 3;
    const Uint32 faint = engine::pack_argb(44, 48, 66);
    const Uint32 centre = engine::pack_argb(74, 82, 112);

    for (int i = -k_reach; i <= k_reach; ++i)
    {
        // i == 0 is drawn too, and slightly brighter. Leaving it out was a real
        // bug in the first version of this demo: without the images of the lines
        // x = 0 and y = 0, the cell containing the origin has no left or bottom
        // edge, so it reads as twice the size it is and the unit square below
        // looks as though it does not line up with the grid. The unit square IS
        // one cell of this grid — that is the picture.
        const Uint32 colour = (i == 0) ? centre : faint;
        const float f = static_cast<float>(i);
        const float e = static_cast<float>(k_reach);
        line_maths(fb, m * engine::vec2{f, -e}, m * engine::vec2{f, e}, colour);
        line_maths(fb, m * engine::vec2{-e, f}, m * engine::vec2{e, f}, colour);
    }
}

/// An arrow from the origin, with a small head. Used for the two basis vectors.
void draw_basis_arrow(engine::framebuffer& fb, engine::vec2 tip, Uint32 colour)
{
    line_maths(fb, {0.0f, 0.0f}, tip, colour);

    // The head: two short lines swept back from the tip. Built with mat2 itself,
    // because a rotation is exactly the tool for "the same arrow, turned a bit".
    const float len = engine::length(tip);
    if (len < 0.12f) { return; }          // too short to draw a head on

    const engine::vec2 back = tip * (-0.22f / len);
    line_maths(fb, tip, tip + engine::rotation(0.5f) * back, colour);
    line_maths(fb, tip, tip + engine::rotation(-0.5f) * back, colour);
}

/// The transformed unit square, filled — the shape whose area IS the determinant.
void fill_unit_square(engine::framebuffer& fb, const engine::mat2& m, Uint32 colour)
{
    const engine::vec2 a = to_screen({0.0f, 0.0f});
    const engine::vec2 b = to_screen(m.c0);
    const engine::vec2 c = to_screen(m.c0 + m.c1);
    const engine::vec2 d = to_screen(m.c1);
    auto ix = [](float f) { return static_cast<int>(std::lround(f)); };

    engine::fill_triangle(fb, ix(a.x), ix(a.y), ix(b.x), ix(b.y), ix(c.x), ix(c.y), colour);
    engine::fill_triangle(fb, ix(a.x), ix(a.y), ix(c.x), ix(c.y), ix(d.x), ix(d.y), colour);
}

/// The same square as an outline — used to ghost in the other composition order.
void outline_unit_square(engine::framebuffer& fb, const engine::mat2& m, Uint32 colour)
{
    line_maths(fb, {0.0f, 0.0f}, m.c0, colour);
    line_maths(fb, m.c0, m.c0 + m.c1, colour);
    line_maths(fb, m.c0 + m.c1, m.c1, colour);
    line_maths(fb, m.c1, {0.0f, 0.0f}, colour);
}

/// An asymmetric glyph, so a reflection is impossible to miss.
///
/// A blob would look identical mirrored; an F does not. Every point of it is put
/// through the same matrix, which is the claim being demonstrated: ONE
/// transformation applies to arbitrarily many vertices, and that is precisely
/// what the per-vertex trigonometry of Lessons 2.2-2.4 could not do.
void draw_glyph(engine::framebuffer& fb, const engine::mat2& m, Uint32 colour)
{
    static constexpr engine::vec2 k_f[] = {
        {0.18f, 0.18f}, {0.18f, 0.82f}, {0.68f, 0.82f}, {0.68f, 0.66f},
        {0.36f, 0.66f}, {0.36f, 0.55f}, {0.60f, 0.55f}, {0.60f, 0.39f},
        {0.36f, 0.39f}, {0.36f, 0.18f},
    };
    constexpr int n = static_cast<int>(std::size(k_f));

    for (int i = 0; i < n; ++i)
    {
        line_maths(fb, m * k_f[i], m * k_f[(i + 1) % n], colour);
    }
}

/// Rasterise the transformed unit square off-screen and count the lit pixels.
///
/// The determinant claims to be an area factor. This checks the claim with the
/// rasterizer we built in Lesson 2.2 rather than believing it: fill the shape,
/// count what got covered, compare against `unit^2 * |det|`. Verified in the
/// harness at better than 0.3% for every transformation in this demo.
[[nodiscard]] int measure_area_px(const engine::mat2& m)
{
    engine::framebuffer scratch(k_fb_width, k_fb_height);
    scratch.clear(0u);
    fill_unit_square(scratch, m, 0xFFFFFFFFu);

    int lit = 0;
    for (int y = 0; y < k_fb_height; ++y)
    {
        for (int x = 0; x < k_fb_width; ++x)
        {
            if (scratch.pixel_at(x, y) != 0u) { ++lit; }
        }
    }
    return lit;
}

// ===========================================================================
// Lesson 2.1 — lines
// ===========================================================================

using line_fn = void (*)(engine::framebuffer&, int, int, int, int, Uint32);

constexpr int k_spokes = 32;
constexpr engine::vec2 k_fan_centre{88.0f, 88.0f};
constexpr float k_fan_radius = 74.0f;

/// A fan of spokes crosses every one of the eight octants, so a routine that is
/// wrong in any of them is wrong on screen rather than wrong in theory.
void draw_fan(engine::framebuffer& fb, line_fn draw, float phase)
{
    for (int i = 0; i < k_spokes; ++i)
    {
        const float angle = phase + static_cast<float>(i) * 6.28318531f
                                    / static_cast<float>(k_spokes);
        const engine::vec2 tip{k_fan_centre.x + std::cos(angle) * k_fan_radius,
                               k_fan_centre.y + std::sin(angle) * k_fan_radius};

        const bool steep = std::fabs(std::sin(angle)) > std::fabs(std::cos(angle));
        const Uint32 colour = steep ? engine::pack_argb(236, 122, 92)
                                    : engine::pack_argb(122, 196, 152);

        draw(fb, static_cast<int>(k_fan_centre.x), static_cast<int>(k_fan_centre.y),
             static_cast<int>(tip.x), static_cast<int>(tip.y), colour);
    }
}

// ===========================================================================
// Presentation
// ===========================================================================
//
// LESSON 5.2 EMPTIED THIS SECTION. `upload()` lived here — thirty lines that
// locked the streaming texture and copied the framebuffer into it row by row,
// because the pitch SDL hands back may exceed width*4 and one big memcpy would
// shear the image on machines you do not own (Lesson 1.5 §4.3). It was correct,
// it was subtle, and it was duplicated verbatim in hello_cube.
//
// It is `engine::platform::blit_framebuffer()` now. The heading stays as a
// marker: this is where presentation used to be a demo's problem.

// ===========================================================================
// Lesson 3.10 — the frame budget, on screen
// ===========================================================================

/// One colour per zone, plus one for the unaccounted remainder.
///
/// The remainder is drawn in a **desaturated grey**, deliberately: it is not a
/// phase, it is the part of the frame nobody is measuring, and giving it a
/// cheerful colour of its own would suggest otherwise.
struct zone_colour { Uint8 r, g, b; };

[[nodiscard]] zone_colour colour_of(engine::zone z)
{
    switch (z)
    {
    case engine::zone::build:   return {120, 200, 140};
    case engine::zone::collect: return {235, 200, 100};
    case engine::zone::sort:    return {220, 140, 200};
    case engine::zone::fill:    return {235, 120, 110};
    case engine::zone::overlay: return {120, 180, 235};
    case engine::zone::present: return {150, 150, 220};
    case engine::zone::count:   break;
    }
    return {110, 112, 128};
}

/// Draw the frame budget: a stacked bar, then the numbers behind it.
///
/// **A stacked bar rather than one bar per zone**, and the choice is the whole
/// design. Separate bars answer "how big is fill?"; a stacked bar answers "what is
/// a frame made of?", which is the question a budget exists for — and it makes the
/// unaccounted remainder impossible to overlook, because it is the gap at the end
/// rather than a row you can skip reading.
///
/// Coordinates are in the HUD's 2x text space, so they line up with
/// `SDL_RenderDebugText` without any conversion.
void draw_budget(SDL_Renderer* r, const engine::profiler& prof, float wall_ns,
                 int covered_px, double fill_ns_per_px,
                 engine::encode_mode encode, int encode_wrong,
                 engine::traversal walk, const engine::quad_stats& quads,
                 float x, float y)
{
    const double total = static_cast<double>(prof.median_frame_ns());
    if (total <= 0.0) { return; }

    constexpr float k_bar_w = 400.0f;
    constexpr float k_bar_h = 8.0f;
    constexpr float k_col_w = 268.0f;   ///< two columns of zone rows, to stay short
    constexpr int k_rows = 4;

    // A backing panel, because this is drawn OVER the render. That is not a
    // compromise for want of screen space — it is what a profiler HUD is: it
    // covers the thing it is measuring, which is why it lives on a key ([3]) and
    // why its own cost is charged to `overlay` rather than being free.
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 10, 12, 18, 225);
    const SDL_FRect panel{x - 5.0f, y - 5.0f, 550.0f, 104.0f};
    SDL_RenderFillRect(r, &panel);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    // ENGINE TIME AND WALL TIME, SIDE BY SIDE, and the gap between them is the
    // point: with vsync on, the wall clock reads 16.7 ms no matter what the
    // renderer does, so a frame-rate counter cannot tell you that you have made
    // anything faster. That is why this panel exists and the fps counter does not
    // replace it.
    SDL_SetRenderDrawColor(r, 210, 212, 220, 255);
    SDL_RenderDebugTextFormat(r, x, y,
        "FRAME BUDGET  engine %8.2f us   wall %8.2f us   %5.1f%% of 16.7 ms",
        total / 1000.0, static_cast<double>(wall_ns) / 1000.0,
        100.0 * total / 16666667.0);

    // The stacked bar. Zones in enum order, so it reads left-to-right as the
    // order the work happens in.
    float cursor = x;
    for (int i = 0; i < engine::k_zone_count; ++i)
    {
        const auto z = static_cast<engine::zone>(i);
        const float w = static_cast<float>(k_bar_w * static_cast<double>(prof.median_ns(z)) / total);
        const zone_colour c = colour_of(z);
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
        const SDL_FRect seg{cursor, y + 11.0f, w, k_bar_h};
        SDL_RenderFillRect(r, &seg);
        cursor += w;
    }
    // Whatever is left is the remainder, drawn to the end of the bar so the bar
    // is always exactly full. A budget that does not add up to the frame is not
    // a budget.
    SDL_SetRenderDrawColor(r, 110, 112, 128, 255);
    const SDL_FRect rest{cursor, y + 11.0f, std::max(0.0f, x + k_bar_w - cursor), k_bar_h};
    SDL_RenderFillRect(r, &rest);

    // The numbers, in two columns so the panel stays short enough to sit between
    // the lesson's own HUD lines. `other` is the seventh entry and gets the same
    // treatment as a real zone, which is the whole reason it is displayed.
    const auto row_at = [&](int i, const char* label, Uint64 ns, zone_colour c,
                            const char* note) {
        const float rx = x + static_cast<float>(i / k_rows) * k_col_w;
        const float ry = y + 23.0f + static_cast<float>(i % k_rows) * 11.0f;
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
        const SDL_FRect swatch{rx, ry + 1.0f, 6.0f, 6.0f};
        SDL_RenderFillRect(r, &swatch);
        SDL_RenderDebugTextFormat(r, rx + 11.0f, ry, "%-8s %8.2f us %5.1f%% %s",
                                  label, static_cast<double>(ns) / 1000.0,
                                  100.0 * static_cast<double>(ns) / total, note);
    };

    for (int i = 0; i < engine::k_zone_count; ++i)
    {
        const auto z = static_cast<engine::zone>(i);
        row_at(i, engine::zone_name(z), prof.median_ns(z), colour_of(z), "");
    }
    row_at(engine::k_zone_count, "other", prof.other_ns(), {110, 112, 128},
           "<- unmeasured");

    // THE UNIT COST, and the optimisation, on one line. Total microseconds are a
    // fact about this machine at this resolution; nanoseconds per pixel is a fact
    // about the fill loop, and it is the one that survives being quoted elsewhere.
    // The pixel count beside it is what a speedup costs — a speedup quoted without
    // its error is half a claim.
    const bool fast = (encode == engine::encode_mode::fast);
    SDL_SetRenderDrawColor(r, 235, 120, 110, 255);
    SDL_RenderDebugTextFormat(r, x, y + 74.0f,
                              "fill %5.2f ns/px over %6d px", fill_ns_per_px, covered_px);
    SDL_SetRenderDrawColor(r, fast ? 140 : 210, fast ? 210 : 212, fast ? 150 : 220, 255);
    SDL_RenderDebugTextFormat(r, x + k_col_w, y + 74.0f,
                              "[4] encode %-5s  %5d px differ by 1 code",
                              fast ? "FAST" : "exact", encode_wrong);

    // ---- Lesson 4.1 --------------------------------------------------------
    //
    // Only under a quad traversal. Under `scanline` every lane is covered by
    // construction, so a row reading "100% efficient" would be announcing that
    // the feature is switched off, dressed up as a result.
    if (walk != engine::traversal::scanline && quads.shaded > 0)
    {
        SDL_SetRenderDrawColor(r, 120, 180, 235, 255);
        SDL_RenderDebugTextFormat(r, x, y + 86.0f,
            "[5] 2x2 QUADS%s  %ld lanes shaded, %ld wasted on helpers -> %.1f%% efficient",
            walk == engine::traversal::quad_debug ? " (helpers in magenta)" : "",
            quads.shaded, quads.helpers, 100.0 * quads.efficiency());
    }

    if (prof.zones_overlapped())
    {
        SDL_SetRenderDrawColor(r, 240, 120, 110, 255);
        SDL_RenderDebugText(r, x, y + 98.0f,
                            "ZONES OVERLAPPED - two timers alive at once");
    }
}

// ============================================================================
// LESSON 4.2 — THE GPU PROBE
// ============================================================================
//
// A second program inside this executable, reached with `engine --gpu`, and the
// reason it is separate rather than a key on the demo is a hard constraint
// rather than a preference: **a window can be claimed by an SDL_GPU device or
// driven by an SDL_Renderer, never both**. Everything above draws its HUD with
// SDL_RenderDebugText, so taking the window for the GPU would delete the HUD from
// every demo in Modules 1 to 3. Deleting working functionality to make room for
// new functionality is the one thing this codebase has refused to do since 2.12.
//
// So: the window is created first, and whoever claims it does so before any
// renderer exists. From Lesson 4.8 the GPU path becomes the default and this
// branch inverts; until then the software demo is what `engine` means.
//
// WHAT IT DRAWS. The same software rasterizer, into the same 320x180 framebuffer,
// carried to the display by SDL_GPU instead of SDL_Renderer. The picture is
// deliberately unimpressive — a checkerboard, a spinning vertex-coloured triangle
// from Lesson 2.4, and a live graph. Nothing here is a GPU *drawing* anything;
// there are no shaders in this lesson, and a triangle drawn by hardware is
// Lesson 4.4.
//
// WHAT THE GRAPH IS FOR. There is no text in this mode, because text needs a
// font and a shader, and both are later. The graph is the HUD: each column is one
// frame, and the three stacked bands are the three places a frame's time actually
// goes. Watching the orange band appear the instant you press [4] is the whole
// lesson in one gesture.

/// One vertex, in the layout `shaders/triangle.vert.hlsl` declares.
///
/// THIS STRUCT AND THAT SHADER ARE ONE DECLARATION SPLIT IN TWO, and nothing
/// checks that the halves agree — except, since Lesson 4.5, `check_layout`, which
/// reads the shader's half out of the reflection JSON and compares.
///
/// **LESSON 4.5 SHRANK THIS STRUCT FROM 28 BYTES TO 16, AND CHANGED NOTHING ELSE.**
/// The colour was four floats; it is now four bytes, declared to the pipeline as
/// `UBYTE4_NORM`. `shaders/triangle.vert.hlsl` was not touched: it still says
/// `float4 colour : TEXCOORD1`, and it still receives a `float4`, because the
/// hardware divides each byte by 255 on the way in at no cost. That is the whole
/// distinction between what a buffer STORES and what a shader RECEIVES, and it is
/// worth 43% of this vertex.
struct gpu_vertex
{
    float x, y, z;              ///< CLIP SPACE already: triangle.vert applies no matrix.
    Uint8 r, g, b, a;           ///< 0..255, arriving in the shader as 0..1 floats
};

/// One instance of the mesh: where it goes, how big, which way up, what colour.
///
/// Lesson 4.5. **28 bytes**, rewritten every frame, against 39,200 bytes of torus
/// that is uploaded once and never touched again. That ratio — 0.07% of the data
/// moving per frame — is the argument for instancing stated as a number.
///
/// A real engine sends a MATRIX here (or a 4x3, or a quaternion and a scale). We
/// send a rotation about one axis as its cosine and sine, because a matrix has to
/// be built somewhere and the somewhere is a uniform buffer, which is Lesson 4.6.
/// The shape of the idea is identical; only the amount of it is smaller.
struct gpu_instance
{
    float ox, oy, oz;   ///< world offset
    float scale;        ///< uniform — which is why the shader may reuse the rotation
    float spin_c;       ///< cos of the rotation about x
    float spin_s;       ///< sin of it
    Uint8 r, g, b, a;   ///< tint, four bytes, arriving as a float4
};

static_assert(sizeof(gpu_instance) == 28, "the pitch declared for slot 1 is this size");

/// The first triangle the GPU will ever draw for us.
///
/// Clip space, so x and y run -1..+1 with +Y UP (conventions §4), and z = 0 is
/// the near plane. Placed on the RIGHT half of the screen on purpose: the
/// software rasterizer's triangle is on the left, and the point of this lesson is
/// to see both at once.
///
/// COUNTER-CLOCKWISE as seen on screen, because the pipeline says CCW is
/// front-facing and culls the back. Reverse any two of these and the triangle
/// vanishes — which is Lesson 3.4's winding test, now enforced by hardware.
constexpr gpu_vertex k_triangle[3] = {
    // The same three colours Lesson 4.4 drew, quantised to eight bits per channel:
    // 0.92 * 255 = 234.6 -> 235, and so on. The picture differs from the float
    // version by at most one code out of 255, which `verify_45` §G measures rather
    // than assumes.
    {  0.15f, -0.55f, 0.0f,  235,  89,  79, 255 },   // bottom left,  red
    {  0.85f, -0.55f, 0.0f,   89, 209, 120, 255 },   // bottom right, green
    {  0.50f,  0.55f, 0.0f,   94, 150, 235, 255 },   // top,          blue
};

// ---- Lesson 4.5: the scene the mesh is drawn in ---------------------------
//
// Seven places: one at the origin and six on a ring around it. The camera in
// `shaders/mesh.vert.hlsl` is frozen looking at this arrangement, so the numbers
// here and the constants there are one decision written in two files — the same
// split the vertex layout has, and worth noticing for the same reason.
constexpr int k_max_instances = 7;
constexpr float k_ring_radius = 2.25f;
constexpr float k_instance_scale = 0.62f;

/// Tints for the seven, walked around the hue circle so that neighbouring rings
/// are distinguishable at a glance. Stored as bytes because that is what the
/// vertex layout wants (see `gpu_instance`).
constexpr Uint8 k_instance_tint[k_max_instances][3] = {
    {236, 214, 168},   // centre — pale, so it reads as the odd one out
    {232, 118, 102},
    {236, 176,  86},
    {154, 208, 122},
    { 96, 198, 190},
    {112, 156, 232},
    {186, 134, 226},
};

/// Fill `out` with `count` instances at time `t`.
///
/// The ONLY per-frame work the mesh path does on the CPU, and it is 28 bytes per
/// instance of arithmetic. Everything the shape of the torus is made of was
/// uploaded once, at startup, and is not read by this function at all.
void place_instances(gpu_instance (&out)[k_max_instances], int count, float t)
{
    const float two_pi = 6.28318531f;

    for (int i = 0; i < count && i < k_max_instances; ++i)
    {
        // Index 0 sits at the origin; 1..6 walk the ring. Fixing the ring position
        // by the instance's index rather than by its ordinal means the arrangement
        // does not rearrange itself when the count changes, which makes [9] a
        // comparison rather than a shuffle.
        const float a = two_pi * static_cast<float>(i - 1) / 6.0f;
        const float radius = (i == 0) ? 0.0f : k_ring_radius;

        // Each spins at its own rate, so a stopped frame still shows seven
        // different orientations — which is what makes it obvious at a glance that
        // one buffer of geometry is being drawn seven times rather than seven
        // buffers being drawn once.
        const float rate = 0.55f + 0.13f * static_cast<float>(i);
        const float angle = t * rate + 0.9f * static_cast<float>(i);

        out[i].ox = radius * std::cos(a);
        out[i].oy = 0.0f;
        out[i].oz = radius * std::sin(a);
        out[i].scale = k_instance_scale;
        out[i].spin_c = std::cos(angle);
        out[i].spin_s = std::sin(angle);
        out[i].r = k_instance_tint[i][0];
        out[i].g = k_instance_tint[i][1];
        out[i].b = k_instance_tint[i][2];
        out[i].a = 255;
    }
}

/// Describe `gpu_instance` to a pipeline — one buffer at slot 1, three attributes.
///
/// The twin of `gpu_mesh::describe`, and the only line that differs in kind is the
/// first: `instance_buffer` rather than `vertex_buffer`. Locations 3, 4 and 5
/// continue where the mesh's 0, 1 and 2 stopped, because **locations are numbered
/// across the whole pipeline, not per buffer** — a shader has one input list, and
/// which slot each entry is fetched from is exactly what these calls decide.
///
/// **Lesson 6.7 made that a decision rather than an accident.** `gpu_vertex_pnu`
/// grew a tangent, so `describe` can now emit four attributes and push these to
/// 4, 5 and 6. It does not, because this pipeline's shader (`mesh.vert.hlsl`)
/// does not read a tangent and `describe` takes a `with_tangent` flag — a vertex
/// layout is per-pipeline state, and fetching an attribute nobody reads is waste.
/// 4.5's `check_layout` is what made the collision a failing assertion rather
/// than `placement` silently reading the tangent's bytes.
engine::pipeline_desc& describe_instances(engine::pipeline_desc& desc, Uint32 slot = 1)
{
    // Location 3 is a FLOAT4 covering FOUR fields — `ox, oy, oz, scale` — because
    // they are four contiguous floats and the shader wants them as one `float4`.
    // That is a deliberate pack, not a coincidence: three floats and a lone float
    // would be two attributes and two fetches for the same eight cache lines.
    desc.instance_buffer(slot, static_cast<Uint32>(sizeof(gpu_instance)))
        .attribute(3, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                   static_cast<Uint32>(offsetof(gpu_instance, ox)))
        .attribute(4, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                   static_cast<Uint32>(offsetof(gpu_instance, spin_c)))
        .attribute(5, slot, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
                   static_cast<Uint32>(offsetof(gpu_instance, r)));
    return desc;
}

/// The GPU probe's camera and light, and the keys that move them.
///
/// Lesson 4.6. Everything in here was a `static const` inside
/// `shaders/mesh.vert.hlsl` an hour ago. Moving it to the CPU is what a uniform
/// buffer buys: not new capability in the shader, but the ability for anything
/// outside the shader to have an opinion.
struct probe_view
{
    demo::orbit_camera camera{{0.0f, 0.0f, 0.0f}, 5.8f, 0.55f, 0.42f};

    /// Where the lamp is, as an angle rather than a vector, so that one number
    /// drives it and the vector is derived at the point of use.
    float light_azimuth = 0.9f;
    bool light_orbits = false;

    // ---- Lesson 4.7 --------------------------------------------------------
    bool show_texture = true;     ///< [T] — false brings 4.5's diagnostic grid back
    bool depth_test = true;       ///< [C] — false is what every lesson before this looked like
    bool smooth_texture = true;   ///< [F] — linear or nearest, the same enum 3.9 defined
    float uv_repeat = 2.0f;       ///< [R] — how many times the image tiles across a torus

    /// Field of view, near and far — the same numbers Module 3's scene camera
    /// uses, so the two pictures are comparable.
    static constexpr float k_fovy = 55.0f * 3.14159265358979f / 180.0f;
    static constexpr float k_near = 0.3f;
    static constexpr float k_far = 100.0f;

    /// One matrix per frame, built from two that have existed since Lessons 2.9
    /// and 2.10.
    ///
    /// **`projection * view`, in that order**, because our convention is column
    /// vectors — `v' = M*v`, so `A*B` applies B first, and the view has to happen
    /// before the projection. Reverse them and the scene is not subtly wrong, it
    /// is unrecognisable.
    [[nodiscard]] engine::mat4 clip_from_world(float aspect) const
    {
        return engine::perspective(k_fovy, aspect, k_near, k_far) * camera.view();
    }

    /// Toward the lamp, unit length — Lesson 3.6's convention, so the dot product
    /// with a normal is directly the cosine.
    [[nodiscard]] engine::vec3 to_light() const
    {
        constexpr float elevation = 0.95f;
        const float ce = std::cos(elevation);
        return engine::normalised(engine::vec3{ce * std::sin(light_azimuth),
                                               std::sin(elevation),
                                               ce * std::cos(light_azimuth)});
    }

    /// Held keys, applied per second rather than per frame — Lesson 1.3's rule,
    /// which is why this takes `dt` and multiplies by it.
    void update(const engine::input& in, float dt)
    {
        constexpr float k_turn = 1.6f;    // radians per second
        constexpr float k_zoom = 4.0f;    // units per second

        if (in.key_down(SDL_SCANCODE_LEFT)  || in.key_down(SDL_SCANCODE_A)) { camera.azimuth -= k_turn * dt; }
        if (in.key_down(SDL_SCANCODE_RIGHT) || in.key_down(SDL_SCANCODE_D)) { camera.azimuth += k_turn * dt; }
        if (in.key_down(SDL_SCANCODE_UP)    || in.key_down(SDL_SCANCODE_W)) { camera.elevation += k_turn * dt; }
        if (in.key_down(SDL_SCANCODE_DOWN)  || in.key_down(SDL_SCANCODE_S)) { camera.elevation -= k_turn * dt; }
        if (in.key_down(SDL_SCANCODE_Z)) { camera.radius -= k_zoom * dt; }
        if (in.key_down(SDL_SCANCODE_X)) { camera.radius += k_zoom * dt; }

        // CLAMPED, and the elevation clamp is not cosmetic: `look_at` builds its
        // right vector from `cross(up, backward)`, which is the zero vector when
        // the camera looks straight up. Lesson 2.9 named that degeneracy; this is
        // the line that keeps the demo out of it.
        constexpr float k_limit = 1.5f;   // just under pi/2
        camera.elevation = std::clamp(camera.elevation, -k_limit, k_limit);
        camera.radius = std::clamp(camera.radius, 2.2f, 24.0f);

        if (light_orbits) { light_azimuth += 0.7f * dt; }
    }
};

/// What Lesson 4.3's shader loading produced, in the form the picture needs.
///
/// The probe has no text — that needs a font and a shader, and 4.3 has only just
/// produced the shaders — so "did the toolchain work" has to be a SHAPE. One
/// square per shader is the smallest honest answer.
struct shader_status
{
    static constexpr int k_max = 8;

    int count = 0;
    bool loaded[k_max] = {};
    const char* names[k_max] = {};
};

/// One frame's worth of timings, in milliseconds. A ring of these is the graph.
struct probe_sample
{
    float draw = 0.0f;      ///< the software rasterizer, on the CPU
    float record = 0.0f;    ///< building the command buffer: upload, pass, blit
    float acquire = 0.0f;   ///< blocked in WaitAndAcquireGPUSwapchainTexture
    float fence = 0.0f;     ///< blocked in WaitForGPUFences, when [4] is on
    float frame = 0.0f;     ///< wall clock, for scale
};

/// Milliseconds between two `SDL_GetPerformanceCounter` readings.
[[nodiscard]] float ticks_to_ms(Uint64 a, Uint64 b)
{
    static const double period =
        1000.0 / static_cast<double>(SDL_GetPerformanceFrequency());
    return static_cast<float>(static_cast<double>(b - a) * period);
}

/// The probe's picture: a checkerboard, a spinning triangle, and the graph.
///
/// Everything in it is Module 1-3 machinery, called from a Module 4 program, and
/// that is the point being made — the rasterizer did not change, only who carries
/// its output to the screen.
void draw_probe_scene(engine::framebuffer& fb, float t,
                      const probe_sample* ring, int ring_count, int ring_head,
                      bool fence_each_frame, const shader_status& shaders)
{
    // ---- Background --------------------------------------------------------
    fb.clear(engine::pack_argb(10, 12, 18));
    for (int y = 0; y < fb.height(); y += 16)
    {
        for (int x = 0; x < fb.width(); x += 16)
        {
            if (((x / 16) + (y / 16)) % 2 == 0) { continue; }
            fb.fill_rect(x, y, 16, 16, engine::pack_argb(18, 21, 30));
        }
    }

    // ---- A spinning triangle, Lesson 2.4's interpolation --------------------
    const float cx = static_cast<float>(fb.width()) * 0.5f;
    const float cy = 58.0f;
    const float radius = 42.0f;
    const float two_pi = 6.28318531f;

    engine::vertex v[3];
    const Uint32 corner_colour[3] = {
        engine::pack_argb(235, 90, 80),
        engine::pack_argb(90, 210, 130),
        engine::pack_argb(95, 150, 240)
    };
    for (int i = 0; i < 3; ++i)
    {
        const float a = t + two_pi * static_cast<float>(i) / 3.0f;
        v[i].x = static_cast<int>(cx + radius * std::cos(a));
        v[i].y = static_cast<int>(cy + radius * std::sin(a));
        v[i].colour = corner_colour[i];
    }

    engine::fill_style style;
    style.shade = engine::shading::vertex_colour;
    engine::fill_triangle(fb, v[0], v[1], v[2], style);

    // ---- Lesson 4.3: one square per shader ---------------------------------
    //
    // Green means SDL_CreateGPUShader returned an object: the HLSL compiled, the
    // right binary format was chosen for this device, the entry point was named
    // correctly, and the resource counts came out of the reflection file. Red
    // means one of those failed and the log says which.
    //
    // Nothing is DRAWN with them. A shader that exists and a shader that draws
    // are two different achievements, and this lesson claims only the first.
    for (int i = 0; i < shaders.count; ++i)
    {
        const int x = 8 + i * 14;
        const Uint32 fill = shaders.loaded[i] ? engine::pack_argb(90, 200, 120)
                                              : engine::pack_argb(235, 90, 80);
        fb.fill_rect(x, 8, 10, 10, fill);
        fb.fill_rect(x + 2, 10, 6, 6, engine::pack_argb(10, 12, 18));
        fb.fill_rect(x + 3, 11, 4, 4, fill);
    }

    // ---- The graph ---------------------------------------------------------
    //
    // Bottom 64 rows. One column per frame, newest at the right, stacked bands:
    //
    //   pale grey   the whole frame, wall clock — the envelope everything fits in
    //   green       CPU: the software rasterizer
    //   cyan        CPU: recording commands (upload + render pass + blit)
    //   blue        blocked waiting for a swapchain image — this is vsync
    //   orange      blocked waiting on a fence — this is [4], and it is a choice
    //
    // Full height is 20 ms, with a line at the 60 Hz budget of 16.667 ms.
    const int graph_h = 64;
    const int graph_y = fb.height() - graph_h;
    const float ms_full = 20.0f;
    const float px_per_ms = static_cast<float>(graph_h) / ms_full;

    fb.fill_rect(0, graph_y, fb.width(), graph_h, engine::pack_argb(6, 7, 11));

    const auto bar = [&](int col, int from_ms_px, int height_px, Uint32 colour)
    {
        if (height_px <= 0) { return; }
        fb.fill_rect(col, graph_y + graph_h - from_ms_px - height_px, 2, height_px, colour);
    };

    const int columns = fb.width() / 2;
    for (int c = 0; c < columns; ++c)
    {
        // Oldest on the left. `ring_head` is where the NEXT sample will be
        // written, so head-1 is the newest.
        const int age = columns - 1 - c;
        const int idx = ((ring_head - 1 - age) % ring_count + ring_count) % ring_count;
        const probe_sample& s = ring[idx];
        if (s.frame <= 0.0f) { continue; }

        const int col = c * 2;
        const auto px = [&](float ms) { return static_cast<int>(ms * px_per_ms); };

        bar(col, 0, px(s.frame), engine::pack_argb(30, 33, 42));

        int base = 0;
        bar(col, base, px(s.draw), engine::pack_argb(90, 200, 120));
        base += px(s.draw);
        bar(col, base, px(s.record), engine::pack_argb(90, 205, 215));
        base += px(s.record);
        bar(col, base, px(s.acquire), engine::pack_argb(80, 130, 230));
        base += px(s.acquire);
        bar(col, base, px(s.fence), engine::pack_argb(240, 150, 70));
    }

    // The 60 Hz budget line, and a brighter one when [4] is on so the mode is
    // visible in a screenshot without the log beside it.
    const int budget_row = graph_y + graph_h - static_cast<int>(16.667f * px_per_ms);
    for (int x = 0; x < fb.width(); x += 4)
    {
        fb.put_pixel(x, budget_row, engine::pack_argb(120, 124, 140));
    }
    if (fence_each_frame)
    {
        fb.fill_rect(0, graph_y, fb.width(), 1, engine::pack_argb(240, 150, 70));
    }
}

/// Average a ring, ignoring the samples never written.
[[nodiscard]] probe_sample average(const probe_sample* ring, int count)
{
    probe_sample sum;
    int n = 0;
    for (int i = 0; i < count; ++i)
    {
        if (ring[i].frame <= 0.0f) { continue; }
        sum.draw += ring[i].draw;
        sum.record += ring[i].record;
        sum.acquire += ring[i].acquire;
        sum.fence += ring[i].fence;
        sum.frame += ring[i].frame;
        ++n;
    }
    if (n == 0) { return sum; }
    const float inv = 1.0f / static_cast<float>(n);
    sum.draw *= inv;
    sum.record *= inv;
    sum.acquire *= inv;
    sum.fence *= inv;
    sum.frame *= inv;
    return sum;
}

/// Lesson 4.2's runnable program. Returns a process exit code.
int run_gpu_probe(SDL_Window* window)
{
    SDL_SetWindowTitle(window, "The SDL_GPU Mental Model - Lesson 4.2");

    // ---- The device, and the claim -----------------------------------------
    //
    // debug = true. The validation layer costs real time and catches API misuse
    // that would otherwise be a black window with no message at all, which is the
    // worst failure mode in graphics programming. Lesson 4.9 turns it off to
    // measure; until then, leave it on and read what it says.
    engine::gpu_device gpu;
    const engine::gpu_report rep = gpu.create(window, true);
    if (!rep.ok())
    {
        SDL_Log("GPU probe cannot start: %s", engine::name_of(rep.status));
        SDL_Log("Run without --gpu for the software screen.");
        return 1;
    }
    gpu.log_report();

    // ---- The framebuffer, and its device-side mirror ------------------------
    engine::framebuffer fb(k_fb_width, k_fb_height);

    engine::gpu_present_target present;
    if (!present.create(gpu, fb.width(), fb.height()))
    {
        SDL_Log("GPU probe cannot start: the present target could not be created.");
        return 1;
    }
    SDL_Log("  present target  : %dx%d as %s",
            present.width(), present.height(), engine::name_of(present.format()));

    // ---- Lesson 4.3: the shaders --------------------------------------------
    //
    // Loaded, reported, and held for the rest of the run — and NOT drawn with.
    // Lesson 4.4 binds them into a pipeline; the achievement here is that four
    // HLSL files became objects on the device, in a binary format chosen from
    // what the device said it accepts, with resource counts read from the
    // compiler's own reflection rather than guessed.
    struct shader_load
    {
        const char* name;
        engine::shader_stage stage;
    };
    const shader_load wanted[] = {
        {"triangle.vert", engine::shader_stage::vertex},
        {"triangle.frag", engine::shader_stage::fragment},
        {"textured.vert", engine::shader_stage::vertex},
        {"textured.frag", engine::shader_stage::fragment},
        // Lesson 4.5. Indices 4 and 5, and the ORDER MATTERS because the pipeline
        // built below indexes this array — which is exactly the kind of implicit
        // coupling Module 5's asset system replaces with a handle.
        {"mesh.vert", engine::shader_stage::vertex},
        {"mesh.frag", engine::shader_stage::fragment},
    };

    engine::gpu_shader shaders[shader_status::k_max];
    shader_status shader_state;
    shader_state.count = static_cast<int>(SDL_arraysize(wanted));

    const engine::shader_target chosen = engine::choose_shader_target(gpu.report().granted);
    SDL_Log("  shader format   : %s (.%s), entry point \"%s\"",
            engine::name_of(chosen.format), chosen.extension, chosen.entrypoint);

    for (int i = 0; i < shader_state.count; ++i)
    {
        shader_state.names[i] = wanted[i].name;
        shader_state.loaded[i] = shaders[i].load(gpu, wanted[i].name, wanted[i].stage);
        if (shader_state.loaded[i])
        {
            const engine::shader_resources& r = shaders[i].resources();
            SDL_Log("    %-14s %5zu bytes   samplers %u  storage tex %u  storage buf %u"
                    "  uniform buf %u",
                    wanted[i].name, shaders[i].code_bytes(), r.samplers,
                    r.storage_textures, r.storage_buffers, r.uniform_buffers);
        }
        else
        {
            SDL_Log("    %-14s NOT LOADED - see the message above", wanted[i].name);
        }
    }

    // ---- Lesson 4.4: the pipeline, and three vertices -----------------------
    //
    // Two objects and one measurement. The pipeline gathers every piece of render
    // state into something immutable (4.1's argument, now a call); the buffer
    // holds the geometry. The measurement is 4.3's unanswered question — that
    // lesson found SDL_CreateGPUShader takes eight microseconds, which cannot
    // include a compile, and predicted the compile happens HERE.
    engine::pipeline_desc desc(gpu, shaders[0].handle(), shaders[1].handle());
    desc.vertex_buffer(0, sizeof(gpu_vertex))
        .attribute(0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(gpu_vertex, x))
        .attribute(1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(gpu_vertex, r));

    engine::gpu_pipeline triangle_pipeline;
    const bool pipeline_ok = shader_state.loaded[0] && shader_state.loaded[1]
                          && triangle_pipeline.create(gpu, desc.info());

    if (pipeline_ok)
    {
        // EXPECT THIS NUMBER TO VARY ENORMOUSLY BETWEEN RUNS, and do not read it
        // as "the cost of creating a pipeline". Measured on one machine: ~42 ms
        // the first time these shaders were ever compiled, ~32 ms for the first
        // pipeline in a fresh process, ~2.4 ms for a state permutation the driver
        // has not seen, and under 0.1 ms once it has — because Metal caches
        // compiled pipelines ON DISK and the cache outlives the process.
        // Lesson 4.4 §4.4 is the whole story, including the two wrong conclusions
        // drawn from this line before it was measured properly.
        SDL_Log("  pipeline        : created in %.3f ms  (varies hugely with the driver's"
                " pipeline cache — see 4.4 §4.4)", triangle_pipeline.create_ms());
    }
    else
    {
        SDL_Log("  pipeline        : NOT created - the triangle will be missing");
    }

    engine::gpu_buffer triangle_vertices;
    bool geometry_ok = triangle_vertices.create(gpu, SDL_GPU_BUFFERUSAGE_VERTEX,
                                                sizeof(k_triangle), "triangle vertices");
    if (geometry_ok)
    {
        // The upload needs a command buffer of its own, submitted before the
        // first frame that draws from it. A frame's command buffer would work
        // too — the copy would simply happen at the start of that frame — but a
        // one-off upload belongs in a one-off submission, where it is obviously
        // not per-frame work.
        SDL_GPUCommandBuffer* upload_cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
        geometry_ok = triangle_vertices.upload(upload_cb, k_triangle, sizeof(k_triangle));
        if (!SDL_SubmitGPUCommandBuffer(upload_cb)) { geometry_ok = false; }
        SDL_Log("  geometry        : %zu bytes, %zu vertices%s", sizeof(k_triangle),
                SDL_arraysize(k_triangle), geometry_ok ? "" : "  - UPLOAD FAILED");
    }

    // ---- Lesson 4.5: a real mesh, its layout, and its instances -------------
    //
    // Everything below happens ONCE. The torus is read off disk, converted from
    // Lesson 3.5's parallel arrays into interleaved vertices, and pushed to the
    // device; from then on the only thing that moves per frame is 28 bytes per
    // instance. That sentence is the lesson.
    // LESSON 5.5 REPLACED SIX LINES WITH ONE, and the six are worth reading before
    // they go: `load_obj(asset_path("torus.obj").c_str(), torus)` assembled a path,
    // opened a file, parsed it, and then — twenty lines further down, in a
    // different branch — applied `flip_uv_v` by hand. Four steps, three of them
    // the engine's job, and the fourth (the import convention) sitting far enough
    // from the load that the two could drift.
    //
    // It is now an acquire against a store, with the flip as an import setting.
    // The store is a local because this probe outlives nothing and shares nothing;
    // that it is cheap to have one per program is the argument against a global.
    engine::asset_store assets;
    const engine::mesh_load torus_asset = assets.load_mesh("torus.obj");
    const engine::obj_report torus_report = torus_asset.report;

    engine::gpu_mesh mesh_indexed;
    engine::gpu_mesh mesh_expanded;
    bool mesh_ok = false;

    if (!torus_asset.ok())
    {
        SDL_Log("  mesh            : torus.obj did not load (%s) - [6] will do nothing",
                engine::name_of(torus_report.status));
    }
    else
    {
        SDL_GPUCommandBuffer* mesh_cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
        const engine::mesh view = assets.mesh_at(torus_asset.handle)->view();

        // BOTH FORMS OF THE SAME MESH, so that [7] is a comparison rather than a
        // claim. `expanded` writes three vertices per triangle and keeps no index
        // buffer — what a renderer without index buffers is forced to do.
        const bool a_ok = mesh_indexed.create(gpu, mesh_cb, view,
                                              engine::index_mode::indexed, "torus (indexed)");
        const bool b_ok = mesh_expanded.create(gpu, mesh_cb, view,
                                               engine::index_mode::expanded, "torus (expanded)");
        mesh_ok = a_ok && b_ok && SDL_SubmitGPUCommandBuffer(mesh_cb);

        SDL_Log("  mesh            : torus.obj  %zu vertices, %zu triangles",
                view.vertices.size(), view.triangle_count());
        SDL_Log("    indexed       : %6u vertices + %6u indices = %6u bytes"
                "  (%u-%u shader invocations)",
                mesh_indexed.vertex_count(), mesh_indexed.index_count(),
                mesh_indexed.total_bytes(),
                mesh_indexed.best_case_invocations(), mesh_indexed.worst_case_invocations());
        SDL_Log("    expanded      : %6u vertices + %6u indices = %6u bytes"
                "  (%u shader invocations, no reuse possible)",
                mesh_expanded.vertex_count(), mesh_expanded.index_count(),
                mesh_expanded.total_bytes(), mesh_expanded.best_case_invocations());
        if (mesh_indexed.total_bytes() > 0)
        {
            SDL_Log("    the index buffer costs %u bytes and saves %u: %.2fx",
                    mesh_indexed.index_bytes(),
                    mesh_expanded.vertex_bytes() - mesh_indexed.vertex_bytes(),
                    static_cast<double>(mesh_expanded.total_bytes())
                        / static_cast<double>(mesh_indexed.total_bytes()));
        }
    }

    // ASK, DO NOT ASSUME. SDL guarantees exactly one depth format — D16_UNORM —
    // and this machine turns out not to support D24_UNORM at all, which is
    // precisely the assumption a desktop renderer would have made.
    const SDL_GPUTextureFormat depth_wanted[] = {
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    };
    const SDL_GPUTextureFormat depth_format =
        engine::supported_depth_format(gpu, depth_wanted, SDL_arraysize(depth_wanted));

    SDL_Log("  depth format    : %s (%d bits)%s", engine::name_of(depth_format),
            engine::depth_bits(depth_format),
            depth_format == SDL_GPU_TEXTUREFORMAT_INVALID ? "  - NO DEPTH TEST" : "");


    // ---- Three pipelines that differ in ONE NUMBER --------------------------
    //
    // The pitch: 32 bytes (right), 28 (too small), 36 (too large). Nothing else
    // about them differs, none of the three fails to create, and two of the three
    // draw a mesh made of noise. Lesson 4.5 §4 is what [8] cycles through.
    constexpr Uint32 k_right_pitch = static_cast<Uint32>(sizeof(engine::gpu_vertex_pnu));
    const Uint32 pitches[3] = {k_right_pitch, k_right_pitch - 4u, k_right_pitch + 4u};
    const char* pitch_names[3] = {"32 (correct)", "28 (four bytes short)", "36 (four bytes long)"};

    engine::gpu_pipeline mesh_pipelines[3];
    engine::gpu_pipeline mesh_pipeline_nodepth;   // Lesson 4.7, for [C]
    bool mesh_pipeline_ok = false;

    if (shader_state.loaded[4] && shader_state.loaded[5])
    {
        for (int i = 0; i < 3; ++i)
        {
            engine::pipeline_desc mdesc(gpu, shaders[4].handle(), shaders[5].handle());

            // Slot 0 by hand rather than through `gpu_mesh::describe`, because the
            // pitch is what varies and describe() would (rightly) refuse to lie
            // about it. The attributes are identical in all three.
            mdesc.vertex_buffer(0, pitches[i])
                 .attribute(0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                            static_cast<Uint32>(offsetof(engine::gpu_vertex_pnu, px)))
                 .attribute(1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                            static_cast<Uint32>(offsetof(engine::gpu_vertex_pnu, nx)))
                 .attribute(2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                            static_cast<Uint32>(offsetof(engine::gpu_vertex_pnu, u)));
            describe_instances(mdesc);

            // Lesson 4.7: the three fields we have been zero-filling since 4.4.
            // `enable_depth_write` is documented as ignored while the test is off,
            // so the two travel together; `LESS` is Lesson 3.1's comparison, and
            // the reason it is LESS is that SDL_GPU's NDC puts 0 at the near
            // plane (conventions §4) — smaller is nearer.
            if (depth_format != SDL_GPU_TEXTUREFORMAT_INVALID)
            {
                mdesc.raw().target_info.has_depth_stencil_target = true;
                mdesc.raw().target_info.depth_stencil_format = depth_format;
                mdesc.raw().depth_stencil_state.enable_depth_test = true;
                mdesc.raw().depth_stencil_state.enable_depth_write = true;
                mdesc.raw().depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
            }

            // THE CHECK LESSON 4.4 SAID NOTHING PERFORMS. It catches the 28-byte
            // pitch, because the uv at offset 24 then ends at byte 32 of a 28-byte
            // vertex. It does NOT catch the 36-byte one — every attribute fits
            // inside an over-long vertex — and being clear about what a check
            // cannot see is most of what makes it trustworthy.
            const engine::layout_report lr =
                mdesc.check_layout(shaders[4].inputs(), pitch_names[i]);
            if (!lr.ok())
            {
                SDL_Log("  layout %s: %d problem(s) — see above",
                        pitch_names[i], lr.problems());
            }

            if (!mesh_pipelines[i].create(gpu, mdesc.info()))
            {
                SDL_Log("  mesh pipeline %s: NOT created", pitch_names[i]);
            }
        }
        // A FOURTH PIPELINE, identical but for the depth test, because [C] has to
        // switch it and the test is PIPELINE STATE — there is no
        // `SDL_SetGPUDepthTest`. Built at startup for the reason Lesson 4.4
        // measured: a new state permutation costs about 2.4 ms, which is 15% of a
        // 60 Hz frame and not something to pay on a keypress.
        {
            engine::pipeline_desc nd(gpu, shaders[4].handle(), shaders[5].handle());
            // `false`: mesh.vert.hlsl reads position, normal and uv and
            // nothing else, so declaring the tangent would both waste a fetch
            // and collide with `describe_instances`' location 3 (Lesson 6.7).
            engine::gpu_mesh::describe(nd, 0, false);
            describe_instances(nd);
            if (depth_format != SDL_GPU_TEXTUREFORMAT_INVALID)
            {
                // The FORMAT is still declared — the pass has a depth attachment
                // either way, and a pipeline whose target info disagrees with the
                // pass is a mismatch. What changes is only whether it TESTS.
                nd.raw().target_info.has_depth_stencil_target = true;
                nd.raw().target_info.depth_stencil_format = depth_format;
            }
            nd.raw().depth_stencil_state.enable_depth_test = false;
            nd.raw().depth_stencil_state.enable_depth_write = false;
            if (!mesh_pipeline_nodepth.create(gpu, nd.info()))
            {
                SDL_Log("  mesh pipeline (no depth): NOT created");
            }
        }

        mesh_pipeline_ok = mesh_pipelines[0].valid();
    }

    // ---- Lesson 4.7: the image, the samplers, and the depth format ---------
    //
    // The texture is uploaded once, by the transfer path Lesson 4.2 built for the
    // framebuffer. The two samplers exist because a sampler is an OBJECT and
    // swapping filters at runtime means swapping objects rather than passing a
    // different argument — which is the whole difference from Lesson 3.9.
    engine::gpu_texture uv_texture;
    engine::gpu_sampler sampler_linear;
    engine::gpu_sampler sampler_nearest;
    bool texture_ok = false;

    // Lesson 5.3: a report, not a bare status. Note what the demo does NOT do —
    // it does not log the failure at ERROR. `load_image` already did that, with
    // the path and the reason, at the point that knew them. Adding a second
    // error line here would produce two entries for one problem, the second of
    // them less informative than the first. What the demo adds is the
    // CONSEQUENCE, which is the only thing it knows and the loader does not.
    const engine::image_load uv_asset = assets.load_image("uv_grid.png");
    const engine::image_report img = uv_asset.report;

    if (!img.ok())
    {
        SDL_Log("  texture         : uv_grid.png did not load (%s) — [T] will do nothing",
                engine::name_of(img.status));
    }
    else
    {
        SDL_Log("  texture         : uv_grid.png %dx%d, %d channels on disk,"
                " %zu bytes decoded from %zu (%.1fx)",
                img.width, img.height, img.source_channels,
                img.bytes, img.file_bytes,
                static_cast<double>(img.bytes) / static_cast<double>(img.file_bytes));

        SDL_GPUCommandBuffer* tex_cb = SDL_AcquireGPUCommandBuffer(gpu.handle());

        // srgb = true. Lesson 3.9's whole argument in one flag: an albedo is a
        // reflectance, a reflectance multiplies a quantity of light, so it must be
        // LINEAR before the multiply. The sampler now does that decode per read,
        // for free, and before the filter rather than after.
        texture_ok = uv_texture.create_sampled(gpu, tex_cb, *assets.image_at(uv_asset.handle),
                                              true, "uv grid")
                  && SDL_SubmitGPUCommandBuffer(tex_cb);

        texture_ok = texture_ok
                  && sampler_linear.create(gpu, engine::filter::linear,
                                           engine::address_mode::repeat)
                  && sampler_nearest.create(gpu, engine::filter::nearest,
                                            engine::address_mode::repeat);

        SDL_Log("  texture         : uv_grid.png %dx%d, %d channels in the file,"
                " uploaded %u bytes as %s",
                img.width, img.height, img.source_channels,
                uv_texture.uploaded_bytes(), engine::name_of(uv_texture.format()));
    }

    // The depth target itself is created lazily, because it must match the
    // SWAPCHAIN's size and the window is resizable — see the frame loop.
    engine::gpu_texture depth_target;
    Uint32 depth_w = 0;
    Uint32 depth_h = 0;

    // ---- The per-instance buffer -------------------------------------------
    engine::gpu_stream_buffer instance_buffer;
    const bool instances_ok =
        instance_buffer.create(gpu, SDL_GPU_BUFFERUSAGE_VERTEX,
                               static_cast<Uint32>(sizeof(gpu_instance)) * k_max_instances,
                               "instance placements");

    SDL_Log("Keys: [1] filter  [2] present mode  [3] frames in flight"
            "  [4] wait on a fence every frame  [5] GPU triangle");
    SDL_Log("      [6] the mesh  [7] indexed/expanded  [8] the pitch  [9] instances");
    SDL_Log("      arrows or WASD orbit  [Z]/[X] dolly  [0] reset  [L] orbit the lamp");
    SDL_Log("      [T] texture/grid  [F] filter  [R] uv scale  [C] depth test  [Esc] quit");
    SDL_Log("Graph: green = software raster, cyan = recording, blue = waiting for a"
            " swapchain image, orange = waiting on a fence.");

    engine::clock clk;
    engine::input in;

    constexpr int k_ring = 160;
    probe_sample ring[k_ring] = {};
    int ring_head = 0;

    bool running = true;
    bool logged_swapchain_size = false;
    probe_view view;                       // [6] arrows/WASD, [Z]/[X], [0], [L]
    bool draw_gpu_triangle = true;
    bool draw_mesh = true;                 // [6]
    bool mesh_use_indices = true;          // [7]
    int mesh_pitch_choice = 0;             // [8] — index into `pitches`
    int instance_count = k_max_instances;  // [9]
    bool smooth = false;
    bool fence_each_frame = false;
    int present_mode_index = 0;
    Uint32 frames_in_flight = 2;
    float spin = 0.0f;
    Uint64 last_log = SDL_GetTicks();

    const SDL_GPUPresentMode modes[3] = {
        SDL_GPU_PRESENTMODE_VSYNC,
        SDL_GPU_PRESENTMODE_IMMEDIATE,
        SDL_GPU_PRESENTMODE_MAILBOX
    };
    const char* mode_names[3] = {"VSYNC", "IMMEDIATE", "MAILBOX"};

    while (running)
    {
        const Uint64 frame_t0 = SDL_GetPerformanceCounter();

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            in.feed_event(event);
            if (event.type == SDL_EVENT_QUIT) { running = false; }
        }

        clk.tick();
        in.update();

        if (in.key_pressed(SDL_SCANCODE_ESCAPE) || in.key_pressed(SDL_SCANCODE_Q))
        {
            running = false;
        }

        if (in.key_pressed(SDL_SCANCODE_1))
        {
            smooth = !smooth;
            SDL_Log("[1] blit filter: %s", smooth ? "LINEAR" : "NEAREST");
        }
        if (in.key_pressed(SDL_SCANCODE_2))
        {
            // Try each mode in turn until one is accepted. Asking and being told
            // no is the supported way to discover support, so a refusal here is
            // information rather than an error.
            for (int step = 1; step <= 3; ++step)
            {
                const int next = (present_mode_index + step) % 3;
                if (gpu.set_present_mode(modes[next]))
                {
                    present_mode_index = next;
                    SDL_Log("[2] present mode: %s", mode_names[next]);
                    break;
                }
                SDL_Log("[2] present mode %s is not supported on this window",
                        mode_names[next]);
            }
        }
        if (in.key_pressed(SDL_SCANCODE_3))
        {
            const Uint32 next = frames_in_flight % 3 + 1;
            if (gpu.set_frames_in_flight(next))
            {
                frames_in_flight = next;
                SDL_Log("[3] frames in flight: %u  (this call stalls and flushes"
                        " the queue, so the next frame's numbers are junk)", next);
            }
        }
        if (in.key_pressed(SDL_SCANCODE_5))
        {
            draw_gpu_triangle = !draw_gpu_triangle;
            SDL_Log("[5] GPU triangle: %s", draw_gpu_triangle ? "ON" : "off");
        }
        // Lesson 4.6: the camera and the lamp, which existed as `static const`
        // floats inside the vertex shader until this lesson.
        view.update(in, clk.dt());

        if (in.key_pressed(SDL_SCANCODE_0))
        {
            view = probe_view{};
            SDL_Log("[0] camera reset");
        }
        if (in.key_pressed(SDL_SCANCODE_L))
        {
            view.light_orbits = !view.light_orbits;
            SDL_Log("[L] the lamp orbits: %s", view.light_orbits ? "ON" : "off");
        }
        if (in.key_pressed(SDL_SCANCODE_T))
        {
            view.show_texture = !view.show_texture;
            SDL_Log("[T] surface: %s", view.show_texture ? "the texture"
                                                         : "Lesson 4.5's diagnostic grid");
        }
        if (in.key_pressed(SDL_SCANCODE_F))
        {
            view.smooth_texture = !view.smooth_texture;
            SDL_Log("[F] filter: %s  (a different sampler OBJECT, not a different argument)",
                    view.smooth_texture ? "LINEAR" : "NEAREST");
        }
        if (in.key_pressed(SDL_SCANCODE_R))
        {
            view.uv_repeat = (view.uv_repeat >= 4.0f) ? 1.0f : view.uv_repeat * 2.0f;
            SDL_Log("[R] uv scale: %.0fx  (address_mode::repeat is what makes >1 mean"
                    " anything)", static_cast<double>(view.uv_repeat));
        }
        if (in.key_pressed(SDL_SCANCODE_C))
        {
            view.depth_test = !view.depth_test;
            SDL_Log("[C] depth test: %s%s", view.depth_test ? "ON" : "off",
                    view.depth_test ? ""
                        : "  <- this is what every lesson before 4.7 looked like");
        }
        if (in.key_pressed(SDL_SCANCODE_6))
        {
            draw_mesh = !draw_mesh;
            SDL_Log("[6] mesh: %s", draw_mesh ? "ON" : "off");
        }
        if (in.key_pressed(SDL_SCANCODE_7))
        {
            mesh_use_indices = !mesh_use_indices;
            const engine::gpu_mesh& m = mesh_use_indices ? mesh_indexed : mesh_expanded;
            SDL_Log("[7] %s: %u vertices, %u indices, %u bytes on the device —"
                    " and the picture is IDENTICAL",
                    mesh_use_indices ? "INDEXED " : "EXPANDED",
                    m.vertex_count(), m.index_count(), m.total_bytes());
        }
        if (in.key_pressed(SDL_SCANCODE_8))
        {
            mesh_pitch_choice = (mesh_pitch_choice + 1) % 3;
            SDL_Log("[8] vertex pitch: %s%s", pitch_names[mesh_pitch_choice],
                    mesh_pitch_choice == 0 ? ""
                        : "  <- the layout now walks the buffer at the wrong rate");
        }
        if (in.key_pressed(SDL_SCANCODE_9))
        {
            // 1, 4, 7 — the same buffer of geometry, drawn a different number of
            // times by changing ONE ARGUMENT of the draw call.
            instance_count = (instance_count == 1) ? 4 : (instance_count == 4 ? 7 : 1);
            SDL_Log("[9] instances: %d  (%zu bytes of per-instance data per frame,"
                    " against %u bytes of geometry that has not moved since startup)",
                    instance_count, sizeof(gpu_instance) * static_cast<std::size_t>(instance_count),
                    mesh_indexed.total_bytes());
        }
        if (in.key_pressed(SDL_SCANCODE_4))
        {
            fence_each_frame = !fence_each_frame;
            SDL_Log("[4] wait on a fence every frame: %s%s",
                    fence_each_frame ? "ON" : "off",
                    fence_each_frame ? "  <- the CPU now finishes when the GPU does" : "");
        }

        spin += clk.dt() * 0.8f;

        probe_sample sample;

        // ---- 1. The picture, made on the CPU -------------------------------
        const Uint64 t_draw0 = SDL_GetPerformanceCounter();
        draw_probe_scene(fb, spin, ring, k_ring, ring_head, fence_each_frame, shader_state);
        const Uint64 t_draw1 = SDL_GetPerformanceCounter();
        sample.draw = ticks_to_ms(t_draw0, t_draw1);

        // ---- 2. Recording ---------------------------------------------------
        //
        // Nothing below this line executes when it is written. Every call appends
        // to a list, and the list runs later, on another processor. That sentence
        // is the whole of Lesson 4.2, and the graph is what it costs.
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
        if (cb == nullptr)
        {
            SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
            break;
        }

        const Uint64 t_rec0 = SDL_GetPerformanceCounter();
        present.upload(cb, fb);

        // Lesson 4.5. The whole of this frame's geometry work: 196 bytes at most,
        // recorded into a copy pass that runs before the draw that reads it. The
        // torus itself was uploaded once, before the loop, and is not touched.
        if (draw_mesh && instances_ok)
        {
            gpu_instance placements[k_max_instances] = {};
            place_instances(placements, instance_count, spin);
            (void)instance_buffer.write(cb, placements,
                                        static_cast<Uint32>(sizeof(gpu_instance))
                                            * static_cast<Uint32>(instance_count));
        }
        const Uint64 t_rec1 = SDL_GetPerformanceCounter();

        // ---- 3. The swapchain image ----------------------------------------
        //
        // THIS is where a vsynced program actually waits, and putting the upload
        // above it is not an accident: the copy is recorded into the command
        // buffer while the display is still busy with the previous frame.
        SDL_GPUTexture* swap = nullptr;
        Uint32 swap_w = 0;
        Uint32 swap_h = 0;
        const Uint64 t_acq0 = SDL_GetPerformanceCounter();
        const bool acquired =
            SDL_WaitAndAcquireGPUSwapchainTexture(cb, window, &swap, &swap_w, &swap_h);
        const Uint64 t_acq1 = SDL_GetPerformanceCounter();
        sample.acquire = ticks_to_ms(t_acq0, t_acq1);

        if (!acquired)
        {
            SDL_Log("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
            break;
        }

        // Printed once, because the relationship is not the one people assume. A
        // window is measured in POINTS and a swapchain in PIXELS, and whether
        // those are the same number depends on the window: without
        // SDL_WINDOW_HIGH_PIXEL_DENSITY they agree even on a Retina display,
        // which is what this window does and what the line below reports. Ask for
        // that flag and they stop agreeing — and code that assumed they were equal
        // then renders into a quarter of the window and looks for the bug in its
        // projection matrix. The acquire hands back the real numbers; use those.
        if (!logged_swapchain_size && swap != nullptr)
        {
            logged_swapchain_size = true;
            int win_w = 0;
            int win_h = 0;
            SDL_GetWindowSize(window, &win_w, &win_h);
            const engine::blit_rect fit =
                engine::fit_centred(static_cast<Uint32>(fb.width()),
                                    static_cast<Uint32>(fb.height()), swap_w, swap_h);
            SDL_Log("  window %dx%d points -> swapchain %ux%u pixels;"
                    " %dx%d framebuffer lands at %u,%u size %ux%u",
                    win_w, win_h, swap_w, swap_h, fb.width(), fb.height(),
                    fit.x, fit.y, fit.w, fit.h);
        }

        const Uint64 t_rec2 = SDL_GetPerformanceCounter();
        if (swap != nullptr)
        {
            // A NULL texture is not an error — a minimised window has nothing to
            // present to. The documented behaviour is to skip the frame, and
            // submitting the command buffer anyway keeps the upload's cycling
            // bookkeeping consistent.

            // The render pass exists to CLEAR, and only to clear. It has no
            // pipeline bound and issues no draw, which is legal and is exactly
            // what a load op is for: clearing is not a draw, it is a property of
            // beginning a pass. Without it the letterbox bars are undefined.
            SDL_GPUColorTargetInfo target{};
            target.texture = swap;
            target.clear_color = SDL_FColor{0.02f, 0.02f, 0.03f, 1.0f};
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cb, &target, 1, nullptr);
            SDL_EndGPURenderPass(pass);

            // Outside the pass, because a blit is itself a pass.
            present.blit_onto(cb, swap, swap_w, swap_h, smooth);

            // ---- Lessons 4.4 and 4.5: the GPU draws -------------------------
            //
            // A SECOND render pass, loading what the blit just wrote rather than
            // clearing it, so the CPU's picture and the GPU's geometry share one
            // window. That is the comparison Module 4 is for: everything under it
            // was computed by code you wrote, and everything drawn into it by
            // hardware running a shader you wrote.
            const bool want_triangle = draw_gpu_triangle && pipeline_ok && geometry_ok;
            const bool want_mesh = draw_mesh && mesh_ok && mesh_pipeline_ok && instances_ok
                                && mesh_pipelines[mesh_pitch_choice].valid();

            if (want_triangle || want_mesh)
            {
                SDL_GPUColorTargetInfo over{};
                over.texture = swap;
                over.load_op = SDL_GPU_LOADOP_LOAD;    // keep the blit
                over.store_op = SDL_GPU_STOREOP_STORE;

                // ---- Lesson 4.6: the per-FRAME data ---------------------
                //
                // Pushed onto the COMMAND BUFFER, not bound to the pass, and
                // deliberately before the pass begins: every draw recorded after
                // this point in this command buffer reads it. There is no object
                // to create and nothing to release, which is why this is three
                // lines rather than a resource type.
                //
                // The aspect ratio comes from the rectangle we are about to draw
                // into. Lesson 4.5 could not do that — it had no way to send a
                // number to a shader, so 16:9 was compiled in and the viewport
                // had to make it true.
                if (want_mesh)
                {
                    const engine::blit_rect fit =
                        engine::fit_centred(static_cast<Uint32>(fb.width()),
                                            static_cast<Uint32>(fb.height()), swap_w, swap_h);
                    const float aspect = (fit.h > 0)
                        ? static_cast<float>(fit.w) / static_cast<float>(fit.h)
                        : 16.0f / 9.0f;

                    const engine::camera_uniforms camera{view.clip_from_world(aspect)};
                    SDL_PushGPUVertexUniformData(cb, 0, &camera, sizeof(camera));

                    engine::light_uniforms light{};
                    light.to_light = view.to_light();
                    light.ambient = 0.22f;
                    light.sky = engine::vec3{0.62f, 0.74f, 1.0f};   // a blue sky, not grey
                    light.diffuse = 0.90f;
                    light.uv_scale = engine::vec2{view.uv_repeat, view.uv_repeat};
                    light.grid_mix = (view.show_texture && texture_ok) ? 0.0f : 1.0f;
                    SDL_PushGPUFragmentUniformData(cb, 0, &light, sizeof(light));
                }

                // ---- Lesson 4.7: the depth attachment ------------------
                //
                // Created lazily and RE-created when the swapchain changes size,
                // because a depth attachment must match its colour target exactly
                // and this window is resizable. That is the one piece of
                // bookkeeping a depth buffer costs, and forgetting it is a crash
                // on the first resize rather than a wrong picture.
                SDL_GPUDepthStencilTargetInfo depth_info{};
                bool have_depth = false;

                if (want_mesh && depth_format != SDL_GPU_TEXTUREFORMAT_INVALID)
                {
                    if (!depth_target.valid() || depth_w != swap_w || depth_h != swap_h)
                    {
                        if (depth_target.create_depth(gpu, depth_format, swap_w, swap_h,
                                                      "scene depth"))
                        {
                            depth_w = swap_w;
                            depth_h = swap_h;
                            SDL_Log("  depth target    : %ux%u %s", swap_w, swap_h,
                                    engine::name_of(depth_format));
                        }
                    }

                    if (depth_target.valid())
                    {
                        depth_info.texture = depth_target.handle();

                        // CLEARED TO 1, the far plane, because SDL_GPU's NDC runs
                        // 0 at near to 1 at far (conventions §4) and the test is
                        // LESS. Clear it to 0 instead and every fragment fails.
                        depth_info.clear_depth = 1.0f;
                        depth_info.load_op = SDL_GPU_LOADOP_CLEAR;

                        // DONT_CARE, not STORE: nothing reads this buffer after
                        // the pass ends. Module 6's shadow maps and depth-based
                        // post-processing are where that changes, and on tiled
                        // hardware the difference is real bandwidth.
                        depth_info.store_op = SDL_GPU_STOREOP_DONT_CARE;
                        depth_info.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
                        depth_info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
                        have_depth = true;
                    }
                }

                SDL_GPURenderPass* draw_pass =
                    SDL_BeginGPURenderPass(cb, &over, 1, have_depth ? &depth_info : nullptr);

                if (want_mesh)
                {
                    // THE VIEWPORT. Lesson 4.5 needed this to make a compile-time
                    // aspect ratio true; Lesson 4.6 does not, because the aspect
                    // is now a number we compute and put in the matrix. It stays
                    // for the reason it should have been there all along: the
                    // GPU's picture and the blitted software picture share a
                    // window, and they should share a rectangle. Lesson 2.11's
                    // viewport transform, handed to hardware as a struct.
                    const engine::blit_rect fit =
                        engine::fit_centred(static_cast<Uint32>(fb.width()),
                                            static_cast<Uint32>(fb.height()), swap_w, swap_h);

                    SDL_GPUViewport vp{};
                    vp.x = static_cast<float>(fit.x);
                    vp.y = static_cast<float>(fit.y);
                    vp.w = static_cast<float>(fit.w);
                    vp.h = static_cast<float>(fit.h);
                    vp.min_depth = 0.0f;
                    vp.max_depth = 1.0f;   // SDL_GPU's depth range (conventions §4)
                    SDL_SetGPUViewport(draw_pass, &vp);

                    // [C] selects a different PIPELINE, not a different flag —
                    // the depth test is baked in, which is Lesson 4.1's argument
                    // arriving for the fourth time.
                    const engine::gpu_pipeline& chosen =
                        (view.depth_test && have_depth && mesh_pipeline_nodepth.valid())
                            ? mesh_pipelines[mesh_pitch_choice]
                            : (mesh_pipeline_nodepth.valid() ? mesh_pipeline_nodepth
                                                             : mesh_pipelines[mesh_pitch_choice]);
                    SDL_BindGPUGraphicsPipeline(draw_pass, chosen.handle());

                    // Lesson 4.7: the texture and the sampler, bound as a PAIR.
                    // Two objects, one binding — which is why one image can be
                    // read three ways in a frame without being duplicated.
                    if (texture_ok)
                    {
                        SDL_GPUTextureSamplerBinding tex_bind{};
                        tex_bind.texture = uv_texture.handle();
                        tex_bind.sampler = view.smooth_texture ? sampler_linear.handle()
                                                               : sampler_nearest.handle();
                        SDL_BindGPUFragmentSamplers(draw_pass, 0, &tex_bind, 1);
                    }

                    const engine::gpu_mesh& m = mesh_use_indices ? mesh_indexed : mesh_expanded;
                    m.bind(draw_pass, 0);

                    // Slot 1: the per-instance buffer. Bound with the SAME call as
                    // the geometry, because at this level there is no difference
                    // between them — `input_rate` in the pipeline is the only place
                    // that knows one advances per vertex and the other per instance.
                    SDL_GPUBufferBinding ib{};
                    ib.buffer = instance_buffer.handle();
                    ib.offset = 0;
                    SDL_BindGPUVertexBuffers(draw_pass, 1, &ib, 1);

                    m.draw(draw_pass, static_cast<Uint32>(instance_count));

                    // Put the viewport back for anything drawn after us. Viewport
                    // is PASS state, not pipeline state: it survives a pipeline
                    // change, which is exactly the sort of thing that makes a
                    // second draw mysteriously land in the wrong rectangle.
                    SDL_GPUViewport full{};
                    full.w = static_cast<float>(swap_w);
                    full.h = static_cast<float>(swap_h);
                    full.max_depth = 1.0f;
                    SDL_SetGPUViewport(draw_pass, &full);
                }

                if (want_triangle)
                {
                    SDL_BindGPUGraphicsPipeline(draw_pass, triangle_pipeline.handle());

                    SDL_GPUBufferBinding binding{};
                    binding.buffer = triangle_vertices.handle();
                    binding.offset = 0;
                    SDL_BindGPUVertexBuffers(draw_pass, 0, &binding, 1);

                    // Three vertices, one instance, starting at the beginning of
                    // both. The `1` is what the mesh above passes seven of.
                    SDL_DrawGPUPrimitives(draw_pass, 3, 1, 0, 0);
                }

                SDL_EndGPURenderPass(draw_pass);
            }
        }
        const Uint64 t_rec3 = SDL_GetPerformanceCounter();
        sample.record = ticks_to_ms(t_rec0, t_rec1) + ticks_to_ms(t_rec2, t_rec3);

        // ---- 4. Submit, and optionally wait ---------------------------------
        if (fence_each_frame)
        {
            SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cb);
            if (fence == nullptr)
            {
                SDL_Log("submit failed: %s", SDL_GetError());
                break;
            }
            const Uint64 t_f0 = SDL_GetPerformanceCounter();
            SDL_WaitForGPUFences(gpu.handle(), true, &fence, 1);
            const Uint64 t_f1 = SDL_GetPerformanceCounter();
            SDL_ReleaseGPUFence(gpu.handle(), fence);
            sample.fence = ticks_to_ms(t_f0, t_f1);
        }
        else if (!SDL_SubmitGPUCommandBuffer(cb))
        {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            break;
        }

        sample.frame = ticks_to_ms(frame_t0, SDL_GetPerformanceCounter());
        ring[ring_head] = sample;
        ring_head = (ring_head + 1) % k_ring;

        // A log line a second, because the graph has no numbers on it.
        const Uint64 now = SDL_GetTicks();
        if (now - last_log >= 1000)
        {
            last_log = now;
            const probe_sample avg = average(ring, k_ring);
            SDL_Log("%5.1f fps | draw %5.3f  record %5.3f  acquire %6.3f  fence %6.3f"
                    "  frame %6.3f ms | %s, %s, %u in flight, fence %s",
                    clk.fps(), avg.draw, avg.record, avg.acquire, avg.fence, avg.frame,
                    mode_names[present_mode_index], smooth ? "LINEAR" : "NEAREST",
                    frames_in_flight, fence_each_frame ? "ON" : "off");
        }
    }

    // Destruction order is the reverse of creation and it matters: the present
    // target's texture belongs to the device, so it must go first. Writing them
    // in this order is not enough on its own — `present` is declared after `gpu`,
    // so C++ destroys it first anyway — but saying it explicitly makes the
    // dependency visible rather than accidental.
    // Shaders first, then the present target, then the device — every GPU object
    // must be released before the device that created it. The array's destructors
    // would do this anyway (they are declared after `gpu`, so C++ destroys them
    // first), and saying it explicitly is the same choice 4.2 made: a dependency
    // that is visible is a dependency that survives the next edit.
    depth_target.destroy();
    uv_texture.destroy();
    sampler_linear.destroy();
    sampler_nearest.destroy();
    mesh_pipeline_nodepth.destroy();
    instance_buffer.destroy();
    mesh_indexed.destroy();
    mesh_expanded.destroy();
    for (engine::gpu_pipeline& mp : mesh_pipelines) { mp.destroy(); }
    triangle_vertices.destroy();
    triangle_pipeline.destroy();
    for (engine::gpu_shader& shader : shaders) { shader.destroy(); }
    present.destroy();
    gpu.destroy();
    return 0;
}

// ============================================================================
// LESSON 4.8 — MODULE 3'S SCENE, ON THE GPU
// ============================================================================
//
// This is the payoff of Module 4 and it should feel undramatic. Every piece has
// already been built: geometry (4.5), a camera (4.6), textures and a depth buffer
// (4.7). What has been missing is the SCENE — Module 3's floor, its models, its
// lighting controls and its HUD have gone on running entirely on the CPU beside
// the GPU path, and this function is where they stop.
//
// THE CLAIM UNDER TEST. Modules 2 and 3 pinned down their conventions
// deliberately — NDC ranges, a depth range with 0 at the near plane,
// counter-clockwise front faces, column-major matrices, sampler enums spelled the
// way SDL spells them — on the stated promise that this lesson would then be an
// API change rather than a maths change. `verify_48` counts what actually
// changed, and the answer is in §2 of the lesson.
//
// WHAT IS NOT HERE, AND WHY. There is no text. The software demo's HUD is drawn
// with SDL_RenderDebugText, which needs an SDL_Renderer, and Lesson 4.2
// established that a window is claimed by SDL_GPU or driven by SDL_Renderer and
// never both. Text on the GPU needs a font atlas and a shader — stb_truetype,
// Module 6 — and inventing three lessons' worth of that here to print eleven
// numbers would be the tail wagging the dog. So the numbers go to the log, once a
// second and on every keypress, and the picture gets [V]: a split view with the
// software rasterizer's frame on the left and the GPU's on the right, the same
// scene, the same camera, the same light. That comparison is worth more than the
// HUD was.

/// The one texture the ported scene needs, and the meshes it needs on the device.
///
/// **LESSON 5.4 IS THE ANSWER THIS CLASS WAS WRITTEN TO ASK FOR.** Its 4.8
/// comment read: *"it keys a mesh by the address of its first vertex … watch what
/// it cannot do — answer 'is this the same mesh?' without comparing pointers,
/// free anything, or survive the geometry it points at being rebuilt — and
/// Module 5 arrives as an answer."*
///
/// Here is the diff, and it is the shortest summary of what a handle buys that
/// this course will produce. The cache key WAS five fields:
///
///     key == m.vertices.data()                 // the address, which can be reused
///     && style == style
///     && cpu.vertices.size() >= m.vertices.size()
///     && source_vertices == m.vertices.size()  // …so compare the shape as well
///     && source_indices == m.indices.size()    // …and hope that is enough
///
/// and it is now two: `source == handle && style == style`. Four of the five
/// tests existed for exactly one reason — a `std::vector` rebuilt in place keeps
/// its address and changes its contents, so the address alone was never an
/// identity. A handle IS an identity. Rebuild the floor and the handle changes;
/// compare two handles and you have compared two occupants of two slots, not two
/// addresses that happen to agree today.
///
/// **LESSON 5.5 CLOSED THE LIFETIME HOLE 5.4 LEFT HERE.** The imported geometry —
/// `with_normals` output, a different vertex count and a different index buffer
/// from the mesh it came from — used to live in a second `mesh_pool` this class
/// owned, with no rule connecting it to the mesh it was made from. Unload the
/// source and the derived copy lived on, keyed by a handle that no longer
/// resolved: a leak with a clean bill of health.
///
/// It is now an `asset_store::derive_mesh`, which binds the two lifetimes: the
/// source's unload cascades. What this class has to do about it is the part worth
/// reading — **nothing, except notice.** An entry whose derived handle stops
/// resolving is an entry whose GPU buffers should go, and `get()` checks that on
/// the way past. A cache keyed on a pointer could never have been told; a cache
/// keyed on a handle finds out by asking the question it was already asking.
class scene_mesh_cache
{
public:
    /// One import, TWO consumers, and this is the part that matters.
    ///
    /// `cpu` is the geometry after normal generation, and both renderers draw
    /// from it: the software rasterizer takes `cpu.view()`, the GPU takes the
    /// interleaved upload made from the same arrays. If the two paths imported
    /// separately, every pixel of disagreement would be suspect — is that the
    /// rasterizer, or did they start from different triangles? Sharing the import
    /// is what makes §4's per-pixel comparison mean anything at all.
    struct entry
    {
        engine::mesh_handle source;         ///< WHICH mesh this was imported from
        engine::normal_style style = engine::normal_style::smooth;
        engine::mesh_handle cpu;            ///< with normals, in the store; what BOTH paths draw
        engine::gpu_mesh gpu;               ///< …and its device-side interleaving
    };

    /// Find or upload the device-side copy of `m`.
    ///
    /// Returns `nullptr` when the cache is full or the upload failed — a caller
    /// that skips such an object draws the rest of the scene, which is what a
    /// renderer should do when one asset is missing.
    const entry* get(const engine::gpu_device& dev, engine::asset_store& store,
                     engine::mesh_handle source, engine::normal_style style)
    {
        const engine::mesh_data* src = store.mesh_at(source);
        if (src == nullptr || src->vertices.empty() || src->indices.empty()) { return nullptr; }
        const engine::mesh m = src->view();

        for (int i = 0; i < count_; ++i)
        {
            // Two comparisons, and the first is a single 32-bit equality. A
            // handle carries its own generation, so "the same slot, refilled" and
            // "the same mesh" are different values — which is the entire content
            // of the four tests this line replaced.
            if (slots_[i].source == source && slots_[i].style == style)
            {
                // …and one more, added in 5.5: is the DERIVED asset still there?
                // The store may have unloaded it from under us, because unloading
                // the source cascades. A hit whose geometry is gone is a miss, and
                // its GPU buffers are ours to destroy — nobody else knows they
                // exist. This is the only eviction this cache has, and it arrives
                // for free: it is the same `contains` question a handle answers
                // everywhere else.
                if (store.meshes().contains(slots_[i].cpu)) { return &slots_[i]; }

                slots_[i].gpu.destroy();
                slots_[i] = slot{};
                if (i != count_ - 1) { slots_[i] = std::move(slots_[count_ - 1]); }
                --count_;
                break;
            }
        }

        if (count_ >= k_max) { return nullptr; }

        slot& e = slots_[count_];
        e.source = source;
        e.style = style;

        // Lesson 4.8's first real finding: three of the four built-in meshes carry
        // no normals, the ground plane carries none either, and a VERTEX SHADER
        // CANNOT INVENT THEM — it is handed one vertex and cannot see the other
        // two corners of the triangle. `collect_triangles` could and did. So the
        // fallback moves out of the renderer and into the geometry, which is where
        // every real engine puts it.
        // The IMPORTED geometry is a mesh in its own right — a different vertex
        // count, different normals, a different index buffer — and as of Lesson
        // 5.5 it is stored as a DERIVED asset, with its lifetime bound to the mesh
        // it was made from. It gets no name, because nobody asked for it by one:
        // it exists only as a consequence of its source existing, and it should
        // stop existing for the same reason.
        e.cpu = store.derive_mesh(source, engine::with_normals(m, style));
        const engine::mesh_data* cpu = store.mesh_at(e.cpu);
        if (cpu == nullptr) { return nullptr; }

        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(dev.handle());
        if (cb == nullptr) { return nullptr; }
        const bool ok = e.gpu.create(dev, cb, cpu->view(), engine::index_mode::indexed,
                                     "scene mesh")
                     && SDL_SubmitGPUCommandBuffer(cb);
        if (!ok) { return nullptr; }

        SDL_Log("  mesh uploaded   : %5zu -> %5zu vertices, %5zu triangles, %s normals"
                "  [handle %u:%u]",
                m.vertices.size(), cpu->vertices.size(), cpu->triangle_count(),
                m.normals.empty()
                    ? (style == engine::normal_style::flat ? "generated FLAT"
                                                           : "generated SMOOTH")
                    : "authored",
                source.index(), source.generation());

        ++count_;
        return &e;
    }

    /// Destroy the device-side buffers. The derived MESHES are the store's and
    /// are released with it — which is the division this class exists to
    /// demonstrate: an asset store owns data, and a cache owns the device objects
    /// it built from that data.
    void destroy()
    {
        for (int i = 0; i < count_; ++i) { slots_[i].gpu.destroy(); }
        count_ = 0;
    }

    [[nodiscard]] int size() const { return count_; }

private:
    static constexpr int k_max = 8;

    using slot = entry;

    slot slots_[k_max];
    int count_ = 0;
};

/// Every knob the ported demo has, in one place.
struct scene_controls
{
    demo::orbit_camera camera;
    float light_azimuth = 0.85f;
    static constexpr float k_light_elevation = 0.70f;   ///< the software demo's value

    demo::scene_kind scene = demo::scene_kind::solids;              ///< [C]
    engine::specular_model spec = engine::specular_model::cook_torrance;   ///< [H]
    int shininess_step = 4;                             ///< [E]

    bool correct_normals = true;    ///< [J] — inverse transpose, or the naive matrix
    bool wireframe = false;         ///< [W]
    bool depth_test = true;         ///< [Z]
    bool smooth_texture = true;     ///< [F]
    bool floor_textured = true;     ///< [M]
    bool sort_by_pipeline = false;  ///< [O] — what a sorted draw list is worth
    int cull_override = 0;          ///< [U] — 0 auto, 1 none, 2 back

    /// 0 = GPU only, 1 = split (software left, GPU right), 2 = software only.
    int view_mode = 1;              ///< [V]

    /// The same numbers Module 3's scene camera uses, so the two pictures are
    /// comparable without anybody having to argue about the frustum.
    static constexpr float k_fovy = 55.0f * 3.14159265358979f / 180.0f;
    static constexpr float k_near = 0.3f;
    static constexpr float k_far = 100.0f;

    [[nodiscard]] engine::vec3 to_light() const
    {
        const float ce = std::cos(k_light_elevation);
        return engine::normalised(engine::vec3{ce * std::sin(light_azimuth),
                                               std::sin(k_light_elevation),
                                               ce * std::cos(light_azimuth)});
    }
};

/// The eight roughness values [E] cycles — the software demo's array, unchanged,
/// so a highlight can be compared between the two renderers at the same surface.
///
/// **LESSON 6.4 REPLACED THE LADDER, AND THE REPLACEMENT IS THE LESSON.** It used
/// to be `{2, 4, 8, ..., 256}` — powers of two, because an exponent has no natural
/// scale and doubling was as good a guess as any. Roughness has a scale: it runs
/// 0 to 1, 0 is a mirror and 1 is chalk, and the values below are spaced so the
/// steps LOOK evenly spaced, which is exactly what Lesson 6.3's `alpha = r^2`
/// remap exists to buy. Index 4 is 0.49, which is where shininess 32 landed.
constexpr float k_gpu_roughness[] = {0.05f, 0.12f, 0.22f, 0.35f,
                                     0.49f, 0.65f, 0.82f, 1.00f};
constexpr int k_gpu_roughness_count = static_cast<int>(std::size(k_gpu_roughness));

/// Turn one of Module 3's `scene_object`s into something the GPU renderer can draw.
///
/// **Read this function next to the top of `collect_triangles` and the port is
/// visible in twenty lines.** Both compute the same two matrices from the same
/// transform, by the same two functions, in the same order. What differs is what
/// happens next: the CPU version goes on to multiply every vertex by them, and
/// this one hands them to a shader that will.
[[nodiscard]] engine::gpu_draw_item make_draw_item(const engine::scene_object& obj,
                                                   const scene_mesh_cache::entry& mesh,
                                                   const scene_controls& ctl,
                                                   SDL_GPUTexture* texture)
{
    engine::gpu_draw_item item;
    item.mesh = &mesh.gpu;

    // T*R*S — Lesson 2.8's composition, and the SAME function the software path
    // calls. Not a re-derivation: `parent_from_local` is in math/transform.hpp and
    // both renderers include it.
    item.world_from_model = engine::parent_from_local(obj.xform);

    // The inverse transpose (Lesson 3.6), or the naive linear part on [J]. Two
    // thirds of this scene looks perfect either way, which is exactly why the bug
    // survives in real codebases — and now it survives on a GPU too.
    item.normal_from_model = ctl.correct_normals
        ? engine::normal_matrix(item.world_from_model)
        : engine::linear_of(item.world_from_model);

    // ---- The material -------------------------------------------------------
    //
    // DECODED TO LINEAR HERE, once per object per frame, rather than per pixel in
    // the shader. `obj.mat.tint` is an sRGB-encoded `Uint32` because that is what a
    // framebuffer holds and what a person types; it is about to multiply a
    // quantity of light, and Lesson 1.6's rule has not softened.
    // LESSON 6.5: five hand-written assignments became one call. `uniforms_of`
    // is the single place a material becomes GPU numbers, and it DERIVES the
    // `textured` flag from the albedo handle rather than from a second opinion
    // about whether a texture is bound.
    item.material = engine::uniforms_of(obj.mat);
    item.texture = texture;

    // ---- The pipeline this object needs -------------------------------------
    //
    // `closed` has been on `scene_object` since Lesson 3.4, where it was described
    // as belonging on a material in a real engine "because cull mode is pipeline
    // state and pipeline state is what a material *is*". Here it selects a
    // pipeline OBJECT, which is that sentence with the hedging removed.
    if (ctl.wireframe)
    {
        item.style = engine::surface_style::wireframe;
    }
    else if (ctl.cull_override == 1)
    {
        item.style = engine::surface_style::two_sided;
    }
    else if (ctl.cull_override == 2 || obj.closed)
    {
        item.style = engine::surface_style::solid;
    }
    else
    {
        item.style = engine::surface_style::two_sided;
    }

    return item;
}

int run_gpu_scene(SDL_Window* window, bool trace_and_exit)
{
    SDL_SetWindowTitle(window, "Module 3's Scene, on the GPU - Lesson 4.8");

    engine::gpu_device gpu;
    const engine::gpu_report rep = gpu.create(window, true);
    if (!rep.ok())
    {
        SDL_Log("The GPU scene cannot start: %s", engine::name_of(rep.status));
        SDL_Log("Run with --software for the Module 1-3 screen.");
        return 1;
    }
    gpu.log_report();

    // ---- The software rasterizer, still here --------------------------------
    //
    // Not a fallback and not nostalgia: it is the REFERENCE. Every claim Modules 2
    // and 3 made was measured against this code, and the only way to know the port
    // preserved them is to run both and compare. [V] puts them side by side.
    engine::framebuffer fb(k_fb_width, k_fb_height);
    engine::depth_buffer cpu_depth(k_fb_width, k_fb_height);

    engine::gpu_present_target present;
    if (!present.create(gpu, fb.width(), fb.height()))
    {
        SDL_Log("The GPU scene cannot start: the present target could not be created.");
        return 1;
    }

    // ---- Shaders ------------------------------------------------------------
    engine::gpu_shader scene_vs;
    engine::gpu_shader scene_fs;
    if (!scene_vs.load(gpu, "scene.vert", engine::shader_stage::vertex)
        || !scene_fs.load(gpu, "scene.frag", engine::shader_stage::fragment))
    {
        SDL_Log("The GPU scene cannot start: scene.vert/scene.frag did not load.");
        SDL_Log("Build the shaders (see Lesson 4.3) or run with --software.");
        present.destroy();
        gpu.destroy();
        return 1;
    }

    // ---- Depth, asked for rather than assumed (Lesson 4.7) ------------------
    const SDL_GPUTextureFormat depth_wanted[] = {
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    };
    const SDL_GPUTextureFormat depth_format =
        engine::supported_depth_format(gpu, depth_wanted, SDL_arraysize(depth_wanted));

    engine::gpu_scene_renderer renderer;
    if (!renderer.create(gpu, scene_vs.handle(), scene_fs.handle(), depth_format))
    {
        SDL_Log("The GPU scene cannot start: the pipelines were not created.");
        scene_vs.destroy();
        scene_fs.destroy();
        present.destroy();
        gpu.destroy();
        return 1;
    }
    SDL_Log("  depth format    : %s (%d bits)", engine::name_of(depth_format),
            engine::depth_bits(depth_format));
    SDL_Log("  pipelines       : 3 (solid, two-sided, wireframe) in %.3f ms total",
            renderer.create_ms());

    // ---- The floor's texture, on both sides ---------------------------------
    //
    // ONE image, built once by Lesson 3.9's `make_checker`, given to the software
    // sampler as a `texture` and uploaded to the device as a `gpu_texture`. The
    // two paths therefore read the same texels, which is the other half of what
    // makes the comparison honest (the first half was sharing the geometry).
    //
    // LESSON 6.5: it goes into a POOL, so a material can refer to it. The
    // software path still wants a pointer for its fill loop and gets one by
    // resolving the handle — once, here, not per pixel.
    engine::texture_pool floor_textures;
    const engine::texture_handle gpu_checker_source =
        floor_textures.insert(engine::make_checker(64, 8, 0xFFE8E2D6u, 0xFF3A4058u));
    const engine::texture& cpu_checker = *floor_textures.get(gpu_checker_source);

    engine::image_data checker_image;
    checker_image.width = 64;
    checker_image.height = 64;
    checker_image.source_channels = 4;
    checker_image.pixels.resize(64u * 64u * 4u);
    for (int y = 0; y < 64; ++y)
    {
        for (int x = 0; x < 64; ++x)
        {
            const Uint32 texel = cpu_checker.texel(x, y);
            const std::size_t o = (static_cast<std::size_t>(y) * 64u
                                 + static_cast<std::size_t>(x)) * 4u;
            checker_image.pixels[o + 0] = static_cast<std::uint8_t>((texel >> 16) & 0xFFu);
            checker_image.pixels[o + 1] = static_cast<std::uint8_t>((texel >> 8) & 0xFFu);
            checker_image.pixels[o + 2] = static_cast<std::uint8_t>(texel & 0xFFu);
            checker_image.pixels[o + 3] = 255;
        }
    }

    engine::gpu_texture gpu_checker;
    engine::gpu_sampler sampler_linear;
    engine::gpu_sampler sampler_nearest;
    bool texture_ok = false;
    {
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
        texture_ok = cb != nullptr
                  && gpu_checker.create_sampled(gpu, cb, checker_image, true, "floor checker")
                  && SDL_SubmitGPUCommandBuffer(cb)
                  && sampler_linear.create(gpu, engine::filter::linear,
                                           engine::address_mode::repeat)
                  && sampler_nearest.create(gpu, engine::filter::nearest,
                                            engine::address_mode::repeat);
        SDL_Log("  floor texture   : 64x64 checker, %s", texture_ok ? "uploaded" : "FAILED");
    }

    // ---- Module 3's scene, unchanged ----------------------------------------
    //
    // `build_scene`, `build_floor` and `load_model` are the SAME functions the
    // software demo calls, taken verbatim. Not one of them knows a GPU exists,
    // which is the strongest evidence available that a scene description and a
    // renderer are different things — and the argument Module 5's engine/demo
    // split is going to make at length.
    demo::scene_assets assets;
    assets.build();

    demo::floor_geometry floor;
    demo::build_floor(assets, floor, 1);

    demo::model_state model;
    model.generated = assets.store.insert_mesh("generated:torus",
                                               engine::make_torus(48, 24, 1.0f, 0.36f));
    demo::load_model(assets, model, demo::model_choice::torus, true);

    scene_mesh_cache meshes;
    scene_controls ctl;

    engine::lighting lights;
    lights.ambient = {0.06f, 0.07f, 0.10f};

    // The software reference's working storage — reused, never reallocated per
    // frame, exactly as the Module 3 demo does it.
    std::vector<engine::raster_triangle> cpu_tris;
    engine::projection_scratch scratch;

    SDL_Log("Keys: arrows orbit  [-]/[=] dolly  [A]/[D] the lamp  [0] reset");
    SDL_Log("      [C] scene  [L] model  [V] view (gpu/split/software)  [W] wireframe");
    SDL_Log("      [U] cull  [Z] depth test  [J] normal matrix  [H] specular  [E] exponent");
    SDL_Log("      [M] floor texture  [F] filter  [O] sort the draw list");
    SDL_Log("      [P] print the next frame's command stream  [Esc] quit");

    engine::clock clk;
    engine::input in;

    bool running = true;
    float t = 0.0f;
    Uint64 last_log = SDL_GetTicks();
    engine::draw_stats stats;
    double cpu_ms = 0.0;

    // Lesson 4.9. Recording is armed for ONE frame by [P] and disarmed as soon as
    // that frame is printed, because a log that runs every frame is a log nobody
    // reads and a per-frame cost nobody asked for. That is also how a real capture
    // works: you press a key, you get one frame.
    engine::frame_log flog;
    bool want_frame_log = trace_and_exit;   // `--trace` arms the very first frame

    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            in.feed_event(event);
            if (event.type == SDL_EVENT_QUIT) { running = false; }
        }

        clk.tick();
        in.update();
        t += clk.dt();

        if (in.key_pressed(SDL_SCANCODE_ESCAPE) || in.key_pressed(SDL_SCANCODE_Q))
        {
            running = false;
        }

        // ---- The camera and the lamp ---------------------------------------
        {
            constexpr float k_turn = 1.6f;
            constexpr float k_zoom = 4.0f;
            constexpr float k_light_speed = 1.4f;
            const float dt = clk.dt();

            if (in.key_down(SDL_SCANCODE_LEFT))  { ctl.camera.azimuth -= k_turn * dt; }
            if (in.key_down(SDL_SCANCODE_RIGHT)) { ctl.camera.azimuth += k_turn * dt; }
            if (in.key_down(SDL_SCANCODE_UP))    { ctl.camera.elevation += k_turn * dt; }
            if (in.key_down(SDL_SCANCODE_DOWN))  { ctl.camera.elevation -= k_turn * dt; }
            if (in.key_down(SDL_SCANCODE_MINUS)) { ctl.camera.radius += k_zoom * dt; }
            if (in.key_down(SDL_SCANCODE_EQUALS)) { ctl.camera.radius -= k_zoom * dt; }
            if (in.key_down(SDL_SCANCODE_A)) { ctl.light_azimuth -= k_light_speed * dt; }
            if (in.key_down(SDL_SCANCODE_D)) { ctl.light_azimuth += k_light_speed * dt; }

            // Lesson 2.9's degeneracy, clamped away: `look_at` builds its right
            // vector from `cross(up, backward)`, which is zero when the camera
            // looks straight down.
            ctl.camera.elevation = std::clamp(ctl.camera.elevation, -1.5f, 1.5f);
            ctl.camera.radius = std::clamp(ctl.camera.radius, 2.0f, 30.0f);
        }

        if (in.key_pressed(SDL_SCANCODE_0)) { ctl.camera = demo::orbit_camera{}; }

        if (in.key_pressed(SDL_SCANCODE_C))
        {
            ctl.scene = next_scene(ctl.scene);
            SDL_Log("[C] scene: %s", name_of(ctl.scene));
        }
        if (in.key_pressed(SDL_SCANCODE_L))
        {
            demo::load_model(assets, model, next_model(model.choice), true);
        }
        if (in.key_pressed(SDL_SCANCODE_V))
        {
            ctl.view_mode = (ctl.view_mode + 1) % 3;
            static const char* names[3] = {"GPU only", "SPLIT (software | GPU)",
                                           "software only"};
            SDL_Log("[V] view: %s", names[ctl.view_mode]);
        }
        if (in.key_pressed(SDL_SCANCODE_W))
        {
            ctl.wireframe = !ctl.wireframe;
            SDL_Log("[W] wireframe: %s  <- Module 2 spent two lessons on this;"
                    " here it is one enum value", ctl.wireframe ? "ON" : "off");
        }
        if (in.key_pressed(SDL_SCANCODE_U))
        {
            ctl.cull_override = (ctl.cull_override + 1) % 3;
            static const char* names[3] = {"AUTO (per object's `closed` flag)",
                                           "NONE (everything two-sided)",
                                           "BACK (everything, including sheets)"};
            SDL_Log("[U] cull: %s", names[ctl.cull_override]);
        }
        if (in.key_pressed(SDL_SCANCODE_Z))
        {
            ctl.depth_test = !ctl.depth_test;
            SDL_Log("[Z] depth attachment: %s%s", ctl.depth_test ? "ON" : "off",
                    ctl.depth_test ? "" : "  <- Lesson 3.1's problem, back");
        }
        if (in.key_pressed(SDL_SCANCODE_J))
        {
            ctl.correct_normals = !ctl.correct_normals;
            SDL_Log("[J] normal matrix: %s%s",
                    ctl.correct_normals ? "inverse transpose (correct)" : "the model matrix",
                    ctl.correct_normals ? ""
                        : "  <- watch the SLAB only; the icosahedron is uniform");
        }
        if (in.key_pressed(SDL_SCANCODE_H))
        {
            ctl.spec = (ctl.spec == engine::specular_model::blinn)
                ? engine::specular_model::phong
                : ((ctl.spec == engine::specular_model::phong)
                       ? engine::specular_model::none
                       : engine::specular_model::blinn);
            SDL_Log("[H] specular: %s",
                    ctl.spec == engine::specular_model::blinn ? "BLINN"
                        : (ctl.spec == engine::specular_model::phong ? "PHONG" : "none"));
        }
        if (in.key_pressed(SDL_SCANCODE_E))
        {
            ctl.shininess_step = (ctl.shininess_step + 1) % k_gpu_roughness_count;
            SDL_Log("[E] roughness: %.2f",
                    static_cast<double>(k_gpu_roughness[ctl.shininess_step]));
        }
        if (in.key_pressed(SDL_SCANCODE_M))
        {
            ctl.floor_textured = !ctl.floor_textured;
            SDL_Log("[M] floor texture: %s", ctl.floor_textured ? "ON" : "off");
        }
        if (in.key_pressed(SDL_SCANCODE_F))
        {
            ctl.smooth_texture = !ctl.smooth_texture;
            SDL_Log("[F] filter: %s", ctl.smooth_texture ? "LINEAR" : "NEAREST");
        }
        if (in.key_pressed(SDL_SCANCODE_O))
        {
            ctl.sort_by_pipeline = !ctl.sort_by_pipeline;
            SDL_Log("[O] draw order: %s", ctl.sort_by_pipeline
                        ? "SORTED by pipeline" : "as the scene was built");
        }
        if (in.key_pressed(SDL_SCANCODE_P))
        {
            want_frame_log = true;
            SDL_Log("[P] recording the next frame...");
        }

        // ---- The scene, built by Module 3's own code -----------------------
        engine::scene_object objects[demo::k_max_objects];
        const int object_count = demo::build_scene(objects, ctl.scene, demo::spin::about_y, t,
                                             assets, floor, model,
                                             k_gpu_roughness[ctl.shininess_step]);

        const engine::vec3 eye = ctl.camera.eye();
        const engine::mat4 view_from_world = ctl.camera.view();
        const engine::vec3 to_light = ctl.to_light();

        // The sign error light.hpp shouts about: `direction` is the direction the
        // light TRAVELS, so it is the negation of the direction toward it.
        lights.key.direction = -to_light;
        lights.key.colour = {1.0f, 0.97f, 0.90f};
        // Lesson 6.2: `intensity` became `irradiance`, and the value became pi.
        // A light of "intensity 1" was always a light of irradiance pi; the
        // pi has moved into the BRDF where it belongs, so it must now be
        // written down here. Same picture, stated honestly.
        lights.key.irradiance = engine::k_reference_irradiance;

        // ---- Turn the scene into draws -------------------------------------
        engine::gpu_draw_item items[demo::k_max_objects];
        const scene_mesh_cache::entry* entries[demo::k_max_objects] = {};
        int item_count = 0;

        for (int i = 0; i < object_count; ++i)
        {
            // FLAT for the box-like scenes and SMOOTH for anything meant to look
            // curved — a decision that used to be a keypress ([Q] in Lesson 3.8)
            // and is now a property of the vertex buffer. The icosahedron is the
            // interesting case and it goes FLAT: it is a faceted solid, and
            // averaging its corner normals rounds off twenty faces into a ball.
            const engine::normal_style style =
                (ctl.scene == demo::scene_kind::model) ? engine::normal_style::smooth
                                                 : engine::normal_style::flat;

            const scene_mesh_cache::entry* mesh =
                meshes.get(gpu, assets.store, objects[i].geometry, style);
            if (mesh == nullptr) { continue; }

            const bool is_floor = (ctl.scene == demo::scene_kind::floor);
            const bool textured = (is_floor && ctl.floor_textured && texture_ok);
            SDL_GPUTexture* tex = textured ? gpu_checker.handle() : nullptr;

            // LESSON 6.5: THE MATERIAL SAYS WHETHER THERE IS AN ALBEDO MAP, and
            // everything downstream derives from that one field. This decision is
            // still made here rather than in `build_scene`, because it is a
            // keypress ([T]) rather than a property of the scene — but it is
            // recorded ON THE MATERIAL, so `uniforms_of` cannot disagree with the
            // texture actually bound. Writing the flag separately, next to a
            // pointer that is chosen separately, is precisely the pair of opinions
            // this lesson exists to collapse.
            engine::scene_object obj = objects[i];
            obj.mat.albedo_map = textured ? gpu_checker_source : engine::texture_handle{};

            entries[item_count] = mesh;
            items[item_count] = make_draw_item(obj, *mesh, ctl, tex);
            ++item_count;
        }

        // [O]. A stable sort by pipeline, which is the cheapest useful draw-list
        // policy there is and the one every engine starts with. The stats below
        // report what it saved; on a four-object scene the answer is "one bind",
        // and the reason to do it anyway is that the number scales with the scene
        // and the sort does not.
        if (ctl.sort_by_pipeline)
        {
            std::stable_sort(items, items + item_count,
                             [](const engine::gpu_draw_item& a,
                                const engine::gpu_draw_item& b) {
                                 return static_cast<int>(a.style) < static_cast<int>(b.style);
                             });
        }

        // ---- The software reference, when the view needs it -----------------
        const bool want_cpu = (ctl.view_mode != 0);
        if (want_cpu)
        {
            const Uint64 c0 = SDL_GetPerformanceCounter();

            fb.clear(engine::pack_argb(18, 20, 28));
            cpu_depth.clear();

            const engine::projector pr{
                engine::perspective(scene_controls::k_fovy, 16.0f / 9.0f,
                                    scene_controls::k_near, scene_controls::k_far),
                demo::k_full_viewport, engine::near_mode::clip};

            // ONE draw_triangles CALL PER OBJECT, and this is the port pushing
            // back on the software path rather than the other way round. Module 3
            // gathered every object's triangles into one array and filled them in
            // one loop, because it could: a software rasterizer can change its
            // material and its cull mode between two triangles. To be COMPARABLE
            // with the GPU it has to stop doing that, because the GPU cannot.
            for (int i = 0; i < item_count; ++i)
            {
                if (entries[i] == nullptr) { continue; }

                engine::scene_object one = objects[i];
                one.geometry = entries[i]->cpu;   // the SAME geometry the GPU got

                engine::fill_style style;
                style.interp = engine::interpolation::perspective;
                style.shade = engine::shading::lit;
                style.lights = &lights;
                style.model = ctl.spec;
                style.eye = eye;
                style.cull = (items[i].style == engine::surface_style::solid)
                    ? engine::cull_mode::back : engine::cull_mode::none;
                if (items[i].material.textured > 0.5f)
                {
                    style.albedo.image = &cpu_checker;
                    style.albedo.samp.texel_filter = ctl.smooth_texture
                        ? engine::filter::linear : engine::filter::nearest;
                }

                const engine::render_options cpu_opts{
                    .shading = engine::shade_eval::per_pixel,
                    .specular = ctl.spec,
                    .correct_normal_matrix = ctl.correct_normals};
                engine::collect_triangles(cpu_tris, scratch, {&one, 1}, assets.meshes(),
                                          {view_from_world, eye}, pr, lights, cpu_opts);

                engine::draw_triangles(fb, ctl.depth_test ? &cpu_depth : nullptr,
                               cpu_tris, false, style, nullptr, nullptr);
            }

            const Uint64 c1 = SDL_GetPerformanceCounter();
            cpu_ms = static_cast<double>(ticks_to_ms(c0, c1));
        }

        // ---- The frame ------------------------------------------------------
        SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(gpu.handle());
        if (cb == nullptr)
        {
            SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
            break;
        }

        if (want_frame_log) { flog.begin(); }

        {
            // A debug group per PHASE of the frame, so a capture reads as five
            // headings rather than as two hundred calls. The scope is what pops
            // it — see gpu_debug.hpp on why that matters more on Metal than it
            // looks like it should.
            const engine::debug_group g(cb, "upload", &flog);
            if (want_cpu)
            {
                present.upload(cb, fb);
                flog.record(engine::gpu_event_kind::copy, "framebuffer -> texture",
                            0, present.last_upload_bytes());
            }
        }

        SDL_GPUTexture* swap = nullptr;
        Uint32 swap_w = 0;
        Uint32 swap_h = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(cb, window, &swap, &swap_w, &swap_h))
        {
            SDL_Log("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
            break;
        }

        if (swap != nullptr)
        {
            // Pass 1 exists to CLEAR, and only to clear — no pipeline, no draw.
            // Lesson 4.2's point that a clear is a property of BEGINNING a pass
            // rather than a thing you draw.
            SDL_GPUColorTargetInfo clear_target{};
            clear_target.texture = swap;
            clear_target.clear_color = SDL_FColor{0.018f, 0.020f, 0.030f, 1.0f};
            clear_target.load_op = SDL_GPU_LOADOP_CLEAR;
            clear_target.store_op = SDL_GPU_STOREOP_STORE;
            {
                const engine::debug_group g(cb, "clear", &flog);
                flog.record(engine::gpu_event_kind::pass_begin, "clear (no draws)");
                SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cb, &clear_target, 1, nullptr));
                flog.record(engine::gpu_event_kind::pass_end, nullptr);
            }

            // Where each renderer's picture goes. In split mode the window is cut
            // down the middle and each half is letterboxed to 16:9 inside its own
            // half — `fit_centred` called twice against a half-width rectangle,
            // which is the whole implementation of a comparison view.
            const Uint32 half = swap_w / 2u;
            engine::blit_rect cpu_rect{};
            SDL_GPUViewport gpu_vp{};
            bool draw_gpu = true;

            if (ctl.view_mode == 0)
            {
                const engine::blit_rect fit = engine::fit_centred(16, 9, swap_w, swap_h);
                gpu_vp.x = static_cast<float>(fit.x);
                gpu_vp.y = static_cast<float>(fit.y);
                gpu_vp.w = static_cast<float>(fit.w);
                gpu_vp.h = static_cast<float>(fit.h);
            }
            else if (ctl.view_mode == 1)
            {
                cpu_rect = engine::fit_centred(static_cast<Uint32>(fb.width()),
                                               static_cast<Uint32>(fb.height()),
                                               half, swap_h);
                const engine::blit_rect fit =
                    engine::fit_centred(16, 9, swap_w - half, swap_h);
                gpu_vp.x = static_cast<float>(half + fit.x);
                gpu_vp.y = static_cast<float>(fit.y);
                gpu_vp.w = static_cast<float>(fit.w);
                gpu_vp.h = static_cast<float>(fit.h);
            }
            else
            {
                cpu_rect = engine::fit_centred(static_cast<Uint32>(fb.width()),
                                               static_cast<Uint32>(fb.height()),
                                               swap_w, swap_h);
                draw_gpu = false;
            }
            gpu_vp.min_depth = 0.0f;
            gpu_vp.max_depth = 1.0f;   // SDL_GPU's depth range — conventions §4

            // Outside any pass, because a blit IS a pass.
            if (want_cpu && cpu_rect.w > 0)
            {
                const engine::debug_group g(cb, "blit (software picture)", &flog);
                present.blit_region(cb, swap, cpu_rect, false);
                flog.record(engine::gpu_event_kind::blit, "framebuffer -> swapchain",
                            0, static_cast<Uint32>(cpu_rect.w) * cpu_rect.h * 4u);
            }

            if (draw_gpu && item_count > 0)
            {
                SDL_GPUColorTargetInfo over{};
                over.texture = swap;
                over.load_op = SDL_GPU_LOADOP_LOAD;   // keep the clear and the blit
                over.store_op = SDL_GPU_STOREOP_STORE;

                const bool have_depth = ctl.depth_test
                                     && renderer.ensure_depth(gpu, swap_w, swap_h);
                SDL_GPUDepthStencilTargetInfo depth_info{};
                if (have_depth) { depth_info = renderer.depth_target_info(); }

                // NOTE the placement: the group opens BEFORE the pass and closes
                // after it, which is legal everywhere and is the shape SDL's own
                // Metal caveat is happiest with. Groups that begin inside a pass
                // must also end inside it.
                const engine::debug_group g(cb, "scene", &flog);

                SDL_GPURenderPass* pass =
                    SDL_BeginGPURenderPass(cb, &over, 1, have_depth ? &depth_info : nullptr);
                flog.record(engine::gpu_event_kind::pass_begin,
                            have_depth ? "colour + depth" : "colour only");

                SDL_SetGPUViewport(pass, &gpu_vp);

                // The aspect ratio comes from the rectangle actually being drawn
                // into, which is what Lesson 4.6's uniform block bought: a
                // compile-time 16:9 could not have survived a split view.
                const float aspect = (gpu_vp.h > 0.0f) ? gpu_vp.w / gpu_vp.h : 16.0f / 9.0f;

                engine::camera_uniforms camera{
                    engine::perspective(scene_controls::k_fovy, aspect,
                                        scene_controls::k_near, scene_controls::k_far)
                    * view_from_world};

                engine::scene_light_uniforms light{};
                light.to_light = to_light;
                // Colour times IRRADIANCE (Lesson 6.2's rename), so the shader
                // receives `E_perp` per channel and applies the cosine and the
                // BRDF itself — the same three factors as `engine::shade()`.
                light.key = engine::vec3{lights.key.colour.r * lights.key.irradiance,
                                         lights.key.colour.g * lights.key.irradiance,
                                         lights.key.colour.b * lights.key.irradiance};
                light.ambient = engine::vec3{lights.ambient.r, lights.ambient.g,
                                             lights.ambient.b};
                light.eye_world = eye;
                // Lesson 6.4 added a fourth code. The mapping is spelled out
                // rather than cast from the enum, because an enum's underlying
                // value is a C++ detail and the shader's contract is a NUMBER —
                // reordering the enum must not silently re-map the shader.
                light.spec_model =
                      (ctl.spec == engine::specular_model::none)  ? 0.0f
                    : (ctl.spec == engine::specular_model::phong) ? 1.0f
                    : (ctl.spec == engine::specular_model::blinn) ? 2.0f
                    : 3.0f;

                // LESSON 6.1. Asked, never assumed — and asked of the device
                // rather than of a constant, because the answer is a property of
                // this machine's swapchain and was different on this one before
                // 6.1 changed what we request. Getting it wrong in either
                // direction is a picture-wide error: too dark one way, milky the
                // other.
                light.encode_output = gpu.report().output_encodes_in_hardware ? 0.0f : 1.0f;

                stats = renderer.render(cb, pass, items, item_count, camera, light,
                                        ctl.smooth_texture ? sampler_linear.handle()
                                                           : sampler_nearest.handle(),
                                        &flog);

                flog.record(engine::gpu_event_kind::pass_end, nullptr);
                SDL_EndGPURenderPass(pass);
            }
        }

        if (!SDL_SubmitGPUCommandBuffer(cb))
        {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
            break;
        }

        // Printed AFTER submission, so the log describes a frame that was
        // actually issued rather than one still being built — and so the printing
        // itself is not inside the region being described.
        if (want_frame_log)
        {
            flog.end();
            flog.print();

            // The cross-check that makes the log worth trusting: two independent
            // readings of the same frame, from the same statements.
            SDL_Log("  cross-check: log says %d draw(s) / %u uniform bytes;"
                    " draw_stats says %d / %u  %s",
                    flog.draws(), flog.uniform_bytes(), stats.draws,
                    stats.uniform_bytes,
                    (flog.draws() == stats.draws
                     && flog.uniform_bytes() == stats.uniform_bytes) ? "AGREE" : "DISAGREE");
            want_frame_log = false;

            // `--trace` is the headless mode: dump one frame and stop. A frame
            // debugger cannot run in CI and this can, which is the whole reason
            // to have written the small version.
            if (trace_and_exit) { running = false; }
        }

        const Uint64 now = SDL_GetTicks();
        if (now - last_log >= 1000)
        {
            last_log = now;
            SDL_Log("%5.1f fps | %s | %d items -> %d draws, %d pipeline binds"
                    " (%d if sorted), %d texture binds, %u tris, %u uniform bytes"
                    " | software reference %.2f ms",
                    clk.fps(), name_of(ctl.scene), stats.items, stats.draws,
                    stats.pipeline_binds, stats.ideal_pipeline_binds,
                    stats.texture_binds, stats.triangles, stats.uniform_bytes,
                    want_cpu ? cpu_ms : 0.0);
        }
    }

    // Reverse of creation, and stated rather than left to the compiler for the
    // reason Lesson 4.2 gave: a dependency that is visible is a dependency that
    // survives the next edit.
    meshes.destroy();
    gpu_checker.destroy();
    sampler_linear.destroy();
    sampler_nearest.destroy();
    renderer.destroy();
    scene_vs.destroy();
    scene_fs.destroy();
    present.destroy();
    gpu.destroy();
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    // ---- Which program is this? --------------------------------------------
    //
    // LESSON 4.8 INVERTS THE DEFAULT, and Lesson 4.2 said it would: "From Lesson
    // 4.8 the GPU path becomes the default and this branch inverts; until then the
    // software demo is what `engine` means." It is now the other way round.
    //
    //     engine              Module 3's scene, drawn by the GPU        (4.8)
    //     engine --software   Module 3's scene, drawn by the CPU, with the
    //                         HUD and all five demos on [Tab]           (1.8-3.10)
    //     engine --probe      the instrument Lessons 4.2-4.7 were built on
    //     engine --gpu        an alias for --probe, kept because every one of
    //                         those six lessons tells you to type it
    //     engine --trace      print one frame's command stream and exit
    //                         (Lesson 4.9; the mode that works without a GUI)
    //
    // The software path is not deprecated and is not going away. It is the
    // REFERENCE: every measured claim in Modules 2 and 3 was made against it, and
    // a port whose reference has been deleted is a port nobody can check. The
    // split view inside the GPU scene runs both at once for exactly this reason.
    //
    // Still a flag rather than a key, because the choice has to be made before the
    // window belongs to anybody — a window is claimed by an SDL_GPU device or
    // driven by an SDL_Renderer, never both (Lesson 4.2).
    enum class program { scene, software, probe };
    program which_program = program::scene;
    bool trace_and_exit = false;
    const char* shot_path = nullptr;   ///< Lesson 5.1's characterization shot

    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--software") == 0) { which_program = program::software; }
        else if (SDL_strcmp(argv[i], "--probe") == 0) { which_program = program::probe; }
        else if (SDL_strcmp(argv[i], "--gpu") == 0) { which_program = program::probe; }
        else if (SDL_strcmp(argv[i], "--trace") == 0) { trace_and_exit = true; }
        // Lesson 5.1: --shot PATH. Render six pinned frames and exit, so that a
        // refactor has something it can be checked against. Takes the NEXT
        // argument as the output path, and defaults if there is not one.
        else if (SDL_strcmp(argv[i], "--shot") == 0)
        {
            shot_path = (i + 1 < argc) ? argv[++i] : "shot.ppm";
        }
    }
    const bool want_gpu = (which_program == program::probe);

    // ---- Lesson 5.2: the platform ------------------------------------------
    //
    // Everything that used to be here — SDL_Init, the window, the renderer, the
    // vsync call, the streaming texture, and four teardown paths that each had
    // to unwind a different amount — is `engine::platform` now. What is left is
    // the part that was ever this program's own business: WHICH surface, and
    // therefore what kind of program this run is.
    //
    // NOTE WHAT SANDBOX DID NOT ADOPT: `engine::app`. This file keeps its own
    // main() and its own `while` loop, deliberately, and Lesson 5.2 §6 argues
    // it. In one sentence: this program chooses between three incompatible loops
    // at runtime and is the instrument every measurement in Modules 2 to 4 was
    // taken with, so it wants the arrangement where the loop is visible in front
    // of you. The framework path is for programs that want a loop; sandbox is a
    // program that wants THIS loop.
    engine::platform plat;

    // Lesson 5.1's characterization shot, now honestly headless: no window, no
    // renderer, and — the part that used to be a lie — no video subsystem. The
    // old code called SDL_Init(SDL_INIT_VIDEO) first and then never used it, so
    // the shot could not run on a machine without a display, which is most of
    // what a shot is for.
    if (shot_path != nullptr)
    {
        if (!plat.start({.title = "sandbox — reference shot",
                         .draw_to = engine::surface::headless,
                         .argc = argc, .argv = argv}))
        {
            return 1;
        }
        return demo::write_reference_shot(shot_path);
    }

    // Lesson 4.2's fork in the road, and it still has to be made before anybody
    // owns the window. `surface::gpu` is exactly "make a window and claim it for
    // nobody" — the platform deliberately does not create a renderer, because
    // creating one would poison the window for SDL_CreateGPUDevice.
    // Lesson 5.3. `--trace` asks for the GPU command log, and that log is emitted
    // on the `gpu` category at INFO — which 5.3 made silent by default along with
    // every other engine category. So the flag that requests the report also
    // raises the level that lets it out. A diagnostic switch that silently does
    // nothing is worse than no switch, and this is the one line that prevents it.
    if (trace_and_exit && !engine::set_log_levels("gpu=info"))
    {
        return 1;
    }

    const bool gpu_program = want_gpu || (which_program == program::scene);

    if (!plat.start({.title = gpu_program ? "Engine — Module 4"
                                          : "Engine — the software reference",
                     .draw_to = gpu_program ? engine::surface::gpu
                                            : engine::surface::renderer,
                     .fb_width = gpu_program ? 0 : k_fb_width,
                     .fb_height = gpu_program ? 0 : k_fb_height,
                     .argc = argc, .argv = argv}))
    {
        return 1;
    }

    SDL_Window* const window = plat.window();

    if (gpu_program)
    {
        return (which_program == program::scene)
            ? run_gpu_scene(window, trace_and_exit)
            : run_gpu_probe(window);
    }

    SDL_Log("Software screen (the reference). Run with no flag for Lesson 4.8's GPU"
            " scene, or --probe for Lessons 4.2-4.7's instrument.");

    SDL_Renderer* const renderer = plat.renderer();

    // Borrowed references, so the four thousand lines below read exactly as they
    // did. The platform owns these; this is a name, not a copy.
    engine::framebuffer& fb = plat.fb();
    engine::clock& clk = plat.time();
    engine::input& in = plat.in();
    engine::fixed_step& stepper = plat.steps();

    // Lesson 3.1's depth attachment. Same dimensions as the colour buffer,
    // allocated once and cleared every frame — never reallocated, because a
    // per-frame allocation of 230 KB is a per-frame page fault storm for a buffer
    // whose size cannot change.
    engine::depth_buffer scene_depth(k_fb_width, k_fb_height);

    // A second colour+depth pair, used only to run the OTHER hidden-surface
    // algorithm on the same geometry so the two can be compared pixel for pixel.
    // Purely a teaching instrument; a real renderer has one of each.
    engine::framebuffer scratch_fb(k_fb_width, k_fb_height);
    engine::depth_buffer scratch_depth(k_fb_width, k_fb_height);


    // ---- Demo state --------------------------------------------------------
    screen which = screen::scene;
    demo::spin cube_mode = demo::spin::about_z;     ///< Lesson 2.6 — z shows 2.8's shear best
    float cube_t = 0.6f;
    bool cube_animating = true;
    float cube_point_w = 1.0f;          ///< 1 = positions (correct); 0 = Lesson 2.6
    float cube_dir_w = 0.0f;            ///< 0 = directions (correct); 1 = the normal bug
    engine::mat3 cube_m;

    // ---- Lesson 2.8's scene ------------------------------------------------
    engine::trs_order order = engine::trs_order::trs;   ///< [O] — only the first is right
    int selected = 0;                   ///< [X] — which object the HUD reports on
    engine::scene_object scene[demo::k_max_objects];
    int scene_count = 0;                ///< how many of them this scene uses
    engine::mat4 selected_m;            ///< the selected object's model matrix
    engine::vec3 selected_world;        ///< one model vertex, carried into world space
    float selected_axis_len = 0.0f;     ///< |model x axis| in world units
    float selected_corner = 0.0f;       ///< angle between model x and y, in degrees

    /// Vertex 0 of the SELECTED object's mesh — the one the HUD narrates through
    /// every space, refreshed each frame because [X] can change which mesh it is.
    engine::vec3 selected_probe;
    // Lesson 5.4: the HUD's geometry facts, read once where the mesh is resolved
    // rather than four times at four points of the HUD. A handle can be copied
    // anywhere; the RESOLUTION should happen once, at a place that can say "this
    // did not resolve" — which is the same rule `collect_triangles` follows.
    std::size_t selected_verts = 0;
    std::size_t selected_tris = 0;
    std::size_t selected_idx = 0;

    // ---- Lesson 2.9's camera -----------------------------------------------
    demo::orbit_camera cam;                   ///< arrow keys orbit; [-]/[=] dolly
    engine::mat4 view_from_world;       ///< look_at(eye, target, up), rebuilt each frame
    engine::vec3 selected_view;         ///< the probe vertex carried on into VIEW space
    engine::vec4 selected_clip;         ///< …and on into CLIP space (before the divide)
    engine::vec3 selected_ndc;          ///< …then NDC (after the perspective divide)
    engine::vec3 selected_screen;       ///< …and finally SCREEN pixels + depth (2.11's viewport)

    // Lesson 2.10's two projections. Perspective is the lesson's subject; the
    // orthographic one is the pre-2.10 behaviour, kept on [P] so the difference
    // can be toggled rather than described.
    bool use_perspective = true;        ///< [P] toggles perspective vs orthographic
    const engine::mat4 scene_perspective =
        engine::perspective(demo::k_scene_fovy, demo::k_scene_aspect, demo::k_scene_near, demo::k_scene_far);
    const engine::mat4 scene_orthographic = demo::demo_orthographic();

    // ---- Lesson 3.1 --------------------------------------------------------
    hidden_surface hs = hidden_surface::zbuffer;   ///< [F]
    demo::scene_kind scene_mode = demo::scene_kind::solids;    ///< [C]
    engine::depth_format depth_fmt = engine::depth_format::f32;   ///< [B]
    std::vector<engine::raster_triangle> scene_tris;       ///< reused, never reallocated per frame
    engine::projection_scratch scratch;                    ///< ditto, for the per-vertex arrays
    int painter_wrong = 0;                         ///< px where the two algorithms disagree
    engine::depth_range shown_depth;                       ///< what the depth view actually contained

    // ---- Lessons 5.4 and 5.5 -----------------------------------------------
    //
    // One owner for every mesh in this demo, declared before anything that names
    // one. Everything below that used to hold geometry now holds a handle into
    // here — the floor, the model, the round-trip control, and every
    // `scene_object` `build_scene` writes.
    //
    // As of 5.5 the owner is `engine::asset_store`: it brings the search path,
    // the name -> handle map, and the only `unload` in the program. [Bksp]
    // exercises it.
    demo::scene_assets assets;

    // ---- Lesson 3.2 --------------------------------------------------------
    engine::interpolation interp = engine::interpolation::perspective;   ///< [I]
    demo::floor_geometry floor;                          ///< rebuilt only when [T] changes it
    int floor_cells = 1;                           ///< quads per side; [T] cycles
    int interp_wrong = 0;                          ///< px where affine and perspective differ

    // ---- Lesson 3.3 --------------------------------------------------------
    engine::near_mode near_handling = engine::near_mode::clip;     ///< [K]
    engine::collect_stats scene_stats;             ///< what the vertex stage measured
    std::vector<engine::raster_triangle> compare_tris;     ///< the reference render's own geometry
    int near_wrong = 0;                            ///< px this near mode gets wrong vs clipping

    // ---- Lesson 3.4 --------------------------------------------------------
    engine::cull_choice culling = engine::cull_choice::none;       ///< [U]
    engine::cull_stats scene_cull;                         ///< kept / culled / disagreeing, this frame
    int cull_wrong = 0;                            ///< px the current cull mode gets wrong

    // ---- Lesson 3.5 --------------------------------------------------------
    demo::model_state model;                             ///< [L] chooses; loaded on the spot
    int roundtrip_wrong = 0;                       ///< px between the loaded and generated torus

    // ---- Lesson 3.6 --------------------------------------------------------
    engine::shade_eval eval = engine::shade_eval::gouraud;         ///< [G] — palette / flat / Gouraud / per-pixel
    engine::normal_source nsrc = engine::normal_source::vertex;    ///< [Q] — face vs vertex normals
    int grid_wrong = 0;                            ///< px this cell differs from per-pixel by
    double shade_ns = 0.0;                         ///< smoothed cost of the scene fill, ns
    bool correct_normals = true;                   ///< [J] — inverse transpose vs the naive M
    int normal_wrong = 0;                          ///< px the naive normal transform costs

    /// The one light. Its direction is described by two angles for the same reason
    /// the camera's is (Lesson 2.9): an angle pair cannot drift away from being a
    /// unit vector, and it is what a person actually wants to adjust.
    ///
    /// The elevation is fixed and the azimuth is on [A]/[D], which is enough to make
    /// the point that matters: the shading follows the LIGHT. Orbit the camera with
    /// the arrow keys and nothing about the shading changes at all — Lambert does
    /// not depend on where you are standing, and Lesson 3.7's specular will be the
    /// first term that does.
    float light_azimuth = 0.85f;
    constexpr float k_light_elevation = 0.70f;     ///< ~40 degrees above the horizon
    engine::lighting lights;

    // ---- Lesson 3.7 --------------------------------------------------------
    engine::specular_model spec_model = engine::specular_model::cook_torrance;   ///< [H]
    int shininess_step = 4;                        ///< [E] indexes k_shininess below
    int model_wrong = 0;                           ///< px Phong and Blinn disagree about
    int spec_peak = 0;                             ///< brightest pixel in the viewport, 0..255

    /// The exponents [E] cycles through. Powers of two, because that is how the
    /// parameter behaves: each step roughly halves the width of the highlight, so a
    /// linear slider would spend most of its travel on differences you cannot see.
    /// 2 is a damp, broad sheen; 32 a polished plastic; 256 close to a mirror.
    // Lesson 6.4: roughness, not an exponent. See `k_gpu_roughness` for why the
    // spacing changed as well as the numbers.
    constexpr float k_roughness[] = {0.05f, 0.12f, 0.22f, 0.35f,
                                     0.49f, 0.65f, 0.82f, 1.00f};
    constexpr int k_roughness_count = static_cast<int>(std::size(k_roughness));

    // ---- Lesson 3.9 --------------------------------------------------------
    demo::albedo_source albedo = demo::albedo_source::checker;   ///< [M] — which image, or the rule
    engine::sampler samp;                            ///< [S] filter, [R] address, [1] origin
    bool uv_flip_on_load = true;                     ///< [2] — OBJ v-up to texture v-down
    demo::texture_set textures;                            ///< the three images, built once
    int texel_wrong = 0;                             ///< px the half-texel error costs
    int filter_wrong = 0;                            ///< px bilinear and nearest disagree about

    // ---- Lesson 4.1 --------------------------------------------------------
    engine::traversal walk = engine::traversal::scanline;   ///< [5]
    engine::quad_stats scene_quads;                  ///< lanes shaded vs covered

    // ---- Lesson 3.10 -------------------------------------------------------
    engine::profiler prof;                           ///< the frame budget
    bool show_budget = true;                         ///< [3]
    engine::encode_mode encode = engine::encode_mode::fast;   ///< [4]
    int encode_wrong = 0;                            ///< px the fast encode moves
    double fill_ns_per_px = 0.0;                     ///< the fill's own unit cost
    int covered_px = 0;                              ///< how many pixels the scene painted

    SDL_Log("profiler: counter resolves to %.2f ns, a scope_timer costs %.2f ns "
            "-> instrument nothing shorter than %.2f us",
            prof.resolution_ns(), prof.overhead_ns(),
            std::max(prof.resolution_ns(), prof.overhead_ns()) * 100.0 / 1000.0);

    textures.build();
    assets.build();

    // The control mesh, built once. `assets/torus.obj` was written from exactly this
    // call, so "loaded == generated" is a real end-to-end check of writer and reader
    // together — and it is a claim the demo re-tests on every frame it is shown.
    model.generated = assets.store.insert_mesh("generated:torus",
                                               engine::make_torus(48, 24, 1.0f, 0.4f));
    demo::load_model(assets, model, demo::model_choice::torus, uv_flip_on_load);

    xform basis_mode = xform::rotate;   ///< Lesson 2.5
    float basis_t = 0.6f;               ///< the one parameter every mode reads
    bool basis_animating = false;
    engine::mat2 basis_m;               ///< the matrix currently on screen
    int basis_area_px = 0;              ///< its unit square, measured in pixels
    tri_mode mode = tri_mode::filled;
    bool use_fill_rule = true;
    bool spinning = true;
    float phase = 0.6f;

    bool linear_blend = true;      ///< Lesson 2.4: which space corner colours mix in
    float stripe_bands = 3.5f;     ///< magnifier band count — [ and ] sweep it
    int doubled_px = 0;
    int bias_differ = 0;           ///< magnifier cells the fill-rule bias gets wrong
    Uint32 centroid_px = 0;        ///< the shaded triangle's centre pixel, read back
    double tri_ns_avg = 0.0;

    engine::barycentric probe;
    int probe_x = 0;
    int probe_y = 0;

    line_fn line_algo = engine::draw_line;
    const char* line_algo_name = "Bresenham (int)";

    const Uint32 k_bg = engine::pack_argb(12, 14, 20);

    SDL_Log("Triangles: [1] filled [2] wireframe [3] half-planes [4] weights [5] iso-lines");
    SDL_Log("  [4]/[5] follow the mouse: the three sub-triangles ARE the three weights. [R] fill rule.");
    SDL_Log("  [6] Gouraud (three corner colours) [7] uv checker — [M] switches blend space");
    SDL_Log("Basis (2.5): [Z] transform  [,] [.] adjust  [0] reset  [Space] animate");
    SDL_Log("Scene (3.1-3.3): [F] wireframe/painter/z-buffer/depth  [C] scene  [B] depth format");
    SDL_Log("  [I] affine/perspective interpolation  [T] floor tessellation");
    SDL_Log("  [K] near plane: clip / drop / none - on the floor scene, hold [=] to walk into it");
    SDL_Log("  [U] cull: none / back / front / back-by-dot(n,fwd) (the classic bug)");
    SDL_Log("  [L] load a model: torus.obj / cube.obj / twisted.obj / quirks.obj / generated");
    SDL_Log("      …the first lap reads files; every lap after it is a cache hit (5.5)");
    SDL_Log("  [Bksp] UNLOAD the current model out from under the scene — watch it vanish");
    SDL_Log("  [G] evaluate: debug palette / flat / Gouraud (per-vertex) / PER-PIXEL");
    SDL_Log("  [Q] normal source: face (cross product) / vertex (as authored)");
    SDL_Log("  [J] normal matrix: inverse-transpose (correct) vs the naive model matrix");
    SDL_Log("  [A]/[D] swing the light. With [H] off the camera does NOT change the shading -");
    SDL_Log("          Lambert is view-independent. Turn [H] on and orbiting moves the highlight.");
    SDL_Log("  [H] specular: none / Phong / Blinn / Cook-Torrance   [E] roughness");
    SDL_Log("  [M] albedo: procedural rule / checker / uv grid / fine checker (on floor + model)");
    SDL_Log("  [S] filter: bilinear / nearest   [R] address: repeat / mirrored / clamp");
    SDL_Log("  [1] texel origin: centre (correct) vs corner - half a texel, invisible under [S] nearest");
    SDL_Log("  [2] uv v flip on import: OBJ counts v upwards, a texture counts it downwards");
    SDL_Log("  [3] the frame budget: which phase the frame is actually spent in");
    SDL_Log("  [4] sRGB encode: fitted sqrt chain (fast) vs std::pow (exact) - watch `fill`");
    SDL_Log("  [5] traversal: scanline / 2x2 quads / quads with the helper lanes shown");
    SDL_Log("  [arrows] orbit  [-]/[=] dolly  [P] persp/ortho  [O] model order  [X] object");
    SDL_Log("  [Z] rotation axis  [,] [.] t  [Space] demo::spin  [W]/[N] the 2.7 w bugs");
    SDL_Log("[Tab] cycles demos: scene (2.6-3.3) -> basis (2.5) -> triangles -> lines");
    SDL_Log("Pong (1.8) is its own program now: ./build/demos/pong");
    SDL_Log("[V] vsync · [Y] throttle · [Esc] quit");

    while (plat.running())
    {
        // Lesson 3.10. The frame starts HERE — before the event drain, because
        // draining events is work the frame does and a budget that starts after it
        // has a hole in it by construction. It ends just before SDL_RenderPresent,
        // so the total is *our* work and does not include the vsync wait; the HUD
        // prints the wall-clock frame time beside it so the gap between the two is
        // visible rather than hidden.
        prof.begin_frame();

        // The drain is still written out here rather than replaced by
        // `plat.pump()`, and the difference is the whole reason `handle()` and
        // `pump()` are two functions. `pump()` drains the queue and tells you
        // nothing; this loop hands each event to the platform AND keeps it, which
        // a program with per-event business of its own needs. Sandbox has none
        // left today — the quit cases moved into `handle` — but it is the file
        // that grows a drag-and-drop handler first, and the shape should be here
        // when it does.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            plat.handle(event);
        }

        // Lesson 1.2's contract and 1.3's, in one call: tick the clock, publish
        // the input snapshot, and hand the elapsed time to the accumulator — in
        // that order, which is now impossible to get wrong here because it is not
        // written here.
        plat.begin_frame();

        if (in.key_pressed(SDL_SCANCODE_ESCAPE)) { plat.request_quit(); }

        if (in.key_pressed(SDL_SCANCODE_TAB))
        {
            which = next_demo(which);
            SDL_SetWindowTitle(window,
                which == screen::scene     ? "The Z-Buffer — Module 3"
              : which == screen::basis     ? "Basis Transforms — Module 2"
              : which == screen::triangles ? "Triangles — Module 2"
                                           : "Lines — Module 2");
        }

        if (in.key_pressed(SDL_SCANCODE_V) && !plat.set_vsync(!plat.vsync()))
        {
            SDL_Log("vsync toggle refused: %s", SDL_GetError());
        }

        if (which == screen::scene)
        {
            // ---- Lesson 2.8's scene, through 2.9's camera and 2.10's projection ---
            if (in.key_pressed(SDL_SCANCODE_Z)) { cube_mode = next_spin(cube_mode); }
            if (in.key_pressed(SDL_SCANCODE_O)) { order = next_order(order); }
            if (in.key_pressed(SDL_SCANCODE_X) && scene_count > 0)
            {
                selected = (selected + 1) % scene_count;
            }
            if (in.key_pressed(SDL_SCANCODE_P)) { use_perspective = !use_perspective; }
            if (in.key_pressed(SDL_SCANCODE_F)) { hs = next_hidden(hs); }
            if (in.key_pressed(SDL_SCANCODE_I))
            {
                interp = (interp == engine::interpolation::perspective)
                       ? engine::interpolation::affine
                       : engine::interpolation::perspective;
            }
            if (in.key_pressed(SDL_SCANCODE_T))
            {
                // 1 -> 2 -> 4 -> 8 -> 16 -> 1. Doubling rather than incrementing
                // because the question this answers is "how much subdivision
                // would it take", and the answer moves in octaves.
                floor_cells = (floor_cells >= 16) ? 1 : floor_cells * 2;
            }
            if (in.key_pressed(SDL_SCANCODE_K)) { near_handling = next_near(near_handling); }
            if (in.key_pressed(SDL_SCANCODE_U)) { culling = next_cull(culling); }
            if (in.key_pressed(SDL_SCANCODE_G)) { eval = next_eval(eval); }
            if (in.key_pressed(SDL_SCANCODE_Q)) { nsrc = next_normal_source(nsrc); }
            if (in.key_pressed(SDL_SCANCODE_J)) { correct_normals = !correct_normals; }
            if (in.key_pressed(SDL_SCANCODE_H)) { spec_model = next_specular(spec_model); }
            if (in.key_pressed(SDL_SCANCODE_E))
            {
                shininess_step = (shininess_step + 1) % k_roughness_count;
            }
            if (in.key_pressed(SDL_SCANCODE_L))
            {
                // Lesson 3.5 wrote: "a real load, on a keypress, every time — not
                // a cache lookup … Module 5 caches; today we measure." Module 5
                // caches. The FIRST lap round the five choices reads five files;
                // every lap after that is a hit, and the HUD prints which.
                demo::load_model(assets, model, next_model(model.choice), uv_flip_on_load);
                scene_mode = demo::scene_kind::model;
                selected = 0;
            }
            if (in.key_pressed(SDL_SCANCODE_BACKSPACE))
            {
                // Lesson 5.5's key, and the one worth pressing. Free the model
                // out from under a scene that is still drawing it: the object
                // vanishes, everything else keeps drawing, and the HUD's
                // `unresolved` count goes to 1. Before 5.4 that was a dangling
                // span; before 5.5 there was no way to free anything at all.
                demo::unload_model(assets, model);
                scene_mode = demo::scene_kind::model;
            }

            // ---- Lesson 3.9 ---------------------------------------------
            if (in.key_pressed(SDL_SCANCODE_M)) { albedo = next_albedo(albedo); }
            if (in.key_pressed(SDL_SCANCODE_S))
            {
                samp.texel_filter = (samp.texel_filter == engine::filter::linear)
                                  ? engine::filter::nearest
                                  : engine::filter::linear;
            }
            if (in.key_pressed(SDL_SCANCODE_R))
            {
                // Both axes together. They are separate fields because a real
                // sampler needs them separate (a road strip repeats along its length
                // and clamps across its width), but one key that changes both is
                // what makes the three modes legible on a floor — cycling them
                // independently mostly produces pictures that are hard to name.
                samp.address_u = next_address(samp.address_u);
                samp.address_v = samp.address_u;
            }
            if (in.key_pressed(SDL_SCANCODE_1))
            {
                samp.origin = (samp.origin == engine::texel_origin::centre)
                            ? engine::texel_origin::corner
                            : engine::texel_origin::centre;
            }
            if (in.key_pressed(SDL_SCANCODE_2))
            {
                // Re-imports the current model, because the flip happens at import
                // and a toggle that only affected the NEXT load would be a knob that
                // appears not to work.
                uv_flip_on_load = !uv_flip_on_load;
                demo::load_model(assets, model, model.choice, uv_flip_on_load);
            }
            // ---- Lesson 4.1 ---------------------------------------------
            if (in.key_pressed(SDL_SCANCODE_5))
            {
                walk = (walk == engine::traversal::scanline) ? engine::traversal::quad
                     : (walk == engine::traversal::quad)     ? engine::traversal::quad_debug
                                                             : engine::traversal::scanline;
            }

            // ---- Lesson 3.10 --------------------------------------------
            if (in.key_pressed(SDL_SCANCODE_3)) { show_budget = !show_budget; }
            if (in.key_pressed(SDL_SCANCODE_4))
            {
                encode = (encode == engine::encode_mode::fast)
                       ? engine::encode_mode::exact
                       : engine::encode_mode::fast;
            }
            if (in.key_pressed(SDL_SCANCODE_B))
            {
                depth_fmt = next_depth_format(depth_fmt);
                scene_depth.set_format(depth_fmt);
                scratch_depth.set_format(depth_fmt);
            }
            if (in.key_pressed(SDL_SCANCODE_C))
            {
                scene_mode = next_scene(scene_mode);
                selected = 0;
                // Put the camera where the new scene reads best. The failure
                // cases are arrangements in DEPTH, and depth is what you cannot
                // see from the side.
                cam.azimuth = 0.0f;
                // Zero elevation for the CYCLE, and only there: it is what makes
                // the three planks exactly equidistant from the eye.
                cam.elevation = (scene_mode == demo::scene_kind::solids) ? 0.35f
                              : (scene_mode == demo::scene_kind::cycle)  ? 0.0f
                              : (scene_mode == demo::scene_kind::floor)  ? 0.10f
                              : (scene_mode == demo::scene_kind::model)  ? 0.45f
                                                                   : 0.08f;
                // Looking down at 26 degrees on the model scene, because a torus
                // seen edge-on is a rectangle: the hole — the thing that makes it
                // non-convex and gives the z-buffer real work — is only visible
                // from above.
                cam.radius = 7.0f;
            }
            if (in.key_pressed(SDL_SCANCODE_SPACE)) { cube_animating = !cube_animating; }
            if (in.key_pressed(SDL_SCANCODE_0)) { cube_t = 0.0f; }
            if (in.key_pressed(SDL_SCANCODE_W)) { cube_point_w = (cube_point_w == 1.0f) ? 0.0f : 1.0f; }
            if (in.key_pressed(SDL_SCANCODE_N)) { cube_dir_w = (cube_dir_w == 0.0f) ? 1.0f : 0.0f; }

            while (stepper.next_step())
            {
                if (cube_animating) { cube_t += 0.7f * stepper.h(); }
                if (in.key_down(SDL_SCANCODE_COMMA))  { cube_t -= 1.2f * stepper.h(); }
                if (in.key_down(SDL_SCANCODE_PERIOD)) { cube_t += 1.2f * stepper.h(); }

                // Orbit the camera. Arrow keys are level-triggered so holding one
                // sweeps smoothly; the elevation is CLAMPED short of straight up,
                // because there the look direction meets the up hint and the view
                // basis goes degenerate (§7 of the harness, a pitfall in the text).
                constexpr float k_orbit = 1.4f;   // radians / second
                if (in.key_down(SDL_SCANCODE_LEFT))  { cam.azimuth   -= k_orbit * stepper.h(); }
                if (in.key_down(SDL_SCANCODE_RIGHT)) { cam.azimuth   += k_orbit * stepper.h(); }
                if (in.key_down(SDL_SCANCODE_UP))    { cam.elevation += k_orbit * stepper.h(); }
                if (in.key_down(SDL_SCANCODE_DOWN))  { cam.elevation -= k_orbit * stepper.h(); }
                cam.elevation = std::clamp(cam.elevation, -1.45f, 1.45f);   // ~ +-83 degrees

                // Dolly in and out. Under an orthographic projection this changes
                // NOTHING on screen — which is exactly the point, and the HUD says
                // so. Perspective (Lesson 2.10) is what finally makes it matter.
                if (in.key_down(SDL_SCANCODE_MINUS))  { cam.radius += 4.0f * stepper.h(); }
                if (in.key_down(SDL_SCANCODE_EQUALS)) { cam.radius -= 4.0f * stepper.h(); }
                // The near limit is 4, not 3: verified that at 4 the whole scene
                // stays inside the viewport rectangle at every orbit angle and
                // elevation, so nothing is lost off the framebuffer's edge. (The
                // ground grid still runs off-screen, which is what a floor should do.)
                //
                // The FLOOR is the exception, and Lesson 3.3 is why. Its near edge
                // sits at world z = +6 and the eye at radius 7 sits at z ≈ 6.97 —
                // barely a unit in front of it. Dollying in walks the camera PAST
                // that edge, which is the only way to put geometry behind the eye
                // and therefore the only way to see the near plane matter. One
                // unit is close enough to stand on the ground and look along it.
                const float min_radius = (scene_mode == demo::scene_kind::floor) ? 1.0f : 4.0f;
                cam.radius = std::clamp(cam.radius, min_radius, 14.0f);

                // Swing the light. Level-triggered, like the camera orbit, so
                // holding a key sweeps — which is what makes "the terminator moves
                // across the surface" something you watch rather than infer.
                constexpr float k_light_speed = 1.6f;   // radians / second
                if (in.key_down(SDL_SCANCODE_A)) { light_azimuth -= k_light_speed * stepper.h(); }
                if (in.key_down(SDL_SCANCODE_D)) { light_azimuth += k_light_speed * stepper.h(); }
            }

            // Rebuild the light from its angles, every frame, for the same reason
            // the scene's transforms are rebuilt from `t` rather than accumulated
            // (build_scene): a direction derived from an authoritative angle cannot
            // drift away from unit length, and a repeatedly-rotated vector can.
            {
                const float ce = std::cos(k_light_elevation);
                const engine::vec3 to_light{ce * std::sin(light_azimuth),
                                            std::sin(k_light_elevation),
                                            ce * std::cos(light_azimuth)};
                // The stored direction is the direction light TRAVELS, so it is the
                // negation of the vector pointing at the source. light.hpp shouts
                // about this because it is the classic sign error.
                lights.key.direction = -to_light;
                lights.key.colour = {1.0f, 0.97f, 0.90f};   // a touch warm, like daylight
                // Lesson 6.2: `intensity` became `irradiance`, and the value became pi.
                // A light of "intensity 1" was always a light of irradiance pi; the
                // pi has moved into the BRDF where it belongs, so it must now be
                // written down here. Same picture, stated honestly.
                lights.key.irradiance = engine::k_reference_irradiance;
            }

            {
                // zone::build — placing the objects. It will read 0.00 us, and
                // that is not a bug: §2.2 measured this machine's counter at a
                // 41.7 ns tick, so anything under about 4 us cannot be resolved.
                // A zone that reads zero is either work you are not doing or work
                // you cannot measure, and this one is the second.
                const engine::scope_timer z{prof, engine::zone::build};
                cube_m = demo::build_spin(cube_mode, cube_t);
                demo::build_floor(assets, floor, floor_cells);
                scene_count = demo::build_scene(scene, scene_mode, cube_mode, cube_t,
                                                assets, floor, model,
                                          k_roughness[shininess_step]);
            }
            if (selected >= scene_count) { selected = 0; }

            // The view matrix (2.9) and the projection matrix (2.10). The
            // projection is [P]-selectable; everything downstream is identical, so
            // the toggle isolates exactly what perspective changes.
            view_from_world = cam.view();
            const engine::mat4& proj = use_perspective ? scene_perspective : scene_orthographic;

            // Lesson 3.7. The eye's WORLD position, which the shading now needs.
            //
            // Note that it is available *only* as an input to `look_at` — the view
            // matrix contains it, but recovering it means an inverse, whereas the
            // camera has been asked for it directly since 2.9. The general lesson is
            // worth keeping: when a transform is built from meaningful inputs, keep
            // the inputs. Module 5's camera stores a `transform` for this reason.
            const engine::vec3 eye_world = cam.eye();

            // The floor needs the whole frame; everything else keeps the inset
            // rectangle it has had since Lesson 2.10, with the HUD beside it.
            const engine::viewport& vp = (scene_mode == demo::scene_kind::floor)
                                       ? demo::k_full_viewport : demo::k_scene_viewport;

            // Lesson 3.3's gathering: the projection, the viewport and the
            // near-plane policy, travelling together. Everything that draws in 3-D
            // now takes exactly one of these.
            const engine::projector pr{proj, vp, near_handling};

            fb.clear(k_bg);

            // The world FIRST, through the camera and projection, so the floor and
            // origin turn with the viewpoint and its rails converge with distance.
            //
            // Drawn before anything else and never depth-tested — which is the
            // painter's algorithm surviving as a legitimate special case. A
            // background is the one thing you always know is behind everything,
            // so it needs no test; that is exactly how a skybox works (Module 6),
            // and it is why the grid vanishes correctly behind solid objects here
            // without a depth value of its own. Giving lines a real depth test is
            // Exercise 3.1.4.
            //
            // zone::overlay — debug draw. It is timed with the HUD rather than with
            // the scene because it is the same KIND of work: things drawn so a human
            // can see what the engine is doing, which ship disabled and must
            // therefore be budgeted separately from the things that do not.
            {
                const engine::scope_timer z{prof, engine::zone::overlay};
                demo::draw_world(fb, view_from_world, pr);
            }

            if (hs == hidden_surface::wireframe)
            {
                // Lesson 2.12's picture, unchanged: edges only, so there are no
                // surfaces to hide and nothing for a depth buffer to do.
                for (int i = 0; i < scene_count; ++i)
                {
                    const engine::mat4 world_from_model = engine::model_matrix(scene[i].xform, order);
                    const engine::mat4 view_from_model = view_from_world * world_from_model;
                    // Resolved at the point of use, and skipped if it does not
                    // resolve — the same two lines the renderer runs, because a
                    // handle has exactly one way to be turned into geometry.
                    const engine::mesh_data* geom = assets.meshes().get(scene[i].geometry);
                    if (geom == nullptr) { continue; }
                    engine::draw_mesh(fb, geom->view(), view_from_model, pr, cube_point_w);
                    engine::draw_axes3(fb, view_from_model, pr, cube_point_w, cube_dir_w);
                }
                painter_wrong = 0;
                near_wrong = 0;
                cull_wrong = 0;
                normal_wrong = 0;
                roundtrip_wrong = 0;
                model_wrong = 0;
                spec_peak = 0;
                grid_wrong = 0;
                texel_wrong = 0;
                filter_wrong = 0;
                encode_wrong = 0;
                scene_quads = {};
                covered_px = 0;
                fill_ns_per_px = 0.0;
                shown_depth = {};
                scene_stats.clip = {};
                scene_cull = {};
                scene_stats.normals = {};
            }
            else
            {
                // Project the whole scene ONCE, into one flat list. Both
                // algorithms below then run on identical geometry, which is what
                // makes the pixel-for-pixel comparison honest.
                // Lesson 5.1. Seven policy choices, gathered — and gathered HERE,
                // once a frame, rather than spelled out at each of the seven calls
                // below. Every comparison render that follows takes a COPY and
                // changes the one field it is comparing, which is both shorter and
                // impossible to get subtly wrong: the reference render can no
                // longer differ from the real one in a field somebody forgot.
                const engine::render_options opts{
                    .compose = order,
                    .cull = culling,
                    .normals = nsrc,
                    .shading = eval,
                    .specular = spec_model,
                    .correct_normal_matrix = correct_normals};
                const engine::camera_view camera{view_from_world, eye_world};
                const std::span<const engine::scene_object> objects{
                    scene, static_cast<std::size_t>(scene_count)};

                {
                    // zone::collect — the vertex stage. Everything from the model
                    // matrix to a screen-space triangle: transform, clip, per-vertex
                    // lighting. §3.3 shows this bar is flat in resolution and linear
                    // in triangle count, which is the whole of why it is a separate
                    // zone from the one below it.
                    const engine::scope_timer z{prof, engine::zone::collect};
                    engine::collect_triangles(scene_tris, scratch, objects, assets.meshes(), camera, pr,
                                              lights, opts, &scene_stats);
                }

                const bool want_painter = (hs == hidden_surface::painter);

                // zone::sort — the painter's algorithm, and only then. Hoisted out
                // of `draw_triangles` so it can be timed apart from the fill;
                // `sort_back_to_front` is the single copy of the rule, called from
                // here and from there.
                if (want_painter)
                {
                    const engine::scope_timer z{prof, engine::zone::sort};
                    engine::sort_back_to_front(scene_tris);
                }

                // ---- Lesson 3.9: what supplies this draw's albedo? ---------
                //
                // An invalid handle under `albedo_source::rule`, which is what
                // makes the rule and the lookup a single keypress apart.
                //
                // LESSON 6.5 CHANGED WHAT `pick` RETURNS, and the old comment
                // here is worth keeping in view: it argued the raw pointer was
                // safe because "`textures` lives for the whole program, so there
                // is nothing here that can dangle". That was true, and it was an
                // argument about THIS demo rather than about the type. A handle
                // needs no such argument.
                const engine::texture* image = textures.pool.get(textures.pick(albedo));

                // Which surfaces read a texture at all. The FLOOR always: it is the
                // one mesh in the demo with uvs authored for tiling, and it has been
                // the uv testbed since 3.2. The MODEL too, because its uvs came off
                // a disk and are therefore the only ones that can demonstrate the
                // OBJ-versus-texture-space disagreement.
                const bool uv_surface = (scene_mode == demo::scene_kind::floor)
                                     || (scene_mode == demo::scene_kind::model);

                // Only the floor still falls back to the procedural rule when no
                // image is bound; the model has no uv-derived colour to show and
                // reads its vertex tint instead.
                const bool checkered = (scene_mode == demo::scene_kind::floor)
                                    && (image == nullptr);

                // One style for the whole batch — the pipeline-object model. The
                // floor is shaded from its uvs; everything else from its vertex
                // colours. Lesson 3.2 §4.1.
                // Lesson 3.8. `lit` is the new third option, and note that it is
                // chosen by the EVALUATION POINT rather than by anything about the
                // geometry: flat and Gouraud both hand the fill a finished colour,
                // and only per-pixel hands it the inputs.
                //
                // The floor still wins, because a checkered debug pattern has no
                // albedo to light and 3.9 is where texture and lighting learn to
                // multiply. Said here so the omission is a decision.
                // Lesson 3.9 closes that gap. An image bound to a `lit` fill IS the
                // albedo, so texture and light finally multiply — and the three-way
                // choice below is the whole of §6 in one expression:
                //
                //   lit      an image (or a vertex colour) times a quantity of light
                //   textured an image, unlit
                //   checker  a formula, unlit
                //
                // Note that "textured AND lit" is not a fourth value. It is `lit`
                // with a binding, which is precisely the point 3.8 made about one
                // enum holding two questions — arriving again, and this time the
                // answer is Module 4's programmable fragment stage rather than
                // another enum.
                const bool bind_texture = uv_surface && (image != nullptr);
                const bool shade_per_pixel = (eval == engine::shade_eval::per_pixel) && !checkered;
                const engine::fill_style style{
                    .interp = interp,
                    .shade = checkered ? engine::shading::uv_checker
                           : shade_per_pixel ? engine::shading::lit
                           : bind_texture    ? engine::shading::textured
                                             : engine::shading::vertex_colour,
                    .space = engine::blend_space::linear,
                    .cull = engine::to_engine_cull(culling),
                    .lights = shade_per_pixel ? &lights : nullptr,
                    .surface = {},                      // rebound per triangle
                    .model = spec_model,
                    .eye = eye_world,
                    .albedo = {bind_texture ? image : nullptr, samp},
                    // Lesson 3.10. The demo defaults to `fast` because it is a
                    // real-time renderer and that is the right answer for one;
                    // `fill_style`'s own default stays `exact` so that nothing
                    // written in Lessons 3.1-3.9 changes by a single code.
                    .encode = encode,
                    // Lesson 4.1. `scanline` unless [5] asks otherwise — the quad
                    // walk is here to be measured, not to be used.
                    .traverse = walk};

                // Clearing to FAR is not optional and not cosmetic. Skip it and
                // last frame's depths survive into this one; §7 has the picture.
                scene_depth.clear();

                // zone::fill — the fragment stage, and on this scene about 96% of
                // the frame.
                //
                // Lesson 3.8 timed this one call by hand, with SDL_GetTicksNS and
                // an exponential smoothing, because "per-pixel shading is
                // expensive" is a claim and a claim wants a number. Lesson 3.10
                // retires that: it is the same measurement, taken by the same kind
                // of instrument, but now it sits in a budget beside its neighbours
                // instead of floating alone — and it is a MEDIAN rather than a
                // running average, so one scheduling hiccup no longer drags it.
                //
                // Note that `sorted` is now false even under the painter's
                // algorithm: the sort happened above, under its own zone, so that
                // this bar measures rasterization and nothing else.
                scene_quads = {};
                {
                    const engine::scope_timer z{prof, engine::zone::fill};
                    engine::draw_triangles(fb, want_painter ? nullptr : &scene_depth,
                                   scene_tris, false, style, &scene_cull, &scene_quads);
                }
                shade_ns = static_cast<double>(prof.median_ns(engine::zone::fill));

                // ---- Lesson 3.4's own comparison --------------------------
                // Culling is an OPTIMISATION, so the claim to check is not "does
                // it look better" but "does it look IDENTICAL". Render the same
                // scene with culling off and count the pixels that differ: on
                // closed geometry with `back` this must read exactly 0, and any
                // other reading means the cull removed something visible.
                //
                // Run before the main comparison because both want scratch_fb,
                // and this one is the cheaper claim to settle.
                if (culling != engine::cull_choice::none)
                {
                    engine::fill_style unculled = style;
                    unculled.cull = engine::cull_mode::none;

                    scratch_fb.clear(k_bg);
                    demo::draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();

                    if (culling == engine::cull_choice::back_by_forward)
                    {
                        // That mode culls in collect_triangles, not in the
                        // rasterizer, so the reference needs its own geometry.
                        engine::render_options unculled_opts = opts;
                        unculled_opts.cull = engine::cull_choice::none;
                        engine::collect_triangles(compare_tris, scratch, objects, assets.meshes(), camera, pr,
                                                  lights, unculled_opts);
                        engine::draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                       compare_tris, want_painter, unculled);
                    }
                    else
                    {
                        engine::draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                       scene_tris, want_painter, unculled);
                    }
                    cull_wrong = engine::count_differences(fb, scratch_fb, vp);
                }
                else
                {
                    cull_wrong = 0;
                }

                // ---- Lesson 3.5's own comparison --------------------------
                // THE ROUND TRIP, ON SCREEN. assets/torus.obj was written from
                // make_torus() by save_obj and is read back by load_obj. Draw the
                // in-memory mesh over the same background with the same everything
                // and count the pixels that differ: writer and reader together must
                // be the identity, and any other reading means one of them lies.
                //
                // Only meaningful for the torus — the other four have no in-memory
                // twin to be compared against.
                if (scene_mode == demo::scene_kind::model && model.choice == demo::model_choice::torus)
                {
                    engine::scene_object control = scene[0];
                    control.geometry = model.generated;

                    engine::collect_triangles(compare_tris, scratch, {&control, 1}, assets.meshes(), camera,
                                              pr, lights, opts);

                    scratch_fb.clear(k_bg);
                    demo::draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    engine::draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   compare_tris, want_painter, style);
                    roundtrip_wrong = engine::count_differences(fb, scratch_fb, vp);
                }
                else
                {
                    roundtrip_wrong = 0;
                }

                // ---- Lesson 3.6's own comparison --------------------------
                // What does getting the normal transform wrong actually cost? Not
                // an argument — a pixel count, on this frame, of this scene.
                //
                // Guarded on `max_tilt > 0`, which is exactly the condition under
                // which the two transforms can differ at all: with no non-uniform
                // scale in the scene the inverse transpose and the model matrix
                // agree to the last bit, so there is nothing to render twice. That
                // guard is also the lesson — the bug is invisible until something
                // is squashed.
                if (eval != engine::shade_eval::palette && scene_stats.normals.max_tilt > 0.0f)
                {
                    engine::render_options other_normals = opts;
                    other_normals.correct_normal_matrix = !correct_normals;
                    engine::collect_triangles(compare_tris, scratch, objects, assets.meshes(), camera, pr,
                                              lights, other_normals);

                    scratch_fb.clear(k_bg);
                    demo::draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    engine::draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   compare_tris, want_painter, style);
                    normal_wrong = engine::count_differences(fb, scratch_fb, vp);
                }
                else
                {
                    normal_wrong = 0;
                }

                // ---- Lesson 3.7's own comparison --------------------------
                // PHONG vs BLINN, AT THE SAME VISUAL TIGHTNESS. Render the scene
                // again with the other model and count the pixels that differ.
                //
                // The exponent is CONVERTED, not held fixed, and that is what makes
                // the number mean something. `dot(n,h)` falls off at half the rate
                // of `dot(R,v)` (§3.4), so comparing the two at one exponent mostly
                // measures that one lobe is wider than the other — which is true,
                // uninteresting, and swamps the effect under test. Matching the
                // widths first leaves only the real difference: the shape of the
                // tail, and Phong's cut-off at grazing angles. Swing the camera and
                // the light low and watch this count climb.
                if (spec_model != engine::specular_model::none
                    && eval != engine::shade_eval::palette)
                {
                    engine::scene_object other_scene[demo::k_max_objects];
                    for (int i = 0; i < scene_count; ++i)
                    {
                        other_scene[i] = scene[i];
                        other_scene[i].mat.surface.roughness =
                            matched_roughness(spec_model, scene[i].mat.surface.roughness);
                    }

                    engine::render_options other_model = opts;
                    other_model.specular = (spec_model == engine::specular_model::blinn)
                                         ? engine::specular_model::phong
                                         : engine::specular_model::blinn;
                    engine::collect_triangles(compare_tris, scratch,
                                              {other_scene, static_cast<std::size_t>(scene_count)},
                                              assets.meshes(), camera, pr, lights, other_model);

                    scratch_fb.clear(k_bg);
                    demo::draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    engine::draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   compare_tris, want_painter, style);
                    model_wrong = engine::count_differences(fb, scratch_fb, vp);
                }
                else
                {
                    model_wrong = 0;
                }

                // ---- Lesson 3.8's own comparison --------------------------
                // EVERY CELL OF THE GRID AGAINST PER-PIXEL, which is the reference
                // because it is the one that evaluates the equation where the
                // answer is used. Renders the same geometry with `shading::lit` and
                // counts the pixels that differ.
                //
                // Two cells must read exactly 0, and checking that is how you know
                // the two axes are wired up right: `face x gouraud` always, and
                // `face x per_pixel` whenever the specular is off (§4.2). Any other
                // reading there is a bug, not a shading difference.
                if (eval != engine::shade_eval::palette && !checkered
                    && hs != hidden_surface::wireframe)
                {
                    engine::fill_style reference_style = style;
                    reference_style.shade = engine::shading::lit;
                    reference_style.lights = &lights;

                    engine::render_options per_pixel_opts = opts;
                    per_pixel_opts.shading = engine::shade_eval::per_pixel;
                    engine::collect_triangles(compare_tris, scratch, objects, assets.meshes(), camera, pr,
                                              lights, per_pixel_opts);

                    scratch_fb.clear(k_bg);
                    demo::draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    engine::draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   compare_tris, want_painter, reference_style);
                    grid_wrong = engine::count_differences(fb, scratch_fb, vp);
                }
                else
                {
                    grid_wrong = 0;
                }

                // ---- Lesson 3.9's own comparisons -------------------------
                //
                // The cheapest comparisons in the demo, and cheap for a reason worth
                // saying out loud: a sampler is PIPELINE STATE. Changing it changes
                // no geometry at all, so unlike every comparison above there is no
                // second `collect_triangles` — the same `scene_tris` is redrawn with
                // one field of `style` altered. That is the same fact that lets a
                // GPU swap a sampler without re-running the vertex stage.
                if (bind_texture)
                {
                    // THE HALF TEXEL, IN PIXELS. Flip `texel_origin` and count. The
                    // number is zero under NEAREST — necessarily, because nearest
                    // asks which texel contains the point and the offset does not
                    // change the answer — and it is large under BILINEAR, because
                    // every sample lands half a texel from where it belongs. A knob
                    // whose effect depends on another knob is exactly the sort of
                    // thing worth measuring rather than asserting.
                    engine::fill_style other = style;
                    other.albedo.samp.origin =
                        (samp.origin == engine::texel_origin::centre)
                            ? engine::texel_origin::corner
                            : engine::texel_origin::centre;

                    scratch_fb.clear(k_bg);
                    demo::draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    engine::draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   scene_tris, want_painter, other);
                    texel_wrong = engine::count_differences(fb, scratch_fb, vp);

                    // ...and what the filter itself is worth, by the same method.
                    engine::fill_style flipped = style;
                    flipped.albedo.samp.texel_filter =
                        (samp.texel_filter == engine::filter::linear)
                            ? engine::filter::nearest
                            : engine::filter::linear;

                    scratch_fb.clear(k_bg);
                    demo::draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    engine::draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   scene_tris, want_painter, flipped);
                    filter_wrong = engine::count_differences(fb, scratch_fb, vp);
                }
                else
                {
                    texel_wrong = 0;
                    filter_wrong = 0;
                }

                // ---- Lesson 3.10's own comparison -------------------------
                //
                // What does the approximate encode actually cost, on THIS frame, in
                // pixels? The same method as every comparison above — redraw with
                // one thing changed and count — and it is the only honest way to
                // present an optimisation, because a speedup without its error is
                // half a claim.
                //
                // Note that it is the CURRENT setting against the other one, not
                // "fast against exact": pressing [4] must move the number to zero
                // and back, which is what proves the toggle is actually reaching the
                // rasterizer rather than the HUD lying about a knob that does
                // nothing. The pixels differ by at most one code (verify_310 §I),
                // and one code is invisible — so the counter is the only way to see
                // that anything happened at all.
                {
                    engine::fill_style other_encode = style;
                    other_encode.encode = (encode == engine::encode_mode::fast)
                                        ? engine::encode_mode::exact
                                        : engine::encode_mode::fast;

                    scratch_fb.clear(k_bg);
                    demo::draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    engine::draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   scene_tris, want_painter, other_encode);
                    encode_wrong = engine::count_differences(fb, scratch_fb, vp);
                }

                // The fill's UNIT cost, which is the number that transfers between
                // machines and resolutions — total microseconds do not. Counting
                // covered pixels is itself a pass over the viewport, so it happens
                // here, outside every zone, and is not part of the budget.
                covered_px = 0;
                {
                    const int x0 = static_cast<int>(vp.x);
                    const int y0 = static_cast<int>(vp.y);
                    const int x1 = std::min(fb.width(), static_cast<int>(vp.x + vp.w));
                    const int y1 = std::min(fb.height(), static_cast<int>(vp.y + vp.h));
                    for (int y = y0; y < y1; ++y)
                    {
                        const Uint32* const row = fb.row(y);
                        for (int x = x0; x < x1; ++x) { if (row[x] != k_bg) { ++covered_px; } }
                    }
                }
                fill_ns_per_px = (covered_px > 0)
                    ? static_cast<double>(prof.median_ns(engine::zone::fill)) / covered_px
                    : 0.0;

                // The brightest pixel on screen. One number, and on a coarse mesh it
                // is the whole of Lesson 3.8's argument: spin the icosahedron and
                // watch it lurch, because the highlight is being sampled at twelve
                // points and interpolated in between.
                spec_peak = engine::brightest_channel(fb, vp);

                // ---- The comparison ---------------------------------------
                // Run the scene a second time with ONE THING CHANGED, over the
                // same background, and count the pixels the two renders disagree
                // about. Which thing changes depends on what is on trial: the
                // near-plane policy (3.3) whenever it is set to something wrong,
                // then the interpolation (3.2) on the floor, then the
                // hidden-surface strategy (3.1) everywhere else.
                //
                // The near-plane comparison comes FIRST and outranks the others
                // because it is the only one that changes what geometry exists.
                // Comparing interpolation modes on a scene whose triangles are
                // being dropped would be measuring the wrong thing carefully.
                const bool near_on_trial = (pr.near != engine::near_mode::clip);

                // The reference render. Identical in every respect except the one
                // under test — including the background grid, which is drawn
                // through the projector and is therefore itself affected by [K].
                engine::projector reference = pr;
                if (near_on_trial) { reference.near = engine::near_mode::clip; }

                scratch_fb.clear(k_bg);
                demo::draw_world(scratch_fb, view_from_world, reference);
                scratch_depth.clear();

                if (near_on_trial)
                {
                    // A second collection, because the clipper produces a
                    // DIFFERENT SET OF TRIANGLES — this is the one comparison in
                    // the demo where the two renders cannot share geometry.
                    engine::collect_stats reference_stats;
                    engine::collect_triangles(compare_tris, scratch, objects, assets.meshes(), camera,
                                              reference, lights, opts, &reference_stats);
                    engine::draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   compare_tris, want_painter, style);
                    near_wrong = engine::count_differences(fb, scratch_fb, vp);
                    interp_wrong = 0;
                    painter_wrong = 0;
                }
                else if (checkered)
                {
                    engine::fill_style other = style;
                    other.interp = (interp == engine::interpolation::perspective)
                                 ? engine::interpolation::affine
                                 : engine::interpolation::perspective;
                    engine::draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   scene_tris, want_painter, other);
                    interp_wrong = engine::count_differences(fb, scratch_fb, vp);
                    painter_wrong = 0;
                    near_wrong = 0;
                }
                else
                {
                    engine::draw_triangles(scratch_fb, want_painter ? &scratch_depth : nullptr,
                                   scene_tris, !want_painter, style);
                    painter_wrong = engine::count_differences(fb, scratch_fb, vp);
                    interp_wrong = 0;
                    near_wrong = 0;
                }

                // The depth view runs last, over the z-buffered image, because it
                // needs the buffer the z-buffer pass just filled.
                shown_depth = (hs == hidden_surface::depth_view)
                            ? engine::show_depth(fb, scene_depth, vp)
                            : engine::depth_range{};
            }

            // Everything the HUD reports is read back out of the matrices that were
            // actually used to draw. The probe is now vertex 0 OF THE SELECTED MESH,
            // carried the whole chain — model -> world -> view -> clip -> NDC ->
            // screen — so the HUD narrates a real vertex of the shape on screen.
            selected_probe = engine::vec3{};
            selected_verts = 0;
            selected_tris = 0;
            selected_idx = 0;
            if (const engine::mesh_data* sel = assets.meshes().get(scene[selected].geometry))
            {
                if (!sel->vertices.empty()) { selected_probe = sel->vertices[0]; }
                selected_verts = sel->vertices.size();
                selected_tris = sel->triangle_count();
                selected_idx = sel->indices.size();
            }
            selected_m = engine::model_matrix(scene[selected].xform, order);
            selected_world = engine::xyz(selected_m
                                       * engine::to_vec4(selected_probe, cube_point_w));
            selected_view = engine::xyz(view_from_world * engine::point(selected_world));
            selected_clip = proj * engine::point(selected_view);
            selected_ndc = engine::perspective_divide(selected_clip);
            selected_screen = vp.to_screen(selected_ndc);
            selected_axis_len = axis_length(selected_m, {1.0f, 0.0f, 0.0f});
            selected_corner = axis_angle_deg(selected_m, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        }
        else if (which == screen::basis)
        {
            // ---- Lesson 2.5's basis transform ---------------------------------
            if (in.key_pressed(SDL_SCANCODE_Z)) { basis_mode = next_xform(basis_mode); }
            if (in.key_pressed(SDL_SCANCODE_SPACE)) { basis_animating = !basis_animating; }
            if (in.key_pressed(SDL_SCANCODE_0)) { basis_t = 0.0f; }

            while (stepper.next_step())
            {
                if (basis_animating) { basis_t += 0.6f * stepper.h(); }
                if (in.key_down(SDL_SCANCODE_COMMA))  { basis_t -= 1.2f * stepper.h(); }
                if (in.key_down(SDL_SCANCODE_PERIOD)) { basis_t += 1.2f * stepper.h(); }
            }
            basis_t = std::clamp(basis_t, -3.2f, 3.2f);

            basis_m = build(basis_mode, basis_t);

            fb.clear(k_bg);

            draw_lattice(fb, basis_m);

            // The transformed unit square first, so the arrows and the glyph read
            // on top of it rather than under it.
            fill_unit_square(fb, basis_m, engine::pack_argb(48, 58, 92));

            // The other composition order, ghosted, in the two modes where the
            // order is the subject. If the two outlines coincide the
            // transformations commute; when they do not, you are looking at the
            // reason matrix multiplication is not commutative.
            const bool composing = (basis_mode == xform::scale_then_rotate
                                 || basis_mode == xform::rotate_then_scale);
            if (composing)
            {
                outline_unit_square(fb, build_reversed(basis_mode, basis_t),
                                    engine::pack_argb(226, 196, 110));
            }

            // The ORIGINAL axes, faint, so the transformed basis has something to
            // be measured against.
            line_maths(fb, {-3.0f, 0.0f}, {3.0f, 0.0f}, engine::pack_argb(70, 74, 96));
            line_maths(fb, {0.0f, -2.5f}, {0.0f, 2.5f}, engine::pack_argb(70, 74, 96));

            draw_glyph(fb, basis_m, engine::pack_argb(232, 226, 214));
            draw_basis_arrow(fb, basis_m.c0, engine::pack_argb(236, 92, 92));    // i-hat
            draw_basis_arrow(fb, basis_m.c1, engine::pack_argb(122, 196, 152));  // j-hat

            basis_area_px = measure_area_px(basis_m);
        }
        else if (which == screen::lines)
        {
            // ---- Lesson 2.1's fan ---------------------------------------------
            if (in.key_pressed(SDL_SCANCODE_1)) { line_algo = engine::draw_line_naive; line_algo_name = "naive y=mx+b"; }
            if (in.key_pressed(SDL_SCANCODE_2)) { line_algo = engine::draw_line_dda;   line_algo_name = "DDA (float)"; }
            if (in.key_pressed(SDL_SCANCODE_3)) { line_algo = engine::draw_line;       line_algo_name = "Bresenham (int)"; }
            if (in.key_pressed(SDL_SCANCODE_SPACE)) { spinning = !spinning; }

            while (stepper.next_step())
            {
                if (spinning) { phase += 0.12f * stepper.h(); }
            }

            fb.clear(k_bg);
            draw_fan(fb, line_algo, phase);
        }
        else
        {
            // ---- Lesson 2.2's triangles ---------------------------------------
            if (in.key_pressed(SDL_SCANCODE_1)) { mode = tri_mode::filled; }
            if (in.key_pressed(SDL_SCANCODE_2)) { mode = tri_mode::wireframe; }
            if (in.key_pressed(SDL_SCANCODE_3)) { mode = tri_mode::halfplanes; }
            if (in.key_pressed(SDL_SCANCODE_4)) { mode = tri_mode::weights; }
            if (in.key_pressed(SDL_SCANCODE_5)) { mode = tri_mode::isolines; }
            if (in.key_pressed(SDL_SCANCODE_6)) { mode = tri_mode::gouraud; }
            if (in.key_pressed(SDL_SCANCODE_7)) { mode = tri_mode::checker; }
            if (in.key_pressed(SDL_SCANCODE_M)) { linear_blend = !linear_blend; }
            if (in.key_pressed(SDL_SCANCODE_LEFTBRACKET))
            {
                stripe_bands = std::max(1.0f, stripe_bands - 0.25f);
            }
            if (in.key_pressed(SDL_SCANCODE_RIGHTBRACKET))
            {
                stripe_bands = std::min(12.0f, stripe_bands + 0.25f);
            }
            if (in.key_pressed(SDL_SCANCODE_R)) { use_fill_rule = !use_fill_rule; }
            if (in.key_pressed(SDL_SCANCODE_SPACE)) { spinning = !spinning; }

            while (stepper.next_step())
            {
                if (spinning) { phase += 0.35f * stepper.h(); }
            }

            fb.clear(k_bg);

            // The left panel: one triangle, spun so it passes through every
            // orientation — including the two where its signed area changes sign.
            const engine::vec2 centre{92.0f, 96.0f};
            const float radius = 62.0f;
            int vx[3];
            int vy[3];
            for (int i = 0; i < 3; ++i)
            {
                const float a = phase + static_cast<float>(i) * 2.09439510f;   // 120 degrees
                vx[i] = static_cast<int>(std::lround(centre.x + std::cos(a) * radius));
                vy[i] = static_cast<int>(std::lround(centre.y + std::sin(a) * radius * 0.82f));
            }

            const Uint64 t0 = SDL_GetTicksNS();
            switch (mode)
            {
            case tri_mode::filled:
                engine::fill_triangle(fb, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2],
                                      engine::pack_argb(226, 196, 110));
                break;
            case tri_mode::wireframe:
                engine::draw_triangle(fb, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2],
                                      engine::pack_argb(226, 196, 110));
                break;
            case tri_mode::halfplanes:
                draw_halfplanes(fb, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2]);
                break;
            case tri_mode::weights:
                draw_weights(fb, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2]);
                break;
            case tri_mode::isolines:
                draw_isolines(fb, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2]);
                engine::draw_triangle(fb, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2],
                                      engine::pack_argb(210, 212, 220));
                break;
            case tri_mode::gouraud:
                // Red, green and blue at the corners — the classic, and chosen
                // because the three primaries are exactly where the encoded
                // blend goes most obviously wrong. [M] switches the space.
                // Designated initialisers, because `vertex` now has seven fields
                // and only two of them are interesting here. This is a FLAT
                // picture: `z` stays 0 (and there is no depth attachment to read
                // it), and `inv_w` stays 1 — which is exactly why the
                // perspective-correct path returns the same pixels it always did.
                // Lesson 3.2 §4.2.
                engine::fill_triangle(fb,
                    engine::vertex{.x = vx[0], .y = vy[0], .colour = engine::pack_argb(255, 0, 0)},
                    engine::vertex{.x = vx[1], .y = vy[1], .colour = engine::pack_argb(0, 255, 0)},
                    engine::vertex{.x = vx[2], .y = vy[2], .colour = engine::pack_argb(0, 0, 255)},
                    engine::fill_style{.space = linear_blend ? engine::blend_space::linear
                                                             : engine::blend_space::encoded});
                break;
            case tri_mode::checker:
                fill_triangle_uv(fb, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2], false);
                break;
            }

            // In the two Lesson 2.3 views the cursor is a probe: it draws the
            // three sub-triangles whose areas ARE the weights, and reports them.
            if (mode == tri_mode::weights || mode == tri_mode::isolines)
            {
                int win_w = 1280;
                int win_h = 720;
                SDL_GetWindowSize(window, &win_w, &win_h);

                probe_x = static_cast<int>(std::lround(
                    in.mouse_x() * static_cast<float>(k_fb_width) / static_cast<float>(win_w)));
                probe_y = static_cast<int>(std::lround(
                    in.mouse_y() * static_cast<float>(k_fb_height) / static_cast<float>(win_h)));

                probe = engine::barycentric_at(vx[0], vy[0], vx[1], vy[1], vx[2], vy[2],
                                               probe_x, probe_y);
                draw_probe(fb, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2],
                           probe_x, probe_y, probe);
            }
            const Uint64 t1 = SDL_GetTicksNS();

            const double ns = static_cast<double>(t1 - t0);
            tri_ns_avg = (tri_ns_avg <= 0.0) ? ns : (tri_ns_avg * 0.95 + ns * 0.05);

            // Read the centre pixel back out of the framebuffer rather than
            // recomputing it. The HUD then reports what was actually drawn, so
            // it cannot agree with a fill that has drifted from the formula.
            centroid_px = fb.pixel_at((vx[0] + vx[1] + vx[2]) / 3,
                                      (vy[0] + vy[1] + vy[2]) / 3);

            // The right-hand panel belongs to whichever lesson the view is from.
            if (is_attribute_view(mode)) { bias_differ = draw_bias_magnifier(fb, stripe_bands); }
            else                         { doubled_px = draw_coverage(fb, use_fill_rule); }
        }

        // zone::present — the part of presenting that is OURS. The row-by-row copy
        // into the locked streaming texture (Lesson 1.5) and the two draw calls
        // that put it on screen. `SDL_RenderPresent` is deliberately outside: on a
        // vsynced frame it blocks until the display is ready, and a bar that is
        // 90% "waiting for the monitor" tells you nothing about your renderer.
        {
            const engine::scope_timer z{prof, engine::zone::present};
            plat.blit_framebuffer();
        }

        // The HUD is a phase, but it is not a lexical scope: it runs to the bottom
        // of this loop through several hundred lines of per-demo text. So it is
        // timed with an explicit pair rather than a `scope_timer` — which is why
        // `profiler::add` is public. The two calls are three lines apart in intent
        // and far apart in the file, and that is exactly the case RAII cannot cover.
        const Uint64 hud_t0 = engine::profiler::now_ticks();

        // A text coordinate is exactly twice a framebuffer coordinate: 2x text
        // scale over a 4x framebuffer scale.
        SDL_SetRenderScale(renderer, 2.0f, 2.0f);

        if (which == screen::scene)
        {
            const bool correct_order = (order == engine::trs_order::trs);

            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "SCENE   %-26s   t = %+.2f", name_of(cube_mode),
                                      static_cast<double>(cube_t));

            // The composition order, coloured because two of the three are bugs.
            SDL_SetRenderDrawColor(renderer, correct_order ? 122 : 236,
                                             correct_order ? 196 : 92,
                                             correct_order ? 152 : 92, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 20.0f,
                                      "[O] model matrix = %s", name_of(order));

            // ---- Lesson 3.1's readout, above the picture ------------------
            // Green when the strategy on screen is the correct one, amber when it
            // is a demonstration of something broken.
            const bool sound = (hs == hidden_surface::zbuffer
                             || hs == hidden_surface::depth_view
                             || hs == hidden_surface::wireframe);
            SDL_SetRenderDrawColor(renderer, sound ? 122 : 236,
                                             sound ? 196 : 196,
                                             sound ? 152 : 110, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 34.0f,
                                      "[F] %-18s   [C] scene = %s",
                                      name_of(hs), name_of(scene_mode));

            if (hs == hidden_surface::wireframe)
            {
                SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                SDL_RenderDebugText(renderer, 6.0f, 48.0f,
                    "  no surfaces, so nothing to hide - [F] fills them in");
            }
            else if (scene_mode == demo::scene_kind::floor)
            {
                // Lesson 3.2's readout. The interpolation mode, the tessellation,
                // and the number that connects them: how many pixels affine
                // interpolation gets wrong on this exact frame.
                const bool corrected = (interp == engine::interpolation::perspective);
                SDL_SetRenderDrawColor(renderer, corrected ? 122 : 236,
                                                 corrected ? 196 : 196,
                                                 corrected ? 152 : 110, 255);
                SDL_RenderDebugTextFormat(renderer, 6.0f, 48.0f,
                    "[I] %-13s  [T] %2dx%-2d floor (%zu tris)   affine vs correct: %d px",
                    corrected ? "PERSPECTIVE" : "AFFINE (wrong)",
                    floor_cells, floor_cells, scene_tris.size(), interp_wrong);
            }
            else if (scene_mode == demo::scene_kind::model)
            {
                // Lesson 3.5's readout. The three numbers that ARE the index
                // problem: how many positions the file holds, how many extra
                // vertices reconciling the attribute streams cost, and the total.
                const bool good = model.load.ok() && model.check.consistently_wound()
                               && !model.unloaded;
                SDL_SetRenderDrawColor(renderer, good ? 122 : 236,
                                                 good ? 196 : 92,
                                                 good ? 152 : 92, 255);
                if (model.unloaded)
                {
                    // Lesson 5.5. The scene still holds the handle; the store no
                    // longer holds the asset. Both numbers are on screen so the
                    // two facts can be read against each other, which is the whole
                    // demonstration: nothing crashed, one object is missing, and
                    // the renderer says exactly how many.
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 48.0f,
                        "[Bksp] %-14s UNLOADED  handle %u:%u is stale, unresolved = %d",
                        name_of(model.choice), model.geometry.index(),
                        model.geometry.generation(), scene_stats.unresolved);
                }
                else if (!model.load.ok())
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 48.0f,
                        "[L] %-16s  FAILED: %s (line %d)",
                        name_of(model.choice), engine::name_of(model.load.status),
                        model.load.line);
                }
                else
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 48.0f,
                        "[L] %-16s  %d+%d -> %d verts, %d tris  [%s %.2f ms]",
                        name_of(model.choice), model.load.positions,
                        model.load.split_vertices, model.load.vertices,
                        model.load.triangles,
                        model.cached ? "cached" : "read", model.load_ms);
                }

            }
            else
            {
                // The number the whole lesson turns on. Red the moment sorting
                // and per-pixel depth disagree about a single pixel.
                SDL_SetRenderDrawColor(renderer, painter_wrong > 0 ? 236 : 122,
                                                 painter_wrong > 0 ? 92 : 196,
                                                 painter_wrong > 0 ? 92 : 152, 255);
                SDL_RenderDebugTextFormat(renderer, 6.0f, 48.0f,
                    "[B] depth = %-10s  %zu tris   painter vs z-buffer: %d px differ",
                    engine::name_of(depth_fmt), scene_tris.size(), painter_wrong);
            }

            // ---- Lesson 3.3's readout -------------------------------------
            // The near-plane policy, and the three numbers that say what the
            // geometry is actually doing about it. `straddling` is the count that
            // matters: while it is zero all three modes agree exactly, which is
            // why this bug hides until the moment you walk into something.
            {
                const bool near_ok = (near_handling == engine::near_mode::clip);
                SDL_SetRenderDrawColor(renderer, near_ok ? 122 : 236,
                                                 near_ok ? 196 : 92,
                                                 near_ok ? 152 : 92, 255);
                if (hs == hidden_surface::wireframe)
                {
                    // No triangles were collected, so there are no counts — but
                    // [K] still governs the LINES, which are clipped by the same
                    // plane with the same crossing parameter.
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 62.0f,
                        "[K] near = %-16s  (lines are clipped too - the 1-D case)",
                        name_of(near_handling));
                }
                else if (near_ok)
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 62.0f,
                        "[K] near = %-16s  %d tris -> %d  (%d straddle, %d behind)",
                        name_of(near_handling), scene_stats.clip.input, scene_stats.clip.output,
                        scene_stats.clip.straddling, scene_stats.clip.behind);
                }
                else
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 62.0f,
                        "[K] near = %-16s  %d tris -> %d  vs clipped: %d px WRONG",
                        name_of(near_handling), scene_stats.clip.input, scene_stats.clip.output,
                        near_wrong);
                }
            }

            // ---- Lesson 3.4's readout -------------------------------------
            // Culling is an optimisation, so the honest headline is a PAIR of
            // numbers: how much work it saved, and how many pixels it cost. The
            // second must be zero, or it was not an optimisation.
            {
                bool any_open = false;
                for (int i = 0; i < scene_count; ++i)
                {
                    if (!scene[i].closed) { any_open = true; }
                }
                const bool cull_sound = (culling == engine::cull_choice::none)
                                     || (culling == engine::cull_choice::back && !any_open);
                SDL_SetRenderDrawColor(renderer, cull_sound ? 122 : 236,
                                                 cull_sound ? 196 : 196,
                                                 cull_sound ? 152 : 110, 255);
                if (hs == hidden_surface::wireframe)
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 76.0f,
                        "[U] cull = %-18s  (wireframe draws no faces to cull)",
                        name_of(culling));
                }
                else if (culling == engine::cull_choice::none)
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 76.0f,
                        "[U] cull = %-18s  %d tris, %d front   dot(n,fwd) misjudges %d",
                        name_of(culling), scene_cull.submitted, scene_cull.front,
                        scene_cull.disagree);
                }
                else
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 76.0f,
                        "[U] cull = %-18s  %d of %d drawn   vs no culling: %d px",
                        name_of(culling), scene_cull.drawn, scene_cull.submitted,
                        cull_wrong);
                }
            }

            // ---- Lesson 3.6's readout -------------------------------------
            // Two facts, side by side: which normal each surface is being shaded
            // from, and what the *wrong* normal transform would cost. The second
            // number is 0 whenever nothing in the scene is non-uniformly scaled,
            // which is the honest way to say "this bug hides".
            if (scene_mode != demo::scene_kind::floor)
            {
                const bool lit = (eval != engine::shade_eval::palette);
                SDL_SetRenderDrawColor(renderer, lit ? 122 : 236,
                                                 lit ? 196 : 196,
                                                 lit ? 152 : 110, 255);
                if (hs == hidden_surface::wireframe)
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 292.0f,
                        "[G] %-22s (wireframe has no surfaces to light)", name_of(eval));
                }
                else if (!lit)
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 292.0f,
                        "[G] %-22s no light, no normal - it does NOT change as it spins",
                        name_of(eval));
                }
                else
                {
                    SDL_SetRenderDrawColor(renderer, correct_normals ? 122 : 236,
                                                     correct_normals ? 196 : 92,
                                                     correct_normals ? 152 : 92, 255);
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 292.0f,
                        "[J] %-13s tilt %4.1f deg  dif %d px",
                        correct_normals ? "inv-transpose" : "NAIVE M",
                        static_cast<double>(scene_stats.normals.max_tilt), normal_wrong);
                }

                // ---- Lesson 3.8's readout ---------------------------------
                // THE TWO AXES, ON TWO LINES, because they are two questions and
                // one line reading "[G] smooth" was exactly the conflation this
                // lesson exists to undo. Then the grid's own number: how far this
                // cell is from per-pixel, which is 0 for per-pixel itself and for
                // the two degenerate cells §4.2 names.
                if (hs != hidden_surface::wireframe && lit)
                {
                    const bool degenerate = is_degenerate(nsrc, eval, spec_model);
                    SDL_SetRenderDrawColor(renderer, 122, 196, 152, 255);
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 320.0f,
                        "[G] %-22s [Q] %-22s", name_of(eval), name_of(nsrc));

                    SDL_SetRenderDrawColor(renderer, degenerate ? 226 : 150,
                                                     degenerate ? 206 : 152,
                                                     degenerate ? 130 : 170, 255);
                    if (eval == engine::shade_eval::per_pixel)
                    {
                        SDL_RenderDebugTextFormat(renderer, 6.0f, 334.0f,
                            "    the reference: shaded per fragment   %.1f us/frame",
                            shade_ns / 1000.0);
                    }
                    else if (degenerate)
                    {
                        SDL_RenderDebugTextFormat(renderer, 6.0f, 334.0f,
                            "    DEGENERATE cell = flat, %d px   %.1f us/frame",
                            grid_wrong, shade_ns / 1000.0);
                    }
                    else
                    {
                        SDL_RenderDebugTextFormat(renderer, 6.0f, 334.0f,
                            "    vs PER-PIXEL: %d px differ   %.1f us/frame",
                            grid_wrong, shade_ns / 1000.0);
                    }
                }

                // ---- Lesson 3.7's readout ---------------------------------
                // Which model, at what exponent, how far it is from the other one
                // AT THE SAME TIGHTNESS, and the brightest pixel on screen. That
                // last number is the one to watch while the object spins.
                if (hs != hidden_surface::wireframe && lit)
                {
                    const bool on = (spec_model != engine::specular_model::none);
                    SDL_SetRenderDrawColor(renderer, on ? 226 : 150,
                                                     on ? 206 : 152,
                                                     on ? 130 : 170, 255);
                    if (!on)
                    {
                        SDL_RenderDebugText(renderer, 6.0f, 306.0f,
                            "[H] specular OFF - orbit the camera: nothing changes at all");
                    }
                    else
                    {
                        SDL_RenderDebugTextFormat(renderer, 6.0f, 306.0f,
                            "[H] %-22s [E] p=%-5.0f vs other %d px   peak %d",
                            name_of(spec_model),
                            static_cast<double>(k_roughness[shininess_step]),
                            model_wrong, spec_peak);
                    }
                }
            }

            const bool floor_scene = (scene_mode == demo::scene_kind::floor);

            if (floor_scene)
            {
                // The floor uses the whole framebuffer, so there is no column of
                // spare pixels to write into — every HUD line moves up into the
                // sky above the horizon, which is the only empty part left.
                SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                if (near_handling == engine::near_mode::clip
                    && interp == engine::interpolation::affine)
                {
                    // Lesson 3.2's subject is what is on screen, so 3.2 gets the
                    // sky. Each wrong mode explains itself and only itself —
                    // stacking every explanation at once is how a HUD becomes
                    // wallpaper nobody reads.
                    SDL_RenderDebugText(renderer, 6.0f, 96.0f,
                        "AFFINE: the checker swims and buckles, and breaks along the");
                    SDL_RenderDebugText(renderer, 6.0f, 110.0f,
                        "diagonal each quad is split on. [T] subdivides: the error falls");
                    SDL_RenderDebugText(renderer, 6.0f, 124.0f,
                        "with the SQUARE of the subdivision and never reaches zero. That");
                    SDL_RenderDebugText(renderer, 6.0f, 138.0f,
                        "is why 1990s floors were tessellated to death. [I] for the fix.");
                }
                else if (near_handling == engine::near_mode::clip)
                {
                    SDL_RenderDebugText(renderer, 6.0f, 96.0f,
                        "hold [=] to walk forward. The floor's near edge passes the eye at");
                    SDL_RenderDebugText(renderer, 6.0f, 110.0f,
                        "radius 7, and from there part of it is BEHIND you. Clipping cuts");
                    SDL_RenderDebugText(renderer, 6.0f, 124.0f,
                        "those triangles along the near plane and draws the rest. [K] to");
                    SDL_RenderDebugText(renderer, 6.0f, 138.0f,
                        "see what the two obvious alternatives do instead.");
                }
                else if (near_handling == engine::near_mode::drop)
                {
                    SDL_RenderDebugText(renderer, 6.0f, 96.0f,
                        "DROP: one corner behind the near plane sinks the WHOLE triangle, so");
                    SDL_RenderDebugText(renderer, 6.0f, 110.0f,
                        "the ground you stand on is simply not drawn. [T] shrinks the hole -");
                    SDL_RenderDebugText(renderer, 6.0f, 124.0f,
                        "it is one cell deep, so it goes like 1/N - but at 16x16 there is");
                    SDL_RenderDebugText(renderer, 6.0f, 138.0f,
                        "still a viewpoint that loses half the frame. 512 tris to hide it.");
                }
                else
                {
                    SDL_RenderDebugText(renderer, 6.0f, 96.0f,
                        "NONE: no guard at all. A vertex behind the eye has w < 0, so the");
                    SDL_RenderDebugText(renderer, 6.0f, 110.0f,
                        "divide FLIPS ITS SIGN and it lands on the far side of the screen.");
                    SDL_RenderDebugText(renderer, 6.0f, 124.0f,
                        "The triangle spans the frame or turns inside out; magenta marks");
                    SDL_RenderDebugText(renderer, 6.0f, 138.0f,
                        "pixels whose uv came back infinite. This is 2.7's warning, live.");
                }
                SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
                SDL_RenderDebugText(renderer, 6.0f, 158.0f,
                    "[U] cull  [K] near  [I] interp  [T] tessellate  [C] scene  [arrows] orbit  [-][=] dolly");
            }

            // ---- Lesson 3.9's readout -------------------------------------
            // One line, on the two scenes that have uvs worth sampling. Which
            // line depends on what is on trial, the same discipline the floor's
            // sky commentary follows: stacking every explanation at once is how a
            // HUD becomes wallpaper nobody reads.
            {
                const engine::texture* hud_image = textures.pool.get(textures.pick(albedo));
                const bool hud_uv = (scene_mode == demo::scene_kind::floor
                                  || scene_mode == demo::scene_kind::model);

                if (hud_uv && hs != hidden_surface::wireframe)
                {
                    if (hud_image == nullptr)
                    {
                        SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                        SDL_RenderDebugText(renderer, 6.0f, 348.0f,
                            "[M] RULE - a formula, not an image: no memory, no sampler, exact at any zoom");
                    }
                    else if (samp.origin == engine::texel_origin::corner)
                    {
                        // Amber, because this is a mode that exists to be wrong.
                        // The parenthetical is the finding: nearest-neighbour
                        // sampling cannot see this error AT ALL, so a texture
                        // pipeline can carry it for years and only reveal it the
                        // day somebody switches filtering on.
                        SDL_SetRenderDrawColor(renderer, 236, 196, 110, 255);
                        SDL_RenderDebugTextFormat(renderer, 6.0f, 348.0f,
                            "[1] TEXEL CORNER (wrong) - half a texel off: %d px%s",
                            texel_wrong,
                            samp.texel_filter == engine::filter::nearest
                                ? "  (0 under NEAREST - it cannot see this)" : "");
                    }
                    else
                    {
                        SDL_SetRenderDrawColor(renderer, 122, 196, 152, 255);
                        SDL_RenderDebugTextFormat(renderer, 6.0f, 348.0f,
                            "[M] %-21s [S] %-8s [R] %-8s  vs %s: %d px",
                            name_of(albedo), name_of(samp.texel_filter),
                            name_of(samp.address_u),
                            samp.texel_filter == engine::filter::linear ? "nearest" : "bilinear",
                            filter_wrong);
                    }
                }
            }

            // Everything below writes into the column of pixels beside the inset
            // viewport, which the floor scene does not have. Guarded rather than
            // skipped with a `continue`, so that anything added to the end of the
            // frame keeps running for every scene.
            if (!floor_scene)
            {
                        // The camera identity (its axes are the view matrix's rows; 2.9).
                        const engine::vec3 eye = cam.eye();

                        SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 40.0f,
                                                  "CAMERA   eye (%+.2f %+.2f %+.2f)",
                                                  static_cast<double>(eye.x), static_cast<double>(eye.y),
                                                  static_cast<double>(eye.z));
                        SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 54.0f,
                                                  "azim %4.0f  elev %+3.0f deg  r %.1f",
                                                  static_cast<double>(cam.azimuth * 180.0f / 3.14159265f),
                                                  static_cast<double>(cam.elevation * 180.0f / 3.14159265f),
                                                  static_cast<double>(cam.radius));
                        // The projection mode — 2.10's toggle, and what distance does under it.
                        SDL_SetRenderDrawColor(renderer, use_perspective ? 122 : 226,
                                                         use_perspective ? 196 : 196,
                                                         use_perspective ? 152 : 110, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 68.0f, "[P] projection = %s",
                                                  use_perspective ? "PERSPECTIVE" : "orthographic");
                        SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                        SDL_RenderDebugText(renderer, 380.0f, 82.0f, use_perspective
                                            ? "  [-][=] dolly: far shrinks, near grows"
                                            : "  [-][=] dolly: NO effect on size");

                        // THE LESSON'S SPINE, now complete: one corner carried through EVERY
                        // space in the chain — model -> world -> view -> clip -> NDC -> SCREEN.
                        // You can see w stop being 1 at clip, and the +y flip at screen.
                        SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 100.0f, "[X] %s", scene[selected].name);
                        // Indexed geometry, made concrete: how few vertices, how many triangles.
                        SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 114.0f,
                            "  %zu verts, %zu tris, %zu idx -> vertex 0:",
                            selected_verts, selected_tris, selected_idx);
                        SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 128.0f, "model (%+.2f %+.2f %+.2f)",
                            static_cast<double>(selected_probe.x), static_cast<double>(selected_probe.y),
                            static_cast<double>(selected_probe.z));
                        SDL_SetRenderDrawColor(renderer, 226, 196, 110, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 142.0f, "world (%+.2f %+.2f %+.2f)",
                            static_cast<double>(selected_world.x), static_cast<double>(selected_world.y),
                            static_cast<double>(selected_world.z));
                        SDL_SetRenderDrawColor(renderer, 130, 190, 220, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 156.0f, "view  (%+.2f %+.2f %+.2f)",
                            static_cast<double>(selected_view.x), static_cast<double>(selected_view.y),
                            static_cast<double>(selected_view.z));
                        // clip: w is the star — it is -z_view now, no longer 1 (2.10).
                        SDL_SetRenderDrawColor(renderer, 236, 196, 110, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 170.0f, "clip  (%+.2f %+.2f %+.2f w=%.2f)",
                            static_cast<double>(selected_clip.x), static_cast<double>(selected_clip.y),
                            static_cast<double>(selected_clip.z), static_cast<double>(selected_clip.w));
                        SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 184.0f, "  w=%.2f %s",
                            static_cast<double>(selected_clip.w),
                            use_perspective ? "=-z_view (was 1!)" : "=1 (ortho keeps it)");
                        // ndc: after the perspective divide. xy in [-1,1], z in [0,1], +y UP.
                        SDL_SetRenderDrawColor(renderer, 130, 220, 170, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 198.0f, "ndc   (%+.3f %+.3f %+.3f) /w",
                            static_cast<double>(selected_ndc.x), static_cast<double>(selected_ndc.y),
                            static_cast<double>(selected_ndc.z));
                        // screen: THIS lesson's viewport transform. Pixels + device depth.
                        SDL_SetRenderDrawColor(renderer, 236, 210, 150, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 212.0f, "scr   (%.1f %.1f  d=%.3f) px",
                            static_cast<double>(selected_screen.x), static_cast<double>(selected_screen.y),
                            static_cast<double>(selected_screen.z));
                        SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                        SDL_RenderDebugText(renderer, 380.0f, 226.0f, "  ndc +y UP -> screen +y DOWN (flip)");

                        // Deform/move check from 2.8, plus the 2.7 w-bug flag when engaged.
                        const float want_len = scene[selected].xform.scale.x;
                        const bool rigid = std::fabs(selected_corner - 90.0f) < 0.01f
                                        && std::fabs(selected_axis_len - want_len) < 0.001f;
                        const bool w_bug = (cube_point_w != 1.0f || cube_dir_w != 0.0f);
                        SDL_SetRenderDrawColor(renderer, (rigid && !w_bug) ? 150 : 236,
                                                         (rigid && !w_bug) ? 152 : 92,
                                                         (rigid && !w_bug) ? 170 : 92, 255);
                        SDL_RenderDebugTextFormat(renderer, 380.0f, 244.0f, "|x|=%.2f corner %.0f%s%s",
                            static_cast<double>(selected_axis_len), static_cast<double>(selected_corner),
                            rigid ? "" : " DEFORM", w_bug ? "  [W/N bug]" : "");

                        // What to look for. Lesson 3.1's scenes each have one thing to see,
                        // so they say it themselves; otherwise the older commentary stands.
                        SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                        if (hs == hidden_surface::depth_view)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "the DEPTH buffer, stretched to");
                            SDL_RenderDebugTextFormat(renderer, 380.0f, 276.0f, "[%.4f, %.4f] - the whole",
                                static_cast<double>(shown_depth.lo), static_cast<double>(shown_depth.hi));
                            SDL_RenderDebugTextFormat(renderer, 380.0f, 290.0f,
                                "scene fits in %.1f%% of [0,1].",
                                static_cast<double>((shown_depth.hi - shown_depth.lo) * 100.0f));
                            SDL_RenderDebugText(renderer, 380.0f, 304.0f, "That is the 1/z crowding.");
                        }
                        else if (scene_mode == demo::scene_kind::cycle)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "A over B over C over A. No");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, "order is right, and all three");
                            SDL_RenderDebugText(renderer, 380.0f, 290.0f, "average depths are equal.");
                        }
                        else if (scene_mode == demo::scene_kind::intersect)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "they pass THROUGH each other:");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, "the right answer changes");
                            SDL_RenderDebugText(renderer, 380.0f, 290.0f, "halfway across one triangle.");
                        }
                        else if (scene_mode == demo::scene_kind::floor)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "affine interpolation makes the");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, "checker swim, and breaks along");
                            SDL_RenderDebugText(renderer, 380.0f, 290.0f, "the diagonal. [T] subdivides:");
                            SDL_RenderDebugText(renderer, 380.0f, 304.0f, "the error falls, never to zero.");
                        }
                        else if (scene_mode == demo::scene_kind::model)
                        {
                            // The file's three streams, then what they became, then
                            // whether the result is safe to draw. Every number here
                            // was measured at load time; none of it was promised.
                            SDL_RenderDebugTextFormat(renderer, 380.0f, 262.0f,
                                "file: v %d  vt %d  vn %d", model.load.positions,
                                model.load.uvs, model.load.normals);
                            SDL_RenderDebugTextFormat(renderer, 380.0f, 276.0f,
                                "-> %d verts (%+d split)",
                                model.load.vertices, model.load.split_vertices);

                            const bool wound = model.check.consistently_wound();
                            SDL_SetRenderDrawColor(renderer, wound ? 150 : 236,
                                                             wound ? 152 : 92,
                                                             wound ? 170 : 92, 255);
                            SDL_RenderDebugTextFormat(renderer, 380.0f, 290.0f,
                                "euler %+d  %s  %s", model.check.euler,
                                model.check.closed() ? "closed" : "OPEN",
                                wound ? "wound ok" : "WINDING!");
                            SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                            SDL_RenderDebugTextFormat(renderer, 380.0f, 304.0f,
                                "volume %+.3f  E %d",
                                static_cast<double>(model.check.signed_volume),
                                model.check.edges);

                            if (model.choice == demo::model_choice::torus)
                            {
                                // Writer and reader, checked against each other in
                                // the only currency that matters: pixels.
                                SDL_SetRenderDrawColor(renderer, roundtrip_wrong ? 236 : 122,
                                                                 roundtrip_wrong ? 92 : 196,
                                                                 roundtrip_wrong ? 92 : 152, 255);
                                SDL_RenderDebugTextFormat(renderer, 380.0f, 318.0f,
                                    "round trip: %d px differ", roundtrip_wrong);
                            }
                            else if (model.choice == demo::model_choice::twisted)
                            {
                                SDL_SetRenderDrawColor(renderer, 236, 196, 110, 255);
                                SDL_RenderDebugText(renderer, 380.0f, 318.0f,
                                    "[U] cull back -> a hole");
                            }

                            // ---- Lesson 3.9 -------------------------------
                            // Which uv convention this mesh's coordinates are in.
                            // The mesh cannot answer that by looking at its own
                            // numbers — 0.7 is a perfectly good coordinate in
                            // either — so the importer has to remember, and the
                            // HUD has to say. Every convention bug in a pipeline
                            // is somebody assuming this.
                            SDL_SetRenderDrawColor(renderer, model.uv_flipped ? 150 : 236,
                                                             model.uv_flipped ? 152 : 196,
                                                             model.uv_flipped ? 170 : 110, 255);
                            SDL_RenderDebugText(renderer, 380.0f, 332.0f,
                                model.uv_flipped ? "[2] uv v: flipped (texture)"
                                                 : "[2] uv v: RAW OBJ - upside down");
                        }
                        else if (scene_mode == demo::scene_kind::zfight)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "B is 1 mm in front of A, so B");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, "should win everywhere. [B] to");
                            SDL_RenderDebugText(renderer, 380.0f, 290.0f, "D16_UNORM and watch it stop.");
                        }
                        else if (order != engine::trs_order::trs)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "wrong model order [O]: 2.8's");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, order == engine::trs_order::tsr
                                                ? "shear is back." : "orbit-origin is back.");
                        }
                        else if (use_perspective)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "the whole chain, one corner:");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, "model->world->view->clip->ndc");
                            SDL_RenderDebugText(renderer, 380.0f, 290.0f, "->screen. [F] hides surfaces.");
                        }
                        else
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "orthographic: every cube the same");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, "size, rails parallel, dolly inert.");
                            SDL_RenderDebugText(renderer, 380.0f, 290.0f, "[P] back to perspective.");
                        }

                        SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
                        SDL_RenderDebugText(renderer, 6.0f, 314.0f,
                                            "[F] hidden surface  [C] scene  [B] depth  [I] interp  [T] tessellate");
                        SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                            "[arrows] orbit  [-][=] dolly  [3] budget  [4] encode  [Tab] screen");
            }
        }
        else if (which == screen::basis)
        {
            // Lesson 2.5's readout. The matrix is printed the way it is WRITTEN —
            // rows across — while the struct stores columns; showing both side by
            // side is the point, because that mismatch is where transposed-matrix
            // bugs come from.
            const float det = engine::determinant(basis_m);
            const float predicted = k_basis_unit * k_basis_unit * std::fabs(det);

            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "BASIS TRANSFORM   %-26s  t = %+.2f",
                                      name_of(basis_mode), static_cast<double>(basis_t));

            SDL_RenderDebugTextFormat(renderer, 380.0f, 40.0f, "written:      stored:");
            SDL_RenderDebugTextFormat(renderer, 380.0f, 54.0f, "| %+.3f %+.3f |   [%+.3f,",
                                      static_cast<double>(basis_m.at(0, 0)),
                                      static_cast<double>(basis_m.at(0, 1)),
                                      static_cast<double>(basis_m.c0.x));
            SDL_RenderDebugTextFormat(renderer, 380.0f, 68.0f, "| %+.3f %+.3f |    %+.3f,",
                                      static_cast<double>(basis_m.at(1, 0)),
                                      static_cast<double>(basis_m.at(1, 1)),
                                      static_cast<double>(basis_m.c0.y));
            SDL_RenderDebugTextFormat(renderer, 380.0f, 82.0f, "                  %+.3f,",
                                      static_cast<double>(basis_m.c1.x));
            SDL_RenderDebugTextFormat(renderer, 380.0f, 96.0f, "                  %+.3f]",
                                      static_cast<double>(basis_m.c1.y));

            SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
            SDL_RenderDebugText(renderer, 380.0f, 118.0f, "red = where (1,0) landed");
            SDL_RenderDebugText(renderer, 380.0f, 132.0f, "green = where (0,1) landed");
            SDL_RenderDebugText(renderer, 380.0f, 146.0f, "the columns, drawn.");

            // The determinant, checked against the rasterizer rather than asserted.
            SDL_SetRenderDrawColor(renderer, det < 0.0f ? 236 : 122,
                                             det < 0.0f ? 92 : 196,
                                             det < 0.0f ? 92 : 152, 255);
            SDL_RenderDebugTextFormat(renderer, 380.0f, 170.0f, "det = %+.4f%s",
                                      static_cast<double>(det),
                                      det < 0.0f ? "  (flipped!)" : "");
            SDL_RenderDebugTextFormat(renderer, 380.0f, 184.0f, "area: %d px measured",
                                      basis_area_px);
            SDL_RenderDebugTextFormat(renderer, 380.0f, 198.0f, "      %.0f px predicted",
                                      static_cast<double>(predicted));

            if (basis_mode == xform::scale_then_rotate || basis_mode == xform::rotate_then_scale)
            {
                SDL_SetRenderDrawColor(renderer, 226, 196, 110, 255);
                SDL_RenderDebugText(renderer, 380.0f, 222.0f, "gold outline = the OTHER");
                SDL_RenderDebugText(renderer, 380.0f, 236.0f, "order. Same det, different");
                SDL_RenderDebugText(renderer, 380.0f, 250.0f, "shape.");
            }

            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                "[Z] transform  [,] [.] adjust t  [0] reset  [Space] animate  [Tab] screen");
        }
        else if (which == screen::lines)
        {
            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "LINES (Lesson 2.1)   %-16s   %d spokes   fps %5.1f",
                                      line_algo_name, k_spokes,
                                      static_cast<double>(clk.fps()));
            SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                "[1] naive  [2] DDA  [3] Bresenham   [Space] demo::spin   [Tab] next screen   [Esc] quit");
        }
        else
        {
            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "TRIANGLES   mode: %-12s   %6.1f us/triangle   fps %5.1f",
                                      name_of(mode), tri_ns_avg / 1000.0,
                                      static_cast<double>(clk.fps()));

            if (is_attribute_view(mode))
            {
                // Lesson 2.4's readout. The centre pixel is the number the
                // linear-vs-encoded argument turns on: 156 against 85.
                SDL_SetRenderDrawColor(renderer,
                                       linear_blend ? 150 : 236,
                                       linear_blend ? 152 : 92,
                                       linear_blend ? 170 : 92, 255);
                SDL_RenderDebugTextFormat(renderer, 6.0f, 20.0f,
                                          "[M] blend space: %-8s   centre pixel (%3u,%3u,%3u)",
                                          linear_blend ? "LINEAR" : "encoded",
                                          engine::red_of(centroid_px),
                                          engine::green_of(centroid_px),
                                          engine::blue_of(centroid_px));
            }
            else
            {
                // The line that turns the coverage picture into a diagnosis.
                SDL_SetRenderDrawColor(renderer,
                                       doubled_px > 0 ? 236 : 150,
                                       doubled_px > 0 ? 92 : 152,
                                       doubled_px > 0 ? 92 : 170, 255);
                SDL_RenderDebugTextFormat(renderer, 6.0f, 20.0f,
                                          "[R] fill rule: %-3s    shared edge drawn twice on %d px",
                                          use_fill_rule ? "ON" : "OFF", doubled_px);
            }

            // Lesson 2.3's readout: the three weights, and the sum that must
            // be 1. Printed to four places because the interesting thing is how
            // close to 1 it gets, not that it rounds there.
            if (mode == tri_mode::weights || mode == tri_mode::isolines)
            {
                SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
                SDL_RenderDebugTextFormat(renderer, 6.0f, 292.0f,
                                          "probe (%3d,%3d)   w0 %+.4f  w1 %+.4f  w2 %+.4f   sum %.6f",
                                          probe_x, probe_y,
                                          static_cast<double>(probe.w0),
                                          static_cast<double>(probe.w1),
                                          static_cast<double>(probe.w2),
                                          static_cast<double>(probe.w0 + probe.w1 + probe.w2));
                SDL_RenderDebugText(renderer, 6.0f, 306.0f,
                                    "move the mouse: each sub-triangle's AREA is the weight of the vertex it faces");
            }

            // Text coordinates are exactly twice framebuffer coordinates (2x
            // text scale over a 4x framebuffer scale), so the magnifier at
            // framebuffer y 40..110 occupies text rows 80..220. Everything
            // below is placed against that, not by eye.
            if (is_attribute_view(mode))
            {
                SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                SDL_RenderDebugTextFormat(renderer, 344.0f, 64.0f,
                                          "a 12-px triangle, %.2f bands",
                                          static_cast<double>(stripe_bands));
                SDL_RenderDebugText(renderer, 344.0f, 228.0f, "left:  unbiased (correct)");
                SDL_RenderDebugText(renderer, 344.0f, 240.0f, "right: fill-rule bias left in");

                SDL_SetRenderDrawColor(renderer, bias_differ > 0 ? 236 : 122,
                                                 bias_differ > 0 ? 92 : 196,
                                                 bias_differ > 0 ? 92 : 152, 255);
                SDL_RenderDebugTextFormat(renderer, 344.0f, 256.0f,
                                          "%d px differ (field moves 0.088 px)", bias_differ);

                SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                SDL_RenderDebugText(renderer, 344.0f, 272.0f,
                                    "[ and ] resweep: the count jumps");
                SDL_RenderDebugText(renderer, 344.0f, 284.0f,
                                    "between 0 and 15 for one fixed error");
            }
            else
            {
                SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                SDL_RenderDebugText(renderer, 400.0f, 40.0f, "two triangles,");
                SDL_RenderDebugText(renderer, 400.0f, 52.0f, "one shared edge");
                SDL_RenderDebugText(renderer, 400.0f, 68.0f, "green = drawn once");
                SDL_RenderDebugText(renderer, 400.0f, 80.0f, "red   = drawn twice");
            }

            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugText(renderer, 6.0f, 316.0f,
                                "[1] fill [2] wire [3] planes [4] weights [5] iso [6] gouraud [7] uv");
            SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                "[M] blend  [ ] bands  [R] rule  [Space] demo::spin  [Tab] screen  [Esc] quit");
        }

        // Over the render, deliberately — see draw_budget. Drawn last so nothing
        // else can paint over it, and only in the demo whose frame it describes.
        if (which == screen::scene && show_budget)
        {
            draw_budget(renderer, prof, clk.raw_dt() * 1.0e9f, covered_px, fill_ns_per_px,
                        encode, encode_wrong, walk, scene_quads, 6.0f, 170.0f);
        }

        SDL_SetRenderScale(renderer, 1.0f, 1.0f);

        prof.add(engine::zone::overlay,
                 prof.to_ns(engine::profiler::now_ticks() - hud_t0));

        // The frame ends here, before the blocking call. See `begin_frame` above.
        prof.end_frame();

        plat.present();

        // The frame throttle moved from [T] to [Y] in Lesson 3.2, because [T]
        // now cycles the floor tessellation and a key that means two things in
        // two demos is a key that gets pressed by accident in both.
        if (in.key_down(SDL_SCANCODE_Y))
        {
            SDL_Delay(k_throttle_ms);
        }
    }

    // No teardown. `plat`'s destructor destroys the texture, the renderer and the
    // window — in that order — and calls SDL_Quit, which is the same four lines
    // that used to be here except that they now also run on every early return
    // above. That is not tidiness: this function has five exits, and four of them
    // used to unwind a different amount.
    return 0;
}
