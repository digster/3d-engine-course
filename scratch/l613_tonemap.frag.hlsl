// shaders/tonemap.frag.hlsl — the resolve: exposure, a curve, and the encode.
//
// Lesson 6.12. This is the GPU half of `engine::resolve`, and the two are
// deliberately the same three steps in the same order, because they have to
// agree: §G of the harness renders the same values through both and compares.
//
// ONE FULL-SCREEN PASS, READING A FLOAT TEXTURE, WRITING THE SWAPCHAIN. Which is
// the structural point of the whole lesson: tonemapping is not more fragment
// shader bolted onto the scene pass. It happens after the scene is FINISHED,
// over a target that holds the values the scene produced, which is what makes it
// possible to derive an exposure from the frame's own content (and, in 6.13, to
// put a bloom in front of it).
//
// THE CURVES ARE CHARACTER FOR CHARACTER `engine::apply_tonemap`'s. Two
// implementations of one curve that disagree are a bug that only shows up in a
// diff, which is the same argument `scene.frag.hlsl` makes about the sRGB
// transform and `k_inv_pi`.

Texture2D<float4> hdr_colour : register(t0, space2);
SamplerState hdr_sampler     : register(s0, space2);

// LESSON 6.13 — the second input, at HALF the first one's resolution, which is
// why it gets its own sampler rather than sharing `hdr_sampler`. That one is
// NEAREST because the resolve is 1:1; this fetch is a genuine magnification and a
// nearest tap would make every glow blocky at exactly the scale the pyramid
// worked to smooth away.
Texture2D<float4> bloom_colour : register(t1, space2);
SamplerState bloom_sampler     : register(s1, space2);

cbuffer Tonemap : register(b0, space3)
{
    float exposure;     //  0 — multiplied BEFORE the curve
    float white;        //  4 — the input that `reinhard_white` maps to exactly 1
    float op;           //  8 — 0 clamp, 1 reinhard, 2 reinhard-white, 3 aces
    float per_channel;  // 12 — 1 = curve each channel, 0 = curve the luminance

    float encode;       // 16 — 1 = apply the sRGB transform here; 0 = the target does it
    float bloom_intensity; // 20 — 6.13; zero when there is no bloom
    float pad_t1;       // 24
    float pad_t2;       // 28
};

// Rec. 709 luminance — the same three constants as `engine::luminance`, and the
// same warning: they are only meaningful applied to LINEAR light.
static const float3 k_luma = float3(0.2126f, 0.7152f, 0.0722f);

/// Narkowicz's 2015 fit to the ACES RRT + ODT. A FIT, not a model — see hdr.hpp.
float aces_fit(float x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float curve(float x)
{
    // A negative input is not a brightness (hdr.cpp says why: x/(1+x) at -2 is
    // +2, wrong in the BRIGHT direction), so it is clamped at zero first.
    x = max(x, 0.0f);

    // `op` crosses as a NUMBER and is compared against numbers, which is Lesson
    // 6.4's rule about enums crossing a boundary: reordering `engine::tonemap`
    // must not silently re-map the shader.
    if (op < 0.5f)  { return min(x, 1.0f); }                       // clamp
    if (op < 1.5f)  { return x / (1.0f + x); }                     // reinhard
    if (op < 2.5f)                                                 // reinhard-white
    {
        const float w2 = max(white * white, 1.0e-6f);
        return min(x * (1.0f + x / w2) / (1.0f + x), 1.0f);
    }
    return aces_fit(x);                                            // aces
}

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target0
{
    // A POINT SAMPLE, AND IT MUST BE. The resolve is 1:1 — one source texel per
    // destination pixel — so a linear filter would have nothing to interpolate
    // and would merely risk half-texel error if the two targets ever differ in
    // size. `gpu_tonemap_pass` creates a NEAREST sampler for this reason.
    const float3 hdr = hdr_colour.Sample(hdr_sampler, uv).rgb;

    // ONE: exposure, in linear light.
    float3 lit = hdr * exposure;

    // ONE AND A HALF (Lesson 6.13): the bloom, added while everything is still
    // linear light and BEFORE the curve.
    //
    // THE ORDER IS NOT A PREFERENCE. A bloom is light — it is the light that
    // scattered on its way to the sensor — so it has to be photographed by the
    // same curve as the light that did not scatter. Add it AFTER the curve and
    // the arithmetic says exactly what goes wrong: the curve's output is already
    // in [0, 1], so anything added lands above 1 and the encode clips it, giving
    // every glow a flat white core that grows with the source's brightness.
    // Which is the artefact bloom exists to remove, reintroduced one line later.
    //
    // NO `exposure` HERE. The bright pass already applied it, deliberately, so
    // that the threshold could be a claim about the finished image. Multiplying
    // again would photograph the glow at a different shutter speed from the
    // picture it sits on — the glow would grow quadratically while the image grew
    // linearly, which reads as "the bloom is too strong outdoors".
    //
    // UNCONDITIONAL, and `bloom_intensity` is zero when there is no bloom. A
    // branch here would buy a skipped texture fetch at the cost of divergence
    // across a whole wavefront (Lesson 4.1), which is the wrong trade for one
    // bilinear tap.
    lit += bloom_colour.Sample(bloom_sampler, uv).rgb * bloom_intensity;

    // TWO: the curve, still in linear light.
    float3 mapped;
    if (per_channel > 0.5f)
    {
        mapped = float3(curve(lit.r), curve(lit.g), curve(lit.b));
    }
    else
    {
        // Luminance-only: preserves hue exactly, and can therefore leave a
        // saturated colour with a channel above 1 for the saturate below to
        // clip. hdr.cpp's `apply_tonemap` has the worked case.
        const float lum = dot(lit, k_luma);
        mapped = (lum > 0.0f) ? lit * (curve(lum) / lum) : float3(0, 0, 0);
    }

    // THREE: the encode, last and exactly once.
    //
    // `encode` IS `encode_output`'S THIRD SITUATION, MOVED SOMEWHERE IT CAN BE
    // ANSWERED. In `scene.frag.hlsl` that flag had to mean "does the target I am
    // writing to apply the transfer function?", and once the scene renders to a
    // FLOAT target the honest answer there is "no, and it must not, because this
    // is not the final image". The question has not gone away — it has moved to
    // the pass that actually writes the display, which is the only place it was
    // ever answerable.
    if (encode > 0.5f)
    {
        const float3 c = saturate(mapped);
        const float3 low  = c * 12.92f;
        const float3 high = 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
        return float4(c <= 0.0031308f ? low : high, 1.0f);
    }
    return float4(saturate(mapped), 1.0f);
}
