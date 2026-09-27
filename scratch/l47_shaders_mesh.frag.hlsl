// shaders/mesh.frag.hlsl — the uv finally does its job.
//
// Lesson 4.7. The texture coordinate has travelled through the vertex buffer
// since Lesson 4.5 and has only ever drawn a diagnostic grid, because nothing
// sampled anything. This file is the payoff, and it is four new lines.
//
// THE SPACES ARE FIXED BY SDL, PER STAGE (Lesson 4.3). Fragment-stage textures
// and samplers live in space2; fragment-stage uniform buffers in space3. And note
// the pairing: `t0` goes with `s0`, which is exactly what
// `SDL_GPUTextureSamplerBinding` expresses on the C++ side and exactly the
// pairing `engine::texture` + `engine::sampler` has expressed since Lesson 3.9.
Texture2D<float4> albedo         : register(t0, space2);
SamplerState      albedo_sampler : register(s0, space2);

cbuffer Light : register(b0, space3)
{
    float3 to_light;   //  0 — toward the lamp, world space, unit length
    float  ambient;    // 12 — what a surface facing away still receives
    float3 sky;        // 16 — the colour the ambient term is tinted with
    float  diffuse;    // 28 — how much the cosine term contributes

    float2 uv_scale;   // 32 — how many times the image repeats across the surface
    float  grid_mix;   // 40 — 0 = texture only, 1 = Lesson 4.5's diagnostic grid
    float  _pad;       // 44 — the float that keeps the next float3 off a straddle
};

struct Input
{
    float3 normal : TEXCOORD0;
    float2 uv     : TEXCOORD1;
    float4 tint   : TEXCOORD2;
};

float4 main(Input input) : SV_Target0
{
    // Lesson 3.8's finding, in silicon: the rasterizer interpolates the three
    // corner normals LINEARLY, and a linear blend of unit vectors is shorter than
    // one. Skip this and the surface darkens in the middle of every triangle.
    const float3 n = normalize(input.normal);

    // Lesson 3.6: brightness is the cosine of the angle between the surface
    // normal and the direction to the light, clamped at zero because a surface
    // facing away receives nothing.
    const float lambert = saturate(dot(n, normalize(to_light)));

    // ---- ONE LINE FOR WHAT src/gfx/texture.cpp SPENDS A HUNDRED ON ----------
    //
    // Wrap the coordinate, find the four texels whose CENTRES surround it,
    // weight them by distance. Lesson 3.9 derived every part of that by hand —
    // including the half-texel offset that is the commonest texture bug there is
    // — so that this line is a recognition rather than a magic word.
    //
    // Two things happen here that the software path could not do for free. The
    // filter is fixed-function hardware; and because the texture was created with
    // an _SRGB format, each texel is DECODED TO LINEAR BEFORE it is filtered,
    // which is the ordering Lesson 3.9 measured the cost of getting wrong (0.2139
    // where 0.5 is correct — 43% of the light).
    const float3 texel = albedo.Sample(albedo_sampler, input.uv * uv_scale).rgb;

    // The diagnostic grid from Lesson 4.5, kept and put on a key rather than
    // deleted. It is how the uv was checked when nothing sampled it, and it is
    // still the fastest way to see a texture-coordinate problem as geometry
    // rather than as a smear.
    const float2 cell = abs(frac(input.uv * 8.0f) - 0.5f);
    const float groove = smoothstep(0.42f, 0.5f, max(cell.x, cell.y));

    // The albedo: the texture, tinted by the per-instance colour, or the flat
    // tint with the grid cut into it.
    const float3 lit_grid = input.tint.rgb * (1.0f - 0.45f * groove);
    const float3 albedo_rgb = lerp(texel * input.tint.rgb, lit_grid, grid_mix);

    // The ambient term is TINTED by a sky colour rather than grey: a surface
    // facing away from the sun is not unlit, it is lit by the sky. One multiply,
    // and a one-sample approximation of the whole hemisphere that Module 6's
    // image-based lighting replaces with something defensible.
    return float4(albedo_rgb * (sky * ambient + diffuse * lambert), 1.0f);
}
