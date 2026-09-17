// engine/include/engine/phys/rigid_body.hpp — a thing with a mass, and the
// table of them.
//
// Lesson 8.2. Lesson 8.1 gave this engine a `motion` — a position and a velocity
// — and a set of rules for advancing one through time. What it did not give it is
// any way to say WHY a body accelerates. You hand `integrate` an acceleration and
// it believes you. That is enough to drop a cube, and it is enough for nothing
// else, for two reasons that look small and are not.
//
//   1. AN ACCELERATION CANNOT BE ADDED UP. Gravity wants to accelerate a body
//      downward, a thruster wants to push it forward, drag wants to slow it, and
//      the wind wants to shove it sideways. Each of those is a separate system
//      that knows nothing about the others, and every one of them wants to write
//      the same field. The last writer wins and the rest of the physics silently
//      does not happen. Forces do not have this problem — they ADD, by Newton's
//      second law being linear in F — which is why every physics engine ever
//      written has an accumulator and not a setter.
//
//   2. AN ACCELERATION DOES NOT KNOW ABOUT MASS, so nothing in 8.1's world can
//      tell a crate from a pebble. Push both with the same thruster and both
//      accelerate identically, which is wrong in a way a player notices
//      immediately. And you cannot fix it by passing `F/m` at the call site,
//      because the whole point of the accumulator above is that the division
//      happens ONCE, after everything has been added, at the moment the step
//      runs.
//
// So: `F = m*a`, an accumulator, and a table to keep the bodies in.
//
// ---- THE TWO THINGS IN THIS FILE THAT ARE NOT OBVIOUS ----------------------
//
// INVERSE MASS, NOT MASS, and the reason is not the divide. See `inv_mass`.
//
// AND A BODY'S POSITION IS IN **WORLD SPACE**, unconditionally, which is a
// restriction rather than a feature and is the second half of this lesson. A
// rigid body integrated in a parent's local space is integrated in a frame that
// may scale it, shear it, and — if the parent rotates — accelerate it. Under a
// parent scaled by 2 the body falls at 2 g. Under a NON-UNIFORMLY scaled parent
// gravity stops pointing down at all: §8 measures a body that falls at 63.4
// degrees off vertical and 1.58 g, with nothing in the scene doing anything
// unusual. `body_world` therefore has no concept of a parent, a hierarchy or a
// transform, and `inspect_frame` below exists so that you can MEASURE what one
// would have done to you rather than take that on trust.
//
// ---- WHAT IS STILL NOT HERE -------------------------------------------------
//
// ---- WHAT LESSON 8.3 ADDED --------------------------------------------------
//
// Rotation. 8.2 left this file with a body that was a point with a mass — it
// translated and it did not spin, `add_force` had no application point, and
// there was no torque and no inertia tensor. All of that is here now:
// `orientation`, `angular_velocity`, a `torque` accumulator, `inv_inertia_local`
// and `add_force_at`, which is the first function in this engine that can make
// something tumble.
//
// **The linear half above is untouched by it**, and that is a fact about physics
// rather than about how carefully the edit was made. Linear momentum and angular
// momentum do not mix: the centre of mass of a body moves exactly as `F = ma`
// says however wildly the body is spinning, which is why a thrown hammer's
// centre traces a clean parabola while the hammer tumbles around it. Every
// number 8.2 measured is still the number it measured.
//
// THREE THINGS IN THE ANGULAR HALF ARE NOT MIRRORS OF THE LINEAR ONE, and they
// are where 8.3's difficulty lives:
//
//   * **The tensor is in BODY axes and everything else is in world axes.** See
//     `inv_inertia_local`. It is forced, not chosen.
//   * **The orientation update is not an addition.** See `advance_orientation`
//     in integrate.hpp. `q += omega*h` is not merely inaccurate, it is not a
//     rotation.
//   * **`omega` is not conserved when `tau` is zero — `L` is.** A torque-free
//     body's angular velocity wanders continuously, changes magnitude, and for
//     an asymmetric body can flip end over end without anything acting on it.
//     `angular_momentum` is the quantity to watch in a debugger; `gyroscopic`
//     is the flag that decides whether any of it happens.
//
// No collision (8.4 onward), no constraints (8.10, 8.11), no sleeping (8.10),
// and no continuous collision detection: `step_report::max_travel` measures how
// far the fastest body moves in one step, which is the number that decides
// whether a body will tunnel through a wall, but nothing here does anything
// about it. Lesson 1.8's swept test is the shape of the answer and 8.13 is where
// it comes due.
#pragma once

#include <engine/core/handle.hpp>
#include <engine/core/pool.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/inertia.hpp>
#include <engine/phys/integrate.hpp>

#include <cstdint>
#include <span>

namespace engine::phys
{

/// Standard gravity at the Earth's surface, in metres per second squared.
///
/// **9.80665 is a defined constant, not a measurement** — it is fixed by the
/// General Conference on Weights and Measures, and the actual local value varies
/// from about 9.764 at the top of a mountain near the equator to 9.834 at the
/// poles. Games use 9.81, and this engine rounds to it for the same reason
/// everyone does: the third decimal place of gravity is four orders below the
/// smallest thing a player can perceive, and a round number is easier to
/// recognise in a debugger.
///
/// THE NUMBER IS IN METRES AND SECONDS, AND THAT IS WHY conventions.html §3
/// MATTERS. It has said "one unit is one metre" since Module 2, when nothing in
/// the engine could tell the difference. This constant is the first line of code
/// in the course for which a unit is not a matter of taste: put it in a world
/// where a unit is half a metre and everything in it falls at the wrong rate,
/// with a symptom — "floaty" or "toy-like" — that names the bug if you know how
/// to read it. §7 derives the relationship (every duration scales as the square
/// root of the length scale) and measures it.
inline constexpr float k_gravity = 9.81f;

/// The engine's default gravity vector: down is −y, by conventions.html §1.
inline constexpr vec3 k_gravity_down{0.0f, -k_gravity, 0.0f};

// ---------------------------------------------------------------------------
// What kind of body this is
// ---------------------------------------------------------------------------

/// Three answers to two different questions, which is why this is not a bool.
///
/// Engines universally offer these three and the difference between the last two
/// is the one people get wrong, so it is worth stating as two questions rather
/// than three names:
///
///   **"Can a force move it?"** — answered by `inv_mass`, and the answer is no
///   whenever `inv_mass` is 0. A crate landing on a lift must not push the lift
///   down, however heavy the crate is.
///
///   **"Does it move at all?"** — answered by this enum. A lift moves; a floor
///   does not. Both are immovable BY FORCES, and only one of them has a velocity
///   that the crate standing on it will need to be carried by (8.9).
///
/// Conflating the two gives you the two classic bugs. Make the lift dynamic and
/// heavy, and it sags under load and drifts under gravity. Make it static and
/// teleport it each frame, and anything standing on it is left behind, because a
/// body with no velocity imparts none.
///
/// **`fixed` IS WHAT EVERY OTHER ENGINE CALLS `static`.** `static` is a C++
/// keyword and cannot be an enumerator, even inside an `enum class`, so the name
/// has to change; `fixed` is the word this codebase uses and the doc comment
/// says the other one so that a search for it lands here.
enum class body_kind : std::uint8_t
{
    /// Forces move it and gravity pulls on it. The default, and the only kind
    /// that `step` integrates a velocity for.
    dynamic,

    /// Moved by gameplay, never by physics. Its velocity is whatever you set,
    /// it is integrated (so it travels), gravity does not touch it, and no force
    /// or impulse can change it. A lift, a moving platform, a swinging door.
    kinematic,

    /// Does not move, ever. Not integrated at all — `step` skips it entirely,
    /// which is what lets 8.6's broadphase keep every one of these in a
    /// structure it never has to rebuild. The ground, a wall, a rock.
    fixed,
};

/// Human-readable name, for logs and debug UI. Never null.
[[nodiscard]] const char* name_of(body_kind kind);

// ---------------------------------------------------------------------------
// The body
// ---------------------------------------------------------------------------

/// What to do about the term in Euler's equations that has no linear
/// counterpart. Lesson 8.3 §9.
///
/// `alpha = I^-1 * tau` is only half of the rotational equation of motion. The
/// other half is `-I^-1 * (omega x (I omega))`, and it is the term that makes an
/// asymmetric body **tumble** rather than merely spin: a thrown book wobbles,
/// a rugby ball precesses, and a box spun about its middle axis flips end over
/// end. Without it, none of that happens — a box spun about any axis spins
/// about that axis forever, which looks perfectly stable and is wrong.
///
/// It is also the one term in this engine that an explicit rule cannot be
/// trusted with, and 8.1's determinant argument says exactly why: it feeds the
/// angular velocity back into itself through a cross product, which is a
/// rotation of `omega` — and an explicitly integrated rotation grows by
/// `sqrt(1 + (sigma*h)^2)` per step. §9 measures a 1 kg box gaining **164% of
/// its angular momentum in sixty seconds** at 60 Hz, and diverging outright at
/// 30 Hz.
enum class gyroscopic_mode : std::uint8_t
{
    /// Drop the term. Fast, unconditionally stable, and visibly wrong for
    /// anything the player watches tumble. **The default**, and the default
    /// every engine ships — it is what Bullet and PhysX both do (⚠ VERIFY the
    /// exact spellings against the headers: Bullet's
    /// `BT_ENABLE_GYROSCOPIC_FORCE_IMPLICIT_BODY` flag on `btRigidBody`, and
    /// PhysX's `PxRigidBodyFlag::eENABLE_GYROSCOPIC_FORCES`, both opt-in).
    off,

    /// The term, evaluated at the start of the step and added explicitly.
    ///
    /// **This is the one you would write first and it is the one that
    /// diverges.** It is here so that §9's measurement can be reproduced and so
    /// that the demo can show it — not because it is ever the right choice.
    explicit_term,

    /// The term, solved implicitly in the body frame by one Newton step.
    ///
    /// Write the step as a root-finding problem in the unknown end-of-step
    /// angular velocity, differentiate, invert a 3x3 and take one iteration.
    /// One Newton step is what Bullet does, and the reason one is enough is that
    /// the residual is already `O(h)`.
    ///
    /// **It stops the divergence and does not conserve anything.** §9 measures
    /// the same box: explicit gains 162% of its angular momentum in a minute at
    /// 60 Hz and diverges by 3.3e+11 at 30, while implicit *loses* 35%. That is
    /// 8.1's backward-Euler determinant — `1/(1 + h^2 w^2)`, the exact
    /// reciprocal of the explicit one — arriving in the one place this engine
    /// still integrates something explicitly. It damps rather than explodes,
    /// which is why it is shippable and why it is not the last word.
    implicit_term,

    /// **Make the term unnecessary**, by integrating the angular MOMENTUM and
    /// deriving the angular velocity from it.
    ///
    /// The gyroscopic term is not a force. It is the bookkeeping that appears
    /// when you differentiate `L = I(t) * omega` and insist on treating `omega`
    /// as the state: `I` is turning with the body, so a constant `L` *requires*
    /// a changing `omega`, and the term is the size of that requirement. Store
    /// `L` instead and it is not a term at all —
    ///
    ///     L += tau * h                 exactly, because dL/dt = tau
    ///     omega = I_world(q)^-1 * L    derived, every step, from the new q
    ///
    /// — and the only thing left that can move `L` is arithmetic. §9 measures
    /// **8.0e-04 against implicit's 3.5e-01** at 60 Hz, three orders of
    /// magnitude, and that residue is not truncation: the engine stores `omega`
    /// and rebuilds `L` from it every step, so the error grows with the NUMBER
    /// of steps and is the one column in §9's table that gets WORSE as the step
    /// shrinks. Storing `L` would remove it, at the cost of converting at every
    /// solver iteration.
    ///
    /// **So why is this not the default, and why does every shipping engine
    /// store `omega`?** Because a contact solver speaks velocities. Lessons
    /// 8.9–8.11 iterate impulses against relative velocities at contact points,
    /// dozens of times per body per frame, and each iteration would have to
    /// convert. The choice here is between a cheap exact tumble and a cheap
    /// solver, and this module needs the solver — so the engine keeps `omega`
    /// and offers this for the bodies whose tumbling is the point.
    ///
    /// The orientation still integrates approximately, so the body's ATTITUDE
    /// drifts exactly as §7 says — and §9 shows that conserving `L` is not on
    /// its own enough to make the tumble right, which is why `step` pairs this
    /// mode with a half-step estimate of the angular velocity.
    momentum,
};

[[nodiscard]] const char* name_of(gyroscopic_mode mode);

/// A point with a mass: where it is, how fast it is going, and what is pushing
/// on it.
///
/// **IT HAS A `motion` RATHER THAN BEING ONE**, and 8.1's header said it would.
/// That is composition doing a real job, not an aesthetic preference: every
/// function in `integrate.hpp` — all three rules, both drag helpers, all four
/// diagnostics — operates on a `motion&` and none of them has to learn what a
/// body is. The harness in §3 steps a bare `motion` beside a `rigid_body` and
/// compares them bit for bit, which would be impossible if mass had been added
/// by widening the struct.
///
/// Sixty bytes, which is under a cache line, and the fields are ordered so the
/// two that the step touches every time are first. §10 measures the walk.
struct rigid_body
{
    /// Position and velocity, **in world space**. Metres and metres per second.
    ///
    /// See the file header: this is not negotiable and `body_world` has no way
    /// to express anything else. `place_in_parent` is the bridge to a scene
    /// hierarchy and it converts rather than integrating.
    motion state{};

    /// Which way the body is facing, as a **unit** quaternion (Lesson 7.4).
    ///
    /// Lesson 8.3. `state` says where the body is; this says how it is turned,
    /// and the two are independent in exactly the way 8.1's `motion` was not
    /// ready for — which is why orientation could not simply be a fourth field
    /// in `motion`. A position is a point in a vector space and moves by
    /// addition. An orientation is not, and does not. See `advance_orientation`
    /// in integrate.hpp, and 8.3 §7.
    ///
    /// **`step` renormalises it every step**, and that is not defensive
    /// programming. The linearised update inflates a unit quaternion by
    /// `(|omega|*h)^2/8` per step — 0.35% at 10 rad/s and 60 Hz, compounding to
    /// 23% in one second — so without the renormalisation the body visibly
    /// shears and then collapses. 8.3 §7 measures it.
    quat orientation{};

    /// Angular velocity **in world space**, in radians per second.
    ///
    /// Direction is the axis (right-hand rule, conventions.html §10), magnitude
    /// is the rate. World space rather than body space for the same reason
    /// `state.position` is: a solver in 8.9 has two bodies in front of it and
    /// needs their velocities in one common frame, and converting at every
    /// contact is both slower and a place to get the direction backwards.
    ///
    /// **It is the tensor that lives in body space**, not this. See
    /// `inv_inertia_local` and `world_inv_inertia`.
    vec3 angular_velocity{};

    /// Newtons, accumulated since the last step, and **cleared by `step`**.
    ///
    /// A force is an INPUT to a step, not a property of a body — it is the sum
    /// of what every system had to say this frame, and next frame they will all
    /// say it again. Leaving it set would mean a thruster fired once keeps
    /// firing forever, which is the single most common bug in a first physics
    /// engine and presents as objects that slowly accelerate away.
    ///
    /// **THE CLEARING IS AT THE END OF THE STEP AND NOT THE START**, which
    /// matters the moment anything wants to look at it: a debug overlay drawing
    /// force arrows, an assertion checking that nothing exceeded a sane
    /// magnitude, the solver in 8.10 reading what the contact generator asked
    /// for. Clear it first and every one of those reads zero.
    vec3 force{};

    /// Newton-metres about the body's **centre of mass**, accumulated since the
    /// last step and cleared alongside `force`.
    ///
    /// Lesson 8.3, and everything said above about `force` applies here word for
    /// word: torques add, for the same reason forces do, and a torque left set
    /// is a body that spins up forever.
    ///
    /// **About the centre of mass, world axes.** A torque is only a number once
    /// you say what it is about, and choosing the centre of mass is what
    /// decouples the two halves of `step`: about any other point, a force
    /// through the centre of mass would produce a torque, and pushing a crate
    /// squarely in the middle would set it spinning. 8.3 §3.
    ///
    /// Most callers never touch this directly — `add_force_at` computes
    /// `r x F` and feeds both accumulators at once, which is what a thruster, an
    /// explosion or a contact impulse actually wants.
    vec3 torque{};

    /// **One over the mass, in 1/kg. Zero means immovable.**
    ///
    /// Storing the reciprocal is a decision every physics engine makes and the
    /// usual justification — "it turns a divide into a multiply" — is the least
    /// of it. A divide is about four times a multiply on modern hardware and
    /// this happens once per body per step; you would never restructure a public
    /// API for that. Three real reasons, in increasing order of weight:
    ///
    ///   1. **IMMOVABLE IS REPRESENTABLE, AND IT IS THE COMMON CASE.** The floor
    ///      is immovable, and so is every wall, every rock and every piece of
    ///      level geometry — which is most of the bodies in a real scene. Its
    ///      mass is not large, it is *infinite*, and `inv_mass = 0` says so
    ///      exactly, in a float, with no sentinel and no branch. Storing mass
    ///      instead forces either a magic value (and `HUGE_VALF` propagates into
    ///      NaN the moment two of them are subtracted, which §9.1 shows) or a
    ///      separate bool that every piece of arithmetic has to remember to
    ///      consult.
    ///
    ///   2. **THE SOLVER WANTS EXACTLY THIS QUANTITY AND NOT MASS.** Every
    ///      contact and every joint in 8.9–8.11 computes an effective mass that
    ///      is built from `inv_mass_a + inv_mass_b`, and that sum has a clean
    ///      meaning when one of the two is zero. In terms of masses it is
    ///      `(m_a*m_b)/(m_a+m_b)` — the reduced mass — which is what you get
    ///      when you take the reciprocal of a sum of reciprocals, and which
    ///      needs a special case at every infinity. The storage is chosen to
    ///      match the arithmetic that the hot loop actually performs.
    ///
    ///   3. **THE DEGENERATE CASE MOVES TO WHERE IT BELONGS.** With mass
    ///      stored, `mass = 0` is a divide by zero and a massless body is a
    ///      crash. With the reciprocal stored, a massless body is
    ///      `inv_mass = infinity`, which is a body that any force accelerates
    ///      infinitely — still nonsense, but nonsense that does not divide, and
    ///      which `set_mass` refuses at the one place it can be created.
    ///
    /// The default is 1, i.e. one kilogram, because a default of 0 would make
    /// every default-constructed body immovable and that is the kind of default
    /// that costs an afternoon.
    float inv_mass = 1.0f;

    /// The **inverse inertia tensor about the centre of mass, in BODY axes**.
    /// All zeros means "cannot rotate".
    ///
    /// Lesson 8.3, and it is `inv_mass`'s argument repeated with nine floats
    /// instead of one — all three reasons survive intact, and the first one
    /// survives best. A zero tensor is exactly what an immovable body wants: it
    /// is representable, it is exact, it needs no sentinel, and it passes
    /// through the basis change `R * I^-1 * R^T` unchanged, so a wall absorbs
    /// every torque in the scene with no branch anywhere in the step.
    ///
    /// **BODY axes, unlike every other field in this struct**, and the asymmetry
    /// is forced rather than chosen. In world space a tensor changes the instant
    /// the body turns, so storing it there would mean rebuilding it from the
    /// shape every step; in body axes it is constant for the life of the body.
    /// `world_inv_inertia` is the one line that carries it out, and 8.3 §6 is
    /// about why that line is a sandwich and not a product.
    ///
    /// The default is the identity, i.e. a body that resists being spun about
    /// any axis by 1 kg·m². It is a *shape-free* default and no real shape has
    /// it; `set_inertia` is how a body gets a real one, and the demo and harness
    /// both go through `inertia_solid_box` and friends.
    mat3 inv_inertia_local{};

    /// The **forward** tensor, same point and same axes. All zeros means
    /// "cannot rotate", exactly as above.
    ///
    /// Thirty-six bytes of redundancy, and §12 is the reason it is here. The
    /// first version of this struct stored only the inverse — which is the
    /// argument `inv_mass` makes, carried over honestly — and every consumer of
    /// the forward tensor paid a **3x3 inverse per body per step**: the
    /// gyroscopic term, and `step_report::angular_momentum`, which is computed
    /// whether or not anybody reads it. Measured at 3.591 ns of a 33.793 ns
    /// update, on top of the second basis change each of them also needed.
    ///
    /// **THE INVARIANT IS THAT THESE TWO AGREE**, and it is maintained by
    /// `set_inertia` being the only supported way to write either. Assigning
    /// `inv_inertia_local` directly leaves a body whose angular momentum and
    /// angular acceleration disagree about what it is made of — which no
    /// assertion here can catch, and which is exactly the cost of caching
    /// anything. It is paid because the measurement said so.
    mat3 inertia_local{};

    /// Velocity-space damping, in 1/s. **This is not air resistance.**
    ///
    /// Applied by `step` as `apply_drag` — the exact `v *= exp(-k*h)` from 8.1,
    /// so it is frame-rate independent and cannot reverse the velocity the way
    /// `v *= (1 - k*h)` does. `k = 0` is no damping; `k = 1` costs a body 63% of
    /// its speed per second.
    ///
    /// **AND IT IS MASS-INDEPENDENT, WHICH IS EXACTLY WRONG AS PHYSICS AND
    /// EXACTLY RIGHT AS A KNOB.** Real linear drag is a FORCE, `F = -b*v`, so
    /// the acceleration it produces is `-(b/m)*v` and a heavy body is harder for
    /// the air to slow: a cannonball and a feather of the same shape have wildly
    /// different terminal speeds, which is the entire reason Galileo's
    /// experiment needed a tower rather than a bench. This field skips the mass,
    /// so every body damped at the same `k` reaches the SAME terminal speed
    /// regardless of what it weighs, and §5 measures both and puts the numbers
    /// side by side.
    ///
    /// It is here anyway, and named `damping` rather than `drag`, because what
    /// gameplay wants nine times in ten is "bleed off speed so this feels
    /// controllable", which is a stability and feel control with no physical
    /// content at all. When you want the physics, apply `-b*v` through
    /// `add_force` and let the mass do its job; `terminal_speed_damped` and
    /// `terminal_speed_dragged` sit next to each other below so the difference
    /// is one line of reading.
    float damping = 0.0f;

    /// The same knob for spin, in 1/s: `omega *= exp(-k*h)` every step.
    ///
    /// Lesson 8.3, and it is the same shape of lie as `damping` and for the same
    /// reason — real rotational drag depends on the body's shape, its speed and
    /// which way round it is, and none of that is what a designer means by
    /// "stop it spinning forever". Separate from `damping` because the two are
    /// tuned independently in practice: a thrown axe should keep tumbling long
    /// after air resistance has taken the sting out of its flight.
    float angular_damping = 0.0f;

    /// How this body handles the **gyroscopic term** `omega x (I*omega)`.
    /// Off by default.
    ///
    /// Lesson 8.3 §9. See `gyroscopic_mode` — this is the one field in the
    /// struct whose default was chosen by a measurement rather than by an
    /// argument, and the measurement is not the one you would expect.
    gyroscopic_mode gyroscopic = gyroscopic_mode::off;

    /// Multiplier on the world's gravity for this body alone.
    ///
    /// The one knob every engine ships, and it is content rather than physics: a
    /// balloon rises, a boss's hammer falls harder than it should because that
    /// is what makes it feel heavy, and a projectile arcs on a curve an artist
    /// chose. 1 is normal, 0 is weightless, negative floats.
    ///
    /// It multiplies the world gravity and nothing else, so it does not change
    /// how a thruster or a contact behaves — which is what distinguishes it from
    /// the mistake of editing the body's mass to get the same visual result.
    float gravity_scale = 1.0f;

    /// Dynamic, kinematic or fixed. See `body_kind` — it answers a different
    /// question from `inv_mass` and both have to be right.
    body_kind kind = body_kind::dynamic;
};

/// A stable reference to a body in a `body_world`. Four bytes, 5.4's split.
using body_id = handle<rigid_body>;

// ---------------------------------------------------------------------------
// Mass
// ---------------------------------------------------------------------------

/// The body's mass in kilograms — `1/inv_mass`, with infinity for immovable.
///
/// Returns `HUGE_VALF` when `inv_mass` is 0, which is the true answer and is
/// also why you should reach for `inv_mass` in any arithmetic. This function is
/// for debug UI and for the rare piece of gameplay that genuinely wants
/// kilograms.
[[nodiscard]] float mass_of(const rigid_body& b);

/// Set the mass in kilograms, refusing the two inputs that are not masses.
///
/// A mass of 0 or less is rejected and the body is left alone — a negative mass
/// accelerates *towards* a push and would break the solver's assumptions from
/// 8.9 onward in ways that are very hard to trace back here, and a mass of zero
/// is the infinite-acceleration case the `inv_mass` doc comment describes. Pass
/// `HUGE_VALF` to make a body immovable, or set `inv_mass = 0` directly, which
/// is what `make_fixed` does.
///
/// @return true if the mass was applied.
bool set_mass(rigid_body& b, float kilograms);

// ---------------------------------------------------------------------------
// Applying things
// ---------------------------------------------------------------------------

/// Add a force, in newtons, for the duration of the next step.
///
/// **Accumulates.** Call it from as many systems as you like, in any order; they
/// sum, because Newton's second law is linear in F. §4 measures the one way that
/// sentence is not quite true in floating point, and what it costs.
///
/// No application point, so no torque: this body cannot spin (8.3).
void add_force(rigid_body& b, vec3 newtons);

/// Add an impulse, in newton-seconds, applied **immediately** to the velocity.
///
/// An impulse is a force integrated over a time — `J = F*dt` — and the reason it
/// is a separate entry point is that the time in question is shorter than a step
/// and nobody knows what it is. A bat hitting a ball, a bullet landing, a
/// character jumping: the interesting quantity is the total change in momentum,
/// `Δv = J/m`, and it does not depend on `h` at all.
///
/// **THIS IS THE FRAME-RATE BUG YOU WILL ACTUALLY HIT.** Write a jump as
/// `add_force(b, up * 500.0f)` for one step and the resulting velocity is
/// `500*h/m`, so the character jumps twice as high at 30 Hz as at 60 — §6
/// measures four rates and four jump heights. Write it as
/// `add_impulse(b, up * 8.0f)` and every rate gives the same jump, because there
/// is no `h` in the arithmetic.
///
/// Every contact response in 8.9 and every constraint in 8.10 is an impulse, for
/// exactly this reason: a collision does not last a step, and pretending it does
/// makes the bounce depend on the frame rate.
void add_impulse(rigid_body& b, vec3 newton_seconds);

/// Add an acceleration directly, in m/s², bypassing the mass.
///
/// For fields rather than pushes. Gravity is the canonical one — it is a force
/// of `m*g`, so the mass cancels and every body accelerates at `g` regardless of
/// what it weighs — and a magnetic sweep, a vertical wind or a designer's
/// "everything in this volume drifts upward" are the same shape.
///
/// Implemented as `add_force(b, a / inv_mass)`, which is `m*a`, so an immovable
/// body is unaffected and the accumulator still sums. That round trip through
/// the mass looks pointless and is the thing that makes it correct: a field that
/// wrote the velocity directly would move the floor.
void add_acceleration(rigid_body& b, vec3 metres_per_second_squared);

/// Discard any accumulated force without stepping. Mostly for tests and for a
/// system that changed its mind.
void clear_force(rigid_body& b);

// ---------------------------------------------------------------------------
// Torque, and forces that are not aimed at the centre
// ---------------------------------------------------------------------------

/// Add a pure torque, in newton-metres about the centre of mass, world axes.
///
/// "Pure" means it spins the body without pushing it anywhere — which is
/// physically what you get from a *couple*, a pair of equal and opposite forces
/// offset from each other. A reaction wheel, a motor mounted on the body, a
/// character's own effort to right themselves: all torque, no force.
void add_torque(rigid_body& b, vec3 newton_metres);

/// Apply a force at a **world-space point**, which is what almost everything in
/// a real game actually does.
///
///     force  += F
///     torque += (point - centre of mass) x F
///
/// One call, both accumulators, and the cross product is the entire content of
/// Lesson 8.3 §3. Note what falls out of it for free: a force aimed straight at
/// the centre of mass produces `r x F` with `F` parallel to `r`, which is
/// exactly zero — so pushing a crate squarely in the middle does not spin it,
/// and pushing it at a corner does, without either case being special-cased.
///
/// **The point is in world space and the body's centre of mass is
/// `state.position`.** This engine has no concept of a centre of mass offset
/// from the origin of the body: `inertia_assembly` computes where a compound
/// body balances so that you can place it there, rather than carrying an offset
/// through every subsequent calculation. 8.3 §5 explains the trade.
void add_force_at(rigid_body& b, vec3 newtons, vec3 world_point);

/// The impulse form of `add_force_at`: an instantaneous change in both
/// velocities, with no `h` anywhere in it.
///
///     velocity         += J * inv_mass
///     angular_velocity += world_inv_inertia * ((point - centre) x J)
///
/// 8.2 §6 argued that anything instantaneous must be an impulse rather than a
/// one-step force, and the argument carries over word for word — with one extra
/// consequence that is the reason 8.9 exists. **This function is a contact
/// response.** A ball striking a plank off-centre spins the plank and slows
/// itself, and both halves of that are this one call applied twice with opposite
/// signs. Everything Lesson 8.9 adds is deciding what `J` should be.
///
/// Costs one basis change (`world_inv_inertia`), which is two matrix products.
/// A solver applying thousands of these per frame hoists that out — 8.10's cached
/// per-body tensor — and this entry point is the correct, obvious one.
void add_impulse_at(rigid_body& b, vec3 newton_seconds, vec3 world_point);

/// Change the angular velocity directly by an angular impulse, in
/// newton-metre-seconds: `omega += world_inv_inertia * L`.
void add_angular_impulse(rigid_body& b, vec3 newton_metre_seconds);

/// Zero the torque accumulator. `clear_force` does not — they are separate so
/// that a debug overlay can consume one and leave the other.
void clear_torque(rigid_body& b);

// ---------------------------------------------------------------------------
// Inertia
// ---------------------------------------------------------------------------

/// Give a body an inertia tensor, expressed **about its centre of mass, in body
/// axes** — normally straight from one of inertia.hpp's shape functions.
///
/// Stores the inverse, validates first, and returns false without changing
/// anything if the tensor is not one a real mass distribution could have:
/// asymmetric, non-positive, or violating the triangle inequality. The
/// refusal is the same shape as `set_mass`'s and for the same reason — a body
/// that reaches the step with a nonsense tensor gains energy from nowhere, and
/// the symptom appears several seconds and several systems away from the cause.
///
/// **A thin rod is refused**, because `inertia_thin_rod` has an exact zero
/// principal moment and its inverse does not exist. Give it a real radius; every
/// physical object has one.
bool set_inertia(rigid_body& b, const mat3& body_inertia);

/// The body's inertia tensor about its centre of mass in body axes.
///
/// Free — it reads `inertia_local`, which is stored. It was not free before
/// §12; see that field's note.
[[nodiscard]] mat3 inertia_of(const rigid_body& b);

/// The body's **world-space** inverse inertia tensor: `R * inv_inertia_local *
/// R^T`, rebuilt from the current orientation.
///
/// The one quantity every rotational calculation in the rest of this module
/// starts from, and the reason `world_inverse_inertia` in inertia.hpp is worth
/// its own function: this is called once per body per step by `step`, and once
/// per contact per iteration by 8.10 unless it is hoisted.
[[nodiscard]] mat3 world_inv_inertia(const rigid_body& b);

/// The body's angular velocity after one step's worth of the gyroscopic term
/// alone — no applied torque, no damping.
///
/// Exposed as a function rather than buried in `step` because §9 compares the
/// two modes against each other and against doing nothing, and a measurement of
/// a branch inside a loop is a measurement of the loop.
///
/// `mode::off` returns `b.angular_velocity` unchanged, bit for bit.
[[nodiscard]] vec3 gyroscopic_step(const rigid_body& b, float h, gyroscopic_mode mode);

/// Angular momentum in world space: `I_world * omega`.
///
/// **The quantity to watch.** With no torque applied this is conserved exactly,
/// forever, while `angular_velocity` is not — it changes direction and magnitude
/// on its own, which is not a bug and is the subject of 8.3 §9. A debug overlay
/// that plots `|L|` is the single most useful rotational diagnostic there is,
/// because a rising `|L|` on a torque-free body means the integrator is inventing
/// energy and nothing else does.
[[nodiscard]] vec3 angular_momentum(const rigid_body& b);

/// Total kinetic energy, in joules: `(1/2) m v^2 + (1/2) omega . (I omega)`.
///
/// The second term is the rotational half, and its shape is worth noticing: it
/// is a quadratic form in `omega` rather than a product of scalars, which is the
/// price of the resistance-to-turning being a tensor. Like `|L|`, it should be
/// flat on a torque-free body.
[[nodiscard]] float kinetic_energy(const rigid_body& b);

/// The world-space velocity of a point rigidly attached to the body:
/// `v + omega x r`, with `r` measured from the centre of mass.
///
/// The formula that makes a rigid body rigid, and the one every contact in 8.9
/// is written against — a collision does not happen at a body's centre, it
/// happens at a point on its surface which may be moving very differently. A
/// wheel's contact patch is the standard demonstration: its centre moves forward
/// at `v`, and the patch touching the road is instantaneously **stationary**,
/// because `omega x r` exactly cancels `v` there.
[[nodiscard]] vec3 point_velocity(const rigid_body& b, vec3 world_point);

/// Where a point authored in body coordinates is right now, in world space.
///
/// `position + rotate(orientation, local)`. The other half of the pair above,
/// and what a debug renderer needs to draw a box around a tumbling body.
[[nodiscard]] vec3 world_point_of(const rigid_body& b, vec3 local_point);

// ---------------------------------------------------------------------------
// Terminal speeds — the two that look the same and are not
// ---------------------------------------------------------------------------

/// Terminal speed under `rigid_body::damping`: `g/k`. **Independent of mass.**
///
/// Falling under gravity and velocity-space damping, the speed settles where
/// `g = k*v`. There is no mass in that equation because `damping` was defined
/// without one, so a paper cup and a piano damped identically fall at identical
/// terminal speeds. Read this function and the next one together.
[[nodiscard]] float terminal_speed_damped(float g, float k);

/// Terminal speed under a linear drag FORCE `F = -b*v`: `m*g/b`. **Proportional
/// to mass.**
///
/// Falling under gravity and a drag force, the speed settles where `m*g = b*v`.
/// This is the physical one, it is the reason a cannonball beats a feather, and
/// getting it costs one line at the call site:
///
///     add_force(b, body.state.velocity * -drag_coefficient);
///
/// @param b_coefficient Drag coefficient in N·s/m. Must be positive.
[[nodiscard]] float terminal_speed_dragged(float g, float mass, float b_coefficient);

// ---------------------------------------------------------------------------
// Units, and the scaling law
// ---------------------------------------------------------------------------

/// Time to fall `distance` metres from rest under acceleration `g`:
/// `sqrt(2d/g)`.
///
/// A function rather than a comment because §7 measures against it, and because
/// it is the cleanest possible statement of the thing the next function is
/// about: the time depends on the square root of a LENGTH.
[[nodiscard]] float free_fall_time(float distance, float g);

/// The factor every duration in a world is multiplied by when every length in it
/// is multiplied by `length_scale` and gravity is left alone: `sqrt(s)`.
///
/// **THIS IS WHY MINIATURES LOOK LIKE MINIATURES**, and it is the whole of §7.
/// A model ship at 1/8 scale sits in water that falls at the same 9.81 m/s² the
/// real one does, so every splash, every roll and every collapse happens
/// `sqrt(8) = 2.83` times too fast, and an audience reads that instantly without
/// being able to say why. Film crews fix it by overcranking the camera by
/// exactly this factor.
///
/// A game has the same problem in reverse and the symptom has a name. Model your
/// world twice as large as it depicts — a "1.8 m" character built 3.6 units tall
/// — and everything in it takes `sqrt(2) = 1.41` times too long to fall. That is
/// the complaint "the jumping feels floaty", and the fix is not a bigger `g`;
/// see §7.4 for what a bigger `g` does and does not repair.
[[nodiscard]] float time_scale_for_length_scale(float length_scale);

// ---------------------------------------------------------------------------
// Frames: what a parent would have done to you
// ---------------------------------------------------------------------------

/// What integrating a body in some other space would actually produce.
///
/// Nothing in `body_world` calls this. It exists so that the rule in the file
/// header — **bodies are world space, full stop** — is a measurable claim rather
/// than an assertion, and so that a tool can point at a scene node and say why
/// physics must not happen there.
struct frame_report
{
    /// Where a local-space `gravity` actually points once the frame has had its
    /// way with it, in world space.
    vec3 gravity_in_world{};

    /// `|gravity_in_world| / |gravity|`. 1 for any rigid frame — a rotation and
    /// a translation cannot change a length — and equal to the scale factor for
    /// a uniformly scaled one. **2.0 means the body falls at twice gravity.**
    float gain = 1.0f;

    /// The angle between where gravity was supposed to point and where it does,
    /// in degrees. Nonzero whenever the frame rotates, which is expected and
    /// fine for a rotation alone; the interesting case is the one where a
    /// NON-UNIFORM scale changes an angle that a rotation had already set.
    float tilt_degrees = 0.0f;

    /// The three column lengths of the frame's linear part — its scale.
    vec3 scale{1.0f, 1.0f, 1.0f};

    /// `transform_extraction::out_of_square` for this frame: the worst |cosine|
    /// between two of its axes, so 0 for anything square and positive for shear.
    ///
    /// **Lesson 7.5 built this number for a completely different reason** — to
    /// report what a `quat` could not hold when a `mat3` field was narrowed —
    /// and it is the exact diagnostic a physics frame needs, because a sheared
    /// frame is one that does not preserve angles, and a frame that does not
    /// preserve angles turns a sphere into an ellipsoid and a straight fall into
    /// a slanted one.
    float out_of_square = 0.0f;

    /// Is the scale the same on all three axes (within a tolerance)? A uniform
    /// scale is survivable — it changes `gain` and nothing else. A non-uniform
    /// one changes DIRECTIONS, and no amount of tuning recovers from that.
    bool uniform = true;

    /// Is this frame safe to integrate a body in? True only for a frame that is
    /// a rotation and a translation: `gain` 1, square, uniform.
    bool inertial = true;
};

/// Measure what `world_from_parent` would do to a body integrated in its space.
///
/// @param gravity The acceleration as the body's own space would express it.
[[nodiscard]] frame_report inspect_frame(const mat4& world_from_parent, vec3 gravity);

/// Convert a body's world position into a local `transform`, for a scene node
/// whose parent's world matrix is `world_from_parent`.
///
/// The bridge, and the only one: the body owns world space, the scene owns local
/// space, and this converts at the boundary once per frame rather than letting
/// either one leak into the other. `authored` supplies the rotation and scale,
/// which physics does not own yet (8.3 takes the rotation).
[[nodiscard]] transform place_in_parent(const rigid_body& b, const mat4& world_from_parent,
                                        const transform& authored);

// ---------------------------------------------------------------------------
// The world
// ---------------------------------------------------------------------------

/// What one call to `body_world::step` did.
///
/// Every field here is something a later lesson in this module needs, and they
/// are cheap because the step is already walking every body.
struct step_report
{
    /// How many bodies of each kind the walk saw.
    int bodies = 0;
    int dynamic = 0;
    int kinematic = 0;
    int fixed = 0;

    /// The fastest body's speed at the END of the step, in m/s.
    float max_speed = 0.0f;

    /// **How far the fastest body moved during this step, in metres.**
    ///
    /// The tunnelling number. A body that travels further in one step than the
    /// thickness of the wall it is heading for passes through it without any
    /// test ever seeing an overlap, because the tests in 8.4 onward are all
    /// asked about a body's position and never about the segment between two of
    /// them. At 60 Hz a bullet at 400 m/s covers 6.67 m per step.
    ///
    /// Nothing here fixes it; reporting it is how you find out that you need to.
    /// Lesson 1.8's swept test is the shape of the answer, and 8.13's character
    /// controller is where it stops being optional.
    float max_travel = 0.0f;

    /// Total linear momentum of every dynamic body, `sum(m*v)`, in kg·m/s.
    ///
    /// **The conservation law this module is eventually judged by.** Gravity and
    /// thrusters change it — they are external forces, and that is what an
    /// external force IS — but a collision between two bodies must not, to the
    /// last bit the arithmetic allows. 8.9 checks its impulse response against
    /// this field; measuring it now costs one multiply-add per body and means
    /// the instrument predates the thing it measures.
    ///
    /// Immovable bodies are excluded, because their momentum is either zero or
    /// infinite and neither is a useful contribution to a sum.
    vec3 momentum{};

    /// Total **angular** momentum of every dynamic body about the world origin,
    /// in kg·m²/s. Lesson 8.3.
    ///
    /// Two terms per body, and the second one surprises people: `I_world*omega`
    /// is the body's spin about its own centre, and `r x (m*v)` is the angular
    /// momentum its linear motion has *about the origin*. A body sailing past in
    /// a straight line without rotating at all has angular momentum about any
    /// point not on its path, which is why a planet's orbit conserves one.
    ///
    /// Like `momentum`, it is the law a collision response is judged against —
    /// and it is the stricter of the two, because getting the contact point
    /// wrong changes this without changing that.
    vec3 angular_momentum{};

    /// The fastest-spinning body's angular speed at the end of the step, rad/s.
    ///
    /// The rotational twin of `max_speed`, and it has its own tunnelling
    /// analogue: a body turning more than a radian or so per step cannot have
    /// its contacts tracked frame to frame, because the features that were
    /// touching are on the other side by the time the next test runs. 8.8's
    /// manifold persistence is where that bites.
    float max_spin = 0.0f;

    /// The largest `| |q| - 1 |` over all bodies, measured as the step **found**
    /// them — before it touched anything.
    ///
    /// A pure instrument, and it is deliberately *not* a measurement of the
    /// integrator's drift. `step` renormalises on the way out, so by the time
    /// the next step looks, the integrator's own inflation is already gone and
    /// what is left is a couple of ulps. §7 measures the inflation in the
    /// harness, where the un-normalised intermediate is still visible.
    ///
    /// What this catches is the other thing: **code outside the step writing
    /// `orientation` without normalising it.** A gameplay system that lerps two
    /// rotations, an animation blend, a network packet, a hand-authored
    /// `quat{0.7f, {0.7f, 0, 0}}` — all of them leave a body slightly off the
    /// unit sphere, all of them render as a subtle shear that is very hard to
    /// see and impossible to search for, and all of them show up here as a
    /// number that is not 1e-7.
    float max_unit_error = 0.0f;
};

/// The table of bodies, and the thing that steps them.
///
/// **A `pool<rigid_body>` and eight lines of policy**, which is deliberate:
/// Lesson 5.4 built generational handles precisely so that subsystems would not
/// each invent their own storage, and a body table is the textbook case for it —
/// bodies are created and destroyed constantly, everything else in the engine
/// wants to refer to them, and the solver in 8.10 wants them in a flat array it
/// can walk without chasing a pointer.
///
/// What the pool gives for free and is worth naming: bodies live in a **dense**
/// array with no holes, so `bodies()` is a contiguous span and `step` is a linear
/// walk; a removal swaps the last body into the hole, so a stale `body_id` fails
/// its generation check instead of silently naming whichever body moved in; and
/// the whole table serialises as integers, which Module 9's scene format needs.
class body_world
{
public:
    /// Add a body. The `body_id` is stable across every later insertion and
    /// removal.
    body_id add(const rigid_body& body);

    /// Remove a body. Returns false if the handle was already stale.
    bool remove(body_id id);

    /// Resolve a handle, or null if it is stale. See `handle::valid` for why
    /// this is a different question from "is the handle non-null".
    [[nodiscard]] rigid_body* get(body_id id);
    [[nodiscard]] const rigid_body* get(body_id id) const;

    /// Every body, densely packed, in no particular order.
    ///
    /// **THE ORDER IS NOT STABLE** — a removal swaps the last element into the
    /// gap — and code that iterates this span must not depend on it. That is not
    /// a defect to work around; it is what keeps the walk contiguous, and 5.4
    /// §6 measured what the alternative costs.
    [[nodiscard]] std::span<rigid_body> bodies();
    [[nodiscard]] std::span<const rigid_body> bodies() const;

    [[nodiscard]] std::size_t size() const;
    void clear();

    /// The world's gravity, applied to every dynamic body scaled by its
    /// `gravity_scale`. Defaults to `k_gravity_down`.
    void set_gravity(vec3 g);
    [[nodiscard]] vec3 gravity() const;

    /// The integration rule, for every body. Defaults to semi-implicit Euler,
    /// which Lesson 8.1 settled and this lesson does not relitigate.
    void set_integrator(integrator rule);
    [[nodiscard]] integrator integrator_rule() const;

    /// How orientations are advanced. Lesson 8.3 §7, and the default is
    /// `linearised` — the cheap one — because §12 measures the difference and
    /// the honest answer is that it depends on what you are simulating.
    void set_spin_rule(spin_rule rule);
    [[nodiscard]] spin_rule spin() const;

    /// Advance every body by `h` seconds.
    ///
    /// **THE ORDER OF OPERATIONS IS THE LESSON**, and it is four steps:
    ///
    ///   1. `a = F * inv_mass` — one multiply, once, after every system has
    ///      added what it had to say, which is the entire reason the accumulator
    ///      exists;
    ///   2. `a += gravity * gravity_scale`, AFTER the division rather than
    ///      before it as a weight of `m*g`. Both are correct physics and only
    ///      one of them needs the mass: §10.1 measures the divide that the other
    ///      arrangement costs at 15% of the whole update. **The consequence is
    ///      that `force` does not contain the body's weight** — see §9, and use
    ///      `mass_of(b) * world.gravity()` if a tool needs to draw it;
    ///   3. the body is integrated by the chosen rule, then damped by 8.1's
    ///      exact `exp(-k*h)`;
    ///   4. the force is cleared, LAST, so that anything wanting to look at what
    ///      was applied this step still can.
    ///
    /// Kinematic bodies skip 1, 2 and the velocity half of 3: they travel at
    /// whatever velocity you set and nothing else touches them. Fixed bodies are
    /// skipped entirely.
    const step_report& step(float h);

    /// What the last `step` saw.
    [[nodiscard]] const step_report& report() const;

private:
    pool<rigid_body> bodies_;
    vec3 gravity_ = k_gravity_down;
    integrator rule_ = integrator::semi_implicit_euler;
    spin_rule spin_ = spin_rule::linearised;
    step_report report_{};
};

// ---------------------------------------------------------------------------
// Small conveniences
// ---------------------------------------------------------------------------

/// A dynamic body of `mass` kilograms at `position`, at rest.
///
/// **Its inertia tensor is the default identity**, which is no shape at all. A
/// body made this way will spin, and will spin wrong. Use `make_box` or
/// `set_inertia` for anything whose rotation is going to be looked at; this one
/// remains because most of what a physics engine simulates never rotates
/// visibly, and because 8.2's demos and harness call it.
[[nodiscard]] rigid_body make_dynamic(vec3 position, float mass);

/// A dynamic body shaped like a box: mass, half-extents, and the matching
/// inertia tensor, all set consistently. Lesson 8.3.
///
/// The convenience that makes the common case right by default. Note that this
/// is the FIRST factory in the file that could not have existed in 8.2 — a mass
/// and a position are enough to describe a point, and a shape is the extra thing
/// rotation needs.
[[nodiscard]] rigid_body make_box(vec3 position, float mass, vec3 half_extents);

/// A dynamic body shaped like a solid sphere. Lesson 8.3.
///
/// The only shape whose tensor is a multiple of the identity — so it is the only
/// shape for which "resistance to turning" really is one number, and the only
/// one that behaves the way the single-`float` version of this engine's angular
/// dynamics would have.
[[nodiscard]] rigid_body make_sphere(vec3 position, float mass, float radius);

/// An immovable body at `position`: `inv_mass` 0, kind `fixed`.
///
/// **Its inverse inertia tensor is all zeros too**, from 8.3 onward, which is
/// the same statement as `inv_mass = 0` made nine floats wide: no torque can
/// spin it, exactly, with no branch and no sentinel.
[[nodiscard]] rigid_body make_fixed(vec3 position);

/// A kinematic body at `position` moving at `velocity`: immovable by forces,
/// moved by the step, unaffected by gravity.
[[nodiscard]] rigid_body make_kinematic(vec3 position, vec3 velocity);

} // namespace engine::phys
