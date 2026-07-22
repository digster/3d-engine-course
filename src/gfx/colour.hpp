// src/gfx/colour.hpp — what the numbers in a pixel actually mean.
//
// Lesson 1.5 treated a pixel as 32 bits and got away with it, because copying
// bits around never asks what they represent. The moment we *combine* two
// colours — mixing, fading, averaging, or later adding up light from several
// sources — the question becomes unavoidable, and the intuitive answer is wrong.

#pragma once

#include <SDL3/SDL.h>

namespace engine {

// ---- Packing and unpacking --------------------------------------------------
// ARGB8888: alpha in the most significant byte, blue in the least. See Lesson
// 1.5 §3.3 for why this layout, and why we only ever touch pixels as Uint32.

/// Pack 8-bit components into one 32-bit pixel: 0xAARRGGBB.
[[nodiscard]] constexpr Uint32 pack_argb(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255)
{
    return (static_cast<Uint32>(a) << 24)
         | (static_cast<Uint32>(r) << 16)
         | (static_cast<Uint32>(g) << 8)
         | static_cast<Uint32>(b);
}

[[nodiscard]] constexpr Uint8 alpha_of(Uint32 c) { return static_cast<Uint8>((c >> 24) & 0xFFu); }
[[nodiscard]] constexpr Uint8 red_of(Uint32 c)   { return static_cast<Uint8>((c >> 16) & 0xFFu); }
[[nodiscard]] constexpr Uint8 green_of(Uint32 c) { return static_cast<Uint8>((c >> 8) & 0xFFu); }
[[nodiscard]] constexpr Uint8 blue_of(Uint32 c)  { return static_cast<Uint8>(c & 0xFFu); }

// ---- The sRGB transfer functions --------------------------------------------
//
// A stored channel value is NOT a measure of light. It is a measure of light
// that has been put through a curve, so that the 256 available steps are spent
// where the eye can tell them apart — which is overwhelmingly at the dark end.
//
// The consequence, in one number: the stored value 128 emits about 21.6% of
// white's light, not 50%. Half the light is stored as 188.
//
// These two functions are the exact standard transform and its inverse. They are
// piecewise: a short linear segment near black, then a power curve. The linear
// toe exists because the power curve's slope goes to infinity at zero, which
// would make the darkest few values impossible to represent sensibly.

/// Encoded sRGB [0,1] -> linear light [0,1].
[[nodiscard]] float srgb_to_linear(float encoded);

/// Linear light [0,1] -> encoded sRGB [0,1].
[[nodiscard]] float linear_to_srgb(float linear);

/// Encoded 8-bit channel -> linear light, via a 256-entry lookup table.
///
/// There are only 256 possible inputs, so the entire function fits in a kilobyte
/// of floats computed once. That turns a `pow` — tens of cycles — into a load,
/// which matters when it is called three times per pixel. The reverse direction
/// gets no such table, because its input is a continuous float.
[[nodiscard]] float srgb_to_linear_u8(Uint8 encoded);

/// Linear light -> encoded 8-bit channel, clamped and rounded.
[[nodiscard]] Uint8 linear_to_srgb_u8(float linear);

// ---- Linear-light colour ----------------------------------------------------

/// A colour as the three quantities of light it represents, each in [0,1].
///
/// This is the form to do **arithmetic** in. A `Uint32` is a *storage* format:
/// its channels have been through the transfer function above, so adding or
/// scaling them does not add or scale light. Every operation that models light —
/// interpolating a colour across a triangle (Lesson 2.4), summing the
/// contributions of several lamps (Module 3), multiplying by a material's
/// reflectance (Module 6) — belongs in this type, and the conversion happens at
/// the edges of the calculation rather than inside it.
///
/// Values are allowed to exceed 1.0. That is what "brighter than white" means,
/// and Module 6's HDR pipeline treats it as signal; `to_encoded` clamps only
/// because an 8-bit pixel has nowhere to put the excess.
struct linear_rgb
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

/// Decode a stored pixel into the light it represents.
///
/// Alpha is dropped, deliberately: alpha is a coverage fraction rather than a
/// quantity of light, so it does not belong in a type whose whole purpose is to
/// be linear in light. Carry it separately if you need it.
[[nodiscard]] linear_rgb to_linear(Uint32 encoded);

/// Encode light back into a stored pixel — clamped to [0,1], alpha 255.
[[nodiscard]] Uint32 to_encoded(linear_rgb light);

// ---- Mixing -----------------------------------------------------------------

/// Mix two colours by interpolating their **stored values** directly.
///
/// This is what everyone writes first, and it is what almost every naive
/// blending, fading and image-scaling routine does. It is wrong in a specific,
/// visible way: mixing red and green half-and-half gives a dark olive rather
/// than a bright yellow, because averaging the encoded numbers does not average
/// the light. Kept here — and used in the demo — so the error can be seen next
/// to the correct version rather than described.
[[nodiscard]] Uint32 mix_encoded(Uint32 a, Uint32 b, float t);

/// Mix two colours by interpolating the **light** they represent.
///
/// Decode both to linear, interpolate, re-encode. This is what "half way between
/// these two colours" physically means, and it is what a renderer must do
/// whenever it is modelling light rather than choosing an artistic ramp.
///
/// Note that alpha is interpolated in its stored form, without conversion: alpha
/// is a coverage fraction, not a quantity of light, and putting it through a
/// transfer function meant for light would be a category error.
[[nodiscard]] Uint32 mix_linear(Uint32 a, Uint32 b, float t);

} // namespace engine
