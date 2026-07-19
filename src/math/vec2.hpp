// src/math/vec2.hpp — the first piece of the maths library.
//
// A vector is an arrow: a direction and a length, with no position of its own.
// Everything here follows from that sentence, and everything in Modules 2 and 3
// is built on top of it.
//
// Header-only, and deliberately so. These functions are two or three lines each
// and are called thousands of times per frame; putting them where the compiler
// can inline them at every call site is worth more than the tidiness of a .cpp.
// That trade only holds for small, hot, stable code — which is exactly what a
// vector type is, and is not what most of the engine is.

#pragma once

#include <cmath>

namespace engine {

/// A 2-D vector: an arrow with components along x and y.
///
/// A plain struct with public members, passed and returned **by value**. Eight
/// bytes fit in a register, so a `const vec2&` would mean passing an address the
/// callee must then dereference — more work, not less. The rule of thumb this
/// establishes: pass small value types by value, and reach for a reference only
/// when the object is large or must be mutated in place.
struct vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

// ---- Arithmetic -------------------------------------------------------------
// Adding arrows is tip-to-tail; scaling stretches without turning. Both are
// component-wise, which is the payoff for choosing components in the first place.

[[nodiscard]] constexpr vec2 operator+(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
[[nodiscard]] constexpr vec2 operator-(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
[[nodiscard]] constexpr vec2 operator-(vec2 v)         { return {-v.x, -v.y}; }

[[nodiscard]] constexpr vec2 operator*(vec2 v, float s) { return {v.x * s, v.y * s}; }
[[nodiscard]] constexpr vec2 operator*(float s, vec2 v) { return {v.x * s, v.y * s}; }
[[nodiscard]] constexpr vec2 operator/(vec2 v, float s) { return {v.x / s, v.y / s}; }

constexpr vec2& operator+=(vec2& a, vec2 b) { a.x += b.x; a.y += b.y; return a; }
constexpr vec2& operator-=(vec2& a, vec2 b) { a.x -= b.x; a.y -= b.y; return a; }
constexpr vec2& operator*=(vec2& v, float s) { v.x *= s; v.y *= s; return v; }
constexpr vec2& operator/=(vec2& v, float s) { v.x /= s; v.y /= s; return v; }

[[nodiscard]] constexpr bool operator==(vec2 a, vec2 b) { return a.x == b.x && a.y == b.y; }
[[nodiscard]] constexpr bool operator!=(vec2 a, vec2 b) { return !(a == b); }

// ---- The dot product --------------------------------------------------------

/// The dot product: how much of `a` points along `b`, times how long `b` is.
///
/// Geometrically it is `|a| |b| cos(theta)`, and the component form below is not
/// a separate definition — it is that same quantity, worked out by projecting `a`
/// onto each axis in turn. Lesson 1.7 §3.4 does the derivation.
///
/// The sign alone answers a question that comes up constantly:
///   > 0  the two arrows broadly agree      (less than a quarter turn apart)
///   = 0  they are exactly perpendicular
///   < 0  they broadly disagree             (more than a quarter turn apart)
///
/// "Is that enemy in front of me?" is `dot(facing, to_enemy) > 0` — no angles, no
/// trigonometry, one multiply-add.
[[nodiscard]] constexpr float dot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }

// ---- Length -----------------------------------------------------------------

/// The squared length. **Prefer this whenever you are only comparing.**
///
/// `sqrt` is monotonic, so `|a| < |b|` exactly when `|a|² < |b|²`. Any comparison
/// of lengths — nearest enemy, inside this radius, sort by distance — gives the
/// identical answer without it. The habit is worth forming now: a square root in
/// an inner loop is a real cost, and it is very often buying nothing.
[[nodiscard]] constexpr float length_squared(vec2 v) { return dot(v, v); }

/// The length of the arrow, by Pythagoras. Only call it when you need the number
/// itself rather than a comparison.
[[nodiscard]] inline float length(vec2 v) { return std::sqrt(length_squared(v)); }

[[nodiscard]] constexpr float distance_squared(vec2 a, vec2 b) { return length_squared(b - a); }
[[nodiscard]] inline float distance(vec2 a, vec2 b) { return length(b - a); }

// ---- Direction --------------------------------------------------------------

/// The same direction, with length exactly 1 — a *unit* vector.
///
/// Dividing an arrow by its own length leaves the direction untouched and makes
/// the length 1, which is what lets speed and direction be chosen independently:
/// `position += normalised(input) * speed * dt` moves at `speed` whichever way
/// the arrow points. Without it, a diagonal input travels sqrt(2) times too far —
/// the bug Exercise 1.2.3 left standing five lessons ago.
///
/// **A zero-length vector has no direction**, and dividing by its length would be
/// 0/0, which is NaN — and NaN spreads silently through every later calculation
/// (Lesson 1.3 §3.5). There is no mathematically correct answer to "which way does
/// a zero-length arrow point", so this returns the zero vector: it keeps a
/// no-input frame meaning "no movement" rather than "movement in a broken
/// direction". Use normalised_or() where a specific fallback direction matters.
[[nodiscard]] inline vec2 normalised(vec2 v)
{
    const float len_sq = length_squared(v);
    if (len_sq <= 0.0f)
    {
        return {0.0f, 0.0f};
    }
    return v / std::sqrt(len_sq);
}

/// As normalised(), but a zero-length input yields `fallback` — for the cases
/// where some direction must be produced, such as a facing that must point
/// somewhere even when the player is standing still.
[[nodiscard]] inline vec2 normalised_or(vec2 v, vec2 fallback)
{
    const float len_sq = length_squared(v);
    if (len_sq <= 0.0f)
    {
        return fallback;
    }
    return v / std::sqrt(len_sq);
}

/// Rotated a quarter turn. Useful for "the direction to my left", for surface
/// normals in 2-D, and for the separating axes of Module 7's collision work.
///
/// Which way it turns depends on which way your y axis points, so the answer is
/// only meaningful once that is pinned down — in framebuffer coordinates, where
/// +y is down, this turns clockwise on screen. Dot it with the original and you
/// get zero, which is the check worth remembering.
[[nodiscard]] constexpr vec2 perpendicular(vec2 v) { return {-v.y, v.x}; }

/// Straight-line interpolation between two vectors, component by component.
/// Same operation as Lesson 1.4's scalar lerp, one dimension wider.
[[nodiscard]] constexpr vec2 lerp(vec2 a, vec2 b, float t) { return a + (b - a) * t; }

/// The component of `a` that lies along `b` — the *shadow* of `a` on `b`, as a
/// vector rather than a length.
///
/// This is the geometric picture the dot product came from, reassembled: scale
/// b's direction by how far a's shadow reaches along it. Lesson 1.7 §3.3.
[[nodiscard]] inline vec2 project_onto(vec2 a, vec2 b)
{
    const float len_sq = length_squared(b);
    if (len_sq <= 0.0f)
    {
        return {0.0f, 0.0f};   // no direction to project onto
    }
    return b * (dot(a, b) / len_sq);
}

} // namespace engine
