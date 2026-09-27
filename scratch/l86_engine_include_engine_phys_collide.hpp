// engine/include/engine/phys/collide.hpp — are these two things touching?
//
// Lesson 8.4. One question, and it is the one every physics engine is organised
// around. This file answers it for the three primitives in `shape.hpp`, and the
// answer it gives back is deliberately larger than a `bool`.
//
// ---- THE SHADOW TEST -------------------------------------------------------
//
// Hold two objects up in front of a lamp and look at the wall. If the shadows
// are apart, the objects are apart — no argument, no arithmetic: a point in both
// objects would cast a point in both shadows. Move the lamp around; every
// position gives you a different pair of shadows and another chance to see a
// gap.
//
// That is a proof of separation you can hold in one hand, and it is exactly half
// of a theorem. The half that is obvious runs: **a gap in any shadow proves the
// objects are apart.** The half that is not runs the other way: for CONVEX
// objects, if they really are apart, then some lamp position shows you the gap —
// so a search over directions that finds nothing is not a failed search, it is a
// proof of contact.
//
// A direction is the lamp position. A shadow is the projection onto that
// direction, which is an interval of numbers. And the theorem is the
// SEPARATING AXIS THEOREM:
//
//     Two convex sets are disjoint if and only if there is some direction L on
//     which their projections do not overlap.
//
// Lesson 8.4 §3 derives the "only if" half from convexity and shows exactly what
// goes wrong without it. The practical content is §7: for two BOXES the infinite
// search over directions collapses to **fifteen candidates**, and if none of
// them separates, nothing does.
//
// ---- WHAT THIS FILE RETURNS, AND WHY IT IS NOT A BOOL ----------------------
//
// Every test here returns a `separation`: a direction, a signed distance along
// it, and which candidate axis produced it. That is more than "are they
// touching", and each extra field pays for itself downstream:
//
//   * **When they are apart**, the axis is a CERTIFICATE. Anyone can check it in
//     four dot products, which is the property the harness in §10 is built on:
//     it verifies the proof rather than trusting the answer.
//   * **When they overlap**, the axis and depth are the smallest translation
//     that would separate them — the MTV. Lesson 8.7 turns it into a contact
//     normal and 8.9 pushes along it.
//   * **The axis index** is what makes contact caching possible. Two boxes
//     resting on each other separate on the same axis frame after frame, so 8.6
//     tries last frame's axis first, and 8.7 keys a persistent manifold on it.
//
// ---- THE CONVENTION, FIXED HERE AND NEVER VARIED ---------------------------
//
// **`collide(a, b)` returns an axis pointing FROM `a` TOWARD `b`.** Translating
// `b` by `+axis * depth` separates them; translating `a` by `−axis * depth` does
// too. It is written on conventions.html §9e and it is checked, not assumed —
// §10.E applies the translation and re-tests, because a normal that points the
// wrong way is a solver that sucks objects together instead of pushing them
// apart, and that bug looks like a stability problem rather than a sign error.

#pragma once

#include <engine/math/bounds.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/shape.hpp>

namespace engine::phys
{

// ---------------------------------------------------------------------------
// The answer
// ---------------------------------------------------------------------------

/// Where a separating axis came from. Derived from `separation::axis_index`.
enum class axis_source
{
    radial,      ///< Centre-to-centre. Spheres, and sphere-vs-box.
    face_a,      ///< A face normal of the first box. Index 0-2.
    face_b,      ///< A face normal of the second box. Index 3-5.
    edge_edge,   ///< The cross product of an edge from each. Index 6-14.
    witness,     ///< GJK. Index -3: not a candidate axis but a pair of points.
    none,        ///< No axis: a degenerate or trivially-empty input.
};

/// `"radial"`, `"face A"`, `"face B"`, `"edge-edge"`, `"witness"`, `"none"`.
[[nodiscard]] const char* name_of(axis_source source);

/// The result of one overlap query: a direction, a signed distance, a witness.
///
/// **`depth` carries the whole answer and `hit()` is derived from it**, rather
/// than a `bool` stored beside a number that could contradict it. Positive is
/// penetration in metres; negative is a gap in metres; and the sign change at
/// zero is the moment of contact.
///
/// That the two cases share one field is not a trick to save four bytes. It is
/// the observation that a separating axis and a penetration axis are the same
/// object measured in two directions, which is why the same fifteen candidates
/// answer both questions and why the search does not change shape when the
/// objects touch.
struct separation
{
    /// Unit length, **pointing from the first argument toward the second**.
    /// Zero only when `axis_index` is `-1` and the inputs were degenerate.
    vec3 axis{};

    /// Metres. **Positive = overlapping by this much. Negative = apart by this
    /// much along `axis`.**
    ///
    /// When the objects are apart, this is the gap on ONE axis and therefore a
    /// *lower bound* on the true distance between them, not the distance. Two
    /// boxes offset diagonally have a gap on both x and y, and the real distance
    /// is the length of the combined offset — which the SAT never computes,
    /// because a proof of separation does not need it. Lesson 8.5's GJK is the
    /// algorithm that does, and this is the honest reason it exists.
    float depth = 0.0f;

    /// Which candidate produced this axis. `-1` for radial tests, `0-2` for the
    /// first box's faces, `3-5` for the second's, `6-14` for the nine edge
    /// pairs: `6 + 3*i + j` is `a.axis(i) × b.axis(j)`. `-2` is "no axis at all",
    /// a degenerate input.
    ///
    /// Lesson 8.5 adds `-3`, meaning GJK — an axis that came from a pair of
    /// WITNESS POINTS rather than from any enumerated candidate, which is the
    /// whole difference between the two algorithms expressed as an integer. It
    /// is `-3` and not `-2` because `-2` was already spoken for by
    /// `collide.cpp`'s `k_no_axis`, which `source_of` mapped to `none` through
    /// its final fallthrough rather than through a named case. A sentinel that
    /// is only reachable by falling off the end of a function is a sentinel
    /// waiting to be reused by mistake; both are named cases now.
    int axis_index = -1;

    /// How many candidates were examined before the answer was known.
    ///
    /// Instrumentation rather than result, and it earns its place: separated
    /// pairs almost always exit on one of the first few axes, overlapping pairs
    /// always examine all fifteen, and §12 measures the distribution rather than
    /// guessing at it. A four-byte field that turns a performance argument into a
    /// histogram is cheap.
    int axes_tested = 0;

    /// Are they touching or overlapping?
    [[nodiscard]] bool hit() const { return depth >= 0.0f; }
};

/// Where this result's axis came from.
[[nodiscard]] axis_source source_of(const separation& s);

/// The same result as seen from the other object: the axis is reversed.
///
/// Used by the `(sphere, box)` overloads, which compute the `(box, sphere)`
/// answer and turn it around rather than writing the whole thing twice with
/// every sign flipped. The depth is unchanged — a penetration depth has no
/// direction of its own.
[[nodiscard]] separation flip(const separation& s);

// ---------------------------------------------------------------------------
// Projection: the shadow, as arithmetic
// ---------------------------------------------------------------------------

/// A shadow: the interval a shape projects onto a direction.
struct interval
{
    float lo = 0.0f;
    float hi = 0.0f;

    [[nodiscard]] float centre() const { return 0.5f * (lo + hi); }
    [[nodiscard]] float radius() const { return 0.5f * (hi - lo); }
};

/// Do two intervals overlap, and by how much? Positive is overlap.
///
/// The whole SAT in one line, and the sign convention of `separation::depth`
/// comes straight from it.
[[nodiscard]] float interval_overlap(interval a, interval b);

/// Half the width of the box's shadow on a **unit** direction: `Σᵢ hᵢ |uᵢ · L|`.
///
/// Derived in Lesson 8.4 §6, and the derivation is three lines: a point of the
/// box is `c + Σ sᵢ hᵢ uᵢ` with each `sᵢ` in `[-1, 1]`; dotting with `L` gives
/// `c·L + Σ sᵢ hᵢ (uᵢ·L)`; and the largest value comes from choosing each `sᵢ`
/// to match the sign of its own term, which is what the absolute values say.
///
/// **The absolute values are the entire content.** Drop them and you get
/// `|Σ hᵢ uᵢ · L|`, which is the projection of one particular corner rather than
/// the radius of the shadow, and which is wrong for every direction that does
/// not happen to point at that corner.
[[nodiscard]] float projected_radius(const obb& box, vec3 unit_axis);

/// The box's shadow on a unit direction: `[c·L − r, c·L + r]`.
[[nodiscard]] interval project(const obb& box, vec3 unit_axis);

/// The sphere's shadow on a unit direction. Its radius is the sphere's radius,
/// in every direction, which is the one-line version of why spheres are cheap.
[[nodiscard]] interval project(const engine::sphere& s, vec3 unit_axis);

/// The signed gap between two boxes along one **unit** candidate axis: positive
/// means this axis separates them and the objects are certainly apart.
///
/// The primitive every SAT loop is made of, exposed because the harness quotes
/// it directly and because a reader checking a result by hand wants exactly this
/// function.
[[nodiscard]] float gap_on_axis(const obb& a, const obb& b, vec3 unit_axis);

// ---------------------------------------------------------------------------
// The tunable, and why there has to be one
// ---------------------------------------------------------------------------

/// Cross-product candidate axes shorter than `sqrt` of this are skipped.
///
/// `|u × v|` is `sin θ` for unit vectors, so this is a threshold on `sin²θ`
/// between two edges: `1e-6` corresponds to about **0.057°**.
///
/// WHY A DEGENERATE AXIS EXISTS AT ALL, AND WHY IT IS NOT RARE. Two objects
/// standing on the same floor have parallel up axes whatever their yaw, so one
/// of the nine cross products is degenerate in the single most common
/// arrangement a game ever produces. And it is never *exactly* degenerate: both
/// axes arrive through a quaternion-to-matrix conversion, so their cross product
/// comes out around 1e-8 rather than 0. §F measures exactly that.
///
/// **THE SURPRISE IS THAT `collide` DOES NOT NEED THIS GUARD TO BE CORRECT**,
/// and 8.4 §F is the measurement that says so. The folklore — guard the cross
/// products or objects fall through each other — is true of a *different*
/// formulation. Here every candidate axis is normalised before it is used, and a
/// normalised axis is a real direction however arbitrary its direction is. If
/// two convex bodies overlap then **no** direction separates them, so an axis
/// built from pure rounding error reports an overlap exactly like a good one.
/// Rounding cannot invent a gap where no direction has one. Over the whole
/// degenerate sweep, with the guard removed, the false-separation count is zero.
///
/// So the guard is here for three smaller reasons, all real:
///   * **It skips work.** A parallel edge pair spans no face of the Minkowski
///     sum, so the candidate is genuinely redundant — anything it could separate
///     is already separated by a face normal.
///   * **It keeps the MTV honest.** A near-zero cross normalises to an arbitrary
///     direction, and an arbitrary direction that happened to win the minimum
///     would be a contact normal pointing nowhere in particular.
///   * **It avoids `0/0`.** An exactly-zero cross normalises to NaN. Every
///     comparison against NaN is false, so even that fails safe — but relying on
///     that is relying on an accident.
///
/// `overlaps` below is the formulation the folklore is about, and there the
/// epsilon is load-bearing. See its doc comment.
inline constexpr float k_parallel_sin2 = 1e-6f;

// ---------------------------------------------------------------------------
// The tests
// ---------------------------------------------------------------------------

/// Two spheres. The cheapest true answer in collision detection.
///
/// `depth = rₐ + r_b − |t|`, with the axis along `t = c_b − cₐ`. One subtraction,
/// and the only interesting case is the degenerate one: two spheres at exactly
/// the same point have no direction between them, and this returns `+y` with the
/// full depth rather than a zero axis or a NaN. **A made-up direction is the
/// correct answer to a question with no answer**, provided it is a legal unit
/// vector, because every caller downstream will divide by it.
[[nodiscard]] separation collide(const engine::sphere& a, const engine::sphere& b);

/// Two axis-aligned boxes: three interval tests, and the minimum overlap wins.
///
/// This is the SAT with a candidate list of three, and §5 checks that claim by
/// running the general `collide(obb, obb)` on the same pairs and requiring
/// identical answers.
[[nodiscard]] separation collide(const aabb& a, const aabb& b);

/// A sphere against an axis-aligned box: clamp the centre into the box.
[[nodiscard]] separation collide(const aabb& box, const engine::sphere& s);

/// A sphere against an axis-aligned box, arguments the other way round.
[[nodiscard]] separation collide(const engine::sphere& s, const aabb& box);

/// A sphere against an oriented box.
///
/// One idea, and it is the only test in this file that is not a SAT: carry the
/// sphere's centre into the box's own axes, where the box is an AABB; clamp;
/// carry the clamped point back. The clamp is the closest point on the box, the
/// distance to it decides the question, and the direction to it is the normal.
///
/// **The case worth reading the implementation for is the centre INSIDE the
/// box**, where the clamp returns the centre unchanged, the distance is zero and
/// the direction is undefined. That is not a rare edge case in a physics engine;
/// it is what a fast-moving sphere looks like on the frame after it tunnels into
/// a wall. The answer is the nearest face, which is a different calculation, and
/// an implementation that skips it hands the solver a zero normal.
[[nodiscard]] separation collide(const obb& box, const engine::sphere& s);

/// A sphere against an oriented box, arguments the other way round.
[[nodiscard]] separation collide(const engine::sphere& s, const obb& box);

/// **Two oriented boxes: the Separating Axis Theorem, fifteen candidates.**
///
/// Six face normals — three from each box — and nine edge-edge cross products.
/// Lesson 8.4 §7 derives why those fifteen suffice, and §7.3 builds the
/// configuration that proves six are not enough: two long boxes crossed like the
/// arms of a plus sign, apart, with every face-normal shadow overlapping.
///
/// Returns on the **first** axis that separates, because one witness is a
/// complete proof and the remaining candidates cannot change the answer. When
/// nothing separates, all fifteen have been examined and the one with the
/// smallest overlap is the MTV.
///
/// Cross-product axes are **normalised** before their depth is compared, and
/// that is a requirement rather than tidiness: the overlap *test* is invariant
/// to the axis's length — both sides scale together — but the *depth* is not, so
/// comparing an unnormalised edge-edge depth against a face depth compares
/// metres against metres-times-sin-theta and picks the wrong winner. `overlaps`
/// below is the version that only answers the boolean and therefore skips every
/// square root.
[[nodiscard]] separation collide(const obb& a, const obb& b);

/// The same question, boolean only, with no square roots and no MTV.
///
/// The classic formulation: build `R = Aᵀ B` once, take its element-wise
/// absolute value, and express all fifteen tests in terms of those nine numbers
/// and the offset in A's frame. No cross products are ever formed and no axis is
/// ever normalised, because an unnormalised axis answers the boolean correctly.
///
/// **THE `+ k_parallel_sin2` ON THE ABSOLUTE MATRIX IS LOAD-BEARING HERE**, in a
/// way it is not in `collide`, and 8.4 §F is the measurement. Without
/// normalisation, the test on axis `aᵢ × bⱼ` compares quantities that all scale
/// with `|aᵢ × bⱼ|` — so as two edges approach parallel, both sides of the
/// comparison shrink toward zero while the rounding error in them does not, and
/// at around `1e-7` the comparison is noise against noise. Measured on two
/// crates meeting at a corner with a shared up axis: **188 false separations in
/// 400**, every one of them a pair of solid objects reported as not touching.
/// Adding a small positive number to every `|R|` term lifts the right-hand side
/// back above the noise floor, which biases the comparison toward "overlapping"
/// — the safe direction, since a false overlap costs one narrow-phase call and a
/// false separation costs an object falling through the floor. With the epsilon:
/// **zero** false separations on the same fixture.
///
/// §12 measures it against `collide` at **the same answers and a fraction of the
/// cost**, which is what makes it the right thing for a broadphase's confirmation
/// pass and the wrong thing for anything that needs a normal.
[[nodiscard]] bool overlaps(const obb& a, const obb& b);

// ---------------------------------------------------------------------------
// Lesson 8.5: the same question, asked of any convex shape
// ---------------------------------------------------------------------------

/// **Two arbitrary convex shapes, by GJK.** The façade over `gjk.hpp`.
///
/// Every test above needed to know what it was holding. This one does not: it
/// takes two `convex` views — box, sphere, capsule, arbitrary point set, any
/// mixture — and asks the Minkowski difference whether it contains the origin.
/// There is no candidate axis list, because there is no enumeration.
///
/// The result is the same `separation` the rest of this file returns, so a
/// caller can swap the specialised test for the general one without touching
/// anything downstream. Three fields are filled differently and the doc comment
/// is the contract:
///
///   * `axis_index` is `-3`, so `source_of` reports `witness`.
///   * `axes_tested` is the GJK ITERATION COUNT PLUS THE EPA ONE rather than a
///     candidate count. Same meaning — how much work did the answer take —
///     different unit.
///   * **`depth` is exact on both sides of zero, as of Lesson 8.6.** A gap is
///     GJK's true distance, better than anything the SAT can produce. An overlap
///     is EPA's minimum translation distance, which 8.6 §8 measures against the
///     SAT's MTV on 200,000 box pairs and finds agreeing to **2.4e−05 m** —
///     two algorithms with nothing in common but the set they are asking about.
///
/// 8.5 shipped this function with a `+0` here and its doc comment called that a
/// placeholder in as many words. It is worth knowing what filled it in: not a
/// second search, but the SAME search continued. EPA starts from `gjk_result::
/// terminal`, the simplex GJK was already carrying.
[[nodiscard]] separation collide(const convex& a, const convex& b);

// ---------------------------------------------------------------------------
// Closest points
// ---------------------------------------------------------------------------

/// The point of the box nearest `p` — `p` itself when `p` is inside.
///
/// Three independent clamps, one per axis, and the independence is the whole
/// reason this is three lines rather than a case analysis over 27 regions
/// (inside, six faces, twelve edges, eight corners). Each coordinate of the
/// closest point depends only on the same coordinate of `p`, because the box is
/// a product of three intervals — so "which of the 27 regions is `p` in" is a
/// question the clamp answers without ever asking it.
[[nodiscard]] vec3 closest_point(const aabb& box, vec3 p);

/// The point of the oriented box nearest `p`.
///
/// The same clamp, carried into the box's axes and back. `dot(p − c, uᵢ)` is the
/// coordinate of `p` along axis `i` — which is the transpose of the axis matrix
/// applied to the offset, and the transpose is the inverse because the axes are
/// orthonormal. Lesson 2.5's identity, doing real work.
[[nodiscard]] vec3 closest_point(const obb& box, vec3 p);

/// Squared distance from `p` to the nearest point of the box. Zero inside.
///
/// Squared, because the callers that want a comparison want it without a square
/// root, and the caller that wants a distance can take one.
[[nodiscard]] float distance_squared_to(const aabb& box, vec3 p);

/// Squared distance from `p` to the nearest point of the oriented box.
[[nodiscard]] float distance_squared_to(const obb& box, vec3 p);

} // namespace engine::phys
