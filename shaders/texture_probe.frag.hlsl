// shaders/texture_probe.frag.hlsl — the image, straight onto the target.
//
// Lesson 4.7. One texel of the file per pixel of the target, with no geometry,
// no lighting and no transform in the way — so that if the picture that comes
// back is not the picture that went in, exactly one of the steps between them is
// responsible and there is a short list.
//
// The two questions it answers, both numerically:
//
//   WHICH WAY UP? The file has a differently-coloured block in each corner, so
//   reading the four corners of the readback says whether the image, the upload,
//   and the sampler all agreed about which way v runs.
//
//   WHAT DID _SRGB DO? Sampling the same texel through an _SRGB texture and a
//   _UNORM one and comparing gives the decode Lesson 3.9 performed by hand, as a
//   difference in bytes.

Texture2D<float4> image         : register(t0, space2);
SamplerState      image_sampler : register(s0, space2);

struct Input
{
    float2 uv : TEXCOORD0;
};

float4 main(Input input) : SV_Target0
{
    return image.Sample(image_sampler, input.uv);
}
