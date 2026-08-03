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

/// The light leaving a surface of colour `albedo`, facing `normal`, under `lights`.
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
/// instead of poisoning the framebuffer.
[[nodiscard]] inline linear_rgb shade(linear_rgb albedo, vec3 normal, const lighting& lights)
{
    const vec3 n = normalised_or(normal, vec3{0.0f, 0.0f, 0.0f});
    const float n_dot_l = lambert(n, lights.key.to_light());

    const float kr = lights.key.colour.r * lights.key.intensity * n_dot_l + lights.ambient.r;
    const float kg = lights.key.colour.g * lights.key.intensity * n_dot_l + lights.ambient.g;
    const float kb = lights.key.colour.b * lights.key.intensity * n_dot_l + lights.ambient.b;

    return {albedo.r * kr, albedo.g * kg, albedo.b * kb};
}

/// The same, taking and returning an encoded colour — the form the demo wants.
///
/// Decode, shade, re-encode. `to_encoded` clamps, which is where an over-bright
/// result gets clipped to white; Module 6's HDR pipeline is precisely the machinery
/// for not throwing that information away.
[[nodiscard]] inline Uint32 shade_encoded(Uint32 albedo_encoded, vec3 normal,
                                          const lighting& lights)
{
    return to_encoded(shade(to_linear(albedo_encoded), normal, lights));
}

} // namespace engine
