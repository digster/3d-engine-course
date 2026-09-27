// engine/include/engine/gfx/draw_order.hpp — the first thing this engine has to decide BEFORE it draws.
//
// Lesson 6.11, and the reason this file exists is one sentence long:
//
//   **The z-buffer is a sort, and it only sorts things that stop the light.**
//
// Lesson 3.1 put it exactly that way and it has been true for eight lessons.
// Opaque geometry has never needed ordering, in any of the demos, in either
// renderer, because the depth test resolves visibility per PIXEL and a per-pixel
// answer does not care what order the pixels arrived in. That is a remarkably
// strong property and it is easy to stop noticing you have it.
//
// Blending takes it away. `over` is not commutative — red over blue is not blue
// over red — so the moment two transparent surfaces overlap, the order they were
// drawn in is part of the answer. There is no per-pixel structure that fixes
// this, because the information needed (every fragment along the ray, sorted) is
// exactly what a depth buffer throws away in order to be one number deep.
//
// So the engine acquires a per-frame sort. Three things about it are worth
// having in mind before reading the code:
//
//   IT IS PER OBJECT, AND THAT IS NOT ENOUGH. Sorting whole draws is correct
//              when transparent objects do not interpenetrate and is wrong when
//              they do — and no ordering of two intersecting quads is right,
//              which the lesson's §6 shows rather than asserts. This is the
//              ninety-percent picture; the missing ten percent is order-
//              independent transparency, named in `blend.hpp` and not built.
//   IT IS AXIAL DEPTH, NOT DISTANCE. `dot(p - eye, forward)`, the same quantity
//              Lesson 6.9 chose for cascade selection and for the same reason:
//              `length(p - eye)` is up to 22% larger at the corner of a
//              60-degree frame, which would sort a wide-angle frame by how far
//              things are from the CAMERA rather than by how far they are ALONG
//              the view — and reorder objects that are side by side.
//   MASKED GEOMETRY DOES NOT BELONG IN IT. A discarded fragment writes no depth
//              and no colour, so it occludes nothing and no order can be wrong.
//              Masked draws go with the opaque ones. Sorting them would be a
//              per-frame sort over every leaf of every tree in the scene, in
//              exchange for nothing at all — and it is a genuinely common
//              mistake, because "transparent" reads like one category.
//
// Lesson 6.16's frustum culling wants this same traversal — walk the draw list,
// compute a per-object quantity, partition — which is why the bucketing below is
// a general `span` operation over indices rather than something wired into
// either renderer.

#pragma once

#include <engine/gfx/blend.hpp>
#include <engine/math/vec3.hpp>

#include <span>

namespace engine {

/// How far along the view axis a point is. **Not** how far away it is.
///
/// @param world    the point, in world space — a transparent object's centre
/// @param eye      the camera's position
/// @param forward  the camera's forward axis, unit length (course conventions
///                 §2: the camera looks down its own −Z)
///
/// Negative for anything behind the camera, which is a useful answer rather than
/// an error: it sorts to the far end of a back-to-front list, which is where
/// something behind you belongs.
[[nodiscard]] inline float view_depth(vec3 world, vec3 eye, vec3 forward)
{
    return dot(world - eye, forward);
}

/// One entry in a frame's draw list, as the sort sees it.
///
/// **It carries an INDEX, not a draw.** The sort permutes these sixteen-byte
/// records and the caller then walks its own array in the order they name, which
/// keeps the sort's memory traffic proportional to the number of objects rather
/// than to the size of whatever a draw happens to be — `gpu_draw_item` is 140
/// bytes, and moving those around to reorder them would be sorting the wrong
/// thing. It is the same argument Lesson 5.6 made about the ECS: sort keys, not
/// payloads.
struct draw_key
{
    /// Where this draw lives in the caller's array. Untouched by the sort except
    /// by being moved.
    int index = 0;

    /// Axial view depth of the object's centre — `view_depth` above. Only read
    /// for `alpha_mode::blend` entries; the others keep whatever the caller put
    /// here, and the caller is free not to compute it for them.
    float depth = 0.0f;

    /// Which bucket. The whole reason this is on the key rather than looked up
    /// through the index is that the partition is the hot half of this operation
    /// and a pointer chase per comparison would dominate it.
    alpha_mode mode = alpha_mode::opaque;
};

/// What ordering the frame's draw list needed. Counters, not correctness.
///
/// The same shape as `draw_stats::ideal_pipeline_binds` (4.8) and for the same
/// purpose: **make the policy measurable instead of arguable.** A frame whose
/// `out_of_order` is zero paid for a sort it did not need, and a frame with three
/// hundred blended draws is telling you something about the scene rather than
/// about the sort.
struct order_report
{
    int opaque = 0;   ///< entries drawn in the first pass, unsorted
    int masked = 0;   ///< …of which were alpha-tested. Counted separately, drawn together.
    int blended = 0;  ///< entries in the sorted tail

    /// Adjacent pairs of blended entries that were in the WRONG order when they
    /// arrived — measured before the sort runs.
    ///
    /// **A lower bound on the work, not the work**, and deliberately so: it is
    /// one pass over the tail rather than the O(n²) inversion count, and a single
    /// object in the wrong place can produce one adjacent inversion while needing
    /// n swaps. What it answers exactly is "did this frame's draw list arrive
    /// already sorted", which is the question worth asking every frame — a scene
    /// whose objects barely move usually answers yes, and that is when a
    /// re-sortable cached order starts to look attractive.
    int out_of_order = 0;
};

/// Partition into `[opaque and masked][blended, back to front]`.
///
/// **Two operations, and only the second one is a sort.** The partition is
/// stable, so opaque draws keep whatever order the caller gave them — which
/// matters, because that order is usually the one Lesson 4.8 sorted by pipeline,
/// and undoing it to make room for transparency would trade three pipeline binds
/// for one per draw.
///
/// The tail is sorted by **descending** depth: farthest first, because `over`
/// composites the source on top of what is already there, so what is already
/// there has to be everything further away. Getting this backwards produces a
/// picture that is subtly, consistently wrong — the near pane of glass tinting
/// the far one instead of the other way round — and it is easy not to notice
/// with only two objects, which is why `verify_611` §F checks the composite
/// numerically rather than by eye.
///
/// **`std::stable_sort` and not `std::sort`**, for a reason that shows up as
/// flicker rather than as a wrong picture: two transparent objects at exactly the
/// same depth (a decal on a pane, two leaves on one branch) have no correct
/// order, and an unstable sort is free to pick a different one every frame as
/// their depths wobble in the last bit. Stability makes "no correct order" at
/// least mean "the same order as last frame".
///
/// @param keys  modified in place; every entry survives, none is dropped
order_report order_draws(std::span<draw_key> keys);

} // namespace engine
