// src/gfx/obj.hpp — reading (and writing) Wavefront OBJ.
//
// Lesson 3.5. The rasterizer is finished and has nothing to draw. Three meshes so
// far: a cube, a quad, and an icosahedron, all typed by hand, all convex, all
// closed, all correctly wound — because we wound them. Every assumption Module 3
// leans on is an assumption about data we authored ourselves, which is no assumption
// at all. This file is where the engine starts reading geometry it did not write.
//
// THE FORMAT IS THE EASY PART, AND IT IS NOT THE LESSON. OBJ is line-oriented ASCII
// invented at Wavefront Technologies in the 1980s, and the whole of what we need is:
//
//     # a comment
//     v  0.5 -0.5 0.5          a POSITION           (1-based, in order of appearance)
//     vt 0.0 1.0               a TEXTURE COORDINATE (its own separate numbering)
//     vn 0.0 1.0 0.0           a NORMAL             (its own separate numbering again)
//     f  1/1/1 2/2/1 3/3/1     a FACE: corners, each naming one of each
//
// An afternoon's work. What deserves a lesson is the sentence hiding in that fourth
// line: **each corner names its three attributes independently.** A vertex buffer
// cannot do that. It has ONE index per vertex, and that index selects the position,
// the uv and the normal together — because the hardware fetches a vertex as a unit.
//
// So the loader's real job is a translation between two different ideas of what a
// vertex is, and the translation is not free: a position shared by two faces that
// disagree about its uv or its normal must become TWO vertices. That is why a cube
// exported from any tool on earth arrives with 24 vertices and not 8. Lesson 3.5 §3
// derives it, and `assets/cube.obj` demonstrates it in twenty readable lines.
//
// WHAT WE DO NOT SUPPORT, said plainly rather than discovered later:
//
//   - Materials. `mtllib` and `usemtl` are parsed as "a line we skip", and the .mtl
//     file is not opened. Materials are Module 6, and reading them now would mean
//     inventing a material system to put them in.
//   - Smoothing groups (`s`), object and group names (`o`, `g`), lines (`l`) and
//     points (`p`): skipped and counted, so the report says how much of the file we
//     ignored rather than pretending it was not there.
//   - Backslash line continuation. Legal in the format, essentially unused by real
//     exporters; a continued line will be read as two malformed lines and reported
//     as such rather than silently mis-parsed.
//   - Free-form geometry (`curv`, `surf`, and friends). Nobody ships these.

#pragma once

#include "gfx/mesh.hpp"

#include <string>
#include <string_view>

namespace engine {

/// How a load ended. `ok` is the only success.
///
/// **This is an enum and a line number, not an exception and not a string.** The
/// engine core does not throw (CLAUDE.md §4), for a reason worth stating once here
/// and properly in Module 5: an exception crossing a library boundary drags in the
/// unwind tables, forbids `noexcept`, and — most importantly for a renderer — makes
/// the cost of an error path invisible at the call site. An enum makes the caller
/// write the branch.
///
/// It is also NOT a general-purpose `Result<T, E>`. Building one of those here would
/// be inventing a language feature to solve a problem we have exactly once. When
/// Module 5 has ten loaders that all need the same shape, that is when the shape
/// gets a name.
enum class obj_status
{
    ok,                 ///< the file parsed and produced drawable geometry
    cannot_open,        ///< the file is missing, unreadable, or empty
    no_faces,           ///< parsed, but contains no `f` statements: nothing to draw
    bad_face,           ///< a face with fewer than three corners, or a corner we
                        ///< could not read. The file is not what it claims to be.
    bad_index,          ///< a face names a position/uv/normal that does not exist
    too_many_vertices   ///< more unified vertices than a uint16 index can name
};

[[nodiscard]] const char* name_of(obj_status s);

/// Everything the loader learned, whether or not it succeeded.
///
/// A bare `bool` would answer "did it work". These fields answer the questions you
/// actually ask when a model looks wrong: how much did the file contain, how much
/// did we keep, and how far did the two diverge? `split_vertices` in particular is
/// the index problem measured on this exact file — see §3.
struct obj_report
{
    obj_status status = obj_status::cannot_open;
    int line = 0;   ///< 1-based line the failure was on; 0 when not line-specific

    // ---- What the file contained -----------------------------------------
    int positions = 0;      ///< `v` statements
    int uvs = 0;            ///< `vt` statements
    int normals = 0;        ///< `vn` statements
    int faces = 0;          ///< `f` statements
    int ngons = 0;          ///< …of which had more than three corners
    int max_corners = 0;    ///< the largest face seen, for a sense of how wild it got
    int skipped_lines = 0;  ///< recognised-but-ignored keywords (o, g, s, usemtl, …)
    int unknown_lines = 0;  ///< keywords we do not know at all

    // ---- What we built ----------------------------------------------------
    int vertices = 0;       ///< unified vertices produced
    int triangles = 0;      ///< after fan triangulation
    int degenerate = 0;     ///< triangles dropped for naming a corner twice
    int split_vertices = 0; ///< vertices - positions: the cost of the index problem
    int reused_corners = 0; ///< face corners that matched an existing vertex

    [[nodiscard]] bool ok() const { return status == obj_status::ok; }
};

/// Parse OBJ text into owning geometry.
///
/// Separate from `load_obj` on purpose, and the purpose is not abstraction for its
/// own sake: a parser that takes bytes can be tested from a string literal, with no
/// file, no path, and no filesystem state — which is how every awkward case in
/// Lesson 3.5's §8 gets tested. It is also the shape Module 5's asset system needs,
/// where bytes may come from a pack file rather than a path.
///
/// `out` is CLEARED first, and it is an out-parameter rather than a return value
/// because `mesh_data` owns heap arrays: filling a caller-provided object lets the
/// caller keep its capacity across loads, and makes it impossible to accidentally
/// return a `mesh` view of a local (which would compile, and dangle).
///
/// On failure `out` may hold partial data; the caller should not draw it. Saying so
/// is better than promising a rollback we would have to implement and test.
[[nodiscard]] obj_report parse_obj(std::string_view text, mesh_data& out);

/// Read a file from disk and parse it. Uses `SDL_LoadFile`, so the path handling is
/// SDL's on every platform and the buffer arrives null-terminated.
[[nodiscard]] obj_report load_obj(const char* path, mesh_data& out);

/// Write a mesh out as OBJ. Returns false and leaves `SDL_GetError()` set on failure.
///
/// A writer is fifty lines and it earns them twice over. It is how `assets/torus.obj`
/// was made, so the course ships no third-party geometry and every number in the
/// lesson is reproducible. And it makes the **round trip** testable: generate a mesh,
/// write it, read it back, and compare the triangles corner for corner. A reader
/// alone can only be checked against expectations you typed by hand.
///
/// It COMPACTS as it writes — each distinct position, uv and normal appears once,
/// with faces naming them independently — which is what a real exporter does, and
/// which means the round trip exercises the index problem in both directions.
[[nodiscard]] bool save_obj(const char* path, const mesh& m);

/// Turn a path relative to the executable into an absolute one.
///
/// Assets sit next to the binary because CMake copies them there after every build
/// (see CMakeLists.txt), and the binary finds them with `SDL_GetBasePath()` — never
/// with a path relative to the CURRENT WORKING DIRECTORY, which is wherever the user
/// happened to be standing when they launched the program. That distinction is the
/// difference between "works when I run it from the build folder" and "works".
///
/// This is a placeholder for Module 5's asset system, which will own a search path,
/// a virtual root, and handles. It is one function here because one function is what
/// the problem currently is.
[[nodiscard]] std::string asset_path(const char* relative);

} // namespace engine
