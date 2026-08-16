// src/gfx/gpu_present.hpp — putting a CPU framebuffer on screen with the GPU.
//
// Lesson 4.2. Modules 1 to 3 presented the framebuffer through an SDL_Renderer
// streaming texture, which was the right tool while the CPU owned every pixel.
// This file does the same job through SDL_GPU, and the reason to do it now — a
// module before any shader exists — is that it needs *every object in the API
// except the pipeline*, and each of those objects has a counterpart the student
// has already built:
//
//   engine::framebuffer      the pixels                 (Lesson 1.5)
//     -> transfer buffer     memory both sides can see
//     -> SDL_GPUTexture      the pixels, on the device  (Lesson 3.9's texture)
//     -> blit                a 1:1 copy onto...
//     -> swapchain texture   the image the display scans out
//
// Nothing here draws anything. The picture is still made by the software
// rasterizer; what changes is who carries it to the screen — and with it, *when*
// the carrying happens, which is the subject of the whole lesson.
//
// THE ONE SUBTLETY IS THE TRANSFER FUNCTION, and it is not optional. Our
// framebuffer holds sRGB-ENCODED bytes (Lesson 1.6). A swapchain may be a plain
// `_UNORM` format, which applies no transfer function at all, or a `_UNORM_SRGB`
// one, which DECODES on read and ENCODES on write. Copy encoded bytes into an
// sRGB target without thinking and they are encoded twice, which looks exactly
// like a washed-out, foggy image — the Module 6 artifact, arriving three modules
// early. The fix is to give the source texture the same sRGB-ness as the
// destination, so a decode on read cancels the encode on write. `create` does
// this from the device's reported swapchain format; §5 of the lesson measures it.

#pragma once

#include "gfx/framebuffer.hpp"
#include "gfx/gpu_device.hpp"

#include <SDL3/SDL.h>

namespace engine {

/// A rectangle in swapchain pixels — where the image lands inside the window.
struct blit_rect
{
    Uint32 x = 0;
    Uint32 y = 0;
    Uint32 w = 0;
    Uint32 h = 0;
};

/// The largest centred rectangle of `src_w` x `src_h`'s aspect ratio that fits
/// inside `dst_w` x `dst_h`. Letterboxing, in one function.
///
/// Needed because the swapchain is *not* our framebuffer's size and never was:
/// the window is resizable, and on a HiDPI display the swapchain is measured in
/// physical pixels, so a 1280x720 window can hand back a 2560x1440 texture.
/// Stretching to fill would distort every circle in the scene; this keeps the
/// shape and pays for it with bars.
[[nodiscard]] blit_rect fit_centred(Uint32 src_w, Uint32 src_h,
                                    Uint32 dst_w, Uint32 dst_h);

/// Owns the device-side copy of a CPU framebuffer, and the staging memory that
/// feeds it.
///
/// **Movable, not copyable**, like every resource type in this engine — a copy
/// would mean a second owner releasing the same texture.
///
/// Two objects live in here and they are easy to confuse, so: the TEXTURE lives
/// in memory only the GPU reads efficiently, and cannot be written by the CPU at
/// all. The TRANSFER BUFFER lives in memory *both* can see, which is why it
/// exists and why it is slower. Every pixel we upload takes both hops, and no
/// API in this class can hide that, because it is the shape of the machine.
class gpu_present_target
{
public:
    gpu_present_target() = default;
    ~gpu_present_target();

    gpu_present_target(const gpu_present_target&) = delete;
    gpu_present_target& operator=(const gpu_present_target&) = delete;

    gpu_present_target(gpu_present_target&& other) noexcept;
    gpu_present_target& operator=(gpu_present_target&& other) noexcept;

    /// Allocate a `width` x `height` device texture and the staging buffer for it.
    ///
    /// The texture's format is chosen to match the device's swapchain in
    /// sRGB-ness, for the reason in this file's header comment.
    ///
    /// @return false on failure, having released anything partially created. The
    ///         reason is logged.
    [[nodiscard]] bool create(const gpu_device& dev, int width, int height);

    void destroy();

    [[nodiscard]] bool valid() const { return image_ != nullptr; }

    /// Record "copy these pixels to the device" into `cb`.
    ///
    /// The memcpy into the staging buffer happens NOW, on the CPU, before this
    /// returns. The copy from staging to texture happens LATER, when the command
    /// buffer is submitted and the GPU reaches it. Both halves are in this one
    /// call because they must stay adjacent, and the split is the lesson.
    ///
    /// `fb` must be exactly the size passed to `create`; a mismatch is a
    /// programmer error and the call does nothing.
    void upload(SDL_GPUCommandBuffer* cb, const framebuffer& fb);

    /// Record a 1:1 (or scaled) copy of our texture onto `swapchain`.
    ///
    /// Must be called OUTSIDE any render or copy pass — SDL says so of
    /// `SDL_BlitGPUTexture`, because a blit is itself implemented as a pass.
    ///
    /// @param smooth linear filtering when scaled up. `false` gives the honest
    ///               nearest-neighbour look of Modules 1-3, where one framebuffer
    ///               pixel is a visible square.
    void blit_onto(SDL_GPUCommandBuffer* cb, SDL_GPUTexture* swapchain,
                   Uint32 swap_w, Uint32 swap_h, bool smooth) const;

    [[nodiscard]] SDL_GPUTexture* image() const { return image_; }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

    /// The format actually chosen for the device texture — worth logging, since
    /// it is derived from the swapchain rather than picked.
    [[nodiscard]] SDL_GPUTextureFormat format() const { return format_; }

    /// Bytes copied into staging by the most recent `upload`. The frame's upload
    /// bandwidth, for the HUD.
    [[nodiscard]] Uint32 last_upload_bytes() const { return last_upload_bytes_; }

private:
    SDL_GPUDevice* device_ = nullptr;          ///< NOT owned; the gpu_device owns it
    SDL_GPUTexture* image_ = nullptr;          ///< owning by contract
    SDL_GPUTransferBuffer* staging_ = nullptr; ///< owning by contract
    SDL_GPUTextureFormat format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    int width_ = 0;
    int height_ = 0;
    Uint32 last_upload_bytes_ = 0;
};

} // namespace engine
