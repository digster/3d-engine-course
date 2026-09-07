// engine/include/engine/gfx/microfacet.hpp — what a surface actually is.
//
// Lesson 6.3. Everything in `light.hpp` describes a surface as a smooth plane with
// a `shininess` exponent, and Lesson 6.2 measured what that costs: the same surface
// reflects 0.1386 of the arriving light toward an eye 37 degrees off the normal and
// 0.0259 toward one at 75, a swing of 5.36x that nobody asked for and no parameter
// controls — and a white surface with a white highlight reflects 1.1386 of what
// hits it, because the diffuse and specular lobes are added with no coupling.
//
// Those are symptoms of one cause: THE MODEL HAS NO STORY ABOUT WHAT A SURFACE IS.
// `shininess` is a number that makes the highlight the right size. It is not a
// measurement of anything, it is not comparable between Phong and Blinn (3.7 fitted
// the ratio at 4.38x falling to 4.01x), and there is no experiment you could do on
// a real material to find out what its shininess is.
//
// MICROFACET THEORY REPLACES THE EXPONENT WITH A STATISTICAL CLAIM. A surface is a
// landscape of microscopic mirrors, far smaller than a pixel and far larger than a
// wavelength. Each is a perfect mirror, so each reflects light toward the eye only
// if it happens to face the halfway vector `h` (Lesson 3.7 already derived that
// `h` is "the normal this surface would need"; that sentence is now literal). What
// a rough surface looks like is therefore decided by ONE question:
//
//     what fraction of the microfacets face this way?
//
// That is a probability distribution over normals, and it is the NDF below. It is
// a distribution rather than a fudge, which means it must integrate to one, which
// means it can be CHECKED — verify_63 §A does exactly that for all three models.
//
// WHAT THIS HEADER WAS NOT, AND NOW IS. Lesson 6.3 built D and G and wired
// neither in, because a microfacet BRDF is D, G and F over a denominator and
// Fresnel is the piece that couples the diffuse and specular lobes back together.
// Shipping two thirds of it live would have left the renderer running a model
// that was neither the old one nor the new one for a lesson.
//
// LESSON 6.4 ADDED F AND ASSEMBLED, and the second half of this file is that
// work: Schlick's Fresnel and the constant it needs, the 4(n.l)(n.v) denominator
// DERIVED from a change of variables rather than quoted, `microsurface` (the
// parameters that replace `shininess` and `specular::colour`), and the two lobes
// composed into one BRDF that cannot emit more light than arrives. `shade()` and
// `scene.frag.hlsl` both call into it, and the reference render moved for the
// first time in fifteen lessons.

#pragma once

#include <engine/gfx/colour.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace engine {

/// The smallest roughness the model is allowed to see.
///
/// **A perfectly smooth surface is a delta function**, not a small number: every
/// microfacet faces exactly `n`, so `D` is infinite at `h == n` and zero
/// everywhere else. That is the correct physics and it is unrepresentable in
/// floating point — `ndf()` divides by a quantity that goes to zero, and the
/// answer arrives as `inf` and then as `NaN` the moment it meets a zero.
///
/// Clamping is the standard answer, and it is worth being clear that it is a
/// *lie with a physical reading*: no real surface is smooth at the scale of a
/// light wavelength, so a floor on roughness is closer to the truth than zero is.
/// The value is chosen so that `alpha*alpha` stays well clear of `float`'s
/// denormals: at 1e-3 the peak of GGX is 1/(pi*alpha^2) = 318,310, which is large
/// and finite.
inline constexpr float k_min_alpha = 1.0e-3f;

/// Which distribution of microfacet normals to use.
///
/// **This is pipeline state, not a material parameter** — the same call 3.7 made
/// for `specular_model`, and for the same reason. A real engine bakes one choice
/// into a shader; it is a runtime knob here so the three can be rendered side by
/// side and the differences measured (Lesson 6.3 §5) rather than asserted.
enum class ndf_model
{
    /// Blinn-Phong's `cos^s` lobe — **which is a microfacet distribution**, and
    /// always was. Lesson 3.7 introduced it as a shape that looked right and said
    /// so; all it has ever been missing is its normalising constant. Kept because
    /// that continuity is worth being able to see, and because it is the cheapest
    /// of the three.
    blinn,

    /// Beckmann's: a Gaussian on the microfacet *slopes*. The distribution that
    /// physical-optics derivations arrive at, and the one against which "does this
    /// look like a real surface?" was argued for thirty years.
    beckmann,

    /// GGX / Trowbridge-Reitz. Same peak as Beckmann for the same roughness, and a
    /// **far heavier tail** — measured at 456x Beckmann's value 45 degrees off the
    /// peak (verify_63 §C). That tail is the whole reason it won: real surfaces
    /// have a glow around the highlight that Beckmann cuts off abruptly.
    ggx
};

/// Perceptual roughness in [0,1] to the `alpha` the distributions actually use.
///
/// **`alpha = roughness^2`, and this is a CHOICE, not a derivation.** The
/// distributions are written in terms of `alpha`, which is the RMS slope of the
/// microfacet field — a physical quantity. But `alpha` is a terrible thing to hand
/// an artist: almost all the visible change happens in its bottom fifth, so a
/// slider in `alpha` spends four fifths of its travel doing nothing. Squaring a
/// [0,1] parameter pushes resolution toward the smooth end where the eye is, which
/// is the same argument Lesson 6.1 made about sRGB spending codes where the eye is.
///
/// It is Disney's remap, it is what UE4 and Filament ship, and it is worth knowing
/// that it is a convention rather than physics: a roughness value copied from one
/// renderer into another that squares it differently will not match.
[[nodiscard]] inline float alpha_from_roughness(float roughness)
{
    const float r = std::clamp(roughness, 0.0f, 1.0f);
    return std::max(k_min_alpha, r * r);
}

/// The inverse, for reading a stored `alpha` back into artist units.
[[nodiscard]] inline float roughness_from_alpha(float alpha)
{
    return std::sqrt(std::max(0.0f, alpha));
}

/// The Blinn exponent whose distribution PEAKS at the same height as GGX's.
///
/// **The word "peaks" is doing all the work here, and the folklore leaves it
/// out.** Both distributions are largest at `h == n`; setting those two values
/// equal gives `(s + 2) / 2pi = 1 / (pi * alpha^2)`, hence `s = 2/alpha^2 - 2`.
/// That is the mapping everyone quotes.
///
/// It matches the peak and nothing else. Lesson 6.3 §6 instead *fits* the exponent
/// by minimising the difference over the whole hemisphere, and gets consistently
/// **about 0.80x** this value (0.806 at alpha 0.1, 0.758 at 0.5) — because a lobe
/// fit trades peak height for the tail, and GGX has a tail Blinn cannot reproduce
/// at any exponent. Both numbers are right; they are answers to different
/// questions, and quoting one without saying which is how the folklore travels.
[[nodiscard]] inline float blinn_exponent_from_alpha(float alpha)
{
    const float a = std::max(k_min_alpha, alpha);
    return 2.0f / (a * a) - 2.0f;
}

/// The reverse: what roughness a Lesson 3.7 `shininess` was asking for.
///
/// Useful for exactly one thing, and it is the thing this lesson needs: reading
/// the engine's existing materials into the new vocabulary. The demo's default
/// shininess of 32 turns out to mean `alpha = 0.2425`, a perceptual roughness of
/// **0.49** — almost exactly half way, which is a reasonable thing for a default
/// to be and was never chosen on purpose.
[[nodiscard]] inline float alpha_from_blinn_exponent(float shininess)
{
    return std::max(k_min_alpha, std::sqrt(2.0f / (std::max(0.0f, shininess) + 2.0f)));
}

/// **The normal distribution function**: the fraction of microfacets facing `h`.
///
/// Derived in Lesson 6.3 §4. `n_dot_h` is the cosine between the surface's
/// macroscopic normal and the halfway vector — so this asks "how much of the
/// surface is tilted to send this light at my eye?", which is the question Lesson
/// 3.7's halfway vector was already a way of asking.
///
/// **THE DEFINING PROPERTY IS THAT IT IS A DISTRIBUTION**, and the test is not
/// that it integrates to 1 over the hemisphere but that it does so *weighted by
/// `cos(theta_h)`*:
///
///     integral over hemisphere of D(h) * cos(theta_h) dw = 1
///
/// The cosine is there because a microfacet tilted away presents less of its own
/// area to the macroscopic surface — the same projected-area factor as everywhere
/// else in this module (Lesson 6.2 §3). Read the identity the other way and it
/// says something nicer: **the microfacets' projected areas add up to exactly the
/// area of the flat surface they stand on.** Nothing else would make sense.
///
/// All three models satisfy it exactly (verify_63 §A, to 1e-6 at 200k samples),
/// and `blinn` satisfies it only *because* of the `(s + 2) / 2pi` this function
/// applies. The engine's own highlight, which has shipped since 3.7 with `1/pi`
/// instead, is therefore off by a factor of `(s + 2) / 2` — **exactly 17x at the
/// default shininess of 32**. See Lesson 6.3 §5; it is the finding.
///
/// Returns 0 for a halfway vector below the surface, which is not a degenerate
/// case to guard against but a true statement: no microfacet faces that way.
[[nodiscard]] inline float ndf(ndf_model model, float n_dot_h, float alpha)
{
    if (n_dot_h <= 0.0f) { return 0.0f; }
    const float a = std::max(k_min_alpha, alpha);
    const float c2 = n_dot_h * n_dot_h;

    switch (model)
    {
    case ndf_model::blinn:
    {
        const float s = blinn_exponent_from_alpha(a);
        return ((s + 2.0f) / (2.0f * std::numbers::pi_v<float>)) * std::pow(n_dot_h, s);
    }
    case ndf_model::beckmann:
    {
        // exp(-tan^2(theta_h) / alpha^2) / (pi * alpha^2 * cos^4(theta_h)).
        // tan^2 is written from the cosine rather than via std::tan, because the
        // cosine is what we have and a round trip through an angle would cost an
        // acos and lose bits for nothing. `sin2` is computed the stable way — see
        // the note on GGX below, which is where it actually bites.
        const float sin2 = (1.0f - n_dot_h) * (1.0f + n_dot_h);
        return std::exp(-(sin2 / c2) / (a * a))
             / (std::numbers::pi_v<float> * a * a * c2 * c2);
    }
    case ndf_model::ggx:
        break;
    }

    // GGX. The denominator is squared, which is the entire difference from
    // Beckmann: a rational function has a power-law tail where an exponential has
    // none. That is why the highlight has a glow around it.
    //
    // THE ALGEBRA HERE IS NOT THE TEXTBOOK'S, AND THE DIFFERENCE IS MEASURABLE.
    // Every reference writes the denominator as
    //
    //     d = cos^2(theta_h) * (alpha^2 - 1) + 1
    //
    // which is correct, and in `float` it is a catastrophic cancellation: near the
    // peak `cos^2` is close to 1 and `alpha^2 - 1` is close to -1, so the result is
    // the difference of two numbers of size 1 that very nearly agree. A `float`
    // resolves that to about 1e-7 ABSOLUTE — and at alpha = 0.01 the true value of
    // `d` at the peak is 1e-4, so a thousandth of it is noise. Then it is squared.
    //
    // The identical expression, rearranged, does not cancel:
    //
    //     d = (1 - cos^2) + alpha^2 * cos^2 = (1 - c)(1 + c) + alpha^2 * c^2
    //
    // `(1 - c)` is where the win is, and the reason is **Sterbenz's lemma**: for
    // c in [0.5, 1], `1.0f - c` is computed with NO ROUNDING AT ALL, exactly, in
    // any IEEE format. The whole peak region is inside that range.
    //
    // MEASURED, by the §A normalisation test, which is what found this:
    //
    //     alpha    textbook form   this form     (double, for truth)
    //     0.001    0.9891063       1.0023035     1.0000013
    //     0.010    0.9998169       1.0000004     1.0000000
    //     0.050    1.0000006       1.0000000     1.0000000
    //
    // At mirror-like roughness the textbook form loses **1.1% of the model's
    // energy** to rounding, and it does not improve with a finer integration grid,
    // because it is not a quadrature error. This is exactly the sort of thing a
    // test whose expected answer is a round number finds and a test that compares
    // against "what it printed last time" cannot.
    const float a2 = a * a;
    const float d = (1.0f - n_dot_h) * (1.0f + n_dot_h) + a2 * c2;
    return a2 / (std::numbers::pi_v<float> * d * d);
}

/// Smith's masking function for **one** direction: the fraction of microfacets
/// visible from `n_dot_x` rather than hidden behind their neighbours.
///
/// Derived in Lesson 6.3 §7. The exact GGX form (Walter 2007), not the Schlick
/// approximation — the approximation belongs with the rest of 6.4's arithmetic
/// budget, and this lesson is about knowing what is being approximated.
///
/// It is 1 looking straight down and falls toward 0 at grazing incidence, and how
/// fast it falls depends entirely on roughness: at `alpha = 0.05` it is still
/// 0.9914 at 75 degrees, and at `alpha = 1.0` it is 0.4112 there. **That is the
/// term Lesson 6.2 was missing when it measured a 5.36x view-angle swing it could
/// not explain** — a rough surface really does hide part of itself from you, and
/// a model with no G has no way to say so.
[[nodiscard]] inline float smith_g1(float n_dot_x, float alpha)
{
    if (n_dot_x <= 0.0f) { return 0.0f; }
    const float a2 = std::max(k_min_alpha, alpha) * std::max(k_min_alpha, alpha);
    return 2.0f * n_dot_x
         / (n_dot_x + std::sqrt(a2 + (1.0f - a2) * n_dot_x * n_dot_x));
}

/// The separable Smith geometry term: mask the light, then mask the eye.
///
/// **The assumption is stated in the name.** Multiplying the two G1s assumes that
/// whether a microfacet is lit and whether it is visible are independent events.
/// They are not: both depend on the facet's height in the same landscape, so a
/// facet down in a valley tends to be hidden from *both* directions at once, and
/// treating the two as independent counts that facet as hidden twice.
///
/// Kept, named, and measured rather than quietly replaced — see `smith_g`.
[[nodiscard]] inline float smith_g_separable(float n_dot_l, float n_dot_v, float alpha)
{
    return smith_g1(n_dot_l, alpha) * smith_g1(n_dot_v, alpha);
}

/// **Height-correlated Smith** (Heitz 2014), and the default, because the
/// correlation is not a small effect.
///
/// Same idea as the separable form with the independence assumption dropped: the
/// two maskings are resolved together, over the shared height distribution.
///
/// Measured against the separable version (verify_63 §D): identical to four
/// decimals on smooth surfaces and at steep angles, and **1.715x apart at
/// `alpha = 0.8` with both directions 80 degrees off the normal** — 0.1255 against
/// 0.2152. The separable form loses that light. Grazing angles on rough surfaces
/// are exactly where sunset lighting, wet roads and worn metal live, so this is
/// not a corner case, and "either is fine" would have been a comfortable thing to
/// write and wrong.
[[nodiscard]] inline float smith_g(float n_dot_l, float n_dot_v, float alpha)
{
    if (n_dot_l <= 0.0f || n_dot_v <= 0.0f) { return 0.0f; }
    const float a = std::max(k_min_alpha, alpha);
    const float a2 = a * a;
    const float lv = n_dot_l * std::sqrt(a2 + (1.0f - a2) * n_dot_v * n_dot_v);
    const float ll = n_dot_v * std::sqrt(a2 + (1.0f - a2) * n_dot_l * n_dot_l);
    return 2.0f * n_dot_l * n_dot_v / (lv + ll);
}


// ===========================================================================
//  LESSON 6.4 — FRESNEL, AND THE ASSEMBLY
// ===========================================================================
//
// Everything above describes the surface. Nothing above says how much light it
// actually reflects, and that is the missing third: `D` says which facets face
// the right way, `G` says which of those you can see, and **`F` says what
// fraction of the light that reaches a facet bounces off it instead of going
// in.** That last sentence is why Fresnel is not one more multiplier. It is the
// only term in the model that talks about the light that DOESN'T reflect, and
// the diffuse lobe is made of exactly that light.
//
// Lesson 6.2 measured a white surface with a white highlight reflecting 1.1386
// of what hit it, and named the cause: the two lobes were added with no coupling,
// so the same photon was counted once as having bounced off the surface and once
// as having gone into it. Normalising D (Lesson 6.3) does not touch that, and
// 6.3 said so and left the number standing. `F` is the coupling.

/// The reflectance of a smooth interface at NORMAL incidence, for the ordinary
/// dielectrics — plastic, glass, skin, wood, water, most of the world.
///
/// **This is not a tuning constant, it is a measurement**, and one line of
/// algebra away from the index of refraction. `f0_from_ior(1.5)` is exactly
/// 0.04, and 1.5 is roughly every common dielectric: glass is 1.5, most plastics
/// 1.46-1.55, skin 1.4, water 1.33. The whole visible range of dielectric F0 is
/// **0.02 to 0.08**, which is a remarkably small box, and it is the reason a
/// single default is defensible where `specular::colour` never was.
///
/// Compare what it replaces. This engine shipped `specular::colour` values of
/// 0.85, 0.60, 0.55 and 0.30 — between seven and twenty-one times a physically
/// possible dielectric reflectance. They were not wrong, exactly; they were
/// absorbing the 17x normalisation error Lesson 6.3 measured, plus an exposure,
/// plus taste. An error a PARAMETER can absorb is invisible, and this is the
/// constant that makes it visible.
inline constexpr float k_dielectric_f0 = 0.04f;

/// Normal-incidence reflectance from an index of refraction, for light arriving
/// from air.
///
/// Derived in Lesson 6.4 §5.1 from the Fresnel equations at `theta = 0`, where
/// the s- and p-polarised forms collapse to the same expression:
///
///     F0 = ((n1 - n2) / (n1 + n2))^2,  with n1 = 1 for air
///
/// It is squared because the Fresnel equations give an *amplitude* ratio and
/// reflectance is a ratio of POWERS. That squaring is the single most common
/// place to lose a factor of two in this subject.
[[nodiscard]] inline float f0_from_ior(float ior)
{
    const float n = std::max(1.0f, ior);
    const float r = (1.0f - n) / (1.0f + n);
    return r * r;
}

/// The inverse: what index of refraction an authored F0 is claiming.
///
/// Worth having for exactly one reason, and it is a debugging reason: an F0 of
/// 0.85 comes back as an IOR of **25.3**, and there is no such material. A
/// number that round-trips into an absurdity is a number somebody guessed.
[[nodiscard]] inline float ior_from_f0(float f0)
{
    const float r = std::sqrt(std::clamp(f0, 0.0f, 0.9999f));
    return (1.0f + r) / (1.0f - r);
}

/// **Schlick's approximation to the Fresnel equations.**
///
/// The exact equations need the transmitted angle (so Snell's law, so a square
/// root), separate s- and p-polarised terms, and an average of the two — about
/// twenty operations, per light, per pixel. Schlick 1994 replaces all of it with
///
///     F(cos) = F0 + (1 - F0) * (1 - cos)^5
///
/// and the shape is right for a reason that is worth seeing rather than
/// accepting: **every interface reflects everything at grazing incidence.** Look
/// along a table top, a puddle, a sheet of paper — all of them turn into mirrors.
/// So F must rise to exactly 1 at `cos = 0` no matter what F0 is, and the
/// expression above does that by construction: at `cos = 0` it is `F0 + 1 - F0`.
/// It is also exactly F0 at `cos = 1` by construction. Schlick's contribution is
/// the *fifth power* in between, and that is the part that is fitted.
///
/// **HOW GOOD IS THE FIT? Measured against the exact equations rather than
/// asserted** (verify_64 §C), and the answer is less flattering than the folklore:
///
///   - worst ABSOLUTE error 0.0357 for glass (ior 1.5), out at 85 degrees;
///   - worst RELATIVE error **23.2%**, and it sits at 55 degrees, in the middle
///     of the range where surfaces are usually seen. At 60 degrees the exact
///     answer is 0.0892 and Schlick says 0.0700.
///
/// A 23% error in a term that is itself 0.04 is a 0.019 error in reflectance,
/// which is invisible — and that is the honest defence of Schlick, not that the
/// fit is tight. It is cheap, it is exact at both ends, and it is wrong in the
/// middle by an amount that does not matter *at dielectric F0*. It matters more
/// as F0 rises: at ior 2.4 (diamond) the worst absolute error doubles to 0.074.
///
/// `cos_theta` is the cosine between the view direction and the MICROFACET
/// normal — that is, `v.h`, not `n.v`. The facet is the mirror; its own normal
/// is the one the light is bouncing off. Lesson 6.4 §5.3 is about how easy that
/// is to get wrong and what it looks like when you do.
[[nodiscard]] inline float fresnel_schlick(float cos_theta, float f0)
{
    const float m = 1.0f - std::clamp(cos_theta, 0.0f, 1.0f);
    const float m2 = m * m;
    return f0 + (1.0f - f0) * (m2 * m2 * m);   // (1-cos)^5, four multiplies
}

/// The same, per channel — which is what makes a metal a metal.
///
/// **A metal's F0 is COLOURED, and that is very nearly the whole of what
/// separates it from a dielectric.** Gold reflects about (1.00, 0.71, 0.29) at
/// normal incidence, copper (0.95, 0.64, 0.54), silver (0.97, 0.96, 0.92) — so a
/// gold surface tints the light it mirrors, where a plastic one does not. A
/// dielectric's F0 is grey because its index of refraction barely varies across
/// the visible spectrum; a metal's does not have that courtesy.
[[nodiscard]] inline linear_rgb fresnel_schlick(float cos_theta, linear_rgb f0)
{
    const float m = 1.0f - std::clamp(cos_theta, 0.0f, 1.0f);
    const float m2 = m * m;
    const float w = m2 * m2 * m;
    return {f0.r + (1.0f - f0.r) * w,
            f0.g + (1.0f - f0.g) * w,
            f0.b + (1.0f - f0.b) * w};
}

/// How the diffuse lobe is told about the light the specular lobe already took.
///
/// **This is a real choice with a measured answer, kept as an enum for the same
/// reason `smith_g_separable` was kept in Lesson 6.3: the difference is large
/// enough that picking one is a decision rather than a default.**
enum class diffuse_coupling
{
    /// `(1 - F(n.l)) * (1 - F(n.v))` — **the default, and the one the physics
    /// actually says.** Light crosses the interface TWICE: once going in, at the
    /// light's angle, and once coming out, at the eye's. Each crossing reflects
    /// its own Fresnel fraction and only the remainder gets through.
    ///
    /// Reciprocal (it is symmetric in `l` and `v`, which a BRDF must be), and
    /// measured never to exceed 1: worst hemispherical reflectance **0.9255**
    /// over a full sweep of roughness and view angle (verify_64 §E).
    two_crossing,

    /// `1 - F(v.h)` — what most real-time renderers ship, including the glTF 2.0
    /// reference BRDF.
    ///
    /// **VERIFIED IN LESSON 6.6** against Khronos glTF 2.0, Appendix B "BRDF
    /// Implementation", which gives the dielectric as
    ///
    ///     dielectric_brdf = mix(diffuse_brdf, specular_brdf,
    ///                           0.04 + 0.96 * (1 - |V.H|)^5)
    ///
    /// The claim was true in substance and imprecise in a way worth recording:
    /// `mix(a, b, F)` is `(1-F)a + Fb`, so the SAME Fresnel that scales the
    /// diffuse down scales the specular UP — and `cook_torrance_specular`
    /// already carries that factor. The divergence between this engine and the
    /// spec is therefore exactly ONE factor, on the diffuse lobe alone, and
    /// nothing else in the model differs: same GGX, same height-correlated
    /// Smith, same `alpha = roughness^2`, same 0.04 from an IOR of 1.5.
    ///
    /// **And the metallic blend is not even a divergence.** The spec lerps two
    /// whole BRDFs by `metallic`; `f0_of` lerps the F0 and evaluates one. Those
    /// commute EXACTLY, because Schlick is affine in f0 — `F(f0) = f0(1-w) + w`
    /// — which `verify_66` §E measures at 1.19e-07 worst over 4,851 points.
    ///
    /// The spec permits the remaining difference outright (§3.9.6:
    /// "Implementations of the bidirectional reflectance distribution function
    /// (BRDF) itself **MAY** vary based on device performance and resource
    /// constraints") and requires of a physically accurate one that it be
    /// "positive, reciprocal, and energy conserving" — which is the requirement
    /// `two_crossing` meets at 0.9255 and this form fails at 1.3395. So the
    /// engine loads glTF materials and shades them with `two_crossing`, and is
    /// conformant in doing so.
    ///
    /// **It is exact at normal incidence and over-unity at grazing**, and Lesson
    /// 6.4 §7 diagnoses exactly why: `1 - F(v.h)` is the fraction of light that
    /// got IN. It says nothing about the light that fails to get OUT. A diffuse
    /// ray leaving toward an eye 75 degrees off the normal meets the interface at
    /// 75 degrees, where a quarter of it reflects back inside — and this form
    /// lets all of it out.
    ///
    /// Measured (verify_64 §E): white furnace reads **0.9999** at normal
    /// incidence, which is better than `two_crossing` manages, and **1.2002** for
    /// a smooth surface at 75 degrees, which is a surface emitting a fifth more
    /// light than reaches it. Worst over the sweep: 1.3395.
    ///
    /// Kept, named and measured rather than quietly dropped, because it is what
    /// the student will find in every engine they read next.
    half_vector
};

/// The fraction of arriving light that is available to the diffuse lobe.
///
/// Derived in Lesson 6.4 §6. The sentence the whole lesson turns on is short:
/// **F is what bounces off, so 1 - F is what goes in**, and only what goes in
/// can scatter around under the surface and come back out as diffuse. Before
/// this function the two lobes were independent and their sum was unbounded;
/// after it, every photon is spent once.
///
/// `f0` is the coloured F0 (see `f0_of`), because a metal's coupling is coloured
/// too — though a metal has no diffuse lobe to couple, so in practice this
/// matters only for the partly-metallic values in between.
[[nodiscard]] inline linear_rgb diffuse_transmission(diffuse_coupling coupling,
                                                     linear_rgb f0,
                                                     float n_dot_l, float n_dot_v,
                                                     float v_dot_h)
{
    if (coupling == diffuse_coupling::half_vector)
    {
        const linear_rgb f = fresnel_schlick(v_dot_h, f0);
        return {1.0f - f.r, 1.0f - f.g, 1.0f - f.b};
    }

    // TWO CROSSINGS: in at the light's angle, out at the eye's.
    const linear_rgb fi = fresnel_schlick(n_dot_l, f0);
    const linear_rgb fo = fresnel_schlick(n_dot_v, f0);
    return {(1.0f - fi.r) * (1.0f - fo.r),
            (1.0f - fi.g) * (1.0f - fo.g),
            (1.0f - fi.b) * (1.0f - fo.b)};
}

/// **What a surface is, in four numbers** — the struct that replaces `specular`.
///
/// The rename is the point, exactly as it was when Lesson 6.2 turned
/// `intensity` into `irradiance`. `specular::colour = 0.85f` and `f0 = 0.04f`
/// are both plausible-looking floats, so keeping the old name would let every
/// material in the engine keep compiling while meaning something twenty-one
/// times different. A rename is the only tool that reaches every call site, and
/// the name it reaches them with should say what changed: this struct no longer
/// describes the highlight, it describes THE SURFACE — including, through
/// `metallic`, whether there is a diffuse lobe at all.
///
/// It lives in this header rather than `light.hpp` because these are the
/// parameters of the microfacet model, and this file is the model.
///
/// **It is still not a `material`.** A material is also the albedo, the
/// textures, the cull and blend modes, and eventually the shader; Lesson 6.5
/// builds that, with the arguments for its shape. This is the reflectance half,
/// and it is the half that had to change now because the constants moved.
struct microsurface
{
    /// Perceptual roughness in [0,1]; `alpha_from_roughness` squares it.
    ///
    /// **The parameter that replaces `shininess`, and the difference is that
    /// this one measures something.** 0 is a mirror, 1 is chalk. Lesson 6.3's
    /// `alpha_from_blinn_exponent` translates the old values: the engine's
    /// default shininess of 32 was asking for roughness 0.49.
    float roughness = 0.5f;

    /// 0 = dielectric, 1 = metal. Nothing in between is a real material.
    ///
    /// **This is a switch wearing a float's clothes, and that is deliberate** —
    /// not because half-metals exist, but because a *texture* that says "this
    /// pixel is the painted part and that one is the bare metal" has to filter
    /// across the boundary, and a filtered switch is a float. Lesson 6.6's glTF
    /// materials store exactly this.
    ///
    /// It does two things at once, and both are consequences rather than rules:
    /// a metal's F0 becomes its albedo (`f0_of`), and a metal loses its diffuse
    /// lobe entirely (`diffuse_albedo_of`). Both follow from one fact — metals
    /// conduct, so light that crosses the interface is absorbed within a few
    /// atoms instead of scattering back out.
    float metallic = 0.0f;

    /// Normal-incidence reflectance for the DIELECTRIC case, in [0.02, 0.08].
    ///
    /// Defaults to `k_dielectric_f0`, which covers most of the world. Raise it
    /// for gemstones, lower it for water. Ignored entirely when `metallic` is 1,
    /// because a metal's F0 comes from its albedo instead.
    float f0 = k_dielectric_f0;
};

/// The coloured normal-incidence reflectance this surface actually has.
///
/// **The metallic workflow in one line**, and it arrives as a consequence rather
/// than a checkbox: a dielectric reflects a grey 4% and scatters the rest
/// coloured, so its F0 is grey and its albedo is the colour you see. A metal
/// absorbs everything that gets in, so the only light leaving it is the mirrored
/// part — which means the colour has to live in F0 instead. Gold's albedo IS its
/// F0, (1.00, 0.71, 0.29).
///
/// So one `albedo` field serves both, and `metallic` chooses which question it
/// is answering. That is why every modern pipeline authors base colour +
/// metallic + roughness: it is not a compression, it is the physics.
[[nodiscard]] inline linear_rgb f0_of(microsurface surface, linear_rgb albedo)
{
    const float m = std::clamp(surface.metallic, 0.0f, 1.0f);
    const float d = surface.f0;
    return {d + (albedo.r - d) * m,
            d + (albedo.g - d) * m,
            d + (albedo.b - d) * m};
}

/// The albedo the DIFFUSE lobe gets, which is nothing at all for a metal.
///
/// See `microsurface::metallic`: light that crosses into a conductor is absorbed
/// within a few atomic layers, so there is no subsurface scattering to come back
/// out. A metal is a mirror and nothing else, which is why a rough metal looks
/// dark rather than pale — and why Lesson 6.3's missing 69% at full roughness
/// is a visible problem for metals specifically and barely matters for plastic.
[[nodiscard]] inline linear_rgb diffuse_albedo_of(microsurface surface, linear_rgb albedo)
{
    const float k = 1.0f - std::clamp(surface.metallic, 0.0f, 1.0f);
    return {albedo.r * k, albedo.g * k, albedo.b * k};
}

/// **The Cook-Torrance specular BRDF**: `D * G * F / (4 (n.l)(n.v))`.
///
/// Every part of that expression is derived in Lesson 6.4 §4, including the
/// denominator, which is the single most quoted and least explained line in
/// real-time graphics. In brief, because it is short enough to state here:
///
///   - **the 4 is a Jacobian.** The distribution `D` is a density over
///     MICROFACET NORMALS, and the BRDF is a density over LIGHT DIRECTIONS.
///     Converting between them needs the stretch factor of the map `l -> h`.
///     Put spherical coordinates on the fixed direction: a facet tilted by
///     `theta_h` swings the reflected ray by `2 theta_h`, so
///     `dw_out = sin(2 theta_h) * 2 dtheta_h dphi` against
///     `dw_h = sin(theta_h) dtheta_h dphi`, and the ratio is
///     `2 sin(2 theta_h)/sin(theta_h) = 4 cos(theta_h) = 4 (v.h)`.
///     The double-angle identity supplies both the 4 and the cosine.
///     Measured against a finite-difference change of variables in verify_64 §B.
///
///   - **the (v.h) that Jacobian leaves behind cancels**, against the projected
///     area of the facets toward the light — which is also `(l.h)`, and
///     `l.h == v.h` because `h` bisects them. That cancellation is why the
///     denominator has no `(v.h)` in it, and its absence is what makes the
///     formula look unmotivated.
///
///   - **the (n.v) converts flux to radiance** (radiance is per unit PROJECTED
///     area) and **the (n.l) is the BRDF's own definition** (a BRDF is per unit
///     irradiance, and irradiance already carries a cosine).
///
/// The dots are passed in rather than the vectors, so this function is pure
/// arithmetic that a test can drive directly — and so the shader can compute
/// them once and share them with the diffuse half.
///
/// Returns black rather than a NaN when either cosine is non-positive: light
/// below the horizon and a surface facing away are both real cases, not
/// degeneracies to guard against.
[[nodiscard]] inline linear_rgb cook_torrance_specular(microsurface surface,
                                                       linear_rgb f0,
                                                       float n_dot_l, float n_dot_v,
                                                       float n_dot_h, float v_dot_h,
                                                       ndf_model model = ndf_model::ggx)
{
    if (n_dot_l <= 0.0f || n_dot_v <= 0.0f) { return {}; }

    const float alpha = alpha_from_roughness(surface.roughness);
    const float d = ndf(model, n_dot_h, alpha);
    const float g = smith_g(n_dot_l, n_dot_v, alpha);
    const linear_rgb f = fresnel_schlick(v_dot_h, f0);

    // The whole denominator, computed once. `n_dot_l` and `n_dot_v` are both
    // strictly positive here, so this cannot divide by zero — but they can be
    // small, and `smith_g` goes to zero faster than they do, which is what keeps
    // the quotient finite at grazing angles rather than exploding.
    const float k = d * g / (4.0f * n_dot_l * n_dot_v);
    return {f.r * k, f.g * k, f.b * k};
}

} // namespace engine
