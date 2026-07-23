// src/math/transform.hpp — where a thing is, which way it faces, and how big it is.
//
// Lessons 2.5-2.7 built the machinery: mat3 for the linear part, mat4 for the
// affine one, and `w` to say which of a position and a direction you meant.
// Nothing in any of those files knows what a scene is. This one does.
//
// A `transform` is the answer to three questions an artist and a designer ask
// about every object — how big, which way, where — and `parent_from_local()`
// turns those three answers into the one matrix that carries the object's
// vertices out of MODEL space and into WORLD space.
//
// The name of that function is the point of Lesson 2.8, and it is not decorative:
//
//     parent_from_local   reads right to left, like the matrices it builds.
//                         Feed it something in LOCAL space, get something in
//                         PARENT space. Compose two and the inner labels have to
//                         match, or you have written a bug:
//
//                             view_from_model = view_from_world * world_from_model
//                                                     ^^^^^        ^^^^^
//                                                     these must agree
//
// Nearly every transform bug in graphics is a coordinate used in the space it is
// not in. The maths cannot catch it — a vec3 is three floats whatever room it
// lives in — so the naming has to. Lesson 2.8 §2.
//
// Why "parent" and not "world": in Module 2 every object stands on its own, so
// its parent IS the world and `parent_from_local` returns a world-from-model
// matrix. Module 5 adds a transform HIERARCHY, where an object's transform is
// relative to another object rather than to the world. When that happens this
// function does not change by a single character — only the meaning of "parent"
// widens. Naming it `world_from_local` today would have been a lie we had to go
// back and correct.

#pragma once

#include "math/mat4.hpp"

namespace engine {

/// Position, orientation and size — an object's place in the world.
///
/// This is the seed of Module 5's transform component, and the shape is not
/// arbitrary: it is what essentially every engine exposes, because it is what a
/// human can actually author. Nobody types sixteen floats to place a crate.
///
/// The rotation is a `mat3` for now, which is honest but not final. A matrix is
/// a poor thing to *store* a rotation in — nine floats for three degrees of
/// freedom, no natural way to interpolate between two of them, and it drifts out
/// of being a rotation as you accumulate updates. Lesson 7.1 replaces it with a
/// quaternion, and this struct is where that replacement will land. Everything
/// downstream asks for a matrix, so the swap touches one line of
/// `parent_from_local()` and nothing else — which is the argument for having a
/// named type here at all rather than passing three loose variables around.
struct transform
{
    /// Where the object's origin sits, in parent space. A POSITION (Lesson 2.7).
    vec3 position{0.0f, 0.0f, 0.0f};

    /// Which way the object faces. Identity means "aligned with the parent's axes".
    mat3 rotation = mat3::identity();

    /// How big, along the object's OWN axes — which is why scale is applied
    /// first and why `(1,1,1)` rather than `(0,0,0)` is the do-nothing value.
    vec3 scale{1.0f, 1.0f, 1.0f};
};

/// The matrix that carries the object's vertices from local space to parent space.
///
/// This is `T * R * S` — **scale first, then rotate, then translate** — and the
/// order is derived rather than decreed (Lesson 2.8 §3.2). In one line: scale is
/// defined along the object's own axes, so it has to act while the coordinates
/// are still the object's own; rotation is about the object's own origin, so it
/// has to act while the object is still at the origin; translation puts the
/// finished object where it belongs, so it goes last. Every other order breaks
/// one of those three sentences, and two of the breakages are visible on screen
/// in this lesson's demo.
///
/// **Read the returned matrix column by column and it spells out the object's
/// frame**, which is the single most useful thing to know when a scene has gone
/// wrong:
///
///     column 0 = the object's x axis in parent space, times its x size
///     column 1 = the object's y axis in parent space, times its y size
///     column 2 = the object's z axis in parent space, times its z size
///     column 3 = the object's origin in parent space  (= `position`)
///
/// That is not a coincidence to be memorised. Lesson 2.5 established that a
/// matrix's columns are where the basis vectors land, and the basis vectors of
/// model space ARE the object's own axes. The scale multiplies each column
/// because `S` sends `(1,0,0)` to `(sx,0,0)` before `R` ever sees it.
[[nodiscard]] inline mat4 parent_from_local(const transform& t)
{
    // Written as three scaled columns rather than `t.rotation * scale(...)`,
    // which produces exactly the same nine floats. Two reasons, in order of
    // importance:
    //
    //   1. It SAYS the thing. The line below is the doc comment above, in code:
    //      column k is axis k scaled by size k. Composing two matrices and
    //      trusting the result to have that property is a fact you have to
    //      remember; writing it is a fact you can read.
    //   2. It is nine multiplies instead of twenty-seven, because a diagonal
    //      matrix's product is not a general product. Small, and not the reason —
    //      but there is no cost to taking it.
    const mat3 axes{t.rotation.c0 * t.scale.x,
                    t.rotation.c1 * t.scale.y,
                    t.rotation.c2 * t.scale.z};

    return affine(axes, t.position);
}

// There is deliberately no `local_from_parent` here yet.
//
// It exists, it is useful, and it is cheap for this particular shape — undoing
// `T * R * S` is `S⁻¹ * Rᵀ * T⁻¹`, with no general 4x4 inversion needed, because
// a rotation's inverse is its transpose and a scale's inverse is the reciprocals.
// It is left out because nothing needs it yet, and because the first thing that
// will need it — Lesson 2.9's view matrix, which is exactly "the inverse of where
// the camera is" — deserves to derive it rather than to find it already written.
// Same reasoning as `mat4`'s missing `inverse`, and the same discipline: a maths
// library grows one justified function at a time or it grows without getting
// better.

} // namespace engine
