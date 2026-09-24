// engine/include/engine/phys/ragdoll.hpp — a skeleton the solver can drop.
//
// Lesson 8.12. Module 7 gave the engine a character that moves because a clip
// says so, and Module 8 gave it bodies that move because Newton says so. A
// ragdoll is the one object that has to be both, one after the other, and the
// whole of this file is the seam between them.
//
// ---- WHAT A RAGDOLL IS ----------------------------------------------------------
//
// A skeleton with mass. Some of its joints — not all; 7.7's humanoid has 23 and
// its ragdoll has 11 bodies — get a capsule, a mass and an inertia, and each of
// those PARTS is joined to its nearest ancestor part by one of 8.11's joints:
// a hinge at an elbow or a knee, and at a shoulder, a hip, the neck and the
// waist a ball-socket with a swing cone and a twist range (8.12 §3–§5). The
// joints in between that are not parts — a clavicle, a hand, a toe — are
// PASSENGERS: they ride rigidly on the nearest part above them, at whatever
// local transform the animation left them at.
//
// ---- THE ONE RULE: EVERY BODY HAS EXACTLY ONE OWNER AT A TIME -------------------
//
// A ragdoll is in one of two modes, and the mode is who owns the bodies:
//
//   ANIMATED   the clip owns them. They are KINEMATIC — infinite mass, moved only
//              by their own velocity — and `steer` sets that velocity every step
//              to the one that lands each body exactly on the animated pose. They
//              still collide, so a jogging character pushes crates, and the push
//              is at the speed the limb is really moving.
//   SIMULATED  the solver owns them. They are DYNAMIC, their joints are in the
//              solver, and `read_pose` turns wherever they are into a pose the
//              skinning code can draw.
//
// The handoff each way is then almost nothing, and that is the design paying
// off rather than a shortcut:
//
//   * ANIMATION → PHYSICS is `simulate`: give the bodies their mass back. Their
//     velocities are ALREADY right, because `steer` put them there — and for the
//     semi-implicit Euler this engine integrates with, the velocity that carried
//     a body from its last pose to this one is not an approximation of the
//     body's velocity, it IS the body's velocity (`x_n = x_{n−1} + h·v_n`).
//     8.12 §9 measures the alternative, which is a character that stops dead in
//     mid-stride and then falls.
//   * PHYSICS → ANIMATION is `animate`: take the mass away and start steering
//     toward a pose that BEGINS where the ragdoll lies (`read_pose`) and blends
//     toward the clip. Nothing jumps, because the first target is where the
//     bodies already are.
//
// The tempting alternative — leave the bodies dynamic and write the animated
// pose into them — is 8.11 §1's teleport, and 8.12 §12 measures what it does:
// the velocities stop describing the motion, and anything the character touches
// is hit by a body whose velocity is a lie.
//
// ---- WHAT THIS FILE DOES NOT DO -------------------------------------------------
//
// It does not run the narrow phase (the caller's, as since 8.7), and it does
// not own a `body_world`. Its bodies are a CONTIGUOUS RUN of the world's dense
// array, starting at `ragdoll::first_body`, which is what `spawn` produces in a
// world nothing is removed from. That is the same convention every physics
// demo and harness in Module 8 uses, and it is a precondition, not a
// guarantee: `body_world::remove` swaps the last body into the hole. Module 9's
// scene layer, which will own bodies by handle, lifts it.
//
// It does not get a character up. `read_pose` and `realign_model` put the
// animation back where the ragdoll fell; a production game then plays a GET-UP
// clip whose first frame is lying down, and this engine has no such clip. 8.12
// §11 blends to a standing pose instead and says plainly what that looks like.

#pragma once

#include <engine/anim/skeleton.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/constraint.hpp>
#include <engine/phys/integrate.hpp>
#include <engine/phys/rigid_body.hpp>
#include <engine/phys/shape.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace engine::phys
{

class constraint_solver;

// ---------------------------------------------------------------------------
// Authoring
// ---------------------------------------------------------------------------

/// How a part hangs from its parent part.
enum class ragdoll_link : std::uint8_t
{
    /// No parent. Exactly one part — conventionally the pelvis — is the root.
    root,

    /// One degree of freedom: an elbow or a knee. `ragdoll_part_desc::axis` is
    /// the hinge axis and `lower`/`upper` its range, measured from the bind pose.
    hinge,

    /// Three degrees of freedom, two of them limited: a shoulder, a hip, the
    /// neck, the waist. `axis` is the TWIST axis (the bone; zero means "along
    /// the capsule"), `cone_axis` the middle of the swing cone (zero means "the
    /// twist axis"), `swing` the cone's half-angle and `lower`/`upper` the twist
    /// range. 8.12 §3–§5.
    cone_twist,
};

/// `"root"`, `"hinge"`, `"cone-twist"`.
[[nodiscard]] const char* name_of(ragdoll_link link);

/// **One body of a ragdoll, authored against the skeleton's BIND pose, in MODEL
/// space.**
///
/// Model space rather than joint space because that is where a person
/// authoring a ragdoll is looking: the character stands in its bind pose, the
/// capsules are drawn over it, and the numbers are read off the picture.
/// `build_ragdoll` converts everything into the frames the runtime needs.
struct ragdoll_part_desc
{
    /// For logs and debug UI.
    std::string name;

    /// The skeleton joint this body follows. Its bind position is also where
    /// the part's joint to its parent is anchored — a shoulder joint sits at
    /// the shoulder.
    ///
    /// **Called `bone` rather than `joint` because this file has two kinds of
    /// joint in it**: the skeleton's, which is a frame in a hierarchy, and the
    /// solver's, which is a constraint between two bodies. Every part has one of
    /// each and they sit at the same place, which is exactly why one name for
    /// both would be a bug waiting for a reader.
    anim::joint_index bone = anim::k_no_parent;

    /// The capsule's segment end points, model space, bind pose. The capsule
    /// is this segment inflated by `radius`, so a limb that should reach from
    /// joint to joint wants its segment shortened by the radius at each end.
    vec3 from{};
    vec3 to{};
    float radius = 0.05f;

    /// This part's share of `ragdoll_desc::mass`. The fractions should sum to
    /// one; `ragdoll_report::mass_fraction_sum` says whether they do.
    float mass_fraction = 0.0f;

    ragdoll_link link = ragdoll_link::root;

    /// The hinge axis (hinge) or the twist axis (cone-twist), model space,
    /// bind pose. Zero for a cone-twist means "along the capsule".
    vec3 axis{};

    /// The swing cone's middle, model space, bind pose. Cone-twist only; zero
    /// means "the twist axis".
    vec3 cone_axis{};

    /// The swing cone's half-angle, radians. Cone-twist only.
    float swing = 0.0f;

    /// The hinge range (hinge) or twist range (cone-twist), radians, measured
    /// from the bind pose.
    float lower = 0.0f;
    float upper = 0.0f;
};

/// Which pairs of parts a ragdoll keeps from colliding with each other.
///
/// **8.12 §6 is the argument, measured on the humanoid.** A jointed pair
/// overlaps at its joint by construction and must be excluded — the control
/// that excludes nothing leaves a 68 mm joint gap and a ragdoll that never
/// settles. Beyond that, the humanoid's audit finds NO unjointed pair within
/// 4 cm at rest, in the bind pose or over either motion 8.12 hands over. The
/// obvious wider rule, "anything within two links", excludes thirteen more
/// pairs — and three of them are exactly the pairs that touch when a ragdoll
/// falls: the forearms against the torso (in nine falls of twelve) and the
/// thighs against each other. Under it a forearm comes to rest inside the
/// chest, 180 mm deep. So the rule is about what overlaps AT REST, and it is
/// only a default because the audit is cheap: a description with a fatter
/// pelvis or a longer torso capsule does overlap its thighs, and then it
/// matters.
enum class ragdoll_exclusion : std::uint8_t
{
    /// Only pairs a joint connects. 8.11's rule, and not enough.
    jointed,

    /// Jointed pairs, and every other pair whose capsules overlap (within
    /// `ragdoll_desc::overlap_margin`) in the bind pose. The default: it
    /// excludes exactly the pairs that would otherwise fight their joints
    /// forever, and nothing else.
    overlapping,

    /// Every pair of parts at most two links apart in the tree. What most
    /// people write first.
    two_links,
};

/// A whole ragdoll, before it has met a skeleton.
struct ragdoll_desc
{
    /// Parent parts BEFORE their children — the skeleton's own precondition,
    /// and checked (`ragdoll_report::out_of_order`).
    std::vector<ragdoll_part_desc> parts;

    /// The character's total mass, kilograms.
    float mass = 70.0f;

    ragdoll_exclusion exclusion = ragdoll_exclusion::overlapping;

    /// How close two capsules may come at rest, metres, before `overlapping`
    /// excludes them. A few millimetres of air between two limbs is contact on
    /// the first frame, and a pair that starts in contact never settles.
    float overlap_margin = 0.01f;
};

// ---------------------------------------------------------------------------
// The runtime
// ---------------------------------------------------------------------------

/// One body of a built ragdoll: everything the runtime needs, in the frames it
/// needs it in.
struct ragdoll_part
{
    std::string name;

    /// The skeleton joint this part follows (see `ragdoll_part_desc::bone`),
    /// and its parent PART (−1 for the root).
    anim::joint_index bone = anim::k_no_parent;
    int parent = -1;

    /// A capsule along body +y, centred on the centre of mass (8.4's rule).
    shape collider{};

    /// Kilograms, and the tensor about the centre in body axes. Kept here
    /// rather than only on the body because an ANIMATED ragdoll's bodies are
    /// kinematic and carry no mass at all; `simulate` gives it back from here.
    float mass = 0.0f;
    mat3 inertia{};

    /// **Where the body sits in its skeleton joint's frame**: `joint_from_body`,
    /// as a position and a rotation. Constant — the body is bolted to the bone —
    /// and the only link between the two representations. A pose places the
    /// body with it; a body gives back the pose with its inverse.
    vec3 offset{};
    quat rotation{};

    /// The joint to the parent part, authored at the bind pose in the two
    /// bodies' own frames, so it is valid at any pose. Also where the solver's
    /// warm-start impulses live, so the part vector must not reallocate between
    /// `add_joints` and `constraint_solver::solve`.
    joint link{};
    ragdoll_link kind = ragdoll_link::root;
};

/// Who owns the bodies. See the file header.
enum class ragdoll_mode : std::uint8_t
{
    animated,
    simulated,
};

/// A built ragdoll.
struct ragdoll
{
    std::vector<ragdoll_part> parts;

    /// Skeleton joint → part index, or −1 for a passenger. Sized to the skeleton.
    std::vector<int> part_of_joint;

    /// Pairs of PART indices that must not collide, sorted. `exclude_pairs`
    /// hands them to a `collision_filter` as body indices.
    std::vector<std::uint32_t> excluded;   // packed: (i << 16) | j, i < j

    /// **The animation's local pose at the moment of the last `simulate`.** The
    /// passengers — joints that are not parts — keep these locals while the
    /// solver owns the parts, which is what "rides rigidly on the part above"
    /// means.
    std::vector<transform> frozen_local;

    /// The first of `parts.size()` consecutive bodies in the world's dense array.
    std::uint32_t first_body = 0;

    ragdoll_mode mode = ragdoll_mode::animated;
};

/// What `build_ragdoll` found. Counts rather than a bool, 5.3's shape.
struct ragdoll_report
{
    std::size_t parts = 0;
    std::size_t hinges = 0;
    std::size_t cone_twists = 0;

    /// Parts with no parent part. Must be exactly one.
    std::size_t roots = 0;

    /// Parts whose `joint` names no skeleton joint, or whose parent part comes
    /// after them.
    std::size_t bad_joint = 0;
    std::size_t out_of_order = 0;

    /// Two parts following the same skeleton joint.
    std::size_t duplicate_joint = 0;

    /// **The self-collision audit** (8.12 §6). How many pairs the exclusion
    /// rule removed, split by why; and how many NON-excluded pairs overlap at
    /// the bind pose — which must be zero, or those pairs fight from frame one.
    std::size_t excluded_jointed = 0;
    std::size_t excluded_overlapping = 0;
    std::size_t excluded_two_links = 0;
    std::size_t overlapping_colliding = 0;

    /// The sum of `mass_fraction`. One, for a desc that means what it says.
    float mass_fraction_sum = 0.0f;

    [[nodiscard]] bool ok() const
    {
        return roots == 1 && bad_joint == 0 && out_of_order == 0 && duplicate_joint == 0
               && overlapping_colliding == 0;
    }
};

/// **Turn a description into a ragdoll**, against the skeleton's bind pose.
///
/// Builds each part's capsule, mass and inertia; finds each part's parent part
/// (its nearest ancestor joint that is also a part); places every body at the
/// bind pose in MODEL space, creates each joint there with 8.11's `make_hinge`
/// or 8.12's `make_cone_twist`, and keeps only the body-local result; and
/// audits every pair of capsules for overlap at rest.
///
/// Reads only `sk.joints` — the bind pose is composed here, so the inverse
/// binds need not have been baked. Requires unit scale on every joint: a
/// ragdoll bolts rigid bodies to bones, and a scaled bone is not rigid.
ragdoll_report build_ragdoll(const anim::skeleton& sk, const ragdoll_desc& desc, ragdoll& out);

/// **Where every part's body belongs for a pose**, world space:
///
///     world_from_body = world_from_model · model_from_joint[part.bone] · joint_from_body
///
/// `model_from_joint` is `anim::compose_pose`'s output. `out` is resized to the
/// part count; each entry's `scale` is one.
void part_targets(const ragdoll& rd, std::span<const mat4> model_from_joint,
                  const mat4& world_from_model, std::vector<transform>& out);

/// **Add the ragdoll's bodies to `world`, KINEMATIC, at `targets`, at rest.**
/// Returns the index of the first; the rest follow it, in part order. Also
/// records it in `rd.first_body` and puts the ragdoll in `animated` mode.
std::uint32_t spawn(ragdoll& rd, body_world& world, std::span<const transform> targets);

/// **The angular velocity that turns `from` into `to` in exactly one step of
/// `rule`.** Lesson 8.12.
///
/// Not the calculus answer. The calculus answer is `angle · axis / h` — the
/// logarithm — and it is exact only for `spin_rule::exponential`. The engine's
/// default rule, `linearised`, steps `q ← normalise(q + ½·h·ω·q)`, which turns
/// by `2·atan(|ω|·h/2)` rather than `|ω|·h`; inverting THAT gives
///
///     ω = (2/h) · Δq.v / Δq.w,       Δq = to · conj(from), moved to Δq.w > 0
///
/// the Rodrigues vector, which is `tan(θ/2)` where the logarithm has `θ/2`. The
/// two differ by `θ²/12` of the rotation — 0.06% for a limb turning 5 rad/s at
/// 60 Hz — and 8.12 §9 measures the landing error of each. The point is not the
/// size: it is that the velocity to hand a body is whatever its integrator will
/// invert, and the same is true of the linear half, where it is the chord.
[[nodiscard]] vec3 steering_angular_velocity(quat from, quat to, float h, spin_rule rule);

/// **Set every kinematic body's velocity so that the next `integrate_positions`
/// lands it exactly on its target.** Linear: `(target − x)/h`, the chord.
/// Angular: `steering_angular_velocity`. Does nothing to dynamic bodies.
///
/// Call it BEFORE the step, every step the ragdoll is animated. Its bodies are
/// then never teleported, their velocities are always the ones that actually
/// move them, and contacts see the truth — which is why `simulate` can leave
/// the velocities exactly as they are.
void steer(const ragdoll& rd, std::span<rigid_body> bodies, std::span<const transform> targets,
           float h, spin_rule rule = spin_rule::linearised);

/// **Hand the ragdoll to the solver.** Every body becomes dynamic with its
/// part's mass and inertia, its velocities untouched, awake. `pose` is the
/// animation's LOCAL pose at this instant, kept as `frozen_local` for the
/// passengers.
void simulate(ragdoll& rd, std::span<rigid_body> bodies, std::span<const transform> pose);

/// **Hand the ragdoll back to the animation.** Every body becomes kinematic —
/// no mass, no inertia — keeping its position, orientation and velocity, and
/// the joints' cached impulses are cleared so that the next `simulate` does
/// not inherit an impulse from a pose it is no longer in.
void animate(ragdoll& rd, std::span<rigid_body> bodies);

/// **Add the ragdoll's joints to a solve.** Every step, between `begin` and
/// `solve`, while the ragdoll is simulated; nothing while it is animated,
/// because a joint between two kinematic bodies has an effective mass of zero
/// and nothing to do.
void add_joints(ragdoll& rd, constraint_solver& solver);

/// Exclude the ragdoll's non-colliding pairs from a filter, as body indices.
/// Call `finalize` on the filter afterwards.
void exclude_pairs(const ragdoll& rd, collision_filter& filter);

/// **Read a local pose back off the bodies**, for the whole skeleton.
///
/// A part's joint goes where its body says, through the inverse of
/// `joint_from_body`; a passenger rides on its parent at its `frozen_local`;
/// and every joint's local transform is then its parent's inverse times its
/// own, which is the form `anim::compose_pose` and a blend both want.
///
/// `world_from_model` is the frame the pose is to be expressed in — the
/// character's placement, or `realign_model`'s answer. Scales are copied from
/// `frozen_local`.
void read_pose(const ragdoll& rd, const anim::skeleton& sk, std::span<const rigid_body> bodies,
               const mat4& world_from_model, std::vector<transform>& local_out);

/// **A character placement that puts the animation back where the ragdoll
/// fell**: `world_from_model` translated in the ground plane (world x and z) so
/// that the root part's joint, posed as `anim_root_part_model` describes it in
/// model space, lands directly over the root part's body. Height and heading
/// are left alone — the first is the clip's business and the second is
/// exercise 4.
///
/// Without it the return blend drags the whole character from where it fell to
/// where it was standing when it tripped; 8.12 §11 measures the slide.
[[nodiscard]] mat4 realign_model(const ragdoll& rd, std::span<const rigid_body> bodies,
                                 const mat4& world_from_model, vec3 anim_root_part_model);

/// The worst joint error in a simulated ragdoll right now — the largest anchor
/// gap, metres, and the largest angular violation, radians. Instrumentation.
[[nodiscard]] joint_error worst_joint_error(const ragdoll& rd, std::span<const rigid_body> bodies);

} // namespace engine::phys
