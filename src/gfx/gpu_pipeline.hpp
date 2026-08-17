// src/gfx/gpu_pipeline.hpp — every piece of render state, gathered into one object.
//
// Lesson 4.4. Lesson 4.1 argued that a GPU bakes state into an immutable object
// and measured the usual justification for it (sparing the inner loop a branch)
// and found it FALSE — the real reason being that the driver must validate the
// combination and compile machine code specialised to it. Lesson 4.3 then found
// that `SDL_CreateGPUShader` takes eight microseconds, far too little to be
// compiling anything, and wrote down a prediction: the compile happens HERE.
// `scratch/verify_44.cpp` §C is where that prediction gets its answer.
//
// The type below exists for one reason that is not obvious until it bites:
//
//   **`SDL_GPUGraphicsPipelineCreateInfo` HOLDS POINTERS.** Its vertex input
//   state points at an array of buffer descriptions and an array of attributes;
//   its target info points at an array of colour target descriptions. So a
//   function that fills in a create-info and RETURNS IT returns a struct pointing
//   at its own dead stack frame. The compiler will not say a word.
//
// `pipeline_desc` owns those arrays and hands out the create-info that points
// into them, so the pointers stay valid exactly as long as the object does.

#pragma once

#include "gfx/gpu_device.hpp"

#include <SDL3/SDL.h>

#include <array>

namespace engine {

/// Builds an `SDL_GPUGraphicsPipelineCreateInfo` and owns the arrays it points at.
///
/// Not a fluent "builder" hiding the API — every field of the create-info is
/// still reachable and still spelled SDL's way. This is a lifetime holder with
/// the course's conventions pre-filled, which is the same bargain `gpu_device`
/// made about its raw handle in Lesson 4.2.
class pipeline_desc
{
public:
    /// Start from the course's defaults: triangle list, fill mode, **CCW-front
    /// and cull-back** (conventions §7), no depth test yet, no blending, and one
    /// colour target whose format is *taken from the device's swapchain* rather
    /// than chosen.
    ///
    /// That last one is load-bearing. A pipeline whose colour target format does
    /// not match the texture it renders to fails to create, and the swapchain's
    /// format is not ours to pick — Lesson 4.2 logged it for this moment.
    pipeline_desc(const gpu_device& dev, SDL_GPUShader* vertex, SDL_GPUShader* fragment);

    /// Describe one vertex buffer binding: which slot, and the stride between
    /// consecutive vertices.
    ///
    /// `pitch` is the whole vertex's size, not one attribute's. Getting it wrong
    /// does not fail to create — it walks the buffer at the wrong rate and draws
    /// a smear, which Lesson 4.5 will make a much bigger point of.
    pipeline_desc& vertex_buffer(Uint32 slot, Uint32 pitch);

    /// Describe one attribute: which shader input location, from which buffer
    /// slot, in what format, at what byte offset into the vertex.
    ///
    /// `location` matches the HLSL semantic index — `TEXCOORD0` is location 0 —
    /// which is why Lesson 4.3 insisted the semantics be numbered from zero with
    /// no gaps.
    pipeline_desc& attribute(Uint32 location, Uint32 buffer_slot,
                             SDL_GPUVertexElementFormat format, Uint32 offset);

    /// Render to something other than the swapchain.
    ///
    /// The constructor takes the format from the device's swapchain, which is
    /// right for the ninety percent of pipelines that draw to the window. It is
    /// wrong for a shadow map, a post-processing chain, or a headless test — all
    /// of which render into a texture whose format they chose. Module 6 lives on
    /// this method; Lesson 4.4's harness needs it because a device with no window
    /// has no swapchain format to inherit.
    pipeline_desc& colour_target_format(SDL_GPUTextureFormat format);

    /// The finished create-info, valid while this object is.
    ///
    /// Returned by const reference on purpose: a copy would be a struct whose
    /// pointers outlive nothing in particular, and handing one out by value is
    /// precisely the bug this class exists to prevent.
    [[nodiscard]] const SDL_GPUGraphicsPipelineCreateInfo& info();

    /// Direct access, for the fields this class has no opinion about. The
    /// create-info has nine top-level fields and fifty-three once its nested
    /// state structs are expanded; pre-empting all of them behind setters would
    /// be inventing an API to avoid learning one.
    [[nodiscard]] SDL_GPUGraphicsPipelineCreateInfo& raw() { return info_; }

private:
    static constexpr std::size_t k_max_buffers = 4;
    static constexpr std::size_t k_max_attributes = 8;

    SDL_GPUGraphicsPipelineCreateInfo info_{};

    // The arrays the create-info points at. Fixed-size and owned by value: a
    // std::vector would work too, right up until somebody copies the desc and
    // the vector reallocates.
    std::array<SDL_GPUVertexBufferDescription, k_max_buffers> buffers_{};
    std::array<SDL_GPUVertexAttribute, k_max_attributes> attributes_{};
    std::array<SDL_GPUColorTargetDescription, 4> colour_targets_{};

    Uint32 buffer_count_ = 0;
    Uint32 attribute_count_ = 0;
};

/// Owns an `SDL_GPUGraphicsPipeline`.
///
/// A pipeline holds its own reference to the shaders it was built from, so the
/// `gpu_shader` objects may be released as soon as every pipeline using them
/// exists. We keep ours alive anyway, because a lesson that releases them would
/// have to explain why the picture still works, which is a distraction.
class gpu_pipeline
{
public:
    gpu_pipeline() = default;
    ~gpu_pipeline();

    gpu_pipeline(const gpu_pipeline&) = delete;
    gpu_pipeline& operator=(const gpu_pipeline&) = delete;

    gpu_pipeline(gpu_pipeline&& other) noexcept;
    gpu_pipeline& operator=(gpu_pipeline&& other) noexcept;

    /// Create it. Failure is logged with SDL's message, which is unusually good
    /// for this call — a format mismatch says so by name.
    [[nodiscard]] bool create(const gpu_device& dev, const SDL_GPUGraphicsPipelineCreateInfo& info);

    void destroy();

    [[nodiscard]] bool valid() const { return pipeline_ != nullptr; }
    [[nodiscard]] SDL_GPUGraphicsPipeline* handle() const { return pipeline_; }

    /// Milliseconds the creation call took. Lesson 4.4's whole §5 is this number.
    [[nodiscard]] double create_ms() const { return create_ms_; }

private:
    SDL_GPUDevice* device_ = nullptr;            ///< NOT owned
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr; ///< owning by contract
    double create_ms_ = 0.0;
};

} // namespace engine
