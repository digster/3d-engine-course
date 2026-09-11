// shaders/fullscreen.vert.hlsl — a triangle that covers the screen, from nothing.
//
// Lesson 6.12, and it is the first shader in this course with **no vertex buffer
// at all**. That is not a trick for its own sake: a post-processing pass has no
// geometry, and binding a quad mesh to draw over the whole screen would be
// inventing a vertex buffer so that a vertex shader has something to ignore.
//
// ---------------------------------------------------------------------------
// A TRIANGLE, NOT A QUAD, AND THE REASON IS 4.1's QUAD TRAVERSAL
// ---------------------------------------------------------------------------
//
// The obvious shape for "cover the screen" is two triangles meeting along the
// diagonal. It works, and it is measurably worse, for a reason this course
// already built the vocabulary for: fragments are shaded in aligned 2x2 quads
// (Lesson 4.1), and along the shared diagonal every quad straddles BOTH
// triangles — so those quads are issued twice and half of each is a helper lane.
// One oversized triangle has no internal edge, so nothing is shaded twice.
//
// ---------------------------------------------------------------------------
// THE THREE VERTICES, DERIVED RATHER THAN MEMORISED
// ---------------------------------------------------------------------------
//
// We want one triangle whose interior covers the whole of NDC — x and y both in
// [-1, 1] — and we would like the uv to come out as a simple affine function of
// the position so the fragment stage needs no extra varying arithmetic.
//
// Take uv over [0, 2] at the three corners: (0,0), (2,0), (0,2). Those span a
// right triangle twice the size of the unit square, so the unit square is
// strictly inside it. Now map uv to clip space with the standard flip:
//
//     x =  uv.x * 2 - 1        uv 0 -> -1,  uv 1 -> +1,  uv 2 -> +3
//     y =  1 - uv.y * 2        uv 0 -> +1,  uv 1 -> -1,  uv 2 -> -3
//
// giving clip positions (-1, 1), (3, 1), (-1, -3). The screen — the square from
// (-1,-1) to (1,1) — sits entirely inside that triangle, and everything outside
// is clipped by hardware at no cost. The y flip is conventions §6: NDC's +y is
// up and a texture's v runs down.
//
// SV_VertexID is the index of the vertex being processed, supplied by the
// hardware for a non-indexed draw. `SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0)` is
// the whole draw call.

struct Output
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

Output main(uint vertex_id : SV_VertexID)
{
    Output o;

    // (0,0), (2,0), (0,2) — see the derivation above. Written as arithmetic on
    // the id rather than as a constant array, because a constant array in a
    // vertex shader is a small uniform buffer on some backends and two integer
    // operations on all of them.
    o.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    o.position = float4(o.uv.x * 2.0f - 1.0f, 1.0f - o.uv.y * 2.0f, 0.0f, 1.0f);
    return o;
}
