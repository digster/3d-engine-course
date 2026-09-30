// engine/include/engine/gfx/gpu_particles.hpp — a particle system that lives on the GPU.
//
// Lesson 6.18b. `particles.hpp` specifies a particle system and runs it on the
// CPU; this runs the same specification on the GPU, where the state is CREATED,
// STEPPED, COUNTED and DRAWN without a byte of it ever crossing back to the
// CPU. The CPU's whole contribution per frame is a uniform block per fixed step
// — 144 bytes — and a draw call whose instance count it does not know.
//
// ---------------------------------------------------------------------------
// FOUR PASSES, AND WHY THERE ARE FOUR
// ---------------------------------------------------------------------------
//
//     step     (compute, once per FIXED STEP)   state  : read-modify-write
//     clear    (compute, once per frame)        args   : overwritten whole
//     compact  (compute, once per frame)        state  : read
//                                               args   : read-modify-write (atomic)
//                                               alive  : overwritten
//     draw     (render,  once per frame)        state, alive : read (vertex stage)
//                                               args   : read (as the INDIRECT arguments)
//
// Every boundary between them is a place where one pass's writes must be
// finished before the next pass reads, and SDL_GPU's only instrument for that is
// the end of a pass. SDL_gpu.h's note on SDL_BeginGPUComputePass says it in
// capitals: work that reads a previous dispatch's output "MUST end the current
// compute pass and begin a new one". So `clear` and
// `compact` cannot share a pass, however small `clear` is — and on this
// course's Metal machine putting them in one pass WORKS (verify_618b §H: 20
// exact counts in 20 trials; SDL's Metal encoder orders the dispatches), which is
// the most dangerous kind of working there is. §10 of the lesson reads the Vulkan
// and D3D12 backends and finds no barrier between dispatches in either.
//
// ---------------------------------------------------------------------------
// WHO BEGINS THE PASSES
// ---------------------------------------------------------------------------
//
// Not this class. Each `record_*` function binds, pushes and dispatches (or
// draws) inside a pass somebody else began — the frame graph (§11), or
// `simulate` below for a program that has none. That is the shape 6.18's overlay
// settled on and for the same reason, with one addition that makes it
// non-negotiable here: the read-write buffers of a compute pass are named when
// the pass BEGINS (`SDL_BeginGPUComputePass` takes them as an array), so
// whoever begins the pass must know which buffers each kernel writes, in which
// slot order. The comment on each `record_*` says exactly that.

#pragma once

#include <engine/gfx/gpu_buffer.hpp>
#include <engine/gfx/gpu_compute.hpp>
#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_pipeline.hpp>
#include <engine/gfx/particles.hpp>
#include <engine/math/mat4.hpp>

#include <SDL3/SDL.h>

#include <span>

namespace engine {

/// The vertex stage's uniform block. **Must match `cbuffer Draw` in
/// `particle.vert.hlsl`**; five rows.
struct particle_draw_uniforms
{
    mat4 clip_from_world = mat4::identity();
    vec3 right{1.0f, 0.0f, 0.0f};   ///< the camera's right, world space, unit
    float size = 0.03f;             ///< a newborn spark's radius, metres
    vec3 up{0.0f, 1.0f, 0.0f};      ///< the camera's up, world space, unit
    float alpha = 1.0f;             ///< interpolation between the last two steps
    float intensity = 6.0f;         ///< HDR radiance of a newborn spark
    Uint32 seed = 0;                ///< varies size and brightness per particle
    float pad0 = 0.0f;
    float pad1 = 0.0f;
};

static_assert(sizeof(particle_draw_uniforms) == 112, "must match cbuffer Draw");

/// The compaction kernel's uniform block. Matches `cbuffer Compact`.
struct particle_compact_uniforms
{
    Uint32 capacity = 0;
    Uint32 pad0 = 0;
    Uint32 pad1 = 0;
    Uint32 pad2 = 0;
};

static_assert(sizeof(particle_compact_uniforms) == 16, "must match cbuffer Compact");

/// The camera's right and up, in world space, for a billboard: the first two
/// ROWS of the view matrix's rotation, because a rigid view matrix's rotation
/// is orthonormal and its inverse is its transpose (Lesson 2.9).
[[nodiscard]] vec3 camera_right(const mat4& view_from_world);
[[nodiscard]] vec3 camera_up(const mat4& view_from_world);

/// A particle pool on the device, the kernels that run it and the pipeline that
/// draws it.
class gpu_particles
{
public:
    gpu_particles() = default;
    ~gpu_particles();

    gpu_particles(const gpu_particles&) = delete;
    gpu_particles& operator=(const gpu_particles&) = delete;

    /// Create the three buffers, load the three compute pipelines, and build
    /// the draw pipeline.
    ///
    /// @param capacity rounded up to a power of two of at least 64
    ///        (`particle_capacity_for`), for `slot_of`'s reason.
    /// @param vertex, fragment `particle.vert` and `particle.frag`. The compute
    ///        pipelines are loaded here by name, because a compute pipeline IS its
    ///        shader — there is no separate object to hand in (gpu_compute.hpp).
    /// @param colour_format the HDR target's format: the blend adds LIGHT, so it
    ///        belongs before the tonemap (6.12), not after it.
    /// @param depth_format the scene's depth format; the particles are TESTED
    ///        against it and never write it.
    [[nodiscard]] bool create(const gpu_device& dev, Uint32 capacity,
                              SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                              SDL_GPUTextureFormat colour_format,
                              SDL_GPUTextureFormat depth_format);

    void destroy();

    [[nodiscard]] bool valid() const { return draw_.valid() && step_.valid(); }
    [[nodiscard]] Uint32 capacity() const { return capacity_; }

    // ---- The resources, for whoever begins the passes ---------------------

    /// The pool: `capacity` particles, 48 bytes each. Zeroed at creation, which
    /// is a pool of dead particles.
    [[nodiscard]] SDL_GPUBuffer* state() const { return state_.handle(); }

    /// The list of living slots, written by `compact`, read by `draw`.
    [[nodiscard]] SDL_GPUBuffer* alive() const { return alive_.handle(); }

    /// An `SDL_GPUIndirectDrawCommand`: 16 bytes the CPU writes once and never
    /// reads, and whose second word is the number of living particles.
    [[nodiscard]] SDL_GPUBuffer* args() const { return args_.handle(); }

    [[nodiscard]] Uint32 state_bytes() const { return state_.size(); }

    // ---- Recording, one function per pass ---------------------------------

    /// One fixed step. The pass must have been begun with `state` as read-write
    /// storage buffer SLOT 0, and — this is the line that matters — with
    /// `cycle = false`: the kernel reads what the previous step wrote, and a
    /// cycled buffer's contents are undefined (§10).
    void record_step(SDL_GPUCommandBuffer* cb, SDL_GPUComputePass* pass,
                     const step_uniforms& u) const;

    /// Reset the draw arguments. The pass: `args` as read-write slot 0, and
    /// cycling is allowed, because every byte is overwritten.
    void record_clear(SDL_GPUComputePass* pass) const;

    /// List and count the living. The pass: `args` as read-write slot 0 (NOT
    /// cycled: the count accumulates into what `clear` wrote) and `alive` as
    /// slot 1 (cycling allowed). `state` is bound here, READ-ONLY.
    void record_compact(SDL_GPUCommandBuffer* cb, SDL_GPUComputePass* pass,
                        SDL_GPUBuffer* state) const;

    /// Draw the living, into a render pass with the HDR target as colour
    /// attachment 0 and the scene's depth attached. Issues ONE indirect draw.
    void record_draw(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                     const particle_draw_uniforms& u, SDL_GPUBuffer* state) const;

    // ---- Without a frame graph ---------------------------------------------

    /// Every compute pass of a frame, begun and ended here: one per step, then
    /// clear, then compact. For a program that has no frame graph, and for the
    /// harness's hand-recorded control.
    void simulate(SDL_GPUCommandBuffer* cb, std::span<const step_uniforms> steps) const;

    /// Clear and compact only — for a pool whose state was produced elsewhere
    /// (uploaded by the CPU, the lesson's "before").
    void count_living(SDL_GPUCommandBuffer* cb, SDL_GPUBuffer* state) const;

    // ---- The CPU path: the "before" ----------------------------------------

    /// Copy a CPU pool's particles into a device buffer of their own, through a
    /// staging buffer that is kept and cycled (4.5's stream buffer). Returns the
    /// buffer to hand to `count_living` and `record_draw`, or null.
    ///
    /// This is what every frame of a CPU particle system costs the bus:
    /// `capacity * 48` bytes, whether or not a single particle moved.
    [[nodiscard]] SDL_GPUBuffer* upload_cpu_state(const gpu_device& dev, SDL_GPUCommandBuffer* cb,
                                                  std::span<const particle> ps);

    /// Overwrite the device pool itself with `ps` — how a harness starts the GPU
    /// from a known state. Records a one-shot upload into `cb`.
    [[nodiscard]] bool set_state(SDL_GPUCommandBuffer* cb, std::span<const particle> ps);

    [[nodiscard]] const gpu_compute_pipeline& step_pipeline() const { return step_; }
    [[nodiscard]] const gpu_compute_pipeline& compact_pipeline() const { return compact_; }

private:
    gpu_buffer state_;
    gpu_buffer alive_;
    gpu_buffer args_;
    gpu_stream_buffer cpu_state_;   ///< created on first `upload_cpu_state`

    gpu_compute_pipeline step_;
    gpu_compute_pipeline clear_;
    gpu_compute_pipeline compact_;
    gpu_pipeline draw_;

    Uint32 capacity_ = 0;
};

} // namespace engine
