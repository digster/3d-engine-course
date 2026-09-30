// shaders/particle.frag.hlsl — a soft disc of light, added to what is there.
//
// Lesson 6.18b. The quad is square and a spark is round, so the fragment shader
// shapes it: full brightness at the centre, falling to zero at the rim on a
// smooth square so the edge has no visible ring. The pipeline ADDS the result to
// the HDR target (pipeline_desc::blend_add, 6.13), which is what light does and
// what makes the draw order irrelevant — see particles_compact.comp.hlsl.
//
// ALPHA IS ZERO. The target's alpha channel means nothing to this engine's
// resolve, and an additive blend that wrote 1 into it for every covering
// particle would leave a value no later pass expects. Zero leaves it as the
// scene pass left it.

struct Input
{
    float2 corner : TEXCOORD0;
    float3 colour : TEXCOORD1;
};

float4 main(Input input) : SV_Target0
{
    const float r2 = dot(input.corner, input.corner);
    const float a = saturate(1.0f - r2);
    return float4(input.colour * (a * a), 0.0f);
}
