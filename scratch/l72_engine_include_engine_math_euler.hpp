// engine/include/engine/math/euler.hpp — three numbers for three degrees of freedom.
//
// A rotation has three degrees of freedom. Every tool a human uses to author one
// offers exactly three numbers, because three is what a human can hold in their
// head and type into a field. This file is the conversion between those three
// numbers and the `mat3` the renderer actually multiplies by.
//
// It is also the file that says, out loud, what those three numbers MEAN — and
// that sentence is doing more work than it looks like. "Yaw 30, pitch 40, roll
// 50" is not an orientation. It is an orientation only once you have also fixed
// which axes the three turns are about, in which order they are applied, and
// whether each later turn is about a world axis or about the axis the previous
// turns just moved. There are TWENTY-FOUR self-consistent ways to answer that
// (Lesson 7.1 §4.2), they are all in use, and the same three numbers read under
// two of them can differ by more than sixty degrees. A file that converts Euler
// angles without naming its convention has not converted anything.
//
// THIS ENGINE'S CONVENTION, fixed here and on the Conventions page (§8b):
//
//     INTRINSIC Y-X-Z, active, right-handed, radians.
//
//         rotation_from_euler({yaw, pitch, roll})
//             == rotation_y(yaw) * rotation_x(pitch) * rotation_z(roll)
//
//     Read it right to left, the way every matrix product in this engine is
//     read (Conventions §6): roll happens first, in the object's own frame,
//     then pitch, then yaw. Read it LEFT to right and it is the intrinsic
//     story: yaw about world +Y, then pitch about the +X that the yaw just
//     moved, then roll about the +Z that both just moved. Those are the same
//     matrix. §4.3 of the lesson proves it rather than asserting it.
//
// WHY Y-X-Z AND NOT ONE OF THE OTHER TWENTY-THREE. Every three-angle
// parameterisation of rotation is singular somewhere — that is a theorem, not a
// design flaw (§3.4), and no choice of order escapes it. What a choice of order
// DOES decide is *where* the singularity sits. For an X-Y-Z order it sits at
// yaw = ±90°, which a camera passes through constantly. For Y-X-Z it sits at
// pitch = ±90° — nose straight up or straight down — which is the one
// orientation every first-person camera already clamps away from for reasons
// that have nothing to do with mathematics. We are not avoiding the problem; we
// are putting it where the product already forbids the user to go.
//
// WHAT THIS FILE IS NOT. It is not the engine's storage format for rotation.
// `transform` still holds a `mat3` and will hold a quaternion from Lesson 7.4;
// Euler angles are an INTERFACE — for a human, for a file written by somebody
// else's tool, for a debug panel with three sliders in it. Storing them, and
// especially interpolating them, is what §5 of the lesson measures and argues
// against, with numbers.

#pragma once

#include <engine/math/mat3.hpp>
#include <engine/math/rotation.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace engine {

/// Three angles, in radians, under this engine's intrinsic Y-X-Z convention.
///
/// The member names are aviation's and they are worth keeping, because they say
/// which axis each angle turns about in a way that "x, y, z" cannot:
///
///   - **yaw** turns you left and right, about world **+Y** (our up, Conventions §2).
///   - **pitch** tips your nose up and down, about the **+X** that the yaw moved.
///   - **roll** banks you, about the **+Z** that the yaw and pitch both moved.
///
/// Our forward is **−Z** (Conventions §2), so positive yaw turns the nose from
/// −Z toward −X: to the left, counter-clockwise seen from above, which is what
/// the right-hand rule about +Y says it must be. That is worth checking once by
/// hand rather than trusting, and §4.1 of the lesson does exactly that.
///
/// Radians, because §8 of the Conventions page says radians internally, always.
/// Degrees belong at the two edges where a human reads or types a number, and
/// nowhere between them.
struct euler_angles
{
    float yaw   = 0.0f;   ///< about world +Y — left and right
    float pitch = 0.0f;   ///< about the yawed +X — nose up and down
    float roll  = 0.0f;   ///< about the yawed-and-pitched +Z — banking
};

// ---- Angles to a matrix -------------------------------------------------------

/// Build the rotation these three angles name, under the convention above.
///
/// **Written as the product of the three elementary rotations, on purpose**, and
/// not as the expanded nine-float closed form that §4.4 of the lesson derives.
/// The expanded form is about four times cheaper — twelve multiplies instead of
/// the fifty-four two `mat3` products cost — and that is not the deciding
/// argument, because the deciding argument is that THIS LINE IS THE CONVENTION.
/// A reader who wants to know what order this engine applies its angles in can
/// read it off the code. Nobody can read it off nine transcribed sines.
///
/// If a profile ever says this matters, the closed form is in the lesson and in
/// `scratch/verify_71.cpp`, which checks the two agree to 1e-6 across a sweep of
/// 9,261 angle triples. Replace it then, with the test already written.
[[nodiscard]] inline mat3 rotation_from_euler(euler_angles e)
{
    return rotation_y(e.yaw) * rotation_x(e.pitch) * rotation_z(e.roll);
}

// ---- A matrix back to angles ---------------------------------------------------

/// Below this, `euler_from_rotation` reports `degenerate` and stops splitting.
///
/// **A measured threshold, not a round number.** Near pitch = ±90° the yaw and
/// roll come out of `atan2` of two quantities that are both proportional to
/// cos(pitch), so a `float`'s ~1e-7 of absolute error in a matrix entry arrives
/// in the recovered angle divided by cos(pitch). Setting the floor at 1e-4 bounds
/// that amplification at about 1e-3 radians — **0.06°** — which is below what any
/// consumer of these angles can notice. Lower it and the split starts being noise
/// while the flag still says it is fine; raise it and you refuse to answer
/// questions you could have answered. §6.2 of the lesson measures both edges.
///
/// It corresponds to pitch within **0.0057°** of vertical. That number is only
/// reachable at all because `cos_pitch` below is computed by `hypot` rather than
/// by the usual `sqrt(1 - sin²)`, which in `float` hits exactly zero — and so
/// declares lock — a full 0.01° early.
inline constexpr float k_euler_lock_epsilon = 1e-4f;

/// The angles recovered from a rotation, **plus the two facts you need to judge
/// them**.
///
/// A function that returns only `angles` here would be hiding its own confidence.
/// `cos_pitch` is computed inside the extraction and thrown away by every
/// implementation of this routine you will find elsewhere; it is the entire
/// difference between "here are your three angles" and "here are your three
/// angles, and here is how much the last two of them mean". Lesson 6.18 learned
/// this the small way — a packer that computed its own occupancy and discarded
/// it, leaving no way to judge the result from outside. If a routine computes a
/// quantity a caller would need in order to judge it, return it.
struct euler_extraction
{
    /// The recovered angles. Round-tripping them through `rotation_from_euler`
    /// reproduces the input matrix to float precision **even when `degenerate`
    /// is true** — that is the point of the degenerate branch, and §6.1 measures
    /// it. What is lost at lock is not the orientation, it is the SPLIT.
    euler_angles angles{};

    /// cos(pitch), always in [0, 1].
    ///
    /// Non-negative by construction, and the reason is worth one line: pitch
    /// comes out of `std::asin`, whose range is [−π/2, +π/2], where cosine is
    /// never negative. So this is a magnitude — the conditioning of the split —
    /// and never a signed component. 1 is a clean extraction; 0 is gimbal lock.
    float cos_pitch = 1.0f;

    /// `cos_pitch < k_euler_lock_epsilon`: yaw and roll were **not separable**,
    /// so the whole of their combination was put into `yaw` and `roll` was set
    /// to zero. The matrix is still reproduced exactly; the two angles are one
    /// of infinitely many pairs that produce it.
    bool degenerate = false;
};

/// Recover this engine's three angles from a rotation matrix.
///
/// This is the inverse of `rotation_from_euler`, and it is only a true inverse on
/// the strip pitch ∈ [−90°, +90°] — `std::asin` cannot return anything else, so
/// an input built with pitch = 100° comes back as a *different* angle triple
/// naming the *same* rotation. That is not a bug and it cannot be fixed: the map
/// from angles to rotations is many-to-one, so its inverse has to choose, and
/// choosing the shortest pitch is the choice every library makes. §4.5.
///
/// **Requires a rotation.** Feed it a matrix with scale or shear in it and you
/// get angles that mean nothing, silently — `asin` of an entry greater than one
/// would be NaN, so the entry is clamped first, which turns a nonsense input into
/// a plausible-looking output. The engine has no `is_rotation` predicate to
/// assert on yet; `mat4`'s `is_rigid` (2.9) is the closest thing and works on the
/// wrong type. Named as a gap, not papered over (Exercise 4).
[[nodiscard]] inline euler_extraction euler_from_rotation(const mat3& r)
{
    euler_extraction out;

    // Written-notation element (1, 2) — row 1, column 2 — which the derivation in
    // §4.4 shows is exactly −sin(pitch). Every other entry of the composite
    // matrix mixes at least two of the three angles; this one is the single
    // entry that depends on one angle alone, which is why the extraction starts
    // here and not somewhere else.
    //
    // Clamped before `asin` because a matrix that has been through a few hundred
    // compositions is only a rotation to about 1e-6, and `asin(1.0000001)` is
    // NaN — a value that then propagates through the frame and turns into a
    // vanished object three systems away. The clamp costs one instruction and
    // removes the entire failure mode.
    const float sin_pitch = std::clamp(-r.at(1, 2), -1.0f, 1.0f);

    // COS(PITCH) FROM THE MATRIX, NOT FROM SIN(PITCH), and the difference is
    // three hundredths of a degree of working range.
    //
    // The obvious spelling is `sqrt(1 - sin_pitch * sin_pitch)`, and it is wrong
    // in `float` exactly where it matters. At pitch = 89.99° the true sine is
    // 1 - 1.5e-8; a `float` cannot hold that, so `sin_pitch * sin_pitch` rounds
    // to 1.0, the subtraction returns 0, and this routine declares gimbal lock
    // while the pose is still a hundredth of a degree away from it. Measured:
    // §6.2 of the lesson prints the naive column collapsing to zero at 89.99°
    // while the pose is fine.
    //
    // `hypot` of the same two entries `atan2` is about to take the angle of has
    // no such problem, and it is not a trick — it is the same fact read the
    // other way. Those entries are sin(yaw)·cos(pitch) and cos(yaw)·cos(pitch),
    // so their length IS |cos(pitch)|, computed without ever subtracting two
    // nearly-equal numbers. **The conditioning of the yaw extraction is the
    // length of the vector whose angle the yaw extraction takes.**
    out.cos_pitch = std::hypot(r.at(0, 2), r.at(2, 2));
    out.degenerate = out.cos_pitch < k_euler_lock_epsilon;

    // `atan2(sin, cos)` rather than `asin(sin)`, for the third time in this file
    // and the same reason: `asin` has infinite slope at ±1, so it throws away
    // precision precisely where the pitch is interesting. With an accurate
    // cosine already in hand, `atan2` costs nothing extra and is well behaved
    // across the whole range. It also returns the same branch — `atan2` of a
    // non-negative second argument lands in [−π/2, +π/2], which is exactly
    // `asin`'s range, so the choice of canonical strip below is unchanged.
    out.angles.pitch = std::atan2(sin_pitch, out.cos_pitch);

    if (!out.degenerate)
    {
        // Both of these are atan2 of a pair of entries that share a factor of
        // cos(pitch), which atan2 divides out — that shared factor is exactly
        // what vanishes at lock, and exactly why this branch cannot run there.
        //
        //     r(0,2) = sin(yaw)·cos(pitch)      r(2,2) = cos(yaw)·cos(pitch)
        //     r(1,0) = cos(pitch)·sin(roll)     r(1,1) = cos(pitch)·cos(roll)
        out.angles.yaw  = std::atan2(r.at(0, 2), r.at(2, 2));
        out.angles.roll = std::atan2(r.at(1, 0), r.at(1, 1));
    }
    else
    {
        // GIMBAL LOCK, and the honest thing to do about it.
        //
        // At pitch = ±90° the composite matrix depends on yaw and roll only
        // through their difference (pitch = +90°) or their sum (pitch = −90°) —
        // §4.6 derives both. So there is a one-parameter family of (yaw, roll)
        // pairs that all produce this exact matrix, and no amount of care picks
        // the "right" one, because there is not one.
        //
        // We put the whole combination in `yaw` and set `roll` to zero. That is
        // a CHOICE and it is recorded in `degenerate` rather than hidden: a
        // caller who is blending toward this pose needs to know that the roll it
        // gets back is not the roll it put in.
        //
        // The two entries used below are r(0,0) and r(0,1), which at lock are
        // cos(yaw ∓ roll) and sin(yaw ∓ roll) — a clean atan2 pair with no
        // vanishing factor, which is why the degenerate branch is numerically
        // the BETTER-conditioned one. The sign follows the pitch.
        out.angles.roll = 0.0f;
        out.angles.yaw = (sin_pitch > 0.0f) ? std::atan2( r.at(0, 1), r.at(0, 0))
                                            : std::atan2(-r.at(0, 1), r.at(0, 0));
    }

    return out;
}

// ---- The derivative, which is where gimbal lock stops being a metaphor ---------

/// The matrix that turns **angle rates into angular velocity**, at this pose.
///
/// Turning the three knobs at rates (dyaw, dpitch, droll) spins the body at some
/// angular velocity ω, in rad/s about a world axis (Conventions §9b). The
/// relationship is linear in the rates and depends only on where you currently
/// are, so it is a 3x3 matrix, and it is the Jacobian of the angle-to-rotation
/// map. Its columns are just **the three axes the three knobs actually turn
/// about, written in world space**:
///
///     column 0   world +Y                    — yaw's axis, never moves
///     column 1   Ry(yaw) · +X                — pitch's axis, carried by the yaw
///     column 2   Ry(yaw)·Rx(pitch) · +Z      — roll's axis, carried by both
///
/// That is the whole content of the function, and it is also the whole content of
/// gimbal lock. **det(J) = −cos(pitch)** (§4.7 derives it, and the minus sign is
/// only the odd column ordering, not a fact about rotation). At pitch = ±90° the
/// first and third columns become parallel: two knobs, one axis, and a direction
/// of angular velocity that no combination of rates can produce. The rank drops
/// from three to two, and a degree of freedom is gone — not from the body, which
/// can still be turned any way you like, but from the PARAMETERISATION.
///
/// Use it to answer "how bad is it here?" with a number instead of a shrug:
/// `std::abs(determinant(euler_rate_jacobian(e)))` is 1 when the three knobs are
/// orthogonal and 0 at lock. §6.3 sweeps it and prints the singular values too.
[[nodiscard]] inline mat3 euler_rate_jacobian(euler_angles e)
{
    const mat3 after_yaw = rotation_y(e.yaw);
    const mat3 after_pitch = after_yaw * rotation_x(e.pitch);
    return mat3{vec3{0.0f, 1.0f, 0.0f}, after_yaw.c0, after_pitch.c2};
}

// ---- Angles are circular, and the code has to know it -------------------------

/// Fold an angle into (−π, +π].
///
/// Angles are not numbers on a line, they are positions on a circle, and every
/// bug in this area comes from code that forgot. 350° and −10° are the same
/// direction; their *difference as floats* is 360. §5.1 shows the artifact.
///
/// `std::remainder` rather than `fmod` — and the difference is the whole
/// function. `fmod` truncates toward zero and so keeps the sign of its left
/// operand, landing in (−2π, 2π); `remainder` rounds to NEAREST and lands in
/// [−π, π], which is the interval we actually want. Getting this from `fmod`
/// takes a conditional fixup that people write wrong.
[[nodiscard]] inline float wrap_angle(float radians)
{
    return std::remainder(radians, 2.0f * std::numbers::pi_v<float>);
}

/// The shortest signed turn that takes `from` to `to`, in (−π, +π].
///
/// The one function that makes angle blending behave. Interpolating
/// `from + t * shortest_angle_delta(from, to)` always goes the short way round;
/// interpolating `from + t * (to - from)` goes whichever way the numbers happen
/// to be written, which for a compass heading crossing north is the wrong way by
/// 350 degrees. That this is *necessary* is a fact about the representation, not
/// about rotation, and it is the first of the three indictments in §5.
[[nodiscard]] inline float shortest_angle_delta(float from, float to)
{
    return wrap_angle(to - from);
}

// ---- Where the instrument went ------------------------------------------------
//
// `angle_between_rotations` — the angle of the shortest turn carrying one
// orientation onto another, with which every claim in Lesson 7.1 was measured —
// **used to be defined here, and Lesson 7.2 moved it to
// `math/rotation.hpp`.** The doc comment it carried said it did not belong in
// this file and named 7.2 as the lesson that should move it; 7.2 arrived with a
// second thing that needs it, which is what a shared header waits for.
//
// Nothing has to change at a call site: this header includes that one, so a
// translation unit that includes `math/euler.hpp` still sees the function. The
// note is here so that a reader following Lesson 7.1's listing does not conclude
// the function was deleted.

} // namespace engine
