// shaders/scene_instanced.vert.hlsl — scene.vert.hlsl with the per-object data arriving by the other road.
//
// Lesson 6.16. This file and `scene.vert.hlsl` compute the SAME FOUR OUTPUTS from
// the same inputs; they differ in where `world_from_model` and the normal matrix
// come from. In `scene.vert.hlsl` they are a constant buffer pushed once per
// draw. Here they are seven vertex attributes fetched once per INSTANCE.
//
// ---------------------------------------------------------------------------
// WHAT IS NOT HERE, AND WHY THAT IS THE POINT
// ---------------------------------------------------------------------------
//
// There is no instanced fragment shader. `scene.frag.hlsl` is bound unchanged
// beside this one, because instancing is a question about where a vertex's
// transform comes from and the fragment stage never asked. Everything Module 6
// built — the Cook-Torrance BRDF (6.4), normal mapping (6.7), cascaded shadows
// (6.9), the alpha modes (6.11), image-based lighting (6.15) — arrives here for
// free and had to be neither ported nor duplicated.
//
// That is worth pausing on, because the pipeline COUNT says the opposite: a
// tenth pipeline object had to be created (gpu_scene.hpp's `k_styles * k_blends`
// is nine). A pipeline is the pair, so a new vertex shader is a new pipeline
// even when the fragment half is byte-identical.
//
// ---------------------------------------------------------------------------
// A 4x4 IS FOUR ATTRIBUTES
// ---------------------------------------------------------------------------
//
// A vertex attribute carries at most four components in every backend SDL_GPU
// targets, so a matrix cannot be one. It arrives as four `float4` at consecutive
// locations and is reassembled below. WHICH WAY ROUND matters and is the one
// thing in this file that can be wrong without an error: the C++ side writes a
// `mat4` whose storage is COLUMN-MAJOR (conventions §3, and `engine::mat4`'s
// members are literally named `c0`..`c3`), so attribute 4 is column 0. HLSL's
// `float4x4` constructor takes ROWS, so the reassembly below transposes
// deliberately — see the comment at the `mul`.

cbuffer Camera : register(b0, space1)
{
    float4x4 clip_from_world;   // projection * view, built on the CPU once a frame
};

// NO `cbuffer Object`. That is the entire difference, and register b1 in space1
// is simply left unbound for this pipeline — a cbuffer nothing declares is a
// cbuffer nothing reads, unlike a SAMPLER slot, where an unbound declaration is
// undefined behaviour (gpu_scene.hpp has the receipts for that asymmetry).

struct Input
{
    // ---- Per VERTEX, slot 0 — `engine::gpu_vertex_pnu`, 48 bytes ------------
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 tangent  : TEXCOORD3;

    // ---- Per INSTANCE, slot 1 — `engine::gpu_instance`, 112 bytes ----------
    //
    // Semantics continue the TEXCOORDn run because SDL_shadercross maps every
    // non-system-value semantic to a numbered TEXCOORD and the number IS the
    // location (state block: `shader-semantics`). There is no separate
    // "instance" semantic and there does not need to be: the input RATE is a
    // property of the buffer description on the C++ side, not of the shader.
    // The shader cannot tell, and that is the correct separation.
    float4 world_c0 : TEXCOORD4;
    float4 world_c1 : TEXCOORD5;
    float4 world_c2 : TEXCOORD6;
    float4 world_c3 : TEXCOORD7;
    float4 normal_c0 : TEXCOORD8;
    float4 normal_c1 : TEXCOORD9;
    float4 normal_c2 : TEXCOORD10;
};

struct Output
{
    // IDENTICAL to scene.vert.hlsl's Output, field for field and semantic for
    // semantic. It has to be: `scene.frag.hlsl` is the consumer and it is not
    // being recompiled. A mismatch here is a link-time error in the pipeline
    // creation, which is the good outcome — SDL validates the stage interface.
    float3 world    : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 tangent  : TEXCOORD3;
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;

    // ---- Reassemble the model matrix ---------------------------------------
    //
    // `float4x4(a, b, c, d)` builds a matrix from four ROWS. Our four attributes
    // are COLUMNS. So the constructor produces the TRANSPOSE of what we want, and
    // the honest fix is to say so and transpose it back rather than to quietly
    // reorder the attributes until the picture looks right — which works, and
    // leaves the next reader unable to tell a convention from a coincidence.
    //
    // `transpose` is free: the compiler folds it into the `mul` that follows,
    // because `mul(transpose(M), v)` is `mul(v, M)` and both forms are one
    // instruction per row on every backend.
    const float4x4 world_from_model =
        transpose(float4x4(input.world_c0, input.world_c1, input.world_c2, input.world_c3));

    const float4 world = mul(world_from_model, float4(input.position, 1.0f));
    output.world = world.xyz;

    // ---- The normal, by its inverse transpose ------------------------------
    //
    // The same three multiply-adds `scene.vert.hlsl` performs, reading the same
    // three columns. `engine::instance_of` produces them with the same unpack
    // `gpu_scene_renderer::render` uses for the uniform road, in one function, so
    // the two cannot disagree about column order — which would light instanced
    // objects differently from ordinary ones and read as a shader bug.
    output.normal = input.normal_c0.xyz * input.normal.x
                  + input.normal_c1.xyz * input.normal.y
                  + input.normal_c2.xyz * input.normal.z;

    output.uv = input.uv;

    // The tangent rides the MODEL matrix, not the normal matrix — Lesson 6.7's
    // asymmetry, unchanged by how the matrix arrived.
    output.tangent.xyz = mul(world_from_model, float4(input.tangent.xyz, 0.0f)).xyz;
    output.tangent.w = input.tangent.w;

    output.position = mul(clip_from_world, world);
    return output;
}
