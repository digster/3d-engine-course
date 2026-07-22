// src/math/mat2.hpp — a linear transformation of the plane.
//
// A matrix is not a grid of numbers that happens to have a multiplication rule.
// It is the answer to one question:
//
//     WHERE DO THE BASIS VECTORS LAND?
//
// That is the whole of Lesson 2.5, and this file is written so the code says it
// too. The type below stores two columns, and each column *is* the image of one
// basis vector — c0 is where (1,0) goes, c1 is where (0,1) goes. Nothing here
// needs to be remembered separately from that sentence; every function is it,
// worked out.
//
// Header-only for the same reason vec2 is: small, hot, stable code that every
// caller wants inlined.

#pragma once

#include "math/vec2.hpp"

#include <cmath>

namespace engine {

/// A 2x2 matrix: a linear transformation of the plane, stored as its two columns.
///
/// **The columns are where the basis vectors land.** `c0` is the image of
/// `(1,0)`, `c1` is the image of `(0,1)`. Storing it this way is not an
/// implementation detail chosen for speed — it is the definition, made
/// structural, so that reading a `mat2` in a debugger tells you what it does.
///
/// Written on paper, this matrix is
///
///     | c0.x  c1.x |        | m00  m01 |
///     | c0.y  c1.y |   i.e. | m10  m11 |
///
/// which is the trap worth naming immediately: **we write matrices in rows and
/// store them in columns.** The first two floats in memory are `c0.x, c0.y` —
/// the *left column*, reading downwards — not the top row. That is the
/// column-major convention, it is what SDL_GPU and every graphics API in this
/// course expect, and it is the single most common source of transposed-matrix
/// bugs. Use `at(row, col)` when you want to index the way you would write it.
///
/// Defaults to the identity, because a default-constructed transformation that
/// silently collapsed the plane to a point would be a poor kind of surprise.
struct mat2
{
    vec2 c0{1.0f, 0.0f};   ///< where (1,0) lands — the left column
    vec2 c1{0.0f, 1.0f};   ///< where (0,1) lands — the right column

    /// The transformation that does nothing.
    ///
    /// A **static member** rather than a free function, and Lesson 2.6 is why:
    /// once `mat3` existed, a free `identity()` taking no arguments could only be
    /// distinguished by return type, which C++ cannot overload on. Naming the
    /// type at the call site — `mat2::identity()` — turned out to read better
    /// than the free function ever did.
    [[nodiscard]] static constexpr mat2 identity() { return {}; }

    /// Element in written notation: `at(row, col)`, both 0-based.
    ///
    /// The bridge between the two views. `at(0,1)` is the top-right element as
    /// you would write it, which lives in `c1.x` — the *second* column's first
    /// component. Bounds are not checked: this is a 2x2, the indices are
    /// literals at every real call site, and a branch here would be paid in
    /// inner loops for a mistake the compiler will not let you make twice.
    [[nodiscard]] constexpr float at(int row, int col) const
    {
        const vec2 column = (col == 0) ? c0 : c1;
        return (row == 0) ? column.x : column.y;
    }
};

// ---- Applying a transformation ----------------------------------------------

/// Transform a vector: `M * v`.
///
/// This one line is the entire content of the lesson, so it is worth reading
/// slowly. Any vector is a recipe in terms of the basis:
///
///     v = v.x * (1,0) + v.y * (0,1)
///
/// A *linear* transformation is one that survives that decomposition — it
/// distributes over addition and commutes with scaling — so the image of `v` is
/// the same recipe applied to the images of the basis vectors:
///
///     M*v = v.x * (where (1,0) went) + v.y * (where (0,1) went)
///         = v.x * c0 + v.y * c1
///
/// The familiar row-times-column arithmetic is what you get by writing that out
/// component by component. It is a consequence, not a definition, and deriving
/// it this way is why you never again have to remember which index goes where.
[[nodiscard]] constexpr vec2 operator*(const mat2& m, vec2 v)
{
    return m.c0 * v.x + m.c1 * v.y;
}

/// Compose two transformations: `A * B` means **apply B first, then A**.
///
/// Right-to-left, and that follows from the notation rather than being a
/// separate rule to memorise: `(A*B)*v` has to mean the same thing as `A*(B*v)`,
/// and in `A*(B*v)` it is plainly B that touches `v` first. Column vectors and
/// `v' = M*v` force this; a codebase using row vectors and `v' = v*M` reads
/// left-to-right instead, which is why mixing the two conventions produces code
/// that is transposed *and* backwards.
///
/// The implementation is the definition again. Column j of the result must be
/// where the composed transformation sends basis vector j — so send that basis
/// vector through B (which is just B's column j) and then through A.
///
/// **Order matters.** `rotate * scale` is not `scale * rotate` unless the scale
/// happens to be uniform; Lesson 2.5 §3.6 works an example where the two differ
/// visibly, and the demo draws both at once.
[[nodiscard]] constexpr mat2 operator*(const mat2& a, const mat2& b)
{
    return {a * b.c0, a * b.c1};
}

// ---- The three primitives ----------------------------------------------------
// Each one is built by answering the same question and reading off the answer.
// No trigonometric identities are needed anywhere below.

/// Rotation by `radians`.
///
/// Derived, not looked up. Put `(1,0)` on the unit circle and turn it by theta:
/// it lands at `(cos, sin)` — that is what sine and cosine *mean*. Now turn
/// `(0,1)`, which is a quarter turn ahead, so it lands a quarter turn ahead of
/// the first: at `(-sin, cos)`. Those two answers are the two columns, and the
/// rotation matrix is finished.
///
/// **Which way it turns depends on which way y points.** These numbers describe
/// a counter-clockwise turn in a y-up space, which is the mathematical
/// convention and the one this file uses. In **framebuffer coordinates, where +y
/// points down**, the identical numbers appear to turn *clockwise* on screen —
/// not because the matrix changed, but because the picture is flipped. Same
/// situation as `edge_function`'s sign (raster.hpp), and the same advice: pin
/// the space down before arguing about the direction.
[[nodiscard]] inline mat2 rotation(float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {{c, s}, {-s, c}};
}

/// Scale by `sx` along x and `sy` along y.
///
/// `(1,0)` stretches to `(sx, 0)`; `(0,1)` stretches to `(0, sy)`. A negative
/// factor is a reflection, and it is worth noticing that the machinery does not
/// treat it as a special case — it simply sends a basis vector backwards.
[[nodiscard]] constexpr mat2 scale(float sx, float sy) { return {{sx, 0.0f}, {0.0f, sy}}; }

/// Shear: slide x by `kx` per unit of y, and y by `ky` per unit of x.
///
/// `(1,0)` tilts to `(1, ky)` and `(0,1)` tilts to `(kx, 1)`. The picture is a
/// deck of cards pushed sideways: horizontal lines stay horizontal and keep
/// their spacing, but slide. Shear is the transformation that makes "parallel
/// lines stay parallel, evenly spaced" feel like a real constraint rather than
/// an obvious one, because the shape changes completely while it holds.
[[nodiscard]] constexpr mat2 shear(float kx, float ky) { return {{1.0f, ky}, {kx, 1.0f}}; }

// ---- Measuring a transformation ----------------------------------------------

/// The determinant: **the factor by which areas change**, with a sign.
///
/// Take the unit square. Its corners are the basis vectors, so after the
/// transformation it is the parallelogram spanned by `c0` and `c1` — and this is
/// that parallelogram's signed area. Everything else in the plane scales by the
/// same factor, because every region can be approximated by small squares.
///
/// **You have written this expression before.** It is `edge_function` from
/// Lesson 2.2 with its first point at the origin, which is the 2-D cross
/// product, which is twice the area of a triangle. The determinant, the cross
/// product and the edge function are three names for one quantity, and the
/// engine has been computing it every frame since Lesson 2.2 without calling it
/// a determinant.
///
///   > 0  areas scale by this much, orientation preserved
///   = 0  the plane is squashed onto a line (or a point) — information is lost,
///        and there is no inverse
///   < 0  areas scale by |det| and the plane is flipped over
[[nodiscard]] constexpr float determinant(const mat2& m)
{
    return m.c0.x * m.c1.y - m.c1.x * m.c0.y;
}

/// The transpose: rows become columns.
///
/// Included because it is two lines and because it is the operation people reach
/// for when a transformation comes out backwards. Reach carefully: transposing
/// is only the same as inverting for a *rotation* (more generally, for any
/// orthonormal matrix), and using it as a cheap inverse on a matrix that also
/// scales is a bug that looks almost right. Lesson 3.6 meets this properly, when
/// normals need a transform that is not the one used for positions.
[[nodiscard]] constexpr mat2 transpose(const mat2& m)
{
    return {{m.c0.x, m.c1.x}, {m.c0.y, m.c1.y}};
}

/// The transformation that undoes `m`, or the zero matrix if there is none.
///
/// For a 2x2 the formula is short enough to be worth knowing: swap the two
/// diagonal elements, negate the other two, divide by the determinant.
///
/// **A zero determinant means no inverse exists**, and that is a statement about
/// information rather than about arithmetic. A transformation with zero
/// determinant has flattened the plane onto a line: many different inputs now
/// share one output, so nothing could possibly recover which one you started
/// with. Returning zeros rather than dividing by zero keeps that from becoming a
/// silent NaN — check `determinant(m) != 0` yourself when your matrices can
/// degenerate. (The exactly-zero test is deliberate here; a near-zero
/// determinant is an ill-conditioned inverse rather than a missing one, and
/// pretending otherwise would hide the distinction.)
[[nodiscard]] inline mat2 inverse(const mat2& m)
{
    const float det = determinant(m);
    if (det == 0.0f) { return {{0.0f, 0.0f}, {0.0f, 0.0f}}; }

    const float inv = 1.0f / det;
    return {{ m.c1.y * inv, -m.c0.y * inv},
            {-m.c1.x * inv,  m.c0.x * inv}};
}

// ---- Comparison ---------------------------------------------------------------

[[nodiscard]] constexpr bool operator==(const mat2& a, const mat2& b)
{
    return a.c0 == b.c0 && a.c1 == b.c1;
}
[[nodiscard]] constexpr bool operator!=(const mat2& a, const mat2& b) { return !(a == b); }

} // namespace engine
