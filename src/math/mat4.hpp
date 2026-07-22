// src/math/mat4.hpp — a 3x3 in the corner, and one column that does nothing yet.
//
// Read the header of this file before the code, because the code is going to
// look anticlimactic and that is deliberate.
//
// Lesson 2.5 proved that no matrix can translate: every linear transformation
// sends the origin to the origin, and a translation does not. Lesson 2.6 builds
// mat3 and finds the same hole for the same reason. So we make the matrix bigger.
//
// It does not help. Not yet. A 4x4 applied to a 4-vector is exactly a 3x3 applied
// to the first three components, PLUS the fourth column scaled by the fourth
// component — and with nothing sensible to put in that fourth component, the
// column contributes zero and the 4x4 is a 3x3 in a larger coat.
//
// That is the honest state of affairs at the end of Lesson 2.6, and this file
// says so rather than quietly doing something clever. Lesson 2.7 works out what
// the fourth component should be and why, and at that moment every line here
// becomes useful without changing.

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
/// arrives as `(0, 0, 0, 1)`. Both of those choices are the only ones that make
/// this a faithful embedding — anything else would change what the matrix does to
/// the first three components — and neither of them is yet doing any work.
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
/// Look carefully at the last term. `c3` is multiplied by `v.w` — so if `v.w` is
/// zero, the fourth column contributes **nothing at all**, whatever is in it. You
/// can write any translation you like into `c3` and this function will ignore it.
///
/// That is not a bug and it is not a limitation of the implementation. It is the
/// arithmetic being honest: a matrix multiplies things, and to *add* a constant it
/// would need a component of the input that is always the same number. Lesson 2.6
/// §3.6 demonstrates the failure; Lesson 2.7 is where the fourth component stops
/// being a passenger.
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
