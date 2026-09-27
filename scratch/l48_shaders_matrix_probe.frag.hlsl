// shaders/matrix_probe.frag.hlsl — how a 3x3 crosses the uniform boundary.
//
// Lesson 4.8's instrument, and it exists to answer one question the shipping code
// deliberately avoids having to trust: **if you declare a `float3x3` in a constant
// buffer and push three padded columns of our `mat3` at it, do you get the matrix
// or its transpose?**
//
// The question is not academic. `gpu_scene.cpp` pushes the normal matrix as three
// explicit `float4` and the vertex shader rebuilds `M*v` by hand, which cannot be
// wrong either way. Writing `float3x3` instead would be shorter, would occupy the
// same 48 bytes, and would depend on the compiler's matrix-packing default —
// `column_major` for DXC, `row_major` if a `#pragma pack_matrix` anywhere upstream
// says so, and there is no diagnostic when it is not what you assumed. A
// transposed normal matrix does not crash; it lights the scene from a slightly
// wrong direction on every non-symmetric object, which is a bug people spend days
// on.
//
// So: push a matrix whose transpose is unmistakably different, multiply a known
// vector by it in both spellings, and read both answers back. Whatever the answer
// is, it is then measured rather than believed.
//
// Shares `depth_probe.vert` (Lesson 4.7) for its full-target triangle, called with
// params = (1, 0, 0, 0) so the surface sits at z_ndc = 0 and fills the target.

cbuffer Probe : register(b0, space3)
{
    // Register 0-2: the SAME 48 bytes, read two ways. The C++ side pushes three
    // padded columns of an `engine::mat3` — column 0 in register 0, and so on.
    float3x3 as_matrix;

    // Register 3: the vector to multiply, and which answer to return.
    //   w = 0  ->  mul(as_matrix, v)      — the matrix-typed reading
    //   w = 1  ->  c0*v.x + c1*v.y + c2*v.z, rebuilt from the same bytes read as
    //              three separate columns, which is what scene.vert.hlsl does
    float4 v_and_mode;
};

// The same bytes again, as three loose columns. Declaring them in a SECOND
// constant buffer at a second slot, rather than after `as_matrix` in this one, is
// the only way to have both readings of one 48-byte block — a field can only be
// declared once at a given offset.
cbuffer ProbeColumns : register(b1, space3)
{
    float4 c0;
    float4 c1;
    float4 c2;
};

struct Input
{
    float2 uv : TEXCOORD0;
};

float4 main(Input input) : SV_Target0
{
    const float3 v = v_and_mode.xyz;

    const float3 by_matrix = mul(as_matrix, v);
    const float3 by_columns = c0.xyz * v.x + c1.xyz * v.y + c2.xyz * v.z;

    // Returned raw, into a float target, so the comparison on the C++ side is
    // against the exact number rather than against a rounded byte.
    return float4((v_and_mode.w > 0.5f) ? by_columns : by_matrix, 1.0f);
}
