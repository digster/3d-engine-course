// engine/include/engine/phys/constraint.hpp — a contact is a constraint that
// only pushes.
//
// Lesson 8.11. Everything 8.9 and 8.10 built answers one kind of question: two
// surfaces are touching, so what impulse stops them approaching? The answer took
// three lines — measure a relative velocity along a direction, divide by an
// effective mass, clamp the accumulated result at zero — and 8.10 wrapped those
// three lines in a loop that holds a stack up.
//
// This file writes the same three lines ONCE, for any question of that shape,
// and then asks four new questions with them. A door that must turn about its
// hinge and nowhere else. A lamp that hangs from a ceiling by a point. A rope
// that may go slack and may not stretch. A wheel that a motor drives. Not one of
// them is a new algorithm; each is a different choice of the one thing 8.9
// never had to name, because a contact only ever needed one of it:
//
// ---- THE JACOBIAN ------------------------------------------------------------
//
// A constraint is a scalar function of the two bodies' positions, `C`, that the
// solver keeps at zero (an EQUALITY — a hinge pin) or at or above zero (an
// INEQUALITY — a contact, a rope, a limit). Positions change only because
// velocities are non-zero, and a small change in either body's position or
// orientation changes `C` by an amount LINEAR in the velocities that caused it.
// So there is a row of twelve numbers, `J`, with
//
//     dC/dt  =  J · V,     V = (v_a, w_a, v_b, w_b)      (twelve numbers)
//
// and that row is the Jacobian. Three facts about it are the whole of 8.11:
//
//   1. **The impulse is Jᵀλ.** A constraint does no work on any motion it
//      allows — a hinge pin does not slow a door that turns about the pin — and
//      the only impulses that do no work on every V with `J·V = 0` are
//      multiples of Jᵀ. So an entire constraint's response is ONE scalar, λ,
//      in the direction the row already names. 8.11 §2 derives it.
//   2. **The effective mass is 1/(J M⁻¹ Jᵀ).** Apply `Jᵀλ` and `J·V` changes by
//      `(J M⁻¹ Jᵀ) λ`. That is 8.9's `effective_mass` exactly — the 8.9 header
//      calls it "the reason it is a scalar is the whole trick", and the trick
//      is that `J M⁻¹ Jᵀ` is a 1x1 matrix. 8.11 §3 checks the two against each
//      other on real manifolds.
//   3. **What a constraint is ALLOWED to do is a pair of bounds** on the
//      accumulated λ. A contact is `[0, ∞)`. A rope is the same row turned
//      round. A hinge pin is `(−∞, ∞)`. A motor is `[−τh, τh]`, and friction —
//      it turns out — is a motor whose target is zero and whose τ is μ times
//      a normal impulse.
//
// ---- WHAT 8.10 WAS AIMING AT --------------------------------------------------
//
// 8.10's `solve_positions` took no bodies: a batch and two pseudo velocities,
// because everything a solve needs had been hoisted into the batch. That was
// this file, arriving early. Every function below takes velocities and a
// prepared row, and knows nothing about what the row is constraining.
//
// ---- ONE NUMBER, THREE TIMES ---------------------------------------------------
//
// The quantity this lesson kept meeting is the PARALLEL-AXIS RATIO of a body on
// a pin, `ρ = m·d² / (I_cm + m·d²)` — the fraction of the body's rotational
// inertia about the pin that lives in the lever arm rather than in its own
// spin. `lever_ratio` computes it. It sets:
//
//   * **the period** of a swinging body, through `I_pivot = I_cm + m·d²` —
//     8.11 §8 measures a box at 1.645967 s against 1.646241 predicted, and a
//     point-mass formula 16% short;
//   * **the energy a velocity joint loses per step**, `ρ·(ω·h)²` — the
//     projection removes only the centre's ORBIT about the pin, which is the
//     fraction ρ of the kinetic energy. §5 measures it to 0.1% on six
//     configurations;
//   * **how fast an angular row converges against the pin** — a hinge limit or
//     motor, whose row sees only `I_cm`, contracts at exactly ρ per sweep
//     (§10: 0.4261, 0.7481 and 0.9224, measured to four figures). A door,
//     uniform and hinged at its edge, is 3/4 whatever its size.
//
// ---- AND WHAT THIS FILE DELIBERATELY DOES NOT DO -----------------------------
//
// **It does not rewrite the contact solver.** A contact normal IS a row — §3
// proves it to 1.9e-07 — and `solve_contacts` stays hand-written anyway, though
// §3 found the row form FASTER, not slower: 8.75 ns a point per sweep against
// 11.95, because a row stores `M⁻¹Jᵀ` and 8.9 recomputes it with two cross
// products and two matrix products on every visit. That was a hoist 8.9 never
// made, and it is worth making — but it changes, at the rounding level, every
// number 8.9 and 8.10 printed, so it is named in §18 rather than done here.
//
// **It does not solve chains exactly.** A chain is a system Gauss–Seidel
// crosses one joint per sweep, precisely as 8.10's tower was, and a heavy weight
// on a light chain makes it much worse: ten links under a 100:1 end weight
// stretch 388 mm at eight sweeps. §14 measures that, and measures the better
// answer — SUB-STEPPING, eight steps of one sweep instead of one step of eight,
// which stretches it 20 mm for the same work — and names it for Module 9.

#pragma once

#include <engine/math/mat3.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/rigid_body.hpp>

#include <cstdint>
#include <limits>
#include <vector>

namespace engine::phys
{

// ---------------------------------------------------------------------------
// Solver scratch shared by every constraint
// ---------------------------------------------------------------------------

/// **A body's shadow velocity: it moves positions and nothing else.**
///
/// Moved here from `solver.hpp` in Lesson 8.11, unchanged, because joints need
/// it and joints sit below the solver loop rather than inside it. 8.10's doc
/// comment still holds word for word: the position pass solves exactly the same
/// linear system as the velocity pass, against *this* instead of
/// `rigid_body::state.velocity`, and at the end of the step
/// `apply_pseudo_velocity` moves the body by it and throws it away. Nothing
/// here survives into the next step, which is why split impulse adds no energy
/// and Baumgarte does — 8.10 §7 measured **1.14006 m/s** of departure against
/// **0.00000**. 8.11 §6 finds the same asymmetry on a dislocated joint (0.35 J
/// of spin left in a limb against exactly zero), and then finds it cutting the
/// other way on a joint that is turning.
///
/// 8.10 predicted this move in the same comment ("8.11's joints will want the
/// same array and will get it for free"). They do.
struct pseudo_velocity
{
    vec3 linear{};
    vec3 angular{};
};

// ---------------------------------------------------------------------------
// One row
// ---------------------------------------------------------------------------

/// A bound on an accumulated impulse that means "none".
inline constexpr float k_unbounded = std::numeric_limits<float>::infinity();

/// **One scalar constraint, prepared: a Jacobian row, its effective mass, and
/// what it is allowed to do.**
///
/// The general form of 8.9's `contact_constraint`, and a more expensive one in
/// memory: twelve Jacobian entries, six more for `M⁻¹Jᵀ`'s angular halves, and
/// seven scalars — **108 bytes** against a contact point's 76. It buys that
/// back in time: 8.11 §3 measures a normal solve at **8.75 ns** a point per
/// sweep as a row against **11.95** hand-written, because the row applies its
/// impulse with four scaled adds where 8.9 does two cross products and two
/// matrix products.
///
/// **The four halves are stored separately rather than as a `float[12]`**
/// because every one of them is a `vec3` quantity with a meaning: the linear
/// halves are directions (a contact normal, a rope's line), the angular halves
/// are `r × direction` for a point row or a bare axis for an angular one, and
/// every operation on a row is four `dot`s or four scaled adds — never a
/// twelve-long loop that would hide which half is which.
struct jacobian_row
{
    /// `J`, in the four blocks that multiply `v_a`, `w_a`, `v_b` and `w_b`.
    vec3 linear_a{};
    vec3 angular_a{};
    vec3 linear_b{};
    vec3 angular_b{};

    /// `I⁻¹` times each angular half: the angular half of `M⁻¹Jᵀ`, so that
    /// applying `λ` is a scaled add rather than a matrix product per visit.
    /// The linear halves need no such field — `M⁻¹` there is one scalar, the
    /// inverse mass, and the batch already holds it.
    vec3 inv_inertia_angular_a{};
    vec3 inv_inertia_angular_b{};

    /// `1 / (J M⁻¹ Jᵀ)`, kilograms for a linear row and kg·m² for an angular
    /// one. Zero when neither body can respond, for 8.9's reason: no impulse
    /// changes the relative velocity of two immovable bodies, and `0 × anything`
    /// says so without a branch.
    float mass = 0.0f;

    /// **The value `J·V` should have after solving**, in m/s or rad/s.
    ///
    /// Zero for a pin. A motor's speed for a motor. And `−C/h` for an
    /// inequality whose `C` is still positive — a limit the door has not yet
    /// reached, a rope that is still slack: "you may close this gap, but not
    /// faster than it would take one step to close it". That is a SPECULATIVE
    /// constraint, and 8.11 §10 is why limits get it for free and contacts do
    /// not.
    float target = 0.0f;

    /// **The position-correction velocity**, `−β·C/h`, clamped. Zero unless
    /// the constraint is already violated. Kept apart from `target` for 8.10's
    /// reason: the velocity pass adds it only under Baumgarte, and the
    /// position pass uses it only under split impulse, and an instrument should
    /// be able to tell a real target from a made-up one.
    float bias = 0.0f;

    /// `C` itself, in metres or radians, at prepare time. Instrumentation, and
    /// what §5 reads the drift off.
    float error = 0.0f;

    /// **Bounds on the ACCUMULATED impulse**, newton-seconds (or N·m·s).
    /// `[0, ∞)` for anything that may only push; `(−∞, ∞)` for a pin;
    /// `[−τh, τh]` for a motor. The whole difference between the kinds of
    /// constraint in this file lives in these two numbers.
    float lower = -k_unbounded;
    float upper = k_unbounded;

    /// The accumulated impulse of the velocity pass. Clamp THIS, never the
    /// increment — 8.9's rule, and 8.10 §2 is why it is not pedantry.
    float impulse = 0.0f;

    /// The accumulated impulse of the position pass, under split impulse.
    float pseudo_impulse = 0.0f;

    /// Which field of the `joint` this row's impulse is warm-started from and
    /// written back to. See `row_role`.
    std::uint8_t role = 0;

    /// Which component, for the roles that have several (a point row's axis, a
    /// perpendicular row's index).
    std::uint8_t component = 0;
};

/// **The row that constrains two material points along one direction.**
///
///     J = [ −d,  −(r_a × d),  d,  r_b × d ]
///
/// `J·V` is then `d · ((v_b + w_b × r_b) − (v_a + w_a × r_a))`, which is 8.9's
/// relative contact velocity along `d` — because `(r × d)·w = d·(w × r)` by the
/// scalar triple product, the same rearrangement 8.9 §4 used on the effective
/// mass. With `d` the contact normal this IS a contact normal row; with `d`
/// along a rope it is the rope; with `d` each of x, y and z in turn it is a
/// ball-socket written as three rows. 8.11 §3.
///
/// The bounds, target and bias are left at their defaults; call `prepare_row`
/// to fill in the mass.
[[nodiscard]] jacobian_row point_row(vec3 direction, vec3 r_a, vec3 r_b);

/// **The row that constrains the relative angular velocity about one axis.**
///
///     J = [ 0,  −axis,  0,  axis ]
///
/// `J·V = axis · (w_b − w_a)`. The hinge's two perpendicular rows, its limit
/// and its motor are all this, with different axes and different bounds.
[[nodiscard]] jacobian_row angular_row(vec3 axis);

/// **Fill in `M⁻¹Jᵀ`'s angular halves and the effective mass.**
///
///     J M⁻¹ Jᵀ = m_a⁻¹ |lin_a|² + ang_a · I_a⁻¹ ang_a
///              + m_b⁻¹ |lin_b|² + ang_b · I_b⁻¹ ang_b
///
/// Four non-negative terms for a positive semi-definite `I⁻¹`, which is 8.9's
/// proof that the division is safe, now true of every row rather than only of
/// contacts.
void prepare_row(jacobian_row& row, float inv_mass_a, const mat3& inv_inertia_a,
                 float inv_mass_b, const mat3& inv_inertia_b);

/// `J · V`. Four dot products.
[[nodiscard]] float row_velocity(const jacobian_row& row, vec3 v_a, vec3 w_a, vec3 v_b, vec3 w_b);

/// **Solve one row against four velocities, in place.** The general form of
/// 8.9's three lines.
///
///     λ      = mass · (target + extra − J·V)
///     total  = clamp(impulse + λ, lower, upper)
///     apply    (total − impulse) along M⁻¹Jᵀ
///
/// `extra` is added to the target: the solver passes `row.bias` under
/// Baumgarte and zero otherwise, which is the whole of the difference between
/// the two corrections on the velocity side.
///
/// Returns the impulse actually applied this call, which may be negative when
/// an accumulated impulse is being unwound.
///
/// **It takes four `vec3&` and no bodies**, deliberately: the velocity pass
/// hands it a body's real velocities and the position pass hands it the pseudo
/// ones, and a row cannot tell which it has — 8.10's `solve_positions`, finished.
float solve_row(jacobian_row& row, float inv_mass_a, float inv_mass_b, vec3& v_a, vec3& w_a,
                vec3& v_b, vec3& w_b, float extra = 0.0f);

/// The same, against the position pass's accumulator and pseudo velocities,
/// with `row.bias` as the target. Separate from `solve_row` only because it
/// reads and writes a different accumulator.
float solve_row_position(jacobian_row& row, float inv_mass_a, float inv_mass_b, pseudo_velocity& a,
                         pseudo_velocity& b);

// ---------------------------------------------------------------------------
// Joints
// ---------------------------------------------------------------------------

/// What a joint allows.
enum class joint_kind : std::uint8_t
{
    /// **A rod or a rope**: the distance between two anchor points stays within
    /// `[min_length, max_length]`.
    ///
    /// ONE row along the line between the anchors, and the same `point_row` a
    /// contact normal is. With the two lengths equal it is a rigid rod — a
    /// bilateral row, `(−∞, ∞)`. With `min_length` zero it is a ROPE — a row
    /// that may only pull — which is a contact turned inside out: a contact
    /// forbids the separation going below zero, a rope forbids the distance
    /// going above its length. 8.11 §4 swings a bob up past the horizontal and
    /// measures the rope letting go at exactly the height a rod's row changes
    /// sign from tension to compression — two instruments, one fact.
    distance,

    /// **A ball-and-socket**: two anchor points, one on each body, held
    /// together. Three translational degrees of freedom removed, all three
    /// rotational ones kept — a shoulder, a lamp on a chain link, a trailer
    /// hitch.
    ///
    /// Three rows, one per world axis, each a `point_row`. Solved by default
    /// as ONE 3x3 block rather than three rows, because the three are coupled
    /// through the lever arms and have no clamp. 8.11 §7: as rows, one visit
    /// leaves up to 92% of the violation and they contract at the spectral
    /// radius of `K`'s Gauss–Seidel matrix (0.2323 measured, 0.2325 computed);
    /// as a block, one visit leaves 5.9e-07, and costs 10.8 ns against 23.1.
    /// Through the centre of mass, `r = 0`, the rows do not couple and the two
    /// are identical.
    ball_socket,

    /// **A hinge**: a ball-socket plus two angular rows that keep the bodies'
    /// hinge axes aligned. Five degrees of freedom removed; one rotation kept.
    ///
    /// Optionally a LIMIT on the hinge angle (a one-sided angular row at each
    /// end, which is a contact on an angle) and a MOTOR (an angular row with a
    /// target speed and a symmetric clamp on the torque it may use).
    hinge,
};

/// `"distance"`, `"ball-socket"`, `"hinge"`.
[[nodiscard]] const char* name_of(joint_kind kind);

/// An angular range a hinge may turn through, radians, measured from the pose
/// the joint was created in.
struct joint_limit
{
    bool enabled = false;
    float lower = 0.0f;   ///< Radians, `<= 0` for a range that includes the authored pose.
    float upper = 0.0f;   ///< Radians, `>= lower`.
};

/// A hinge motor: drive the relative angular speed about the axis toward
/// `speed`, using at most `max_torque`.
///
/// **A motor is a row with a non-zero target and a symmetric clamp**, and the
/// clamp is what makes it a motor rather than a hinge welded to a spinning
/// frame: `max_torque × h` is the largest impulse it may apply in a step, so a
/// load heavier than it can lift simply stalls — 8.11 §11 bisects the stall
/// torque of an arm at 9.8100 N·m against `m·g·d` = 9.8100 — and with a target
/// of ZERO the row is Coulomb friction in the hinge, which is what 8.9's
/// friction rows were all along.
struct joint_motor
{
    bool enabled = false;
    float speed = 0.0f;        ///< rad/s, of `b` relative to `a`, about the axis.
    float max_torque = 0.0f;   ///< N·m. Zero is a motor that can do nothing.
};

/// **A joint: what two bodies may not do to each other, and what it cost last
/// frame to stop them.**
///
/// Authored in BODY-LOCAL terms — anchors and axes are points and directions
/// fixed in each body — because that is what a joint is: the hinge pin is
/// screwed to the door, not to a place in the world. The `make_*` functions
/// below take a world-space description of the pose you want and convert it.
///
/// The second half of the struct is the joint's equivalent of a manifold's
/// cached impulses, and it has a property the manifold's did not: **every one
/// of them is stored in a frame that cannot rotate out from under it.** 8.10 §4
/// found the friction impulses cached as coordinates in a basis that was
/// rebuilt every frame; the fix was to store the basis. Here the linear and
/// perpendicular-angular impulses are stored as WORLD vectors, which carry
/// their own frame, and the three scalars are along the hinge axis, which is
/// fixed in body `a`. §9 counts how often the obvious alternative would have
/// flipped: **127 of 7,787** hinge-steps on a sagging bridge, by exactly 90
/// degrees, against none.
///
/// The joint does not know which bodies it joins. The caller hands the two
/// indices to `constraint_solver::add` every step, exactly as it does for a
/// manifold — and for the same reason: `body_world::bodies()` is dense and its
/// order is not stable, so an index is only meaningful for one step.
struct joint
{
    joint_kind kind = joint_kind::ball_socket;

    /// The anchor point, in each body's own frame. A ball-socket or hinge
    /// holds these two points together; a distance joint holds them apart.
    vec3 anchor_a{};
    vec3 anchor_b{};

    /// The hinge axis, in each body's own frame, unit length. Unused by the
    /// other two kinds.
    vec3 axis_a{0.0f, 0.0f, 1.0f};
    vec3 axis_b{0.0f, 0.0f, 1.0f};

    /// **The relative orientation at which the hinge angle is zero**:
    /// `conjugate(q_a) * q_b` at the authored pose. The limit and the angle
    /// `hinge_angle` reports are both measured from here, so a door authored
    /// half open reads zero half open.
    quat rest{};

    /// A distance joint's range, metres. Equal for a rod; `min_length = 0` for
    /// a rope. Unused by the other two kinds.
    float min_length = 0.0f;
    float max_length = 0.0f;

    joint_limit limit{};
    joint_motor motor{};

    /// Should the two bodies this joint connects also collide with each other?
    /// **Almost never**, which is why the default is false: two links of a
    /// chain overlap at the joint by construction, and a contact solver that
    /// sees that overlap spends every step pushing apart two bodies the joint
    /// is pulling together. The engine does not filter pairs itself — the
    /// caller runs the narrow phase — so `collision_filter` below is the tool.
    bool collide_connected = false;

    // ---- warm-start state: what holding the joint cost last step ----------

    /// The point block's accumulated impulse, WORLD space, newton-seconds.
    vec3 point_impulse{};

    /// The hinge's perpendicular angular impulse, WORLD space, N·m·s. Always
    /// perpendicular to the axis, because it is a combination of two
    /// perpendicular rows.
    vec3 angular_impulse{};

    /// The two one-sided rows: a hinge's lower and upper angle limits, or a
    /// distance joint's short and long ends. A rod's single bilateral row uses
    /// `[0]`.
    float limit_impulse[2] = {0.0f, 0.0f};

    /// The motor row's accumulated impulse, N·m·s.
    float motor_impulse = 0.0f;
};

/// A ball-socket holding `a` and `b` together at `world_anchor`, as they stand.
[[nodiscard]] joint make_ball_socket(const rigid_body& a, const rigid_body& b, vec3 world_anchor);

/// A hinge at `world_anchor` about `world_axis`, as the bodies stand. The
/// hinge angle reads zero in this pose. `world_axis` need not be unit length.
[[nodiscard]] joint make_hinge(const rigid_body& a, const rigid_body& b, vec3 world_anchor,
                               vec3 world_axis);

/// A rigid rod between `anchor_a` on `a` and `anchor_b` on `b`, both given in
/// world space, at the length they are apart now.
[[nodiscard]] joint make_rod(const rigid_body& a, const rigid_body& b, vec3 anchor_a, vec3 anchor_b);

/// A rope between the two world-space anchors that may not exceed
/// `max_length`, and may be as short as it likes.
[[nodiscard]] joint make_rope(const rigid_body& a, const rigid_body& b, vec3 anchor_a,
                              vec3 anchor_b, float max_length);

/// **The hinge angle, radians, in (−π, π]**: how far `b` has turned about the
/// hinge axis, relative to `a`, since the pose the joint was authored in.
///
/// The TWIST part of a swing–twist split of the relative rotation — 7.4's
/// quaternion read in the one way that ignores how far the axes have drifted
/// apart:
///
///     q = conjugate(q_a) · q_b · conjugate(rest)          (a's frame)
///     θ = 2 · atan2(q.v · axis_a, q.w)
///
/// with `q` first moved to the hemisphere `w >= 0`, because `q` and `−q` are
/// the same rotation (7.4's double cover) and only one of them gives an angle
/// in (−π, π]. Its rate of change is `axis · (w_b − w_a)`, which is exactly
/// the limit row's `J·V` — the angle and the row agree by construction.
[[nodiscard]] float hinge_angle(const joint& j, const rigid_body& a, const rigid_body& b);

/// How far a joint is from satisfied, right now. Instrumentation.
struct joint_error
{
    /// Metres. The anchor gap for a ball-socket or hinge; for a distance
    /// joint, how far outside `[min_length, max_length]` the anchors are.
    float linear = 0.0f;

    /// Radians. The angle between the two hinge axes, plus any violation of
    /// an enabled limit. Zero for the other kinds.
    float angular = 0.0f;
};

/// Measure a joint against the bodies as they stand.
[[nodiscard]] joint_error measure_joint(const joint& j, const rigid_body& a, const rigid_body& b);

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/// Joint-specific knobs. Every default is something 8.11 measured.
///
/// The position correction a joint uses is the SOLVER's — `solver_config::
/// correction` — and that is a decision 8.11 §6 made with the evidence in both
/// directions. Split impulse adds no energy to anything, and costs every
/// turning joint `projection_loss_per_step`: a 90-degree pendulum keeps 75.4%
/// of its energy per period at 60 Hz. Baumgarte keeps 92.0% of it, and leaves
/// 0.35 J of spin in a limb spawned 0.2 m out of its socket. A game survives a
/// joint that loses energy — it reads as damping — and does not survive one
/// that makes still things move, so the default stays split impulse, and
/// letting joints choose separately is §18's exercise.
struct joint_config
{
    /// **Solve a joint's coupled equality rows together rather than one at a
    /// time.** The point constraint as one 3x3 block, the hinge's two
    /// perpendicular rows as one 2x2 block.
    ///
    /// The three point rows are coupled through the lever arms — an impulse
    /// along x at an anchor off the centre of mass also spins the body, and the
    /// spin moves the anchor in y and z — so solving them one at a time is
    /// Gauss–Seidel on a 3x3 system and leaves a residual. Solving the block is
    /// exact in one visit. And it is *affordable* here in a way it is not for
    /// contacts: a joint's rows have no clamp, so the block is a linear solve,
    /// where a block of contact rows is a small LCP (Box2D solves the 2-point
    /// case by enumerating its four cases, and nobody ships the 4-point one).
    /// 8.11 §7 finds it better on every axis — exact, and cheaper — and finds
    /// it barely helps a CHAIN (worst gap 94 mm against 102), because the
    /// chain's problem is between joints, not within one.
    bool block_solve = true;

    /// **Solve a limit before it is reached.** A limit row whose `C` is still
    /// positive is added anyway, with a target of `−C/h`: the door may approach
    /// its stop, but not faster than it would take one step to arrive.
    ///
    /// Off, the row only exists once the limit is violated, which is exactly
    /// 8.10's arrival-depth problem moved to an angle: a turnstile overshoots
    /// its stop by a fraction of `ω·h` that is uniform on [0, 1] — mean
    /// 0.4985 over 200 phases in 8.11 §10. On, it overshoots by zero. It costs
    /// nothing, and contacts cannot have it for free, because a contact that
    /// does not exist yet has not been found by the narrow phase, while a
    /// hinge always knows its angle.
    ///
    /// **On a DOOR it is not zero**, and the reason is `lever_ratio`: the limit
    /// row converges against the pin at ρ per sweep, 0.748 for a door, so
    /// eight sweeps leave 9.8% of the stop undone and a door with no
    /// restitution bounces off its stop at 7.5% of its arrival speed.
    bool speculative_limits = true;

    /// Seed each row's accumulator from the joint's stored impulses, and apply
    /// them once before iterating. Joints' own switch, separate from
    /// `solver_config::warm_start`, so that §7 can measure one without the
    /// other.
    bool warm_start = true;

    /// The fraction of a joint's error removed per step, dimensionless. The
    /// same 0.2 as contacts. 8.11 §6 derives the stretch it leaves on a
    /// turning joint in closed form — each step removes β of the error and the
    /// motion adds a drift δ, so it settles at `δ/β` — and measures 12.50 mm
    /// against 12.22 predicted on a rod spinning at 5 m/s.
    float baumgarte = 0.2f;

    /// A ceiling on the correction velocity, m/s (or rad/s for an angular
    /// row). 8.10's `max_correction_speed`, for 8.10's reason.
    float max_correction_speed = 3.0f;
};

// ---------------------------------------------------------------------------
// A prepared joint
// ---------------------------------------------------------------------------

/// The most rows one joint can produce: a hinge's three point rows, two
/// perpendicular rows, two limit rows and a motor.
inline constexpr int k_max_joint_rows = 8;

/// What each prepared row feeds back into, for warm starting.
enum class row_role : std::uint8_t
{
    point,           ///< One of the three point rows; `component` is the axis.
    perpendicular,   ///< One of the hinge's two alignment rows; `component` 0 or 1.
    limit_lower,     ///< The lower end of a limit or a length range; a rod's row.
    limit_upper,     ///< The upper end.
    motor,           ///< The motor.
};

/// One joint, with everything the solve needs computed once.
///
/// The shape of 8.9's `contact_batch`, for the same reason: the orientations do
/// not change during a solve, so the world inverse inertia tensors, the lever
/// arms, the world axes and every effective mass are constant across
/// iterations and are hoisted here.
struct joint_batch
{
    joint_kind kind = joint_kind::ball_socket;

    float inv_mass_a = 0.0f;
    float inv_mass_b = 0.0f;
    mat3 inv_inertia_a{};
    mat3 inv_inertia_b{};

    /// Lever arms from each centre of mass to its anchor, world space.
    vec3 r_a{};
    vec3 r_b{};

    /// The hinge axis in world space, as body `a` carries it.
    vec3 axis{};

    // ---- the point block (ball-socket, hinge) -------------------------------

    /// Solve the point constraint as one 3x3 block. False when the joint has
    /// no point constraint, or when `joint_config::block_solve` is off and the
    /// three rows are in `rows` instead.
    bool point_block = false;

    /// `(J M⁻¹ Jᵀ)⁻¹` for the three point rows together: the 3x3 generalisation
    /// of the effective mass, and a genuine matrix — its off-diagonal entries
    /// are the coupling a row-by-row solve has to iterate away.
    mat3 point_mass{};

    /// `C = p_b − p_a`, world space, metres.
    vec3 point_error{};

    /// `−β·C/h`, clamped in length. The block's bias.
    vec3 point_bias{};

    /// Accumulated impulses, velocity and position passes, world space.
    vec3 point_impulse{};
    vec3 point_pseudo_impulse{};

    // ---- the perpendicular angular block (hinge) ------------------------------

    bool angular_block = false;

    /// Two world directions perpendicular to `axis`, chosen in body `a`'s own
    /// frame so that they rotate with it rather than being re-chosen from a
    /// world vector every step. See `joint`.
    vec3 perp[2] = {};

    /// `I⁻¹` times each perpendicular, for both bodies — the angular halves of
    /// `M⁻¹Jᵀ` for the two rows.
    vec3 inv_inertia_perp_a[2] = {};
    vec3 inv_inertia_perp_b[2] = {};

    /// The inverse of the 2x2 `J M⁻¹ Jᵀ`, symmetric, stored `{m00, m01, m11}`.
    float angular_mass[3] = {0.0f, 0.0f, 0.0f};

    /// `C` for the two rows: the misalignment `axis_a × axis_b`, resolved
    /// along each perpendicular. Radians, for small errors.
    float angular_error[2] = {0.0f, 0.0f};
    float angular_bias[2] = {0.0f, 0.0f};
    float angular_impulse[2] = {0.0f, 0.0f};
    float angular_pseudo_impulse[2] = {0.0f, 0.0f};

    // ---- every other row ------------------------------------------------------

    /// Motor, limits and a distance joint's row(s) — and, when
    /// `block_solve` is off, the three point and two perpendicular rows as
    /// well. Solved in this order, which puts the rows that may give way (a
    /// motor out of torque, a limit that may let go) BEFORE the ones that may
    /// not, so that the pin is the last thing satisfied in every sweep.
    jacobian_row rows[k_max_joint_rows];
    int row_count = 0;

    /// The hinge angle at prepare time, radians. Instrumentation.
    float angle = 0.0f;
};

/// What one visit to one joint did. Instrumentation, in 3.10's shape.
struct joint_report
{
    /// The worst `|J·V − target|` over the equality rows (and the blocks,
    /// component by component) after this visit, m/s or rad/s.
    ///
    /// **Zero to float precision for a single joint solved as a block** (8.11
    /// §7: 5.9e-07 of the violation, worst of a thousand), and not zero for
    /// the same joint solved as rows (up to 0.92 of it) — that difference is
    /// the whole of §7.
    float max_residual = 0.0f;

    /// Total magnitude of impulse applied this visit.
    float impulse = 0.0f;
};

/// **Compute everything about a joint the solve will need.**
///
/// Reads the bodies and does not write them. Takes `h` where
/// `prepare_contacts` did not, because two things here depend on the step:
/// the bias `−β·C/h`, and a speculative limit's target `−C/h` — "arrive, but
/// not faster than this step allows" has a step in it.
[[nodiscard]] joint_batch prepare_joint(const rigid_body& a, const rigid_body& b, const joint& j,
                                        float h, const joint_config& cfg = {});

/// **Apply the impulses the batch inherited, once per step.** The joint
/// version of `warm_start_contacts`, and for the same reason not inside
/// `solve_joint`: 8.9 applied a contact's warm start once per ITERATION in its
/// first draft and slid a crate 810 mm down a slope it should have gripped.
void warm_start_joint(rigid_body& a, rigid_body& b, const joint_batch& batch);

/// **One visit to one joint: every row, in order, then the blocks.**
///
/// `use_bias` adds each row's position-correction bias to its target, which is
/// what `position_correction::baumgarte` means; the solver passes false under
/// the other two corrections.
joint_report solve_joint(rigid_body& a, rigid_body& b, joint_batch& batch, bool use_bias);

/// One visit of the position pass, in pseudo-velocity space, under split
/// impulse. No motor and no speculative target — a position correction only
/// repairs error that already exists.
///
/// Returns the largest remaining error in the batch, metres.
float solve_joint_positions(joint_batch& batch, pseudo_velocity& pa, pseudo_velocity& pb);

/// Copy the accumulated impulses back into the joint for next step.
///
/// An overload of 8.9's `write_back(contact_batch, contact_manifold)`, and the
/// same mechanism: the write-back IS the warm start. A row that did not exist
/// this step — a limit nowhere near its stop, with speculation off — writes a
/// zero, so that a limit that is left is a limit that is forgotten.
void write_back(const joint_batch& batch, joint& j);

// ---------------------------------------------------------------------------
// Which pairs a joint excuses from colliding
// ---------------------------------------------------------------------------

/// **A sorted set of body pairs that must not produce contacts.**
///
/// The narrow phase is the caller's, so the caller has to skip the pairs a
/// joint connects — `joint::collide_connected` is false by default and a chain
/// whose adjacent links collide spends every step fighting itself. A sorted
/// vector of 8.7's `pair_key`s, and a binary search per broadphase pair: the
/// same "rebuild each frame, query in log time" shape as 8.7's cache, without
/// the hash, because the set is small and changes only when a joint is added
/// or removed.
class collision_filter
{
public:
    void clear();

    /// Exclude the pair `(a, b)`, in either order.
    void exclude(std::uint32_t a, std::uint32_t b);

    /// Sort and deduplicate. Call after the last `exclude` and before the
    /// first `excluded`.
    void finalize();

    /// Is the pair `(a, b)`, in either order, excluded?
    [[nodiscard]] bool excluded(std::uint32_t a, std::uint32_t b) const;

    [[nodiscard]] std::size_t size() const;

private:
    std::vector<std::uint64_t> keys_;
};

// ---------------------------------------------------------------------------
// Closed forms, for checking simulations against
// ---------------------------------------------------------------------------

/// **The small-amplitude period of a physical pendulum**, seconds:
/// `2π·sqrt(I_pivot / (m·g·d))`.
///
/// `I_pivot` is the moment of inertia about the pivot axis — which is 8.3's
/// parallel-axis theorem, `I_cm + m·d²`, and is the reason a swinging crate
/// and a swinging ball on the same length of rod keep different time. 8.11 §8
/// hears the difference: a 1 m box swings at 1.645967 s against 1.646241
/// predicted at its 2-degree amplitude, and a point mass would be 13.8% short.
[[nodiscard]] float pendulum_period(float inertia_about_pivot, float mass, float gravity,
                                    float pivot_to_centre);

/// **The same pendulum's period at a finite amplitude**, seconds.
///
///     T(θ₀) = T₀ · (2/π) · K(sin(θ₀/2))
///
/// with `K` the complete elliptic integral of the first kind, evaluated here by
/// Gauss's arithmetic–geometric mean, `K(k) = π / (2·AGM(1, sqrt(1 − k²)))`.
/// The π's cancel, and what is left is `T₀ / AGM(1, cos(θ₀/2))` — a dozen lines
/// that converge quadratically. 8.11 §8 checks it at 3840 Hz from 10 to 120
/// degrees and the worst disagreement is 2.6e-03, at 120.
[[nodiscard]] float pendulum_period_at(float small_amplitude_period, float amplitude_radians);

/// **How far a velocity-level distance constraint drifts in one step**, metres:
/// `sqrt(r² + (v·h)²) − r`.
///
/// A velocity solve leaves the bob moving exactly perpendicular to the rod, and
/// a position update moves it along that straight line — which is a tangent,
/// not the circle. Pythagoras does the rest. For small steps it is
/// `v²h²/(2r)`: half the centripetal acceleration times `h²`, which is how far
/// the bob would have "fallen outward" had the rod let go for one step. 8.11
/// §5 measures 0.003466 m against 0.003466 on the first step of a rod
/// spinning at 5 m/s, and the recurrence it implies — with angular momentum
/// conserved exactly — tracks sixty steps to 1.7e-07.
[[nodiscard]] float constraint_drift_per_step(float speed, float radius, float h);

/// **The parallel-axis ratio of a body on a pin**: `m·d² / (I_cm + m·d²)`,
/// dimensionless, in [0, 1).
///
/// The fraction of the body's rotational inertia about the pin that is in the
/// lever arm — its centre orbiting the pin — rather than in its spin about
/// itself. Zero for a wheel on its axle; 3/4 for any uniform slab hinged at its
/// edge; approaching 1 for a small weight on a long arm. The file header lists
/// the three things it sets. Returns zero when both inputs are zero.
[[nodiscard]] float lever_ratio(float inertia_about_centre, float mass, float pivot_to_centre);

/// **The fraction of a joint's kinetic energy a velocity solve removes in one
/// step**: `ρ·(ω·h)²`, with `ω` the joint's angular speed and ρ `lever_ratio`.
///
/// The price of solving a joint's velocity at the start of a step and moving
/// the body along a straight line for the rest of it: the next solve projects
/// out the radial velocity the turn has produced, and that radial part is
/// `ω·h` of the orbital speed. 8.11 §5 measures it to 0.1% on a slab spun at
/// three pin offsets under both joint kinds. At 60 Hz it takes a quarter of
/// a 90-degree pendulum's energy per period under split impulse — and under
/// Baumgarte very little, because the bias velocity that holds the joint
/// stretched puts back exactly what this removes (§6).
[[nodiscard]] float projection_loss_per_step(float lever, float angular_speed, float h);

/// How long a motor of `max_torque` takes to spin `inertia` up from rest to
/// `speed`: `I·ω/τ`, seconds. The torque clamp is saturated for the whole of
/// the spin-up, so the applied torque is exact — and so is this, for a wheel
/// on its axle. **For anything with a lever arm it is optimistic**, because the
/// joint bleeds energy at `projection_loss_per_step` while the motor works:
/// 8.11 §11 measures a door at 2.8833 s where this says 2.6733, and the ODE
/// with the loss in it says 2.8847.
[[nodiscard]] float motor_spin_up_time(float inertia, float speed, float max_torque);

} // namespace engine::phys
