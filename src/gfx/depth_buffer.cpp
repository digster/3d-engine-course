// src/gfx/depth_buffer.cpp — implementation of the CPU depth attachment.

#include "gfx/depth_buffer.hpp"

#include <algorithm>

namespace engine {

namespace {

/// How many representable steps a format has, or 0 for "no quantisation".
///
/// `2^24 - 1 = 16777215` and `2^16 - 1 = 65535` are both exactly representable
/// in a float, which is what makes `round(z * codes) / codes` land on a genuine
/// grid point rather than near one.
[[nodiscard]] float codes_for(depth_format format)
{
    switch (format)
    {
    case depth_format::f32:     return 0.0f;
    case depth_format::unorm24: return 16777215.0f;   // 2^24 - 1
    case depth_format::unorm16: return 65535.0f;      // 2^16 - 1
    }
    return 0.0f;
}

} // namespace

const char* name_of(depth_format format)
{
    switch (format)
    {
    case depth_format::f32:     return "D32_FLOAT";
    case depth_format::unorm24: return "D24_UNORM";
    case depth_format::unorm16: return "D16_UNORM";
    }
    return "?";
}

depth_buffer::depth_buffer(int width, int height, depth_format format)
    : width_(width > 0 ? width : 1)
    , height_(height > 0 ? height : 1)
    , format_(format)
    , codes_(codes_for(format))
{
    // Value-initialised to FAR, not to zero. Zero is the *near* plane under this
    // course's [0,1] depth convention, so a zeroed depth buffer would claim the
    // camera's own eyeball occludes the entire scene and nothing would ever draw.
    // That is a real bug people hit, and it is worth the buffer never being able
    // to exist in that state.
    depth_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), k_far);
}

void depth_buffer::clear(float depth)
{
    std::fill(depth_.begin(), depth_.end(), depth);
}

float depth_buffer::depth_at(int x, int y) const
{
    if (!in_bounds(x, y))
    {
        return k_far;
    }
    return depth_[index(x, y)];
}

void depth_buffer::set_format(depth_format format)
{
    format_ = format;
    codes_ = codes_for(format);
}

float* depth_buffer::row(int y)
{
    const int clamped = std::clamp(y, 0, height_ - 1);
    return &depth_[index(0, clamped)];
}

const float* depth_buffer::row(int y) const
{
    const int clamped = std::clamp(y, 0, height_ - 1);
    return &depth_[index(0, clamped)];
}

} // namespace engine
