// src/math/vec3.hpp — the same arrow, one dimension wider.
//
// There is almost nothing new here, and that is the point worth noticing. Every
// idea from Lesson 1.7 — adding tip-to-tail, scaling without turning, the dot
// product as a shadow, normalising to separate direction from speed — is stated
// in terms that never mentioned how many components a vector had. So they all
// survive the move to 3-D unchanged, and this file is mostly vec2 with a `z`.
//
// The one genuinely 3-D operation is the CROSS PRODUCT, at the bottom of this
// file. It waited until Lesson 2.9 to be added, because that is where the engine
// first NEEDED it: building a camera's orthonormal frame from a look direction and
// an up hint is exactly the question "give me a vector perpendicular to these two",
// and the cross product is its answer. Lesson 3.4 meets it again from the other
// side — a triangle's surface normal for back-face culling — and connects it to
// the signed area and the determinant. Introduced where used, deepened where it
// recurs: the course's spiral, on a single function.

#pragma once

#include <cmath>

namespace engine {

/// A 3-D vector: an arrow with components along x, y and z.
///
/// Twelve bytes, passed and returned **by value** for the same reason `vec2` is
/// (Lesson 1.7): it still fits comfortably in registers, so a reference would
/// mean handing over an address the callee has to chase.
struct vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// ---- Arithmetic -------------------------------------------------------------

[[nodiscard]] constexpr vec3 operator+(vec3 a, vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
[[nodiscard]] constexpr vec3 operator-(vec3 a, vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
[[nodiscard]] constexpr vec3 operator-(vec3 v)         { return {-v.x, -v.y, -v.z}; }

[[nodiscard]] constexpr vec3 operator*(vec3 v, float s) { return {v.x * s, v.y * s, v.z * s}; }
[[nodiscard]] constexpr vec3 operator*(float s, vec3 v) { return {v.x * s, v.y * s, v.z * s}; }
[[nodiscard]] constexpr vec3 operator/(vec3 v, float s) { return {v.x / s, v.y / s, v.z / s}; }

constexpr vec3& operator+=(vec3& a, vec3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
constexpr vec3& operator-=(vec3& a, vec3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
constexpr vec3& operator*=(vec3& v, float s) { v.x *= s; v.y *= s; v.z *= s; return v; }
constexpr vec3& operator/=(vec3& v, float s) { v.x /= s; v.y /= s; v.z /= s; return v; }

[[nodiscard]] constexpr bool operator==(vec3 a, vec3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
[[nodiscard]] constexpr bool operator!=(vec3 a, vec3 b) { return !(a == b); }

// ---- The dot product --------------------------------------------------------

/// Identical in meaning to Lesson 1.7's: `|a| |b| cos(theta)`, and still the
/// answer to "how much of a points along b".
///
/// The geometry did not change when we added an axis. Two arrows in 3-D still
/// span a plane between them, the angle between them still lives in that plane,
/// and the projection argument from 1.7 §3.4 runs word for word. That is why
/// there is one more multiply here and no new idea.
[[nodiscard]] constexpr float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// ---- Length and direction ---------------------------------------------------

[[nodiscard]] constexpr float length_squared(vec3 v) { return dot(v, v); }
[[nodiscard]] inline float length(vec3 v) { return std::sqrt(length_squared(v)); }

[[nodiscard]] constexpr float distance_squared(vec3 a, vec3 b) { return length_squared(b - a); }
[[nodiscard]] inline float distance(vec3 a, vec3 b) { return length(b - a); }

/// The same direction, length 1. A zero-length vector has no direction, so this
/// returns zero rather than producing NaN — the reasoning is Lesson 1.7's and has
/// not changed.
[[nodiscard]] inline vec3 normalised(vec3 v)
{
    const float len_sq = length_squared(v);
    if (len_sq <= 0.0f) { return {0.0f, 0.0f, 0.0f}; }
    return v / std::sqrt(len_sq);
}

[[nodiscard]] inline vec3 normalised_or(vec3 v, vec3 fallback)
{
    const float len_sq = length_squared(v);
    if (len_sq <= 0.0f) { return fallback; }
    return v / std::sqrt(len_sq);
}

[[nodiscard]] constexpr vec3 lerp(vec3 a, vec3 b, float t) { return a + (b - a) * t; }

// Note what is NOT here: `perpendicular`. In 2-D there is essentially one arrow
// at right angles to a given one (up to sign), so `perpendicular(v)` was a
// sensible function. In 3-D there is a whole *plane* of them, so the question
// "what is perpendicular to this?" has no single answer. Getting a *specific*
// perpendicular needs a SECOND vector to disambiguate — and given two vectors,
// the cross product below is the arrow perpendicular to both.

// ---- The cross product ------------------------------------------------------

/// The vector perpendicular to both `a` and `b`, with the side fixed by the
/// right-hand rule — which IS this course's handedness convention.
///
/// Two readings, both worth carrying (Lesson 2.9 §3.3):
///
///   - DIRECTION: perpendicular to the plane the two arrows span. Which of the
///     two perpendicular directions you get is the right-hand rule: point the
///     fingers along `a`, curl them toward `b`, and the thumb is `cross(a, b)`.
///     This is why `cross(x, y) = z` exactly — it is the same right-handedness
///     the world-space axes use (see conventions.html).
///   - MAGNITUDE: `|a||b|sin(theta)`, the AREA of the parallelogram they span —
///     the perpendicular sibling of the dot product's `|a||b|cos(theta)`. So the
///     cross of two parallel vectors is zero (no area, no unique perpendicular),
///     and the cross of two unit vectors at right angles is a unit vector.
///
/// The component formula is a determinant expansion; Lesson 3.4 shows the same
/// number arriving as a signed area when a triangle needs a normal. Order and
/// sign matter: `cross(a, b) == -cross(b, a)`.
[[nodiscard]] constexpr vec3 cross(vec3 a, vec3 b)
{
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

} // namespace engine
