// src/gfx/colour.cpp — the sRGB transfer functions and gamma-correct mixing.

#include "gfx/colour.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace engine {

float srgb_to_linear(float encoded)
{
    // The standard's exact piecewise definition. The threshold and the 12.92
    // slope are chosen so the two pieces meet smoothly; they are not roundable
    // to "gamma 2.2", which is why we spell them out rather than using pow(x,
    // 2.2) as a shortcut. The shortcut is close, and close is how a pipeline
    // accumulates error nobody can later locate.
    if (encoded <= 0.04045f)
    {
        return encoded / 12.92f;
    }
    return std::pow((encoded + 0.055f) / 1.055f, 2.4f);
}

float linear_to_srgb(float linear)
{
    if (linear <= 0.0031308f)
    {
        return linear * 12.92f;
    }
    return 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}

namespace {

/// Decoding table for all 256 possible encoded values, built once at startup.
///
/// Initialised by an immediately-invoked lambda so the table can be `const`:
/// the alternative — a mutable array plus an `init()` somebody has to remember
/// to call — is a category of bug this design simply does not have.
const std::array<float, 256> k_to_linear = [] {
    std::array<float, 256> table{};
    for (int i = 0; i < 256; ++i)
    {
        table[static_cast<std::size_t>(i)] = srgb_to_linear(static_cast<float>(i) / 255.0f);
    }
    return table;
}();

/// Interpolate one channel's stored values directly — the naive path.
[[nodiscard]] Uint8 mix_channel_encoded(Uint8 x, Uint8 y, float t)
{
    const float a = static_cast<float>(x);
    const float b = static_cast<float>(y);
    return static_cast<Uint8>(std::clamp(a + (b - a) * t, 0.0f, 255.0f) + 0.5f);
}

/// Interpolate one channel's *light*, then re-encode — the correct path.
[[nodiscard]] Uint8 mix_channel_linear(Uint8 x, Uint8 y, float t)
{
    const float a = srgb_to_linear_u8(x);
    const float b = srgb_to_linear_u8(y);
    return linear_to_srgb_u8(a + (b - a) * t);
}

} // namespace

float srgb_to_linear_u8(Uint8 encoded)
{
    return k_to_linear[encoded];
}

Uint8 linear_to_srgb_u8(float linear)
{
    // Clamp before encoding. Light values arriving from a calculation can
    // legitimately exceed 1.0 — that is what "brighter than white" means, and
    // Module 6's HDR pipeline treats it as signal rather than error. Here there
    // is nowhere to put it, so it saturates.
    const float encoded = linear_to_srgb(std::clamp(linear, 0.0f, 1.0f));

    // + 0.5 rounds to nearest rather than truncating. Truncation costs half a
    // level on every channel of every pixel, which is a visible darkening once
    // it compounds through a few passes.
    return static_cast<Uint8>(encoded * 255.0f + 0.5f);
}

linear_rgb to_linear(Uint32 encoded)
{
    // Three table loads. The table is the reason this is cheap enough to call
    // per vertex — or, if you must, per pixel.
    return {srgb_to_linear_u8(red_of(encoded)),
            srgb_to_linear_u8(green_of(encoded)),
            srgb_to_linear_u8(blue_of(encoded))};
}

Uint32 to_encoded(linear_rgb light)
{
    // The expensive direction, and it is worth knowing why the asymmetry
    // exists. Decoding has 256 possible inputs, so it fits in a table. Encoding
    // takes a continuous float, so it runs the real `pow` — around thirty times
    // the cost of a load. Lesson 2.4 §3.7 measures what that means when it
    // happens once per pixel, and Exercise 2.4.3 builds the table that removes
    // it, with the error bound worked out rather than hoped for.
    return pack_argb(linear_to_srgb_u8(light.r),
                     linear_to_srgb_u8(light.g),
                     linear_to_srgb_u8(light.b),
                     255);
}

Uint32 mix_encoded(Uint32 a, Uint32 b, float t)
{
    return pack_argb(mix_channel_encoded(red_of(a), red_of(b), t),
                     mix_channel_encoded(green_of(a), green_of(b), t),
                     mix_channel_encoded(blue_of(a), blue_of(b), t),
                     mix_channel_encoded(alpha_of(a), alpha_of(b), t));
}

Uint32 mix_linear(Uint32 a, Uint32 b, float t)
{
    // Colour channels go through the transfer function; alpha does not. Alpha
    // measures how much of a surface is covered, which is already a linear
    // quantity — see the header.
    return pack_argb(mix_channel_linear(red_of(a), red_of(b), t),
                     mix_channel_linear(green_of(a), green_of(b), t),
                     mix_channel_linear(blue_of(a), blue_of(b), t),
                     mix_channel_encoded(alpha_of(a), alpha_of(b), t));
}

} // namespace engine
