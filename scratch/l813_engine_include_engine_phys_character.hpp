// engine/include/engine/phys/character.hpp — a character that obeys the design,
// not Newton.
//
// Lesson 8.13. Twelve lessons built a simulation that is CORRECT: bodies that
// keep their momentum, contacts that push exactly as hard as they must, friction
// that holds a crate on a slope up to atan(μ) and not a degree further. This
// file is the one place in the engine where that correctness is the problem.
//
// ---- WHY A CHARACTER IS NOT A RIGID BODY ------------------------------------------
//
// A player character is a capsule that walks, and the obvious way to build one
// is a dynamic capsule with its rotation locked, pushed around by forces. 8.13
// §1 builds exactly that on 8.10's solver and measures it, and every failure it
// finds is Newton being RIGHT about something a designer wants to be false:
//
//   * friction is ONE number for a pair of materials, and the design wants two —
//     enough to stand on a 30° ramp, none at all against a wall mid-jump. With μ
//     = 0.6, enough for the ramp, a character pressed into a wall at any stick
//     speed above `g·h/μ` — 27 cm/s — hangs there;
//   * momentum means a released stick is a SLIDE: 2.91 m from 6 m/s;
//   * a ledge is an obstacle unless you run at it hard enough to be thrown over
//     it — 20 cm is climbed at 8 m/s and at no speed below — so whether a
//     staircase can be climbed depends on SPEED;
//   * and a step is sampled at instants, so a dash through a thin wall is a
//     lottery with odds 8.10 §6 already computed: 50% at 42 m/s through 10 cm.
//
// None of that is a bug to fix in the solver. It is what the solver is FOR. The
// answer is to take the character out of the simulation entirely.
//
// ---- WHAT THIS FILE IS INSTEAD ----------------------------------------------------
//
// A KINEMATIC controller. The character is a capsule with a position and no
// mass, and it moves by asking the world one question — `cast.hpp`'s "how far
// can this shape go along `d` before it comes within `skin` of something?" —
// and then deciding, as policy, what to do about the answer:
//
//   SLIDE      the rest of the motion, projected off every surface hit so far
//              (Quake's clip-plane list, 8.13 §4);
//   SLOPES     a surface is WALKABLE when its normal is within `max_slope` of up;
//              walkable ground redirects the motion along itself, anything
//              steeper is a wall whose normal is flattened so that it cannot be
//              climbed (§5);
//   GROUND     a short cast down after every move; its normal and body are kept
//              (§6);
//   SNAP       when the ground drops away faster than one step of gravity, cast
//              down and stay on it rather than skipping off (§7);
//   STEP UP    a wall shorter than `step_height` is climbed: up, across, down
//              (§8);
//   CARRY      standing on a moving body moves you with it — by its TRANSFORM,
//              not its velocity, or you drift off a turntable (§10).
//
// Every one of those is a design decision with a number on it, and each is
// measured in the lesson. None of them could be expressed as a force.
//
// ---- ONE OWNER, AND THE PROXY -----------------------------------------------------
//
// 8.12's rule: every body has exactly one owner. The controller owns the
// character's position; the solver owns everything else. They meet through a
// PROXY — a kinematic capsule in the `body_world`, at the character's position,
// STEERED there by velocity (`steer_proxy`, 8.12's chord) so that a crate the
// character walks into is pushed at the speed the character is really moving
// and keeps that speed when the character stops. A light enough dynamic body is
// PUSHABLE: the controller's sideways moves ignore it and the proxy shoves it.
// Anything heavier is a wall. 8.13 §11 measures what teleporting the proxy
// instead does to the crate.
//
// ---- WHEN TO CALL IT --------------------------------------------------------------
//
// AFTER the physics step, each fixed step:
//
//     steer_proxy(proxy, ch.position, h);        // catch up to last move
//     world step (velocities, contacts, solve, positions);
//     move_character(ch, displacement, h, view, cfg);
//
// After the step, the world is where it is for this frame, and the one thing
// the controller cannot see in it — how far a moving platform just travelled —
// is exactly `step_motion` of that platform, which the carry reads.
//
// ---- WHAT IT DOES NOT DO ----------------------------------------------------------
//
// It does not decide how the character moves: speed, acceleration, jump height
// and air control are the GAME's, and they arrive as a displacement. The demo
// shows one way; `jump_speed` is the only piece of that arithmetic here.
//
// It is not pushed BY anything. The proxy is kinematic — infinite mass to the
// solver — so a boulder that rolls into a standing character stops dead. That is
// the one-way coupling every kinematic controller has, and 8.13's exercise 4
// reads the proxy's contact impulses back as a knockback.
//
// It culls candidates by AABB over the WHOLE scene, linearly. §12 prices that,
// and a region query on 8.8's grid is exercise 5.

#pragma once

#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/cast.hpp>
#include <engine/phys/integrate.hpp>
#include <engine/phys/rigid_body.hpp>
#include <engine/phys/shape.hpp>

#include <cstdint>
#include <span>

namespace engine::phys
{

/// No body. A `character::ground_body` when the ground is nothing, and a
/// `character_world::self` when there is no proxy to skip.
inline constexpr std::uint32_t k_no_body = 0xFFFFFFFFu;

// ---------------------------------------------------------------------------
// Configuration and state
// ---------------------------------------------------------------------------

/// The character's shape and every design knob, with the defaults 8.13 measured.
struct character_config
{
    /// The capsule. A 1.8 m person: `2·(half_height + radius)`.
    float radius = 0.3f;
    float half_height = 0.6f;

    /// **How far short of contact every cast stops**, in metres.
    ///
    /// The character therefore stands `skin` above the floor, and the drawn
    /// mesh should be lowered by it. Too small and GJK starts to meet touching
    /// shapes (see `cast_config::skin`); too large and the gap is visible. 8.13
    /// §3 measures both ends.
    float skin = 0.01f;

    /// **The steepest ground that counts as floor**, in radians from horizontal.
    ///
    /// Compared as `normal.y ≥ cos(max_slope)`. Not a friction coefficient: a
    /// walkable slope is walked at full speed and stood on without sliding, and
    /// anything steeper is a wall, whatever its material. That split is the
    /// thing Newton's one μ cannot express (8.13 §1).
    float max_slope = 0.78539816f;   // 45°

    /// **How high the step-up lifts the capsule to look for a ledge**, metres.
    ///
    /// Not quite "the tallest ledge climbed". Anything up to `free_curb_height`
    /// is climbed by the round bottom of the capsule with no step-up at all
    /// (8.13 §6), and after a step-up the round bottom rolls over whatever of the
    /// ledge is left — so the two ADD, and at the defaults the tallest ledge is
    /// 0.3 + 0.0908 = 0.39 m, measured to the millimetre in §8. Author it as
    /// `tallest − free_curb_height` if the level designer's number is the ledge.
    float step_height = 0.3f;

    /// **How far down to look for ground that has dropped away**, metres.
    ///
    /// Only while grounded and not rising. Must cover a step down a staircase
    /// and `v·h·tan(slope)` on a ramp (8.13 §7). It is also the height of the
    /// drop a character will be pulled down in ONE step instead of falling, so
    /// it is a decision about how ledges look, not only a tolerance.
    float snap_distance = 0.35f;

    /// Planes a single move may slide along before it gives up the rest.
    int max_slides = 4;

    // ---- The alternatives 8.13 measures, one knob each ------------------------
    //
    // Each of these exists because a section of the lesson needs to show what
    // happens WITHOUT the decision it names, and each default is the decision.
    // `step_height = 0` and `snap_distance = 0` switch those two off the same way.

    /// **What the rest of a move is clipped from, and against what** (§4).
    enum class clip_rule : std::uint8_t
    {
        /// The ORIGINAL intended motion, scaled to the time left, clipped
        /// against every surface hit so far. Quake 1's `SV_FlyMove`. The
        /// default, and the only one of the three that is still in a corner.
        original,

        /// What was LEFT after the last hit, clipped against every surface. The
        /// loop everybody writes first — 8.13's own first draft did — and in an
        /// obtuse corner the leftover has already been bent by one wall, the
        /// other wall's projection of it points back OUT of the corner, and the
        /// character walks back and forth at 58 cm/s (§4).
        remainder,

        /// What was left, clipped against the LAST surface only. Burns every
        /// slide on every move in a 60° corner, and in a 120° one walks back
        /// and forth at 58 cm/s.
        last_plane,
    };
    clip_rule clip = clip_rule::original;

    /// Flatten an unwalkable surface's normal before sliding along it (§5).
    ///
    /// `false` lets the slide lift the character up a slope it may not climb —
    /// though on the ground, laying the move along the floor already throws the
    /// climb away. It is IN THE AIR that it matters: hopping 0.5 m at a time into
    /// a 50° slope, the character gets 0.64 m in with it off and 0.26 with it on.
    bool flatten_walls = true;

    /// Lay a grounded sideways move along the ground, horizontal part kept (§5).
    /// `false` moves horizontally and leaves gravity to keep up (§7).
    bool lay_along_ground = true;

    /// Stop a downward move on walkable ground rather than sliding along it (§5).
    /// `false` slides gravity's drop along the slope, and a character standing
    /// still creeps downhill at `slope_creep_speed`.
    bool stop_on_ground = true;

    /// How standing on a moving body moves you (§10).
    enum class carry_rule : std::uint8_t
    {
        transform,   ///< By the body's step, exactly. The default.
        velocity,    ///< By the point velocity under you. Spirals outward.
        none,        ///< Not at all. The platform leaves without you.
    };
    carry_rule carry = carry_rule::transform;

    /// **A dynamic body this light or lighter is pushed, not walked into.** kg.
    ///
    /// The controller's sideways moves ignore it and the proxy shoves it; the
    /// downward ones still stand on it. Heavier bodies are walls.
    float push_mass_limit = 40.0f;

    /// Handed to every cast. `skin` is overwritten with the field above.
    cast_config cast{};
};

/// A character: a capsule with a position and no mass.
struct character
{
    /// The capsule's centre, world space. Its axis is world +y, always.
    vec3 position{};

    /// **What the last move achieved**, `(end − start)/h` — never what was asked
    /// for. Walk into a wall and this is zero. It is the velocity to show a
    /// camera, an animation blend, or a proxy.
    vec3 velocity{};

    /// Standing on walkable ground after the last move.
    bool grounded = false;

    /// The ground's contact normal (surface → character), unit. Meaningful when
    /// `grounded`. **For a round-bottomed capsule on an edge this is the
    /// direction from the edge to the capsule, not the surface's normal**, and
    /// 8.13 §6 makes a measurement of it.
    vec3 ground_normal{0.0f, 1.0f, 0.0f};

    /// The contact point on the ground, world space.
    vec3 ground_point{};

    /// The body index of the ground, or `k_no_body`. The carry reads it.
    std::uint32_t ground_body = k_no_body;
};

/// What the controller may collide with: the same parallel arrays every Module 8
/// scene keeps — `bodies[i]` has `shapes[i]` — which is a precondition, as it is
/// for `ragdoll`. Module 9's scene layer owns bodies by handle and lifts it.
struct character_world
{
    std::span<const rigid_body> bodies{};
    std::span<const shape> shapes{};

    /// The character's own proxy, skipped by every query. `k_no_body` if none.
    std::uint32_t self = k_no_body;

    /// The world's spin rule, so that `step_motion` turns a platform the way
    /// `integrate_positions` did. `body_world::spin()`.
    spin_rule spin = spin_rule::linearised;
};

// ---------------------------------------------------------------------------
// The one query, and what it found
// ---------------------------------------------------------------------------

/// Which bodies a sweep collides with.
enum class sweep_filter : std::uint8_t
{
    everything,   ///< Every body but the proxy. Downward moves: you can stand on a crate.
    solid,        ///< Leaves out pushable dynamic bodies. Sideways moves: you push a crate.
};

/// The nearest thing a sweep hit.
struct character_hit
{
    bool hit = false;
    float t = 1.0f;                     ///< Fraction of the displacement that is free.
    vec3 normal{};                      ///< Surface normal, obstacle → character. Unit.
    vec3 point{};                       ///< On the obstacle, world space.
    std::uint32_t body = k_no_body;     ///< Which body.
};

/// What one `move_character` call did, for the demo's panel and the harness.
struct move_report
{
    int sweeps = 0;            ///< Calls to `sweep`.
    int candidates = 0;        ///< Obstacles that survived the AABB cull, summed.
    int casts = 0;             ///< Capsule-against-obstacle casts, summed.
    int cast_iterations = 0;   ///< Newton steps, summed over every cast.
    int gjk_iterations = 0;    ///< GJK iterations, summed over every Newton step.
    int slides = 0;            ///< Surfaces hit in the sideways pass.
    int recovered = 0;         ///< Overlaps pushed out before moving.
    bool blocked = false;      ///< The sideways pass ran out of slides with motion left.
    bool stepped = false;      ///< A step-up was taken.
    float step_rise = 0.0f;    ///< ...and it raised the character by this much.
    bool snapped = false;      ///< Snapped down to ground that had dropped away.
    float snap_drop = 0.0f;    ///< ...by this much,
    float snap_shift = 0.0f;   ///< ...after rolling this far sideways off an edge.
    bool landed = false;       ///< Became grounded this move.
    bool hit_ceiling = false;  ///< A rising move was stopped from above.
    vec3 carried{};            ///< Displacement taken from the ground body's motion.
    quat carried_turn{};       ///< Rotation taken from it. Apply to the facing.
};

/// The character's capsule, placed.
[[nodiscard]] capsule character_capsule(vec3 position, const character_config& cfg);

/// **Is this surface floor?** `normal.y ≥ cos(max_slope)`.
[[nodiscard]] bool walkable(vec3 surface_normal, const character_config& cfg);

/// **Is this body pushed rather than walked into?** Dynamic, and no heavier
/// than `push_mass_limit`. Fixed and kinematic bodies never are.
[[nodiscard]] bool pushable(const rigid_body& b, const character_config& cfg);

/// **The one query.** Sweep the capsule at `position` along `d`; return the
/// nearest hit over every body that passes `filter` and the swept-AABB cull.
///
/// ONE CAST PER OBSTACLE, KEEPING THE SMALLEST `t` — never one cast against the
/// union, which is not convex and lets conservative advancement overshoot
/// (`cast.hpp`; 8.13 §3 measures it). An obstacle the capsule already overlaps
/// is skipped: overlap is `recover`'s business, and a sweep that refused to
/// move because of it would pin the character inside whatever pushed into it.
[[nodiscard]] character_hit sweep(vec3 position, vec3 d, const character_world& w,
                                  const character_config& cfg, sweep_filter filter,
                                  move_report* report = nullptr);

// ---------------------------------------------------------------------------
// Policy, one function per decision
// ---------------------------------------------------------------------------

/// Remove the part of `v` that points INTO a surface: `v − min(0, v·n)·n`.
[[nodiscard]] vec3 slide_along(vec3 v, vec3 n);

/// **Quake's rule: `v` with nothing left that goes INTO any of `planes`.**
///
/// Try each plane alone; if the result goes into none of the others, use it.
/// Otherwise try each pair's CREASE — the line `n_i × n_j` both planes contain;
/// if that goes into none of the rest, use it. Otherwise stop. An empty list
/// returns `v`.
///
/// Which `v` to hand it is the other half of the rule, and `character_config::
/// clip_rule` names the three answers §4 measures. Quake 1's is the ORIGINAL
/// motion, every time — not what was left after the last hit.
[[nodiscard]] vec3 clip_to_planes(vec3 v, std::span<const vec3> planes);

/// **A wall's normal with its vertical part removed.** An unwalkable surface
/// may stop you and slide you sideways, but must never lift you; flattening the
/// normal makes the slide horizontal. Returns `n` itself when it is too close to
/// vertical to flatten (a ceiling or a floor).
[[nodiscard]] vec3 wall_normal(vec3 n);

/// **A sideways motion laid onto walkable ground, keeping its horizontal part.**
///
///     t = d − ŷ·(d·n)/n.y
///
/// The unique vector with `d`'s horizontal part that lies in the ground plane.
/// So a character crosses a map at the same horizontal speed on a ramp as on the
/// flat — a design choice, and 8.13 §5 names the other one.
[[nodiscard]] vec3 along_ground(vec3 d, vec3 ground_normal);

/// **Push the character out of any FIXED or KINEMATIC body it overlaps, and
/// back to `skin` from any nearer than half of it.** Returns how many pushes.
///
/// Overlap arrives from outside the controller — a platform moving into a
/// standing character, a door swinging shut on it — and is a question for 8.6's
/// EPA, not for a cast. **Dynamic bodies are left out, heavy ones included**:
/// they are the solver's, and they meet the character through the proxy. 8.13
/// §11 measured the version that pushed out of heavy dynamic bodies too: a
/// 200 kg boulder carried the character 2.94 m, because the solver was now
/// moving a body the controller owns. Two owners, one position.
int recover(character& c, const character_world& w, const character_config& cfg);

// ---------------------------------------------------------------------------
// Standing on things that move
// ---------------------------------------------------------------------------

/// Exactly what one `integrate_positions` step does to a body: it travels
/// `translation` and turns by `rotation` about where its centre WAS.
struct body_step
{
    vec3 translation{};
    quat rotation{};
    vec3 pivot{};   ///< The centre before the step.
};

/// **The step `b` has just taken**, read back from its current state.
///
/// Call it AFTER the step. The translation is `v·h` and the rotation is the one
/// `advance_orientation` performs — for the linearised rule `normalise(1, ½hω)`,
/// NOT the axis-angle `|ω|h` a reader would write down. That is 8.12's lesson
/// again: whatever reads a body's motion must invert the integrator the body
/// was actually moved by.
[[nodiscard]] body_step step_motion(const rigid_body& b, float h, spin_rule rule);

/// Where a point riding on the body ends up after `m`: `c + v·h + R(p − c)`.
[[nodiscard]] vec3 carry_by_transform(vec3 p, const body_step& m);

/// **The tempting carry, and it is wrong.** `p + h·(v + ω × (p − c))`.
///
/// The point velocity at `p` is tangent to the circle it is on, so each step
/// moves the rider along a TANGENT and it spirals outward:
/// `|r|² ← |r|²(1 + (ωh)²)`, a factor `e^(πωh)` per revolution. The same drift
/// as 8.11's velocity joint, for the same reason. Kept for 8.13 §10.
[[nodiscard]] vec3 carry_by_velocity(vec3 p, const rigid_body& b, float h);

// ---------------------------------------------------------------------------
// The move
// ---------------------------------------------------------------------------

/// **Move the character by `displacement` as the design says, not as Newton
/// would.** `h` is the step, for the carry and for `character::velocity`.
///
/// In order: carry by the ground body's last step; recover from overlaps; the
/// SIDEWAYS pass (x and z, laid along walkable ground, slid off walls, stepping
/// up what it can); the VERTICAL pass (y: landing, sliding off steep slopes,
/// stopping at ceilings); then the ground probe, and the snap if the ground has
/// dropped away. 8.13 §13 writes it in that order and says why each is there.
move_report move_character(character& c, vec3 displacement, float h, const character_world& w,
                           const character_config& cfg);

/// **Steer a kinematic proxy onto the character by velocity**: the chord
/// `(target − x)/h`, which the next `integrate_positions` lands exactly — 8.12's
/// `steer`, for one body and no rotation. Never teleport it: 8.13 §11.
void steer_proxy(rigid_body& proxy, vec3 target, float h);

// ---------------------------------------------------------------------------
// Closed forms, each measured against the controller in 8.13
// ---------------------------------------------------------------------------

// The three edge formulas take the radius THE CASTS SEE, which is the capsule's
// radius plus its skin: every cast stops `skin` short, so it is the inflated
// capsule that touches.

/// **The tallest ledge a capsule climbs with no step-up at all**: `r(1 − cos θ)`.
/// The round bottom meets a ledge's edge with a normal tilted by
/// `acos((r − s)/r)`, and while that is walkable the edge is a ramp. §6.
[[nodiscard]] float free_curb_height(float radius, float max_slope);

/// **How far a capsule's centre may hang past an edge and still stand**:
/// `r·sin θ`. §6.
[[nodiscard]] float edge_overhang(float radius, float max_slope);

/// **The least a step-up must carry the capsule forward** for it to land on the
/// ledge's edge with a walkable normal: `r(1 − sin θ)`. At the riser the centre
/// is up to `r` short of the edge, and it has to be within `r·sin θ`. §8.
[[nodiscard]] float step_forward_min(float radius, float max_slope);

/// **How fast a character creeps down a slope it is standing still on** when
/// its gravity is slid along the ground instead of stopped by it: `g·h·sin α`
/// — the per-step drop `g·h²`, projected. §5.
[[nodiscard]] float slope_creep_speed(float g, float h, float slope);

/// **The speed down a slope above which a character that moves horizontally
/// and leaves gravity to keep it down loses the ground every step**:
/// `(g·h² + reach) / (h·tan α)`. One step of gravity drops it `g·h²`, the ground
/// drops `v·h·tan α`, and whatever the ground probe reaches below that is a
/// small snap of its own — `2·skin` in `move_character`. With no probe at all it
/// is `g·h / tan α`: 28 cm/s on a 30° slope at 60 Hz. §7.
[[nodiscard]] float skip_speed(float g, float h, float slope, float reach = 0.0f);

/// **How much a rider carried by velocity drifts outward per revolution**, as a
/// factor on its radius: `(1 + (ωh)²)^(N/2)` with `N = 2π / θ_step` steps and
/// `θ_step = 2·atan(ωh/2)`, the turn of one linearised step. `≈ e^(πωh)`. §10.
[[nodiscard]] float velocity_carry_growth(float omega, float h);

/// **The launch speed for a jump of `height` metres**: `√(2 g H)`. A designer
/// names the height; the speed is arithmetic.
[[nodiscard]] float jump_speed(float g, float height);

/// **How far a sliding body travels before friction stops it**: `v²/(2μg)`.
/// The rigid-body character's stop, §1.
[[nodiscard]] float stopping_distance(float speed, float friction, float g);

} // namespace engine::phys
