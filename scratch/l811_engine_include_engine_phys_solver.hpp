// engine/include/engine/phys/solver.hpp — the first file in phys/ that changes
// anything, and then the loop around it.
//
// Lessons 8.9 and 8.10. 8.9 resolves ONE contact; 8.10 resolves a world of them
// and is the second half of this header, from `position_correction` onward.
//
// Five lessons of collision detection have built a machine that
// ANSWERS QUESTIONS. 8.4 asks whether two boxes overlap, 8.5 how far apart two
// convex shapes are, 8.6 how deep they are inside each other and which way is
// out, 8.7 at which four points, and 8.8 which pairs are worth asking about at
// all. Run the whole of it on a crate falling onto a floor and the crate falls
// through the floor, because nothing in `engine::phys` has ever written a
// velocity.
//
// This file writes velocities.
//
// ---- WHY THE ARITHMETIC IS IN VELOCITY AND NOT IN FORCE --------------------
//
// The obvious response to "these two objects are 3 mm inside each other" is a
// spring: push them apart with a force proportional to the overlap, let `F = ma`
// do the rest, and reuse every line of 8.2. It is called a PENALTY METHOD, it is
// four lines long, and it does not work at the rates a game runs at.
//
// The stiffness is not a free parameter. A crate of mass `m` resting on a floor
// needs `k*x = m*g` at whatever penetration `x` you are willing to see, so
// `k = m*g/x` — and a spring of stiffness `k` on a mass `m` oscillates at
// `omega = sqrt(k/m) = sqrt(g/x)`, which does not contain the mass at all.
// 8.1 measured semi-implicit Euler's stability limit at `h*omega < 2` and
// `max_stable_step` returns it. Put the two together and the allowed penetration
// alone decides the rate the whole simulation has to run at:
//
//     1 cm of sink  -> omega =  31.3 rad/s -> needs at least  15.7 Hz
//     1 mm of sink  -> omega =  99.0 rad/s -> needs at least  49.5 Hz
//     0.1 mm        -> omega = 313.2 rad/s -> needs at least 156.6 Hz
//
// At 60 Hz the best a STABLE penalty spring can offer is 0.68 mm, and 8.1 §6 is
// the reason "stable" is not the same as "right": at `h*omega = 1.95` the orbit
// is perfectly bounded and swings by a factor of forty.
//
// **AND THE STATIC NUMBER IS THE OPTIMISTIC ONE.** A spring tuned to hold a
// RESTING crate at 1 mm has no say whatever in how far a MOVING one sinks: the
// impact's kinetic energy all has to go into the spring, so the dynamic
// penetration is `v*sqrt(m/k)` and depends on the impact speed rather than on
// the weight. §1 measures a crate arriving at 2 m/s going 44 mm into a floor
// tuned for 1 mm — forty-four times the design target — and computes the
// stiffness the 1 mm really wanted, which turns out to demand nearly a kilohertz.
// See `penalty_impact_stiffness`.
//
// The impulse formulation has no stiffness in it, because it does not model the
// contact as a thing that lasts. **A collision is an event, not an interval.**
// The question it answers is not "what force acts during the overlap" but "what
// single instantaneous change in velocity leaves the two surfaces moving the way
// a contact requires" — and 8.2 §6 already established that anything
// instantaneous must be an impulse, because `delta_v = J/m` has no `h` in it and
// therefore produces the same answer at 30 Hz and at 144.
//
// ---- THE THREE THINGS A CONTACT WANTS -------------------------------------
//
// Everything in this file is one of three demands on the relative velocity at a
// contact point, and each is one linear equation in one scalar unknown:
//
//   NON-PENETRATION. The surfaces may separate and may slide; they may not
//     approach. The impulse that enforces it may only PUSH, never pull, which is
//     a `max(0, ...)` and is the only nonlinearity in the normal solve.
//   RESTITUTION. A bouncy contact leaves at `e` times the speed it arrived at.
//     `e = 0` is a dead landing, `e = 1` is a superball. It rides on the same
//     equation as non-penetration: change the target velocity from zero to
//     `-e * approach_speed` and the same solve delivers it.
//   FRICTION. The tangential velocity wants to be zero — that is what "not
//     sliding" means — but only as far as Coulomb's inequality allows, so the
//     tangential impulse is clipped to a cone of radius `mu * normal_impulse`.
//
// The third is coupled to the first two: the cone's radius is the answer to the
// normal solve. That coupling is why friction cannot be a second independent
// impulse computed in parallel, and §10 measures what solving them in the wrong
// order costs (a box on a slope that slides on its first frame of contact and
// then stops, once, every time it lands).
//
// ---- WHAT THIS LESSON DELIBERATELY DOES NOT DO -----------------------------
//
// **It does not fix penetration.** Every equation here is about velocity, so two
// bodies that begin the step already overlapping end it still overlapping, with
// the overlap frozen rather than repaired. 8.9 §12 measures the residue and it
// is small and permanent. Pushing them apart is a POSITION problem, it
// introduces energy that the velocity solve did not ask for, and doing it
// without making a stack of crates jitter is most of what 8.10 is about.
//
// **It does not iterate.** `solve_contacts` makes one pass over the points of
// ONE manifold. Two crates leaning on each other are two manifolds that disagree
// about the answer, and reconciling them is a system of inequalities rather than
// an equation — 8.10, and the reason this file's types are shaped for a solver
// that will call them many times per step.
//
// **It does not warm start.** `contact_point::normal_impulse` has been on the
// manifold since 8.7 and `carry_impulses` has been matching ids across frames
// since 8.7 §9; `write_back` finally puts a number in the field, and
// `solver_config::warm_start` reads it. On a single pass it buys close to
// nothing, which 8.9 §12 measures rather than assumes. It is 8.10 that makes it
// matter.
//
// ---- AND WHAT LESSON 8.10 ADDED -------------------------------------------
//
// All three of those, and a fourth thing that turns out to be the difference
// between a demo and an engine.
//
//   **ITERATION.** `contact_solver` sweeps every contact in the world
//     `velocity_iterations` times, which is Gauss-Seidel: each impulse is used
//     by the next contact as soon as it is computed. 8.10 §3 measures the
//     residual falling by a constant factor of **0.53 per sweep**.
//
//   **WARM STARTING**, on by default, which is what makes a handful of sweeps
//     enough instead of hundreds — 89 iterations against more than 400 on a
//     settled five-crate tower. It also exposed a bug three lessons old: the
//     two cached FRICTION impulses are coordinates in a basis that was never
//     stored, and `tangent_basis` re-chooses that basis every frame from the
//     normal's smallest component. See `contact_manifold::tangent`.
//
//   **A POSITION CORRECTION**, because iteration fixes the residual and cannot
//     touch the overlap the contact ARRIVED with. Two of them, and the default
//     is the one that adds no energy. See `position_correction`.
//
//   **ISLANDS AND SLEEPING.** The contact graph, partitioned by a union-find
//     in which a body that cannot move is not a bridge; and, on top of it, a
//     rule for taking whole islands out of the simulation. Sleeping is what
//     makes a settled scene cost what an empty one costs, and the waking half
//     of it is the half that has to be right.
//
// `body_world::step` was split into `integrate_velocities` and
// `integrate_positions` in the same edit, because a contact solve has to happen
// between them and there was no way to say so.
//
// ---- AND WHAT LESSON 8.11 ADDED -------------------------------------------
//
// Joints, in the same loop. `constraint.hpp` writes a constraint as a Jacobian
// row and builds rods, ropes, ball-sockets and hinges out of rows; this file
// learned three things to carry them:
//
//   **THE SOLVER HOLDS JOINTS BESIDE MANIFOLDS.** `contact_solver` is now
//     `constraint_solver`, because a class that solves hinges is not a contact
//     solver, and the old name survives as an alias so that 8.10's demo and
//     harness still compile unchanged. `add` gains an overload for a joint.
//     Within every sweep of every island the joints are visited first and the
//     contacts second — Box2D's order, argued rather than measured; the
//     argument is in `solve`, and measuring it is one of 8.11's exercises.
//
//   **A JOINT IS AN EDGE OF THE ISLAND GRAPH.** A chain whose links do not
//     touch is still one island, or it would sleep a link at a time — and a
//     sleeping link is an immovable body to its neighbour, so the chain hangs
//     from a frozen link in mid-air. 8.11 §12 measures it. A joint to a fixed
//     body is not a bridge, for 8.10's reason.
//
//   **`pseudo_velocity` MOVED DOWN A LAYER**, into `constraint.hpp`, because
//     joints need it and joints sit below the loop. 8.10's comment on it
//     predicted exactly this. Nothing that includes this header notices.
//
// What did NOT change is the contact solve. A contact normal IS a Jacobian row
// — 8.11 §3 proves it numerically, row by row — and it stays hand-written;
// §3 measures why.
//
// ---- A NOTE ON WHERE THE MATERIAL LIVES ------------------------------------
//
// 8.8's handover said this lesson would put `restitution` and `friction` on
// `rigid_body`. It does not, and the reason is worth more than the convenience:
// **friction is a property of a PAIR of surfaces, not of a surface.** Rubber on
// rubber is 1.16, ice on ice is 0.10, and rubber on ice is 0.15 — which no
// function of 1.16 and 0.10 reproduces exactly, because there is no such
// function. Every engine stores per-body values and combines them anyway,
// because a pair table is O(n^2) in materials and nobody authors one; but the
// combination is an approximation, and burying it in `rigid_body` makes it look
// like a lookup. So `contact_material` is an argument to `prepare_contacts` and
// `combine_material` is a named and switchable rule — and §8, having actually
// checked the rules against published coefficients, ships a DIFFERENT default
// from every engine this course draws on.

#pragma once

#include <engine/math/mat3.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/constraint.hpp>
#include <engine/phys/manifold.hpp>
#include <engine/phys/rigid_body.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace engine::phys
{

// ---------------------------------------------------------------------------
// Materials
// ---------------------------------------------------------------------------

/// **How a pair of surfaces behaves when they meet.** Two numbers.
///
/// Both are dimensionless and both are ratios, which is why neither carries a
/// unit and why neither depends on the mass, the speed or the scale of the
/// scene — at least in the model. Reality is less tidy and §6 says where.
struct contact_material
{
    /// The **coefficient of restitution**, `e`: the fraction of the approach
    /// speed that comes back as separation speed.
    ///
    /// `0` is a dead landing (a crate on concrete), `1` is a perfectly elastic
    /// collision that conserves kinetic energy exactly, and values above 1 add
    /// energy from nowhere and will eventually make your scene explode — the
    /// solver does not forbid them, because a trampoline pad or a bumper is a
    /// legitimate thing to fake this way, but it is a decision and not a
    /// material.
    ///
    /// **IT IS NOT A CONSTANT OF THE MATERIAL**, which is the part every table
    /// of coefficients leaves out. A real `e` falls as the impact speed rises,
    /// because more of the energy goes into permanently deforming the thing; a
    /// steel ball is nearly elastic at 0.1 m/s and visibly not at 10. Games use
    /// one number per material because the error is invisible next to the error
    /// in everything else.
    float restitution = 0.0f;

    /// The **Coulomb coefficient of friction**, `mu`: the largest ratio of
    /// tangential to normal impulse the contact can sustain before it slides.
    ///
    /// The only thing `mu` says, physically, is the angle of a slope a thing
    /// will sit still on: `atan(mu)`. 0.5 is 26.6 degrees, 1.0 is 45, and
    /// `critical_slope_degrees` is that function, checked by bisection in §8
    /// against the engine's own simulation.
    ///
    /// **This engine has ONE coefficient, not two.** Real surfaces have a static
    /// coefficient slightly above the sliding one, which is why a heavy box
    /// lurches when it finally gives — see `solver_config::friction` for the
    /// reason a single number is the right call here anyway.
    float friction = 0.5f;
};

/// How two per-body materials become one pair material.
///
/// There is no physically correct answer — see the file header — so this is a
/// named convention rather than a derivation, and the name is in the call so
/// that a reader knows a choice was made.
enum class combine_rule : std::uint8_t
{
    /// `sqrt(a*b)`. **What Box2D, Bullet and PhysX all ship for friction, and
    /// what this engine does NOT use** — see `minimum` below, and §8.
    ///
    /// It behaves sensibly at the extremes (one frictionless surface makes the
    /// pair frictionless) and, unlike the minimum, it is *sensitive to both
    /// inputs*: a designer raising one material's friction sees the pair change.
    /// That is a real argument, and it is a usability argument rather than a
    /// physical one.
    geometric_mean,

    /// `min(a, b)`. **The default for both fields in this engine.**
    ///
    /// For restitution it is the obvious choice: a superball dropped into wet
    /// sand does not bounce, and the softer surface decides.
    ///
    /// For friction it is a decision §8 measured rather than inherited. Of the
    /// five surface pairs whose three coefficients are all tabulated — a on a,
    /// b on b, and a on b — the minimum predicts the pair with a mean error of
    /// **23%** against the geometric mean's **120%**, and it wins **100% of the
    /// time** when every coefficient is perturbed by up to 30% to account for
    /// how much published tables disagree. The physical story is that the
    /// interface is governed by the more lubricious of the two surfaces, which
    /// is why PTFE on anything behaves like PTFE and why the geometric mean
    /// predicts steel on PTFE at 0.172 where the real number is 0.04.
    minimum,

    /// `max(a, b)`. Occasionally what a designer wants for restitution — a
    /// bumper that is bouncy no matter what hits it.
    maximum,

    /// `(a + b)/2`. Cheapest, and the worst behaved: a frictionless surface
    /// still has half the friction of whatever touches it.
    average,
};

/// `"geometric mean"`, `"minimum"`, `"maximum"`, `"average"`.
[[nodiscard]] const char* name_of(combine_rule rule);

/// Apply a `combine_rule` to two numbers. Exposed because §8 sweeps it against
/// published coefficients, and because a material system in Module 9 will want
/// the same rule for its own fields.
[[nodiscard]] float combine(float a, float b, combine_rule rule);

/// Combine two per-body materials into the material of the pair.
///
/// **`minimum` for both**, which is not what the engines this course draws on
/// do, and §8 is why. There is no simulation that can settle the question — the
/// rule is an act of modelling — so the evidence is arithmetic against the five
/// surface pairs whose three coefficients are all published, plus a perturbation
/// sweep to check the answer survives how much those tables disagree. It does,
/// 100% of the time.
///
/// Pass `combine_rule::geometric_mean` for the industry-standard behaviour; the
/// argument for it is in `combine_rule` and it is about knobs rather than
/// surfaces.
[[nodiscard]] contact_material combine_material(contact_material a, contact_material b,
                                                combine_rule restitution_rule = combine_rule::minimum,
                                                combine_rule friction_rule = combine_rule::minimum);

// ---------------------------------------------------------------------------
// Friction models
// ---------------------------------------------------------------------------

/// **How the tangential impulse is clipped to Coulomb's limit.**
///
/// Coulomb's condition is `|j_t| <= mu * j_n` with `j_t` a two-dimensional
/// vector in the contact plane, so the admissible set is a DISC of radius
/// `mu*j_n`, and stacking that disc over the normal axis is where the phrase
/// "friction cone" comes from. The two implementations differ in whether they
/// respect that the set is round.
enum class friction_model : std::uint8_t
{
    /// No tangential impulse at all. Everything slides on everything, forever.
    /// Kept because it is the control for every friction measurement in §7 and
    /// §8, and because an ice level is a real thing.
    none,

    /// Clip each tangent component independently to `[-mu*j_n, mu*j_n]`.
    ///
    /// **The admissible set is a SQUARE circumscribing the true disc**, so the
    /// available friction depends on the direction of sliding: `mu*j_n` along a
    /// tangent axis and up to `sqrt(2)*mu*j_n` along the diagonal. That is a
    /// 41% anisotropy in a basis that `tangent_basis` picked arbitrarily from
    /// the normal, so it is a physical difference with no physical cause — a
    /// puck slid north stops in a different distance from one slid north-east.
    /// §7 sweeps 360 degrees of sliding direction and measures it.
    ///
    /// It is here because it is what most 2D engines and several famous 3D ones
    /// do, it is one `clamp` per axis instead of a `sqrt`, and in a scene full
    /// of crates nobody notices. **Naming the approximation is the point**; the
    /// default is the cone.
    box,

    /// Clip the tangential impulse as a VECTOR to `|j_t| <= mu*j_n`, scaling
    /// both components by the same factor.
    ///
    /// Isotropic, correct, and one `sqrt` more expensive than the box — which
    /// §7 measures rather than leaving as a worry.
    cone,
};

/// `"none"`, `"box"`, `"cone"`.
[[nodiscard]] const char* name_of(friction_model model);

// ---------------------------------------------------------------------------
// The geometry of one contact
// ---------------------------------------------------------------------------

/// **Two unit vectors spanning the plane perpendicular to `n`.**
///
/// There is no canonical answer — 3.5 and `vec3.hpp` both say so: in three
/// dimensions "perpendicular to this" names a whole circle of directions. Any
/// orthonormal pair will do, PROVIDED the choice is continuous enough not to
/// spin as the normal wobbles, and PROVIDED it never degenerates when `n`
/// happens to line up with whatever reference axis the construction uses.
///
/// The implementation is the standard branch on the smallest component of `n`,
/// which guarantees the cross product it takes is never near-parallel: the
/// worst case is a normal at 45 degrees to two axes, where the seed axis is
/// still 54.7 degrees away. §7 measures the worst orthonormality error over a
/// million directions.
///
/// **Under `friction_model::cone` the choice of basis cannot affect the
/// answer**, because a disc is round; under `box` it decides which directions
/// get 41% more friction. That asymmetry is the cleanest statement of what the
/// box model gets wrong, and §7 measures it by rotating the basis and watching
/// the physics change.
void tangent_basis(vec3 n, vec3& t1, vec3& t2);

/// The world-space velocity of `b`'s material point at `world_point`, **relative
/// to `a`'s**.
///
///     (v_b + omega_b x r_b) - (v_a + omega_a x r_a)
///
/// `point_velocity` from 8.2 is each half. The sign convention follows
/// `contact_manifold::normal`, which points from `a` toward `b`: so
/// `dot(contact_velocity(...), normal)` is NEGATIVE when the two are
/// approaching, positive when separating, and zero at a resting contact. Get the
/// sign backwards and every contact in the scene sucks instead of pushing, which
/// is a memorable ten minutes.
[[nodiscard]] vec3 contact_velocity(const rigid_body& a, const rigid_body& b, vec3 world_point);

/// **The effective mass of a contact along one direction**, in kilograms.
///
/// The single most important quantity in this file, and the reason it is a
/// scalar is the whole trick. Applying `J = j*dir` at a contact changes the
/// relative velocity at that contact along `dir` by exactly `j * k`, where
///
///     k = inv_m_a + inv_m_b
///       + dot(r_a x dir, inv_I_a * (r_a x dir))
///       + dot(r_b x dir, inv_I_b * (r_b x dir))
///
/// — a single number, independent of `j`, because every step from impulse to
/// velocity is linear. So "how big an impulse do I need to change this velocity
/// by that much" is a division rather than a solve, and this function returns
/// `1/k`, which has units of mass and behaves like one: it is the mass the
/// contact *appears* to have when pushed along `dir`.
///
/// **THE FORM ABOVE IS NOT THE ONE THE DERIVATION PRODUCES.** Working through
/// `delta_v + delta_omega x r` gives the angular terms as
/// `dot(dir, (inv_I * (r x dir)) x r)`, which is the same number by the scalar
/// triple product — and the rearrangement is worth doing, because the version
/// above is manifestly `w . (inv_I * w)` with `inv_I` positive semi-definite,
/// which proves `k >= inv_m_a + inv_m_b >= 0` and therefore that a contact
/// between two bodies that are not both immovable can never divide by zero. §4
/// evaluates both forms and reports their worst disagreement.
///
/// **Returns zero when both bodies are immovable**, rather than an infinity: two
/// static bodies overlapping is a level-design bug and the solver's job is to
/// apply no impulse, which `0 * anything` does without a branch. That is
/// `inv_mass = 0` doing the same work 8.2 §3 chose it for.
[[nodiscard]] float effective_mass(float inv_mass_a, const mat3& inv_inertia_a, vec3 r_a,
                                   float inv_mass_b, const mat3& inv_inertia_b, vec3 r_b,
                                   vec3 direction);

/// `effective_mass` for two bodies, computing each world inverse inertia tensor
/// on the spot.
///
/// Convenient and **not what the solver calls**: `world_inv_inertia` is two
/// matrix products, and a four-point manifold needs the tensor twelve times
/// (three directions per point). `prepare_contacts` computes it once per body
/// and keeps it in the batch. §13 measures the difference.
[[nodiscard]] float effective_mass(const rigid_body& a, const rigid_body& b, vec3 r_a, vec3 r_b,
                                   vec3 direction);

// ---------------------------------------------------------------------------
// Position correction — Lesson 8.10
// ---------------------------------------------------------------------------

/// **How the solver repairs penetration that has already happened.**
///
/// Iterating the velocity solve to convergence fixes one of 8.9's two failures
/// and cannot touch the other. A converged solve leaves the relative normal
/// velocity at zero, which means *the overlap stops growing* — it says nothing
/// at all about the overlap the contact ARRIVED with, and a crate arriving at
/// 3.6 m/s at 60 Hz arrives up to 60 mm deep however many passes you make.
/// 8.10 §6 measures that depth moving by **3%** across a sixty-four-fold range
/// of iteration count while the residual moves by six orders of magnitude.
///
/// Position error is not velocity error, so it needs its own machinery.
enum class position_correction : std::uint8_t
{
    /// Leave it. The stack rests at whatever depth it landed at.
    ///
    /// Not as useless as it sounds and it is the control for 8.10 §7 and §8:
    /// the
    /// depth is bounded by one step of approach travel, which for a scene of
    /// gently settling crates is under a millimetre, and a renderer that draws
    /// the shapes a millimetre inside each other is a renderer nobody
    /// complains about. It is unusable the moment anything arrives fast, is
    /// spawned overlapping, or is pushed into a wall by something that does
    /// not care.
    none,

    /// **Baumgarte stabilisation**: feed a fraction of the penetration back
    /// into the velocity solve as a target, so that the contact pushes apart
    /// rather than merely stopping.
    ///
    /// One line, no extra state, and it is what every first implementation
    /// does — Baumgarte 1972, by way of every rigid-body paper since. The
    /// penetration `d` beyond the slop becomes a bias velocity
    /// `beta*d/h`, added to the target, so a fraction `beta` of the excess is
    /// removed per step and the depth decays geometrically: 20% per step is a
    /// time constant of `-h/ln(0.8)` = **74.7 ms**, measured at 83.3 — which is
    /// 74.7 rounded up to a whole 16.67 ms step, and is as close as a
    /// measurement taken once a frame can come.
    ///
    /// **AND IT ADDS ENERGY, WHICH IS THE WHOLE OF THE ARGUMENT AGAINST IT.**
    /// The bias is a velocity the solver was not asked for and cannot tell
    /// apart from a real one, so when the overlap runs out the body is still
    /// carrying it: a crate spawned inside the floor does not rise to the
    /// surface and stop, it **leaves**. 8.10 §7 measures the departure at
    /// **1.140 m/s** from a 100 mm start with gravity off, and with gravity on
    /// finds the threshold at which the push beats the climb: a crate spawned
    /// 200 mm deep is thrown **99 mm clear of the floor**, and one spawned
    /// 300 mm deep reaches 215.
    baumgarte,

    /// **Split impulse**: run the same solve a second time against a *shadow*
    /// velocity that only ever moves positions, and never touches the real one.
    ///
    /// Catto's formulation, and it is Baumgarte's arithmetic with the energy
    /// leak cut off at the source. Each body carries a `pseudo_velocity` for
    /// the duration of the step; the position pass drives *that* toward the
    /// separation target with exactly the same effective masses, and
    /// `integrate_positions` is handed the sum. Nothing the position pass does
    /// survives into the next step, so the crate rises to the surface and
    /// **stops there** — measured at **0.00000 m/s** of departure against
    /// Baumgarte's 1.14006, and settling at the slop from every start depth
    /// 8.10 §8 tried, including the ones that throw a Baumgarte crate into the
    /// air.
    ///
    /// It costs a second solve. 8.10 §13 prices one position iteration at
    /// **46%** of a velocity one, and three position iterations against eight
    /// velocity ones is the default because the position problem is easier: no
    /// friction to clip, no restitution, and one target per point.
    split_impulse,
};

/// `"none"`, `"Baumgarte"`, `"split impulse"`.
[[nodiscard]] const char* name_of(position_correction correction);

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/// Knobs. Every default is something 8.9 or 8.10 measured.
struct solver_config
{
    /// Which Coulomb clip to use. Defaults to the cone; see `friction_model`.
    friction_model friction = friction_model::cone;

    /// **Below this approach speed, restitution is switched off.** Metres per
    /// second, and 1.0 is the number every engine ships.
    ///
    /// Without it a resting body with any `e > 0` never comes to rest: gravity
    /// gives it a few millimetres per second of approach each step, the solver
    /// gives back `e` times that, and the crate buzzes on the floor forever.
    /// The threshold is a discontinuity in the physics — an impact at 0.99 m/s
    /// behaves differently from one at 1.01 — and it is accepted universally
    /// because the alternative is visible and this is not. §6 measures the
    /// buzz amplitude with the threshold at zero.
    float restitution_threshold = 1.0f;

    /// **The velocity that external acceleration added to the bodies before
    /// this solve**, world space. Pass `gravity * h`; leave it zero if you solve
    /// contacts before integrating velocities.
    ///
    /// THE MOST SURPRISING FIELD IN THIS FILE, and it exists because of an
    /// artifact that looks like a physics bug and is an ordering bug. A
    /// semi-implicit Euler step applies gravity to the velocity FIRST, so by the
    /// time the solver sees the contact, the approach speed already contains
    /// this step's `g*h` — 16.35 cm/s at 60 Hz. Restitution then returns `e`
    /// times a speed that is too large by `g*h`, every bounce, and the error
    /// does not shrink with the bounce: a ball settles into a permanent hop at
    /// `e*g*h/(1-e)` metres per second, which is `terminal_bounce_speed` and is
    /// **65 cm/s at e = 0.8** — a ball that bounces two centimetres high
    /// forever. At `e = 0.95` it is 3.1 m/s and 49 centimetres, which no
    /// restitution threshold will hide.
    ///
    /// Subtracting this bias from the approach speed before applying `e` removes
    /// it exactly. §6 measures ten bounce heights against `e^(2n)*h0` with the
    /// field set and with it zero.
    vec3 restitution_bias{};

    /// Begin each point from the impulse it carried in the manifold.
    /// **Default changed to `true` in Lesson 8.10**, which is the lesson that
    /// earned it.
    ///
    /// `carry_impulses` (8.7) puts last frame's impulse in
    /// `contact_point::normal_impulse` when the ids match; with this set,
    /// `prepare_contacts` seeds the accumulator with it and
    /// `warm_start_contacts` applies it up front, so the solve begins from the
    /// velocity the scene would have if last frame's answer were still right.
    /// On a settled stack it is, to the third decimal.
    ///
    /// On 8.9's single pass it was close to a no-op. Over an iterated solve it
    /// is the difference between a tractable solver and an intractable one. A
    /// settled five-crate tower reaches a residual of 1e-4 in **89 iterations**
    /// warm and **has not reached it in 400** cold; and at the eight iterations
    /// this file ships, over twenty seconds with the position correction
    /// switched off, the cold tower sinks **2,020 mm** — it collapses — while
    /// the warm one holds at **40**. 8.10 §4.
    ///
    /// It is free in time: the impulses are already in the manifold, matched by
    /// 8.7's feature ids at a measured **97.6%** on a settled stack. What it
    /// costs is one `bool` of honesty — a warm start is an *initial guess*, and
    /// a guess that is wrong is worse than no guess. 8.10 §4 scales the
    /// inherited impulse deliberately and finds 2x costing more iterations than
    /// 0x, which is also why the accumulator is still clamped like any other.
    bool warm_start = true;

    /// **How many times to sweep every contact before moving anything.**
    /// Lesson 8.10, and the number that makes a stack a stack.
    ///
    /// One pass solves each contact exactly and each *in turn*, so point four's
    /// impulse undoes part of point one's and the pass ends with a residual
    /// that 8.9 measured at 2.6e-02 m/s on a single crate. Sweeping again
    /// re-solves each point against what the others have since done, which is
    /// Gauss–Seidel, and the residual falls geometrically: 8.10 §3 measures a
    /// factor of **0.53 per iteration** on a four-point manifold, constant over
    /// four decades until the residual reaches the float floor. So the count
    /// you need goes as the logarithm of the accuracy you want.
    ///
    /// **At the scene sizes this course reaches, eight is a quality decision
    /// rather than a performance one.** 8.10 §13 prices one iteration over a
    /// hundred manifolds at 12.9 us, so a 60 Hz frame would afford more than a
    /// thousand. What decides it is 8.10 §14: eight holds a five-crate tower
    /// and does NOT hold a ten-crate one, because a Gauss–Seidel sweep carries
    /// information across one contact and a chain of ten needs about twenty
    /// sweeps. Box2D ships 8, Bullet 10 and PhysX 4, and none of them stacks
    /// ten boxes at its default either.
    int velocity_iterations = 8;

    /// How many passes the position solve makes, when there is one.
    ///
    /// Three, against the velocity solve's eight, because the position problem
    /// is easier in every way: no friction to clip, no restitution, and a
    /// right-hand side that moves by microns between iterations. 8.10 §13
    /// prices one of them at 46% of a velocity iteration, and 8.10 §8 finds no
    /// fixture whose settled depth it changes.
    int position_iterations = 3;

    /// Which position correction to use. Defaults to `split_impulse`; see
    /// `position_correction`, and 8.10 §7 for what the cheaper one costs.
    ///
    /// **Lesson 8.11: joints use it too**, and 8.11 §6 found the choice cuts
    /// both ways there. Baumgarte leaves a limb spawned out of its socket
    /// spinning with 0.35 J it was never given; on a joint that is turning,
    /// it puts back what the velocity projection removes, so a 90-degree
    /// pendulum keeps 92.0% of its energy per period against split impulse's
    /// 75.4%. Split impulse stays the default because a joint that loses
    /// energy reads as damping and a joint that invents it reads as a bug; see
    /// `joint_config` for the whole argument.
    position_correction correction = position_correction::split_impulse;

    /// **The fraction of the excess penetration removed per step**, for both
    /// corrections. Dimensionless, on (0, 1].
    ///
    /// 0.2 is the number every engine ships and 8.10 §7 is why it is not 1.0: at
    /// `beta = 1` the correction tries to remove the whole overlap in one step,
    /// which on a stack means every contact simultaneously asking for the full
    /// separation and the pile popping apart. The geometric decay at 0.2 is a
    /// **74.7 ms** time constant at 60 Hz — four frames to halve an overlap,
    /// which is faster than an eye and slower than a solver.
    float baumgarte = 0.2f;

    /// **Penetration this deep is not corrected at all.** Metres.
    ///
    /// The single most important number in this file for whether a stack looks
    /// settled, and the reason is that a correction driving the overlap to
    /// EXACTLY zero has no rest state: at zero depth the contact separates, the
    /// next frame's gravity puts it back, and the stack breathes at 60 Hz. The
    /// slop gives it somewhere to sit.
    ///
    /// **AND ONE RESTING CONTACT DOES NOT NEED IT**, which 8.10 §8 measured
    /// after setting out to show the opposite: a single crate is bit-for-bit
    /// motionless at a slop of zero, because the correction is geometric and
    /// therefore never reaches zero. What needs the slop is contacts that
    /// COMPETE — a five-crate tower moves **1.80 mm** peak to peak at zero slop
    /// and **0.00** at 2 mm and above.
    ///
    /// It is also a scale decision and the one place this engine's "metres and
    /// seconds" convention bites: 5 mm is invisible under a crate and enormous
    /// under a marble. Scale it with your content, and `time_scale_for_length_scale`
    /// (8.2) is the neighbouring knob that has the same problem.
    float penetration_slop = 0.005f;

    /// A ceiling on the bias velocity, in m/s. Defaults to 3.
    ///
    /// Without it a body spawned inside a wall is told to leave at
    /// `beta*depth/h`, which for a metre of overlap at 60 Hz is **12 m/s** —
    /// and under Baumgarte that is a real velocity it keeps. The clamp turns an
    /// explosion into a push that takes a few frames. Under `split_impulse` it
    /// matters much less, because nothing survives the step; it is kept common
    /// to both so that switching the correction does not change what a badly
    /// placed body does by an order of magnitude.
    float max_correction_speed = 3.0f;

    /// Solve the normal impulse before the friction impulse.
    ///
    /// **This is not a preference.** Coulomb's limit is `mu` times the normal
    /// impulse *of this solve*, so with friction first there is no normal
    /// impulse to clip against and the cone has radius zero (or, with warm
    /// starting, last frame's radius). The field exists so that §10 can measure
    /// what the wrong order does, which is a box that slides a few millimetres
    /// on the first frame of every landing and then grips.
    bool normal_before_friction = true;

    /// The joints' own knobs. Lesson 8.11; see `joint_config`.
    ///
    /// Nested rather than flattened into this struct because they are about a
    /// different kind of constraint, and a reader tuning a stack of crates
    /// should not have to read past a hinge's block-solve switch to find the
    /// slop.
    joint_config joints{};
};

// ---------------------------------------------------------------------------
// A prepared contact
// ---------------------------------------------------------------------------

/// One contact point, with everything that does not change during the solve
/// computed once.
///
/// The split between this and `contact_point` is the split between GEOMETRY and
/// SOLVING. A `contact_point` says where the surfaces touch and how deep; a
/// `contact_constraint` says what an impulse there would do, which depends on
/// both bodies' masses, inertia tensors and lever arms. 8.10 will call the solve
/// many times against one prepare, so anything constant across iterations
/// belongs here — and the orientation does not change during a solve, so the
/// inertia tensors and therefore all three effective masses are constant.
struct contact_constraint
{
    /// Lever arms from each body's centre of mass to the contact point, world
    /// space. `r_a = p - a.state.position`.
    vec3 r_a{};
    vec3 r_b{};

    /// **The impulse applied along the normal so far**, newton-seconds, and
    /// always `>= 0`.
    ///
    /// An accumulator rather than a per-iteration delta, which matters the
    /// moment there is more than one iteration: clamping the TOTAL to be
    /// non-negative is not the same as clamping each increment, and only the
    /// first is correct. 8.10 leans on this; here it makes the single pass read
    /// the same as the many-pass version.
    float normal_impulse = 0.0f;

    /// The accumulated tangential impulses in the batch's tangent basis.
    float tangent_impulse[2] = {0.0f, 0.0f};

    /// `1/k` along the normal, in kilograms. See `effective_mass`.
    float normal_mass = 0.0f;

    /// `1/k` along each tangent. **They differ from each other** whenever the
    /// contact is off-centre, because the lever arm resists one direction more
    /// than the other — which is why there are two numbers here and not one.
    float tangent_mass[2] = {0.0f, 0.0f};

    /// **The relative normal velocity this point should have after solving.**
    ///
    /// Zero for a resting or slow contact, `-e * approach_speed` for a bouncy
    /// one. Folding restitution into a target velocity rather than into a second
    /// equation is what lets non-penetration and bounce share one solve, and it
    /// is the reason `e` never appears in `solve_contacts`.
    float target_normal_velocity = 0.0f;

    /// The relative normal velocity at prepare time, before anything was
    /// applied. Negative when approaching. Instrumentation, and what §5 checks
    /// the post-solve velocity against.
    float initial_normal_velocity = 0.0f;

    /// **How far apart the two surfaces are at this point**, in metres, signed:
    /// negative is overlap. Lesson 8.10.
    ///
    /// `-contact_point::depth`, and the sign flip is not bookkeeping. The
    /// velocity solve's unknown is a *rate* and its target is zero; the
    /// position solve's unknown is a *displacement* and its target is the
    /// separation, so the two solves want the same quantity with opposite
    /// conventions and one of them has to be written down. Separation is the
    /// one every joint in 8.11 will also want, because a joint's error is a
    /// displacement that can be either sign, and a contact is the special case
    /// that only pushes.
    float separation = 0.0f;

    /// **The velocity Baumgarte adds to this point's target**, m/s, `>= 0`.
    ///
    /// `min(beta*max(0, -separation - slop)/h, max_correction_speed)`, computed
    /// once in `prepare_contacts` and zero under the other two corrections. It
    /// is kept apart from `target_normal_velocity` rather than folded into it
    /// so that `solve_report` can tell a real target from a made-up one — and
    /// because the made-up one is the part that adds energy, which is a thing
    /// an instrument should be able to point at.
    float bias = 0.0f;

    /// **The accumulated impulse of the POSITION solve**, newton-seconds,
    /// always `>= 0`. Lesson 8.10, and only under `split_impulse`.
    ///
    /// Its own accumulator, and it must be, for the same reason
    /// `normal_impulse` is one: the position solve is also a sequence of
    /// inequalities and also needs to clamp the total rather than the
    /// increment. It never touches a real velocity — it drives
    /// `pseudo_velocity`, which exists for the length of one step and then
    /// becomes a displacement.
    float pseudo_impulse = 0.0f;

    /// Which point of the manifold this came from, so `write_back` can return
    /// the impulses to it for next frame's warm start.
    int point_index = 0;

    /// Friction reached Coulomb's limit at this point — the surfaces are
    /// SLIDING rather than gripping. Instrumentation, and the thing a character
    /// controller in 8.13 will want to ask.
    bool sliding = false;
};

/// Every prepared contact of one manifold, plus the per-body quantities they
/// share.
///
/// **The per-body fields are a hoist, and it is the hoist that matters.**
/// `world_inv_inertia` is a basis change — two 3x3 products — and a four-point
/// manifold asks for the tensor twelve times if each `effective_mass` call
/// recomputes it. Computing it twice per manifold instead of twenty-four times
/// is §13's measurement and it is not small.
struct contact_batch
{
    contact_constraint points[k_max_clip_points];
    int count = 0;

    /// The manifold's normal, from `a` toward `b`.
    vec3 normal{};

    /// The shared tangent basis. One basis for the whole manifold, not one per
    /// point: every point shares the normal, so every point shares the plane.
    vec3 tangent[2] = {};

    /// The pair material, already combined.
    contact_material material{};

    /// The restitution actually used, after `restitution_threshold` had its say.
    /// Zero on most frames of most contacts, which is the point of the
    /// threshold.
    float restitution_used = 0.0f;

    float inv_mass_a = 0.0f;
    float inv_mass_b = 0.0f;
    mat3 inv_inertia_a{};
    mat3 inv_inertia_b{};
};

/// What a solve did. Instrumentation, in the shape 3.10 fixed and every physics
/// header since has followed.
struct solve_report
{
    /// Total normal impulse applied, newton-seconds. For a body resting under
    /// gravity this converges on `m*g*h` per step, which is a useful sanity
    /// check with a closed form attached.
    float normal_impulse = 0.0f;

    /// Total tangential impulse applied, newton-seconds, as a magnitude.
    float friction_impulse = 0.0f;

    /// The worst `|u_n - target|` over the points after solving, in m/s.
    ///
    /// **On a single point this is zero to float precision and on four points it
    /// is not**, because four impulses applied one after another each disturb
    /// the velocity the previous one just set. That residue is not an error in
    /// this file; it is the reason 8.10 iterates, and §12 measures how it grows
    /// with the point count.
    float max_residual = 0.0f;

    /// How many points ended up pushing (`normal_impulse > 0`). A four-point
    /// manifold on a tilted crate routinely has two.
    int pushing_points = 0;

    /// How many points hit Coulomb's limit and slid.
    int sliding_points = 0;
};

// ---------------------------------------------------------------------------
// The solve
// ---------------------------------------------------------------------------

/// **Compute everything about a manifold that the solve will need.**
///
/// Reads the bodies and does not write them. Everything here is constant for as
/// long as the orientations are, which is for the whole of a solver step.
///
/// Note what decides the restitution: the relative normal velocity NOW, minus
/// `solver_config::restitution_bias`, compared against
/// `solver_config::restitution_threshold`. Both corrections are about the same
/// thing — the velocity a contact "arrived with" is not the velocity it has when
/// the solver looks at it — and §6 measures them separately.
[[nodiscard]] contact_batch prepare_contacts(const rigid_body& a, const rigid_body& b,
                                             const contact_manifold& m,
                                             const contact_material& material,
                                             const solver_config& cfg = {});

/// **Apply the impulses the batch inherited, once.** Warm starting.
///
/// `prepare_contacts` seeds each constraint's accumulators from the manifold
/// when `solver_config::warm_start` is set; this function puts those impulses
/// into the bodies, so that the solve below begins from the velocity the scene
/// would have if last frame's answer were still right. On a settled stack it is.
///
/// **IT IS A ONCE-PER-STEP CALL AND NOT A ONCE-PER-ITERATION ONE**, which is
/// why it is not inside `solve_contacts`. The first draft of this file did put
/// it there, and with sixteen iterations the inherited impulse was applied
/// sixteen times: a crate dropped on a 20-degree ramp slid **810 mm** down a
/// slope it should have gripped. The symptom did not look like "warm starting is
/// applied too often" — it looked like friction had stopped working, because the
/// spurious normal impulses had launched the crate off the surface.
void warm_start_contacts(rigid_body& a, rigid_body& b, contact_batch& batch);

/// **One pass of impulses over a prepared batch.** This is the function.
///
/// For each point, in order: solve the normal, clamping the ACCUMULATED impulse
/// to be non-negative, and apply the difference; then solve both tangents and
/// clip the accumulated tangential impulse to Coulomb's cone at `mu` times that
/// point's accumulated normal impulse.
///
/// **The two loops are separate and the normal loop runs first**, which is
/// `solver_config::normal_before_friction` and is measured in §10.
///
/// Mutates both bodies' `state.velocity` and `angular_velocity` directly rather
/// than through `add_impulse_at`, because the tensors are already in the batch —
/// see `contact_batch`, and §13 for the cost of not doing that. Immovable bodies
/// are unaffected without a branch, because their `inv_mass` and inverse tensor
/// are exact zeros.
solve_report solve_contacts(rigid_body& a, rigid_body& b, contact_batch& batch,
                            const solver_config& cfg = {});

/// Copy the accumulated impulses back into the manifold's points, where
/// `carry_impulses` will find them next frame.
///
/// Separate from `solve_contacts` because 8.10 iterates the solve and writes
/// back once, and because a caller that is not warm starting has no reason to
/// touch the manifold at all.
void write_back(const contact_batch& batch, contact_manifold& m);

/// `prepare_contacts`, `solve_contacts`, `write_back`. The whole of a contact
/// response in one call, for a caller with one pair and no solver loop.
///
/// **Not what 8.10 will call** — a real step prepares every manifold, iterates
/// over all of them several times, and writes back at the end — but it is what
/// the demo and most of §5 through §10 call, and having it makes the one-pair
/// case as short to write as it is to think about.
solve_report resolve_contact(rigid_body& a, rigid_body& b, contact_manifold& m,
                             const contact_material& material, const solver_config& cfg = {});

// ---------------------------------------------------------------------------
// The position solve — Lesson 8.10
// ---------------------------------------------------------------------------

// `pseudo_velocity` — a body's shadow velocity, which moves positions and
// nothing else — was defined here in Lesson 8.10 and now lives in
// `constraint.hpp`, unchanged, because joints need it and joints sit below this
// file's loop rather than inside it. 8.10's comment predicted the move: "8.11's
// joints will want the same array and will get it for free". The split-impulse
// argument is in its doc comment there.

/// **Compute each point's correction bias**, which is the one prepared
/// quantity that depends on how long the step is.
///
///     bias = min(beta * max(0, depth - slop) / h, max_correction_speed)
///
/// A velocity, in m/s, and the same number for both corrections — what differs
/// is *which* velocity it is applied to. Under `baumgarte` it is added to the
/// real target in `solve_contacts`; under `split_impulse` it is the target of
/// `solve_positions` and never touches a real velocity. Under `none` it is
/// zero.
///
/// **It is separate from `prepare_contacts` because `prepare_contacts` does
/// not know `h` and should not.** Everything else it computes — lever arms,
/// effective masses, the restitution target — is a fact about the contact at
/// an instant; this is a fact about the contact *and the schedule*. Separating
/// them keeps 8.9's signature, and names the dependency instead of hiding it
/// in a config struct where a stale value would silently mis-scale every
/// correction in the scene.
void prepare_bias(contact_batch& batch, float h, const solver_config& cfg = {});

/// **One pass of the position solve, in pseudo-velocity space.**
///
/// For each point: measure the *pseudo* relative normal velocity, compare it
/// against the separation target, and apply the impulse that closes the gap —
/// clamping the accumulated `pseudo_impulse` to be non-negative exactly as the
/// velocity solve clamps `normal_impulse`, because a position correction may
/// push and may not pull.
///
/// **It takes the batch and two pseudo velocities and no bodies at all**,
/// which is worth noticing: everything a solve needs — the lever arms, the
/// inverse masses, the world inverse inertia tensors, the effective masses —
/// was hoisted into `contact_batch` by `prepare_contacts`, and the bodies
/// themselves were only ever the place the velocity happened to live. That is
/// the shape 8.11 generalises: a constraint is a Jacobian, an effective mass
/// and a clamp, and the solver never needs to know what it is constraining.
///
/// There is no friction here and no restitution. A position correction that
/// applied friction would be inventing a displacement sideways that no
/// physical process produced, which is the classic "boxes glued to walls" bug.
///
/// Returns the largest remaining penetration over the batch, in metres, so a
/// caller can stop iterating when it is under the slop.
float solve_positions(contact_batch& batch, pseudo_velocity& pa, pseudo_velocity& pb,
                      const solver_config& cfg = {});

/// Move a body by its pseudo velocity and clear it.
///
/// Called by `contact_solver` after the position pass and before
/// `body_world::integrate_positions`, so the two displacements simply add. It
/// advances the orientation by the pseudo angular velocity as well — a
/// correction at a corner is a rotation as much as a translation, and omitting
/// it makes a tilted crate climb out of the floor by sliding rather than by
/// levelling.
void apply_pseudo_velocity(rigid_body& b, pseudo_velocity& p, float h,
                           spin_rule spin = spin_rule::linearised);

// ---------------------------------------------------------------------------
// Islands — Lesson 8.10
// ---------------------------------------------------------------------------

/// **One manifold, with the two bodies it belongs to.** The solver's input.
///
/// A deliberately thin record: the solver needs the bodies' *indices* (to find
/// them in the world's array, and to build the contact graph), a pointer to
/// the manifold (to seed the warm start from and to write the impulses back
/// into), and the pair material.
///
/// `manifold` is a pointer into the caller's `manifold_cache`, which must
/// outlive the step. It is a pointer and not a copy because the write-back is
/// the entire mechanism of warm starting: the impulses this step computes are
/// next step's initial guess, and they get there by living in the cache.
struct contact_pair
{
    std::uint32_t body_a = 0;
    std::uint32_t body_b = 0;
    contact_manifold* manifold = nullptr;
    contact_material material{};
};

/// **One joint, with the two bodies it belongs to.** Lesson 8.11.
///
/// `contact_pair`'s twin, and for the same reasons: indices rather than
/// pointers so the island graph is a set of integers, and a pointer to the
/// caller's `joint` because the write-back into it is the whole mechanism of
/// warm starting. The joint must outlive the step.
struct joint_pair
{
    std::uint32_t body_a = 0;
    std::uint32_t body_b = 0;
    joint* joint_ptr = nullptr;
};

/// **A group of bodies that can only affect one another.**
///
/// Two bodies are in the same island when a chain of contacts joins them.
/// Everything in an island has to be solved together, because an impulse
/// anywhere in it propagates everywhere in it; nothing in one island can
/// influence another this step, by construction, which is what makes islands
/// worth computing. Three things fall out:
///
///   1. **Sleeping becomes decidable.** "Is this body quiet" is not a question
///      you can answer about one body — the crate on top of the pile is quiet
///      right up until the crate at the bottom is nudged. "Is this whole
///      island quiet" is answerable, and it is the right question.
///   2. **The solve is parallelisable.** Two islands share no body, so two
///      threads can solve them with no synchronisation at all. Module 9's job
///      system is where that is collected; the partition is here.
///   3. **Convergence is per-island.** A residual of 1e-6 in a five-crate
///      tower is not improved by iterating over the ten crates on the other
///      side of the level, and an early-out can be taken island by island.
///
/// The fields are ranges into `contact_solver::island_bodies()` and
/// `contact_solver::island_contacts()`, which are permutations of the inputs
/// grouped by island — the same counting-sort-into-contiguous-ranges layout
/// 8.8's grid uses for its cells, for the same reason.
struct island
{
    int first_body = 0;
    int body_count = 0;
    int first_contact = 0;
    int contact_count = 0;

    /// A range into `constraint_solver`'s joint ordering, grouped the same way
    /// as the contacts. Lesson 8.11.
    int first_joint = 0;
    int joint_count = 0;

    /// Every body in it was quiet for long enough. Nothing in a sleeping
    /// island is integrated and none of its contacts are solved.
    bool sleeping = false;
};

/// **Partition bodies into islands over the contact graph.** Union–find.
///
/// Writes `island_of[i]` for every body: the island index, or `-1` for a body
/// that belongs to none. Returns how many islands there are.
///
/// *** A FIXED BODY IS NOT A BRIDGE, AND THIS IS THE ONE RULE THAT MATTERS. ***
/// The floor touches everything. Let a contact through a fixed body join its
/// two neighbours and every object standing on the ground is in one island, so
/// a single rolling marble keeps a level of ten thousand crates awake forever
/// — which is not a slow simulation, it is *no sleeping at all*, in the exact
/// scene sleeping exists for. The rule is that an edge is only an edge when
/// **both** ends can move, and 8.10 §9 measures the difference at **1 island
/// against 20** on a scene of twenty separate towers.
///
/// It is safe because a fixed body has nothing to propagate: its velocity is
/// never written, so an impulse applied to it changes nothing that any other
/// contact can read. A KINEMATIC body is the same — gameplay owns its velocity
/// and the solver cannot change it — so it is not a bridge either, and the lift
/// does not weld its passengers to the floor below it.
///
/// A body with no contacts at all (a projectile in flight) gets its own island
/// of one. That is not a special case in the implementation and it is the right
/// answer: a falling body is quiet nowhere and its island never sleeps.
int build_islands(std::span<const rigid_body> bodies, std::span<const contact_pair> contacts,
                  std::vector<int>& island_of);

/// **The same partition, over contacts AND joints.** Lesson 8.11.
///
/// A joint is an edge of the graph exactly as a contact is, and under exactly
/// the same rule — both ends must be able to move — so a lamp hanging from a
/// fixed ceiling is its own island, and two lamps hanging from the same ceiling
/// are two.
///
/// **LEAVE THE JOINTS OUT AND A CHAIN SLEEPS ONE LINK AT A TIME.** The links of
/// a chain do not touch (their joints stop them), so without joint edges each
/// is its own island of one, each goes quiet on its own schedule, and a
/// sleeping link is an immovable body to the link below it. Strike the bottom
/// of a resting chain and one link swings from a frozen chain above it. 8.11
/// §12 counts the bodies that move.
///
/// The three-argument overload above is this with no joints, and is kept
/// because 8.10's harness and demo call it.
int build_islands(std::span<const rigid_body> bodies, std::span<const contact_pair> contacts,
                  std::span<const joint_pair> joints, std::vector<int>& island_of);

// ---------------------------------------------------------------------------
// Sleeping — Lesson 8.10
// ---------------------------------------------------------------------------

/// When a body counts as quiet, and for how long it has to stay that way.
struct sleep_config
{
    /// Off puts nothing to sleep and wakes nothing. The control for 8.10 §10,
    /// and
    /// the switch to reach for when a physics bug looks like "things stop
    /// reacting" — half of those are a waking rule with a hole in it.
    bool enabled = true;

    /// Metres per second. 0.05 is 5 cm/s, which is 0.8 mm per frame at 60 Hz.
    ///
    /// The knob that trades responsiveness for saving, and it is bounded from
    /// below by the solver: a settled stack does not reach zero velocity, it
    /// reaches whatever residual the iteration count leaves, and a threshold
    /// under that residual never fires. 8.10 §10 sweeps it over a yard of a
    /// hundred crates and the smallest value at which the yard sleeps at all is
    /// **0.02 m/s** — so the default is a factor of 2.5 above the floor rather
    /// than the comfortable margin it looks like. A stiffer scene wants more.
    float linear_threshold = 0.05f;

    /// Radians per second. 0.1 is 5.7 °/s, which is a degree every ten frames.
    ///
    /// Separate from the linear threshold and **not derivable from it**: a
    /// half-metre crate turning at 0.1 rad/s has corner speeds of 3.5 cm/s, so
    /// one number in metres per second cannot express both without knowing the
    /// body's size. Two knobs, because there are two quantities.
    float angular_threshold = 0.10f;

    /// Seconds of continuous quiet before an island sleeps. Half a second.
    ///
    /// The timer is what stops an oscillation being mistaken for rest: a crate
    /// rocking on one edge passes through zero velocity twice a cycle, and an
    /// engine sleeping on the instantaneous test freezes it mid-rock at a
    /// visible angle.
    ///
    /// It is also why `wake` resets it rather than only clearing the flag, and
    /// 8.10 §10 measures the difference in the one case that shows it: a body
    /// woken and then left alone falls asleep again after **30 frames** with
    /// the reset and after **1** without it — a body woken so that gameplay can
    /// act on it next frame, asleep again before gameplay gets there.
    float time_to_sleep = 0.5f;
};

/// **Decide which islands are already asleep, and wake every island that is
/// not entirely so.** Runs BEFORE the solve.
///
/// An island is asleep only when every one of its bodies is flagged asleep;
/// otherwise all of them are woken. That single sentence is the whole waking
/// rule, and it needs no event, no callback and no dirty list: a moving body
/// that collides with a sleeping pile is joined to it by `build_islands`, so
/// the pile's island now contains something awake, so the pile wakes — on the
/// same frame, which is why this runs before the solve rather than after it.
///
/// Returns how many islands are asleep and will therefore be skipped.
int wake_islands(std::span<rigid_body> bodies, std::span<const std::uint32_t> island_bodies,
                 std::span<island> islands);

/// **Advance every body's sleep timer and put whole islands to sleep.** Runs
/// AFTER the solve, and that is not a detail.
///
/// A semi-implicit step applies gravity before the solver looks, so a crate
/// that has rested on a floor for a minute is moving at `g*h` — **16.35 cm/s**
/// at 60 Hz — at every instant before the solve, and at a few millimetres per
/// second after it. A sleep test placed before the solve therefore never fires
/// at any threshold a designer would accept, and the symptom is
/// indistinguishable from thresholds that are merely too tight: 8.10 §10 reads
/// a yard that has been motionless for six seconds and finds **0 of 100**
/// crates quiet before the solve and **96 of 100** after it. It is 8.9's
/// restitution artifact wearing a different hat — the same `g*h`, in the same
/// place, breaking a different thing.
///
///
/// For each island: if every body in it is a sleep candidate *and* allows
/// sleeping, every timer advances by `h`; otherwise every timer is reset to
/// zero. When the smallest timer in the island passes `time_to_sleep`, the
/// whole island sleeps and its bodies' velocities are zeroed.
///
/// **ALL OF AN ISLAND OR NONE OF IT**, and the reason is a picture: sleep the
/// top crate of a tower individually and it becomes, in effect, an immovable
/// body — the solver still sees its contacts, but nothing integrates it. Pull
/// the crate beneath it out and the sleeping one **hangs in the air**, because
/// nothing woke it. 8.10 §10 measures exactly that, with a crate thrown at the
/// bottom of a three-crate tower: under the island rule the top crate falls
/// **0.500 m**, and under a per-body rule it falls **0.000**. The island is
/// the smallest unit for which "nothing here is going to change" is a
/// statement that can be true.
///
/// Zeroing the velocities is not tidiness either. A body frozen at 4 cm/s is a
/// body that will resume at 4 cm/s when something wakes it, half a second
/// later, in a direction that no longer makes sense — and its momentum would
/// keep showing up in `step_report`. Asleep is exactly at rest.
///
/// Returns how many islands are now sleeping.
///
/// It takes the **grouped** body list rather than the per-body labels, because
/// the question it asks is per island and asking it of a scattered array would
/// need a second pass and a scratch accumulator for every island. That is the
/// dividend of the counting sort `contact_solver` already does, and it is the
/// same shape 8.8's grid uses to make a cell's members adjacent.
int update_sleep(std::span<rigid_body> bodies, std::span<const std::uint32_t> island_bodies,
                 std::span<island> islands, float h, const sleep_config& cfg = {});

// ---------------------------------------------------------------------------
// The solver — Lesson 8.10
// ---------------------------------------------------------------------------

/// What one `contact_solver::solve` did. Instrumentation, in 3.10's shape.
struct solver_stats
{
    /// Manifolds handed in, and how many were actually solved — the difference
    /// is the ones in sleeping islands, which is the saving, as a number.
    int manifolds = 0;
    int solved_manifolds = 0;

    /// Contact points across the solved manifolds, and how many of them
    /// inherited an impulse from last frame. On a settled stack the second
    /// should be essentially the first; when it is not, warm starting is doing
    /// nothing and 8.10 §4 says what that costs.
    int points = 0;
    int warm_points = 0;

    int islands = 0;
    int sleeping_islands = 0;
    int sleeping_bodies = 0;

    /// Iterations actually run, which is `velocity_iterations` unless the
    /// early-out fired.
    int velocity_iterations = 0;
    int position_iterations = 0;

    /// The worst `|u_n - target|` over every solved point after the last
    /// velocity iteration, m/s. **The convergence number**, and the one
    /// 8.10 §3 plots against the iteration count.
    float max_residual = 0.0f;

    /// The deepest penetration left after the position pass, metres. Should
    /// settle at the slop and not below it; see `solver_config::penetration_slop`.
    float max_penetration = 0.0f;

    float normal_impulse = 0.0f;
    float friction_impulse = 0.0f;

    /// Joints handed in, and how many were solved. Lesson 8.11.
    int joints = 0;
    int solved_joints = 0;

    /// The worst joint residual after the last velocity iteration — the joint
    /// half of `max_residual`, kept separate because the two converge at
    /// different rates and a single number would report whichever was worse.
    float joint_residual = 0.0f;

    /// The largest joint error at prepare time, metres and radians. What the
    /// position correction is working on; see `joint_error`.
    float joint_linear_error = 0.0f;
    float joint_angular_error = 0.0f;
};

/// **The sequential-impulse solver: every contact and every joint in the world,
/// iterated.**
///
/// Named `contact_solver` in Lesson 8.10, when contacts were all it held; the
/// alias below keeps that name compiling. Lesson 8.11 added joints, and a class
/// that holds a hinge is not a contact solver.
///
/// Four calls per step, in order:
///
/// ```
/// solver.begin(world.bodies());
/// for (auto& [key, m] : cache) { solver.add(a, b, m, material); }
/// for (auto& jp : joints)      { solver.add(jp.a, jp.b, jp.joint); }
/// solver.solve(h, cfg, sleep);
/// // world.integrate_positions(h)
/// ```
///
/// Joints go through every stage contacts do, in the same place: prepared and
/// warm-started once, visited in every velocity sweep (FIRST in each island,
/// then the contacts), visited in every position sweep under split impulse,
/// and written back at the end.
///
/// `solve` does, in this order: build the islands; wake every island that is
/// not entirely asleep; prepare and warm-start every awake manifold once; sweep
/// the velocity solve `velocity_iterations` times; sweep the position solve
/// `position_iterations` times; apply the pseudo velocities; write the impulses
/// back into the manifolds for next step; and only then run the sleep test.
///
/// **THE ORDER OF THOSE STAGES IS ALL LOAD-BEARING** and three of the six
/// orderings are bugs this lesson made and measured:
///
///   * warm starting **once per step, outside the iteration loop** — 8.9's
///     scar, and applying it per iteration slid a crate 810 mm down a slope it
///     should have gripped;
///   * all velocity iterations **before any position iteration**, because the
///     position pass reads separations that the velocity pass is about to make
///     stale, and interleaving them spends corrections on a geometry that is
///     still moving;
///   * **waking before the solve and the sleep TEST after it** — two different
///     things that read like one. Waking has to be early, or an island that has
///     just acquired a moving neighbour is skipped on the frame it most needs
///     solving. The test has to be late, because before the solve every resting
///     body in the scene is travelling at `g*h` and nothing would ever qualify.
///     8.10 §10 measures 0 of 100 against 96 of 100.
///
/// **It keeps its scratch between steps.** `begin` clears the arrays without
/// releasing them, so a scene of a fixed size allocates in its first step and
/// never again — the same contract 8.8's `uniform_grid` makes, for the same
/// reason, and `clear` is how a level change gets the memory back.
class constraint_solver
{
public:
    /// Begin a step over these bodies. The span is retained until `solve`
    /// returns and the bodies must not move in memory in between — which is
    /// exactly the guarantee `body_world::bodies()` makes for the duration of
    /// a step in which nothing is added or removed.
    void begin(std::span<rigid_body> bodies);

    /// Add one manifold. Ignored when it has no points.
    ///
    /// `m` is retained by pointer and written back into at the end of `solve`.
    /// The two indices are indices into the span given to `begin`, which is
    /// what makes the contact graph cheap to build: an island is a set of
    /// integers, not a set of pointers.
    void add(std::uint32_t body_a, std::uint32_t body_b, contact_manifold& m,
             const contact_material& material);

    /// Add one joint between two bodies. Lesson 8.11.
    ///
    /// `j` is retained by pointer and written back into at the end of `solve`,
    /// exactly as a manifold is. A joint between two bodies neither of which
    /// can move is kept out of every island and never solved — a level-design
    /// bug reported by its absence, as for contacts.
    void add(std::uint32_t body_a, std::uint32_t body_b, joint& j);

    /// Solve everything. See the class comment for the stage order.
    const solver_stats& solve(float h, const solver_config& cfg = {},
                              const sleep_config& sleep = {});

    [[nodiscard]] std::span<const island> islands() const;

    /// Body indices, grouped by island. `islands()[i]` names a range of it.
    [[nodiscard]] std::span<const std::uint32_t> island_bodies() const;

    /// Which island each body is in, or `-1`. Indexed by body index.
    [[nodiscard]] std::span<const int> island_of() const;

    [[nodiscard]] const solver_stats& stats() const;

    /// Release the scratch. A new step does not need this; a level change does.
    void clear();

    /// The prepared joints of the last `solve`, in the order they were added.
    /// Instrumentation: a demo draws the anchors from these, and 8.11's
    /// harness reads the residuals.
    [[nodiscard]] std::span<const joint_batch> joint_batches() const;

private:
    std::span<rigid_body> bodies_{};
    std::vector<contact_pair> pairs_;
    std::vector<contact_batch> batches_;
    std::vector<joint_pair> joints_;
    std::vector<joint_batch> joint_batches_;
    std::vector<pseudo_velocity> pseudo_;
    std::vector<int> island_of_;
    std::vector<island> islands_;
    std::vector<std::uint32_t> island_bodies_;
    std::vector<int> island_contacts_;
    std::vector<int> island_joints_;
    solver_stats stats_{};
};

/// **8.10's name for `constraint_solver`**, kept so that code written against
/// it — 8.10's demo and harness among it — compiles unchanged. New code should
/// use the new name; the old one says less than the class does.
using contact_solver = constraint_solver;

// ---------------------------------------------------------------------------
// Closed forms, for checking simulations against
// ---------------------------------------------------------------------------

/// The height of the `n`-th bounce of a ball dropped from `drop_height` with
/// coefficient of restitution `e`: `e^(2n) * h0`.
///
/// Each bounce keeps `e` of the speed, energy goes as the square of speed, and
/// height goes as energy. §6 measures ten of them.
[[nodiscard]] float bounce_height(float drop_height, float restitution, int bounces);

/// **The speed a ball's bounce converges on when restitution is applied to a
/// velocity that already contains this step's gravity**: `e*dv/(1 - e)`.
///
/// The fixed point of `v -> e*(v + dv)`. Solve it and the bounce neither grows
/// nor decays, which is exactly the artifact: a ball that hops forever at a
/// constant height. `dv` is `g*h`. Returns infinity at `e = 1`, correctly —
/// a perfectly elastic ball under this bug gains `g*h` per bounce without limit.
///
/// See `solver_config::restitution_bias`, which is the fix, and §6, which
/// measures the predicted hop against the simulated one.
[[nodiscard]] float terminal_bounce_speed(float restitution, float gravity_step);

/// The steepest slope a body of friction `mu` sits still on, in degrees:
/// `atan(mu)`.
///
/// The entire physical content of `mu`, and the only statement about friction
/// that contains no mass, no area and no speed. §8 bisects the engine's own
/// behaviour against it.
[[nodiscard]] float critical_slope_degrees(float friction);

/// The fraction of its initial speed a sliding body keeps when friction has spun
/// it up into a roll: `1/(1 + c)`, where `c` is the inertia coefficient in
/// `I = c*m*r^2` — 2/5 for a solid sphere, 2/3 for a shell.
///
/// A sphere dropped spinning-free onto a floor at `v0` slides, and friction does
/// two things at once: it slows the centre of mass and it spins the sphere up.
/// When `v = omega*r` the sliding stops and friction has nothing left to act on.
/// For a solid sphere the answer is **5/7**, independent of `mu` and of `g`,
/// which is one of the prettier results in elementary mechanics: friction
/// decides HOW LONG the transition takes and not where it ends up. §9 measures
/// both halves.
[[nodiscard]] float rolling_speed_fraction(float inertia_coefficient);

/// How long that transition takes: `v0 / (mu*g*(1 + 1/c))`.
///
/// The half that does depend on `mu`, and the half that a 60 Hz simulation can
/// get wrong by a step. §9 measures it.
[[nodiscard]] float rolling_time(float initial_speed, float friction, float gravity,
                                 float inertia_coefficient);

/// The stiffness a penalty spring needs to hold `mass` kilograms at `penetration`
/// metres of overlap under gravity `g`: `mass*g/penetration`, in N/m.
///
/// §1's opening argument, as a function so that the table in the lesson can be
/// regenerated rather than transcribed.
[[nodiscard]] float penalty_stiffness(float mass, float gravity, float penetration);

/// The oscillation rate of that spring, in rad/s: `sqrt(g/penetration)`.
///
/// **The mass cancels**, which is the whole problem: you cannot buy stability by
/// making the crate heavier, and the allowed penetration alone decides the rate
/// your simulation has to run at. Pair it with `max_stable_step` from 8.1.
[[nodiscard]] float penalty_omega(float gravity, float penetration);

/// The stiffness a penalty spring needs to stop `mass` arriving at `speed`
/// within `penetration` metres: `mass*speed^2/penetration^2`, in N/m.
///
/// **The number that matters, and the one the static formula above hides.** A
/// spring tuned to hold a resting crate at 1 mm has no say at all in how far a
/// MOVING one sinks: all the kinetic energy has to go into the spring, so the
/// dynamic penetration is `speed*sqrt(mass/k)` and is set by the impact rather
/// than by the weight. §1 measures a spring tuned for 1 mm of static sink
/// letting the same crate 44 mm in at 2 m/s, and computes what stiffness the
/// 1 mm really wanted — along with the rate that stiffness would demand, which
/// is in the hundreds of hertz.
[[nodiscard]] float penalty_impact_stiffness(float mass, float speed, float penetration);

/// **The DEEPEST penetration a contact can arrive with**: `approach_speed * h`
/// metres. Lesson 8.10.
///
/// A ceiling, and not an equality — 8.10 §6 set out to claim the equality and
/// the measurement refused it by up to 81%. `collide_manifold` reports nothing
/// until the shapes actually overlap, so the first frame on which a contact
/// exists is the frame *after* the body crossed the surface; but the body
/// crossed it at some instant INSIDE a step, and how much of that step was left
/// over depends on where it happened to be when the step began. Over 200 finely
/// spaced drop heights the arrival depth is **uniform on [0, v*h]**: mean 0.49
/// of the ceiling, largest 0.97, smallest 0.002.
///
/// The ceiling is what matters, because it is the number no velocity solve can
/// repair and therefore the reason a position correction is separate machinery
/// rather than a bigger iteration count. It is also the bound on how good
/// `position_correction::none` can be, and the reason a slow-moving scene does
/// not need one at all: a crate lowered at 1 cm/s arrives 0.16 mm in.
[[nodiscard]] float arrival_depth(float approach_speed, float h);

/// **How long a Baumgarte correction takes to remove `1 - 1/e` of an
/// overlap**: `-h / ln(1 - beta)` seconds. Lesson 8.10.
///
/// Each step removes the fraction `beta` of whatever excess remains, so the
/// depth decays geometrically as `(1 - beta)^n` — which is an exponential in
/// disguise, with this as its time constant. At `beta = 0.2` and 60 Hz it is
/// **74.7 ms**, which is the answer to "how fast does a stack settle" and is
/// independent of the mass, the depth and the number of contacts.
///
/// Returns infinity at `beta = 0` (nothing is corrected, ever) and zero at
/// `beta = 1` (all of it in one step — see `solver_config::baumgarte` for why
/// that is not the setting you want). 8.10 §7 measures it against the
/// simulation at four values of beta and agrees to within one 16.67 ms step,
/// which is the finest a measurement taken once a frame can be.
[[nodiscard]] float baumgarte_time_constant(float beta, float h);

} // namespace engine::phys
