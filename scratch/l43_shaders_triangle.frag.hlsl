// shaders/triangle.frag.hlsl — the fragment stage Lesson 4.4 will draw with.
//
// Lesson 4.3. This is `pixel_from` in src/gfx/raster.cpp, moved to the GPU: it
// runs once per fragment and returns a colour. It has no resources at all —
// no textures, no uniform buffers — which is why every count in its
// SDL_GPUShaderCreateInfo is zero, and why it is the right shader to start with.

struct Input
{
    // Must match the vertex stage's output semantics, name for name. The value
    // arriving here has been INTERPOLATED across the triangle — perspective-
    // correctly, by hardware, for the reason Lesson 3.2 derived by hand.
    float4 colour : TEXCOORD0;
};

// SV_Target0 is the first colour target of the render pass. A pass may have up
// to four; Module 6's deferred experiments will use more than one.
float4 main(Input input) : SV_Target0
{
    return input.colour;
}
