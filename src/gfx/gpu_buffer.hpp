// src/gfx/gpu_buffer.hpp — geometry (and later, indices and uniforms) on the device.
//
// Lesson 4.4. The smallest thing that can hold three vertices, and no more than
// that: Lesson 4.5 is where vertex layouts get taken seriously, and building the
// general case now would mean designing for requirements we have not met yet.
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

} // namespace engine
