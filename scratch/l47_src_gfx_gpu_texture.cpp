// src/gfx/gpu_texture.cpp — the upload path Lesson 4.2 built, aimed at an asset.

#include "gfx/gpu_texture.hpp"

#include <cstring>
#include <utility>

namespace engine {

int depth_bits(SDL_GPUTextureFormat format)
{
    switch (format)
    {
    case SDL_GPU_TEXTUREFORMAT_D16_UNORM:         return 16;
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT: return 24;
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT: return 32;
    default:                                      return 0;
    }
}

SDL_GPUTextureFormat supported_depth_format(const gpu_device& dev,
                                            const SDL_GPUTextureFormat* candidates, int count)
{
    if (!dev.valid() || candidates == nullptr) { return SDL_GPU_TEXTUREFORMAT_INVALID; }

    for (int i = 0; i < count; ++i)
    {
        if (SDL_GPUTextureSupportsFormat(dev.handle(), candidates[i],
                                         SDL_GPU_TEXTURETYPE_2D,
                                         SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
        {
            return candidates[i];
        }
    }
    return SDL_GPU_TEXTUREFORMAT_INVALID;
}

// ===========================================================================
// gpu_texture
// ===========================================================================

gpu_texture::~gpu_texture()
{
    destroy();
}

gpu_texture::gpu_texture(gpu_texture&& other) noexcept
    : device_(other.device_), texture_(other.texture_), width_(other.width_),
      height_(other.height_), uploaded_bytes_(other.uploaded_bytes_), format_(other.format_)
{
    other.device_ = nullptr;
    other.texture_ = nullptr;
    other.width_ = 0;
    other.height_ = 0;
    other.uploaded_bytes_ = 0;
    other.format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
}

gpu_texture& gpu_texture::operator=(gpu_texture&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        device_ = other.device_;
        texture_ = other.texture_;
        width_ = other.width_;
        height_ = other.height_;
        uploaded_bytes_ = other.uploaded_bytes_;
        format_ = other.format_;
        other.device_ = nullptr;
        other.texture_ = nullptr;
        other.width_ = 0;
        other.height_ = 0;
        other.uploaded_bytes_ = 0;
        other.format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    }
    return *this;
}

bool gpu_texture::create_sampled(const gpu_device& dev, SDL_GPUCommandBuffer* cb,
                                 const image_data& src, bool srgb, const char* name)
{
    destroy();

    if (!dev.valid() || cb == nullptr || !src.valid()) { return false; }

    device_ = dev.handle();
    width_ = static_cast<Uint32>(src.width);
    height_ = static_cast<Uint32>(src.height);

    // THE FORMAT IS WHERE THE COLOUR SPACE DECISION LIVES, and it is one enum
    // apart. `_SRGB` means the sampler decodes each texel to linear on the way
    // out — before the filter, which is the ordering Lesson 3.9 had to construct
    // by hand and measured the cost of getting wrong.
    format_ = srgb ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                   : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = format_;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = width_;
    ti.height = height_;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;              // no mipmaps yet — Module 6
    ti.sample_count = SDL_GPU_SAMPLECOUNT_1;

    texture_ = SDL_CreateGPUTexture(device_, &ti);
    if (texture_ == nullptr)
    {
        SDL_Log("SDL_CreateGPUTexture(%ux%u %s) failed: %s",
                width_, height_, name_of(format_), SDL_GetError());
        destroy();
        return false;
    }
    if (name != nullptr) { SDL_SetGPUTextureName(device_, texture_, name); }

    // ---- The transfer path, unchanged from Lesson 4.2 -----------------------
    const Uint32 bytes = width_ * height_ * 4u;

    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = bytes;

    SDL_GPUTransferBuffer* staging = SDL_CreateGPUTransferBuffer(device_, &tb);
    if (staging == nullptr)
    {
        SDL_Log("SDL_CreateGPUTransferBuffer(%u) failed: %s", bytes, SDL_GetError());
        destroy();
        return false;
    }

    void* mapped = SDL_MapGPUTransferBuffer(device_, staging, false);
    if (mapped == nullptr)
    {
        SDL_Log("SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device_, staging);
        destroy();
        return false;
    }
    std::memcpy(mapped, src.pixels.data(), bytes);
    SDL_UnmapGPUTransferBuffer(device_, staging);

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);

    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = staging;
    source.offset = 0;
    // PIXELS AND ROWS, not bytes. Lesson 4.2 recorded this one after being bitten
    // by it: handing `width * 4` to pixels_per_row shears the image, exactly as a
    // wrong pitch did in Lesson 1.5 and again in Lesson 4.5.
    source.pixels_per_row = width_;
    source.rows_per_layer = height_;

    SDL_GPUTextureRegion dest{};
    dest.texture = texture_;
    dest.w = width_;
    dest.h = height_;
    dest.d = 1;

    // cycle = false: this texture is written once, at load, and nothing is
    // reading it yet — the same argument `gpu_buffer::upload` makes for geometry.
    SDL_UploadToGPUTexture(copy, &source, &dest, false);

    SDL_EndGPUCopyPass(copy);
    SDL_ReleaseGPUTransferBuffer(device_, staging);

    uploaded_bytes_ = bytes;
    return true;
}

bool gpu_texture::create_depth(const gpu_device& dev, SDL_GPUTextureFormat format,
                               Uint32 width, Uint32 height, const char* name)
{
    destroy();

    if (!dev.valid() || width == 0 || height == 0
        || format == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        return false;
    }

    device_ = dev.handle();
    width_ = width;
    height_ = height;
    format_ = format;

    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = format_;
    // DEPTH_STENCIL_TARGET and nothing else. Adding SAMPLER here would let a
    // later pass read the depth buffer — which Module 6's shadow maps need and
    // this lesson does not, and asking for a usage you do not need can force the
    // driver into a slower layout.
    ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    ti.width = width_;
    ti.height = height_;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    ti.sample_count = SDL_GPU_SAMPLECOUNT_1;

    texture_ = SDL_CreateGPUTexture(device_, &ti);
    if (texture_ == nullptr)
    {
        SDL_Log("SDL_CreateGPUTexture(depth %ux%u %s) failed: %s",
                width_, height_, name_of(format_), SDL_GetError());
        destroy();
        return false;
    }
    if (name != nullptr) { SDL_SetGPUTextureName(device_, texture_, name); }

    // Nothing is uploaded. A depth buffer is written by the render pass that
    // clears it and by every fragment that passes the test — never by us.
    uploaded_bytes_ = 0;
    return true;
}

void gpu_texture::destroy()
{
    if (device_ != nullptr && texture_ != nullptr)
    {
        SDL_ReleaseGPUTexture(device_, texture_);
    }
    device_ = nullptr;
    texture_ = nullptr;
    width_ = 0;
    height_ = 0;
    uploaded_bytes_ = 0;
    format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
}

// ===========================================================================
// gpu_sampler
// ===========================================================================

gpu_sampler::~gpu_sampler()
{
    destroy();
}

gpu_sampler::gpu_sampler(gpu_sampler&& other) noexcept
    : device_(other.device_), sampler_(other.sampler_)
{
    other.device_ = nullptr;
    other.sampler_ = nullptr;
}

gpu_sampler& gpu_sampler::operator=(gpu_sampler&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        device_ = other.device_;
        sampler_ = other.sampler_;
        other.device_ = nullptr;
        other.sampler_ = nullptr;
    }
    return *this;
}

bool gpu_sampler::create(const gpu_device& dev, filter min_mag, address_mode wrap,
                         const char* name)
{
    destroy();

    if (!dev.valid()) { return false; }
    device_ = dev.handle();

    SDL_GPUSamplerCreateInfo si{};

    // THE CAST IS THE PORT. Lesson 3.9 defined these enums to match SDL's
    // enumerator for enumerator, and verify_42 §G has checked it on every run
    // since — so this is a rename with a regression test behind it rather than a
    // coincidence being relied on.
    const SDL_GPUFilter f = static_cast<SDL_GPUFilter>(min_mag);
    const SDL_GPUSamplerAddressMode a = static_cast<SDL_GPUSamplerAddressMode>(wrap);

    // MINIFICATION AND MAGNIFICATION ARE SEPARATE FIELDS, which the CPU sampler
    // had no equivalent of because it never minified — Lesson 3.9's floor demo
    // showed the shimmer that results and named mipmaps as the answer. We set
    // both the same; Module 6 sets them differently and adds the mipmap chain
    // that makes minification mean anything.
    si.min_filter = f;
    si.mag_filter = f;
    si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;

    // THREE ADDRESS MODES FOR THREE AXES. Lesson 3.9's sampler had one, because a
    // 2-D image has two axes and we wrapped both the same way. Real uses differ:
    // a terrain splat clamps v and repeats u. `w` is for 3-D textures and is set
    // for completeness.
    si.address_mode_u = a;
    si.address_mode_v = a;
    si.address_mode_w = a;

    si.mip_lod_bias = 0.0f;
    si.min_lod = 0.0f;
    si.max_lod = 0.0f;

    si.enable_anisotropy = false;
    si.max_anisotropy = 1.0f;

    // enable_compare is for SHADOW sampling — the sampler performs the depth
    // comparison itself and returns a filtered 0..1 occlusion instead of a depth.
    // That is Module 6's percentage-closer filtering, and it is worth knowing it
    // lives here rather than in the shader.
    si.enable_compare = false;
    si.compare_op = SDL_GPU_COMPAREOP_NEVER;

    sampler_ = SDL_CreateGPUSampler(device_, &si);
    if (sampler_ == nullptr)
    {
        SDL_Log("SDL_CreateGPUSampler failed: %s", SDL_GetError());
        destroy();
        return false;
    }

    (void)name;   // SDL exposes no sampler-naming call; RenderDoc infers it
    return true;
}

void gpu_sampler::destroy()
{
    if (device_ != nullptr && sampler_ != nullptr)
    {
        SDL_ReleaseGPUSampler(device_, sampler_);
    }
    device_ = nullptr;
    sampler_ = nullptr;
}

} // namespace engine
