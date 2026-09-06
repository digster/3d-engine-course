// shaders/scene.frag.hlsl — `engine::shade()`, in HLSL.
//
// Lesson 4.8, and this file is the strongest evidence the course has for the
// claim that the software rasterizer was not a detour. Every line below has a
// counterpart in src/gfx/light.hpp, most of them line for line, and the two were
// written three modules apart without either being adjusted to suit the other.
//
//     light.hpp                                this file
//     ------------------------------------     -------------------------------
//     normalised_or(normal, 0)                 normalize(input.normal)
//     lambert(n, l) = max(0, dot(n, l))        saturate(dot(n, l))
//     mirror_direction(n, l)                   n * (2*dot(n,l)) - l
//     halfway(l, v) = normalised(l + v)        normalize(l + v)
//     pow(min(1, alignment), shininess)        pow(min(1, alignment), shininess)
//     lambert_brdf(albedo) = albedo * inv_pi   base * k_inv_pi
//     specular_brdf(surface, lobe)             specular * lobe * k_inv_pi
//     (f_d + f_s) * E + albedo * ambient       the return statement
//
// LESSON 6.2 UPDATED THREE OF THOSE ROWS AND THE PICTURE DID NOT MOVE, which is
// the strongest evidence available that the two implementations really are one
// equation: the pi came out of the light and went into the BRDF on both sides,
// independently, and verify_48's pixel-for-pixel comparison still agrees.
//
// THE ONE REAL DIFFERENCE IS WHERE IT RUNS, and it is not a difference in the
// maths. `shade()` is called once per VERTEX under Gouraud and once per PIXEL
// under Lesson 3.8's per-pixel mode; this file is only ever the second. There is
// no Gouraud pipeline here, and that is not an omission — a GPU has a vertex
// stage that could compute a colour and pass it down, and Exercise 4.8.3 asks you
// to build exactly that and count the pixels it costs.
//
// THE SECOND DIFFERENCE IS THE CLAMP. `shade_encoded` ends in `to_encoded`, which
// clamps to [0,1] and rounds to eight bits; here the clamp is the render target's,
// applied by the hardware on write, and the sRGB encode with it when the target
// is a `_SRGB` format. Same two operations, moved from our code into the machine.
// Module 6's HDR pipeline is the lesson where throwing that headroom away stops
// being acceptable.

// ---- The albedo image ------------------------------------------------------
//
// Fragment-stage textures and samplers live in space2; the pairing `t0`/`s0` is
// what `SDL_GPUTextureSamplerBinding` expresses on the C++ side (Lesson 4.7).
//
// A texture is bound for EVERY draw, including the ones whose material says not
// to sample it. Binding nothing at a slot a shader declares draws nothing at all
// — the failure the 4.6 and 4.7 sessions each hit once — so the renderer binds a
// 1x1 white texture when an object has no image. That is the identity element of
// sampling, and using it is cheaper in every sense than a second pipeline.
Texture2D<float4> albedo_map     : register(t0, space2);
SamplerState      albedo_sampler : register(s0, space2);

// ---- Per FRAME -------------------------------------------------------------
cbuffer Light : register(b0, space3)
{
    float3 to_light;    //  0 — toward the lamp, world space, unit length
    float  pad0;        // 12
    float3 key;         // 16 — the lamp's LINEAR colour times its IRRADIANCE
                        //      (Lesson 6.2). E_perp: the light landing on a
                        //      surface held square-on to the beam. The cosine
                        //      below projects it onto the surface we have.
    float  pad1;        // 28
    float3 ambient;     // 32 — a uniform hemispherical RADIANCE (Lesson 6.2).
                        //      Its own pi cancelled against the hemisphere it is
                        //      integrated over, which is why the albedo multiplies
                        //      it bare while everything else goes through a BRDF.
    float  encode_output; // 44 — Lesson 6.1: 1 = this shader must encode sRGB,
                          //      0 = the swapchain is _SRGB and does it for us.
                          //      Was `pad2`; HLSL's packing had already reserved
                          //      the slot, so the flag cost nothing.
    float3 eye_world;   // 48 — a highlight is view-dependent (Lesson 3.7)
    float  spec_model;  // 60 — 0 = none, 1 = Phong, 2 = Blinn
};

// ---- Per DRAW --------------------------------------------------------------
cbuffer Material : register(b1, space3)
{
    float3 albedo;      //  0 — LINEAR; the CPU decoded the demo's Uint32 tint once
    float  shininess;   // 12 — the exponent; not comparable between the two models
    float3 specular;    // 16 — the highlight's reflectance ratio; black = matte
    float  textured;    // 28 — 0 = use `albedo`, 1 = sample `albedo_map`
};

// ---- Lesson 6.2 -------------------------------------------------------------
//
// One over pi, the constant that makes the Lambert BRDF conserve energy. HLSL has
// no <numbers>, so the digits are written out — and they are written to more
// precision than a float can hold on purpose, so the compiler rounds once, the
// same way `std::numbers::inv_pi_v<float>` does. `engine::k_inv_pi` is the same
// number on the CPU side; two spellings of a constant that disagree in the last
// bit is the sort of thing that costs an afternoon in a pixel diff.
static const float k_inv_pi = 0.318309886183790671538f;

struct Input
{
    float3 world  : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv     : TEXCOORD2;
};

float4 main(Input input) : SV_Target0
{
    // Lesson 3.8's finding, in silicon: the three corner normals are interpolated
    // LINEARLY, and a linear blend of unit vectors is shorter than one. Skip this
    // and every triangle darkens toward its middle.
    const float3 n = normalize(input.normal);
    const float3 l = normalize(to_light);

    // Lambert's cosine law (Lesson 3.6 §3.1). `saturate` is `max(0, min(1, x))`
    // and is free on this hardware; light.hpp spells the same clamp `std::max`
    // and explains at length why a negative answer must not be let through.
    const float n_dot_l = saturate(dot(n, l));

    // ---- The highlight (Lesson 3.7) ----------------------------------------
    //
    // Guarded exactly as `shade()` guards it, and for the same reason: a matte
    // surface — which is most of them, and every surface in Lesson 3.6's scene —
    // must pay nothing for this lesson existing. On a GPU "pay nothing" is worth
    // less than it sounds, because a warp executes both sides of a branch when
    // its lanes disagree; §5.3 measures what this one actually costs.
    float lobe = 0.0f;
    const bool shiny = (specular.r + specular.g + specular.b) > 0.0f;

    if (n_dot_l > 0.0f && spec_model > 0.5f && shiny)
    {
        // `to_eye`, from the world position the vertex stage went to the trouble
        // of forwarding. This is the line that varying exists for.
        const float3 v = normalize(eye_world - input.world);

        // Phong asks how nearly the eye lies along the MIRRORED RAY; Blinn asks
        // how nearly the surface already faces the HALFWAY VECTOR. Lesson 3.7
        // §3.5 measured what separates them: `dot(R, v)` is non-positive for
        // 50.4% of light/eye pairs above a surface and `dot(n, h)` for none of
        // them, which is the hard edge Phong has and Blinn does not.
        const float alignment = (spec_model < 1.5f)
            ? dot(n * (2.0f * dot(n, l)) - l, v)     // mirror_direction(n, l) . v
            : dot(n, normalize(l + v));              // n . halfway(l, v)

        // The clamp before `pow` is not defensive tidying. Two unit vectors' dot
        // product is a cosine and cannot exceed 1, but it can ROUND above it, and
        // `pow` amplifies the excess rather than absorbing it — light.hpp
        // measured an un-clamped peak of 1.00001. `min` here is `std::min` there.
        lobe = (alignment <= 0.0f) ? 0.0f : pow(min(1.0f, alignment), shininess);
    }

    // ---- Where the surface's own colour comes from --------------------------
    //
    // Lesson 3.9's sentence, unchanged: the texture REPLACES the tint, it does
    // not multiply it, because both of them are the albedo and a surface has one.
    // The `_SRGB` texture format means the decode to linear happens in the
    // sampler, before the filter — the ordering Lesson 3.9 measured the cost of
    // getting wrong (0.2139 where 0.5 is correct).
    //
    // `lerp` rather than `if`, because the two sides cost one instruction each
    // and a branch on a uniform value is not free enough to be worth the words.
    const float3 sampled = albedo_map.Sample(albedo_sampler, input.uv).rgb;
    const float3 base = lerp(albedo, sampled, textured);

    // ---- The shading equation, which is `shade()`'s return statement --------
    //
    // LESSON 6.2, and the three factors are three separate physical claims:
    //
    //     L_o  =  (f_d + f_s) * E_perp * cos(theta)  +  albedo * L_ambient
    //
    // `e` is WHAT THE LIGHT DELIVERS: irradiance measured square-on to the beam,
    // projected onto this surface by the cosine. No albedo appears in it, because
    // how much light arrives cannot depend on the colour of what it lands on.
    //
    // BOTH TERMS CARRY n_dot_l, and light.hpp explains why at length: the cosine
    // law is about how much light ARRIVES per unit of surface, and says nothing
    // about what the surface then does with it. Classic Phong shading as
    // published left it off the specular, and the artifact is a highlight glowing
    // past the terminator on geometry the light cannot reach. Stated this way the
    // rule needs no defending: the cosine is on the LIGHT's side of the product,
    // so of course it multiplies everything the surface does.
    const float3 e = key * n_dot_l;

    // `f_d` and `f_s` are WHAT THE SURFACE DOES, both per steradian, added
    // because a real surface scatters some light and mirrors some at once. The
    // 1/pi on the specular is not a normalisation — light.hpp measures exactly
    // how un-normalised it remains, and Lesson 6.4 fixes it with Fresnel.
    const float3 f_d = base * k_inv_pi;
    const float3 f_s = specular * lobe * k_inv_pi;

    // The ambient term stands outside the product because its own pi already
    // cancelled against the hemisphere it was integrated over (light.hpp).
    const float3 lit = (f_d + f_s) * e + base * ambient;

    // ---- The last place light exists — Lesson 6.1 ---------------------------
    //
    // `lit` is a QUANTITY OF LIGHT. Everything above this line is arithmetic on
    // light and is only meaningful because it is: a reflectance multiplies an
    // amount of light, and two lights add. What a display consumes is not light,
    // it is a CODE, and the transfer function between them is not optional.
    //
    // There are exactly two right answers to "who applies it", and this shader
    // supports both because SDL guarantees only the second-best one:
    //
    //   encode_output == 0  the swapchain is an _SRGB format (SDR_LINEAR), so the
    //                       hardware encodes during the write. Free, exact, and
    //                       correct through blending — which is the reason to
    //                       prefer it: hardware blending happens AFTER this
    //                       shader, and if we encode here, the blender adds codes.
    //   encode_output == 1  the swapchain is plain SDR, whose values are already
    //                       sRGB codes, so we must encode. Correct for opaque
    //                       geometry and wrong the moment anything blends.
    //
    // The curve is the EXACT piecewise sRGB transform — the same constants as
    // engine::linear_to_srgb, because two implementations of "the sRGB curve"
    // that disagree are a bug that only shows up in a diff. Never pow(x, 1/2.2):
    // that misses the linear toe and is visibly wrong in the darkest codes, which
    // is precisely where this whole subject does its damage.
    if (encode_output > 0.5f)
    {
        const float3 c = saturate(lit);
        const float3 low  = c * 12.92f;
        const float3 high = 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
        return float4(c <= 0.0031308f ? low : high, 1.0f);
    }

    return float4(lit, 1.0f);
}
