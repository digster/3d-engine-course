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

// Lesson 3.5 adds the other half of the story: geometry that OWNS its arrays.
// Everything above describes a VIEW, which is exactly right for data compiled into
// the program and exactly wrong for data read off a disk. `mesh_data` at the bottom
// of this file is the owning form, and `validate()` is the tool that decides whether
// geometry we did not author is safe to draw.

#pragma once

#include "math/vec2.hpp"
#include "math/vec3.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

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

    /// Texture coordinates, one per position — or **empty**, meaning this mesh has
    /// none. Added in Lesson 3.2, where a surface finally needs something painted
    /// on it that varies faster than its corners.
    ///
    /// A PARALLEL ARRAY rather than an interleaved `struct vertex { pos; uv; }`,
    /// and that is a real choice with a real trade. Parallel arrays let a mesh
    /// carry an attribute some consumers ignore without paying for it — the cube
    /// and icosahedron have no uvs and store none. Interleaving puts a vertex's
    /// data in one cache line, which is what the GPU wants and what Module 4's
    /// vertex buffers will use. We keep parallel arrays while the meshes are
    /// static data and the reader is a CPU loop, and Module 4 revisits it with the
    /// memory-layout diagram the decision deserves.
    ///
    /// **Empty is a valid state, not a missing one.** `uvs.empty()` means "this
    /// geometry has no texture coordinates", and a renderer that wants them
    /// substitutes zero rather than failing.
    std::span<const vec2> uvs;

    /// Surface normals, one per position — or **empty**, meaning this mesh has none.
    /// Added in Lesson 3.5 for a reason worth being honest about: **nothing draws
    /// with them yet.** Lesson 3.6 turns a normal into a brightness; today they are
    /// here only because the file on disk contains them, and a loader that reads a
    /// file and throws away a third of it has to read it again later.
    ///
    /// They are not inert, though. A normal participates in deciding *what a vertex
    /// is* — two faces meeting at a corner with different normals need two vertices
    /// there, even though the position is one point. That is why a cube's vertex
    /// buffer holds 24 vertices and not 8, and Lesson 3.5 §3 measures exactly that.
    ///
    /// In MODEL space, like the positions, and **not necessarily unit length**: a
    /// file may contain whatever it contains. Lesson 3.6 normalises at the point of
    /// use, which is also where the normal matrix problem (a non-uniform scale does
    /// not transform normals the way it transforms points) finally has to be faced.
    std::span<const vec3> normals;

    /// Number of triangles. Three indices each, so this is simply the count / 3.
    [[nodiscard]] constexpr std::size_t triangle_count() const { return indices.size() / 3; }

    /// The uv at vertex `i`, or `(0,0)` if this mesh carries none.
    [[nodiscard]] constexpr vec2 uv_at(std::size_t i) const
    {
        return i < uvs.size() ? uvs[i] : vec2{};
    }

    /// The normal at vertex `i`, or `(0,0,0)` if this mesh carries none.
    ///
    /// Zero is a deliberate choice of "absent" rather than, say, `(0,1,0)`: a zero
    /// vector has no direction, so any shading term built from it is visibly wrong
    /// rather than plausibly wrong. A default of "up" would light every unshaded
    /// mesh as though the sun were overhead, which is the kind of bug that ships.
    [[nodiscard]] constexpr vec3 normal_at(std::size_t i) const
    {
        return i < normals.size() ? normals[i] : vec3{};
    }
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
///
/// Designated initialisers, as of Lesson 3.5: `mesh` has four members now and only
/// two of them apply here. Listing them by name means adding a fifth attribute one
/// day cannot silently shift a value into the wrong field — and it silences
/// `-Wmissing-field-initializers`, which was right to complain.
[[nodiscard]] inline mesh cube_mesh()
{
    return mesh{.vertices = k_cube_vertices, .indices = k_cube_indices};
}

// ---- The unit quad ---------------------------------------------------------

// Four corners in the z = 0 plane, two triangles, wound counter-clockwise seen
// from +z — so its front face looks along +z, in the direction its own normal
// points. Added in Lesson 3.1, where the failure scenes need flat panels that can
// be tilted independently: a cube cannot be arranged into a depth-sorting cycle,
// because a cycle needs surfaces that pass over and under one another.
//
// It will earn its keep again in 3.7 (a texture wants somewhere flat to live) and
// in Module 6 (a full-screen post-processing pass is one quad).
inline constexpr vec3 k_quad_vertices[4] = {
    {-0.5f, -0.5f, 0.0f},   // 0  bottom-left
    {+0.5f, -0.5f, 0.0f},   // 1  bottom-right
    {+0.5f, +0.5f, 0.0f},   // 2  top-right
    {-0.5f, +0.5f, 0.0f},   // 3  top-left
};

inline constexpr std::uint16_t k_quad_indices[6] = {
    0, 1, 2,   0, 2, 3,
};

// The unit square of texture space, laid on the quad so that +u runs with +x and
// +v runs with +y. Note that this is a MODEL-space convention, not a screen one:
// which way "up" ends up on screen depends entirely on where the quad is put.
inline constexpr vec2 k_quad_uvs[4] = {
    {0.0f, 0.0f},   // 0  bottom-left
    {1.0f, 0.0f},   // 1  bottom-right
    {1.0f, 1.0f},   // 2  top-right
    {0.0f, 1.0f},   // 3  top-left
};

/// A 1x1 square in the z = 0 plane, centred on the origin, with uvs over [0,1].
[[nodiscard]] inline mesh quad_mesh()
{
    return mesh{.vertices = k_quad_vertices, .indices = k_quad_indices, .uvs = k_quad_uvs};
}

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
    return mesh{.vertices = k_icosahedron_vertices, .indices = k_icosahedron_indices};
}

// ---- Geometry that owns itself ---------------------------------------------
//
// Lesson 3.5. Every mesh above views `inline constexpr` arrays with program
// lifetime, so `mesh`'s spans can never dangle. Geometry read from a file has no
// such guarantee: somebody has to hold the arrays, and "somebody" has to be a type,
// because the alternative is a pile of `std::vector`s in whatever function happened
// to do the loading. Lesson 3.2's `floor_geometry` was exactly that pile, and it was
// admitted at the time to be the pressure that produces this.

/// Geometry that OWNS its arrays: the form a loaded mesh arrives in.
///
/// The relationship to `mesh` is the one C++ draws everywhere and is worth naming
/// once: `std::string` owns, `std::string_view` views; `std::vector` owns,
/// `std::span` views. `mesh_data` owns, `mesh` views. Functions that merely *read*
/// geometry take a `mesh` and so work on both; only the loader needs the owner.
///
/// The four arrays are **parallel and index-aligned**: `vertices[i]`, `uvs[i]` and
/// `normals[i]` are the position, texture coordinate and normal of one vertex, and
/// `indices` names vertices in triples. `uvs` and `normals` may be empty, meaning
/// the geometry carries none; if non-empty they must be exactly as long as
/// `vertices`, which `view()` quietly relies on and `validate()` checks.
///
/// **Copyable, movable, and cheap to move** — it is four vectors and nothing else,
/// so the compiler-generated special members are all correct. That is the RAII
/// bargain (Lesson 0.6): own resources in members that manage themselves and you
/// write no destructor, no copy constructor, and no assignment operator, and you
/// cannot leak. The moment you write one of the five by hand you owe all of them.
struct mesh_data
{
    std::vector<vec3> vertices;
    std::vector<vec2> uvs;
    std::vector<vec3> normals;
    std::vector<std::uint16_t> indices;

    /// A non-owning view of this data. **The view dies with the owner** — the usual
    /// span rule, and the reason `view()` is not called on a temporary anywhere in
    /// this codebase. Returning a `mesh` from a function that built a `mesh_data`
    /// locally would compile and dangle; the loader therefore fills an out-parameter
    /// the caller owns, rather than returning geometry by value.
    [[nodiscard]] mesh view() const { return {vertices, indices, uvs, normals}; }

    /// Drop everything, keeping the allocated capacity for the next load.
    void clear()
    {
        vertices.clear();
        uvs.clear();
        normals.clear();
        indices.clear();
    }

    [[nodiscard]] std::size_t triangle_count() const { return indices.size() / 3; }
};

/// Convert texture coordinates from **OBJ's** convention to **the texture's**:
/// `v -> 1 - v`. Lesson 3.9.
///
/// Two conventions, both entirely reasonable, disagreeing about one axis:
///
///   - **Wavefront OBJ** puts `(0, 0)` at the **bottom** left of the image and
///     `v` increases **upwards**. That is the mathematician's convention, and it is
///     what every exporter writes.
///   - **SDL_GPU** puts `(0, 0)` at the **top** left and `v` increases
///     **downwards**. `SDL3/SDL_gpu.h`, "Coordinate System": *"Texture Coordinates:
///     The top-left corner has an x,y coordinate of (0, 0) and extends to the
///     bottom-right corner at (1.0, 1.0). +Y is down."*
///
/// So the two differ by a vertical flip, and something has to do it. **This
/// function is where**, and where it is *not* is the more interesting half:
///
///   - **not in the parser.** Lesson 3.5 settled that a loader stores what the file
///     says, so that two loads of a file are diffable and a loader is never a place
///     where data quietly differs from its source. `load_obj` still stores `vt`
///     verbatim.
///   - **not in the sampler.** The sampler is SDL_GPU's, and SDL_GPU will not flip
///     anything for us. A sampler that "helpfully" flipped would be correct in this
///     renderer and wrong in Module 4's, which is the worst possible place to hide
///     a convention.
///
/// It belongs to the **import step** — the boundary where somebody else's data
/// becomes ours — which is exactly where every real engine puts it. Assimp calls it
/// `aiProcess_FlipUVs`; Unity and Unreal do it on import and never mention it again.
///
/// **Correct for tiled coordinates too**, which is not obvious. `1 - v` is a
/// reflection of the whole `v` axis about `0.5`, so a coordinate of 2.5 (two and a
/// half tiles up) becomes -1.5, whose fractional part under `repeat` is 0.5 — half
/// way down a tile, which is the same point of the same tile. The flip commutes with
/// wrapping, so it can be applied once at load and forgotten.
///
/// A mesh with no uvs is left alone; there is nothing to flip.
void flip_uv_v(mesh_data& m);

/// **The index-space ceiling.** `mesh::indices` is `std::uint16_t`, so a mesh can
/// name at most 65,536 distinct vertices. That is not a limitation we invented: GPU
/// index buffers come in exactly these two widths, and SDL_GPU spells them
/// `SDL_GPU_INDEXELEMENTSIZE_16BIT` and `_32BIT` (Module 4). Sixteen bits halves the
/// bandwidth of every index fetch, which is why it is still worth having.
///
/// A loader's job is to **notice** when a file exceeds it. Silently wrapping at
/// 65,536 produces triangles that connect unrelated corners — geometry that looks
/// like a bag of glass shards and gives no clue why.
inline constexpr std::size_t k_max_mesh_vertices = 65536;

// ---- Validation ------------------------------------------------------------
//
// Lesson 2.12 checked the hand-typed icosahedron against four properties — Euler's
// formula, every edge shared by exactly two faces, uniform vertex degree, outward
// winding — and did it in prose, by hand, because the data was twenty lines long and
// we had written it ourselves. Lesson 3.5 makes those checks a function, because the
// data is now four thousand lines long and we did not.

/// What `validate()` found. Every field is a count you can act on, not a verdict.
///
/// The two booleans at the end are conveniences derived from the counts; the counts
/// are the useful part, because "this mesh has 4 boundary edges" tells you where to
/// look and "invalid" does not.
struct mesh_report
{
    // ---- As stored -------------------------------------------------------
    int vertices = 0;          ///< entries in the vertex array
    int triangles = 0;         ///< index triples
    int unused_vertices = 0;   ///< present in the array, named by no triangle
    int out_of_range = 0;      ///< indices naming a vertex that does not exist
    int degenerate = 0;        ///< triangles with a repeated index, or zero area

    // ---- After welding by position ---------------------------------------
    //
    // Two vertices at the same POSITION are one point of the surface even when the
    // arrays store them separately — which they must, whenever the two carry
    // different uvs or normals (§3 of the lesson). Topology is a property of the
    // surface, so every question below is asked of the WELDED graph. Ask them of the
    // stored arrays instead and a perfectly watertight model reports a seam-shaped
    // hole, which is one of the most confusing false alarms in asset pipelines.
    int welded_vertices = 0;   ///< distinct positions
    int split_vertices = 0;    ///< vertices - welded_vertices: the index problem, counted
    int edges = 0;             ///< distinct undirected edges of the welded graph
    int euler = 0;             ///< V - E + F. 2 for a sphere, 0 for a torus (§6.3)
    int boundary_edges = 0;    ///< used by exactly one triangle: the surface has a rim
    int nonmanifold_edges = 0; ///< used by three or more: not a surface at all
    int reversed_edges = 0;    ///< used twice in the SAME direction: winding disagrees

    /// Total signed volume enclosed, by the divergence theorem (§6.4). Meaningful
    /// only when the surface is closed; **positive means wound outward**, which is
    /// the property Lesson 3.4's back-face culling depends on and cannot check.
    float signed_volume = 0.0f;

    /// No rim, no non-manifold junctions: the surface encloses a region.
    [[nodiscard]] bool closed() const
    {
        return boundary_edges == 0 && nonmanifold_edges == 0 && triangles > 0;
    }

    /// Every shared edge traversed once in each direction — the definition of
    /// consistent winding, and the thing a mesh from disk most often lacks.
    [[nodiscard]] bool consistently_wound() const { return reversed_edges == 0; }
};

/// Measure a mesh against the properties a renderer quietly assumes it has.
///
/// Costs one hash-map pass over the positions and one over the edges, so it is a
/// load-time or tool-time check — not something to run per frame. Which is the
/// normal shape of validation in an engine: pay once, at the boundary where
/// untrusted data enters, and let everything downstream assume.
[[nodiscard]] mesh_report validate(const mesh& m);

// ---- A procedural mesh worth loading ---------------------------------------

/// A torus in the y-up world: a tube of radius `minor` swept around a ring of radius
/// `major`, lying in the xz plane. Wound counter-clockwise seen from outside.
///
/// Built rather than typed, because at 48 x 24 it is 2,304 triangles and nobody
/// types that. It is here for four reasons, each of which a cube fails at:
///
///   - **Non-convex.** Parts of it hide other parts, so the z-buffer does work no
///     sort could fake, and back-face culling has something real to bite on.
///   - **It has a uv seam.** Going once around, `u` runs 0 → 1 and then must be 0
///     again — but the same vertex cannot hold both. Splitting that seam is the
///     index problem in its purest form, and this mesh contains it by construction.
///   - **Its Euler characteristic is 0, not 2.** A torus is not a sphere, and a
///     validator that "knows" V - E + F = 2 is about to learn what that formula is
///     actually a statement about (§6.3).
///   - **Its volume has a closed form**, 2·pi²·major·minor², so the signed-volume
///     check has an exact number to converge to rather than a plausible one.
///
/// @param major_segments  divisions around the ring   (>= 3)
/// @param minor_segments  divisions around the tube   (>= 3)
[[nodiscard]] mesh_data make_torus(int major_segments, int minor_segments,
                                   float major_radius, float minor_radius);

} // namespace engine
