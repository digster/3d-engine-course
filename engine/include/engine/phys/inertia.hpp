// engine/include/engine/phys/inertia.hpp — how a body's mass is spread out.
//
// Lesson 8.3. Lesson 8.2 gave a body a mass, which answers one question: how
// hard is it to push? This file answers the other one: how hard is it to TURN?
//
// Those are genuinely different questions, and the difference is the reason this
// file exists rather than a `float inertia` next to `float inv_mass`. Push a
// crate and only its total mass matters — where the mass sits inside it is
// irrelevant, which is the content of "the centre of mass moves as though all
// the mass were there". Try to SPIN the same crate and where the mass sits is
// the whole story: a figure skater changes nothing about their mass by pulling
// their arms in, and speeds up anyway.
//
// ---- WHY NINE NUMBERS ------------------------------------------------------
//
// It gets worse before it gets better. Resistance to turning is not one number
// per body; it is not even three. Spin a book about its three axes and you get
// three different resistances — that is three numbers — but spin it about an
// axis BETWEEN two of them and the angular momentum does not, in general, point
// along the spin axis at all. The body resists *and deflects*. A thing that
// takes a vector and hands back a vector pointing somewhere else is a matrix,
// and this one is called the INERTIA TENSOR.
//
// The derivation is four lines and Lesson 8.3 §4 walks it slowly. In outline:
// angular momentum is `L = sum of m_i * (r_i x v_i)`, a point on a rigid body
// moves at `v_i = omega x r_i`, and the vector triple product identity turns
// `r x (omega x r)` into `omega*(r.r) - r*(r.omega)`. Written with the outer
// product from Lesson 8.3's additions to mat3.hpp, that second expression IS a
// matrix acting on omega:
//
//     I = sum of m_i * (|r_i|^2 * identity - outer(r_i, r_i))          (1)
//     L = I * omega                                                    (2)
//
// Equation (2) is this file's reason to exist, and equation (1) is every
// function in it. Every closed form below — box, sphere, cylinder, capsule — is
// that sum evaluated as an integral over a shape, once, by somebody else,
// centuries ago. We check three of them against a brute-force sum over point
// masses in 8.3 §5, because a formula you cannot check is a formula you are
// trusting.
//
// ---- THE THREE THINGS THAT ARE EASY TO GET WRONG ---------------------------
//
//   1. **A tensor is about a POINT and in a BASIS.** "The inertia tensor of a
//      box" is not a thing. "The inertia tensor of a box about its centre of
//      mass, in its own body axes" is. Moving the point is `shift_inertia` (the
//      parallel-axis theorem); changing the basis is `rotate_inertia` (a
//      similarity transform). They are different operations and neither is a
//      substitute for the other.
//
//   2. **Everything here is in BODY axes**, fixed to the body and turning with
//      it — which is the only frame in which the tensor is constant. In world
//      space it changes every time the body moves, and `world_inertia` is the
//      one line that rebuilds it. 8.2 fixed the rule that a body's POSITION is
//      world space; its inertia is the one quantity in this engine that is not,
//      and the asymmetry is not sloppiness. See `world_inertia`.
//
//   3. **Inertia is not a scale.** `diagonal(vec3{2,2,2})` and
//      `scale(2,2,2)` are the same nine floats and mean unrelated things. If you
//      ever find yourself multiplying a tensor by a transform matrix, stop: the
//      only product a tensor takes part in is `R I R^T` and `I * omega`.

#pragma once

#include <engine/math/mat3.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>

#include <span>

namespace engine::phys
{

// ---------------------------------------------------------------------------
// The atom: one point mass
// ---------------------------------------------------------------------------

/// A lump of mass at an offset from the point the tensor is taken about.
struct point_mass
{
    vec3 offset{};       ///< metres, from the reference point
    float mass = 0.0f;   ///< kilograms
};

/// Equation (1) for a single point: `m * (|r|^2 * identity - outer(r, r))`.
///
/// This is the whole subject in one line, and it is worth reading as geometry
/// rather than as arithmetic. `|r|^2 * identity` says "resist every axis by
/// `m r^2`"; `- outer(r, r)` then takes all of that back along `r` itself. What
/// is left resists rotation about any axis perpendicular to `r` by `m r^2` and
/// rotation about `r` not at all — which is exactly right, because a point mass
/// spun about an axis running through it does not move.
///
/// Two consequences worth having before you read anything below:
///   * The result is **symmetric**, because `identity` and `outer(r, r)` both
///     are. Sums of symmetric matrices are symmetric, so every tensor in this
///     file is, and `asymmetry()` on any of them is a bug detector rather than a
///     tolerance.
///   * It is **quadratic in `r`**, so doubling every distance quadruples the
///     tensor. That is the `r^2` in every textbook formula, and it is why a hoop
///     is so much harder to spin than a disc of the same mass.
[[nodiscard]] mat3 inertia_of_point(float mass, vec3 offset);

/// Equation (1) evaluated literally, over a cloud of point masses.
///
/// The slow, obviously-correct version. Nothing in the engine's hot path calls
/// it; 8.3 §5 uses it to check the closed forms below by filling a shape with a
/// few hundred thousand sample points and summing. A formula and a brute-force
/// sum agreeing to five digits is evidence; either one on its own is a hope.
[[nodiscard]] mat3 inertia_of_points(std::span<const point_mass> points);

// ---------------------------------------------------------------------------
// Closed forms, about the centre of mass, in body axes
// ---------------------------------------------------------------------------
//
// Every function below is equation (1) integrated over a uniform-density shape.
// All of them are DIAGONAL, which is not a coincidence and not an approximation:
// each of these shapes is symmetric about all three coordinate planes, so for
// every point mass at `r` there is a mirror one that cancels the off-diagonal
// terms exactly. Body axes chosen along the symmetries of a shape are its
// PRINCIPAL AXES, and a tensor in its principal axes is diagonal. This is why
// engines let you author a box and not a nine-float matrix.

/// A uniform solid box, `half_extents` from centre to face along each axis.
///
/// `Ixx = m*(b^2 + c^2)/3` for half-extents `(a, b, c)` — note that the `x`
/// entry does not contain `a`. A body's resistance to spinning about an axis
/// depends on how far its mass is from that AXIS, not from the origin, and the
/// `x` extent is measured along the `x` axis, where it contributes nothing.
/// Getting this backwards is the single most common inertia bug there is, and it
/// looks almost right, which is worse.
[[nodiscard]] mat3 inertia_solid_box(float mass, vec3 half_extents);

/// A uniform solid sphere: `(2/5) m R^2` about every axis.
///
/// The one shape whose tensor is a multiple of the identity, which makes it the
/// only shape that spins the same way about every axis. That is why a thrown
/// ball does not tumble and a thrown book does — 8.3 §10.
[[nodiscard]] mat3 inertia_solid_sphere(float mass, float radius);

/// A thin spherical shell: `(2/3) m R^2`.
///
/// Two thirds against the solid sphere's two fifths, for the same mass and
/// radius — a 67% difference from nothing but moving the mass outward. Worth
/// knowing when you are tempted to approximate a hollow object with a solid one.
[[nodiscard]] mat3 inertia_hollow_sphere(float mass, float radius);

/// A uniform solid cylinder whose axis is **+y**, of the given radius and total
/// height.
///
/// `Iyy = m R^2 / 2` about the axis; `Ixx = Izz = m*(3R^2 + H^2)/12` across it.
///
/// The `+y` convention is this engine's, not physics': conventions.html §10 has
/// had `+y` up since Lesson 2.6, so a capsule standing on a floor stands along
/// `y`. Every textbook you will read uses `z`. Transcribe carefully.
[[nodiscard]] mat3 inertia_solid_cylinder(float mass, float radius, float height);

/// A thin rod of the given length along **+y**: `Ixx = Izz = m L^2 / 12`, and
/// exactly zero about the rod itself.
///
/// The zero is real and it is a trap: a body with a zero principal moment has a
/// SINGULAR inertia tensor, so `inverse_inertia` on it produces zeros and the
/// body will not spin about its own length no matter what torque you apply. That
/// is correct physics for an infinitely thin rod and wrong for anything you can
/// hold, which is why `set_inertia` refuses it — see rigid_body.hpp.
[[nodiscard]] mat3 inertia_thin_rod(float mass, float length);

/// A uniform capsule along **+y**: a cylinder of `cylinder_height` capped with
/// two hemispheres of `radius`. Total height is `cylinder_height + 2*radius`.
///
/// The workhorse shape for characters, limbs and most props, and the one whose
/// formula is genuinely fiddly — the hemisphere contributions need the
/// parallel-axis theorem applied about each cap's own centre of mass, which sits
/// `3R/8` from its flat face and not at the sphere centre. 8.3 §5 checks it in
/// both directions: `cylinder_height = 0` must reproduce `inertia_solid_sphere`
/// to the bit, and `radius -> 0` must approach `inertia_thin_rod`.
[[nodiscard]] mat3 inertia_capsule(float mass, float radius, float cylinder_height);

// ---------------------------------------------------------------------------
// Moving a tensor: the parallel-axis theorem
// ---------------------------------------------------------------------------

/// Carry a tensor from a body's centre of mass **out** to a point offset by
/// `offset` from it.
///
///     I_point = I_centre + m * (|d|^2 * identity - outer(d, d))
///
/// The correction term is `inertia_of_point(m, d)` — the tensor of a single
/// point mass `m` sitting at the offset — which is the theorem's real content:
/// *moving the reference point costs exactly as much as if the entire body were
/// concentrated at its centre of mass*. 8.3 §5 derives it in three lines from
/// equation (1) and the definition of the centre of mass, which is where the
/// cross term vanishes.
///
/// The offset is squared, so its sign does not matter.
///
/// **It only works from the centre of mass.** Shifting from an arbitrary point
/// to another arbitrary point by calling this twice is wrong unless one of them
/// is the centre of mass; `unshift_inertia` is the way back.
[[nodiscard]] mat3 shift_inertia(const mat3& about_centre, float mass, vec3 offset);

/// The inverse move: carry a tensor taken about some point **back** to the
/// centre of mass, which sits at `-offset` from that point.
///
/// Every compound body needs this. You measure or look up each part about its
/// own centre, shift them all to a common origin to add them up, and then shift
/// the total back to the assembly's centre of mass — which you only knew after
/// adding the parts. `inertia_assembly` does all three steps for you.
[[nodiscard]] mat3 unshift_inertia(const mat3& about_point, float mass, vec3 offset);

// ---------------------------------------------------------------------------
// Changing the basis: the similarity transform
// ---------------------------------------------------------------------------

/// Re-express a tensor in axes rotated by `rotation`: `R * I * transpose(R)`.
///
/// **This is a basis change, not a transformation of the body**, and the
/// sandwich is where that shows. Read it right to left on an angular velocity:
/// `transpose(R)` carries `omega` from the new axes into the ones `I` is written
/// in, `I` turns it into an angular momentum there, and `R` carries that
/// momentum back out. A quantity that eats a vector and produces a vector has to
/// be converted on both sides, which is why a tensor transforms with `R ... R^T`
/// where a plain vector transforms with `R`.
///
/// The consequences are the cheapest tests in this file, and 8.3 §6 runs all
/// three: `trace`, `determinant` and the set of principal moments are all
/// unchanged by any rotation, because none of them depends on which axes you
/// wrote the tensor in.
///
/// `rotation` must be a rotation. Hand it a matrix with a scale in it and the
/// result is not an inertia tensor of anything — which is the deeper reason 8.2
/// insisted that bodies live in world space, restated in 8.3 §11: a
/// non-uniformly scaled parent does not tilt a body's inertia, it turns the body
/// into a different body.
[[nodiscard]] mat3 rotate_inertia(const mat3& rotation, const mat3& inertia);

/// The world-space tensor of a body whose body-axis tensor is `body_inertia` and
/// whose orientation is `orientation`.
///
/// Convenience for `rotate_inertia(mat3_from_quat(q), I)`, and the function a
/// solver actually calls — once per body per step, because `q` changed.
[[nodiscard]] mat3 world_inertia(quat orientation, const mat3& body_inertia);

/// The world-space **inverse** tensor: `R * inv_body_inertia * transpose(R)`.
///
/// Note what this is NOT: it is not the inverse of `world_inertia`, computed
/// fresh each step. It is the same sandwich applied to a tensor that was
/// inverted once, when the body was created — which is legitimate because
/// `(R I R^T)^-1 = R I^-1 R^T` exactly (the transpose of a rotation is its
/// inverse, so the sandwich's inverse is the sandwich of the inverse). A 3x3
/// inverse per body per step becomes two matrix products, and 8.3 §12 measures
/// what that is worth.
///
/// This is also where 8.2's inverse-mass argument pays off a second time: a body
/// that must not rotate stores a **zero tensor**, and zero survives the sandwich
/// unchanged, so every torque applied to it produces exactly no angular
/// acceleration with no branch and no special case.
[[nodiscard]] mat3 world_inverse_inertia(quat orientation, const mat3& inv_body_inertia);

// ---------------------------------------------------------------------------
// Inverting, and refusing to
// ---------------------------------------------------------------------------

/// Invert an inertia tensor, returning all zeros when it cannot be inverted.
///
/// Zeros rather than NaN, for the third time in this engine (`normalised`,
/// `mat3::inverse`, and now here): a zero inverse tensor is the exact,
/// representable, physically meaningful statement "no torque can spin this",
/// which is what an immovable body wants. A NaN is that statement smeared over
/// every subsequent frame.
[[nodiscard]] mat3 inverse_inertia(const mat3& inertia);

// ---------------------------------------------------------------------------
// Compound bodies
// ---------------------------------------------------------------------------

/// One part of an assembly, as its own mass, its own tensor, and where it sits.
struct inertia_part
{
    float mass = 0.0f;              ///< kilograms
    vec3 centre{};                  ///< the part's centre of mass, in assembly coordinates
    mat3 inertia{mat3::identity()}; ///< about the PART's own centre, in assembly AXES
};

/// The result of assembling parts: a total mass, where the whole thing balances,
/// and the tensor about that point.
struct inertia_assembly_result
{
    float mass = 0.0f;
    vec3 centre{};
    mat3 inertia{};
};

/// Combine parts into one rigid body.
///
/// Three steps, and the middle one is the theorem: total the masses and take the
/// mass-weighted mean of the centres; shift each part's tensor from its own
/// centre out to the assembly's centre; add. Tensors add because they are
/// *quantities* — see the note at the head of mat3.hpp's arithmetic block.
///
/// **Each part's tensor must already be expressed in the assembly's axes.** A
/// part that is rotated relative to the assembly needs `rotate_inertia` first;
/// this function has no way to know and cannot check, which is exactly the kind
/// of silent-wrong-answer 8.3 §5 demonstrates rather than describes.
[[nodiscard]] inertia_assembly_result inertia_assembly(std::span<const inertia_part> parts);

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

/// What can be said about a tensor without knowing what shape it came from.
struct inertia_report
{
    /// Largest `|I_ij - I_ji|`. Must be zero for anything built by this file.
    float asymmetry = 0.0f;

    /// `trace(I) = 2 * sum(m |r|^2)`, invariant under any rotation of the axes.
    float trace = 0.0f;

    /// The diagonal entries. These are the principal moments **only if the
    /// tensor is diagonal** — which `off_diagonal` tells you.
    vec3 diagonal{};

    /// Largest off-diagonal magnitude, relative to the largest diagonal entry.
    /// Zero means the current axes are the principal axes.
    float off_diagonal = 0.0f;

    /// Every diagonal entry strictly positive. A zero is a degenerate shape (a
    /// thin rod along an axis, a point mass); a negative is a bug.
    bool positive = false;

    /// The **triangle inequality**: for principal moments sorted `I1 <= I2 <=
    /// I3`, no real mass distribution can have `I1 + I2 < I3`. It is the tensor
    /// equivalent of "a mass cannot be negative", and it catches hand-authored
    /// tensors that could not exist. Only meaningful when `off_diagonal` is
    /// zero — this field reports the test on the diagonal as it stands.
    bool triangle = false;

    /// True when the tensor is one this engine will accept: symmetric, positive,
    /// and satisfying the triangle inequality.
    bool usable = false;
};

/// Measure a tensor against the four things every real one satisfies.
///
/// Called by `set_inertia` (rigid_body.hpp) before a tensor is allowed into a
/// body, and by 8.3's harness after every construction in this file. The check
/// is cheap and the failure it prevents is not: a tensor that violates the
/// triangle inequality integrates into a body that gains energy from nothing.
[[nodiscard]] inertia_report inspect_inertia(const mat3& inertia);

} // namespace engine::phys
