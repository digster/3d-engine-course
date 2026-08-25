// shaders/scene.vert.hlsl — Module 3's vertex stage, as a vertex stage.
//
// Lesson 4.8. There has been a vertex stage in this engine since Lesson 2.8; it
// was a `for` loop at the top of `collect_triangles` in src/main.cpp, and it did
// exactly what the twenty lines below do. Reading the two side by side is the
// point of this lesson, so the correspondence is spelled out rather than left to
// be noticed:
//
//     collect_triangles                        this file
//     ------------------------------------     -------------------------------
//     world_from_model = model_matrix(...)     pushed, per draw (b1, space1)
//     to_world_normal  = normal_matrix(...)    pushed, per draw, as 3 columns
//     world_pos = world_from_model * point(v)  mul(world_from_model, float4(p,1))
//     view_pos  = view_from_world * ...        \  composed on the CPU into ONE
//     clip_pos  = proj * point(view_pos)       /  matrix, per FRAME (b0, space1)
//     world_normal = to_world_normal * n       c0*n.x + c1*n.y + c2*n.z
//
// The one structural difference is the middle pair. The CPU took world -> view ->
// clip as two hops because it wanted the view position for nothing in particular
// and had already paid for it; here they are one matrix, multiplied once per
// frame on the CPU rather than twice per vertex on the GPU. That is not a
// cleverness — it is the same `projection * view` that Lesson 4.6 introduced, and
// it is the only line of arithmetic in the whole port that MOVED.
//
// WHAT DID NOT CHANGE: the composition order (T*R*S, Lesson 2.8), the fourth
// coordinate's value for a position (1) and for a direction (0) (Lesson 2.7), the
// inverse transpose for normals (Lesson 3.6), and the handedness and winding
// (conventions §4, §7). Modules 2 and 3 fixed all of those deliberately so that
// this file could be a translation instead of a redesign.

// ---- Per FRAME -------------------------------------------------------------
//
// Unchanged from Lesson 4.6, deliberately: the camera block is the same struct,
// at the same slot, in the same space. A port that had to renegotiate the camera
// would be evidence that 4.6 got it wrong.
cbuffer Camera : register(b0, space1)
{
    float4x4 clip_from_world;   // projection * view, built on the CPU once a frame
};

// ---- Per DRAW --------------------------------------------------------------
//
// The block Lesson 4.8 adds. Pushed between draws, which SDL's header licenses in
// one sentence: "Subsequent draw calls in this command buffer will use this
// uniform data."
//
// The normal matrix arrives as THREE COLUMNS rather than a `float3x3`, and
// gpu_uniform.hpp gives the reason at length: a 3x3 in a constant buffer occupies
// three padded registers whichever way you spell it, so the only thing the matrix
// type would add is a dependence on the compiler's matrix-packing default. A
// silently transposed normal matrix is a lighting bug that looks like a modelling
// bug, and it is not worth twelve saved bytes we do not save anyway.
cbuffer Object : register(b1, space1)
{
    float4x4 world_from_model;   // T*R*S — Lesson 2.8's composition
    float4 normal_c0;            // column 0 of the inverse transpose; .w unused
    float4 normal_c1;
    float4 normal_c2;
};

struct Input
{
    // The SAME layout Lesson 4.5 built — `engine::gpu_vertex_pnu`, 32 bytes,
    // position/normal/uv interleaved. The scene's meshes go through exactly the
    // same `interleave()` the torus did, which is the first piece of evidence
    // that this is a port and not a rewrite.
    float3 position : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
};

struct Output
{
    // THREE VARYINGS AND A POSITION, and the first one is new since Lesson 4.7.
    // `world` exists because the shading is view-dependent (Lesson 3.7): a
    // highlight needs the direction from the surface to the eye, and that needs
    // the surface's world position. On the CPU that cost one extra matrix
    // multiply per vertex; here it costs three interpolated floats per FRAGMENT,
    // for every fragment, forever. Minimising varyings is a real optimisation for
    // exactly this reason.
    float3 world    : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 position : SV_Position;
};

Output main(Input input)
{
    Output output;

    // ---- Model -> world -----------------------------------------------------
    //
    // One multiply, against Lesson 4.5's eleven hand-written lines and 4.6's six.
    // The `1.0f` is Lesson 2.7's fourth coordinate: this is a POSITION, so the
    // matrix's fourth column — the translation — is added in full.
    const float4 world = mul(world_from_model, float4(input.position, 1.0f));
    output.world = world.xyz;

    // ---- The normal, which is not carried by that matrix --------------------
    //
    // `M * v` written out as its definition: the columns of M, weighted by the
    // components of v. Lesson 2.5 said a matrix IS where the basis vectors land;
    // this is that sentence as three multiply-adds.
    //
    // The matrix is the INVERSE TRANSPOSE (Lesson 3.6), computed on the CPU by
    // `engine::normal_matrix`. Feed the model matrix here instead and every
    // non-uniformly scaled object is lit wrongly while every uniformly scaled one
    // looks perfect — which is why the bug survives in real codebases, and why
    // the demo keeps it on a key.
    //
    // NOT normalised here. It is about to be interpolated across the triangle,
    // which shortens it again (Lesson 3.8 measured that), so the fragment stage
    // normalises and this stage would be paying for a result nothing keeps.
    output.normal = normal_c0.xyz * input.normal.x
                  + normal_c1.xyz * input.normal.y
                  + normal_c2.xyz * input.normal.z;

    output.uv = input.uv;

    // ---- World -> clip ------------------------------------------------------
    //
    // The view and the projection, pre-multiplied on the CPU. Lesson 2.9 built
    // the first, Lesson 2.10 derived the second, and Lesson 4.6 established that
    // both cross this boundary as sixteen floats with no transpose.
    output.position = mul(clip_from_world, world);
    return output;
}
