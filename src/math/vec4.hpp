// src/math/vec4.hpp — four components, and an honest admission about the fourth.
//
// This type exists for one reason today: a 4x4 matrix's columns have to be
// something, and that something has four components. Everything below is the
// obvious extension of vec3.
//
// The fourth component is called `w`, and as of Lesson 2.7 it has a job. It says
// WHAT KIND OF THING this is:
//
//     w = 1   a POSITION.  Somewhere in space. Translation moves it.
//     w = 0   a DIRECTION. Which way, how far — but not "where".
//                          Translation must NOT move it.
//
// That is one number carrying a distinction the type system would otherwise need
// two types for, and it is not a trick: it falls out of asking what translation
// should do to each. Moving the world two metres east moves every position two
// metres east. It leaves every direction exactly as it was — "north" is still
// north. Lesson 2.7 §3.2.
//
// A third case exists — w being neither 0 nor 1 — and it is what perspective
// projection produces. Lesson 2.10.

#pragma once

#include "math/vec3.hpp"

#include <cmath>

namespace engine {

/// A 4-component vector.
struct vec4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

// ---- Building and unpacking --------------------------------------------------

/// A **position**: somewhere in space. Carries `w = 1`, so translation moves it.
///
/// Prefer this and `direction()` over `to_vec4` at every call site. The whole
/// point of Lesson 2.7 is that `w` is not a padding byte — it is the difference
/// between "the lamp is at (3,0,4)" and "the light points that way" — and a call
/// that says `point(p)` cannot be misread, where one that says `to_vec4(p, 1.0f)`
/// invites somebody to change the 1 to a 0 to make a compile error go away.
[[nodiscard]] constexpr vec4 point(vec3 p) { return {p.x, p.y, p.z, 1.0f}; }

/// A **direction**: which way and how far, but not where. Carries `w = 0`, so
/// translation leaves it alone — which is correct rather than a limitation.
///
/// Surface normals, velocities, "the direction to the light", the axis a door
/// swings about. Move the whole world two metres east and none of them change.
/// Lesson 3.6's lighting depends on this being right; a normal that has been
/// translated is no longer a normal, and the resulting shading is wrong in a way
/// that gets worse the further the object is from the origin.
[[nodiscard]] constexpr vec4 direction(vec3 d) { return {d.x, d.y, d.z, 0.0f}; }

/// A vec3 with an arbitrary fourth component.
///
/// Spelled out as a named function rather than an implicit conversion, because an
/// implicit one would let a 3-D vector silently acquire a `w` nobody chose. Reach
/// for `point()` or `direction()` unless you genuinely mean some other value —
/// which, before Lesson 2.10's projection, you almost certainly do not.
[[nodiscard]] constexpr vec4 to_vec4(vec3 v, float w) { return {v.x, v.y, v.z, w}; }

/// The first three components, with the fourth simply dropped.
///
/// **Dropped, not divided by.** Correct while `w` is 1: an affine transformation
/// leaves `w` exactly as it found it (Lesson 2.7 §3.3), so a position comes back
/// with `w = 1` and dividing by it would be a no-op.
///
/// It stops being correct the moment a projection matrix is in the chain, because
/// those deliberately produce a `w` that is neither 0 nor 1 (Lesson 2.10). There,
/// you want `perspective_divide` below, not this — and the two are kept as
/// separate named functions on purpose, so "drop `w`" and "divide by `w`" can
/// never be confused for one another at a call site.
[[nodiscard]] constexpr vec3 xyz(vec4 v) { return {v.x, v.y, v.z}; }

/// The first three components, each **divided by** the fourth — the perspective
/// divide, the single most important operation in the projection pipeline.
///
/// A projection matrix (Lesson 2.10) deliberately writes `-z` — the distance in
/// front of the camera — into `w`. Dividing `x` and `y` by it is exactly the
/// similar-triangles shrink that makes distant things smaller: at twice the depth,
/// half the size. Dividing `z` by it lands depth in the `[0, 1]` range the GPU
/// wants. Before this point in the course `w` was always 1 and this was a no-op;
/// from here on it is where perspective actually happens.
///
/// **No guard on `w`.** A `w` of zero means a point exactly on the plane through
/// the eye (Lesson 2.7's "point at infinity"), and a negative `w` means a point
/// *behind* the camera — both are handled by clipping the geometry away *before*
/// it reaches here, which is Lesson 3.3. Until then the demo keeps its vertices in
/// front of the camera so `w` stays safely positive; dividing here is unconditional
/// because the real fix is upstream, not a branch in an accessor.
[[nodiscard]] constexpr vec3 perspective_divide(vec4 v)
{
    return {v.x / v.w, v.y / v.w, v.z / v.w};
}

// ---- Arithmetic -------------------------------------------------------------

[[nodiscard]] constexpr vec4 operator+(vec4 a, vec4 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}
[[nodiscard]] constexpr vec4 operator-(vec4 a, vec4 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}
[[nodiscard]] constexpr vec4 operator-(vec4 v) { return {-v.x, -v.y, -v.z, -v.w}; }

[[nodiscard]] constexpr vec4 operator*(vec4 v, float s)
{
    return {v.x * s, v.y * s, v.z * s, v.w * s};
}
[[nodiscard]] constexpr vec4 operator*(float s, vec4 v) { return v * s; }
[[nodiscard]] constexpr vec4 operator/(vec4 v, float s)
{
    return {v.x / s, v.y / s, v.z / s, v.w / s};
}

constexpr vec4& operator+=(vec4& a, vec4 b) { a = a + b; return a; }
constexpr vec4& operator-=(vec4& a, vec4 b) { a = a - b; return a; }
constexpr vec4& operator*=(vec4& v, float s) { v = v * s; return v; }

[[nodiscard]] constexpr bool operator==(vec4 a, vec4 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}
[[nodiscard]] constexpr bool operator!=(vec4 a, vec4 b) { return !(a == b); }

/// The four-component dot product.
///
/// Provided because matrix arithmetic wants it, and flagged because its
/// *geometric* reading is not the one you have from Lessons 1.7 and 2.6. Once
/// `w` means something (Lesson 2.7), summing all four products stops being "how
/// much of a points along b" and becomes something you have to think about. Use
/// `dot(xyz(a), xyz(b))` when you mean the geometric one.
[[nodiscard]] constexpr float dot(vec4 a, vec4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

} // namespace engine
