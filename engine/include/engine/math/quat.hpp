// engine/include/engine/math/quat.hpp — the rotations of space, as a number system.
//
// Lesson 7.4, and it is written to be read as a **diff against
// `math/complex.hpp`**. That file is not a prerequisite out of politeness: every
// function below has a twin there, the twins have the same names and the same
// shapes on purpose, and the only honest way to describe this header is to say
// what changed. Three things did.
//
//   ONE MORE IMAGINARY UNIT IS NOT ENOUGH. Lesson 7.3 §12.1 proved it rather
//   than asserted it — assume a three-dimensional system spanned by 1, i and j
//   with an associative product, ask where `ij` has to land, and three lines of
//   algebra demand a real number whose square is −1. There is none. So the
//   product `ij` is a NEW basis element, `k`, and the system has four components.
//   That proof is finished; this file starts where it stops.
//
//   MULTIPLICATION STOPS COMMUTING, and this is the price. `q*p` and `p*q` are
//   different quaternions describing different orientations, and §4 of the
//   lesson shows that the difference is exactly the cross product — the one term
//   in the product formula that changes sign when you swap the operands. That is
//   not a defect of the algebra. Rotations of space genuinely do not commute
//   (turn a book face-up 90° about two different axes in the two orders and the
//   spine ends up pointing two different ways), so an algebra that described them
//   AND commuted would be describing them wrongly.
//
//   THE ROTATION IS APPLIED FROM BOTH SIDES. In the plane, `z * v` rotates. Here
//   it is `q v conj(q)` — the "sandwich" — and Lesson 7.3 §12.3 already explained
//   why, from the other end: the two-sided form is the one that works in general,
//   and the plane can use a one-sided form only BECAUSE its algebra commutes,
//   which collapses `z v conj(z)` into the useless `|z|² v`. The sandwich is not
//   a complication introduced by the fourth dimension. It is the normal case, and
//   the plane was the special one.
//
// WHAT DID NOT CHANGE, which is more than you would expect: `conjugate` is still
// the inverse of a unit element, `length_squared` is still multiplicative,
// `renormalised_fast` is still the same two-term Taylor series about |q|² = 1,
// and the half-angle is still a half-angle for the reason 7.3 §7 gave with a
// protractor — a rotation is two reflections, and the object you build it out of
// carries half its angle. Nothing in this file had to invent the θ/2.
//
// WHY THE ENGINE WANTS IT, priced in Lesson 7.4 §10 rather than asserted here:
//
//   - **Storage.** Four floats against nine.
//   - **Composition.** 16 multiplies + 12 adds against 27 + 18.
//   - **Renormalisation is a division**, or with `renormalised_fast` not even
//     that. A `mat3` that has drifted needs Gram-Schmidt.
//   - **It can be interpolated.** Averaging two rotation matrices entrywise gives
//     something that is not a rotation. Lesson 7.5 added the bottom section of
//     this file, and it is three lines long because 7.2 and 7.3 had already done
//     the derivation twice in two other notations.
//
// And the honest entry, which is a cost rather than a saving and is worse here
// than it was in the plane: **applying a quaternion to a vector is more expensive
// than applying a matrix.** `mat3 * vec3` is 9 multiplies and 6 adds; the
// sandwich in its best-known form is 18 multiplies and 12 adds. Lesson 7.4 §10.3
// measures the crossover — how many vectors you have to rotate before building
// the matrix first pays for itself — and the answer is small. That is why a
// renderer handed quaternions converts them to matrices before it draws
// anything, and why `mat3_from_quat` below is not an afterthought.
//
// WHAT LESSON 7.5 ADDED, at the bottom: `nearest`, `quat_pow_unit`, `quat_slerp`
// and `quat_nlerp`. 7.4 left them out on purpose — the shortest-arc sign choice
// that the double cover (§8 below) forces has enough content to be a section
// rather than a line — and `angle_between` stayed, because it is a metric and not
// an interpolation, and because this file's own tests needed it before slerp did.
//
// Header-only, like the rest of `math/`: small, hot, stable code that every
// caller wants inlined.

#pragma once

#include <engine/math/axis_angle.hpp>
#include <engine/math/euler.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/vec3.hpp>

#include <algorithm>
#include <cmath>

namespace engine {

// ---- The type ----------------------------------------------------------------

/// A quaternion `w + x·i + y·j + z·k`, stored as a scalar and a vector.
///
/// **The storage is the derivation.** Lesson 7.4 §4 multiplies two quaternions
/// out and finds that the answer collects into
///
///     (w₁ + v₁)(w₂ + v₂) = (w₁w₂ − v₁·v₂,  w₁v₂ + w₂v₁ + v₁×v₂)
///
/// — a scalar part built from a product and a dot product, and a vector part
/// built from two scalings and a cross product. Splitting the four floats into
/// `w` and `v` is what makes `operator*` below readable as that formula instead
/// of as sixteen indexed products. **The dot product and the cross product are
/// both inside the quaternion product**, which is not a coincidence and is not
/// a repackaging: Gibbs and Heaviside extracted them from exactly this line in
/// the 1880s, and every vector-algebra course since has taught the pieces
/// without the thing they came out of.
///
/// In memory this is four contiguous floats in the order `w, x, y, z`, because
/// `vec3` is three contiguous floats and the scalar precedes it. Note that a
/// good deal of published code — and most GPU-side packing — uses `x, y, z, w`
/// instead. Neither order is more correct; they are incompatible, and code that
/// memcpy's a quaternion across that boundary without saying which it means has a
/// bug that looks like a 180° error about a diagonal axis. See the Conventions
/// page §8e.
///
/// **For a unit quaternion, `w = cos(θ/2)` and `v = sin(θ/2)·n̂`** for a rotation
/// of θ about the unit axis n̂. That is derived in §5 from two reflections and is
/// not a definition — see `quat_from_axis_angle`.
///
/// Defaults to `1 + 0i + 0j + 0k`: the number that multiplies by doing nothing,
/// which is also the rotation by nothing. A default-constructed orientation that
/// collapsed space would be as poor a surprise here as it is in `mat3`.
struct quat
{
    float w{1.0f};              ///< the real part — `cos(θ/2)` for a unit rotation
    vec3 v{0.0f, 0.0f, 0.0f};   ///< the imaginary part — `sin(θ/2)·n̂` for a unit rotation

    /// The rotation that does nothing: `1 + 0i + 0j + 0k`.
    ///
    /// A static member rather than a free function, for the reason `mat3` and
    /// `complex` both give in the same place: a free `identity()` taking no
    /// arguments could only be distinguished from the others by return type, and
    /// C++ cannot overload on that.
    [[nodiscard]] static constexpr quat identity() { return {}; }

    /// The three imaginary units, so that the multiplication table of §3 is one
    /// line of a test rather than a paragraph of a comment.
    ///
    /// Every one of them is a **half-turn**, not a quarter turn, and that is the
    /// first place the half-angle bites a newcomer. `complex::i()` is the quarter
    /// turn of the plane because the plane rotates one-sidedly; `quat::i()` has
    /// `w = cos(θ/2) = 0`, so θ = π. Multiplying by it twice gives −1, which is
    /// the same quaternion as +1 as far as any rotation is concerned — and that
    /// is the double cover, visible in the smallest possible example.
    [[nodiscard]] static constexpr quat i() { return {0.0f, {1.0f, 0.0f, 0.0f}}; }
    [[nodiscard]] static constexpr quat j() { return {0.0f, {0.0f, 1.0f, 0.0f}}; }
    [[nodiscard]] static constexpr quat k() { return {0.0f, {0.0f, 0.0f, 1.0f}}; }

    /// A **pure** quaternion: zero real part, so it is just a vector of space
    /// wearing a coat. This is how a point enters the sandwich.
    [[nodiscard]] static constexpr quat pure(vec3 p) { return {0.0f, p}; }
};

// ---- Arithmetic ---------------------------------------------------------------
//
// Addition is componentwise and unremarkable — four floats plus four floats.
// It is here because interpolation needs it (a lerp is two adds and a scale) and
// because `renormalised_fast` is a scale. What is worth slowing down for is the
// multiplication three blocks below.

[[nodiscard]] constexpr quat operator+(quat a, quat b) { return {a.w + b.w, a.v + b.v}; }
[[nodiscard]] constexpr quat operator-(quat a, quat b) { return {a.w - b.w, a.v - b.v}; }
[[nodiscard]] constexpr quat operator-(quat q)         { return {-q.w, -q.v}; }

[[nodiscard]] constexpr quat operator*(quat q, float s) { return {q.w * s, q.v * s}; }
[[nodiscard]] constexpr quat operator*(float s, quat q) { return q * s; }
[[nodiscard]] constexpr quat operator/(quat q, float s) { return {q.w / s, q.v / s}; }

/// The **Hamilton product** — the one operation this whole file is about.
///
/// **Derived, not memorised, and the derivation is four lines.** Multiply out
/// `(w₁ + v₁)(w₂ + v₂)` by distributing, exactly as you would for two binomials:
///
///     (w₁ + v₁)(w₂ + v₂) = w₁w₂ + w₁v₂ + v₁w₂ + v₁v₂
///
/// The first three terms are a real times a real and a real times a vector, which
/// need no rules. Everything interesting is in `v₁v₂`, the product of two pure
/// quaternions, and §4.1 of the lesson multiplies that out against the
/// multiplication table to get
///
///     v₁v₂ = −(v₁·v₂) + (v₁×v₂)
///
/// so the whole product is
///
///     (w₁w₂ − v₁·v₂,  w₁v₂ + w₂v₁ + v₁×v₂)
///
/// which is the code below, read straight across.
///
/// **It does not commute, and the non-commuting part is exactly one term.** Swap
/// the operands and `w₁w₂`, `v₁·v₂`, `w₁v₂ + w₂v₁` are all unchanged — only
/// `v₁×v₂` flips sign. So:
///
///     q·p − p·q = (0, 2·v₁×v₂)
///
/// Three statements, one fact: *quaternions do not commute*, *the cross product
/// is antisymmetric*, and *rotations of space do not commute*. §4.3 pushes real
/// numbers through by hand and §4.4 measures the gap in degrees on a pose you can
/// see, because "does not commute" is a sentence and "32.56° apart" is a
/// measurement.
///
/// **Composition order reads like matrices.** `q * p` means *do `p`, then do
/// `q`* — the same right-to-left reading as `A * B * v`, and `mat3_from_quat(q*p)
/// == mat3_from_quat(q) * mat3_from_quat(p)` exactly. That correspondence is not
/// free: it is why the sandwich is `q v conj(q)` and not `conj(q) v q`, and
/// §6.2 shows the other choice reverses every composition.
///
/// 16 multiplies and 12 adds. A `mat3` product is 27 and 18.
[[nodiscard]] constexpr quat operator*(quat a, quat b)
{
    return {a.w * b.w - dot(a.v, b.v),
            b.v * a.w + a.v * b.w + cross(a.v, b.v)};
}

[[nodiscard]] constexpr quat& operator*=(quat& a, quat b)
{
    a = a * b;
    return a;
}

[[nodiscard]] constexpr bool operator==(quat a, quat b) { return a.w == b.w && a.v == b.v; }
[[nodiscard]] constexpr bool operator!=(quat a, quat b) { return !(a == b); }

// ---- Conjugate, norm, inverse --------------------------------------------------
//
// Every function in this block is `complex.hpp`'s function with `im` widened to
// a `vec3`. Not "analogous to" — the same algebra, and the same proofs, because
// none of those proofs used commutativity. That is worth noticing while reading:
// the things that broke are exactly the things that mentioned the ORDER of a
// product, and nothing here does.

/// `w − v` — negate the imaginary part.
///
/// For a **unit** `q` this is the inverse rotation, which is the fact worth
/// keeping: undoing an orientation costs three sign flips. The proof is one line,
/// `q·conj(q) = |q|²`, and it is the same line as in the plane.
///
/// Geometrically, negating `v = sin(θ/2)n̂` can be read two ways — as reversing
/// the angle or as reversing the axis — and both are correct, which is Lesson
/// 7.2's `(n, θ) ≡ (−n, −θ)` ambiguity showing up as a sign.
[[nodiscard]] constexpr quat conjugate(quat q) { return {q.w, -q.v}; }

/// `|q|²`, which is `q · conj(q)` with the vector part guaranteed zero.
///
/// **It is multiplicative**, `|qp|² = |q|²|p|²`, and that one line is why unit
/// quaternions are closed under multiplication: compose two rotations and you get
/// a rotation, with no renormalisation step and no drift beyond rounding. It is
/// also the property that forces the multiplication table — §3.2 derives
/// anticommutativity from nothing but "every unit imaginary is a half-turn",
/// which is the same demand in geometric clothing.
[[nodiscard]] constexpr float length_squared(quat q) { return q.w * q.w + dot(q.v, q.v); }

/// `|q|` — the norm. For a rotation this is 1, and §10.4 measures how far it
/// drifts from 1 under a million compositions.
[[nodiscard]] inline float length(quat q) { return std::sqrt(length_squared(q)); }

/// The same rotation with norm exactly 1.
///
/// A zero quaternion has no direction and no angle, so — following `vec3`, and
/// `complex`, and for the identical reason — this returns the identity rather
/// than a NaN that would spread silently through a scene graph. "No rotation" is
/// the only answer that keeps a frame meaning what the caller intended.
[[nodiscard]] inline quat normalised(quat q)
{
    const float len_sq = length_squared(q);
    if (len_sq <= 0.0f)
    {
        return quat::identity();
    }
    return q / std::sqrt(len_sq);
}

/// As `normalised`, but a zero-norm input yields `fallback`.
[[nodiscard]] inline quat normalised_or(quat q, quat fallback)
{
    const float len_sq = length_squared(q);
    if (len_sq <= 0.0f)
    {
        return fallback;
    }
    return q / std::sqrt(len_sq);
}

/// Renormalise a nearly-unit quaternion without a square root.
///
/// **The identical trick, the identical proof, one dimension up — which is the
/// point.** If `|q|² = 1 + ε` for small ε, the factor we want is
/// `1/√(1+ε) ≈ 1 − ε/2`, and `1 − ε/2 = (3 − |q|²)/2`. One subtract and five
/// multiplies fix the drift with no `sqrt` and no divide. The residual is `O(ε²)`:
/// at `ε = 1e-3` it is under `4e-7`, below `float`'s resolution at 1.
///
/// The series is about `|q|² = 1` and knows nothing about dimension, so this
/// generalised without a character changing — and **so did its failure mode**.
/// Hand it `|q| = 2` and it returns something further from unit than it started.
/// Lesson 7.3 §9.4 found that a unit test on the MODULUS certifies this function
/// even when it is wrong, because at `|q| = 2` it returns norm exactly 1.000000
/// while turning the object 180° the wrong way. **A rotation is not its norm**,
/// and §10.5 keeps the test on the rotation for that reason.
///
/// Use it where a rotation is stepped incrementally and needs tidying every frame
/// (integrating angular velocity, Module 8), and `normalised` everywhere else.
[[nodiscard]] constexpr quat renormalised_fast(quat q)
{
    return q * (0.5f * (3.0f - length_squared(q)));
}

/// The multiplicative inverse: the `p` with `q * p == 1`.
///
/// `p = conj(q) / |q|²`, one line of algebra from `q · conj(q) = |q|²`. For a
/// **unit** `q` the division is by 1, so the inverse is just the conjugate —
/// undoing a rotation is free, and code that keeps its quaternions normalised
/// should call `conjugate` and say so.
///
/// Note that this really is a two-sided inverse even though the algebra does not
/// commute: `q·conj(q)` and `conj(q)·q` are both `|q|²`, because the cross
/// product of `v` with itself is zero. The one place non-commutativity would have
/// hurt most is the one place it cannot reach.
///
/// Returns the identity for `q = 0`, which has no inverse, on the same grounds as
/// `normalised`.
[[nodiscard]] inline quat inverse(quat q)
{
    const float len_sq = length_squared(q);
    if (len_sq <= 0.0f)
    {
        return quat::identity();
    }
    return conjugate(q) / len_sq;
}

// ---- Reflections: where the quaternion comes from --------------------------------
//
// Lesson 7.4 §5. This block is the derivation, not a utility, and it is short
// because Lesson 7.3 §7 did the hard half in the plane where it could be drawn.
// The claim there was that two mirrors φ apart generate a rotation of 2φ, so the
// object built from them carries HALF the angle. The claim here is that the same
// sentence is true in space with "line" replaced by "plane", and that carrying it
// out produces the quaternion with no further choices.

/// Reflect `v` in the plane through the origin whose **unit** normal is `n`,
/// written as a quaternion product.
///
/// **This function is `vec3::reflect(v, n)`**, which this engine has had since
/// Lesson 1.8, where it was derived for a bouncing ball and has been used for
/// mirror directions in `gfx/light.hpp` ever since. It is here in this spelling
/// because the spelling is the derivation: expand `n v n` for pure `n` and `v`
/// using §4's product formula twice and the terms collapse to
///
///     n v n = v − 2(n·v)n
///
/// which is that function, character for character. §5.1 does the expansion. Two
/// routes to one answer is a check; a single route is a hope, and §E.2 of the
/// harness measures the two against each other across a sweep.
///
/// Compare `complex.hpp`'s `reflect_in_line`, which is `m² conj(v)`. The shapes
/// differ because a mirror in the plane is named by its DIRECTION and a mirror in
/// space by its NORMAL, and because conjugating a pure quaternion negates it. The
/// content is the same: a reflection is one multiplication on each side.
///
/// `n` must be unit. A normal of length `s` scales the correction by `s²`, which
/// is `vec3::reflect`'s warning in the same words.
[[nodiscard]] constexpr vec3 reflect_in_plane(vec3 v, vec3 unit_normal)
{
    const quat n = quat::pure(unit_normal);
    return (n * quat::pure(v) * n).v;
}

/// The **rotor** taking the mirror plane `n0` to the mirror plane `n1`: the
/// half-angle object, and therefore the quaternion.
///
/// Reflect in the plane with normal `n0`, then in the plane with normal `n1`. Two
/// flips make a turn — the same fact you can check with two hands and a table in
/// two dimensions — and the composite is a rotation about the LINE where the two
/// planes meet, by twice the angle between them. §5.2 derives it:
///
///     v ↦ n₁(n₀ v n₀)n₁ = (n₁n₀) v (n₀n₁)     and   n₀n₁ = conj(n₁n₀)
///
/// so the whole thing is `q v conj(q)` with `q = n₁n₀`. **The sandwich was not
/// chosen. It fell out of composing two reflections**, and the right-hand factor
/// is a conjugate because reversing a product of pure quaternions is what
/// conjugation does.
///
/// Working out `q` itself gives the form everything else in this file assumes:
///
///     n₁n₀ = −(n₁·n₀) + n₁×n₀ = −cos φ − sin φ · n̂     (n̂ along n₀×n₁)
///
/// which is `−(cos φ + sin φ · n̂)`. Negate it — legal, because §8 shows `q` and
/// `−q` are the same rotation — and with θ = 2φ you have
///
///     q = cos(θ/2) + sin(θ/2)·n̂
///
/// **The double cover appeared inside the derivation**, as a sign nobody could
/// pin down, before it was ever stated as a property. That is the honest order.
///
/// Both normals must be unit. Parallel normals give the identity rotor (the two
/// mirrors coincide, and reflecting twice in one mirror does nothing).
[[nodiscard]] constexpr quat rotor_from_mirrors(vec3 n0, vec3 n1)
{
    return quat::pure(n1) * quat::pure(n0);
}

// ---- Applying a rotation: the sandwich --------------------------------------------

/// Rotate `v` by the **unit** quaternion `q`: `q v conj(q)`, read back as a vector.
///
/// **Why two-sided, in one sentence, because Lesson 7.3 §12.3 gave the long
/// version.** The plane can rotate one-sidedly precisely BECAUSE its algebra
/// commutes, which is also what makes `z v conj(z)` collapse to the useless
/// `|z|² v` there. Space does not commute, so the two factors do not cancel — and
/// what they do instead is rotate by twice the half-angle each of them carries.
/// The plane was the special case; this is the general one.
///
/// **Written out rather than as three products**, and the algebra is §6.1's.
/// Expanding `q v conj(q)` for unit `q` and collecting gives
///
///     v' = v + 2·w·(u × v) + 2·(u × (u × v))        with u = q.v
///
/// — Rodrigues' formula (Lesson 7.2 §5) in disguise, which it must be, since
/// both describe the same turn. Two crosses, two scales and two adds: **18
/// multiplies and 12 adds**, against 24 and 17 for the literal three-quaternion
/// route, and against 9 and 6 for `mat3 * vec3`.
///
/// **So this is the operation quaternions are WORSE at**, and the honest number
/// is the crossover: §10.3 measures how many vectors you must rotate by one
/// quaternion before `mat3_from_quat` plus a matrix multiply is cheaper, and the
/// answer is between two and three. Rotate one vector, use this. Rotate a mesh,
/// build the matrix. Lesson 7.3 predicted this trade in the plane, where the two
/// were merely equal; one dimension up, the equality becomes a loss.
///
/// **A NON-UNIT `q` DOES NOT SCALE THE RESULT. IT CORRUPTS IT**, and this is the
/// one place where the analogy with the plane breaks and an early draft of this
/// comment got it wrong. The literal `q v conj(q)` does scale by `|q|²` — that
/// much is the same statement as in `complex.hpp`. The form below is not the
/// literal one: getting from `(w² − |u|²)v + 2w(u×v) + 2(u·v)u` to the two
/// crosses above substitutes `u×(u×v) = (u·v)u − |u|²v`, and the two expressions
/// agree **only when `w² + |u|² = 1`**. Off the unit sphere this is a different
/// linear map — not a rotation, not a scaled rotation, not a similarity at all.
/// §10.5 measures it: at `|q| = 1.5` the pose it produces is **34.7° wrong**,
/// which is a great deal worse than being 2.25× too big, and it will not look
/// like a normalisation problem when you meet it.
///
/// Nothing here normalises for you, on the same grounds as everywhere else in
/// `math/`; what this paragraph buys is that the consequence is written down.
[[nodiscard]] constexpr vec3 rotate(quat q, vec3 v)
{
    const vec3 t = cross(q.v, v) * 2.0f;
    return v + t * q.w + cross(q.v, t);
}

/// Sugar, so a call site can read `q * v` where that is clearer.
///
/// Deliberately the same spelling `complex` uses for the same job, and
/// deliberately NOT the same thing as `q * p`: one takes a `vec3` and rotates it,
/// the other takes a `quat` and composes. In the plane those two were the same
/// function called with the operands read two ways, and that was the lesson
/// there. Here they are genuinely different code with different costs, and the
/// overload set is the place a reader is most likely to assume otherwise.
[[nodiscard]] constexpr vec3 operator*(quat q, vec3 v) { return rotate(q, v); }

// ---- Axes and angles: in and out ----------------------------------------------------

/// The unit quaternion for a turn of `angle` about `unit_axis`.
///
/// **`cos(θ/2) + sin(θ/2)·n̂`, and the halving is derived** — see
/// `rotor_from_mirrors` above, or Lesson 7.3 §7 for the same fact in a space you
/// can draw. It is not a normalisation convention and it is not an efficiency
/// trick; it is what "a rotation is two reflections" means once you write the
/// reflections down.
///
/// **This is the only trigonometry in the file**, together with its inverse below
/// and the Euler bridge that calls it, and noticing that is the point. Trig
/// appears exactly at the boundary where a human angle enters or leaves;
/// composing, inverting, applying and (in 7.5) interpolating are arithmetic.
/// Lesson 7.2 measured 82% of an axis-angle `slerp` call going on trig, and that
/// number is what this file exists to delete.
///
/// `unit_axis` must be unit — nothing here normalises it, on exactly the grounds
/// `axis_angle` gives at length: a non-unit axis does not give a slightly-wrong
/// rotation, it gives something that is not a rotation at all, and hiding a
/// `normalised()` in here would move the surprise somewhere the reader cannot see
/// it.
[[nodiscard]] inline quat quat_from_axis_angle(vec3 unit_axis, float angle)
{
    const float half = angle * 0.5f;
    return {std::cos(half), unit_axis * std::sin(half)};
}

/// The same, taking Lesson 7.2's struct.
[[nodiscard]] inline quat quat_from_axis_angle(axis_angle r)
{
    return quat_from_axis_angle(r.axis, r.angle);
}

/// Turns about the three coordinate axes — the twins of `mat3`'s `rotation_x`,
/// `rotation_y` and `rotation_z`, and the reason most call sites can move to
/// quaternions without getting longer.
///
/// `quat_y(a) * quat_x(b)` is the same orientation as
/// `rotation_y(a) * rotation_x(b)` and costs 16 multiplies instead of 27. The
/// spellings were kept deliberately parallel so that the diff at a call site is
/// four characters and the reader can see that nothing else moved.
///
/// Each is written out rather than delegating to `quat_from_axis_angle`, for the
/// same reason `mat3`'s three are written out: a literal basis vector multiplied
/// by a sine is two zeros the compiler must then be trusted to fold, and the
/// explicit form is both shorter to read and obviously free of them.
[[nodiscard]] inline quat quat_x(float radians)
{
    const float h = radians * 0.5f;
    return {std::cos(h), {std::sin(h), 0.0f, 0.0f}};
}

[[nodiscard]] inline quat quat_y(float radians)
{
    const float h = radians * 0.5f;
    return {std::cos(h), {0.0f, std::sin(h), 0.0f}};
}

[[nodiscard]] inline quat quat_z(float radians)
{
    const float h = radians * 0.5f;
    return {std::cos(h), {0.0f, 0.0f, std::sin(h)}};
}

/// The engine's Euler convention, as a quaternion.
///
/// **Intrinsic y-x-z, active, right-handed, radians** — Lesson 7.1's convention,
/// unchanged, and this function is `rotation_from_euler` with `rotation_*`
/// replaced by `quat_*` and nothing else touched. The two agree to float
/// precision (§C.4 measures it), which is the check that this file's product and
/// `mat3`'s product describe the same composition.
///
/// Euler angles remain an INTERFACE, exactly as 7.1 insisted: something a human
/// types or a file format stores, converted at the boundary and never kept.
/// Nothing in the engine stores a `euler_angles`, and nothing should start.
[[nodiscard]] inline quat quat_from_euler(euler_angles e)
{
    return quat_y(e.yaw) * quat_x(e.pitch) * quat_z(e.roll);
}

/// Recover the axis and angle `q` performs. The inverse of `quat_from_axis_angle`,
/// up to the two genuine ambiguities Lesson 7.2 named.
///
/// **`w = cos(θ/2)` and `|v| = sin(θ/2)`, so the angle is one `atan2`** — and
/// taking it that way rather than as `2·acos(w)` is the same argument
/// `angle_between_rotations` makes in `math/rotation.hpp`, one representation
/// removed. Near θ = 0, `w` is `1 − θ²/8` and a `float` cannot hold the change;
/// `|v|` is `θ/2` and holds it perfectly. Near θ = π it is the other way round,
/// and `atan2` is accurate at both ends because it is given both halves. §C.6
/// measures the two forms against a known answer and finds `acos` losing four
/// decimal places below a hundredth of a degree.
///
/// **The angle is returned in `[0, 2π)`, not `[0, π]`, and that is not a
/// mistake.** `atan2(|v|, w)` gives a half-angle in `[0, π]` because `|v| ≥ 0`,
/// so doubling covers a full turn — and a quaternion genuinely distinguishes
/// "turn 350° about n̂" from "turn 10° about −n̂", which a rotation matrix cannot.
/// That is the double cover being useful rather than annoying: it is exactly the
/// information an animation system needs in order to take the long way round on
/// purpose. Callers that want the canonical short form should negate `q` when
/// `q.w < 0` first, and §8.3 shows what that costs and what it buys.
///
/// The `axis_route` field carries the same meaning it does in
/// `axis_angle_from_rotation`, and there are only two cases here rather than
/// three: **the reversal route does not exist for quaternions.** Near θ = π the
/// matrix route is starved because the antisymmetric part it divides by vanishes;
/// `|v| = sin(θ/2)` is at its MAXIMUM there. The representation with no
/// singularity also has the unconditional extraction, and §9.3 measures both
/// across the range to show it is not an accident of the formula.
[[nodiscard]] inline axis_angle_extraction axis_angle_from_quat(quat q)
{
    axis_angle_extraction out;

    const float sin_half = length(q.v);
    const float half_angle = std::atan2(sin_half, q.w);

    out.value.angle = 2.0f * half_angle;
    out.sin_angle = std::sin(out.value.angle);

    if (out.value.angle < k_axis_angle_identity_angle)
    {
        // No turn, therefore no axis — and it is not a numerical failure that we
        // cannot find one. Every axis is a correct answer for θ = 0, which is the
        // same statement as "no axis is". The placeholder axis stays and the
        // route says so, exactly as the matrix version does.
        out.route = axis_route::no_axis;
        return out;
    }

    out.value.axis = q.v / sin_half;
    out.route = axis_route::general;
    return out;
}

/// The angle between two orientations, in `[0, π]` — the metric on rotations.
///
/// `conj(a)·b` is the single turn carrying one onto the other, and its angle is how
/// far apart they are. Its scalar part is exactly the four-component dot product
/// of `a` and `b` — one of the small miracles of this algebra, and the reason every
/// slerp implementation starts with a dot product — and its vector part is
/// `sin(θ/2)·n̂`, which is the half this function used to throw away.
///
/// **LESSON 7.5 REWROTE THIS, AND THE OLD VERSION WAS NOT MERELY LESS TIDY.**
/// 7.4 wrote it as `2·acos|a·b|`, which is the formula every reference gives and is
/// a *blind instrument near zero* — the same defect `axis_angle_from_quat`, eleven
/// lines above, already refuses in as many words. The cosine is flat at 1
/// (`cos(Ω) = 1 − Ω²/2`), so for a small separation the dot product lands within a
/// few `float` ulps of 1 and `acos` reads the angle off a quantity that has
/// already lost most of it. At `float` the noise floor is about 0.04°, and the
/// error is *biased*, not random.
///
/// That matters because a path integral asks this question thousands of times
/// with a small answer each time. **Lesson 7.5 §7.3 measured the old form summing
/// slerp's own geodesic to 8.01% SHORT of its endpoints' separation** over 4,096
/// steps — an instrument that fails a function for walking the path it is walking.
/// The `atan2` form below reports 0.00% on the same walk, because `|v|` is
/// *linear* in the angle where the cosine is quadratic.
///
/// Lesson 7.1 §6.4 reached the identical conclusion for matrices and
/// `angle_between_rotations` has used `atan2` since the day it was written. The
/// quaternion metric shipping the `acos` form for one lesson is the fourth
/// appearance of this trap in Module 7 and the first one inside the engine.
///
/// **`fabs` on the scalar part is the double cover, and leaving it out is the
/// single most common quaternion bug.** `a` and `−a` are the same orientation, so
/// the angle between them must be 0 and not 2π; without it, a pair of identical
/// poses reads as a full turn apart and an animation system built on it spins the
/// character all the way round between two adjacent keyframes. `quat_slerp` pays
/// for the same fact with the same one comparison, in `nearest`.
///
/// Both arguments must be unit. `atan2` needs no clamp: it takes a ratio of two
/// numbers rather than a cosine that float error can push past 1.
///
/// The cost is the honest part. `conj(a)·b`'s vector part is
/// `a.w·b.v − b.w·a.v − a.v×b.v`, so this is about twelve multiplies where the old
/// form was four. For a metric — called in tests, editors and path integrals, and
/// not per vertex — that is the right side of the trade, and `angle_between_by_cosine`
/// below is kept so the two can be measured against each other rather than argued about.
[[nodiscard]] inline float angle_between(quat a, quat b)
{
    const float scalar = a.w * b.w + dot(a.v, b.v);
    const vec3 imaginary = a.w * b.v - b.w * a.v - cross(a.v, b.v);
    return 2.0f * std::atan2(length(imaginary), std::fabs(scalar));
}

/// The same distance, computed the way every reference states it.
///
/// Kept **only** so that Lesson 7.5 §7.3 can measure the two against each other,
/// and kept in the engine rather than in a harness so that the comparison is
/// against the real thing. It is the right formula to have in your head and the
/// wrong one to call, exactly as `angle_between_rotations_by_trace` is for
/// matrices (`math/rotation.hpp`, Lesson 7.1). Nothing in the engine calls it.
[[nodiscard]] inline float angle_between_by_cosine(quat a, quat b)
{
    const float d = a.w * b.w + dot(a.v, b.v);
    return 2.0f * std::acos(std::clamp(std::fabs(d), 0.0f, 1.0f));
}

// ---- The bridge to mat3 ------------------------------------------------------------

/// The rotation matrix `q` performs.
///
/// **Built by asking where the three basis vectors land**, which is the definition
/// of a matrix this course has used since Lesson 2.5 and is the same construction
/// `rotation_from_axis_angle` uses. Column 0 is `rotate(q, x̂)`, column 1 is
/// `rotate(q, ŷ)`, column 2 is `rotate(q, ẑ)`, and `mat3` stores columns, so the
/// three results are the matrix with no rearrangement.
///
/// The closed form every reference gives — nine entries in `w`, `x`, `y`, `z`,
/// each a sum of two products — is the same nine floats and is written out below
/// in the comment rather than in the code:
///
///     | 1−2(y²+z²)   2(xy−wz)    2(xz+wy)  |
///     | 2(xy+wz)     1−2(x²+z²)  2(yz−wx)  |
///     | 2(xz−wy)     2(yz+wx)    1−2(x²+y²)|
///
/// We do not write it that way, and the reason is Lesson 7.2's in the same
/// position: nine transcribed products of components are not checkable by a
/// reader who has the derivation in their head, and three calls to a function
/// that has just been derived are. **Measured** at §10.2: the closed form is
/// 1.3× faster and both are a rounding error next to the `sin`/`cos` pair that
/// `quat_from_axis_angle` already paid. Where it matters — a skinning loop, an
/// instance buffer — write the closed form and measure it; here, readability is
/// nearly free.
///
/// **Requires a unit `q`.** A quaternion of norm `s` produces a matrix scaled by
/// `s²`, which is `determinant = s⁶` and not a rotation.
[[nodiscard]] constexpr mat3 mat3_from_quat(quat q)
{
    return {rotate(q, vec3{1.0f, 0.0f, 0.0f}),
            rotate(q, vec3{0.0f, 1.0f, 0.0f}),
            rotate(q, vec3{0.0f, 0.0f, 1.0f})};
}

/// The quaternion a rotation matrix performs — **Shepperd's method**.
///
/// **The naive route, and why it is not what is written here.** The trace of a
/// rotation matrix is `4w² − 1`, so `w = √(1 + tr)/2` and the other three
/// components fall out of the antisymmetric part divided by `4w`. That is four
/// lines and it is correct — until `w` is small, which is to say until the
/// rotation approaches a half-turn, where it divides by nearly nothing and then
/// loses the sign entirely at exactly θ = π. §9.2 measures it: at 179.99° the
/// naive route's round trip is off by **4.4°** while the one below is off by
/// 6.6e-05°, and at exactly 180° it returns the identity.
///
/// **The fix is Lesson 7.2's fix, and that is the point of teaching them in this
/// order.** That lesson pivoted on the largest of the axis's three squared
/// components so that it never divided by something near zero. Here there are
/// FOUR squared components to pivot on:
///
///     4w² = 1 + r₀₀ + r₁₁ + r₂₂          4x² = 1 + r₀₀ − r₁₁ − r₂₂
///     4y² = 1 − r₀₀ + r₁₁ − r₂₂          4z² = 1 − r₀₀ − r₁₁ + r₂₂
///
/// — each of them one trace-like combination of the diagonal, and the four sum
/// to 4 because the diagonal terms cancel. **So the largest is always at least
/// 1**, the divisor is always at least 2, and this routine has no bad case
/// anywhere on the range. Compare axis-angle, whose two routes had a crossover to
/// derive at 120° and a hole at 0 that no route could fill. Four candidates
/// instead of three is what buys that, and four candidates is what having a
/// fourth component means.
///
/// **The sign is arbitrary and that is correct.** `q` and `−q` name the same
/// matrix (§8), so no implementation can recover which one you started with. This
/// one returns the branch whose pivot component is positive, which is stable and
/// is not canonical — feed it `−q`'s matrix and you get `q` back. Code that
/// compares two quaternions for equality has to know this; code that rebuilds a
/// matrix from them does not.
///
/// **Requires a rotation.** Hand it a matrix carrying scale or shear and the
/// answer is quiet nonsense: the four candidates above are defined for any matrix
/// and this routine will report a quaternion for a shear without complaint. The
/// engine still has no `is_rotation` predicate to assert on; it is Lesson 7.1's
/// Exercise 5 and is deliberately still open.
[[nodiscard]] inline quat quat_from_rotation(const mat3& m)
{
    // Written with `at(row, col)` throughout rather than with column members, so
    // that a reader can check every index against the formulas above without
    // performing a mental transpose — mat3.hpp §3.3's trap, and the same choice
    // `axis_angle_from_rotation` makes for the same reason.
    const float r00 = m.at(0, 0);
    const float r11 = m.at(1, 1);
    const float r22 = m.at(2, 2);

    const float candidate[4] = {1.0f + r00 + r11 + r22,      // 4w²
                                1.0f + r00 - r11 - r22,      // 4x²
                                1.0f - r00 + r11 - r22,      // 4y²
                                1.0f - r00 - r11 + r22};     // 4z²

    int pivot = 0;
    for (int i = 1; i < 4; ++i)
    {
        if (candidate[i] > candidate[pivot]) { pivot = i; }
    }

    // `max` against zero for the same reason `axis_angle_from_rotation` clamps
    // before its `sqrt`: float error can push a candidate that should be exactly
    // zero a hair below it, and `sqrt` of a negative is a NaN. On this branch the
    // pivot is at least 1, so the guard can only ever fire on a matrix that was
    // not a rotation — which is precisely when a NaN would be least welcome.
    const float root = std::sqrt(std::max(candidate[pivot], 0.0f));   // 2·|component|
    const float scale = 0.5f / root;                                  // 1/(4·component)

    // The three off-diagonal differences and sums, named once. Each pair
    // (i, j) of the antisymmetric part is `2·w·component`, and each of the
    // symmetric part is `2·component_i·component_j` — so whichever component the
    // pivot gave us, the other three are one multiply away.
    const float d21 = m.at(2, 1) - m.at(1, 2);   // 4wx
    const float d02 = m.at(0, 2) - m.at(2, 0);   // 4wy
    const float d10 = m.at(1, 0) - m.at(0, 1);   // 4wz
    const float s10 = m.at(1, 0) + m.at(0, 1);   // 4xy
    const float s02 = m.at(0, 2) + m.at(2, 0);   // 4xz
    const float s21 = m.at(2, 1) + m.at(1, 2);   // 4yz

    switch (pivot)
    {
    case 0:  return {root * 0.5f, {d21 * scale, d02 * scale, d10 * scale}};
    case 1:  return {d21 * scale, {root * 0.5f, s10 * scale, s02 * scale}};
    case 2:  return {d02 * scale, {s10 * scale, root * 0.5f, s21 * scale}};
    default: return {d10 * scale, {s02 * scale, s21 * scale, root * 0.5f}};
    }
}

// ---- Interpolation: the geodesic -------------------------------------------
//
// Lesson 7.5, and almost nothing here is new — which is the finding, not an
// apology. Lesson 7.2 built `rotation_slerp` for `mat3` and measured that
// scaling the angle of a single turn traces the shortest path. Lesson 7.3 built
// `complex_slerp` in the plane and observed that the three steps it takes are
// not about the plane and are not about complex numbers:
//
//     slerp(a, b, t) = a · (a⁻¹ b)^t          undo a, take t of the difference,
//                                             redo a
//
// That is the definition of a geodesic on any group that has an inverse, a
// product and a power, and a unit quaternion has all three. So the body below is
// `complex_slerp`'s body with `quat` substituted, and the interesting content of
// this section is entirely in the two things that DID change.
//
//   THE POWER NEEDS AN AXIS, AND THE PLANE'S DID NOT. `complex_pow_unit` is
//   `complex_from_angle(t · arg z)` — two trig calls and no division, because in
//   the plane the axis is the same one every time. Here the axis has to be
//   recovered from `v` before the angle can be scaled, which is a divide by
//   `|v| = sin(θ/2)`, and §6.3 of the lesson asks the obvious question about
//   `θ → 0`. The answer is better than "guard it": the ratio the division feeds
//   is `sin(t·θ/2)/sin(θ/2)`, which tends to `t`, so the formula is CONTINUOUS
//   through the identity. The textbook form of slerp is not, and §6.4 measures
//   the difference.
//
//   THE DOUBLE COVER IS NOW HALF OF EVERY PAIR. In the plane `−z` is a rotation
//   by `θ + π` and is genuinely a different rotation; antipodal endpoints are a
//   measure-zero nuisance. Here `−q` IS `q` (§8 of Lesson 7.4, measured at
//   0.000e+00), so every pair of poses has TWO arcs between it whose lengths sum
//   to 2π, and one of them is the wrong one. `nearest` below is the entire fix
//   and it is one comparison — but it is not optional, and leaving it out is the
//   most common quaternion bug in shipped animation code.

/// Whichever of `q` and `−q` lies on the same half of the sphere as `reference`.
///
/// **This is the double cover being paid for, once, in one comparison.** `q` and
/// `−q` name the same orientation (Lesson 7.4 §8 — the sandwich is quadratic in
/// `q`, so a global sign cannot survive it, measured bitwise over 20,000
/// rotations). But they are not the same *point* on the 4-sphere, and every
/// interpolation in this file walks between points. Two poses therefore have two
/// arcs between them — one through `b` and one through `−b` — whose angles sum to
/// 2π, and exactly one of them is the short way.
///
/// The test is the four-component dot product, which is the cosine of the angle
/// between the two points and is the real part of `conj(a)·b`. Negative means the
/// half-angle exceeds 90°, which means the turn exceeds 180°, which means going
/// round the other way is shorter.
///
/// **The bug this prevents is worth naming precisely**, because it does not look
/// like a sign error when you meet it. An exporter writes keyframes as
/// quaternions and has no reason to keep their signs consistent; two adjacent
/// frames a degree apart can perfectly well be stored as `q` and `−q'`. Blend
/// them without this and the character takes the 359° route between two poses
/// that were a degree apart — a whole-body spin lasting exactly one keyframe
/// interval, in the middle of a walk cycle, which reads as "the animation is
/// corrupt" rather than as "someone forgot a dot product".
///
/// Both arguments should be unit; the comparison only cares about the sign, so a
/// positive scale on either cannot change the answer.
[[nodiscard]] constexpr quat nearest(quat reference, quat q)
{
    return (reference.w * q.w + dot(reference.v, q.v) < 0.0f) ? -q : q;
}

/// A **unit** quaternion raised to a real power: the rotation `q` performs,
/// scaled by `t`.
///
/// `t = 0.5` is the halfway rotation, `t = 2` the doubled one, `t = −1` the
/// inverse. The twin of `complex_pow_unit`, and the function `quat_slerp` is
/// built out of.
///
/// **Derivation, in one line each.** A unit `q` is `cos(θ/2) + sin(θ/2) n̂`
/// (Lesson 7.4 §5, from two reflections). Scaling the rotation by `t` means
/// scaling its angle, so the answer is `cos(tθ/2) + sin(tθ/2) n̂` — the same axis,
/// a fraction of the turn. Everything below is recovering `θ/2` and `n̂` from the
/// four floats and putting them back:
///
///     sin(θ/2) = |v|                    the vector part's length
///     θ/2      = atan2(|v|, w)          quadrant-correct, so θ ∈ [0, 2π)
///     n̂        = v / |v|                the axis
///
/// **`atan2` and not `acos(w)`, for the reason this course has now met three
/// times.** Near `θ = 0` the cosine is flat — `w = 1 − θ²/8` — so `acos` of a
/// float near 1 throws away most of the angle's significant digits, while
/// `atan2` reads the angle off a length that is *linear* in it. Lesson 7.1 §6.4
/// found this in a metric, 7.2 in an extraction, and 7.4 §G.3 in a test that
/// printed `0.000e+00` in every row because of it.
///
/// **The division by `|v|` is where the plane's version had nothing, and it is
/// safe for a reason rather than by a guard.** What the code actually computes is
/// `v · sin(tθ/2)/|v|`, and since `|v| = sin(θ/2)` the factor is
/// `sin(tθ/2)/sin(θ/2)`, which tends to `t` as `θ → 0` — finite, and exactly what
/// the limit should be, since a tiny rotation scaled by `t` is `t` times as tiny
/// about the same axis. So the formula is continuous through the identity and
/// needs no epsilon, no branch and no "if the angle is small, lerp instead". The
/// `sin_half <= 0` test below is there for the one input where the axis genuinely
/// does not exist, not for the small-angle case.
///
/// **The exact identity and its negative are the two inputs with no axis.** For
/// `q = 1` every axis is right, `θ = 0`, and the identity is the answer for every
/// `t`. For `q = −1` — a full turn about *something* — `θ = 2π`, and `q^t` is a
/// rotation of `2πt` about an axis nobody recorded; there is no continuous answer
/// and this returns the identity, which is correct only at `t = 0` and `t = 1`.
/// That is a placeholder on the same terms as `normalised(0)`, and the caller who
/// can produce it is the caller who has to say what it wants. `quat_slerp` cannot
/// reach either case with a `b` it has already passed through `nearest`.
[[nodiscard]] inline quat quat_pow_unit(quat q, float t)
{
    const float sin_half = length(q.v);
    if (sin_half <= 0.0f)
    {
        return quat::identity();
    }

    const float half_angle = std::atan2(sin_half, q.w);
    const float scaled = t * half_angle;
    return {std::cos(scaled), q.v * (std::sin(scaled) / sin_half)};
}

/// Spherical linear interpolation between two **unit** orientations: the shortest
/// path, at constant angular speed.
///
/// **Three lines, and they are Lesson 7.3's three lines.** `conj(a)·b` is the
/// single turn carrying `a` onto `b`; `quat_pow_unit(·, t)` is a fraction of it;
/// multiplying back onto `a` starts the journey where it should start. Read
/// aloud: *undo a, take t of the difference, redo a.*
///
///     slerp(a, b, t) = a · (a⁻¹ b)^t
///
/// `a⁻¹` is `conj(a)` because `a` is unit (Lesson 7.4's `inverse`), so the whole
/// function is a conjugate, a power and two products.
///
/// **`nearest` is the only thing here that the plane did not need**, and it is
/// what makes this the SHORT way. Without it the arc taken is whichever one the
/// signs of the inputs happen to name, and half of all input pairs name the long
/// one. See `nearest` for the bug that produces.
///
/// **Constant angular speed, exactly.** The angle covered by time `t` is
/// `t · angle_between(a, b)`, linear in `t` by construction rather than by
/// accident: the power scales an angle, and nothing else in the expression
/// depends on `t`. §7.2 of the lesson measures it against a sweep and finds no
/// variation beyond rounding.
///
/// **The path is a great circle on the unit 4-sphere, which is the definition of
/// the geodesic** — and Lesson 7.2 already measured what that buys on `mat3`
/// (0.00% of excess turning, against Euler interpolation's +14% on a generic pair
/// and +209% near lock). Nothing about that argument mentioned dimension, so it
/// carries here unchanged, and §7.3 checks it rather than re-deriving it.
///
/// **Not clamped.** `t` outside `[0, 1]` extrapolates along the same great
/// circle, which is occasionally exactly what a caller wants (overshoot on an
/// ease-out, a one-frame prediction) and is never what it wants by accident.
/// Clamping here would make the useful case impossible and hide the accidental
/// one; the engine's convention, since `lerp` in Lesson 1.6, is that the caller
/// owns its parameter.
///
/// Both inputs must be unit. Non-unit inputs do not fail loudly — `quat_pow_unit`
/// reads an angle off a direction and ignores the modulus, so the answer is a
/// rotation, just not between the two you meant.
[[nodiscard]] inline quat quat_slerp(quat a, quat b, float t)
{
    return a * quat_pow_unit(conjugate(a) * nearest(a, b), t);
}

/// Normalised linear interpolation: the straight chord between them, pushed back
/// onto the sphere.
///
/// **The right path on the wrong schedule, and both halves of that were proved in
/// the plane** (Lesson 7.3 §10.3) by an argument that never mentions dimension.
///
///   THE PATH IS EXACT. The chord from `a` to `b` lies in the 2-plane those two
///   points span, normalising moves a point along its own radius, and a radius
///   through a point of that 2-plane stays in it. So every point this function
///   returns is on the great circle through `a` and `b` — the same curve
///   `quat_slerp` walks. Excess turning: 0.00%, measured in §7.3 on the same
///   pairs and with the same instrument as the slerp row.
///
///   THE SCHEDULE IS WRONG BY EXACTLY `sec²(Ω/2)`, the ratio between its speed at
///   the midpoint and at the ends for an arc of `Ω`. It moves fastest through the
///   middle and slows at both ends, because equal steps along a chord subtend
///   unequal angles at the centre.
///
/// **The number to design with is the gap in degrees at the same `t`**, which
/// §7.4 measures for quaternion arcs: 0.13° at 30°, 4.07° at 90°, 26.34° at 150°.
/// Under about 30° of arc — which is where a 30 Hz animation clip's adjacent
/// keyframes live — nlerp is indistinguishable from slerp and costs no
/// trigonometry at all, which is why a great deal of shipped animation code uses
/// it and is right to. Over about 90° it lurches visibly through its middle.
///
/// `nearest` applies here for the identical reason it applies to `quat_slerp`,
/// and its absence is louder: the chord between antipodal representatives passes
/// through the *origin*, where `normalised` has nothing to return.
///
/// Truly antipodal inputs (`b == −a` after `nearest`, i.e. `a` blended with the
/// same pose reached the long way) have no shortest arc and no defined midpoint;
/// `normalised` returns the identity rather than a NaN, which is a placeholder and
/// not an answer. `nearest` makes this unreachable from any pair of distinct
/// orientations.
[[nodiscard]] inline quat quat_nlerp(quat a, quat b, float t)
{
    const quat to = nearest(a, b);
    return normalised(quat{a.w + (to.w - a.w) * t,
                           a.v + (to.v - a.v) * t});
}

} // namespace engine
