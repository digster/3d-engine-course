// engine/include/engine/gfx/scene.hpp — a thing in the world, and where it is.
//
// The smallest description of something worth drawing: a placement, a shape, and
// the handful of surface parameters the shading needs. Lesson 5.1 lifted it out
// of the demo, and the argument for doing so is the one that decides most of
// these questions: BOTH renderers consume it. The software rasterizer walks a
// list of these; so does the GPU path, after a translation step. A type that two
// renderers agree on is the engine's, whoever typed it first.
//
// It is deliberately NOT the final answer. Module 5's ECS replaces the struct
// with components, and Module 6 pulls the surface fields out into a material.
// Both of those are named in the comments below where the seams already show.

#pragma once

#include <engine/gfx/light.hpp>
#include <engine/gfx/material.hpp>   // 6.5
#include <engine/gfx/mesh.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>

#include <SDL3/SDL_stdinc.h>

namespace engine {

// ---------------------------------------------------------------------------
// Lesson 2.8 — model space, world space, and the order of T, R and S
// ---------------------------------------------------------------------------

/// Which order the model matrix is composed in. [O] cycles.
///
/// Only the first is right. The other two are here because they are the two
/// mistakes people actually make, and because each one fails in a specific,
/// nameable way that is far more instructive than being told the correct answer.
enum class trs_order
{
    trs,   ///< T * R * S — scale, then rotate, then translate. Correct.
    tsr,   ///< T * S * R — rotate first, then scale along WORLD axes. Deforms.
    rts    ///< R * T * S — translate before rotating. Orbits the world origin.
};

[[nodiscard]] inline const char* name_of(trs_order o)
{
    switch (o)
    {
    case trs_order::trs: return "T * R * S   (correct)";
    case trs_order::tsr: return "T * S * R   (scale last)";
    case trs_order::rts: return "R * T * S   (rotate last)";
    }
    return "?";
}


/// Build a model matrix in a chosen — possibly wrong — order.
///
/// `parent_from_local()` in transform.hpp always builds T*R*S, because that is
/// the only order an engine should ever offer. This function exists purely so the
/// demo can put the wrong answers on screen next to the right one; it is the same
/// bargain struck for `draw_line_naive` (2.1), Pong's unswept collision (1.8),
/// `blend_space::encoded` (2.4) and the `w` toggles (2.7). A failure you can
/// summon on a keypress teaches more than a paragraph describing it.
[[nodiscard]] inline mat4 model_matrix(const transform& t, trs_order order)
{
    if (order == trs_order::trs) { return parent_from_local(t); }

    const mat4 T = translation(t.position);
    const mat4 R = to_mat4(t.rotation);
    const mat4 S = to_mat4(scale(t.scale.x, t.scale.y, t.scale.z));

    // Spelled out as three separate 4x4 factors rather than folded, so the source
    // reads in the same order as the name printed on screen.
    if (order == trs_order::tsr) { return T * S * R; }
    return R * T * S;
}

/// One thing in the world: where it is, and what shape it is.
///
/// A transform plus a mesh. That pairing is the smallest useful definition of a
/// renderable object, and it is deliberately kept OUT of `engine::transform` — a
/// transform is a placement, not a thing, and Module 5's ECS will attach geometry
/// to a transform as a separate component for exactly this reason. Here it is a
/// two-field demo struct, which is the honest amount of machinery for three objects.
///
/// **Lesson 5.4 changed one word here and it is the most consequential word in
/// the struct.** `geometry` used to be a `mesh` — three pointers and three
/// lengths, borrowed from whoever happened to own the arrays, valid for exactly
/// as long as that owner said nothing and moved nothing. It is now a
/// `mesh_handle`: four bytes, owning nothing, borrowing nothing, and answerable.
/// A `scene_object` can now be copied, stored, serialized and outlive its
/// geometry, and every one of those is a thing it could not do yesterday.
///
/// The cost is real and is named rather than hidden: a handle is meaningless
/// without its pool, so everything that draws a `scene_object` now needs the
/// `mesh_pool` too. `collect_triangles` grew a parameter for it. Module 5's ECS
/// eventually folds the pools into a `world` that carries them together — but a
/// context object invented before three callers have asked for one is a guess,
/// so for now the dependency is spelled out at every call site where it exists.
///
/// Lesson 3.1 adds a `tint`. Triangles are filled now, and a fill needs a colour;
/// there is no lighting until 3.6, so each object simply carries one.
struct scene_object
{
    transform xform;
    mesh_handle geometry;
    const char* name;

    /// **What this surface looks like** — Lesson 6.5.
    ///
    /// `tint` and `surface` used to be two loose fields here, and the comment
    /// that lived on the second said the quiet part out loud: *"that is two
    /// different kinds of data wearing one struct, and the second kind has a
    /// name: a material."* This is that name.
    ///
    /// Nothing about the values changed — `mat.tint` is the old `tint` and
    /// `mat.surface` is the old `surface` — which is why the reference render
    /// comes out byte-identical across this lesson. That is the whole claim a
    /// refactor is allowed to make, and 5.1's rule for making it: **move without
    /// changing, then change without moving, verifying separately.**
    ///
    /// **By value, not by handle.** A `scene_object` has exactly one material and
    /// a material is 36 bytes; a handle would add a lookup to save nothing. The
    /// pool exists for where objects *share* one, which `ecs_swarm` demonstrates
    /// with two hundred drones. The rule that decides between them is about
    /// sharing, not about size.
    material mat{};

    /// Is this geometry a **closed surface** — a solid with an inside you can never
    /// see into? Added in Lesson 3.4, because it is the precondition for back-face
    /// culling and nothing else in the engine knows it.
    ///
    /// The cube and the icosahedron are closed. A quad is not, and neither is the
    /// ground plane: they are infinitely thin sheets with two visible sides, so
    /// culling their back faces makes them disappear when seen from behind. That is
    /// not a bug in the culler, it is culling being applied to geometry that does
    /// not satisfy its assumption.
    ///
    /// **LESSON 6.5 CORRECTED THE COMMENT THAT USED TO BE HERE.** It predicted
    /// this field would move onto the material in Module 6 — "because cull mode is
    /// pipeline state and pipeline state is what a material *is*". Building the
    /// material is what showed that to be wrong twice over. Cull mode is pipeline
    /// state and a material is explicitly *not* pipeline state (see
    /// `gfx/material.hpp`); and `closed` is not cull mode anyway, it is a fact
    /// about the MESH — a cube is closed whatever colour you paint it — which
    /// `validate()` reports, as `mesh_report::closed()`, from the geometry rather than
    /// taking anyone's word.
    ///
    /// So it stays, as the *intent* half of that pair: the mesh supplies the fact,
    /// this supplies the choice, and `cull_of()` is the one place the rule lives.
    /// The two are genuinely different questions, because a closed mesh may still
    /// be drawn two-sided on purpose — to look inside it, or to debug a winding.
    bool closed = true;
};

}   // namespace engine
