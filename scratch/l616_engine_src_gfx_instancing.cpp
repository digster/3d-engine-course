// engine/src/gfx/instancing.cpp — turning a visible set into as few draws as possible.
//
// Lesson 6.16. Three things live here: the `object_uniforms` -> `gpu_instance`
// unpack, the instance-rate vertex layout, and the sort-then-run-length pass that
// is the whole of batching.

#include <engine/gfx/instancing.hpp>

#include <algorithm>
#include <cstring>

namespace engine {

gpu_instance instance_of(const mat4& world_from_model, const mat3& normal_from_model)
{
    gpu_instance out{};
    out.world_from_model = world_from_model;

    // THE SAME UNPACK `gpu_scene_renderer::render` DOES BEFORE PUSHING. Three
    // columns, each padded to a `vec4` whose `w` nothing reads — gpu_uniform.hpp
    // argues at length why a `float3x3` is not used, and every word of that
    // argument survives the move to a vertex attribute, because a vertex
    // attribute of three floats is a `float3` and reassembling a 3x3 from three
    // of them is exactly the same three multiply-adds.
    out.normal_from_model[0] = vec4{normal_from_model.c0.x, normal_from_model.c0.y,
                                    normal_from_model.c0.z, 0.0f};
    out.normal_from_model[1] = vec4{normal_from_model.c1.x, normal_from_model.c1.y,
                                    normal_from_model.c1.z, 0.0f};
    out.normal_from_model[2] = vec4{normal_from_model.c2.x, normal_from_model.c2.y,
                                    normal_from_model.c2.z, 0.0f};
    return out;
}

pipeline_desc& describe_instances(pipeline_desc& desc, Uint32 slot)
{
    // `instance_buffer`, not `vertex_buffer`. One enum value inside
    // (`SDL_GPU_VERTEXINPUTRATE_INSTANCE`), and it changes what the hardware
    // multiplies the stride by: the vertex index for a vertex-rate buffer, the
    // instance index for this one. Everything else about the fetch — base
    // address, pitch, offset — is identical, which is Lesson 4.5's formula
    // unchanged.
    desc.instance_buffer(slot, static_cast<Uint32>(sizeof(gpu_instance)));

    // FOUR ATTRIBUTES FOR ONE MATRIX. A vertex attribute is at most four
    // components in every backend SDL_GPU targets, so a 4x4 is four consecutive
    // `float4`s and the shader puts them back together. `offsetof` per column,
    // never a literal 16 — Lesson 4.4's rule, and here it matters more than
    // usual because the four offsets are the one place a transpose could be
    // introduced without any type noticing.
    desc.attribute(4, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                   static_cast<Uint32>(offsetof(gpu_instance, world_from_model)))
        .attribute(5, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                   static_cast<Uint32>(offsetof(gpu_instance, world_from_model)
                                       + sizeof(vec4)))
        .attribute(6, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                   static_cast<Uint32>(offsetof(gpu_instance, world_from_model)
                                       + 2 * sizeof(vec4)))
        .attribute(7, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                   static_cast<Uint32>(offsetof(gpu_instance, world_from_model)
                                       + 3 * sizeof(vec4)))
        .attribute(8, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                   static_cast<Uint32>(offsetof(gpu_instance, normal_from_model)))
        .attribute(9, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                   static_cast<Uint32>(offsetof(gpu_instance, normal_from_model)
                                       + sizeof(vec4)))
        .attribute(10, slot, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                   static_cast<Uint32>(offsetof(gpu_instance, normal_from_model)
                                       + 2 * sizeof(vec4)));
    return desc;
}

bool batch_key::matches(const batch_key& other) const
{
    // `memcmp` ON THE MATERIAL, and it is safe here for a reason worth stating
    // rather than assuming: `material_uniforms` is a plain struct of floats with
    // no padding (its own `static_assert` in gpu_uniform.hpp pins the size), so
    // there are no indeterminate bytes for `memcmp` to read. On a struct WITH
    // padding this would compare uninitialised memory and two identical
    // materials would fail to batch, intermittently, depending on what was last
    // on the stack — one of the nastiest bugs in this shape of code.
    //
    // Float equality is also deliberate. Two materials that differ in the last
    // bit genuinely cannot share a draw, because the shader would receive one of
    // the two values for both; a tolerance here would be a rendering error
    // rather than a convenience.
    return mesh == other.mesh
        && texture == other.texture
        && normal_map == other.normal_map
        && style == other.style
        && blend == other.blend
        && std::memcmp(&material, &other.material, sizeof(material)) == 0;
}

namespace {

/// A total order over keys, so that equal keys become adjacent runs.
///
/// The order itself is arbitrary — it exists to group, not to rank — which is
/// why it compares POINTERS. That makes the batching non-deterministic across
/// runs (allocation addresses move), and for opaque geometry that is harmless
/// because the z-buffer does not care what order draws arrive in (Lesson 3.1).
/// It would be unacceptable for anything whose output depends on draw order,
/// which is exactly why blended draws never reach this comparator.
[[nodiscard]] bool key_less(const batch_key& a, const batch_key& b)
{
    if (a.mesh != b.mesh) { return std::less<const void*>{}(a.mesh, b.mesh); }
    if (a.texture != b.texture) { return std::less<const void*>{}(a.texture, b.texture); }
    if (a.normal_map != b.normal_map)
    {
        return std::less<const void*>{}(a.normal_map, b.normal_map);
    }
    if (a.style != b.style) { return a.style < b.style; }
    if (a.blend != b.blend) { return a.blend < b.blend; }
    return std::memcmp(&a.material, &b.material, sizeof(a.material)) < 0;
}

[[nodiscard]] batch_key key_of(const gpu_draw_item& item)
{
    return {item.mesh, item.texture, item.normal_map, item.style, item.blend, item.material};
}

} // namespace

void batch_instances(std::span<const gpu_draw_item> items,
                     std::span<const int> visible,
                     std::vector<gpu_instance>& out_instances,
                     std::vector<instance_batch>& out_batches,
                     batch_report* report)
{
    // `clear()` KEEPS THE CAPACITY, so a steady-state frame allocates nothing.
    // The same bargain `collect_triangles` struck in Lesson 3.10 and for the
    // same reason: the fix for allocation in a hot loop is not a faster
    // allocator, it is not allocating.
    out_instances.clear();
    out_batches.clear();

    batch_report local;

    // ---- Gather the candidate indices --------------------------------------
    //
    // An empty `visible` means "all of them", which keeps the un-culled path a
    // default argument rather than a second function.
    static thread_local std::vector<int> opaque;
    static thread_local std::vector<int> blended;
    opaque.clear();
    blended.clear();

    const std::size_t n = visible.empty() ? items.size() : visible.size();
    for (std::size_t k = 0; k < n; ++k)
    {
        const int idx = visible.empty() ? static_cast<int>(k) : visible[k];
        if (idx < 0 || static_cast<std::size_t>(idx) >= items.size()) { continue; }
        const gpu_draw_item& item = items[static_cast<std::size_t>(idx)];

        // NULL IS SKIPPED; INVALID IS NOT CHECKED, and the asymmetry is a
        // separation of concerns rather than an oversight. Batching is a pure
        // grouping over the draw list — it compares pointers, enums and 32 bytes
        // of material — and asking a mesh whether its GPU buffers uploaded drags
        // device state into a function that otherwise needs none. `render_batched`
        // checks `valid()` on each batch's mesh and skips it there, which is
        // where a scene whose model failed to load stops costing pixels.
        //
        // The practical dividend is that this function is testable with no GPU at
        // all, which is how §I of the lesson's harness runs headless.
        if (item.mesh == nullptr) { continue; }

        ++local.items;
        if (item.blend == blend_style::opaque) { opaque.push_back(idx); }
        else { blended.push_back(idx); ++local.blended; }
    }

    // ---- Sort the opaque half by key ---------------------------------------
    //
    // THIS IS THE COST OF INSTANCING, and it is O(n log n) against the O(n) walk
    // it is trying to improve. §10 measures where that stops paying. Note that
    // the sort is over INDICES, not over 140-byte draw items — the same argument
    // `draw_key` made in Lesson 6.11.
    std::sort(opaque.begin(), opaque.end(), [&](int a, int b) {
        return key_less(key_of(items[static_cast<std::size_t>(a)]),
                        key_of(items[static_cast<std::size_t>(b)]));
    });

    // ---- Run-length encode -------------------------------------------------
    for (std::size_t k = 0; k < opaque.size(); ++k)
    {
        const gpu_draw_item& item = items[static_cast<std::size_t>(opaque[k])];
        const batch_key key = key_of(item);

        if (out_batches.empty() || !out_batches.back().key.matches(key)
            || out_batches.back().key.blend != blend_style::opaque)
        {
            if (!out_batches.empty())
            {
                // WHY THE RUN ENDED, field by field. Checked in the order a
                // content author can act on: a different mesh is a modelling
                // decision, a different texture is an atlas decision, a different
                // material is a shader-parameter decision. Only the FIRST
                // difference is counted, so the numbers sum to the number of
                // splits rather than over-counting a draw that differs in three
                // ways at once.
                const batch_key& prev = out_batches.back().key;
                if (prev.mesh != key.mesh) { ++local.split_by_mesh; }
                else if (prev.texture != key.texture
                         || prev.normal_map != key.normal_map) { ++local.split_by_texture; }
                else if (prev.style != key.style || prev.blend != key.blend)
                {
                    ++local.split_by_style;
                }
                else { ++local.split_by_material; }
            }
            out_batches.push_back(instance_batch{key, static_cast<int>(out_instances.size()), 0});
        }

        out_instances.push_back(instance_of(item.world_from_model, item.normal_from_model));
        ++out_batches.back().count;
    }

    // ---- The blended tail, one batch each, IN ORDER ------------------------
    //
    // Not sorted, not merged, and both of those are correctness rather than
    // laziness: Lesson 6.11 established that `over` is not commutative, so the
    // back-to-front order the caller arrived with is part of the picture. They
    // still become `instance_batch`es of one so that the renderer has a single
    // list to walk — an instanced draw of one instance is an ordinary draw with
    // its matrices arriving by a different road, and costs nothing extra.
    for (int idx : blended)
    {
        const gpu_draw_item& item = items[static_cast<std::size_t>(idx)];
        out_batches.push_back(instance_batch{key_of(item),
                                             static_cast<int>(out_instances.size()), 1});
        out_instances.push_back(instance_of(item.world_from_model, item.normal_from_model));
    }

    for (const instance_batch& b : out_batches)
    {
        ++local.batches;
        if (b.count > 1) { ++local.instanced; }
        local.largest = std::max(local.largest, b.count);
    }

    if (report != nullptr) { *report = local; }
}

} // namespace engine
