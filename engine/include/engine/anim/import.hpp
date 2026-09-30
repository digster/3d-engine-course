// engine/include/engine/anim/import.hpp — a character, from a file somebody else wrote.
//
// Lesson 7.7b. Lessons 7.6 and 7.7 built every piece of a character player out
// of data they wrote themselves: a skeleton laid out joint by joint, a tube
// weighted by a formula, clips baked from functions. That was the right order to
// learn it in and it proves nothing about real content, because data you write
// yourself agrees with your code by construction. This file is where the engine
// meets data it did not write — a glTF 2.0 file from an exporter — and turns it
// into the three things 7.6 and 7.7 already know how to play:
//
//     gltf_scene_data (gfx/gltf.hpp)           imported_rig (this file)
//     ------------------------------           ------------------------
//     nodes, skins[k]              ------->    skeleton        (7.6)
//     primitives with JOINTS/WEIGHTS ----->    skinned_mesh[]  (7.6)
//     animations                   ------->    clip[]          (7.7)
//
// The parser in `gfx/gltf.cpp` reads bytes into descriptions and makes no
// decisions. Every decision is here, and there are four worth knowing about
// before reading the code, because each one is a place a real importer goes
// wrong on a real file.
//
// 1. THE SKELETON IS THE SKIN'S JOINTS AND EVERY NODE ABOVE THEM. A glTF joint
//    matrix is built from the joint's GLOBAL transform (spec §3.7.3.3: inverse
//    binds are applied "before the base node transforms"), and a global transform
//    includes every ancestor — joint or not. Blender's `Armature` node is not a
//    joint and is the parent of the hip; a file converted from another tool
//    commonly hangs a 0.01 scale or a 90-degree turn there. So the ancestors are
//    carried INTO the skeleton as joints no vertex is weighted to. Dropping them
//    — the obvious reading of "channels on non-joint nodes are dropped" — loses
//    that transform, and loses any animation on it.
//
// 2. PARENT-BEFORE-CHILD IS OURS TO ESTABLISH. The spec imposes no order on
//    `skins.joints`, `skeleton` requires one (7.6), and the sort is paid here,
//    once. It is STABLE — a skin that is already ordered keeps its order, which
//    is Blender's case, and `rig_import_report::resorted` says whether it moved
//    anything. The permutation it produces is applied to the inverse binds, to
//    every vertex's joint indices and to every clip track in the same pass,
//    because three arrays sorted by one permutation and a fourth by another is a
//    character animated by its neighbours.
//
// 3. THE FILE'S INVERSE BINDS WIN. 7.6 derived them from the bind pose and
//    called them derived data. In glTF they are PRIMARY data: the node
//    transforms are whatever pose the file happened to be saved in, and the
//    matrices are the bind. Where the two agree — Blender's case, measured —
//    nothing changes; where they disagree the file's matrices are the truth,
//    and `bind_vs_rest` says by how much. When the file gives no matrices at all
//    they are the IDENTITY, by the spec's own default, not baked from the pose.
//
// 4. THE FILE'S CURVES ARE NOT OUR CURVES, SO THEY ARE CONVERTED TO A TOLERANCE.
//    Our sampler (7.7) interpolates positions and scales linearly and rotations
//    by nlerp. A glTF sampler can be LINEAR — which for a rotation means SLERP
//    (spec §C.4) — STEP, or CUBICSPLINE. None of the three is what we do,
//    except LINEAR on positions and scales. So each file interval is split into
//    the fewest equal pieces for which OUR interpolation of the file's curve
//    stays within `import_settings::tolerance` of the file's curve itself, at
//    seven probes inside every piece; STEP is held with one extra key a single
//    float step before each change. The worst residual is reported, so "the
//    clip is what the file said" is a number and not a hope.
//
// A NOTE ON THE PROBES, because they are the difference between this working and
// this passing. nlerp and slerp agree EXACTLY at the midpoint of an arc — both are
// symmetric about it — and differ most near a quarter and three quarters of the
// way. A check at the midpoint alone would report zero error on every interval
// and never split anything. Lesson 7.7b measures that control.

#pragma once

#include <engine/anim/clip.hpp>
#include <engine/anim/skeleton.hpp>
#include <engine/anim/skin.hpp>
#include <engine/gfx/gltf.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine::anim {

/// How an import ended. `ok` is the only success.
enum class rig_status
{
    ok,
    no_such_skin,       ///< the skin index is not in the file
    empty_skin,         ///< the skin names no joints
    bad_joint,          ///< a joint names a node the file does not have
    too_many_joints,    ///< more than `k_max_joints`, joints and ancestors together
    no_meshes           ///< nothing in the file is skinned by this skin
};

[[nodiscard]] const char* name_of(rig_status s);

/// The choices a file does not make for you.
struct import_settings
{
    /// How closely OUR interpolation must follow the file's curve.
    ///
    /// **A tenth of `reduce`'s defaults** — 0.1 mm, 0.05°, 1e-4 of scale. The two
    /// errors CAN add, since `reduce` judges the importer's curve and not the
    /// file's, and the first argument for a tenth was a budget split between them.
    /// Measured (Lesson 7.7b, Exercise 5), they barely do: importing Blender's
    /// cubic walk at 0.005°, 0.05° and 0.25° and reducing at 0.5° ends 0.501°,
    /// 0.512° and 0.513° from an exact import. What a looser import costs is KEYS
    /// after reduction — 258, 276, 329 — because it leaves wiggles the reducer
    /// then has to follow. A tenth is where that saving had mostly arrived.
    reduction_limits tolerance{1.0e-4f, 8.72665e-4f, 1.0e-4f};

    /// The most pieces one file interval may be split into. A curve that still
    /// misses at this many is reported (`capped`) rather than split forever.
    int max_split = 64;

    /// glTF has no loop flag — the spec leaves "loops" to the client (§3.11) —
    /// so the importer has to pick one for every clip. True, because the clips
    /// this engine plays are cycles; a death animation wants false.
    bool loops = true;
};

/// What `import_rig` did, measured. Counts rather than a bool, like every
/// report in this engine.
struct rig_import_report
{
    rig_status status = rig_status::no_such_skin;

    // ---- The skeleton ------------------------------------------------------
    std::size_t skin_joints = 0;     ///< joints the skin names
    std::size_t joints = 0;          ///< joints in the skeleton: the skin's plus the ancestors
    std::size_t ancestors = 0;       ///< non-joint nodes above the joints, carried as joints

    /// Skin joints whose parent joint comes LATER in `skins.joints` — the joints a
    /// skeleton built in the file's order would have composed before their
    /// parents. Zero for Blender's exporter, which writes parents first.
    std::size_t file_order_breaks = 0;

    /// Skin joints whose rank among the skin's joints the sort changed. The sort
    /// is stable, so this is zero whenever `file_order_breaks` is.
    std::size_t resorted = 0;

    /// The file gave no `inverseBindMatrices`: the spec's identity was used.
    bool identity_inverse_binds = false;

    /// **How far the file's rest pose is from its bind pose** — `validate`'s
    /// `worst_bind_residual`, which with the file's own inverse binds in place
    /// measures exactly that. Float noise when the exporter wrote both from the
    /// same pose; large, and not an error, when the file was saved posed.
    float bind_vs_rest = 0.0f;

    /// The worst `out_of_square` among skeleton nodes that wrote a `matrix`. The
    /// spec says such a matrix "MUST be decomposable"; this is the check.
    float worst_out_of_square = 0.0f;

    skeleton_report skeleton{};      ///< `validate` of the result

    // ---- The meshes --------------------------------------------------------
    std::size_t meshes = 0;          ///< skinned primitives of this skin
    std::size_t vertices = 0;        ///< summed over them

    /// Vertices by how many non-zero influences they KEPT (0 to 4).
    std::size_t influences[k_max_influences + 1]{};

    /// Vertices the file gave MORE than four non-zero influences, and the largest
    /// single weight that keeping four threw away. Keep the four largest and
    /// renormalise is the standard import step (`anim/skin.hpp`), and its error is
    /// bounded by what was dropped — which is why that is the number reported.
    std::size_t truncated = 0;
    float worst_dropped = 0.0f;

    /// Non-zero weights naming a joint past the end of the skin, or negative
    /// weights: both MUST NOTs in the spec, both dropped, both counted.
    std::size_t bad_influences = 0;

    /// The largest `|sum − 1|` of any vertex's weights **as the file stored
    /// them**, before anything here renormalised. The spec's validator allows
    /// 2e-7 per non-zero weight for floats, and exactly zero for normalised
    /// integers — their sum "MUST be 255 or 65535".
    float worst_weight_sum = 0.0f;

    /// Vertices `normalise_weights` had to change: the truncated, and any the
    /// file got wrong.
    std::size_t repaired = 0;

    /// **Vertices no clip in the file can move** — every joint they are weighted
    /// to is one no channel animates, directly or through an ancestor.
    ///
    /// Not an error: a pack strapped to an unanimated root is frozen correctly.
    /// It is here because it is the number that found this lesson's asset bug —
    /// Blender's exporter gives any vertex bone heat failed to weight a joint it
    /// invents, `neutral_bone`, which nothing animates. Twenty-five of them, at
    /// the tip of a nose that stayed put whenever the head turned.
    std::size_t frozen_vertices = 0;

    // ---- The clips ---------------------------------------------------------
    std::size_t clips = 0;
    std::size_t channels = 0;        ///< every channel in every animation
    std::size_t bound = 0;           ///< …that animate a joint of this skeleton
    std::size_t unbound = 0;         ///< …that target another node, or none
    std::size_t morph = 0;           ///< …that animate morph weights (not played)

    std::size_t linear = 0;          ///< bound channels by the file's interpolation
    std::size_t step = 0;
    std::size_t cubic = 0;

    std::size_t keys_in = 0;         ///< keys in the bound channels, as the file had them
    std::size_t keys_out = 0;        ///< keys in the clips this produced
    std::size_t split_keys = 0;      ///< added so our interpolation follows the file's
    std::size_t step_keys = 0;       ///< added to hold a STEP until its next key
    std::size_t capped = 0;          ///< intervals still out of tolerance at `max_split`

    /// The worst distance between our interpolation and the file's curve, at the
    /// probes, after splitting: metres, radians, scale units.
    float worst_position = 0.0f;
    float worst_rotation = 0.0f;
    float worst_scale = 0.0f;

    std::size_t sign_flips = 0;      ///< `canonicalise_rotations`, over every clip
    std::size_t unnormalised_keys = 0; ///< rotation keys more than 1e-3 off unit, renormalised

    [[nodiscard]] bool ok() const { return status == rig_status::ok; }
};

/// A character, ready for 7.6 and 7.7 to play.
///
/// Owning and movable; four vectors and a skeleton, so the compiler's special
/// members are right and nothing here needs a destructor.
struct imported_rig
{
    skeleton sk;

    /// For each joint of `sk`, the glTF node it came from — how a caller gets from
    /// a joint back to the file (a socket named in the file, a debug label).
    std::vector<int> joint_node;

    /// One per skinned primitive of the skin, in file order.
    std::vector<skinned_mesh> meshes;

    /// For each mesh, its material as an index into `gltf_scene_data::materials`,
    /// or -1 for the spec's default material. Descriptions stay descriptions:
    /// making them into engine materials is the caller's, as it was in 6.6.
    std::vector<int> mesh_material;

    /// One per animation in the file that animates at least one of the joints,
    /// with tracks parallel to `sk.joints`.
    std::vector<clip> clips;

    void clear()
    {
        sk.clear();
        joint_node.clear();
        meshes.clear();
        mesh_material.clear();
        clips.clear();
    }
};

/// **Import one skin of a parsed glTF file**: its skeleton, the primitives it
/// skins, and every animation that moves it.
///
/// `skin` indexes `file.skins`. A file with two characters has two skins and is
/// imported twice; a file whose body and clothes are two skins over one armature
/// gives two skeletons with the same joints, which is correct and wasteful and
/// is the price of one skin, one rig.
///
/// `out` is cleared first and may hold partial data on failure. Load-time: every
/// step is linear in what it touches except the curve conversion, which tries 1, 2,
/// 3… pieces per interval, each try evaluating the file's curve at its `m + 1`
/// nodes and `7m` probes — `4m(m+1) + m` evaluations to settle on `m`, and nine for
/// an interval that needs no split, which is nearly all of them on sampled content.
[[nodiscard]] rig_import_report import_rig(const gltf_scene_data& file, int skin,
                                           const import_settings& settings, imported_rig& out);

}   // namespace engine::anim
