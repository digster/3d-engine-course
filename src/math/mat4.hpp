// src/math/mat4.hpp — a 3x3 in the corner, and a fourth column that now works.
//
// Lesson 2.5 proved that no matrix can translate: every linear transformation
// sends the origin to the origin. Lesson 2.6 built this file, put a translation
// in the fourth column, and watched it do absolutely nothing.
//
// Lesson 2.7 fixed that, and it is worth being precise about HOW, because the
// answer is not what people expect: **not one line of arithmetic in this file
// changed.** `operator*` below is byte for byte the function Lesson 2.6 wrote.
// What changed is the fourth component of the vectors handed to it.
//
//     w = 1   a POSITION  -> the fourth column is added in full  -> it translates
//     w = 0   a DIRECTION -> the fourth column is multiplied by 0 -> it does not
//
// Both are correct, and neither is a special case: translation should move a
// position and should not move a direction, and one number in the input selects
// between them. Lesson 2.7 §3.2.

#pragma once

#include "math/mat3.hpp"
#include "math/vec4.hpp"

namespace engine {

/// A 4x4 matrix, stored as its four columns.
///
/// Same shape as `mat2` and `mat3`: columns are the images of the basis vectors,
/// storage is column-major because the columns are the meaningful objects, and
/// the sixteen floats sit in memory as `c0.x, c0.y, c0.z, c0.w, c1.x, …`.
///
/// That layout is not only ours. It is what SDL_GPU and HLSL constant buffers
/// expect, which is why Module 4 will be able to upload one of these with a plain
/// `memcpy` and no transpose at the boundary — the single most common place for a
/// transform to get flipped exactly once.
struct mat4
{
    vec4 c0{1.0f, 0.0f, 0.0f, 0.0f};
    vec4 c1{0.0f, 1.0f, 0.0f, 0.0f};
    vec4 c2{0.0f, 0.0f, 1.0f, 0.0f};
    vec4 c3{0.0f, 0.0f, 0.0f, 1.0f};

    [[nodiscard]] static constexpr mat4 identity() { return {}; }

    /// Element in written notation: `at(row, col)`, both 0-based.
    [[nodiscard]] constexpr float at(int row, int col) const
    {
        const vec4 column = (col == 0) ? c0 : (col == 1) ? c1 : (col == 2) ? c2 : c3;
        return (row == 0) ? column.x : (row == 1) ? column.y
             : (row == 2) ? column.z : column.w;
    }
};

// ---- Building ------------------------------------------------------------------

/// Put a 3x3 in the top-left corner and leave the rest as the identity.
///
/// The three original columns gain a fourth component of 0, and a fourth column
/// arrives as `(0, 0, 0, 1)`. Both are forced: any other fourth component on the
/// first three columns would corrupt `w` as a side effect of rotating, and any
/// other fourth column would translate when we asked only to rotate.
///
/// That bottom row of `(0, 0, 0, 1)` is what makes `w` survive the trip — see
/// `affine()` below, and Lesson 2.7 §3.3.
[[nodiscard]] constexpr mat4 to_mat4(const mat3& m)
{
    return {to_vec4(m.c0, 0.0f),
            to_vec4(m.c1, 0.0f),
            to_vec4(m.c2, 0.0f),
            {0.0f, 0.0f, 0.0f, 1.0f}};
}

// ---- Applying and composing ------------------------------------------------------

/// Transform a vector: the same recipe, one term wider again.
///
/// Look carefully at the last term. `c3` is multiplied by `v.w`, and that single
/// multiplication is the whole of homogeneous coordinates:
///
///   - a **position** carries `w = 1`, so `c3 * 1` adds the fourth column in
///     full — which is exactly "add a constant offset", i.e. a translation;
///   - a **direction** carries `w = 0`, so `c3 * 0` contributes nothing and the
///     direction is rotated and scaled but never moved.
///
/// One function, no branch, no special case. The type of the thing being
/// transformed is carried *in the thing itself*, and the arithmetic reads it.
///
/// **This function is unchanged since Lesson 2.6.** It was always correct; what
/// was missing was a reason for `w` to be anything in particular.
[[nodiscard]] constexpr vec4 operator*(const mat4& m, vec4 v)
{
    return m.c0 * v.x + m.c1 * v.y + m.c2 * v.z + m.c3 * v.w;
}

/// Compose. `A * B` still means apply B first — the argument never depended on
/// the size.
[[nodiscard]] constexpr mat4 operator*(const mat4& a, const mat4& b)
{
    return {a * b.c0, a * b.c1, a * b.c2, a * b.c3};
}

// ---- Comparison -------------------------------------------------------------------

[[nodiscard]] constexpr bool operator==(const mat4& a, const mat4& b)
{
    return a.c0 == b.c0 && a.c1 == b.c1 && a.c2 == b.c2 && a.c3 == b.c3;
}
[[nodiscard]] constexpr bool operator!=(const mat4& a, const mat4& b) { return !(a == b); }

// ---- Affine transformations ------------------------------------------------------

/// Move everything by `t`. The transformation Lessons 2.5 and 2.6 could not express.
///
/// The linear part is the identity — nothing is rotated or scaled — and the offset
/// sits in the fourth column, where it is multiplied by `w`. Applied to a position
/// it adds `t`; applied to a direction it does nothing at all.
[[nodiscard]] constexpr mat4 translation(vec3 t)
{
    return {{1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            to_vec4(t, 1.0f)};
}

/// A linear transformation followed by a translation — a full **affine** transform.
///
/// This is the shape of essentially every transform in a scene: rotate and scale
/// the model, then put it where it belongs. Building it as
/// `translation(t) * to_mat4(linear)` gives the same answer; this spells it out so
/// the structure is visible, and so nobody has to remember which order those two
/// go in.
///
/// **The bottom row stays `(0, 0, 0, 1)`, and that is load-bearing.** It is what
/// makes the transformed `w` equal the original `w` — the last component of the
/// result is `0*x + 0*y + 0*z + 1*w`. So a position comes out as a position, a
/// direction comes out as a direction, and this survives any amount of composing:
/// multiply two matrices with that bottom row and the product has it too.
/// Lesson 2.7 §3.3 proves it; a matrix that breaks it is a projection, and that is
/// Lesson 2.10.
[[nodiscard]] constexpr mat4 affine(const mat3& linear, vec3 t)
{
    return {to_vec4(linear.c0, 0.0f),
            to_vec4(linear.c1, 0.0f),
            to_vec4(linear.c2, 0.0f),
            to_vec4(t, 1.0f)};
}

/// The offset an affine transform applies — i.e. where it sends the origin.
///
/// Reading it back out is a one-liner because it is simply the fourth column, and
/// it is worth having a name for: "where does this transform put the origin" is
/// the question you ask when a scene graph has gone wrong.
[[nodiscard]] constexpr vec3 translation_of(const mat4& m) { return xyz(m.c3); }

// ---- The view matrix -------------------------------------------------------

/// The view matrix for a camera at `eye`, looking at `target`, with `up_hint`
/// telling it which way is up. Takes world-space coordinates to VIEW space, where
/// the camera sits at the origin looking down its own −z axis.
///
/// This is `view_from_world`, and the one idea behind it (Lesson 2.9) is that a
/// camera is just another object with a placement: it has a `world_from_camera`
/// matrix like anything else, and looking *through* it means undoing that
/// placement. **The view matrix is the inverse of the camera's model matrix.**
///
/// We never call a general 4×4 inverse, because a camera has no size — its
/// placement is a rotation `R` (columns `right, up, backward`) plus a translation
/// `eye`, and the inverse of such a rigid transform is written down directly:
/// `R` is orthonormal, so its inverse is its transpose, and the whole inverse is
/// `affine(transpose(R), −transpose(R)·eye)`. Spelling that out, row `k` of the
/// result is camera-axis `k` followed by `−(axis · eye)` — which is exactly
/// "measure the world point along each camera axis, relative to the eye". That is
/// the construction below, stored column-major so the columns read as written.
///
/// The `−z forward` convention (conventions.html) is why `backward = eye − target`
/// rather than the reverse: the camera looks down `−z`, so its `+z` axis points
/// back toward the viewer. Lesson 2.9 §3.4 derives every sign.
[[nodiscard]] inline mat4 look_at(vec3 eye, vec3 target, vec3 up_hint)
{
    // The camera's own axes, in world space. `backward` points from the target
    // back to the eye; `right` is perpendicular to both up and backward; `up` is
    // recomputed so the three are exactly orthonormal even if `up_hint` was not
    // perpendicular to the look direction.
    const vec3 backward = normalised(eye - target);
    const vec3 right    = normalised(cross(up_hint, backward));
    const vec3 up       = cross(backward, right);   // already unit; no normalise needed

    // Stored column-major. The three rotation rows are the camera axes; the
    // fourth column is −(axis · eye) per row, which is the −transpose(R)·eye that
    // the rigid-inverse derivation produced.
    return {{right.x, up.x, backward.x, 0.0f},
            {right.y, up.y, backward.y, 0.0f},
            {right.z, up.z, backward.z, 0.0f},
            {-dot(right, eye), -dot(up, eye), -dot(backward, eye), 1.0f}};
}

// There is deliberately no `determinant` or `inverse` here.
//
// Both exist for 4x4 matrices and both are considerably more work than the 3x3
// versions — and nothing in this course has needed either yet. When an inverse is
// finally wanted (Lesson 2.9's view matrix), it will be for a matrix of a very
// specific structured form whose inverse can be written down directly and far more
// cheaply than the general formula. Writing the general one now would mean
// carrying, testing and trusting a large piece of code on the chance it is useful,
// which is how maths libraries get big without getting better.

} // namespace engine
