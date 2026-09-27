// shaders/textured.vert.hlsl — a vertex stage with a resource, for Lesson 4.6.
//
// Lesson 4.3 compiles it but nothing draws with it yet. It exists here because a
// shader with no resources at all cannot demonstrate the one thing that most
// often breaks a working shader: the register spaces SDL_GPU requires.

// THE SPACE IS NOT OPTIONAL AND NOT ARBITRARY. SDL_gpu.h fixes it, per stage:
//
//   vertex   : t/s registers in space0, uniform buffers in space1
//   fragment : t/s registers in space2, uniform buffers in space3
//
// Put this cbuffer in space0 and it compiles, links, runs, and reads whatever
// happens to be bound at the wrong slot. Nothing reports an error.
cbuffer Camera : register(b0, space1)
{
    float4x4 clip_from_model;
};

struct Input
{
    float3 position : TEXCOORD0;
    float2 uv       : TEXCOORD1;
};

struct Output
{
    float2 uv       : TEXCOORD0;
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.uv = input.uv;

    // `mul(M, v)` treats v as a COLUMN vector, which is the convention this
    // engine has used since Lesson 2.5 — v' = M*v, and A*B means B first. Our
    // mat4 is column-major in memory and HLSL packs cbuffer matrices
    // column-major by default, so the bytes go across untouched. Lesson 4.6
    // pays that off properly.
    output.position = mul(clip_from_model, float4(input.position, 1.0f));
    return output;
}
