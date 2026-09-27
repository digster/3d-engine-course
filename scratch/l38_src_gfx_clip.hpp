// src/gfx/clip.hpp — cutting geometry against the near plane, before the divide.
//
// Everything the rasterizer has drawn so far has been geometry that was already
// safely in front of the camera. Lesson 3.3 is where that assumption is retired,
// because it is not an assumption a renderer gets to make: walk toward a wall and
// at some point part of it is behind your eye, and the triangle that describes it
// has one corner in front and two behind.
//
// The perspective divide (Lesson 2.10) cannot cope with that, and the reason is
// worth stating precisely rather than as a warning. The divide is x/w, and the
// projection matrix writes w = -z_view: positive in front of the eye, ZERO on the
// plane through it, NEGATIVE behind. So a vertex behind the eye does not merely
// land somewhere odd — dividing by a negative number FLIPS ITS SIGN, and it lands
// on the OPPOSITE side of the screen from where it belongs. A triangle with one
// such corner is drawn stretched across the whole frame, or inside out. And a
// vertex exactly on the eye plane divides by zero.
//
// The fix is not a bigger guard and it is not a branch inside the divide. It is to
// CUT the triangle along the near plane and rasterize only the part in front —
// which is a different triangle, with vertices the mesh never contained. Building
// those vertices, with all their attributes, is what this file does.
//
// TWO THINGS ARE NON-NEGOTIABLE ABOUT WHERE THIS RUNS.
//
//   1. AFTER the projection matrix, BEFORE the divide. Clip space — the space in
//      which a vertex is a vec4 whose w is not yet 1 — is the last place where the
//      information the clipper needs still exists. The divide is what destroys it.
//
//   2. Against z_clip >= 0, NOT w_clip >= 0. Those are two different planes, and
//      the difference is the whole near/far distinction. See `near_distance`.
//
// Only the near plane is handled here, and that asymmetry is deliberate rather than
// unfinished. The other five frustum planes are an OPTIMISATION: a triangle that
// falls off the left of the screen is drawn correctly today, just wastefully, because
// the rasterizer clamps its bounding box to the framebuffer. The near plane is a
// CORRECTNESS requirement, because the divide is undefined there. Lesson 3.3 §3.8
// makes that argument properly, and Exercise 3.3.4 generalises the code below to
// all six planes for the student who wants the optimisation as well.

#pragma once

#include "math/vec2.hpp"
#include "math/vec4.hpp"

#include <SDL3/SDL.h>

#include <cstddef>
#include <span>

namespace engine {

/// A triangle corner as the CLIPPER sees it: a clip-space position, plus whatever
/// that corner happens to know.
///
/// The deliberate contrast is with `engine::vertex` (raster.hpp), which is a
/// SCREEN-space type — integer pixels, a device depth in [0,1], and a `1/w` that
/// has already been computed. Everything about that type assumes the divide has
/// happened, which is exactly what makes it the wrong type to clip with. So the
/// projection pipeline now has two vertex types and one conversion between them:
///
///     model -> world -> view -> CLIP -> [clip_vertex] -> divide -> viewport -> [vertex]
///                                        ^ here                                 ^ raster.hpp
///
/// Two types rather than one flexible type is a real decision. A single struct with
/// a "have I been divided yet" flag would compile, and would let a screen-space
/// vertex be handed to the clipper — which would silently produce nonsense, because
/// the clipper's arithmetic is only valid before the divide. Making the two states
/// two types makes that call fail to compile instead. Lesson 3.3 §4.1.
struct clip_vertex
{
    /// Position in CLIP space: `proj * point(v_view)`, with the divide **not** yet
    /// applied. `w` is meaningful and may be negative or zero — that possibility is
    /// the entire reason this type exists.
    vec4 position{};

    vec2 uv{};                       ///< texture coordinate (Lesson 3.2)
    Uint32 colour = 0xFFFFFFFFu;     ///< ARGB8888 as stored — i.e. sRGB-encoded

    /// The surface normal at this corner, in **world** space — Lesson 3.8.
    ///
    /// Not needed until the shading equation moved into the fragment loop. Up to
    /// 3.7 the normal was consumed in the vertex stage and only its *result*, a
    /// colour, travelled onward; per-pixel shading has to carry the input instead.
    ///
    /// **It must be clipped like everything else.** A clipped triangle's new corner
    /// is a genuinely new surface point and needs the normal that belongs there —
    /// forget it and every triangle crossing the near plane is lit from whatever
    /// happened to be in the field, which is a bug that only appears when you walk
    /// into geometry. It is interpolated linearly and left un-normalised; the
    /// fragment renormalises anyway (§3.5), so doing it here would be work thrown
    /// away twice.
    vec3 normal{};

    /// The position of this corner in **world** space — Lesson 3.8.
    ///
    /// The specular term needs `eye - position` per fragment, and `position` above
    /// is in clip space, which is the wrong space and (before the divide) not even
    /// a position yet. So the world position rides along as its own varying.
    vec3 world{};
};

/// The most vertices a triangle can have after being clipped against **one** plane.
///
/// A plane cuts a convex polygon into a convex polygon, and each cut can add at most
/// one vertex to a triangle: three corners, one edge in and one edge out, gives a
/// quadrilateral. Four is therefore not a guess or a safety margin, it is the exact
/// bound — which matters, because it is what lets the caller use a fixed-size array
/// with no capacity check and no allocation in the middle of the frame.
///
/// Clipping against all six frustum planes would need nine (Exercise 3.3.4): each
/// further plane can add one more vertex.
inline constexpr std::size_t k_clip_max_vertices = 4;

/// How far in front of the near plane a clip-space point is — **positive inside**.
///
/// This is `z_clip`, and that it is simply the z component is a gift from Lesson
/// 2.10's projection matrix rather than a coincidence. That matrix's depth row is
/// `z_clip = A*z_view + B` with `A = far/(near-far)` and `B = far*near/(near-far)`,
/// chosen precisely so that `z_clip/w` — the device depth — is 0 at the near plane
/// and 1 at the far one. Setting `z_clip = 0` therefore says `A*z_view + B = 0`,
/// i.e. `z_view = -B/A = -near`: exactly `near` units in front of the eye. The
/// plane we want has already been arranged to be the coordinate plane `z = 0`, so
/// the "signed distance" to it costs nothing to evaluate.
///
/// **This is not `w >= 0`, and the difference matters.** `w_clip = -z_view`, so
/// `w >= 0` is the plane through the EYE, a distance `near` closer than this one.
/// It is the more obvious test — "is this in front of me?" — and it is not enough:
/// a vertex just inside it has a `w` of `1e-6`, survives the guard, and divides to
/// a coordinate in the millions. Clipping to the near plane bounds the divide;
/// clipping to the eye plane merely postpones it blowing up. Lesson 3.3 §3.4.
///
/// Note that this correctly rejects points *behind* the eye too, with no separate
/// test: there `z_view > 0`, and since `A` and `B` are both negative, `z_clip` is
/// negative as well.
[[nodiscard]] constexpr float near_distance(const vec4& clip_position)
{
    return clip_position.z;
}

/// Where along the segment a→b the near plane is crossed, as a fraction in [0, 1].
///
/// Signed distances make this a two-line derivation rather than a plane-intersection
/// routine. Distance varies affinely along the segment — `d(t) = da + t*(db - da)` —
/// because a clip-space position is a linear function of the view-space position,
/// which is itself affine along a straight edge. Setting `d(t) = 0` and solving:
///
///     t = da / (da - db)
///
/// Read it: `da` is how far in we start, `da - db` is how much distance we lose
/// over the whole edge, so the ratio is the fraction of the edge spent getting to
/// zero. When the two endpoints are on opposite sides — the only case this is
/// called for — the denominator cannot be zero and the result is strictly inside
/// (0, 1). Lesson 3.3 §3.5.
///
/// **Both distances are passed in rather than recomputed** because the caller has
/// already evaluated them to decide there was a crossing at all, and because
/// recomputing invites the classic sign slip of writing `db / (db - da)`.
[[nodiscard]] constexpr float near_crossing(float da, float db)
{
    return da / (da - db);
}

/// The vertex a fraction `t` of the way from `a` to `b`, attributes and all.
///
/// Every component is interpolated with the **same** `t`, including `w`, and that
/// uniformity is the point of clipping here rather than after the divide. In clip
/// space the position is a linear image of the view-space position, so a straight
/// edge is still straight and a parameter that is affine along it stays affine.
/// Any attribute the artist authored linearly over the surface — a uv, a colour, a
/// normal — is therefore recovered exactly by a plain lerp.
///
/// It is worth being clear about what `t` is **not**. It is not the fraction of the
/// way along the edge *as drawn on screen*. Lesson 3.2 spent itself on precisely
/// that distinction: screen-space position is `a/w`, which is not affine in the 3-D
/// parameter. So the crossing vertex will not appear at the fraction `t` along the
/// on-screen segment, and it should not — it appears where the 3-D geometry says.
/// Clipping before the divide and interpolating attributes after it are the same
/// fact seen from two directions.
///
/// **Colour is blended in linear light**, decoding and re-encoding around the lerp,
/// for the reason Lesson 1.6 established and Lesson 2.4 measured: a stored sRGB
/// value is not a quantity of light, and averaging stored values does not average
/// light. This costs two decodes and an encode per new vertex — paid once per
/// clipped edge, never per pixel — and it means the new corner hands `fill_triangle`
/// a value it can decode again and get the same light back.
[[nodiscard]] clip_vertex lerp(const clip_vertex& a, const clip_vertex& b, float t);

/// Clip a **line segment** against the near plane, in place.
///
/// The one-dimensional case, and the honest way in to the polygon version below:
/// there are only three outcomes, and you can hold all three in your head at once.
///
///   - both endpoints in front  → nothing to do, return `true`
///   - both behind              → nothing survives, return `false`
///   - one of each              → move the outside endpoint onto the plane
///
/// This exists because the demo draws a great many lines — the world grid, the
/// object axes, every wireframe edge — and a line that crosses the near plane is
/// exactly as broken as a triangle that does. Before Lesson 3.3 those lines simply
/// vanished; now they are cut, so a gridline running under the camera reaches the
/// bottom of the screen instead of disappearing when you walk over it.
///
/// @return `false` if the segment lies entirely behind the near plane, in which
///         case `a` and `b` are left untouched and there is nothing to draw.
[[nodiscard]] bool clip_segment_near(clip_vertex& a, clip_vertex& b);

/// Clip a convex polygon against the near plane — **Sutherland–Hodgman**.
///
/// The algorithm is one loop and it is easier than its reputation. Walk the
/// polygon's edges in order; for each edge from `current` to `next`, decide what
/// the *output* polygon should contain, based only on which side of the plane those
/// two vertices are on:
///
///   | current | next    | emit                                    |
///   |---------|---------|-----------------------------------------|
///   | inside  | inside  | `next`                                  |
///   | inside  | outside | the crossing point                      |
///   | outside | inside  | the crossing point, then `next`         |
///   | outside | outside | nothing                                 |
///
/// Three observations make it click. First, a vertex is emitted when the edge
/// ARRIVES at it and it is inside — so each vertex is considered exactly once and
/// nothing is emitted twice. Second, a crossing is emitted whenever the two ends
/// disagree, in either direction. Third, the four rows collapse to two independent
/// questions: "did we cross?" (emit the crossing) and "is `next` inside?" (emit
/// `next`) — which is how the implementation is written, because four branches that
/// are really two invite one of them being wrong.
///
/// The output is still convex and still wound the same way as the input, which is
/// what lets the caller fan it into triangles with `(out[0], out[k-1], out[k])` and
/// what keeps Lesson 3.4's back-face test meaningful afterwards.
///
/// @param in   the polygon, in order. A triangle is `count == 3`.
/// @param out  storage for the result; must hold at least `k_clip_max_vertices`.
/// @return the number of vertices written — **0** (nothing survives) or **3** or
///         **4**. Never 1 or 2: a convex polygon cut by a plane is either empty or
///         has an interior.
[[nodiscard]] std::size_t clip_polygon_near(std::span<const clip_vertex> in,
                                            std::span<clip_vertex> out);

} // namespace engine
