// engine/include/engine/gfx/renderable.hpp — what the ECS hands the renderer.
//
// A PROMISE MADE IN LESSON 5.1 AND KEPT HERE. `gfx/scene.hpp` opens by saying of
// `scene_object`: *"It is deliberately NOT the final answer. Module 5's ECS
// replaces the struct with components, and Module 6 pulls the surface fields out
// into a material."* Module 5 built the ECS in 5.8, the hierarchy in 5.9 — and
// never took the second step. `scene_object` is still a struct, and every program
// that wants to draw from a registry has written the same loop to build one.
//
// ---- AND IT IS ONLY THE SECOND CALLER, WHICH NEEDS AN ARGUMENT -------------
//
// The loop exists once today, in `ecs_swarm`'s `render_system` (5.8), and
// `demos/collector` was about to write the second. This engine's own stated rule
// — written twice in `gfx/scene.hpp` — is that *a type invented before three
// callers have asked for one is a guess*, so hoisting at two is a deviation and
// is owed a reason.
//
// The reason is that **what is duplicated is not an idiom, it is a known-broken
// workaround.** The last four lines of `render_system` convert a `mat4` back
// into a `transform` by putting the matrix's whole linear part into a field
// named `rotation`, which works only because that field is a general `mat3`.
// ecs_swarm's own comment says so and predicts its end: *"Module 6 gives the
// renderer a matrix directly and this function loses its last four lines."*
//
// Module 6 has now been written. `scene_object` still holds a `transform`
// (gfx/scene.hpp, line 104), so the prediction did not come true and the trick
// is still load-bearing. A stable idiom repeated twice is a coincidence; a
// workaround repeated twice is two places to fix on the day somebody finally
// changes `scene_object` — and one of them will be in a demo nobody thinks of
// as engine code.
//
// So: hoisted at two, against the rule, because the rule is about speculative
// TYPES and this is a known-temporary CONVERSION. `renderable` itself is not
// speculative: it is `scene_object` minus the placement, which is a struct that
// has existed since Lesson 3.1.
//
// ---- WHY THE ENGINE DEFINES A COMPONENT, WHICH IS NOT OBVIOUS ---------------
//
// Publishing a *system* is uncontroversial. Publishing a *component* is a real
// commitment: every program that wants this loop must now store its mesh handle
// in the engine's struct rather than its own, and an engine that publishes
// components is taking a position on how a game is laid out.
//
// The precedent is already set, and by this same module. `ecs/hierarchy.hpp`
// defines `parent` and `world_transform`, for exactly one reason: a system the
// ENGINE owns has to name the data it reads, or it cannot be compiled at all.
// `hierarchy::resolve` cannot be written against "whatever component you happen
// to call parent". Neither can this. The rule that falls out is narrow and worth
// keeping:
//
//     The engine defines a component when, and only when, an engine system
//     reads it. Everything else is the game's.
//
// So `renderable` is the engine's, and `collectible`, `rover` and `carousel` in
// demos/collector are not — nothing in `engine/` will ever look at them.
//
// ---- WHY IT LIVES IN gfx/ AND NOT ecs/ -------------------------------------
//
// Because of the direction of the arrow. A dependency should point towards the
// more general and the more stable, and the ECS is both: a registry has no
// opinion about meshes, and a great deal of simulation code wants entities and
// will never want a `mesh_handle`. Putting `renderable` under `ecs/` would make
// every program that includes the ECS compile `mesh.hpp`, `light.hpp` and their
// transitive closure — to store a component the simulation never reads.
//
// So graphics knows about entities; entities know nothing about graphics.
//
// ---- AND WHY THE REGISTRY IS FORWARD-DECLARED ------------------------------
//
// Lesson 5.11's rule: a header's include list is its interface. This one names
// `ecs::registry` in a signature and never dereferences it, so a FORWARD
// DECLARATION is enough — and the difference is not cosmetic. Including
// `ecs/registry.hpp` here would drag the pools, the views and the type-erasure
// machinery into every translation unit that so much as stores a `renderable`.
// Lesson 5.12 §3.2 measures where this engine's compile time actually lives,
// and it is the templated ECS headers: a translation unit that includes them
// costs 0.37 s against 0.01 s for an empty file. That is the bill every file
// which merely DECLARES a `renderable` would otherwise pay.
//
// The same trick `gpu_scene.hpp` uses for `instance_batch`, for the same reason.

#pragma once

#include <engine/gfx/light.hpp>
#include <engine/gfx/material.hpp>   // 6.5
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/scene.hpp>

#include <SDL3/SDL_stdinc.h>

#include <cstddef>
#include <vector>

// Named, not included — see the note above. `collect_renderables` takes a
// reference, and a reference to an incomplete type is legal C++.
namespace engine::ecs { class registry; }

namespace engine {

/// **"This entity has a shape and a surface."** The component half of Lesson
/// 5.1's `scene_object`.
///
/// Compare the two structs side by side and note what is missing here: the
/// `transform`. A `scene_object` carries where it is; a `renderable` does not,
/// because Lesson 5.9 already answered that question with `world_transform`, and
/// two components that both claim to say where something is would be a bug
/// waiting for its first disagreement. That is the whole of what the split buys
/// — each fact has exactly one home, and an entity is what it has.
///
/// **There is no `visible` flag, deliberately.** It is the first field anybody
/// adds, and Lesson 5.8 spent a demo arguing against it: twenty-four of that
/// swarm's entities are invisible *because they lack a `geometry` component*,
/// not because a bool hid them. `remove<renderable>(e)` is how you hide
/// something, it costs one structural change (~4 ns, measured in 5.7), and it
/// makes the invisible entity genuinely cheaper rather than merely skipped. A
/// bool would have put the branch back in the loop this header exists to remove.
///
/// **There is no `name`, for a different reason.** Naming an entity is a real
/// need and a separate concern — it wants to serve the editor, the log and the
/// save file, not just the renderer — and this engine's own stated rule is that
/// a type invented before three callers have asked for one is a guess
/// (`gfx/scene.hpp`, twice). One caller has asked. `collect_renderables` writes
/// a constant into `scene_object::name` and says so below.
struct renderable
{
    /// The geometry, by handle — Lesson 5.4. A stale handle resolves to null and
    /// the object is skipped, which is the property that makes it safe to unload
    /// a mesh out from under a live scene (Lesson 5.5).
    mesh_handle mesh;

    /// What the surface looks like — Lesson 6.5's `material`.
    ///
    /// **This is the one field Module 6 changed.** Lesson 5.12 shipped it as a
    /// loose `Uint32 tint` beside a `specular surface`, mirroring `scene_object`
    /// exactly; 6.5 folded both into a material, on this struct for the same
    /// reason it did on that one. By value, not by handle: a renderable has one
    /// material, and a handle would add a lookup to save nothing.
    material mat{};

    /// Is this a **closed surface**, so back-face culling is valid on it?
    /// Lesson 3.4. A ground plane is not; a cube is.
    bool closed = true;
};

/// What `collect_renderables` found.
///
/// The shape Lesson 5.3 settled on for an operation that cannot fail but has
/// diagnostics worth having: no status, just counts. Both numbers exist because
/// both describe a way an entity can be *absent from the picture while looking
/// perfectly correct in the registry*, which is the class of bug that costs an
/// afternoon.
struct renderable_report
{
    /// Entities written into `out`.
    std::size_t drawn = 0;

    /// Entities with a `renderable` and **no `world_transform`** — so the
    /// hierarchy has never resolved them and there is nowhere to put them.
    ///
    /// This is not a hypothetical. `hierarchy::resolve` deliberately never
    /// creates components (5.9), so an entity built with `add<renderable>` and
    /// without `add_hierarchy_components` is silently invisible. Before this
    /// counter existed the only symptom was "the orb is not there".
    std::size_t unresolved = 0;

    /// Entities whose `mesh` handle did not resolve — unloaded, or never loaded.
    /// Lesson 5.5 made this survivable; this makes it *countable*.
    std::size_t missing_mesh = 0;

    /// Entities whose world matrix carries **shear**, and whose placement in
    /// `out` is therefore an approximation rather than the matrix.
    ///
    /// **Lesson 7.5, and this counter exists because a type got narrower.** A
    /// `transform` is (position, rotation, scale) and a rotation is now a `quat`,
    /// so the set of matrices it can hold is exactly {rotate, then scale along the
    /// object's own axes, then translate}. Shear is outside it, and the hierarchy
    /// can produce shear from perfectly reasonable inputs: give a parent a
    /// non-uniform scale and rotate the child, and the product of the two
    /// matrices has columns that are no longer perpendicular.
    ///
    /// Nothing is dropped and nothing goes NaN — the object still draws, in the
    /// nearest placement the struct can express. What this counter buys is that
    /// "the crate looks subtly wrong and I cannot see why" becomes a number in a
    /// warning line. Before the swap the same scenes had the same defect and
    /// there was nothing to count, because `rotation` was a `mat3` and a `mat3`
    /// holds shear without comment.
    std::size_t skewed = 0;
};

/// Walk the registry and produce the `scene_object` list both renderers consume.
///
/// `out` is cleared and refilled. Pass the same vector every frame: it keeps its
/// capacity, so a steady-state frame allocates nothing — the same reason every
/// demo in this course owns its `std::vector<raster_triangle>` as a member.
///
/// **The conversion from `mat4` back to a `transform` used to be exact, and it
/// was a trick rather than a design.** Until Lesson 7.5 this function put the
/// matrix's whole linear part into `transform::rotation` and left `scale` at 1,
/// which reproduced any affine matrix bit for bit — because the field was *named*
/// `rotation` and *typed* `mat3`, and a `mat3` will hold anything, including the
/// scale that came down the hierarchy from a parent.
///
/// `rotation` is a `quat` now. A quaternion will not hold a scale, so that line
/// stopped compiling and the trick had to become the decomposition it was always
/// pretending to be: scale off as the three column lengths, sign of the
/// determinant handed back so a mirrored object stays mirrored, then extract.
/// `renderable.cpp`'s `placement_of` is the whole of it and carries the
/// derivation.
///
/// **One case is genuinely outside the struct now, and `skewed` counts it.** A
/// non-uniformly scaled parent with a rotated child produces a world matrix with
/// shear, which is not (rotation × scale) in any decomposition. The object still
/// draws, in the nearest placement a `transform` can express, and the count says
/// how many frames are being approximated. The honest fix is unchanged and
/// unmade: `scene_object` should hold a matrix, which is what Module 6's GPU path
/// already does with `gpu_draw_item::world_from_model`. Until the software
/// renderer follows, this function is where the seam is, and having exactly one
/// seam is most of the value of hoisting the loop here.
///
/// @param meshes the pool to resolve handles against — normally
///        `store.meshes()`. It is a parameter rather than something fetched
///        from the registry because an asset store is not ECS data and putting
///        one in the registry to shorten this signature would be the tail
///        wagging the dog.
///
/// @param world **takes a mutable reference, and reads nothing.** That is not a
///        style slip, it is a defect this function found and does not fix:
///        `registry::view()` has no `const` overload, and `ecs::view` hands out
///        `Ts&`, so there is no way to spell "I am going to walk this registry
///        and change nothing". Every read-only system in this engine — this one,
///        `hierarchy_debug_system`, anything Module 8's broadphase will want —
///        has to ask for write access it will not use, and `const` stops being
///        able to tell a reader anything.
///
///        The fix is the one EnTT reaches for: allow `view<const renderable>`
///        and propagate the constness into `each`'s parameters. It is a genuine
///        change to two published headers, so Lesson 5.12 reports it rather than
///        smuggling it in, and Exercise 4 is to make it. The signature is left
///        looking wrong ON PURPOSE — a comment nobody reads is a worse bug
///        report than a parameter everybody trips over.
[[nodiscard]] renderable_report collect_renderables(ecs::registry& world,
                                                    const mesh_pool& meshes,
                                                    std::vector<scene_object>& out);

}   // namespace engine
