// engine/src/gfx/gpu_particles.cpp — three buffers, three kernels, one indirect draw.
//
// Lesson 6.18b. Read `gpu_particles.hpp` first: it says which pass each function
// belongs in and which buffers that pass must be begun with. This file is the
// binding and the dispatching, and the usage flags, which are where most of the
// ways to get this wrong live.

#include <engine/gfx/gpu_particles.hpp>

#include <engine/core/log.hpp>

#include <vector>

namespace engine {

vec3 camera_right(const mat4& view_from_world)
{
    // Row 0 of the view rotation: the world-space vector that the view maps to
    // +x. `at` reads in WRITTEN notation (row, column), whatever the storage.
    return normalised_or(vec3{view_from_world.at(0, 0), view_from_world.at(0, 1),
                              view_from_world.at(0, 2)},
                         vec3{1.0f, 0.0f, 0.0f});
}

vec3 camera_up(const mat4& view_from_world)
{
    return normalised_or(vec3{view_from_world.at(1, 0), view_from_world.at(1, 1),
                              view_from_world.at(1, 2)},
                         vec3{0.0f, 1.0f, 0.0f});
}

gpu_particles::~gpu_particles()
{
    destroy();
}

bool gpu_particles::create(const gpu_device& dev, Uint32 capacity,
                           SDL_GPUShader* vertex, SDL_GPUShader* fragment,
                           SDL_GPUTextureFormat colour_format,
                           SDL_GPUTextureFormat depth_format)
{
    destroy();

    if (!dev.valid() || vertex == nullptr || fragment == nullptr) { return false; }

    capacity_ = particle_capacity_for(capacity);
    const Uint32 state_bytes = capacity_ * static_cast<Uint32>(sizeof(particle));

    // ---- The buffers, and every usage bit argued for -----------------------
    //
    // STATE: written by `step` (COMPUTE_STORAGE_WRITE), read by `step` in the
    // same thread that writes it and by `compact` (COMPUTE_STORAGE_READ — for a
    // buffer, SDL_gpu.h says READ | WRITE is what permits simultaneous
    // read-write), and read by the vertex stage (GRAPHICS_STORAGE_READ). Miss
    // the last and the draw's bind fails validation; miss the READ and the
    // compaction's read-only bind does.
    if (!state_.create(dev,
                       SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                           | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE
                           | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                       state_bytes, "particles: state")
        // ALIVE: written by `compact`, read by the vertex stage.
        || !alive_.create(dev,
                          SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE
                              | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                          capacity_ * static_cast<Uint32>(sizeof(Uint32)), "particles: alive list")
        // ARGS: written whole by `clear`, ADDED to by `compact` — an atomic is a
        // read-modify-write, so both compute bits — and read by the draw as its
        // INDIRECT arguments.
        || !args_.create(dev,
                         SDL_GPU_BUFFERUSAGE_INDIRECT
                             | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                             | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
                         static_cast<Uint32>(sizeof(SDL_GPUIndirectDrawCommand)),
                         "particles: draw arguments"))
    {
        ENGINE_LOG_ERROR(log_gpu, "gpu_particles: the buffers were not created");
        destroy();
        return false;
    }

    // ---- The kernels --------------------------------------------------------
    if (!step_.load(dev, "particles_step.comp") || !clear_.load(dev, "particles_clear.comp")
        || !compact_.load(dev, "particles_compact.comp"))
    {
        ENGINE_LOG_ERROR(log_gpu, "gpu_particles: a compute pipeline did not load");
        destroy();
        return false;
    }

    // ---- The draw pipeline ---------------------------------------------------
    pipeline_desc desc(dev, vertex, fragment);
    desc.colour_target_format(colour_format);

    // NO VERTEX BUFFERS AND NO ATTRIBUTES. The vertex shader indexes storage
    // buffers by SV_InstanceID and SV_VertexID, so the pipeline's vertex input
    // state is empty — which `pipeline_desc` already is until somebody calls
    // `vertex_buffer`.

    // ADDITIVE: light adds (6.13's bloom composite made the same call). It is
    // also what lets the compaction's list come out in any order: addition is
    // commutative, so the draw's order cannot change the picture — up to the
    // last bit of a half-float, which §9 measures.
    (void)desc.blend_add();

    // TESTED AGAINST THE SCENE, NEVER WRITING IT. The test hides sparks behind
    // geometry; the write would make every spark occlude the ones behind it, and
    // since they are drawn in whatever order the atomics came out, which ones
    // were hidden would change every frame (6.11's rule, and its failure).
    SDL_GPUGraphicsPipelineCreateInfo& raw = desc.raw();
    if (depth_format != SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        raw.target_info.has_depth_stencil_target = true;
        raw.target_info.depth_stencil_format = depth_format;
        raw.depth_stencil_state.enable_depth_test = true;
        raw.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    }
    (void)desc.depth_write(false);

    if (!draw_.create(dev, desc.info()))
    {
        ENGINE_LOG_ERROR(log_gpu, "gpu_particles: the draw pipeline was not created");
        destroy();
        return false;
    }

    // ---- A pool of dead particles --------------------------------------------
    //
    // SDL_GPU DOES NOT ZERO A NEW BUFFER. Nothing in SDL_gpu.h promises initial
    // contents, and "whatever the allocator last held" is a pool of particles
    // with random ages and lives — some of them alive, drawn on the first frame
    // at random positions. One upload of zeros makes every record the dead
    // particle `particle{}` is, and the pool starts empty by construction.
    SDL_GPUCommandBuffer* cb = SDL_AcquireGPUCommandBuffer(dev.handle());
    if (cb == nullptr)
    {
        destroy();
        return false;
    }
    const std::vector<particle> zeros(capacity_);
    const bool zeroed = state_.upload(cb, zeros.data(), state_bytes);
    if (!SDL_SubmitGPUCommandBuffer(cb) || !zeroed)
    {
        ENGINE_LOG_ERROR(log_gpu, "gpu_particles: the pool could not be zeroed");
        destroy();
        return false;
    }

    ENGINE_LOG_INFO(log_gpu, "gpu_particles: %u particles, %u B state + %u B list + 16 B args",
                    capacity_, state_bytes, capacity_ * 4u);
    return true;
}

void gpu_particles::destroy()
{
    draw_.destroy();
    compact_.destroy();
    clear_.destroy();
    step_.destroy();
    cpu_state_.destroy();
    args_.destroy();
    alive_.destroy();
    state_.destroy();
    capacity_ = 0;
}

void gpu_particles::record_step(SDL_GPUCommandBuffer* cb, SDL_GPUComputePass* pass,
                                const step_uniforms& u) const
{
    if (cb == nullptr || pass == nullptr || !step_.valid()) { return; }

    SDL_BindGPUComputePipeline(pass, step_.handle());

    // Pushed INSIDE the pass, which SDL_gpu.h allows ("It is valid to push
    // uniform data during a render or compute pass"), and pushed per step:
    // each step's emission window is different.
    SDL_PushGPUComputeUniformData(cb, 0, &u, sizeof(u));

    // One thread per slot. `capacity_` is a power of two of at least 64, so
    // this divides exactly — and the kernel checks the tail anyway.
    SDL_DispatchGPUCompute(pass, step_.groups_for_items(capacity_), 1, 1);
}

void gpu_particles::record_clear(SDL_GPUComputePass* pass) const
{
    if (pass == nullptr || !clear_.valid()) { return; }
    SDL_BindGPUComputePipeline(pass, clear_.handle());
    SDL_DispatchGPUCompute(pass, 1, 1, 1);
}

void gpu_particles::record_compact(SDL_GPUCommandBuffer* cb, SDL_GPUComputePass* pass,
                                   SDL_GPUBuffer* state) const
{
    if (cb == nullptr || pass == nullptr || state == nullptr || !compact_.valid()) { return; }

    SDL_BindGPUComputePipeline(pass, compact_.handle());

    // READ-ONLY storage is bound by a call, unlike read-write storage, which was
    // bound when the pass began. Slot 0 of the read-only set: t0 in space0.
    SDL_BindGPUComputeStorageBuffers(pass, 0, &state, 1);

    particle_compact_uniforms cu{};
    cu.capacity = capacity_;
    SDL_PushGPUComputeUniformData(cb, 0, &cu, sizeof(cu));

    SDL_DispatchGPUCompute(pass, compact_.groups_for_items(capacity_), 1, 1);
}

void gpu_particles::record_draw(SDL_GPUCommandBuffer* cb, SDL_GPURenderPass* pass,
                                const particle_draw_uniforms& u, SDL_GPUBuffer* state) const
{
    if (cb == nullptr || pass == nullptr || state == nullptr || !draw_.valid()) { return; }

    SDL_BindGPUGraphicsPipeline(pass, draw_.handle());

    // The vertex stage's storage buffers, in slot order: t0 the pool, t1 the list.
    SDL_GPUBuffer* const buffers[2] = {state, alive_.handle()};
    SDL_BindGPUVertexStorageBuffers(pass, 0, buffers, 2);

    SDL_PushGPUVertexUniformData(cb, 0, &u, sizeof(u));

    // THE COUNT IS NOT HERE. `SDL_DrawGPUPrimitives` would need the number of
    // instances as an argument, which would mean reading it back to the CPU —
    // waiting a frame, or stalling for one. The indirect form reads all four
    // arguments from a buffer when the GPU reaches the draw, by which time the
    // compaction has written them.
    SDL_DrawGPUPrimitivesIndirect(pass, args_.handle(), 0, 1);
}

void gpu_particles::simulate(SDL_GPUCommandBuffer* cb, std::span<const step_uniforms> steps) const
{
    if (cb == nullptr || !valid()) { return; }

    // ONE PASS PER STEP. Step k+1 reads what step k wrote, and within a pass
    // nothing orders two dispatches — so each step ends its pass. The binding's
    // cycle flag is FALSE: the state is read-modify-write, and cycling would
    // hand the kernel a buffer whose contents SDL declares undefined.
    for (const step_uniforms& u : steps)
    {
        SDL_GPUStorageBufferReadWriteBinding rw{};
        rw.buffer = state_.handle();
        rw.cycle = false;
        SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cb, nullptr, 0, &rw, 1);
        record_step(cb, pass, u);
        SDL_EndGPUComputePass(pass);
    }

    count_living(cb, state_.handle());
}

void gpu_particles::count_living(SDL_GPUCommandBuffer* cb, SDL_GPUBuffer* state) const
{
    if (cb == nullptr || state == nullptr || !valid()) { return; }

    // CLEAR: every byte of `args` is overwritten, so cycling is allowed — and it
    // is worth allowing, because it lets this frame's clear proceed without
    // waiting for last frame's draw to finish reading the old arguments.
    {
        SDL_GPUStorageBufferReadWriteBinding rw{};
        rw.buffer = args_.handle();
        rw.cycle = true;
        SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cb, nullptr, 0, &rw, 1);
        record_clear(pass);
        SDL_EndGPUComputePass(pass);
    }

    // COMPACT: `args` accumulates into what clear wrote — NOT cycled; `alive` is
    // overwritten up to the count and nothing past it is ever read — cycled.
    {
        SDL_GPUStorageBufferReadWriteBinding rw[2]{};
        rw[0].buffer = args_.handle();
        rw[0].cycle = false;
        rw[1].buffer = alive_.handle();
        rw[1].cycle = true;
        SDL_GPUComputePass* pass = SDL_BeginGPUComputePass(cb, nullptr, 0, rw, 2);
        record_compact(cb, pass, state);
        SDL_EndGPUComputePass(pass);
    }
}

SDL_GPUBuffer* gpu_particles::upload_cpu_state(const gpu_device& dev, SDL_GPUCommandBuffer* cb,
                                               std::span<const particle> ps)
{
    if (cb == nullptr || ps.empty()) { return nullptr; }

    const Uint32 bytes = static_cast<Uint32>(ps.size_bytes());
    if (!cpu_state_.valid() || cpu_state_.size() < bytes)
    {
        // Read by `compact` (compute, read-only) and by the vertex stage — the
        // CPU path writes it through a copy pass, which needs no usage bit.
        if (!cpu_state_.create(dev,
                               SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ
                                   | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                               bytes, "particles: CPU state"))
        {
            return nullptr;
        }
    }

    // `write` cycles BOTH halves — the right call here and the wrong one for the
    // GPU pool, and the difference is the whole of §10: the CPU path overwrites
    // every byte every frame, so the old contents are dead the moment it starts.
    return cpu_state_.write(cb, ps.data(), bytes) ? cpu_state_.handle() : nullptr;
}

bool gpu_particles::set_state(SDL_GPUCommandBuffer* cb, std::span<const particle> ps)
{
    if (cb == nullptr || ps.size() != capacity_) { return false; }
    return state_.upload(cb, ps.data(), static_cast<Uint32>(ps.size_bytes()));
}

} // namespace engine
