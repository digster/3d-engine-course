// src/gfx/gpu_texture.hpp — an image on the device, and the object that reads it.
//
// Lesson 4.7. Two types, and the interesting one is the second.
//
// `gpu_texture` is the storage. It arrives by the transfer path Lesson 4.2 built
// for the framebuffer, unchanged in shape: CPU memory -> transfer buffer ->
// texture, because texture memory is TILED and possibly swizzled in a layout only
// the driver knows, so there is no pointer to it and there cannot be.
//
// `gpu_sampler` is the part that is genuinely new, and it is worth being precise
// about why. In Lesson 3.9 the filter and the address mode were ARGUMENTS:
//
//     sample(image, uv, filter::linear, address_mode::repeat)
//
// On the GPU they are an OBJECT, created once and bound alongside the texture.
// That is Lesson 4.1's argument arriving in a third place — after the pipeline
// (4.4) and the vertex layout (4.5) — and for the same reason: a decision the
// hardware must specialise for cannot be a parameter of the innermost loop.
//
// A texture and a sampler are SEPARATE objects bound as a PAIR
// (`SDL_GPUTextureSamplerBinding`), which is not how either OpenGL or the CPU
// path modelled it, and is better: one image can be read three ways in one frame
// by binding it with three samplers, and one sampler serves every texture in a
// material system without being duplicated per image.
//
// A DEPTH TARGET IS ALSO A TEXTURE — same SDL type, different usage and format,
// never uploaded and (usually) never sampled. `create_depth` below is that path,
// and it lives here rather than in its own file because SDL models it as one
// type and pretending otherwise would be inventing a distinction to explain.

#pragma once

#include "gfx/gpu_device.hpp"
#include "gfx/image.hpp"
#include "gfx/texture.hpp"   // engine::filter, engine::address_mode — Lesson 3.9

#include <SDL3/SDL.h>

namespace engine {

/// Owns an `SDL_GPUTexture`. Move-only, like every device resource here.
class gpu_texture
{
public:
    gpu_texture() = default;
    ~gpu_texture();

    gpu_texture(const gpu_texture&) = delete;
    gpu_texture& operator=(const gpu_texture&) = delete;

    gpu_texture(gpu_texture&& other) noexcept;
    gpu_texture& operator=(gpu_texture&& other) noexcept;

    /// Create a sampled 2-D texture and record the upload of `src` into `cb`.
    ///
    /// @param srgb ask for an `_SRGB` format, so the SAMPLER decodes to linear on
    ///             every read, for free. This is the single most important
    ///             argument in the file and Lesson 3.9 is why: an albedo is a
    ///             reflectance, a reflectance multiplies a quantity of light, and
    ///             both sides of that multiply must be linear. Lesson 3.9 paid
    ///             for the decode per texel in software and measured what
    ///             skipping it costs — blending two texels in encoded space gives
    ///             0.2139 where 0.5 is correct, 43% of the light. Here it is a
    ///             flag, and it happens BEFORE the filter, which is the order
    ///             software cannot easily achieve.
    [[nodiscard]] bool create_sampled(const gpu_device& dev, SDL_GPUCommandBuffer* cb,
                                      const image_data& src, bool srgb,
                                      const char* name = nullptr);

    /// Create a depth attachment: no upload, no sampling, cleared by the render
    /// pass that uses it.
    ///
    /// `format` must be one the device supports for this usage — ask with
    /// `supported_depth_format` below rather than assuming, because only
    /// `D16_UNORM` is guaranteed.
    [[nodiscard]] bool create_depth(const gpu_device& dev, SDL_GPUTextureFormat format,
                                    Uint32 width, Uint32 height, const char* name = nullptr);

    void destroy();

    [[nodiscard]] bool valid() const { return texture_ != nullptr; }
    [[nodiscard]] SDL_GPUTexture* handle() const { return texture_; }
    [[nodiscard]] Uint32 width() const { return width_; }
    [[nodiscard]] Uint32 height() const { return height_; }
    [[nodiscard]] SDL_GPUTextureFormat format() const { return format_; }

    /// Bytes the upload moved. Zero for a depth target, which is never written
    /// from the CPU at all.
    [[nodiscard]] Uint32 uploaded_bytes() const { return uploaded_bytes_; }

private:
    SDL_GPUDevice* device_ = nullptr;    ///< NOT owned
    SDL_GPUTexture* texture_ = nullptr;  ///< owning by contract
    Uint32 width_ = 0;
    Uint32 height_ = 0;
    Uint32 uploaded_bytes_ = 0;
    SDL_GPUTextureFormat format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
};

/// Owns an `SDL_GPUSampler` — the *how* of a texture read, frozen into an object.
class gpu_sampler
{
public:
    gpu_sampler() = default;
    ~gpu_sampler();

    gpu_sampler(const gpu_sampler&) = delete;
    gpu_sampler& operator=(const gpu_sampler&) = delete;

    gpu_sampler(gpu_sampler&& other) noexcept;
    gpu_sampler& operator=(gpu_sampler&& other) noexcept;

    /// Create one from Lesson 3.9's own enums.
    ///
    /// The parameters are `engine::filter` and `engine::address_mode`, not SDL's,
    /// and the conversion inside is a `static_cast`. Lesson 3.9 wrote those enums
    /// to match `SDL_GPUFilter` and `SDL_GPUSamplerAddressMode` enumerator for
    /// enumerator specifically so that this moment would be a rename, and
    /// `verify_42` §G has asserted the correspondence on every run since — so if
    /// SDL ever inserts an enumerator, that test fails rather than this cast
    /// silently selecting the wrong mode.
    [[nodiscard]] bool create(const gpu_device& dev,
                              filter min_mag = filter::linear,
                              address_mode wrap = address_mode::repeat,
                              const char* name = nullptr);

    void destroy();

    [[nodiscard]] bool valid() const { return sampler_ != nullptr; }
    [[nodiscard]] SDL_GPUSampler* handle() const { return sampler_; }

private:
    SDL_GPUDevice* device_ = nullptr;      ///< NOT owned
    SDL_GPUSampler* sampler_ = nullptr;    ///< owning by contract
};

/// The first of `candidates` this device accepts as a depth target, or
/// `INVALID` if it accepts none.
///
/// **Ask, never assume.** SDL guarantees exactly one depth format —
/// `D16_UNORM` — and everything else is a query. Lesson 4.2 established the habit
/// with the swapchain format and the present modes; this is the same discipline
/// applied to the one piece of texture state that has a precision consequence.
[[nodiscard]] SDL_GPUTextureFormat supported_depth_format(const gpu_device& dev,
                                                          const SDL_GPUTextureFormat* candidates,
                                                          int count);

// `name_of(SDL_GPUTextureFormat)` lives in gpu_device.hpp, where Lesson 4.2 put
// it to log the swapchain format; Lesson 4.7 added the depth rows to the same
// table rather than starting a second one.

/// Bits of depth precision `format` stores, or 0 if it is not a depth format.
///
/// For the `_UNORM` formats this is exact — evenly spaced codes across [0, 1].
/// For `D32_FLOAT` it is a convenient lie: a float has 24 bits of mantissa but
/// its precision is not uniform across the range, which turns out to matter in
/// exactly the direction that helps. Lesson 4.7 §3.2 measures what each format
/// can actually separate.
[[nodiscard]] int depth_bits(SDL_GPUTextureFormat format);

} // namespace engine
