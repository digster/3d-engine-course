// src/gfx/gpu_buffer.cpp — allocating a device buffer, and getting bytes into it
// once (gpu_buffer) or every frame (gpu_stream_buffer, Lesson 4.5).

#include "gfx/gpu_buffer.hpp"

#include <cstring>
#include <utility>

namespace engine {

gpu_buffer::~gpu_buffer()
{
    destroy();
}

gpu_buffer::gpu_buffer(gpu_buffer&& other) noexcept
    : device_(other.device_), buffer_(other.buffer_), size_(other.size_)
{
    other.device_ = nullptr;
    other.buffer_ = nullptr;
    other.size_ = 0;
}

gpu_buffer& gpu_buffer::operator=(gpu_buffer&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        device_ = other.device_;
        buffer_ = other.buffer_;
        size_ = other.size_;
        other.device_ = nullptr;
        other.buffer_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

bool gpu_buffer::create(const gpu_device& dev, SDL_GPUBufferUsageFlags usage,
                        Uint32 size, const char* name)
{
    destroy();

    if (!dev.valid() || size == 0) { return false; }

    device_ = dev.handle();

    SDL_GPUBufferCreateInfo info{};
    info.usage = usage;
    info.size = size;

    buffer_ = SDL_CreateGPUBuffer(device_, &info);
    if (buffer_ == nullptr)
    {
        SDL_Log("SDL_CreateGPUBuffer(%u bytes) failed: %s", size, SDL_GetError());
        destroy();
        return false;
    }

    if (name != nullptr) { SDL_SetGPUBufferName(device_, buffer_, name); }
    size_ = size;
    return true;
}

bool gpu_buffer::upload(SDL_GPUCommandBuffer* cb, const void* data, Uint32 bytes)
{
    if (!valid() || cb == nullptr || data == nullptr) { return false; }
    if (bytes > size_)
    {
        // Refuse rather than truncate. A short upload leaves the tail of the
        // buffer undefined, and undefined geometry draws triangles reaching off
        // to infinity — a spectacular symptom for a mundane arithmetic slip.
        SDL_Log("gpu_buffer::upload: %u bytes into a %u-byte buffer", bytes, size_);
        return false;
    }

    // ---- The staging buffer, created for this one upload --------------------
    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = bytes;

    SDL_GPUTransferBuffer* staging = SDL_CreateGPUTransferBuffer(device_, &tb);
    if (staging == nullptr)
    {
        SDL_Log("SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        return false;
    }

    // ---- Hop 1: now, on the CPU ---------------------------------------------
    void* mapped = SDL_MapGPUTransferBuffer(device_, staging, false);
    if (mapped == nullptr)
    {
        SDL_Log("SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device_, staging);
        return false;
    }
    std::memcpy(mapped, data, bytes);
    SDL_UnmapGPUTransferBuffer(device_, staging);

    // ---- Hop 2: recorded now, executed later --------------------------------
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);

    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = staging;
    source.offset = 0;

    SDL_GPUBufferRegion dest{};
    dest.buffer = buffer_;
    dest.offset = 0;
    dest.size = bytes;

    // cycle = false: this buffer is written once, at startup, and nothing is
    // reading it yet. Cycling here would allocate a second copy of the geometry
    // to protect against a hazard that does not exist. The per-frame case is the
    // opposite and `gpu_present_target::upload` passes true for exactly that
    // reason.
    SDL_UploadToGPUBuffer(copy, &source, &dest, false);

    SDL_EndGPUCopyPass(copy);

    // Released while the copy that reads it is still only RECORDED — and that is
    // safe by documentation, not by luck. SDL_gpu.h: "Frees the given transfer
    // buffer AS SOON AS IT IS SAFE TO DO SO." A deferred free is a strange idea
    // until you remember that in this API everything is deferred.
    SDL_ReleaseGPUTransferBuffer(device_, staging);
    return true;
}

void gpu_buffer::destroy()
{
    if (device_ != nullptr && buffer_ != nullptr)
    {
        SDL_ReleaseGPUBuffer(device_, buffer_);
    }
    device_ = nullptr;
    buffer_ = nullptr;
    size_ = 0;
}

// ===========================================================================
// gpu_stream_buffer — Lesson 4.5
// ===========================================================================

gpu_stream_buffer::~gpu_stream_buffer()
{
    destroy();
}

gpu_stream_buffer::gpu_stream_buffer(gpu_stream_buffer&& other) noexcept
    : device_(other.device_), buffer_(other.buffer_), staging_(other.staging_),
      size_(other.size_), last_write_bytes_(other.last_write_bytes_)
{
    other.device_ = nullptr;
    other.buffer_ = nullptr;
    other.staging_ = nullptr;
    other.size_ = 0;
    other.last_write_bytes_ = 0;
}

gpu_stream_buffer& gpu_stream_buffer::operator=(gpu_stream_buffer&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        device_ = other.device_;
        buffer_ = other.buffer_;
        staging_ = other.staging_;
        size_ = other.size_;
        last_write_bytes_ = other.last_write_bytes_;
        other.device_ = nullptr;
        other.buffer_ = nullptr;
        other.staging_ = nullptr;
        other.size_ = 0;
        other.last_write_bytes_ = 0;
    }
    return *this;
}

bool gpu_stream_buffer::create(const gpu_device& dev, SDL_GPUBufferUsageFlags usage,
                               Uint32 size, const char* name)
{
    destroy();

    if (!dev.valid() || size == 0) { return false; }

    device_ = dev.handle();

    SDL_GPUBufferCreateInfo info{};
    info.usage = usage;
    info.size = size;

    buffer_ = SDL_CreateGPUBuffer(device_, &info);
    if (buffer_ == nullptr)
    {
        SDL_Log("SDL_CreateGPUBuffer(%u bytes, streaming) failed: %s", size, SDL_GetError());
        destroy();
        return false;
    }

    // The staging buffer, created ONCE. This single line is the whole difference
    // from `gpu_buffer::upload`, which creates and releases one per call — right
    // for a load-time upload, an allocation per frame for this one.
    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = size;

    staging_ = SDL_CreateGPUTransferBuffer(device_, &tb);
    if (staging_ == nullptr)
    {
        SDL_Log("SDL_CreateGPUTransferBuffer(%u bytes) failed: %s", size, SDL_GetError());
        destroy();
        return false;
    }

    if (name != nullptr) { SDL_SetGPUBufferName(device_, buffer_, name); }
    size_ = size;
    return true;
}

bool gpu_stream_buffer::write(SDL_GPUCommandBuffer* cb, const void* data, Uint32 bytes)
{
    if (!valid() || cb == nullptr || data == nullptr) { return false; }
    if (bytes == 0) { last_write_bytes_ = 0; return true; }
    if (bytes > size_)
    {
        SDL_Log("gpu_stream_buffer::write: %u bytes into a %u-byte buffer", bytes, size_);
        return false;
    }

    // ---- Hop 1: now, on the CPU --------------------------------------------
    //
    // `cycle = true`. See the class comment: the previous frame's copy pass may
    // still be reading this staging buffer, and cycling is how SDL_GPU lets us
    // write without waiting to find out.
    void* mapped = SDL_MapGPUTransferBuffer(device_, staging_, true);
    if (mapped == nullptr)
    {
        SDL_Log("SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        return false;
    }
    std::memcpy(mapped, data, bytes);
    SDL_UnmapGPUTransferBuffer(device_, staging_);

    // ---- Hop 2: recorded now, executed later --------------------------------
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);

    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = staging_;
    source.offset = 0;

    SDL_GPUBufferRegion dest{};
    dest.buffer = buffer_;
    dest.offset = 0;
    dest.size = bytes;

    // Cycle the DESTINATION too, and against the same hazard on the other side:
    // last frame's draw may still be reading the device buffer.
    SDL_UploadToGPUBuffer(copy, &source, &dest, true);

    SDL_EndGPUCopyPass(copy);

    last_write_bytes_ = bytes;
    return true;
}

void gpu_stream_buffer::destroy()
{
    if (device_ != nullptr)
    {
        if (staging_ != nullptr) { SDL_ReleaseGPUTransferBuffer(device_, staging_); }
        if (buffer_ != nullptr) { SDL_ReleaseGPUBuffer(device_, buffer_); }
    }
    device_ = nullptr;
    buffer_ = nullptr;
    staging_ = nullptr;
    size_ = 0;
    last_write_bytes_ = 0;
}

} // namespace engine
