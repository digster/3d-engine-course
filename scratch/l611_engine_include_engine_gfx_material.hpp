// engine/include/engine/gfx/material.hpp — what a surface IS.
//
// Lesson 6.5, and this file is the answer to a question the codebase has been
// asking since Lesson 3.4. It is worth reading the evidence before the type,
// because the type is unremarkable and the evidence is the lesson.
//
// THREE PLACES GREW THE SAME STRUCT-SHAPED HOLE, INDEPENDENTLY.
//
//   1. `scene_object` (3.4, 3.7) collected `tint`, `closed` and `surface`
//      alongside `xform` and `geometry`, and its own doc comment says so: "that
//      is two different kinds of data wearing one struct, and the second kind has
//      a name: a material. Module 6 gives it one."
//   2. `fill_style` (3.8) collected `lights`, `surface`, `eye` and `albedo`
//      beside the four rasterizer knobs, and its own comment says "`fill_style`
//      is now two structs wearing one name."
//   3. `ecs_swarm` — a DEMO, restricted to the public API — declared
//      `struct material { Uint32 tint; microsurface surface; }` because the
//      engine did not offer one. A demo that has to invent an engine type is the
//      strongest evidence available that the type is missing.
//
// AND LESSON 6.4 PRICED IT. Changing the surface description meant editing
// forty-four call sites across the engine, four demos and six harnesses. That
// number is not a complaint about 6.4; it is the measurement that says the
// description had no single home.
//
// THE SPLIT THAT MATTERS IS NOT struct-VERSUS-struct. It is:
//
//     WHICH HALF CAN BE A NUMBER IN A BUFFER?
//
// Everything in `material` below can. It is per-draw data: the fragment stage
// reads it, a GPU pushes it as a uniform block, and two objects that differ only
// in these fields can be drawn back to back with no state change between them.
// Cull mode, fill mode and the choice of BRDF cannot — they are baked into a
// PIPELINE OBJECT, and two objects that differ in them need two pipelines and a
// sort between them. Lesson 4.8 measured that: three pipelines, and the sort
// that keeps the bind count at three instead of one per draw.
//
// So this file draws the line where the hardware draws it, and the things on the
// other side of it stay where they are — in `fill_style`, `surface_style` and
// `render_options`. A material that swallowed them would be a material that
// cannot be a uniform, which is not a material at all.
//
// AND ONE THING TURNED OUT TO BE ON NEITHER SIDE. `scene_object::closed` says
// "this mesh is a closed solid", which reads like material state and is not: it
// is a fact about the GEOMETRY, true of a cube whatever colour you paint it.
// `validate()` already reports it, as `mesh_report::closed()`. See `cull_of`.

#pragma once

#include <engine/core/handle.hpp>
#include <engine/core/pool.hpp>
#include <engine/gfx/blend.hpp>    // 6.11: alpha_mode, k_default_alpha_cutoff
#include <engine/gfx/colour.hpp>
#include <engine/gfx/cull.hpp>
#include <engine/gfx/microfacet.hpp>
#include <engine/gfx/texture.hpp>

#include <SDL3/SDL.h>

namespace engine {

/// **Everything about a surface that can be a number in a buffer.**
///
/// Five fields, and every one of them is read by the fragment stage and by
/// nothing else. That is the test this struct is built to pass, and it is worth
/// applying to anything you are tempted to add: *could this be pushed as a
/// uniform, or does it change which pipeline runs?*
///
/// **It is a VALUE, and small.** 36 bytes as measured — a `Uint32`, a 4-byte
/// handle, a 16-byte `sampler` and three floats — so it is copied rather than
/// referenced wherever there is one of them. `material_handle` exists for where
/// there are many; see the note on `material_pool`.
///
/// (The sampler is the largest field, and it is worth knowing that is where the
/// bytes went: it is four enums that a real engine would intern into a handle of
/// its own, exactly as this lesson does for the texture. Named rather than done,
/// because nothing has yet needed two materials to share a sampler.)
struct material
{
    /// The surface's own colour, **sRGB-encoded**, as an artist types it.
    ///
    /// It stays a `Uint32` rather than becoming a `linear_rgb`, and the reason is
    /// Lesson 6.1's rule rather than laziness: *decode once at the input edge*.
    /// A hex colour is authored data, it arrives sRGB-encoded, and the decode
    /// belongs at the point where it meets arithmetic — which is `shade_encoded`
    /// on the CPU side and the uniform push on the GPU side. Storing it decoded
    /// would move the conversion earlier without removing it, and would quietly
    /// widen an 8-bit authored value into a float that looks more precise than
    /// the thing it came from.
    Uint32 tint = 0xFFFFFFFFu;

    /// The albedo image, or an invalid handle for "this surface has none".
    ///
    /// **A HANDLE, not a `const texture*`, and that is Lesson 5.4 collecting.** A
    /// material outlives the frame it was used in; a pointer into a container
    /// that can reallocate does not. A handle is checkable — `pool::contains()`
    /// answers "does this still resolve?" and a raw pointer cannot be asked.
    ///
    /// **When it is valid it REPLACES `tint`; it does not multiply it** — Lesson
    /// 3.9's rule, and the reason is that both of them *are* the albedo and a
    /// surface has one albedo. That is why there is no separate "textured" flag
    /// here: see `textured()`.
    texture_handle albedo_map{};

    /// The **normal map**, or an invalid handle for "this surface is as flat as
    /// its triangles". Lesson 6.7.
    ///
    /// A second texture handle, and it passes 6.5's membership test for exactly
    /// the same reason the first one does: it is a number in a buffer — an index
    /// the fragment stage reads, not a decision about which pipeline runs.
    ///
    /// **The image behind it must have `texel_space::linear`**, and that is not
    /// a convention this field can enforce. A normal map is a direction packed
    /// into bytes, not a colour, so reading it through the sRGB curve is
    /// arithmetic on numbers that were never a colour — see `texel_space`.
    /// `asset_store::load_texture` takes the space as a parameter and the glTF
    /// importer supplies it from which slot the texture was bound to, so the
    /// engine's own paths cannot get it wrong; a caller building a material by
    /// hand can, which is why `verify_67` §C asserts it on every shipped path.
    texture_handle normal_map{};

    /// How to read those images. Independent of *which* image, which is why it is
    /// a separate field and why `SDL_GPUTextureSamplerBinding` is a pair (3.9).
    ///
    /// **ONE sampler for both maps**, and that is a compromise named rather than
    /// hidden. glTF gives every texture its own sampler, and a material whose
    /// albedo tiles while its normal map clamps is legal and unrepresentable
    /// here. In practice an exporter emits the same sampler for both, because
    /// they are baked against the same uv chart; and the right fix is the one
    /// 6.5 already named as a debt — intern the sampler into a handle of its own,
    /// at which point a second one costs four bytes instead of sixteen.
    sampler samp{};

    /// Roughness, metallic, F0 — Lesson 6.4's `microsurface`.
    microsurface surface{};

    // ---- Lesson 6.11: transparency ----------------------------------------
    //
    // THREE FIELDS, AND THEY TEST 6.5'S OWN LINE. This file opens by asking
    // "which half can be a number in a buffer?" and putting everything that can
    // on this struct. Two of the three below can. The first cannot — and it is
    // here anyway, on purpose, which needs justifying rather than glossing.

    /// Opaque, alpha-tested, or blended. **The one field on this struct that is
    /// not a number in a buffer**, and the precedent for keeping it here is four
    /// declarations further down: `cull_of`.
    ///
    /// The distinction 6.5 drew was between per-draw DATA and pipeline STATE. A
    /// blend mode is pipeline state — `SDL_GPUColorTargetBlendState` lives inside
    /// the create-info, and two objects that differ in it need two pipelines and
    /// a sort between them. So by that rule this field belongs in `fill_style`
    /// and `surface_style`, not here.
    ///
    /// But `cull_of` already settled the shape of this argument, about `closed`:
    /// **what belongs to the material is the INTENT, not the state.** A window
    /// *is* transparent — that is a fact about the surface, true whichever
    /// renderer draws it and true in a file on disk with no pipeline anywhere
    /// near it. What the renderer does about it (which pipeline, which pass,
    /// which sort) is derived at draw time, in one place, from this. The GPU path
    /// derives a `blend_style`, the software path derives a `fill_style`, and
    /// neither of them is where an artist's decision should have been stored.
    ///
    /// **Defaults to `opaque`, and that is what keeps this lesson an addition
    /// rather than a re-baseline.** Every material written in the previous
    /// sixty-five lessons means precisely what it meant.
    alpha_mode mode = alpha_mode::opaque;

    /// The surface's own opacity, **linear and independent of `tint`**.
    ///
    /// glTF calls it `baseColorFactor[3]`, and Lesson 6.6 has been importing it
    /// into `gltf_material_desc::alpha` and dropping it on the floor ever since,
    /// because there was nothing here to put it in. There is now.
    ///
    /// **It multiplies the albedo texture's alpha; it does not replace it.** Note
    /// that this is the opposite of the rule `tint` and `albedo_map` follow —
    /// there, the texture REPLACES the factor, because both of them *are* the
    /// albedo and a surface has one albedo (3.9). Here they compose, because they
    /// are two different statements: the image says which *parts* of the surface
    /// are there, and this says how transparent the *whole* surface is. A glass
    /// pane with a decal needs both. glTF specifies the multiply, and it is the
    /// only reading that lets a fade-out animate an existing material.
    ///
    /// Ignored entirely under `alpha_mode::opaque`.
    float alpha = 1.0f;

    /// Under `alpha_mode::mask`, the coverage at which a fragment starts to
    /// exist. glTF's `alphaCutoff`, default 0.5.
    ///
    /// **A number in a buffer, unambiguously** — the fragment stage reads it and
    /// nothing else does, which is 6.5's test passed on the first try. It is why
    /// masking needs no pipeline of its own in this engine (see `alpha_mode`).
    float alpha_cutoff = k_default_alpha_cutoff;

    /// Does the albedo come from the image rather than from `tint`?
    ///
    /// **DERIVED, never stored**, and that is the point of writing it as a
    /// function. Lesson 4.8's `material_uniforms` carries a `textured` float
    /// beside the texture pointer, which is two fields that can disagree — and a
    /// material with `textured = 1` and no image bound draws the sampler's debug
    /// magenta, while one with `textured = 0` and an image bound silently ignores
    /// it. Neither is diagnosable at the point of the mistake.
    ///
    /// One field cannot contradict itself. The GPU still needs the flag as a
    /// number, and `material_uniforms` still has it — but it is now *computed* at
    /// the push, from the only field that knows.
    [[nodiscard]] bool textured() const { return albedo_map.valid(); }

    /// Does this surface perturb its normal per pixel? **Derived**, for the same
    /// reason `textured()` is: one field cannot contradict itself, and a
    /// `normal_mapped` bool beside a separately-chosen handle is two opinions
    /// that can disagree (6.5 §5).
    [[nodiscard]] bool normal_mapped() const { return normal_map.valid(); }

    /// Does this surface need the sorted, depth-write-disabled pass?
    ///
    /// **Derived, and deliberately NOT "is it see-through"** — masked geometry is
    /// see-through and belongs with the opaque draws, because a discarded
    /// fragment writes no depth and therefore occludes nothing, so no order can
    /// be wrong. Only `blend` makes draw order part of the answer, and only
    /// `blend` is what a renderer must sort for. Confusing the two costs a
    /// per-frame sort over every leaf in the scene for no benefit whatsoever.
    [[nodiscard]] bool needs_sorting() const { return mode == alpha_mode::blend; }
};

/// A reference to a material held in a `material_pool`.
using material_handle = handle<material>;

/// Storage for materials, for when many objects share one.
///
/// **A pool is not automatically the right answer, and it is worth saying when it
/// is.** With one material per object, a `material` value on the object is
/// simpler, cheaper and cannot dangle — which is why `scene_object` holds one by
/// value. The pool earns its place the moment objects SHARE: `ecs_swarm` gives
/// two hundred drones the same appearance, and a handle makes that one material
/// instead of two hundred copies of it (§6.5 measures what that saves).
///
/// It is the same judgement Lesson 5.4 made about meshes and 5.5 about images,
/// arriving a third time — and the third time it is worth noticing that the rule
/// is about *sharing*, not about size.
using material_pool = pool<material>;

/// Resolve a material's texture reference into something the fill loop can read.
///
/// **This function is the whole handle/pointer story in four lines, and the
/// separation it makes is the architectural point of this lesson:**
///
///   - a **handle** is how you STORE a reference — durable, checkable, safe
///     across a reallocation, and cheap to copy into a component or a file;
///   - a **pointer** is how you USE one in an inner loop — a dereference, no
///     lookup, no branch;
///   - and the **resolve** is a named step that happens ONCE PER DRAW, between
///     the two.
///
/// Getting that boundary wrong in either direction is a real cost. Store
/// pointers and the scene dangles the first time a pool grows. Resolve inside the
/// fill loop and you have added a bounds check and a generation compare to every
/// pixel — Lesson 5.4 measured what a handle lookup costs, and per-pixel is
/// exactly where you cannot afford it.
///
/// Returns an unbound binding for an invalid or stale handle, which is not an
/// error: it is a surface with no image, and the fill falls back to `tint`
/// exactly as it did before this lesson existed.
[[nodiscard]] inline texture_binding bind_albedo(const material& m,
                                                 const texture_pool& textures)
{
    return {textures.get(m.albedo_map), m.samp};
}

/// The same binding, with a mip chain attached. Lesson 6.10.
///
/// A separate function rather than a defaulted argument, because the chain does
/// not live in the `texture_pool` — it is built by whoever wanted mipmapping and
/// owned by them. Making `bind_albedo` take it would force every existing caller
/// to pass a null, which is the sort of change that makes nineteen lessons of
/// listings stop compiling to add one feature none of them use.
[[nodiscard]] inline texture_binding bind_albedo_mipped(const material& m,
                                                        const texture_pool& textures,
                                                        const mip_chain* chain)
{
    texture_binding b{textures.get(m.albedo_map), m.samp};
    b.mips = chain;
    return b;
}

/// The same resolve, for the normal map — Lesson 6.7.
///
/// A second function rather than a second field on one binding, because the two
/// images are independent: a surface may have an albedo and no normal map, a
/// normal map and no albedo, both, or neither, and all four are ordinary. The
/// fill loop reads whichever bindings are bound and falls back where they are
/// not.
///
/// Note that it resolves against the same pool and with the same sampler, so the
/// per-draw cost is one extra pointer lookup and nothing else.
[[nodiscard]] inline texture_binding bind_normal_map(const material& m,
                                                     const texture_pool& textures)
{
    return {textures.get(m.normal_map), m.samp};
}

/// The cull mode a mesh's own geometry justifies.
///
/// **`closed` was never material state, and this is where that gets fixed.**
/// `scene_object::closed` has carried a hand-typed bool since Lesson 3.4, and its
/// doc comment predicted it would move onto the material in Module 6. It should
/// not: it is a fact about the *mesh* — a cube is closed whatever colour you
/// paint it — and `validate()` already reports it, as `mesh_report::closed()`, from the geometry
/// rather than taking anyone's word.
///
/// What belongs to the material is the DECISION, not the fact. Back-face culling
/// is only valid on a closed surface, so a sheet must not be culled; but a closed
/// mesh may still be drawn two-sided deliberately (to see inside it, to debug a
/// winding). Hence: the mesh supplies the fact, the caller supplies the intent,
/// and this function is the one place the rule lives.
///
/// Note what that means for the demo's `closed` flags. They stop being a promise
/// somebody typed and become a cached answer — which is exactly the transition
/// `demos/common/demo_scene.cpp` already made for the loaded model in Lesson 5.4,
/// and now makes everywhere.
[[nodiscard]] constexpr cull_mode cull_of(bool mesh_is_closed, bool want_two_sided = false)
{
    return (mesh_is_closed && !want_two_sided) ? cull_mode::back : cull_mode::none;
}

} // namespace engine
