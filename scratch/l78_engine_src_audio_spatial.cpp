// engine/src/audio/spatial.cpp — the distance curve and the pan law.
//
// Lesson 7.8. Short, and every line of it is derived in the lesson rather than
// copied from a reference implementation. The two things to hold on to while
// reading:
//
//   * amplitude falls as 1/d, because a sample is a pressure and pressure is the
//     square root of intensity;
//   * gains combine in POWER, because two speakers playing the same signal are
//     not two speakers playing the same sound.
//
// Both are factors of two in an exponent, both are invisible in the code, and
// both are audible.

#include <engine/audio/spatial.hpp>

#include <engine/math/quat.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace engine::audio
{
namespace
{

/// The floor for `amplitude_to_db`. -120 dB is about a millionth of full scale,
/// which is below the noise floor of any consumer device and comfortably below
/// the resolution of 16-bit output (-96 dB). Choosing a floor rather than
/// returning -inf is a convenience for the debug panel, which has to print it.
inline constexpr float k_db_floor = -120.0f;

}   // namespace

listener listener_from(const transform& placement)
{
    listener l;
    l.position = placement.position;
    l.right    = rotate(placement.rotation, vec3{1.0f, 0.0f, 0.0f});
    l.up       = rotate(placement.rotation, vec3{0.0f, 1.0f, 0.0f});
    // THE MINUS. conventions.html §2: this course's world space is right-handed,
    // Y-up, -Z forward, so the direction a placement faces is where -ẑ lands.
    l.forward  = rotate(placement.rotation, vec3{0.0f, 0.0f, -1.0f});
    return l;
}

float attenuation(const emitter& e, float distance)
{
    // A reference distance of zero is a division by zero one frame later, and the
    // value it would want to produce (infinite gain at the source) is not a value.
    // Clamping rather than asserting: a designer typing 0 into a field is the
    // world misbehaving, not the program (Lesson 5.3's rule).
    const float ref = std::max(e.ref_distance, 1e-4f);
    const float max_d = std::max(e.max_distance, ref);
    const float d = std::max(distance, 0.0f);

    switch (e.law)
    {
        case falloff::none:
            return 1.0f;

        case falloff::inverse:
        {
            // OpenAL's inverse-distance-clamped, and the shape every other
            // engine's "logarithmic" rolloff also is. Clamp d into [ref, max]
            // first: below ref it would exceed 1 (a sound louder than itself),
            // above max it would keep going forever.
            const float dc = std::clamp(d, ref, max_d);
            return ref / (ref + e.rolloff * (dc - ref));
        }

        case falloff::linear:
        {
            const float dc = std::clamp(d, ref, max_d);
            const float t = (dc - ref) / (max_d - ref);
            return std::max(0.0f, 1.0f - e.rolloff * t);
        }

        case falloff::inverse_ranged:
        {
            // The inverse curve, affinely rescaled so that it is 1 at `ref` and
            // exactly 0 at `max`. `g_max` is what the bare inverse law would have
            // produced at the far edge — 0.0200 for the default 1 m / 50 m pair,
            // which is -33.98 dB — and subtracting it is what turns a cliff into
            // a landing. The division by (1 - g_max) restores the value at ref.
            const float dc = std::clamp(d, ref, max_d);
            const float g = ref / (ref + e.rolloff * (dc - ref));
            const float g_max = ref / (ref + e.rolloff * (max_d - ref));
            // g_max < 1 always, because max_d >= ref and rolloff >= 0; the guard
            // is for a rolloff of exactly 0, where the curve is flat and there is
            // nothing to rescale.
            const float span = 1.0f - g_max;
            if (span <= 1e-6f) { return d >= max_d ? 0.0f : 1.0f; }
            return std::max(0.0f, (g - g_max) / span);
        }
    }
    return 1.0f;
}

float pan_of(const listener& l, vec3 emitter_position)
{
    const vec3 to_source = emitter_position - l.position;
    const float len_sq = length_squared(to_source);
    if (len_sq <= 1e-12f) { return 0.0f; }

    const vec3 dir = to_source / std::sqrt(len_sq);
    // `right` is assumed unit — `listener_from` guarantees it, and a listener
    // assembled by hand from a non-unit axis gets a pan outside [-1, 1], which
    // the clamp below absorbs rather than propagating into a NaN in the acos-free
    // pan law.
    return std::clamp(dot(dir, l.right), -1.0f, 1.0f);
}

stereo_gain pan_constant_power(float pan)
{
    const float p = std::clamp(pan, -1.0f, 1.0f);
    // Map [-1, 1] onto [0, pi/2]. At p = 0 this is pi/4, where cos and sin are
    // both 1/sqrt(2) = 0.7071 — the two gains that sum to 1.4142 in amplitude and
    // to exactly 1 in power. That 0.7071 is the whole answer to "why does my
    // centre sound quiet": the naive law puts 0.5 there.
    const float theta = (p + 1.0f) * 0.25f * std::numbers::pi_v<float>;
    return {std::cos(theta), std::sin(theta)};
}

stereo_gain pan_linear(float pan)
{
    const float p = std::clamp(pan, -1.0f, 1.0f);
    const float u = (p + 1.0f) * 0.5f;
    return {1.0f - u, u};
}

spatial_result spatialise(const listener& l, const emitter& e)
{
    spatial_result r;
    r.distance = distance(l.position, e.position);
    r.attenuation = attenuation(e, r.distance);
    r.pan = pan_of(l, e.position);

    const stereo_gain p = pan_constant_power(r.pan);
    const float a = e.gain * r.attenuation;
    r.gain = {p.left * a, p.right * a};
    return r;
}

float amplitude_to_db(float amplitude)
{
    const float a = std::abs(amplitude);
    if (a <= 1e-6f) { return k_db_floor; }
    return std::max(k_db_floor, 20.0f * std::log10(a));
}

float db_to_amplitude(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

}   // namespace engine::audio
