// shaders/bloom_down.frag.hlsl — halve, with one tap.
//
// Lesson 6.13, stage two. The shortest shader in this course, and the reason it
// is this short is the identity the lesson's §3.5 derives:
//
//   A BILINEAR SAMPLE PLACED AT THE CORNER SHARED BY FOUR TEXELS RETURNS THEIR
//   UNWEIGHTED MEAN, because the two fractional weights are both exactly 1/2 and
//   the four products are all 1/4.
//
// Mapping a half-size destination's texel centres through `uv` puts every sample
// on exactly such a corner. So a 2x2 box downsample — which reads like four
// fetches and an add — is ONE fetch, performed by the texture unit's filtering
// hardware, which was going to run anyway.
//
// `verify_613` §C checks it on real numbers rather than asserting it, because an
// identity that depends on the half-texel convention is exactly the kind that is
// true in the derivation and false in the code.
//
// WHAT THIS DELIBERATELY IS NOT: Karis's 13-tap downsample, which shipping
// engines use to suppress the flickering a 2x2 box lets through. We suppress it
// with an explicit clamp in the bright pass instead — a named knob with a
// measured cost (bloom.hpp's `clamp_max`) rather than a wider kernel that hides
// the problem inside a filter. Exercise 3 builds the 13-tap and compares.

Texture2D<float4> src_colour : register(t0, space2);
SamplerState src_sampler     : register(s0, space2);

cbuffer Downsample : register(b0, space3)
{
    float2 texel_size;   //  0 — 1 / SOURCE dimensions, in texture units
    float  pad_d0;       //  8
    float  pad_d1;       // 12
};

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target0
{
    // `(2x + 1, 2y + 1)` in SOURCE texels — the corner of the 2x2 block. See
    // bloom_bright.frag.hlsl for why this is computed from `SV_Position` rather
    // than taken from the interpolated `uv`: integer halving makes odd levels
    // not-quite-half, and only the explicit form stays exact there.
    const float2 src_texel = floor(position.xy) * 2.0f + 1.0f;
    return float4(src_colour.Sample(src_sampler, src_texel * texel_size).rgb, 1.0f);
}
