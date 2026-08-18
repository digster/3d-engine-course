// src/gfx/gpu_pipeline.cpp — filling in fifty-three fields, timing the call that
// consumes them, and (Lesson 4.5) checking the half of them nobody else checks.

#include "gfx/gpu_pipeline.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace engine {

// ---------------------------------------------------------------------------
// Lesson 4.5 — the two questions you can ask about a vertex element format
// ---------------------------------------------------------------------------
//
// They have different answers, and confusing them is a whole class of bug.
// `size_of` is about the BUFFER: how many bytes this attribute eats, which is
// what decides whether the layout fits inside the pitch. `shader_type_of` is
// about the REGISTER: what the shader ends up holding, which is what decides
// whether the declaration on the HLSL side agrees.
//
// For FLOAT3 the two agree in spirit — twelve bytes, a `float3`. For UBYTE4_NORM
// they do not: four bytes in the buffer, a `float4` in the shader, with the
// divide by 255 done by fixed-function hardware for free.

Uint32 size_of(SDL_GPUVertexElementFormat format)
{
    switch (format)
    {
    case SDL_GPU_VERTEXELEMENTFORMAT_INT:
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT:
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT:          return 4;

    case SDL_GPU_VERTEXELEMENTFORMAT_INT2:
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT2:
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2:         return 8;

    case SDL_GPU_VERTEXELEMENTFORMAT_INT3:
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT3:
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3:         return 12;

    case SDL_GPU_VERTEXELEMENTFORMAT_INT4:
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT4:
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4:         return 16;

    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE2:
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2:
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE2_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM:    return 2;

    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE4:
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4:
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM:    return 4;

    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT2:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT2:
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT2_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_HALF2:          return 4;

    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT4:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT4:
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_HALF4:          return 8;

    case SDL_GPU_VERTEXELEMENTFORMAT_INVALID:
    default:                                         return 0;
    }
}

const char* shader_type_of(SDL_GPUVertexElementFormat format)
{
    switch (format)
    {
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT:          return "float";
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2:         return "float2";
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3:         return "float3";
    case SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4:         return "float4";

    case SDL_GPU_VERTEXELEMENTFORMAT_INT:            return "int";
    case SDL_GPU_VERTEXELEMENTFORMAT_INT2:           return "int2";
    case SDL_GPU_VERTEXELEMENTFORMAT_INT3:           return "int3";
    case SDL_GPU_VERTEXELEMENTFORMAT_INT4:           return "int4";

    case SDL_GPU_VERTEXELEMENTFORMAT_UINT:           return "uint";
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT2:          return "uint2";
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT3:          return "uint3";
    case SDL_GPU_VERTEXELEMENTFORMAT_UINT4:          return "uint4";

    // The integer formats arrive as integers...
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE2:
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT2:         return "int2";
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE4:
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT4:         return "int4";
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT2:        return "uint2";
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT4:        return "uint4";

    // ...and the _NORM ones do NOT. This is the interesting row of the table:
    // the bytes are integers and the shader sees floats, because the hardware
    // divides by 255 (or 32767, or 65535) on the way in. Half floats likewise
    // expand to full floats for free.
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE2_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE2_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT2_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_HALF2:          return "float2";
    case SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_USHORT4_NORM:
    case SDL_GPU_VERTEXELEMENTFORMAT_HALF4:          return "float4";

    case SDL_GPU_VERTEXELEMENTFORMAT_INVALID:
    default:                                         return "";
    }
}

namespace {

/// Split "float3" into ("float", 3). A trailing digit is the component count;
/// its absence means one component.
void split_type(const char* type, std::string_view& base, int& components)
{
    std::string_view t(type);
    components = 1;
    if (!t.empty() && t.back() >= '2' && t.back() <= '4')
    {
        components = t.back() - '0';
        t.remove_suffix(1);
    }
    base = t;
}

void split_type(const std::string& type, std::string_view& base, int& components)
{
    split_type(type.c_str(), base, components);
}

} // namespace

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

pipeline_desc& pipeline_desc::instance_buffer(Uint32 slot, Uint32 pitch)
{
    if (buffer_count_ >= k_max_buffers) { return *this; }

    SDL_GPUVertexBufferDescription& d = buffers_[buffer_count_];
    d = SDL_GPUVertexBufferDescription{};
    d.slot = slot;
    d.pitch = pitch;

    // THE ONE DIFFERENT FIELD. Everything else about this binding — the buffer
    // type, the attributes that read it, the call that binds it — is identical to
    // a per-vertex one. `INSTANCE` says: advance by `pitch` once per instance, so
    // every vertex of instance 7 reads element 7 of this buffer.
    d.input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
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

layout_report pipeline_desc::check_layout(const shader_inputs& inputs, const char* label) const
{
    layout_report r;
    const char* who = (label != nullptr) ? label : "pipeline";

    // ---- Every attribute we declared, against the shader --------------------
    for (Uint32 i = 0; i < attribute_count_; ++i)
    {
        const SDL_GPUVertexAttribute& a = attributes_[i];

        // SDL's header states the rule plainly: "All vertex attribute locations
        // provided to an SDL_GPUVertexInputState must be unique." It does not say
        // what happens otherwise, and a rule with no stated consequence is a rule
        // you want to be checking yourself.
        for (Uint32 j = 0; j < i; ++j)
        {
            if (attributes_[j].location == a.location)
            {
                ++r.duplicate;
                SDL_Log("  layout %s: TWO attributes at location %u — SDL requires them unique",
                        who, a.location);
                break;
            }
        }

        // Does the attribute fit inside the vertex it claims to live in? An
        // offset past the pitch is not caught anywhere else, and it reads the
        // NEXT vertex's bytes rather than failing.
        for (Uint32 b = 0; b < buffer_count_; ++b)
        {
            if (buffers_[b].slot != a.buffer_slot) { continue; }

            const Uint32 end = a.offset + size_of(a.format);
            if (end > buffers_[b].pitch)
            {
                ++r.overrun;
                SDL_Log("  layout %s: location %u ends at byte %u of a %u-byte vertex"
                        " — it reads into the NEXT one",
                        who, a.location, end, buffers_[b].pitch);
            }
            break;
        }

        // The cross-language half: does the shader declare this location at all,
        // and if so does it expect what we are sending?
        const shader_input* match = nullptr;
        for (const shader_input& in : inputs)
        {
            if (in.location == a.location) { match = &in; break; }
        }

        if (match == nullptr)
        {
            // Lesson 4.4 §B measured this being accepted by pipeline creation
            // without a word. It is harmless to the picture and it is almost
            // always a typo, so it is worth a line.
            ++r.extra;
            SDL_Log("  layout %s: location %u is supplied but the shader never declares it",
                    who, a.location);
            continue;
        }

        ++r.checked;

        std::string_view ours_base;
        std::string_view theirs_base;
        int ours_n = 0;
        int theirs_n = 0;
        split_type(shader_type_of(a.format), ours_base, ours_n);
        split_type(match->type, theirs_base, theirs_n);

        if (ours_base != theirs_base)
        {
            // The bad kind. Float bits read as an integer are not a small error;
            // 1.0f read as an int is 1065353216.
            ++r.type_mismatch;
            SDL_Log("  layout %s: location %u (%s) supplies %s, the shader reads %s"
                    " — the BITS are reinterpreted, not converted",
                    who, a.location, match->name.c_str(),
                    shader_type_of(a.format), match->type.c_str());
        }
        else if (ours_n < theirs_n)
        {
            // The legal kind — see `layout_report::widening`.
            ++r.widening;
            SDL_Log("  layout %s: location %u (%s) supplies %s into a %s"
                    " — hardware fills the rest; verify_45 §F says with what",
                    who, a.location, match->name.c_str(),
                    shader_type_of(a.format), match->type.c_str());
        }
        else if (ours_n > theirs_n)
        {
            // Also legal, also worth saying: the extra components are read out of
            // the buffer and then discarded, which is bandwidth spent on nothing.
            ++r.widening;
            SDL_Log("  layout %s: location %u (%s) supplies %s into a %s"
                    " — the extra components are fetched and thrown away",
                    who, a.location, match->name.c_str(),
                    shader_type_of(a.format), match->type.c_str());
        }
    }

    // ---- Every input the shader declared, against us ------------------------
    //
    // THE ONE THAT ACTUALLY BITES. A shader input nothing supplies does not stop
    // pipeline creation and does not stop the draw; it reads undefined data, and
    // undefined data in a POSITION is geometry stretching off to infinity.
    for (const shader_input& in : inputs)
    {
        bool supplied = false;
        for (Uint32 i = 0; i < attribute_count_; ++i)
        {
            if (attributes_[i].location == in.location) { supplied = true; break; }
        }
        if (!supplied)
        {
            ++r.missing;
            SDL_Log("  layout %s: the shader declares %s (%s) at location %u"
                    " and NOTHING supplies it",
                    who, in.name.c_str(), in.type.c_str(), in.location);
        }
    }

    if (r.ok())
    {
        SDL_Log("  layout %s: %d attribute%s checked against the reflection, no problems%s",
                who, r.checked, r.checked == 1 ? "" : "s",
                r.widening > 0 ? " (see the note above)" : "");
    }
    return r;
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
