// shaders/uniform_probe.vert.hlsl — a full-target triangle, and nothing else.
//
// Lesson 4.6. The instrument, not the exhibit. `verify_46` needs a way to ask the
// GPU what it actually received in a uniform buffer, and the only channel a
// fragment shader has for answering is the colour it writes. So this stage draws
// one oversized triangle covering the whole render target, and the fragment stage
// turns each pixel into one probe of the uniform data.
//
// THE TRIANGLE HAS NO VERTEX BUFFER. `SV_VertexID` is a system value the hardware
// fills in, so three vertices can be generated from arithmetic — which means this
// pipeline needs no vertex layout at all, and therefore cannot be measuring one by
// accident. Lesson 4.5 spent a whole lesson on how easily a layout goes wrong;
// keeping it out of the instrument is the point.

struct Output
{
    float4 position : SV_Position;
};

Output main(uint vertex_id : SV_VertexID)
{
    Output output;

    // The standard full-screen trick: (-1,-1), (3,-1), (-1,3). A triangle twice
    // the size of the target in each direction, so the target is entirely inside
    // it and the two extra corners are clipped away. One triangle rather than two
    // means no shared edge, so no seam and no pixel rasterised twice.
    const float2 corner = float2((vertex_id == 1u) ? 3.0f : -1.0f,
                                 (vertex_id == 2u) ? 3.0f : -1.0f);
    output.position = float4(corner, 0.0f, 1.0f);
    return output;
}
