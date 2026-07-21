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

} // namespace engine
