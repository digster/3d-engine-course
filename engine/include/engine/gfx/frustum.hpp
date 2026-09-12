// engine/include/engine/gfx/frustum.hpp — the six planes the camera can see between.
//
// Lesson 6.16, and the first half of a lesson whose two halves are one idea:
// **decide what NOT to draw, then draw what is left in as few calls as possible.**
// This file is the deciding. `instancing.hpp` is the drawing.
//
// ---------------------------------------------------------------------------
// THE PROBLEM, STATED AS A COUNT
// ---------------------------------------------------------------------------
//
// Every renderer in this engine submits every object every frame. The GPU throws
// the off-screen ones away — but it throws them away in the CLIPPER, which is
// downstream of the vertex shader, so a thousand-triangle prop two kilometres
// behind the camera costs a full vertex-shader dispatch, a pipeline bind, two
// uniform pushes and a draw call before anything notices it is not on screen.
// Lesson 4.8's `draw_stats` has been counting that work honestly since Module 4
// and nothing has ever acted on the count.
//
// Frustum culling acts on it, and the reason it can is that the test is
// spectacularly cheap relative to what it skips: six dot products against a box,
// once per object, on the CPU, against a whole draw's worth of GPU work.
//
// ---------------------------------------------------------------------------
// WHY THE PLANES COME FROM THE MATRIX AND NOT FROM THE CAMERA
// ---------------------------------------------------------------------------
//
// The obvious construction is geometric: take the eye, the forward/right/up
// basis, the field of view and the near and far distances, and build six planes
// by trigonometry. It works, it is what most first attempts do, and it has one
// fatal property — **it re-derives, in a second place, information the projection
// matrix already contains.** The moment the two disagree (an off-centre
// projection for VR, a sheared frustum for a mirror, the oblique near plane of a
// water clip, or simply a different `perspective()` than the one you assumed)
// the culler silently rejects geometry the renderer would have drawn.
//
// The matrix-row construction cannot drift, because it reads the matrix the
// renderer actually uses. It is also four lines long, and it comes from ONE
// observation, which §3 of the lesson derives properly:
//
//     **A CLIP-SPACE COORDINATE IS ALREADY A SIGNED DISTANCE.**
//
// A point is inside the left plane exactly when `x_clip >= -w_clip`, i.e. when
// `x_clip + w_clip >= 0`. And `x_clip` is row 0 of `clip_from_world` dotted with
// the world position, while `w_clip` is row 3 dotted with it. So
//
//     x_clip + w_clip = (row0 + row3) . (x, y, z, 1)
//
// and `row0 + row3` IS the left plane, in `(a, b, c, d)` form, with no
// trigonometry anywhere. Each of the six inequalities that define the clip volume
// gives one plane the same way. The near plane is the odd one out and the reason
// is a convention, not an accident: SDL_GPU's clip volume runs `0 <= z <= w`
// (conventions §4), not OpenGL's `-w <= z <= w`, so near is `row2` ALONE where
// OpenGL's would be `row2 + row3`. Porting that one line wrongly is the classic
// way to lose everything within one near-plane distance of the camera.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE IS HONEST ABOUT
// ---------------------------------------------------------------------------
//
// An AABB test is CONSERVATIVE in both directions and neither is a bug:
//
//   * A box that straddles a plane is KEPT, even if the object inside it is
//     wholly outside. That is the price of bounding anything.
//   * A box can fail to be rejected while being entirely outside the frustum —
//     the **false-positive corner case**, where a large box passes all six
//     half-space tests by poking past six different planes without ever entering
//     the volume. §7 measures how often: **0.78% of kept boxes** on a sweep of
//     200,000 randomly placed boxes. Not rare enough to forget, not common
//     enough to pay for a better test.
//
// Both are safe. The culler is allowed to keep something invisible; it is never
// allowed to reject something visible, and that asymmetry is what makes the test
// a legitimate optimisation rather than a rendering decision.
//
// AND THE NUMBER THAT MATTERS IS NOT THE CULL RATE. It is frame time, and there
// is a scene size below which culling LOSES — the test costs more than the draw
// it saves. §8 finds the crossover on this engine rather than asserting one.

#pragma once

#include <engine/gfx/bounds.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/vec3.hpp>

#include <span>

namespace engine {

/// A plane, as the four coefficients of `a*x + b*y + c*z + d`.
///
/// **The sign convention is: positive is INSIDE.** `signed_distance` returns how
/// far a point is along the inward normal, so a point is inside the half-space
/// when the result is `>= 0`. Every plane this file produces points inward,
/// which is what lets the six tests be one loop instead of six special cases.
///
/// `normal` is unit length **only after `normalise()`** — and whether that has
/// been done is not cosmetic, it is the difference between `signed_distance`
/// returning metres and returning "metres times whatever scale this row happened
/// to have". A pure inside/outside test does not care (the sign survives any
/// positive scale); anything that compares a distance against a length — a
/// sphere's radius, a bias, a fade band — cares completely.
struct plane
{
    vec3 normal{};      ///< points INTO the volume
    float d = 0.0f;     ///< the constant term; `normal . p + d` is the distance
};

/// How far `p` is along the plane's inward normal. Negative means outside.
[[nodiscard]] inline float signed_distance(const plane& pl, vec3 p)
{
    return dot(pl.normal, p) + pl.d;
}

/// Scale the four coefficients so `normal` is unit length.
///
/// A degenerate plane (a zero normal, which a singular matrix can produce) is
/// left alone rather than producing infinities — the caller then sees a plane
/// that accepts everything, which is the safe failure for a culler.
[[nodiscard]] inline plane normalised(const plane& pl)
{
    const float len = length(pl.normal);
    if (len <= 0.0f) { return pl; }
    const float inv = 1.0f / len;
    return {pl.normal * inv, pl.d * inv};
}

/// The six planes bounding what a camera can see.
///
/// **Stored inward-facing and normalised**, both by `frustum_of` below. The order
/// is fixed by the constants so that a test can name a plane by index and mean
/// the same one every time — the same rule `aabb::corners` fixed for corners in
/// Lesson 6.8, and for the same reason: a diagnostic that says "rejected by plane
/// 3" is useless if plane 3 moves.
struct frustum
{
    /// Index by the constants below, never by a literal.
    plane planes[6]{};

    static constexpr int k_left   = 0;
    static constexpr int k_right  = 1;
    static constexpr int k_bottom = 2;
    static constexpr int k_top    = 3;

    /// **Spelled `near_z` / `far_z`, not `near` / `far`.** `<windows.h>` defines
    /// both of those as macros (a relic of 16-bit segmented pointers), so a
    /// member called `near` compiles on macOS and Linux and fails on MSVC with an
    /// error pointing at whatever line happens to follow. `mat4::perspective`
    /// dodged it the same way in Lesson 2.10; this is that decision being kept.
    static constexpr int k_near_z  = 4;
    static constexpr int k_far_z   = 5;

    static constexpr int k_count = 6;
};

/// The name of a plane index, for diagnostics and the HUD.
[[nodiscard]] const char* name_of_plane(int index);

/// Extract the six planes from a world -> clip matrix.
///
/// `clip_from_world` is exactly the matrix the renderer uses — `projection *
/// view`, the one `camera_uniforms::clip_from_world` carries to the GPU. Passing
/// a different one is the single most common way to get a culler that works
/// until the camera does something unusual.
///
/// **Worked example (Lesson 6.16 §3.4).** For the course's default camera the
/// extraction is checkable against three facts that need no trust:
///   * `signed_distance(left, eye)` is **0.000000**, and so are right, bottom and
///     top — the eye is the APEX of the pyramid, so it lies exactly ON all four
///     side planes. Four independent zeros is a strong test of the row algebra;
///     the first draft of this code put the sign on the wrong row and produced
///     left and right as exact negatives of each other, which those four zeros
///     would still have passed — so the near and far checks below are not
///     optional.
///   * `signed_distance(near_z, eye)` is **-0.300000**, i.e. the eye sits exactly
///     `near` BEHIND the near plane. Negative is correct: the eye is outside the
///     volume.
///   * `signed_distance(far_z, eye)` is **99.998642** where the arithmetic says
///     100. That 1.36e-3 discrepancy is NOT this function's: §4 of the lesson
///     traces it into `perspective()` itself, where forming `A + 1` with
///     `A = -1.003009` cancels away eight bits and amplifies A's last unit in the
///     last place by **332x**. The matrix's own far plane is at -99.998217. This
///     function reports the matrix it was given, faithfully, including its error.
[[nodiscard]] frustum frustum_of(const mat4& clip_from_world);

/// Where a volume sits relative to a frustum.
///
/// Three states and not two, because the third is worth money: a box that is
/// WHOLLY inside needs no further testing, which is what makes a bounding-volume
/// hierarchy able to accept an entire subtree in one test. We have no hierarchy
/// yet (that is this lesson's Exercise 7 and Module 9's spatial structures), so
/// `inside` is currently only reported — but reporting it is how you find out
/// whether a hierarchy would pay, before building one.
enum class visibility
{
    outside,        ///< no part of it is in the frustum — safe to reject
    intersecting,   ///< it crosses at least one plane
    inside          ///< every corner is inside every plane
};

[[nodiscard]] const char* name_of(visibility v);

/// Classify a box. **The full answer**, with the `inside` case distinguished.
///
/// Costs up to twelve plane evaluations (two corners per plane) against
/// `intersects`' six, so it is the one to call when the extra state is used and
/// not the one to call in a hot loop that only needs a yes or no.
[[nodiscard]] visibility classify(const frustum& f, const aabb& box);

/// Is any part of `box` inside `f`? **The hot-loop test.**
///
/// The classic "negative vertex" formulation: for each plane, pick the single
/// box corner FURTHEST along that plane's inward normal, and if even that corner
/// is outside then all eight are. One corner selection (three comparisons, or
/// three `select`s once the compiler vectorises it) plus one dot product per
/// plane — six planes, no loop over corners, and an early exit on the first
/// rejecting plane.
///
/// **Returns true for a box it cannot reject, which is not the same as "visible"**
/// — see the false-positive note in this file's header. Conservative in the safe
/// direction, always.
[[nodiscard]] bool intersects(const frustum& f, const aabb& box);

/// Is any part of `s` inside `f`?
///
/// **Cheaper and looser than the box test**, and both halves of that matter. One
/// dot product and one comparison per plane, with no corner selection at all —
/// but a sphere derived from a box by `bounding_sphere` is 2.72x the box's volume
/// (see `bounds.hpp`), so it rejects strictly less. It exists to be a PRE-TEST,
/// and §7 measures whether the pre-test earns its place.
[[nodiscard]] bool intersects(const frustum& f, const sphere& s);

/// What a frame's culling pass did. Counters, not correctness.
///
/// The same shape and the same purpose as `draw_stats::ideal_pipeline_binds`
/// (4.8) and `order_report` (6.11): **make the policy measurable instead of
/// arguable.** A frame with `culled == 0` has not proved that culling is useless;
/// it has proved that this scene, from this camera, has nothing off screen — and
/// telling those two apart is the whole reason the counter exists.
struct cull_report
{
    int tested = 0;     ///< bounds offered to the culler
    int visible = 0;    ///< …kept
    int culled = 0;     ///< …rejected. `tested == visible + culled`, always.

    /// …of which were wholly inside every plane, so a hierarchy could have
    /// accepted them without testing their children. Only filled in by
    /// `cull_visible` when `classify_fully` is set, because it costs the second
    /// corner per plane.
    int fully_inside = 0;

    /// Empty bounds — an object with no geometry. Counted as culled, and counted
    /// separately because a scene where this is large has an asset problem
    /// rather than a visibility one.
    int empty = 0;

    /// **Plane evaluations actually performed.** The cost, in the unit the cost
    /// is paid in — not in objects, which hides the early exit entirely.
    ///
    /// Six per surviving object and fewer per rejected one, so a scene that culls
    /// heavily is CHEAPER per object to cull than one that culls nothing. That is
    /// backwards from most optimisations and is worth seeing in a number.
    int plane_tests = 0;

    /// Which plane did the rejecting, indexed by `frustum::k_*`. Diagnostics:
    /// a scene rejecting almost everything on `k_far_z` is telling you the far
    /// plane is too close, and nothing else in the engine would say so.
    int rejected_by[frustum::k_count]{};
};

/// Walk `bounds` and write the indices of the survivors into `out`.
///
/// **Indices out, not bounds out** — the same decision `draw_key` made in Lesson
/// 6.11 and for the same reason: the caller's draw items are 140 bytes each and
/// permuting them to express a subset would be moving the wrong thing. The caller
/// walks its own array in the order these name.
///
/// `out` must have room for `bounds.size()` entries; the return value is how many
/// were written. A short `out` is a precondition violation and asserts in debug
/// rather than truncating silently, because a silently truncated visible set
/// renders a partial scene and looks exactly like a culling bug.
///
/// @param sphere_prefilter  run `bounding_sphere` as a cheap first test. Loose,
///        so it can only reject things the box test would also have rejected —
///        it is a speed knob, never a correctness one, and §7 measures whether
///        it is a positive one (on this engine, at this scene size, it is not).
/// @param classify_fully    also fill in `cull_report::fully_inside`, at the cost
///        of the second corner per plane. Off by default.
int cull_visible(const frustum& f, std::span<const aabb> bounds, std::span<int> out,
                 cull_report* report = nullptr,
                 bool sphere_prefilter = false, bool classify_fully = false);

} // namespace engine
