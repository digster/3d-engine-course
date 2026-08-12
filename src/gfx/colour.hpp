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

// ---- The same encode, without the pow (Lesson 3.10) -------------------------
//
// `linear_to_srgb` above runs `std::pow`, and Lesson 3.10 measures what that
// costs: with a texture bound and no lighting, the encode is the single largest
// item in the fill loop — larger than the depth test, the perspective divide and
// the four texel fetches together.
//
// The obvious fix does not exist. Decoding fits in a table because it has 256
// possible inputs; encoding takes a continuous float, so there is nothing to
// index. What is available instead is an APPROXIMATION, and the honest way to
// ship one is to state its error rather than to call it "fast":
//
//   * Below the toe (`linear <= 0.0031308`) the exact function is `12.92 * x`.
//     One multiply. It is kept exactly, which costs nothing and removes the
//     hardest part of the curve — the part with a finite slope at zero, where
//     every square root has an infinite one — from the approximation's job.
//   * Above it, a four-term fit in `{sqrt(x), x^1/4, x^1/8, x}`. Three CHAINED
//     square roots, each a single hardware instruction, and four multiply-adds.
//
// MEASURED (scratch/fit_srgb.py, then scratch/verify_310.cpp §F): worst error
// 0.0000451 in [0,1] — about one hundredth of an 8-bit code — and yet **0.60% of
// inputs still come out one code different**, because rounding is a cliff and an
// error of a hundredth of a code flips the answer for every input that lands
// within a hundredth of a halfway point. Exact at 0 and exact at 1.
//
// The coefficients were FITTED for this course, by least squares reweighted
// toward minimax, and constrained to be exact at white. They are not magic
// numbers to be copied around: `scratch/fit_srgb.py` derives them and prints the
// error, and re-running it is how you would change the trade.

/// Linear light [0,1] -> encoded sRGB [0,1], approximately. See above for the error.
[[nodiscard]] float linear_to_srgb_fast(float linear);

/// Linear light -> encoded 8-bit channel, via `linear_to_srgb_fast`.
[[nodiscard]] Uint8 linear_to_srgb_u8_fast(float linear);

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

/// Which sRGB encode a fill uses — Lesson 3.10.
///
/// This is a **speed/accuracy point**, not a right-and-wrong pair, which makes it
/// the first knob in this engine that is neither. `blend_space::encoded`,
/// `interpolation::affine` and `draw_line_naive` are all kept so a *mistake* can
/// be summoned; this one is kept because both answers are defensible and the
/// choice belongs to whoever knows what the pixels are for.
enum class encode_mode
{
    /// `std::pow`, to the last bit of the standard. What an offline renderer, a
    /// texture compressor or a test that compares against a reference wants.
    exact,

    /// The square-root fit. **The default for real-time work**, because it is
    /// several times cheaper and its worst error is one 8-bit code on 0.60% of
    /// inputs — a difference that is, by construction, at the very limit of what
    /// the output format can even represent.
    fast
};

/// Encode light back into a stored pixel — clamped to [0,1], alpha 255.
[[nodiscard]] Uint32 to_encoded(linear_rgb light, encode_mode mode = encode_mode::exact);

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
