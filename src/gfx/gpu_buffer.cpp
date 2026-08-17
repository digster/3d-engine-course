// src/gfx/gpu_buffer.cpp — allocating a device buffer, and getting bytes into it.

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

} // namespace engine
