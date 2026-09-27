// engine/include/engine/math/mat3.hpp — a linear transformation of space.
//
// Lesson 2.5 asked one question and got a matrix out of it:
//
//     WHERE DO THE BASIS VECTORS LAND?
//
// Nothing about that question mentioned how many basis vectors there are. In 3-D
// there are three, so the answer has three columns, and every rule from 2.5
// survives unchanged: the columns are the images, applying is "x of the first
// plus y of the second plus z of the third", composing is "send each column of B
// through A", and the determinant is a scale factor — for volume now instead of
// area.
//
// If this file feels like it is repeating mat2.hpp, that is the lesson.

#pragma once

#include <engine/math/vec3.hpp>

#include <cmath>

namespace engine {

/// A 3x3 matrix, stored as its three columns.
///
/// Same shape as `mat2` and for the same reasons: the columns are the images of
/// the basis vectors, so naming them as vectors makes the idea structural and
/// makes column-major storage a consequence rather than a rule to enforce. The
/// nine floats sit in memory in the order `c0.x, c0.y, c0.z, c1.x, …` — the left
/// column first, read downwards.
///
/// Written on paper this is
///
///     | c0.x  c1.x  c2.x |
///     | c0.y  c1.y  c2.y |
///     | c0.z  c1.z  c2.z |
///
/// which is the same written-in-rows / stored-in-columns trap as Lesson 2.5 §3.3,
/// now with three chances to fall into it instead of two.
struct mat3
{
    vec3 c0{1.0f, 0.0f, 0.0f};   ///< where (1,0,0) lands
    vec3 c1{0.0f, 1.0f, 0.0f};   ///< where (0,1,0) lands
    vec3 c2{0.0f, 0.0f, 1.0f};   ///< where (0,0,1) lands

    /// The transformation that does nothing.
    ///
    /// A **static member** rather than a free `identity()`, because adding a
    /// second matrix type made a free function impossible: `identity()` takes no
    /// arguments, so `mat2` and `mat3` versions would differ only in return type,
    /// and C++ cannot overload on that. Naming the type at the call site —
    /// `mat3::identity()` — is clearer than the free function ever was anyway.
    /// Lesson 2.6 §4.2.
    [[nodiscard]] static constexpr mat3 identity() { return {}; }

    /// Element in written notation: `at(row, col)`, both 0-based.
    [[nodiscard]] constexpr float at(int row, int col) const
    {
        const vec3 column = (col == 0) ? c0 : (col == 1) ? c1 : c2;
        return (row == 0) ? column.x : (row == 1) ? column.y : column.z;
    }
};

// ---- Applying and composing --------------------------------------------------

/// Transform a vector: `M * v` — the recipe, one term wider than Lesson 2.5's.
[[nodiscard]] constexpr vec3 operator*(const mat3& m, vec3 v)
{
    return m.c0 * v.x + m.c1 * v.y + m.c2 * v.z;
}

/// Compose: `A * B` means **apply B first, then A**, exactly as in 2-D.
///
/// Each column of the result is where that basis vector ends up after B and then
/// A — so it is B's column, put through A. The rule did not change because the
/// argument for it never mentioned dimension.
[[nodiscard]] constexpr mat3 operator*(const mat3& a, const mat3& b)
{
    return {a * b.c0, a * b.c1, a * b.c2};
}

// ---- The primitives ----------------------------------------------------------

/// Scale along each axis independently.
[[nodiscard]] constexpr mat3 scale(float sx, float sy, float sz)
{
    return {{sx, 0.0f, 0.0f}, {0.0f, sy, 0.0f}, {0.0f, 0.0f, sz}};
}

// Rotation in 3-D needs an AXIS, and that is the one genuinely new idea in this
// file. In 2-D "rotate by theta" was unambiguous because there was only one plane
// to rotate in. In 3-D a rotation turns one plane and leaves the direction
// perpendicular to it alone — so you have to say which.
//
// Each of the three below is therefore Lesson 2.5's 2-D rotation, acting in the
// plane of the OTHER two axes, with the axis's own column left untouched. Read
// them that way and there is nothing to memorise: rotation_z is literally the
// mat2 rotation in the top-left corner.

/// Rotate about the x axis — turns y toward z, leaves x alone.
[[nodiscard]] inline mat3 rotation_x(float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {{1.0f, 0.0f, 0.0f},    // x is the axis: it does not move
            {0.0f,    c,    s},    // y swings toward z
            {0.0f,   -s,    c}};   // z swings away from y
}

/// Rotate about the y axis — turns z toward x, leaves y alone.
///
/// **This is the one where the signs look wrong, and they are not.** Going
/// around the axes in the cyclic order x -> y -> z -> x, the plane that y's
/// rotation acts in is the z-x plane *in that order* — so the "first" axis is z
/// and the "second" is x, and the minus sign lands in the column you do not
/// expect. Every graphics programmer has typed this one transposed at least once;
/// the fix is to derive it from the cycle rather than to recall the layout.
[[nodiscard]] inline mat3 rotation_y(float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {{   c, 0.0f,   -s},    // x swings away from z
            {0.0f, 1.0f, 0.0f},    // y is the axis
            {   s, 0.0f,    c}};   // z swings toward x
}

/// Rotate about the z axis — turns x toward y, leaves z alone.
///
/// Identical to Lesson 2.5's 2-D rotation with a third row and column of the
/// identity bolted on, which is what "the z axis points out of the 2-D page"
/// means made concrete.
[[nodiscard]] inline mat3 rotation_z(float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {{   c,    s, 0.0f},
            {  -s,    c, 0.0f},
            {0.0f, 0.0f, 1.0f}};
}

// ---- Measuring ----------------------------------------------------------------

/// The determinant: **the factor by which volumes change**, with a sign.
///
/// The 2-D story lifts directly. The unit *cube* has the three basis vectors as
/// its edges, so after the transformation it is the box spanned by the three
/// columns, and this is that box's signed volume. Every region scales by the same
/// factor because every region can be approximated by small cubes.
///
///   > 0  volumes scale by this much, handedness preserved
///   = 0  space is flattened onto a plane, a line or a point — no inverse
///   < 0  volumes scale by |det| and space is turned inside out
///
/// The sign now measures **handedness** rather than orientation-in-a-plane: a
/// negative determinant turns a right-handed set of axes into a left-handed one.
/// A model scaled by (-1, 1, 1) renders inside-out for exactly this reason, and
/// Lesson 3.4's back-face culling is where that stops being trivia.
///
/// Written as a cofactor expansion along the top row. Lesson 3.4 will show the
/// same number arriving as a triple product once the cross product exists.
[[nodiscard]] constexpr float determinant(const mat3& m)
{
    return m.c0.x * (m.c1.y * m.c2.z - m.c2.y * m.c1.z)
         - m.c1.x * (m.c0.y * m.c2.z - m.c2.y * m.c0.z)
         + m.c2.x * (m.c0.y * m.c1.z - m.c1.y * m.c0.z);
}

/// Rows become columns.
///
/// Same warning as in 2-D, and it matters more here: transposing equals inverting
/// **only** for a rotation. It is worth knowing that a product of rotations is
/// still a rotation, so `transpose` does invert a whole chain of them — but the
/// moment a scale joins the chain it stops, silently.
[[nodiscard]] constexpr mat3 transpose(const mat3& m)
{
    return {{m.c0.x, m.c1.x, m.c2.x},
            {m.c0.y, m.c1.y, m.c2.y},
            {m.c0.z, m.c1.z, m.c2.z}};
}

/// The transformation that undoes `m`, or all zeros if there is none.
///
/// The 2x2 formula had a memorable shape; the 3x3 one does not, and pretending
/// otherwise would be a disservice. It is the *adjugate* — the matrix of
/// cofactors, transposed — divided by the determinant. Each cofactor is the 2x2
/// determinant of what is left after deleting one row and one column, with a
/// checkerboard of signs.
///
/// You do not need to hold that in your head. You do need to know what it means
/// when it fails: a zero determinant says the transformation collapsed space onto
/// something lower-dimensional, so many inputs share one output and no inverse
/// can exist. Zeros are returned rather than NaN so the failure is a value you can
/// test — the same choice as `normalised()` (1.7), `barycentric_at` (2.3) and
/// `mat2`'s inverse (2.5).
[[nodiscard]] inline mat3 inverse(const mat3& m)
{
    // Named in WRITTEN notation — m01 is row 0, column 1 — because that is the
    // notation every derivation of this formula uses. Transcribing an adjugate
    // straight into column-stored members is how sign and index errors get in;
    // naming the elements first costs nine lines and removes the whole class.
    const float m00 = m.c0.x, m01 = m.c1.x, m02 = m.c2.x;
    const float m10 = m.c0.y, m11 = m.c1.y, m12 = m.c2.y;
    const float m20 = m.c0.z, m21 = m.c1.z, m22 = m.c2.z;

    // Cofactors: the 2x2 determinant left after deleting one row and one column,
    // with a checkerboard of signs. C_ij is named for the row and column deleted.
    const float c00 =  (m11 * m22 - m12 * m21);
    const float c01 = -(m10 * m22 - m12 * m20);
    const float c02 =  (m10 * m21 - m11 * m20);

    // Expanding along the top row with those three is the determinant, so it
    // comes out of work we are doing anyway.
    const float det = m00 * c00 + m01 * c01 + m02 * c02;
    if (det == 0.0f) { return {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}}; }

    const float c10 = -(m01 * m22 - m02 * m21);
    const float c11 =  (m00 * m22 - m02 * m20);
    const float c12 = -(m00 * m21 - m01 * m20);
    const float c20 =  (m01 * m12 - m02 * m11);
    const float c21 = -(m00 * m12 - m02 * m10);
    const float c22 =  (m00 * m11 - m01 * m10);

    // The adjugate is the cofactor matrix TRANSPOSED, so written-form element
    // (i, j) of the inverse is cofactor (j, i). Column k of the result is then
    // written-form column k read downwards.
    const float inv = 1.0f / det;
    return {{c00 * inv, c01 * inv, c02 * inv},
            {c10 * inv, c11 * inv, c12 * inv},
            {c20 * inv, c21 * inv, c22 * inv}};
}

// ---- A matrix as a QUANTITY, not a transformation -----------------------------
//
// Lesson 8.3, and the block below is the first thing in this file that is not
// about moving space around.
//
// For eighty lessons a `mat3` has been one thing: "where do the basis vectors
// land". Under that reading the operators below are meaningless — the sum of two
// rotations is not a rotation, half a shear is not half as sheared in any useful
// sense, and nothing in the renderer ever wanted either. That is exactly why
// they were never written.
//
// An INERTIA TENSOR is a mat3 under a different reading. It is not a map from
// vectors to vectors that happens to be useful; it is a *quantity* — the way a
// body's mass is spread out around its centre — which turns out to need nine
// numbers rather than one, and which is USED as a map (it takes an angular
// velocity to an angular momentum) only because that is what its nine numbers
// mean. Quantities add. Two lumps of matter bolted together have the sum of
// their tensors about a common point, for the same reason two lumps have the sum
// of their masses, and Lesson 8.3 §5 builds a compound body by doing exactly
// that.
//
// So `mat3` grows three arithmetic operators and one product, and the reason the
// arithmetic arrives eighty lessons late is not oversight. It is that until
// today there was nothing in this engine a matrix could be the *amount* of.

/// Element-wise sum. Meaningful for tensors; meaningless for transformations.
[[nodiscard]] constexpr mat3 operator+(const mat3& a, const mat3& b)
{
    return {a.c0 + b.c0, a.c1 + b.c1, a.c2 + b.c2};
}

/// Element-wise difference — the parallel-axis theorem runs in this direction
/// when it carries a tensor *back* to the centre of mass (8.3 §5).
[[nodiscard]] constexpr mat3 operator-(const mat3& a, const mat3& b)
{
    return {a.c0 - b.c0, a.c1 - b.c1, a.c2 - b.c2};
}

/// Negation, which is the difference from zero and is written out so that
/// `-outer(r, r)` reads the way the derivation does.
[[nodiscard]] constexpr mat3 operator-(const mat3& m)
{
    return {-m.c0, -m.c1, -m.c2};
}

[[nodiscard]] constexpr mat3 operator*(const mat3& m, float s)
{
    return {m.c0 * s, m.c1 * s, m.c2 * s};
}

[[nodiscard]] constexpr mat3 operator*(float s, const mat3& m) { return m * s; }

constexpr mat3& operator+=(mat3& a, const mat3& b)
{
    a.c0 += b.c0;
    a.c1 += b.c1;
    a.c2 += b.c2;
    return a;
}

constexpr mat3& operator-=(mat3& a, const mat3& b)
{
    a.c0 -= b.c0;
    a.c1 -= b.c1;
    a.c2 -= b.c2;
    return a;
}

constexpr mat3& operator*=(mat3& m, float s)
{
    m.c0 *= s;
    m.c1 *= s;
    m.c2 *= s;
    return m;
}

/// The **outer product** `a ⊗ b`: the matrix whose action is `v ↦ a (b · v)`.
///
/// The dot product of two vectors is a number. The outer product is the other
/// thing you can do with a pair of vectors, and it is a matrix — which sounds
/// like trivia until you meet the one place in this engine that needs it.
///
/// Read the definition rather than the formula. `(a ⊗ b) v` measures how much of
/// `v` lies along `b` (that is the dot product) and hands back that much of `a`.
/// Two consequences follow immediately and are worth having in your hands before
/// Lesson 8.3 §4 uses them:
///
///   * **Its rank is one.** Every output is a multiple of `a`, so the whole of
///     space is squashed onto a single line. Its determinant is therefore zero
///     and it has no inverse. An outer product is never a transformation you
///     would want; it is always a *term* in something else.
///   * **`a ⊗ a` is the projection onto `a`, times `|a|²`.** So
///     `|r|²·1 − r ⊗ r` — the expression at the centre of the inertia tensor —
///     is "keep everything, then remove the part along `r`", scaled by `|r|²`.
///     It annihilates `r` itself and leaves anything perpendicular to `r`
///     multiplied by `|r|²`, which is precisely the statement that a point mass
///     resists being spun about an axis through itself not at all, and about a
///     perpendicular axis by `m r²`.
///
/// Columns, as always, are the images of the basis vectors: `(a ⊗ b) x̂` keeps
/// `b.x` of `a`.
[[nodiscard]] constexpr mat3 outer(vec3 a, vec3 b)
{
    return {a * b.x, a * b.y, a * b.z};
}

/// The **cross-product matrix** `[w]ₓ`, whose action is `v ↦ w × v`.
///
/// Lesson 3.4's cross product, written as a matrix so it can be composed with
/// other matrices instead of only applied to vectors. Two uses, one in this
/// lesson and one waiting:
///
///   * 8.3 §4 checks its own derivation with the identity
///     `−[r]ₓ[r]ₓ = |r|²·1 − r ⊗ r`, which is the inertia tensor's integrand
///     arriving by a completely different route. Two expressions built from
///     different operators agreeing to the bit is worth more than either one
///     re-read carefully.
///   * 8.9's contact Jacobian is written in terms of `[r]ₓ`, because the
///     velocity of a point on a body is `v + ω × r` and that `×` has to become
///     a matrix before it can be a row of anything.
///
/// It is **antisymmetric**: `transpose(skew(w)) == -skew(w)`, which is the matrix
/// way of saying `w × v = −v × w`, and its diagonal is therefore zero.
[[nodiscard]] constexpr mat3 skew(vec3 w)
{
    return {{0.0f, w.z, -w.y},      // image of x̂ is w × x̂
            {-w.z, 0.0f, w.x},      // image of ŷ is w × ŷ
            {w.y, -w.x, 0.0f}};     // image of ẑ is w × ẑ
}

/// The sum of the diagonal.
///
/// For an inertia tensor this is not decoration: `trace(I) = 2 Σ m|r|²`, which is
/// twice the body's mass-weighted mean-square radius and is **invariant under any
/// rotation of the axes**. 8.3 §6 uses it as the cheapest possible check that a
/// basis change `R I Rᵀ` moved a tensor without changing what it is.
[[nodiscard]] constexpr float trace(const mat3& m)
{
    return m.c0.x + m.c1.y + m.c2.z;
}

/// A diagonal matrix from a vector of diagonal entries.
///
/// Numerically identical to `scale(d.x, d.y, d.z)` and deliberately a separate
/// name: a diagonal inertia tensor is not a scale, it is a body whose principal
/// axes happen to be the coordinate axes, and code that reads
/// `diagonal(moments)` says which of the two it meant.
[[nodiscard]] constexpr mat3 diagonal(vec3 d)
{
    return {{d.x, 0.0f, 0.0f}, {0.0f, d.y, 0.0f}, {0.0f, 0.0f, d.z}};
}

/// The largest absolute difference between `m` and its own transpose.
///
/// Zero exactly when `m` is symmetric. An inertia tensor is symmetric by
/// construction — `|r|²·1 − r ⊗ r` is, and sums of symmetric matrices are — so
/// any nonzero value here is arithmetic error or a bug, and 8.3's harness prints
/// it beside every tensor it builds.
[[nodiscard]] inline float asymmetry(const mat3& m)
{
    const float d0 = std::fabs(m.c1.x - m.c0.y);   // (0,1) against (1,0)
    const float d1 = std::fabs(m.c2.x - m.c0.z);   // (0,2) against (2,0)
    const float d2 = std::fabs(m.c2.y - m.c1.z);   // (1,2) against (2,1)
    return (d0 > d1) ? ((d0 > d2) ? d0 : d2) : ((d1 > d2) ? d1 : d2);
}

// ---- Comparison ---------------------------------------------------------------

[[nodiscard]] constexpr bool operator==(const mat3& a, const mat3& b)
{
    return a.c0 == b.c0 && a.c1 == b.c1 && a.c2 == b.c2;
}
[[nodiscard]] constexpr bool operator!=(const mat3& a, const mat3& b) { return !(a == b); }

} // namespace engine
