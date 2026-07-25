// src/gfx/depth_buffer.hpp — one number per pixel: how far away the thing we
// have already drawn there is.
//
// The framebuffer answers "what colour is this pixel". This answers "and how far
// away was whatever put it there", and those two answers together are enough to
// solve hidden-surface removal for *any* geometry, in any order, without sorting
// anything. Lesson 3.1.
//
// WHY THIS IS ITS OWN TYPE AND NOT A FIELD OF framebuffer
//
// It is tempting to bolt a `std::vector<float>` onto `framebuffer` and be done.
// Three reasons not to, and the third is the one that settles it:
//
//   1. Not every framebuffer wants depth. The 2-D demos, Pong, the HUD overlay,
//      and Module 6's post-processing intermediates are colour-only. Fusing the
//      two would make every one of them allocate a depth buffer it never reads —
//      at 1920x1080 that is 8 MB of nothing.
//   2. They are different formats. Colour is four 8-bit channels, packed; depth
//      is a single value with its own precision story (see `depth_format`).
//      Their clear values are unrelated: a background colour, and *far*.
//   3. **The hardware keeps them separate.** SDL_GPU's `SDL_BeginGPURenderPass`
//      takes an array of `SDL_GPUColorTargetInfo` and, as a *separate* parameter,
//      a `SDL_GPUDepthStencilTargetInfo*` — which "may be NULL". Colour and depth
//      are two attachments, bound independently, each with its own load and store
//      operations. Modelling that split now means Module 4 is a rename, not a
//      redesign. (Verified against SDL3/SDL_gpu.h.)
//
// WHY THE DEPTH *TEST* IS NOT A MEMBER OF THIS CLASS
//
// Because the comparison is not a property of the storage. On real hardware it is
// pipeline state: `SDL_GPUDepthStencilState` carries a `compare_op`
// (`SDL_GPU_COMPAREOP_LESS` and friends), an `enable_depth_test` and an
// `enable_depth_write` — three independent knobs that different passes set
// differently. A shadow pass writes depth and no colour; a transparent pass tests
// depth and does not write it. Baking `less-than, always write` into the buffer
// would make those unexpressible. So this class stores and clears; the rasterizer
// compares.

#pragma once

#include <cmath>     // std::round, in quantise()
#include <cstddef>
#include <vector>

namespace engine {

/// The precision the buffer stores depth at.
///
/// A real GPU depth attachment has a *format* — `SDL_GPU_TEXTUREFORMAT_D16_UNORM`,
/// `D24_UNORM`, `D32_FLOAT` — and that choice is one of the more consequential
/// ones in a renderer, because it decides how far apart two surfaces must be
/// before the buffer can tell them apart at all. Lesson 3.1 §3.6.
///
/// **What we model and what we do not.** We keep the storage `float` in every
/// case and *round to the format's grid* on write. The visible behaviour — which
/// is what z-fighting is — is therefore exactly right, while the memory saving of
/// a genuine 16-bit buffer is the part we are not modelling. That is a deliberate
/// simplification: it costs one multiply and one round per pixel and buys a
/// single code path, and the failure it exists to demonstrate is unaffected.
enum class depth_format
{
    f32,       ///< full float precision — no quantisation at all
    unorm24,   ///< 2^24 - 1 evenly spaced codes in [0, 1]
    unorm16    ///< 2^16 - 1 codes. Enough to see z-fighting on demand.
};

[[nodiscard]] const char* name_of(depth_format format);

/// A CPU-side depth attachment: `width × height` floats, row-major, same layout
/// rule as `framebuffer`:
///
///     index = y * width + x
///
/// Depth is in the **device range `[0, 1]`**, `0` at the near plane and `1` at
/// the far plane, matching SDL_GPU's NDC (conventions.html §4). *Smaller is
/// nearer*, which is why the test is a `<` and why a cleared buffer is full of
/// `1.0`.
///
/// The value stored here is the third component of the viewport transform's
/// output (`viewport::to_screen`, Lesson 2.11) — the one that has been computed
/// and thrown away since 2.11 for want of somewhere to put it.
class depth_buffer
{
public:
    /// The far value. A cleared buffer holds this, so the first triangle to
    /// cover a pixel always wins.
    static constexpr float k_far = 1.0f;

    /// Allocates `width * height` depths, all `k_far`. Dimensions below 1 are
    /// clamped, exactly as in `framebuffer`.
    depth_buffer(int width, int height, depth_format format = depth_format::f32);

    // ---- Per-frame ----------------------------------------------------------

    /// Set every depth. Defaults to `k_far` — "nothing has been drawn here yet".
    ///
    /// **This must happen every frame**, and forgetting it is the single most
    /// common z-buffer bug: last frame's depths survive, so this frame's geometry
    /// loses the test against surfaces that are no longer on screen and the image
    /// slowly eats itself. Lesson 3.1 §7.
    void clear(float depth = k_far);

    // ---- Access -------------------------------------------------------------

    /// Read one depth. Returns `k_far` for out-of-bounds coordinates — the
    /// answer that makes a stray read behave like empty space rather than like an
    /// occluder.
    [[nodiscard]] float depth_at(int x, int y) const;

    /// Round a depth to the grid this buffer's format can actually represent.
    ///
    /// Applied by the rasterizer **before** the comparison, not after, because
    /// that is the order the hardware uses: the incoming fragment's depth is
    /// converted to the attachment's format and then compared against a stored
    /// value already in that format. Comparing at full precision and *then*
    /// rounding would let two surfaces that store identically still order
    /// themselves, which is precisely the flicker we want to be able to
    /// reproduce.
    ///
    /// For `depth_format::f32` this returns its argument unchanged.
    [[nodiscard]] float quantise(float depth) const
    {
        if (codes_ <= 0.0f) { return depth; }
        // No clamp: the rasterizer only ever passes depths that came from a
        // vertex inside the frustum, and Lesson 3.3's clipping is what keeps
        // that true. A value outside [0,1] rounds to a grid point outside [0,1],
        // which is a wrong answer rather than a crash — the right place to fix
        // it is upstream, not here.
        return std::round(depth * codes_) / codes_;
    }

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }
    [[nodiscard]] depth_format format() const { return format_; }

    /// Change the storage precision. Existing contents are left alone; the next
    /// `clear` and every subsequent write use the new grid.
    void set_format(depth_format format);

    /// Pointer to the first depth of row `y` — the same escape hatch
    /// `framebuffer::row` provides, for the same reason.
    ///
    /// A triangle fill has already clipped its bounding box to the buffer, so it
    /// knows its indices are valid and should not pay to be told so again. The
    /// row index is still clamped, so the worst a caller can do is scribble
    /// within one row.
    [[nodiscard]] float* row(int y);
    [[nodiscard]] const float* row(int y) const;

private:
    [[nodiscard]] bool in_bounds(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < width_ && y < height_;
    }

    [[nodiscard]] std::size_t index(int x, int y) const
    {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
             + static_cast<std::size_t>(x);
    }

    std::vector<float> depth_;
    int width_;
    int height_;
    depth_format format_;

    /// The number of representable steps, or 0 for "no quantisation". Cached
    /// rather than switched on per pixel — `2^24 - 1 = 16777215` is exactly
    /// representable in a float (it is below `2^24`), so the round-trip
    /// multiply/divide introduces exactly one rounding and no drift.
    float codes_ = 0.0f;
};

} // namespace engine
