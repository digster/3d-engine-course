// engine/include/engine/phys/epa.hpp — how deep, and which way out?
//
// Lesson 8.6. 8.5 ended with a sentence that was more of an IOU than a result.
// `collide(convex, convex)` returns `+0` when two shapes overlap and its own doc
// comment calls that a placeholder, because GJK cannot measure penetration:
//
//     the origin is inside `A ⊖ B` and the depth is a distance to its boundary,
//     which the search never looks at.
//
// A solver cannot do anything with `+0`. Lesson 8.9 will push two overlapping
// bodies apart with an impulse along a normal, scaled by how far they have
// interpenetrated; both of those numbers are missing. This file computes them.
//
// ---- WHAT PENETRATION DEPTH ACTUALLY IS ------------------------------------
//
// Not "how far in do they go", which is vague enough to have several answers.
// The definition that a solver can use is the MINIMUM TRANSLATION:
//
//     depth = the length of the shortest vector t such that A and B + t
//             no longer overlap.
//
// And that definition hands you the algorithm in two lines, because 8.5 §3
// already proved the equivalence it needs. A and `B + t` overlap exactly when
// some point of A equals some point of `B + t`:
//
//     a = b + t   for some a ∈ A, b ∈ B
//     ⟺  t = a − b
//     ⟺  t ∈ A ⊖ B
//
// **The translations that keep them overlapping ARE the Minkowski difference.**
// So the shortest translation that separates them is the shortest vector that is
// NOT in `A ⊖ B` — which is to say, the vector from the origin to the nearest
// point of the difference's BOUNDARY. 8.6 §2 works that through with a picture
// and a number.
//
// GJK measured the distance from the origin to the SET, which is zero when the
// origin is inside it. EPA measures the distance from the origin to the set's
// BOUNDARY, which is the interesting number in exactly the case GJK gives up on.
// Same set, same support function, same sandwich of bounds — the other side of
// one idea, which is why this file is two hundred lines rather than a thousand.
//
// ---- THE ALGORITHM, AND WHY IT IS GJK RUN BACKWARDS ------------------------
//
// You are in a dark room and want the distance to the nearest wall. You cannot
// see the walls, but you can ask "how far can I go in direction `d` before I
// leave the room?" — that is the support function, and it is all you have.
//
// So inflate a balloon. The balloon is a POLYTOPE you know exactly, it fits
// inside the room, and therefore its nearest wall is no farther than the room's:
//
//     P ⊆ S  ⟹  for every direction n, support_P(n) ≤ support_S(n)
//            ⟹  min over n of support_P(n) ≤ min over n of support_S(n)
//
// and for a convex set containing the origin, `min over n of support(n)` IS the
// distance to the boundary (§3 derives that; it is the one fact the whole method
// rests on). So **the polytope's closest face is a LOWER bound on the depth.**
// Push the balloon out at its own closest face: one support call in that face's
// normal direction gives a new vertex, and `dot(w, n)` is the room's wall in
// that direction — an UPPER bound. Add the vertex, re-triangulate, repeat.
//
// Two bounds, one from a real inner polytope and one from a real supporting
// plane, squeezing the answer from both sides until they meet. That is exactly
// 8.5's structure with the roles reversed: there the upper bound fell and the
// lower rose toward it; here the lower bound rises and the upper falls. The
// reason to notice the symmetry is that it gives the same gift — a result you
// can CERTIFY from the output alone, in one support call, rather than compare
// against a second implementation.
//
// ---- THE THING NOBODY WARNS YOU ABOUT --------------------------------------
//
// Every description of EPA begins "start from the tetrahedron GJK terminated
// with". 8.5 §9 measured that on the single commonest arrangement in a game —
// two boxes standing on the same floor — **GJK produces no tetrahedron at all**,
// on 100,000 pairs out of 100,000, including every pair overlapping by more than
// ten centimetres. A shared up axis puts `y = 0` on every vertex of the
// difference the search visits, so the simplex is trapped in a plane.
//
// The difference set is three-dimensional; it is the SEARCH that never left its
// equator. Building the starting polytope is therefore on this file's CRITICAL
// PATH and not on its error path, and §4 is the section that does it. It is also
// the reason the seed produces a POLYTOPE rather than a tetrahedron: the natural
// repair for a flat quadrilateral is a bipyramid, which has five vertices, and a
// `simplex` holds four.

#pragma once

#include <engine/math/vec3.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/gjk.hpp>

namespace engine::phys
{

// ---------------------------------------------------------------------------
// Capacity
// ---------------------------------------------------------------------------
//
// THE POLYTOPE LIVES ON THE STACK AND NOTHING HERE ALLOCATES. A narrow phase
// runs thousands of times a frame, and 6.17 §9 made the same argument for the
// frame graph with a measurement: an allocation in a per-pair loop is a lock, a
// cache miss and a variance source, in exchange for a flexibility nothing wants.
//
// The two numbers are not independent. Every expansion adds exactly one vertex,
// and a triangulated convex polytope satisfies Euler's formula: with every face
// a triangle, `3F = 2E`, so `V − E + F = 2` collapses to **`F = 2V − 4`**. Sixty
// vertices therefore need 116 faces and never more, so `k_epa_max_faces` is not
// a guess — it is `2 * k_epa_max_vertices − 4`, rounded up to a round number.
// 8.6 §10 measures what a real scene uses: the worst case over 200,000 pairs was
// well under a third of this.

/// The most vertices the expanding polytope may hold. One per iteration, plus
/// the seed.
inline constexpr int k_epa_max_vertices = 64;

/// The most faces the expanding polytope may hold: `2V − 4` by Euler, rounded.
inline constexpr int k_epa_max_faces = 128;

// ---------------------------------------------------------------------------
// The answer
// ---------------------------------------------------------------------------

/// How EPA finished.
enum class epa_status
{
    /// The bounds met: `depth` is proven to the configured tolerance.
    proven,

    /// **The shapes touch rather than overlap, and there is nothing to measure.**
    ///
    /// GJK reports `intersecting` when the origin is within a contact margin of
    /// the simplex (8.5 §F.6 measures that margin at 2.11 × tolerance × size),
    /// so the origin can be a hair OUTSIDE the difference set when EPA is
    /// called. `depth` is zero and `normal` is the direction along which they
    /// are least overlapping, which is what a solver wants at a resting contact.
    touching,

    /// Stopped improving before the bounds met. `depth` is the best face found
    /// and is a valid LOWER bound; `upper` says how much is still unproven.
    stalled,

    /// Ran out of passes. Same contract as `stalled`, different cause.
    iteration_limit,

    /// Ran out of vertices or faces. Same contract as `stalled`.
    capacity,

    /// **No starting polytope could be built**, because the difference set has
    /// no volume — two coplanar triangles, a point against a plane. The depth of
    /// a flat set really is zero, so this is reported rather than papered over,
    /// and `normal` still carries the plane's direction.
    degenerate,
};

/// `"proven"`, `"touching"`, `"stalled"`, `"iteration limit"`, `"capacity"`,
/// `"degenerate"`.
[[nodiscard]] const char* name_of(epa_status status);

/// What EPA found.
struct epa_result
{
    epa_status status = epa_status::degenerate;

    /// Metres, never negative. **The minimum translation distance.**
    float depth = 0.0f;

    /// Unit, **pointing from `a` toward `b`** — `collide.hpp`'s convention,
    /// conventions.html §9e, unchanged since 8.4. Translating `b` by
    /// `+normal * depth` puts the two shapes exactly in contact.
    vec3 normal{};

    /// The contact point on `a`, in world space. The two witnesses differ by
    /// exactly `normal * depth` when the answer is proven, which is one of the
    /// invariants §9 checks rather than believes.
    vec3 point_a{};

    /// The contact point on `b`, in world space.
    vec3 point_b{};

    /// The closest face of the inner polytope: a **lower** bound on the depth,
    /// and what `depth` is set from.
    float lower = 0.0f;

    /// The best supporting plane seen: an **upper** bound on the depth. Nothing
    /// in the difference set reaches past it in the direction that produced it.
    float upper = 0.0f;

    /// `upper − lower`: the width of what is still unproven, in metres.
    [[nodiscard]] float slack() const { return upper - lower; }

    /// Expansion passes. One vertex each.
    int iterations = 0;

    /// Calls into `convex::support`, seeding included. Two per CSO vertex.
    int support_calls = 0;

    /// How many extra CSO vertices the SEED had to find before the loop could
    /// start: `0` when GJK handed over a usable tetrahedron, `2` for the flat
    /// quadrilateral that a shared up axis produces. **§5 turns this field into
    /// the histogram that justifies the whole seeding section.**
    int seed_vertices = 0;

    /// The polytope at exit. Instrumentation, and the argument for the
    /// capacities above.
    ///
    /// `vertices` is every CSO vertex the query found; `surface_vertices` is how
    /// many of them are still ON the hull, which is smaller whenever an
    /// expansion swallowed one. `faces` is live faces, because dead ones are
    /// reclaimed rather than flagged.
    int vertices = 0;
    int surface_vertices = 0;
    int faces = 0;

    /// **Euler's formula, as a test rather than a fact.**
    ///
    /// A closed triangulated surface of genus zero satisfies `V − E + F = 2`,
    /// and every face here is a triangle, so `3F = 2E` and the whole thing
    /// collapses to `F = 2V − 4`. Any expansion that tears the surface —
    /// stitching a fan across two disjoint horizon loops is the way it happens —
    /// breaks it immediately.
    ///
    /// It costs one comparison and it is computable from this struct ALONE,
    /// which is the property §7 is built on: the harness does not need to see
    /// inside EPA to know whether EPA built a polytope.
    [[nodiscard]] bool manifold() const { return faces == 2 * surface_vertices - 4; }
};

/// Knobs. The defaults are what §10 measured.
struct epa_config
{
    /// The termination threshold, **relative to the size of the shapes**.
    ///
    /// Stop when `upper − lower ≤ tolerance * scale`, where `scale` is the
    /// largest difference-set vertex the query has seen — on the order of the
    /// two shapes' sizes. It is relative for 8.5 §11's reason and not for
    /// tidiness: an absolute threshold here quits early on a kilometre-wide
    /// scene and spins to the cap on a millimetre-wide one.
    ///
    /// **It is NOT relative to the depth**, which is the difference from GJK's
    /// tolerance and is forced rather than chosen: a resting contact has a depth
    /// of a few microns or of exactly zero, and a test relative to that would
    /// never terminate on the most common contact in the engine.
    float tolerance = 1e-4f;

    /// Give up after this many expansions. Measured at 32: over 200,000 pairs
    /// the worst case was 21 and the mean was under 8.
    int max_iterations = 32;

    /// **Keep only the connected component of the visible set.** Leave it on.
    ///
    /// The knob exists because §7 needs to measure what happens without it, and
    /// what happens is not subtle: two cubes meeting face to face with a tenth
    /// of a millimetre of overlap produce a support point exactly coplanar with
    /// four existing faces, rounding scatters which of them count as visible,
    /// and the horizon comes back as three disjoint pieces. See `expand` in
    /// `epa.cpp` for the mechanism and the numbers.
    bool flood_fill_visible = true;
};

// ---------------------------------------------------------------------------
// The queries
// ---------------------------------------------------------------------------

/// **The minimum translation that separates two overlapping convex shapes.**
///
/// `start` is GJK's terminal simplex — `gjk_result::terminal`, which 8.5 carried
/// through the whole search for exactly this moment. It may be a tetrahedron, a
/// flat tetrahedron, a triangle, a segment or a single point; §4 handles all
/// five, and `epa_result::seed_vertices` reports which one arrived.
///
/// Call it only when GJK reported `intersecting` or `iteration_limit`. On a
/// genuinely separated pair the origin is outside the difference set, there is
/// no boundary point to walk to, and the honest answer is `touching` with a zero
/// depth — which is what this returns, rather than a number that looks like a
/// measurement.
[[nodiscard]] epa_result epa_penetration(const convex& a, const convex& b,
                                         const simplex& start,
                                         const epa_config& cfg = {});

/// **How far `b` must move along `unit_axis` before it clears `a`.**
///
/// One line, two support calls, and it is three things at once:
///
///   * the support function of the difference set, `h_{A⊖B}(n)`, which is the
///     quantity §3's definition minimises over all directions;
///   * the generalisation of 8.4's `gap_on_axis` to any convex shape, with the
///     sign flipped so that positive means overlapping — because 8.4's SAT and
///     this lesson's EPA are minimising **the same function** over different
///     candidate sets, which is §8's finding and the reason the two algorithms
///     agree to six digits on boxes;
///   * the CERTIFICATE. `depth ≤ depth_along(a, b, n)` for every unit `n`, by
///     definition, so a claimed depth that EXCEEDS this for its own normal is
///     provably wrong, and one that equals it is provably achievable. §9 runs it
///     over every pair it generates and over ten thousand sampled directions per
///     pair, which is a falsifier no second implementation could be.
///
/// Negative when the shapes are already apart along that axis.
[[nodiscard]] float depth_along(const convex& a, const convex& b, vec3 unit_axis);

} // namespace engine::phys
