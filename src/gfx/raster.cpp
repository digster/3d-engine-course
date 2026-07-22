// src/gfx/raster.cpp — the three line algorithms of Lesson 2.1.
//
// Read them in the order they appear. Each one fixes something the one above it
// got wrong, and the last one is the only one that should ever be called.

#include "gfx/raster.hpp"

#include "gfx/framebuffer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace engine {

// ---- 1. The obvious one, which does not work -------------------------------

void draw_line_naive(framebuffer& fb, int x0, int y0, int x1, int y1, Uint32 colour)
{
    // "A line is y = mx + b" — so walk x and evaluate y. The trouble starts
    // before the loop does.
    //
    // A vertical line has dx = 0, and its slope is not merely large, it does not
    // exist: there is no m for which y = mx + b describes x = 5. That is not a
    // rounding problem or a special case to patch, it is the equation being the
    // wrong tool. Any formulation that needs an if-statement to survive one of
    // its inputs is telling you something.
    if (x0 == x1)
    {
        const int step = (y1 >= y0) ? 1 : -1;
        for (int y = y0; y != y1 + step; y += step) { fb.put_pixel(x0, y, colour); }
        return;
    }

    // Walking x from left to right means the caller's endpoint order is lost;
    // the pixels are the same set either way here, which is not true of the
    // algorithms below.
    if (x0 > x1)
    {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    const float slope = static_cast<float>(y1 - y0) / static_cast<float>(x1 - x0);

    for (int x = x0; x <= x1; ++x)
    {
        // One pixel per COLUMN. That is the whole bug: if the line rises by more
        // than one pixel per column — any line steeper than 45° — the pixels it
        // lights are vertically separated, and the result is a dotted line with
        // gaps that widen as the slope does. Lesson 2.1 §1.
        const float y = static_cast<float>(y0) + slope * static_cast<float>(x - x0);
        fb.put_pixel(x, static_cast<int>(std::lround(y)), colour);
    }
}

// ---- 2. DDA: step the major axis ------------------------------------------

void draw_line_dda(framebuffer& fb, int x0, int y0, int x1, int y1, Uint32 colour)
{
    const int dx = x1 - x0;
    const int dy = y1 - y0;

    // The fix for the gaps, and it is one line. Step whichever axis changes
    // MORE, so that axis advances by exactly one pixel per iteration and the
    // other advances by at most one. No step can skip a row or a column, so the
    // line cannot come apart.
    const int steps = std::max(std::abs(dx), std::abs(dy));

    if (steps == 0)
    {
        fb.put_pixel(x0, y0, colour);   // a line from a point to itself
        return;
    }

    const float x_inc = static_cast<float>(dx) / static_cast<float>(steps);
    const float y_inc = static_cast<float>(dy) / static_cast<float>(steps);

    float x = static_cast<float>(x0);
    float y = static_cast<float>(y0);

    for (int i = 0; i <= steps; ++i)
    {
        fb.put_pixel(static_cast<int>(std::lround(x)),
                     static_cast<int>(std::lround(y)),
                     colour);
        // The defect worth naming, and it is NOT speed. This is a running sum:
        // the value at step i is the result of i additions, each of which
        // rounded. Two mathematically identical lines computed by different
        // routes can therefore light different pixels, and the error grows with
        // the length of the line. Bresenham's error term, below, is exact at
        // every step because it never leaves the integers.
        x += x_inc;
        y += y_inc;
    }
}

// ---- 3. Bresenham: the same decision, in integers --------------------------

void draw_line(framebuffer& fb, int x0, int y0, int x1, int y1, Uint32 colour)
{
    // Lesson 2.1 §3.3 derives this for the first octant, where it reads as:
    // step x, add dy to an error term, and when the error passes half a pixel,
    // step y and take a whole pixel back out. The form below is that algorithm
    // with the octant bookkeeping folded in, and §3.5 checks the two produce
    // identical pixels rather than asserting it.
    //
    // Two conventions make the folding work:
    //   - dx is kept POSITIVE and dy NEGATIVE, so a single error term can be
    //     compared against both without a second sign to track;
    //   - sx and sy carry the direction, so the loop never needs to know which
    //     octant it is in.
    const int dx = std::abs(x1 - x0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = (y0 < y1) ? 1 : -1;

    // err is twice the signed distance from the ideal line to the pixel centre,
    // scaled so it stays an integer — the doubling is what turns the "half a
    // pixel" test of the derivation into a comparison with no fraction in it.
    int err = dx + dy;

    for (;;)
    {
        fb.put_pixel(x0, y0, colour);

        // Tested after plotting, so both endpoints are lit and a zero-length
        // line lights exactly one pixel.
        if (x0 == x1 && y0 == y1) { break; }

        const int e2 = 2 * err;

        // The two tests are INDEPENDENT, and that is the whole trick. A shallow
        // line trips only the first most steps; a steep line trips only the
        // second; a 45° line trips both every step and moves diagonally. One
        // loop, eight octants, no cases.
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }

    // A note on what this does NOT do: a line whose endpoints are far outside
    // the framebuffer is still walked pixel by pixel, and every one of those
    // pixels is discarded by put_pixel's bounds check. The output is correct and
    // the work is wasted — a line from (-100000, 0) to (100000, 0) costs 200001
    // iterations to draw at most 320 pixels. Clipping the line to the viewport
    // BEFORE walking it is the fix (Cohen–Sutherland; Exercise 2.1.4), and it
    // becomes non-optional in Module 3, where geometry behind the camera must be
    // clipped for reasons of correctness rather than speed.
}

// ---- 4. Triangles: three half-planes ---------------------------------------

namespace {

/// Is the directed edge A→B a "top" or a "left" edge of its triangle?
///
/// Assumes the triangle has already been oriented so that its signed area is
/// **positive** — which in framebuffer coordinates (+y down) means its vertices
/// run clockwise on screen. Under that orientation:
///
///   - a **top** edge is horizontal and travels rightwards (`dy == 0, dx > 0`);
///   - a **left** edge is any edge travelling upwards (`dy < 0`).
///
/// Both are geometric properties of the edge, which is the entire point. Two
/// triangles that share an edge traverse it in opposite directions, so exactly
/// one of them sees it as top-or-left and claims the pixels lying exactly on it.
/// Neither triangle needs to know the other exists. Lesson 2.2 §3.6.
[[nodiscard]] constexpr bool is_top_left(int ax, int ay, int bx, int by)
{
    const int dx = bx - ax;
    const int dy = by - ay;
    return (dy == 0 && dx > 0) || (dy < 0);
}

} // namespace

barycentric barycentric_at(int x0, int y0, int x1, int y1, int x2, int y2,
                           int px, int py)
{
    const int area = edge_function(x0, y0, x1, y1, x2, y2);
    if (area == 0) { return {}; }   // collinear: no interior, so no answer

    // Each weight comes from the sub-triangle OPPOSITE its vertex — the one
    // that does not touch it. w0 therefore uses the edge v1->v2.
    //
    // These are the same three numbers fill_triangle already computes for its
    // inside test. Nothing new is being calculated here; the only new thing is
    // the division, which turns "twice an area" into "a fraction of the whole".
    const int e0 = edge_function(x1, y1, x2, y2, px, py);
    const int e1 = edge_function(x2, y2, x0, y0, px, py);
    const int e2 = edge_function(x0, y0, x1, y1, px, py);

    // e0 + e1 + e2 == area EXACTLY, in integers, for every point in the plane —
    // inside or outside. The float sum below is therefore 1 up to one rounding,
    // not up to accumulated drift.
    const float inv = 1.0f / static_cast<float>(area);
    return {static_cast<float>(e0) * inv,
            static_cast<float>(e1) * inv,
            static_cast<float>(e2) * inv};
}

void fill_triangle(framebuffer& fb,
                   int x0, int y0, int x1, int y1, int x2, int y2,
                   Uint32 colour)
{
    // Twice the signed area. One number that answers two questions: is this
    // triangle degenerate, and which way round is it?
    int area = edge_function(x0, y0, x1, y1, x2, y2);

    // Three collinear points enclose nothing. Returning here is not just an
    // optimisation — every test below would be simultaneously satisfiable only
    // on the line itself, and the top-left rule's orientation assumption would
    // have no meaning. A "sliver" triangle of zero area is a real thing to hit
    // once meshes arrive, so it is handled rather than assumed away.
    if (area == 0) { return; }

    // Orient to positive area by swapping two vertices, which flips the sign of
    // every edge function at once. Doing this once, here, is what lets the
    // inside test below be a plain `>= 0` rather than "same sign as the area"
    // evaluated per pixel.
    if (area < 0)
    {
        std::swap(x1, x2);
        std::swap(y1, y2);
        area = -area;
    }

    // The bounding box, clipped to the framebuffer. Two jobs: it bounds the
    // search (a triangle covering 1% of the screen should not cost a full-screen
    // scan), and clipping it here means the inner loop never needs a bounds
    // check — which is why this can write through row() instead of put_pixel.
    const int min_x = std::max(0, std::min({x0, x1, x2}));
    const int min_y = std::max(0, std::min({y0, y1, y2}));
    const int max_x = std::min(fb.width() - 1, std::max({x0, x1, x2}));
    const int max_y = std::min(fb.height() - 1, std::max({y0, y1, y2}));

    if (min_x > max_x || min_y > max_y) { return; }   // entirely off-screen

    // The top-left rule as an integer bias. An edge function is zero exactly on
    // the edge, so testing `w >= 0` includes boundary pixels and `w - 1 >= 0`
    // excludes them. Folding the -1 into the starting value costs nothing per
    // pixel — the test stays a single comparison against zero.
    const int bias0 = is_top_left(x1, y1, x2, y2) ? 0 : -1;   // edge opposite v0
    const int bias1 = is_top_left(x2, y2, x0, y0) ? 0 : -1;   // edge opposite v1
    const int bias2 = is_top_left(x0, y0, x1, y1) ? 0 : -1;   // edge opposite v2

    // An edge function is *affine* in the pixel position, so its value at the
    // next pixel differs from this one by a constant. Evaluating it once at the
    // corner and then adding is the same trick as Lesson 2.1's error term, and
    // it turns two multiplies per edge per pixel into one add.
    //
    //   E(x+1, y) - E(x, y) = -(By - Ay) = Ay - By
    //   E(x, y+1) - E(x, y) =  (Bx - Ax)
    const int step_x0 = y1 - y2, step_y0 = x2 - x1;
    const int step_x1 = y2 - y0, step_y1 = x0 - x2;
    const int step_x2 = y0 - y1, step_y2 = x1 - x0;

    int row_w0 = edge_function(x1, y1, x2, y2, min_x, min_y) + bias0;
    int row_w1 = edge_function(x2, y2, x0, y0, min_x, min_y) + bias1;
    int row_w2 = edge_function(x0, y0, x1, y1, min_x, min_y) + bias2;

    for (int y = min_y; y <= max_y; ++y)
    {
        int w0 = row_w0;
        int w1 = row_w1;
        int w2 = row_w2;

        // The whole row is in bounds by construction, so this takes the
        // documented fast path from Lesson 1.5 and skips put_pixel's per-pixel
        // bounds check and index multiply.
        Uint32* const row = fb.row(y);

        for (int x = min_x; x <= max_x; ++x)
        {
            // Inside all three half-planes at once. That is the entire test,
            // and it is the same three numbers for every pixel — only their
            // values change.
            //
            // Worth knowing: `(w0 | w1 | w2) >= 0` is exactly equivalent and
            // costs one comparison instead of three. A negative int has its top
            // bit set, OR keeps any bit that any operand has, so the result is
            // negative precisely when some input was. C++20 guarantees signed
            // integers are two's complement, which is what makes that a
            // portable claim rather than a lucky one. Written the long way here
            // because you should be able to read this line without knowing that.
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                row[x] = colour;
            }

            w0 += step_x0;
            w1 += step_x1;
            w2 += step_x2;
        }

        row_w0 += step_y0;
        row_w1 += step_y1;
        row_w2 += step_y2;
    }
}

void draw_triangle(framebuffer& fb,
                   int x0, int y0, int x1, int y1, int x2, int y2,
                   Uint32 colour)
{
    draw_line(fb, x0, y0, x1, y1, colour);
    draw_line(fb, x1, y1, x2, y2, colour);
    draw_line(fb, x2, y2, x0, y0, colour);
}

} // namespace engine
