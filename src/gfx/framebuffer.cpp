// src/gfx/framebuffer.cpp — implementation of the CPU pixel buffer.

#include "gfx/framebuffer.hpp"

#include <algorithm>

namespace engine {

framebuffer::framebuffer(int width, int height)
    : width_(width > 0 ? width : 1)
    , height_(height > 0 ? height : 1)
{
    // One allocation, value-initialised to zero. A framebuffer full of whatever
    // happened to be in that memory would show as static on the first frame —
    // and, worse, would look like a bug in whatever drew next.
    pixels_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), 0u);
}

void framebuffer::clear(Uint32 colour)
{
    // std::fill over the whole vector, not a nested x/y loop. The buffer is one
    // contiguous run of memory and the two-dimensionality is our interpretation,
    // so a clear has no reason to know about rows at all.
    std::fill(pixels_.begin(), pixels_.end(), colour);
}

void framebuffer::put_pixel(int x, int y, Uint32 colour)
{
    if (!in_bounds(x, y))
    {
        return;
    }
    pixels_[index(x, y)] = colour;
}

Uint32 framebuffer::pixel_at(int x, int y) const
{
    if (!in_bounds(x, y))
    {
        return 0u;
    }
    return pixels_[index(x, y)];
}

void framebuffer::fill_rect(int x, int y, int w, int h, Uint32 colour)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }

    // Clip once. Everything after this point is guaranteed to be inside the
    // buffer, so the fill itself needs no checks at all.
    //
    // Doing it this way round matters more than it looks. The alternative —
    // testing each pixel as you write it — costs two comparisons per pixel, so a
    // full-screen rectangle at 320x180 pays 115,200 comparisons to answer a
    // question that four comparisons settled up front.
    const int x0 = std::max(x, 0);
    const int y0 = std::max(y, 0);
    const int x1 = std::min(x + w, width_);
    const int y1 = std::min(y + h, height_);

    if (x0 >= x1 || y0 >= y1)
    {
        return;   // entirely off-screen
    }

    for (int row_y = y0; row_y < y1; ++row_y)
    {
        // Fill each row as one contiguous run — rows outer, columns inner, never
        // the reverse. A 64-byte cache line holds sixteen 32-bit pixels, so
        // walking along a row uses all sixteen before needing another line;
        // walking down a column uses one of every sixteen and touches a new line
        // every write. Measured on this codebase (Apple M4 Pro, -O2): the column
        // order costs 10.9x more at 320x180 and 48.8x more at 3840x2160, the gap
        // widening as the buffer outgrows the caches. Lesson 1.5 §3.5.
        Uint32* const start = &pixels_[index(x0, row_y)];
        std::fill_n(start, x1 - x0, colour);
    }
}

Uint32* framebuffer::row(int y)
{
    // Clamp rather than assert: this is an inner-loop accessor, and returning a
    // valid row for a bad index keeps a caller's arithmetic mistake inside the
    // buffer instead of loose in the process.
    const int clamped = std::clamp(y, 0, height_ - 1);
    return &pixels_[index(0, clamped)];
}

const Uint32* framebuffer::row(int y) const
{
    const int clamped = std::clamp(y, 0, height_ - 1);
    return &pixels_[index(0, clamped)];
}

} // namespace engine
