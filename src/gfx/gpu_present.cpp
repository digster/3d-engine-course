// src/gfx/gpu_present.cpp — the three hops from a CPU pixel to the display.

#include "gfx/gpu_present.hpp"

#include <cstring>
#include <utility>

namespace engine {

blit_rect fit_centred(Uint32 src_w, Uint32 src_h, Uint32 dst_w, Uint32 dst_h)
{
    blit_rect out;
    if (src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) { return out; }

    // Compare aspect ratios by cross-multiplying, which keeps this in integers
    // and avoids a divide-by-zero the guard above has already ruled out anyway.
    // src_w/src_h > dst_w/dst_h  <=>  src_w*dst_h > dst_w*src_h.
    const Uint64 src_wide = static_cast<Uint64>(src_w) * dst_h;
    const Uint64 dst_wide = static_cast<Uint64>(dst_w) * src_h;

    if (src_wide > dst_wide)
    {
        // Source is proportionally wider: fill the width, bars top and bottom.
        out.w = dst_w;
        out.h = static_cast<Uint32>(static_cast<Uint64>(dst_w) * src_h / src_w);
    }
    else
    {
        out.h = dst_h;
        out.w = static_cast<Uint32>(static_cast<Uint64>(dst_h) * src_w / src_h);
    }

    out.x = (dst_w - out.w) / 2;
    out.y = (dst_h - out.h) / 2;
    return out;
}

/// Is this format one that decodes on read and encodes on write?
///
/// A free function in the .cpp rather than a method, because nothing outside
/// this file needs to ask, and the list is short enough to be honest about:
/// these are the two 8-bit sRGB formats a swapchain can arrive as.
namespace {

bool is_srgb_format(SDL_GPUTextureFormat f)
{
    return f == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB
        || f == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
}

} // namespace

gpu_present_target::~gpu_present_target()
{
    destroy();
}

gpu_present_target::gpu_present_target(gpu_present_target&& other) noexcept
    : device_(other.device_), image_(other.image_), staging_(other.staging_),
      format_(other.format_), width_(other.width_), height_(other.height_),
      last_upload_bytes_(other.last_upload_bytes_)
{
    other.device_ = nullptr;
    other.image_ = nullptr;
    other.staging_ = nullptr;
    other.format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    other.width_ = 0;
    other.height_ = 0;
    other.last_upload_bytes_ = 0;
}

gpu_present_target& gpu_present_target::operator=(gpu_present_target&& other) noexcept
{
    if (this != &other)
    {
        destroy();
        device_ = other.device_;
        image_ = other.image_;
        staging_ = other.staging_;
        format_ = other.format_;
        width_ = other.width_;
        height_ = other.height_;
        last_upload_bytes_ = other.last_upload_bytes_;
        other.device_ = nullptr;
        other.image_ = nullptr;
        other.staging_ = nullptr;
        other.format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
        other.width_ = 0;
        other.height_ = 0;
        other.last_upload_bytes_ = 0;
    }
    return *this;
}

bool gpu_present_target::create(const gpu_device& dev, int width, int height)
{
    destroy();

    if (!dev.valid() || width <= 0 || height <= 0) { return false; }

    device_ = dev.handle();
    width_ = width;
    height_ = height;

    // ---- The format ---------------------------------------------------------
    //
    // The BYTE LAYOUT is fixed by our framebuffer: SDL_PIXELFORMAT_ARGB8888 is a
    // packed 32-bit value, so on a little-endian machine its bytes are B, G, R, A
    // — which is B8G8R8A8. We do not guess that; SDL is asked, because the answer
    // is endianness-dependent and asking costs nothing.
    //
    // The sRGB-NESS is chosen to match the swapchain, so that a decode on read
    // cancels an encode on write. See this file's header comment for what happens
    // when it does not.
    const SDL_GPUTextureFormat layout =
        SDL_GetGPUTextureFormatFromPixelFormat(SDL_PIXELFORMAT_ARGB8888);

    format_ = layout;
    if (is_srgb_format(dev.report().swapchain_format))
    {
        format_ = (layout == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM)
                      ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                      : SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    }

    // ---- The texture --------------------------------------------------------
    SDL_GPUTextureCreateInfo tex{};
    tex.type = SDL_GPU_TEXTURETYPE_2D;
    tex.format = format_;
    // SAMPLER, because a blit reads its source the way a shader would — through a
    // sampler. Not COLOR_TARGET: nothing ever renders *into* this texture, and
    // asking for a usage we do not need can cost real memory on tiled hardware.
    tex.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex.width = static_cast<Uint32>(width);
    tex.height = static_cast<Uint32>(height);
    tex.layer_count_or_depth = 1;
    tex.num_levels = 1;
    tex.sample_count = SDL_GPU_SAMPLECOUNT_1;

    image_ = SDL_CreateGPUTexture(device_, &tex);
    if (image_ == nullptr)
    {
        SDL_Log("SDL_CreateGPUTexture failed: %s", SDL_GetError());
        destroy();
        return false;
    }
    SDL_SetGPUTextureName(device_, image_, "framebuffer mirror");

    // ---- The staging buffer -------------------------------------------------
    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = static_cast<Uint32>(width) * static_cast<Uint32>(height) * 4u;

    staging_ = SDL_CreateGPUTransferBuffer(device_, &tb);
    if (staging_ == nullptr)
    {
        SDL_Log("SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        destroy();
        return false;
    }

    return true;
}

void gpu_present_target::destroy()
{
    if (device_ != nullptr)
    {
        // Release order does not matter between these two — they are independent
        // allocations — but the null checks do: SDL 3.4 does not document these
        // as null-safe. (SDL's main branch has since added that guarantee; we
        // build against the pinned release and do not rely on it.)
        if (staging_ != nullptr) { SDL_ReleaseGPUTransferBuffer(device_, staging_); }
        if (image_ != nullptr) { SDL_ReleaseGPUTexture(device_, image_); }
    }
    device_ = nullptr;
    staging_ = nullptr;
    image_ = nullptr;
    format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    width_ = 0;
    height_ = 0;
    last_upload_bytes_ = 0;
}

void gpu_present_target::upload(SDL_GPUCommandBuffer* cb, const framebuffer& fb)
{
    if (!valid() || cb == nullptr) { return; }
    if (fb.width() != width_ || fb.height() != height_) { return; }

    const Uint32 bytes = static_cast<Uint32>(width_) * static_cast<Uint32>(height_) * 4u;

    // ---- Hop 1: CPU memory -> memory both sides can see ---------------------
    //
    // `cycle = true` is the important argument, and it is SDL_GPU's answer to a
    // problem every other API makes you solve yourself. This buffer may still be
    // referenced by last frame's command buffer, which the GPU may not have
    // reached yet. Cycling hands us a *different* internal buffer in that case,
    // so the write cannot land on top of a read that has not happened. Pass false
    // and the picture tears intermittently under load — a bug that only appears
    // when the GPU falls behind, i.e. only on the slow machine you do not own.
    void* mapped = SDL_MapGPUTransferBuffer(device_, staging_, true);
    if (mapped == nullptr)
    {
        SDL_Log("SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        return;
    }
    std::memcpy(mapped, fb.data(), bytes);
    SDL_UnmapGPUTransferBuffer(device_, staging_);
    last_upload_bytes_ = bytes;

    // ---- Hop 2: recorded now, executed later --------------------------------
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cb);

    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = staging_;
    source.offset = 0;
    // Both are counts of PIXELS and ROWS, not bytes — the one field in this
    // struct that is routinely filled in with a pitch and silently produces a
    // sheared image, exactly like Lesson 1.5's framebuffer pitch bug.
    source.pixels_per_row = static_cast<Uint32>(width_);
    source.rows_per_layer = static_cast<Uint32>(height_);

    SDL_GPUTextureRegion dest{};
    dest.texture = image_;
    dest.w = static_cast<Uint32>(width_);
    dest.h = static_cast<Uint32>(height_);
    dest.d = 1;

    // Cycle the texture too, for the same reason and against the same hazard:
    // the previous frame's blit may still be reading it.
    SDL_UploadToGPUTexture(copy, &source, &dest, true);

    SDL_EndGPUCopyPass(copy);
}

void gpu_present_target::blit_onto(SDL_GPUCommandBuffer* cb, SDL_GPUTexture* swapchain,
                                   Uint32 swap_w, Uint32 swap_h, bool smooth) const
{
    if (!valid() || cb == nullptr || swapchain == nullptr) { return; }

    const blit_rect fit = fit_centred(static_cast<Uint32>(width_),
                                      static_cast<Uint32>(height_), swap_w, swap_h);
    if (fit.w == 0 || fit.h == 0) { return; }

    SDL_GPUBlitInfo blit{};
    blit.source.texture = image_;
    blit.source.w = static_cast<Uint32>(width_);
    blit.source.h = static_cast<Uint32>(height_);

    blit.destination.texture = swapchain;
    blit.destination.x = fit.x;
    blit.destination.y = fit.y;
    blit.destination.w = fit.w;
    blit.destination.h = fit.h;

    // LOAD, not CLEAR: the caller has already cleared the whole swapchain in a
    // render pass, and CLEAR here would only clear the region we are about to
    // overwrite anyway — leaving the letterbox bars undefined, which on some
    // drivers means last frame's contents and on others means garbage.
    blit.load_op = SDL_GPU_LOADOP_LOAD;
    blit.filter = smooth ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;

    SDL_BlitGPUTexture(cb, &blit);
}

} // namespace engine
