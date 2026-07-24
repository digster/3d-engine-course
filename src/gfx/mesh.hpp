// src/gfx/mesh.hpp — geometry as a vertex array plus an index array.
//
// Every mesh the engine has drawn so far has been eight hard-coded corners and a
// hand-written edge list. That was fine for a cube and stops being fine the moment
// a shape has real structure. Lesson 2.12 introduces the representation every real
// renderer uses, and — importantly — the one the GPU wants in Module 4:
//
//     VERTICES   an array of positions, each appearing ONCE
//     INDICES    an array of integers, in triples, each triple naming a triangle
//
// Why the indirection? Because vertices are shared. An icosahedron has 12 corners
// and 20 triangular faces; if each face carried its own three positions we would
// store 60 vertices instead of 12, and transform 60 instead of 12 every frame. The
// index array buys a 5x reduction here and much more on real meshes, which is why
// Module 4's vertex and index BUFFERS have exactly this shape.
//
// Triangles, not edges, even though we are still drawing wireframe. A triangle list
// is what a mesh actually is: Module 3 fills these same triangles, Module 3.4 culls
// them by winding, Module 4 uploads them. Drawing a wireframe from triangles means
// each shared edge is drawn twice — honest waste we name rather than hide, and which
// disappears the moment the triangles are filled.

#pragma once

#include "math/vec3.hpp"

#include <cstdint>
#include <span>

namespace engine {

/// A non-owning view of some geometry: positions, and triples of indices into them.
///
/// Both members are `std::span`, which is C++20's "pointer and a length" — a VIEW,
/// not a container. A `mesh` owns nothing, copies in a few words, and cannot outlive
/// the arrays it points at. That is exactly right here, where the geometry is static
/// data with program lifetime; it will stop being right in Module 5, when meshes are
/// loaded at runtime and something has to own them. The handle-based asset system
/// there is the answer, and this is the "before" picture.
struct mesh
{
    std::span<const vec3> vertices;             ///< positions, in MODEL space
    std::span<const std::uint16_t> indices;     ///< triples; each triple is a triangle

    /// Number of triangles. Three indices each, so this is simply the count / 3.
    [[nodiscard]] constexpr std::size_t triangle_count() const { return indices.size() / 3; }
};

// ---- The unit cube ---------------------------------------------------------

// Eight corners, centred on the origin so rotation spins it in place (Lesson 2.5
// §3.8), and twelve triangles — two per face. `inline` because these live in a
// header that more than one translation unit may include; without it each would be
// a separate object and the One Definition Rule would be violated.
inline constexpr vec3 k_cube_vertices[8] = {
    {-0.5f, -0.5f, -0.5f}, {+0.5f, -0.5f, -0.5f}, {+0.5f, +0.5f, -0.5f}, {-0.5f, +0.5f, -0.5f},
    {-0.5f, -0.5f, +0.5f}, {+0.5f, -0.5f, +0.5f}, {+0.5f, +0.5f, +0.5f}, {-0.5f, +0.5f, +0.5f},
};

// Every triangle is wound COUNTER-CLOCKWISE as seen from OUTSIDE the cube, which is
// this course's front-face convention (conventions.html §7). Nothing enforces that
// today — a wireframe does not care which way a triangle faces — but Lesson 3.4's
// back-face culling will discard every triangle wound the other way, so getting it
// right now means the mesh does not have to be re-authored then.
inline constexpr std::uint16_t k_cube_indices[36] = {
    0, 3, 2,   0, 2, 1,     // back   (-z)
    4, 5, 6,   4, 6, 7,     // front  (+z)
    0, 4, 7,   0, 7, 3,     // left   (-x)
    1, 2, 6,   1, 6, 5,     // right  (+x)
    3, 7, 6,   3, 6, 2,     // top    (+y)
    0, 1, 5,   0, 5, 4,     // bottom (-y)
};

/// The unit cube, as a mesh.
[[nodiscard]] inline mesh cube_mesh() { return {k_cube_vertices, k_cube_indices}; }

// ---- The icosahedron -------------------------------------------------------

// Twelve vertices, twenty faces, and not one arbitrary number in it.
//
// The construction is one of the loveliest in solid geometry: take three identical
// rectangles, each of width 1 and height phi (the GOLDEN RATIO), and stand them at
// right angles to one another through a common centre. The twelve corners of those
// three rectangles ARE the twelve vertices of a regular icosahedron. In coordinates
// that is every cyclic permutation of (0, +-1, +-phi).
//
//     phi = (1 + sqrt(5)) / 2 = 1.6180339887...
//
// Why phi and not some other proportion? Because it is the shape that makes all
// thirty edges the same length — the rectangle's short side (1, an edge of the
// rectangle) has to equal the distance between corners of two different rectangles,
// and solving that gives phi exactly. Lesson 2.12 §3.2 does the algebra.
//
// The values below are the vertices already divided by their common length,
// sqrt(1 + phi^2) = 1.9021130326..., so every vertex sits exactly one unit from the
// origin: the icosahedron is inscribed in the unit sphere. Scaling it is then the
// transform's job, not the data's.
inline constexpr float k_icos_a = 0.5257311121f;   ///< 1     / sqrt(1 + phi^2)
inline constexpr float k_icos_b = 0.8506508084f;   ///< phi   / sqrt(1 + phi^2)

inline constexpr vec3 k_icosahedron_vertices[12] = {
    {-k_icos_a, +k_icos_b, 0.0f},   //  0
    {+k_icos_a, +k_icos_b, 0.0f},   //  1
    {-k_icos_a, -k_icos_b, 0.0f},   //  2
    {+k_icos_a, -k_icos_b, 0.0f},   //  3
    {0.0f, -k_icos_a, +k_icos_b},   //  4
    {0.0f, +k_icos_a, +k_icos_b},   //  5
    {0.0f, -k_icos_a, -k_icos_b},   //  6
    {0.0f, +k_icos_a, -k_icos_b},   //  7
    {+k_icos_b, 0.0f, -k_icos_a},   //  8
    {+k_icos_b, 0.0f, +k_icos_a},   //  9
    {-k_icos_b, 0.0f, -k_icos_a},   // 10
    {-k_icos_b, 0.0f, +k_icos_a},   // 11
};

// Twenty faces, all wound counter-clockwise seen from outside. Grouped the way the
// solid is actually built: five triangles meeting at vertex 0, five more filling the
// gaps below them, then the same twice more around vertex 3. Every vertex belongs to
// exactly five triangles — that is what "regular" means here — and the whole list
// satisfies Euler's formula, 12 - 30 + 20 = 2.
inline constexpr std::uint16_t k_icosahedron_indices[60] = {
    0, 11,  5,    0,  5,  1,    0,  1,  7,    0,  7, 10,    0, 10, 11,   // around vertex 0
    1,  5,  9,    5, 11,  4,   11, 10,  2,   10,  7,  6,    7,  1,  8,   // the gaps below
    3,  9,  4,    3,  4,  2,    3,  2,  6,    3,  6,  8,    3,  8,  9,   // around vertex 3
    4,  9,  5,    2,  4, 11,    6,  2, 10,    8,  6,  7,    9,  8,  1,   // the gaps above
};

/// A regular icosahedron inscribed in the unit sphere, as a mesh.
[[nodiscard]] inline mesh icosahedron_mesh()
{
    return {k_icosahedron_vertices, k_icosahedron_indices};
}

} // namespace engine
