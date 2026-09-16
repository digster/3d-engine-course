// engine/include/engine/anim/skeleton.hpp — a hierarchy that bends one surface.
//
// Lesson 7.6. Module 5 gave the engine a transform hierarchy, and it is already
// most of a skeleton: `ecs/hierarchy.hpp` walks parents before children and
// composes `parent_from_local` down the chain, which is exactly what a limb
// does. A tank's turret follows its hull with that and nothing else.
//
// A CHARACTER IS THE CASE IT CANNOT DO, and the reason is not the maths — it is
// the geometry. A tank is two rigid objects, so two matrices describe it
// completely. An arm is ONE continuous surface, and the vertices around the
// elbow do not belong to the upper arm or to the forearm. They belong to both,
// and they have to move by something in between, or the skin tears open at the
// joint.
//
// SKINNING is the answer, and it is one sentence: let every vertex be moved by
// several joints at once, and blend the results by weight. The whole of this
// header is the bookkeeping that makes that sentence well-typed.
//
//     v' = sum_i  w_i * skin_i * v
//
// The difficulty is entirely in `skin_i`, which is the matrix this file exists
// to produce, and it is NOT a joint's world matrix. A vertex is modelled once,
// in a chosen pose — the BIND POSE — and its coordinates are in MODEL space,
// not in any joint's space. Multiplying it by a joint's posed matrix would carry
// it from joint space to model space, and it was never in joint space to begin
// with. It has to be put there first, and the matrix that does that is recorded
// once, at bind time, and never changes:
//
//     skin_j  =  model_from_joint(posed)  *  joint_from_model(bind)
//                ^^^^^^^^^^^^^^^^^^^^^^      ^^^^^^^^^^^^^^^^^^^^^
//                where joint j is NOW        where joint j WAS
//
// READ THE LABELS. The inner ones agree, which is Lesson 2.8's rule and the only
// reason the product means anything — and the two `joint`s are the same joint at
// two different TIMES, which is the one part of the vocabulary that trips people
// and the one part no naming convention can carry for you.
//
// THEN READ THE OUTER ONES. They are the same space. A skinning matrix is
// `model_from_model`: it takes model space to model space and never leaves. Two
// consequences fall straight out of that, and they are the two halves of Lesson
// 7.6:
//
//   1. IT IS WHY THE BLEND IS LEGAL AT ALL. You may add matrices only when they
//      share a domain and a codomain; four skinning matrices do, so the weighted
//      sum is a map from model space to model space like each of its terms.
//   2. IT IS ALSO WHY THE BLEND IS NOT A ROTATION. The set of maps model-to-model
//      is closed under addition and the set of ROTATIONS is not, so the average
//      of two rotations is a shear that shrinks. That shrink has an exact size —
//      `cos(theta/2)`, the same half-angle the quaternion carries — and it is the
//      "candy wrapper" collapse every rigger knows. Lesson 7.6 §9 derives and
//      measures it; `anim/skin.hpp` names the fix.
//
// AT THE BIND POSE EVERY SKINNING MATRIX IS THE IDENTITY, exactly, because the
// two factors are inverses of each other. That is not a nicety — it is the
// cheapest true test of an entire rig, it needs no reference image, and
// `skeleton_report::worst_bind_residual` is it.

#pragma once

#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace engine::anim {

/// How a joint names its parent, and how a vertex names a joint.
///
/// 16 bits, so a skeleton may hold 65,534 joints and a per-vertex influence
/// costs two bytes rather than four. The size is chosen by the CONSUMER rather
/// than by the skeleton: `skin_influence` stores four of these per vertex and a
/// million-vertex character is 8 MB at 16 bits and 16 MB at 32. Real rigs run to
/// a few hundred joints — a film-grade one with facial controls reaches low
/// thousands — so the ceiling is three orders above the need, which is the right
/// place for a ceiling to be.
using joint_index = std::uint16_t;

/// "This joint has no parent" — it is a root, and its `local_bind` is already in
/// model space.
///
/// The all-ones sentinel rather than `-1` on a signed type, for the reason
/// `handle.hpp` gives: an unsigned index cannot be accidentally compared as
/// negative, and the one invalid value is the one the type cannot otherwise
/// reach.
inline constexpr joint_index k_no_parent = 0xFFFFu;

/// The largest skeleton this representation can address.
inline constexpr std::size_t k_max_joints = 0xFFFFu;

/// One joint: where it rests, and what it hangs from.
///
/// **Note what is NOT here.** No children list (derivable, and a second source of
/// truth — the argument `ecs::parent` makes in 5.9). No world matrix (that is a
/// property of a POSE, not of the skeleton; a skeleton is static data shared by
/// every character wearing it). And no inverse bind matrix, which lives in the
/// skeleton's own parallel array because it is DERIVED — `bake_inverse_binds`
/// computes it, and a field an editor could hand-write would be a field that can
/// disagree with the bind pose beside it.
struct joint
{
    /// The joint's resting placement, **in its parent's space**.
    ///
    /// The same `transform` every object in the engine uses, and that is worth a
    /// sentence: a joint is not a special kind of object, it is an ordinary
    /// placement that happens to have vertices weighted to it. Since Lesson 7.5
    /// the rotation field is a `quat`, which is what makes a pose interpolable —
    /// 40 bytes per joint, so a hundred-joint character's pose is 4 KB and a
    /// thirty-second clip at 30 Hz is 3.6 MB before any compression. Lesson 7.7
    /// spends that budget.
    transform local_bind{};

    /// Index into `skeleton::joints`, or `k_no_parent`.
    ///
    /// **MUST BE LESS THAN THIS JOINT'S OWN INDEX.** See `skeleton` below: that
    /// one precondition replaces the entire level-order machinery Lesson 5.9
    /// needed, and `validate` is what enforces it.
    joint_index parent = k_no_parent;

    /// The joint's name in whatever authored it. Diagnostic, and not optional in
    /// practice: "joint 34 is wrong" is a far worse bug report than
    /// `"forearm.twist.L"`, and an animation clip authored in another tool
    /// addresses joints by name because indices do not survive a re-export.
    std::string name;
};

/// A rig: joints in parent-before-child order, and the bind pose, inverted.
///
/// **THE ORDERING PRECONDITION IS THE WHOLE DESIGN, so it is worth comparing to
/// the place this engine solved the same problem differently.** Lesson 5.9's
/// `ecs::hierarchy` could not require any order: a component pool's dense order
/// is insertion order disturbed by every swap-and-pop, so the resolver had to
/// compute depths, counting-sort into level buckets and keep that order fresh —
/// three passes and a dirty flag.
///
/// A skeleton is different in kind, in two ways that both point the same way.
/// It is AUTHORED, not accumulated: a rig arrives whole from an exporter and its
/// joints never move afterwards. And it is SMALL: hundreds of joints, not the
/// hundred thousand entities 5.9 was measured at. So the order can simply be a
/// requirement on the data, checked once by `validate`, and `compose_pose`
/// becomes a single flat loop over an array with no sorting, no bookkeeping and
/// no staleness to track.
///
/// **That is the same trade the whole engine keeps making**: a precondition the
/// producer can satisfy cheaply beats an algorithm the consumer must run every
/// frame. What it costs is an importer that has to reorder — glTF's `skins.joints`
/// is an arbitrary permutation of node indices, and the node tree is not sorted
/// — and that is one topological sort at load time, paid once.
struct skeleton
{
    /// Parent-before-child. `joints[i].parent < i` or `k_no_parent`, always.
    std::vector<joint> joints;

    /// The INVERSE BIND MATRICES, one per joint, parallel to `joints`.
    ///
    /// `joint_from_model[j]` carries a point from the space the mesh was modelled
    /// in into joint `j`'s own space, **as that space stood at bind time**. It is
    /// the thing a vertex needs before a posed joint matrix can mean anything to
    /// it, and it is the only piece of a skeleton that is not directly authorable.
    ///
    /// Written by `bake_inverse_binds` and by nothing else. Empty until then, and
    /// `validate` reports that as `unbaked` rather than letting a palette of
    /// identity matrices ship a character that renders in its bind pose forever.
    ///
    /// **The name is deliberate.** "Inverse bind matrix" is what every file format
    /// and every paper calls it, and it is the vocabulary that makes skinning
    /// sound harder than it is: it says how the thing was MADE rather than what it
    /// DOES. Named for its job in this engine's convention it is
    /// `joint_from_model`, which composes with `model_from_joint` by inspection.
    std::vector<mat4> joint_from_model;

    [[nodiscard]] std::size_t size() const { return joints.size(); }
    [[nodiscard]] bool empty() const { return joints.empty(); }

    void clear()
    {
        joints.clear();
        joint_from_model.clear();
    }
};

/// What `validate` found. The shape Lesson 5.3 settled on and `mesh_report`
/// follows: counts rather than a bool, because the question you ask after "is it
/// broken" is always "how".
struct skeleton_report
{
    std::size_t joints = 0;     ///< how many the skeleton holds
    std::size_t roots = 0;      ///< …of which these have no parent
    std::size_t depth = 0;      ///< the longest chain, counted in joints (1 = flat)

    /// Joints whose `parent` is **not less than their own index**.
    ///
    /// The precondition `compose_pose` relies on and cannot afford to re-check per
    /// frame. A violation is not a slightly wrong picture: the parent's matrix has
    /// not been written yet when the child reads it, so the child composes against
    /// last frame's value — which looks like a one-frame lag that grows with
    /// depth, appears only when the character moves fast, and is the kind of bug
    /// that gets blamed on the animation.
    std::size_t out_of_order = 0;

    /// Joints whose `parent` names an index that does not exist.
    std::size_t bad_parent = 0;

    /// Joints whose bind transform has a **zero scale on some axis**.
    ///
    /// A collapsed joint has no inverse, so `local_from_parent` hands back a
    /// projection instead (see `math/transform.hpp`), and a projection in a bind
    /// chain silently flattens every vertex below it. Reported here because it is
    /// exactly the case that stays finite and wrong — Lesson 7.5's rule — and
    /// because a zero-scale joint is nearly always an exporter accident rather
    /// than an intent.
    std::size_t singular_binds = 0;

    /// Joints whose bind transform scales **unevenly** across its axes.
    ///
    /// Not an error, and not counted by `ok()` — it is legal, it composes
    /// correctly, and `local_from_parent` inverts it exactly. It is reported
    /// because it is the one authored condition under which this engine's NORMAL
    /// rule is wrong, and wrong by a lot: `skin_directions` uses each palette
    /// matrix's linear part where the strictly correct transform is its inverse
    /// transpose, and those agree exactly for a rotation and for a uniform scale
    /// and diverge fast otherwise — **22.6 degrees of normal at a 1.5:1 stretch
    /// and 36.9 at 2:1**, measured, costing 0.083 and 0.134 of Lambert
    /// brightness respectively. A rig that trips this
    /// counter is a rig whose lighting will be wrong in a way no amount of
    /// looking at the silhouette will find. Lesson 7.6 §10.
    std::size_t nonuniform_binds = 0;

    /// `joint_from_model` is missing or the wrong length: `bake_inverse_binds`
    /// was never run, or was run before the joints were finished.
    bool unbaked = false;

    /// **The largest deviation from the identity of any skinning matrix at the
    /// bind pose** — the one number that tests a whole rig.
    ///
    /// Pose the skeleton at its own bind pose and every skinning matrix must come
    /// out as the identity, because its two factors are then exact inverses.
    /// Anything else means the inverse binds do not match the joints they were
    /// baked from. Float error through a chain of `n` matrices accumulates, so
    /// this is a small positive number rather than zero: measured at 4.77e-07 for
    /// a six-joint chain and 6.68e-06 at depth 32, so it grows with depth but
    /// stays five orders below anything a rig's own dimensions would notice.
    float worst_bind_residual = 0.0f;

    /// Nothing structural is wrong. The residual is deliberately NOT part of this
    /// — it is a measurement, and what counts as too much depends on the rig's
    /// scale in a way this file cannot know.
    [[nodiscard]] bool ok() const
    {
        return out_of_order == 0 && bad_parent == 0 && singular_binds == 0 && !unbaked;
    }
};

/// Check a skeleton's structure, and measure the bind-pose residual.
///
/// O(n) in the joint count plus one full palette build, so it is a load-time
/// tool and not a per-frame one. `bake_inverse_binds` must have run first, or
/// `unbaked` is set and the residual is left at zero.
[[nodiscard]] skeleton_report validate(const skeleton& sk);

/// Compute `joint_from_model` for every joint, from the joints' bind transforms.
///
/// **NO MATRIX IS INVERTED HERE**, which is the part worth reading the source
/// for. The obvious route builds each joint's bind world matrix by composing down
/// the chain and then inverts it — a general 4x4 inverse per joint, which is
/// forty-odd operations and can be numerically poor on a deep chain. Inverting a
/// product reverses it, so the inverse chain composes just as happily as the
/// forward one, in the opposite order:
///
///     model_from_joint[j] = model_from_joint[parent] * parent_from_local(bind[j])
///     joint_from_model[j] = local_from_parent(bind[j]) * joint_from_model[parent]
///
/// Each step uses the EXACT per-node inverse `math/transform.hpp` derives — a
/// transpose, three reciprocals and a negated offset — so the whole array is one
/// forward pass, parent before child, with the same cost as the forward one.
///
/// Requires the ordering precondition (`validate`). A skeleton whose parents come
/// after their children produces a plausible, wrong array here and no complaint;
/// that is what `skeleton_report::out_of_order` is for.
void bake_inverse_binds(skeleton& sk);

/// Fill `out` with the skeleton's own bind pose, ready to be edited into a pose.
///
/// One line, and it exists so that the concept has a name. A pose is just "a
/// local transform per joint" — the same type the skeleton stores — and the bind
/// pose is the particular pose the mesh was modelled in, which is why posing a
/// character at it must produce the character exactly as modelled.
void rest_pose(const skeleton& sk, std::vector<transform>& out);

/// Compose a local pose into **model-space joint matrices**, parents first.
///
/// `model_from_joint[j]` is where joint `j` is NOW: the matrix that carries
/// something expressed in joint `j`'s space out into the character's model space.
///
/// **This is the output you attach props with, and the palette is not.** A sword
/// in a hand socket wants `model_from_joint[hand]` — it is a rigid object with
/// its own vertices in its own space, exactly the case Lesson 5.9 already
/// handles. Multiplying a sword by a SKINNING matrix would be asking "where did
/// this model-space point move to", which is a question about the character's
/// skin and not about the sword.
///
/// One flat loop and no sorting, because of the ordering precondition. `out` is
/// resized to the joint count. A pose shorter than the skeleton is a programming
/// error and the missing joints fall back to their bind transforms, which keeps
/// a partially-filled pose renderable rather than undefined.
void compose_pose(const skeleton& sk, std::span<const transform> local_pose,
                  std::vector<mat4>& model_from_joint);

/// Turn posed joint matrices into the matrices a VERTEX is multiplied by.
///
///     palette[j] = model_from_joint[j] * sk.joint_from_model[j]
///
/// One matrix product per joint — a hundred of them for a character, against tens
/// of thousands of vertices — which is the asymmetry the whole design rests on.
/// The per-joint work is negligible and the per-vertex work is the frame, so
/// everything that CAN be hoisted to the joint is, and what remains per vertex is
/// four multiply-adds (`anim/skin.hpp`).
///
/// It is also exactly the array a GPU skinning path uploads: a uniform or storage
/// buffer of `palette.size()` matrices, with the vertex shader doing the same
/// weighted sum. Nothing above this line changes when the skinning moves to the
/// GPU, which is why the CPU version is not throwaway.
void build_palette(const skeleton& sk, std::span<const mat4> model_from_joint,
                   std::vector<mat4>& palette);

/// `compose_pose` then `build_palette`, with the intermediate array supplied by
/// the caller so that nothing allocates per frame.
///
/// The two are kept separate above because they answer different questions and
/// have different consumers; this is here because the common case wants both and
/// a call site that has to remember an order is a call site that will get it
/// wrong once.
void skinning_palette(const skeleton& sk, std::span<const transform> local_pose,
                      std::vector<mat4>& model_from_joint, std::vector<mat4>& palette);

}   // namespace engine::anim
