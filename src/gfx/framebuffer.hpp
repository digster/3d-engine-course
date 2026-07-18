// src/gfx/framebuffer.hpp — a block of memory that is an image.
//
// The first pixels the engine owns. Everything drawn from here to the end of
// Module 3 lands in one of these, one pixel at a time, by code we wrote.

#pragma once

#include <SDL3/SDL.h>

#include <cstddef>
#include <vector>

namespace engine {

/// Pack 8-bit colour components into one 32-bit pixel, alpha in the most
/// significant byte: 0xAARRGGBB.
///
/// This layout is not arbitrary — it is exactly what SDL calls
/// `SDL_PIXELFORMAT_ARGB8888`. SDL's "8888" names describe a value packed into
/// a native-endianness 32-bit integer, with the leftmost component in the most
/// significant bits. So as long as we only ever touch these pixels as `Uint32`,
/// this matches byte-for-byte on both little- and big-endian machines, and we
/// never have to think about endianness at all. (Look at the individual bytes
/// instead and the order flips underneath you — see Lesson 1.5 §3.3.)
///
/// Colour is treated as a bag of bits here on purpose. What those bits actually
/// mean — why 128 is not half as bright as 255 — is Lesson 1.6.
[[nodiscard]] constexpr Uint32 pack_argb(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255)
{
    return (static_cast<Uint32>(a) << 24)
         | (static_cast<Uint32>(r) << 16)
         | (static_cast<Uint32>(g) << 8)
         | static_cast<Uint32>(b);
}

/// A CPU-side image: `width × height` 32-bit pixels, tightly packed, row-major.
///
/// "Row-major" is the whole idea, and it is worth stating as a formula because
/// every drawing routine in the next two modules is built on it:
///
///     index = y * width + x
///
/// Memory is one-dimensional. The two-dimensionality of an image is a
/// *convention* about how to read that line of memory — we agree to treat the
/// first `width` pixels as the top row, the next `width` as the row below it,
/// and so on. Nothing in the hardware enforces it, which is precisely why an
/// x-coordinate that strays past `width` does not produce an error: it produces
/// a pixel on the *next row down*, and a rectangle drawn off the right edge
/// reappears, shredded, on the left. See Lesson 1.5 §3.2.
class framebuffer
{
public:
    /// Allocates `width * height` pixels, zero-initialised (transparent black).
    /// Dimensions below 1 are clamped: a zero-sized buffer would make every
    /// index calculation degenerate rather than fail.
    framebuffer(int width, int height);

    // ---- Drawing ------------------------------------------------------------

    /// Set every pixel to one colour.
    void clear(Uint32 colour);

    /// Set one pixel. **Bounds-checked**: coordinates outside the buffer are
    /// silently ignored rather than corrupting a neighbouring row.
    ///
    /// This is the safe default, and it costs two comparisons and a multiply per
    /// pixel. Code that fills large areas should hoist that work out of its
    /// inner loop with row() instead.
    void put_pixel(int x, int y, Uint32 colour);

    /// Read one pixel. Returns 0 for out-of-bounds coordinates.
    [[nodiscard]] Uint32 pixel_at(int x, int y) const;

    /// Fill an axis-aligned rectangle, clipped to the buffer.
    ///
    /// Note what this does *not* do: call put_pixel in a loop. The rectangle is
    /// clipped once, up front, and then each row is filled as a contiguous run.
    /// Clipping once instead of per-pixel is the difference between two
    /// comparisons and two-comparisons-times-the-area.
    void fill_rect(int x, int y, int w, int h, Uint32 colour);

    // ---- Access -------------------------------------------------------------

    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

    /// Bytes per row. For this tightly-packed buffer it is always
    /// `width * 4`, but the concept is separate from the width for a reason:
    /// the GPU texture we upload into may have a *different* pitch, with padding
    /// at the end of each row. Lesson 1.5 §4.3.
    [[nodiscard]] int pitch() const { return width_ * static_cast<int>(sizeof(Uint32)); }

    /// Pointer to the first pixel. For uploads and tests.
    [[nodiscard]] const Uint32* data() const { return pixels_.data(); }

    /// Pointer to the first pixel of row `y`.
    ///
    /// This is the deliberate escape hatch: a tight inner loop that has already
    /// established its own bounds can write through this pointer and skip
    /// put_pixel's per-pixel check and index multiply entirely. The row index
    /// itself is still validated, so the worst a caller can do is scribble within
    /// one row rather than anywhere in the process.
    ///
    /// Safe by default, fast when you ask — the pattern every part of this
    /// engine's drawing API will follow.
    [[nodiscard]] Uint32* row(int y);
    [[nodiscard]] const Uint32* row(int y) const;

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

    // std::vector owns the allocation, so the buffer frees itself when the
    // framebuffer goes out of scope — no matching delete to forget, on any exit
    // path. That is RAII, and it is why the engine core will keep reaching for
    // owning containers rather than new/delete. Module 5 makes the principle
    // explicit; here it is simply the obvious choice.
    std::vector<Uint32> pixels_;
    int width_;
    int height_;
};

} // namespace engine
