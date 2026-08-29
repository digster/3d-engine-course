// engine/src/gfx/gpu_debug.cpp — see gpu_debug.hpp for why this file exists.

#include <engine/gfx/gpu_debug.hpp>

#include <engine/core/log.hpp>

#include <cstring>

namespace engine {

// ---------------------------------------------------------------------------
// Names
// ---------------------------------------------------------------------------

SDL_GPUBuffer* create_named_buffer(SDL_GPUDevice* device,
                                   SDL_GPUBufferCreateInfo info, const char* name)
{
    if (device == nullptr) { return nullptr; }

    scoped_properties props;
    props.set_name(SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING, name);

    // A zero properties id is what SDL means by "none", so a failed allocation
    // silently costs the name and never the buffer. That ordering matters: a
    // debugging aid must not be able to break the thing it is aiding.
    info.props = (name != nullptr) ? props.id() : 0;
    return SDL_CreateGPUBuffer(device, &info);
}

SDL_GPUTexture* create_named_texture(SDL_GPUDevice* device,
                                     SDL_GPUTextureCreateInfo info, const char* name)
{
    if (device == nullptr) { return nullptr; }

    scoped_properties props;
    props.set_name(SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, name);

    info.props = (name != nullptr) ? props.id() : 0;
    return SDL_CreateGPUTexture(device, &info);
}

SDL_GPUTransferBuffer* create_named_transfer_buffer(SDL_GPUDevice* device,
                                                    SDL_GPUTransferBufferCreateInfo info,
                                                    const char* name)
{
    if (device == nullptr) { return nullptr; }

    scoped_properties props;
    props.set_name(SDL_PROP_GPU_TRANSFERBUFFER_CREATE_NAME_STRING, name);

    info.props = (name != nullptr) ? props.id() : 0;
    return SDL_CreateGPUTransferBuffer(device, &info);
}

// ---------------------------------------------------------------------------
// Debug groups
// ---------------------------------------------------------------------------

debug_group::debug_group(SDL_GPUCommandBuffer* cb, const char* name, frame_log* log)
    : cb_(cb), log_(log)
{
    if (cb_ != nullptr && name != nullptr) { SDL_PushGPUDebugGroup(cb_, name); }
    if (log_ != nullptr) { log_->push_group(name); }
}

debug_group::~debug_group()
{
    // Popped in the destructor, so the pop happens on every path out of the
    // block including an early return — and, on Metal, inside the same pass the
    // push happened in (SDL_gpu.h's rule, quoted in the header).
    if (cb_ != nullptr) { SDL_PopGPUDebugGroup(cb_); }
    if (log_ != nullptr) { log_->pop_group(); }
}

// ---------------------------------------------------------------------------
// The frame log
// ---------------------------------------------------------------------------

const char* name_of(gpu_event_kind k)
{
    switch (k)
    {
    case gpu_event_kind::group_begin:   return "group";
    case gpu_event_kind::group_end:     return "end";
    case gpu_event_kind::label:         return "label";
    case gpu_event_kind::pass_begin:    return "pass";
    case gpu_event_kind::pass_end:      return "end pass";
    case gpu_event_kind::bind_pipeline: return "pipeline";
    case gpu_event_kind::bind_vertex:   return "vertex buf";
    case gpu_event_kind::bind_index:    return "index buf";
    case gpu_event_kind::bind_sampler:  return "sampler";
    case gpu_event_kind::push_uniform:  return "uniform";
    case gpu_event_kind::draw:          return "DRAW";
    case gpu_event_kind::copy:          return "copy";
    case gpu_event_kind::blit:          return "blit";
    }
    return "?";
}

void frame_log::begin()
{
    count_ = 0;
    depth_ = 0;
    recording_ = true;
    overflowed_ = false;
}

void frame_log::record(gpu_event_kind kind, const char* name,
                       Uint32 a, Uint32 b, Uint32 c)
{
    if (!recording_) { return; }
    if (count_ >= k_max_events) { overflowed_ = true; return; }

    gpu_event& e = events_[count_++];
    e.kind = kind;
    e.depth = depth_;
    e.a = a;
    e.b = b;
    e.c = c;

    // Truncating rather than allocating. `SDL_strlcpy` always null-terminates,
    // which `strncpy` famously does not — the one-line difference that turns a
    // truncated name into a buffer overrun in every C codebase that has ever had
    // one.
    if (name != nullptr) { SDL_strlcpy(e.name, name, sizeof(e.name)); }
    else { e.name[0] = '\0'; }
}

void frame_log::push_group(const char* name)
{
    record(gpu_event_kind::group_begin, name);
    ++depth_;
}

void frame_log::pop_group()
{
    if (depth_ > 0) { --depth_; }
    record(gpu_event_kind::group_end, nullptr);
}

int frame_log::draws() const
{
    int n = 0;
    for (int i = 0; i < count_; ++i)
    {
        if (events_[i].kind == gpu_event_kind::draw) { ++n; }
    }
    return n;
}

Uint32 frame_log::uniform_bytes() const
{
    Uint32 total = 0;
    for (int i = 0; i < count_; ++i)
    {
        if (events_[i].kind == gpu_event_kind::push_uniform) { total += events_[i].b; }
    }
    return total;
}

void frame_log::print() const
{
    ENGINE_LOG_INFO(engine::log_gpu, "---- frame: %d events%s ----", count_,
            overflowed_ ? "  (TRUNCATED — raise frame_log::k_max_events)" : "");

    for (int i = 0; i < count_; ++i)
    {
        const gpu_event& e = events_[i];
        if (e.kind == gpu_event_kind::group_end) { continue; }   // the tree shows it

        // Two spaces per level. A tree drawn with box characters looks better and
        // is worse: this output is meant to be grepped and diffed, and every
        // frame-capture tool's text export makes the same choice.
        char indent[32];
        const int pad = (e.depth * 2 < 30) ? e.depth * 2 : 30;
        for (int k = 0; k < pad; ++k) { indent[k] = ' '; }
        indent[pad] = '\0';

        switch (e.kind)
        {
        case gpu_event_kind::draw:
            ENGINE_LOG_INFO(engine::log_gpu, "  %s%-10s %-24s %u indices, %u instance(s), %u triangles",
                    indent, name_of(e.kind), e.name, e.a, e.b, e.c);
            break;
        case gpu_event_kind::push_uniform:
            ENGINE_LOG_INFO(engine::log_gpu, "  %s%-10s %-24s slot %u, %u bytes",
                    indent, name_of(e.kind), e.name, e.a, e.b);
            break;
        case gpu_event_kind::bind_vertex:
        case gpu_event_kind::bind_index:
        case gpu_event_kind::bind_sampler:
            ENGINE_LOG_INFO(engine::log_gpu, "  %s%-10s %-24s slot %u", indent, name_of(e.kind), e.name, e.a);
            break;
        case gpu_event_kind::copy:
        case gpu_event_kind::blit:
            ENGINE_LOG_INFO(engine::log_gpu, "  %s%-10s %-24s %u bytes", indent, name_of(e.kind), e.name, e.b);
            break;
        default:
            ENGINE_LOG_INFO(engine::log_gpu, "  %s%-10s %s", indent, name_of(e.kind), e.name);
            break;
        }
    }

    ENGINE_LOG_INFO(engine::log_gpu, "---- %d draw(s), %u uniform bytes ----", draws(), uniform_bytes());
}

} // namespace engine
