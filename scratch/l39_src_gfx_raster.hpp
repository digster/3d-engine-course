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

// Lesson 3.8. A real include, not a forward declaration, and the difference is the
// point: `fill_style` holds a `specular` BY VALUE and `shading::lit` calls
// `shade()`, so the rasterizer needs the layout and the definitions, not just the
// names. That is the dependency this lesson adds and Module 4's programmable
// fragment stage removes. (`vec3` comes along with it.)
#include "gfx/light.hpp"

// Lesson 3.9, and the same kind of dependency for the same kind of reason:
// `fill_style` holds a `texture_binding` by value and the fragment path calls
// `sample()`, so the layout and the definitions are both needed. Note what this
// makes the file: a rasterizer that knows about coverage, interpolation, depth,
// lighting AND texturing — four responsibilities that a real pipeline separates
// into "fixed function" and "the fragment shader". Module 4 does the separating.
#include "gfx/texture.hpp"

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

    /// The world-space surface normal at this corner — Lesson 3.8.
    ///
    /// **This is where `vertex` stops being a position and becomes a position plus
    /// varyings.** Up to 3.7 every field here was either geometry (`x`, `y`, `z`,
    /// `inv_w`) or a final answer (`colour`, `u`, `v`); this one is an *input* to a
    /// calculation that has not happened yet, carried across the triangle so the
    /// fragment can do it. That is exactly what a GPU vertex shader's output is,
    /// and Module 4 gives it the name: a varying.
    ///
    /// Interpolated perspective-correctly like every other non-depth attribute, and
    /// **renormalised in the fragment** — interpolating two unit vectors gives a
    /// shorter one, worst in the middle. Left un-normalised here on purpose: the
    /// clipper may have already produced this corner by interpolation, so a unit
    /// length at the corner would not survive to the pixel anyway.
    ///
    /// Zero when the fill is not lighting anything, which costs three multiply-adds
    /// per pixel in `shading::lit` and nothing at all in the other modes.
    vec3 normal{};

    /// The world-space position of this corner — Lesson 3.8.
    ///
    /// The specular term needs `eye - position` at the *fragment*, not at the
    /// vertex, and neither `x, y` (pixels) nor `z` (device depth) can be turned
    /// back into a world position without undoing the whole chain. So the world
    /// position rides along as a second varying.
    ///
    /// Note the cost, because it is not small: these two fields take `vertex` from
    /// 28 bytes to 52, and the fill interpolates six more floats per pixel. Lesson
    /// 3.8 §4.6 measures what that is worth.
    vec3 world{};
};

/// Is this triangle facing the camera?
///
/// The whole of back-face culling, in one comparison — and the comparison is one
/// the rasterizer was already making. `edge_function` returns twice the signed
/// area, `fill_triangle` measures its sign to orient itself, and culling is
/// nothing more than *reading* that sign instead of correcting for it.
///
/// **Front-facing means a NEGATIVE edge function**, and that is worth stating in
/// full because it is the single easiest thing to get backwards. Lesson 3.4 §3.3
/// derives it; the short version is that our convention (`conventions.html` §7) is
/// stated in **NDC**, where +y is up and counter-clockwise means front — and the
/// viewport transform (Lesson 2.11) flips y on the way to framebuffer coordinates.
/// A mirror reverses orientation. So a triangle that is counter-clockwise in NDC
/// arrives here with a negative signed area, and one that is clockwise arrives
/// positive.
///
/// Measured rather than argued: an NDC triangle with a positive (counter-clockwise)
/// signed area maps through a 320x180 viewport to pixels whose `edge_function` is
/// `-5184`. Verified in `scratch/verify_34.cpp` §A.
///
/// **Takes screen-space vertices, and that is not an accident.** The other way to
/// ask this question — dot a face normal with the camera's forward axis — is the
/// one most people write first and it is *wrong* under perspective, because
/// "facing away" is relative to the ray from the eye to the triangle, not to the
/// camera's axis. Lesson 3.4 §3.4 shows the failure and measures it. Doing the test
/// after the projection makes the perspective divide do the work for free.
///
/// A degenerate triangle (zero area) is neither front nor back; this reports it as
/// back-facing, which is harmless because `fill_triangle` discards it either way.
[[nodiscard]] constexpr bool is_front_facing(const vertex& a, const vertex& b, const vertex& c)
{
    return edge_function(a.x, a.y, b.x, b.y, c.x, c.y) < 0;
}

/// Which faces to throw away before rasterizing.
///
/// Field order and meaning mirror `SDL_GPUCullMode` exactly — verified against
/// `SDL3/SDL_gpu.h`, where the enumerators are `NONE`, `FRONT`, `BACK` in that
/// order — so Module 4's port is a rename. It lives in `fill_style` for the same
/// reason: SDL_GPU carries `cull_mode` in `SDL_GPURasterizerState` alongside
/// `fill_mode` and `front_face`, which is to say it is *pipeline state*, decided
/// once and bound, not a per-draw argument.
///
/// **This is an optimisation, not a correctness fix**, and Lesson 3.4 spends real
/// time on that distinction. The z-buffer already produces the right picture with
/// culling switched off; what culling buys is not drawing roughly half of a closed
/// mesh's triangles at all.
enum class cull_mode
{
    /// Draw everything. **The default**, and the only setting that is correct for
    /// *every* mesh — because culling is only valid on geometry that is closed.
    /// A ground plane, a billboard, a leaf card and a sheet of paper are all
    /// single-sided, and back-face culling makes them vanish when seen from behind.
    none,

    /// Throw away front faces. Useful for looking inside a closed mesh, and
    /// genuinely used in real renderers — rendering the inside of a skybox cube, or
    /// the back faces of shadow volumes.
    front,

    /// Throw away back faces. **The one you want** on any closed mesh: no face
    /// pointing away from the camera can be visible, because a closer front face is
    /// always in the way.
    back
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
///
/// **Lesson 3.8 is where it strained.** Adding `lit` made this header include
/// `light.hpp`, so the rasterizer — which had no idea light existed for seven
/// lessons — now depends on the lighting model. That is a real layering violation
/// and it is being shipped deliberately, because the alternative (a callback the
/// caller supplies) *is* the fragment shader, and inventing it here would be
/// building Module 4's answer before the question is fully asked. Fixed-function
/// hardware made exactly this trade, and lost exactly this way.
enum class shading
{
    /// The barycentric blend of the three corner colours — Gouraud, from
    /// Lesson 2.4.
    vertex_colour,

    /// A procedural checkerboard evaluated at `(u, v)`, one cell per unit.
    ///
    /// A **debug pattern**, not a texture: no image, no sampler, no filtering.
    /// Every engine ships something like it, because it is how you check a mesh's
    /// uv layout before there is any art to put on it.
    ///
    /// **Kept, and not superseded** — Lesson 3.9 adds `textured` beside it rather
    /// than replacing it. A rule is not a worse texture, it is a different kind of
    /// thing: it costs no memory, it is exact at every magnification, and it never
    /// needs an artist. That is why the checkerboard survives in every engine's
    /// debug menu long after real textures arrive. Having both one keystroke apart
    /// is also what lets §5.1 compare a lookup against a formula that computes the
    /// same pattern.
    uv_checker,

    /// **Sample `fill_style::albedo`** at the interpolated `(u, v)` — Lesson 3.9.
    ///
    /// Unlit: the sampled colour goes straight to the pixel. What changes from
    /// `uv_checker` is only *where the colour comes from*; the uv interpolation
    /// (3.2), the perspective correction, and the shape of the fill loop are
    /// untouched. Lesson 2.4's claim that "the rasterizer never learns what it
    /// carries" is collecting for the last time in Module 3.
    ///
    /// With nothing bound this draws the sampler's debug magenta rather than
    /// falling back to vertex colours, because a `textured` fill with no texture is
    /// a mistake and there is no second thing it could have meant.
    textured,

    /// **Evaluate the shading equation per pixel** — Lesson 3.8.
    ///
    /// Interpolate the normal and the world position, renormalise the normal, and
    /// call `shade()` for every covered fragment. `vertex::colour` changes meaning
    /// in this mode: it is the **albedo**, the surface's own colour, not a lit
    /// result. That is not a wart — deciding what a varying means is the vertex
    /// stage's job, and this is the first time this engine has had a varying whose
    /// meaning depends on the pipeline it is bound to.
    ///
    /// Requires `fill_style::lights` to be non-null; with it null the fill falls
    /// back to `vertex_colour`, so a misconfigured pipeline draws an unlit surface
    /// rather than dereferencing nothing.
    ///
    /// **Lesson 3.9: bind `fill_style::albedo` and the albedo comes from the
    /// texture instead of from the vertex colour.** Everything else is identical —
    /// the same `shade()`, the same normal, the same light. Texture and light
    /// simply multiply, which is what "albedo" meant all along.
    ///
    /// And notice what that does to this enum, because it is the point: "textured
    /// AND lit" is not a value here. It is `lit` *plus a binding*, which means the
    /// enum no longer describes the fragment on its own. Lesson 3.8 split
    /// `shade_mode` in two when one enum tried to hold two questions; this is the
    /// same pressure arriving a second time, and this time the honest answer is not
    /// another enum. It is a fragment shader (Module 4).
    lit
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

    /// Which faces to discard. **Defaults to `none`**, and this is the one field
    /// whose default is *safe* rather than *correct* — because there is no
    /// universally correct answer. `back` is right for a closed mesh and wrong for
    /// a ground plane, so the choice belongs to whoever knows which they have.
    /// SDL_GPU makes the same call: a zero-initialised `SDL_GPURasterizerState` has
    /// `SDL_GPU_CULLMODE_NONE`.
    cull_mode cull = cull_mode::none;

    // ---- Lesson 3.8: what `shading::lit` needs ----------------------------
    //
    // Four more fields, and they are a different KIND of thing from the four
    // above. Those are pipeline state — how to rasterize. These are *uniforms*:
    // values constant across a draw that the fragment calculation reads. A GPU
    // keeps them in separate places for that reason, and Module 4 will too. They
    // sit here because there is nowhere else yet, and that is worth noticing
    // rather than tidying: `fill_style` is now two structs wearing one name.

    /// The scene's lighting, or `nullptr` for none. A **non-owning** pointer, and
    /// nullable because most fills have no lighting at all — the 2-D demos, the
    /// HUD, the checkered floor. Same argument `fill_triangle`'s `depth_buffer*`
    /// made in 3.1: what a reference cannot express here is optionality.
    const lighting* lights = nullptr;

    /// The surface's highlight colour and shininess (Lesson 3.7). Ignored unless
    /// `shade == shading::lit`.
    specular surface{};

    /// Which specular approximation to evaluate — pipeline state proper, and in a
    /// real engine a compile-time shader choice rather than a runtime branch.
    specular_model model = specular_model::blinn;

    /// The eye's world position, for the view-dependent term.
    ///
    /// One value for the whole draw, which is correct — the camera does not move
    /// between two pixels of the same triangle. Note that this is the *position*,
    /// not a direction: the direction is what varies per fragment, and computing
    /// it is the fragment's job.
    vec3 eye{};

    /// The image `shading::textured` reads, and the albedo `shading::lit` uses when
    /// one is bound — Lesson 3.9.
    ///
    /// An image **and** its sampler, because they are independent: the same floor
    /// texture is repeated here and clamped there, and the same clamped-bilinear
    /// rules apply to a hundred different images. `SDL_GPUTextureSamplerBinding` is
    /// this exact pair, for this exact reason.
    ///
    /// Unbound by default, so every fill written before 3.9 behaves identically —
    /// the same bargain `vertex::inv_w = 1` made in 3.2 and `lights = nullptr` made
    /// in 3.8. A default that changes nothing is what lets a feature be added to a
    /// pipeline object without auditing its call sites.
    texture_binding albedo{};
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
/// **Culling.** `style.cull` decides whether a triangle is discarded before any
/// pixel work happens, and the test is `is_front_facing` above — the sign of the
/// area this function was already computing. Culling happens *here*, at the front
/// of the rasterizer, because that is where the hardware does it: after clipping
/// and the divide, before rasterization. Doing it earlier would mean doing it in
/// view space, which Lesson 3.4 §3.4 shows is a different and wrong question.
///
/// @param depth  the depth attachment to test and write against, or `nullptr`.
/// @param style  interpolation, shading, blend space and cull mode.
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
