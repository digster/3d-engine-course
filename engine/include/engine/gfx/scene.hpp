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
/// The `mesh` is stored BY VALUE and that is cheap: it is two spans, four words,
/// pointing at static geometry that outlives everything.
///
/// Lesson 3.1 adds a `tint`. Triangles are filled now, and a fill needs a colour;
/// there is no lighting until 3.6, so each object simply carries one.
struct scene_object
{
    transform xform;
    mesh geometry;
    const char* name;
    Uint32 tint = 0xFFFFFFFFu;

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
    /// In a real engine this lives on the **material**, because cull mode is
    /// pipeline state and pipeline state is what a material *is* (Module 6). Here it
    /// is a bool on the object and the demo only warns, because the whole scene is
    /// drawn in one batch with one style — which is itself the honest lesson: two
    /// cull modes means two batches.
    bool closed = true;

    /// How shiny this object is, and what colour its highlight comes out —
    /// Lesson 3.7.
    ///
    /// **Look at what just happened to this struct.** `tint` was enough while a
    /// surface was one colour; then 3.4 needed `closed`; now 3.7 needs two more
    /// numbers, and all four describe the same thing — *the surface* — while
    /// `xform` and `geometry` describe where it is and what shape it is. That is
    /// two different kinds of data wearing one struct, and the second kind has a
    /// name: a material. Module 6 gives it one. It is being left visible here
    /// rather than fixed early, because a material invented before three lessons
    /// have asked for one is a guess.
    ///
    /// Defaults to a black highlight, which is exactly Lesson 3.6's shading — so
    /// every object that says nothing about shininess looks precisely as it did.
    specular surface{};
};

}   // namespace engine
