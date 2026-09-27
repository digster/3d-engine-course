// src/gfx/gpu_mesh.cpp — the conversion, the two buffers, and the two draw calls.

#include "gfx/gpu_mesh.hpp"

#include <cmath>
#include <cstddef>
#include <utility>

namespace engine {

namespace {

/// Fill one interleaved vertex from the parallel arrays at index `i`.
///
/// The normal is normalised HERE and not at load time, and that is Lesson 3.5's
/// rule showing its shape: the loader stores what the file said so that two loads
/// of a file are diffable, and the consumer normalises because the consumer is the
/// one that needs a unit vector. A zero normal (a mesh that carries none) stays
/// zero rather than becoming a NaN, which `mesh::normal_at` already defines as the
/// meaning of absent.
gpu_vertex_pnu make_vertex(const mesh& m, std::size_t i)
{
    const vec3 p = m.vertices[i];
    const vec3 n = m.normal_at(i);
    const vec2 t = m.uv_at(i);

    const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    const float inv = (len > 1e-12f) ? 1.0f / len : 0.0f;

    return gpu_vertex_pnu{
        p.x, p.y, p.z,
        n.x * inv, n.y * inv, n.z * inv,
        t.x, t.y
    };
}

} // namespace

void interleave(const mesh& m, std::vector<gpu_vertex_pnu>& out)
{
    out.clear();
    out.reserve(m.vertices.size());

    // THE WHOLE CONVERSION. Three arrays walked in lockstep into one, and the only
    // reason it deserves a name is that the choice it embodies — a vertex's data
    // being contiguous rather than its attribute's — is the one the hardware cares
    // about and the one the code makes invisible.
    for (std::size_t i = 0; i < m.vertices.size(); ++i)
    {
        out.push_back(make_vertex(m, i));
    }
}

void expand(const mesh& m, std::vector<gpu_vertex_pnu>& out)
{
    out.clear();
    out.reserve(m.indices.size());

    // Every index becomes a vertex. A position shared by six triangles is written
    // six times, and the index array is then redundant and thrown away. This is
    // what a renderer without index buffers must do, and the byte count is the
    // measurement Lesson 4.5 §5 is built on.
    for (const std::uint16_t index : m.indices)
    {
        if (index >= m.vertices.size()) { continue; }   // `validate()` reports these
        out.push_back(make_vertex(m, index));
    }
}

gpu_mesh::~gpu_mesh()
{
    destroy();
}

gpu_mesh::gpu_mesh(gpu_mesh&& other) noexcept
    : vertices_(std::move(other.vertices_)), indices_(std::move(other.indices_)),
      mode_(other.mode_), vertex_count_(other.vertex_count_),
      index_count_(other.index_count_)
{
    other.mode_ = index_mode::indexed;
    other.vertex_count_ = 0;
    other.index_count_ = 0;
}

gpu_mesh& gpu_mesh::operator=(gpu_mesh&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        vertices_ = std::move(other.vertices_);
        indices_ = std::move(other.indices_);
        mode_ = other.mode_;
        vertex_count_ = other.vertex_count_;
        index_count_ = other.index_count_;
        other.mode_ = index_mode::indexed;
        other.vertex_count_ = 0;
        other.index_count_ = 0;
    }
    return *this;
}

bool gpu_mesh::create(const gpu_device& dev, SDL_GPUCommandBuffer* cb,
                      const mesh& m, index_mode mode, const char* name)
{
    destroy();

    if (cb == nullptr || m.vertices.empty() || m.indices.size() < 3) { return false; }

    mode_ = mode;

    // ---- The conversion, on the CPU, once ----------------------------------
    std::vector<gpu_vertex_pnu> verts;
    if (mode == index_mode::indexed)
    {
        // THE 16-BIT CEILING, ENFORCED. `mesh.hpp` declared `k_max_mesh_vertices`
        // in Lesson 3.5 and explained that it comes from the hardware rather than
        // from us: SDL_GPU's index buffers are 16- or 32-bit, and we chose 16 for
        // half the bandwidth on every index fetch. This is the line that finally
        // cashes that constant in.
        if (m.vertices.size() > k_max_mesh_vertices)
        {
            SDL_Log("gpu_mesh '%s': %zu vertices exceeds the 16-bit index ceiling of %zu",
                    (name != nullptr) ? name : "?", m.vertices.size(), k_max_mesh_vertices);
            return false;
        }
        interleave(m, verts);
    }
    else
    {
        expand(m, verts);
    }

    if (verts.empty()) { return false; }

    vertex_count_ = static_cast<Uint32>(verts.size());
    const Uint32 vertex_bytes = vertex_count_ * static_cast<Uint32>(sizeof(gpu_vertex_pnu));

    if (!vertices_.create(dev, SDL_GPU_BUFFERUSAGE_VERTEX, vertex_bytes, name)) { return false; }
    if (!vertices_.upload(cb, verts.data(), vertex_bytes)) { destroy(); return false; }

    // ---- The index buffer, if this mode has one ----------------------------
    if (mode == index_mode::indexed)
    {
        index_count_ = static_cast<Uint32>(m.indices.size());
        const Uint32 index_bytes = index_count_ * static_cast<Uint32>(sizeof(std::uint16_t));

        // A DIFFERENT USAGE BIT, and it is not advisory. A buffer bound as an
        // index buffer must have been created with `_INDEX`; SDL says so field by
        // field, and the validation layer says so loudly when it is missed.
        if (!indices_.create(dev, SDL_GPU_BUFFERUSAGE_INDEX, index_bytes, name))
        {
            destroy();
            return false;
        }
        if (!indices_.upload(cb, m.indices.data(), index_bytes)) { destroy(); return false; }
    }
    else
    {
        index_count_ = 0;
    }

    return true;
}

void gpu_mesh::bind(SDL_GPURenderPass* pass, Uint32 slot) const
{
    if (pass == nullptr || !valid()) { return; }

    SDL_GPUBufferBinding vb{};
    vb.buffer = vertices_.handle();
    vb.offset = 0;
    SDL_BindGPUVertexBuffers(pass, slot, &vb, 1);

    if (mode_ == index_mode::indexed && indices_.valid())
    {
        SDL_GPUBufferBinding ib{};
        ib.buffer = indices_.handle();
        ib.offset = 0;

        // The element size is passed HERE, at bind time, not baked into the
        // pipeline and not carried by the buffer. So one buffer of bytes can be
        // read as 16- or 32-bit indices depending on this argument, and getting it
        // wrong reads pairs of 16-bit indices as single enormous 32-bit ones —
        // which is not an error, it is a mesh made of glass shards.
        SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    }
}

void gpu_mesh::draw(SDL_GPURenderPass* pass, Uint32 instances) const
{
    if (pass == nullptr || !valid() || instances == 0) { return; }

    if (mode_ == index_mode::indexed)
    {
        // num_indices, num_instances, first_index, vertex_offset, first_instance.
        //
        // `vertex_offset` is the one with no counterpart in the un-indexed call: it
        // is added to every index before the fetch, which is how several meshes
        // share one vertex buffer without their indices being rewritten. We pass 0
        // and Module 6's batching is where it earns its keep.
        SDL_DrawGPUIndexedPrimitives(pass, index_count_, instances, 0, 0, 0);
    }
    else
    {
        SDL_DrawGPUPrimitives(pass, vertex_count_, instances, 0, 0);
    }
}

pipeline_desc& gpu_mesh::describe(pipeline_desc& desc, Uint32 slot)
{
    // sizeof for the pitch, offsetof for every offset. Not one literal in this
    // function, which is the only defence C++ can offer against a layout that
    // drifts when a field is inserted.
    desc.vertex_buffer(slot, static_cast<Uint32>(sizeof(gpu_vertex_pnu)))
        .attribute(0, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                   static_cast<Uint32>(offsetof(gpu_vertex_pnu, px)))
        .attribute(1, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                   static_cast<Uint32>(offsetof(gpu_vertex_pnu, nx)))
        .attribute(2, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                   static_cast<Uint32>(offsetof(gpu_vertex_pnu, u)));
    return desc;
}

void gpu_mesh::destroy()
{
    vertices_.destroy();
    indices_.destroy();
    mode_ = index_mode::indexed;
    vertex_count_ = 0;
    index_count_ = 0;
}

} // namespace engine
