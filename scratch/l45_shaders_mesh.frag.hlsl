// shaders/mesh.frag.hlsl — Lambert, and a line of proof that the uv arrived.
//
// Lesson 4.5. This is Lesson 3.6's shading term, unchanged, running per pixel on
// hardware instead of per pixel in `pixel_from`. The light direction is a constant
// for the same reason the camera is: a uniform buffer is Lesson 4.6.
//
// THE GRID IS NOT DECORATION. Attribute 2 is the texture coordinate, and nothing
// in this lesson samples a texture yet (that is Lesson 4.7), so a uv that arrived
// scrambled — wrong offset, wrong pitch, wrong location — would look exactly like
// a uv that arrived correctly. Drawing thin lines along the surface's own
// parameterisation makes the third attribute VISIBLE, which turns a layout bug
// from a mystery into a picture.

// Toward the light, world space, unit length. Lesson 3.6's convention: the vector
// points AT the lamp, so the dot product with the normal is directly the cosine.
static const float3 k_to_light = float3(0.3931975f, 0.8060550f, 0.4423472f);

static const float k_ambient = 0.22f;   // so the unlit side is dark, not black
static const float k_diffuse = 0.90f;

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
    const float lambert = saturate(dot(n, k_to_light));

    float3 colour = input.tint.rgb * (k_ambient + k_diffuse * lambert);

    // The uv grid: eight cells each way, a thin darker line at every boundary.
    // `frac` gives position within a cell; the distance to the nearest edge is
    // 0.5 - |frac - 0.5|, and `smoothstep` softens the line so it does not alias
    // into a dotted mess when the surface is edge-on.
    // `groove`, not `line` — `line` is a reserved word in HLSL (it names a
    // geometry-shader primitive type), and the parse error it produces points at
    // the semicolon rather than at the name.
    const float2 cell = abs(frac(input.uv * 8.0f) - 0.5f);
    const float edge = max(cell.x, cell.y);
    const float groove = smoothstep(0.42f, 0.5f, edge);

    colour *= (1.0f - 0.45f * groove);

    return float4(colour, 1.0f);
}
