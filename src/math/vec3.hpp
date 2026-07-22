// src/math/vec3.hpp — the same arrow, one dimension wider.
//
// There is almost nothing new here, and that is the point worth noticing. Every
// idea from Lesson 1.7 — adding tip-to-tail, scaling without turning, the dot
// product as a shadow, normalising to separate direction from speed — is stated
// in terms that never mentioned how many components a vector had. So they all
// survive the move to 3-D unchanged, and this file is mostly vec2 with a `z`.
//
// The one genuinely 3-D operation, the CROSS PRODUCT, is deliberately absent.
// It is not an oversight and it is not hard: it is waiting for Lesson 3.4, where
// back-face culling needs a surface normal and the cross product can be derived
// from that need rather than declared in advance. Nothing in Modules 2 uses it.

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
// "what is perpendicular to this?" has no single answer and the 2-D routine has
// no honest generalisation. Getting a specific perpendicular needs a second
// vector to disambiguate — which is exactly what the cross product takes, and
// exactly why Lesson 3.4 introduces it when a surface finally supplies that
// second vector.

} // namespace engine
