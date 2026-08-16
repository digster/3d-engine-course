// shaders/triangle.vert.hlsl — the vertex stage Lesson 4.4 will draw with.
//
// Lesson 4.3. Everything this file does, `collect_triangles` in main.cpp has been
// doing on the CPU since Lesson 2.8: take a vertex, produce a clip-space position
// and whatever varyings the fragment stage needs. The difference is where it runs.

// The input, one vertex at a time.
//
// SEMANTICS ARE NOT DECORATION. SDL_gpu.h states the rule: non system-value
// semantics are all assumed to be TEXCOORD, numbered from 0 upwards, because
// D3D12 matches vertex-buffer elements to shader inputs by semantic and SDL
// standardises the name so the matching is positional in practice. Number them
// out of order and the wrong attribute arrives, silently.
struct Input
{
    float3 position : TEXCOORD0;
    float4 colour   : TEXCOORD1;
};

// The output. `SV_Position` is a SYSTEM-VALUE semantic — it names the one output
// the rasterizer itself consumes, in clip space, before the perspective divide.
// It is the same float4 our vertex pipeline has been producing since 2.10.
struct Output
{
    float4 colour   : TEXCOORD0;
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;
    output.colour = input.colour;

    // No matrix yet: these vertices arrive already in clip space, because Lesson
    // 4.4 is about getting a triangle drawn and not about transforming it. The
    // w of 1 is what Lesson 2.7 spent a lesson on — a position, not a direction.
    output.position = float4(input.position, 1.0f);
    return output;
}
