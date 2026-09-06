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
//     ndf(ggx, n_dot_h, alpha)                 ndf_ggx()
//     smith_g(n_dot_l, n_dot_v, alpha)         smith_g()
//     fresnel_schlick(v_dot_h, f0)             fresnel_schlick()
//     cook_torrance_brdf(...)                  cook_torrance_brdf()
//     f_r * E + albedo * ambient               the return statement
//
// LESSON 6.4 REPLACED THE SURFACE HALF ON BOTH SIDES IN ONE COMMIT, which is the
// first time in the course that the two implementations had to move together
// rather than one following the other. The discipline that made it survivable is
// verify_48 §F, which renders the same scene through both and compares pixels:
// a shader that drifts from `light.hpp` does not fail loudly, it renders
// something plausible, and the only thing that catches it is a diff.
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
    float  spec_model;  // 60 — 0 = none, 1 = Phong, 2 = Blinn,
                        //      3 = Cook-Torrance (Lesson 6.4, the default)
};

// ---- Per DRAW --------------------------------------------------------------
cbuffer Material : register(b1, space3)
{
    float3 albedo;      //  0 — LINEAR; the CPU decoded the demo's Uint32 tint once
    float  roughness;   // 12 — Lesson 6.3: perceptual, [0,1]; alpha is its square
    float  metallic;    // 16 — Lesson 6.4: 0 = dielectric, 1 = conductor
    float  f0;          // 20 — dielectric normal-incidence reflectance, ~0.04
    float  textured;    // 24 — 0 = use `albedo`, 1 = sample `albedo_map`
    float  pad_m;       // 28 — NOT `pad0`: HLSL cbuffer members share one global
                        //      namespace across every buffer in the shader, so a
                        //      second `pad0` is a redefinition, not a local name.
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
static const float k_pi     = 3.14159265358979323846f;

// Lesson 6.3's floor on roughness. A perfectly smooth surface is a DELTA
// FUNCTION, not a small number: D goes to infinity at h == n and to zero
// everywhere else, which arrives here as `inf` and then `NaN`. `engine::k_min_alpha`
// is the same constant on the CPU side.
static const float k_min_alpha = 1.0e-3f;

struct Input
{
    float3 world  : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv     : TEXCOORD2;
};

// ---- Lesson 6.3: the microfacet model, in HLSL ------------------------------
//
// Three functions, each the counterpart of one in engine/gfx/microfacet.hpp, and
// each carrying the same numerical care. That care is not decoration: §A of
// verify_63 found that the TEXTBOOK form of the GGX denominator loses 1.1% of the
// model's energy to catastrophic cancellation in `float`, and a GPU is no less
// IEEE-754 than a CPU.

/// GGX / Trowbridge-Reitz, the distribution of microfacet normals.
float ndf_ggx(float n_dot_h, float alpha)
{
    if (n_dot_h <= 0.0f) { return 0.0f; }
    const float a2 = alpha * alpha;

    // NOT `n_dot_h*n_dot_h * (a2 - 1) + 1`, which every reference prints. Near the
    // peak that is the difference of two nearly-equal numbers of size 1, and a
    // `float` resolves it to ~1e-7 ABSOLUTE where the true value can be 1e-4. The
    // identical algebra below does not cancel, because Sterbenz's lemma makes
    // `1 - c` EXACT for c in [0.5, 1] — which is the whole peak region.
    const float d = (1.0f - n_dot_h) * (1.0f + n_dot_h) + a2 * n_dot_h * n_dot_h;
    return a2 / (k_pi * d * d);
}

/// Height-correlated Smith masking-shadowing (Heitz 2014).
///
/// Measured against the separable form (multiply the two G1s) in verify_63 §D:
/// 1.715x apart at alpha 0.8 with both directions 80 degrees off the normal.
/// Grazing angles on rough surfaces are where sunset lighting and wet roads live,
/// so "either is fine" would have been comfortable and wrong.
float smith_g(float n_dot_l, float n_dot_v, float alpha)
{
    if (n_dot_l <= 0.0f || n_dot_v <= 0.0f) { return 0.0f; }
    const float a2 = alpha * alpha;
    const float lv = n_dot_l * sqrt(a2 + (1.0f - a2) * n_dot_v * n_dot_v);
    const float ll = n_dot_v * sqrt(a2 + (1.0f - a2) * n_dot_l * n_dot_l);
    return 2.0f * n_dot_l * n_dot_v / (lv + ll);
}

/// Schlick's approximation to the Fresnel equations — Lesson 6.4.
///
/// Exact at both ends by construction (F0 at normal incidence, 1 at grazing) and
/// fitted in between. verify_64 §C measures the fit against the exact equations:
/// worst absolute error 0.0357 for glass, worst RELATIVE error 23.2% around 55
/// degrees. A 23% error in a term that is itself 0.04 is invisible, and that —
/// not a tight fit — is the honest defence of Schlick.
float3 fresnel_schlick(float cos_theta, float3 f0)
{
    const float m = 1.0f - saturate(cos_theta);
    const float m2 = m * m;
    return f0 + (1.0f - f0) * (m2 * m2 * m);
}

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

    // ---- Where the surface's own colour comes from --------------------------
    //
    // Lesson 3.9's sentence, unchanged: the texture REPLACES the tint, it does
    // not multiply it, because both of them are the albedo and a surface has one.
    // The `_SRGB` texture format means the decode to linear happens in the
    // sampler, before the filter — the ordering Lesson 3.9 measured the cost of
    // getting wrong (0.2139 where 0.5 is correct).
    //
    // MOVED ABOVE THE SHADING IN LESSON 6.4, because the albedo is now an INPUT
    // to the specular half as well: `f0_of` reads it for a metal. Until this
    // lesson the two halves were independent enough that the order did not
    // matter, and that independence is exactly what Fresnel removed.
    const float3 sampled = albedo_map.Sample(albedo_sampler, input.uv).rgb;
    const float3 base = lerp(albedo, sampled, textured);

    // ---- What the light delivers (Lesson 6.2) -------------------------------
    //
    // Irradiance measured square-on to the beam, projected onto this surface by
    // the cosine. No albedo appears in it, because how much light arrives cannot
    // depend on the colour of what it lands on. Untouched by Lesson 6.4 — the
    // light's half of the equation and the surface's half really are separable.
    const float3 e = key * n_dot_l;

    // ---- What the surface does ----------------------------------------------
    //
    // `to_eye`, from the world position the vertex stage went to the trouble of
    // forwarding. This is the line that varying exists for.
    const float3 v = normalize(eye_world - input.world);

    // THE METALLIC WORKFLOW, and it is two lines because it is two consequences
    // of one fact. A conductor absorbs whatever crosses its interface within a
    // few atoms, so (a) there is no diffuse lobe at all, and (b) the only light
    // leaving is the mirrored part — which means the colour has to live in F0.
    // A dielectric reflects a grey 4% and scatters the rest coloured. One albedo
    // field serves both, and `metallic` says which question it is answering.
    const float3 spec_f0 = lerp(float3(f0, f0, f0), base, metallic);
    const float3 diffuse_albedo = base * (1.0f - metallic);

    float3 f_r = float3(0.0f, 0.0f, 0.0f);

    if (spec_model > 2.5f)
    {
        // ---- COOK-TORRANCE (Lesson 6.4) -------------------------------------
        const float  n_dot_v = max(0.0f, dot(n, v));
        const float3 h = normalize(l + v);
        const float  n_dot_h = max(0.0f, dot(n, h));
        const float  v_dot_h = max(0.0f, dot(v, h));

        const float alpha = max(k_min_alpha, saturate(roughness) * saturate(roughness));

        // THE SPECULAR HALF: D * G * F over the derived denominator. The 4 is a
        // Jacobian (the map from light directions to microfacet normals stretches
        // solid angle by 4(v.h)); the (n.v) converts flux to radiance; the (n.l)
        // is the BRDF's own definition. Lesson 6.4 §4 derives all three.
        float3 f_s = float3(0.0f, 0.0f, 0.0f);
        if (n_dot_l > 0.0f && n_dot_v > 0.0f)
        {
            const float  d = ndf_ggx(n_dot_h, alpha);
            const float  g = smith_g(n_dot_l, n_dot_v, alpha);
            const float3 f = fresnel_schlick(v_dot_h, spec_f0);
            f_s = f * (d * g / (4.0f * n_dot_l * n_dot_v));
        }

        // THE DIFFUSE HALF, AND THE COUPLING IS THE WHOLE LESSON. F is what
        // bounces off the interface, so 1 - F is what goes in — and light crosses
        // the interface TWICE, once at the light's angle and once at the eye's.
        // Before this line the two lobes were independent and their sum was
        // unbounded (Lesson 6.2 measured 1.1386 for white on white).
        const float3 kd = (1.0f - fresnel_schlick(n_dot_l, spec_f0))
                        * (1.0f - fresnel_schlick(n_dot_v, spec_f0));
        f_r = f_s + kd * diffuse_albedo * k_inv_pi;
    }
    else
    {
        // ---- THE LEGACY LOBES (Lessons 3.6 and 3.7) -------------------------
        //
        // Kept selectable so the old model and the new one can be seen side by
        // side, and reading the SAME material through Lesson 6.3's mapping — the
        // demonstration that the old parameters were the new ones badly spelled.
        // `2/alpha^2 - 2` is `blinn_exponent_from_alpha` on the CPU side.
        const float alpha = max(k_min_alpha, saturate(roughness) * saturate(roughness));
        const float shininess = 2.0f / (alpha * alpha) - 2.0f;

        float lobe = 0.0f;
        if (n_dot_l > 0.0f && spec_model > 0.5f)
        {
            // Phong asks how nearly the eye lies along the MIRRORED RAY; Blinn
            // asks how nearly the surface already faces the HALFWAY VECTOR.
            // Lesson 3.7 §3.5 measured what separates them: `dot(R, v)` is
            // non-positive for 50.4% of light/eye pairs above a surface and
            // `dot(n, h)` for none, which is the hard edge Phong has.
            const float alignment = (spec_model < 1.5f)
                ? dot(n * (2.0f * dot(n, l)) - l, v)     // mirror_direction(n, l) . v
                : dot(n, normalize(l + v));              // n . halfway(l, v)

            // The clamp before `pow` is not defensive tidying. Two unit vectors'
            // dot product is a cosine and cannot exceed 1, but it can ROUND above
            // it, and `pow` amplifies the excess rather than absorbing it —
            // light.hpp measured an un-clamped peak of 1.00001.
            lobe = (alignment <= 0.0f) ? 0.0f : pow(min(1.0f, alignment), shininess);
        }

        // The 1/pi on the legacy specular is NOT a normalisation — Lesson 6.3
        // measured it as short by (s+2)/2, which is 17x at shininess 32. It is
        // left wrong on purpose; the fix is the branch above, not a constant.
        f_r = base * k_inv_pi + spec_f0 * lobe * k_inv_pi;
    }

    // The ambient term stands outside the product because its own pi already
    // cancelled against the hemisphere it was integrated over (light.hpp). It
    // still has no specular counterpart, so a mirror in a bright uniform room
    // renders black except where the key light reaches it — that missing term is
    // Lesson 6.12's image-based lighting.
    const float3 lit = f_r * e + base * ambient;

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
