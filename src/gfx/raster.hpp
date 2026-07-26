// src/gfx/raster.hpp — turning shapes into pixels.
//
// The first file of the software rasterizer, and the beginning of Module 2. A
// framebuffer knows how to set one pixel; this knows which pixels a shape is
// made of. Everything from here to the end of Module 3 — triangles, depth,
// texture — is that same question asked about a more interesting shape.
//
// Three line routines live here, and only one of them is meant to be used. The
// other two are kept because Lesson 2.1 is an argument, and an argument needs
// something to compare against.

#pragma once

#include "gfx/colour.hpp"   // linear_rgb: the space vertex colours are combined in

#include <SDL3/SDL.h>

// Forward declarations, not includes: every function below takes a framebuffer
// (and, from Lesson 3.1, a depth buffer) by reference or pointer, and none of
// them needs the layout of either. raster.cpp includes the real headers because
// it actually writes pixels and depths. The habit is from Lesson 1.8 §4.1 — in a
// header, include what you use and forward declare what you merely mention.
namespace engine { class framebuffer; }
namespace engine { class depth_buffer; }

namespace engine {

/// Draw a line between two pixel centres. **This is the one to call.**
///
/// Integer Bresenham, valid in all eight octants. The pixels chosen are exactly
/// those a midpoint decision would choose (Lesson 2.1 §3.3), computed with no
/// floating point at all: for a given four integers the output is exact,
/// repeatable, and free of any accumulated drift.
///
/// Endpoints are inclusive — both `(x0,y0)` and `(x1,y1)` are lit — so a line
/// from a point to itself lights that one pixel. Coordinates outside the
/// framebuffer are dropped by put_pixel rather than clipped away up front; see
/// the note in raster.cpp about what that costs and when it will need fixing.
void draw_line(framebuffer& fb, int x0, int y0, int x1, int y1, Uint32 colour);

/// The same line, by DDA: step the major axis by one and accumulate the minor
/// axis in a float.
///
/// **Kept for comparison, not for use.** It is the algorithm most people write
/// first, it is correct, and Lesson 2.1 measures it against draw_line rather
/// than asserting which is better. Its real defect is not speed but exactness:
/// the position is a running float sum, so the pixels it picks depend on the
/// order the additions happened in.
void draw_line_dda(framebuffer& fb, int x0, int y0, int x1, int y1, Uint32 colour);

/// The broken one: walk x from x0 to x1 and evaluate y = mx + b at each step.
///
/// **Kept so it can be seen failing.** It is the most obvious possible reading
/// of "a line is y = mx + b", and it produces a dotted line rather than a solid
/// one whenever the line is steeper than 45°, because y then changes by more
/// than one pixel per column. Lesson 2.1 §1 shows the gaps before explaining
/// them. Never call this for real work.
void draw_line_naive(framebuffer& fb, int x0, int y0, int x1, int y1, Uint32 colour);

// ---- Triangles --------------------------------------------------------------

/// Which side of the directed line A→B does P lie on?
///
/// This is the z component of the 2-D cross product `(B−A) × (P−A)`, and it is
/// the single most useful number in rasterization. Two readings, both exact:
///
///   **Sign** — which side. Zero means P is exactly on the line.
///   **Magnitude** — twice the area of triangle ABP.
///
/// The sign convention is worth stating precisely, because getting it backwards
/// is invisible until something is culled that should not be. In **framebuffer
/// coordinates, where +y points down**, a triangle whose vertices appear
/// **counter-clockwise on screen** has a **negative** value here. That is not a
/// quirk of this formula — it is the y-flip: the same vertices in a y-up space
/// give the opposite sign. Lesson 2.11's viewport transform is where the flip
/// formally happens, and Lesson 3.4 is where the sign starts deciding whether a
/// face is visible. Until then `fill_triangle` accepts either winding.
///
/// Integer, and therefore exact — no accumulation, no rounding, identical on
/// every machine (the argument from Lesson 2.1 §3.7, applied to a second thing).
///
/// **Range.** The products overflow a 32-bit int if coordinates exceed roughly
/// ±16000. Every framebuffer in this course is far inside that; a renderer that
/// clips before rasterising, which Module 3 builds, keeps it that way.
[[nodiscard]] constexpr int edge_function(int ax, int ay, int bx, int by, int px, int py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

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
///
/// This was private to raster.cpp until Lesson 2.4. It is public now because the
/// bias it produces is no longer only a coverage decision: 2.4 has to *undo* it
/// before interpolating, and a rule you cannot inspect is a rule you cannot
/// check. The demo reproduces the fill loop with and without the bias, and it
/// must ask the same question the engine asks, not a re-typed approximation.
[[nodiscard]] constexpr bool is_top_left(int ax, int ay, int bx, int by)
{
    const int dx = bx - ax;
    const int dy = by - ay;
    return (dy == 0 && dx > 0) || (dy < 0);
}

/// Where a point sits inside a triangle, as three fractions that sum to 1.
///
/// These are **barycentric coordinates**, and they are the triangle's own
/// coordinate system: `w0` is 1 at vertex 0 and 0 along the opposite edge, and
/// likewise for the other two. They say *where* in a triangle a point is without
/// reference to x or y at all, which is what lets any quantity known at the
/// corners be carried across the interior (Lesson 2.4 onwards: colour, then
/// depth, then texture coordinates, then normals).
struct barycentric
{
    float w0 = 0.0f;   ///< weight of vertex 0 — 1 at v0, 0 on the edge v1→v2
    float w1 = 0.0f;
    float w2 = 0.0f;
};

/// The barycentric coordinates of `(px, py)` with respect to the triangle.
///
/// Each weight is the area of the sub-triangle **opposite** its vertex, divided
/// by the whole. That pairing is the one thing to get right and the easiest
/// thing to get wrong: `w0` uses the edge `v1→v2`, which is the edge that does
/// *not* touch `v0`. Pair them the other way and the weights still sum to 1 and
/// still look plausible — they simply describe a different point. Lesson 2.3 §3.5.
///
/// Weights are **negative outside** the triangle, and that is useful rather than
/// a failure: the same formula extends smoothly over the whole plane, which is
/// exactly why it can be used to extrapolate as well as interpolate.
///
/// **Degenerate triangles.** Three collinear points have zero area and no
/// interior, so there is no answer; this returns all zeros, which is the one
/// case where the weights do not sum to 1. Check the area yourself if your
/// geometry can produce slivers.
[[nodiscard]] barycentric barycentric_at(int x0, int y0, int x1, int y1, int x2, int y2,
                                         int px, int py);

/// Fill a triangle, given three corners in pixel coordinates.
///
/// A pixel is inside when it is on the same side of all three edges — three
/// half-planes intersected, which is what a triangle *is*. Accepts either
/// winding: the sign of the total area is measured once up front and the test
/// is oriented to match, so a "backwards" triangle fills rather than vanishing.
/// A degenerate triangle (three collinear points, zero area) draws nothing.
///
/// Boundary pixels follow the **top-left rule**, so two triangles sharing an
/// edge cover every pixel of that edge exactly once — no cracks, no
/// double-drawing. Lesson 2.2 §3.6 derives it, and the demo shows what happens
/// without it. This is what fixes the asymmetry Lesson 2.1 §3.6 found in
/// `draw_line`: a fill rule depends only on an edge's geometry, never on which
/// direction it happens to be traversed.
void fill_triangle(framebuffer& fb,
                   int x0, int y0, int x1, int y1, int x2, int y2,
                   Uint32 colour);

// ---- Attributes: things a vertex knows and the interior does not -------------

/// A triangle corner as the rasterizer sees it: a pixel position, plus whatever
/// that corner happens to know.
///
/// Bundling the position with the attributes is not tidiness, it is
/// correctness. `fill_triangle` reorients a backwards triangle by swapping two
/// vertices, and every attribute must move with the position it belongs to. Six
/// loose ints and three loose colours make it possible — easy, in fact — to swap
/// the coordinates and forget the colours, which produces a triangle whose
/// shading is rotated by one corner and whose geometry is perfect. One `struct`
/// and one `std::swap` make that bug unwritable. Lesson 2.4 §4.2.
///
/// Lesson 2.4 predicted that Module 3 would add depth, texture coordinates and
/// normals here, "each one three more lines in the same loop". Lesson 3.1
/// collected on `z`; Lesson 3.2 collects on `u`, `v` — and on `inv_w`, which is
/// the price of carrying them correctly.
struct vertex
{
    int x = 0;                       ///< pixel column
    int y = 0;                       ///< pixel row

    /// **Device depth** in `[0, 1]` — 0 at the near plane, 1 at the far plane.
    ///
    /// Specifically the third component of `viewport::to_screen` (Lesson 2.11),
    /// which has been computed and discarded ever since for want of a depth
    /// buffer to put it in. Note what it is *not*: it is not the view-space `z`,
    /// and the difference is not cosmetic. Device depth is the value that
    /// interpolates **correctly** across a triangle in screen space, because the
    /// projective divide has already made it an affine function of the pixel
    /// position; view-space `z` is a reciprocal of one and interpolating it
    /// linearly is simply wrong. Lesson 3.1 §3.4 derives this, and Lesson 3.2
    /// generalises the same fact into perspective-correct interpolation for
    /// every *other* attribute.
    ///
    /// Defaults to 0 — nearest — so a purely 2-D vertex passed to a depth-tested
    /// fill behaves like an overlay that always wins, which is the sensible thing
    /// for something with no depth to speak of.
    float z = 0.0f;

    /// **The reciprocal of the clip-space `w`** at this corner — that is,
    /// `1 / (-z_view)` under a perspective projection.
    ///
    /// This is the one number that makes every *other* attribute interpolate
    /// correctly, and Lesson 3.2 derives why: `1/w` is affine in screen space
    /// (Lesson 3.1 §3.4), and so is `a/w` for any attribute `a` that is linear
    /// over the surface. Interpolate both, divide at the end, and you get `a`.
    ///
    /// Stored **pre-divided**, not as `w`, for two reasons. The inner loop wants
    /// `1/w` — it interpolates it directly — so storing `w` would mean a divide
    /// per vertex *and* the same divide again inside the loop. And the final
    /// "divide back" becomes a multiply by the reciprocal we already had to
    /// compute.
    ///
    /// **Defaults to 1**, which is exactly right: `w = 1` is the orthographic /
    /// 2-D case, where the correction is the identity and affine interpolation is
    /// already correct. Nothing written before Lesson 3.2 needs updating, and
    /// nothing written before it was wrong.
    float inv_w = 1.0f;

    float u = 0.0f;                  ///< texture coordinate; 3.9 gives it texels
    float v = 0.0f;

    Uint32 colour = 0xFFFFFFFFu;     ///< ARGB8888 as stored — i.e. sRGB-encoded
};

/// Whether attributes are corrected for perspective.
///
/// Like `blend_space` and `draw_line_naive`, the wrong one is kept so it can be
/// summoned rather than described. This is the fifth such bargain in the engine.
enum class interpolation
{
    /// Interpolate attributes directly from the barycentric weights, ignoring
    /// `w`. **Wrong** on any surface that is not parallel to the screen, and
    /// wrong in the specific, famous way Lesson 3.2 opens with: textures that
    /// swim and buckle, with the error largest along a triangle's longest run in
    /// depth and a visible break along the diagonal a quad is split on.
    ///
    /// It is not *arbitrarily* wrong. It is exactly what you get if you believe
    /// `w = 1` everywhere — which is to say, it is a perspective renderer doing
    /// orthographic interpolation.
    affine,

    /// Interpolate `a/w` and `1/w`, then divide. **The correct one**, and the
    /// default. Costs one divide per pixel, which is the honest headline: depth
    /// came free in Lesson 3.1, and this does not.
    perspective
};

/// How a covered pixel turns its interpolated attributes into a colour.
///
/// This enum is a **placeholder for a fragment shader**, and saying so now is
/// cheaper than pretending otherwise later. A real renderer lets the caller supply
/// the function; Module 4 does exactly that, and calls it a fragment shader.
/// Lesson 3.6 will add a lighting term and this enum will start to strain, which
/// is the point at which the right structure will have earned itself.
enum class shading
{
    /// The barycentric blend of the three corner colours — Gouraud, from
    /// Lesson 2.4.
    vertex_colour,

    /// A procedural checkerboard evaluated at `(u, v)`, one cell per unit.
    ///
    /// A **debug pattern**, not a texture: no image, no sampler, no filtering.
    /// Every engine ships something like it, because it is how you check a mesh's
    /// uv layout before there is any art to put on it. Lesson 3.9 replaces the
    /// rule with a lookup, and nothing else about the loop changes.
    uv_checker
};

/// Which space three vertex colours are combined in.
///
/// This is a real choice with a visibly different answer, not a tuning knob.
/// See Lesson 2.4 §3.4: the centre of a red/green/blue triangle comes out as
/// mid-grey 156 in linear light and as a murky 85 in encoded space, because
/// averaging stored values does not average light.
enum class blend_space
{
    /// Decode to light, interpolate, re-encode. **The correct one**, and the
    /// default. What Gouraud shading has always meant.
    linear,

    /// Interpolate the stored values directly. **Wrong**, and kept anyway — the
    /// same bargain as `draw_line_naive` and Pong's naive collision test. A
    /// failure you can summon with one keystroke teaches more than a paragraph
    /// describing it, and this particular failure is responsible for the muddy
    /// band down the middle of a great many gradients.
    encoded
};

/// The knobs a fill runs under, gathered into one object.
///
/// Three booleans-worth of state used to be three trailing parameters, and a
/// fourth was about to arrive. Gathering them is not only tidiness: it is the
/// shape the hardware uses. A GPU does not take render state as call arguments —
/// it bakes it into a **pipeline object** created once and bound before drawing,
/// because validating and compiling that state per draw call would be ruinous.
/// `SDL_GPUGraphicsPipelineCreateInfo` is exactly this struct, several times
/// larger. Module 4 §2 makes the argument properly; this is a first taste, and it
/// means adding a knob in 3.6 costs one field rather than one more parameter at
/// every call site.
///
/// Every field has the *correct* value as its default, so `fill_triangle(fb, a, b, c)`
/// is right, and every deliberately-wrong mode has to be asked for by name.
struct fill_style
{
    interpolation interp = interpolation::perspective;
    shading shade = shading::vertex_colour;
    blend_space space = blend_space::linear;
};

/// Fill a triangle whose corners carry their own attributes — the shaded fill.
///
/// Every interior pixel gets the barycentric-weighted average of the three
/// corners, which is the unique affine function agreeing with them (Lesson 2.3
/// §3.7). Three details that are easy to get wrong and invisible when you do:
///
///   - the weights used here are **unbiased**. The top-left rule's `-1` decides
///     *coverage*, and feeding it into an attribute displaces the whole field by
///     `1 / edge_length` pixels and breaks the sum-to-one identity. Lesson 2.4
///     §3.5.
///   - the blend happens in **linear light** by default, because a colour is a
///     quantity of light and averaging encoded values does not average light.
///   - attributes are **perspective-corrected** by default (Lesson 3.2). What is
///     interpolated is `a/w` and `1/w`; the division at the end recovers `a`.
///     Skip it and a texture swims. Note the asymmetry with depth: `z` is
///     interpolated *directly*, because the projection has already made device
///     depth affine in screen space and correcting it again would be wrong.
///
/// Same coverage as the flat `fill_triangle` — identical bounding box, identical
/// fill rule, identical pixels — so the two can be swapped without a seam
/// appearing anywhere.
///
/// **Depth.** Pass a `depth_buffer` and the fill becomes hidden-surface correct:
/// each pixel's depth is interpolated from the three corners, compared against
/// what is already stored, and colour and depth are written only if it wins.
/// Pass `nullptr` and no depth work happens at all — every covered pixel is
/// written, which is what a 2-D fill wants.
///
/// A nullable pointer rather than two functions, because that is exactly the
/// shape the hardware has: `SDL_BeginGPURenderPass` takes a
/// `SDL_GPUDepthStencilTargetInfo*` that "may be NULL", and a pass with no depth
/// attachment simply cannot depth-test. One rasterizer, one attachment slot,
/// filled or not.
///
/// The comparison is a fixed `<` with depth writes always on — the equivalent of
/// `SDL_GPU_COMPAREOP_LESS` with `enable_depth_write = true`. Real pipelines make
/// both configurable (a transparent pass tests but does not write); we do not
/// need that until Module 6, and a knob with one setting is worse than no knob.
///
/// @param depth  the depth attachment to test and write against, or `nullptr`.
/// @param style  interpolation, shading and blend space. The default is correct.
void fill_triangle(framebuffer& fb, depth_buffer* depth,
                   const vertex& a, const vertex& b, const vertex& c,
                   fill_style style = {});

/// The same fill with no depth attachment — `fill_triangle(fb, nullptr, …)`.
///
/// Kept as its own name because two thirds of the calls in this engine are 2-D
/// and should not have to say `nullptr` to mean "this is a flat picture".
inline void fill_triangle(framebuffer& fb,
                          const vertex& a, const vertex& b, const vertex& c,
                          fill_style style = {})
{
    fill_triangle(fb, nullptr, a, b, c, style);
}

/// The outline only — three calls to draw_line.
///
/// Note that this does **not** light the same pixels as the boundary of
/// fill_triangle, and cannot: an outline wants both endpoints of every edge,
/// while a fill wants each shared edge counted once. Two different questions.
void draw_triangle(framebuffer& fb,
                   int x0, int y0, int x1, int y1, int x2, int y2,
                   Uint32 colour);

} // namespace engine
