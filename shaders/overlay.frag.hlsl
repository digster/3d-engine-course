// shaders/overlay.frag.hlsl — coverage times colour, in the right space.
//
// Lesson 6.18, and this eighteen-line shader is where the lesson's argument
// lands. Three facts have to be true at once and each is a category:
//
//   1. THE ATLAS HOLDS COVERAGE. One channel, `R8_UNORM`, never `_SRGB`. A
//      transfer function encodes a light intensity and coverage is an area
//      fraction, so a sampler that "decodes" this texture applies an inverse
//      gamma to a number that was never encoded. The result is not garbage,
//      which is the dangerous part — it is text about 15% too thin.
//   2. THE VERTEX COLOUR IS ENCODED. A UI colour is authored the way every UI
//      colour is authored, as sRGB bytes (`#FFCC00`), and `UBYTE4_NORM` hands
//      it over as 0..1 without decoding. Multiplying that by anything, or
//      blending with it, requires decoding first — this is `mix_encoded`'s
//      error (1.6) waiting in a fourth place.
//   3. THE OUTPUT MUST MATCH WHAT THE BLENDER EXPECTS. On an `_SRGB` target the
//      ROP decodes the destination, lerps light and re-encodes, so we hand it
//      LINEAR. On a plain UNORM target with `encode = 1` we hand it a code, the
//      ROP lerps codes, and the result is wrong by 43% of the light at half
//      coverage — a fact this shader cannot fix and should not hide.

Texture2D<float>  glyph_atlas   : register(t0, space2);
SamplerState      glyph_sampler : register(s0, space2);

cbuffer Overlay : register(b0, space3)
{
    /// 1 = this shader applies the sRGB transfer function, because the target is
    /// a plain UNORM format. 0 = the target is `_SRGB` and the hardware does it.
    /// Same flag, same meaning and same caveat as `scene.frag.hlsl`'s
    /// `encode_output`: the value 1 is the fallback, and it is wrong for blended
    /// geometry in a way no shader can repair.
    float encode;        //  0
    /// Stem darkening, `k` in `coverage^(1-k)`. Zero disables it, and zero is
    /// the default — see overlay.hpp for why a correction that helps light text
    /// on dark hurts dark text on light by the same amount.
    float stem_darken;   //  4
    float pad_o1;        //  8
    float pad_o2;        // 12
};

/// The exact piecewise sRGB decode — the same constants as `engine::srgb_to_linear`.
/// Never `pow(x, 2.2)`: that misses the linear toe and is visibly wrong in the
/// darkest codes, which for text is every antialiased edge pixel.
float3 srgb_to_linear(float3 c)
{
    const float3 low  = c / 12.92f;
    const float3 high = pow((c + 0.055f) / 1.055f, 2.4f);
    return c <= 0.04045f ? low : high;
}

float3 linear_to_srgb(float3 c)
{
    const float3 s = saturate(c);
    const float3 low  = s * 12.92f;
    const float3 high = 1.055f * pow(s, 1.0f / 2.4f) - 0.055f;
    return s <= 0.0031308f ? low : high;
}

struct PSInput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 colour   : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target
{
    // ONE CHANNEL. `Texture2D<float>` on an R8_UNORM texture returns the red
    // channel directly, with no swizzle and no three-quarters of a fetch thrown
    // away — which is the payoff for the atlas being one byte per texel.
    float coverage = glyph_atlas.Sample(glyph_sampler, input.uv).r;

    if (stem_darken > 0.0f)
    {
        coverage = pow(coverage, 1.0f - stem_darken);
    }

    // Decode the authored colour, then scale coverage by the vertex alpha. Note
    // which of the two is decoded and which is not: the COLOUR is light and goes
    // through the curve, the ALPHA is coverage and does not. That asymmetry is
    // `blend.hpp`'s whole thesis in two lines.
    const float3 tint = srgb_to_linear(input.colour.rgb);
    const float alpha = coverage * input.colour.a;

    if (encode > 0.5f)
    {
        // The fallback. Correct for an opaque draw, wrong for this one, and
        // said out loud rather than papered over: the ROP is about to lerp
        // these codes.
        return float4(linear_to_srgb(tint), alpha);
    }
    return float4(tint, alpha);
}
