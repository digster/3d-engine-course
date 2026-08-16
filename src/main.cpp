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

#include "core/clock.hpp"
#include "core/fixed_step.hpp"
#include "core/input.hpp"
#include "core/profile.hpp"   // Lesson 3.10: the frame budget
#include "game/pong.hpp"
#include "gfx/clip.hpp"
#include "gfx/colour.hpp"
#include "gfx/depth_buffer.hpp"
#include "gfx/framebuffer.hpp"
#include "gfx/gpu_device.hpp"    // Lesson 4.2: the device, and the window claim
#include "gfx/gpu_present.hpp"   // Lesson 4.2: a framebuffer, carried by the GPU
#include "gfx/gpu_shader.hpp"    // Lesson 4.3: HLSL, compiled and on the device
#include "gfx/light.hpp"
#include "gfx/mesh.hpp"
#include "gfx/obj.hpp"
#include "gfx/raster.hpp"
#include "gfx/texture.hpp"
#include "gfx/viewport.hpp"
#include "math/mat2.hpp"
#include "math/mat3.hpp"
#include "math/mat4.hpp"
#include "math/transform.hpp"
#include "math/vec2.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int k_fb_width = 320;
constexpr int k_fb_height = 180;

constexpr Uint32 k_throttle_ms = 50;

/// Which lesson's demo is on screen. [Tab] cycles.
enum class demo
{
    scene,       ///< Lessons 2.6 – 2.8 — this lesson
    basis,       ///< Lesson 2.5
    triangles,   ///< Lessons 2.2 – 2.4
    lines,       ///< Lesson 2.1
    pong         ///< Lesson 1.8
};

[[nodiscard]] demo next_demo(demo d)
{
    switch (d)
    {
    case demo::scene:     return demo::basis;
    case demo::basis:     return demo::triangles;
    case demo::triangles: return demo::lines;
    case demo::lines:     return demo::pong;
    case demo::pong:      return demo::scene;
    }
    return demo::scene;
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

// ===========================================================================
// Lesson 2.6 — mat3, mat4, and the first three-dimensional thing we have drawn
// ===========================================================================

/// Which rotation the cube view is showing. [Z] cycles.
enum class spin
{
    about_x,
    about_y,
    about_z,
    tumble_xy,   ///< Rx * Ry — y first
    tumble_yx    ///< Ry * Rx — x first
};

[[nodiscard]] const char* name_of(spin s)
{
    switch (s)
    {
    case spin::about_x:   return "rotation_x(t)";
    case spin::about_y:   return "rotation_y(t)";
    case spin::about_z:   return "rotation_z(t)";
    case spin::tumble_xy: return "Rx(t) * Ry(1.1)   [y first]";
    case spin::tumble_yx: return "Ry(1.1) * Rx(t)   [x first]";
    }
    return "?";
}

[[nodiscard]] spin next_spin(spin s)
{
    switch (s)
    {
    case spin::about_x:   return spin::about_y;
    case spin::about_y:   return spin::about_z;
    case spin::about_z:   return spin::tumble_xy;
    case spin::tumble_xy: return spin::tumble_yx;
    case spin::tumble_yx: return spin::about_x;
    }
    return spin::about_x;
}

[[nodiscard]] engine::mat3 build_spin(spin s, float t)
{
    // The two tumble modes share both ingredients and differ ONLY in which
    // rotation is applied first — the 3-D restatement of Lesson 2.5 §3.6.
    switch (s)
    {
    case spin::about_x:   return engine::rotation_x(t);
    case spin::about_y:   return engine::rotation_y(t);
    case spin::about_z:   return engine::rotation_z(t);
    case spin::tumble_xy: return engine::rotation_x(t) * engine::rotation_y(1.1f);
    case spin::tumble_yx: return engine::rotation_y(1.1f) * engine::rotation_x(t);
    }
    return engine::mat3::identity();
}

// The viewport: the 3-D scene is drawn into a 16:9 rectangle in the left part of
// the 320x180 framebuffer. NDC's [-1,1] x [-1,1] square maps onto it, and because
// the projection matrix bakes the 16:9 aspect into x, the rectangle itself must be
// 16:9 or the picture stretches.
//
// As of Lesson 2.11 this is an `engine::viewport` (mirroring SDL_GPUViewport)
// rather than three loose constants: top-left (6, 41.625), 172 x 96.75 px (16:9),
// depth [0, 1]. Its centre is (92, 90) and it produces exactly the same pixels the
// old constants did — this lesson named and homed the transform, it did not move a
// single vertex.
constexpr engine::viewport k_scene_viewport{6.0f, 41.625f, 172.0f, 96.75f, 0.0f, 1.0f};

// ...and, as of Lesson 3.2, a second one: the WHOLE framebuffer.
//
// A ground plane cannot be made to fit a sub-rectangle. Measured (scratch/fit_floor.cpp):
// across every floor extent worth having, the near edge projects to |x_ndc| > 1 —
// which is not a tuning failure but geometry. A surface you are standing on fills
// the bottom of your view, and that is what makes it a good subject for this
// lesson in the first place.
//
// So the floor scene draws through a viewport that is the entire 320x180 buffer.
// That costs nothing and needs no new projection matrix, because 320x180 IS 16:9 —
// the same aspect the projection already bakes in. It is also the first time this
// course changes the viewport at all, which is the point of Lesson 2.11 having
// made it a parameter rather than three constants.
constexpr engine::viewport k_full_viewport{0.0f, 0.0f,
                                           static_cast<float>(k_fb_width),
                                           static_cast<float>(k_fb_height), 0.0f, 1.0f};

// ---------------------------------------------------------------------------
// Lesson 3.3 — the near plane
// ---------------------------------------------------------------------------

/// What this demo does with geometry that crosses the near plane. [K] cycles.
///
/// Three settings rather than a bool, because there are **two different wrong
/// answers** here and they fail in visibly different ways. Seeing both is the
/// lesson; a bool would only let you see one.
enum class near_mode
{
    /// Lesson 3.2's behaviour: if any vertex is behind the near plane, drop the
    /// whole triangle (and the whole line). Never crashes, never produces a wild
    /// coordinate — and is catastrophically wrong the moment the geometry matters,
    /// because the surface you are walking into *disappears* instead of filling
    /// the view. Dolly into the floor scene and the ground under your feet goes.
    drop,

    /// No guard at all: divide by `w` whatever it happens to be. This is what the
    /// code did before the guard existed, and it is the failure Lesson 2.7 warned
    /// about in the abstract. A vertex behind the eye has `w < 0`, so `x/w` flips
    /// its sign and the vertex lands on the **opposite side of the screen**; the
    /// triangle stretches across the whole frame or turns inside out.
    none,

    /// Cut the geometry along the near plane and rasterize the part in front.
    /// **The answer**, and the default.
    clip
};

[[nodiscard]] const char* name_of(near_mode m)
{
    switch (m)
    {
    case near_mode::drop: return "DROP (3.2's bug)";
    case near_mode::none: return "NONE (no guard)";
    case near_mode::clip: return "CLIP (correct)";
    }
    return "?";
}

[[nodiscard]] near_mode next_near(near_mode m)
{
    switch (m)
    {
    case near_mode::clip: return near_mode::drop;
    case near_mode::drop: return near_mode::none;
    case near_mode::none: return near_mode::clip;
    }
    return near_mode::clip;
}

/// Everything needed to turn a view-space point into a pixel, in one object.
///
/// This used to be two loose parameters (`proj`, `vp`) threaded through six
/// functions, and Lesson 3.3 was about to make it three. Gathering them is the
/// same argument `fill_style` made in Lesson 3.2, applied one level up: state that
/// always travels together should travel as one thing, and adding a knob should
/// cost one field rather than one more parameter at every call site. It is also
/// the shape Module 5's renderer has — a camera hands the frame a projection, a
/// viewport, and its clipping rules, and everything downstream reads them.
struct projector
{
    engine::mat4 proj;                  ///< view -> clip (Lesson 2.10)
    engine::viewport vp;                ///< NDC -> pixels, with the y flip (2.11)
    near_mode near = near_mode::clip;   ///< what happens at the near plane (3.3)
};

/// VIEW space -> CLIP space. One matrix multiply — and the pipeline's new stopping
/// point, because clipping has to happen *before* the divide.
[[nodiscard]] engine::vec4 to_clip(engine::vec3 v_view, const engine::mat4& proj)
{
    return proj * engine::point(v_view);
}

/// A point that has been through the divide: pixels, device depth, and `1/w`.
///
/// `depth` is the device depth in [0, 1] that Lesson 2.11's viewport transform has
/// been producing all along, and which Lesson 3.1 gave somewhere to go. `inv_w` is
/// the reciprocal of the clip-space `w`, which Lesson 3.2 needs at every corner;
/// it is computed here because this is the last place that still *has* `w` —
/// `perspective_divide` consumes it and does not give it back.
///
/// Note what this struct no longer carries: a `visible` flag. Before Lesson 3.3
/// the projection answered "…and is this point usable at all?", which was the
/// polite way of saying the pipeline had no plan for geometry near the eye. Now it
/// has one, and it runs a step earlier.
struct screen_point
{
    engine::vec2 xy;
    float depth;
    float inv_w;
};

/// A float pixel coordinate, rounded and made **safe to convert**.
///
/// Converting a float to an int is undefined behaviour when the value does not fit
/// in the int, and `near_mode::none` produces exactly that: a `w` near zero sends a
/// coordinate into the millions. This is not the fix for the near plane — clipping
/// is — it is what lets the broken mode be *shown* without the program losing its
/// meaning while you look at it.
///
/// The bound is 8,000 and not something larger for a second reason. `edge_function`
/// (raster.hpp) multiplies coordinate *differences*: at ±8,000 the products stay
/// comfortably inside a 32-bit int, and signed overflow is undefined too. One
/// constant, two undefined behaviours held off.
[[nodiscard]] int to_pixel(float f)
{
    constexpr float k_wild = 8000.0f;
    if (std::isnan(f)) { return 0; }   // every comparison with a NaN is false, so clamp cannot
    return static_cast<int>(std::lround(std::clamp(f, -k_wild, k_wild)));
}

/// CLIP space -> framebuffer pixels: the perspective divide, then the viewport map.
///
/// **This function assumes its input has already been clipped.** That is the whole
/// contract, and it is why it has no guard: `w` at or behind the eye is not a case
/// to branch on here, it is a case that must not reach here. The demo's `none` mode
/// deliberately breaks that contract to show what happens.
[[nodiscard]] screen_point screen_from_clip(const engine::vec4& clip,
                                            const engine::viewport& vp)
{
    // `w == 0` is the plane through the eye itself, where every ratio is 0/0 — a
    // NaN, not a large number. Nudging it off zero is demo scaffolding, not engine
    // policy: it makes `none` produce a wildly wrong *picture* rather than
    // undefined behaviour. Under `clip` the branch is unreachable, because the near
    // plane sits a whole `near` in front of `w = 0`.
    const float w = (clip.w == 0.0f) ? 1.0e-6f : clip.w;
    const engine::vec3 ndc = engine::perspective_divide({clip.x, clip.y, clip.z, w});

    // The viewport does the NDC -> pixel map AND the +y-up-to-+y-down flip. Its
    // third output is the device depth the z-buffer stores (Lesson 3.1).
    const engine::vec3 screen = vp.to_screen(ndc);
    return {{screen.x, screen.y}, screen.z, 1.0f / w};
}

/// Draw a line whose endpoints are in VIEW space.
///
/// The one-dimensional case of the whole lesson, and the place the fix is easiest
/// to read: build both endpoints in clip space, hand them to the clipper, and only
/// then divide. Before Lesson 3.3 a line crossing the near plane simply vanished —
/// which is why walking over a gridline used to make it disappear rather than run
/// off the bottom of the screen.
void line3(engine::framebuffer& fb, engine::vec3 a, engine::vec3 b,
           Uint32 colour, const projector& pr)
{
    engine::clip_vertex ca{to_clip(a, pr.proj), {}, colour};
    engine::clip_vertex cb{to_clip(b, pr.proj), {}, colour};

    switch (pr.near)
    {
    case near_mode::clip:
        if (!engine::clip_segment_near(ca, cb)) { return; }
        break;

    case near_mode::drop:
        if (engine::near_distance(ca.position) < 0.0f
         || engine::near_distance(cb.position) < 0.0f) { return; }
        break;

    case near_mode::none:
        break;
    }

    const screen_point pa = screen_from_clip(ca.position, pr.vp);
    const screen_point pb = screen_from_clip(cb.position, pr.vp);
    engine::draw_line(fb, to_pixel(pa.xy.x), to_pixel(pa.xy.y),
                          to_pixel(pb.xy.x), to_pixel(pb.xy.y), colour);
}

/// Draw a WORLD-space line through the view matrix and the projector. Convenience
/// for the world grid and axes, whose endpoints are natural world-space constants.
void line3_world(engine::framebuffer& fb, const engine::mat4& view, const projector& pr,
                 engine::vec3 a, engine::vec3 b, Uint32 colour)
{
    line3(fb, engine::xyz(view * engine::point(a)),
          engine::xyz(view * engine::point(b)), colour, pr);
}

// ---------------------------------------------------------------------------
// Drawing a MESH, in MODEL SPACE
// ---------------------------------------------------------------------------
//
// The geometry itself now lives in gfx/mesh.hpp as vertices + triangle indices
// (Lesson 2.12). What has not changed since Lesson 2.8 is the claim those
// coordinates make: a mesh is not ANYWHERE. It is not at the left of the screen,
// not two metres from the door, not big or small. It is a shape in ITS OWN
// coordinates — model space — and the model matrix says where that space goes.
//
// Which is why the same twelve-vertex icosahedron and the same eight-vertex cube
// serve every object in the scene below, at different sizes, orientations and
// places. That is what model space buys, and it is why a game ships one crate and
// places four hundred.

/// Draw a mesh as a wireframe, with edge brightness standing in for depth.
///
/// Every triangle contributes its three edges. Shared edges are therefore drawn
/// TWICE — 60 line draws for the icosahedron's 30 unique edges — which is honest
/// waste worth naming rather than hiding. Deduplicating would mean building an edge
/// list, and the moment Module 3 fills these triangles the duplication vanishes on
/// its own, so we pay it and say so.
///
/// The brightness is a **cue, not a calculation** — there is no depth buffer yet
/// (Lesson 3.1) and no lighting (Lesson 3.6), so this is simply "edges further away
/// are dimmer" so the wireframe can be read at all. Without it a spinning wireframe
/// is a genuinely ambiguous picture: your eye flips between two interpretations,
/// because a wireframe has discarded the only information that could settle it.
///
/// @param point_w  the fourth component the vertices are sent with. 1 is correct;
///                  0 reproduces Lesson 2.6, where every translation is ignored.
void draw_mesh(engine::framebuffer& fb, const engine::mesh& geometry,
               const engine::mat4& m, const projector& pr, float point_w)
{
    // Transform each vertex ONCE, into view space, and keep the results. This is
    // the whole practical argument for indexed geometry: the icosahedron's twelve
    // vertices are shared by twenty triangles, so transforming per-vertex rather
    // than per-triangle-corner does 12 matrix multiplies instead of 60.
    engine::vec3 p[64];
    const std::size_t vertex_count = std::min(geometry.vertices.size(), std::size(p));
    for (std::size_t i = 0; i < vertex_count; ++i)
    {
        // A vertex is a POSITION, so w = 1 and the model+view matrix's translation
        // is added in full. p[] comes out in VIEW space — the projection to the
        // screen happens later, inside line3, through `proj`.
        p[i] = engine::xyz(m * engine::to_vec4(geometry.vertices[i], point_w));
    }

    // Walk the index array three at a time; each triple is one triangle, and each
    // triangle draws its three edges.
    for (std::size_t t = 0; t + 2 < geometry.indices.size(); t += 3)
    {
        const std::size_t tri[3] = {geometry.indices[t], geometry.indices[t + 1],
                                    geometry.indices[t + 2]};
        for (int e = 0; e < 3; ++e)
        {
            const std::size_t ia = tri[e];
            const std::size_t ib = tri[(e + 1) % 3];
            if (ia >= vertex_count || ib >= vertex_count) { continue; }

            // Depth from view-space z (distance in front of the camera, negative
            // since the camera looks down -z). Nearer edges (z closer to 0) are
            // brighter. The window -13..-2 covers the scene's view-space z spread
            // across the dolly range; a cue, not a z-buffer (Lesson 3.1).
            const float mid_z = 0.5f * (p[ia].z + p[ib].z);
            const float t01 = std::clamp((mid_z + 13.0f) / 11.0f, 0.0f, 1.0f);
            const Uint8 v = static_cast<Uint8>(70.0f + 165.0f * t01);
            line3(fb, p[ia], p[ib], engine::pack_argb(v, v, static_cast<Uint8>(v * 0.94f)), pr);
        }
    }
}

/// The three transformed basis vectors, in the course's axis colours.
///
/// Exactly Lesson 2.5's picture with a third arrow: the columns of the matrix,
/// drawn. Whatever the cube is doing, these three arrows are why — and as of
/// Lesson 2.8 they have a name. Sending the model's basis vectors through the
/// model matrix as DIRECTIONS returns its first three columns, so these arrows
/// are the object's own axes, expressed in world space.
/// @param dir_w  the fourth component the AXES are sent with. 0 is correct — they
///               are directions. 1 translates them, which is the classic
///               transform-a-normal-as-a-point bug, drawn.
void draw_axes3(engine::framebuffer& fb, const engine::mat4& m, const projector& pr,
                float point_w, float dir_w)
{
    const Uint32 col[3] = {engine::pack_argb(236, 92, 92),     // x
                           engine::pack_argb(122, 196, 152),   // y
                           engine::pack_argb(126, 162, 236)};  // z
    const engine::vec3 basis[3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

    // Where the object actually is. A position, so w = 1.
    const engine::vec3 origin = engine::xyz(m * engine::to_vec4(engine::vec3{}, point_w));

    for (int i = 0; i < 3; ++i)
    {
        // …and the arrows are DIRECTIONS, so w = 0. They must be rotated by the
        // matrix and not moved by it — we place them ourselves, at the object.
        const engine::vec3 d = engine::xyz(m * engine::to_vec4(basis[i], dir_w));
        const engine::vec3 tip = origin + d * 0.75f;
        const engine::vec3 offset = origin;
        line3(fb, offset, tip, col[i], pr);

        // A small head, built in SCREEN space by rotating the projected arrow —
        // the same trick as 2.5's, now after the projection because that is where
        // the head has to be legible.
        //
        // This is the ONE place in the demo where dropping is still the right
        // answer, and it is worth being explicit about why rather than leaving it
        // looking like an oversight. The head is screen-space DECORATION: it is
        // constructed by rotating the projected shaft, so if either end of that
        // shaft is behind the eye the rotation has nothing meaningful to act on.
        // A decoration with no defined position is dropped; GEOMETRY is clipped.
        // Lesson 3.3 §4.4.
        const engine::vec4 clip_a = to_clip(offset, pr.proj);
        const engine::vec4 clip_b = to_clip(tip, pr.proj);
        if (engine::near_distance(clip_a) < 0.0f || engine::near_distance(clip_b) < 0.0f)
        {
            continue;
        }
        const screen_point a = screen_from_clip(clip_a, pr.vp);
        const screen_point b = screen_from_clip(clip_b, pr.vp);
        const engine::vec2 back = engine::normalised(a.xy - b.xy) * 5.0f;
        if (engine::length_squared(back) > 0.0f)
        {
            const engine::vec2 h1 = b.xy + engine::rotation(0.5f) * back;
            const engine::vec2 h2 = b.xy + engine::rotation(-0.5f) * back;
            auto ix = [](float f) { return static_cast<int>(std::lround(f)); };
            engine::draw_line(fb, ix(b.xy.x), ix(b.xy.y), ix(h1.x), ix(h1.y), col[i]);
            engine::draw_line(fb, ix(b.xy.x), ix(b.xy.y), ix(h2.x), ix(h2.y), col[i]);
        }
    }
}

// ---------------------------------------------------------------------------
// Lesson 2.8 — model space, world space, and the order of T, R and S
// ---------------------------------------------------------------------------

/// Which order the model matrix is composed in. [O] cycles.
///
/// Only the first is right. The other two are here because they are the two
/// mistakes people actually make, and because each one fails in a specific,
/// nameable way that is far more instructive than being told the correct answer.
enum class trs_order
{
    trs,   ///< T * R * S — scale, then rotate, then translate. Correct.
    tsr,   ///< T * S * R — rotate first, then scale along WORLD axes. Deforms.
    rts    ///< R * T * S — translate before rotating. Orbits the world origin.
};

[[nodiscard]] const char* name_of(trs_order o)
{
    switch (o)
    {
    case trs_order::trs: return "T * R * S   (correct)";
    case trs_order::tsr: return "T * S * R   (scale last)";
    case trs_order::rts: return "R * T * S   (rotate last)";
    }
    return "?";
}

[[nodiscard]] trs_order next_order(trs_order o)
{
    switch (o)
    {
    case trs_order::trs: return trs_order::tsr;
    case trs_order::tsr: return trs_order::rts;
    case trs_order::rts: return trs_order::trs;
    }
    return trs_order::trs;
}

/// Build a model matrix in a chosen — possibly wrong — order.
///
/// `parent_from_local()` in transform.hpp always builds T*R*S, because that is
/// the only order an engine should ever offer. This function exists purely so the
/// demo can put the wrong answers on screen next to the right one; it is the same
/// bargain struck for `draw_line_naive` (2.1), Pong's unswept collision (1.8),
/// `blend_space::encoded` (2.4) and the `w` toggles (2.7). A failure you can
/// summon on a keypress teaches more than a paragraph describing it.
[[nodiscard]] engine::mat4 model_matrix(const engine::transform& t, trs_order order)
{
    if (order == trs_order::trs) { return engine::parent_from_local(t); }

    const engine::mat4 T = engine::translation(t.position);
    const engine::mat4 R = engine::to_mat4(t.rotation);
    const engine::mat4 S = engine::to_mat4(engine::scale(t.scale.x, t.scale.y, t.scale.z));

    // Spelled out as three separate 4x4 factors rather than folded, so the source
    // reads in the same order as the name printed on screen.
    if (order == trs_order::tsr) { return T * S * R; }
    return R * T * S;
}

/// One thing in the world: where it is, and what shape it is.
///
/// A transform plus a mesh. That pairing is the smallest useful definition of a
/// renderable object, and it is deliberately kept OUT of `engine::transform` — a
/// transform is a placement, not a thing, and Module 5's ECS will attach geometry
/// to a transform as a separate component for exactly this reason. Here it is a
/// two-field demo struct, which is the honest amount of machinery for three objects.
///
/// The `mesh` is stored BY VALUE and that is cheap: it is two spans, four words,
/// pointing at static geometry that outlives everything.
///
/// Lesson 3.1 adds a `tint`. Triangles are filled now, and a fill needs a colour;
/// there is no lighting until 3.6, so each object simply carries one.
struct scene_object
{
    engine::transform xform;
    engine::mesh geometry;
    const char* name;
    Uint32 tint = 0xFFFFFFFFu;

    /// Is this geometry a **closed surface** — a solid with an inside you can never
    /// see into? Added in Lesson 3.4, because it is the precondition for back-face
    /// culling and nothing else in the engine knows it.
    ///
    /// The cube and the icosahedron are closed. A quad is not, and neither is the
    /// ground plane: they are infinitely thin sheets with two visible sides, so
    /// culling their back faces makes them disappear when seen from behind. That is
    /// not a bug in the culler, it is culling being applied to geometry that does
    /// not satisfy its assumption.
    ///
    /// In a real engine this lives on the **material**, because cull mode is
    /// pipeline state and pipeline state is what a material *is* (Module 6). Here it
    /// is a bool on the object and the demo only warns, because the whole scene is
    /// drawn in one batch with one style — which is itself the honest lesson: two
    /// cull modes means two batches.
    bool closed = true;

    /// How shiny this object is, and what colour its highlight comes out —
    /// Lesson 3.7.
    ///
    /// **Look at what just happened to this struct.** `tint` was enough while a
    /// surface was one colour; then 3.4 needed `closed`; now 3.7 needs two more
    /// numbers, and all four describe the same thing — *the surface* — while
    /// `xform` and `geometry` describe where it is and what shape it is. That is
    /// two different kinds of data wearing one struct, and the second kind has a
    /// name: a material. Module 6 gives it one. It is being left visible here
    /// rather than fixed early, because a material invented before three lessons
    /// have asked for one is a guess.
    ///
    /// Defaults to a black highlight, which is exactly Lesson 3.6's shading — so
    /// every object that says nothing about shininess looks precisely as it did.
    engine::specular surface{};
};

/// The three objects in the world, and why each one is here.
///
/// The icosahedron is the milestone's hero — a real mesh, twelve vertices and twenty
/// faces, not an axis-aligned box, so a rotation reads richly instead of looking like
/// a square wobbling. The other two are the SAME eight-vertex cube at different
/// transforms, kept because their roles are still teaching something (Lesson 2.8):
///
///   icosahedron  UNIFORM scale, spinning.   T*R*S and T*S*R are bit-identical.
///   slab (cube)  non-uniform, spinning.     The order matters, visibly and always.
///   plinth(cube) non-uniform, still.        Identity rotation, so likewise identical.
///
/// Two of the three are controls. That is the point of [O]: get the composition
/// order wrong and two thirds of the scene still looks perfect, which is exactly how
/// such a bug ships.
constexpr int k_max_objects = 4;

// ---------------------------------------------------------------------------
// Lesson 3.1 — the scenes that break sorting, and the scene that does not
// ---------------------------------------------------------------------------

/// Which scene is loaded. [C] cycles.
///
/// The first is the Module 2 milestone, on which a back-to-front sort works
/// perfectly. The other three each break it in a different, irreparable way —
/// which is the argument for solving visibility per PIXEL instead.
enum class scene_kind
{
    solids,      ///< 2.12's icosahedron and cubes. Sorting works here.
    cycle,       ///< three planks: A over B over C over A. No order is correct.
    intersect,   ///< two quads passing through each other. Order changes mid-triangle.
    zfight,      ///< two near-coplanar quads. The z-buffer's own failure mode.
    floor,       ///< Lesson 3.2 — a checkered plane running to the horizon.
    model        ///< Lesson 3.5 — geometry read off a disk. [L] chooses which.
};

[[nodiscard]] const char* name_of(scene_kind k)
{
    switch (k)
    {
    case scene_kind::solids:    return "solids (sorting looks fine)";
    case scene_kind::cycle:     return "CYCLE  (A>B>C>A)";
    case scene_kind::intersect: return "INTERSECTING quads";
    case scene_kind::zfight:    return "near-coplanar (z-fight)";
    case scene_kind::floor:     return "FLOOR (checker to horizon)";
    case scene_kind::model:     return "MODEL (loaded from disk)";
    }
    return "?";
}

[[nodiscard]] scene_kind next_scene(scene_kind k)
{
    switch (k)
    {
    case scene_kind::solids:    return scene_kind::cycle;
    case scene_kind::cycle:     return scene_kind::intersect;
    case scene_kind::intersect: return scene_kind::zfight;
    case scene_kind::zfight:    return scene_kind::floor;
    case scene_kind::floor:     return scene_kind::model;
    case scene_kind::model:     return scene_kind::solids;
    }
    return scene_kind::solids;
}

/// A flat panel through `a` and `b`, `width` across, whose `a` end is pushed AWAY
/// from the camera by `tilt` and whose `b` end is pulled TOWARD it by the same
/// amount, and which extends `overhang` past both ends.
///
/// The construction is worth reading, because it is Lesson 2.5's claim used as a
/// tool rather than admired: **a rotation matrix is its three columns, and its
/// columns are where the basis vectors land.** We want the quad's own x axis to
/// run across the plank, its y axis along it, and its z axis to be the face
/// normal — so we build those three directions and hand them over as columns.
/// No angles are computed anywhere, and no `rotation_x/y/z` is composed.
///
/// The three axes are orthonormal by construction: `axis_x` is perpendicular to
/// the plank's horizontal run and has no z component, so it is perpendicular to
/// `axis_y` whatever the tilt; and `axis_z` is their cross product. Its
/// determinant is +1, verified — a reflection here would flip every triangle's
/// winding and quietly matter in Lesson 3.4.
///
/// The `overhang` lengthens the quad WITHOUT moving its plane, so the depths at
/// `a` and `b` are still exactly -tilt and +tilt. It exists so the planks
/// properly cross at the corners rather than merely touching: end-to-end panels
/// overlap in a sliver, and a sliver is not a demonstration.
[[nodiscard]] engine::transform make_plank(engine::vec2 a, engine::vec2 b,
                                           float width, float tilt, float overhang)
{
    const engine::vec3 end_a{a.x, a.y, -tilt};
    const engine::vec3 end_b{b.x, b.y, +tilt};

    const engine::vec3 along = end_b - end_a;

    const engine::vec3 axis_y = engine::normalised(along);               // model +y
    const engine::vec3 axis_x = engine::normalised(                      // model +x
        engine::vec3{-(b.y - a.y), b.x - a.x, 0.0f});
    const engine::vec3 axis_z = engine::cross(axis_x, axis_y);           // model +z

    engine::transform t;
    t.rotation = engine::mat3{axis_x, axis_y, axis_z};
    t.scale    = {width, engine::length(along) + 2.0f * overhang, 1.0f};
    t.position = (end_a + end_b) * 0.5f;
    return t;
}

// ---------------------------------------------------------------------------
// Lesson 3.2 — a surface that recedes, and something painted on it
// ---------------------------------------------------------------------------

/// The ground plane's extent, in world units. Deliberately lopsided: it starts
/// just in front of the default camera and runs a long way away, so one quad
/// spans a **37:1 range of depth**. That ratio is the whole demo — affine
/// interpolation is exact when w is constant and worst when it is not.
constexpr float k_floor_x = 9.0f;      ///< half-width
constexpr float k_floor_near = 6.0f;   ///< +z edge, about a unit in front of the eye
constexpr float k_floor_far = -30.0f;  ///< -z edge, 37 units away

/// How many world units one checker cell covers. Fixed, so that changing the
/// TESSELLATION changes only the interpolation error and never the pattern —
/// which is what makes the two comparable.
constexpr float k_floor_cell = 3.0f;

/// Storage for the floor. A `mesh` is a pair of non-owning spans (Lesson 2.12),
/// so unlike every mesh so far — which view `inline constexpr` arrays with
/// program lifetime — this one needs somebody to own its arrays. That somebody is
/// the demo, and the awkwardness is the point: it is exactly the pressure that
/// produces Module 5's asset system.
struct floor_geometry
{
    std::vector<engine::vec3> vertices;
    std::vector<engine::vec2> uvs;
    std::vector<std::uint16_t> indices;
    int cells = 0;                       ///< quads per side; 0 = not built yet

    /// Designated initialisers, as of Lesson 3.5: `mesh` grew a fourth member and
    /// this floor carries no normals, so naming the three it does carry is both
    /// clearer and what stops `-Wmissing-field-initializers` complaining.
    ///
    /// This struct is what `engine::mesh_data` generalises — Lesson 3.2 said at the
    /// time that the awkwardness of owning geometry by hand was the pressure that
    /// would eventually produce a real type. It is kept as it is because the floor
    /// is *generated*, not loaded, and rewriting it to use `mesh_data` would gain
    /// nothing but churn in a demo that already works.
    [[nodiscard]] engine::mesh view() const
    {
        return engine::mesh{.vertices = vertices, .indices = indices, .uvs = uvs};
    }
};

/// Tessellate the ground plane into `cells` x `cells` quads.
///
/// The uvs are computed from the **world position**, not from the grid index, so
/// the checker pattern is bit-identical at every tessellation level. Only the
/// number of triangles the interpolation has to span changes — which is what lets
/// the demo answer "how much subdivision would affine interpolation need?" with a
/// number instead of a shrug.
void build_floor(floor_geometry& g, int cells)
{
    if (g.cells == cells) { return; }   // nothing to do; rebuilt only on [T]
    g.cells = cells;
    g.vertices.clear();
    g.uvs.clear();
    g.indices.clear();

    const int n = cells + 1;   // vertices per side
    for (int j = 0; j < n; ++j)
    {
        const float tz = static_cast<float>(j) / static_cast<float>(cells);
        const float z = k_floor_near + tz * (k_floor_far - k_floor_near);
        for (int i = 0; i < n; ++i)
        {
            const float tx = static_cast<float>(i) / static_cast<float>(cells);
            const float x = -k_floor_x + tx * (2.0f * k_floor_x);
            g.vertices.push_back({x, 0.0f, z});
            g.uvs.push_back({x / k_floor_cell, z / k_floor_cell});
        }
    }

    // Two triangles per quad, both wound counter-clockwise seen from ABOVE
    // (+y), which is the side anybody stands on.
    for (int j = 0; j < cells; ++j)
    {
        for (int i = 0; i < cells; ++i)
        {
            const auto v = [&](int ii, int jj) {
                return static_cast<std::uint16_t>(jj * n + ii);
            };
            g.indices.push_back(v(i, j));     g.indices.push_back(v(i, j + 1));
            g.indices.push_back(v(i + 1, j + 1));
            g.indices.push_back(v(i, j));     g.indices.push_back(v(i + 1, j + 1));
            g.indices.push_back(v(i + 1, j));
        }
    }
}

// ---------------------------------------------------------------------------
// Lesson 3.5 — geometry from disk
// ---------------------------------------------------------------------------

/// Which model the [C] = model scene is showing. [L] cycles.
///
/// Four files and one mesh that never touched a disk. The last one is not padding:
/// `generated` is `make_torus()` in memory, and `torus` is that same mesh written
/// out by `save_obj` and read back. Rendering both and counting the pixels that
/// differ is how this lesson checks its own loader — the same trick Lessons 3.1
/// through 3.4 each used, applied now to an asset pipeline instead of an algorithm.
enum class model_choice
{
    torus,       ///< assets/torus.obj — 2,304 triangles, non-convex, a real workout
    cube,        ///< assets/cube.obj — twenty readable lines, and the index problem
    twisted,     ///< assets/twisted.obj — cube.obj with one face wound backwards
    quirks,      ///< assets/quirks.obj — every awkward-but-legal construct at once
    generated    ///< make_torus(), never written or read. The round trip's control.
};

[[nodiscard]] const char* name_of(model_choice c)
{
    switch (c)
    {
    case model_choice::torus:     return "torus.obj";
    case model_choice::cube:      return "cube.obj";
    case model_choice::twisted:   return "twisted.obj";
    case model_choice::quirks:    return "quirks.obj";
    case model_choice::generated: return "make_torus()";
    }
    return "?";
}

[[nodiscard]] const char* file_of(model_choice c)
{
    switch (c)
    {
    case model_choice::torus:     return "torus.obj";
    case model_choice::cube:      return "cube.obj";
    case model_choice::twisted:   return "twisted.obj";
    case model_choice::quirks:    return "quirks.obj";
    case model_choice::generated: break;
    }
    return nullptr;
}

[[nodiscard]] model_choice next_model(model_choice c)
{
    switch (c)
    {
    case model_choice::torus:     return model_choice::cube;
    case model_choice::cube:      return model_choice::twisted;
    case model_choice::twisted:   return model_choice::quirks;
    case model_choice::quirks:    return model_choice::generated;
    case model_choice::generated: return model_choice::torus;
    }
    return model_choice::torus;
}

/// How big to draw each model, so that all five fill the viewport comparably.
///
/// The torus spans 1.4 units, the cube 1, the pyramid 2 — a single scale would make
/// one of them a speck. In an engine this is the ASSET's business, not the
/// renderer's: a model is authored at its real size and placed by a transform.
/// Module 5's asset system computes and stores a bounding box for exactly this, and
/// Module 6's frustum culling needs the same number. Here it is a switch.
[[nodiscard]] float display_scale(model_choice c)
{
    switch (c)
    {
    case model_choice::torus:
    case model_choice::generated: return 1.5f;
    case model_choice::cube:      return 2.4f;
    case model_choice::twisted:   return 2.4f;
    case model_choice::quirks:    return 1.1f;
    }
    return 1.0f;
}

/// The loaded model, plus everything measured about it.
///
/// **`data` owns the arrays and `view()` borrows them**, which is the hazard this
/// type exists to make obvious. Reloading on [L] clears the vectors, so any `mesh`
/// taken before the reload now points at freed memory. The demo is safe because
/// `build_scene` runs after key handling and re-takes the view every frame — but
/// "safe because of the order two things happen in" is exactly the kind of safety
/// Module 5's handles replace with something a compiler can check.
struct model_state
{
    model_choice choice = model_choice::torus;
    engine::mesh_data data;        ///< whatever [L] currently names
    engine::obj_report load;       ///< what the file contained, and what we built
    engine::mesh_report check;     ///< …and whether it is safe to draw
    double load_ms = 0.0;          ///< wall-clock cost of the last load

    /// Whether `flip_uv_v` was applied on the way in — Lesson 3.9. Recorded rather
    /// than inferred, because "are these uvs in OBJ space or texture space?" is
    /// exactly the question a mesh cannot answer by looking at its own numbers, and
    /// every convention bug in a pipeline is somebody assuming the answer.
    bool uv_flipped = true;

    /// `make_torus()`, built once. The control for the round-trip comparison, and
    /// the source `assets/torus.obj` was written from.
    engine::mesh_data generated;
};

/// Load (or regenerate) the current model and measure it.
///
/// Both halves matter. The loader answers "what does the file say"; `validate()`
/// answers "is that safe to draw", and until this lesson nothing in the engine could
/// ask the second question at all. Its answer feeds `scene_object::closed`, so the
/// demo's back-face culling is now gated on a MEASUREMENT rather than on a promise —
/// which is the debt Lesson 3.4 §3.6 left, paid.
///
/// **Lesson 3.9 adds the uv flip**, and adds it here rather than inside the parser:
/// OBJ writes `v` from the bottom up and a texture is addressed from the top down
/// (`mesh.hpp`'s `flip_uv_v` quotes SDL_gpu.h on the point), so somebody has to
/// reconcile them and the import step is the somebody. `apply_uv_flip` is a
/// parameter rather than a constant so the disagreement can be switched back on
/// with [2] and looked at — it is not subtle when you can see it, and it is nearly
/// invisible when you cannot.
void load_model(model_state& m, model_choice c, bool apply_uv_flip)
{
    m.choice = c;
    m.uv_flipped = apply_uv_flip;

    const Uint64 t0 = SDL_GetTicksNS();
    if (c == model_choice::generated)
    {
        m.data = m.generated;   // a copy: four vectors, and it happens on a keypress
        m.load = {};
        m.load.status = engine::obj_status::ok;
        m.load.vertices = static_cast<int>(m.data.vertices.size());
        m.load.triangles = static_cast<int>(m.data.triangle_count());
    }
    else
    {
        // asset_path() puts us next to the executable, wherever it was launched
        // from. std::string, because the path is assembled at runtime and something
        // has to own the characters — the same ownership question as the mesh, one
        // level down.
        const std::string path = engine::asset_path(file_of(c));
        m.load = engine::load_obj(path.c_str(), m.data);
    }
    // OBJ space -> texture space, before anything measures or draws the mesh.
    //
    // Applied to EVERY branch, including `generated`, and that uniformity is what
    // keeps the round-trip comparison honest: `assets/torus.obj` was written from
    // `make_torus()`, so if the file's copy were flipped and the in-memory copy were
    // not, `roundtrip_wrong` would start counting the import step instead of the
    // loader. An import applied to everything cannot break a round trip; an import
    // applied to some things silently can. `m.data` is re-copied from `m.generated`
    // on every load, so nothing accumulates.
    //
    // Inside the timed region on purpose: it is part of what importing an asset
    // costs, and the HUD's load time should not quietly exclude the steps that
    // happen after the parse. (It is 24 subtractions on the cube and 2,352 on the
    // torus, so the answer is "nothing measurable" — which is worth knowing rather
    // than assuming.)
    if (apply_uv_flip) { engine::flip_uv_v(m.data); }

    const Uint64 t1 = SDL_GetTicksNS();
    m.load_ms = static_cast<double>(t1 - t0) / 1.0e6;

    // Validate whatever we got, INCLUDING a failed load — on failure the arrays are
    // empty, and an empty mesh reports zeroes rather than crashing the report.
    m.check = engine::validate(m.data.view());

    if (m.load.ok())
    {
        SDL_Log("Loaded %-24s %5d verts (%+d split), %5d tris, euler %+d, "
                "volume %+.4f, %s%s  [%.2f ms]",
                name_of(c), m.load.vertices, m.load.split_vertices, m.load.triangles,
                m.check.euler, static_cast<double>(m.check.signed_volume),
                m.check.closed() ? "closed" : "OPEN",
                m.check.consistently_wound() ? "" : ", WINDING INCONSISTENT",
                m.load_ms);
    }
    else
    {
        SDL_Log("FAILED to load %s: %s (line %d)", name_of(c),
                engine::name_of(m.load.status), m.load.line);
    }
}

/// Rebuild the scene for the current time, rotation mode and scene kind.
/// Returns how many objects were written.
///
/// Rebuilt from scratch every frame rather than accumulated into, deliberately.
/// Repeatedly multiplying a rotation by a small delta drifts — the matrix stops
/// being a rotation, and the object slowly shears. Deriving the whole transform
/// from one authoritative `t` cannot drift, and it is the pattern the engine keeps
/// (Module 5's transform component stores the *inputs*, never a running matrix).
int build_scene(scene_object (&out)[k_max_objects], scene_kind kind, spin mode, float t,
                const floor_geometry& floor, const model_state& model, float shininess)
{
    const engine::mat3 spinning = build_spin(mode, t);

    // Three hues that are not the axis colours, so nothing in the scene can be
    // mistaken for a coordinate axis (conventions.html §8 reserves red/green/blue
    // for x/y/z, and that rule is worth honouring outside diagrams too).
    constexpr Uint32 k_amber = 0xFFE0A83Cu;
    constexpr Uint32 k_teal = 0xFF3CB8A8u;
    constexpr Uint32 k_violet = 0xFFA070D8u;

    if (kind == scene_kind::floor)
    {
        // One object, no transform at all: the floor is authored in world
        // coordinates, so its model matrix is the identity. That is unusual and
        // worth noticing — every other object in this course is a unit shape
        // placed by a transform, and a ground plane is the one thing it is more
        // honest to build where it lives.
        out[0].xform = engine::transform{};
        out[0].geometry = floor.view();
        out[0].name = "ground plane (checkered)";
        out[0].tint = k_amber;   // unused: the floor is shaded from its uvs
        out[0].closed = false;   // a sheet, not a solid — 3.4 must not cull it
        return 1;
    }

    if (kind == scene_kind::model)
    {
        // ONE OBJECT, AND NOTHING ABOUT IT WAS TYPED HERE. Its vertex count, its
        // triangle count, its winding and whether it is closed at all are properties
        // of a file, discovered at load time. Every other branch of this function
        // builds geometry the course authored; this one places geometry it did not.
        const float s = display_scale(model.choice);
        out[0].xform.scale = {s, s, s};
        out[0].xform.position = {0.0f, 1.0f, 0.0f};
        out[0].xform.rotation = spinning * engine::rotation_x(0.35f);
        out[0].geometry = model.data.view();
        out[0].name = name_of(model.choice);
        out[0].tint = k_amber;
        // The torus is the one mesh in the demo dense enough for a per-vertex
        // highlight to look like a highlight at all — 1,225 vertices against the
        // icosahedron's 12. [L] to the coarser models and watch it fall apart, which
        // is Lesson 3.8's argument arriving as a picture rather than a claim.
        out[0].surface = {{0.85f, 0.85f, 0.85f}, shininess};

        // The 3.4 debt, paid. `closed` used to be a promise typed next to the
        // geometry; here it is the validator's answer. Note that it takes BOTH
        // conditions: twisted.obj is topologically closed and still unsafe to cull,
        // because culling reads winding and its winding disagrees with itself.
        out[0].closed = model.check.closed() && model.check.consistently_wound();
        return 1;
    }

    if (kind == scene_kind::solids)
    {
        // The hero. Tilted by a fixed rotation as well as the animated one so its
        // symmetry is never accidentally axis-aligned.
        out[0].xform.scale    = {0.9f, 0.9f, 0.9f};
        out[0].xform.position = {0.0f, 1.0f, 0.0f};
        out[0].xform.rotation = spinning * engine::rotation_x(0.5f);
        out[0].geometry       = engine::icosahedron_mesh();
        out[0].name           = "icosahedron (uniform, spinning)";
        out[0].tint           = k_amber;
        out[0].closed         = true;
        // A WHITE highlight on an amber body — which is what a plastic looks like,
        // and not a stylistic choice. A dielectric (plastic, paint, glass, skin)
        // mirrors light off its clear surface layer without tinting it, so its
        // highlight is the colour of the LAMP; only the light that gets *inside*
        // picks up the pigment, and that is the diffuse term. Module 6 gives this a
        // name and a number (F0 ~ 0.04 for most dielectrics).
        out[0].surface        = {{0.85f, 0.85f, 0.85f}, shininess};

        out[1].xform.scale    = {1.8f, 0.35f, 0.9f};
        out[1].xform.position = {-1.6f, 0.5f, 0.4f};
        out[1].xform.rotation = spinning;
        out[1].geometry       = engine::cube_mesh();
        out[1].name           = "slab   (non-uniform, spinning)";
        out[1].tint           = k_teal;
        out[1].closed         = true;
        // A TINTED highlight, and the contrast with the icosahedron is the point: a
        // METAL colours what it reflects, because it has no clear layer over a
        // pigmented interior — a metal is reflection all the way down. Gold is
        // yellow in its highlight, copper orange. Compare the two objects under a
        // white light and the difference is unmistakable.
        out[1].surface        = {{0.30f, 0.72f, 0.66f}, shininess};

        out[2].xform.scale    = {1.2f, 0.25f, 1.2f};
        out[2].xform.position = {1.4f, 0.125f, 0.9f};
        out[2].xform.rotation = engine::mat3::identity();
        out[2].geometry       = engine::cube_mesh();
        out[2].name           = "plinth (non-uniform, still)";
        out[2].tint           = k_violet;
        out[2].closed         = true;
        // THE CONTROL. No highlight at all, so [H] and [E] must not change one pixel
        // of it. Two of three objects were controls for the composition order in 2.8
        // and for the normal matrix in 3.6; the habit is worth keeping.
        out[2].surface        = {};
        return 3;
    }

    if (kind == scene_kind::cycle)
    {
        // THE CYCLE. Three planks laid along the sides of an equilateral
        // triangle, each one tilted so that it passes IN FRONT OF the next at the
        // corner they share and BEHIND the previous one at the other end. Woven,
        // exactly like three sticks laid over and under each other.
        //
        //   at corner 2:  A in front of B
        //   at corner 3:  B in front of C
        //   at corner 1:  C in front of A     <- and now it is a loop
        //
        // No ordering of three items can satisfy all three constraints; that is
        // what "cyclic" means. And every plank's centre sits at exactly z = 0, so
        // their average depths are IDENTICAL — the sort key cannot even prefer a
        // wrong answer, it has nothing to compare. Verified in
        // scratch/verify_31.cpp; Lesson 3.1 §1.3.
        constexpr float R = 1.2f;
        constexpr float k_third = 2.0f * 3.14159265358979f / 3.0f;
        const engine::vec2 c1{R * std::cos(1.5707963f), R * std::sin(1.5707963f)};
        const engine::vec2 c2{R * std::cos(1.5707963f + k_third),
                              R * std::sin(1.5707963f + k_third)};
        const engine::vec2 c3{R * std::cos(1.5707963f + 2.0f * k_third),
                              R * std::sin(1.5707963f + 2.0f * k_third)};

        constexpr float k_width = 1.05f;
        constexpr float k_tilt = 0.55f;
        constexpr float k_overhang = 0.5f;

        out[0].xform = make_plank(c1, c2, k_width, k_tilt, k_overhang);
        out[0].geometry = engine::quad_mesh();
        out[0].name = "plank A (C1->C2)";
        out[0].tint = k_amber;
        out[0].closed = false;

        out[1].xform = make_plank(c2, c3, k_width, k_tilt, k_overhang);
        out[1].geometry = engine::quad_mesh();
        out[1].name = "plank B (C2->C3)";
        out[1].tint = k_teal;
        out[1].closed = false;

        out[2].xform = make_plank(c3, c1, k_width, k_tilt, k_overhang);
        out[2].geometry = engine::quad_mesh();
        out[2].name = "plank C (C3->C1)";
        out[2].tint = k_violet;
        out[2].closed = false;

        // Lift the weave so its centre sits exactly on the camera's target. With
        // the elevation at zero that makes all three planks EQUIDISTANT from the
        // eye, so their average view depths are identical to the last bit — a
        // renderer that sorts whole objects has literally nothing to compare.
        for (int i = 0; i < 3; ++i) { out[i].xform.position.y += 0.6f; }
        return 3;
    }

    if (kind == scene_kind::intersect)
    {
        // TWO QUADS PASSING THROUGH EACH OTHER. Their centres are a clear 0.2
        // apart in z, so the sort key gives a confident answer — and the answer is
        // right on one side of the intersection line and wrong on the other.
        //
        // This is the deeper failure. The cycle at least has no correct order; here
        // a correct order exists PER PIXEL and simply cannot be expressed
        // per-triangle. No sorting algorithm, however clever, can fix that: the
        // question "which triangle is in front" has no single answer.
        out[0].xform.scale    = {2.6f, 2.0f, 1.0f};
        out[0].xform.position = {0.0f, 1.1f, +0.1f};
        out[0].xform.rotation = engine::rotation_y(+0.7f);
        out[0].geometry       = engine::quad_mesh();
        out[0].name           = "quad A (nearer centre)";
        out[0].tint           = k_amber;
        out[0].closed         = false;

        out[1].xform.scale    = {2.6f, 2.0f, 1.0f};
        out[1].xform.position = {0.0f, 1.1f, -0.1f};
        out[1].xform.rotation = engine::rotation_y(-0.7f);
        out[1].geometry       = engine::quad_mesh();
        out[1].name           = "quad B (further centre)";
        out[1].tint           = k_teal;
        out[1].closed         = false;
        return 2;
    }

    // NEAR-COPLANAR. The z-buffer's own failure mode, and the reason a depth
    // format is a decision rather than a detail.
    //
    // Two large panels a hair apart, both turned well away from face-on so that
    // depth varies strongly across the screen. B is nearer everywhere, so the
    // correct picture is "B, entirely". Whether you get it depends on whether the
    // buffer can represent a gap this small at this distance — press [B] and
    // watch D16_UNORM fail to. Lesson 3.1 §3.6.
    out[0].xform.scale    = {3.0f, 2.4f, 1.0f};
    out[0].xform.position = {0.0f, 1.1f, 0.0f};
    out[0].xform.rotation = engine::rotation_y(0.9f);
    out[0].geometry       = engine::quad_mesh();
    out[0].name           = "panel A (behind)";
    out[0].tint           = k_amber;
    out[0].closed         = false;

    out[1].xform.scale    = {3.0f, 2.4f, 1.0f};
    out[1].xform.position = {0.0f, 1.1f, 0.001f};   // one millimetre nearer. That is all.
    out[1].xform.rotation = engine::rotation_y(0.9f);
    out[1].geometry       = engine::quad_mesh();
    out[1].name           = "panel B (1 mm in front)";
    out[1].tint           = k_teal;
    out[1].closed         = false;
    return 2;
}

/// Draw the world through the camera: a ground grid on y = 0 and a marked origin.
///
/// This exists so that "world space" is a place you can see rather than a claim,
/// and as of Lesson 2.9 it is drawn THROUGH the view matrix — so the floor tilts
/// and turns as the camera orbits. As of Lesson 2.10 it goes on through `proj`, so
/// the far edge of the floor is smaller than the near edge: the floor's parallel
/// rails now visibly converge, which is perspective made obvious.
void draw_world(engine::framebuffer& fb, const engine::mat4& view, const projector& pr)
{
    constexpr float reach = 2.5f;
    const Uint32 faint = engine::pack_argb(40, 44, 60);
    const Uint32 axis_line = engine::pack_argb(70, 76, 100);

    // Gridlines every half unit, with the two lines through the origin brighter
    // so the world's own axes are readable inside the mesh of the floor.
    for (int i = -5; i <= 5; ++i)
    {
        const float f = static_cast<float>(i) * 0.5f;
        const Uint32 c = (i == 0) ? axis_line : faint;
        line3_world(fb, view, pr, {f, 0.0f, -reach}, {f, 0.0f, reach}, c);
        line3_world(fb, view, pr, {-reach, 0.0f, f}, {reach, 0.0f, f}, c);
    }

    // The world's own axis triad, at the world origin, in the course colours.
    // Every object's position is measured from exactly this point.
    line3_world(fb, view, pr, {0.0f, 0.0f, 0.0f}, {0.9f, 0.0f, 0.0f}, engine::pack_argb(150, 66, 66));
    line3_world(fb, view, pr, {0.0f, 0.0f, 0.0f}, {0.0f, 0.9f, 0.0f}, engine::pack_argb(78, 130, 100));
    line3_world(fb, view, pr, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.9f}, engine::pack_argb(82, 108, 156));
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

/// One triangle, already projected, ready to hand to the rasterizer.
///
/// The three vertices carry pixel coordinates AND device depth, which is all the
/// z-buffer needs. `sort_key` is the extra thing the painter's algorithm needs
/// and the z-buffer does not — keeping it in the struct rather than recomputing
/// it lets the two paths be compared on exactly the same geometry.
struct raster_triangle
{
    engine::vertex v[3];

    /// Average VIEW-space z of the three corners: the painter's algorithm's
    /// entire idea, and its entire problem. View z is negative in front of the
    /// camera, so a MORE negative key is further away and sorts first.
    ///
    /// Note what this number is not: it is not "the depth of the triangle",
    /// because a triangle stretched in depth does not have one. §1.2.
    float sort_key = 0.0f;

    /// What the **wrong** facing test says about this triangle: the sign of
    /// `dot(face_normal, camera_forward)`, computed in VIEW space (Lesson 3.4 §3.4).
    ///
    /// Kept alongside the geometry purely so the two tests can be compared on
    /// identical triangles, every frame, and the disagreement counted. The right
    /// test needs no storage at all — it is the sign of an area the rasterizer
    /// already computes, which is most of the argument for using it.
    bool front_by_forward = false;

    /// The surface parameters this triangle should be shaded with — Lesson 3.8.
    ///
    /// **This field is a cheat, and the cheat is instructive.** Under per-pixel
    /// shading the material is read inside the fragment loop, so it has to be
    /// available per triangle; a GPU cannot do that, because a material is
    /// pipeline state and changing it means ending the draw call and starting
    /// another. Real renderers therefore **batch by material**, and a scene with
    /// three materials is three draws.
    ///
    /// A software rasterizer can just carry the pointer along, and this one does,
    /// because splitting the demo's single pass into per-object draws would break
    /// the painter's-algorithm comparison that has run since 3.1 (that one needs a
    /// sort ACROSS objects). It is named here rather than hidden: the moment this
    /// engine reaches Module 4, this field stops being possible. That is the same
    /// pressure 3.4 found with cull modes and 3.7 found with `specular`, arriving
    /// for the third time and from a new direction.
    engine::specular surface{};
};

/// Per-face brightness, so adjacent faces of a solid can be told apart.
///
/// **This is a debug palette, not lighting.** There is no light in the scene, no
/// normal is consulted, and the number depends only on which triangle this
/// happens to be. Lesson 3.6 replaces it with a Lambert term computed from a real
/// normal and a real light direction — and keeps this behind `[G]`, because the two
/// pictures side by side are the fastest way to see what "the shading does not
/// change when the object turns" actually means. Eighth keep-the-wrong-thing
/// bargain in this engine, and the first where the wrong thing was ever the default.
///
/// The scaling happens in LINEAR LIGHT, decoding and re-encoding around it,
/// because "70% as bright" is a statement about light and multiplying a stored
/// sRGB value by 0.7 does not produce 70% of the light (Lesson 1.6).
[[nodiscard]] Uint32 face_shade(Uint32 base, std::size_t face)
{
    constexpr float k_steps[5] = {1.00f, 0.84f, 0.70f, 0.57f, 0.45f};
    const float k = k_steps[face % 5];
    const engine::linear_rgb light = engine::to_linear(base);
    return engine::to_encoded({light.r * k, light.g * k, light.b * k});
}

// ---------------------------------------------------------------------------
// Lesson 3.6 — normals, and light that is actually light
// ---------------------------------------------------------------------------

// LESSON 3.8 SPLIT THE OLD `shade_mode` IN TWO, and that split is the lesson.
//
// 3.6 shipped one enum with `palette / flat / smooth`, and its own doc comment
// admitted the problem: "interpolating the COLOUR across a triangle and
// interpolating the NORMAL and shading each pixel are different things". They are
// different things because they are answers to two INDEPENDENT questions:
//
//   WHERE DOES THE NORMAL COME FROM?   a property of the MESH and how it was split
//   WHERE IS THE EQUATION EVALUATED?   a property of the PIPELINE
//
// One enum could not hold both, because the combinations are a grid rather than a
// list. Two enums can, and the grid has cells worth naming (see `is_degenerate`).

/// Where the surface normal comes from. [Q] cycles.
enum class normal_source
{
    /// One normal per TRIANGLE, from the cross product of its own two edges. Every
    /// point on the face gets the same normal, so the face is flat by construction
    /// whatever the evaluation does with it.
    face,

    /// One normal per VERTEX, as the mesh authored it. Shared vertices carry an
    /// averaged normal, which is what makes a tessellated surface read as curved —
    /// and is also why a mesh with SPLIT vertices (a cube's 24) stays faceted no
    /// matter which evaluation is chosen. Lesson 3.6 measured that: 0 pixels differ
    /// between the two sources on `cube.obj`.
    vertex
};

/// Where the shading equation is evaluated. [G] cycles.
enum class shade_eval
{
    /// Lesson 3.1's debug ramp: no light, no normal, indexed by triangle number.
    /// Not an evaluation point at all — kept in the same cycle because it is the
    /// "off" position, and because a fake next to three real ones is instructive.
    palette,

    /// ONCE PER TRIANGLE, at the centroid. All three corners get that one answer,
    /// so the fill interpolates between three equal values and the face comes out
    /// uniform.
    flat,

    /// ONCE PER VERTEX, and the fill interpolates the ANSWERS. This is **Gouraud
    /// shading** (1971) — and note that this engine has been doing it since 3.6
    /// without the name. 2.4 built the interpolation; 3.6 fed it lit colours; that
    /// combination is Gouraud, complete.
    gouraud,

    /// ONCE PER PIXEL, with the fill interpolating the NORMAL and the POSITION and
    /// evaluating the equation itself. This is **Phong shading** — the
    /// interpolation scheme, which is a different thing from the Phong reflection
    /// model of 3.7 by the same author. We pair Phong *shading* with the
    /// Blinn-Phong *reflection model*, which is the usual modern combination and
    /// sounds like a contradiction until the two axes are separated.
    per_pixel
};

[[nodiscard]] const char* name_of(normal_source n)
{
    switch (n)
    {
    case normal_source::face:   return "FACE   (cross product)";
    case normal_source::vertex: return "VERTEX (as authored)";
    }
    return "?";
}

[[nodiscard]] const char* name_of(shade_eval e)
{
    switch (e)
    {
    case shade_eval::palette:   return "debug palette (3.1)";
    case shade_eval::flat:      return "FLAT      per triangle";
    case shade_eval::gouraud:   return "GOURAUD   per vertex";
    case shade_eval::per_pixel: return "PER-PIXEL per fragment";
    }
    return "?";
}

[[nodiscard]] normal_source next_normal_source(normal_source n)
{
    return (n == normal_source::face) ? normal_source::vertex : normal_source::face;
}

[[nodiscard]] shade_eval next_eval(shade_eval e)
{
    switch (e)
    {
    case shade_eval::palette:   return shade_eval::flat;
    case shade_eval::flat:      return shade_eval::gouraud;
    case shade_eval::gouraud:   return shade_eval::per_pixel;
    case shade_eval::per_pixel: return shade_eval::palette;
    }
    return shade_eval::palette;
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
[[nodiscard]] bool is_degenerate(normal_source n, shade_eval e, engine::specular_model m)
{
    if (n != normal_source::face) { return false; }
    if (e == shade_eval::flat) { return false; }   // flat IS the thing compared against
    return m == engine::specular_model::none;
}

// ---------------------------------------------------------------------------
// Lesson 3.9 — where a fragment's colour comes from
// ---------------------------------------------------------------------------

/// Which image the surface reads its albedo from. [M] cycles.
///
/// The first entry is not an image at all, and keeping it first is the argument:
/// a procedural rule and a texture lookup answer the same question, and putting
/// them one keypress apart is the cheapest way to see what the array buys (art you
/// could not have written a formula for) and what it costs (a finite grid, which
/// runs out under magnification and aliases under minification).
enum class albedo_source
{
    /// `shading::uv_checker` — Lesson 3.2's formula. No memory, no sampler, and
    /// **exact at every magnification**, because there is no grid to run out of.
    rule,

    /// A 64x64 checkerboard of 8x8 squares: the same pattern as `rule`, now as
    /// texels. Chosen so the two can be compared directly — every visible
    /// difference between them is the sampler's doing and nothing else's.
    checker,

    /// The orientation chart. Four coloured quadrants and a white top-left mark,
    /// so "which way is v" is a thing you read rather than reason about.
    uv_grid,

    /// A 64x64 checkerboard of 32x32 squares — two texels per square. Deliberately
    /// finer than the screen can resolve at any distance, so the floor running to
    /// the horizon sparkles. That sparkle is §7's subject and mipmaps are Module 6.
    fine
};

[[nodiscard]] const char* name_of(albedo_source a)
{
    switch (a)
    {
    case albedo_source::rule:    return "RULE (procedural, 3.2)";
    case albedo_source::checker: return "checker 64px/8 cells";
    case albedo_source::uv_grid: return "uv grid (orientation)";
    case albedo_source::fine:    return "fine 64px/32 cells";
    }
    return "?";
}

[[nodiscard]] albedo_source next_albedo(albedo_source a)
{
    switch (a)
    {
    case albedo_source::rule:    return albedo_source::checker;
    case albedo_source::checker: return albedo_source::uv_grid;
    case albedo_source::uv_grid: return albedo_source::fine;
    case albedo_source::fine:    return albedo_source::rule;
    }
    return albedo_source::rule;
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

/// The three images, built once at startup.
///
/// Owned by the demo, like `floor_geometry` and for the same reason and with the
/// same complaint: there is still nothing in this engine whose job is to own
/// assets. Two lessons running have now noted it. Module 5's asset system is the
/// answer, and the pressure is being allowed to accumulate rather than relieved
/// early with a guess.
struct texture_set
{
    engine::texture checker;
    engine::texture uv_grid;
    engine::texture fine;

    void build()
    {
        // 64x64: big enough that magnification is visible on a 320x180 framebuffer
        // and small enough that a whole image fits comfortably in L1, which is what
        // keeps the per-pixel fetch honest rather than a cache-miss benchmark.
        checker = engine::make_checker(64, 8, 0xFFE8E2D6u, 0xFF3A4058u);
        uv_grid = engine::make_uv_grid(64);
        fine = engine::make_checker(64, 32, 0xFFE8E2D6u, 0xFF3A4058u);
    }

    /// The image for a choice, or `nullptr` for `rule` — which is not an error, it
    /// is the mode that has no image by definition.
    [[nodiscard]] const engine::texture* pick(albedo_source a) const
    {
        switch (a)
        {
        case albedo_source::rule:    return nullptr;
        case albedo_source::checker: return &checker;
        case albedo_source::uv_grid: return &uv_grid;
        case albedo_source::fine:    return &fine;
        }
        return nullptr;
    }
};

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
    }
    return "?";
}

[[nodiscard]] engine::specular_model next_specular(engine::specular_model m)
{
    switch (m)
    {
    case engine::specular_model::none:  return engine::specular_model::phong;
    case engine::specular_model::phong: return engine::specular_model::blinn;
    case engine::specular_model::blinn: return engine::specular_model::none;
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
[[nodiscard]] float matched_shininess(engine::specular_model from, float shininess)
{
    if (from == engine::specular_model::phong) { return shininess * 4.0f; }   // -> blinn
    if (from == engine::specular_model::blinn) { return shininess * 0.25f; }  // -> phong
    return shininess;
}

/// What this frame's normals did, for the HUD.
struct normal_stats
{
    int shaded = 0;         ///< vertices given a Lambert term
    int fell_back = 0;      ///< …of which had no authored normal, so used the face's
    float max_tilt = 0.0f;  ///< worst angle, in degrees, between the correct normal
                            ///< transform and the naive one. Zero unless something
                            ///< in the scene is non-uniformly scaled.
};

/// The angle between two directions, in degrees. Used only for the HUD's honesty
/// check, so the acos guard matters more than the speed.
[[nodiscard]] float angle_between_deg(engine::vec3 a, engine::vec3 b)
{
    const engine::vec3 ua = engine::normalised_or(a, {});
    const engine::vec3 ub = engine::normalised_or(b, {});
    const float c = std::clamp(engine::dot(ua, ub), -1.0f, 1.0f);
    return std::acos(c) * 180.0f / 3.14159265358979f;
}

// ---------------------------------------------------------------------------
// Lesson 3.4 — back-face culling
// ---------------------------------------------------------------------------

/// What the demo does about faces pointing away from the camera. [U] cycles.
///
/// Three of these are `engine::cull_mode` and one is not: `back_by_forward` culls
/// with the *wrong test* — the one almost everybody writes first — so it can be
/// watched failing rather than described. Seventh time this bargain has been made
/// in the engine, and the pattern is now the house style: keep the wrong thing,
/// behind a key.
enum class cull_choice
{
    none,              ///< draw everything (engine::cull_mode::none)
    back,              ///< the right test, the useful direction (cull_mode::back)
    front,             ///< the right test, inverted — see inside things (cull_mode::front)
    back_by_forward    ///< THE CLASSIC BUG: dot(normal, camera_forward). Demo-only.
};

[[nodiscard]] const char* name_of(cull_choice c)
{
    switch (c)
    {
    case cull_choice::none:            return "NONE (draw all)";
    case cull_choice::back:            return "BACK (correct)";
    case cull_choice::front:           return "FRONT (inverted)";
    case cull_choice::back_by_forward: return "BACK by dot(n,fwd)";
    }
    return "?";
}

[[nodiscard]] cull_choice next_cull(cull_choice c)
{
    switch (c)
    {
    case cull_choice::none:            return cull_choice::back;
    case cull_choice::back:            return cull_choice::front;
    case cull_choice::front:           return cull_choice::back_by_forward;
    case cull_choice::back_by_forward: return cull_choice::none;
    }
    return cull_choice::none;
}

/// The `engine::cull_mode` this choice asks the rasterizer for.
///
/// `back_by_forward` maps to `none`, because that mode does its (wrong) culling in
/// the demo, in view space, before the triangles ever reach the rasterizer. That is
/// exactly where the bug lives in the codebases that have it.
[[nodiscard]] engine::cull_mode to_engine_cull(cull_choice c)
{
    switch (c)
    {
    case cull_choice::back:  return engine::cull_mode::back;
    case cull_choice::front: return engine::cull_mode::front;
    case cull_choice::none:
    case cull_choice::back_by_forward: break;
    }
    return engine::cull_mode::none;
}

/// What culling did to this frame, for the HUD.
struct cull_stats
{
    int submitted = 0;   ///< triangles handed to the rasterizer
    int front = 0;       ///< …of which front-facing, by the SCREEN-SPACE test
    int drawn = 0;       ///< …and how many survived the current cull mode
    int disagree = 0;    ///< triangles the two facing tests classify differently
};

/// Reusable working storage for `collect_triangles`.
///
/// The per-vertex arrays used to be fixed 64-element stacks, which was fine while
/// the largest mesh was a twelve-vertex icosahedron and silently wrong the moment
/// Lesson 3.2's tessellated floor arrived with 289. Vectors remove the cap; owning
/// them **across frames** rather than declaring them inside the function is what
/// removes the per-frame allocation, since `clear()` keeps the capacity.
///
/// This is the smallest possible taste of Module 8's allocators: the fix for
/// allocation in a hot loop is almost never a faster allocator, it is not
/// allocating.
struct projection_scratch
{
    std::vector<engine::vec3> view_pos;

    /// The per-vertex CLIP positions. This used to be `std::vector<screen_point>`
    /// — the vertices were carried straight through to pixels here. Lesson 3.3
    /// stops one step earlier, because the divide is the thing clipping has to
    /// happen before, and a vertex that has already been divided has thrown away
    /// the only information the clipper could have used.
    std::vector<engine::vec4> clip_pos;

    /// Lesson 3.6. The per-vertex WORLD-space normals, and the colour each vertex
    /// ends up with once the light has been applied to it.
    ///
    /// Both are computed ONCE PER VERTEX and then read by every triangle that uses
    /// that vertex — which is the same argument indexed geometry made in Lesson 2.12
    /// and the reason per-vertex lighting is cheap. On the icosahedron it is 12
    /// shading calls instead of 60.
    ///
    /// Lighting in WORLD space, not view space, and that is a real choice. A light's
    /// direction is authored in world space, so shading there means the light does
    /// not have to be re-derived every time the camera moves. It also makes the HUD
    /// honest: orbit the camera and the shading must NOT change, because Lambert
    /// does not depend on where you are standing. (Specular does — Lesson 3.7.)
    std::vector<engine::vec3> world_normal;
    std::vector<Uint32> vertex_colour;

    /// Lesson 3.7. The per-vertex WORLD positions.
    ///
    /// **Lambert never needed these, and specular does.** A directional light *is* a
    /// direction, and Lambert asks only for the angle between two directions — so
    /// through 3.6 this function could compose `view_from_world * world_from_model`
    /// once and send each vertex straight into view space, never forming a world
    /// position at all. A highlight has to know where the *eye* is relative to *this
    /// point*, which is a question about places rather than directions, so the world
    /// position now has to exist.
    ///
    /// It costs a second matrix multiply per vertex — model to world, then world to
    /// view, rather than one combined hop. That is the honest price of view
    /// dependence, and it is worth noticing that there is a price at all. (The usual
    /// dodge is to transform the *eye* into model space once per object and shade
    /// there, trading one matrix inverse per object against one multiply per vertex;
    /// Exercise 3.7.4 works it out.)
    std::vector<engine::vec3> world_pos;
};

/// What the near plane did to this frame's geometry. Purely for the HUD.
///
/// Reported as properties of the GEOMETRY rather than of the mode, so the same
/// three numbers mean the same thing whichever setting [K] is on: `straddling` is
/// how many triangles genuinely cross the plane, whether or not the current mode
/// does anything sensible about them. That is what makes the modes comparable —
/// only `output` changes.
struct clip_stats
{
    int input = 0;        ///< triangles the scene offered
    int in_front = 0;     ///< wholly in front of the near plane — the common case
    int straddling = 0;   ///< crossing it: the ones that need cutting
    int behind = 0;       ///< wholly behind it: nothing to draw either way
    int output = 0;       ///< triangles actually handed to the rasterizer
};

/// Project every triangle of every object into screen space.
///
/// This is the same per-vertex transform `draw_mesh` does, with the projection
/// carried all the way through to pixels and depth instead of stopping at view
/// space. Doing it for the whole scene at once, into one flat list, is what makes
/// the two hidden-surface strategies comparable: the painter's algorithm needs to
/// sort ACROSS objects (sorting each object's triangles separately would be
/// wrong the moment two objects overlap), and the z-buffer needs nothing at all.
/// As of Lesson 3.3 the per-vertex work stops at CLIP space, and the divide has
/// moved *inside* the per-triangle loop, after the clipper. That reordering is the
/// entire structural change of this lesson, and it costs something worth naming:
/// the divide is now done per triangle CORNER rather than per vertex, so a shared
/// vertex is divided once for each triangle that uses it. On the icosahedron that
/// is 60 divides instead of 12.
///
/// It is paid knowingly, because the alternative is a two-pass scheme (divide the
/// unclipped vertices, then re-divide only the clipped ones) that is more code, more
/// state, and wrong in a way that is easy to miss. Real engines pay it too: the
/// hardware runs the vertex shader, clips, and *then* divides, which is precisely
/// this order. Module 4 hands the whole problem to the GPU.
void collect_triangles(std::vector<raster_triangle>& out, projection_scratch& scratch,
                       const scene_object* objects, int count,
                       const engine::mat4& view_from_world,
                       const projector& pr, trs_order order, clip_stats& stats,
                       cull_choice cull, normal_source nsrc, shade_eval eval,
                       const engine::lighting& lights,
                       bool correct_normals, normal_stats* normals_out,
                       engine::vec3 eye_world, engine::specular_model spec_model)
{
    out.clear();
    stats = {};
    normal_stats nstats;

    for (int i = 0; i < count; ++i)
    {
        // Only the model matrix now. `view_from_model = view_from_world *
        // world_from_model` used to be composed right here, and Lesson 3.7 deleted
        // it: the shading needs each vertex's WORLD position, so the two hops have
        // to be taken separately and there is nothing left for the composed matrix
        // to do. The composition was a real optimisation; view-dependent shading is
        // what bought it out.
        const engine::mat4 world_from_model = model_matrix(objects[i].xform, order);

        // ---- Lesson 3.6: the matrix that transforms NORMALS ----------------
        //
        // Not the model matrix. A normal is defined by being perpendicular to the
        // surface, and only the inverse transpose preserves that under a
        // non-uniform scale (mat4.hpp derives it). `[J]` switches to the naive
        // version so the failure can be watched rather than described — and note
        // that on the icosahedron, which is uniformly scaled, the two are
        // indistinguishable. That is the whole reason this bug survives in
        // codebases: two thirds of a typical scene looks perfect.
        const engine::mat3 to_world_normal = correct_normals
            ? engine::normal_matrix(world_from_model)
            : engine::linear_of(world_from_model);
        const engine::mat3 reference_normal = engine::normal_matrix(world_from_model);

        // Transform each vertex ONCE. The icosahedron's twelve vertices are
        // shared by twenty triangles, so this is 12 matrix multiplies instead of
        // 60 — the practical argument for indexed geometry, from Lesson 2.12.
        const std::size_t vertex_count = objects[i].geometry.vertices.size();
        std::vector<engine::vec3>& view_pos = scratch.view_pos;
        std::vector<engine::vec4>& clip_pos = scratch.clip_pos;
        std::vector<engine::vec3>& world_normal = scratch.world_normal;
        std::vector<Uint32>& vertex_colour = scratch.vertex_colour;
        std::vector<engine::vec3>& world_pos = scratch.world_pos;
        view_pos.clear();
        clip_pos.clear();
        world_normal.clear();
        vertex_colour.clear();
        world_pos.clear();
        view_pos.reserve(vertex_count);
        clip_pos.reserve(vertex_count);
        world_normal.reserve(vertex_count);
        vertex_colour.reserve(vertex_count);
        world_pos.reserve(vertex_count);

        // Shade at the vertices only when the evaluation point IS the vertex.
        // Under `flat` the answer is computed once per triangle below, and under
        // `per_pixel` the fill computes it — in which case the vertex colour must
        // stay the raw albedo, because that is what the fragment multiplies.
        const bool per_vertex_light = (eval == shade_eval::gouraud);

        for (std::size_t v = 0; v < vertex_count; ++v)
        {
            // Lesson 3.7 split this hop in two. It used to be one multiply by the
            // composed `view_from_model`; a highlight needs the world POSITION, so
            // the composition has to be taken apart. Same destination, one more
            // matrix multiply per vertex, and the extra multiply is what view
            // dependence costs.
            world_pos.push_back(engine::xyz(world_from_model
                                          * engine::point(objects[i].geometry.vertices[v])));
            view_pos.push_back(engine::xyz(view_from_world * engine::point(world_pos.back())));
            clip_pos.push_back(to_clip(view_pos.back(), pr.proj));

            // The authored normal, carried into world space. `normal_at` returns
            // the zero vector when the mesh has none, and zero survives the matrix
            // as zero — so the per-triangle loop can detect it and fall back to the
            // face normal without a second flag travelling alongside.
            const engine::vec3 n_model = objects[i].geometry.normal_at(v);
            world_normal.push_back(to_world_normal * n_model);

            if (per_vertex_light && nsrc == normal_source::vertex)
            {
                vertex_colour.push_back(
                    engine::shade_encoded(objects[i].tint, world_normal.back(),
                                          eye_world - world_pos.back(), lights,
                                          objects[i].surface, spec_model));
            }
            else
            {
                vertex_colour.push_back(objects[i].tint);
            }

            // How far the naive transform would have tilted this normal. Measured
            // against the correct one every frame, whichever is in use, so the
            // number means the same thing in both modes — the same discipline
            // 3.3's clip_stats and 3.4's cull_stats follow.
            if (n_model != engine::vec3{})
            {
                ++nstats.shaded;
                const float tilt = angle_between_deg(reference_normal * n_model,
                                                     engine::linear_of(world_from_model) * n_model);
                nstats.max_tilt = std::max(nstats.max_tilt, tilt);
            }
        }

        // CLIP space -> pixels, for one already-clipped corner. Everything the
        // rasterizer needs, and nothing it does not: `engine::vertex` is a
        // screen-space type and this is the one conversion into it.
        const auto to_vertex = [&](const engine::clip_vertex& cv) {
            const screen_point s = screen_from_clip(cv.position, pr.vp);
            // Lesson 3.8: the two varyings ride through unchanged. They are in
            // WORLD space and the divide does not touch them — only the position
            // is projected. A varying is data the fragment wants; the pipeline
            // carries it and does not interpret it.
            return engine::vertex{to_pixel(s.xy.x), to_pixel(s.xy.y),
                                  s.depth, s.inv_w, cv.uv.x, cv.uv.y, cv.colour,
                                  cv.normal, cv.world};
        };

        const std::span<const std::uint16_t> idx = objects[i].geometry.indices;
        for (std::size_t f = 0; f * 3 + 2 < idx.size(); ++f)
        {
            const std::size_t a = idx[f * 3 + 0];
            const std::size_t b = idx[f * 3 + 1];
            const std::size_t c = idx[f * 3 + 2];
            if (a >= vertex_count || b >= vertex_count || c >= vertex_count) { continue; }

            ++stats.input;

            // ---- Lesson 3.6: what colour is this triangle's light? ---------
            //
            // The FACE normal, from this triangle's own edges, in MODEL space —
            // and then through the same normal matrix as everything else, because
            // a face normal is a normal and obeys the same rule. Our winding is
            // counter-clockwise seen from outside (conventions §7), so
            // cross(b - a, c - a) points OUT, which is what a surface normal means.
            //
            // Computed for every triangle because both remaining shading modes can
            // need it: `flat` always, and `smooth` for any corner whose mesh gave
            // it no normal — the fallback that lets an unauthored mesh light
            // correctly instead of turning black.
            const std::span<const engine::vec3> mv = objects[i].geometry.vertices;
            const engine::vec3 face_model = engine::cross(mv[b] - mv[a], mv[c] - mv[a]);
            const engine::vec3 face_world = to_world_normal * face_model;

            // ---- Lesson 3.8: the normal each corner will be shaded with ----
            //
            // ONE axis of the grid, resolved here. `face` gives all three corners
            // the triangle's own normal; `vertex` gives each its own, falling back
            // to the face normal for a mesh that authored none (3.6's rule, and
            // the fallback is counted so the HUD can say it happened).
            const auto normal_for = [&](std::size_t vi) -> engine::vec3 {
                if (nsrc == normal_source::face) { return face_world; }
                if (world_normal[vi] != engine::vec3{}) { return world_normal[vi]; }
                ++nstats.fell_back;
                return face_world;
            };
            const engine::vec3 na = normal_for(a);
            const engine::vec3 nb = normal_for(b);
            const engine::vec3 nc = normal_for(c);

            Uint32 colour_a = 0;
            Uint32 colour_b = 0;
            Uint32 colour_c = 0;
            switch (eval)
            {
            case shade_eval::palette:
                // Lesson 3.1's ramp: no normal, no light, indexed by triangle
                // number. Kept so the comparison is one keypress away.
                colour_a = colour_b = colour_c = face_shade(objects[i].tint, f);
                break;

            case shade_eval::flat:
            {
                // ONE evaluation for the whole triangle, so all three corners get
                // the same colour and the fill interpolates between three equal
                // values — a flat face, at no extra cost.
                //
                // WHERE that one sample is taken has to be said out loud, because
                // both inputs vary across a face. The centroid, for both: it is the
                // only point that privileges no corner. With a face normal that is
                // the face normal; with vertex normals it is their average, which
                // is the normal the fill would have interpolated at the centre.
                const engine::vec3 centroid =
                    (world_pos[a] + world_pos[b] + world_pos[c]) / 3.0f;
                const engine::vec3 n_mid = (nsrc == normal_source::face)
                                         ? face_world
                                         : na + nb + nc;
                colour_a = colour_b = colour_c =
                    engine::shade_encoded(objects[i].tint, n_mid, eye_world - centroid,
                                          lights, objects[i].surface, spec_model);
                break;
            }

            case shade_eval::gouraud:
            {
                // ONE evaluation per corner, and the fill interpolates the ANSWERS.
                // With vertex normals the per-vertex loop above already did the
                // work — `vertex_colour[vi]` is that cached answer, computed once
                // per vertex rather than once per corner-of-a-triangle, which is
                // the saving indexed geometry bought in 2.12.
                const auto pick = [&](std::size_t vi, engine::vec3 n) -> Uint32 {
                    if (nsrc == normal_source::vertex
                        && world_normal[vi] != engine::vec3{})
                    {
                        return vertex_colour[vi];
                    }
                    return engine::shade_encoded(objects[i].tint, n,
                                                 eye_world - world_pos[vi], lights,
                                                 objects[i].surface, spec_model);
                };
                colour_a = pick(a, na);
                colour_b = pick(b, nb);
                colour_c = pick(c, nc);
                break;
            }

            case shade_eval::per_pixel:
                // NO evaluation here at all. The corner colour stays the raw
                // albedo, and the fill does the shading once per fragment from the
                // interpolated normal and position. This is the only branch that
                // sends the equation's INPUTS down the pipeline instead of its
                // output — which is the whole of Lesson 3.8 in one case label.
                colour_a = colour_b = colour_c = objects[i].tint;
                break;
            }

            // Lesson 3.8 adds the two varyings. They are attached HERE, before
            // clipping, so a triangle cut by the near plane gets a correctly
            // interpolated normal and position at its new corners — the clipper
            // lerps every field with the one crossing parameter (3.3 §3.4).
            const engine::clip_vertex src[3] = {
                {clip_pos[a], objects[i].geometry.uv_at(a), colour_a, na, world_pos[a]},
                {clip_pos[b], objects[i].geometry.uv_at(b), colour_b, nb, world_pos[b]},
                {clip_pos[c], objects[i].geometry.uv_at(c), colour_c, nc, world_pos[c]}};

            // How the triangle sits relative to the near plane — measured from the
            // geometry, not inferred from what the current mode does about it, so
            // the HUD's numbers mean the same thing in all three modes.
            int outside = 0;
            for (const engine::clip_vertex& v : src)
            {
                if (engine::near_distance(v.position) < 0.0f) { ++outside; }
            }
            if (outside == 0)      { ++stats.in_front; }
            else if (outside == 3) { ++stats.behind; }
            else                   { ++stats.straddling; }

            // The painter's sort key (Lesson 3.1) belongs to the SOURCE triangle,
            // so it is computed before clipping and shared by every piece the
            // clipper produces. Recomputing it per piece would let one half of a
            // wall sort in front of the other half — the pieces are the same
            // surface, and a sort must not be able to tell them apart.
            const float key = (view_pos[a].z + view_pos[b].z + view_pos[c].z) / 3.0f;

            // ---- The WRONG facing test (Lesson 3.4 §3.4) --------------------
            //
            // The face normal in VIEW space, from the cross product of two edges
            // (Lesson 1.7's right-hand rule, in 3-D). Our meshes are wound
            // counter-clockwise seen from outside, so for a triangle facing the
            // camera this points back toward the eye — which in view space, where
            // the camera sits at the origin looking down -z, means a POSITIVE z.
            //
            // The camera's forward axis is (0, 0, -1), so
            // `dot(normal, forward) = -normal.z`, and "facing me" comes out as
            // `normal.z > 0`. That is the test almost everybody writes first. It is
            // wrong under perspective, and §3.4 shows exactly where: it asks
            // whether the face points against the camera's AXIS, when the question
            // is whether it points against the RAY FROM THE EYE TO IT. Those differ
            // by more the further off-axis the triangle is.
            const engine::vec3 face_normal =
                engine::cross(view_pos[b] - view_pos[a], view_pos[c] - view_pos[a]);
            const bool front_by_forward = (face_normal.z > 0.0f);

            // ...and in this mode, act on it. Note WHERE this happens: in view
            // space, before the projection, which is precisely how the bug gets
            // into a codebase — it looks like a sensible early-out.
            if (cull == cull_choice::back_by_forward && !front_by_forward) { continue; }

            engine::clip_vertex poly[engine::k_clip_max_vertices];
            std::size_t n = 0;

            switch (pr.near)
            {
            case near_mode::clip:
                // One call, and the output is a polygon of 0, 3 or 4 vertices.
                n = engine::clip_polygon_near(src, poly);
                break;

            case near_mode::drop:
                // Lesson 3.2's rule: one bad corner sinks the whole triangle.
                if (outside == 0) { poly[0] = src[0]; poly[1] = src[1]; poly[2] = src[2]; n = 3; }
                break;

            case near_mode::none:
                // Straight through to the divide, whatever `w` turns out to be.
                poly[0] = src[0]; poly[1] = src[1]; poly[2] = src[2]; n = 3;
                break;
            }

            // FAN the clipped polygon: (0, k-1, k) for k = 2 … n-1. Three vertices
            // give one triangle and four give two, which is where "a clipper
            // returns a variable number of triangles" stops being a design note and
            // becomes a loop. The fan is valid because Sutherland–Hodgman preserves
            // both convexity and winding — so every piece is wound the way the
            // original was, which Lesson 3.4's back-face test will depend on.
            for (std::size_t k = 2; k < n; ++k)
            {
                raster_triangle tri;
                tri.v[0] = to_vertex(poly[0]);
                tri.v[1] = to_vertex(poly[k - 1]);
                tri.v[2] = to_vertex(poly[k]);
                tri.sort_key = key;
                // The material travels with the geometry, because per-pixel
                // shading reads it inside the fill. See `raster_triangle::surface`
                // for why that is a cheat a GPU could not make.
                tri.surface = objects[i].surface;
                // Every piece of a clipped triangle lies in the SAME plane, so they
                // share one face normal and one answer from the wrong test.
                tri.front_by_forward = front_by_forward;
                out.push_back(tri);
                ++stats.output;
            }
        }
    }

    if (normals_out != nullptr) { *normals_out = nstats; }
}

/// Draw a collected list of triangles.
///
/// One function, two algorithms, and the difference between them is two lines —
/// which is exactly the point worth taking away. The painter's algorithm needs a
/// sort of the whole scene, O(n log n) and growing, and it is still wrong; the
/// z-buffer needs no sort, no ordering, and no knowledge of the other triangles
/// at all, and it is right.
///
/// @param depth  the depth attachment, or nullptr for the painter's algorithm.
/// @param sorted true to sort back-to-front before drawing.
/// @param style  one style for the whole batch, which is how hardware works: you
///               bind a pipeline, draw everything that uses it, then bind
///               another. A scene with two materials is two batches, and sorting
///               draws by pipeline is a real optimisation in Module 6.
/// Sort the painter's algorithm's triangles, furthest first.
///
/// Extracted from `draw_triangles` in Lesson 3.10 so that the demo can run it
/// under `zone::sort` and `draw_triangles` can still do it for callers that do
/// not care. **One rule, one place, two callers** — the same discipline
/// `is_front_facing` established in 3.4: instrumentation may duplicate the
/// question, never the answer.
void sort_back_to_front(std::vector<raster_triangle>& tris)
{
    // Furthest first. View-space z is NEGATIVE in front of the camera, so
    // "furthest" is "most negative" and plain ascending order is what we want.
    // Getting this backwards paints the scene inside out, which at least fails
    // loudly — unlike everything else about this algorithm.
    std::sort(tris.begin(), tris.end(),
              [](const raster_triangle& a, const raster_triangle& b)
              { return a.sort_key < b.sort_key; });
}

void draw_triangles(engine::framebuffer& fb, engine::depth_buffer* depth,
                    std::vector<raster_triangle>& tris, bool sorted,
                    engine::fill_style style, cull_stats* culled = nullptr,
                    engine::quad_stats* quads = nullptr)
{
    // The HUD's numbers, and a note on why they are gathered HERE rather than
    // returned by the rasterizer. `fill_triangle` culls internally — that is where
    // the hardware does it — so it could report back, but making every fill return
    // a bool would put a value at 100% of call sites that 99% of them ignore.
    // Counting in the caller costs one extra `edge_function` per triangle and keeps
    // the rule itself in ONE place: `is_front_facing`, which is what the rasterizer
    // calls too. Instrumentation duplicates the *question*, never the answer.
    if (culled != nullptr)
    {
        *culled = {};
        for (const raster_triangle& t : tris)
        {
            ++culled->submitted;
            const bool front = engine::is_front_facing(t.v[0], t.v[1], t.v[2]);
            if (front) { ++culled->front; }
            if (front != t.front_by_forward) { ++culled->disagree; }

            const bool kept = (style.cull == engine::cull_mode::none)
                           || (style.cull == engine::cull_mode::back && front)
                           || (style.cull == engine::cull_mode::front && !front);
            if (kept) { ++culled->drawn; }
        }
    }

    if (sorted) { sort_back_to_front(tris); }

    for (const raster_triangle& t : tris)
    {
        // Lesson 3.8: rebind the material per triangle when the fragment is the
        // one reading it. `style` is taken BY VALUE, so this is a local edit to a
        // local copy and the caller's pipeline object is untouched.
        //
        // Naming it again because it matters: a GPU cannot do this. Material
        // parameters live in a bound pipeline or a uniform buffer, and changing
        // them mid-draw means ending the draw. A renderer that shades per pixel
        // therefore sorts its geometry by material and issues one draw per batch,
        // and the reason this loop can be lazy is that it is not a GPU.
        style.surface = t.surface;
        // Lesson 4.1: `quads` accumulates across the whole draw, which is why
        // fill_triangle adds to it rather than assigning. A per-triangle lane
        // efficiency is not a number anybody wants.
        engine::fill_triangle(fb, depth, t.v[0], t.v[1], t.v[2], style, quads);
    }
}

/// Paint the depth buffer itself into the framebuffer, stretched to its own range.
///
/// Two things are true at once and both are worth seeing. Raw, the buffer is very
/// nearly uniform white: with near = 0.3 the entire visible scene occupies about
/// two percent of the [0,1] range, because depth is distributed as 1/z (§3.6).
/// Stretched between the minimum and maximum actually present, the same numbers
/// show a perfectly readable depth image. The HUD prints the two endpoints, so
/// the readable picture never lets you forget how narrow the band is.
///
/// Returns the (min, max) actually found, for the HUD.
struct depth_range { float lo = 1.0f; float hi = 0.0f; };

[[nodiscard]] depth_range show_depth(engine::framebuffer& fb,
                                     const engine::depth_buffer& db,
                                     const engine::viewport& vp)
{
    const int x0 = std::max(0, static_cast<int>(vp.x));
    const int y0 = std::max(0, static_cast<int>(vp.y));
    const int x1 = std::min(fb.width(), static_cast<int>(vp.x + vp.w));
    const int y1 = std::min(fb.height(), static_cast<int>(vp.y + vp.h));

    depth_range range;
    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            const float d = db.depth_at(x, y);
            if (d >= engine::depth_buffer::k_far) { continue; }   // never written
            range.lo = std::min(range.lo, d);
            range.hi = std::max(range.hi, d);
        }
    }

    const float span = range.hi - range.lo;
    const float inv = (span > 1e-9f) ? 1.0f / span : 0.0f;

    for (int y = y0; y < y1; ++y)
    {
        for (int x = x0; x < x1; ++x)
        {
            const float d = db.depth_at(x, y);
            if (d >= engine::depth_buffer::k_far)
            {
                fb.put_pixel(x, y, engine::pack_argb(16, 18, 26));   // untouched
                continue;
            }
            // NEAR is bright, far is dark — the opposite of the stored value, so
            // the picture reads the way a torch beam does rather than the way the
            // number does.
            const float t01 = 1.0f - std::clamp((d - range.lo) * inv, 0.0f, 1.0f);
            const Uint8 v = static_cast<Uint8>(30.0f + 215.0f * t01);
            fb.put_pixel(x, y, engine::pack_argb(v, v, v));
        }
    }
    return range;
}

/// How many pixels two framebuffers disagree on, inside the viewport rectangle.
///
/// The lesson's headline number. The painter's algorithm and the z-buffer are run
/// on identical geometry every frame and their outputs compared; the count is the
/// size of the region where sorting gets the wrong answer. On the icosahedron it
/// is zero and stays zero. On the cycle it is hundreds, and no amount of
/// improving the sort will move it.
[[nodiscard]] int count_differences(const engine::framebuffer& a,
                                    const engine::framebuffer& b,
                                    const engine::viewport& vp)
{
    const int x0 = std::max(0, static_cast<int>(vp.x));
    const int y0 = std::max(0, static_cast<int>(vp.y));
    const int x1 = std::min(a.width(), static_cast<int>(vp.x + vp.w));
    const int y1 = std::min(a.height(), static_cast<int>(vp.y + vp.h));

    int differ = 0;
    for (int y = y0; y < y1; ++y)
    {
        const Uint32* const ra = a.row(y);
        const Uint32* const rb = b.row(y);
        for (int x = x0; x < x1; ++x)
        {
            if (ra[x] != rb[x]) { ++differ; }
        }
    }
    return differ;
}

/// The brightest single channel anywhere in the viewport, 0..255.
///
/// Lesson 3.7's cheapest honest instrument. A highlight is by definition the
/// brightest thing on a surface, so one number tracks it: watch it climb as the
/// camera swings into the mirror direction, and — on a coarse mesh — watch it
/// COLLAPSE as the object turns and the peak falls between vertices. Reading the
/// maximum rather than a named pixel means the measurement does not have to know
/// where the highlight went, which is exactly the thing under investigation.
///
/// The value is an *encoded* channel (Lesson 1.6), because that is what the screen
/// shows and what the reader can compare against the picture.
[[nodiscard]] int brightest_channel(const engine::framebuffer& fb, const engine::viewport& vp)
{
    const int x0 = std::max(0, static_cast<int>(vp.x));
    const int y0 = std::max(0, static_cast<int>(vp.y));
    const int x1 = std::min(fb.width(), static_cast<int>(vp.x + vp.w));
    const int y1 = std::min(fb.height(), static_cast<int>(vp.y + vp.h));

    int peak = 0;
    for (int y = y0; y < y1; ++y)
    {
        const Uint32* const row = fb.row(y);
        for (int x = x0; x < x1; ++x)
        {
            const Uint32 c = row[x];
            peak = std::max(peak, static_cast<int>(engine::red_of(c)));
            peak = std::max(peak, static_cast<int>(engine::green_of(c)));
            peak = std::max(peak, static_cast<int>(engine::blue_of(c)));
        }
    }
    return peak;
}

// ---------------------------------------------------------------------------
// Lesson 2.10 — the projection matrices the scene is drawn with
// ---------------------------------------------------------------------------

// The scene's field of view and clip planes. The near plane is small (0.3) so the
// orbiting/dollying camera never pushes a vertex behind it — verified: the closest
// the scene's geometry comes to the eye across the whole dolly range is ~1.7 units.
constexpr float k_scene_fovy = 55.0f * 3.14159265358979f / 180.0f;
constexpr float k_scene_aspect = 16.0f / 9.0f;   // matches the viewport rectangle
constexpr float k_scene_near = 0.3f;
constexpr float k_scene_far = 100.0f;

/// An ORTHOGRAPHIC projection matrix, for the [P] comparison — this is what the
/// demo used before Lesson 2.10, expressed as a matrix so it drops into the same
/// pipeline. It keeps `w = 1` (so the perspective divide is a harmless divide by
/// one) and maps a fixed view-space box to NDC, so distance changes nothing.
///
/// The half-extents are chosen so the on-screen scale matches the perspective
/// projection at the camera's default distance — that way pressing [P] swaps how
/// depth is handled without jumping the overall size, and the difference you see is
/// purely "near grows / far shrinks" versus "everything the same size".
///
/// A general `orthographic()` belongs in the engine eventually (2-D overlays,
/// shadow maps); it is kept local here because nothing but this comparison needs it
/// yet, and Lesson 2.11 is where the viewport/ortho machinery gets its real home.
[[nodiscard]] engine::mat4 demo_orthographic()
{
    constexpr float half_w = 6.478f;                    // view-space units mapped to the viewport
    constexpr float half_h = half_w / k_scene_aspect;   // keep the same 16:9 shape
    constexpr float depth = k_scene_far - k_scene_near;
    return {{1.0f / half_w, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f / half_h, 0.0f, 0.0f},
            {0.0f, 0.0f, -1.0f / depth, 0.0f},
            {0.0f, 0.0f, -k_scene_near / depth, 1.0f}};
}

// ---------------------------------------------------------------------------
// Lesson 2.9 — the camera, and the view matrix
// ---------------------------------------------------------------------------

/// The demo camera orbits a fixed target on a sphere: azimuth around, elevation
/// up, radius out. This is not part of the engine — it is the demo's way of moving
/// an `eye` around so `look_at` has something to chew on. A real camera (Module 5)
/// stores a `transform`; this stores the three orbit angles because they are what
/// two arrow keys map onto cleanly.
struct orbit_camera
{
    engine::vec3 target{0.0f, 0.6f, 0.0f};
    float radius = 7.0f;
    float azimuth = 0.0f;     ///< radians; 0 looks down -z at the target
    float elevation = 0.35f;  ///< radians above the ground plane

    /// Where the eye sits, from the orbit angles. Standard spherical placement:
    /// azimuth sweeps around y, elevation lifts toward +y.
    [[nodiscard]] engine::vec3 eye() const
    {
        return target + engine::vec3{radius * std::cos(elevation) * std::sin(azimuth),
                                     radius * std::sin(elevation),
                                     radius * std::cos(elevation) * std::cos(azimuth)};
    }

    /// The view matrix for this camera. up_hint is world up; elevation is clamped
    /// (below) so it never becomes parallel to the look direction, which would make
    /// the `right = cross(up, backward)` degenerate.
    [[nodiscard]] engine::mat4 view() const
    {
        return engine::look_at(eye(), target, {0.0f, 1.0f, 0.0f});
    }
};

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

/// Copy the framebuffer into a streaming texture, row by row.
///
/// Row by row rather than one big memcpy because the pitch SDL hands back may
/// exceed width*4 — some drivers pad each row — and copying as one block would
/// shear the image diagonally on exactly the machines you do not own. Lesson 1.5
/// §4.3.
[[nodiscard]] bool upload(SDL_Texture* texture, const engine::framebuffer& fb)
{
    void* dst_pixels = nullptr;
    int dst_pitch = 0;

    if (!SDL_LockTexture(texture, nullptr, &dst_pixels, &dst_pitch))
    {
        return false;
    }

    Uint8* const dst = static_cast<Uint8*>(dst_pixels);
    const int row_bytes = fb.pitch();

    for (int y = 0; y < fb.height(); ++y)
    {
        std::memcpy(dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(dst_pitch),
                    fb.row(y),
                    static_cast<std::size_t>(row_bytes));
    }

    SDL_UnlockTexture(texture);
    return true;
}

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

/// Turn the keyboard into the two numbers Pong's simulation wants.
[[nodiscard]] game::intent read_intent(const engine::input& in, bool right_is_ai)
{
    game::intent wanted;
    wanted.right_is_ai = right_is_ai;

    if (in.key_down(SDL_SCANCODE_W)) { wanted.left -= 1.0f; }
    if (in.key_down(SDL_SCANCODE_S)) { wanted.left += 1.0f; }

    if (in.key_down(SDL_SCANCODE_UP))   { wanted.right -= 1.0f; }
    if (in.key_down(SDL_SCANCODE_DOWN)) { wanted.right += 1.0f; }

    return wanted;
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
        SDL_Log("Run without --gpu for the software demo.");
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

    SDL_Log("Keys: [1] filter  [2] present mode  [3] frames in flight"
            "  [4] wait on a fence every frame  [Esc] quit");
    SDL_Log("Graph: green = software raster, cyan = recording, blue = waiting for a"
            " swapchain image, orange = waiting on a fence.");

    engine::clock clk;
    engine::input in;

    constexpr int k_ring = 160;
    probe_sample ring[k_ring] = {};
    int ring_head = 0;

    bool running = true;
    bool logged_swapchain_size = false;
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
    for (engine::gpu_shader& shader : shaders) { shader.destroy(); }
    present.destroy();
    gpu.destroy();
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    // Lesson 4.2. One flag, and the only one this executable has: `--gpu` runs the
    // GPU probe instead of the software demo. A flag rather than a key, because
    // the choice has to be made before the window belongs to anybody — see the
    // comment above `run_gpu_probe`.
    bool want_gpu = false;
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--gpu") == 0) { want_gpu = true; }
    }

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const int sdl_version = SDL_GetVersion();
    SDL_Log("Engine starting — SDL %d.%d.%d",
            SDL_VERSIONNUM_MAJOR(sdl_version),
            SDL_VERSIONNUM_MINOR(sdl_version),
            SDL_VERSIONNUM_MICRO(sdl_version));

    SDL_Window* window = SDL_CreateWindow("The Z-Buffer — Module 3", 1280, 720,
                                          SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Lesson 4.2's fork in the road, and it has to be here — before any renderer
    // exists. A window can be claimed by an SDL_GPU device or driven by an
    // SDL_Renderer, and there is no order of operations in which it is both.
    if (want_gpu)
    {
        const int rc = run_gpu_probe(window);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return rc;
    }

    SDL_Log("Software demo. Run with --gpu for Lesson 4.2's GPU probe.");

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int vsync = 1;
    if (!SDL_SetRenderVSync(renderer, vsync))
    {
        SDL_Log("SDL_SetRenderVSync(%d) failed: %s — continuing unsynchronised",
                vsync, SDL_GetError());
        vsync = SDL_RENDERER_VSYNC_DISABLED;
    }

    engine::framebuffer fb(k_fb_width, k_fb_height);

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

    SDL_Texture* screen_texture = SDL_CreateTexture(renderer,
                                                    SDL_PIXELFORMAT_ARGB8888,
                                                    SDL_TEXTUREACCESS_STREAMING,
                                                    k_fb_width, k_fb_height);
    if (screen_texture == nullptr)
    {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureScaleMode(screen_texture, SDL_SCALEMODE_NEAREST);

    engine::clock clk;
    engine::input in;
    engine::fixed_step stepper(60.0f);

    // ---- Demo state --------------------------------------------------------
    demo which = demo::scene;
    spin cube_mode = spin::about_z;     ///< Lesson 2.6 — z shows 2.8's shear best
    float cube_t = 0.6f;
    bool cube_animating = true;
    float cube_point_w = 1.0f;          ///< 1 = positions (correct); 0 = Lesson 2.6
    float cube_dir_w = 0.0f;            ///< 0 = directions (correct); 1 = the normal bug
    engine::mat3 cube_m;

    // ---- Lesson 2.8's scene ------------------------------------------------
    trs_order order = trs_order::trs;   ///< [O] — only the first is right
    int selected = 0;                   ///< [X] — which object the HUD reports on
    scene_object scene[k_max_objects];
    int scene_count = 0;                ///< how many of them this scene uses
    engine::mat4 selected_m;            ///< the selected object's model matrix
    engine::vec3 selected_world;        ///< one model vertex, carried into world space
    float selected_axis_len = 0.0f;     ///< |model x axis| in world units
    float selected_corner = 0.0f;       ///< angle between model x and y, in degrees

    /// Vertex 0 of the SELECTED object's mesh — the one the HUD narrates through
    /// every space, refreshed each frame because [X] can change which mesh it is.
    engine::vec3 selected_probe;

    // ---- Lesson 2.9's camera -----------------------------------------------
    orbit_camera cam;                   ///< arrow keys orbit; [-]/[=] dolly
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
        engine::perspective(k_scene_fovy, k_scene_aspect, k_scene_near, k_scene_far);
    const engine::mat4 scene_orthographic = demo_orthographic();

    // ---- Lesson 3.1 --------------------------------------------------------
    hidden_surface hs = hidden_surface::zbuffer;   ///< [F]
    scene_kind scene_mode = scene_kind::solids;    ///< [C]
    engine::depth_format depth_fmt = engine::depth_format::f32;   ///< [B]
    std::vector<raster_triangle> scene_tris;       ///< reused, never reallocated per frame
    projection_scratch scratch;                    ///< ditto, for the per-vertex arrays
    int painter_wrong = 0;                         ///< px where the two algorithms disagree
    depth_range shown_depth;                       ///< what the depth view actually contained

    // ---- Lesson 3.2 --------------------------------------------------------
    engine::interpolation interp = engine::interpolation::perspective;   ///< [I]
    floor_geometry floor;                          ///< rebuilt only when [T] changes it
    int floor_cells = 1;                           ///< quads per side; [T] cycles
    int interp_wrong = 0;                          ///< px where affine and perspective differ

    // ---- Lesson 3.3 --------------------------------------------------------
    near_mode near_handling = near_mode::clip;     ///< [K]
    clip_stats scene_clip;                         ///< what the near plane did this frame
    std::vector<raster_triangle> compare_tris;     ///< the reference render's own geometry
    int near_wrong = 0;                            ///< px this near mode gets wrong vs clipping

    // ---- Lesson 3.4 --------------------------------------------------------
    cull_choice culling = cull_choice::none;       ///< [U]
    cull_stats scene_cull;                         ///< kept / culled / disagreeing, this frame
    int cull_wrong = 0;                            ///< px the current cull mode gets wrong

    // ---- Lesson 3.5 --------------------------------------------------------
    model_state model;                             ///< [L] chooses; loaded on the spot
    int roundtrip_wrong = 0;                       ///< px between the loaded and generated torus

    // ---- Lesson 3.6 --------------------------------------------------------
    shade_eval eval = shade_eval::gouraud;         ///< [G] — palette / flat / Gouraud / per-pixel
    normal_source nsrc = normal_source::vertex;    ///< [Q] — face vs vertex normals
    int grid_wrong = 0;                            ///< px this cell differs from per-pixel by
    double shade_ns = 0.0;                         ///< smoothed cost of the scene fill, ns
    bool correct_normals = true;                   ///< [J] — inverse transpose vs the naive M
    normal_stats scene_normals;                    ///< what the normals did this frame
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
    engine::specular_model spec_model = engine::specular_model::blinn;   ///< [H]
    int shininess_step = 4;                        ///< [E] indexes k_shininess below
    int model_wrong = 0;                           ///< px Phong and Blinn disagree about
    int spec_peak = 0;                             ///< brightest pixel in the viewport, 0..255

    /// The exponents [E] cycles through. Powers of two, because that is how the
    /// parameter behaves: each step roughly halves the width of the highlight, so a
    /// linear slider would spend most of its travel on differences you cannot see.
    /// 2 is a damp, broad sheen; 32 a polished plastic; 256 close to a mirror.
    constexpr float k_shininess[] = {2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f};
    constexpr int k_shininess_count = static_cast<int>(std::size(k_shininess));

    // ---- Lesson 3.9 --------------------------------------------------------
    albedo_source albedo = albedo_source::checker;   ///< [M] — which image, or the rule
    engine::sampler samp;                            ///< [S] filter, [R] address, [1] origin
    bool uv_flip_on_load = true;                     ///< [2] — OBJ v-up to texture v-down
    texture_set textures;                            ///< the three images, built once
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

    // The control mesh, built once. `assets/torus.obj` was written from exactly this
    // call, so "loaded == generated" is a real end-to-end check of writer and reader
    // together — and it is a claim the demo re-tests on every frame it is shown.
    model.generated = engine::make_torus(48, 24, 1.0f, 0.4f);
    load_model(model, model_choice::torus, uv_flip_on_load);

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

    const Uint32 seed = static_cast<Uint32>(SDL_GetTicksNS() & 0xFFFFFFFFu);
    game::state pong_current = game::make_state(seed);
    game::state pong_previous = pong_current;
    bool pong_right_is_ai = true;

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
    SDL_Log("  [G] evaluate: debug palette / flat / Gouraud (per-vertex) / PER-PIXEL");
    SDL_Log("  [Q] normal source: face (cross product) / vertex (as authored)");
    SDL_Log("  [J] normal matrix: inverse-transpose (correct) vs the naive model matrix");
    SDL_Log("  [A]/[D] swing the light. With [H] off the camera does NOT change the shading -");
    SDL_Log("          Lambert is view-independent. Turn [H] on and orbiting moves the highlight.");
    SDL_Log("  [H] specular: none / Phong / Blinn-Phong   [E] shininess exponent");
    SDL_Log("  [M] albedo: procedural rule / checker / uv grid / fine checker (on floor + model)");
    SDL_Log("  [S] filter: bilinear / nearest   [R] address: repeat / mirrored / clamp");
    SDL_Log("  [1] texel origin: centre (correct) vs corner - half a texel, invisible under [S] nearest");
    SDL_Log("  [2] uv v flip on import: OBJ counts v upwards, a texture counts it downwards");
    SDL_Log("  [3] the frame budget: which phase the frame is actually spent in");
    SDL_Log("  [4] sRGB encode: fitted sqrt chain (fast) vs std::pow (exact) - watch `fill`");
    SDL_Log("  [5] traversal: scanline / 2x2 quads / quads with the helper lanes shown");
    SDL_Log("  [arrows] orbit  [-]/[=] dolly  [P] persp/ortho  [O] model order  [X] object");
    SDL_Log("  [Z] rotation axis  [,] [.] t  [Space] spin  [W]/[N] the 2.7 w bugs");
    SDL_Log("[Tab] cycles demos: scene (2.6-3.3) -> basis (2.5) -> triangles -> lines -> Pong");
    SDL_Log("[V] vsync · [Y] throttle · [Esc] quit");

    bool running = true;
    while (running)
    {
        // Lesson 3.10. The frame starts HERE — before the event drain, because
        // draining events is work the frame does and a budget that starts after it
        // has a hole in it by construction. It ends just before SDL_RenderPresent,
        // so the total is *our* work and does not include the vsync wait; the HUD
        // prints the wall-clock frame time beside it so the gap between the two is
        // visible rather than hidden.
        prof.begin_frame();

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            in.feed_event(event);

            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                SDL_Log("Quit requested");
                running = false;
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                SDL_Log("Window %u close requested", static_cast<unsigned>(event.window.windowID));
                running = false;
                break;

            default:
                break;
            }
        }

        clk.tick();
        in.update();

        if (in.key_pressed(SDL_SCANCODE_ESCAPE)) { running = false; }

        if (in.key_pressed(SDL_SCANCODE_TAB))
        {
            which = next_demo(which);
            SDL_SetWindowTitle(window,
                which == demo::scene     ? "The Z-Buffer — Module 3"
              : which == demo::basis     ? "Basis Transforms — Module 2"
              : which == demo::triangles ? "Triangles — Module 2"
              : which == demo::lines     ? "Lines — Module 2"
                                         : "Pong — Module 1 Checkpoint");
        }

        if (in.key_pressed(SDL_SCANCODE_V))
        {
            vsync = (vsync == SDL_RENDERER_VSYNC_DISABLED) ? 1 : SDL_RENDERER_VSYNC_DISABLED;
            if (!SDL_SetRenderVSync(renderer, vsync))
            {
                SDL_Log("SDL_SetRenderVSync(%d) failed: %s", vsync, SDL_GetError());
            }
        }

        stepper.begin_frame(clk.dt());

        if (which == demo::scene)
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
                shininess_step = (shininess_step + 1) % k_shininess_count;
            }
            if (in.key_pressed(SDL_SCANCODE_L))
            {
                // A real load, on a keypress, every time — not a cache lookup. It
                // costs about half a millisecond for the torus and the HUD says so,
                // which is the honest way to introduce the fact that asset loading
                // is work. Module 5 caches; today we measure.
                load_model(model, next_model(model.choice), uv_flip_on_load);
                scene_mode = scene_kind::model;
                selected = 0;
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
                load_model(model, model.choice, uv_flip_on_load);
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
                cam.elevation = (scene_mode == scene_kind::solids) ? 0.35f
                              : (scene_mode == scene_kind::cycle)  ? 0.0f
                              : (scene_mode == scene_kind::floor)  ? 0.10f
                              : (scene_mode == scene_kind::model)  ? 0.45f
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
                const float min_radius = (scene_mode == scene_kind::floor) ? 1.0f : 4.0f;
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
                lights.key.intensity = 1.0f;
            }

            {
                // zone::build — placing the objects. It will read 0.00 us, and
                // that is not a bug: §2.2 measured this machine's counter at a
                // 41.7 ns tick, so anything under about 4 us cannot be resolved.
                // A zone that reads zero is either work you are not doing or work
                // you cannot measure, and this one is the second.
                const engine::scope_timer z{prof, engine::zone::build};
                cube_m = build_spin(cube_mode, cube_t);
                build_floor(floor, floor_cells);
                scene_count = build_scene(scene, scene_mode, cube_mode, cube_t, floor, model,
                                          k_shininess[shininess_step]);
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
            const engine::viewport& vp = (scene_mode == scene_kind::floor)
                                       ? k_full_viewport : k_scene_viewport;

            // Lesson 3.3's gathering: the projection, the viewport and the
            // near-plane policy, travelling together. Everything that draws in 3-D
            // now takes exactly one of these.
            const projector pr{proj, vp, near_handling};

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
                draw_world(fb, view_from_world, pr);
            }

            if (hs == hidden_surface::wireframe)
            {
                // Lesson 2.12's picture, unchanged: edges only, so there are no
                // surfaces to hide and nothing for a depth buffer to do.
                for (int i = 0; i < scene_count; ++i)
                {
                    const engine::mat4 world_from_model = model_matrix(scene[i].xform, order);
                    const engine::mat4 view_from_model = view_from_world * world_from_model;
                    draw_mesh(fb, scene[i].geometry, view_from_model, pr, cube_point_w);
                    draw_axes3(fb, view_from_model, pr, cube_point_w, cube_dir_w);
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
                scene_clip = {};
                scene_cull = {};
                scene_normals = {};
            }
            else
            {
                // Project the whole scene ONCE, into one flat list. Both
                // algorithms below then run on identical geometry, which is what
                // makes the pixel-for-pixel comparison honest.
                {
                    // zone::collect — the vertex stage. Everything from the model
                    // matrix to a screen-space triangle: transform, clip, per-vertex
                    // lighting. §3.3 shows this bar is flat in resolution and linear
                    // in triangle count, which is the whole of why it is a separate
                    // zone from the one below it.
                    const engine::scope_timer z{prof, engine::zone::collect};
                    collect_triangles(scene_tris, scratch, scene, scene_count,
                                      view_from_world, pr, order, scene_clip, culling,
                                      nsrc, eval, lights, correct_normals, &scene_normals,
                                      eye_world, spec_model);
                }

                const bool want_painter = (hs == hidden_surface::painter);

                // zone::sort — the painter's algorithm, and only then. Hoisted out
                // of `draw_triangles` so it can be timed apart from the fill;
                // `sort_back_to_front` is the single copy of the rule, called from
                // here and from there.
                if (want_painter)
                {
                    const engine::scope_timer z{prof, engine::zone::sort};
                    sort_back_to_front(scene_tris);
                }

                // ---- Lesson 3.9: what supplies this draw's albedo? ---------
                //
                // `nullptr` under `albedo_source::rule`, which is what makes the
                // rule and the lookup a single keypress apart. `texture_binding`
                // holds a non-owning pointer, and `textures` lives for the whole
                // program, so there is nothing here that can dangle.
                const engine::texture* image = textures.pick(albedo);

                // Which surfaces read a texture at all. The FLOOR always: it is the
                // one mesh in the demo with uvs authored for tiling, and it has been
                // the uv testbed since 3.2. The MODEL too, because its uvs came off
                // a disk and are therefore the only ones that can demonstrate the
                // OBJ-versus-texture-space disagreement.
                const bool uv_surface = (scene_mode == scene_kind::floor)
                                     || (scene_mode == scene_kind::model);

                // Only the floor still falls back to the procedural rule when no
                // image is bound; the model has no uv-derived colour to show and
                // reads its vertex tint instead.
                const bool checkered = (scene_mode == scene_kind::floor)
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
                const bool shade_per_pixel = (eval == shade_eval::per_pixel) && !checkered;
                const engine::fill_style style{
                    .interp = interp,
                    .shade = checkered ? engine::shading::uv_checker
                           : shade_per_pixel ? engine::shading::lit
                           : bind_texture    ? engine::shading::textured
                                             : engine::shading::vertex_colour,
                    .space = engine::blend_space::linear,
                    .cull = to_engine_cull(culling),
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
                    draw_triangles(fb, want_painter ? nullptr : &scene_depth,
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
                if (culling != cull_choice::none)
                {
                    engine::fill_style unculled = style;
                    unculled.cull = engine::cull_mode::none;

                    scratch_fb.clear(k_bg);
                    draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();

                    if (culling == cull_choice::back_by_forward)
                    {
                        // That mode culls in collect_triangles, not in the
                        // rasterizer, so the reference needs its own geometry.
                        clip_stats ignored;
                        collect_triangles(compare_tris, scratch, scene, scene_count,
                                          view_from_world, pr, order, ignored,
                                          cull_choice::none, nsrc, eval, lights,
                                          correct_normals, nullptr, eye_world, spec_model);
                        draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                       compare_tris, want_painter, unculled);
                    }
                    else
                    {
                        draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                       scene_tris, want_painter, unculled);
                    }
                    cull_wrong = count_differences(fb, scratch_fb, vp);
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
                if (scene_mode == scene_kind::model && model.choice == model_choice::torus)
                {
                    scene_object control = scene[0];
                    control.geometry = model.generated.view();

                    clip_stats ignored;
                    collect_triangles(compare_tris, scratch, &control, 1,
                                      view_from_world, pr, order, ignored, culling,
                                      nsrc, eval, lights, correct_normals, nullptr,
                                      eye_world, spec_model);

                    scratch_fb.clear(k_bg);
                    draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   compare_tris, want_painter, style);
                    roundtrip_wrong = count_differences(fb, scratch_fb, vp);
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
                if (eval != shade_eval::palette && scene_normals.max_tilt > 0.0f)
                {
                    clip_stats ignored;
                    collect_triangles(compare_tris, scratch, scene, scene_count,
                                      view_from_world, pr, order, ignored, culling,
                                      nsrc, eval, lights, !correct_normals, nullptr,
                                      eye_world, spec_model);

                    scratch_fb.clear(k_bg);
                    draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   compare_tris, want_painter, style);
                    normal_wrong = count_differences(fb, scratch_fb, vp);
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
                    && eval != shade_eval::palette)
                {
                    scene_object other_scene[k_max_objects];
                    for (int i = 0; i < scene_count; ++i)
                    {
                        other_scene[i] = scene[i];
                        other_scene[i].surface.shininess =
                            matched_shininess(spec_model, scene[i].surface.shininess);
                    }

                    clip_stats ignored;
                    collect_triangles(compare_tris, scratch, other_scene, scene_count,
                                      view_from_world, pr, order, ignored, culling,
                                      nsrc, eval, lights, correct_normals, nullptr,
                                      eye_world,
                                      spec_model == engine::specular_model::blinn
                                          ? engine::specular_model::phong
                                          : engine::specular_model::blinn);

                    scratch_fb.clear(k_bg);
                    draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   compare_tris, want_painter, style);
                    model_wrong = count_differences(fb, scratch_fb, vp);
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
                if (eval != shade_eval::palette && !checkered
                    && hs != hidden_surface::wireframe)
                {
                    engine::fill_style reference_style = style;
                    reference_style.shade = engine::shading::lit;
                    reference_style.lights = &lights;

                    clip_stats ignored;
                    collect_triangles(compare_tris, scratch, scene, scene_count,
                                      view_from_world, pr, order, ignored, culling,
                                      nsrc, shade_eval::per_pixel, lights,
                                      correct_normals, nullptr, eye_world, spec_model);

                    scratch_fb.clear(k_bg);
                    draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   compare_tris, want_painter, reference_style);
                    grid_wrong = count_differences(fb, scratch_fb, vp);
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
                    draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   scene_tris, want_painter, other);
                    texel_wrong = count_differences(fb, scratch_fb, vp);

                    // ...and what the filter itself is worth, by the same method.
                    engine::fill_style flipped = style;
                    flipped.albedo.samp.texel_filter =
                        (samp.texel_filter == engine::filter::linear)
                            ? engine::filter::nearest
                            : engine::filter::linear;

                    scratch_fb.clear(k_bg);
                    draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   scene_tris, want_painter, flipped);
                    filter_wrong = count_differences(fb, scratch_fb, vp);
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
                    draw_world(scratch_fb, view_from_world, pr);
                    scratch_depth.clear();
                    draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   scene_tris, want_painter, other_encode);
                    encode_wrong = count_differences(fb, scratch_fb, vp);
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
                spec_peak = brightest_channel(fb, vp);

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
                const bool near_on_trial = (pr.near != near_mode::clip);

                // The reference render. Identical in every respect except the one
                // under test — including the background grid, which is drawn
                // through the projector and is therefore itself affected by [K].
                projector reference = pr;
                if (near_on_trial) { reference.near = near_mode::clip; }

                scratch_fb.clear(k_bg);
                draw_world(scratch_fb, view_from_world, reference);
                scratch_depth.clear();

                if (near_on_trial)
                {
                    // A second collection, because the clipper produces a
                    // DIFFERENT SET OF TRIANGLES — this is the one comparison in
                    // the demo where the two renders cannot share geometry.
                    clip_stats reference_clip;
                    collect_triangles(compare_tris, scratch, scene, scene_count,
                                      view_from_world, reference, order, reference_clip,
                                      culling, nsrc, eval, lights, correct_normals, nullptr,
                                      eye_world, spec_model);
                    draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   compare_tris, want_painter, style);
                    near_wrong = count_differences(fb, scratch_fb, vp);
                    interp_wrong = 0;
                    painter_wrong = 0;
                }
                else if (checkered)
                {
                    engine::fill_style other = style;
                    other.interp = (interp == engine::interpolation::perspective)
                                 ? engine::interpolation::affine
                                 : engine::interpolation::perspective;
                    draw_triangles(scratch_fb, want_painter ? nullptr : &scratch_depth,
                                   scene_tris, want_painter, other);
                    interp_wrong = count_differences(fb, scratch_fb, vp);
                    painter_wrong = 0;
                    near_wrong = 0;
                }
                else
                {
                    draw_triangles(scratch_fb, want_painter ? &scratch_depth : nullptr,
                                   scene_tris, !want_painter, style);
                    painter_wrong = count_differences(fb, scratch_fb, vp);
                    interp_wrong = 0;
                    near_wrong = 0;
                }

                // The depth view runs last, over the z-buffered image, because it
                // needs the buffer the z-buffer pass just filled.
                shown_depth = (hs == hidden_surface::depth_view)
                            ? show_depth(fb, scene_depth, vp)
                            : depth_range{};
            }

            // Everything the HUD reports is read back out of the matrices that were
            // actually used to draw. The probe is now vertex 0 OF THE SELECTED MESH,
            // carried the whole chain — model -> world -> view -> clip -> NDC ->
            // screen — so the HUD narrates a real vertex of the shape on screen.
            selected_probe = scene[selected].geometry.vertices.empty()
                           ? engine::vec3{}
                           : scene[selected].geometry.vertices[0];
            selected_m = model_matrix(scene[selected].xform, order);
            selected_world = engine::xyz(selected_m
                                       * engine::to_vec4(selected_probe, cube_point_w));
            selected_view = engine::xyz(view_from_world * engine::point(selected_world));
            selected_clip = proj * engine::point(selected_view);
            selected_ndc = engine::perspective_divide(selected_clip);
            selected_screen = vp.to_screen(selected_ndc);
            selected_axis_len = axis_length(selected_m, {1.0f, 0.0f, 0.0f});
            selected_corner = axis_angle_deg(selected_m, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        }
        else if (which == demo::basis)
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
        else if (which == demo::pong)
        {
            // ---- Lesson 1.8's game, unchanged --------------------------------
            if (in.key_pressed(SDL_SCANCODE_C)) { pong_right_is_ai = !pong_right_is_ai; }
            if (in.key_pressed(SDL_SCANCODE_K))
            {
                pong_current.swept_collision = !pong_current.swept_collision;
                pong_previous.swept_collision = pong_current.swept_collision;
            }

            const game::intent wanted = read_intent(in, pong_right_is_ai);
            while (stepper.next_step())
            {
                pong_previous = pong_current;
                game::step(pong_current, wanted, stepper.h());
                if (pong_current.teleported) { pong_previous = pong_current; }
            }
            game::draw(fb, pong_previous, pong_current, stepper.alpha());
        }
        else if (which == demo::lines)
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
            if (!upload(screen_texture, fb))
            {
                SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
            }

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, screen_texture, nullptr, nullptr);
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

        if (which == demo::scene)
        {
            const bool correct_order = (order == trs_order::trs);

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
            else if (scene_mode == scene_kind::floor)
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
            else if (scene_mode == scene_kind::model)
            {
                // Lesson 3.5's readout. The three numbers that ARE the index
                // problem: how many positions the file holds, how many extra
                // vertices reconciling the attribute streams cost, and the total.
                const bool good = model.load.ok() && model.check.consistently_wound();
                SDL_SetRenderDrawColor(renderer, good ? 122 : 236,
                                                 good ? 196 : 92,
                                                 good ? 152 : 92, 255);
                if (!model.load.ok())
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 48.0f,
                        "[L] %-16s  FAILED: %s (line %d)",
                        name_of(model.choice), engine::name_of(model.load.status),
                        model.load.line);
                }
                else
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 48.0f,
                        "[L] %-16s  %d+%d -> %d verts, %d tris  [%.2f ms]",
                        name_of(model.choice), model.load.positions,
                        model.load.split_vertices, model.load.vertices,
                        model.load.triangles, model.load_ms);
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
                const bool near_ok = (near_handling == near_mode::clip);
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
                        name_of(near_handling), scene_clip.input, scene_clip.output,
                        scene_clip.straddling, scene_clip.behind);
                }
                else
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 62.0f,
                        "[K] near = %-16s  %d tris -> %d  vs clipped: %d px WRONG",
                        name_of(near_handling), scene_clip.input, scene_clip.output,
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
                const bool cull_sound = (culling == cull_choice::none)
                                     || (culling == cull_choice::back && !any_open);
                SDL_SetRenderDrawColor(renderer, cull_sound ? 122 : 236,
                                                 cull_sound ? 196 : 196,
                                                 cull_sound ? 152 : 110, 255);
                if (hs == hidden_surface::wireframe)
                {
                    SDL_RenderDebugTextFormat(renderer, 6.0f, 76.0f,
                        "[U] cull = %-18s  (wireframe draws no faces to cull)",
                        name_of(culling));
                }
                else if (culling == cull_choice::none)
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
            if (scene_mode != scene_kind::floor)
            {
                const bool lit = (eval != shade_eval::palette);
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
                        static_cast<double>(scene_normals.max_tilt), normal_wrong);
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
                    if (eval == shade_eval::per_pixel)
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
                            static_cast<double>(k_shininess[shininess_step]),
                            model_wrong, spec_peak);
                    }
                }
            }

            const bool floor_scene = (scene_mode == scene_kind::floor);

            if (floor_scene)
            {
                // The floor uses the whole framebuffer, so there is no column of
                // spare pixels to write into — every HUD line moves up into the
                // sky above the horizon, which is the only empty part left.
                SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
                if (near_handling == near_mode::clip
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
                else if (near_handling == near_mode::clip)
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
                else if (near_handling == near_mode::drop)
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
                const engine::texture* hud_image = textures.pick(albedo);
                const bool hud_uv = (scene_mode == scene_kind::floor
                                  || scene_mode == scene_kind::model);

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
                            scene[selected].geometry.vertices.size(),
                            scene[selected].geometry.triangle_count(),
                            scene[selected].geometry.indices.size());
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
                        else if (scene_mode == scene_kind::cycle)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "A over B over C over A. No");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, "order is right, and all three");
                            SDL_RenderDebugText(renderer, 380.0f, 290.0f, "average depths are equal.");
                        }
                        else if (scene_mode == scene_kind::intersect)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "they pass THROUGH each other:");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, "the right answer changes");
                            SDL_RenderDebugText(renderer, 380.0f, 290.0f, "halfway across one triangle.");
                        }
                        else if (scene_mode == scene_kind::floor)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "affine interpolation makes the");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, "checker swim, and breaks along");
                            SDL_RenderDebugText(renderer, 380.0f, 290.0f, "the diagonal. [T] subdivides:");
                            SDL_RenderDebugText(renderer, 380.0f, 304.0f, "the error falls, never to zero.");
                        }
                        else if (scene_mode == scene_kind::model)
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

                            if (model.choice == model_choice::torus)
                            {
                                // Writer and reader, checked against each other in
                                // the only currency that matters: pixels.
                                SDL_SetRenderDrawColor(renderer, roundtrip_wrong ? 236 : 122,
                                                                 roundtrip_wrong ? 92 : 196,
                                                                 roundtrip_wrong ? 92 : 152, 255);
                                SDL_RenderDebugTextFormat(renderer, 380.0f, 318.0f,
                                    "round trip: %d px differ", roundtrip_wrong);
                            }
                            else if (model.choice == model_choice::twisted)
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
                        else if (scene_mode == scene_kind::zfight)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "B is 1 mm in front of A, so B");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, "should win everywhere. [B] to");
                            SDL_RenderDebugText(renderer, 380.0f, 290.0f, "D16_UNORM and watch it stop.");
                        }
                        else if (order != trs_order::trs)
                        {
                            SDL_RenderDebugText(renderer, 380.0f, 262.0f, "wrong model order [O]: 2.8's");
                            SDL_RenderDebugText(renderer, 380.0f, 276.0f, order == trs_order::tsr
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
                                            "[arrows] orbit  [-][=] dolly  [3] budget  [4] encode  [Tab] demo");
            }
        }
        else if (which == demo::basis)
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
                                "[Z] transform  [,] [.] adjust t  [0] reset  [Space] animate  [Tab] demo");
        }
        else if (which == demo::pong)
        {
            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "PONG (Lesson 1.8)   collision: %-6s   rally %d",
                                      pong_current.swept_collision ? "swept" : "naive",
                                      pong_current.rally_hits);
            SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                "[Tab] next demo   [W]/[S]   [C] 2P   [K] collision   [Esc] quit");
        }
        else if (which == demo::lines)
        {
            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "LINES (Lesson 2.1)   %-16s   %d spokes   fps %5.1f",
                                      line_algo_name, k_spokes,
                                      static_cast<double>(clk.fps()));
            SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                "[1] naive  [2] DDA  [3] Bresenham   [Space] spin   [Tab] next demo   [Esc] quit");
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
                                "[M] blend  [ ] bands  [R] rule  [Space] spin  [Tab] demo  [Esc] quit");
        }

        // Over the render, deliberately — see draw_budget. Drawn last so nothing
        // else can paint over it, and only in the demo whose frame it describes.
        if (which == demo::scene && show_budget)
        {
            draw_budget(renderer, prof, clk.raw_dt() * 1.0e9f, covered_px, fill_ns_per_px,
                        encode, encode_wrong, walk, scene_quads, 6.0f, 170.0f);
        }

        SDL_SetRenderScale(renderer, 1.0f, 1.0f);

        prof.add(engine::zone::overlay,
                 prof.to_ns(engine::profiler::now_ticks() - hud_t0));

        // The frame ends here, before the blocking call. See `begin_frame` above.
        prof.end_frame();

        SDL_RenderPresent(renderer);

        // The frame throttle moved from [T] to [Y] in Lesson 3.2, because [T]
        // now cycles the floor tessellation and a key that means two things in
        // two demos is a key that gets pressed by accident in both.
        if (in.key_down(SDL_SCANCODE_Y))
        {
            SDL_Delay(k_throttle_ms);
        }
    }

    SDL_DestroyTexture(screen_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
