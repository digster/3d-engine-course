// engine/src/gfx/gpu_texture.cpp — the upload path Lesson 4.2 built, aimed at an asset.

#include <engine/gfx/gpu_texture.hpp>

#include <engine/core/log.hpp>
#include <engine/gfx/gpu_debug.hpp>   // Lesson 4.9: named at CREATION, as SDL asks

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

SDL_GPUTextureFormat supported_shadow_format(const gpu_device& dev,
                                             const SDL_GPUTextureFormat* candidates, int count)
{
    if (!dev.valid() || candidates == nullptr) { return SDL_GPU_TEXTUREFORMAT_INVALID; }

    for (int i = 0; i < count; ++i)
    {
        // BOTH USAGES IN ONE QUERY, because that is the texture we are actually
        // going to ask for. Querying them separately and taking the intersection
        // would be asking two questions neither of which is ours: a device may
        // support a format as an attachment and as a texture and still refuse
        // the combination, and the combination is the whole point of a shadow
        // map.
        if (SDL_GPUTextureSupportsFormat(dev.handle(), candidates[i],
                                         SDL_GPU_TEXTURETYPE_2D,
                                         SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
                                         | SDL_GPU_TEXTUREUSAGE_SAMPLER))
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

    texture_ = create_named_texture(device_, ti, name);
    if (texture_ == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "SDL_CreateGPUTexture(%ux%u %s) failed: %s",
                width_, height_, name_of(format_), SDL_GetError());
        destroy();
        return false;
    }

    // ---- The transfer path, unchanged from Lesson 4.2 -----------------------
    const Uint32 bytes = width_ * height_ * 4u;

    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = bytes;

    SDL_GPUTransferBuffer* staging =
        create_named_transfer_buffer(device_, tb, "staging (texture upload)");
    if (staging == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "SDL_CreateGPUTransferBuffer(%u) failed: %s", bytes, SDL_GetError());
        destroy();
        return false;
    }

    void* mapped = SDL_MapGPUTransferBuffer(device_, staging, false);
    if (mapped == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
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
                               Uint32 width, Uint32 height, const char* name,
                               bool sampled)
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
    // DEPTH_STENCIL_TARGET, and SAMPLER as well when the caller asks — Lesson
    // 6.8. That second flag is the difference between a depth buffer and a
    // SHADOW MAP: one is consumed by the pass that writes it, the other is read
    // back by a pass that comes after. The flag is not free (gpu_texture.hpp
    // says why), which is exactly why it is a parameter and not a default.
    ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
             | (sampled ? SDL_GPU_TEXTUREUSAGE_SAMPLER : 0u);
    ti.width = width_;
    ti.height = height_;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    ti.sample_count = SDL_GPU_SAMPLECOUNT_1;

    texture_ = create_named_texture(device_, ti, name);
    if (texture_ == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "SDL_CreateGPUTexture(depth %ux%u %s) failed: %s",
                width_, height_, name_of(format_), SDL_GetError());
        destroy();
        return false;
    }

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
    // That is Lesson 6.8's percentage-closer filtering, and `create_comparison`
    // below is where it is switched on. An ordinary sampler must leave it off:
    // a comparison sampler bound to a colour texture is a validation error on
    // every backend.
    si.enable_compare = false;
    si.compare_op = SDL_GPU_COMPAREOP_NEVER;

    sampler_ = SDL_CreateGPUSampler(device_, &si);
    if (sampler_ == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "SDL_CreateGPUSampler failed: %s", SDL_GetError());
        destroy();
        return false;
    }

    (void)name;   // SDL exposes no sampler-naming call; RenderDoc infers it
    return true;
}

bool gpu_sampler::create_comparison(const gpu_device& dev, const char* name)
{
    destroy();

    if (!dev.valid()) { return false; }
    device_ = dev.handle();

    SDL_GPUSamplerCreateInfo si{};

    // LINEAR, and it is the entire reason this object exists. The filter runs
    // on the COMPARISON RESULTS, not on the depths — four booleans blended into
    // a 0..1 coverage — which is hardware 2x2 PCF for the price of one fetch.
    si.min_filter = SDL_GPU_FILTER_LINEAR;
    si.mag_filter = SDL_GPU_FILTER_LINEAR;
    si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;

    // CLAMP, never repeat. A lookup that falls outside the light's box must read
    // the edge and not wrap round to the opposite side of the scene, which would
    // paint a shadow of one corner onto the other. Clamping gives the edge texel;
    // §5.4 explains why the shader ALSO range-checks rather than relying on it.
    si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    si.mip_lod_bias = 0.0f;
    si.min_lod = 0.0f;
    si.max_lod = 0.0f;
    si.enable_anisotropy = false;
    si.max_anisotropy = 1.0f;

    // THE TWO FIELDS. `LESS_OR_EQUAL` because our device depth runs 0 at the near
    // plane and 1 at the far one (conventions §4), so "the fragment is at or in
    // front of what the light recorded" means lit. Flip the depth convention and
    // this must flip with it — which is why it is written here beside the reason
    // rather than chosen once and forgotten.
    si.enable_compare = true;
    si.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    sampler_ = SDL_CreateGPUSampler(device_, &si);
    if (sampler_ == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "SDL_CreateGPUSampler(comparison) failed: %s",
                         SDL_GetError());
        destroy();
        return false;
    }

    (void)name;
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
