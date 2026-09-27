// src/gfx/gpu_pipeline.cpp — filling in fifty-three fields, and timing the call
// that consumes them.

#include "gfx/gpu_pipeline.hpp"

#include <cstddef>
#include <utility>

namespace engine {

pipeline_desc::pipeline_desc(const gpu_device& dev, SDL_GPUShader* vertex,
                             SDL_GPUShader* fragment)
{
    info_.vertex_shader = vertex;
    info_.fragment_shader = fragment;

    // Three separate triangles per three vertices. TRIANGLESTRIP shares two
    // vertices between neighbours and is cheaper for a fan or a ribbon; a list
    // is what indexed geometry wants, and Lesson 2.12's meshes are indexed.
    info_.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    // ---- Rasterizer state ---------------------------------------------------
    //
    // EXPLICIT, not defaulted, and LEARNINGS.md records why: every enum here has
    // its first enumerator at zero, so a zero-initialised rasterizer state means
    // "CCW is front-facing, cull nothing". A forgotten cull_mode therefore shows
    // up as no culling at all rather than as an error — a silent bug wearing the
    // costume of a working program.
    info_.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info_.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info_.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info_.rasterizer_state.enable_depth_clip = true;

    // ---- Multisample state --------------------------------------------------
    // One sample. SDL requires sample_mask = 0 and enable_mask = false; the
    // header says both are reserved, and the zero-initialisation above already
    // set them, but naming the sample count is worth the line.
    info_.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;

    // ---- Depth-stencil state ------------------------------------------------
    // Off. There is one triangle and nothing to be hidden behind. Lesson 4.7
    // turns this on and it becomes Lesson 3.1's depth test, as a struct field —
    // note that `enable_depth_write` is documented as ignored while
    // `enable_depth_test` is false, so leaving both off is one decision.
    info_.depth_stencil_state.enable_depth_test = false;
    info_.depth_stencil_state.enable_depth_write = false;

    // ---- The target -----------------------------------------------------
    //
    // THE FORMAT IS NOT A CHOICE. It must equal the format of the texture this
    // pipeline will render into, and for the swapchain that is negotiated
    // between the display, the compositor and the driver. Lesson 4.2 asked and
    // logged it; this is what the answer was for.
    colour_targets_[0] = SDL_GPUColorTargetDescription{};
    colour_targets_[0].format = dev.report().swapchain_format;

    info_.target_info.color_target_descriptions = colour_targets_.data();
    info_.target_info.num_color_targets = 1;
    info_.target_info.has_depth_stencil_target = false;

    // The vertex input arrays start empty and are filled in by the two methods
    // below. Pointing at them here rather than in `info()` means the create-info
    // is coherent at every moment, not only after a build step nobody called.
    info_.vertex_input_state.vertex_buffer_descriptions = buffers_.data();
    info_.vertex_input_state.num_vertex_buffers = 0;
    info_.vertex_input_state.vertex_attributes = attributes_.data();
    info_.vertex_input_state.num_vertex_attributes = 0;
}

pipeline_desc& pipeline_desc::vertex_buffer(Uint32 slot, Uint32 pitch)
{
    if (buffer_count_ >= k_max_buffers) { return *this; }

    SDL_GPUVertexBufferDescription& d = buffers_[buffer_count_];
    d = SDL_GPUVertexBufferDescription{};
    d.slot = slot;
    d.pitch = pitch;
    d.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    d.instance_step_rate = 0;   // reserved; the header says it must be 0

    ++buffer_count_;
    info_.vertex_input_state.num_vertex_buffers = buffer_count_;
    return *this;
}

pipeline_desc& pipeline_desc::attribute(Uint32 location, Uint32 buffer_slot,
                                        SDL_GPUVertexElementFormat format, Uint32 offset)
{
    if (attribute_count_ >= k_max_attributes) { return *this; }

    SDL_GPUVertexAttribute& a = attributes_[attribute_count_];
    a = SDL_GPUVertexAttribute{};
    a.location = location;
    a.buffer_slot = buffer_slot;
    a.format = format;
    a.offset = offset;

    ++attribute_count_;
    info_.vertex_input_state.num_vertex_attributes = attribute_count_;
    return *this;
}

pipeline_desc& pipeline_desc::colour_target_format(SDL_GPUTextureFormat format)
{
    colour_targets_[0].format = format;
    return *this;
}

const SDL_GPUGraphicsPipelineCreateInfo& pipeline_desc::info()
{
    return info_;
}

gpu_pipeline::~gpu_pipeline()
{
    destroy();
}

gpu_pipeline::gpu_pipeline(gpu_pipeline&& other) noexcept
    : device_(other.device_), pipeline_(other.pipeline_), create_ms_(other.create_ms_)
{
    other.device_ = nullptr;
    other.pipeline_ = nullptr;
    other.create_ms_ = 0.0;
}

gpu_pipeline& gpu_pipeline::operator=(gpu_pipeline&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        device_ = other.device_;
        pipeline_ = other.pipeline_;
        create_ms_ = other.create_ms_;
        other.device_ = nullptr;
        other.pipeline_ = nullptr;
        other.create_ms_ = 0.0;
    }
    return *this;
}

bool gpu_pipeline::create(const gpu_device& dev, const SDL_GPUGraphicsPipelineCreateInfo& info)
{
    destroy();

    if (!dev.valid()) { return false; }
    device_ = dev.handle();

    // Timed, because Lesson 4.3 published a prediction about this call and a
    // prediction that is never checked is a decoration.
    const Uint64 t0 = SDL_GetPerformanceCounter();
    pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
    const Uint64 t1 = SDL_GetPerformanceCounter();

    create_ms_ = 1000.0 * static_cast<double>(t1 - t0)
               / static_cast<double>(SDL_GetPerformanceFrequency());

    if (pipeline_ == nullptr)
    {
        SDL_Log("SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
        destroy();
        return false;
    }
    return true;
}

void gpu_pipeline::destroy()
{
    if (device_ != nullptr && pipeline_ != nullptr)
    {
        SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
    }
    device_ = nullptr;
    pipeline_ = nullptr;
    create_ms_ = 0.0;
}

} // namespace engine
