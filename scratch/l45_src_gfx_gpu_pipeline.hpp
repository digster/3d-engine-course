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
//
// LESSON 4.5 adds the two things a real mesh needs and one thing every project
// eventually wishes it had written on day one:
//
//   * `instance_buffer` — a binding whose attributes advance once per INSTANCE
//     instead of once per vertex. One field of one struct; that is all instancing
//     costs at this level.
//   * `check_layout` — the cross-check Lesson 4.4 said nothing performs, run
//     against the reflection JSON the build has been emitting since Lesson 4.3.
//     SDL validates almost nothing here (4.4 §B measured it), so this is the only
//     thing standing between a mistyped offset and an evening of confusion.

#pragma once

#include "gfx/gpu_device.hpp"
#include "gfx/gpu_shader.hpp"

#include <SDL3/SDL.h>

#include <array>

namespace engine {

/// Bytes one attribute of this format occupies **in the buffer**.
///
/// Lesson 4.5. Needed for exactly one thing: deciding whether the attributes you
/// declared actually fit inside the pitch you declared. Every silent layout bug
/// is an arithmetic disagreement, and this is the arithmetic.
[[nodiscard]] Uint32 size_of(SDL_GPUVertexElementFormat format);

/// The HLSL type the **shader** receives when an attribute of this format arrives.
///
/// NOT the same question as `size_of`, and the gap between them is the single
/// most useful thing on this page. `UBYTE4_NORM` occupies four bytes in the
/// buffer and arrives in the shader as a `float4` — four bytes of storage, sixteen
/// bytes of register, and a divide by 255 performed by fixed-function hardware on
/// the way. That conversion is free, which is why a vertex colour has no business
/// being four floats, and why Lesson 4.5 shrinks ours.
///
/// @return a static string like "float3"; "" for `INVALID`.
[[nodiscard]] const char* shader_type_of(SDL_GPUVertexElementFormat format);

/// What `check_layout` found. Zero problems is the only good answer.
struct layout_report
{
    int checked = 0;        ///< attributes compared against a shader input
    int missing = 0;        ///< the shader declares a location nothing supplies
    int extra = 0;          ///< we supply a location the shader never declares
    int type_mismatch = 0;  ///< float bits arriving where the shader reads an int, or vice versa
    int overrun = 0;        ///< the attribute does not fit inside its buffer's pitch
    int duplicate = 0;      ///< two attributes at the same location — SDL requires unique

    /// Same base type, **fewer components** than the shader declares — `FLOAT3`
    /// supplied to a `float4`. Reported and deliberately NOT counted as a
    /// problem: it is a legal and widely-used layout, and the components you did
    /// not supply are filled in by fixed-function hardware. What they are filled
    /// with is measured in `verify_45` §F rather than assumed, because SDL's
    /// header does not say and the three backends could in principle differ.
    int widening = 0;

    [[nodiscard]] int problems() const
    {
        return missing + extra + type_mismatch + overrun + duplicate;
    }
    [[nodiscard]] bool ok() const { return problems() == 0; }
};

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

    /// Describe one **per-instance** buffer binding: same slot-and-pitch shape as
    /// `vertex_buffer`, and one field different.
    ///
    /// That field is `input_rate`. A `VERTEX`-rate buffer advances by `pitch`
    /// once per vertex; an `INSTANCE`-rate buffer advances once per *instance*,
    /// so every vertex of instance 7 reads element 7. Nothing else changes — same
    /// buffer type, same attributes, same `SDL_BindGPUVertexBuffers` — which is
    /// why instancing costs so much less machinery than it sounds like it should.
    ///
    /// `instance_step_rate` is **not** exposed, because SDL's header says it is
    /// reserved and must be zero. A setter for a field with one legal value is an
    /// invitation to a bug.
    pipeline_desc& instance_buffer(Uint32 slot, Uint32 pitch);

    /// Compare the layout declared here against the inputs the shader declares.
    ///
    /// **This is the check Lesson 4.4 said nothing performs.** SDL does not
    /// perform it — Lesson 4.4 measured an attribute at a location the shader
    /// never declared being accepted without complaint — and the compiler cannot,
    /// because the two halves are written in different languages. So we do it,
    /// from the reflection JSON that Lesson 4.3's build already produces.
    ///
    /// Diagnostic, not a gate: it logs what it finds and returns the tally, and
    /// the caller decides. Every disagreement it can find is a bug, but some of
    /// them draw a picture anyway, and a lesson about layout wants those pictures.
    ///
    /// @param label prefix for the log lines — the shader's name reads best.
    [[nodiscard]] layout_report check_layout(const shader_inputs& inputs,
                                             const char* label) const;

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
