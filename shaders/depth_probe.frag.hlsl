// shaders/depth_probe.frag.hlsl — a flat colour, so the survivor can be counted.
//
// Lesson 4.7. The depth experiment draws a FAR surface and then a NEAR one and
// asks which is left. That question only has an answer if the two are
// distinguishable in the readback, so each draw is given its own flat colour and
// the harness counts pixels.
//
// No lighting, no texture, no interpolation: anything that varied across the
// surface would make "which one is here" a judgement rather than a comparison.

cbuffer Ink : register(b0, space3)
{
    float4 colour;
};

// The uv arrives because this stage shares `depth_probe.vert.hlsl` with the
// texture probe, and a fragment stage must declare what the vertex stage sends.
// Unused here, deliberately: this experiment is about depth, and anything that
// varied across the surface would make "which surface is here" a judgement.
struct Input
{
    float2 uv : TEXCOORD0;
};

float4 main(Input input) : SV_Target0
{
    return colour;
}
