// engine/src/gfx/blend.cpp — compositing, in about forty lines of arithmetic.
//
// Lesson 6.11. Everything here is `over` (Porter-Duff, 1984) plus the discipline
// about *where* the transfer function goes, and the second half is the whole
// difficulty. The functions are short on purpose: the argument lives in
// `blend.hpp`, and the code should be short enough to check against it by eye.

#include <engine/gfx/blend.hpp>

#include <algorithm>

namespace engine {

const char* name_of(alpha_mode m)
{
    switch (m)
    {
    case alpha_mode::opaque: return "opaque";
    case alpha_mode::mask:   return "mask";
    case alpha_mode::blend:  return "blend";
    }
    return "?";
}

const char* name_of(alpha_storage s)
{
    switch (s)
    {
    case alpha_storage::straight:      return "straight";
    case alpha_storage::premultiplied: return "premultiplied";
    }
    return "?";
}

namespace {

/// Encode three quantities of light back into a pixel, keeping somebody else's
/// alpha byte.
///
/// `to_encoded` writes 255 into the alpha byte — deliberately, and Lesson 6.1's
/// harness asserts it, because a *colour* has no coverage. Here we are writing
/// into a framebuffer that already had a pixel in it, and the destination's
/// alpha is a property of the destination rather than of the light we just
/// computed, so it is carried across rather than overwritten.
///
/// (In this engine every framebuffer pixel is opaque, so the two spellings
/// currently produce the same bits. That is exactly why it is worth writing the
/// correct one now: the day a render target's alpha matters — an offscreen layer,
/// a UI atlas composited later — the wrong one would be a bug with no symptom
/// until then.)
[[nodiscard]] Uint32 encode_keeping_alpha(linear_rgb light, Uint32 alpha_from, encode_mode mode)
{
    return (to_encoded(light, mode) & 0x00FFFFFFu) | (alpha_from & 0xFF000000u);
}

} // namespace

Uint32 blend_over(Uint32 dst, Uint32 src, float a, encode_mode mode, alpha_storage how)
{
    // Clamp rather than trust. Coverage outside [0, 1] is not a brighter
    // fragment, it is a bug — an un-normalised interpolation, a material whose
    // alpha was authored as 0-255, a NaN arriving from an unclipped 1/w (3.3).
    // `std::clamp` on a NaN returns the NaN, so the guard is written the way
    // `sample_nearest` writes its index guard: only the negated comparison
    // catches it.
    const float cov = (a > 0.0f) ? ((a < 1.0f) ? a : 1.0f) : 0.0f;

    // DECODE BOTH, and this is the entire point of the function. `dst` is a
    // stored sRGB pixel and `src` is a stored sRGB pixel; the operator between
    // them is a weighted average of the LIGHT they represent, and a weighted
    // average of their stored codes is a different number (see
    // `blend_over_encoded`, and Lesson 1.6 for why).
    const linear_rgb d = to_linear(dst);
    const linear_rgb s = to_linear(src);

    const linear_rgb out = (how == alpha_storage::premultiplied)
                         ? over_premultiplied(s, cov, d)
                         : over(s, cov, d);

    return encode_keeping_alpha(out, dst, mode);
}

Uint32 blend_over_encoded(Uint32 dst, Uint32 src, float a)
{
    const float cov = (a > 0.0f) ? ((a < 1.0f) ? a : 1.0f) : 0.0f;
    const float inv = 1.0f - cov;

    // Byte arithmetic on values that are not quantities of anything. Written in
    // floats so that only ONE thing differs from `blend_over` — the missing
    // decode — and the measurement in §7 is of that difference alone rather than
    // of a rounding change smuggled in beside it.
    const auto mix = [cov, inv](Uint8 sc, Uint8 dc) {
        const float m = static_cast<float>(sc) * cov + static_cast<float>(dc) * inv;
        return static_cast<Uint8>(std::clamp(m + 0.5f, 0.0f, 255.0f));
    };

    return (dst & 0xFF000000u)
         | (static_cast<Uint32>(mix(red_of(src), red_of(dst))) << 16)
         | (static_cast<Uint32>(mix(green_of(src), green_of(dst))) << 8)
         | static_cast<Uint32>(mix(blue_of(src), blue_of(dst)));
}

Uint32 premultiply(Uint32 encoded)
{
    const float a = static_cast<float>(alpha_of(encoded)) * (1.0f / 255.0f);
    const linear_rgb c = to_linear(encoded);
    return encode_keeping_alpha({c.r * a, c.g * a, c.b * a}, encoded, encode_mode::exact);
}

Uint32 unpremultiply(Uint32 encoded)
{
    const Uint8 a8 = alpha_of(encoded);

    // Nothing to recover, and nothing to invent. A fully transparent premultiplied
    // texel is (0,0,0,0) BY CONSTRUCTION — that is the property the whole
    // representation exists for — so there is no colour hiding in it, and a
    // division by zero would manufacture one.
    if (a8 == 0) { return encoded; }

    const float inv_a = 255.0f / static_cast<float>(a8);
    const linear_rgb c = to_linear(encoded);
    return encode_keeping_alpha({c.r * inv_a, c.g * inv_a, c.b * inv_a},
                                encoded, encode_mode::exact);
}

} // namespace engine
