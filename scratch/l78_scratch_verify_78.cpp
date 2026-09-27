// scratch/verify_78.cpp — every number Lesson 7.8 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_78.sh
//
// Nine sections, in the lesson's order:
//
//   A  the container: a WAV written by hand, read back, and resampled
//   B  mixing is addition, and that is not a metaphor
//   C  headroom: N voices are sqrt(N) loud, not N loud
//   D  what clipping actually does to a waveform
//   E  the two pan laws, and the hole in the middle of one
//   F  distance: 1/d, 1/d^2, and the cliff at max_distance
//   G  the gain step against the gain ramp
//   H  resampling: ours against SDL's, in decibels
//   I  the budget: what a buffer costs and what the lock costs
//
// EVERY SECTION CARRIES A CONTROL, for the reason 7.1 states and 7.2 through 7.7
// repeat: a check whose degenerate case is a pass is not a check. 7.6 sharpened
// it into a question — ASK WHAT THE CONTROL WOULD SAY IF THE THING WERE
// COMPLETELY BROKEN — and this file has two written directly against that. C.4
// mixes IDENTICAL voices, where the peak genuinely does grow as N, so the sqrt(N)
// result in C.2 cannot be an artefact of the meter. G.4 holds the gain CONSTANT,
// where a ramp and a step must agree bit for bit, so the difference in G.2 cannot
// be an artefact of the two code paths.
//
// NOTHING HERE OPENS A DEVICE. `mixer::open_offline` builds the voice table and
// nothing else, and `mix_into` is then a pure function from that table to an
// array of floats. Every waveform in this lesson was produced on a machine that
// was never asked whether it had speakers, and that is the point rather than a
// convenience: a real-time callback is the worst place in a program to learn
// anything.
//
// OUTPUT WIDTH IS A CONSTRAINT, not a preference. The lesson quotes this
// program's transcript in <pre> blocks that scroll and never wrap, and the fold
// is at about 66 characters. Nothing below exceeds 66.

#include <engine/audio/mixer.hpp>
#include <engine/audio/sound.hpp>
#include <engine/audio/spatial.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <numbers>
#include <string>
#include <vector>

using engine::quat;
using engine::transform;
using engine::vec3;
using engine::audio::amplitude_to_db;
using engine::audio::attenuation;
using engine::audio::emitter;
using engine::audio::falloff;
using engine::audio::listener;
using engine::audio::listener_from;
using engine::audio::mixer;
using engine::audio::mixer_config;
using engine::audio::mixer_report;
using engine::audio::pan_constant_power;
using engine::audio::pan_linear;
using engine::audio::pan_of;
using engine::audio::sound;
using engine::audio::spatial_result;
using engine::audio::spatialise;
using engine::audio::stereo_gain;
using engine::audio::voice_id;
using engine::audio::wav_report;

namespace
{

constexpr int k_freq = 48000;

void rule(const char* title)
{
    std::printf("\n%s\n", title);
    std::printf("------------------------------------------------------------\n");
}

// ---------------------------------------------------------------------------
// Measuring tools
// ---------------------------------------------------------------------------

/// Peak magnitude and RMS over a raw span.
void stats(const float* x, std::size_t n, double& peak, double& rms)
{
    peak = 0.0;
    double energy = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const double v = static_cast<double>(x[i]);
        peak = std::max(peak, std::abs(v));
        energy += v * v;
    }
    rms = n > 0 ? std::sqrt(energy / static_cast<double>(n)) : 0.0;
}

/// Power at exactly `cycles` periods per window, by the Goertzel recurrence.
///
/// Used instead of a full DFT because we only ever want a handful of bins, and
/// used with an INTEGER number of periods in the window so that there is no
/// spectral leakage to argue about — an off-bin frequency would smear energy
/// into its neighbours and every SNR below would be a measurement of the
/// window rather than of the signal.
double goertzel_power(const float* x, std::size_t n, double cycles)
{
    const double w = 2.0 * std::numbers::pi * cycles / static_cast<double>(n);
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0.0;
    double s2 = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const double s0 = static_cast<double>(x[i]) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double real = s1 - s2 * std::cos(w);
    const double imag = s2 * std::sin(w);
    // Normalised so that a unit-amplitude sine gives a power of 0.5, which is
    // its mean square — the same convention `stats` reports RMS in.
    const double mag2 = (real * real + imag * imag) / (0.25 * static_cast<double>(n)
                                                       * static_cast<double>(n));
    return mag2 * 0.5;
}

double to_db_power(double ratio)
{
    return ratio <= 0.0 ? -200.0 : 10.0 * std::log10(ratio);
}

/// Total mean-square energy of a span.
double mean_square(const float* x, std::size_t n)
{
    double e = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        e += static_cast<double>(x[i]) * static_cast<double>(x[i]);
    }
    return n > 0 ? e / static_cast<double>(n) : 0.0;
}

/// Largest jump between adjacent samples of one channel. The instrument for §G:
/// a click IS a discontinuity, and this is the smallest honest measure of one.
double max_step(const float* x, std::size_t frames, int channel)
{
    double worst = 0.0;
    for (std::size_t f = 1; f < frames; ++f)
    {
        const double a = static_cast<double>(x[(f - 1) * 2 + static_cast<std::size_t>(channel)]);
        const double b = static_cast<double>(x[f * 2 + static_cast<std::size_t>(channel)]);
        worst = std::max(worst, std::abs(b - a));
    }
    return worst;
}

// ---------------------------------------------------------------------------
// A WAV file, written by hand
// ---------------------------------------------------------------------------

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    out.push_back(static_cast<std::uint8_t>(v & 0xffu));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xffu));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xffu));
}

void put_u16(std::vector<std::uint8_t>& out, std::uint16_t v)
{
    out.push_back(static_cast<std::uint8_t>(v & 0xffu));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xffu));
}

void put_tag(std::vector<std::uint8_t>& out, const char* tag)
{
    for (int i = 0; i < 4; ++i) { out.push_back(static_cast<std::uint8_t>(tag[i])); }
}

/// The canonical 44-byte RIFF/WAVE header, followed by the samples.
///
/// This is the whole of the format that everybody means when they say "WAV is
/// simple", and it IS simple — which is exactly why `load_wav` does not use it.
/// The moment a file has a LIST chunk, a fact chunk, or a WAVE_FORMAT_EXTENSIBLE
/// tag, this reader would have to grow a chunk walker, and the one in SDL is
/// already written. Here it exists so that the harness can make its own fixtures
/// with no binary asset in the repository.
bool write_wav_s16(const char* path, const std::vector<std::int16_t>& samples,
                   int channels, int freq)
{
    std::vector<std::uint8_t> bytes;
    const std::uint32_t data_bytes =
        static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));

    put_tag(bytes, "RIFF");
    put_u32(bytes, 36u + data_bytes);      // everything after this field
    put_tag(bytes, "WAVE");

    put_tag(bytes, "fmt ");
    put_u32(bytes, 16u);                   // PCM fmt chunk is 16 bytes
    put_u16(bytes, 1u);                    // WAVE_FORMAT_PCM
    put_u16(bytes, static_cast<std::uint16_t>(channels));
    put_u32(bytes, static_cast<std::uint32_t>(freq));
    put_u32(bytes, static_cast<std::uint32_t>(freq * channels * 2));   // byte rate
    put_u16(bytes, static_cast<std::uint16_t>(channels * 2));          // block align
    put_u16(bytes, 16u);                                               // bits

    put_tag(bytes, "data");
    put_u32(bytes, data_bytes);
    for (std::int16_t s : samples)
    {
        put_u16(bytes, static_cast<std::uint16_t>(s));
    }

    SDL_IOStream* io = SDL_IOFromFile(path, "wb");
    if (io == nullptr) { return false; }
    const bool ok = SDL_WriteIO(io, bytes.data(), bytes.size()) == bytes.size();
    SDL_CloseIO(io);
    return ok;
}

/// A 1 kHz sine, quantised to signed 16-bit, `seconds` long at `freq`.
std::vector<std::int16_t> tone_s16(double hz, double seconds, int freq, double amplitude)
{
    const std::size_t frames = static_cast<std::size_t>(seconds * freq);
    std::vector<std::int16_t> out(frames);
    for (std::size_t f = 0; f < frames; ++f)
    {
        const double v = amplitude * std::sin(2.0 * std::numbers::pi * hz
                                              * static_cast<double>(f) / freq);
        out[f] = static_cast<std::int16_t>(std::lround(v * 32767.0));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Offline mixing
// ---------------------------------------------------------------------------

/// Run `mix` for `buffers` buffers, appending everything to `out`. `per_buffer`
/// is called before each one, which is where a caller pushes the gains a game
/// would push once a frame.
template <typename F>
void render(mixer& mix, int buffers, std::vector<float>& out, F per_buffer)
{
    const int frames = mix.config().buffer_frames;
    std::vector<float> chunk(static_cast<std::size_t>(frames) * 2u, 0.0f);
    for (int b = 0; b < buffers; ++b)
    {
        per_buffer(b);
        mix.mix_into(chunk.data(), frames);
        out.insert(out.end(), chunk.begin(), chunk.end());
    }
}

/// 7.7's lesson, applied: spin before measuring anything. A cold CPU ramps for
/// tens of milliseconds and `best_of_three` cannot see past it when all three
/// attempts are inside the ramp.
void warm_up()
{
    volatile double sink = 0.0;
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
    while (std::chrono::steady_clock::now() < until)
    {
        for (int i = 0; i < 4096; ++i) { sink += std::sin(static_cast<double>(i)); }
    }
    (void)sink;
}

double best_of_three(const std::function<double()>& run)
{
    double best = run();
    best = std::min(best, run());
    best = std::min(best, run());
    return best;
}

}   // namespace

// ---------------------------------------------------------------------------
// A — the container
// ---------------------------------------------------------------------------
namespace
{
void section_a()
{
    rule("A  the container, and what loading one costs");

    const std::string dir = "build/demos";
    const std::string same_rate = dir + "/l78_tone48.wav";
    const std::string off_rate = dir + "/l78_tone441.wav";
    const std::string junk = dir + "/l78_junk.wav";

    const std::vector<std::int16_t> at48 = tone_s16(1000.0, 1.0, 48000, 0.5);
    const std::vector<std::int16_t> at441 = tone_s16(1000.0, 1.0, 44100, 0.5);

    if (!write_wav_s16(same_rate.c_str(), at48, 1, 48000)
        || !write_wav_s16(off_rate.c_str(), at441, 1, 44100))
    {
        std::printf("A.0  could not write fixtures into %s\n", dir.c_str());
        return;
    }

    // A.1 — the round trip. A file written as S16 and read back as float must
    // reproduce every sample to within one quantisation step, and the step is
    // known exactly: 1/32768.
    sound s;
    wav_report r;
    if (!engine::audio::load_wav(same_rate.c_str(), k_freq, s, &r))
    {
        std::printf("A.1  load failed\n");
        return;
    }

    double worst = 0.0;
    for (std::size_t i = 0; i < s.samples.size(); ++i)
    {
        const double want = static_cast<double>(at48[i]) / 32768.0;
        worst = std::max(worst, std::abs(static_cast<double>(s.samples[i]) - want));
    }

    std::printf("A.1  %s %dch %d Hz -> f32 %dch %d Hz\n",
                r.source_format, r.source_channels, r.source_freq,
                r.out_channels, r.out_freq);
    std::printf("     frames %zu -> %zu   bytes %zu -> %zu  (%.2fx)\n",
                r.source_frames, r.out_frames, r.source_bytes, r.out_bytes,
                static_cast<double>(r.out_bytes) / static_cast<double>(r.source_bytes));
    std::printf("     peak %.4f  rms %.4f  over-unity %zu  resampled %s\n",
                static_cast<double>(r.peak), static_cast<double>(r.rms),
                r.over_unity, r.resampled ? "yes" : "no");
    std::printf("     round trip worst error  %.3e  (1 LSB = %.3e)\n",
                worst, 1.0 / 32768.0);

    // A.2 — the same tone at 44.1 kHz. The frame count must scale by the ratio
    // of the rates, and the resampled flag must be set: a loader that silently
    // kept the file's own rate would produce a sound that plays 8.8% sharp, which
    // is a semitone and a half and is not subtle.
    sound s441;
    wav_report r441;
    if (engine::audio::load_wav(off_rate.c_str(), k_freq, s441, &r441))
    {
        std::printf("A.2  %d Hz -> %d Hz   %zu -> %zu frames  (%.4fx)\n",
                    r441.source_freq, r441.out_freq, r441.source_frames, r441.out_frames,
                    static_cast<double>(r441.out_frames)
                        / static_cast<double>(r441.source_frames));
        std::printf("     resampled %s   duration %.6f s -> %.6f s\n",
                    r441.resampled ? "yes" : "no",
                    static_cast<double>(r441.source_frames) / r441.source_freq,
                    static_cast<double>(s441.duration()));
    }

    // A.3 — CONTROL. A file that is not a WAV must be REFUSED, not loaded as
    // noise. The failure mode this guards against is the one that sounds like a
    // hardware fault: forty kilobytes of text interpreted as samples is full-scale
    // white noise, and a loader that returns true on it has handed the mixer a
    // scream.
    {
        SDL_IOStream* io = SDL_IOFromFile(junk.c_str(), "wb");
        if (io != nullptr)
        {
            const char* text = "this is not a RIFF file, it is a sentence.";
            SDL_WriteIO(io, text, SDL_strlen(text));
            SDL_CloseIO(io);
        }
        sound bad;
        const bool loaded = engine::audio::load_wav(junk.c_str(), k_freq, bad, nullptr);
        std::printf("A.3  CONTROL  junk file loaded: %s   samples %zu\n",
                    loaded ? "yes (BAD)" : "no", bad.samples.size());
        sound missing;
        const bool loaded2 = engine::audio::load_wav("build/demos/l78_nope.wav",
                                                     k_freq, missing, nullptr);
        std::printf("     CONTROL  missing file loaded: %s\n",
                    loaded2 ? "yes (BAD)" : "no");
    }
}

// ---------------------------------------------------------------------------
// B — mixing is addition
// ---------------------------------------------------------------------------
void section_b()
{
    rule("B  mixing is addition, literally");

    mixer mix;
    if (!mix.open_offline({.freq = k_freq, .buffer_frames = 512})) { return; }

    const sound a = engine::audio::make_tone(440.0f, 0.25f, k_freq, 0.20f);
    const sound b = engine::audio::make_tone(660.0f, 0.25f, k_freq, 0.15f);

    // B.1 — one voice at unity gain and centre pan reproduces the source, scaled
    // by the pan law's 0.7071 and by nothing else. A mixer that did anything
    // else to a single voice would be doing it to every voice.
    const voice_id v1 = mix.play(a, {.gain = 1.0f, .pan = 0.0f});
    (void)v1;
    std::vector<float> one;
    render(mix, 8, one, [](int) {});

    double worst_one = 0.0;
    const float centre = std::cos(std::numbers::pi_v<float> * 0.25f);
    for (std::size_t f = 0; f < one.size() / 2; ++f)
    {
        const double want = static_cast<double>(a.samples[f]) * centre;
        worst_one = std::max(worst_one, std::abs(static_cast<double>(one[f * 2]) - want));
    }
    std::printf("B.1  one voice, centre: worst error %.3e\n", worst_one);
    std::printf("     centre gain per channel %.6f  (= 1/sqrt(2))\n",
                static_cast<double>(centre));

    // B.2 — two voices. The output must equal the sum of what each produces
    // alone, to the last bit, because that is what the inner loop does.
    mix.stop_all();
    mix.update();
    const voice_id va = mix.play(a, {.gain = 1.0f});
    const voice_id vb = mix.play(b, {.gain = 1.0f});
    (void)va;
    (void)vb;
    std::vector<float> both;
    render(mix, 8, both, [](int) {});

    double worst_sum = 0.0;
    for (std::size_t f = 0; f < both.size() / 2; ++f)
    {
        const double want = (static_cast<double>(a.samples[f])
                             + static_cast<double>(b.samples[f])) * centre;
        worst_sum = std::max(worst_sum, std::abs(static_cast<double>(both[f * 2]) - want));
    }
    std::printf("B.2  two voices vs the arithmetic sum: worst %.3e\n", worst_sum);

    // B.3 — CONTROL. Zero voices must be silence, and it must be EXACT silence
    // rather than something small: a mixer that leaves its scratch buffer alone
    // between calls replays the previous buffer forever, which is a drone.
    mix.stop_all();
    mix.update();
    std::vector<float> none;
    render(mix, 4, none, [](int) {});
    double p = 0.0;
    double q = 0.0;
    stats(none.data(), none.size(), p, q);
    std::printf("B.3  CONTROL  zero voices: peak %.1f  rms %.1f\n", p, q);

    const mixer_report rep = mix.report();
    std::printf("     buffers %llu  frames %llu  live voices %zu\n",
                static_cast<unsigned long long>(rep.buffers),
                static_cast<unsigned long long>(rep.frames_mixed), rep.live_voices);
}

// ---------------------------------------------------------------------------
// C — headroom
// ---------------------------------------------------------------------------
void section_c()
{
    rule("C  N voices are sqrt(N) loud, not N loud");

    // 0.05 rather than 0.1, and the first run is why: at 0.1 the peak reached
    // full scale at 32 voices and the CLAMP then held it there, so the rms
    // column for the last two rows was measuring a clipped signal rather than a
    // sum. A measurement whose own subject changes halfway down the table is
    // two measurements. The clipping is real and C.6 prices it separately.
    std::printf("  N   peak    rms     peak/a   rms/a   sqrt(N)   N\n");

    const float amp = 0.05f;
    std::vector<sound> noises;
    noises.reserve(64);
    for (int i = 0; i < 64; ++i)
    {
        noises.push_back(engine::audio::make_noise(0.25f, 0x51ed'0000u
                                                   + static_cast<std::uint32_t>(i) * 2654435761u,
                                                   k_freq, amp));
    }

    for (int n : {1, 2, 4, 8, 16, 32, 64})
    {
        mixer mix;
        if (!mix.open_offline({.freq = k_freq, .buffer_frames = 512, .master_gain = 1.0f,
                               .max_voices = 128}))
        {
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            // Hard left, so that the pan law's 0.7071 does not enter the
            // arithmetic and the column really is "how many times one voice".
            const voice_id id = mix.play(noises[static_cast<std::size_t>(i)],
                                         {.gain = 1.0f, .pan = -1.0f});
            (void)id;
        }
        std::vector<float> out;
        render(mix, 16, out, [](int) {});

        // Left channel only.
        std::vector<float> left(out.size() / 2);
        for (std::size_t f = 0; f < left.size(); ++f) { left[f] = out[f * 2]; }

        double pk = 0.0;
        double rm = 0.0;
        stats(left.data(), left.size(), pk, rm);
        std::printf("%3d  %.4f  %.4f   %6.2f  %6.2f   %6.2f  %3d\n",
                    n, pk, rm, pk / amp, rm / amp, std::sqrt(static_cast<double>(n)), n);
    }

    // C.3 — the finding the table above cannot show: the peak GROWS WITH HOW
    // LONG YOU LISTEN. The rms of a sum of uncorrelated signals is a property of
    // the signals; the peak is the largest coincidence that happened to occur in
    // the window, and a longer window contains more chances. This is why headroom
    // is a bet rather than an arithmetic bound.
    std::printf("\n     16 voices, peak against how long you listen:\n");
    std::printf("       seconds   peak    peak/a\n");
    {
        mixer mix;
        if (!mix.open_offline({.freq = k_freq, .buffer_frames = 512, .max_voices = 128}))
        {
            return;
        }
        std::vector<sound> longer;
        for (int i = 0; i < 16; ++i)
        {
            longer.push_back(engine::audio::make_noise(
                8.0f, 0x1234'0000u + static_cast<std::uint32_t>(i) * 2654435761u, k_freq, amp));
        }
        for (int i = 0; i < 16; ++i)
        {
            const voice_id id = mix.play(longer[static_cast<std::size_t>(i)],
                                         {.gain = 1.0f, .pan = -1.0f});
            (void)id;
        }
        std::vector<float> out;
        render(mix, 8 * k_freq / 512, out, [](int) {});

        for (double secs : {0.01, 0.1, 1.0, 8.0})
        {
            const std::size_t n = std::min(out.size() / 2,
                                           static_cast<std::size_t>(secs * k_freq));
            double pk = 0.0;
            for (std::size_t f = 0; f < n; ++f)
            {
                pk = std::max(pk, std::abs(static_cast<double>(out[f * 2])));
            }
            std::printf("       %7.2f   %.4f  %6.2f\n", secs, pk, pk / amp);
        }
    }

    // C.4 — CONTROL, and the one that makes C mean anything. The SAME sound
    // played N times is perfectly correlated, and correlated signals add in
    // AMPLITUDE: the peak column must now track N exactly. If this row also said
    // sqrt(N), the meter would be measuring itself.
    std::printf("\n     CONTROL  the same sound N times (correlated):\n");
    std::printf("  N   peak    peak/a   N\n");
    for (int n : {1, 2, 4, 8, 16})
    {
        mixer mix;
        if (!mix.open_offline({.freq = k_freq, .buffer_frames = 512, .max_voices = 128}))
        {
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            const voice_id id = mix.play(noises[0], {.gain = 1.0f, .pan = -1.0f});
            (void)id;
        }
        std::vector<float> out;
        render(mix, 4, out, [](int) {});
        std::vector<float> left(out.size() / 2);
        for (std::size_t f = 0; f < left.size(); ++f) { left[f] = out[f * 2]; }
        double pk = 0.0;
        double rm = 0.0;
        stats(left.data(), left.size(), pk, rm);
        std::printf("%3d  %.4f   %6.2f  %3d\n", n, pk, pk / amp, n);
    }

    // C.5 — the two headroom rules, priced. 1/N is safe and throws away signal;
    // 1/sqrt(N) keeps the level and risks the rare coincidence.
    std::printf("\n     headroom for 16 voices:\n");
    std::printf("       1/N      gain %.4f  = %7.2f dB\n", 1.0 / 16.0,
                static_cast<double>(amplitude_to_db(1.0f / 16.0f)));
    std::printf("       1/sqrt(N) gain %.4f  = %7.2f dB\n", 1.0 / 4.0,
                static_cast<double>(amplitude_to_db(0.25f)));

    // C.6 — and what happens if you budget for neither. Thirty-two voices at a
    // tenth of full scale each: the arithmetic bound says 3.2, the rms says
    // 0.577 x 0.1 x 5.66 = 0.33, and the CLAMP says the peaks are gone.
    std::printf("\n     no headroom at all — 32 voices at 0.1 each:\n");
    {
        mixer mix;
        if (mix.open_offline({.freq = k_freq, .buffer_frames = 512, .max_voices = 128}))
        {
            std::vector<sound> loud;
            for (int i = 0; i < 32; ++i)
            {
                loud.push_back(engine::audio::make_noise(
                    0.5f, 0xabcd'0000u + static_cast<std::uint32_t>(i) * 2654435761u,
                    k_freq, 0.1f));
            }
            for (int i = 0; i < 32; ++i)
            {
                const voice_id id = mix.play(loud[static_cast<std::size_t>(i)],
                                             {.gain = 1.0f, .pan = -1.0f});
                (void)id;
            }
            std::vector<float> out;
            render(mix, 40, out, [](int) {});
            const mixer_report rep = mix.report();
            std::printf("       clipped samples %llu of %llu  (%.3f%%)\n",
                        static_cast<unsigned long long>(rep.clipped),
                        static_cast<unsigned long long>(rep.frames_mixed * 2),
                        100.0 * static_cast<double>(rep.clipped)
                            / static_cast<double>(rep.frames_mixed * 2));
            std::printf("       peak before the clamp %.4f\n",
                        static_cast<double>(rep.peak));
        }
    }
}
}   // namespace

// ---------------------------------------------------------------------------
// D — clipping
// ---------------------------------------------------------------------------
namespace
{
void section_d()
{
    rule("D  what clipping does to a waveform");

    // A 1 kHz tone, one second, rendered at increasing master gains. The window
    // is an exact number of periods so that the Goertzel bins do not leak.
    const std::size_t window = 48000;
    std::printf("  gain  peak   clipped     H3      H5      H7    THD\n");

    for (float gain : {1.0f, 1.5f, 2.0f, 3.0f, 6.0f})
    {
        mixer mix;
        if (!mix.open_offline({.freq = k_freq, .buffer_frames = 512, .master_gain = gain}))
        {
            return;
        }
        const sound tone = engine::audio::make_tone(1000.0f, 1.2f, k_freq, 0.5f);
        const voice_id id = mix.play(tone, {.gain = 1.0f, .pan = -1.0f});
        (void)id;

        std::vector<float> out;
        render(mix, 100, out, [](int) {});

        std::vector<float> left(window);
        for (std::size_t f = 0; f < window; ++f) { left[f] = out[f * 2]; }

        const double p1 = goertzel_power(left.data(), window, 1000.0);
        const double p3 = goertzel_power(left.data(), window, 3000.0);
        const double p5 = goertzel_power(left.data(), window, 5000.0);
        const double p7 = goertzel_power(left.data(), window, 7000.0);
        const double p9 = goertzel_power(left.data(), window, 9000.0);
        const double thd = std::sqrt((p3 + p5 + p7 + p9) / p1);

        double pk = 0.0;
        double rm = 0.0;
        stats(left.data(), window, pk, rm);

        const mixer_report rep = mix.report();
        std::printf("%6.2f  %.3f  %8llu  %6.1f  %6.1f  %6.1f  %5.2f%%\n",
                    static_cast<double>(gain), pk,
                    static_cast<unsigned long long>(rep.clipped),
                    to_db_power(p3 / p1), to_db_power(p5 / p1), to_db_power(p7 / p1),
                    thd * 100.0);
    }

    std::printf("\n     CONTROL  gain 1.0 above is the unclipped row: its\n");
    std::printf("     harmonics are the floor of the measurement, not of\n");
    std::printf("     the signal. A sine has none.\n");
}

// ---------------------------------------------------------------------------
// E — the pan laws
// ---------------------------------------------------------------------------
void section_e()
{
    rule("E  two pan laws, and the hole in the middle of one");

    std::printf("  pan     linear L/R      power    constant-power L/R   power\n");
    for (float p : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f})
    {
        const stereo_gain lin = pan_linear(p);
        const stereo_gain cp = pan_constant_power(p);
        std::printf("%5.2f  %.4f %.4f  %7.2f dB   %.4f %.4f  %7.2f dB\n",
                    static_cast<double>(p),
                    static_cast<double>(lin.left), static_cast<double>(lin.right),
                    to_db_power(static_cast<double>(lin.power())),
                    static_cast<double>(cp.left), static_cast<double>(cp.right),
                    to_db_power(static_cast<double>(cp.power())));
    }

    // E.2 — the sweep, as it is actually heard. Two voices of the same noise,
    // panned across the image over one second, mixed and measured in blocks. The
    // linear law must duck in the middle; the constant-power law must not.
    double lin_worst = 0.0;
    double cp_worst = 0.0;
    double lin_min_power = 1e9;
    double cp_min_power = 1e9;
    for (int i = 0; i <= 200; ++i)
    {
        const float p = -1.0f + 2.0f * static_cast<float>(i) / 200.0f;
        lin_min_power = std::min(lin_min_power, static_cast<double>(pan_linear(p).power()));
        cp_min_power = std::min(cp_min_power,
                                static_cast<double>(pan_constant_power(p).power()));
        lin_worst = std::max(lin_worst,
                             std::abs(to_db_power(static_cast<double>(pan_linear(p).power()))));
        cp_worst = std::max(cp_worst,
                            std::abs(to_db_power(
                                static_cast<double>(pan_constant_power(p).power()))));
    }
    std::printf("\nE.2  over a full sweep, worst deviation from unity power:\n");
    std::printf("       linear          %6.3f dB   (min power %.4f)\n",
                lin_worst, lin_min_power);
    std::printf("       constant power  %6.3f dB   (min power %.4f)\n",
                cp_worst, cp_min_power);

    // E.3 — CONTROL. At the extremes the two laws MUST agree exactly: all of one
    // channel, none of the other. A difference here would mean the comparison in
    // E.1 is between two things that differ everywhere, which proves nothing
    // about the middle.
    const stereo_gain l_lin = pan_linear(-1.0f);
    const stereo_gain l_cp = pan_constant_power(-1.0f);
    const stereo_gain r_lin = pan_linear(1.0f);
    const stereo_gain r_cp = pan_constant_power(1.0f);
    std::printf("E.3  CONTROL  hard left  diff %.3e / %.3e\n",
                std::abs(static_cast<double>(l_lin.left - l_cp.left)),
                std::abs(static_cast<double>(l_lin.right - l_cp.right)));
    std::printf("     CONTROL  hard right diff %.3e / %.3e\n",
                std::abs(static_cast<double>(r_lin.left - r_cp.left)),
                std::abs(static_cast<double>(r_lin.right - r_cp.right)));

    // E.4 — the geometry. A listener at the origin facing -Z, an emitter walked
    // around them, and the pan that falls out of one dot product.
    std::printf("\nE.4  an emitter orbiting a listener facing -Z:\n");
    std::printf("      angle   position            pan     L      R\n");
    const listener lis = listener_from(transform{});
    for (int deg = 0; deg <= 360; deg += 45)
    {
        const double rad = deg * std::numbers::pi / 180.0;
        // 0 degrees is straight ahead (-Z); positive angles swing to the right.
        const vec3 pos{static_cast<float>(5.0 * std::sin(rad)), 0.0f,
                       static_cast<float>(-5.0 * std::cos(rad))};
        const float pan = pan_of(lis, pos);
        const stereo_gain g = pan_constant_power(pan);
        std::printf("      %4d   (%5.2f,%5.2f,%5.2f)  %5.2f  %.3f  %.3f\n",
                    deg, static_cast<double>(pos.x), static_cast<double>(pos.y),
                    static_cast<double>(pos.z), static_cast<double>(pan),
                    static_cast<double>(g.left), static_cast<double>(g.right));
    }
    const vec3 above{0.0f, 5.0f, 0.0f};
    std::printf("      up     (%5.2f,%5.2f,%5.2f)  %5.2f   -- folds to centre\n",
                static_cast<double>(above.x), static_cast<double>(above.y),
                static_cast<double>(above.z), static_cast<double>(pan_of(lis, above)));
}

// ---------------------------------------------------------------------------
// F — distance
// ---------------------------------------------------------------------------
void section_f()
{
    rule("F  distance: 1/d, 1/d^2, and the cliff at max_distance");

    emitter inv;
    inv.law = falloff::inverse;
    emitter lin;
    lin.law = falloff::linear;
    emitter ranged;
    ranged.law = falloff::inverse_ranged;

    std::printf("    d     inverse        linear       inv-ranged     1/d^2\n");
    for (float d : {0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 49.0f, 50.0f, 60.0f})
    {
        const float a = attenuation(inv, d);
        const float b = attenuation(lin, d);
        const float c = attenuation(ranged, d);
        // The mistake: treating amplitude as if it fell with intensity.
        const float wrong = 1.0f / std::max(1.0f, d * d);
        std::printf("%6.1f  %.4f %6.1f  %.4f %6.1f  %.4f %6.1f  %.4f %6.1f\n",
                    static_cast<double>(d),
                    static_cast<double>(a), static_cast<double>(amplitude_to_db(a)),
                    static_cast<double>(b), static_cast<double>(amplitude_to_db(b)),
                    static_cast<double>(c), static_cast<double>(amplitude_to_db(c)),
                    static_cast<double>(wrong), static_cast<double>(amplitude_to_db(wrong)));
    }

    // F.2 — dB per doubling. The whole of the 1/d vs 1/d^2 argument in one
    // column: the physical law is -6 dB and the mistake is -12.
    std::printf("\nF.2  dB per doubling of distance, 2 m -> 32 m:\n");
    for (int i = 1; i <= 4; ++i)
    {
        const float d0 = static_cast<float>(1 << i);
        const float d1 = d0 * 2.0f;
        const float g0 = attenuation(inv, d0);
        const float g1 = attenuation(inv, d1);
        const float w0 = 1.0f / (d0 * d0);
        const float w1 = 1.0f / (d1 * d1);
        std::printf("     %5.1f -> %5.1f   1/d %6.2f dB    1/d^2 %6.2f dB\n",
                    static_cast<double>(d0), static_cast<double>(d1),
                    static_cast<double>(amplitude_to_db(g1) - amplitude_to_db(g0)),
                    static_cast<double>(amplitude_to_db(w1) - amplitude_to_db(w0)));
    }

    // F.3 — the cliff. The bare inverse law is still audible when it is cut off,
    // and cutting off an audible signal is a step, and a step is a click.
    const float at_max = attenuation(inv, 50.0f);
    std::printf("\nF.3  bare `inverse` at max_distance = 50 m:\n");
    std::printf("       gain %.4f = %.2f dB, then cut to silence.\n",
                static_cast<double>(at_max), static_cast<double>(amplitude_to_db(at_max)));
    std::printf("       `inverse_ranged` there: %.6f = %.2f dB\n",
                static_cast<double>(attenuation(ranged, 50.0f)),
                static_cast<double>(amplitude_to_db(attenuation(ranged, 50.0f))));
    std::printf("       and at 49.0 m: %.6f  (approach, not a cliff)\n",
                static_cast<double>(attenuation(ranged, 49.0f)));

    // F.4 — CONTROL. Every law must be exactly 1 at the reference distance and
    // inside it, or the three curves are not comparable at all.
    std::printf("\nF.4  CONTROL  at and inside ref_distance (1 m):\n");
    std::printf("       inverse %.6f  linear %.6f  ranged %.6f\n",
                static_cast<double>(attenuation(inv, 1.0f)),
                static_cast<double>(attenuation(lin, 1.0f)),
                static_cast<double>(attenuation(ranged, 1.0f)));
    std::printf("       at 0.1 m: %.6f  %.6f  %.6f  (never > 1)\n",
                static_cast<double>(attenuation(inv, 0.1f)),
                static_cast<double>(attenuation(lin, 0.1f)),
                static_cast<double>(attenuation(ranged, 0.1f)));
    emitter none_law;
    none_law.law = falloff::none;
    std::printf("       falloff::none at 1000 m: %.6f\n",
                static_cast<double>(attenuation(none_law, 1000.0f)));

    // F.5 — the whole spatial answer for one placement, so the lesson can show
    // its work end to end.
    const listener lis = listener_from(transform{});
    emitter e;
    e.position = vec3{3.0f, 0.0f, -4.0f};
    e.gain = 0.8f;
    const spatial_result sr = spatialise(lis, e);
    std::printf("\nF.5  worked example: emitter at (3, 0, -4), gain 0.8\n");
    std::printf("       distance %.4f   attenuation %.4f\n",
                static_cast<double>(sr.distance), static_cast<double>(sr.attenuation));
    std::printf("       pan %.4f  ->  L %.4f  R %.4f\n",
                static_cast<double>(sr.pan), static_cast<double>(sr.gain.left),
                static_cast<double>(sr.gain.right));
    std::printf("       power %.4f = %.2f dB\n",
                static_cast<double>(sr.gain.power()),
                to_db_power(static_cast<double>(sr.gain.power())));
}
}   // namespace

// ---------------------------------------------------------------------------
// G — the gain step against the gain ramp
// ---------------------------------------------------------------------------
namespace
{
/// A fly-by: the emitter travels along +x at `speed`, passing `closest` metres in
/// front of a listener at the origin. The gains change fastest at the moment of
/// closest approach, which is exactly where a step is most audible.
struct flyby
{
    float speed = 20.0f;
    float closest = 1.0f;
    float start_x = -40.0f;

    [[nodiscard]] emitter at(float t) const
    {
        emitter e;
        e.position = vec3{start_x + speed * t, 0.0f, -closest};
        e.gain = 1.0f;
        e.ref_distance = 1.0f;
        e.max_distance = 60.0f;
        e.law = falloff::inverse_ranged;
        return e;
    }
};

void section_g()
{
    rule("G  the gain step against the gain ramp");

    // 800 frames at 48 kHz is exactly 16.667 ms, so one buffer is one frame of a
    // 60 Hz game and the gain update rate below is exactly the rate a game
    // updates at. Choosing the buffer to match the frame is not what a real
    // program does; it is what makes this measurement about ONE thing.
    constexpr int frames_per_buffer = 800;
    constexpr int buffers = 240;   // 4 seconds

    const sound tone = engine::audio::make_tone(200.0f, 5.0f, k_freq, 0.5f);
    const listener lis = listener_from(transform{});
    const flyby path;

    // G.1 — what the gains actually do over the pass.
    std::printf("G.1  the gains a 60 Hz game would push, near the pass:\n");
    std::printf("       t(s)    x      dist    L       R     dL/frame\n");
    float prev_l = 0.0f;
    float worst_dl = 0.0f;
    for (int b = 0; b < buffers; ++b)
    {
        const float t = static_cast<float>(b) * frames_per_buffer / k_freq;
        const spatial_result r = spatialise(lis, path.at(t));
        if (b > 0) { worst_dl = std::max(worst_dl, std::abs(r.gain.left - prev_l)); }
        if (b >= 118 && b <= 123)
        {
            std::printf("     %6.3f %6.2f  %6.3f  %.4f  %.4f   %.4f\n",
                        static_cast<double>(t),
                        static_cast<double>(path.at(t).position.x),
                        static_cast<double>(r.distance),
                        static_cast<double>(r.gain.left), static_cast<double>(r.gain.right),
                        static_cast<double>(b > 0 ? std::abs(r.gain.left - prev_l) : 0.0f));
        }
        prev_l = r.gain.left;
    }
    std::printf("       worst gain change in one frame: %.4f\n",
                static_cast<double>(worst_dl));

    // The raw signal: one voice, hard left, gain 1, never changed. Everything
    // below multiplies THIS, so the two methods differ in nothing but the gain.
    std::vector<float> raw;
    {
        mixer mix;
        if (!mix.open_offline({.freq = k_freq, .buffer_frames = frames_per_buffer})) { return; }
        const voice_id id = mix.play(tone, {.gain = 1.0f, .pan = -1.0f});
        (void)id;
        render(mix, buffers, raw, [](int) {});
    }

    // The mixer's own output, with the gains pushed once per buffer. The mixer
    // ramps; this is the shipping path.
    std::vector<float> ramped;
    {
        mixer mix;
        if (!mix.open_offline({.freq = k_freq, .buffer_frames = frames_per_buffer})) { return; }
        const voice_id id = mix.play(tone, {.gain = 1.0f, .pan = -1.0f});
        render(mix, buffers, ramped, [&](int b)
        {
            const float t = static_cast<float>(b) * frames_per_buffer / k_freq;
            mix.set_gain(id, spatialise(lis, path.at(t)).gain);
        });
    }

    // The step: the same gains, held constant across each buffer. This is what
    // you get by writing `v.gain = g;` in the setter, which is the obvious thing
    // to write.
    std::vector<float> stepped(raw.size(), 0.0f);
    for (int b = 0; b < buffers; ++b)
    {
        const float t = static_cast<float>(b) * frames_per_buffer / k_freq;
        const stereo_gain g = spatialise(lis, path.at(t)).gain;
        for (int f = 0; f < frames_per_buffer; ++f)
        {
            const std::size_t i = (static_cast<std::size_t>(b) * frames_per_buffer
                                   + static_cast<std::size_t>(f)) * 2u;
            if (i + 1 >= stepped.size()) { break; }
            stepped[i] = raw[i] * g.left;
            stepped[i + 1] = raw[i + 1] * g.right;
        }
    }

    const std::size_t frames_total = raw.size() / 2;
    const double raw_slope = max_step(raw.data(), frames_total, 0);
    const double step_jump = max_step(stepped.data(), frames_total, 0);
    const double ramp_jump = max_step(ramped.data(), frames_total, 0);

    std::printf("\nG.2  largest jump between adjacent samples (left):\n");
    std::printf("       the signal's own slope   %.6f\n", raw_slope);
    std::printf("       gain applied as a step   %.6f  (%.2fx)\n",
                step_jump, step_jump / raw_slope);
    std::printf("       gain applied as a ramp   %.6f  (%.2fx)\n",
                ramp_jump, ramp_jump / raw_slope);

    // G.3 — where the injected energy goes. The error between the two renders is
    // a train of impulses, one per buffer boundary, and an impulse is broadband —
    // which is why a gain step is heard as a click rather than as a level change.
    std::vector<float> err(frames_total);
    for (std::size_t f = 0; f < frames_total; ++f)
    {
        err[f] = stepped[f * 2] - ramped[f * 2];
    }
    const std::size_t w = 48000;
    const double fund = goertzel_power(err.data(), w, 200.0);
    const double total = mean_square(err.data(), w);
    std::printf("\nG.3  step minus ramp, over one second:\n");
    std::printf("       rms of the difference    %.3e\n", std::sqrt(total));
    std::printf("       at the tone's own 200 Hz %.3e  (%.1f%% of it)\n",
                std::sqrt(fund), 100.0 * fund / std::max(total, 1e-30));
    std::printf("       the rest is broadband: that is the click.\n");

    // G.4 — CONTROL, and the one that makes G mean anything. With the gain HELD
    // CONSTANT the ramp has nothing to interpolate, so the two renders must be
    // bit-identical. A difference here would mean G.2 compares two code paths
    // rather than two reconstructions.
    std::vector<float> const_ramped;
    {
        mixer mix;
        if (!mix.open_offline({.freq = k_freq, .buffer_frames = frames_per_buffer})) { return; }
        const voice_id id = mix.play(tone, {.gain = 1.0f, .pan = -1.0f});
        render(mix, 8, const_ramped, [&](int)
        {
            mix.set_gain(id, stereo_gain{0.37f, 0.11f});
        });
    }
    double worst_const = 0.0;
    for (std::size_t f = 1; f < const_ramped.size() / 2; ++f)
    {
        // After the first buffer the gain has arrived and every sample must be
        // exactly raw * 0.37.
        if (f < static_cast<std::size_t>(frames_per_buffer)) { continue; }
        const double want = static_cast<double>(raw[f * 2]) * 0.37;
        worst_const = std::max(worst_const,
                               std::abs(static_cast<double>(const_ramped[f * 2]) - want));
    }
    std::printf("\nG.4  CONTROL  constant gain, ramp vs plain multiply:\n");
    std::printf("       worst difference %.3e\n", worst_const);

    // G.5 — the same argument one level up. A sound that starts at a non-zero
    // sample is a step from silence, which is a click that no mixer can fix
    // because it is in the CONTENT. `apply_fade` is three lines and removes it.
    std::printf("\nG.5  a sound that starts on a peak:\n");
    {
        // A cosine, so sample 0 is at full amplitude: the worst case, and a
        // completely ordinary thing for an edited sample to be.
        sound hard;
        hard.channels = 1;
        hard.freq = k_freq;
        hard.samples.resize(k_freq / 4);
        for (std::size_t f = 0; f < hard.samples.size(); ++f)
        {
            hard.samples[f] = 0.25f * std::cos(2.0f * std::numbers::pi_v<float> * 200.0f
                                               * static_cast<float>(f) / k_freq);
        }
        sound faded = hard;
        engine::audio::apply_fade(faded, 0.005f, 0.005f);

        // The step into the sound from the silence before it, and then between
        // its own samples. x[-1] is 0 by definition: before a voice starts, the
        // mixer's buffer is silent.
        auto worst_start = [](const sound& snd, std::size_t n) {
            double prev = 0.0;
            double worst = 0.0;
            for (std::size_t f = 0; f < n && f < snd.samples.size(); ++f)
            {
                const double v = static_cast<double>(snd.samples[f]);
                worst = std::max(worst, std::abs(v - prev));
                prev = v;
            }
            return worst;
        };

        const double hard_step = worst_start(hard, 512);
        const double fade_step = worst_start(faded, 512);
        // What the waveform does on its own: A x 2 pi f / rate.
        const double own_slope = 0.25 * 2.0 * std::numbers::pi * 200.0 / k_freq;
        std::printf("       first sample value       %.7f\n",
                    static_cast<double>(hard.samples[0]));
        std::printf("       worst step, no fade      %.7f  (%.1fx)\n",
                    hard_step, hard_step / own_slope);
        std::printf("       worst step, 5 ms fade    %.7f  (%.2fx)\n",
                    fade_step, fade_step / own_slope);
        std::printf("       the waveform\'s own slope %.7f\n", own_slope);
        std::printf("       the fade does not make the step small. It\n");
        std::printf("       removes it, leaving the signal\'s own motion.\n");
    }
}

// ---------------------------------------------------------------------------
// H — resampling
// ---------------------------------------------------------------------------
void section_h(bool sdl_audio_ready)
{
    rule("H  resampling 44.1 -> 48 kHz: ours against SDL's");

    // The fixture: exactly one second of a 1 kHz tone at 44.1 kHz. One second is
    // 1000 whole periods at both rates, so every Goertzel window below is an
    // integer number of periods and no window function is needed.
    const std::vector<std::int16_t> src = tone_s16(1000.0, 1.0, 44100, 0.5);
    const std::string path = "build/demos/l78_tone441.wav";

    sound ours;
    if (!engine::audio::load_wav(path.c_str(), 48000, ours, nullptr))
    {
        std::printf("H.0  fixture missing; run section A first\n");
        return;
    }

    // A window in the middle, away from both ends: a resampler with a real filter
    // has a start-up transient, and measuring it would be measuring the edge
    // rather than the steady state. 38,400 frames is 800 whole periods.
    const std::size_t skip = 4800;
    const std::size_t w = 38400;

    auto snr_of = [&](const float* x) {
        const double p1 = goertzel_power(x, w, 800.0);
        const double tot = mean_square(x, w);
        const double noise = std::max(tot - p1, 1e-30);
        return to_db_power(p1 / noise);
    };

    std::printf("H.1  a 1 kHz tone, 44100 -> 48000, measured in the middle:\n");
    std::printf("       ours (linear, 2 taps)    %7.2f dB SNR\n", snr_of(ours.samples.data() + skip));

    if (sdl_audio_ready)
    {
        const SDL_AudioSpec from{SDL_AUDIO_S16, 1, 44100};
        const SDL_AudioSpec to{SDL_AUDIO_F32, 1, 48000};
        std::uint8_t* dst = nullptr;
        int dst_len = 0;
        if (SDL_ConvertAudioSamples(&from, reinterpret_cast<const std::uint8_t*>(src.data()),
                                    static_cast<int>(src.size() * sizeof(std::int16_t)),
                                    &to, &dst, &dst_len))
        {
            const auto* f = reinterpret_cast<const float*>(dst);
            const std::size_t n = static_cast<std::size_t>(dst_len) / sizeof(float);
            if (n > skip + w)
            {
                std::printf("       SDL_ConvertAudioSamples       %7.2f dB SNR\n",
                            snr_of(f + skip));
            }
            SDL_free(dst);
        }
        else
        {
            std::printf("       SDL_ConvertAudioSamples failed: %s\n", SDL_GetError());
        }
    }
    else
    {
        std::printf("       SDL converter unavailable (no audio subsystem)\n");
    }

    // H.2 — CONTROL. The same file loaded at ITS OWN rate does not resample, and
    // its SNR is the floor of the whole measurement: 16-bit quantisation and
    // nothing else. Without this row, "-40 dB" could plausibly be the meter.
    sound native;
    engine::audio::load_wav(path.c_str(), 44100, native, nullptr);
    const double p1 = goertzel_power(native.samples.data() + 4410, 35280, 800.0);
    const double tot = mean_square(native.samples.data() + 4410, 35280);
    std::printf("H.2  CONTROL  no resample at all      %7.2f dB SNR\n",
                to_db_power(p1 / std::max(tot - p1, 1e-30)));
    std::printf("       (16-bit quantisation floor: 6.02 x 16 = 96.3 dB\n");
    std::printf("        minus 1.76 for a full-scale sine, and this tone\n");
    std::printf("        is at half scale, so about 92 dB is right.)\n");

    // H.3 — how the error depends on frequency. Linear interpolation is a
    // low-pass filter: it barely touches a low tone and badly damages one near
    // Nyquist, which is why the number above is a property of the CONTENT and
    // not of the resampler alone.
    std::printf("\nH.3  the same resampler at four frequencies:\n");
    std::printf("       tone      SNR\n");
    for (double hz : {200.0, 1000.0, 4000.0, 10000.0})
    {
        const std::vector<std::int16_t> t = tone_s16(hz, 1.0, 44100, 0.5);
        const std::string p2 = "build/demos/l78_h3.wav";
        if (!write_wav_s16(p2.c_str(), t, 1, 44100)) { continue; }
        sound r;
        if (!engine::audio::load_wav(p2.c_str(), 48000, r, nullptr)) { continue; }
        const double cycles = hz * static_cast<double>(w) / 48000.0;
        const double f1 = goertzel_power(r.samples.data() + skip, w, cycles);
        const double ft = mean_square(r.samples.data() + skip, w);
        std::printf("     %7.0f Hz  %7.2f dB\n", hz,
                    to_db_power(f1 / std::max(ft - f1, 1e-30)));
    }
}
}   // namespace

// ---------------------------------------------------------------------------
// I — the budget
// ---------------------------------------------------------------------------
namespace
{
std::uint64_t g_log_lines = 0;

void SDLCALL counting_log(void*, int, SDL_LogPriority, const char*)
{
    ++g_log_lines;
}

void section_i()
{
    rule("I  the budget: what a buffer costs, and what the lock costs");

    warm_up();

    constexpr int frames_per_buffer = 512;
    const double budget_us = 1e6 * frames_per_buffer / static_cast<double>(k_freq);
    std::printf("I.1  buffer %d frames at %d Hz = %.3f ms of audio.\n",
                frames_per_buffer, k_freq, budget_us / 1000.0);
    std::printf("     voices    us/buffer   %% of budget   ns/voice/frame\n");

    std::vector<sound> noises;
    for (int i = 0; i < 64; ++i)
    {
        noises.push_back(engine::audio::make_noise(1.0f, 0x7f4a'7c15u
                                                   + static_cast<std::uint32_t>(i) * 2246822519u,
                                                   k_freq, 0.05f));
    }

    for (int n : {0, 1, 4, 16, 32, 64})
    {
        mixer mix;
        if (!mix.open_offline({.freq = k_freq, .buffer_frames = frames_per_buffer,
                               .max_voices = 128}))
        {
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            const voice_id id = mix.play(noises[static_cast<std::size_t>(i)],
                                         {.gain = 0.5f, .loops = true});
            (void)id;
        }

        std::vector<float> buf(static_cast<std::size_t>(frames_per_buffer) * 2u, 0.0f);
        const int reps = 400;
        const double us = best_of_three([&]() {
            const auto t0 = std::chrono::steady_clock::now();
            for (int r = 0; r < reps; ++r) { mix.mix_into(buf.data(), frames_per_buffer); }
            const auto t1 = std::chrono::steady_clock::now();
            return std::chrono::duration<double, std::micro>(t1 - t0).count() / reps;
        });

        const double per_voice_frame = n > 0
            ? us * 1000.0 / (static_cast<double>(n) * frames_per_buffer)
            : 0.0;
        std::printf("     %6d   %9.3f   %10.2f%%   %12.2f\n",
                    n, us, 100.0 * us / budget_us, per_voice_frame);
    }

    // I.2 — the main thread's critical sections. Every setter takes the lock the
    // mix holds, so this is how long the audio thread can be made to wait.
    {
        mixer mix;
        if (!mix.open_offline({.freq = k_freq, .buffer_frames = frames_per_buffer})) { return; }
        const sound tone = engine::audio::make_tone(440.0f, 1.0f, k_freq, 0.2f);
        const voice_id id = mix.play(tone, {.loops = true});

        const int reps = 200000;
        const double ns = best_of_three([&]() {
            const auto t0 = std::chrono::steady_clock::now();
            for (int r = 0; r < reps; ++r)
            {
                mix.set_gain(id, stereo_gain{0.5f, 0.5f});
            }
            const auto t1 = std::chrono::steady_clock::now();
            return std::chrono::duration<double, std::nano>(t1 - t0).count() / reps;
        });
        std::printf("\nI.2  set_gain (lock, resolve, two stores): %.1f ns\n", ns);
        std::printf("     that is the longest the audio thread can be made\n");
        std::printf("     to wait by one call, and it is bounded.\n");
    }

    // I.3 — the thing the audio thread must never do. Formatting alone is
    // already twenty times a voice's per-frame cost; the WRITE is the part with
    // no upper bound at all, because it is a syscall that can block on a pipe, a
    // terminal, or a file on a network share.
    {
        SDL_LogOutputFunction old_fn = nullptr;
        void* old_data = nullptr;
        SDL_GetLogOutputFunction(&old_fn, &old_data);
        SDL_SetLogOutputFunction(&counting_log, nullptr);

        const int reps = 20000;
        const double fmt_ns = best_of_three([&]() {
            const auto t0 = std::chrono::steady_clock::now();
            for (int r = 0; r < reps; ++r)
            {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "voice %d gain %.3f", r, 0.5);
            }
            const auto t1 = std::chrono::steady_clock::now();
            return std::chrono::duration<double, std::nano>(t1 - t0).count() / reps;
        });
        SDL_SetLogOutputFunction(old_fn, old_data);

        std::FILE* sink = std::fopen("build/demos/l78_log.txt", "w");
        double io_ns = 0.0;
        if (sink != nullptr)
        {
            const int wreps = 5000;
            io_ns = best_of_three([&]() {
                const auto t0 = std::chrono::steady_clock::now();
                for (int r = 0; r < wreps; ++r)
                {
                    std::fprintf(sink, "voice %d gain %.3f\n", r, 0.5);
                    std::fflush(sink);
                }
                const auto t1 = std::chrono::steady_clock::now();
                return std::chrono::duration<double, std::nano>(t1 - t0).count() / wreps;
            });
            std::fclose(sink);
        }

        std::printf("\nI.3  one log line from inside the mix:\n");
        std::printf("       format only, output discarded  %8.1f ns\n", fmt_ns);
        std::printf("       format + write + flush         %8.1f ns\n", io_ns);
        std::printf("       (%llu lines went through the counter)\n",
                    static_cast<unsigned long long>(g_log_lines));
    }

    // I.4 — CONTROL. An empty timed loop, so that the numbers above can be read
    // as costs rather than as the clock's own overhead.
    {
        const int reps = 400;
        volatile int sink = 0;
        const double us = best_of_three([&]() {
            const auto t0 = std::chrono::steady_clock::now();
            for (int r = 0; r < reps; ++r) { sink += r; }
            const auto t1 = std::chrono::steady_clock::now();
            return std::chrono::duration<double, std::micro>(t1 - t0).count() / reps;
        });
        std::printf("\nI.4  CONTROL  empty loop body: %.4f us per rep\n", us);
    }

    // I.5 — latency, which is the other half of the buffer-size trade.
    std::printf("\nI.5  buffer size against latency, at %d Hz:\n", k_freq);
    std::printf("       frames    latency    callbacks/s\n");
    for (int f : {128, 256, 512, 1024, 2048})
    {
        std::printf("       %6d   %6.2f ms   %8.1f\n", f,
                    1000.0 * f / k_freq, static_cast<double>(k_freq) / f);
    }
}
}   // namespace

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    SDL_SetAppMetadata("verify_78", "1.0", "dev.engine.verify78");

    // The audio subsystem is initialised only so that section H can call
    // SDL_ConvertAudioSamples. NOTHING else here opens a device, and the harness
    // still runs — with one row missing — on a machine where this fails.
    const bool sdl_audio_ready = SDL_Init(SDL_INIT_AUDIO);
    if (!sdl_audio_ready)
    {
        std::printf("note: SDL_Init(AUDIO) failed (%s); H.1's SDL row is skipped\n",
                    SDL_GetError());
    }

    std::printf("verify_78 — Lesson 7.8, SDL3 audio\n");
    std::printf("============================================================\n");

    section_a();
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();
    section_g();
    section_h(sdl_audio_ready);
    section_i();

    std::printf("\ndone.\n");
    SDL_Quit();
    return 0;
}
