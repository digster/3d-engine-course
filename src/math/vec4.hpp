// src/math/vec4.hpp — four components, and an honest admission about the fourth.
//
// This type exists for one reason today: a 4x4 matrix's columns have to be
// something, and that something has four components. Everything below is the
// obvious extension of vec3.
//
// The fourth component is called `w`, because that is what everyone calls it and
// pretending otherwise would only make this course harder to read alongside every
// other reference. But **we are not yet saying what it means.** Lesson 2.6 builds
// the 4x4 machinery and discovers that its fourth column does nothing useful; the
// question of what `w` should be, and why, is Lesson 2.7's — and it is a much
// better lesson if you arrive at it having already watched the column sit there
// doing nothing.
//
// If you have met homogeneous coordinates before: yes, you know where this is
// going. Resist skipping ahead; the derivation is the part worth having.

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

/// A vec3 with a fourth component bolted on.
///
/// Spelled out as a named function rather than an implicit conversion, because
/// an implicit one would let a 3-D vector silently acquire a `w` of zero at a
/// call site — and "silently acquires a value nobody chose" is the exact shape
/// of the bug Lesson 2.7 is about to spend a lesson on.
[[nodiscard]] constexpr vec4 to_vec4(vec3 v, float w) { return {v.x, v.y, v.z, w}; }

/// The first three components, with the fourth simply dropped.
///
/// **Dropped, not divided by.** There is a version of this that divides by `w`
/// and it is the single most important operation in the whole projection
/// pipeline — but it belongs to Lesson 2.10, it has a name (the perspective
/// divide), and doing it here silently would be indefensible. Today `w` is a
/// number we carry and ignore.
[[nodiscard]] constexpr vec3 xyz(vec4 v) { return {v.x, v.y, v.z}; }

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
