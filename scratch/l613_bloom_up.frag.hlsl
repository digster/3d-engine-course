// shaders/bloom_up.frag.hlsl — double, with a 1-2-1 tent, and ADD.
//
// Lesson 6.13, stage three, and the one that makes the pyramid a bloom rather
// than a mip chain.
//
// THE TENT IS DERIVED, NOT CHOSEN. A bilinear upsample IS a tent filter, because
// a box convolved with a box is a triangle — discretely `[1 1] * [1 1] = [1 2 1]`
// (the lesson's §3.4 does the convolution by hand, and `verify_613` §C does it in
// code). The 3x3 kernel is that vector's outer product with itself over 16, which
// sums to exactly 1: the filter widens without brightening or darkening.
//
// AND IT ADDS RATHER THAN REPLACES, which is the whole architecture of the thing.
// Each level is added into the level below it on the way back up, so level 0 ends
// up holding the SUM of every level's contribution — a sum of Gaussians whose
// widths double. That sum is what produces the measured `r^-2` envelope, and a
// single Gaussian cannot produce it at any width (bloom.hpp has the 403x-against-
// 6.0x measurement over one octave).
//
// THE ADD IS THE HARDWARE'S, not this shader's: the pipeline is created with an
// additive blend (`pipeline_desc::blend_add`, Lesson 6.13's one addition to
// 6.11's blending), so the output below is the SOURCE term of `src*1 + dst*1`.
// Doing it in the shader instead would mean reading the destination texture while
// rendering into it, which is a read-after-write hazard with no defined ordering
// — the reason a post-processing chain ping-pongs between targets in general, and
// the reason it does not have to here.

Texture2D<float4> src_colour : register(t0, space2);
SamplerState src_sampler     : register(s0, space2);

cbuffer Upsample : register(b0, space3)
{
    float2 texel_size;   //  0 — 1 / SOURCE dimensions, in texture units
    float  radius;       //  8 — tap spacing, in SOURCE texels
    float  pad_u0;       // 12
};

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target0
{
    // The nine taps of the tent, written out. `texel_size * radius` converts the
    // integer offsets into texture units against the SOURCE's dimensions — which
    // is why `texel_size` is the source's and not the destination's, and is the
    // single most common way to get this pass subtly wrong (the symptom is a
    // bloom that is too tight at the top of the pyramid and too loose at the
    // bottom, because the error scales with the level).
    const float2 d = texel_size * radius;

    // THE DESTINATION TEXEL'S CENTRE, IN SOURCE TEXELS: `(x + 0.5) * 0.5`. That is
    // the exact inverse of the downsample's `2x + 1` — the same mapping between
    // the two grids, read in the other direction — which is what keeps the
    // pyramid from drifting as it is walked down and back up. Computed from
    // `SV_Position` for the reason bloom_bright.frag.hlsl gives.
    const float2 uv_src = (floor(position.xy) + 0.5f) * 0.5f * texel_size;

    float3 acc = float3(0.0f, 0.0f, 0.0f);

    // Corners: weight 1 each.
    acc += src_colour.Sample(src_sampler, uv_src + float2(-d.x, -d.y)).rgb * 1.0f;
    acc += src_colour.Sample(src_sampler, uv_src + float2( d.x, -d.y)).rgb * 1.0f;
    acc += src_colour.Sample(src_sampler, uv_src + float2(-d.x,  d.y)).rgb * 1.0f;
    acc += src_colour.Sample(src_sampler, uv_src + float2( d.x,  d.y)).rgb * 1.0f;

    // Edges: weight 2 each.
    acc += src_colour.Sample(src_sampler, uv_src + float2( 0.0f, -d.y)).rgb * 2.0f;
    acc += src_colour.Sample(src_sampler, uv_src + float2(-d.x,  0.0f)).rgb * 2.0f;
    acc += src_colour.Sample(src_sampler, uv_src + float2( d.x,  0.0f)).rgb * 2.0f;
    acc += src_colour.Sample(src_sampler, uv_src + float2( 0.0f,  d.y)).rgb * 2.0f;

    // Centre: weight 4. Total 1+1+1+1 + 2+2+2+2 + 4 = 16.
    acc += src_colour.Sample(src_sampler, uv_src).rgb * 4.0f;

    // ALPHA IS 1, NOT 0, AND IT DOES NOT MATTER — which is worth one sentence
    // rather than none. The additive blend this pass uses is `ONE, ONE` on colour
    // and `ONE, ONE` on alpha, so the alpha accumulates too; nothing downstream
    // reads it (6.11 established that a colour target's alpha has no consumer in
    // this engine), and on a float target it cannot overflow into anything.
    return float4(acc * (1.0f / 16.0f), 1.0f);
}
