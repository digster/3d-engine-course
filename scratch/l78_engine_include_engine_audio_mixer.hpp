// engine/include/engine/audio/mixer.hpp — voices, a device, and a deadline.
//
// Lesson 7.8. **The first code in this engine that runs on a thread we do not
// own, cannot step through, and must never make wait.** Everything strange about
// this file comes from that one sentence, so it is worth stating what the
// arrangement actually is before any API appears.
//
// ---- THE TWO CLOCKS --------------------------------------------------------
//
// Since Lesson 1.4 this engine has had one clock: the accumulator, which runs
// the simulation at a fixed 60 Hz and draws whenever it can. A late frame there
// is a hitch — the picture holds for 30 ms instead of 16, the player notices or
// does not, and the NEXT frame fixes it.
//
// The device has its own clock and it does not negotiate. It consumes
// `buffer_frames` frames every `buffer_frames / freq` seconds, forever, and if
// the data is not there it plays whatever is in the buffer — usually silence,
// sometimes the previous buffer again. That is a **dropout**, and the reason it
// matters out of proportion to its length is that a 10 ms gap in a picture is
// invisible and a 10 ms gap in a sound is a CLICK, because the discontinuity at
// each end of it contains every frequency at once (§6 measures exactly this).
//
// So audio's budget is not "be fast on average". It is "never be late", and the
// difference between those two sentences is the entire design below:
//
//   * the mix happens in a callback SDL runs on its own thread;
//   * that callback allocates nothing, opens nothing, locks one mutex for a
//     bounded time, and logs NOTHING — §8 measures why the last one is not
//     paranoia;
//   * everything it wants to tell you it tells you by incrementing a counter,
//     which the main thread reads through `report()`.
//
// ---- WHY A HANDLE, AND WHY THIS IS LESSON 5.4's BEST ARGUMENT ---------------
//
// A voice is the most transient object this engine has ever had: it exists for
// as long as a footstep, ends on a thread you do not control, and its slot is
// immediately reused by the next footstep. A caller who kept a `voice*` would be
// holding a pointer to a different sound within a second, and the symptom would
// be "turning down the explosion made the music quiet" — a bug with no
// plausible mechanism, arriving hours later.
//
// `voice_id` is a generational handle (`engine/core/handle.hpp`), so the stale
// id simply stops resolving. **Every function below that takes one returns false
// rather than doing something to the wrong voice**, and that is not defensive
// programming, it is the only correct behaviour: "the sound you are talking
// about has finished" is a true and useful answer.
//
// ---- WHAT THIS IS NOT ------------------------------------------------------
//
// Not a streaming mixer: every sound is fully decoded in memory, so a four-minute
// music track costs 44 MB and Module 9's job system is where that gets fixed. No
// effects, no filters, no submix buses, no reverb. No voice stealing — a play
// past `max_voices` is refused and counted, and §3's arithmetic is why that is
// less brutal than it sounds. Those are all exercises or later lessons, and each
// one is named where it belongs.
#pragma once

#include <engine/audio/sound.hpp>
#include <engine/audio/spatial.hpp>
#include <engine/core/handle.hpp>

// SDL, in a public header, and the one place this file has no choice: the
// callback SDL runs is a member of this class, and its declaration needs SDL's
// calling convention macro. SDL is already a PUBLIC dependency of the engine
// target (engine/CMakeLists.txt), so this costs nothing new — but note what is
// NOT here. The mutex, the voice pool and the scratch buffer all live in `state`
// below, which is an incomplete type, so the only SDL name this header exposes
// is the stream it is holding.
#include <SDL3/SDL_audio.h>

#include <cstddef>
#include <cstdint>

namespace engine::audio
{

/// The tag a `voice_id` names. Incomplete on purpose: a caller may hold, copy and
/// compare handles to voices without the layout of one ever being public, which
/// is `handle<T>`'s phantom-parameter trick (5.4) used for exactly what it is for.
struct voice;

/// A reference to a playing sound. Four bytes, copyable, and safe to keep
/// forever — it goes stale, it never dangles.
using voice_id = handle<voice>;

/// How the device is opened and how big a bite the mixer takes.
struct mixer_config
{
    /// 0 means "ask the device what it runs at and use that", which is what you
    /// want: it makes the common case resample nothing. A non-zero value forces
    /// a rate and lets SDL convert, which is one more resampler in the path.
    int freq = 0;

    /// 2. Mono output would make `spatial.hpp` pointless and more than two
    /// channels would make its pan law wrong rather than merely limited, so this
    /// is checked rather than honoured: `open` refuses anything but 2.
    int channels = 2;

    /// How many frames the mixer produces per call to `mix_into`.
    ///
    /// **The single most consequential number in this file**, and it is a
    /// straight trade: at 48 kHz, 256 frames is 5.33 ms of latency and a callback
    /// every 5.33 ms; 1024 frames is 21.3 ms and a quarter as many callbacks.
    /// Small buffers mean a sound starts sooner after you ask for it and mean
    /// less slack before a late mix becomes a dropout. §8 measures both halves on
    /// this machine. 512 is the usual compromise and is what games ship.
    int buffer_frames = 512;

    /// Multiplied into everything at the end, after the voices are summed and
    /// before the clip check. Not a volume knob for the player — that belongs to
    /// the game — but the **headroom** control of §3.
    float master_gain = 1.0f;

    /// The voice table's size. 64 is generous: see §3 for why the 65th
    /// simultaneous sound is inaudible anyway.
    int max_voices = 64;
};

/// How a voice plays. Everything here can be changed while it is playing.
struct voice_params
{
    /// Applied to both channels before the pan law. Amplitude, not loudness:
    /// 0.5 is -6.02 dB.
    float gain = 1.0f;

    /// -1 hard left, 0 centre, +1 hard right. Ignored for a stereo `sound`,
    /// which carries its own image and cannot be panned without destroying it —
    /// the mixer counts the attempt rather than silently doing nothing.
    float pan = 0.0f;

    /// Playback speed AND pitch, because they are the same knob when you resample
    /// by stepping a cursor: 2.0 plays twice as fast an octave up. Chipmunks are
    /// not a bug, they are what this costs; a pitch shift that preserves duration
    /// is a phase vocoder and is not in this course.
    float pitch = 1.0f;

    /// Restart at the beginning on reaching the end. A looping voice never
    /// finishes on its own and must be stopped by id — which is the one case
    /// where letting the handle go is a leak.
    bool loops = false;
};

/// What the audio thread has been doing, sampled at the moment you asked.
///
/// A plain copyable struct assembled from the mixer's internal atomics, not the
/// atomics themselves, so that reading it cannot accidentally become a
/// synchronisation point. Every field is a COUNT or a measured time, for the
/// reason every report in this engine is: when audio is wrong the question is
/// never "is it wrong", it is *which of six things happened*.
struct mixer_report
{
    std::uint64_t buffers = 0;         ///< calls to `mix_into` since `open`
    std::uint64_t frames_mixed = 0;    ///< output frames produced
    std::uint64_t voices_started = 0;
    std::uint64_t voices_finished = 0; ///< reached the end and retired themselves

    /// `play` calls refused because the table was full. Non-zero means the game
    /// is asking for more simultaneous sound than it budgeted for; it does not
    /// mean anything was audibly lost (§3).
    std::uint64_t voices_rejected = 0;

    /// Attempts to pan a stereo sound, or to spatialise one. Counted rather than
    /// asserted because it is a CONTENT mistake — somebody exported a footstep in
    /// stereo — and content mistakes must not stop the program.
    std::uint64_t stereo_pan_refused = 0;

    /// Output samples whose magnitude exceeded 1.0 before the final clamp. The
    /// number §3 exists to keep at zero, and the only distortion measurement the
    /// engine takes for free.
    std::uint64_t clipped = 0;

    /// Callbacks that found the device's queue empty on entry.
    ///
    /// **This was written as the dropout counter and it is not one**, which is
    /// worth keeping in the API rather than quietly deleting. In SDL3's pull
    /// model the callback is invoked when the device wants more, so finding the
    /// queue drained at that instant is the NORMAL state and not a failure: the
    /// first run of `scratch/l78_devcheck` reported **28 of 58 buffers "starved"**
    /// on a mix using 0.2% of its budget, which is the shape of a counter that is
    /// measuring the wrong event. Renamed, and kept because it is still a useful
    /// number — a queue that is empty EVERY time has no slack at all — but the
    /// question "did the player hear a gap" is answered by `late` below.
    ///
    /// The general lesson is 7.7's, in a new subsystem: a control that fires on
    /// the healthy case tells you nothing about the sick one.
    std::uint64_t queue_empty = 0;

    /// Buffers whose mix took longer than the audio they produced.
    ///
    /// **The counter that means something.** Producing 512 frames at 48 kHz has
    /// 10.67 ms of wall clock to happen in; taking longer than that means the
    /// mixer cannot sustain real time, and if it keeps happening the device runs
    /// dry and the player hears silence or a repeat. Unlike `queue_empty` this is
    /// computed entirely from our own two numbers and needs nothing from the
    /// driver, so it is true on every platform.
    ///
    /// Non-zero once, at start-up, is normal — the first buffer touches cold code
    /// and a cold cache. Non-zero repeatedly is a bug or a budget.
    std::uint64_t late = 0;

    std::size_t live_voices = 0;       ///< entries in the voice table right now

    /// Largest output magnitude since `open`, before the clamp. Above 1.0 means
    /// `clipped` is climbing and the master gain is too high.
    float peak = 0.0f;

    double mix_us_total = 0.0;         ///< time spent inside `mix_into`
    double mix_us_max = 0.0;           ///< the worst single buffer, which is the one that matters
    double budget_us = 0.0;            ///< `buffer_frames / freq`, in microseconds

    /// `mix_us_max / budget_us`. Under 1.0 by a wide margin or you are gambling —
    /// and the margin is what §8 argues about, because the mean is not the risk.
    [[nodiscard]] double worst_load() const
    {
        return budget_us > 0.0 ? mix_us_max / budget_us : 0.0;
    }
};

/// The voice table, the mix, and (optionally) a device to play it on.
///
/// Two-phase construction, the same shape as `platform` (5.2) and `gpu_device`
/// (4.2) and for the same reason: opening a device can fail and a constructor
/// has no way to say so.
///
/// **It is useful without a device**, and that is a deliberate design property
/// rather than a side effect. `mix_into` is a pure function of the voice table —
/// call it yourself, into your own buffer, on any thread, with no audio hardware
/// anywhere — which is what makes every number in this lesson reproducible on a
/// build machine with no sound card, and what let this file be tested before it
/// ever made a noise. **Real-time code you cannot call from a test is real-time
/// code you cannot debug.**
///
///     engine::audio::mixer mix;
///     if (!mix.open({})) { /* no device; the game still runs */ }
///
///     const engine::audio::sound step = ...;
///     const engine::audio::voice_id v = mix.play(step, {.gain = 0.8f});
///     ...
///     mix.update();   // once a frame, on the main thread
class mixer
{
public:
    mixer() = default;
    ~mixer();

    mixer(const mixer&) = delete;
    mixer& operator=(const mixer&) = delete;
    mixer(mixer&&) = delete;
    mixer& operator=(mixer&&) = delete;

    // ---- Lifecycle ---------------------------------------------------------

    /// Open a playback device and start the callback.
    ///
    /// Initialises SDL's audio subsystem itself, by refcount
    /// (`SDL_InitSubSystem`), so a program that never mentions audio never pays
    /// for it and a program that does need not care whether `app_config` already
    /// asked. Returns false and logs on failure — **and a program with no sound
    /// card is not a broken program**, so every caller in this repository treats
    /// false as "no audio today" rather than as a fatal error.
    [[nodiscard]] bool open(const mixer_config& cfg = {});

    /// Build the voice table with **no device at all**.
    ///
    /// Everything works afterwards except making a noise: `play`, `set_gain`,
    /// `update`, `report` and above all `mix_into`, which you now call yourself,
    /// into your own buffer, on whatever thread you like. Nothing is asynchronous
    /// and nothing is timed by hardware.
    ///
    /// **This is not a testing convenience bolted on afterwards; it is what makes
    /// the subsystem legible.** A real-time callback is the worst place in a
    /// program to learn anything: you cannot breakpoint it without changing the
    /// result, you cannot print from it, and its output is a pressure wave.
    /// Offline, the same code is a pure function from a voice table to an array
    /// of floats, and an array of floats can be diffed, plotted and asserted on.
    /// Every waveform figure in Lesson 7.8 and every number in
    /// `scratch/verify_78.cpp` comes from this entry point, on a machine that was
    /// never asked whether it had speakers.
    ///
    /// It is also how an engine renders audio to a file, which is what a
    /// regression test and a trailer capture both need.
    [[nodiscard]] bool open_offline(const mixer_config& cfg = {});

    /// Stop the callback, close the device, drop every voice. Idempotent, and the
    /// destructor calls it.
    void close();

    [[nodiscard]] bool is_open() const { return stream_ != nullptr; }

    /// The configuration actually in force, which is not necessarily the one you
    /// passed: `freq` is filled in from the device when you asked for 0.
    [[nodiscard]] const mixer_config& config() const { return cfg_; }

    // ---- Playing -----------------------------------------------------------

    /// Start `s` playing and return a handle to the voice.
    ///
    /// **`s` must outlive the voice.** The mixer holds a non-owning pointer, on
    /// purpose — a four-megabyte sound is not going to be copied per voice, and
    /// a shared_ptr would put a refcount decrement, and therefore possibly a
    /// `free`, on the audio thread. The discipline that makes this safe is
    /// `stop_sound()` immediately before releasing any sound, and it is the one
    /// rule in this file whose violation is undefined behaviour rather than a
    /// counter.
    ///
    /// Returns the null handle if the table is full or `s` is empty; check it the
    /// way you would check an allocation.
    [[nodiscard]] voice_id play(const sound& s, const voice_params& p = {});

    /// Start `s` playing with gains computed from a listener and an emitter.
    ///
    /// Equivalent to `play` followed by `set_gain(spatialise(l, e).gain)`, and it
    /// exists so that the FIRST buffer is already correct: a spatial sound started
    /// at centre and corrected next frame begins with a 16 ms pan sweep, which is
    /// the click of §6 arriving before the sound does.
    [[nodiscard]] voice_id play_spatial(const sound& s, const listener& l, const emitter& e,
                                        const voice_params& p = {});

    /// Set both channel gains directly. This is how a moving emitter is tracked:
    /// call `spatialise` once a frame and push the result here.
    ///
    /// **The mixer RAMPS to this value across the next buffer rather than jumping
    /// to it**, which is the whole of §6 and the reason this function does not
    /// simply assign. Returns false for a stale id.
    bool set_gain(voice_id id, stereo_gain g);

    /// Convenience: gain and pan, combined through the constant-power law.
    /// Refused (and counted) for a stereo sound.
    bool set_gain_pan(voice_id id, float gain, float pan);

    bool set_pitch(voice_id id, float pitch);

    /// True while the voice is still in the table. A non-looping voice becomes
    /// false on its own, one `update()` after it reaches its end.
    [[nodiscard]] bool playing(voice_id id) const;

    /// Stop one voice. Returns false for a stale id, which is the normal outcome
    /// for a fire-and-forget sound and is not an error.
    ///
    /// **Stops it at the next buffer boundary, not mid-buffer**, so the tail of
    /// the current buffer still plays. That is deliberate: a voice cut at an
    /// arbitrary sample is a step to zero, and a step is a click.
    bool stop(voice_id id);

    /// Stop every voice reading `s`. **Call this before releasing any sound**;
    /// see `play`.
    void stop_sound(const sound& s);

    void stop_all();

    // ---- Per-frame ---------------------------------------------------------

    /// Retire voices that finished on the audio thread, and publish counters.
    /// Call once a frame from the main thread; nothing breaks if you forget
    /// except that finished voices keep their table slots.
    void update();

    [[nodiscard]] mixer_report report() const;

    /// Zero the report's cumulative fields (not `live_voices`). For a demo panel
    /// that wants "since I pressed R" rather than "since the program started".
    void reset_report();

    // ---- The mix itself ----------------------------------------------------

    /// Produce `frames` interleaved stereo frames into `out`, advancing every
    /// voice. `out` must have room for `frames * 2` floats.
    ///
    /// **This is the real-time path, and it is public so that it can be
    /// measured.** It is what SDL's callback calls; it is also what
    /// `scratch/verify_78.cpp` calls, with no device open, to produce every
    /// waveform in this lesson. Calling it yourself while a device is open means
    /// two threads mixing at once — the mutex makes that safe and the result
    /// interleaved nonsense, so do one or the other.
    void mix_into(float* out, int frames);

private:
    /// The SDL callback. Static because C calls it; forwards to `mix_into`.
    static void SDLCALL stream_callback(void* userdata, SDL_AudioStream* stream,
                                        int additional_amount, int total_amount);

    struct state;

    mixer_config cfg_{};
    SDL_AudioStream* stream_ = nullptr;

    /// The mutex, the voice pool, the scratch buffer and the counters, in one
    /// heap allocation. A pimpl here is not about compile time: it is so that
    /// this header does not have to name `pool<voice>`, `std::atomic` or
    /// `SDL_Mutex`, and so that `voice` can stay an incomplete type for the
    /// handle's sake.
    state* st_ = nullptr;
};

}   // namespace engine::audio
