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

// ---- The normal map — Lesson 6.7 -------------------------------------------
//
// A second slot, and the whole difference between the two is the FORMAT the
// texture was created with. `gpu_texture::create_sampled` takes an `srgb` flag;
// the albedo passes true and this one passes false, so the hardware decodes one
// through the sRGB transfer function on read and hands the other back as
// `byte/255`.
//
// **That flag has existed since Lesson 4.7 and the software renderer had no
// counterpart until 6.7** — which is the same shape of gap 6.6 found between
// `load_image` and `sample`: one half complete, the other absent, and no code
// path crossing between them until a feature needed both. `engine::texel_space`
// is the CPU side of this register declaration.
//
// The same binding rule as the albedo applies: a texture is bound at this slot
// for EVERY draw, because binding nothing at a declared slot draws nothing at
// all. The renderer binds a 1x1 flat-normal texture — (128, 128, 255), the
// lavender that means "no change" — when an object has no map, which is the
// identity element of this operation exactly as white is for the albedo.
Texture2D<float4> normal_map     : register(t1, space2);
SamplerState      normal_sampler : register(s1, space2);

// ---- The shadow map — Lesson 6.8 -------------------------------------------
//
// `Texture2D<float>`, not `float4`: a depth target has one channel and declaring
// four would be asking the driver to invent three.
//
// **`SamplerComparisonState` IS A DIFFERENT TYPE FROM `SamplerState`**, and that
// is the whole reason this pair looks unlike the two above it. An ordinary
// sampler hands back a filtered DEPTH; a comparison sampler performs the test
// against a reference you supply and hands back a filtered VISIBILITY. §6
// derives why those are not the same answer with two numbers: an occluder at 0.3
// and one at 0.9 average to a depth of 0.6, which a receiver at 0.5 passes
// outright, where comparing first gives 0 and 1 and an average of 0.5.
//
// The comparison itself — `LESS_OR_EQUAL` — lives in the SAMPLER object rather
// than in this file (`gpu_sampler::create_comparison`), which is worth noticing:
// it is fixed-function state, so it costs nothing, and it is chosen on the C++
// side where the depth convention that justifies it also lives.
// LESSON 6.9: AN ARRAY, ALWAYS — even for one cascade. `Texture2D` and
// `Texture2DArray` are different binding types, so carrying both would mean two
// shaders; making the single map a one-layer array makes 6.8's case the
// degenerate case of 6.9's and leaves exactly one code path here.
Texture2DArray<float>  shadow_map     : register(t2, space2);
SamplerComparisonState shadow_sampler : register(s2, space2);

// ---- Per FRAME: the cascades — Lesson 6.9 ----------------------------------
//
// A second per-frame block rather than a bigger `Light`, because pushes are
// billed by how often they happen and these are pushed at the same rate but read
// only when shadows are on.
//
// `float4` and not `float[4]`: HLSL puts each element of a scalar array in its
// OWN 16-byte register, so `float splits[4]` would cost 64 bytes to carry 16.
cbuffer Cascades : register(b2, space3)
{
    float4x4 cascade_clip_from_world[4];
    float4   cascade_splits;          // view-space far distance of each cascade
    float4   cascade_texel;           // world_per_texel, per cascade
    float4   cascade_depth_range;     // far - near, per cascade
    float    cascade_count;
    float    cascade_blend;
    float    cascade_pad0;
    float    cascade_pad1;
    float4   cascade_view_forward;    // the camera's axis; see gpu_uniform.hpp
};

// Index a float4 by a runtime value without a scalar array.
float cascade_pick(float4 v, int i)
{
    return (i == 0) ? v.x : (i == 1) ? v.y : (i == 2) ? v.z : v.w;
}

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

    // ---- Lesson 6.8 -------------------------------------------------------
    //
    // The block goes from 64 bytes to 176, and it can afford to: this is a
    // PER-FRAME push, one copy for the whole frame, where 6.7's `material`
    // block is charged once per draw.
    float4x4 light_clip_from_world;   //  64 — world -> the light's clip space

    float shadow_strength;     // 128 — 0 disables the lookup entirely
    float shadow_texel;        // 132 — world units per shadow texel
    float shadow_depth_range;  // 136 — far - near, in world units
    float shadow_bias;         // 140 — the constant term, in device depth
    float shadow_slope_scale;  // 144
    float shadow_max_slope;    // 148 — the clamp on tan(theta)
    float shadow_reach;        // 152 — how many texels the furthest tap is away
    float shadow_pcf;          // 156 — the kernel radius
    float shadow_mode;         // 160 — 0 none, 1 constant, 2 slope, 3 normal
    float shadow_normal_scale; // 164
    float shadow_texel_uv;     // 168 — 1/resolution: one texel, in uv
    float shadow_pad;          // 172
};

// ---- Per DRAW --------------------------------------------------------------
cbuffer Material : register(b1, space3)
{
    float3 albedo;      //  0 — LINEAR; the CPU decoded the demo's Uint32 tint once
    float  roughness;   // 12 — Lesson 6.3: perceptual, [0,1]; alpha is its square
    float  metallic;    // 16 — Lesson 6.4: 0 = dielectric, 1 = conductor
    float  f0;          // 20 — dielectric normal-incidence reflectance, ~0.04
    float  textured;    // 24 — 0 = use `albedo`, 1 = sample `albedo_map`

    // 28 — Lesson 6.7, and it cost ZERO BYTES: this slot was the padding 6.4
    // added to fill the second register, and the block is still exactly 32.
    // No binding code moved and no register allocation changed.
    //
    // (It replaces `pad_m`, whose own name was a lesson: HLSL cbuffer members
    // share ONE global namespace across every buffer in a shader, so a second
    // `pad0` is a redefinition rather than a local name — 6.4 found that out
    // the hard way.)
    float  normal_mapped;

    // ---- Lesson 6.11, and it cost a whole register --------------------------
    //
    // 6.7's flag was free — it landed in a pad 6.4 had already added. These two
    // are not: the block goes 32 -> 40 bytes of payload and therefore 32 -> 48
    // of registers, because a cbuffer is allocated in float4s. The C++ side has
    // the matching `static_assert`s, and `alpha_pad0`/`alpha_pad1` are named
    // rather than numbered because HLSL cbuffer members share ONE namespace
    // across every buffer in a shader.
    float  alpha;         // 32 — the material's opacity; 1 for anything opaque
    float  alpha_cutoff;  // 36 — 0 DISABLES the test; see `main`
    float  alpha_pad0;    // 40
    float  alpha_pad1;    // 44
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
    float3 world   : TEXCOORD0;
    float3 normal  : TEXCOORD1;
    float2 uv      : TEXCOORD2;
    float4 tangent : TEXCOORD3;   // 6.7: xyz the tangent, w the handedness
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

// ---- Lesson 6.8: can this point see the light? ------------------------------
//
// The counterpart of `engine::shadow_map::visibility`, line for line, and the
// two are checked against each other in verify_68 §G. Returns 1 for fully lit
// and 0 for fully shadowed; everything between is what PCF produces.
//
// `geo_cos` is the cosine taken against the GEOMETRIC normal, and the fact that
// it is a different number from the `n_dot_l` the shading equation uses is
// Lesson 6.7 collecting: acne is a disagreement about where the TRIANGLES are,
// and a normal map does not move a triangle.
float shadow_visibility(float3 world_pos, float3 geometric_n, float geo_cos,
                        int cascade)
{
    // Not enabled, or the surface faces away from the light — in which case the
    // direct term is already zero and consulting the map could only introduce a
    // wrong answer at a silhouette.
    if (shadow_strength <= 0.0f || geo_cos <= 0.0f) { return 1.0f; }

    // tan(theta) from the cosine, clamped. At exactly grazing incidence the
    // required bias is infinite and the honest answer is that a shadow map
    // cannot resolve that surface at all; `shadow_max_slope` is where we stop
    // pretending. `engine::slope_from_cosine` is the same three lines.
    // THE PER-CASCADE NUMBERS, and the whole of what a cascade changes. Both of
    // these are what 6.8's bias derivation was written in terms of, which is why
    // that derivation needed no edit: read a different texel size and a different
    // depth range and the same formula is already right.
    const float tex = cascade_pick(cascade_texel, cascade);
    const float range = cascade_pick(cascade_depth_range, cascade);

    const float c = min(1.0f, geo_cos);
    const float sin_theta = sqrt(max(0.0f, 1.0f - c * c));
    const float slope = min(shadow_max_slope, sin_theta / c);

    // NORMAL-OFFSET MOVES THE POINT; EVERY OTHER MODE MOVES THE DEPTH. §4.5.
    float3 p = world_pos;
    if (shadow_mode > 2.5f)
    {
        p += geometric_n * (shadow_normal_scale * tex * sin_theta
                            * shadow_reach * 1.41421356f);
    }

    // Into the light's clip space. `w` is 1 — the light's projection is
    // orthographic and has no perspective divide — so this is affine in the
    // world position, which is exactly why it can be done HERE, per fragment,
    // from a varying that has existed since Lesson 3.7, instead of costing four
    // more interpolated floats on every draw in the scene.
    const float4 clip = mul(cascade_clip_from_world[cascade], float4(p, 1.0f));
    if (clip.w <= 0.0f) { return 1.0f; }
    const float3 ndc = clip.xyz / clip.w;

    // NDC -> uv, with the y flip. NDC's +y is up; a texture's v runs DOWN from
    // the top-left corner (texture.hpp, quoting SDL's own coordinate section),
    // so the two disagree by exactly this subtraction. `viewport::to_screen`
    // performs the identical flip on the CPU side.
    const float2 uv = float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
    const float depth = ndc.z;

    // Outside the box the light was fitted to. The sampler clamps rather than
    // wrapping, but clamping would report the EDGE texel's occluder for
    // everything beyond it — a shadow smeared to the horizon. Range-checking is
    // what makes "outside the map" mean "unlit by nothing", which is lit.
    if (any(abs(ndc.xy) > 1.0f) || depth < 0.0f || depth > 1.0f) { return 1.0f; }

    // ---- The bias, in device depth units -----------------------------------
    //
    // §4.4: the furthest tap is `shadow_reach` texels away laterally, walking
    // that far across a surface of slope `tan(theta)` changes its depth by that
    // distance times the slope, and dividing by the depth range converts a
    // world-space error into the units the comparison is performed in.
    float bias = shadow_bias;
    if (shadow_mode > 1.5f && shadow_mode < 2.5f)
    {
        bias += shadow_slope_scale * shadow_reach * tex * slope
              / max(1e-6f, range);
    }
    else if (shadow_mode < 0.5f)
    {
        bias = 0.0f;   // `none` — the acne, on purpose
    }

    const float reference = depth - bias;

    // ---- PCF: compare, THEN average ----------------------------------------
    //
    // `SampleCmpLevelZero` does the comparing, and it does it on all four texels
    // of its bilinear footprint before blending them — so one tap is already 2x2
    // PCF in hardware, and the loop below widens that further. `LevelZero`
    // rather than `SampleCmp` because there is no mip chain here and the
    // gradient-based variant cannot be called from inside non-uniform control
    // flow anyway.
    const int r = clamp((int)shadow_pcf, 0, 3);

    float lit = 0.0f;
    float taps = 0.0f;
    for (int dy = -r; dy <= r; ++dy)
    {
        for (int dx = -r; dx <= r; ++dx)
        {
            const float2 at = uv + float2(dx, dy) * shadow_texel_uv;
            lit += shadow_map.SampleCmpLevelZero(shadow_sampler,
                                                 float3(at, (float)cascade), reference);
            taps += 1.0f;
        }
    }

    return 1.0f - shadow_strength * (1.0f - lit / taps);
}

// ---- Choosing a cascade, and hiding the seam — Lesson 6.9 ------------------
//
// `view_depth` is the positive distance from the eye along the view axis: the
// same number the splits were computed in, so selection cannot disagree with
// the fit. It is recovered here from the world position rather than carried as
// a varying, for 6.8's reason — a varying costs four bytes on every draw in the
// scene whether it is shadowed or not.
//
// THE SEAM. Two cascades meet at a split distance with different texel grids and
// different biases, so they disagree along that line and the disagreement draws
// a straight edge across the picture. `cascade_blend` fades from one to the
// other across a band: both answers are defensible, so anything between them is
// too. At 0 the band vanishes and the seam is visible, which is the first
// picture the lesson shows.
float shadow_visibility_cascaded(float3 world_pos, float3 geometric_n,
                                 float geo_cos, float view_depth)
{
    const int n = max(1, (int)cascade_count);

    int c = 0;
    for (int i = 0; i < n - 1; ++i)
    {
        if (view_depth > cascade_pick(cascade_splits, i)) { c = i + 1; }
    }

    const float a = shadow_visibility(world_pos, geometric_n, geo_cos, c);

    if (cascade_blend <= 0.0f || c >= n - 1) { return a; }

    const float far_d = cascade_pick(cascade_splits, c);
    const float near_d = (c == 0) ? 0.0f : cascade_pick(cascade_splits, c - 1);
    const float band = (far_d - near_d) * cascade_blend;
    if (band <= 0.0f || view_depth <= far_d - band) { return a; }

    const float w = saturate((view_depth - (far_d - band)) / band);
    const float b = shadow_visibility(world_pos, geometric_n, geo_cos, c + 1);
    return lerp(a, b, w);
}

float4 main(Input input) : SV_Target0
{
    // Lesson 3.8's finding, in silicon: the three corner normals are interpolated
    // LINEARLY, and a linear blend of unit vectors is shorter than one. Skip this
    // and every triangle darkens toward its middle.
    const float3 geometric = normalize(input.normal);

    // ---- LESSON 6.7: THE NORMAL, PERTURBED ----------------------------------
    //
    // Line for line the software renderer's `raster.cpp` fragment, which is the
    // claim this port has to make: the same frame, the same decode, the same
    // basis change. verify_67 §F measures the two against each other.
    //
    // GRAM-SCHMIDT FIRST. Both varyings were interpolated, so neither is unit
    // length and they are no longer perpendicular — interpolation preserves
    // neither property. The NORMAL is what we refuse to move; the tangent only
    // has to span the plane.
    const float3 t_raw = input.tangent.xyz;
    const float3 t_ortho = t_raw - geometric * dot(geometric, t_raw);

    // A degenerate frame (no tangents on the mesh, so `t_raw` is zero) would
    // normalize to NaN and spread it through the whole shading equation. The
    // guard costs one dot product per fragment and turns a black screen into a
    // surface that is merely not normal mapped.
    const float t_len2 = dot(t_ortho, t_ortho);
    const float3 tangent = (t_len2 > 1.0e-12f) ? t_ortho * rsqrt(t_len2)
                                               : float3(1.0f, 0.0f, 0.0f);

    // The bitangent is COMPUTED and its sign comes from the vertex. A mirrored
    // uv chart — which every symmetric model has — needs the other one.
    const float3 bitangent = cross(geometric, tangent) * input.tangent.w;

    // [0,1] -> [-1,1]. The map is a DIRECTION packed into bytes, so the encoding
    // is an offset — which is why an unperturbed normal map is lavender:
    // (0, 0, 1) stores as (0.5, 0.5, 1.0).
    const float3 packed = normal_map.Sample(normal_sampler, input.uv).rgb;
    const float3 tn = packed * 2.0f - 1.0f;

    // TANGENT SPACE -> WORLD, written as a matrix multiply's own definition: a
    // matrix IS where the basis vectors land (Lesson 2.5), and these three ARE
    // the basis vectors. A flat map — z = 1, x = y = 0 — returns `geometric`
    // exactly, which is the round trip verify_67 §D asserts.
    const float3 mapped = normalize(tangent * tn.x
                                    + bitangent * tn.y
                                    + geometric * tn.z);

    // `lerp` and not a branch, for the reason `textured` gives: every fragment
    // in a draw takes the same path, so a branch buys nothing and a multiply
    // costs nothing. `normalize` again because a lerp of two unit vectors is not
    // one — the same fact Lesson 3.8 found about interpolated normals, arriving
    // here for a different reason.
    const float3 n = normalize(lerp(geometric, mapped, normal_mapped));

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
    // LESSON 6.11 KEEPS THE FOURTH CHANNEL. One `.rgb` became a whole `float4`,
    // and the sample itself is unchanged — the alpha was always coming back from
    // the texture unit, and the shader was throwing it away exactly as
    // `engine::sample` was on the CPU side.
    const float4 sampled4 = albedo_map.Sample(albedo_sampler, input.uv);
    const float3 sampled = sampled4.rgb;
    const float3 base = lerp(albedo, sampled, textured);

    // ---- The coverage, and the test (Lesson 6.11) ---------------------------
    //
    // `lerp(1, texture_alpha, textured)` and not a branch, for the reason
    // `textured` has given since 4.7: every fragment in a draw takes the same
    // path, so a branch buys nothing and a multiply costs nothing. A material
    // with no albedo map has coverage 1 from its factor alone.
    const float src_alpha = alpha * lerp(1.0f, sampled4.a, textured);

    // THE ALPHA TEST, IN ONE INTRINSIC. `clip(x)` discards the fragment when x
    // is negative — so at `alpha_cutoff = 0` nothing with non-negative coverage
    // can be discarded and the test disables itself, which is why masking needs
    // no second pipeline and no shader variant. `uniforms_of` pushes the cutoff
    // only for a `mask` material and 0 for everything else.
    //
    // AND HERE IS WHAT IT COSTS, which is not the subtract: a shader containing
    // `clip` (or `discard`) cannot have early-Z. The depth test has to wait for
    // the fragment, because the fragment decides whether there IS a fragment.
    // The software rasterizer pays the identical price explicitly — `raster.cpp`
    // reorders its depth write for exactly this reason — and seeing the same
    // constraint appear in both is the best evidence that it is a fact about the
    // problem rather than about either implementation.
    clip(src_alpha - alpha_cutoff);

    // ---- What the light delivers (Lesson 6.2) -------------------------------
    //
    // Irradiance measured square-on to the beam, projected onto this surface by
    // the cosine. No albedo appears in it, because how much light arrives cannot
    // depend on the colour of what it lands on. Untouched by Lesson 6.4 — the
    // light's half of the equation and the surface's half really are separable.
    // ---- LESSON 6.8: AND WHETHER IT ARRIVES AT ALL --------------------------
    //
    // `visibility` multiplies `E`, the light's half of the equation, and not
    // `f_r` — a shadow is a fact about whether the light ARRIVES and says
    // nothing about what the surface would do with it. That is the same division
    // Lesson 6.2 drew when it moved the cosine onto the light's side. Fold it
    // into the BRDF instead and a shadowed metal stops being metal.
    //
    // Note what it does NOT multiply: `ambient`, three lines below. Ambient is
    // light that has bounced off everything else in the room, and an object
    // standing in the way of the sun does not stop the room existing — the
    // ambient term is precisely what a shadowed surface is left with.
    // AXIAL depth, not radial — gpu_uniform.hpp's `view_forward` says why, and
    // the difference is 22% at the corner of a 60-degree frame.
    const float view_depth = dot(input.world - eye_world, cascade_view_forward.xyz);
    const float visibility = shadow_visibility_cascaded(input.world, geometric,
                                                        dot(geometric, l),
                                                        view_depth);

    const float3 e = key * n_dot_l * visibility;

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
    // Lesson 6.15's image-based lighting.
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
    // LESSON 6.12 GIVES THIS FLAG A THIRD SITUATION AND NO THIRD VALUE, which is
    // worth stating plainly rather than letting a reader infer it:
    //
    //   RENDERING TO A FLOAT HDR TARGET  -> pass 0, and it is now the only legal
    //                       answer. There is no transfer function in a float
    //                       target to be right or wrong about; the values written
    //                       here ARE quantities of light, and the encode happens
    //                       later, in `tonemap.frag.hlsl`, over the finished
    //                       image. Same number as the `_SRGB` case, entirely
    //                       different reason.
    //
    // So the flag's honest meaning has always been "must THIS shader apply the
    // transfer function?", and 6.11's argument about `_SRGB` targets — which was
    // about where the BLENDER sits — does not transfer to a float target at all.
    // Nothing in a float pipeline lerps codes, because nothing stores codes.
    //
    // The curve is the EXACT piecewise sRGB transform — the same constants as
    // engine::linear_to_srgb, because two implementations of "the sRGB curve"
    // that disagree are a bug that only shows up in a diff. Never pow(x, 1/2.2):
    // that misses the linear toe and is visibly wrong in the darkest codes, which
    // is precisely where this whole subject does its damage.
    // ---- LESSON 6.11 CASHES THE WARNING ABOVE -------------------------------
    //
    // "Correct for opaque geometry and wrong the moment anything blends" was
    // written in Lesson 6.1 as a prediction. This is the lesson where something
    // blends, and §7 measures the prediction: half-coverage white over black
    // comes out at code 128 through the `encode_output == 1` path and code 188
    // through the `_SRGB` one. **43% of the light**, and the shader cannot fix
    // it — the blend happens after the shader ends, on whatever is in the target,
    // so encoding here means the ROP lerps sRGB codes and there is no later
    // opportunity to undo that.
    //
    // The fix is not in this file. It is to render to an `_SRGB` target, which
    // this engine does whenever the swapchain offers one. What belongs here is
    // the honest note that the fallback path is a fallback.
    //
    // THE ALPHA IS THE SAME NUMBER IN BOTH BRANCHES, and it is NOT encoded in
    // either. Coverage is a fraction of a pixel, not a quantity of light — the
    // sentence `colour.hpp` has carried since Lesson 1.6 — so putting it through
    // a transfer function would be a category error, and one that would make
    // every half-transparent surface composite as though it covered 73% of the
    // pixel. Note that `to_encoded` on the CPU side makes the same choice and
    // `verify_61` asserts it.
    if (encode_output > 0.5f)
    {
        const float3 c = saturate(lit);
        const float3 low  = c * 12.92f;
        const float3 high = 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
        return float4(c <= 0.0031308f ? low : high, src_alpha);
    }

    return float4(lit, src_alpha);
}
