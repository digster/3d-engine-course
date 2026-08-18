// src/gfx/gpu_buffer.hpp — geometry (and later, uniforms) on the device.
//
// Lesson 4.4 built the smallest thing that can hold three vertices. Lesson 4.5
// adds the second kind of buffer every renderer needs, and the distinction
// between them is not about what they CONTAIN — both hold bytes — but about how
// often they are written:
//
//   `gpu_buffer`         written ONCE, at load time. Geometry. Its staging buffer
//                        is created for the upload and released immediately.
//   `gpu_stream_buffer`  written EVERY FRAME. Per-instance transforms, and from
//                        Module 6 anything else that changes. It KEEPS its staging
//                        buffer and cycles it, because creating one per frame is
//                        an allocation per frame, and because the write has to be
//                        protected against a read the GPU has not performed yet.
//
// Getting this backwards is a real cost in both directions: a one-shot buffer with
// permanent staging wastes memory that is never touched again, and a per-frame
// buffer with one-shot staging allocates 60 times a second and — worse — can
// overwrite data the GPU is still reading.
//
// The upload path is the one Lesson 4.2 built for textures, with the destination
// changed: CPU memory -> transfer buffer -> device buffer, the first hop happening
// now and the second recorded for later. If that sentence needs refreshing,
// `gpu_present_target::upload` is the same three lines with a texture on the end.

#pragma once

#include "gfx/gpu_device.hpp"

#include <SDL3/SDL.h>

#include <cstddef>

namespace engine {

/// Owns an `SDL_GPUBuffer`. Move-only, like every device resource here.
class gpu_buffer
{
public:
    gpu_buffer() = default;
    ~gpu_buffer();

    gpu_buffer(const gpu_buffer&) = delete;
    gpu_buffer& operator=(const gpu_buffer&) = delete;

    gpu_buffer(gpu_buffer&& other) noexcept;
    gpu_buffer& operator=(gpu_buffer&& other) noexcept;

    /// Allocate `size` bytes on the device.
    ///
    /// @param usage `SDL_GPU_BUFFERUSAGE_VERTEX`, `_INDEX`, and so on. It is a
    ///              bitmask, and it is not advisory: a buffer bound as a vertex
    ///              buffer must have been created with the VERTEX bit, and SDL
    ///              says so field by field in `SDL_GPUBufferBinding`.
    /// @param name  For debuggers and RenderDoc captures (Lesson 4.9). Buffers
    ///              have a real setter, `SDL_SetGPUBufferName` — unlike shaders,
    ///              which are named through a creation property because they are
    ///              immutable the moment they exist.
    [[nodiscard]] bool create(const gpu_device& dev, SDL_GPUBufferUsageFlags usage,
                              Uint32 size, const char* name = nullptr);

    /// Copy `bytes` of `data` into the buffer, through a staging buffer.
    ///
    /// The memcpy happens before this returns; the device-side copy is *recorded*
    /// into `cb` and happens when the GPU reaches it. Same split as every upload
    /// since Lesson 4.2, and the reason the staging buffer can be released
    /// immediately afterwards is documented rather than assumed: SDL frees a
    /// resource "as soon as it is safe to do so", which in an API with pending
    /// work means *after* the copy, not now.
    ///
    /// One-shot by design. A buffer written every frame wants a staging buffer it
    /// keeps and cycles — `gpu_present_target` does exactly that — and this one is
    /// for geometry uploaded once at startup.
    [[nodiscard]] bool upload(SDL_GPUCommandBuffer* cb, const void* data, Uint32 bytes);

    void destroy();

    [[nodiscard]] bool valid() const { return buffer_ != nullptr; }
    [[nodiscard]] SDL_GPUBuffer* handle() const { return buffer_; }
    [[nodiscard]] Uint32 size() const { return size_; }

private:
    SDL_GPUDevice* device_ = nullptr;   ///< NOT owned; gpu_device owns it
    SDL_GPUBuffer* buffer_ = nullptr;   ///< owning by contract
    Uint32 size_ = 0;
};

/// A device buffer written every frame, with the staging memory to do it.
///
/// Lesson 4.5. The per-instance data is this: 28 bytes per instance, rewritten
/// each frame while the geometry it decorates — 40 KB of torus — never moves
/// again. That ratio is the entire argument for instancing, and it is why this
/// type exists separately from `gpu_buffer` rather than as a flag on it.
///
/// **Cycling is the whole of the correctness story.** The buffer you are about to
/// write may still be referenced by last frame's command buffer, which the GPU may
/// not have reached. `cycle = true` hands you a *different* internal buffer in
/// exactly that case, so a write can never land on a read that has not happened.
/// Pass false and nothing goes wrong until the GPU falls behind — that is, until
/// somebody with a slower machine than yours runs it.
class gpu_stream_buffer
{
public:
    gpu_stream_buffer() = default;
    ~gpu_stream_buffer();

    gpu_stream_buffer(const gpu_stream_buffer&) = delete;
    gpu_stream_buffer& operator=(const gpu_stream_buffer&) = delete;

    gpu_stream_buffer(gpu_stream_buffer&& other) noexcept;
    gpu_stream_buffer& operator=(gpu_stream_buffer&& other) noexcept;

    /// Allocate `size` bytes on the device **and** a transfer buffer of the same
    /// size that stays alive for as long as this object does.
    [[nodiscard]] bool create(const gpu_device& dev, SDL_GPUBufferUsageFlags usage,
                              Uint32 size, const char* name = nullptr);

    /// Write `bytes` of `data`, cycling both halves.
    ///
    /// Fewer bytes than the buffer holds is normal and expected — the buffer is
    /// sized for the most instances we will ever draw and a frame may draw fewer.
    /// More is refused, as in `gpu_buffer::upload`.
    [[nodiscard]] bool write(SDL_GPUCommandBuffer* cb, const void* data, Uint32 bytes);

    void destroy();

    [[nodiscard]] bool valid() const { return buffer_ != nullptr; }
    [[nodiscard]] SDL_GPUBuffer* handle() const { return buffer_; }
    [[nodiscard]] Uint32 size() const { return size_; }

    /// Bytes written by the last `write`. The number the instancing argument is
    /// made of, so it is worth being able to print.
    [[nodiscard]] Uint32 last_write_bytes() const { return last_write_bytes_; }

private:
    SDL_GPUDevice* device_ = nullptr;            ///< NOT owned
    SDL_GPUBuffer* buffer_ = nullptr;            ///< owning by contract
    SDL_GPUTransferBuffer* staging_ = nullptr;   ///< owning by contract; kept, not recreated
    Uint32 size_ = 0;
    Uint32 last_write_bytes_ = 0;
};

} // namespace engine
