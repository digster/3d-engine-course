// engine/include/engine/gfx/instancing.hpp — many copies, one draw call.
//
// Lesson 6.16, second half. Lesson 4.5 built the mechanism — an instance-rate
// vertex buffer, a pipeline that declares one, and `gpu_mesh::draw(pass, n)` —
// and then, for eleven lessons, nothing fed it. `demos/sandbox` still draws seven
// tori from a hand-rolled 28-byte struct that exists only to demonstrate that the
// API works. **This file is the engine learning to produce instances rather than
// to display them.**
//
// ---------------------------------------------------------------------------
// WHY IT BELONGS IN THE SAME LESSON AS CULLING
// ---------------------------------------------------------------------------
//
// Culling turns a draw list into a SET — the survivors. An instanced draw wants
// exactly that: a set of things which differ only in where they are. So the two
// compose directly, and the composition is the lesson's spine:
//
//     draw list  --cull-->  visible indices  --batch-->  runs  --> one draw each
//
// The middle arrow is `batch_instances` below, and it is where the honest
// constraint lives.
//
// ---------------------------------------------------------------------------
// THE CONSTRAINT, WHICH IS STRICTER THAN "SAME MESH"
// ---------------------------------------------------------------------------
//
// A single instanced draw issues ONE `SDL_DrawGPUIndexedPrimitives` with one
// pipeline, one set of bound textures, and one set of pushed uniforms. Everything
// that is not per-instance vertex data is therefore shared by every instance in
// the batch. That means a batch key is not "the mesh" — it is
//
//     (mesh, albedo texture, normal map, surface style, blend style, material)
//
// and the material is 32 bytes of floats that two objects rarely match on by
// accident. `batch_key` below spells the whole tuple out rather than hashing it,
// because a key you cannot read is a key you cannot debug, and because §9
// measures how much each component of it costs in lost batching.
//
// **This is why instancing is a content decision before it is a renderer one.**
// A forest instances beautifully because somebody authored one tree and placed it
// four hundred times. A hand-built scene of four distinct props instances not at
// all, and §9 measures exactly that on this engine's own demo scene: **12 draws
// collapse to 12 batches**, a saving of zero, which is the correct answer and
// the reason the measurement is in the lesson rather than a claim that it helps.
//
// ---------------------------------------------------------------------------
// THE BYTES DO NOT MOVE. THE CALLS DO.
// ---------------------------------------------------------------------------
//
// `gpu_instance` below is **112 bytes, exactly `sizeof(object_uniforms)`**, and
// that is not a coincidence — it carries the same matrix and the same three
// normal columns, because the vertex shader needs the same things whichever road
// they arrive by. So instancing a hundred objects does not reduce the per-object
// bytes crossing the bus by one byte. What it removes is a hundred
// `SDL_PushGPUVertexUniformData` calls, a hundred `SDL_BindGPUVertexBuffers`
// calls and ninety-nine draw calls.
//
// That distinction is worth holding onto, because "instancing saves bandwidth" is
// a very common and very wrong summary. Instancing saves **submission**, and
// submission is a CPU cost. §10 measures both sides.

#pragma once

#include <engine/gfx/gpu_buffer.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_mesh.hpp>
#include <engine/gfx/gpu_pipeline.hpp>
#include <engine/gfx/gpu_scene.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>

#include <SDL3/SDL.h>

#include <span>
#include <vector>

namespace engine {

/// One instance's per-object data, as VERTEX ATTRIBUTES rather than uniforms.
///
/// **Field-for-field identical to `object_uniforms`** (gpu_uniform.hpp), and the
/// `static_assert` below is what keeps it that way: the same matrix, the same
/// three normal columns, the same 112 bytes. The only thing that changed is the
/// road the data takes to the shader — a per-draw push becomes a per-instance
/// fetch — and the shader reads the same values out the other end.
///
/// **Seven `float4` attributes.** A `float4x4` cannot be a single vertex
/// attribute in any of SDL_GPU's backends: a vertex attribute is at most four
/// components, so a matrix is four consecutive attributes that the shader
/// reassembles. That is why `scene_instanced.vert.hlsl` declares `world_0` …
/// `world_3` and builds the matrix from them, and it is the first thing that
/// surprises people porting a uniform-based renderer.
struct gpu_instance
{
    mat4 world_from_model;      ///< locations 4, 5, 6, 7 — T*R*S (Lesson 2.8)
    vec4 normal_from_model[3];  ///< locations 8, 9, 10 — the inverse transpose's columns
};

static_assert(sizeof(gpu_instance) == 112,
              "a gpu_instance is an object_uniforms that travels by a different road");
static_assert(offsetof(gpu_instance, world_from_model) == 0, "");
static_assert(offsetof(gpu_instance, normal_from_model) == 64, "");

/// Fill a `gpu_instance` from a draw item's two matrices.
///
/// The unpack of the `mat3` into three padded `vec4` is the same one
/// `gpu_scene_renderer::render` does before pushing `object_uniforms`, written
/// once here so the two roads cannot disagree about column order — which would
/// light instanced objects differently from non-instanced ones and look like a
/// shader bug.
[[nodiscard]] gpu_instance instance_of(const mat4& world_from_model,
                                       const mat3& normal_from_model);

/// Describe `gpu_instance` to a pipeline: one INSTANCE-RATE buffer at `slot`,
/// seven `float4` attributes at locations 4 through 10.
///
/// **`input_rate = _INSTANCE` is the entire difference from `gpu_mesh::describe`**
/// — Lesson 4.5 spent a section on it and the state block still says "ONE ENUM
/// VALUE". Same buffer type, same attribute machinery, same fetch formula; the
/// only change is what the hardware multiplies the stride by.
///
/// **Locations start at 4, not 0**, and the reason is the collision Lesson 6.7
/// hit: attribute locations are numbered across the whole PIPELINE, not per
/// buffer, and `gpu_mesh::describe` with tangents occupies 0 through 3. The
/// sandbox's older instancing pipeline passes `with_tangent = false` and takes
/// location 3 for itself; this one does not, because the instanced scene pipeline
/// shares `scene.frag.hlsl` and that shader's inputs include a tangent.
pipeline_desc& describe_instances(pipeline_desc& desc, Uint32 slot = 1);

/// What every instance in one draw must agree about.
///
/// Not a hash. The fields are spelled out because a batch that did not form is a
/// question — "which of these six did they differ on?" — and a 64-bit hash cannot
/// answer it. `batch_report::split_by` below counts exactly that, per field.
struct batch_key
{
    const gpu_mesh* mesh = nullptr;
    SDL_GPUTexture* texture = nullptr;
    SDL_GPUTexture* normal_map = nullptr;
    surface_style style = surface_style::solid;
    blend_style blend = blend_style::opaque;

    /// The 32-byte material block, compared BY VALUE.
    ///
    /// Two objects that were authored with the same material but reached here as
    /// separate copies still batch, because the comparison is on the bytes and
    /// not on an identity. That is the right call for a value type — and it is
    /// also why a future material HANDLE (Module 9's asset work) would make this
    /// comparison one pointer instead of eight floats.
    material_uniforms material{};

    [[nodiscard]] bool matches(const batch_key& other) const;
};

/// A run of instances that share a `batch_key`.
///
/// `first` indexes the instance array `batch_instances` filled in, not the
/// caller's draw list — the caller's order is not preserved within a batch and
/// must not be relied on. For OPAQUE geometry that is free (the z-buffer sorts;
/// Lesson 3.1), and for BLENDED geometry it is exactly wrong, which is why
/// `batch_instances` refuses to batch anything whose blend style is not opaque.
/// See its own comment: that refusal is a correctness rule, not a simplification.
struct instance_batch
{
    batch_key key{};
    int first = 0;   ///< index into the instance array
    int count = 0;   ///< how many consecutive instances belong to this batch
};

/// What batching a frame's draw list achieved, and where it failed to.
struct batch_report
{
    int items = 0;        ///< draws offered
    int batches = 0;      ///< draw calls that will actually be issued
    int instanced = 0;    ///< …of which carry more than one instance
    int largest = 0;      ///< the biggest batch's instance count

    /// Draws that could not batch because they are blended. Counted rather than
    /// hidden: a scene that is mostly glass gets no benefit here and should be
    /// told so.
    int blended = 0;

    /// Why a batch ended, per key field. Sums to `batches - 1` at most (the first
    /// batch is not caused by a difference). **This is the number that tells an
    /// artist what to change**: `split_by_material` dominating means the scene
    /// has one mesh painted many ways, which is a texture-atlas problem, while
    /// `split_by_mesh` dominating means it has many distinct models, which is not
    /// a renderer problem at all.
    int split_by_mesh = 0;
    int split_by_texture = 0;
    int split_by_style = 0;
    int split_by_material = 0;

    /// `items / batches`, or 0 when there are none. The single number worth
    /// putting on a HUD, and the one worth distrusting: 1.0 means batching did
    /// nothing at all, and on a hand-built scene 1.0 is the correct answer.
    [[nodiscard]] float instances_per_draw() const
    {
        return batches > 0 ? static_cast<float>(items) / static_cast<float>(batches) : 0.0f;
    }
};

/// Group `items` (optionally restricted to `visible`) into instanced batches.
///
/// **Sorts by key first, because batching a run requires the run to exist.** An
/// unsorted list of A B A B A B batches into six; sorted, it batches into two.
/// That sort is the cost of instancing on the CPU and §10 measures it against the
/// draw calls it removes — which is the crossover this lesson is really about.
///
/// @param items    the frame's draw list, exactly as `gpu_scene_renderer::render`
///                 would take it
/// @param visible  indices into `items` to consider, from `cull_visible`; pass an
///                 empty span to mean "all of them"
/// @param out_instances  filled with one `gpu_instance` per batched item, in
///                 batch order. Cleared first; reused across frames by the caller
///                 so a steady-state frame allocates nothing (3.10's bargain).
/// @param out_batches    filled with the runs. Cleared first.
/// @param report   optional counters
///
/// **A null mesh is skipped; an INVALID one is not checked.** This function
/// touches no device state — it compares pointers, enums and material bytes —
/// so it runs headless and is deterministic. Whether a mesh's buffers actually
/// uploaded is `render_batched`'s question, and it asks it.
///
/// **Blended draws are never batched**, and are returned as single-instance
/// batches in the caller's original relative order. Lesson 6.11 established that
/// `over` is not commutative, so the back-to-front order `order_draws` produced
/// is load-bearing; a sort by mesh would destroy it, and an instanced draw has no
/// way to express "these hundred copies, in this order, interleaved with those".
/// Order-independent transparency is the technique that lifts this, it is named
/// in `blend.hpp`, and it is not built.
void batch_instances(std::span<const gpu_draw_item> items,
                     std::span<const int> visible,
                     std::vector<gpu_instance>& out_instances,
                     std::vector<instance_batch>& out_batches,
                     batch_report* report = nullptr);

} // namespace engine
