// shaders/overlay.vert.hlsl — pixels to clip space, and the flip lives here.
//
// Lesson 6.18. The whole vertex stage of the overlay is two multiplies and a
// subtraction, and the reason it is a shader at all rather than arithmetic on
// the CPU is that the viewport changes and the vertices do not: a resize pushes
// four bytes instead of rewriting every quad.
//
// THE COORDINATE FLIP HAPPENS EXACTLY ONCE, AND THIS IS IT. `overlay.hpp` fixes
// pixel space as origin top-left, +y DOWN — the framebuffer's convention and the
// window's. SDL_GPU's clip space has +y UP (conventions §5), so screen y = 0 must
// become clip y = +1 and screen y = height must become clip y = -1. Put this
// conversion in a second place — the layout, say, or the CPU compositor — and
// text is upside down on one renderer and correct on the other, which reads as a
// GPU bug and is a convention bug.
//
// NO PROJECTION MATRIX, deliberately. An orthographic matrix would do the same
// job in four dot products and would invite somebody to put a camera in it. An
// overlay has no camera; it has a viewport.

// Vertex-stage uniforms live in space1 — `scene.vert.hlsl` explains the four
// spaces SDL_shadercross assigns. b0 is the per-frame block, and this one is as
// per-frame as it gets.
cbuffer Viewport : register(b0, space1)
{
    /// 2 / width and 2 / height, precomputed on the CPU. The division is per
    /// FRAME rather than per vertex, which is the entire argument for passing
    /// the reciprocal instead of the size.
    float2 inv_half_viewport;   // 0
    float2 viewport_pad;        // 8 — SDL requires 16-byte alignment for a block
};

struct VSInput
{
    // TEXCOORD semantics numbered from zero with no gaps — Lesson 4.3's rule,
    // because the numbers ARE the vertex attribute locations that
    // `pipeline_desc::attribute` declares, and `check_layout` compares the two.
    float2 position : TEXCOORD0;   // pixels, origin top-left, +y down
    float2 uv       : TEXCOORD1;   // normalized atlas coordinates
    float4 colour   : TEXCOORD2;   // UBYTE4_NORM, sRGB-encoded, straight alpha
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 colour   : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // x: [0, w] -> [-1, +1].   y: [0, h] -> [+1, -1], which is the flip.
    output.position = float4(input.position.x * inv_half_viewport.x - 1.0f,
                             1.0f - input.position.y * inv_half_viewport.y,
                             0.0f,   // z: the overlay has no depth and no test
                             1.0f);  // w: 1, so there is no perspective divide
    output.uv = input.uv;
    output.colour = input.colour;
    return output;
}
