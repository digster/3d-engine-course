// src/gfx/gpu_mesh.hpp — a mesh from Lesson 3.5, in the shape the hardware wants.
//
// Lesson 4.5. Everything the engine has loaded since Lesson 3.5 is stored as
// PARALLEL ARRAYS — positions here, normals there, uvs somewhere else, plus a list
// of indices. That is the right shape for a CPU loop that touches one attribute at
// a time, and `mesh.hpp` said so at the time, with a promise:
//
//     "Interleaving puts a vertex's data in one cache line, which is what the GPU
//      wants and what Module 4's vertex buffers will use. We keep parallel arrays
//      while the meshes are static data and the reader is a CPU loop, and Module 4
//      revisits it with the memory-layout diagram the decision deserves."
//
// This file is that revisit. It converts, once, at load time, and the conversion is
// the whole content of `interleave` below — twenty lines, no cleverness, and the
// reason it is worth a lesson is that the *choice* has consequences the code does
// not show:
//
//   INTERLEAVED   p n u | p n u | p n u        one vertex is contiguous
//   SEPARATE      p p p | n n n | u u u        one ATTRIBUTE is contiguous
//
// The vertex shader reads a whole vertex. Interleaved, that is one cache line and
// one memory transaction; separate, it is three lines from three distant addresses,
// and Lesson 4.1 measured what that costs on a CPU. SDL_GPU supports both — a
// separate layout is three `vertex_buffer` slots with one attribute each — and
// interleaved is what this engine uses, for exactly one exception's worth of
// nuance recorded in the lesson.

#pragma once

#include "gfx/gpu_buffer.hpp"
#include "gfx/gpu_pipeline.hpp"
#include "gfx/mesh.hpp"

#include <SDL3/SDL.h>

#include <cstddef>
#include <vector>

namespace engine {

/// One vertex, interleaved: position, normal, texture coordinate. **32 bytes.**
///
/// The size is not an accident and it is not a coincidence either — it is the
/// result of choosing three attributes that a surface actually needs and then
/// checking the arithmetic (12 + 12 + 8). Thirty-two bytes is half of the
/// sixty-four-byte cache line Lesson 4.1 measured, so two consecutive vertices
/// share one line exactly, with nothing wasted at either end.
///
/// **Declared here, next to the layout that describes it** — `gpu_mesh::describe`
/// is four lines below — because Lesson 4.4's hard-won lesson was that the vertex
/// gets declared twice and nothing checks the halves. We cannot do anything about
/// the HLSL half living in another language, but we can refuse to let the two C++
/// halves drift apart by putting them in the same screen.
struct gpu_vertex_pnu
{
    float px, py, pz;   ///< position, MODEL space
    float nx, ny, nz;   ///< normal, MODEL space, unit length after `interleave`
    float u, v;         ///< texture coordinate, already flipped for SDL_GPU (3.9)
};

// The layout always declares `sizeof(gpu_vertex_pnu)` as its pitch, so padding
// could not break the rendering — it would only make the memory-layout figure in
// Lesson 4.5 a lie. Assert anyway: a diagram that stops matching the code is a
// worse bug than one that never did, because it is believed.
static_assert(sizeof(gpu_vertex_pnu) == 32, "gpu_vertex_pnu is expected to be 32 bytes");

/// Whether the device-side copy keeps the index buffer or throws it away.
///
/// `expanded` exists to be MEASURED, not to be used: it writes three vertices per
/// triangle with no index buffer, which is what a renderer without index buffers
/// would have to do. Lesson 4.5 §5 draws the same mesh both ways and compares the
/// byte counts and the vertex-shader invocations, because "indices save memory" is
/// a claim, and a claim about your own mesh is a number.
enum class index_mode
{
    indexed,
    expanded
};

/// Convert Lesson 3.5's parallel arrays into one interleaved array.
///
/// Normals are normalised on the way through — the loader stores what the file
/// said (Lesson 3.5's rule) and a file may contain anything — and a mesh with no
/// normals or no uvs gets zeros, which `mesh::normal_at` and `mesh::uv_at` already
/// define as the meaning of "absent".
///
/// `out` is an out-parameter rather than a return value for the reason `load_obj`
/// is: the caller owns the storage, and a `mesh` view built on a local vector is a
/// dangling span waiting to happen.
void interleave(const mesh& m, std::vector<gpu_vertex_pnu>& out);

/// Expand an indexed mesh into a flat triangle list — three vertices per triangle,
/// every shared vertex duplicated once per triangle that uses it.
///
/// The "before" picture for Lesson 4.5 §5, and a thing you occasionally really do
/// want: flat shading without a geometry shader needs per-face data, and per-face
/// data cannot live on a shared vertex. Lesson 3.8 hit exactly this problem on the
/// CPU and solved it by shading per triangle instead.
void expand(const mesh& m, std::vector<gpu_vertex_pnu>& out);

/// A mesh living on the device: one interleaved vertex buffer, and (in `indexed`
/// mode) one 16-bit index buffer.
///
/// Move-only, like every device resource in this engine.
class gpu_mesh
{
public:
    gpu_mesh() = default;
    ~gpu_mesh();

    gpu_mesh(const gpu_mesh&) = delete;
    gpu_mesh& operator=(const gpu_mesh&) = delete;

    gpu_mesh(gpu_mesh&& other) noexcept;
    gpu_mesh& operator=(gpu_mesh&& other) noexcept;

    /// Interleave, allocate, and record both uploads into `cb`.
    ///
    /// The caller submits `cb`; this function only records, which is the same
    /// split every upload in this engine has used since Lesson 4.2 and lets one
    /// command buffer carry every mesh in a scene.
    ///
    /// **Refuses a mesh with more than 65,536 vertices in `indexed` mode**, because
    /// the index buffer is 16-bit. `mesh.hpp`'s `k_max_mesh_vertices` has said so
    /// since Lesson 3.5; this is the line that finally depends on it.
    [[nodiscard]] bool create(const gpu_device& dev, SDL_GPUCommandBuffer* cb,
                              const mesh& m, index_mode mode, const char* name);

    void destroy();

    /// Bind the vertex buffer at `slot`, and the index buffer if there is one.
    ///
    /// Does NOT bind the pipeline, and does not bind slot 1 — a per-instance
    /// buffer belongs to the *scene*, not to the mesh, and a mesh that bound one
    /// would be deciding how many copies of itself exist.
    void bind(SDL_GPURenderPass* pass, Uint32 slot = 0) const;

    /// Issue the draw — indexed or not, according to how it was created.
    ///
    /// This is the one place in the engine that knows which of the two draw calls
    /// applies, which is the point of storing the mode rather than the caller
    /// remembering it.
    void draw(SDL_GPURenderPass* pass, Uint32 instances = 1) const;

    /// Describe `gpu_vertex_pnu` to a pipeline: one buffer at `slot`, three
    /// attributes at locations 0, 1 and 2.
    ///
    /// `offsetof`, never a literal — Lesson 4.4's rule, and the reason is that a
    /// literal 12 is correct right up until somebody inserts a field above it, at
    /// which point every vertex reads the wrong bytes and nothing reports an error.
    static pipeline_desc& describe(pipeline_desc& desc, Uint32 slot = 0);

    [[nodiscard]] bool valid() const { return vertices_.valid(); }
    [[nodiscard]] index_mode mode() const { return mode_; }
    [[nodiscard]] Uint32 vertex_count() const { return vertex_count_; }
    [[nodiscard]] Uint32 index_count() const { return index_count_; }
    [[nodiscard]] Uint32 vertex_bytes() const { return vertices_.size(); }
    [[nodiscard]] Uint32 index_bytes() const { return indices_.size(); }
    [[nodiscard]] Uint32 total_bytes() const { return vertices_.size() + indices_.size(); }

    /// **Fewest** vertex-shader invocations this draw could possibly need.
    ///
    /// For an indexed mesh that is the number of distinct vertices — reached only
    /// if the post-transform cache catches every reuse, which depends on the order
    /// the triangles are listed in. For an expanded mesh it is three per triangle,
    /// because there is no reuse to catch.
    [[nodiscard]] Uint32 best_case_invocations() const { return vertex_count_; }

    /// **Most** it could need: one per index, i.e. a cache that never hits.
    ///
    /// The two numbers being different is the entire value of an index buffer, and
    /// stating them as a range rather than a figure is the honest form. Hardware
    /// lands somewhere between, nearer the bottom for well-ordered geometry, and
    /// Lesson 4.9's RenderDoc capture is where the real number finally shows up.
    [[nodiscard]] Uint32 worst_case_invocations() const
    {
        return (mode_ == index_mode::indexed) ? index_count_ : vertex_count_;
    }

private:
    gpu_buffer vertices_;
    gpu_buffer indices_;
    index_mode mode_ = index_mode::indexed;
    Uint32 vertex_count_ = 0;
    Uint32 index_count_ = 0;
};

} // namespace engine
