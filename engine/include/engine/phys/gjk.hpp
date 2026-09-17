// engine/include/engine/phys/gjk.hpp — how far apart are these two things?
//
// Lesson 8.5. 8.4 answered "are they touching?" and was honest, in
// `separation::depth`'s own doc comment, about what it could not answer:
//
//     When the objects are apart, this is the gap on ONE axis and therefore a
//     *lower bound* on the true distance between them, not the distance.
//
// Two unit cubes whose centres are (2, 2, 2) apart are √3 ≈ 1.732 m apart,
// measured corner to corner. Every one of the SAT's fifteen candidate axes
// reports exactly 1.0, because every one of them is a coordinate axis and the
// gap along a coordinate axis is the offset's COMPONENT rather than its length.
// The SAT is not wrong — 1.0 really is a gap, and its existence really does
// prove the cubes are apart — it is just not the distance, and it is low by
// 42.3%. 8.5 §2 works that example through by hand.
//
// A lower bound is enough to answer a yes/no question and not enough for
// anything else. A character controller wants to know how far it can step before
// it hits; a continuous-collision test wants the distance so it can bound the
// time of impact; 8.8's broadphase wants a margin. All three need the number.
//
// ---- THE IDEA, IN ONE MOVE -------------------------------------------------
//
// 8.4 §3.2 introduced the MINKOWSKI DIFFERENCE and then walked away from it:
//
//     A ⊖ B = { a − b : a ∈ A, b ∈ B }
//
// Every point of it is a vector from somewhere in B to somewhere in A. So the
// vector from B to A is zero for some pair of points exactly when the two sets
// share a point — which is to say:
//
//     A and B overlap   ⟺   A ⊖ B contains the ORIGIN
//     distance(A, B)     =   distance(ORIGIN, A ⊖ B)
//
// Two sets have become one set, and a question about a pair has become a
// question about a point. The SAT enumerated the FACES of A ⊖ B, which is why
// its candidate list is a fact about boxes. GJK SEARCHES it, which is why its
// candidate list is nothing at all.
//
// And the search is possible because of the identity in `convex.hpp`. You cannot
// build A ⊖ B — for two twenty-vertex hulls it has up to four hundred vertices —
// but you never need to, because its support function is two calls:
//
//     support_{A⊖B}(d) = support_A(d) − support_B(−d)
//
// The farthest point of the difference in direction `d` is the farthest point of
// A that way minus the farthest point of B the OTHER way, because maximising
// `dot(a − b, d)` over independent `a` and `b` maximises `dot(a, d)` and
// `dot(−b, d)` separately. 8.5 §5 derives it and §5.3 checks it against a
// brute-force difference of two point sets.
//
// ---- THE ALGORITHM, IN FOUR LINES ------------------------------------------
//
// Keep a SIMPLEX — one to four points of A ⊖ B, so a point, a segment, a
// triangle or a tetrahedron. Its closest point to the origin, `v`, is an UPPER
// bound on the distance, because it is an actual point of an actual subset. The
// support point in direction `−v` gives a supporting plane, and its distance
// from the origin is a LOWER bound. Then:
//
//     1. w = support_{A⊖B}(−v)          the farthest point toward the origin
//     2. if the two bounds have met, stop — the distance is proven
//     3. add w to the simplex, and reduce the simplex to the smallest face
//        that still contains its closest point
//     4. v = that closest point, and repeat
//
// **The two bounds are the whole design.** They are computable from the output
// alone, they squeeze the answer from both sides, and when they meet the answer
// is not merely believed but PROVEN — which is why §10's harness can verify GJK
// without a second implementation of GJK, exactly as 8.4 verified the SAT by
// re-checking its certificate rather than by trusting a reference.
//
// ---- WHAT IT DOES NOT DO ---------------------------------------------------
//
// When the shapes OVERLAP, the origin is inside A ⊖ B, the distance is zero and
// GJK stops with a tetrahedron around the origin and nothing else to say. It
// cannot report a penetration depth, because depth is a distance to the
// BOUNDARY of A ⊖ B and GJK only ever looks inward. That is Lesson 8.6's
// Expanding Polytope Algorithm, which starts from exactly the tetrahedron this
// file leaves behind — which is why `gjk_result` carries the terminal simplex
// rather than throwing it away.

#pragma once

#include <engine/math/vec3.hpp>
#include <engine/phys/convex.hpp>

namespace engine::phys
{

// ---------------------------------------------------------------------------
// The simplex
// ---------------------------------------------------------------------------

/// One vertex of the Minkowski difference, and the two points that made it.
///
/// **`w` is the only thing the algorithm uses; `pa` and `pb` are what makes the
/// answer useful.** The closest point of the simplex to the origin is a convex
/// combination of its vertices — barycentric coordinates, 2.3, back for a third
/// time — and applying the same weights to the `pa`s and the `pb`s gives the
/// closest point on each ORIGINAL shape. Two real points on two real surfaces,
/// for the cost of carrying six extra floats per vertex through a loop that runs
/// half a dozen times.
///
/// Both are stored relative to their own shape's `convex::origin`, for
/// `convex.hpp`'s reason: adding the world position back is the cancellation the
/// whole arrangement exists to avoid, so it happens once, at the end.
struct gjk_vertex
{
    vec3 w{};    ///< `pa − pb − (b.origin − a.origin)`. A point of `A ⊖ B`.
    vec3 pa{};   ///< The supporting point on A, relative to `a.origin`.
    vec3 pb{};   ///< The supporting point on B, relative to `b.origin`.
};

/// Up to four points of `A ⊖ B`. A point, a segment, a triangle, a tetrahedron.
///
/// Four, because that is the largest simplex in three dimensions, and a
/// tetrahedron is the smallest thing that can enclose the origin. Carathéodory's
/// theorem is the formal reason the number is `d + 1` and never more: any point
/// in the convex hull of a set in `d` dimensions is already in the hull of at
/// most `d + 1` of its points. 8.5 §6 makes that concrete rather than citing it.
struct simplex
{
    gjk_vertex v[4]{};
    int count = 0;

    void clear() { count = 0; }
    void push(const gjk_vertex& x) { if (count < 4) { v[count++] = x; } }
};

/// How GJK finished.
enum class gjk_status
{
    separated,        ///< Apart. `distance`, `point_a` and `point_b` are meaningful.
    intersecting,     ///< The origin is inside `A ⊖ B`. Depth is 8.6's question.
    iteration_limit,  ///< Gave up. Treated as `intersecting` by callers, and counted.
};

/// `"separated"`, `"intersecting"`, `"iteration limit"`.
[[nodiscard]] const char* name_of(gjk_status status);

/// What GJK found.
struct gjk_result
{
    gjk_status status = gjk_status::separated;

    /// Metres, never negative. Zero when `intersecting`.
    float distance = 0.0f;

    /// Unit, **pointing from `a` toward `b`** — `collide.hpp`'s convention,
    /// §9e of conventions.html, unchanged since 8.4. Zero only when the inputs
    /// were degenerate.
    vec3 direction{};

    /// The closest point on `a`, in world space. Meaningful when `separated`.
    vec3 point_a{};

    /// The closest point on `b`, in world space. Meaningful when `separated`.
    vec3 point_b{};

    /// The simplex the search stopped on — **8.6's input**, and the reason this
    /// struct is 180 bytes rather than 32. When `intersecting` it is a
    /// tetrahedron enclosing the origin, which is exactly what EPA expands.
    simplex terminal{};

    /// Loop passes. Instrumentation, and §9 turns it into a histogram.
    int iterations = 0;

    /// Calls into `convex::support`, which is `2 * iterations + 2`. Reported
    /// separately because it is the cost that scales with the SHAPE — a hull's
    /// support is linear in its vertex count and a box's is three sign tests.
    int support_calls = 0;

    /// **The search stopped improving before it reached the tolerance.**
    ///
    /// `distance` is still the best answer available and is usually correct to
    /// several digits — it is the upper bound, and the upper bound is a real
    /// measurement between two real points. What is missing is the PROOF: the
    /// lower bound never came up to meet it, so this result is believed rather
    /// than certified.
    ///
    /// It happens when the gap is very small compared with the shapes, and 8.5
    /// §F.5 measures where: the termination test compares `|v|² − dot(v, w)`,
    /// whose rounding error scales with `|v|·|w|` — the gap times the SHAPE —
    /// while the threshold scales with `|v|²` — the gap squared. Shrink the gap
    /// and the threshold falls twice as fast as the noise. This is 8.4 §11's
    /// finding in a second disguise: there the scale that ruined the answer was
    /// the distance to the world origin, here it is the size of the objects, and
    /// both times the cure is to notice rather than to tune.
    bool stalled = false;
};

/// Knobs. The defaults are what §9 and §11 measured, not what looked round.
struct gjk_config
{
    /// The termination threshold, **relative**.
    ///
    /// The upper and lower bounds on the distance differ by
    /// `(|v|² − dot(v, w)) / |v|`; stop when that is below `tolerance * |v|`.
    /// Dividing by `|v|` a second time is what makes the test scale-free, and
    /// 8.5 §11 is the measurement that says an ABSOLUTE tolerance here is the
    /// same bug 8.4 §11 found in a different disguise: it quits early on a
    /// kilometre-wide scene and spins to the iteration cap on a millimetre-wide
    /// one, with no version of the constant that is right for both.
    float tolerance = 1e-4f;

    /// Give up after this many passes and report `iteration_limit`.
    ///
    /// **A cap is not defensive programming here; it is load-bearing.** GJK
    /// terminates exactly on polytopes and only CONVERGES on curved shapes
    /// (§9), so a capsule against a sphere has no last iteration — it has an
    /// error that halves. The cap is what turns "converges" into "returns".
    /// Measured at 64: box-box never exceeded 9, sphere-sphere never exceeded
    /// 24 at the default tolerance.
    int max_iterations = 64;

    /// Where to start searching. Zero means "centre to centre", pointing from
    /// `a` toward `b` — free, and right often enough to matter.
    ///
    /// **Pass last frame's `direction` here**, which is the same sense, and §G
    /// measures what it buys for a pair that has barely moved. That is the warm
    /// start 8.7's persistent manifold exists to make possible, arriving one
    /// lesson early because the measurement belongs with the algorithm.
    vec3 initial_direction{};
};

// ---------------------------------------------------------------------------
// The queries
// ---------------------------------------------------------------------------

/// **The distance between two convex shapes, and the two points that realise
/// it.**
///
/// Works on any pair of shapes with a support function — box, sphere, capsule,
/// arbitrary point set, and any mixture. It does not know which is which and it
/// has no candidate axis list, which is the entire difference between it and
/// 8.4's SAT.
///
/// Returns `separated` with a proven distance, or `intersecting` with the
/// terminal simplex for 8.6, or `iteration_limit` if it ran out — which the
/// caller should treat as `intersecting`, since running out means the bounds
/// never separated and the shapes are at worst a tolerance apart.
[[nodiscard]] gjk_result gjk_distance(const convex& a, const convex& b,
                                      const gjk_config& cfg = {});

/// The same question, boolean only, and cheaper for it.
///
/// Stops the moment the origin is enclosed rather than refining a distance, so
/// it does no square roots and typically runs fewer iterations. The analogue of
/// 8.4's `overlaps` next to its `collide`, and for the same reason: a broadphase
/// confirmation pass wants a `bool` and paying for a normal it will not read is
/// the commonest wasted work in a physics step.
[[nodiscard]] bool gjk_intersects(const convex& a, const convex& b,
                                  const gjk_config& cfg = {});

// ---------------------------------------------------------------------------
// The certificate
// ---------------------------------------------------------------------------

/// Both sides of the sandwich, recomputed from a result and the shapes alone.
///
/// The verification instrument, and it is stronger than 8.4's. A separating axis
/// proves "apart" and nothing more; this proves a NUMBER, from two directions at
/// once:
///
///   * `upper` is `|point_a − point_b|`, and `point_a` and `point_b` are actual
///     points of actual shapes, so the true distance cannot exceed it.
///   * `lower` is the width of the slab between the two supporting planes with
///     normal `direction`, so the true distance cannot fall below it.
///
/// Two support calls and four dot products, and `lower ≤ truth ≤ upper` holds
/// whatever produced the result — a correct GJK, a broken GJK, or a guess. 8.5
/// §10 runs it over every separated pair it generates and reports the worst
/// `upper − lower` rather than comparing against a reference implementation.
struct gjk_bounds
{
    float lower = 0.0f;   ///< From the supporting planes. May be negative.
    float upper = 0.0f;   ///< From the witness points.

    /// `upper − lower`. The width of what is still unproven, in metres.
    [[nodiscard]] float slack() const { return upper - lower; }
};

/// Re-derive the bounds implied by `r`, using only `a`, `b` and `r.direction`.
[[nodiscard]] gjk_bounds certify(const convex& a, const convex& b, const gjk_result& r);

// ---------------------------------------------------------------------------
// The pieces, exposed because the lesson measures them
// ---------------------------------------------------------------------------

/// The support point of `A ⊖ B` in direction `d`, with its two witnesses.
///
/// `delta` is `b.origin − a.origin`, computed once by the caller. The whole
/// numerical argument of `convex.hpp` is that this parameter exists.
[[nodiscard]] gjk_vertex cso_support(const convex& a, const convex& b, vec3 delta, vec3 d);

/// The point of the simplex closest to the origin, with `s` reduced in place to
/// the smallest face that still contains it.
///
/// Returns `true` when the origin is inside the simplex, in which case the
/// closest point is the origin and `s` is left as the enclosing tetrahedron.
///
/// **The barycentric weights are written into `weights`**, in the order the
/// REDUCED simplex ends up in, so a caller can apply them to `pa` and `pb`. That
/// is how the witness points come out, and it is the only reason this function
/// is in the header: §10 checks the weights sum to one and are non-negative,
/// which is what "inside the face" means and what a buggy simplex solver breaks
/// first.
[[nodiscard]] bool reduce_simplex(simplex& s, vec3& closest, float weights[4]);

} // namespace engine::phys
