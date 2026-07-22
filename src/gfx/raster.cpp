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

/// Everything a triangle fill works out before it touches a pixel.
///
/// This was inline in `fill_triangle` until Lesson 2.4 gave the file a second
/// fill, and Module 3 will give it more. The bookkeeping here — the clipped
/// bounding box, the three fill-rule biases, the six per-pixel steps, the three
/// starting values — is short, and every line of it is subtle. Duplicating it
/// would not be a style problem; it would be three places for one bias to be
/// wrong, producing a crack visible only where two particular triangles meet.
///
/// The rule this follows is not "never repeat yourself". It is *never repeat
/// something subtle* — a duplicated `x + 1` costs nothing, a duplicated
/// top-left rule costs an afternoon.
struct fill_setup
{
    int min_x = 0, min_y = 0, max_x = -1, max_y = -1;

    /// 0 for a top-or-left edge, -1 otherwise. Folded into the starting values
    /// below, so the coverage test stays a single comparison against zero — and
    /// therefore **subtracted back out** by anything that interpolates.
    int bias0 = 0, bias1 = 0, bias2 = 0;

    int step_x0 = 0, step_y0 = 0;
    int step_x1 = 0, step_y1 = 0;
    int step_x2 = 0, step_y2 = 0;

    int row_w0 = 0, row_w1 = 0, row_w2 = 0;

    [[nodiscard]] bool empty() const { return min_x > max_x || min_y > max_y; }
};

/// Prepare a fill for a triangle that is **already oriented to positive area**.
///
/// Orientation is deliberately not done here. Reorienting means swapping two
/// vertices, and from Lesson 2.4 a vertex carries attributes that must swap with
/// it — so the caller, which is the only code that knows what a vertex holds,
/// owns that step. Everything after it is mechanical and identical for every
/// fill, which is exactly what belongs in a shared helper.
[[nodiscard]] fill_setup prepare_fill(const framebuffer& fb,
                                      int x0, int y0, int x1, int y1, int x2, int y2)
{
    fill_setup s;

    // The bounding box, clipped to the framebuffer. Two jobs: it bounds the
    // search (a triangle covering 1% of the screen should not cost a full-screen
    // scan), and clipping it here means the inner loop never needs a bounds
    // check — which is why a fill can write through row() instead of put_pixel.
    s.min_x = std::max(0, std::min({x0, x1, x2}));
    s.min_y = std::max(0, std::min({y0, y1, y2}));
    s.max_x = std::min(fb.width() - 1, std::max({x0, x1, x2}));
    s.max_y = std::min(fb.height() - 1, std::max({y0, y1, y2}));

    if (s.empty()) { return s; }   // entirely off-screen

    // The top-left rule as an integer bias. An edge function is zero exactly on
    // the edge, so testing `w >= 0` includes boundary pixels and `w - 1 >= 0`
    // excludes them. Folding the -1 into the starting value costs nothing per
    // pixel — the test stays a single comparison against zero.
    s.bias0 = is_top_left(x1, y1, x2, y2) ? 0 : -1;   // edge opposite v0
    s.bias1 = is_top_left(x2, y2, x0, y0) ? 0 : -1;   // edge opposite v1
    s.bias2 = is_top_left(x0, y0, x1, y1) ? 0 : -1;   // edge opposite v2

    // An edge function is *affine* in the pixel position, so its value at the
    // next pixel differs from this one by a constant. Evaluating it once at the
    // corner and then adding is the same trick as Lesson 2.1's error term, and
    // it turns two multiplies per edge per pixel into one add.
    //
    //   E(x+1, y) - E(x, y) = -(By - Ay) = Ay - By
    //   E(x, y+1) - E(x, y) =  (Bx - Ax)
    s.step_x0 = y1 - y2; s.step_y0 = x2 - x1;
    s.step_x1 = y2 - y0; s.step_y1 = x0 - x2;
    s.step_x2 = y0 - y1; s.step_y2 = x1 - x0;

    s.row_w0 = edge_function(x1, y1, x2, y2, s.min_x, s.min_y) + s.bias0;
    s.row_w1 = edge_function(x2, y2, x0, y0, s.min_x, s.min_y) + s.bias1;
    s.row_w2 = edge_function(x0, y0, x1, y1, s.min_x, s.min_y) + s.bias2;

    return s;
}

/// Three floats at a triangle corner, waiting to be averaged.
///
/// Deliberately *not* `linear_rgb`. What these numbers mean depends on the blend
/// space in force — light in [0,1] under `blend_space::linear`, stored channel
/// values in [0,255] under `blend_space::encoded` — and a type called
/// `linear_rgb` holding encoded values would be a lie that compiles. Naming a
/// type after what it contains rather than what it is used for is how units get
/// mixed up, and mixing up these particular units is the entire subject of
/// Lesson 1.6.
struct rgb3
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

/// A corner colour, converted into whatever space we are about to average in.
[[nodiscard]] rgb3 corner_in(Uint32 colour, blend_space space)
{
    if (space == blend_space::linear)
    {
        const linear_rgb light = to_linear(colour);
        return {light.r, light.g, light.b};
    }
    return {static_cast<float>(red_of(colour)),
            static_cast<float>(green_of(colour)),
            static_cast<float>(blue_of(colour))};
}

/// The weighted average, back to a storable pixel.
[[nodiscard]] Uint32 pixel_from(rgb3 mixed, blend_space space)
{
    if (space == blend_space::linear)
    {
        return to_encoded({mixed.r, mixed.g, mixed.b});
    }

    // Already in stored units, so there is nothing to encode — which is exactly
    // what makes this path cheap, and exactly what makes it wrong. The +0.5
    // rounds to nearest; the weights sum to 1 to within 2.4e-7, so the result
    // cannot exceed 255.5 and the cast cannot wrap.
    return pack_argb(static_cast<Uint8>(mixed.r + 0.5f),
                     static_cast<Uint8>(mixed.g + 0.5f),
                     static_cast<Uint8>(mixed.b + 0.5f));
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

    const fill_setup s = prepare_fill(fb, x0, y0, x1, y1, x2, y2);
    if (s.empty()) { return; }

    int row_w0 = s.row_w0;
    int row_w1 = s.row_w1;
    int row_w2 = s.row_w2;

    for (int y = s.min_y; y <= s.max_y; ++y)
    {
        int w0 = row_w0;
        int w1 = row_w1;
        int w2 = row_w2;

        // The whole row is in bounds by construction, so this takes the
        // documented fast path from Lesson 1.5 and skips put_pixel's per-pixel
        // bounds check and index multiply.
        Uint32* const row = fb.row(y);

        for (int x = s.min_x; x <= s.max_x; ++x)
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

            w0 += s.step_x0;
            w1 += s.step_x1;
            w2 += s.step_x2;
        }

        row_w0 += s.step_y0;
        row_w1 += s.step_y1;
        row_w2 += s.step_y2;
    }
}

void fill_triangle(framebuffer& fb,
                   const vertex& a, const vertex& b, const vertex& c,
                   blend_space space)
{
    // Local copies, because reorienting reorders the vertices — and this is the
    // whole reason a vertex is a struct. `std::swap` on the struct moves the
    // colour with the position it belongs to. Swapping loose coordinates and
    // forgetting the loose colours is a bug with no geometric symptom at all:
    // the triangle is the right shape, in the right place, shaded one corner
    // out of step. Lesson 2.4 §4.2.
    vertex v0 = a;
    vertex v1 = b;
    vertex v2 = c;

    int area = edge_function(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (area == 0) { return; }
    if (area < 0)
    {
        std::swap(v1, v2);
        area = -area;
    }

    const fill_setup s = prepare_fill(fb, v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (s.empty()) { return; }

    // One reciprocal for the whole triangle. The weights are edge values over
    // the total area, and the total area does not vary across a triangle — so
    // the division that turns "twice an area" into "a fraction" is hoisted, and
    // the inner loop pays a multiply instead. Lesson 2.3 §4.
    const float inv_area = 1.0f / static_cast<float>(area);

    // Convert the corners **once per triangle**, not once per pixel. Three
    // conversions for a triangle covering eight thousand pixels; the interior is
    // then pure arithmetic. This hoist is the reason the correct path is
    // affordable at all — see §3.7 for what it costs when you forget it.
    const rgb3 c0 = corner_in(v0.colour, space);
    const rgb3 c1 = corner_in(v1.colour, space);
    const rgb3 c2 = corner_in(v2.colour, space);

    int row_w0 = s.row_w0;
    int row_w1 = s.row_w1;
    int row_w2 = s.row_w2;

    for (int y = s.min_y; y <= s.max_y; ++y)
    {
        int w0 = row_w0;
        int w1 = row_w1;
        int w2 = row_w2;

        Uint32* const row = fb.row(y);

        for (int x = s.min_x; x <= s.max_x; ++x)
        {
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                // **Unbias, then divide.** The accumulators carry the top-left
                // rule's -1 on any edge that is not top-or-left; that -1 is a
                // statement about who owns a boundary pixel, and it is not a
                // statement about where the pixel is. Left in, it displaces the
                // whole attribute field by 1/edge_length of a pixel and stops
                // the three weights summing to 1. Taking it back out is one
                // integer subtraction against a value that is *exact* — these
                // are the stepped integers, so there is no accumulated drift to
                // undo, only a known constant. Lesson 2.4 §3.5.
                const float f0 = static_cast<float>(w0 - s.bias0) * inv_area;
                const float f1 = static_cast<float>(w1 - s.bias1) * inv_area;
                const float f2 = static_cast<float>(w2 - s.bias2) * inv_area;

                // Three multiply-adds per channel. This is the interpolation,
                // and it is the same three lines whatever the attribute turns
                // out to be — depth in Lesson 3.1, texture coordinates in 3.7,
                // normals in 3.6. The rasterizer never learns what it carries.
                const rgb3 mixed{f0 * c0.r + f1 * c1.r + f2 * c2.r,
                                 f0 * c0.g + f1 * c1.g + f2 * c2.g,
                                 f0 * c0.b + f1 * c1.b + f2 * c2.b};

                // One branch per pixel on a value that is constant for the whole
                // triangle. A branch predictor eats this for free; hoisting it
                // would mean two copies of the loop, which is a worse trade at
                // this size. Measured either way in §3.7.
                row[x] = pixel_from(mixed, space);
            }

            w0 += s.step_x0;
            w1 += s.step_x1;
            w2 += s.step_x2;
        }

        row_w0 += s.step_y0;
        row_w1 += s.step_y1;
        row_w2 += s.step_y2;
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
