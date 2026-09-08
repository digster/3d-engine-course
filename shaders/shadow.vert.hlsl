// shaders/shadow.vert.hlsl — the depth pass, and it is four lines.
//
// Lesson 6.8. This is the shortest shader in the course and the reason is worth
// stating rather than admiring: a shadow map records WHERE SURFACES ARE, and
// nothing about a surface's colour, normal, material, texture or lighting can
// change where it is. Every one of `scene.vert.hlsl`'s other outputs — the world
// position, the normal, the uv, the tangent — exists to serve a fragment stage
// that this pass does not have.
//
// SAME BUFFER, DIFFERENT LAYOUT. The mesh being drawn is the identical
// `gpu_vertex_pnu`, 48 bytes with position, normal, uv and tangent interleaved.
// This pipeline declares ONE attribute out of the four, at offset 0, and the
// hardware simply never fetches the rest. Lesson 6.7 established that a vertex
// layout is per-PIPELINE state rather than a property of the buffer
// (`gpu_mesh::describe`'s `with_tangent` parameter); this is the same fact taken
// to its limit, and it is why a depth pass is cheap on bandwidth as well as on
// arithmetic.

// ---- Per FRAME -------------------------------------------------------------
//
// The LIGHT's clip_from_world, in the slot the camera's usually occupies. That
// is the whole of "render from the light": one matrix changes and the rest of
// the pipeline does not notice.
cbuffer LightCamera : register(b0, space1)
{
    float4x4 light_clip_from_world;
};

// ---- Per DRAW --------------------------------------------------------------
//
// Only the model matrix. The normal matrix that `scene.vert.hlsl` also pushes at
// this slot is absent here, and its absence is not a saving worth counting — the
// point is that the block is a CONTRACT between a shader and the code that fills
// it, and a shader that declares fields it never reads invites somebody to fill
// them.
cbuffer Object : register(b1, space1)
{
    float4x4 world_from_model;
};

struct Input
{
    float3 position : TEXCOORD0;
};

struct Output
{
    // NO VARYINGS AT ALL. `SV_Position` is not a varying — it is the value the
    // rasterizer needs in order to rasterize — and a depth-only pass wants
    // nothing else interpolated. This is what `num_color_targets = 0` looks like
    // from the vertex stage's side.
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;

    // Model -> world -> light clip. Two multiplies, and the second one is the
    // only line in this file that differs from the camera's vertex shader.
    // `1.0f` is Lesson 2.7's fourth coordinate for a POSITION.
    const float4 world = mul(world_from_model, float4(input.position, 1.0f));
    output.position = mul(light_clip_from_world, world);
    return output;
}
