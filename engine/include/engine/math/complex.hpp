// engine/include/engine/math/complex.hpp — the rotations of the plane, as a
// number system.
//
// Lesson 7.3. Lesson 7.2 ended with a list of things axis-angle cannot do, and
// the first item on it was the worst: **there is no usable formula for composing
// two turns.** Given the axis and angle of one rotation and the axis and angle
// of another, finding the axis and angle of "do both" means building two
// matrices, multiplying them, and extracting again — three trigonometric round
// trips to answer a question that ought to be one multiplication.
//
// In the plane, it IS one multiplication, and this file is that fact made into a
// type. Two rotations of the plane compose by multiplying two pairs of floats:
// four multiplies, two adds, no trigonometry anywhere. Not "can be made to"
// compose that way after a clever encoding — the arithmetic of complex numbers,
// invented for entirely unrelated reasons two centuries before anyone drew a
// polygon, IS the arithmetic of turning and scaling the plane.
//
// WHY THE TYPE IS NAMED FOR THE NUMBER AND NOT FOR THE ROTATION. `rot2` would be
// a perfectly good name and the engine would read slightly better for it. It is
// the wrong name here, because the one thing this lesson has to establish is
// that the number and the rotation are the SAME OBJECT, and a rotation-flavoured
// name asks the reader to accept that identification on faith instead of
// watching it happen. `complex` also keeps the file honest about what it is:
// every operation below is ordinary complex arithmetic, checkable against any
// algebra textbook, and nothing in it was bent to make it geometric.
//
// WHY IT EXISTS AT ALL, given that `mat2` already rotates the plane. Four
// reasons, and each is measured in Lesson 7.3 §9 rather than asserted here:
//
//   - **Half the storage.** Two floats against four.
//   - **Half the composition.** 4 multiplies + 2 adds against 8 + 4.
//   - **Renormalisation is a division.** A `mat2` that has drifted needs
//     Gram-Schmidt to become a rotation again; a `complex` needs one divide, and
//     `renormalised_fast` below does it without even that.
//   - **It can be interpolated.** Averaging two rotation matrices entrywise
//     gives something that is not a rotation. Averaging two unit complex numbers
//     and renormalising gives a rotation on the arc between them — which is the
//     whole of §10, and the reason `complex_slerp` is three lines.
//
// What it does NOT buy: applying a rotation to a vector. `z * v` and `m * v` are
// both four multiplies and two adds, and they are the same four multiplies. This
// representation is cheaper to STORE and to COMBINE, and exactly as expensive to
// USE. That trade reappears one dimension up, with the same shape and different
// numbers, and Lesson 7.4 measures it there.
//
// THIS FILE IS THE TEMPLATE FOR `math/quat.hpp`. Lesson 7.4 builds quaternions,
// and it builds them by analogy with this header, function for function:
// `conjugate`, `length_squared`, `normalised`, `inverse`, `operator*`,
// `complex_slerp` — each has a quaternion twin whose body is the same idea with
// one more imaginary unit and one crucial loss (multiplication stops commuting).
// The names and shapes below were chosen with that mapping in mind, so that the
// harder file can be read as a diff against the easier one.
//
// Header-only, like the rest of `math/`: small, hot, stable code that every
// caller wants inlined.

#pragma once

#include <engine/math/mat2.hpp>
#include <engine/math/vec2.hpp>

#include <cmath>

namespace engine {

// ---- The type ----------------------------------------------------------------

/// A complex number `re + im·i` — equivalently, a rotation-and-scale of the plane.
///
/// **The two readings are the same object, and switching between them at will is
/// the skill this file exists to teach.** Read `z` as a number and `z * w` is
/// multiplication. Read `z` as a transformation and `z * w` is "do `w`, then do
/// `z`". Read `z` as a point and it is the place `(re, im)`. Nothing converts;
/// the three readings are three sentences about one pair of floats.
///
/// **Why `i² = −1` is a geometric statement.** Take the transformation "turn a
/// quarter circle anticlockwise". Apply it to `(1,0)` and you get `(0,1)`; apply
/// it again and you get `(−1,0)`. So performing a quarter turn twice is exactly
/// multiplication by `−1`. Call that quarter turn `i`, and `i² = −1` is not a
/// rule imposed on the algebra to make it close — it is a measurement of the
/// plane. Lesson 7.3 §3 derives the rest of the multiplication table from that
/// one observation and nothing else.
///
/// Defaults to `1 + 0i`, the number that multiplies by doing nothing, which is
/// also the rotation by zero. A default-constructed rotation that collapsed the
/// plane would be as poor a surprise here as it is in `mat2`.
struct complex
{
    float re{1.0f};   ///< the real part — also the x coordinate, also cos θ for a unit z
    float im{0.0f};   ///< the imaginary part — also the y coordinate, also sin θ

    /// The rotation that does nothing: `1 + 0i`.
    ///
    /// A static member rather than a free function, for the reason `mat2` gives
    /// in the same place: a free `identity()` taking no arguments could only be
    /// distinguished from `mat2::identity()` by return type, and C++ cannot
    /// overload on that.
    [[nodiscard]] static constexpr complex identity() { return {}; }

    /// The quarter turn. `i * i == -1`, which you can and should check.
    ///
    /// Present because the single most useful thing to be able to type while
    /// reading this file is `complex::i()`, and because a named constant makes
    /// the check above one line of a test rather than a paragraph of a comment.
    [[nodiscard]] static constexpr complex i() { return {0.0f, 1.0f}; }
};

// ---- Arithmetic ---------------------------------------------------------------
//
// Addition is componentwise and unremarkable — it is vector addition, because
// the plane's points and the plane's complex numbers are the same set. What is
// worth slowing down for is the multiplication two blocks below.

[[nodiscard]] constexpr complex operator+(complex a, complex b)
{
    return {a.re + b.re, a.im + b.im};
}

[[nodiscard]] constexpr complex operator-(complex a, complex b)
{
    return {a.re - b.re, a.im - b.im};
}

[[nodiscard]] constexpr complex operator-(complex z) { return {-z.re, -z.im}; }

[[nodiscard]] constexpr complex operator*(complex z, float s)
{
    return {z.re * s, z.im * s};
}

[[nodiscard]] constexpr complex operator*(float s, complex z) { return z * s; }

[[nodiscard]] constexpr complex operator/(complex z, float s)
{
    return {z.re / s, z.im / s};
}

/// The product — the one operation this whole file is about.
///
/// **Derived, not memorised.** Multiply out `(a + bi)(c + di)` treating `i` as an
/// ordinary symbol, then use `i² = −1` exactly once:
///
///     (a + bi)(c + di) = ac + adi + bci + bd·i²
///                     = (ac − bd) + (ad + bc)i
///
/// That is the code below, and the minus sign — the only surprising character in
/// it — is `i²` and nothing else.
///
/// **What it does geometrically: moduli multiply, arguments add.** Lesson 7.3
/// §4 proves it, and §4.3 pushes real numbers through it by hand:
/// `(3 + 4i)(1 + 2i) = −5 + 10i`, where `|3+4i| = 5` and `|1+2i| = √5` and
/// `|−5+10i| = 5√5`, while `53.130° + 63.435° = 116.565°` is the argument of the
/// answer to five decimal places. **Composing two rotations of the plane is this
/// function, and it spends no trigonometry at all** — which is precisely what
/// axis-angle could not do (Lesson 7.2 §10).
///
/// **It commutes, and that will turn out to matter enormously.** `z * w == w * z`
/// here, exactly, because both sides are the same two products of the same four
/// floats. That is fine in the plane, where rotations genuinely do commute — turn
/// 30° then 50° or 50° then 30°, you land in the same place. It is *false* of
/// rotations in space, and so the algebra that describes them cannot commute
/// either. Lesson 7.4 opens on exactly that point: the fourth dimension is not
/// Hamilton's flourish, it is forced, and commutativity is what has to be given
/// up to get it.
[[nodiscard]] constexpr complex operator*(complex z, complex w)
{
    return {z.re * w.re - z.im * w.im,
            z.re * w.im + z.im * w.re};
}

[[nodiscard]] constexpr complex& operator*=(complex& z, complex w)
{
    z = z * w;
    return z;
}

[[nodiscard]] constexpr bool operator==(complex a, complex b)
{
    return a.re == b.re && a.im == b.im;
}

[[nodiscard]] constexpr bool operator!=(complex a, complex b) { return !(a == b); }

// ---- The plane, as points and as numbers ---------------------------------------
//
// These two are reinterpretations, not conversions: no arithmetic happens, and
// both compile to nothing. They exist so that a call site can SAY which reading
// it means, which is worth two function calls that cost zero instructions.

/// The point `(re, im)`, as a `vec2`.
[[nodiscard]] constexpr vec2 vec2_from_complex(complex z) { return {z.re, z.im}; }

/// The number `v.x + v.y·i`, as a `complex`.
[[nodiscard]] constexpr complex complex_from_vec2(vec2 v) { return {v.x, v.y}; }

/// Rotate (and scale) a point of the plane by `z`: the number `z` times the
/// number `v`, read back as a point.
///
/// **This is the same arithmetic as `operator*(complex, complex)`, and that is
/// the lesson.** A rotation acting on a vector and a rotation composed with a
/// rotation are not two operations that happen to resemble each other — in this
/// representation they are one function called with the operands read two ways.
/// Compare `mat2`, where `m * v` and `m * n` are genuinely different code with
/// different costs.
///
/// Cost note, because it is easy to assume otherwise: this is four multiplies
/// and two adds, which is exactly what `mat2 * vec2` costs. Nothing is saved
/// here. The savings are in storing and combining (see the file header).
[[nodiscard]] constexpr vec2 operator*(complex z, vec2 v)
{
    return vec2_from_complex(z * complex_from_vec2(v));
}

// ---- Conjugate, modulus, inverse -----------------------------------------------

/// `re − im·i` — the reflection of `z` in the real axis.
///
/// For a **unit** `z` this is the inverse rotation, which is the fact worth
/// keeping: undoing a turn of the plane costs one sign flip. `conjugate` is also
/// how reflections are written (see `reflect_in_line`), and the two facts are the
/// same fact — Lesson 7.3 §7 shows that conjugation IS the mirror in the x axis,
/// and that every other mirror is that one with a rotation on either side.
[[nodiscard]] constexpr complex conjugate(complex z) { return {z.re, -z.im}; }

/// `|z|²` — the squared modulus, which is `z * conjugate(z)` with the imaginary
/// part guaranteed zero.
///
/// Named to match `vec2`'s `length_squared`, and it is the same number: the
/// squared distance from the origin to the point `(re, im)`.
///
/// **It is multiplicative**, `|zw|² = |z|²|w|²`, and that one line is why unit
/// complex numbers are closed under multiplication — compose two rotations and
/// you get a rotation, with no renormalisation step and no drift beyond rounding.
[[nodiscard]] constexpr float length_squared(complex z) { return z.re * z.re + z.im * z.im; }

/// `|z|` — the modulus, the scale factor the multiplication applies.
[[nodiscard]] inline float length(complex z) { return std::sqrt(length_squared(z)); }

/// The same rotation with modulus exactly 1.
///
/// A zero-length input has no direction and no argument, so — following
/// `vec2::normalised`, and for the identical reason — this returns the identity
/// rotation rather than a NaN that would spread silently. "No rotation" is the
/// only answer that keeps a frame meaning what the caller intended.
[[nodiscard]] inline complex normalised(complex z)
{
    const float len_sq = length_squared(z);
    if (len_sq <= 0.0f)
    {
        return complex::identity();
    }
    return z / std::sqrt(len_sq);
}

/// As `normalised`, but a zero-length input yields `fallback`.
[[nodiscard]] inline complex normalised_or(complex z, complex fallback)
{
    const float len_sq = length_squared(z);
    if (len_sq <= 0.0f)
    {
        return fallback;
    }
    return z / std::sqrt(len_sq);
}

/// Renormalise a nearly-unit rotation without a square root.
///
/// **The trick, in one line of algebra.** If `|z|² = 1 + ε` for small `ε`, then
/// the factor we want is `1/√(1+ε) ≈ 1 − ε/2`, and `1 − ε/2 = (3 − |z|²)/2`.
/// So one subtract, one multiply and two more multiplies fix the drift, with no
/// `sqrt` and no divide. The error left behind is `O(ε²)`: at `ε = 1e-3` the
/// residual is under `4e-7`, which is below `float`'s resolution at 1.
///
/// **Only for numbers already close to unit length.** Hand it `|z| = 2` and it
/// returns something further from unit than it started — the approximation is a
/// Taylor series about `|z|² = 1` and it is doing exactly what it promised. The
/// engine uses it where a rotation is stepped incrementally and needs tidying
/// every frame (Lesson 7.3 §9.4 measures the drift it is there to absorb), and
/// `normalised` everywhere else.
///
/// Lesson 7.4 will want the same function for quaternions, one dimension up and
/// with the same proof.
[[nodiscard]] constexpr complex renormalised_fast(complex z)
{
    return z * (0.5f * (3.0f - length_squared(z)));
}

/// The multiplicative inverse: the `w` with `z * w == 1`.
///
/// `w = conjugate(z) / |z|²`, which is one line of algebra from
/// `z · conjugate(z) = |z|²`. For a **unit** `z` the division is by 1 and the
/// inverse is just the conjugate — so undoing a rotation is free, and code that
/// keeps its rotations normalised should call `conjugate` and say so.
///
/// Returns the identity for `z = 0`, which has no inverse, on the same grounds
/// as `normalised`: a NaN here would surface three subsystems away.
[[nodiscard]] inline complex inverse(complex z)
{
    const float len_sq = length_squared(z);
    if (len_sq <= 0.0f)
    {
        return complex::identity();
    }
    return conjugate(z) / len_sq;
}

// ---- Angles: in and out ---------------------------------------------------------

/// The unit complex number for a turn of `radians` — `cos θ + i·sin θ`.
///
/// **This is the only trigonometry in the file**, and noticing that is the point.
/// Trig appears exactly at the boundary where a human angle enters or leaves;
/// everything in between — composing, inverting, interpolating, applying — is
/// arithmetic. Axis-angle could not make that separation (Lesson 7.2 §10
/// measured 82% of a `slerp` call going on trig), and `mat2` makes it only for
/// composition.
///
/// Anticlockwise in a y-up space, clockwise on screen in framebuffer coordinates
/// where +y is down. Same numbers, flipped picture — see `mat2`'s `rotation` for
/// the same warning in the same words, and the Conventions page §8d.
[[nodiscard]] inline complex complex_from_angle(float radians)
{
    return {std::cos(radians), std::sin(radians)};
}

/// The turn `z` performs, in radians, in `(−π, π]`.
///
/// **Signed**, unlike the 3-D metric in `math/rotation.hpp`, and that difference
/// is real rather than an inconsistency: the plane has an orientation, so
/// "30° clockwise" and "30° anticlockwise" are different rotations and a single
/// number can say which. Space does not — a rotation of space is a turn about an
/// axis, and swapping the axis for its negative and the angle for its negative
/// gives the same rotation, so the angle alone carries no sign to report.
///
/// `atan2` handles all four quadrants and the modulus is irrelevant to it, so an
/// un-normalised `z` gives the right angle anyway.
[[nodiscard]] inline float angle_from_complex(complex z)
{
    return std::atan2(z.im, z.re);
}

/// The signed turn from `a` to `b`, in `(−π, π]` — the plane's distance function
/// on rotations.
///
/// `conjugate(a)` undoes `a` and `b` then performs `b`, so `conjugate(a) * b` is
/// the single rotation carrying one onto the other, and its argument is how far
/// that is. Compare `angle_between_rotations` in `math/rotation.hpp`, which is
/// the same idea for `mat3` and returns an unsigned angle in `[0, π]` for the
/// reason `angle_from_complex` gives.
///
/// Assumes `a` is unit; if it is not, the answer is still the correct angle,
/// because a positive scale factor cannot change an argument.
[[nodiscard]] inline float angle_between(complex a, complex b)
{
    return angle_from_complex(conjugate(a) * b);
}

/// `z` raised to a real power — the rotation `z` performs, scaled by `t`.
///
/// For a **unit** `z` this is `complex_from_angle(t · arg z)`: do a fraction of
/// the turn. `t = 0.5` is the halfway rotation, `t = 2` is the doubled one, and
/// `t = −1` is the inverse. This is the function `complex_slerp` is built out of,
/// and it is *identically* the function quaternion slerp is built out of one
/// dimension up.
///
/// **Restricted to unit `z` on purpose.** The general `z^t = |z|^t · e^{i·t·arg z}`
/// needs a `pow` and raises a branch-cut question this engine has no use for, and
/// every caller in the course is interpolating a rotation. The modulus is
/// ignored, not honoured: hand it a non-unit `z` and you get a unit answer.
[[nodiscard]] inline complex complex_pow_unit(complex z, float t)
{
    return complex_from_angle(t * angle_from_complex(z));
}

// ---- Reflections, and where the half-angle comes from ----------------------------
//
// Lesson 7.3 §7. This block is short and it is the most important thing in the
// file, because it is where the `θ/2` that Lesson 7.4's quaternions are built out
// of stops being a convention and becomes a consequence.

/// Reflect the point `v` in the line through the origin along the unit `mirror`.
///
/// **Derivation, in two moves.** Conjugation `v ↦ v̄` reflects in the x axis: it
/// flips the sign of y and leaves x alone, which is the definition. Every other
/// mirror is the x axis in disguise — rotate the plane so the mirror lands on the
/// x axis, conjugate, rotate back — three steps, left to right:
///
///     turn the mirror onto the x axis      conj(m) · v
///     reflect in the x axis                conj( conj(m) · v ) = m · v̄
///     turn the mirror back                 m · ( m · v̄ )      = m² · v̄
///
/// so a mirror is `m² v̄`, one multiply and a sign flip. **Note the `m²`.** The
/// mirror's own angle enters doubled, which is the first appearance in this
/// course of a half-angle object: to build a mirror at `α` you store `e^{iα}` and
/// the arithmetic squares it for you.
///
/// `mirror` must be unit; a non-unit one scales the result by `|m|²`. The
/// equivalent spelling through `vec2`'s `reflect` — which takes the mirror's
/// *normal* rather than its direction — is
/// `reflect(v, perpendicular(mirror))`, and Lesson 7.3 §7.4 measures the two
/// against each other across a sweep, because two routes to one answer is a
/// check and a single route is a hope.
[[nodiscard]] constexpr vec2 reflect_in_line(vec2 v, complex mirror)
{
    return vec2_from_complex(mirror * mirror * conjugate(complex_from_vec2(v)));
}

/// The **rotor** taking mirror `m0` to mirror `m1`: the half-angle object.
///
/// Reflect in `m0`, then reflect in `m1`, and the result is a rotation — two
/// flips make a turn, which you can check with two hands and a table. The
/// surprise is the angle: if the mirrors are `φ` apart, the rotation is by
/// **2φ**, not by `φ`. Lesson 7.3 §7.2 derives it in one line by composing the
/// two `m² v̄` forms, and §7.3 walks a numeric example round by hand: mirrors at
/// 20° and 50° rotate by 60°.
///
/// So the object this returns — `m1 · conj(m0)`, the rotation from one mirror to
/// the other — carries **half** the angle of the rotation it generates, and the
/// rotation itself is `R * R`. That is the entire origin of the `θ/2` in a
/// quaternion, and it is visible here in the plane with a ruler.
///
/// **It also explains the double cover before you meet it.** `R` and `−R` are
/// different rotors — `−R` is `R` with an extra half-turn, a different pair of
/// mirrors — and they produce the *same* rotation, because `(−R)² = R²`. Two
/// rotors per rotation, exactly. In the plane you can dodge this by writing
/// rotations one-sidedly as `z * v` and never mentioning mirrors; in space there
/// is no one-sided form to dodge into, which is why every quaternion library has
/// a `-q == q` footnote and this file has this paragraph.
[[nodiscard]] constexpr complex rotor_from_mirrors(complex m0, complex m1)
{
    return m1 * conjugate(m0);
}

/// Apply a rotor: the rotation it generates is `r` twice over.
///
/// Kept as a named function rather than left as `r * r * v` at call sites,
/// because the name is the documentation — a rotor is not a rotation, it is a
/// square root of one, and code that multiplies by it once has a bug that looks
/// like a halved turn rate.
[[nodiscard]] constexpr vec2 apply_rotor(complex r, vec2 v) { return (r * r) * v; }

// ---- The bridge to mat2 ----------------------------------------------------------

/// The `mat2` that multiplies by `z`.
///
/// **This is the proof that complex numbers were never a separate thing.** The
/// columns of a matrix are where the basis vectors land (Lesson 2.5), so build
/// the matrix of "multiply by `a + bi`" by multiplying the two basis vectors:
/// `1` lands on `a + bi`, and `i` lands on `(a+bi)i = −b + ai`. Written out:
///
///     | a  −b |
///     | b   a |
///
/// Every such matrix is a rotation-and-scale, every rotation-and-scale is such a
/// matrix, and the correspondence respects multiplication — so `complex` and
/// this two-parameter family of `mat2` are the same algebra with different
/// storage. Setting `a = cos θ`, `b = sin θ` recovers `mat2`'s `rotation`
/// character for character, which Lesson 7.3 §5 checks entry by entry.
///
/// Also the honest cost statement: this is where the other two floats come from
/// and why they are redundant. A `mat2` holding a rotation stores `cos θ` and
/// `sin θ` twice, with one sign flipped.
[[nodiscard]] constexpr mat2 mat2_from_complex(complex z)
{
    return {{z.re, z.im}, {-z.im, z.re}};
}

/// The complex number a rotation-and-scale `m` multiplies by.
///
/// Reads the first column, which by the argument above is the image of `1` and
/// therefore is the number itself. **It does not check that `m` is of that
/// form**: a general `mat2` also shears and reflects, and neither is a complex
/// multiplication. Handed one, this returns the nearest thing it can see and
/// silently discards the rest — so it is for round-tripping rotations, and
/// Lesson 7.3 §5.3 measures what it does to a shear so the failure is on record
/// rather than a surprise.
[[nodiscard]] constexpr complex complex_from_mat2(const mat2& m) { return {m.c0.x, m.c0.y}; }

// ---- Interpolation: the plane's version of the whole of Module 7 ------------------
//
// Lesson 7.3 §10. Lesson 7.2 established the vocabulary on `mat3` — a geodesic
// is the shortest path at constant speed, and `rotation_slerp` walks one. In the
// plane the same three behaviours are separable and each is visible:
//
//   EULER LERP   wrong path, wrong speed.  (Lesson 7.1: +14% to +209% of excess
//                turning. It has no analogue here, because one angle has no
//                order to get wrong — which is itself the finding.)
//   NLERP        RIGHT path, wrong speed.  Straight line through the disc, pushed
//                back out to the circle. It cannot leave the arc, so it wastes no
//                turning at all; it just arrives at the wrong time.
//   SLERP        right path, right speed.
//
// The middle row is the one people get wrong in both directions — some treat
// nlerp as a cheap approximation that drifts off course, others as a free lunch.
// It is neither, and §10.3 puts a closed form on exactly what it costs.

/// Spherical linear interpolation between two **unit** rotations of the plane.
///
/// **Three lines, and the middle one is the whole idea.** `conjugate(a) * b` is
/// the single rotation carrying `a` onto `b`; `complex_pow_unit(·, t)` is a
/// fraction of it; multiplying back onto `a` starts the journey from where it
/// should start. Read aloud: *undo a, take t of the difference, redo a.*
///
///     slerp(a, b, t) = a · (a⁻¹ b)^t
///
/// That formula is not specific to the plane, and it is not specific to complex
/// numbers. It is the definition of a geodesic on any group where those three
/// operations exist, which is why Lesson 7.5's quaternion slerp is this function
/// with `quat` substituted for `complex` — and why Lesson 7.2's
/// `rotation_slerp`, written for `mat3` in a completely different notation,
/// turns out to be the same three steps.
///
/// **Constant angular speed, exactly.** The angle covered by time `t` is
/// `t · angle_between(a, b)`, linear in `t` by construction. §10.2 measures it
/// against a sweep and finds no variation beyond rounding.
///
/// **It takes the short way round.** `angle_from_complex` returns an angle in
/// `(−π, π]`, so the difference rotation is always the shorter of the two ways
/// about — 350° apart becomes −10°. That is almost always what a caller wants
/// and is occasionally exactly what it does not (a turntable meant to spin the
/// long way); Exercise 7.3.4 asks for the other one.
[[nodiscard]] inline complex complex_slerp(complex a, complex b, float t)
{
    return a * complex_pow_unit(conjugate(a) * b, t);
}

/// Normalised linear interpolation: the straight line between them, pushed back
/// onto the circle.
///
/// **Cheaper than `complex_slerp` and it lands on the same arc.** No `atan2`, no
/// `sin`, no `cos` — two lerps and a normalise. Because the straight segment
/// between two points of the circle stays inside the disc and never leaves the
/// plane they span, projecting it outward cannot wander off the arc: nlerp's
/// PATH is exactly the geodesic, and Lesson 7.3 §10.3 measures 0.00% of excess
/// turning to confirm it.
///
/// **What it gets wrong is the schedule.** It moves fastest at the midpoint and
/// slowest at the ends, and the ratio between the two is exactly `sec²(Ω/2)` for
/// an arc of `Ω` — derived in §10.3, measured against a sweep in the same place.
/// At 90° that is a factor of 2; at 179° it is over 13,000. The visible
/// consequence is a turn that lurches through its middle, which is why an
/// animation system that blends short arcs can use nlerp happily and one that
/// blends near-opposite poses cannot.
///
/// Antipodal inputs (`b == −a`) have no shortest arc between them and their
/// midpoint is the origin; `normalised` then returns the identity rather than a
/// NaN. That is a placeholder, not an answer, and a caller that can produce
/// antipodal poses should say what it wants instead.
[[nodiscard]] inline complex complex_nlerp(complex a, complex b, float t)
{
    return normalised(complex{a.re + (b.re - a.re) * t,
                              a.im + (b.im - a.im) * t});
}

} // namespace engine
