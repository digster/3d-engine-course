// shaders/mesh.vert.hlsl — a real mesh, placed by per-instance data, seen by a
// camera that can finally move.
//
// Lesson 4.5 built the input struct below and noticed what it does NOT say: which
// of these six attributes arrive once per vertex and which once per instance. That
// distinction lives entirely in `input_rate` on the C++ side, and instancing costs
// one enum value rather than a new shader stage.
//
// LESSON 4.6 ADDS THE THIRD RATE. `position` changes per vertex; `placement`
// changes per instance; `clip_from_world` changes per FRAME, and is the same for
// every vertex of every instance. It cannot go in a vertex buffer without being
// written 1,225 times per instance, so it arrives another way entirely — pushed
// onto the command buffer, where it is not a resource at all.
//
// What this file lost in the process is thirty lines of `static const`: an eye
// position, a pitch, a focal length, an aspect ratio and a depth remap, all frozen
// at compile time because Lesson 4.5 had no way to send them. The projection they
// spelled out is still exactly the matrix on the Conventions page — it is simply
// built on the CPU now, once per frame, by `engine::perspective` and
// `engine::look_at`, which have existed since Lessons 2.9 and 2.10.

// THE SPACE IS FIXED BY SDL, PER STAGE, AND IS NOT A CHOICE (Lesson 4.3):
// vertex-stage uniform buffers live in space1, fragment-stage ones in space3.
// Put this in space0 and it compiles, links, loads and runs, reading whatever
// happens to be bound at the slot named. `verify_46` §D measures what that is.
cbuffer Camera : register(b0, space1)
{
    // projection * view, built once per frame on the CPU.
    //
    // `mul(M, v)` treats v as a COLUMN vector — v' = M*v, the convention this
    // engine has used since Lesson 2.5 — and our `mat4` stores its four columns
    // contiguously. Lesson 4.6 §3.4 measures all sixteen floats through this
    // boundary and finds them where our storage put them: no transpose, no
    // reordering, `memcpy` and nothing else.
    float4x4 clip_from_world;
};

struct Input
{
    // ---- Per vertex, from buffer slot 0 (gpu_vertex_pnu, 32 bytes) ----------
    float3 position  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float2 uv        : TEXCOORD2;

    // ---- Per instance, from buffer slot 1 (gpu_instance, 28 bytes) ----------
    float4 placement : TEXCOORD3;   // xyz = world offset, w = uniform scale
    float2 spin      : TEXCOORD4;   // (cos, sin) of a rotation about the x axis
    float4 tint      : TEXCOORD5;   // stored as four BYTES; arrives as four floats
};

struct Output
{
    float3 normal   : TEXCOORD0;
    float2 uv       : TEXCOORD1;
    float4 tint     : TEXCOORD2;
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;

    // ---- Model -> world -----------------------------------------------------
    //
    // Scale, then rotate, then translate — Lesson 2.8's order, for Lesson 2.8's
    // reasons. The rotation is about x and matches `engine::rotation_x` exactly:
    // that matrix sends y to (0, c, s) and z to (0, -s, c), so
    //     y' = c*y - s*z      z' = s*y + c*z
    const float c = input.spin.x;
    const float s = input.spin.y;

    const float3 scaled = input.position * input.placement.w;
    const float3 spun = float3(scaled.x,
                               c * scaled.y - s * scaled.z,
                               s * scaled.y + c * scaled.z);
    const float3 world = spun + input.placement.xyz;

    // The normal takes the ROTATION and nothing else. It is a direction, so the
    // translation must not touch it (Lesson 2.7's w = 0), and the scale is
    // UNIFORM, which is the only reason we may reuse the rotation as-is — Lesson
    // 3.6 showed that a non-uniform scale needs the inverse transpose instead.
    output.normal = float3(input.normal.x,
                           c * input.normal.y - s * input.normal.z,
                           s * input.normal.y + c * input.normal.z);

    // ---- World -> clip ------------------------------------------------------
    //
    // One matrix multiply, where Lesson 4.5 had eleven lines of hand-written
    // arithmetic. The `1.0f` is Lesson 2.7's fourth coordinate: a POSITION, so the
    // matrix's fourth column is added in full and the camera's translation
    // actually happens. Write `0.0f` and the scene ignores where the camera is.
    output.position = mul(clip_from_world, float4(world, 1.0f));

    output.uv = input.uv;
    output.tint = input.tint;
    return output;
}
