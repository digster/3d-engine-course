// engine/include/engine/math/axis_angle.hpp — one turn, about one line.
//
// Lesson 7.1 asked "which three turns compose to this orientation?" and got
// twenty-four answers, a singularity, and an interpolation that takes 14% more
// turning than it needs on a generic pair and 209% near lock. This file asks a
// different question:
//
//     What SINGLE turn is this?
//
// Euler's rotation theorem (Lesson 7.2 §4) says the question always has an
// answer: every rotation of 3-space about a fixed origin is a turn of some angle
// about some axis. Not "can be decomposed into" — IS. The proof is three lines
// of determinant algebra and it hinges on 3 being odd, which is why this is a
// fact about our world and not about rotation in general.
//
// WHAT AXIS-ANGLE IS GOOD AT, and it is worth being precise, because this file
// is not the end of the arc:
//
//   - It has no arbitrary convention. There are twenty-four Euler orders and
//     exactly one axis-angle. A file, a tool and an engine that disagree about
//     the order of three angles agree about (axis, angle) immediately.
//   - **It interpolates exactly.** Scaling the angle of the single turn that
//     separates two orientations traces the geodesic between them — the shortest
//     path, at constant speed. That is `rotation_slerp` below, and §9 measures it
//     against 7.1's Euler lerp on the same pairs: 0.00% excess against +14% and
//     +209%. This is the lesson that redeems 7.1's worst finding.
//   - Its angle IS the natural distance between orientations, so "how far is
//     this pose from that one" stops needing a separate instrument.
//
// AND WHAT IT IS BAD AT, which is the whole reason Lessons 7.3 and 7.4 exist:
//
//   - **It does not compose.** Given (n₁,θ₁) and (n₂,θ₂) there is no pleasant
//     formula for the axis and angle of doing one and then the other. You convert
//     to matrices, multiply, and extract — trig out, trig back in, every time.
//     A quaternion composes with sixteen multiplies and no trig at all.
//   - **The extraction has a hard end.** At θ = π the matrix is symmetric, the
//     route that finds the axis everywhere else finds nothing, and the axis's
//     SIGN is genuinely undetermined — (n, π) and (−n, π) are the same rotation.
//     §8 does this properly rather than clamping and hoping.
//   - Four numbers for three degrees of freedom, with a constraint (|n| = 1) that
//     nothing enforces. The rotation vector below fixes the count; nothing here
//     fixes the composition.
//
// CANONICAL FORM, used everywhere in this file: **`axis` is unit, `angle` is in
// [0, π]**. Every rotation has exactly one such spelling except the turn by π,
// which has two (n and −n), and the identity, which has infinitely many because
// its axis is undetermined. Both exceptions are real, both are handled, and
// neither is swept under a clamp.

#pragma once

#include <engine/math/mat3.hpp>
#include <engine/math/rotation.hpp>
#include <engine/math/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace engine {

/// A rotation, written as the single turn Euler's theorem promises exists.
///
/// **The axis is expected to be unit and the angle is in radians.** Nothing here
/// normalises for you, and that is a deliberate choice rather than an oversight:
/// a non-unit axis does not produce a slightly-wrong rotation, it produces a
/// matrix that is not a rotation at all — Rodrigues' formula scales the
/// out-of-plane component by |n|² and the in-plane one by |n|, so the result
/// shears. If you are holding a direction that is not unit (the output of
/// `cross`, say), you have two honest options and a hidden `normalised()` inside
/// this struct is neither of them:
///
///   1. Normalise it yourself, at the call site, where the reader can see it.
///   2. Notice that what you actually have is a **rotation vector** — an axis
///      whose LENGTH is the angle — and call `rotation_from_rotation_vector`,
///      which takes exactly that and has no unit requirement to violate.
struct axis_angle
{
    /// The line that does not move. Unit length; see above.
    ///
    /// The default is +X rather than zero, so that a default-constructed
    /// `axis_angle` is the identity rotation rather than a degenerate matrix.
    /// Which unit axis it is does not matter at angle 0 — that is the point of
    /// `axis_route::no_axis` below — but it does matter that it is *a* unit axis.
    vec3 axis{1.0f, 0.0f, 0.0f};

    /// How far to turn about it, in radians, right-hand rule (Conventions §2).
    ///
    /// Canonically in [0, π]. `axis_angle_from_rotation` always returns that
    /// range; `rotation_from_axis_angle` accepts any angle, because a caller
    /// integrating a spin over time has every right to hold 7.4 radians.
    float angle = 0.0f;
};

// ---- Rodrigues' formula: the turn, applied ------------------------------------

/// Rotate `v` about `unit_axis` by `angle` radians. **Rodrigues' rotation
/// formula**, written the way Lesson 7.2 §5 derives it from the picture.
///
/// The derivation in one paragraph, because the code is unreadable without it.
/// Split `v` into the part along the axis and the part across it:
/// `v = v∥ + v⊥`, with `v∥ = (n·v)n`. The parallel part does not move — it is on
/// the axis. The perpendicular part turns within the plane perpendicular to the
/// axis, and in that plane `v⊥` and `n × v` are two perpendicular vectors of the
/// SAME length, which is to say a pair of axes for it. So the turn is the plain
/// 2-D rotation the course built in Lesson 1.7:
///
///     v⊥' = v⊥·cos θ + (n × v)·sin θ
///
/// Add the fixed part back and substitute `v⊥ = v − (n·v)n`:
///
///     v' = v·cos θ + (n × v)·sin θ + n·(n·v)·(1 − cos θ)
///
/// **Read the three terms and the formula stops needing to be memorised.** The
/// first is "most of the vector just leans over". The second is "and it swings
/// sideways, along the one direction perpendicular to both". The third is the
/// correction that puts back the part of the along-axis component the cosine took
/// away — which is why it carries `1 − cos θ` and vanishes at θ = 0.
///
/// Costs one dot, one cross, two trig calls. If you are rotating many vectors by
/// the same turn, build the matrix once with `rotation_from_axis_angle` instead:
/// it amortises the trig across every vector, which is the entire reason a
/// renderer stores matrices.
[[nodiscard]] inline vec3 rotate_about_axis(vec3 v, vec3 unit_axis, float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return v * c + cross(unit_axis, v) * s + unit_axis * (dot(unit_axis, v) * (1.0f - c));
}

/// The matrix of that turn.
///
/// **Built by asking where the three basis vectors land**, which is not a clever
/// shortcut — it is the definition of a matrix this course has used since Lesson
/// 2.5 (*"a matrix is where the basis vectors go"*). Column 0 is what happens to
/// x̂, column 1 to ŷ, column 2 to ẑ, and `mat3` stores columns, so the three
/// results are the matrix with no rearrangement at all.
///
/// The alternative spelling, which every reference gives, is the closed form
///
///     R = I·cos θ + sin θ·[n]ₓ + (1 − cos θ)·n nᵀ
///
/// and it is the same nine numbers. We do not write it that way for two reasons.
/// The small one is that `mat3` has no `operator+`, no scalar multiply and no
/// outer product — it has never needed them, and Module 8's inertia tensor is
/// the thing that will finally justify adding them, at the point of need rather
/// than in advance. The large one is that the basis-vector form is **checkable by
/// a reader who has the derivation above in their head**, and nine transcribed
/// products of sines are not. §7.4 measures this version against a
/// hand-expanded closed form across a sweep: they agree to 1.8e-7, and the closed
/// form is **1.3x to 1.6x faster depending on the run — never the 3x its multiply
/// count predicts**. Around half to two thirds of the closed form's time is one
/// `sin` and one `cos`, which both
/// spellings pay for exactly once, so the arithmetic they differ in is a minority
/// share of a cost they share. Readability is close to free here, and it is worth
/// knowing WHY it is free rather than just that it is: when a routine's cost is
/// dominated by a transcendental, counting its multiplies predicts nothing.
///
/// **It calls `rotate_about_axis` three times, and therefore asks for the sine
/// and cosine three times, and that is deliberate.** Hoisting them into a local
/// and sharing them across the three columns is the obvious optimisation. It was
/// written, measured, and reverted — not because it lost, but because it could
/// not be shown to win: across two harnesses it came out 11% faster and 3%
/// slower than this, which is to say identical. `sin` and `cos` are pure, so
/// common-subexpression elimination hoists them whether or not you ask, and what
/// is left is a difference in how a lambda folds against literal basis vectors
/// that does not survive a change of surrounding code. §7.4 prints all four
/// spellings. **An optimisation whose sign flips between measurements is not an
/// optimisation, and paying for it in readability is a straight loss.**
///
/// **Requires `r.axis` to be unit.** See `axis_angle` above for why that is not
/// enforced here.
[[nodiscard]] inline mat3 rotation_from_axis_angle(axis_angle r)
{
    return mat3{rotate_about_axis(vec3{1.0f, 0.0f, 0.0f}, r.axis, r.angle),
                rotate_about_axis(vec3{0.0f, 1.0f, 0.0f}, r.axis, r.angle),
                rotate_about_axis(vec3{0.0f, 0.0f, 1.0f}, r.axis, r.angle)};
}

// ---- Going back: a matrix to its single turn -----------------------------------

/// Above this angle, `axis_angle_from_rotation` stops reading the axis off the
/// antisymmetric part and reads it off the symmetric part instead.
///
/// **The crossover is derived and then measured, and the two agree.** The usual
/// route takes the axis from `R − Rᵀ = 2·sin θ·[n]ₓ`, a vector of length
/// `2 sin θ`; a `float`'s absolute error ε in the matrix entries therefore
/// arrives in the axis DIRECTION as roughly `ε / (2 sin θ)`, which grows without
/// bound as θ approaches π. The other route takes `n_i²` from the diagonal of the
/// symmetric part, dividing by `1 − cos θ` instead, with the pivot component at
/// least `1/√3` — so its error is about `√3·ε / (2(1 − cos θ))`. Setting the two
/// equal gives `√3 sin θ = 1 − cos θ`, i.e. `tan(θ/2) = √3`, i.e.
///
///     θ = 2π/3 = 120°
///
/// **and the measurement says something slightly better than "the derivation was
/// right".** §8.4 sweeps both routes on matrices carrying 1e-7 of absolute error
/// and finds that from about 90° to 140° the two are within 25% of each other and
/// the winner flips from probe to probe — which is what a crossover looks like
/// when you sample it finely enough. Outside that band it is decisive: at 30° the
/// skew route is 7x better, and at 179.9° the symmetric route is 570x better.
/// 120° sits in the middle of the band where the choice does not matter, which is
/// the best possible place for a threshold to be. It is not a compromise between
/// two bad options; it is a point at which both are good.
inline constexpr float k_axis_angle_reversal_angle =
    2.0f * std::numbers::pi_v<float> / 3.0f;

/// Below this angle, the recovered axis is reported as a placeholder.
///
/// **A rotation of nothing has no axis**, and it is not a numerical problem that
/// we cannot find one — there is nothing to find. Every axis is a correct answer
/// for θ = 0, which is exactly the same statement as "no axis is".
///
/// The threshold is set where the axis stops being a usable DIRECTION rather than
/// where it stops being computable, because those are very different angles. With
/// 1e-7 of absolute error in the matrix entries, the axis direction is off by
/// about `ε/(2 sin θ)`. **Measured**, that is 0.0515° at θ = 1e-4 and 0.515° at
/// θ = 1e-5, scaling as 1/θ exactly as predicted, so one degree arrives at
/// θ ≈ 5.2e-6. The value below is 1e-5, which raises the flag while the axis is
/// still half a degree good — slightly early rather than slightly late. §8.5
/// prints the whole column.
///
/// **And it matters far less than it looks like it should**, which is the most
/// useful single fact in this file. An error of φ in the axis produces an error
/// of `2·sin(θ/2)·φ` in the resulting ORIENTATION (§8.3), and that factor goes to
/// zero exactly where the axis goes bad. At θ = 1e-6 an axis 90° wrong still
/// misplaces the object by 1e-6 rad. The hole at θ = 0 is a hole in the
/// REPRESENTATION and not in anything a renderer will ever see; the hole at
/// θ = π, where the same factor is at its maximum of 2, is the one to respect.
inline constexpr float k_axis_angle_identity_angle = 1e-5f;

/// Which of the two routes produced the axis — and therefore how to read it.
enum class axis_route : std::uint8_t
{
    /// The antisymmetric part gave the axis outright, sign included. The normal
    /// case, covering every rotation between 1e-5 rad and 120°.
    general,

    /// The turn is too small to have a direction. `axis` is the placeholder
    /// `axis_angle` was default-constructed with, `angle` is right, and the
    /// product of the two — the rotation vector — is right to float precision.
    no_axis,

    /// Near a half-turn. The axis came off the symmetric part, and its sign was
    /// recovered separately from whatever is left of the antisymmetric part. At
    /// exactly θ = π there is nothing left and **the sign is arbitrary**: (n, π)
    /// and (−n, π) are the same rotation, so both answers are correct and no
    /// implementation can prefer one. Code that compares two axes for equality
    /// has to know this; code that rebuilds a matrix from them does not.
    reversal,
};

/// The single turn recovered from a rotation, **with the number needed to judge
/// it**.
///
/// Lesson 7.1's `euler_extraction` established the rule and this follows it: if a
/// routine computes a quantity a caller would need in order to judge its own
/// output, it returns that quantity rather than discarding it.
struct axis_angle_extraction
{
    /// Canonical: `axis` unit, `angle` in [0, π]. Feeding this straight back to
    /// `rotation_from_axis_angle` reproduces the input matrix to float precision
    /// **on every route, including `no_axis`** — §8.2 measures the round trip.
    axis_angle value{};

    /// sin(angle) — the half-length of the antisymmetric part, `|R − Rᵀ|/2`.
    ///
    /// This is the quantity the `general` route divides by, so it is that route's
    /// conditioning directly: the axis it returns is uncertain by about
    /// `ε / (2·sin_angle)`. It is small at BOTH ends of the range and means
    /// something different at each, which is what `route` is for — near zero it
    /// says "there is barely a turn here"; near π it says "there is a large turn
    /// here and the easy route cannot see its axis".
    float sin_angle = 0.0f;

    /// How the axis was obtained. See `axis_route`.
    axis_route route = axis_route::no_axis;
};

/// Recover the single turn that `m` performs. **The inverse of
/// `rotation_from_axis_angle`**, up to the two genuine ambiguities named above.
///
/// **Requires a rotation.** Hand it a matrix with scale or shear in it and the
/// answer is meaningless, quietly — the trace and the antisymmetric part are
/// defined for any matrix and this routine will happily report a turn. The
/// engine still has no `is_rotation` predicate to assert on; it is Lesson 7.1's
/// Exercise 5 and deliberately left there.
///
/// The angle comes from `atan2` of the two halves of the matrix, for the reason
/// `angle_between_rotations` gives at length in `math/rotation.hpp`: the trace
/// alone cannot measure a small turn, and `acos` amplifies what little survives.
[[nodiscard]] inline axis_angle_extraction axis_angle_from_rotation(const mat3& m)
{
    axis_angle_extraction out;

    // R − Rᵀ = 2·sin(θ)·[n]ₓ, whose three distinct entries, in written notation,
    // are (r21 − r12, r02 − r20, r10 − r01). Spelled with `at(row, col)` rather
    // than member names so that a reader can check six index pairs against the
    // formula without performing a mental transpose — mat3.hpp §3.3's trap.
    const vec3 skew{m.at(2, 1) - m.at(1, 2),
                    m.at(0, 2) - m.at(2, 0),
                    m.at(1, 0) - m.at(0, 1)};
    const float trace = m.at(0, 0) + m.at(1, 1) + m.at(2, 2);

    const float two_sin = length(skew);
    const float cos_angle = (trace - 1.0f) * 0.5f;

    out.sin_angle = two_sin * 0.5f;
    out.value.angle = std::atan2(out.sin_angle, cos_angle);

    if (out.value.angle < k_axis_angle_identity_angle)
    {
        // No turn, therefore no axis. The placeholder axis is left in place and
        // the route says so. Note what is NOT done here: we do not normalise
        // `skew` and hope. At θ = 0 exactly it is the zero vector and would
        // normalise to zero, producing an `axis_angle` whose matrix is not a
        // rotation — a silent corruption three systems downstream.
        out.route = axis_route::no_axis;
        return out;
    }

    if (out.value.angle < k_axis_angle_reversal_angle)
    {
        // THE GENERAL ROUTE, and it is one division. `skew` already points along
        // the axis, with the right sign: its length is 2·sin(θ), positive for
        // every θ in (0, π), so normalising cannot flip it.
        out.value.axis = skew / two_sin;
        out.route = axis_route::general;
        return out;
    }

    // THE REVERSAL ROUTE. Near θ = π the antisymmetric part has almost nothing
    // left in it, so we read the axis off the symmetric part instead, where it
    // is at its strongest. From R = I·cos θ + sin θ·[n]ₓ + (1 − cos θ)·n nᵀ:
    //
    //     diagonal:      r_ii = cos θ + (1 − cos θ)·n_i²
    //     off-diagonal:  r_ij + r_ji = 2·(1 − cos θ)·n_i·n_j     (i ≠ j)
    //
    // The `[n]ₓ` term is antisymmetric, so it cancels exactly in the second line
    // — which is why this route is immune to whatever is happening to the part
    // the general route depends on.
    out.route = axis_route::reversal;

    const float one_minus_cos = 1.0f - cos_angle;   // ≥ 1.5 on this branch
    float squared[3] = {(m.at(0, 0) - cos_angle) / one_minus_cos,
                        (m.at(1, 1) - cos_angle) / one_minus_cos,
                        (m.at(2, 2) - cos_angle) / one_minus_cos};

    // PIVOT ON THE LARGEST, and it is not a micro-optimisation. Every other
    // component is obtained by dividing by this one, so choosing the largest
    // bounds the amplification: the three squares sum to 1, so the largest is at
    // least 1/3 and the divisor at least 1/√3. Pivot on the smallest instead and
    // an axis close to a coordinate plane divides by something near zero.
    int pivot = 0;
    if (squared[1] > squared[pivot]) { pivot = 1; }
    if (squared[2] > squared[pivot]) { pivot = 2; }

    // Clamped because float error can push a value that should be exactly zero
    // a hair below it, and `sqrt` of a negative number is NaN — the same
    // one-instruction defence `euler_from_rotation` puts in front of `asin`.
    const float n_pivot = std::sqrt(std::max(squared[pivot], 0.0f));
    const float scale = 1.0f / (2.0f * one_minus_cos * n_pivot);

    float axis[3] = {0.0f, 0.0f, 0.0f};
    axis[pivot] = n_pivot;
    for (int i = 0; i < 3; ++i)
    {
        if (i != pivot) { axis[i] = (m.at(pivot, i) + m.at(i, pivot)) * scale; }
    }

    // THE SIGN, which the symmetric part cannot know. Squares have no sign, so
    // the loop above fixed the axis only up to an overall ±. What is left of the
    // antisymmetric part still knows: `skew` is 2·sin(θ)·n with sin(θ) > 0 below
    // π, so a positive dot product means we guessed right. At exactly θ = π the
    // dot product is zero and we keep what we have — correctly, because both
    // answers name the same rotation.
    vec3 n{axis[0], axis[1], axis[2]};
    if (dot(skew, n) < 0.0f) { n = -n; }

    // Renormalised because the three components came from three different
    // divisions and are unit only to within their errors. One sqrt, and the
    // caller's contract ("axis is unit") becomes true rather than nearly true.
    out.value.axis = normalised(n);
    return out;
}

// ---- The rotation vector: three numbers, no constraint --------------------------

/// The rotation that turns by `|r|` radians about `r`. **The exponential map.**
///
/// A rotation vector packs the axis and the angle into one vector by making the
/// LENGTH the angle. Three numbers for three degrees of freedom, with no
/// normalisation constraint to maintain and — unlike Euler angles — no
/// singularity anywhere near the identity. This is the form angular velocity
/// naturally lives in: ω is a rotation vector per second, so `ω·dt` is the
/// rotation vector of one step, and Module 8 integrates rigid bodies by feeding
/// exactly that to this function.
///
/// **Written so that r = 0 needs no branch of its own.** Substituting `n = r/θ`
/// into Rodrigues and grouping gives
///
///     v' = v + (sin θ / θ)·(r × v) + ((1 − cos θ)/θ²)·(r × (r × v))
///
/// in which θ appears only inside two coefficients that are perfectly finite at
/// zero: `sin θ/θ → 1` and `(1 − cos θ)/θ² → 1/2`. The second is computed as
/// `½·(sin(θ/2)/(θ/2))²` rather than literally, because `1 − cos θ` is the
/// subtraction of two nearly-equal numbers near 1 — the exact cancellation that
/// cost Lesson 7.1 three separate bugs. The half-angle spelling never subtracts
/// anything.
///
/// The guard below is therefore not a special case for correctness; it only
/// avoids 0/0 at the single point where θ is exactly zero.
[[nodiscard]] inline mat3 rotation_from_rotation_vector(vec3 r)
{
    const float angle = length(r);

    float sinc = 1.0f;          // sin θ / θ
    float half = 0.5f;          // (1 − cos θ) / θ²
    if (angle > 0.0f)
    {
        sinc = std::sin(angle) / angle;
        const float h = angle * 0.5f;
        const float sinc_half = std::sin(h) / h;
        half = 0.5f * sinc_half * sinc_half;
    }

    // Again built as "where the basis vectors land" (Lesson 2.5), for the same
    // reason `rotation_from_axis_angle` is: the columns are the formula applied
    // three times, and a reader can check them against it.
    auto turn = [&](vec3 v) {
        const vec3 rxv = cross(r, v);
        return v + rxv * sinc + cross(r, rxv) * half;
    };

    return mat3{turn(vec3{1.0f, 0.0f, 0.0f}),
                turn(vec3{0.0f, 1.0f, 0.0f}),
                turn(vec3{0.0f, 0.0f, 1.0f})};
}

/// The rotation vector of `m`. **The logarithm map**, and the inverse of the
/// function above on the ball `|r| ≤ π`.
///
/// Only an inverse on that ball, and the reason is not numerical: turning by
/// θ + 2π is turning by θ, so the map from rotation vectors to rotations is
/// infinitely many-to-one and its inverse has to choose. It chooses the shortest,
/// which is what `axis_angle_from_rotation`'s [0, π] range already means.
///
/// **Well defined at the identity, where axis-angle is not.** The axis is
/// undetermined there, but it is multiplied by an angle of essentially zero, so
/// the product — this function's answer — is the well-behaved one. That is the
/// concrete payoff of the rotation vector over the (axis, angle) pair, and it is
/// why an integrator stores this and not that.
[[nodiscard]] inline vec3 rotation_vector_from_rotation(const mat3& m)
{
    const axis_angle_extraction e = axis_angle_from_rotation(m);
    return e.value.axis * e.value.angle;
}

// ---- The geodesic, which is the point of the whole lesson ----------------------

/// Interpolate from rotation `a` to rotation `b`, along the shortest path,
/// at constant angular speed.
///
/// **This is spherical linear interpolation**, and it is three lines because
/// axis-angle makes it three lines. Undo `a` to get the single turn that
/// separates the two poses, perform `t` of that turn, and apply it after `a`:
///
///     slerp(A, B, t) = A · R(n, t·θ)      where (n, θ) = axis-angle of Aᵀ·B
///
/// It is exactly right at both ends — `t = 0` gives A·I and `t = 1` gives
/// A·Aᵀ·B = B — and in between it is the geodesic of the rotation group, because
/// turning steadily about one fixed axis is what "straight line" means here.
/// Lesson 7.1 measured an Euler lerp taking 14% more turning than it needed on a
/// generic pair, and 209% near gimbal lock, with the speed varying by 1.56×
/// along the way. §9 runs this on the same pairs and measures **0.00% excess**
/// with a speed ratio of 1.000.
///
/// It always goes the short way round, for free: `axis_angle_from_rotation`
/// returns an angle in [0, π], so there is no long-way-round branch to forget.
/// Lesson 7.1 needed `shortest_angle_delta` for precisely that, per angle,
/// three times.
///
/// **The catch, which is Lesson 7.4's reason to exist.** Every call does a matrix
/// product, an `atan2`, a `sqrt`, and then two more trig calls to rebuild — for
/// ONE bone at ONE blend weight. A quaternion slerp is a dot product, two trig
/// calls, and a four-component lerp, with no matrix anywhere. Skeletal animation
/// blends hundreds of these per frame, which is why no shipping engine stores
/// rotations the way this function needs them stored. §10 prices it.
[[nodiscard]] inline mat3 rotation_slerp(const mat3& a, const mat3& b, float t)
{
    const axis_angle_extraction e = axis_angle_from_rotation(transpose(a) * b);
    return a * rotation_from_axis_angle({e.value.axis, e.value.angle * t});
}

} // namespace engine
