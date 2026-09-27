// engine/include/engine/anim/skin.hpp — one surface, moved by several joints.
//
// Lesson 7.6. `anim/skeleton.hpp` produces the PALETTE — one matrix per joint,
// each of them `model_from_model`. This file spends it, and the spending is four
// lines of arithmetic that have been the industry's default since the mid-90s:
//
//     v' = sum_i  w_i * palette[j_i] * v          LINEAR BLEND SKINNING
//
// It is also called *smooth skinning*, *vertex blending* and *matrix palette
// skinning*, all of which name the same four lines. The vocabulary around
// skinning is much larger than the mathematics under it, which is the single
// most useful thing to know before reading anybody else's implementation.
//
// TWO PRECONDITIONS, AND EACH ONE HAS A VISIBLE FAILURE.
//
//   THE WEIGHTS MUST SUM TO 1. Not as tidiness: a palette matrix is AFFINE, so
//   its translation is blended along with everything else, and a weighted sum of
//   affine maps whose weights sum to `s` is an affine map that also scales the
//   whole result toward the model ORIGIN by `s`. At the bind pose, where every
//   palette matrix is the identity, `sum w = 0.9` sends every vertex to 0.9 of
//   its distance from the origin: a character that is 10% smaller and, because
//   the origin is usually between the feet, sunk 10% into the floor. It reads as
//   a scale bug, and it is a weighting bug. `normalise_weights` is one call and
//   `validate` counts the vertices that need it.
//
//   THE SUM IS NOT A ROTATION, AND NOTHING CAN MAKE IT ONE. Two joints twisted
//   `theta` apart, weighted half and half, move a vertex to the average of two
//   rotated copies of it — and the average of two points on a circle `theta`
//   apart is INSIDE the circle, at `cos(theta/2)` of the radius. That is the
//   "candy wrapper": a forearm rotated 180 degrees against its elbow pinches to
//   a mathematical point, because `cos(90 deg) = 0`. Exactly zero. Lesson 7.6 §9
//   derives it, measures it at 8.7e-08 of the prediction over a sweep from 0 to
//   180 degrees, and then finds that the usual one-line summary — "this is nlerp
//   without the normalise" — is not quite true. LBS gets the SCHEDULE wrong too,
//   by nlerp's formula evaluated at the whole angle instead of the half angle,
//   which is strictly worse at every arc: 10.95 degrees of pose against nlerp's
//   2.23 at a 120-degree twist. What it does get right is the thing 7.5 spent
//   half a lesson on — a rotation matrix is unique, so there is no double cover,
//   so there is no long way round and no `nearest` to forget.
//
//   THE FIX EVERY RIGGER ALREADY KNOWS, with the number attached. Split the
//   twist across `n` segments and each pair carries `theta/n`, so the radius
//   goes to `cos(theta/2n)`: 0.000 at one segment, 0.707 at two, 0.924 at four.
//   That is what forearm twist bones ARE — not a modelling nicety but a direct
//   attack on a half-angle — and it is why rigs have more joints than a skeleton
//   does. Dual-quaternion skinning attacks the same cosine algebraically and is
//   named in §11 with a reference, not implemented here.

#pragma once

#include <engine/anim/skeleton.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/vec3.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace engine::anim {

/// How many joints may move one vertex.
///
/// **Four, which is a hardware number rather than an artistic one.** It is the
/// width of a GPU vector register, so four indices pack into one `uint8x4`/
/// `uint16x4` attribute and four weights into one `float4`, and the vertex
/// shader's blend is four multiply-adds with no loop and no branch. Engines that
/// allow eight do it as two such attributes and pay for it everywhere.
///
/// It is also enough. A vertex genuinely influenced by five joints is nearly
/// always a weighting mistake or a rig that wants another joint; the standard
/// import step is to keep the four largest weights and renormalise, and the
/// error that introduces is bounded by the fifth weight, which on real content
/// is a couple of percent.
inline constexpr int k_max_influences = 4;

/// Which joints move this vertex, and how much.
///
/// Twelve bytes at `joint_index` = 16 bits, one per vertex, parallel to the
/// mesh's positions — the same parallel-array arrangement `mesh_data` uses for
/// uvs and normals and for the same reason: geometry that is not skinned pays
/// nothing for the concept.
///
/// **A zero weight must still carry a VALID joint index**, and defaulting all
/// four to joint 0 is what makes that true by construction. A weight of zero
/// multiplies the matrix away, but the index is read before the weight is
/// applied, so garbage in an unused slot is an out-of-bounds read on the hot
/// path — the classic skinning crash, and it only fires on the one asset whose
/// exporter padded with 0xFFFF.
struct skin_influence
{
    joint_index joints[k_max_influences]{0, 0, 0, 0};
    float weights[k_max_influences]{1.0f, 0.0f, 0.0f, 0.0f};
};

/// A mesh as modelled, plus the weighting that makes it follow a skeleton.
///
/// `bind` is the geometry in MODEL space, exactly as the artist built it and
/// exactly as `gfx/mesh.hpp` stores any other mesh — it is not special, and that
/// is the point. What makes it skinnable is the second array.
struct skinned_mesh
{
    /// The mesh in its bind pose. **Never modified**: skinning reads it every
    /// frame and writes somewhere else, because deforming in place would destroy
    /// the only copy of the pose the weights are relative to. That is the most
    /// common first bug in a hand-written skinning path, and it presents as a
    /// character that melts a little more each frame.
    mesh_data bind;

    /// One per vertex of `bind`. A size mismatch is what `validate` catches.
    std::vector<skin_influence> influences;

    [[nodiscard]] std::size_t vertex_count() const { return bind.vertices.size(); }
};

/// What `validate` found about a weighting.
struct skin_report
{
    std::size_t vertices = 0;

    /// Vertices whose weights do not sum to 1 within `k_weight_tolerance`.
    std::size_t unnormalised = 0;

    /// Vertices naming a joint the palette does not contain, **with a non-zero
    /// weight**. Counted separately from `padded_indices` because only this one
    /// changes the picture; the other is a latent crash.
    std::size_t out_of_range = 0;

    /// Slots with a zero weight and an index outside the skeleton — harmless
    /// today, an out-of-bounds read the moment somebody optimises the weight test
    /// away.
    std::size_t padded_indices = 0;

    /// Vertices with a negative weight. Legal arithmetic, and never what anybody
    /// meant: it pulls a vertex AWAY from where a joint would put it.
    std::size_t negative = 0;

    /// Vertices with no influence at all — every weight zero. They collapse to
    /// the model origin, which is a very visible spike and a very confusing one,
    /// because the geometry is fine and the weighting is empty.
    std::size_t unweighted = 0;

    /// The weight sum furthest from 1 that was seen.
    float worst_weight_sum = 1.0f;

    /// `influences` and `bind.vertices` are different lengths.
    bool size_mismatch = false;

    [[nodiscard]] bool ok() const
    {
        return unnormalised == 0 && out_of_range == 0 && negative == 0
            && unweighted == 0 && !size_mismatch;
    }
};

/// How far a weight sum may stray from 1 before it is worth reporting.
///
/// Weights are commonly stored as normalised 8-bit integers in a file — 1/255
/// per step, so four of them can miss by up to 4/255 = 0.0157 before anyone has
/// done anything wrong. The tolerance sits just above that, and a sum outside it
/// means the weighting is wrong rather than merely quantised.
inline constexpr float k_weight_tolerance = 0.02f;

/// Check a weighting against the skeleton it claims to be for.
///
/// Load-time, O(vertices). `joint_count` is the palette's length rather than a
/// skeleton reference, because the question is about indices and a palette is
/// what the indices will be used against.
[[nodiscard]] skin_report validate(const skinned_mesh& m, std::size_t joint_count);

/// Scale each vertex's weights so they sum to 1, and clamp out-of-range joints
/// to zero weight. Returns how many vertices it had to change.
///
/// **An import-time repair, deliberately not done inside the skinning loop.** It
/// is one divide per vertex; doing it per frame would be paying, forever, for a
/// property the data could simply have. A vertex with no weight at all is given
/// full weight on its first joint, which keeps it attached to something instead
/// of collapsing it to the origin — and `validate` has already counted it, so the
/// repair is not silent.
std::size_t normalise_weights(skinned_mesh& m, std::size_t joint_count);

/// Linear blend skinning, positions only.
///
/// `out` must be at least as long as `bind`. Positions are treated as POINTS —
/// the translation column applies — which is the whole difference from
/// `skin_directions` below and is Lesson 2.7's `w` doing its job.
void skin_positions(std::span<const vec3> bind,
                    std::span<const skin_influence> influences,
                    std::span<const mat4> palette,
                    std::span<vec3> out);

/// Linear blend skinning for NORMALS (and tangents, and any other direction).
///
/// **A direction is not a point, so the translation must not apply** — the fourth
/// component is 0 and only the linear part of each palette matrix is used. Skip
/// that and every normal is offset by wherever the joint happens to be standing,
/// which is a lighting bug that moves when the character walks.
///
/// **What this gets slightly wrong, stated rather than hidden.** The strictly
/// correct transform for a normal is the INVERSE TRANSPOSE of the linear part,
/// not the linear part itself (Lesson 3.6). They agree exactly for a rotation,
/// because `R` is orthonormal and its inverse transpose is `R` — and joints are
/// rotations in every rig that is not doing something exotic. They agree up to a
/// scale factor for a uniform scale, which the renormalise below removes. They
/// disagree for NON-UNIFORM joint scale, where a stretched joint tilts its
/// normals — and not gently. Lesson 7.6 §10 measures **22.6 degrees** of normal
/// error at a 1.5:1 stretch and 36.9 at 2:1, costing 0.083 and 0.134 of Lambert
/// brightness. The engine's rule stands anyway, and the justification is NOT
/// "the error is small": it is that non-uniform scale on a skinning joint is a
/// rig defect rather than a renderer's problem to absorb.
/// `skeleton_report::nonuniform_binds` counts the authored half of it at load
/// time, which is the half a check can see; §12's fourth exercise builds the
/// second palette that would fix the other half and measures what it costs.
///
/// The result is renormalised, because a weighted sum of unit vectors is not one.
void skin_directions(std::span<const vec3> bind,
                     std::span<const skin_influence> influences,
                     std::span<const mat4> palette,
                     std::span<vec3> out);

/// Deform a whole skinned mesh into `out`, ready to draw.
///
/// **ONLY POSITIONS AND NORMALS CHANGE.** Uvs, tangents and indices are copied
/// exactly once, when `out` is the wrong shape, and never again — a texture
/// coordinate does not care where the elbow is, and a triangle list does not
/// change when a character moves. That is not a micro-optimisation, it is the
/// observation the GPU path is built on: the static arrays live in a vertex
/// buffer that is uploaded once and the pose arrives as a hundred matrices, so
/// animating a 50,000-vertex character costs 6 KB of bus traffic per frame rather
/// than 1.2 MB.
///
/// Tangents are deliberately NOT skinned here even though they are directions:
/// nothing in Module 7 draws a normal-mapped skinned mesh, and a transformed
/// tangent that no shader reads is work done to be wrong quietly. §12's third
/// exercise is to add it, which is one more `skin_directions` call and a
/// re-orthogonalisation.
void skin_into(const skinned_mesh& src, std::span<const mat4> palette, mesh_data& out);

}   // namespace engine::anim
