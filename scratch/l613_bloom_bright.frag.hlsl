// shaders/bloom_bright.frag.hlsl — exposure, a clamp, and the soft knee.
//
// Lesson 6.13, stage one of three. This is the only pass in the bloom that
// SELECTS; everything after it is a filter that treats what it is given as light.
//
// IT ALSO HALVES. The destination is pyramid level 0, half the scene's size, so
// this pass replaces what would otherwise be a separate first downsample — one
// full-resolution read of the scene buffer for the whole chain instead of two.
//
// THE ARITHMETIC IS `engine::bright_pass`'s, LINE FOR LINE, and it has to be:
// §G of the harness runs the same values through both and compares the results to
// the code. The order in particular is load-bearing —
//
//     average  ->  exposure  ->  clamp  ->  threshold on LUMINANCE  ->  scale
//
// — because averaging before selecting dilutes a lone bright texel among its
// three neighbours, and testing the luminance rather than the channels keeps the
// hue of what survives. bloom.hpp argues both at length.

Texture2D<float4> scene_colour : register(t0, space2);
SamplerState scene_sampler     : register(s0, space2);

cbuffer Bright : register(b0, space3)
{
    float2 texel_size;   //  0 — 1 / SOURCE dimensions, in texture units
    float  exposure;     //  8 — the SAME number the resolve will use
    float  threshold;    // 12 — in exposure-corrected linear light

    float  knee;         // 16 — half-width of the soft transition
    float  clamp_max;    // 20 — the firefly ceiling; <= 0 disables
    float  pad_b0;       // 24
    float  pad_b1;       // 28
};

static const float3 k_luma = float3(0.2126f, 0.7152f, 0.0722f);

/// The soft knee — `engine::bright_pass_weight`, derived in the lesson's §3.6.
float bright_weight(float luma)
{
    if (!(luma > 0.0f)) { return 0.0f; }
    if (knee <= 0.0f)   { return max(luma - threshold, 0.0f); }
    if (luma <= threshold - knee) { return 0.0f; }
    if (luma >= threshold + knee) { return luma - threshold; }

    const float t = luma - threshold + knee;
    return t * t / (4.0f * knee);
}

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target0
{
    // ONE BILINEAR TAP IS THE 2x2 AVERAGE. The tap goes at `(2x + 1, 2y + 1)` in
    // SOURCE texels, which is exactly the corner shared by the four texels of the
    // block beneath destination texel (x, y) — and there all four bilinear weights
    // are 1/2 x 1/2 = 1/4. So this single fetch is the block's mean, computed by
    // the texture unit for free. That identity is why this pass's sampler is
    // LINEAR where the 6.12 resolve's is deliberately NEAREST: here the filter
    // mode is doing arithmetic, not smoothing.
    //
    // ADDRESSED BY TEXEL INDEX RATHER THAN BY THE INTERPOLATED `uv`, and the
    // difference is not cosmetic. `uv * source_size` equals `2x + 1` only when the
    // source is EXACTLY twice the destination, and integer halving breaks that on
    // every odd level — 135 halves to 67, and 67 doubles to 134. Reading
    // `SV_Position` makes the relationship between the two grids explicit and
    // exact at every level, and makes this pass agree with `engine::bright_pass`
    // to the last bit rather than to the last even dimension.
    const float2 src_texel = floor(position.xy) * 2.0f + 1.0f;
    float3 c = scene_colour.Sample(scene_sampler, src_texel * texel_size).rgb;

    // Exposure FIRST, because the threshold is a claim about the finished image.
    c *= exposure;

    // The firefly ceiling, per channel — a blown-out single channel is exactly
    // what a specular highlight on a coloured metal produces.
    if (clamp_max > 0.0f) { c = min(c, clamp_max.xxx); }

    // Select on luminance, scale the colour: the hue of the glow is the hue of
    // what produced it.
    const float lum = dot(c, k_luma);
    const float kept = bright_weight(lum);
    const float scale = (lum > 0.0f) ? kept / lum : 0.0f;

    return float4(c * scale, 1.0f);
}
