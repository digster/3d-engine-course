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

    /// How to read that image. Independent of *which* image, which is why it is a
    /// separate field and why `SDL_GPUTextureSamplerBinding` is a pair (3.9).
    sampler samp{};

    /// Roughness, metallic, F0 — Lesson 6.4's `microsurface`.
    microsurface surface{};

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
