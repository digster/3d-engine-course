// engine/include/engine/ecs/camera.hpp — the camera stops being a special object.
//
// Lesson 5.9. Through Module 4 the camera was a `demo::orbit_camera` — a struct
// with a target, a radius and two angles, living in the demo, reachable only by
// the one program that owned it. That worked because there was exactly one
// camera and exactly one program, and it stopped working the moment either of
// those became two.
//
// With an ECS the camera is not a kind of thing at all. It is an ENTITY that
// happens to have three components:
//
//     transform         where it is                (already existed)
//     world_transform   …resolved, so it can be PARENTED to something
//     camera            what it sees               (this file)
//     active_camera     …and it is the one in use  (a tag, this file)
//
// Three consequences fall out, and none of them needed designing:
//
//   1. A CAMERA CAN BE PARENTED. Attach it to a car and it rides in the car; the
//      hierarchy resolves it with everything else and the renderer never learns
//      that anything changed. In a world of camera structs this is a special
//      case somebody writes by hand, badly, twice.
//   2. "WHICH CAMERA IS ACTIVE" IS A COMPONENT, NOT A POINTER. Switching is
//      moving a tag; there is no dangling camera pointer to invalidate when the
//      thing it aimed at is destroyed, because a stale entity simply stops
//      resolving (Lesson 5.4).
//   3. A CAMERA IS FINDABLE. `view<camera, active_camera>()` is the query, and it
//      is the same query shape as everything else in the engine.
//
// WHAT THIS FILE DELIBERATELY DOES NOT DO is name a graphics type. `camera_view`
// lives in <engine/gfx/soft_renderer.hpp>, and having `ecs/` depend on `gfx/` to
// borrow a two-field struct would point the dependency arrow the wrong way for
// the sake of a convenience. The functions here return `mat4` and `vec3`, and
// the caller — which is already holding a renderer — assembles whatever the
// renderer wants. Two lines at one call site, against a layering violation that
// would be permanent.

#pragma once

#include <engine/core/assert.hpp>
#include <engine/ecs/hierarchy.hpp>
#include <engine/ecs/registry.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>

#include <cmath>
#include <vector>

namespace engine::ecs
{

// ---- The components --------------------------------------------------------

/// What a camera sees: a frustum, minus the aspect ratio.
///
/// **Aspect is deliberately absent**, and it is the one field people expect. A
/// camera's aspect ratio is not a property of the camera — it is a property of
/// the surface it is being drawn to, which is the window, which the user can
/// resize. Storing it here would mean every camera in a scene file carries a
/// number that was true on the machine that saved it. `projection_of()` takes it
/// as a parameter instead, from whoever owns the framebuffer.
struct camera
{
    /// Vertical field of view, in radians. Radians internally, always
    /// (conventions.html); degrees appear only at a UI edge.
    float fovy = 50.0f * 3.14159265358979f / 180.0f;

    /// The near plane. Small, and not arbitrarily small — Lesson 4.7 measured
    /// that depth precision falls off as the SQUARE of this distance, so moving
    /// it from 0.1 to 0.01 costs a hundredfold, not tenfold.
    float near_plane = 0.1f;
    float far_plane = 100.0f;
};

/// "This is the camera to render from."
///
/// **An empty struct, and that is the whole feature.** A component with no data
/// is a TAG: its presence is the information. `view<camera, active_camera>()`
/// then finds the active camera with the same machinery as any other query, and
/// switching cameras is `remove<active_camera>(a); add<active_camera>(b, {});`
/// — two structural changes, each of which Lesson 5.7 measured at ~4 ns.
///
/// `pool<active_camera>` stores a zero-byte value per row and its `data_` array
/// is `sizeof(active_camera) == 1` per entity, because C++ has no zero-sized
/// objects. One byte per tagged entity is a price worth the uniformity; an
/// engine with hundreds of tag types would specialise the pool to store none,
/// and would say so in a comment much like this one.
struct active_camera
{
};

// ---- Placing a camera ------------------------------------------------------

/// A transform that puts a camera at `eye` looking at `target`.
///
/// **The inverse of Lesson 2.9's question.** `look_at` answers "what matrix takes
/// the world into this camera's view?"; this answers "where does the camera have
/// to BE?", which is what a `transform` component holds. The two are inverses,
/// and the basis is built exactly once, here:
///
///     backward = normalised(eye − target)     the camera looks down its own −z
///     right    = normalised(cross(up, backward))
///     up       = cross(backward, right)        re-derived, so the three are
///                                              orthonormal even if `up_hint` was
///                                              not perpendicular
///
/// Those three vectors ARE the rotation's columns — Lesson 2.5's "a matrix's
/// columns are where the basis vectors land", used in the direction people find
/// harder: we know where we want the axes to point, so we write them down as
/// columns and we are finished.
///
/// The identity worth remembering, and the one verify_59 §D checks:
/// `rigid_inverse(parent_from_local(look_along(e, t, u))) == look_at(e, t, u)`,
/// element for element.
[[nodiscard]] inline transform look_along(vec3 eye, vec3 target, vec3 up_hint)
{
    const vec3 backward = normalised(eye - target);
    const vec3 right = normalised(cross(up_hint, backward));
    const vec3 up = cross(backward, right);

    transform t;
    t.position = eye;
    t.rotation = mat3{right, up, backward};
    t.scale = {1.0f, 1.0f, 1.0f};
    return t;
}

// ---- Reading a camera ------------------------------------------------------

/// The view matrix for a camera whose resolved placement is `placement`.
///
/// **The whole of Lesson 2.9 in one line, now that a camera is an object**: the
/// view matrix is the inverse of the camera's model matrix, and because a
/// camera's placement is rigid that inverse is written down rather than computed
/// (`rigid_inverse`, math/mat4.hpp).
///
/// The precondition — no scale — is asserted rather than checked, because it is a
/// programmer error and not something the world can do to a correct program: a
/// scaled camera is not a camera with a bug, it is a category mistake. Lesson
/// 5.3's test, applied.
[[nodiscard]] inline mat4 view_from_camera(const world_transform& placement)
{
    ENGINE_ASSERT_MSG(is_rigid(placement.matrix), log_core,
                      "ecs::view_from_camera: the camera's placement is not rigid — a scaled "
                      "camera has no meaningful view matrix, and rigid_inverse would return "
                      "something that is not an inverse");
    return rigid_inverse(placement.matrix);
}

/// Where the camera is, in world space — the fourth column of its placement.
///
/// Needed separately from the view matrix because view-dependent shading asks a
/// question about *places* rather than directions: a highlight has to know where
/// the eye is relative to the point being shaded (Lesson 3.7). Reading the
/// translation straight out of the matrix is cheaper and less error-prone than
/// inverting the view matrix back, which is what a renderer without this function
/// ends up doing.
[[nodiscard]] inline vec3 eye_of(const world_transform& placement)
{
    return translation_of(placement.matrix);
}

/// The projection matrix for `c` at the given aspect ratio.
///
/// A thin wrapper over Lesson 2.10's `perspective`, and it earns its place by
/// being the one spot that decides which of the camera's fields go where. Aspect
/// arrives from the caller for the reason given on `camera`: it belongs to the
/// surface, not to the camera.
[[nodiscard]] inline mat4 projection_of(const camera& c, float aspect)
{
    return perspective(c.fovy, aspect, c.near_plane, c.far_plane);
}

// ---- Finding the camera ----------------------------------------------------

/// The entity tagged `active_camera`, or `null_entity`.
///
/// If several are tagged, the first the query yields wins and a warning is
/// logged — "more than one active camera" is a mistake a correct program does not
/// make, but it is also a mistake a *scene file* can make, which puts it on the
/// world's side of Lesson 5.3's line and therefore makes it an error rather than
/// an assertion. Picking one deterministically-enough and complaining is better
/// than either crashing or rendering from an arbitrary viewpoint in silence.
///
/// Takes a non-const `registry&` because `view<>` does; Lesson 5.8 ships no const
/// view, and inventing one for this single caller would be designing an
/// abstraction to fit an accident.
[[nodiscard]] inline entity find_active_camera(registry& world)
{
    entity found = null_entity;
    int count = 0;

    world.view<camera, active_camera>().each(
        [&](entity e, const camera&, const active_camera&) {
            if (count == 0) { found = e; }
            ++count;
        });

    if (count > 1)
    {
        ENGINE_LOG_ERROR(log_core,
                         "ecs: %d entities are tagged active_camera — rendering from %u",
                         count, found.index());
    }
    return found;
}

/// Make `e` the active camera, removing the tag from whoever had it.
///
/// The switch is two structural changes and no pointer anywhere. Note what does
/// not have to happen: nothing is notified, nothing is invalidated, and if `e`
/// is destroyed later the tag goes with it and `find_active_camera` returns null
/// — a case the caller must already handle, because a scene can legitimately
/// have no camera yet.
inline bool set_active_camera(registry& world, entity e)
{
    if (!world.alive(e)) { return false; }

    // Collect, then act: `remove` erases a row from the very pool the view is
    // walking, which is Lesson 5.8's iteration rule. Two entities is a small
    // enough buffer to feel silly and exactly the same rule as two thousand.
    std::vector<entity> previous;
    world.view<active_camera>().each([&previous](entity old, const active_camera&) {
        previous.push_back(old);
    });
    for (const entity old : previous) { world.remove<active_camera>(old); }

    world.add<active_camera>(e, active_camera{});
    return true;
}

}   // namespace engine::ecs
