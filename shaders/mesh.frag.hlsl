// shaders/mesh.frag.hlsl — Lambert, and a line of proof that the uv arrived.
//
// Lesson 4.5 wrote this with the light direction frozen into the file. Lesson 4.6
// gives it a uniform block, which is a smaller change than it sounds and a bigger
// one than it looks: the lighting environment is now a thing the CPU decides per
// frame, which is what makes it a thing you can put on a key.
//
// LOOK AT THE SHAPE OF THE BLOCK BELOW. Every `float3` is followed by a `float`.
// That is deliberate and it is the single habit that keeps uniform blocks out of
// trouble: HLSL packs a vector into the first register where it fits without
// straddling, std140 aligns every `float3` to 16, and a `float3` + `float` pair
// fills a register exactly — so the two rules agree and it does not matter which
// one your toolchain used. `verify_46` §B measures what happens when they
// disagree, which is that three components arrive shifted by one float with a zero
// on the end, silently.

// Fragment-stage uniform buffers live in space3. Not a choice — see
// mesh.vert.hlsl and Lesson 4.3.
cbuffer Light : register(b0, space3)
{
    float3 to_light;   //  0 — toward the lamp, world space, unit length
    float  ambient;    // 12 — what a surface facing away still receives
    float3 sky;        // 16 — the colour the ambient term is tinted with
    float  diffuse;    // 28 — how much the cosine term contributes
};

// THE GRID IS NOT DECORATION. Attribute 2 is the texture coordinate, and nothing
// in this lesson samples a texture yet (that is Lesson 4.7), so a uv that arrived
// scrambled would look exactly like a uv that arrived correctly. Drawing thin
// lines along the surface's own parameterisation makes the third attribute
// VISIBLE, which turns a layout bug from a mystery into a picture.

struct Input
{
    float3 normal : TEXCOORD0;
    float2 uv     : TEXCOORD1;
    float4 tint   : TEXCOORD2;
};

float4 main(Input input) : SV_Target0
{
    // Renormalised HERE, and this is Lesson 3.8's finding arriving in silicon: the
    // rasterizer interpolates the three corner normals LINEARLY, and a linear blend
    // of unit vectors is shorter than one. Skip this and the surface darkens in the
    // middle of every triangle.
    const float3 n = normalize(input.normal);

    // Lesson 3.6: brightness is the cosine of the angle between the surface normal
    // and the direction to the light, clamped at zero because a surface facing away
    // receives nothing — a negative cosine would make it emit.
    const float lambert = saturate(dot(n, normalize(to_light)));

    // The ambient term is TINTED now rather than grey. A surface facing away from
    // the sun is not unlit; it is lit by the sky, which is blue. That is one
    // multiply and it is the whole of what Module 6's image-based lighting is
    // approximating — worth having early, and worth knowing is an approximation.
    float3 colour = input.tint.rgb * (sky * ambient + diffuse * lambert);

    // The uv grid: eight cells each way, a thin darker line at every boundary.
    // `frac` gives position within a cell; the distance to the nearest edge is
    // 0.5 - |frac - 0.5|, and `smoothstep` softens the line so it does not alias
    // into a dotted mess when the surface is edge-on.
    //
    // `groove`, not `line` — `line` is a reserved word in HLSL (it names a
    // geometry-shader primitive type), and the parse error it produces points at
    // the semicolon rather than at the name.
    const float2 cell = abs(frac(input.uv * 8.0f) - 0.5f);
    const float edge = max(cell.x, cell.y);
    const float groove = smoothstep(0.42f, 0.5f, edge);

    colour *= (1.0f - 0.45f * groove);

    return float4(colour, 1.0f);
}
