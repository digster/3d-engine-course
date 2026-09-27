// src/gfx/light.hpp — the first light in the engine.
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
// LESSON 3.7 ADDS THE HIGHLIGHT, and with it two things this file did not have. The
// first is a dependence on where the viewer is standing — everything above 3.7 is
// view-independent, and a highlight is by definition not. The second is a pair of
// per-surface parameters, `specular::colour` and `specular::shininess`, which is the
// point at which "one tint per object" stops being enough and a MATERIAL starts being
// the missing idea. Neither is fixed here; both are named, so Module 6's material
// system arrives as an answer to a question the code has already asked.

#pragma once

#include "gfx/colour.hpp"
#include "math/vec3.hpp"

#include <algorithm>
#include <cmath>

namespace engine {

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
/// meaningless. `intensity` is a separate scalar rather than baked into the colour
/// so that a light's hue and its brightness can be adjusted independently — the same
/// reason every DCC tool separates them.
struct directional_light
{
    vec3 direction{0.0f, -1.0f, 0.0f};   ///< direction of TRAVEL, unit length
    linear_rgb colour{1.0f, 1.0f, 1.0f}; ///< in linear light
    float intensity = 1.0f;

    /// The unit vector from a surface **toward** the light — what the cosine law
    /// actually wants. Just the negation, given a name so the negation cannot be
    /// forgotten.
    [[nodiscard]] vec3 to_light() const { return -direction; }
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
struct lighting
{
    directional_light key;
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
[[nodiscard]] inline linear_rgb shade(linear_rgb albedo, vec3 normal, vec3 to_eye,
                                      const lighting& lights, specular surface = {},
                                      specular_model model = specular_model::blinn)
{
    const vec3 n = normalised_or(normal, vec3{0.0f, 0.0f, 0.0f});
    const vec3 l = lights.key.to_light();
    const float n_dot_l = lambert(n, l);

    // The mirrored part. Skipped entirely for a black highlight, which is both the
    // common case and the default — so 3.6's scenes pay nothing for 3.7 existing.
    float spec = 0.0f;
    if (n_dot_l > 0.0f && model != specular_model::none
        && (surface.colour.r > 0.0f || surface.colour.g > 0.0f || surface.colour.b > 0.0f))
    {
        spec = specular_term(model, n, l, normalised_or(to_eye, vec3{0.0f, 0.0f, 0.0f}),
                             surface.shininess);
    }

    // Arriving light, per channel: the key scaled by the cosine, plus the ambient
    // fudge. The diffuse part multiplies it by the albedo and the specular part by
    // the highlight colour, which is why they cannot simply be added first.
    const float ir = lights.key.colour.r * lights.key.intensity * n_dot_l;
    const float ig = lights.key.colour.g * lights.key.intensity * n_dot_l;
    const float ib = lights.key.colour.b * lights.key.intensity * n_dot_l;

    return {albedo.r * (ir + lights.ambient.r) + surface.colour.r * ir * spec,
            albedo.g * (ig + lights.ambient.g) + surface.colour.g * ig * spec,
            albedo.b * (ib + lights.ambient.b) + surface.colour.b * ib * spec};
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
