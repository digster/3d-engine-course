// shaders/skybox.frag.hlsl — one cube-map fetch, and two decisions around it.
//
// Lesson 6.15. The shader itself is a `Sample` and a multiply. What is worth
// reading is why it does NOT tonemap and why it can select a mip level.

TextureCube<float4> sky_map     : register(t0, space2);
SamplerState        sky_sampler : register(s0, space2);

cbuffer SkyboxShade : register(b0, space3)
{
    /// A scalar on the radiance. 1 draws the environment at the value it was
    /// baked with; anything else is an artistic lie, which is a legitimate thing
    /// to want and an illegitimate thing to hide.
    float intensity;     //  0

    /// **Which mip level to show, and it is a debug feature that earns its
    /// place.** Bound to the prefiltered chain instead of the radiance map, this
    /// turns the sky into a direct view of what `prefilter_environment` produced
    /// at each roughness — which is the only way to see whether the prefilter is
    /// doing the right thing, as opposed to seeing a reflection that is wrong
    /// for one of four possible reasons. Lesson 6.15 §8 uses it to diagnose a
    /// blurred sun that came back as a ring of dots.
    ///
    /// Negative means "level 0", which is what a caller that does not care
    /// passes.
    float level;         //  4
    float pad0;          //  8
    float pad1;          // 12
};

struct Input
{
    float4 position : SV_Position;
    float3 ray      : TEXCOORD0;
};

float4 main(Input input) : SV_Target0
{
    // NORMALISE HERE, NOT IN THE VERTEX STAGE. The ray was interpolated across
    // the triangle and a linear blend of unit vectors is shorter than one
    // (Lesson 3.8). A cube-map lookup does not actually care about the length —
    // it divides by the largest component — so this is not strictly required for
    // the fetch. It is required for anything that later wants the direction as a
    // direction, and normalising in the one place that produces it is cheaper to
    // reason about than remembering which consumers need it.
    const float3 d = normalize(input.ray);

    // `SampleLevel` and not `Sample`, and the reason is not the debug feature.
    // `Sample` picks a mip level from the screen-space derivatives of the
    // coordinate — which is exactly right for a surface texture and meaningless
    // here, because the derivative of a view ray across the screen says how fast
    // the camera is turning, not how much sky a pixel covers. On a one-level
    // radiance map it would make no difference; on the six-level prefiltered
    // chain this shader can also be pointed at, it would silently pick a level
    // based on the wrong quantity.
    const float3 radiance = sky_map.SampleLevel(sky_sampler, d, max(0.0f, level)).rgb;

    // NO TONEMAP, NO sRGB ENCODE, and this is the one line where the skybox has
    // to know where it is in the frame. It renders into the HDR target that
    // Lesson 6.12 built, so what belongs here is a QUANTITY OF LIGHT — the same
    // contract `scene.frag.hlsl` honours with `encode_output == 0` on that path.
    // `tonemap.frag.hlsl` converts the finished image once, over everything,
    // which is the only place the conversion can be applied consistently.
    //
    // Tonemapping the sky here instead is a mistake with a very recognisable
    // signature: the sky looks right on its own and every reflection of it in
    // the scene comes out too bright, because the reflections read the untouched
    // cube map while the background reads a compressed copy.
    return float4(radiance * intensity, 1.0f);
}
