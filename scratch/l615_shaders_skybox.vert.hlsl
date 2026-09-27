// shaders/skybox.vert.hlsl — the sky, drawn with no geometry at all.
//
// Lesson 6.15. A skybox is traditionally a cube: six quads, thirty-six indices,
// scaled up around the camera and drawn with its faces inward. That works, and
// it has two problems this shader avoids by not having geometry.
//
//   IT CAN BE CLIPPED. A cube big enough to enclose the scene may reach past the
//   far plane; a cube small enough not to may be entered by the near plane. Both
//   have standard workarounds (disable depth, or scale by the far distance) and
//   both are fiddling with a mesh that exists only to be a background.
//
//   IT HAS EDGES. Lesson 6.12's `fullscreen.vert` already made this argument for
//   post-processing and it applies unchanged here: fragments are shaded in 2x2
//   quads (Lesson 4.1), so every quad straddling a shared triangle edge is
//   issued twice. A cube has twelve such diagonals across the screen.
//
// So we reuse the oversized-triangle trick and turn each pixel into a RAY.
//
// ---------------------------------------------------------------------------
// THE RAY, DERIVED
// ---------------------------------------------------------------------------
//
// A pixel at normalised device coordinates `(nx, ny)`, both in [-1, 1], is the
// point the camera sees through position
//
//     forward + nx * right + ny * up
//
// *provided* `right` and `up` are already scaled by the half-extents of the
// image plane at unit distance — which is `tan(fov_y/2)` vertically and
// `aspect * tan(fov_y/2)` horizontally. The CPU does that scaling once per frame
// (`skybox_uniforms` in gpu_uniform.hpp) so the shader is three multiplies and
// two adds. It is deliberately NOT an inverse view-projection multiply: that
// costs a 4x4 inverse on the CPU and a matrix-vector product plus a divide per
// pixel here, to compute the same three numbers.
//
// The result does not need normalising in this stage — it is interpolated across
// the triangle and a linear interpolation of unit vectors is not unit anyway
// (Lesson 3.8's finding, arriving for the fourth time). The fragment stage
// normalises once, per pixel, where it is correct to.
//
// ---------------------------------------------------------------------------
// THE DEPTH, AND WHY IT IS EXACTLY 1
// ---------------------------------------------------------------------------
//
// Conventions §4: this engine's device depth runs 0 at the near plane and 1 at
// the far plane. The sky is behind everything, so it belongs at 1 — and writing
// `position = float4(nx, ny, 1, 1)` puts it there after the perspective divide,
// with `w = 1` so the divide is the identity.
//
// The pipeline for this pass therefore uses **LESS_OR_EQUAL, with depth writes
// off**. `OR_EQUAL` and not `LESS` is load-bearing: the depth buffer was cleared
// to 1, so a strict test would reject every sky pixel and the background would
// stay at the clear colour. Getting this wrong produces a completely black sky
// with correctly lit objects in front of it, which reads as "the cube map failed
// to load" and sends you looking in entirely the wrong place.

struct Output
{
    float4 position : SV_Position;
    float3 ray      : TEXCOORD0;   ///< world-space view direction, un-normalised
};

// Per frame. `right` and `up` arrive pre-scaled by the image plane's half-extents
// — see the derivation above — so the shader never sees a field of view.
cbuffer Skybox : register(b0, space1)
{
    float3 ray_right;    //  0
    float  pad0;         // 12
    float3 ray_up;       // 16
    float  pad1;         // 28
    float3 ray_forward;  // 32
    float  pad2;         // 44
};

Output main(uint vertex_id : SV_VertexID)
{
    Output o;

    // (0,0), (2,0), (0,2) in uv — `fullscreen.vert`'s derivation, unchanged, so
    // the two shaders cover the screen with the identical triangle and a reader
    // who has understood one has understood both.
    const float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    const float nx = uv.x * 2.0f - 1.0f;
    const float ny = 1.0f - uv.y * 2.0f;

    // z = 1: the far plane. See the header comment — with depth-test
    // LESS_OR_EQUAL and writes disabled, the sky fills exactly the pixels no
    // geometry claimed.
    o.position = float4(nx, ny, 1.0f, 1.0f);
    o.ray = ray_forward + ray_right * nx + ray_up * ny;
    return o;
}
