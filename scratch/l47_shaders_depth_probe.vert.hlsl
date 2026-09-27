// shaders/depth_probe.vert.hlsl — a full-target surface at an exact distance.
//
// Lesson 4.7. The instrument for the one measurement in this lesson that is not a
// port of something already built: how far apart two surfaces must be before a
// depth buffer of a given format can tell them apart.
//
// It is SHARED by two fragment stages — `depth_probe.frag` and
// `texture_probe.frag` — because both want the same full-target surface and a
// second copy of this file would be a second place to get the corners wrong.
//
// It emits the same three-vertex full-target triangle `uniform_probe.vert.hlsl`
// does — no vertex buffer, corners from `SV_VertexID` — but at a caller-chosen
// VIEW DISTANCE, pushed through in `params`. Nothing here is a camera: the clip
// position is written directly from Lesson 2.10's projection terms, so the depth
// the rasterizer writes is exactly the value the derivation predicts and the
// experiment measures the depth BUFFER rather than a matrix.
//
//     clip = ( corner.x * d, corner.y * d,  A * (n - d),  d )
//
// which after the divide is xy = corner (the whole target) and
// z_ndc = A*(n - d)/d, the expression Lesson 4.7 §3.2 derives.

cbuffer Probe : register(b0, space1)
{
    // x = view distance d, in metres in front of the eye (positive)
    // y = A, the projection's depth scale = far / (near - far)
    // z = near
    // w = 0 for the ordinary mapping, 1 for REVERSED-Z (see §3.3)
    float4 params;
};

struct Output
{
    // Texture space across the whole target: (0,0) at the top-left corner,
    // v increasing DOWNWARDS, which is SDL_GPU's convention (conventions §4).
    // Lesson 4.7 §3.5 uses this to ask whether an image arrives the right way up
    // — if the file's top-left texel lands in the target's top-left corner, every
    // step between the two agreed about which way is up.
    float2 uv       : TEXCOORD0;
    float4 position : SV_Position;
};

Output main(uint vertex_id : SV_VertexID)
{
    Output output;

    const float2 corner = float2((vertex_id == 1u) ? 3.0f : -1.0f,
                                 (vertex_id == 2u) ? 3.0f : -1.0f);

    const float d = params.x;
    const float A = params.y;
    const float near_z = params.z;

    float z_clip = A * (near_z - d);

    // Reversed-Z: map near to 1 and far to 0 instead of 0 and 1. One subtraction
    // here, a flipped compare op and a flipped clear value on the C++ side, and
    // the precision story changes completely — §3.3 measures by how much.
    if (params.w > 0.5f) { z_clip = d - z_clip; }

    // corner.x runs -1..3 left to right, corner.y likewise bottom to top in NDC —
    // so v is 1 - (y + 1)/2, which flips it to run downwards.
    output.uv = float2((corner.x + 1.0f) * 0.5f, 1.0f - (corner.y + 1.0f) * 0.5f);

    output.position = float4(corner * d, z_clip, d);
    return output;
}
