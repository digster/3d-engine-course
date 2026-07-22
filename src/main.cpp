// src/main.cpp — the engine's entry point.
//
// This file hosts the current lesson's demo. As of Lesson 2.4 that is a rotating
// triangle with seven views: filled, wireframe, the three half-planes, a weight
// ramp, the barycentric iso-line grid, Gouraud-shaded corners, and a checker
// mapped from interpolated (u, v). The right-hand panel changes with the view —
// 2.2's coverage counter, or 2.4's bias magnifier.
//
// [Tab] now CYCLES three demos — triangles (2.2), lines (2.1), and Pong (1.8) —
// because deleting a working demo to make room would be a regression. Three is
// already too many for one executable, and the next one will be worse. That is
// not an oversight: Module 5 opens by splitting the tree into a static library
// and a demos/ directory, and this file is where the argument for doing so
// accumulates until it is impossible to ignore.
//
// The loop is the one settled in Lesson 1.4 and does not change again:
//
//     drain events -> tick clock -> update input -> N fixed steps -> render

#include "core/clock.hpp"
#include "core/fixed_step.hpp"
#include "core/input.hpp"
#include "game/pong.hpp"
#include "gfx/colour.hpp"
#include "gfx/framebuffer.hpp"
#include "gfx/raster.hpp"
#include "math/mat2.hpp"
#include "math/mat3.hpp"
#include "math/mat4.hpp"
#include "math/vec2.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr int k_fb_width = 320;
constexpr int k_fb_height = 180;

constexpr Uint32 k_throttle_ms = 50;

/// Which lesson's demo is on screen. [Tab] cycles.
enum class demo
{
    cube,        ///< Lesson 2.6 — this lesson
    basis,       ///< Lesson 2.5
    triangles,   ///< Lessons 2.2 – 2.4
    lines,       ///< Lesson 2.1
    pong         ///< Lesson 1.8
};

[[nodiscard]] demo next_demo(demo d)
{
    switch (d)
    {
    case demo::cube:      return demo::basis;
    case demo::basis:     return demo::triangles;
    case demo::triangles: return demo::lines;
    case demo::lines:     return demo::pong;
    case demo::pong:      return demo::cube;
    }
    return demo::cube;
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

constexpr engine::vec2 k_cube_origin{96.0f, 92.0f};
constexpr float k_cube_unit = 46.0f;   ///< framebuffer pixels per unit

/// 3-D maths space -> framebuffer pixels, by **dropping z**.
///
/// This is an orthographic projection, and it is the crudest one there is: throw
/// the depth away and keep x and y. It is enough to see that the transformations
/// are working, and it is deliberately not enough to look right — parallel edges
/// of the cube stay exactly parallel on screen, so the far face is drawn the same
/// size as the near one and the picture is ambiguous about which is which.
///
/// Making distant things smaller is what a PERSPECTIVE projection does, and it is
/// Lesson 2.10's whole subject. The y negation is the same one Lesson 2.5's demo
/// needed, for the same reason, in the same one place.
[[nodiscard]] engine::vec2 to_screen3(engine::vec3 v)
{
    return {k_cube_origin.x + v.x * k_cube_unit,
            k_cube_origin.y - v.y * k_cube_unit};
}

void line3(engine::framebuffer& fb, engine::vec3 a, engine::vec3 b, Uint32 colour)
{
    const engine::vec2 pa = to_screen3(a);
    const engine::vec2 pb = to_screen3(b);
    engine::draw_line(fb,
        static_cast<int>(std::lround(pa.x)), static_cast<int>(std::lround(pa.y)),
        static_cast<int>(std::lround(pb.x)), static_cast<int>(std::lround(pb.y)), colour);
}

// A unit cube centred on the origin, and the twelve edges joining its corners.
// Centred rather than corner-at-origin so that rotation — which is always about
// the origin (Lesson 2.5 §3.8) — spins it in place rather than swinging it round.
constexpr engine::vec3 k_cube_v[8] = {
    {-0.5f, -0.5f, -0.5f}, {+0.5f, -0.5f, -0.5f}, {+0.5f, +0.5f, -0.5f}, {-0.5f, +0.5f, -0.5f},
    {-0.5f, -0.5f, +0.5f}, {+0.5f, -0.5f, +0.5f}, {+0.5f, +0.5f, +0.5f}, {-0.5f, +0.5f, +0.5f},
};
constexpr int k_cube_e[12][2] = {
    {0,1},{1,2},{2,3},{3,0},   // back face
    {4,5},{5,6},{6,7},{7,4},   // front face
    {0,4},{1,5},{2,6},{3,7},   // the four struts joining them
};

/// Draw the cube, with edge brightness standing in for depth.
///
/// The brightness is a **cue, not a calculation** — there is no depth buffer yet
/// (Lesson 3.1) and no lighting (Lesson 3.6), so this is simply "edges further
/// away are dimmer" so the wireframe can be read at all. Without it a rotating
/// wireframe cube is a genuinely ambiguous picture: your eye flips between two
/// interpretations, because orthographic projection has discarded the only
/// information that could settle it.
void draw_cube(engine::framebuffer& fb, const engine::mat3& m, engine::vec3 offset)
{
    engine::vec3 p[8];
    for (int i = 0; i < 8; ++i) { p[i] = m * k_cube_v[i] + offset; }

    for (const auto& e : k_cube_e)
    {
        const float mid_z = 0.5f * (p[e[0]].z + p[e[1]].z);
        const float t = std::clamp(mid_z + 0.5f, 0.0f, 1.0f);   // -0.5..0.5 -> 0..1
        const Uint8 v = static_cast<Uint8>(70.0f + 165.0f * t);
        line3(fb, p[e[0]], p[e[1]], engine::pack_argb(v, v, static_cast<Uint8>(v * 0.94f)));
    }
}

/// The three transformed basis vectors, in the course's axis colours.
///
/// Exactly Lesson 2.5's picture with a third arrow: the columns of the matrix,
/// drawn. Whatever the cube is doing, these three arrows are why.
void draw_axes3(engine::framebuffer& fb, const engine::mat3& m, engine::vec3 offset)
{
    const Uint32 col[3] = {engine::pack_argb(236, 92, 92),     // x
                           engine::pack_argb(122, 196, 152),   // y
                           engine::pack_argb(126, 162, 236)};  // z
    const engine::vec3 c[3] = {m.c0, m.c1, m.c2};

    for (int i = 0; i < 3; ++i)
    {
        const engine::vec3 tip = c[i] * 0.9f + offset;
        line3(fb, offset, tip, col[i]);

        // A small head, built by rotating the arrow's own direction — the same
        // trick as 2.5's, now in the plane of the screen because that is where
        // the head has to be legible.
        const engine::vec2 a = to_screen3(offset);
        const engine::vec2 b = to_screen3(tip);
        const engine::vec2 back = engine::normalised(a - b) * 5.0f;
        if (engine::length_squared(back) > 0.0f)
        {
            const engine::vec2 h1 = b + engine::rotation(0.5f) * back;
            const engine::vec2 h2 = b + engine::rotation(-0.5f) * back;
            auto ix = [](float f) { return static_cast<int>(std::lround(f)); };
            engine::draw_line(fb, ix(b.x), ix(b.y), ix(h1.x), ix(h1.y), col[i]);
            engine::draw_line(fb, ix(b.x), ix(b.y), ix(h2.x), ix(h2.y), col[i]);
        }
    }
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

} // namespace

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

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

    SDL_Window* window = SDL_CreateWindow("3-D: mat3 and mat4 — Module 2", 1280, 720,
                                          SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

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
    demo which = demo::cube;
    spin cube_mode = spin::tumble_xy;   ///< Lesson 2.6
    float cube_t = 0.6f;
    bool cube_animating = true;
    bool cube_try_translate = false;    ///< stuff a translation into the 4x4's c3
    engine::mat3 cube_m;
    engine::vec3 cube_moved_by;         ///< what the fourth column actually achieved
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
    SDL_Log("Cube (2.6): [Z] rotation  [,] [.] adjust  [Space] spin  [T] try translating via the 4x4");
    SDL_Log("[Tab] cycles demos: cube (2.6) -> basis (2.5) -> triangles -> lines -> Pong");
    SDL_Log("[V] vsync · [T] throttle · [Esc] quit");

    bool running = true;
    while (running)
    {
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
                which == demo::cube      ? "3-D: mat3 and mat4 — Module 2"
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

        if (which == demo::cube)
        {
            // ---- Lesson 2.6's cube --------------------------------------------
            if (in.key_pressed(SDL_SCANCODE_Z)) { cube_mode = next_spin(cube_mode); }
            if (in.key_pressed(SDL_SCANCODE_SPACE)) { cube_animating = !cube_animating; }
            if (in.key_pressed(SDL_SCANCODE_0)) { cube_t = 0.0f; }
            if (in.key_pressed(SDL_SCANCODE_T)) { cube_try_translate = !cube_try_translate; }

            while (stepper.next_step())
            {
                if (cube_animating) { cube_t += 0.7f * stepper.h(); }
                if (in.key_down(SDL_SCANCODE_COMMA))  { cube_t -= 1.2f * stepper.h(); }
                if (in.key_down(SDL_SCANCODE_PERIOD)) { cube_t += 1.2f * stepper.h(); }
            }

            cube_m = build_spin(cube_mode, cube_t);

            // The demonstration this lesson is built around. Embed the rotation in
            // a 4x4, write a translation into the fourth column — the place a
            // translation obviously belongs — and see what it does. Our vectors
            // carry w = 0, so the fourth column is multiplied by zero and the
            // answer is: nothing. The offset below is MEASURED from the result
            // rather than assumed, so the HUD cannot lie about it.
            engine::mat4 m4 = engine::to_mat4(cube_m);
            if (cube_try_translate) { m4.c3 = engine::vec4{1.2f, 0.0f, 0.0f, 1.0f}; }

            const engine::vec3 probe{0.5f, 0.5f, 0.5f};
            const engine::vec3 via_mat3 = cube_m * probe;
            const engine::vec3 via_mat4 = engine::xyz(m4 * engine::to_vec4(probe, 0.0f));
            cube_moved_by = via_mat4 - via_mat3;

            fb.clear(k_bg);
            draw_cube(fb, cube_m, cube_moved_by);
            draw_axes3(fb, cube_m, cube_moved_by);
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
                engine::fill_triangle(fb,
                    engine::vertex{vx[0], vy[0], engine::pack_argb(255, 0, 0)},
                    engine::vertex{vx[1], vy[1], engine::pack_argb(0, 255, 0)},
                    engine::vertex{vx[2], vy[2], engine::pack_argb(0, 0, 255)},
                    linear_blend ? engine::blend_space::linear
                                 : engine::blend_space::encoded);
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

        if (!upload(screen_texture, fb))
        {
            SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, screen_texture, nullptr, nullptr);

        // A text coordinate is exactly twice a framebuffer coordinate: 2x text
        // scale over a 4x framebuffer scale.
        SDL_SetRenderScale(renderer, 2.0f, 2.0f);

        if (which == demo::cube)
        {
            const float det = engine::determinant(cube_m);

            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "CUBE (3-D)   %-28s  t = %+.2f",
                                      name_of(cube_mode), static_cast<double>(cube_t));

            SDL_RenderDebugText(renderer, 380.0f, 40.0f, "the mat3, as written:");
            for (int r = 0; r < 3; ++r)
            {
                SDL_RenderDebugTextFormat(renderer, 380.0f, 56.0f + 14.0f * static_cast<float>(r),
                                          "| %+.3f %+.3f %+.3f |",
                                          static_cast<double>(cube_m.at(r, 0)),
                                          static_cast<double>(cube_m.at(r, 1)),
                                          static_cast<double>(cube_m.at(r, 2)));
            }

            SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
            SDL_RenderDebugTextFormat(renderer, 380.0f, 116.0f,
                                      "det = %+.4f  (volume factor)", static_cast<double>(det));
            SDL_RenderDebugText(renderer, 380.0f, 136.0f, "red/green/blue arrows are");
            SDL_RenderDebugText(renderer, 380.0f, 150.0f, "the three columns, drawn.");

            // The lesson's punchline, on screen and measured.
            SDL_SetRenderDrawColor(renderer, cube_try_translate ? 236 : 150,
                                             cube_try_translate ? 92 : 152,
                                             cube_try_translate ? 92 : 170, 255);
            SDL_RenderDebugTextFormat(renderer, 380.0f, 178.0f,
                                      "[T] 4x4 c3 = (1.2, 0, 0): %s", cube_try_translate ? "SET" : "off");
            SDL_RenderDebugTextFormat(renderer, 380.0f, 192.0f,
                                      "cube actually moved by %.2f px",
                                      static_cast<double>(engine::length(cube_moved_by)) * 46.0);
            if (cube_try_translate)
            {
                SDL_RenderDebugText(renderer, 380.0f, 212.0f, "a translation is sitting in");
                SDL_RenderDebugText(renderer, 380.0f, 226.0f, "the matrix doing nothing.");
                SDL_RenderDebugText(renderer, 380.0f, 240.0f, "our w is 0, so column 3 is");
                SDL_RenderDebugText(renderer, 380.0f, 254.0f, "multiplied by 0.  -> 2.7");
            }

            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                "[Z] rotation  [,] [.] adjust  [0] reset  [Space] spin  [T] translate  [Tab] demo");
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

        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
        SDL_RenderPresent(renderer);

        if (in.key_down(SDL_SCANCODE_T))
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
