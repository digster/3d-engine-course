// engine/src/gfx/gpu_texture.cpp — the upload path Lesson 4.2 built, aimed at an asset.

#include <engine/gfx/gpu_texture.hpp>

#include <engine/gfx/cubemap.hpp>   // Lesson 6.15: the full type the header only names

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
                                 const image_data& src, bool srgb, const char* name,
                                 bool mips)
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
    // LESSON 6.10. COLOR_TARGET is not decoration: SDL generates a chain by
    // BLITTING each level into the next, so every level must be a render target.
    // `SDL_gpu.c` asserts on SAMPLER|COLOR_TARGET — but only when the device is
    // in debug mode, so on a release device the requirement is unchecked and the
    // result is undefined rather than diagnosed.
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
             | (mips ? SDL_GPU_TEXTUREUSAGE_COLOR_TARGET : 0u);
    ti.width = width_;
    ti.height = height_;
    ti.layer_count_or_depth = 1;
    // 1 + floor(log2(max side)): the number of halvings before a side reaches 1.
    // 4.7 wrote `= 1` here with a comment saying Module 6 would change it.
    levels_ = 1;
    if (mips)
    {
        Uint32 side = (width_ > height_) ? width_ : height_;
        while (side > 1u) { side >>= 1; ++levels_; }
    }
    ti.num_levels = static_cast<Uint32>(levels_);
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

    // ---- LESSON 6.10: the chain ---------------------------------------------
    //
    // AFTER the copy pass ends, and that is a requirement rather than tidiness:
    // "This function must not be called inside of any pass", says the header,
    // and `SDL_gpu.c` asserts it in debug mode. It reads level 0 — which the
    // upload above just wrote — and blits its way down.
    //
    // Note what we do NOT do: average in linear light ourselves. The texture was
    // created with an _SRGB format when `srgb` is set, so the hardware decodes
    // on read and encodes on write, and the blit chain averages in linear light
    // for free. That is the same correctness the CPU path has to arrange by hand
    // in `build_mips`, and it is the clearest case in the course of a format
    // flag doing real work.
    if (mips && levels_ > 1)
    {
        SDL_GenerateMipmapsForGPUTexture(cb, texture_);
    }

    uploaded_bytes_ = bytes;
    return true;
}

bool gpu_texture::create_colour_target(const gpu_device& dev, SDL_GPUTextureFormat format,
                                       Uint32 width, Uint32 height, const char* name,
                                       bool sampled, SDL_GPUSampleCount samples)
{
    destroy();

    if (!dev.valid() || width == 0 || height == 0
        || format == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        return false;
    }

    // ---- Lesson 6.14: MSAA SUPPORT IS PER FORMAT, AND MUST BE ASKED ----------
    //
    // `SDL_GPUTextureSupportsSampleCount` exists because this genuinely varies:
    // 4x on an 8-bit swapchain format is near universal, 4x on
    // R16G16B16A16_FLOAT is common but NOT guaranteed, and 8x is a coin toss.
    // Asking costs one call at startup; not asking costs a texture that fails to
    // create with an error the caller usually attributes to something else.
    //
    // FALLING BACK RATHER THAN FAILING is the right behaviour for a quality
    // setting: a program that refuses to start because it cannot have 8x MSAA is
    // worse than one that quietly runs at 4x and says so.
    if (samples != SDL_GPU_SAMPLECOUNT_1
        && !SDL_GPUTextureSupportsSampleCount(dev.handle(), format, samples))
    {
        ENGINE_LOG_WARN(engine::log_gpu,
                        "gpu_texture: %s does not support this sample count; falling back to 1x",
                        name_of(format));
        samples = SDL_GPU_SAMPLECOUNT_1;
    }

    // A MULTISAMPLE TEXTURE CANNOT BE SAMPLED. It is resolved into a
    // single-sample texture first, and that one is what a later pass reads —
    // which is the whole shape of an MSAA frame and is why `gpu_post_stack` now
    // owns two colour targets where it owned one. Asking for SAMPLER here would
    // request a usage the driver cannot provide for this texture.
    if (samples != SDL_GPU_SAMPLECOUNT_1) { sampled = false; }
    samples_ = samples;

    device_ = dev.handle();
    width_ = width;
    height_ = height;
    format_ = format;

    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = format_;
    // BOTH USAGES. See the header: a scene buffer is written by one pass and read
    // by the next, and dropping SAMPLER gives a texture that renders correctly
    // and reads as undefined — with no error on a release device.
    ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
             | (sampled ? SDL_GPU_TEXTUREUSAGE_SAMPLER : 0u);
    ti.width = width_;
    ti.height = height_;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    ti.sample_count = samples_;

    texture_ = create_named_texture(device_, ti, name);
    if (texture_ == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu,
                         "SDL_CreateGPUTexture(colour %ux%u %s) failed: %s",
                         width_, height_, name_of(format_), SDL_GetError());
        destroy();
        return false;
    }

    // Nothing is uploaded: a render target's contents come from a render pass.
    uploaded_bytes_ = 0;
    return true;
}

bool gpu_texture::create_depth(const gpu_device& dev, SDL_GPUTextureFormat format,
                               Uint32 width, Uint32 height, const char* name,
                               bool sampled, SDL_GPUSampleCount samples)
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
    // MUST MATCH THE COLOUR TARGET (6.14): every attachment in a pass is
    // rasterized at the same sample positions, so a 4x colour target beside a
    // 1x depth target is a pass that cannot be begun.
    if (samples != SDL_GPU_SAMPLECOUNT_1) { sampled = false; }
    samples_ = samples;
    ti.sample_count = samples_;

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

bool gpu_texture::create_cube(const gpu_device& dev, SDL_GPUCommandBuffer* cb,
                              const cube_map& src, SDL_GPUTextureFormat format,
                              const char* name)
{
    destroy();

    if (!dev.valid() || cb == nullptr || !src.valid()
        || format == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        return false;
    }

    // ---- ASK, DO NOT ASSUME -------------------------------------------------
    //
    // The same discipline 4.2 applied to the swapchain format, 4.7 to depth and
    // 6.14 to sample counts, arriving at the fourth place it is needed. Note
    // that the query names the TYPE as well as the usage: a device can support a
    // format for a 2-D texture and refuse it for a cube, and asking the 2-D
    // question would be asking a question that is not ours.
    if (!SDL_GPUTextureSupportsFormat(dev.handle(), format,
                                      SDL_GPU_TEXTURETYPE_CUBE,
                                      SDL_GPU_TEXTUREUSAGE_SAMPLER))
    {
        ENGINE_LOG_ERROR(engine::log_gpu,
                         "gpu_texture: %s is not supported as a sampled cube map",
                         name_of(format));
        return false;
    }

    // Two formats, and the difference is only how many bytes a channel takes.
    // Anything else is refused here rather than producing a texture whose
    // contents are a reinterpretation of the wrong number of bytes — the failure
    // mode 4.2 called a sheared image, arriving in the depth axis.
    int channel_bytes;
    if (format == SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT)      { channel_bytes = 2; }
    else if (format == SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT) { channel_bytes = 4; }
    else
    {
        ENGINE_LOG_ERROR(engine::log_gpu,
                         "gpu_texture: create_cube needs a 16- or 32-bit float format, not %s",
                         name_of(format));
        return false;
    }

    device_ = dev.handle();
    width_ = static_cast<Uint32>(src.size());
    height_ = width_;
    format_ = format;
    levels_ = src.levels();
    samples_ = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTextureCreateInfo ti{};
    // THE ONE FIELD THAT MAKES IT A CUBE, and the shader has to agree: a
    // `Texture2D` bound to a cube is a binding error the validation layer
    // catches and a black reflection on drivers that do not.
    ti.type = SDL_GPU_TEXTURETYPE_CUBE;
    ti.format = format_;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = width_;
    ti.height = height_;
    // SIX, ALWAYS. `layer_count_or_depth` is the same field 6.9 used for cascade
    // slices; for a cube SDL requires exactly six and rejects anything else.
    ti.layer_count_or_depth = 6;
    ti.num_levels = static_cast<Uint32>(levels_);
    ti.sample_count = SDL_GPU_SAMPLECOUNT_1;

    texture_ = create_named_texture(device_, ti, name);
    if (texture_ == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu,
                         "SDL_CreateGPUTexture(cube %ux%u x6 x%d %s) failed: %s",
                         width_, height_, levels_, name_of(format_), SDL_GetError());
        destroy();
        return false;
    }

    // ---- ONE STAGING BUFFER, SIZED FOR THE LARGEST FACE ---------------------
    //
    // Not one per face and not one for the whole chain. Per face would be 6 x
    // levels allocations for a job that reuses the same bytes; the whole chain at
    // once would need SDL's per-level alignment rules to be reproduced by hand,
    // and `SDL_UploadToGPUTexture` is perfectly happy to be called repeatedly
    // against one buffer inside a single copy pass. The largest face is level 0,
    // so that is the size.
    const Uint32 max_bytes = width_ * height_ * 4u * static_cast<Uint32>(channel_bytes);

    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = max_bytes;

    SDL_GPUTransferBuffer* staging =
        create_named_transfer_buffer(device_, tb, "staging (cube map upload)");
    if (staging == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu, "SDL_CreateGPUTransferBuffer(%u) failed: %s",
                         max_bytes, SDL_GetError());
        destroy();
        return false;
    }

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);
    Uint32 total = 0;

    for (int level = 0; level < levels_; ++level)
    {
        const int n = src.size_at(level);
        const Uint32 bytes = static_cast<Uint32>(n) * static_cast<Uint32>(n)
                           * 4u * static_cast<Uint32>(channel_bytes);

        for (int f = 0; f < k_cube_faces; ++f)
        {
            void* mapped = SDL_MapGPUTransferBuffer(device_, staging, false);
            if (mapped == nullptr)
            {
                ENGINE_LOG_ERROR(engine::log_gpu, "SDL_MapGPUTransferBuffer failed: %s",
                                 SDL_GetError());
                SDL_EndGPUCopyPass(copy);
                SDL_ReleaseGPUTransferBuffer(device_, staging);
                destroy();
                return false;
            }

            const hdr_buffer& img = src.face(static_cast<cube_face>(f), level);
            if (channel_bytes == 2)
            {
                auto* out = static_cast<Uint16*>(mapped);
                for (int y = 0; y < n; ++y)
                {
                    const linear_rgb* row = img.row(y);
                    for (int x = 0; x < n; ++x)
                    {
                        *out++ = float_to_half(row[x].r);
                        *out++ = float_to_half(row[x].g);
                        *out++ = float_to_half(row[x].b);
                        // ALPHA 1, NOT 0. Nothing reads it — a cube map holds
                        // radiance and radiance has no coverage — but an
                        // uninitialised or zero alpha is the kind of thing a
                        // later blend state picks up silently, and one is the
                        // answer that survives being read by accident.
                        *out++ = float_to_half(1.0f);
                    }
                }
            }
            else
            {
                auto* out = static_cast<float*>(mapped);
                for (int y = 0; y < n; ++y)
                {
                    const linear_rgb* row = img.row(y);
                    for (int x = 0; x < n; ++x)
                    {
                        *out++ = row[x].r;
                        *out++ = row[x].g;
                        *out++ = row[x].b;
                        *out++ = 1.0f;
                    }
                }
            }
            SDL_UnmapGPUTransferBuffer(device_, staging);

            SDL_GPUTextureTransferInfo source{};
            source.transfer_buffer = staging;
            source.offset = 0;
            source.pixels_per_row = static_cast<Uint32>(n);
            source.rows_per_layer = static_cast<Uint32>(n);

            SDL_GPUTextureRegion dest{};
            dest.texture = texture_;
            dest.mip_level = static_cast<Uint32>(level);
            // THE FACE, AS A LAYER. `SDL_GPUCubeMapFace` is documented as
            // "can be passed in as the layer field", and `engine::cube_face`
            // matches its ordering, so this cast is a rename.
            dest.layer = static_cast<Uint32>(f);
            dest.w = static_cast<Uint32>(n);
            dest.h = static_cast<Uint32>(n);
            dest.d = 1;

            // `cycle = false`: nothing has read this texture yet, so there is no
            // hazard to break — the same argument `create_sampled` makes, and it
            // holds even though we are writing the same staging buffer 6 x levels
            // times, because each upload is ordered after the map that filled it
            // within one copy pass.
            SDL_UploadToGPUTexture(copy, &source, &dest, false);
            total += bytes;
        }
    }

    SDL_EndGPUCopyPass(copy);
    SDL_ReleaseGPUTransferBuffer(device_, staging);

    uploaded_bytes_ = total;
    return true;
}

bool gpu_texture::create_depth_array(const gpu_device& dev, SDL_GPUTextureFormat format,
                                     Uint32 width, Uint32 height, Uint32 layers,
                                     const char* name, bool sampled)
{
    destroy();

    if (!dev.valid() || width == 0 || height == 0 || layers == 0
        || format == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        return false;
    }

    device_ = dev.handle();
    width_ = width;
    height_ = height;
    format_ = format;

    SDL_GPUTextureCreateInfo ti{};
    // THE ONE FIELD THAT MAKES IT AN ARRAY, and the one the shader has to agree
    // with. `Texture2D` bound to a 2D_ARRAY is a binding error — caught by the
    // validation layer, and a silently black shadow map without it.
    ti.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
    ti.format = format_;
    ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
             | (sampled ? SDL_GPU_TEXTUREUSAGE_SAMPLER : 0u);
    ti.width = width_;
    ti.height = height_;
    // `layer_count_or_depth` is one field carrying two meanings: the LAYER COUNT
    // for a 2D array, the DEPTH for a 3D texture. The name is SDL being honest
    // about a union it did not make one.
    ti.layer_count_or_depth = layers;
    ti.num_levels = 1;
    ti.sample_count = SDL_GPU_SAMPLECOUNT_1;

    texture_ = create_named_texture(device_, ti, name);
    if (texture_ == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_gpu,
                "SDL_CreateGPUTexture(depth array %ux%ux%u %s) failed: %s",
                width_, height_, layers, name_of(format_), SDL_GetError());
        destroy();
        return false;
    }

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
                         const char* name, filter mip, int max_aniso)
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
    // LESSON 6.10. `linear` here is the "tri" in trilinear: it blends the two
    // straddling levels instead of snapping to the nearer one.
    si.mipmap_mode = (mip == filter::linear) ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
                                             : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;

    // THREE ADDRESS MODES FOR THREE AXES. Lesson 3.9's sampler had one, because a
    // 2-D image has two axes and we wrapped both the same way. Real uses differ:
    // a terrain splat clamps v and repeats u. `w` is for 3-D textures and is set
    // for completeness.
    si.address_mode_u = a;
    si.address_mode_v = a;
    si.address_mode_w = a;

    si.mip_lod_bias = 0.0f;
    si.min_lod = 0.0f;

    // **max_lod = 0 CLAMPS THE WHOLE CHAIN AWAY**, and this line was 0.0f until
    // Lesson 6.10. It did not matter while every texture had one level; the
    // moment one has nine it is the difference between mipmapping and an
    // expensive no-op — you build the chain, set mipmap_mode, see no change, and
    // go looking in the generator. There is no error and no warning, because
    // clamping to level 0 is a perfectly legal thing to ask for.
    si.max_lod = 1000.0f;   // SDL's own "no clamp" idiom; any level the texture has

    // BOTH FIELDS, OR NEITHER. SDL ignores `max_anisotropy` unless
    // `enable_anisotropy` is true, so setting the clamp alone is a silent no-op —
    // the same shape of trap as max_lod, one struct field along.

    si.enable_anisotropy = (max_aniso > 1);
    si.max_anisotropy = static_cast<float>((max_aniso < 1) ? 1 : (max_aniso > 16 ? 16 : max_aniso));

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
    // A shadow map has ONE level (6.8 fits a box; 6.9 adds layers, not levels),
    // so there is nothing between levels to filter. Lesson 6.10 left this alone
    // deliberately rather than making it a parameter nobody could use.
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

    si.max_lod = 0.0f;   // one level, so clamping to it is the honest value
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
