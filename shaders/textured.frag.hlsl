// shaders/textured.frag.hlsl — a fragment stage with a texture and a uniform.
//
// Lesson 4.3. Between them these four resources are what `SDL_GPUShaderCreateInfo`
// is asking about when it wants num_samplers and num_uniform_buffers, and getting
// those counts wrong is the first thing SDL_gpu.h's own FAQ tells you to check.
// This file exists so that the counts are not all zero.

// Fragment-stage textures and samplers live in space2 — see textured.vert.hlsl.
// The sampler's index corresponds to the texture's: t0 pairs with s0, which is
// exactly the pairing `SDL_GPUTextureSamplerBinding` expresses, and exactly the
// pairing engine::texture + engine::sampler has expressed since Lesson 3.9.
Texture2D<float4> albedo        : register(t0, space2);
SamplerState      albedo_sampler : register(s0, space2);

// Fragment-stage uniform buffers live in space3.
cbuffer Tint : register(b0, space3)
{
    float4 tint;
};

struct Input
{
    float2 uv : TEXCOORD0;
};

float4 main(Input input) : SV_Target0
{
    // One line of HLSL for what src/gfx/texture.cpp spends a hundred on: wrap the
    // coordinate, find the four texels whose CENTRES surround it, weight them.
    // Lesson 3.9 wrote it by hand so that this line is a recognition rather than
    // a magic word — and the hardware does it for free, which is the point.
    return albedo.Sample(albedo_sampler, input.uv) * tint;
}
