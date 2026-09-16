// engine/include/engine/audio/spatial.hpp — where a sound is, turned into two gains.
//
// Lesson 7.8. This file is the whole of 3D audio in this engine, and it is
// eighty lines of arithmetic, which is worth saying plainly at the top: the
// difference between "a sound plays" and "a sound is over there" is one
// distance curve and one pair of gains. Everything beyond that — occlusion,
// reverb zones, head-related transfer functions, ambisonics — is a refinement of
// these two numbers, and every one of them still multiplies by them.
//
// ---- A LISTENER IS A TRANSFORM, AND THAT IS THE NEW IDEA -------------------
//
// Six modules of this course have used a placement to decide what is DRAWN. A
// camera is a `transform` (5.9), a light is a `transform` (6.3), a joint is a
// `transform` (7.6). Here a placement decides what is HEARD, and nothing about
// `ecs::camera` or `world_transform` needs to change for it — the listener reads
// the same three axes out of the same quaternion the renderer reads its view
// matrix from. The subsystem that shares nothing with graphics shares its most
// basic type with graphics, which is the clearest evidence this course can offer
// that `transform` was the right thing to build in Module 2.
//
// **The listener is USUALLY the camera and must not be ASSUMED to be.** A
// third-person game hears from the character and looks from behind them, and a
// game that conflates the two has footsteps coming from six metres away. So
// `listener` is its own struct, built from a transform by a named function, and
// the demo sets it from something that is deliberately not the camera.
//
// ---- WHAT THIS FILE DOES NOT DO -------------------------------------------
//
// It computes two gains for a stereo device, by ear-level panning and distance
// attenuation. It does NOT do: elevation (you cannot hear up with two speakers
// and no HRTF), distance-dependent low-pass filtering (air absorbs treble; it is
// two lines and an exercise), Doppler (an exercise, and it is a pitch ratio),
// occlusion, or reverb. This is the ninety-percent picture. The missing ten
// percent begins with HRTFs, which need a measured impulse response per ear per
// direction and a convolution per voice, and the reference at the end of the
// lesson is the place to start.
#pragma once

#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>

#include <cstdint>

namespace engine::audio
{

/// Where the ears are, and which way they face.
///
/// Three orthonormal axes rather than a quaternion, because every consumer below
/// wants the axes and nothing wants the quaternion — `pan_of` is one dot product
/// with `right`, and re-deriving that from a quat on every voice on every buffer
/// would be forty multiplies to avoid storing thirty-six bytes.
///
/// Defaults are the identity placement in this course's world space
/// (conventions.html §2): right-handed, +Y up, **-Z forward**. A listener you
/// forgot to update therefore faces the same way an un-rotated camera does,
/// which puts a sound at the origin dead centre — the least surprising failure
/// available.
struct listener
{
    vec3 position{0.0f, 0.0f, 0.0f};
    vec3 right{1.0f, 0.0f, 0.0f};
    vec3 up{0.0f, 1.0f, 0.0f};
    vec3 forward{0.0f, 0.0f, -1.0f};
};

/// Read a listener out of a world-space placement.
///
/// The axes come from the quaternion exactly as `mat3_from_quat` would build the
/// matrix's columns (7.4): `right` is where x̂ lands, `up` is where ŷ lands, and
/// `forward` is where **-ẑ** lands — note the minus, which is the one place this
/// function can be wrong without looking wrong. A `forward` of +ẑ gives a
/// listener whose left and right are correct and who hears everything behind
/// them as in front, and since this engine does not do front/back distinction
/// with two speakers, **you would not hear the mistake at all** until the day
/// somebody adds a low-pass for sounds behind the head. Scale is ignored: a
/// listener has no size.
[[nodiscard]] listener listener_from(const transform& placement);

/// Which distance curve an emitter uses.
///
/// The names follow OpenAL's, which is the vocabulary every audio person already
/// has, with one addition of our own (`inverse_ranged`) that fixes a real defect
/// in the standard one — see `attenuation`.
enum class falloff : std::uint8_t
{
    /// No attenuation at all. For music, narration, and UI sounds, which are not
    /// in the world and must not get quieter when the player walks away from the
    /// origin. This is not a degenerate case, it is the second most common one.
    none,

    /// The physical law: amplitude falls as `1/d`. Never reaches zero, which is
    /// the defect `inverse_ranged` exists to fix.
    inverse,

    /// Straight line from full at `ref_distance` to silence at `max_distance`.
    /// Not physical and audibly so — it holds up too long and then dies — but it
    /// is bounded, predictable, and what a level designer can reason about with a
    /// radius gizmo, which is why it ships in real games.
    linear,

    /// **Our default.** The inverse curve, rescaled so it reaches exactly zero at
    /// `max_distance` instead of being cut off there. Physical where it matters
    /// (close in, where the curve is steep) and finite where it matters (far
    /// away, where the alternative is a step down to silence). §5 of the lesson
    /// measures the step it removes: **-33.98 dB at the cutoff**, which is quiet
    /// and is still a discontinuity, and a discontinuity is a click.
    inverse_ranged
};

/// A sound in the world.
///
/// Deliberately a plain struct of numbers with no identity: it is an ARGUMENT to
/// `spatialise`, recomputed every frame from wherever the game keeps its truth
/// (an ECS component, a member of a monster). Nothing in the audio system stores
/// one, which means nothing in the audio system can hold a stale position.
struct emitter
{
    vec3 position{0.0f, 0.0f, 0.0f};

    /// The sound's own loudness, before distance. Multiplied in, so 0.5 is half
    /// the amplitude and about **-6 dB**, not half the loudness — see
    /// `amplitude_to_db`, and see §3 for why those are different sentences.
    float gain = 1.0f;

    /// Inside this radius the sound plays at full `gain`. It is the source's
    /// SIZE, roughly: a mosquito has a reference distance of centimetres and a
    /// waterfall has one of tens of metres, and setting it to the default on both
    /// is the most common reason a big sound feels small.
    float ref_distance = 1.0f;

    /// Where the sound ends. `linear` and `inverse_ranged` reach silence here;
    /// `inverse` is clamped here. The mixer uses it for a second job the curve
    /// does not imply: a voice past `max_distance` is inaudible, so it can be
    /// culled before it costs anything (§8).
    float max_distance = 50.0f;

    /// Steepness. 1.0 is the physical inverse law; larger is a sound that dies
    /// faster than physics says it should, which is usually what a mix wants.
    float rolloff = 1.0f;

    falloff law = falloff::inverse_ranged;
};

/// Left and right amplitude multipliers, in that order. Not dB: these get
/// multiplied straight into samples.
struct stereo_gain
{
    float left = 1.0f;
    float right = 1.0f;

    /// `left² + right²` — the POWER the pair delivers, and the quantity the pan
    /// law of §4 exists to hold constant. A pair that sums to 1 in amplitude and
    /// 0.5 in power is 3 dB quiet, and the whole of §4 is that sentence.
    [[nodiscard]] float power() const { return left * left + right * right; }
};

/// Everything `spatialise` worked out, so that a debug panel can show its work.
///
/// Returning the intermediates rather than just the answer is the same decision
/// every `_report` in this engine makes. When a sound is inaudible the question
/// is never "what were the gains" — it is *which of the three factors was zero*,
/// and a function that returns only the product cannot answer it.
struct spatial_result
{
    stereo_gain gain;           ///< the answer: what the mixer multiplies by
    float distance = 0.0f;      ///< listener to emitter, world units
    float pan = 0.0f;           ///< -1 hard left, 0 centre, +1 hard right
    float attenuation = 1.0f;   ///< the distance curve alone, without `emitter::gain`
};

/// The distance curve alone, evaluated at `distance`. In [0, 1].
///
/// **The exponent is 1, not 2, and this is the mistake to get out of the way
/// first.** Energy from a point source spreads over a sphere, so INTENSITY falls
/// as `1/d²` — that part is right and everyone remembers it. But a sample is a
/// PRESSURE, and pressure is the square root of intensity, so amplitude falls as
/// `1/d`. Using `1/d²` on samples gives **-12 dB per doubling instead of -6**,
/// which sounds like every sound in your game is at the bottom of a well. §5
/// measures both curves side by side.
[[nodiscard]] float attenuation(const emitter& e, float distance);

/// Where the emitter sits across the listener's stereo image: `-1` hard left,
/// `0` dead centre, `+1` hard right.
///
/// One dot product with `listener::right`, which is the **sine of the angle off
/// the median plane** and therefore already the thing a pan law wants. Two
/// consequences worth knowing:
///
///   * **Elevation collapses to the centre for free.** A sound directly overhead
///     is perpendicular to `right`, so `pan` is 0 — which is exactly where two
///     speakers can put it, and is what you would have had to special-case if
///     you had used an `atan2` of the ground-plane projection.
///   * **Front and back are indistinguishable.** A sound six metres ahead and
///     one six metres behind produce the same pair of gains. That is not a bug
///     in this function, it is what a stereo pair can express; see this file's
///     header on HRTFs.
///
/// Returns 0 when the emitter is exactly on the listener, because there is no
/// direction there and 0 is the only defensible answer.
[[nodiscard]] float pan_of(const listener& l, vec3 emitter_position);

/// The pan law this engine uses: `L = cos(θ)`, `R = sin(θ)` with `θ` sweeping
/// 0 to π/2 as `pan` goes -1 to +1.
///
/// Holds `left² + right²` at exactly 1 for every pan, which is what "constant
/// power" means and why a sound swept across the image does not dip in the
/// middle. Derived in §4 from the fact that uncorrelated signals add in POWER,
/// not in amplitude.
[[nodiscard]] stereo_gain pan_constant_power(float pan);

/// The pan law nobody should use, kept because §4 measures it: `L + R = 1`.
///
/// Its power at centre is 0.5 — **-3.01 dB** — so a sound panned from left to
/// right audibly ducks as it crosses. This is not a straw man; it is what you
/// get by writing the first thing that occurs to you, and it is what most first
/// mixers do.
[[nodiscard]] stereo_gain pan_linear(float pan);

/// Distance, pan, and the gains, for one emitter heard by one listener.
///
/// The composition is one multiply: `gain = emitter.gain * attenuation(d) * pan`.
/// Nothing here is stateful and nothing is remembered, which is what makes the
/// mixer's job the interesting one — these gains change every frame and the
/// mixer has to get from one to the next without a click (§6).
[[nodiscard]] spatial_result spatialise(const listener& l, const emitter& e);

/// `20 log10(a)`, floored at -120 dB so that silence is a number rather than
/// negative infinity.
///
/// **Twenty, not ten**, and the factor of two is the same one as above: decibels
/// are defined on POWER, power goes as amplitude squared, and `log(a²) = 2log(a)`.
/// The consequences to keep in your head: **halving an amplitude is -6.02 dB**,
/// and a tenth of the amplitude is -20 dB and is roughly a quarter as loud, not a
/// tenth — loudness is a perceptual quantity and it is closer to the cube root of
/// intensity than to intensity.
[[nodiscard]] float amplitude_to_db(float amplitude);

/// The inverse: `10^(db/20)`. `-6.02 dB` gives 0.5, `-20 dB` gives 0.1.
[[nodiscard]] float db_to_amplitude(float db);

}   // namespace engine::audio
