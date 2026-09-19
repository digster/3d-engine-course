// engine/include/engine/phys/solver.hpp — the first file in phys/ that changes anything.
//
// Lesson 8.9. Five lessons of collision detection have built a machine that
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
// the overlap frozen rather than repaired. §12 measures the residue and it is
// small and permanent. Pushing them apart is a POSITION problem, it introduces
// energy that the velocity solve did not ask for, and doing it without making a
// stack of crates jitter is most of what 8.10 is about.
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
// nothing, which §12 measures rather than assumes. It is 8.10 that makes it
// matter.
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
#include <engine/phys/manifold.hpp>
#include <engine/phys/rigid_body.hpp>

#include <cstdint>

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
// Configuration
// ---------------------------------------------------------------------------

/// Knobs. Every default is something §1 through §13 measured.
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
    ///
    /// `carry_impulses` (8.7) puts last frame's impulse in
    /// `contact_point::normal_impulse` when the ids match; with this set,
    /// `prepare_contacts` seeds the accumulator with it and applies it up front.
    /// On the single pass this lesson makes it is close to a no-op, which §12
    /// measures. It is 8.10's whole convergence story.
    bool warm_start = false;

    /// Solve the normal impulse before the friction impulse.
    ///
    /// **This is not a preference.** Coulomb's limit is `mu` times the normal
    /// impulse *of this solve*, so with friction first there is no normal
    /// impulse to clip against and the cone has radius zero (or, with warm
    /// starting, last frame's radius). The field exists so that §10 can measure
    /// what the wrong order does, which is a box that slides a few millimetres
    /// on the first frame of every landing and then grips.
    bool normal_before_friction = true;
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

} // namespace engine::phys
