// engine/include/engine/audio/sound.hpp — decoded audio, and what it weighs.
//
// Lesson 7.8. The first file in the engine that is not about pictures, and the
// first whose output nobody can look at. That second fact is worth taking
// seriously rather than treating as a joke: every bug in Modules 2 through 7 was
// visible, and a graphics bug that is visible has already told you half of what
// you need. An audio bug announces itself as "it sounds a bit wrong", from a
// thread you cannot breakpoint without changing the result, and the ONLY way
// back to solid ground is to measure the samples. Every claim in this file is
// checked in `scratch/verify_78.cpp`, and the ones that surprised us are noted
// where they live.
//
// ---- THE ASSET / PLAYHEAD SPLIT, FOR THE THIRD TIME ------------------------
//
// Lesson 7.6: a skeleton is shared, a pose is not. Lesson 7.7: a clip is shared,
// its cursor is not. Here: **a sound is shared, its playback position is not** —
// and the third instance is the one that makes the rule feel inevitable rather
// than clever. One footstep sample is four hundred kilobytes and eight feet may
// be walking on it; a `sound` is therefore immutable, has no idea it is playing,
// and does not know how many voices are reading it. Everything mutable lives in
// `mixer`'s voice (audio/mixer.hpp), which is twelve bytes of cursor and some
// gains, exactly as `track_cursor` is twelve bytes beside a megabyte of clip.
//
// The split buys something here it did not buy in either of the other two: the
// voice's cursor is written by the AUDIO THREAD and the sound is not written at
// all, so the only shared mutable state in the whole subsystem is the voice
// table. A design where sounds carried their own playhead would have every
// asset in the game mutable from a real-time thread.
//
// ---- WHY FLOAT, AND WHY ONE FORMAT ----------------------------------------
//
// Files arrive as 8-, 16- and 32-bit integers, signed and unsigned, at half a
// dozen sample rates. The mixer sees exactly one of those: interleaved 32-bit
// float at the device's rate. The conversion happens once, at load, and the
// reasons are the same three that made `framebuffer` one pixel format:
//
//   1. **Headroom.** Mixing is addition (§2 of the lesson) and a sum of things
//      that each fit in [-1, 1] does not. In float the sum is simply a number
//      bigger than one, which the output stage can scale back down; in 16-bit
//      integers it has already wrapped and the wrap is the loudest sound your
//      game can make.
//   2. **One code path.** A mixer that handles five formats has five inner
//      loops, and four of them are tested by nobody.
//   3. **It is what the hardware wants.** Every audio backend SDL3 targets takes
//      float; the integer formats are converted on the way out anyway.
//
// The cost is honest and it is memory: a 16-bit mono WAV doubles when we load
// it, and §2.4 of the lesson measures exactly that on a real file. Engines that
// care keep long music tracks compressed and decode them a buffer at a time,
// which is streaming, which is a Module 9 problem and is named as one.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace engine::audio
{

/// The rate everything in this engine runs at unless a device says otherwise.
///
/// 48 kHz rather than 44.1: it is what every modern device actually runs at
/// (44.1 is a CD-era number that survives in files, not in hardware), so picking
/// it means the common case resamples nothing. `mixer::open` asks the device and
/// uses its answer; this is the fallback and the number `load_wav` is usually
/// handed.
inline constexpr int k_default_freq = 48000;

/// Decoded PCM, interleaved, 32-bit float, and nothing else.
///
/// **Interleaved** — L R L R for stereo — because that is what the device takes
/// and what SDL hands back, so a planar layout would mean a shuffle on every
/// buffer for the benefit of code that does not exist. (The renderer made the
/// opposite call in 7.7, where per-channel storage was the whole point. The
/// difference is that a clip's channels are read independently and a sound's
/// never are.)
///
/// Amplitudes are nominally in [-1, 1]. Nothing enforces it and files do exceed
/// it — `load_wav` counts how far.
struct sound
{
    /// `frames * channels` samples, interleaved.
    std::vector<float> samples;

    /// 1 for mono, 2 for stereo. **Spatialised sounds must be mono** (see
    /// audio/spatial.hpp) and the mixer counts violations rather than guessing.
    int channels = 0;

    /// Sample frames per second. After `load_wav` this is the rate you asked for.
    int freq = 0;

    /// Where it came from, for logs and debug UI. "clip 7" is not a bug report.
    std::string name;

    /// Sample FRAMES — one frame is one sample per channel, so this is what
    /// "how long" is counted in. Confusing frames with samples is the single
    /// most common off-by-two in audio code, and it is silent in mono.
    [[nodiscard]] std::size_t frames() const
    {
        return channels > 0 ? samples.size() / static_cast<std::size_t>(channels) : 0;
    }

    [[nodiscard]] float duration() const
    {
        return freq > 0 ? static_cast<float>(frames()) / static_cast<float>(freq) : 0.0f;
    }

    [[nodiscard]] bool empty() const { return samples.empty(); }
    [[nodiscard]] bool mono() const { return channels == 1; }

    /// What this costs in memory, which is the number nobody looks up until the
    /// build is over budget.
    [[nodiscard]] std::size_t bytes() const { return samples.size() * sizeof(float); }
};

/// What `load_wav` found. Counts rather than a bool, like every other report in
/// this engine (Lesson 5.3), because "how" is the question you ask next.
struct wav_report
{
    /// The file's own format, as SDL named it: "S16LE", "F32LE", "U8"...
    /// A `const char*` from SDL's own table, so it outlives the call.
    const char* source_format = "?";

    int source_channels = 0;        ///< before any conversion
    int source_freq = 0;            ///< before any resample
    std::size_t source_frames = 0;  ///< as stored in the file
    std::size_t source_bytes = 0;   ///< what SDL_LoadWAV allocated

    int out_channels = 0;
    int out_freq = 0;
    std::size_t out_frames = 0;
    std::size_t out_bytes = 0;      ///< what we kept: always 4 bytes a sample

    /// True when the file's rate differed from the one asked for, so the linear
    /// resampler in `sound.cpp` ran. §7 of the lesson measures what that costs in
    /// signal-to-noise, and it is not free.
    bool resampled = false;

    /// Samples whose magnitude exceeded 1.0 in the source. Not an error — float
    /// files legitimately carry them and the mixer has headroom — but a stereo
    /// music track with forty thousand of them is mastered hot, and that is
    /// something you want to know before you add nine other voices to it.
    std::size_t over_unity = 0;

    /// The largest magnitude anywhere in the decoded sound. The one number that
    /// tells you whether a "quiet" sound effect is quiet or merely sounds quiet.
    float peak = 0.0f;

    /// Root-mean-square over the whole sound: the amplitude a constant tone would
    /// need to carry the same ENERGY. Peak says whether it will clip; RMS says
    /// whether it will be heard. §3 of the lesson is about why those diverge.
    float rms = 0.0f;
};

/// Load a RIFF/WAVE file, convert it to float, and resample it to `target_freq`.
///
/// Returns false and logs on a missing or malformed file; `out` is left empty.
/// `report` may be null.
///
/// **WHY WE DO NOT HAND-ROLL THE CONTAINER.** The same rule stb_image earned in
/// 4.7, applied to a format that looks much easier than it is. A minimal WAV
/// reader is forty lines — a RIFF header, a `fmt ` chunk, a `data` chunk — and
/// the lesson's harness writes one to prove it. What it will not read is a file
/// with a `LIST` chunk before the data (every file Audacity exports), a `fact`
/// chunk, `WAVE_FORMAT_EXTENSIBLE` (every file with more than two channels),
/// IEEE float, A-law, or ADPCM. `SDL_LoadWAV` reads all of those, is already
/// linked, and there is nothing about an ENGINE to be learned from the fifth
/// chunk type.
///
/// **THE CONVERSION IS OURS, AND THAT IS ON PURPOSE.** SDL will convert format
/// and rate for you (`SDL_ConvertAudioSamples`) and we call it in exactly one
/// place — the harness, to measure ours against it. The resampler here is the
/// same fractional-step linear interpolation the mixer uses to play a sound at a
/// pitch, written once and used twice, and §7 measures its error honestly:
/// **-39.7 dB against SDL's -86.3 dB** on a 1 kHz tone at 44.1 -> 48 kHz. That
/// gap is real and it is why a shipping engine resamples offline.
bool load_wav(const char* path, int target_freq, sound& out, wav_report* report = nullptr);

/// Fill `out` with a sine wave: the test signal for everything in §3 onward, and
/// the only thing the demo makes a noise with, so that this repository carries no
/// binary audio asset.
///
/// **It starts and ends at a zero crossing only if you are lucky**, and an
/// abrupt start is a click — which is §6's whole subject arriving early. Follow
/// it with `apply_fade` unless you want to hear the discontinuity, and listen to
/// it once without, because that click is the sound of every bug in this lesson.
[[nodiscard]] sound make_tone(float hz, float seconds, int freq = k_default_freq,
                              float amplitude = 0.25f, int channels = 1);

/// Deterministic white noise in [-amplitude, amplitude], from a named seed.
///
/// The fixture for §3's headroom measurement, and it must be UNCORRELATED across
/// voices or the measurement is of something else — so the seed is a parameter
/// and the generator is a plain 32-bit xorshift rather than `std::rand`, whose
/// sequence is not specified and is therefore not reproducible across libraries.
[[nodiscard]] sound make_noise(float seconds, std::uint32_t seed,
                               int freq = k_default_freq, float amplitude = 0.25f,
                               int channels = 1);

/// Ramp the first `fade_in` and last `fade_out` seconds linearly to and from
/// silence, in place.
///
/// Three lines of code and the difference between "a tone" and "a click, a tone,
/// and another click". A jump from 0 to 0.25 in one sample is a step, a step
/// contains every frequency, and every frequency is what a click IS — §6 measures
/// it at **0.2500 of full scale in one sample, against 0.0000052 with a 5 ms
/// fade**, which is a factor of 48,000 and is exactly the fade length in samples.
void apply_fade(sound& s, float fade_in, float fade_out);

/// Peak magnitude and RMS, recomputed. `load_wav` fills these into its report;
/// this is for sounds you built yourself.
void measure(const sound& s, float& peak, float& rms);

}   // namespace engine::audio
