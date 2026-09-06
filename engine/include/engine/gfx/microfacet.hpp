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
// WHAT THIS HEADER IS NOT, YET. Nothing here is wired into `shade()`, and that is
// deliberate rather than unfinished: a microfacet BRDF is D, G and F over a
// denominator, and Fresnel is what couples the diffuse and specular lobes back
// together (Lesson 6.4). Shipping two thirds of it live would leave the renderer
// running a model that is neither the old one nor the new one for a lesson. So
// 6.3 builds the parts and tests them; 6.4 assembles and replaces.

#pragma once

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

} // namespace engine
