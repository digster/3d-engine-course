// engine/src/gfx/raster.cpp — the software rasterizer's implementation.
//
// Three line algorithms (Lesson 2.1), then triangles by edge function (2.2),
// barycentric coordinates (2.3), attribute interpolation (2.4), the depth test
// (3.1), per-pixel shading (3.8) and texturing (3.9).
//
// Read the line routines in the order they appear. Each one fixes something the
// one above it got wrong, and the last one is the only one that should ever be
// called.

#include <engine/gfx/raster.hpp>

#include <engine/gfx/cascade.hpp>
#include <engine/gfx/hdr.hpp>   // 6.12: fill_style holds a pointer; the fill needs the type
#include <engine/gfx/mipmap.hpp>
#include <engine/gfx/shadow.hpp>   // 6.8: fill_style holds a pointer; the fill needs the type

#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/framebuffer.hpp>

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
[[nodiscard]] Uint32 pixel_from(rgb3 mixed, blend_space space, encode_mode mode)
{
    if (space == blend_space::linear)
    {
        return to_encoded({mixed.r, mixed.g, mixed.b}, mode);
    }

    // Already in stored units, so there is nothing to encode — which is exactly
    // what makes this path cheap, and exactly what makes it wrong. The +0.5
    // rounds to nearest; the weights sum to 1 to within 2.4e-7, so the result
    // cannot exceed 255.5 and the cast cannot wrap.
    return pack_argb(static_cast<Uint8>(mixed.r + 0.5f),
                     static_cast<Uint8>(mixed.g + 0.5f),
                     static_cast<Uint8>(mixed.b + 0.5f));
}

/// A procedural checkerboard: one cell per unit of texture space.
///
/// `std::floor`, not a cast to int, and the difference is a real bug rather than
/// a style point: a cast truncates TOWARD ZERO, so -0.5 and +0.5 both land in
/// cell 0 and the pattern grows a doubled cell straddling the origin. Floor is
/// the function that means "which cell is this in" for negative coordinates too,
/// and uvs go negative the moment a mesh is tiled or a uv is offset.
///
/// Fixed colours, deliberately. This is a debug pattern with one job — showing
/// you where texture space lands on a surface — and a configurable palette would
/// be a knob nobody turns.
///
/// **Lesson 3.9 did not delete this**, which was the plan when 3.2 wrote it. A
/// procedural rule is not a worse texture; it is a different thing with different
/// costs — no memory, no sampler, no filtering, and *exact at every
/// magnification*, because there is no finite grid of texels to run out of. That
/// last property is why it stays: `shading::textured` right next to it is the
/// cleanest possible demonstration of what a texture buys and what it costs.
[[nodiscard]] Uint32 checker_at(float u, float v)
{
    const float cu = std::floor(u);
    const float cv = std::floor(v);

    // A uv that is infinite or NaN is not a hypothetical, and Lesson 3.3 is where
    // it arrives: let a triangle straddle the near plane with the projective divide
    // still switched on and the interpolated `1/w` passes through zero, so its
    // reciprocal — the factor every attribute is multiplied by — is an infinity.
    //
    // This guard is about UNDEFINED BEHAVIOUR, not tidiness. Converting a float to
    // an int is undefined when the value is not representable, so `static_cast<int>`
    // below on an infinity is not "a big number", it is a program with no defined
    // meaning; the compiler is free to produce anything at all. Magenta is the
    // long-standing debug convention for "this value is invalid", and it is
    // genuinely informative here: it marks exactly the pixels where the divide
    // broke. The fix is upstream — clip the triangle (Lesson 3.3 §3) — and this is
    // only what keeps the broken mode showable.
    //
    // `!(x < limit)` rather than `x >= limit` because every comparison against a
    // NaN is false, so the negated form catches NaN and the direct form does not.
    constexpr float k_cell_limit = 1.0e7f;
    if (!(std::fabs(cu) < k_cell_limit) || !(std::fabs(cv) < k_cell_limit))
    {
        return pack_argb(255, 0, 200);
    }

    const bool light = (static_cast<int>(cu) + static_cast<int>(cv)) % 2 == 0;
    return light ? pack_argb(232, 226, 214) : pack_argb(58, 64, 88);
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

void fill_triangle(framebuffer& fb, depth_buffer* depth,
                   const vertex& a, const vertex& b, const vertex& c,
                   fill_style style, quad_stats* stats)
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

    // ---- Back-face culling (Lesson 3.4) ------------------------------------
    //
    // Note where this sits: AFTER the area is known and BEFORE the reorientation
    // below. That ordering is the whole implementation. The sign of `area` is the
    // facing, and the very next thing this function does is destroy it by
    // swapping two vertices to make the area positive — so the test has exactly
    // one place it can live, and this is it.
    //
    // `is_front_facing` rather than an inline `area < 0`, even though that is
    // literally what it is: the rule now has two readers (this, and the demo's
    // kept/culled counter), and Lesson 2.4 made the same move with `is_top_left`
    // for the same reason — a rule you cannot inspect is a rule you cannot check.
    if (style.cull != cull_mode::none)
    {
        const bool front = is_front_facing(v0, v1, v2);
        if ((style.cull == cull_mode::back && !front)
         || (style.cull == cull_mode::front && front))
        {
            return;
        }
    }

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

    // LESSON 6.8. Is there a map to consult? Decided once per triangle, like
    // every other question whose answer is constant across a fill. `lit` is part
    // of the test because the visibility term multiplies `E`, and `E` only exists
    // on that path.
    const bool shadowed = (style.shadows != nullptr || style.cascades != nullptr)
                          && (style.shade == shading::lit)
                          && (style.lights != nullptr);

    // LESSON 6.8. A depth-only fill, which is what a shadow pass is. Requires a
    // depth attachment: with neither target there would be nothing left for the
    // fill to do, and silently doing nothing is worse than drawing the wrong
    // thing because it looks like the call never happened.
    const bool depth_only = style.depth_only && (depth != nullptr);

    // LESSON 6.11. Which of the three alpha modes this fill is running, decided
    // once per triangle like every other constant question. The names are worth
    // keeping straight because the two paths differ in WHERE THE DEPTH WRITE
    // GOES, which is the structural content of this lesson:
    //
    //   masked   shade first, then discard or write depth. The fragment decides
    //            whether there IS a fragment, so the depth write cannot happen
    //            before it. This is precisely what defeats early-Z on hardware.
    //   blending test depth, never WRITE it, and composite over what is there.
    //            A blended surface does not occlude what is behind it, because
    //            what is behind it is still visible through it — writing depth
    //            would make the surface hide geometry it is supposed to reveal.
    //
    // A depth-only pass ignores both: a shadow map records where surfaces are,
    // and blending has nothing to contribute to that. Masking arguably does — a
    // chain-link fence should cast a chain-link shadow — and that is Exercise 4.
    const bool masked = (style.transparency == alpha_mode::mask) && !depth_only;
    const bool blending = (style.transparency == alpha_mode::blend) && !depth_only;

    // The opaque path, which is every fill written before this lesson. Hoisted
    // into a name so the pixel loop below can take ONE branch back to code that
    // is character for character what 6.10 left, rather than threading two new
    // conditions through the middle of it.
    const bool simple = !masked && !blending;

    // LESSON 6.12. Is this fill writing floats? Decided once per triangle, like
    // every other constant question — and note that it is decided independently
    // of `simple`, because an HDR target and an alpha mode are orthogonal: all
    // six combinations are legitimate and the fill has to serve them.
    //
    // A DEPTH-ONLY FILL IGNORES IT, for the reason `depth_only` already gives:
    // that path computes no colour at all, so the width of the colour it is not
    // computing cannot matter. A shadow map is a depth buffer either way.
    const bool to_hdr = (style.hdr != nullptr) && !depth_only;

    // `lit` needs somewhere to read the light from. A null `lights` is not an
    // error — it is a pipeline that was never given one — so fall back to the
    // unlit path rather than dereferencing nothing. Decided once per triangle.
    const bool lit = (style.shade == shading::lit) && (style.lights != nullptr);

    // Is there an image to read? Decided ONCE PER TRIANGLE, like every other
    // question whose answer is constant across a fill. `bound()` folds together
    // "nobody bound one" and "somebody bound an empty one", because from the
    // fragment's point of view those are the same situation.
    const bool textured = style.albedo.bound();

    // LESSON 6.7. Three conditions, all constant for the whole triangle, so the
    // branch below costs nothing per pixel — only the work behind it does.
    //
    // The third one is the interesting one: geometry with no tangents cannot be
    // normal mapped, because tangent space IS the uv parameterisation and a mesh
    // without one has no frame to map into. The zero `xyz` that `tangent_at`
    // returns for a mesh with no tangents is what makes this checkable here
    // rather than needing a flag alongside — the same trick `normal_at` has used
    // since 3.6.
    const bool normal_mapped = style.normal_map.bound()
                               && (v0.tangent.x != 0.0f || v0.tangent.y != 0.0f
                                   || v0.tangent.z != 0.0f);

    // Under `lit` the corner colours are an ALBEDO that is about to be multiplied
    // by a quantity of light, so they must be decoded whatever `blend_space` says.
    // `blend_space::encoded` is a statement about how to *blend two colours*, and
    // there is no coherent reading of it here: multiplying a stored sRGB byte by a
    // cosine is not dim light, it is nothing at all (Lesson 1.6). So `lit` ignores
    // the field rather than honouring it into nonsense.
    const blend_space space = lit ? blend_space::linear : style.space;

    // Convert the corners **once per triangle**, not once per pixel. Three
    // conversions for a triangle covering eight thousand pixels; the interior is
    // then pure arithmetic. This hoist is the reason the correct path is
    // affordable at all — see §3.7 for what it costs when you forget it.
    const rgb3 c0 = corner_in(v0.colour, space);
    const rgb3 c1 = corner_in(v1.colour, space);
    const rgb3 c2 = corner_in(v2.colour, space);

    // ---- Perspective correction (Lesson 3.2) -------------------------------
    //
    // THE AFFINE MODE IS NOT A SEPARATE CODE PATH. It is this same arithmetic
    // with every `1/w` forced to 1 — which is precisely what affine
    // interpolation *is*: a renderer behaving as though `w = 1` everywhere, i.e.
    // as though the projection were orthographic. Writing it that way rather than
    // as a second loop is not only shorter, it says the thing.
    //
    // Note the consequence for 2-D: `vertex::inv_w` defaults to 1, so a flat fill
    // takes the "correct" path and gets exactly the answer it always got.
    const bool correct = (style.interp == interpolation::perspective);
    const float iw0 = correct ? v0.inv_w : 1.0f;
    const float iw1 = correct ? v1.inv_w : 1.0f;
    const float iw2 = correct ? v2.inv_w : 1.0f;

    // Pre-divide every attribute by w, ONCE PER TRIANGLE. What interpolates
    // affinely across the screen is `a/w`, not `a` — so these are the values the
    // inner loop actually blends, and the division back out happens per pixel
    // using the interpolated `1/w`. Lesson 3.2 §3.3.
    const rgb3 p0{c0.r * iw0, c0.g * iw0, c0.b * iw0};
    const rgb3 p1{c1.r * iw1, c1.g * iw1, c1.b * iw1};
    const rgb3 p2{c2.r * iw2, c2.g * iw2, c2.b * iw2};

    const float pu0 = v0.u * iw0, pv0 = v0.v * iw0;
    const float pu1 = v1.u * iw1, pv1 = v1.v * iw1;
    const float pu2 = v2.u * iw2, pv2 = v2.v * iw2;

    // ---- Lesson 3.8: the two varyings per-pixel shading needs ---------------
    //
    // Pre-divided by w exactly like the colour and the uvs, and for exactly the
    // same reason: what is affine in screen space is `a/w`, not `a`. Lesson 3.2's
    // derivation never mentioned what `a` was, which is why it covers a normal and
    // a position without a word of new argument.
    //
    // Hoisted out of the pixel loop and computed even when the fill is not lit,
    // because six multiplies per triangle is not worth a branch. The PER-PIXEL
    // work below is what is guarded.
    const vec3 pn0 = v0.normal * iw0, pn1 = v1.normal * iw1, pn2 = v2.normal * iw2;
    const vec3 pw0 = v0.world * iw0, pw1 = v1.world * iw1, pw2 = v2.world * iw2;

    // ---- LESSON 6.10: the screen-space uv gradients, once per triangle -------
    //
    // THE CPU'S REPLACEMENT FOR ddx/ddy. A GPU shades in 2x2 quads precisely so
    // a neighbouring fragment is always available to subtract; a scanline
    // rasterizer has no neighbour, so the derivative has to come from the
    // triangle itself.
    //
    // It is CONSTANT over the triangle and therefore hoisted: `U = sum(f_i*u_i/w_i)`
    // and `W = sum(f_i/w_i)` are both AFFINE in screen space, because the
    // normalised barycentrics are, so their gradients do not vary. Only the
    // quotient rule's per-pixel part (`(dU - u*dW) * w_recip`) is paid inside
    // the loop, and only when a chain is bound.
    //
    // df_i/dx is step_x_i * inv_area — the same edge steps the fill already
    // walks, divided by the same area it already reciprocated.
    const float dfx[3] = {static_cast<float>(s.step_x0) * inv_area,
                          static_cast<float>(s.step_x1) * inv_area,
                          static_cast<float>(s.step_x2) * inv_area};
    const float dfy[3] = {static_cast<float>(s.step_y0) * inv_area,
                          static_cast<float>(s.step_y1) * inv_area,
                          static_cast<float>(s.step_y2) * inv_area};

    const vec2 dnum_dx{dfx[0] * pu0 + dfx[1] * pu1 + dfx[2] * pu2,
                       dfx[0] * pv0 + dfx[1] * pv1 + dfx[2] * pv2};
    const vec2 dnum_dy{dfy[0] * pu0 + dfy[1] * pu1 + dfy[2] * pu2,
                       dfy[0] * pv0 + dfy[1] * pv1 + dfy[2] * pv2};
    const float dw_dx = dfx[0] * iw0 + dfx[1] * iw1 + dfx[2] * iw2;
    const float dw_dy = dfy[0] * iw0 + dfy[1] * iw1 + dfy[2] * iw2;

    // Decided once, like every other question constant across a fill.
    const bool mipped = style.albedo.mipped();

    // Lesson 6.7's third varying, pre-divided by exactly the same rule — the
    // derivation in §3.2 never said what `a` was, and it does not start caring
    // now. Only the `xyz` goes through the correction: `w` is a SIGN, constant
    // across a uv chart, and dividing a sign by `w` and multiplying it back is a
    // pair of operations whose only possible effect is float error.
    const vec3 pt0{v0.tangent.x * iw0, v0.tangent.y * iw0, v0.tangent.z * iw0};
    const vec3 pt1{v1.tangent.x * iw1, v1.tangent.y * iw1, v1.tangent.z * iw1};
    const vec3 pt2{v2.tangent.x * iw2, v2.tangent.y * iw2, v2.tangent.z * iw2};


    // ---- The fragment, as a function (Lesson 4.1) --------------------------
    //
    // Everything below was written inline in the pixel loop until Lesson 4.1
    // needed two different loops to share it. Look at what extracting it
    // revealed: a function from three barycentric weights to a colour, with every
    // piece of pipeline state captured. **That is a fragment shader.** The only
    // thing separating it from one is that the caller cannot supply it — which is
    // Module 4's opening argument, and the reason this refactor belongs to a
    // lesson rather than to a tidying commit.
    //
    // It is called for lanes the triangle does NOT cover (see the quad traversal
    // below), so it has to be total. Barycentric weights go negative outside the
    // triangle — Lesson 2.3 said so and called it useful — `w_recip` can come back
    // enormous or negative, and a uv can land anywhere at all. Nothing here reads
    // out of bounds on such a lane (`wrap_texel` folds any index into range,
    // `linear_to_srgb_u8` refuses a NaN), and whatever it computes is discarded.
    // ---- LESSON 6.12: the shading, hoisted out of the fragment ------------
    //
    // WHY THIS MOVED, and it is a structural point rather than tidiness. An HDR
    // target wants the shading equation's answer in LINEAR LIGHT — that is the
    // whole purpose of the buffer — while the 8-bit path wants it encoded. Those
    // are the same computation with a different last step, and leaving the
    // encode inside it would have meant either a second copy of a hundred lines
    // of shading or a decode of a value that had just been encoded.
    //
    // NOTE WHAT IS *NOT* HERE: `to_encoded`. This function is the one thing in
    // this engine that routinely exceeds 1.0 — a polished metal at the mirror
    // angle reaches 59,003, which is 15.8 stops above white (measured in
    // scratch/probe_612.cpp) — and until this lesson its answer was clamped one
    // line later, every time, with no way to ask for the real number.
    const auto shade_lit = [&](float f0, float f1, float f2, float w_recip,
                               float& out_alpha) -> linear_rgb {
        // ---- Per-pixel shading (Lesson 3.8) ----------------
        //
        // The whole lesson, in six lines. Interpolate the NORMAL
        // and the POSITION rather than the answer, then evaluate
        // the shading equation here — where the pixel is — instead
        // of at three corners and blending.
        //
        // The equation is byte-for-byte the one Lessons 3.6 and
        // 3.7 built. Nothing about the lighting changed; only
        // where it is called from. That is the distinction this
        // lesson exists to draw, and it is worth seeing that the
        // code says it too.
        const vec3 n{(f0 * pn0.x + f1 * pn1.x + f2 * pn2.x) * w_recip,
                     (f0 * pn0.y + f1 * pn1.y + f2 * pn2.y) * w_recip,
                     (f0 * pn0.z + f1 * pn1.z + f2 * pn2.z) * w_recip};
        const vec3 p{(f0 * pw0.x + f1 * pw1.x + f2 * pw2.x) * w_recip,
                     (f0 * pw0.y + f1 * pw1.y + f2 * pw2.y) * w_recip,
                     (f0 * pw0.z + f1 * pw1.z + f2 * pw2.z) * w_recip};

        // The albedo, from one of two places — Lesson 3.9.
        //
        // WITH A TEXTURE BOUND it is a sample, and `sample` already
        // returns linear light, so it drops straight into the same
        // slot with no conversion at all. That is not a coincidence
        // and it is worth pausing on: `sample` returns linear
        // *because* this is what a texture is for. An albedo is a
        // reflectance — the fraction of arriving light a surface
        // sends back — and a fraction has to multiply a quantity of
        // light, which means both have to be linear. Texture and
        // light multiply, and that one multiply is Module 3's last
        // structural gap closing.
        //
        // WITHOUT ONE it is the interpolated corner colour, by the
        // same three multiply-adds as any other attribute, exactly
        // as 3.8 left it. Note that it is taken in LINEAR light and
        // handed straight to `shade`, with no encode in between:
        // `blend_space` is ignored under `lit`, because lighting
        // arithmetic is linear by definition (conventions §7c) and
        // there is no meaning to give the encoded variant here. One
        // encode at the very end, which is where an encode belongs.
        // Written as one branch rather than "interpolate, then
        // overwrite if textured", because the interpolation is nine
        // multiply-adds and a discarded result is still a paid one.
        // `textured` is constant for the whole triangle, so the
        // predictor eats the branch itself for free — it is only
        // the WORK behind it that is worth not doing.
        linear_rgb albedo{};
        if (textured)
        {
            const float uu = (f0 * pu0 + f1 * pu1 + f2 * pu2) * w_recip;
            const float vv = (f0 * pv0 + f1 * pv1 + f2 * pv2) * w_recip;

            // LESSON 6.11. The albedo image's alpha is the CUTOUT — the
            // fourth channel of the same fetch, at no extra cost, which is
            // why an alpha test is nearly free on a surface that was already
            // being textured. Note it is taken from the ALBEDO map and
            // nowhere else: glTF says base colour carries the alpha, and a
            // normal map's fourth byte is padding somebody's exporter chose.
            const texel_sample s = mipped
                ? sample_mipped_rgba(*style.albedo.mips, style.albedo.samp,
                                     vec2{uu, vv},
                                     uv_gradients(dnum_dx, dnum_dy, dw_dx, dw_dy,
                                                  vec2{uu, vv}, w_recip))
                : sample_rgba(*style.albedo.image, style.albedo.samp, uu, vv);
            albedo = s.colour;
            out_alpha = style.opacity * s.alpha;
        }
        else
        {
            albedo = {(f0 * p0.r + f1 * p1.r + f2 * p2.r) * w_recip,
                      (f0 * p0.g + f1 * p1.g + f2 * p2.g) * w_recip,
                      (f0 * p0.b + f1 * p1.b + f2 * p2.b) * w_recip};
        }

        // ---- LESSON 6.7: THE NORMAL, PERTURBED --------------
        //
        // Everything above computed the normal the GEOMETRY has.
        // This replaces it with the normal the SURFACE has, read
        // out of an image — and the whole of the work is building
        // the frame that makes the image's numbers mean something.
        vec3 shading_normal = n;
        if (normal_mapped)
        {
            const float uu = (f0 * pu0 + f1 * pu1 + f2 * pu2) * w_recip;
            const float vv = (f0 * pv0 + f1 * pv1 + f2 * pv2) * w_recip;

            // THE MAP IS DATA, NOT COLOUR. `sample` returns
            // `texel/255` here rather than decoding through the
            // sRGB curve, because the texture was built with
            // `texel_space::linear` (6.7 §3). Through the wrong
            // space the flat value 0.5 comes back as 0.2140 and
            // every surface tilts toward its own steepest reading.
            const linear_rgb t = sample(*style.normal_map.image,
                                        style.normal_map.samp, uu, vv);

            // [0,1] -> [-1,1]. A direction has negative components
            // and a byte does not, so the encoding is an offset —
            // which is exactly why an unperturbed normal map is
            // LAVENDER: (0, 0, 1) stores as (0.5, 0.5, 1.0), and a
            // pale blue-violet is what "no change" looks like.
            const vec3 tn{t.r * 2.0f - 1.0f,
                          t.g * 2.0f - 1.0f,
                          t.b * 2.0f - 1.0f};

            // THE FRAME, REBUILT PER FRAGMENT. Both the normal and
            // the tangent were interpolated, so neither is unit
            // length and they are no longer perpendicular to each
            // other — interpolation does not preserve either
            // property. Gram-Schmidt fixes the second and
            // normalising fixes the first, in that order, and the
            // NORMAL is what we refuse to move: it is what the
            // shading is about, and the tangent only has to span
            // the plane.
            const vec3 nn = normalised_or(n, vec3{0.0f, 0.0f, 1.0f});
            const vec3 ti{(f0 * pt0.x + f1 * pt1.x + f2 * pt2.x) * w_recip,
                          (f0 * pt0.y + f1 * pt1.y + f2 * pt2.y) * w_recip,
                          (f0 * pt0.z + f1 * pt1.z + f2 * pt2.z) * w_recip};
            const vec3 tt = normalised_or(ti - nn * dot(nn, ti),
                                          vec3{1.0f, 0.0f, 0.0f});

            // The bitangent is COMPUTED, not stored — and the sign
            // is why the tangent is a vec4. A mirrored uv chart
            // needs the other one, and every symmetric model has a
            // mirrored chart.
            const float handed = (f0 * v0.tangent.w + f1 * v1.tangent.w
                                  + f2 * v2.tangent.w) >= 0.0f ? 1.0f : -1.0f;
            const vec3 bb = cross(nn, tt) * handed;

            // TANGENT SPACE -> WORLD, which is a matrix multiply
            // written as its own definition: a matrix IS where the
            // basis vectors land (Lesson 2.5), and these three ARE
            // the basis vectors. `tn.z` weights the normal, which
            // is why a flat map — z = 1, x = y = 0 — returns `nn`
            // exactly and changes nothing. verify_67 §D asserts
            // that round trip.
            shading_normal = tt * tn.x + bb * tn.y + nn * tn.z;
        }

        // ---- LESSON 6.8: CAN THIS POINT SEE THE LIGHT? -------
        //
        // Two things are worth reading twice here. The first
        // is that the normal handed to the shadow lookup is
        // `n`, the GEOMETRIC one, and not `shading_normal`:
        // shadow acne is a disagreement about where the
        // TRIANGLES are, and a normal map does not move a
        // triangle. 6.7 is what made those two different, and
        // this is the first line in the engine that has to
        // choose between them.
        //
        // The second is that this cosine is computed here
        // rather than taken from `shade`, and it is genuinely
        // a DIFFERENT cosine from the one that scales the
        // light: that one uses the shading normal, because it
        // asks how much light the surface receives; this one
        // uses the geometric normal, because it asks how
        // steeply the surface is tilted relative to the map's
        // texel grid. Sharing one value between them would be
        // shorter and would put a normal map's tilt into the
        // bias, which is a bias that varies per texel of an
        // image and has nothing to do with the geometry.
        float visibility = 1.0f;
        if (shadowed)
        {
            const vec3 gn = normalised_or(n, vec3{0.0f, 1.0f, 0.0f});
            const float geo_cos = dot(gn, style.lights->key.to_light());
            if (style.cascades != nullptr)
            {
                // AXIAL, not radial — raster.hpp says why, and the
                // difference is 22% at the corner of the frame.
                const float vd = dot(p - style.view_eye, style.view_forward);
                visibility = style.cascades->visibility(p, gn, geo_cos, vd);
            }
            else
            {
                visibility = style.shadows->visibility(p, gn, geo_cos);
            }
        }

        // `shade` normalises `n` itself — a decision made in 3.6
        // ("a caller who forgets gets a brightness scaled by the
        // normal's length, which looks like a lighting bug and is
        // not one"), and this is the call site that cashes it in.
        // The interpolated normal is genuinely short here, worst in
        // the middle of the triangle; §3.5 measures by how much.
        const linear_rgb direct = shade(albedo, shading_normal, style.eye - p,
                                        *style.lights, style.surface,
                                        style.model, ndf_model::ggx,
                                        visibility);

        // ---- LESSON 6.15: THE AMBIENT TERM, REPLACED ------------------------
        //
        // Null is the whole of "no environment", so the line above is untouched
        // on every path written before this lesson — which is why the reference
        // render is still byte-identical at hash E917C06C.
        //
        // When there IS an environment, the constant fill `shade()` added has
        // to be REMOVED before the directional one is added, or the surface
        // receives both. `ambient_only` recomputes exactly the term `shade()`
        // folded in (light.hpp's `albedo * ambient`, whose pi cancelled against
        // the hemisphere) so the subtraction is exact rather than approximate.
        // Passing a lighting struct with a zeroed ambient would be cheaper and
        // would also copy the struct per fragment; this is one multiply.
        if (style.env == nullptr) { return direct; }

        const linear_rgb removed = ambient_only(albedo, *style.lights, style.surface);
        const linear_rgb ibl = image_based_light(*style.env, style.surface, albedo,
                                                 shading_normal, style.eye - p);
        return {direct.r - removed.r + ibl.r * style.env_intensity,
                direct.g - removed.g + ibl.g * style.env_intensity,
                direct.b - removed.b + ibl.b * style.env_intensity};
    };

    // LESSON 6.11. `out_alpha` is an out-parameter rather than a second return
    // value, and the reason is the one this whole struct was built around: the
    // OPAQUE PATH MUST NOT MOVE. Returning a pair would have changed the call
    // site that nineteen lessons of measurements were taken through; an ignored
    // reference costs one store the optimiser deletes, and every colour
    // expression below is character for character what it was.
    //
    // It is set on EVERY path, including the ones that cannot be transparent,
    // because a fragment function that sometimes leaves an output untouched is a
    // fragment function whose caller has to know which times those are.
    const auto fragment = [&](float f0, float f1, float f2, float& out_alpha) -> Uint32 {
        // Coverage before colour. The default is the material's own opacity —
        // 1 for everything written before this lesson — and a texture with an
        // alpha channel MULTIPLIES it rather than replacing it (see
        // `material::alpha` for why that differs from the albedo's rule).
        out_alpha = style.opacity;

        // ---- Perspective correction, per pixel (Lesson 3.2) ----
        //
        // Interpolate 1/w with the same weights as everything else —
        // it is affine in screen space (Lesson 3.1 §3.4) — and its
        // reciprocal is the factor that turns every interpolated
        // `a/w` back into an `a`.
        //
        // ONE DIVIDE PER PIXEL, and it is the honest cost of this
        // lesson. It is paid once, not once per attribute, so it
        // amortises the moment a fragment carries more than one
        // thing — which from Lesson 3.6 onwards it always will.
        // In affine mode every iw is 1, so this sum is 1 and the
        // reciprocal is a no-op that we pay for anyway; keeping one
        // loop is worth more than saving a divide on a mode that
        // exists only to be shown failing.
        const float w_recip = 1.0f / (f0 * iw0 + f1 * iw1 + f2 * iw2);

        if (style.shade == shading::uv_checker)
        {
            const float uu = (f0 * pu0 + f1 * pu1 + f2 * pu2) * w_recip;
            const float vv = (f0 * pv0 + f1 * pv1 + f2 * pv2) * w_recip;
            return checker_at(uu, vv);
        }
        else if (style.shade == shading::textured)
        {
            // ---- The lookup (Lesson 3.9) ------------------------
            //
            // Compare these four lines against the four above them.
            // The uvs are obtained by IDENTICAL arithmetic — the same
            // three multiply-adds, the same perspective divide-back —
            // and the only difference is what the pair of numbers is
            // handed to: a formula, or an array somebody painted.
            //
            // That is the entire structural content of this lesson.
            // Nothing about interpolation changed, because nothing
            // needed to: 2.4 built a machine for carrying attributes
            // without knowing what they are, and this is the last
            // attribute Module 3 asks it to carry.
            const float uu = (f0 * pu0 + f1 * pu1 + f2 * pu2) * w_recip;
            const float vv = (f0 * pv0 + f1 * pv1 + f2 * pv2) * w_recip;
            // `to_encoded` is the largest single item in THIS branch —
            // three `pow` calls against one bilinear fetch — which is
            // Lesson 3.10's measured headline and the reason
            // `style.encode` exists.
            //
            // LESSON 6.11 CHANGED THE SPELLING AND NOT THE ARITHMETIC.
            // `sample_rgba` IS `sample` with the fourth number kept —
            // literally, since 6.11 made the three-channel entry point
            // a wrapper over this one — so the colour is bit-identical
            // and the coverage now arrives instead of being dropped on
            // the floor inside the sampler.
            const texel_sample s = mipped
                ? sample_mipped_rgba(*style.albedo.mips, style.albedo.samp,
                                     vec2{uu, vv},
                                     uv_gradients(dnum_dx, dnum_dy, dw_dx, dw_dy,
                                                  vec2{uu, vv}, w_recip))
                : sample_rgba(*style.albedo.image, style.albedo.samp, uu, vv);
            out_alpha = style.opacity * s.alpha;
            return to_encoded(s.colour, style.encode);
        }
        else if (lit)
        {
            // LESSON 6.12. One line, and the hundred it replaced are hoisted
            // above — see `shade_lit`. THE ENCODE IS HERE rather than in there,
            // because it is the only thing that distinguishes this path from the
            // HDR one, and putting it at the boundary is what lets both exist.
            return to_encoded(shade_lit(f0, f1, f2, w_recip, out_alpha),
                              style.encode);
        }
        else
        {
            // Three multiply-adds per channel, then the divide-back.
            // This is the interpolation, and it is the same three
            // lines whatever the attribute turns out to be — texture
            // coordinates just above, normals in 3.6. The rasterizer
            // never learns what it carries.
            const rgb3 mixed{(f0 * p0.r + f1 * p1.r + f2 * p2.r) * w_recip,
                             (f0 * p0.g + f1 * p1.g + f2 * p2.g) * w_recip,
                             (f0 * p0.b + f1 * p1.b + f2 * p2.b) * w_recip};

            // One branch per pixel on a value that is constant for
            // the whole triangle. A branch predictor eats this for
            // free; hoisting it would mean two copies of the loop,
            // which is a worse trade at this size.
            return pixel_from(mixed, space, style.encode);
        }
    };

    // ---- The depth test, also as a function --------------------------------
    //
    // Shared by both traversals, because two copies of a rule are two rules.
    // Returns whether the fragment survives and writes the new depth if it does,
    // so it must only be called for lanes that are genuinely covered.
    const auto depth_test = [&](float* zrow, int x, float f0, float f1, float f2) -> bool {
        // With no attachment bound not one instruction of depth work happens —
        // the 2-D fills of Lessons 2.2-2.4 pay nothing for a feature they do not
        // use.
        if (zrow == nullptr) { return true; }

        // NOTE the asymmetry with the fragment above: `z` is interpolated
        // DIRECTLY, with no perspective correction, because the projection has
        // already made device depth an affine function of the pixel position, so
        // barycentric interpolation of it is the exact answer. Correcting it here
        // would be actively wrong — and it is the single easiest mistake to make
        // once you have learnt Lesson 3.2's trick and start applying it
        // everywhere. Depth is the one attribute that arrives pre-corrected.
        // (Lesson 3.1 §3.4 derives it; Lesson 3.2 §3.5 has the asymmetry.)
        //
        // Quantise BEFORE comparing, because that is the order the hardware uses:
        // the fragment's depth is converted to the attachment's format, then
        // tested against a value already in it. For a full-precision buffer this
        // is the identity.
        const float z = depth->quantise(f0 * v0.z + f1 * v1.z + f2 * v2.z);

        // Smaller is nearer. `<` and not `<=`: on a tie the pixel already there
        // keeps it, so a surface drawn twice does not flicker between two
        // identical answers, and coplanar geometry has one stable winner — the
        // first one drawn.
        if (!(z < zrow[x])) { return false; }
        zrow[x] = z;
        return true;
    };

    // ---- LESSON 6.12: the same fragment, un-encoded ------------------------
    //
    // The HDR path's fragment. It is deliberately NOT a second implementation:
    // the `lit` case calls the same `shade_lit` the 8-bit path calls and simply
    // does not encode it, and every other case goes through `fragment` and
    // decodes.
    //
    // THAT DECODE IS EXACT ENOUGH, AND IT IS WORTH SAYING WHY RATHER THAN
    // HOPING. The other shading modes cannot exceed 1.0 by construction — a uv
    // checker is two constants, a texture sample is a reflectance, and an
    // interpolated vertex colour is a weighted average of three values in [0,1].
    // So the round trip loses at most the 8-bit encode's own rounding, on values
    // that had nowhere else to go, and it buys one implementation of four
    // shading modes instead of two. `lit` is the only mode with a range to
    // preserve, and it is the one that skips the trip.
    const auto fragment_linear = [&](float f0, float f1, float f2, float& out_alpha) -> linear_rgb {
        if (lit)
        {
            const float w_recip = 1.0f / (f0 * iw0 + f1 * iw1 + f2 * iw2);
            out_alpha = style.opacity;
            return shade_lit(f0, f1, f2, w_recip, out_alpha);
        }
        return to_linear(fragment(f0, f1, f2, out_alpha));
    };

    // ---- LESSON 6.11: the depth test WITHOUT the write ---------------------
    //
    // A blended fragment has to ask the depth buffer the same question and must
    // not answer it. Separating the two halves of `depth_test` is the whole of
    // what "disable depth writes" means, and it is worth seeing that it is one
    // missing line rather than a mode:
    //
    //   A blended surface is visible THROUGH, so it does not occlude. Write its
    //   depth and the next transparent surface behind it fails the test and
    //   vanishes — which is the classic symptom of forgetting this, and it looks
    //   like the far pane of glass "disappearing when you look through the near
    //   one", not like a depth bug.
    //
    // It still TESTS, and that half is not optional: transparent geometry behind
    // a wall is behind the wall.
    const auto depth_probe = [&](const float* zrow, int x, float f0, float f1, float f2) -> bool {
        if (zrow == nullptr) { return true; }
        const float z = depth->quantise(f0 * v0.z + f1 * v1.z + f2 * v2.z);
        return z < zrow[x];
    };

    // ---- LESSON 6.11: one covered pixel, under mask or blend ---------------
    //
    // Both non-opaque modes in one place, because they share the property that
    // forced this function to exist: THE FRAGMENT RUNS BEFORE THE DEPTH BUFFER
    // IS COMMITTED TO. In the opaque path a fragment that fails the depth test
    // is never shaded, which is the single largest saving in the rasterizer.
    // Here it is shaded anyway — masked geometry because its alpha decides
    // whether it exists at all, blended geometry because it never writes depth
    // and so has nothing to commit.
    //
    // That reordering is a real cost and §7 measures it. It is also exactly why
    // hardware disables early-Z for shaders containing `discard`: the same
    // dependency, in silicon.
    // THE RULE ITSELF, spelt once and shared by both traversals — because two
    // copies of a rule are two rules, and this one has an ordering constraint in
    // it that nobody wants to have written down twice.
    const auto commit_transparent = [&](Uint32* row, linear_rgb* hdr_row,
                                        float* zrow, int x,
                                        float f0, float f1, float f2,
                                        Uint32 colour, linear_rgb lin, float a) {
        if (masked)
        {
            // THE TEST, and it is a comparison rather than a blend: below the
            // cutoff there is no fragment at all. Note what does NOT happen when
            // it passes — the alpha is thrown away, and the fragment is written
            // fully opaque. That is what makes masking cheap and order-free: the
            // result is an ordinary opaque pixel that happens to have a hole
            // punched somewhere else.
            if (a < style.alpha_cutoff) { return; }

            // NOW the depth write, and the order is the lesson. A discarded
            // fragment must leave the depth buffer alone, or the hole in the
            // leaf would occlude whatever is behind it — a leaf-shaped patch of
            // background, punched out of the tree behind it, which is the
            // unmistakable signature of testing alpha after writing depth.
            if (zrow != nullptr)
            {
                zrow[x] = depth->quantise(f0 * v0.z + f1 * v1.z + f2 * v2.z);
            }
            if (hdr_row != nullptr) { hdr_row[x] = lin; } else { row[x] = colour; }
            return;
        }

        // ---- BLENDING ------------------------------------------------------
        //
        // A read-modify-write, which is the property that makes it expensive and
        // the property that makes it order-dependent. `row[x]` is read, and what
        // is in there depends on everything drawn before — so the answer depends
        // on the draw order, and `draw_order.hpp` exists.
        //
        // NO DEPTH WRITE. Not "a depth write we could skip": the absence is the
        // feature. See `depth_probe` above.
        //
        // `blend_over` decodes both operands, composites in linear light and
        // re-encodes. `blend_encoded` summons the wrong version instead — the
        // one that lerps stored bytes — because a failure you can switch on
        // teaches more than a paragraph, and this particular failure is worth
        // 43% of the light at half coverage.
        // ---- LESSON 6.12: WHAT AN HDR TARGET DOES TO 6.11'S RULE -----------
        //
        // Lesson 6.11 spent a section establishing that compositing must decode
        // both operands, blend in linear light, and re-encode — and measured the
        // round trip at 3.14x a naive byte lerp. On a float target THE ROUND TRIP
        // IS NOT THERE AT ALL. The destination is already linear light and so is
        // the source, so `over` applies directly, which is both faster and
        // exactly correct.
        //
        // THAT IS THE CLEANEST STATEMENT OF WHAT AN HDR BUFFER IS FOR: not
        // "brighter pixels", but "the buffer holds the quantity the arithmetic is
        // defined on". Every conversion 6.11 had to perform was a symptom of
        // storing something other than light.
        //
        // And note that 6.11's `blend_encoded` failure has nowhere to live here:
        // there are no stored codes to lerp. A knob that cannot express its
        // mistake on this path is not silently ignored — the mistake genuinely
        // does not exist.
        if (hdr_row != nullptr)
        {
            hdr_row[x] = (style.src_storage == alpha_storage::premultiplied)
                       ? over_premultiplied(lin, a, hdr_row[x])
                       : over(lin, a, hdr_row[x]);
            return;
        }

        row[x] = style.blend_encoded
               ? blend_over_encoded(row[x], colour, a)
               : blend_over(row[x], colour, a, style.encode, style.src_storage);
    };

    /// Probe, shade, commit — the scanline traversal's whole transparent path.
    const auto shade_transparent = [&](Uint32* row, linear_rgb* hdr_row,
                                       float* zrow, int x,
                                       float f0, float f1, float f2) {
        // Depth first as a REJECTION, not a commitment — this reads the buffer
        // and writes nothing. A fragment behind an opaque surface is invisible
        // whatever its alpha, so this saves the shading in the common case
        // without changing anybody's depth.
        if (!depth_probe(zrow, x, f0, f1, f2)) { return; }

        float a = 1.0f;
        linear_rgb lin{};
        Uint32 colour = 0u;
        if (hdr_row != nullptr) { lin = fragment_linear(f0, f1, f2, a); }
        else                    { colour = fragment(f0, f1, f2, a); }
        commit_transparent(row, hdr_row, zrow, x, f0, f1, f2, colour, lin, a);
    };

    // **Unbias, then divide** — needed by both traversals and spelt once. The
    // accumulators carry the top-left rule's -1 on any edge that is not
    // top-or-left; that -1 is a statement about who owns a boundary pixel, and it
    // is not a statement about where the pixel is. Left in, it displaces the whole
    // attribute field by 1/edge_length of a pixel and stops the three weights
    // summing to 1. Taking it back out is one integer subtraction against a value
    // that is *exact* — these are the stepped integers, so there is no accumulated
    // drift to undo, only a known constant. Lesson 2.4 §3.5.
    const auto weights = [&](int w0, int w1, int w2, float& f0, float& f1, float& f2) {
        f0 = static_cast<float>(w0 - s.bias0) * inv_area;
        f1 = static_cast<float>(w1 - s.bias1) * inv_area;
        f2 = static_cast<float>(w2 - s.bias2) * inv_area;
    };

    if (style.traverse == traversal::scanline)
    {
        // ---- One pixel at a time (Lessons 2.2 - 3.10) ----------------------
        //
        // Shades exactly the pixels the triangle covers, which is the right thing
        // for a CPU to do and is what every measurement before Lesson 4.1 was
        // taken against.
        int row_w0 = s.row_w0;
        int row_w1 = s.row_w1;
        int row_w2 = s.row_w2;

        for (int y = s.min_y; y <= s.max_y; ++y)
        {
            int w0 = row_w0;
            int w1 = row_w1;
            int w2 = row_w2;

            // The whole row is in bounds by construction, so this takes the
            // documented fast path from Lesson 1.5 and skips put_pixel's
            // per-pixel bounds check and index multiply.
            Uint32* const row = fb.row(y);

            // LESSON 6.12. The float target's matching row, or null. Hoisted for
            // the same reason the colour and depth rows are: the row index does
            // not change across a scanline.
            linear_rgb* const hdr_row = to_hdr ? style.hdr->row(y) : nullptr;

            // The matching row of the depth attachment, or nullptr when there is
            // no attachment. Hoisted for the same reason the colour row is: the
            // row index does not change across a scanline, so resolving it per
            // pixel would be paying for an answer we already have.
            float* const zrow = (depth != nullptr) ? depth->row(y) : nullptr;

            for (int x = s.min_x; x <= s.max_x; ++x)
            {
                // Inside all three half-planes at once. That is the entire test,
                // and it is the same three numbers for every pixel — only their
                // values change.
                if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                {
                    float f0, f1, f2;
                    weights(w0, w1, w2, f0, f1, f2);

                    // LESSON 6.11. ONE branch, on a value constant for the whole
                    // triangle, standing in front of code that is otherwise
                    // exactly what 6.10 left behind. Everything transparency
                    // costs is on the other side of it.
                    if (simple)
                    {
                        if (depth_test(zrow, x, f0, f1, f2))
                        {
                            // LESSON 6.8. `depth_only` skips the fragment, not
                            // the store — the store is one word and the fragment
                            // is the whole shading equation. A shadow pass takes
                            // this branch on every pixel it covers.
                            //
                            // LESSON 6.12 adds the second branch and nothing
                            // else: the same fragment, stored un-encoded into a
                            // wider target.
                            float ignored;
                            if (depth_only) { /* nothing to store */ }
                            else if (hdr_row != nullptr)
                            {
                                hdr_row[x] = fragment_linear(f0, f1, f2, ignored);
                            }
                            else { row[x] = fragment(f0, f1, f2, ignored); }
                        }
                    }
                    else
                    {
                        shade_transparent(row, hdr_row, zrow, x, f0, f1, f2);
                    }
                }

                w0 += s.step_x0;
                w1 += s.step_x1;
                w2 += s.step_x2;
            }

            row_w0 += s.step_y0;
            row_w1 += s.step_y1;
            row_w2 += s.step_y2;
        }
        return;
    }

    // ---- 2x2 quads, the way hardware does it (Lesson 4.1) ------------------
    //
    // Three things change, and only the first is visible in the output:
    //
    //   1. NOTHING. The image is bit-identical to the loop above. Helper lanes
    //      are shaded and thrown away; they never write colour and never write
    //      depth. `verify_41` §B checks this over a 2,304-triangle scene.
    //   2. The fragment function runs for lanes the triangle does not cover.
    //   3. Those lanes are counted, and the count is the lesson.
    //
    // QUADS ARE ALIGNED TO EVEN COORDINATES IN THE RENDER TARGET, not to the
    // triangle. That is not a detail: it is why a triangle cannot arrange to be
    // cheap by being positioned well, and why the waste depends on a triangle's
    // size and perimeter rather than on where it happens to land.
    const int qx0 = s.min_x & ~1;
    const int qy0 = s.min_y & ~1;

    // Edge values at the aligned corner. Stepping back from `s.min_x/min_y` to
    // the even boundary is exact, because an edge function is affine and the step
    // is a constant — the same fact the row stepping rests on.
    int quad_w0 = s.row_w0 + (qx0 - s.min_x) * s.step_x0 + (qy0 - s.min_y) * s.step_y0;
    int quad_w1 = s.row_w1 + (qx0 - s.min_x) * s.step_x1 + (qy0 - s.min_y) * s.step_y1;
    int quad_w2 = s.row_w2 + (qx0 - s.min_x) * s.step_x2 + (qy0 - s.min_y) * s.step_y2;

    const bool show_helpers = (style.traverse == traversal::quad_debug);
    quad_stats local{};

    for (int qy = qy0; qy <= s.max_y; qy += 2)
    {
        int lane_w0 = quad_w0;
        int lane_w1 = quad_w1;
        int lane_w2 = quad_w2;

        for (int qx = qx0; qx <= s.max_x; qx += 2)
        {
            // The four lanes' edge values, derived by adding one step in x, one
            // in y, and both. Lane order is the usual one: top-left, top-right,
            // bottom-left, bottom-right — which is also why `ddx` is lane 1 minus
            // lane 0 and `ddy` is lane 2 minus lane 0.
            const int lw0[4] = {lane_w0, lane_w0 + s.step_x0,
                                lane_w0 + s.step_y0, lane_w0 + s.step_x0 + s.step_y0};
            const int lw1[4] = {lane_w1, lane_w1 + s.step_x1,
                                lane_w1 + s.step_y1, lane_w1 + s.step_x1 + s.step_y1};
            const int lw2[4] = {lane_w2, lane_w2 + s.step_x2,
                                lane_w2 + s.step_y2, lane_w2 + s.step_x2 + s.step_y2};

            bool covered[4];
            bool any = false;
            for (int i = 0; i < 4; ++i)
            {
                covered[i] = (lw0[i] >= 0 && lw1[i] >= 0 && lw2[i] >= 0);
                any = any || covered[i];
            }

            // THE ONE EARLY-OUT HARDWARE HAS. A quad with no covered lane is
            // never issued, which is what keeps the cost proportional to the
            // triangle rather than to its bounding box. Everything past this
            // point is paid for.
            if (any)
            {
                ++local.quads;

                for (int i = 0; i < 4; ++i)
                {
                    const int lx = qx + (i & 1);
                    const int ly = qy + (i >> 1);

                    // Outside the render target. Real hardware clips
                    // rasterization to the target as well, so these lanes do not
                    // run; counted separately because they are not waste, they
                    // are simply absent.
                    if (lx >= fb.width() || ly >= fb.height())
                    {
                        ++local.off_target;
                        continue;
                    }

                    float f0, f1, f2;
                    weights(lw0[i], lw1[i], lw2[i], f0, f1, f2);

                    // THE DEPTH TEST IS FOR COVERED LANES ONLY. A helper lane is
                    // not on the surface, so it must not write depth — and this
                    // is the line that keeps the output bit-identical.
                    float* const zrow = (depth != nullptr) ? depth->row(ly) : nullptr;

                    // LESSON 6.11. Under mask or blend the depth buffer must not
                    // be committed to before the fragment has run, so this lane
                    // PROBES instead of testing-and-writing, and the commit
                    // happens below with the alpha in hand. The opaque path is
                    // unchanged, which is what keeps 4.1's bit-identical claim
                    // between the two traversals true.
                    const bool visible = covered[i]
                        && (simple ? depth_test(zrow, lx, f0, f1, f2)
                                   : depth_probe(zrow, lx, f0, f1, f2));

                    // AND HERE IS THE WHOLE LESSON. The fragment runs whether or
                    // not this lane is covered, because its neighbours may need to
                    // difference against it. Moving this call inside the
                    // `if (visible)` below would make the quad traversal cheap and
                    // would stop it modelling anything at all.
                    float lane_alpha = 1.0f;
                    linear_rgb lane_lin{};
                    Uint32 colour = 0u;
                    if (!depth_only)
                    {
                        if (to_hdr) { lane_lin = fragment_linear(f0, f1, f2, lane_alpha); }
                        else        { colour = fragment(f0, f1, f2, lane_alpha); }
                        ++local.shaded;
                    }

                    if (covered[i]) { ++local.covered; } else { ++local.helpers; }

                    linear_rgb* const hdr_row = to_hdr ? style.hdr->row(ly) : nullptr;

                    if (visible && !depth_only)
                    {
                        if (simple)
                        {
                            if (hdr_row != nullptr) { hdr_row[lx] = lane_lin; }
                            else                    { fb.row(ly)[lx] = colour; }
                        }
                        else
                        {
                            commit_transparent(fb.row(ly), hdr_row, zrow, lx,
                                               f0, f1, f2, colour, lane_lin, lane_alpha);
                        }
                    }
                    else if (show_helpers && !covered[i])
                    {
                        // Not a rendering mode — a picture of the waste. Magenta
                        // is this engine's "this value is not real" convention
                        // (`checker_at` in 3.2, an unbound sampler in 3.9), used
                        // for a third time and for the same reason: no real
                        // surface is ever this colour by accident.
                        fb.row(ly)[lx] = pack_argb(255, 0, 255);
                    }
                }
            }

            lane_w0 += 2 * s.step_x0;
            lane_w1 += 2 * s.step_x1;
            lane_w2 += 2 * s.step_x2;
        }

        quad_w0 += 2 * s.step_y0;
        quad_w1 += 2 * s.step_y1;
        quad_w2 += 2 * s.step_y2;
    }

    // ACCUMULATE, never assign: one `quad_stats` totals a whole draw, and a
    // per-triangle lane efficiency is not a number anybody wants.
    if (stats != nullptr) { *stats += local; }
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
