// engine/src/audio/sound.cpp — loading, converting, resampling.
//
// Lesson 7.8. Three jobs, in the order they happen:
//
//   1. SDL_LoadWAV parses the container and hands back bytes in the file's own
//      format.
//   2. `to_float` widens those bytes to [-1, 1] floats. Six source formats, one
//      destination, and the whole thing is a divide by the format's full scale.
//   3. `resample_linear` moves them to the device's rate, if they are not there
//      already.
//
// Step 3 is the one with a real cost and §7 of the lesson measures it. Steps 1
// and 2 are bookkeeping, and the bookkeeping is where the off-by-two lives:
// everything below counts in FRAMES where it means frames and in SAMPLES where
// it means samples, and never uses one word for both.

#include <engine/audio/sound.hpp>

#include <engine/core/assert.hpp>
#include <engine/core/log.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

namespace engine::audio
{
namespace
{

/// Widen one sample of any SDL integer or float format into [-1, 1].
///
/// **The asymmetry in the signed cases is not a mistake.** A 16-bit signed sample
/// runs from -32768 to +32767, so dividing by 32768 maps the most negative value
/// to exactly -1.0 and the most positive to 0.99997. The alternative — divide by
/// 32767 — maps +full-scale to 1.0 and -full-scale to -1.00003, which is a value
/// outside the stated range on a file that did nothing wrong. Every audio API in
/// existence picks the first, and the three-hundred-millionths of a decibel it
/// costs at the top end is the correct thing to spend.
float widen(const std::uint8_t* bytes, SDL_AudioFormat fmt)
{
    switch (fmt)
    {
        case SDL_AUDIO_U8:
        {
            // Unsigned 8-bit is centred on 128, not 0 — the format predates the
            // idea that silence should be zero. Getting this wrong does not
            // sound quiet, it sounds like a very loud DC offset, which most
            // speakers reproduce as a thump and then nothing.
            return (static_cast<float>(bytes[0]) - 128.0f) / 128.0f;
        }
        case SDL_AUDIO_S8:
        {
            return static_cast<float>(static_cast<std::int8_t>(bytes[0])) / 128.0f;
        }
        case SDL_AUDIO_S16LE:
        case SDL_AUDIO_S16BE:
        {
            std::uint16_t raw = 0;
            std::memcpy(&raw, bytes, sizeof(raw));
            raw = SDL_AUDIO_ISBIGENDIAN(fmt) ? SDL_Swap16BE(raw) : SDL_Swap16LE(raw);
            return static_cast<float>(static_cast<std::int16_t>(raw)) / 32768.0f;
        }
        case SDL_AUDIO_S32LE:
        case SDL_AUDIO_S32BE:
        {
            std::uint32_t raw = 0;
            std::memcpy(&raw, bytes, sizeof(raw));
            raw = SDL_AUDIO_ISBIGENDIAN(fmt) ? SDL_Swap32BE(raw) : SDL_Swap32LE(raw);
            return static_cast<float>(static_cast<std::int32_t>(raw)) / 2147483648.0f;
        }
        case SDL_AUDIO_F32LE:
        case SDL_AUDIO_F32BE:
        {
            std::uint32_t raw = 0;
            std::memcpy(&raw, bytes, sizeof(raw));
            raw = SDL_AUDIO_ISBIGENDIAN(fmt) ? SDL_Swap32BE(raw) : SDL_Swap32LE(raw);
            float value = 0.0f;
            // memcpy, not a reinterpret_cast: type punning through a pointer is
            // undefined behaviour and this is the one spelling every compiler
            // turns into zero instructions.
            std::memcpy(&value, &raw, sizeof(value));
            return value;
        }
        default:
            return 0.0f;
    }
}

/// Resample by walking a fractional cursor and interpolating between neighbours.
///
/// **This is the mixer's pitch loop, written once.** `mixer.cpp` does exactly
/// this per voice per buffer with `step = pitch * src_freq / device_freq`; here
/// the step is `src_freq / dst_freq` and the whole sound is done at once. Two
/// copies of an interpolation rule is two copies that eventually disagree, and
/// §7's measurement of this function's error is therefore also a measurement of
/// what pitching a sound costs.
///
/// **What it is NOT is a good resampler**, and the lesson says so with a number
/// rather than a disclaimer. Linear interpolation is a two-tap filter; a correct
/// one is a windowed sinc with dozens of taps and a design procedure. What
/// linear buys is that you can see why it works.
void resample_linear(const std::vector<float>& src, int channels,
                     int src_freq, int dst_freq, std::vector<float>& dst)
{
    const std::size_t src_frames =
        channels > 0 ? src.size() / static_cast<std::size_t>(channels) : 0;
    if (src_frames == 0 || channels <= 0 || src_freq <= 0 || dst_freq <= 0)
    {
        dst.clear();
        return;
    }

    const double ratio = static_cast<double>(dst_freq) / static_cast<double>(src_freq);
    // Round rather than truncate: at 44100 -> 48000 a one-second file is
    // 48000.0 frames exactly, but 0.5 s is 24000.0 and 0.3 s is 14400.0000001,
    // and truncating the last of those loses a frame for no reason.
    const std::size_t dst_frames =
        static_cast<std::size_t>(static_cast<double>(src_frames) * ratio + 0.5);

    dst.assign(dst_frames * static_cast<std::size_t>(channels), 0.0f);

    const double step = static_cast<double>(src_freq) / static_cast<double>(dst_freq);
    for (std::size_t f = 0; f < dst_frames; ++f)
    {
        const double pos = static_cast<double>(f) * step;
        const std::size_t i0 = static_cast<std::size_t>(pos);
        const std::size_t i1 = std::min(i0 + 1, src_frames - 1);
        const float t = static_cast<float>(pos - static_cast<double>(i0));

        for (int c = 0; c < channels; ++c)
        {
            const float a = src[i0 * static_cast<std::size_t>(channels)
                                + static_cast<std::size_t>(c)];
            const float b = src[i1 * static_cast<std::size_t>(channels)
                                + static_cast<std::size_t>(c)];
            dst[f * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c)] =
                a + (b - a) * t;
        }
    }
}

/// xorshift32. Named, seeded, and reproducible on every platform — which
/// `std::rand` is not, and §3's headroom table is a comparison across runs.
std::uint32_t xorshift(std::uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

}   // namespace

void measure(const sound& s, float& peak, float& rms)
{
    peak = 0.0f;
    double energy = 0.0;
    for (float v : s.samples)
    {
        peak = std::max(peak, std::abs(v));
        energy += static_cast<double>(v) * static_cast<double>(v);
    }
    // The accumulator is a double on purpose. A float sum over a million squared
    // samples loses the small ones entirely once the running total is large —
    // the classic catastrophic-absorption failure, and it biases RMS DOWNWARD,
    // which is the direction that makes a loud sound look safe.
    rms = s.samples.empty()
              ? 0.0f
              : static_cast<float>(std::sqrt(energy / static_cast<double>(s.samples.size())));
}

bool load_wav(const char* path, int target_freq, sound& out, wav_report* report)
{
    out = sound{};
    if (report) { *report = wav_report{}; }

    ENGINE_ASSERT(path != nullptr);
    if (target_freq <= 0) { target_freq = k_default_freq; }

    SDL_AudioSpec spec{};
    std::uint8_t* raw = nullptr;
    std::uint32_t raw_len = 0;

    // SDL_LoadWAV does NOT need SDL_INIT_AUDIO: it parses a file and allocates a
    // buffer, and touches no device. That is worth knowing because it means an
    // asset-cooking tool with no sound card can call this, and Module 9's
    // pipeline will.
    if (!SDL_LoadWAV(path, &spec, &raw, &raw_len))
    {
        ENGINE_LOG_ERROR(log_audio, "load_wav: %s: %s", path, SDL_GetError());
        return false;
    }

    const int bytes_per_sample = SDL_AUDIO_BYTESIZE(spec.format);
    const int channels = spec.channels;
    if (bytes_per_sample <= 0 || channels <= 0)
    {
        ENGINE_LOG_ERROR(log_audio, "load_wav: %s: unusable format %s / %d ch",
                         path, SDL_GetAudioFormatName(spec.format), channels);
        SDL_free(raw);
        return false;
    }

    const std::size_t src_samples =
        static_cast<std::size_t>(raw_len) / static_cast<std::size_t>(bytes_per_sample);
    const std::size_t src_frames = src_samples / static_cast<std::size_t>(channels);

    std::vector<float> widened(src_samples, 0.0f);
    std::size_t over_unity = 0;
    for (std::size_t i = 0; i < src_samples; ++i)
    {
        const float v = widen(raw + i * static_cast<std::size_t>(bytes_per_sample), spec.format);
        widened[i] = v;
        if (std::abs(v) > 1.0f) { ++over_unity; }
    }

    if (report)
    {
        report->source_format = SDL_GetAudioFormatName(spec.format);
        report->source_channels = channels;
        report->source_freq = spec.freq;
        report->source_frames = src_frames;
        report->source_bytes = raw_len;
        report->over_unity = over_unity;
    }

    SDL_free(raw);

    out.channels = channels;
    out.freq = target_freq;
    out.name = path;

    if (spec.freq == target_freq)
    {
        out.samples = std::move(widened);
    }
    else
    {
        resample_linear(widened, channels, spec.freq, target_freq, out.samples);
        if (report) { report->resampled = true; }
    }

    if (report)
    {
        report->out_channels = out.channels;
        report->out_freq = out.freq;
        report->out_frames = out.frames();
        report->out_bytes = out.bytes();
        measure(out, report->peak, report->rms);
    }

    ENGINE_LOG_INFO(log_audio,
                    "load_wav: %s  %s %dch %d Hz -> f32 %dch %d Hz  %zu frames  %zu KB",
                    path, SDL_GetAudioFormatName(spec.format), channels, spec.freq,
                    out.channels, out.freq, out.frames(), out.bytes() / 1024);
    return true;
}

sound make_tone(float hz, float seconds, int freq, float amplitude, int channels)
{
    sound s;
    s.channels = std::max(1, channels);
    s.freq = freq > 0 ? freq : k_default_freq;
    s.name = "tone";

    const std::size_t frames =
        static_cast<std::size_t>(std::max(0.0f, seconds) * static_cast<float>(s.freq));
    s.samples.assign(frames * static_cast<std::size_t>(s.channels), 0.0f);

    // The phase accumulates in a double. At 48 kHz a float phase has lost its
    // last decimal digit after about thirty seconds, which drifts the pitch of
    // long tones — inaudible on a beep, obvious on a sustained note, and exactly
    // the kind of bug that only shows up in the content nobody tests with.
    const double w = 2.0 * std::numbers::pi * static_cast<double>(hz)
                     / static_cast<double>(s.freq);
    for (std::size_t f = 0; f < frames; ++f)
    {
        const float v = amplitude * static_cast<float>(std::sin(w * static_cast<double>(f)));
        for (int c = 0; c < s.channels; ++c)
        {
            s.samples[f * static_cast<std::size_t>(s.channels) + static_cast<std::size_t>(c)] = v;
        }
    }
    return s;
}

sound make_noise(float seconds, std::uint32_t seed, int freq, float amplitude, int channels)
{
    sound s;
    s.channels = std::max(1, channels);
    s.freq = freq > 0 ? freq : k_default_freq;
    s.name = "noise";

    const std::size_t frames =
        static_cast<std::size_t>(std::max(0.0f, seconds) * static_cast<float>(s.freq));
    s.samples.assign(frames * static_cast<std::size_t>(s.channels), 0.0f);

    // A zero seed is a fixed point of xorshift — it produces zeros forever, which
    // is silence, which looks exactly like "the mixer is broken". Substituting a
    // constant is the right fix and the comment is the important half.
    std::uint32_t state = seed != 0 ? seed : 0x9e3779b9u;
    for (float& v : s.samples)
    {
        // >> 8 keeps the top 24 bits: the low bits of an xorshift word are the
        // weakest, and 24 bits is more resolution than any consumer DAC has.
        const std::uint32_t bits = xorshift(state) >> 8;
        const float unit = static_cast<float>(bits) / 8388608.0f - 1.0f;   // [-1, 1)
        v = unit * amplitude;
    }
    return s;
}

void apply_fade(sound& s, float fade_in, float fade_out)
{
    if (s.channels <= 0 || s.freq <= 0 || s.samples.empty()) { return; }

    const std::size_t total = s.frames();
    const std::size_t in_frames = std::min(
        total, static_cast<std::size_t>(std::max(0.0f, fade_in) * static_cast<float>(s.freq)));
    const std::size_t out_frames = std::min(
        total, static_cast<std::size_t>(std::max(0.0f, fade_out) * static_cast<float>(s.freq)));

    const auto ch = static_cast<std::size_t>(s.channels);

    for (std::size_t f = 0; f < in_frames; ++f)
    {
        const float g = static_cast<float>(f) / static_cast<float>(in_frames);
        for (std::size_t c = 0; c < ch; ++c) { s.samples[f * ch + c] *= g; }
    }
    for (std::size_t f = 0; f < out_frames; ++f)
    {
        const float g = static_cast<float>(f) / static_cast<float>(out_frames);
        const std::size_t frame = total - 1 - f;
        for (std::size_t c = 0; c < ch; ++c) { s.samples[frame * ch + c] *= g; }
    }
}

}   // namespace engine::audio
