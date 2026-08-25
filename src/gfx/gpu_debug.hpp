// src/gfx/gpu_debug.hpp — making a frame capture readable, and readable by us.
//
// Lesson 4.9. Everything in Module 4 so far has been about making the GPU draw
// the right thing. This file is about the other half of the job, which nobody
// warns you about until it costs you a day: **when the GPU draws the wrong
// thing, a breakpoint cannot help you.**
//
// Lesson 4.2's sentence — "nothing below this line executes when it is written"
// — is the whole reason. Every call you make records into a command buffer, the
// list runs later on another processor, and by the time the picture is wrong the
// stack that produced it is long gone. Stepping through `render()` in a debugger
// shows you a series of calls that all succeed, every pointer non-null, every
// return value fine, and a black window.
//
// A frame debugger is the answer, and there are three of them:
//
//   Windows / Linux / Android    RenderDoc     (Vulkan, D3D11, D3D12, GL, GLES)
//   macOS                        Xcode's Metal Debugger
//   Windows, D3D12               PIX
//
// **RenderDoc does not support Metal**, and that is not an oversight we can wait
// out — it is stated on the front page of its own documentation, and SDL_gpu.h's
// own "Debugging" section tells macOS readers to use Xcode instead. If you are on
// a Mac, the tool is different and every concept in this file is identical.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE PROVIDES, AND WHY EACH PIECE
// ---------------------------------------------------------------------------
//
//   1. NAMES, attached at creation. A capture of an un-named frame is a list of
//      "Buffer 47" and "Texture 12" and you will spend your debugging time
//      working out which is which instead of what is wrong.
//   2. DEBUG GROUPS, as an RAII scope. They turn a flat list of two hundred
//      events into a tree with your own headings on it.
//   3. A FRAME LOG — the ten percent of a frame debugger you can write in an
//      afternoon. It records the same event stream a capture shows, as text, so
//      that the mental model is available to everybody on every platform before
//      any tool is installed. §3.6 of the lesson is honest about the ninety
//      percent it cannot do.

#pragma once

#include <SDL3/SDL.h>

#include <cstddef>

namespace engine {

// ---------------------------------------------------------------------------
// 1. Names, attached where SDL asks for them
// ---------------------------------------------------------------------------
//
// **THIS CORRECTS A COMMENT THIS CODEBASE HAS CARRIED SINCE LESSON 4.3.**
//
// `gpu_shader.cpp` names its shader through a creation property, and explained
// the asymmetry like this: buffers and textures have `SDL_SetGPUBufferName` /
// `SDL_SetGPUTextureName`, "and a SHADER DOES NOT … because a shader is
// immutable the moment it exists — there is no later at which to set anything on
// it."
//
// That is a tidy story reasoned from a pattern, and it is wrong. Read what
// SDL_gpu.h says about the setter it offers:
//
//     "You should use SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING with
//      SDL_CreateGPUBuffer instead of this function to avoid thread safety
//      issues."
//
// So the creation property is not the shader's consolation prize for being
// immutable; it is **the recommended path for all three**, and the setters are
// the older API that SDL now steers you away from. `SDL_SetGPUBufferName` is
// documented "not thread safe: you must make sure the buffer is not
// simultaneously used by any other thread" — which for a name is an absurd
// precondition to accept when a create-time property costs three lines.
//
// The general lesson is worth more than the fix: **a confident explanation of an
// API asymmetry is a hypothesis, and the documentation of the function you are
// already calling is the cheapest place to check it.** Lesson 4.3's comment was
// written by somebody who had just read `SDL_CreateGPUShader`'s docs carefully
// and had not read `SDL_SetGPUBufferName`'s at all.

/// An `SDL_PropertiesID` that releases itself — RAII for a C handle.
///
/// Three lines of ceremony are needed to attach a name at creation (create the
/// properties, set the string, destroy them), and the destroy is the one you
/// forget on the early-return path. Same reasoning as every other resource type
/// in this engine: if a thing must be released, do not rely on remembering.
///
/// **Movable, not copyable**, and a moved-from object releases nothing.
class scoped_properties
{
public:
    scoped_properties() : id_(SDL_CreateProperties()) {}
    ~scoped_properties() { if (id_ != 0) { SDL_DestroyProperties(id_); } }

    scoped_properties(const scoped_properties&) = delete;
    scoped_properties& operator=(const scoped_properties&) = delete;

    scoped_properties(scoped_properties&& other) noexcept : id_(other.id_)
    {
        other.id_ = 0;
    }
    scoped_properties& operator=(scoped_properties&& other) noexcept
    {
        if (this != &other)
        {
            if (id_ != 0) { SDL_DestroyProperties(id_); }
            id_ = other.id_;
            other.id_ = 0;
        }
        return *this;
    }

    /// Zero on failure, which every SDL create-info accepts as "no properties" —
    /// so a failed allocation costs a name, never a resource.
    [[nodiscard]] SDL_PropertiesID id() const { return id_; }

    /// Set the standard name property. `key` is one of SDL's
    /// `SDL_PROP_GPU_*_CREATE_NAME_STRING` constants.
    void set_name(const char* key, const char* name)
    {
        if (id_ != 0 && name != nullptr) { SDL_SetStringProperty(id_, key, name); }
    }

private:
    SDL_PropertiesID id_ = 0;
};

/// `SDL_CreateGPUBuffer`, with the name attached the way SDL asks for.
///
/// A thin wrapper rather than a leaky one: it takes the create-info the caller
/// already built and only supplies `props`. A `nullptr` name is not an error —
/// it means "unnamed", and the call is then exactly `SDL_CreateGPUBuffer`.
[[nodiscard]] SDL_GPUBuffer* create_named_buffer(SDL_GPUDevice* device,
                                                 SDL_GPUBufferCreateInfo info,
                                                 const char* name);

/// `SDL_CreateGPUTexture`, likewise.
[[nodiscard]] SDL_GPUTexture* create_named_texture(SDL_GPUDevice* device,
                                                   SDL_GPUTextureCreateInfo info,
                                                   const char* name);

/// `SDL_CreateGPUTransferBuffer`, likewise — and this one is new territory
/// rather than a correction. **No transfer buffer in this engine has ever been
/// named**, which is why every capture taken before this lesson shows the
/// uploads as anonymous traffic. A transfer buffer is the most confusing thing
/// in a capture to meet without a name, because there are usually several and
/// they all look identical.
[[nodiscard]] SDL_GPUTransferBuffer* create_named_transfer_buffer(
    SDL_GPUDevice* device, SDL_GPUTransferBufferCreateInfo info, const char* name);

// ---------------------------------------------------------------------------
// 2. Debug groups
// ---------------------------------------------------------------------------

class frame_log;   // below; a group reports itself to one if given

/// A named region of the command stream, opened and closed by scope.
///
/// In a capture this becomes a collapsible heading over the events inside it, so
/// a frame reads as "upload → clear → blit → scene → 3 draws" instead of as two
/// hundred undifferentiated calls. It costs nothing at runtime worth measuring
/// (§3.5 puts a number on it) and it is the difference between a capture you can
/// navigate and one you scroll.
///
/// **The scope is not decoration — it is how the Metal rule is obeyed.**
/// SDL_gpu.h says:
///
///     "On some backends (e.g. Metal), pushing a debug group during a
///      render/blit/compute pass will create a group that is scoped to the
///      native pass rather than the command buffer. For best results, if you
///      push a debug group during a pass, always pop it in the same pass."
///
/// A C++ block gives you that for free, provided the block does not straddle a
/// pass boundary — so this type is **neither copyable nor movable**, which makes
/// it impossible to stash one somewhere it would outlive its pass. An unbalanced
/// push/pop is not a crash; it is a capture whose tree is wrong in a way that
/// wastes your time precisely when you have least of it.
class debug_group
{
public:
    /// A null `cb` is legal and does nothing, so a caller need not branch on
    /// whether it has a command buffer this frame.
    debug_group(SDL_GPUCommandBuffer* cb, const char* name, frame_log* log = nullptr);
    ~debug_group();

    debug_group(const debug_group&) = delete;
    debug_group& operator=(const debug_group&) = delete;
    debug_group(debug_group&&) = delete;
    debug_group& operator=(debug_group&&) = delete;

private:
    SDL_GPUCommandBuffer* cb_ = nullptr;
    frame_log* log_ = nullptr;
};

// ---------------------------------------------------------------------------
// 3. The frame log
// ---------------------------------------------------------------------------

/// What kind of thing happened. Deliberately the same vocabulary a capture's
/// event browser uses, so that the two can be read against each other.
enum class gpu_event_kind
{
    group_begin,
    group_end,
    label,          ///< a one-off marker: `SDL_InsertGPUDebugLabel`
    pass_begin,
    pass_end,
    bind_pipeline,
    bind_vertex,
    bind_index,
    bind_sampler,
    push_uniform,
    draw,
    copy,
    blit
};

[[nodiscard]] const char* name_of(gpu_event_kind k);

/// One recorded event: what happened, how deep in the group tree, and up to
/// three numbers whose meaning depends on the kind.
///
/// The name is a fixed buffer rather than a `std::string` for a reason that is
/// not premature optimisation: this is instrumentation, it runs inside the frame
/// it is measuring, and an allocation per event would make the thing being
/// measured different from the thing that ships. Forty-eight bytes holds every
/// name this engine actually uses; longer ones are truncated, which is the right
/// failure for a debugging aid.
struct gpu_event
{
    gpu_event_kind kind = gpu_event_kind::label;
    int depth = 0;
    char name[48] = {};
    Uint32 a = 0;   ///< draw: index/vertex count · push: slot · bind: slot
    Uint32 b = 0;   ///< draw: instances          · push: bytes
    Uint32 c = 0;   ///< draw: triangles
};

/// A frame's command stream, as text.
///
/// **This is the ten percent of a frame debugger you can write yourself**, and
/// writing it is worth doing even though the real tools exist, for three
/// reasons. It works on every platform, including the one RenderDoc does not
/// support. It runs in CI, where a GUI cannot. And it makes the *shape* of a
/// frame something you can assert on — `verify_49` §D checks that the log and
/// the renderer's own `draw_stats` agree, which is a test that a real capture
/// cannot give you.
///
/// What it cannot do is §3.6's subject and the reason the tools exist: it cannot
/// show you the *contents* of a resource at an event, replay the frame stopped
/// at a draw, tell you which pipeline state was live, or measure anything on the
/// GPU's side of the fence.
///
/// Fixed capacity, and full is not an error — it stops recording and says so, on
/// the same principle as the name buffer above.
class frame_log
{
public:
    static constexpr int k_max_events = 512;

    /// Start recording a new frame. Cheap: it resets two integers.
    void begin();

    /// Stop recording, so the events survive to be printed after submission.
    void end() { recording_ = false; }

    [[nodiscard]] bool recording() const { return recording_; }

    /// Record one event. Ignored unless `begin()` has been called and there is
    /// room, so every call site can be unconditional.
    void record(gpu_event_kind kind, const char* name,
                Uint32 a = 0, Uint32 b = 0, Uint32 c = 0);

    /// Group nesting is tracked here rather than by the caller, so the tree in
    /// the printout cannot disagree with the pushes that produced it.
    void push_group(const char* name);
    void pop_group();

    /// Print the frame as an indented tree, through `SDL_Log`.
    void print() const;

    [[nodiscard]] int count() const { return count_; }
    [[nodiscard]] bool overflowed() const { return overflowed_; }

    /// How many `draw` events were recorded — the number `verify_49` §D checks
    /// against `draw_stats::draws`.
    [[nodiscard]] int draws() const;

    /// Total bytes across every `push_uniform` event.
    [[nodiscard]] Uint32 uniform_bytes() const;

    [[nodiscard]] const gpu_event& operator[](int i) const { return events_[i]; }

private:
    gpu_event events_[k_max_events];
    int count_ = 0;
    int depth_ = 0;
    bool recording_ = false;
    bool overflowed_ = false;
};

} // namespace engine
