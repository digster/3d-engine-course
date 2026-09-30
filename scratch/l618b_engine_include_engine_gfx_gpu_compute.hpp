// engine/include/engine/gfx/gpu_compute.hpp — a compute pipeline, and the arithmetic of a dispatch.
//
// Lesson 6.18b. Everything the engine has sent to the GPU so far went through a
// rasterizer: vertices in, a primitive assembled, fragments out, each fragment
// allowed to write exactly one place — its own pixel. A compute shader has none
// of that. It is a function, run once per THREAD over a grid the program sizes,
// and each invocation may read and write any byte of any storage resource it is
// bound to. Lesson 4.1's SIMT picture is unchanged — lanes in lock-step, a warp
// or wave or SIMD-group at a time — but the rasterizer's scheduling is gone, and
// with it the rasterizer's guarantees: nothing orders the threads, nothing
// stops two of them writing the same address, and the only barrier SDL_GPU
// offers between one dispatch's writes and the next dispatch's reads is the end
// of a compute pass.
//
// ---------------------------------------------------------------------------
// WHY THIS IS NOT A `gpu_shader`
// ---------------------------------------------------------------------------
//
// SDL_GPU has no compute SHADER object. A graphics pipeline is assembled from
// two `SDL_GPUShader`s and fifty-odd fields of fixed-function state (Lesson
// 4.4); a compute pipeline is created straight from the code, because there is
// no fixed-function state to assemble it with. So this class owns the pipeline
// and nothing else, and `load` is `gpu_shader::load` with the stage decided for
// it: read the compiled code for this device's format, read the reflection,
// create. The reflection is a different shape — six resource counts and a
// thread-group size, parsed by `parse_compute_reflection` beside its graphics
// twin — and that difference is the whole of the new API surface.
//
// ---------------------------------------------------------------------------
// THE BINDING RULES, WHICH ARE A THIRD SET
// ---------------------------------------------------------------------------
//
// SDL_gpu.h, SDL_CreateGPUComputePipeline, checked at 3.4.12. In HLSL:
//
//     (t[n], space0)   sampled textures, then READ-ONLY storage textures,
//                      then READ-ONLY storage buffers
//     (u[n], space1)   READ-WRITE storage textures, then READ-WRITE storage buffers
//     (b[n], space2)   uniform buffers
//
// A vertex shader uses spaces 0 and 1, a fragment shader 2 and 3 (Lesson 6.17b's
// storage buffer lives at t8, space2). A compute shader's `b0` is in space2 —
// the fragment stage's TEXTURE space — which is the one number in this file a
// muscle memory trained on graphics shaders will get wrong.

#pragma once

#include <engine/gfx/gpu_device.hpp>
#include <engine/gfx/gpu_shader.hpp>

#include <SDL3/SDL.h>

#include <string>

namespace engine {

/// Thread groups needed to cover `items` threads, `per_group` at a time.
///
/// The ceiling, because the grid is sized in whole groups: 1,000 particles at 64
/// a group is 16 groups — 1,024 threads — and the last 24 must find nothing to
/// do. **Every kernel that is dispatched this way needs the bounds check that
/// makes those 24 harmless**, and §3 of the lesson shows what they write when it
/// is missing.
///
/// Written as `items / per_group + (remainder ? 1 : 0)` rather than the familiar
/// `(items + per_group - 1) / per_group`, which overflows for `items` within a
/// group of 2^32 — not a particle count anybody will reach, and exactly the kind
/// of edge a helper should close rather than document.
[[nodiscard]] constexpr Uint32 groups_for(Uint32 items, Uint32 per_group)
{
    return (per_group == 0u) ? 0u
                             : items / per_group + ((items % per_group != 0u) ? 1u : 0u);
}

/// Owns one `SDL_GPUComputePipeline`. Move-only, like every device resource here.
class gpu_compute_pipeline
{
public:
    gpu_compute_pipeline() = default;
    ~gpu_compute_pipeline();

    gpu_compute_pipeline(const gpu_compute_pipeline&) = delete;
    gpu_compute_pipeline& operator=(const gpu_compute_pipeline&) = delete;

    gpu_compute_pipeline(gpu_compute_pipeline&& other) noexcept;
    gpu_compute_pipeline& operator=(gpu_compute_pipeline&& other) noexcept;

    /// Load `shaders/<name>.<ext>` and `shaders/<name>.json` and create the
    /// pipeline, named `name` for debuggers and captures.
    ///
    /// @param name "particles_step.comp" — the stem the build emits.
    /// @return false on any failure, with the reason logged. A missing or
    ///         malformed reflection is a failure, never a guess: a pipeline
    ///         created with too few read-write buffers binds nothing where the
    ///         shader writes, and on most backends that is a write into nowhere
    ///         with no error at all.
    [[nodiscard]] bool load(const gpu_device& dev, const char* name);

    void destroy();

    [[nodiscard]] bool valid() const { return pipeline_ != nullptr; }
    [[nodiscard]] SDL_GPUComputePipeline* handle() const { return pipeline_; }

    /// What the shader declared — the counts the pipeline was created with.
    [[nodiscard]] const compute_resources& resources() const { return resources_; }

    /// `[numthreads]`'s x — the only dimension the engine's kernels use.
    [[nodiscard]] Uint32 threads_x() const { return resources_.threads_x; }

    /// Groups needed to give `items` threads one each, along x.
    [[nodiscard]] Uint32 groups_for_items(Uint32 items) const
    {
        return groups_for(items, resources_.threads_x);
    }

    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] std::size_t code_bytes() const { return code_bytes_; }

private:
    SDL_GPUDevice* device_ = nullptr;             ///< NOT owned; gpu_device owns it
    SDL_GPUComputePipeline* pipeline_ = nullptr;  ///< owning by contract
    compute_resources resources_{};
    std::size_t code_bytes_ = 0;
    std::string name_;
};

} // namespace engine
