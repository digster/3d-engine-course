// engine/include/engine/gfx/light.hpp — the first light in the engine.
//
// Lesson 3.6. Every surface drawn so far has been coloured by `face_shade` — five
// brightness steps indexed by TRIANGLE NUMBER. It consults no normal, knows about no
// light, and gives itself away completely: **it does not change when the object
// turns.** That is the tell for a fake, and it is the thing this file removes.
//
// What arrives here is the smallest honest model of light hitting a surface:
// Lambert's cosine law. One directional light, one ambient fudge, one multiply. It
// is not physically complete and the places it is wrong are named below rather than
// discovered later — Module 6 replaces the whole thing with a microfacet BRDF and
// image-based lighting, and the argument it makes only lands if you have felt this
// model's limits first.
//
// WHERE THIS RUNS, AND WHY IT IS NOT IN THE RASTERIZER. `fill_style` gathers the
// state the *fill* needs: interpolation, shading source, blend space, cull mode.
// Lighting is not one of those. It happens per VERTEX, before rasterization, and its
// output is the vertex colour the rasterizer already knows how to interpolate. That
// split is not our invention — it is the vertex stage and the fragment stage, and
// Module 4 makes it literal by giving each its own shader. Noticing that our
// pipeline has no vertex stage to put this in is the honest pressure that produces
// one.
//
// LESSON 6.2 CHANGES NO PICTURE AND EVERY MEANING. Everything below computes the
// right numbers for a reason it cannot state: `shade()` multiplies an albedo by a
// light and calls the product "the light leaving the surface", which is a sentence
// with no units in it. 6.2 supplies them. The lamp's scalar becomes an IRRADIANCE
// (watts per square metre, near enough), the surface's response becomes a BRDF
// measured per STERADIAN, and the shading equation becomes the one line the rest of
// Module 6 refines rather than replaces:
//
//     L_o  =  ( f_diffuse + f_specular ) * E_perp * cos(theta)  +  albedo * L_ambient
//
// The pi in `lambert_brdf` was here all along, hiding inside the light's old
// `intensity` field: a lamp of "intensity 1" was a lamp of irradiance pi, and
// nothing in the code said so. Moving it into the open is measured rather than
// asserted (verify_62 §E): over 342,225 sampled channel values, 154,240 change in
// their last one or two bits — and NOT ONE of the resulting eight-bit codes moves,
// through either encoder. That empty diff is not luck. It is the proof of where the
// pi had been.
//
// LESSON 3.7 ADDS THE HIGHLIGHT, and with it two things this file did not have. The
// first is a dependence on where the viewer is standing — everything above 3.7 is
// view-independent, and a highlight is by definition not. The second is a pair of
// per-surface parameters, `specular::colour` and `specular::shininess`, which is the
// point at which "one tint per object" stops being enough and a MATERIAL starts being
// the missing idea. Neither is fixed here; both are named, so Module 6's material
// system arrives as an answer to a question the code has already asked.

#pragma once

#include <engine/gfx/colour.hpp>
#include <engine/math/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace engine {

// ---- Lesson 6.2: the constants a BRDF needs ---------------------------------

/// One over pi, from C++20's `<numbers>`.
///
/// **A C++ note, taught here because this is the first line that needs it.**
/// `std::numbers` is C++20's answer to `M_PI`, which was never standard C++ at all
/// — it is a POSIX extension that MSVC hides behind `_USE_MATH_DEFINES`, which is
/// why every cross-platform codebase eventually grows its own copy of the digits.
/// The `_v<float>` suffix is a **variable template**: `pi_v<T>` is a family of
/// constants, one per type, so `pi_v<float>` is correctly rounded to `float`
/// rather than being a `double` that gets narrowed at the point of use. That
/// distinction is not academic here — this constant is multiplied into every
/// shaded pixel, and a narrowing conversion in a hot loop is a real cost as well
/// as a rounding question.
///
/// It lives in this header rather than a maths one because the BRDFs below are
/// the only things in the engine that need it. The eleven `3.14159265358979f`
/// literals still scattered through the camera and demo code are a separate,
/// smaller debt, named here so it is not forgotten (Exercise 6.2.1).
inline constexpr float k_inv_pi = std::numbers::inv_pi_v<float>;

/// **This engine's exposure, named at last.**
///
/// A renderer has to answer one question before any light value means anything:
/// *what quantity of light corresponds to a fully-lit pixel?* Until this lesson
/// the answer was implicit, and it was "whatever a light of `intensity = 1`
/// produces", which is not an answer, it is a restatement of the question.
///
/// Here is the answer, derived in Lesson 6.2 §5.2. A perfect white Lambertian
/// surface (albedo 1) lit square-on reflects
///
///     L_o = (1 / pi) * E_perp * 1
///
/// so it renders at exactly 1.0 when `E_perp` is **pi**. That is the physical
/// meaning of "correctly exposed" for this engine, and it is the same convention
/// a photographer's white card encodes. It is a *choice*, not a law — Lesson 6.10
/// replaces it with a real exposure control and a tonemapper, at which point
/// lights get authored in lux and this constant becomes the default rather than
/// the rule.
///
/// Checked, not assumed: `pi_v<float> * inv_pi_v<float>` is exactly `1.0f` in IEEE
/// single precision, so a white surface under this light encodes to code 255 with
/// no rounding slack at all (verify_62 §B).
inline constexpr float k_reference_irradiance = std::numbers::pi_v<float>;

/// A light infinitely far away, so every surface sees it arriving from the same
/// direction: the sun, near enough.
///
/// **`direction` is the direction the light TRAVELS**, not the direction to the
/// light. Sunlight at midday travels *downward*, so its direction is `(0, -1, 0)`.
/// This is the single most common sign error in lighting code, and the reason the
/// convention is stated here in capitals and `to_light()` exists at all: the cosine
/// law needs the vector *toward* the source, and asking for it by name is harder to
/// get wrong than remembering to negate.
///
/// The colour is in **linear light** (Lesson 1.6), because it is going to be
/// multiplied. Multiplying sRGB-encoded values is not "half as bright"; it is
/// meaningless. `irradiance` is a separate scalar rather than baked into the colour
/// so that a light's hue and its brightness can be adjusted independently — the same
/// reason every DCC tool separates them.
///
/// **LESSON 6.2 RENAMED `intensity` TO `irradiance`, AND THE RENAME IS THE POINT.**
/// `intensity` meant nothing: it was a number you turned up until the picture
/// looked right. `irradiance` is a measurable physical quantity — the light power
/// landing on each square metre of a surface held **square-on to the beam** — and
/// naming it that way settles three arguments at once. It says what unit the field
/// is in; it says where the cosine goes (the beam is square-on, so a surface at an
/// angle receives `irradiance * cos(theta)`, §2.3); and it makes every call site
/// written before this lesson a **compile error** rather than a silent change of
/// meaning. That last one is deliberate, and it is the same bargain Lesson 3.7 made
/// when it gave `shade()` a `to_eye` parameter with no default: the old value 1.0
/// is not merely stale, it is wrong by a factor of pi, and a rename is the only
/// tool that reaches every caller.
///
/// It defaults to `k_reference_irradiance` (= pi) rather than 1, so that a
/// default-constructed light still means "a white surface facing me renders white".
/// A default of 1.0 would compile everywhere and darken every defaulted scene by
/// 3.14x, which is precisely the silent failure the rename exists to prevent.
struct directional_light
{
    vec3 direction{0.0f, -1.0f, 0.0f};   ///< direction of TRAVEL, unit length
    linear_rgb colour{1.0f, 1.0f, 1.0f}; ///< in linear light

    /// Irradiance on a surface perpendicular to the beam — `E_perp` in §2.3.
    float irradiance = k_reference_irradiance;

    /// The unit vector from a surface **toward** the light — what the cosine law
    /// actually wants. Just the negation, given a name so the negation cannot be
    /// forgotten.
    [[nodiscard]] vec3 to_light() const { return -direction; }

    /// The irradiance actually landing on a surface facing `normal_unit`:
    /// `E_perp * cos(theta)`, the whole of Lambert's cosine law.
    ///
    /// This function exists to give the cosine a *place to live* that is on the
    /// light's side of the equation rather than the surface's, because that is
    /// where it belongs and where §2.3 derives it: the beam spreads over a larger
    /// patch, so less of it lands per square metre. The BRDF — what the surface
    /// does with the light once it has arrived — is a separate question, and the
    /// habit of writing `albedo * n_dot_l` as one expression is exactly what hides
    /// the fact that they are two.
    [[nodiscard]] float irradiance_on(vec3 normal_unit) const
    {
        return irradiance * std::max(0.0f, dot(normal_unit, to_light()));
    }
};

/// Everything illuminating the scene. One light and a constant, for now.
///
/// **`ambient` is a fudge, and calling it anything else would be dishonest.** In a
/// real room a surface facing away from the window is not black: light bounces off
/// the walls, the floor and everything else and arrives from every direction. That
/// bounced light is *global illumination*, it is expensive, and it is Module 6's
/// subject. A constant added everywhere is the cheapest possible stand-in — it gets
/// the "not black" right and everything else wrong, since real bounced light varies
/// with where a surface is and which way it faces.
///
/// It is kept here anyway, at a low value, for a specific reason: with pure Lambert
/// and no ambient, the unlit half of an object is exactly the background colour and
/// the silhouette disappears. That would make this lesson's pictures worse at
/// teaching the thing the lesson is about.
///
/// **LESSON 6.2 PROMOTES IT FROM FUDGE TO MODEL, and this is the lesson's nicest
/// surprise.** Give the word `ambient` a physical reading — *a uniform radiance
/// `L_a` arriving from every direction of the hemisphere* — and the term stops
/// being arbitrary. Put that constant radiance through the Lambert BRDF and
/// integrate over the hemisphere (§4.4):
///
///     L_o = integral of (albedo/pi) * L_a * cos(theta) dw
///         = (albedo/pi) * L_a * pi
///         = albedo * L_a
///
/// **The pi cancels exactly.** So `albedo * ambient` — the expression this file has
/// used since Lesson 3.6 — is not a stand-in for the right answer, it *is* the
/// right answer for a uniform environment, and the constant that has to be
/// remembered everywhere else is the one place it is not needed. What remains wrong
/// with it is not the arithmetic but the assumption: real bounced light is not
/// uniform, it comes mostly from the sky and the floor, and Lesson 6.12 replaces
/// `L_a` with an environment map that says which direction it came from.
struct lighting
{
    directional_light key;

    /// A uniform hemispherical **radiance** — see the note above on why the pi
    /// cancels, and why this field alone needs no BRDF constant applied to it.
    linear_rgb ambient{0.06f, 0.07f, 0.10f};   ///< a cool fill, so shadows are not dead
};

/// **Lambert's cosine law**: how much of a light's power lands on a unit of surface.
///
/// Derived in Lesson 3.6 §3.1 from a picture rather than quoted. A beam of fixed
/// cross-section striking a surface at an angle `theta` from the normal spreads over
/// an area `1/cos(theta)` larger, so the power per unit area falls by `cos(theta)`.
/// That is the whole law. Both vectors must be unit length, and then their dot
/// product IS that cosine (Lesson 1.7).
///
/// **The clamp is not a detail.** A negative dot product means the surface faces
/// *away* from the light — it is in shadow by its own geometry, and the correct
/// answer is zero, not a negative amount of light. Let it through and the far side
/// of every object *subtracts* illumination, which shows up as a black rim eating
/// into the lit region: a distinctive artifact, and one you will now recognise.
///
/// **LESSON 6.2, ON THE NAME.** This function is Lambert's *cosine law* — a fact
/// about how much light ARRIVES. `lambert_brdf` below is Lambert's *BRDF* — a fact
/// about what the surface then DOES with it. They are different quantities with
/// different units (one is dimensionless, the other is per steradian) and they got
/// the same man's name for the same reason two streets do. Writing
/// `albedo * n_dot_l` in one expression, as every introduction does, fuses them and
/// is precisely why the pi has nowhere to go and ends up in the light.
[[nodiscard]] inline float lambert(vec3 normal_unit, vec3 to_light_unit)
{
    return std::max(0.0f, dot(normal_unit, to_light_unit));
}

// ---- Lesson 3.7: the highlight -----------------------------------------------
//
// Everything above is VIEW-INDEPENDENT. Lambert asks only where the surface faces
// and where the light is; move your head and the answer does not change by one bit,
// which Lesson 3.6 measured rather than claimed. That is exactly right for chalk,
// unfinished wood and matte paint, and exactly wrong for almost everything else —
// because a real surface does not scatter light equally in all directions. It sends
// more of it in one PREFERRED direction, and a highlight is what you see when your
// eye happens to be near that direction.
//
// So this is where the shading finally has to know where the viewer is standing.

/// The direction a **perfect mirror** would send light arriving from `to_light_unit`.
///
/// Both arguments point *away* from the surface: `n` out along the normal, `l` out
/// toward the source. Split `l` into its shadow on the normal and the leftover
/// running along the surface, keep the first and flip the second — Lesson 1.8's
/// bounce, and the same three lines of algebra:
///
///     mirror = 2 * dot(n, l) * n - l
///
/// **Note the sign against `reflect`.** `vec3.hpp`'s `reflect(v, n)` mirrors a
/// vector travelling *into* the surface, which is what a ball's velocity does; `l`
/// travels *out* of it. So `mirror_direction(n, l) == -reflect(l, n)`, exactly, and
/// the minus sign is a consequence of which way the arrow points rather than a
/// convention to look up. Both are here because both readings come up, and picking
/// the wrong one puts the highlight on the far side of the object.
///
/// Both inputs must be unit length; the result then is too (verified in Lesson 3.7
/// §3.1 — the algebra cancels to exactly 1).
[[nodiscard]] constexpr vec3 mirror_direction(vec3 normal_unit, vec3 to_light_unit)
{
    return normal_unit * (2.0f * dot(normal_unit, to_light_unit)) - to_light_unit;
}

/// The **halfway vector**: the unit direction exactly between the light and the eye.
///
/// This is not merely a cheap stand-in for the mirror direction, and Lesson 3.7 §3.3
/// derives what it actually is: `h` is *the normal this surface would need in order
/// to bounce the light straight into the eye*. Demanding
/// `mirror_direction(h, l) == v` gives `2*dot(h,l)*h = l + v`, so `h` must lie along
/// `l + v` — and being a unit normal fixes it to exactly `normalise(l + v)`.
///
/// That reframing is the whole reason to prefer it. `dot(n, h)` asks a question about
/// the SURFACE ("how much of it is already oriented to send light at me?") rather than
/// about a reflected ray, and that is the question microfacet theory answers properly
/// in Module 6. Blinn-Phong is the first, crudest microfacet model, and it is worth
/// knowing that before meeting the real ones.
///
/// Returns the zero vector when the eye is exactly opposite the light (`v == -l`),
/// where no normal could do the job. `dot(n, 0) = 0` then kills the term, which is
/// the right answer arriving for free — deliberately, rather than returning `n` and
/// producing a maximal highlight in the one configuration that cannot have one.
[[nodiscard]] inline vec3 halfway(vec3 to_light_unit, vec3 to_eye_unit)
{
    return normalised(to_light_unit + to_eye_unit);
}

/// Which approximation of the highlight to evaluate.
///
/// This is **pipeline state, not a material parameter**: in a real engine the choice
/// is baked into a shader at compile time, and a scene does not mix the two. It is a
/// runtime knob here for exactly one reason — so the two can be rendered side by side
/// and the difference measured (Lesson 3.7 §3.4). That difference is real, it is not
/// only speed, and it is the honest reason Blinn's version won.
enum class specular_model
{
    none,    ///< no highlight at all — Lesson 3.6's picture, bit for bit
    phong,   ///< the original: how closely the eye lines up with the mirror ray
    blinn    ///< Blinn's: how closely the surface lines up with the halfway vector
};

/// How shiny a surface is, and what colour its highlight comes out.
///
/// **These two travel together, always, and they belong to the SURFACE** — not to the
/// light, not to the rasterizer, and not to the fill style. That is a material, and
/// this struct is the first two fields of one. It is not called `material` because it
/// is not one yet: a material is also the albedo, the cull mode, the blend mode, the
/// textures and eventually the shader itself, and inventing four fifths of that here
/// would be guessing. Module 6 builds it properly, with the arguments for its shape.
/// Watch this struct pull the others in; that pull is the design telling you what it
/// wants to be.
///
/// `colour` is a **reflectance ratio in linear light**, like `albedo` — the fraction
/// of arriving light the surface mirrors rather than scatters. Black means "no
/// highlight", which is why it is the default: a caller who has nothing to say about
/// shininess gets Lesson 3.6's shading unchanged.
///
/// `shininess` is the exponent, and it is **not comparable between the two models**:
/// Blinn's must be roughly four times Phong's for the same visual tightness, for the
/// reason derived in §3.4. Larger means a tighter, glossier highlight — 4 is a damp
/// plastic, 32 a polished one, 256 approaching a mirror. It must be positive.
struct specular
{
    linear_rgb colour{0.0f, 0.0f, 0.0f};   ///< reflectance ratio; black = matte
    float shininess = 32.0f;
};

/// Phong's term: how nearly the eye lies along the mirrored ray, raised to a power.
///
/// The literal reading of "a mirror, blurred". A perfect mirror returns light only
/// when `v` equals the mirror direction exactly; a glossy surface returns some of it
/// when `v` is merely *near*. `dot(mirror, v)` is the cosine of how near, and the
/// exponent is how quickly "near" stops counting.
///
/// **Its failure, and it is not what the folklore says** (Lesson 3.7 §3.5). The usual
/// account is that the mirror ray "dips below the surface at grazing angles". It never
/// does: `R` makes the same angle with the normal that `l` does, so it is above the
/// surface whenever the light is. What actually happens is that `cos^p` only answers
/// over the hemisphere *around R*, and that is not the hemisphere you can see from.
/// With the light `a` degrees off the normal, the visible directions the lobe fails to
/// cover form a wedge exactly `a` degrees wide — and an eye anywhere in it gets zero,
/// along a hard boundary. Measured: over all light/eye pairs above a surface,
/// `dot(R,v) <= 0` for 50.4% of them and `dot(n,h) <= 0` for none. Real surfaces have
/// no such edge, and this is what Blinn's version fixes.
[[nodiscard]] inline float phong_term(vec3 normal_unit, vec3 to_light_unit,
                                      vec3 to_eye_unit, float shininess)
{
    const float alignment = dot(mirror_direction(normal_unit, to_light_unit), to_eye_unit);
    if (alignment <= 0.0f) { return 0.0f; }   // also keeps pow() away from 0^0
    // The dot product of two unit vectors is a cosine and cannot exceed 1 — but it
    // can ROUND above it, and `pow` then amplifies the excess instead of absorbing
    // it. Measured: an un-clamped peak reads 1.00001. Clamping here is what makes
    // this function's advertised range, [0,1], a fact rather than an intention; the
    // GPU spells the same thing `saturate` and pays nothing for it.
    return std::pow(std::min(1.0f, alignment), shininess);
}

/// Blinn's term: how nearly the surface already faces the halfway direction.
///
/// Same shape, different question — and the different question is the point, not the
/// saved arithmetic. `h` lies *between* `l` and `v`, so if both are above the surface
/// then so is `h`, so `dot(n, h)` cannot be negative and the cut-off above simply
/// cannot happen. One sentence, and it is the whole reason Blinn's version won.
[[nodiscard]] inline float blinn_term(vec3 normal_unit, vec3 to_light_unit,
                                      vec3 to_eye_unit, float shininess)
{
    const float alignment = dot(normal_unit, halfway(to_light_unit, to_eye_unit));
    if (alignment <= 0.0f) { return 0.0f; }
    return std::pow(std::min(1.0f, alignment), shininess);   // see phong_term on the clamp
}

/// Dispatch to whichever model is selected. Returns a scalar in [0,1].
[[nodiscard]] inline float specular_term(specular_model model, vec3 normal_unit,
                                         vec3 to_light_unit, vec3 to_eye_unit,
                                         float shininess)
{
    switch (model)
    {
    case specular_model::phong: return phong_term(normal_unit, to_light_unit, to_eye_unit, shininess);
    case specular_model::blinn: return blinn_term(normal_unit, to_light_unit, to_eye_unit, shininess);
    case specular_model::none:  break;
    }
    return 0.0f;
}

// ---- Lesson 6.2: the same shading, stated as BRDFs ---------------------------
//
// A **BRDF** is a ratio: the radiance a surface sends toward the eye, divided by
// the irradiance arriving from the light. Two directions in, one number out, and
// its unit is inverse steradians — which is the part worth sitting with, because
// it is why a BRDF may legitimately exceed 1 without any energy being invented.
// A mirror concentrates a whole hemisphere's worth of response into a sliver of
// solid angle; "per steradian" is large exactly when the sliver is small.
//
// Neither function below computes anything the engine was not already computing.
// They give the two halves of `shade()`'s return statement names, units, and —
// for the diffuse half — the constant that makes it conserve energy.

/// **Lambert's BRDF: `albedo / pi`, and the pi is the whole lesson.**
///
/// Derived in Lesson 6.2 §4.2 rather than quoted. A Lambertian surface scatters
/// equally in every direction, so its BRDF is some constant `k` — and the value of
/// `k` is fixed by refusing to let the surface emit more light than it received.
/// Integrate a constant BRDF over the hemisphere, weighted by the cosine that
/// converts radiance into what actually leaves per unit area:
///
///     integral over hemisphere of k * cos(theta) dw  =  k * pi
///
/// A surface that reflects a fraction `albedo` of what arrives must therefore have
/// `k * pi = albedo`, so `k = albedo / pi`. The pi is not a fudge factor and it is
/// not a convention: it is the **area of the projected hemisphere**, and it turns
/// up here for the same reason it turns up in the area of a circle.
///
/// Verified numerically rather than trusted: `verify_62` §C integrates this BRDF
/// over a 1024x2048 grid and gets 1.0000004 for a white surface.
[[nodiscard]] inline linear_rgb lambert_brdf(linear_rgb albedo)
{
    return {albedo.r * k_inv_pi, albedo.g * k_inv_pi, albedo.b * k_inv_pi};
}

/// The Blinn-Phong highlight as a BRDF — on the same scale, and **not normalised**.
///
/// `lobe` is whatever `specular_term()` returned: a bare `cos^shininess`, with no
/// claim to be anything but a shape. Dividing it by pi puts it in the same units as
/// `lambert_brdf` so that one irradiance can multiply both, and that is *all* the
/// division does. It is deliberately not called a normalisation, because it does
/// not normalise anything:
///
///   - The lobe's hemispherical reflectance depends on where the eye is. Measured
///     in `verify_62` §D at shininess 32: **0.1386 with the eye 37 degrees off the
///     normal, 0.0259 at 75 degrees.** The same surface reflects five times as
///     much light one way as the other, and nothing in the model asked for that.
///   - Diffuse and specular are simply added, with no coupling at all. So a white
///     surface with any highlight whatever reflects **more light than arrives** —
///     1.1386 at shininess 32, 1.7333 at shininess 2.
///
/// That second failure is the one Cook-Torrance fixes in Lesson 6.4, and it fixes
/// it with a mechanism, not a constant: Fresnel says what fraction of the arriving
/// light bounces off the surface, and only the remainder is available to scatter
/// diffusely. The diffuse and specular terms stop being independent. This lesson's
/// job is to make that failure *a number you have measured*, so that 6.4 arrives as
/// an answer rather than a formula.
///
/// (Without the `1 / pi` the lobe alone is worse still: `verify_62` §D finds it
/// exceeds unity for every shininess below about 12, peaking at 2.67 at shininess
/// 1. A "rough plastic" would then be a light source.)
///
/// **LESSON 6.3 EXPLAINS THE LOBE AND MEASURES THIS CONSTANT.** `cos^shininess` is
/// not an arbitrary shape: it is a microfacet distribution, and `gfx/microfacet.hpp`
/// gives it its proper normalisation, `(s + 2) / 2pi`. Against that, the `1 / pi`
/// here is off by a factor of `(s + 2) / 2` — **exactly 17x at the default
/// shininess of 32**, which is why this engine's materials have always needed a
/// `specular` colour larger than physical plausibility would suggest. Nothing is
/// changed here yet; Lesson 6.4 replaces this function's specular half outright,
/// with Fresnel supplying the coupling whose absence is measured above.
[[nodiscard]] inline linear_rgb specular_brdf(specular surface, float lobe)
{
    return {surface.colour.r * lobe * k_inv_pi,
            surface.colour.g * lobe * k_inv_pi,
            surface.colour.b * lobe * k_inv_pi};
}

/// The light leaving a surface of colour `albedo`, facing `normal`, seen from
/// `to_eye`, under `lights`.
///
/// `albedo` is the fraction of arriving light the surface reflects, per channel — so
/// it lives in [0,1] and it is a *ratio*, not a colour you can see. A surface with
/// albedo 0.5 under a light of intensity 2 emits 1.0; the same surface in the dark
/// emits nothing. Keeping that straight is most of what makes lighting code behave.
///
/// **Everything here is a multiply, and every multiply is in linear light.** That is
/// the whole of why Lesson 1.6 insisted on the distinction: "half the light" is a
/// statement about photons, and halving an sRGB-encoded byte does not halve them.
/// The caller decodes once, shades, and re-encodes once.
///
/// `normal` need not be unit length — it is normalised here, because after a normal
/// matrix and an interpolation it will not be, and a caller who forgets gets a
/// brightness scaled by the normal's length, which looks like a lighting bug and is
/// not one. `normalised_or` returns the fallback for a zero-length input rather than
/// producing NaN (Lesson 1.7's rule), so a mesh with no normals shades flat black
/// instead of poisoning the framebuffer. `to_eye` is normalised for the same reason:
/// it is `eye_position - surface_position`, and nothing upstream made it unit.
///
/// **`to_eye` HAS NO DEFAULT, on purpose.** Lambert never needed it, so every call
/// site written before Lesson 3.7 is now a compile error rather than a silent
/// highlight in the wrong place — the same bargain Lesson 3.1 made when it inserted
/// `z` ahead of `colour` in `vertex`. The surface parameters *do* default, to a black
/// highlight, so a caller with nothing to say about shininess gets 3.6's picture
/// exactly.
///
/// **Both terms carry the same `n_dot_l` factor**, and that is not a tidying-up. The
/// cosine law is a statement about how much light *arrives* per unit of surface
/// (§3.1 of Lesson 3.6, the spreading beam); it says nothing about what the surface
/// does with it afterwards. So it multiplies the mirrored part exactly as it
/// multiplies the scattered part. Classic Phong shading, as published, left it off
/// the specular — and the artifact is a highlight that survives past the terminator,
/// glowing on geometry the light cannot reach. Including it also means the "clamp the
/// specular where the surface faces away" rule needs no separate code: `lambert()`
/// already returned zero there.
///
/// **LESSON 6.2 REWRITES THE RETURN STATEMENT WITHOUT CHANGING THE PICTURE**, and
/// the rewrite is worth reading twice, because it is the shape every renderer in
/// this module and the next will keep:
///
///     L_o  =  ( f_diffuse + f_specular ) * E_perp * cos(theta)  +  albedo * L_ambient
///
/// Three factors, three separate physical claims, each now with a home in the code:
/// the **BRDF** is what the surface does (`lambert_brdf` + `specular_brdf`, per
/// steradian); **`E_perp * cos(theta)`** is what the light delivers (the lamp's
/// `irradiance`, projected onto this surface); and the ambient term stands apart
/// because its own pi already cancelled (see `lighting::ambient`).
///
/// What used to be written `albedo * (key * intensity * n_dot_l)` is now
/// `(albedo / pi) * (key * pi * n_dot_l)`. Algebraically the same product; the
/// difference is that both halves can now be *checked*, and one of them is checked
/// in `verify_62` §C by integrating it. In floating point the two are NOT identical
/// — re-associating the multiplies moves 154,240 of 342,225 sampled results by one
/// or two ULP, worst relative error 2.465e-07 — and after the 8-bit output stage
/// **not one code moves**, through either encoder. Both halves of that sentence are
/// measurements, and the second one is why the reference render is byte-identical
/// for a thirteenth lesson.
[[nodiscard]] inline linear_rgb shade(linear_rgb albedo, vec3 normal, vec3 to_eye,
                                      const lighting& lights, specular surface = {},
                                      specular_model model = specular_model::blinn)
{
    const vec3 n = normalised_or(normal, vec3{0.0f, 0.0f, 0.0f});
    const vec3 l = lights.key.to_light();
    const float n_dot_l = lambert(n, l);

    // The mirrored part's SHAPE — a bare cos^shininess, not yet a BRDF. Skipped
    // entirely for a black highlight, which is both the common case and the
    // default, so 3.6's scenes pay nothing for 3.7 existing.
    float lobe = 0.0f;
    if (n_dot_l > 0.0f && model != specular_model::none
        && (surface.colour.r > 0.0f || surface.colour.g > 0.0f || surface.colour.b > 0.0f))
    {
        lobe = specular_term(model, n, l, normalised_or(to_eye, vec3{0.0f, 0.0f, 0.0f}),
                             surface.shininess);
    }

    // WHAT THE LIGHT DELIVERS, per channel: irradiance measured square-on to the
    // beam, projected onto this surface by the cosine. This is `E_perp * cos(theta)`
    // and nothing else — no albedo in it, because how much light arrives cannot
    // depend on the colour of what it lands on.
    const float er = lights.key.colour.r * lights.key.irradiance * n_dot_l;
    const float eg = lights.key.colour.g * lights.key.irradiance * n_dot_l;
    const float eb = lights.key.colour.b * lights.key.irradiance * n_dot_l;

    // WHAT THE SURFACE DOES: two lobes, both per steradian, added because a real
    // surface scatters some light and mirrors some and does both at once.
    const linear_rgb f_d = lambert_brdf(albedo);
    const linear_rgb f_s = specular_brdf(surface, lobe);

    // L_o = f_r * E + the ambient term, whose pi cancelled against the hemisphere
    // it was integrated over — which is why `albedo` multiplies it bare.
    return {(f_d.r + f_s.r) * er + albedo.r * lights.ambient.r,
            (f_d.g + f_s.g) * eg + albedo.g * lights.ambient.g,
            (f_d.b + f_s.b) * eb + albedo.b * lights.ambient.b};
}

/// The same, taking and returning an encoded colour — the form the demo wants.
///
/// Decode, shade, re-encode. `to_encoded` clamps, which is where an over-bright
/// result gets clipped to white; Module 6's HDR pipeline is precisely the machinery
/// for not throwing that information away. A specular highlight is the first thing in
/// this engine that routinely exceeds 1.0, so the clamp stops being theoretical here:
/// a bright highlight arrives at the screen as a flat white blob with its shape
/// clipped off, which is exactly the information tonemapping exists to keep.
[[nodiscard]] inline Uint32 shade_encoded(Uint32 albedo_encoded, vec3 normal, vec3 to_eye,
                                          const lighting& lights, specular surface = {},
                                          specular_model model = specular_model::blinn)
{
    return to_encoded(shade(to_linear(albedo_encoded), normal, to_eye, lights, surface, model));
}

} // namespace engine
