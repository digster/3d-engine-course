// engine/src/audio/mixer.cpp — the voice table, and the loop that must not be late.
//
// Lesson 7.8. Read `mix_into` first; everything else in this file exists to keep
// it safe to call from SDL's audio thread.
//
// ---- WHAT THE AUDIO THREAD IS NOT ALLOWED TO DO ----------------------------
//
// The rules below are not stylistic. Each one has a mechanism behind it and each
// one, broken, produces the same symptom — a click — from a completely different
// cause, which is why they are worth listing in one place:
//
//   * **No allocation.** `new`, `std::vector::push_back`, `std::string` — all of
//     them can take a global heap lock held by the main thread, for an unbounded
//     time. The scratch buffer is sized in `open()` and never grows.
//   * **No logging and no printf.** Both do I/O. §8 measures one log line at
//     **82 ns to format and 1,022 ns to format, write and flush** — and the
//     second number is not the point either, because it is a MEAN. It is a
//     syscall, and a syscall has no upper bound: the same line costs
//     milliseconds when the terminal is a pipe nobody is reading or the file is
//     on a network share. Sixty-four voices of mixing cost 29.8 us; one
//     unlucky log line can cost more than the whole buffer's budget. Counters instead, one atomic increment each.
//   * **No unbounded waiting.** One mutex, held for the length of the mix and
//     nothing else. That is a real and acknowledged compromise; see `state::lock`.
//   * **No exceptions, no RTTI.** Engine-core rule anyway (Lesson 5.3), and here
//     it is also a latency rule.

#include <engine/audio/mixer.hpp>

#include <engine/core/assert.hpp>
#include <engine/core/log.hpp>
#include <engine/core/pool.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace engine::audio
{

/// One playing sound. Completed here rather than in the header so that
/// `handle<voice>` can exist without anybody outside seeing this layout.
struct voice
{
    /// Non-owning. See `mixer::play` for the lifetime rule and why it is a raw
    /// pointer rather than something cleverer.
    const sound* src = nullptr;

    /// Where we are, in SOURCE frames, fractional because of pitch.
    ///
    /// **A double, and it is not paranoia.** A float has 24 bits of mantissa,
    /// which is 16.7 million: at 48 kHz a float cursor stops being able to
    /// represent every frame after **349 seconds**, and what happens then is not
    /// a crash but a playhead that advances in jerks — a slow, unrepeatable
    /// warble on exactly the long ambient loops nobody tests. Lesson 7.7 made the
    /// same call for a clip's clock and for the same reason.
    double cursor = 0.0;

    /// Source frames consumed per output frame: `pitch * src.freq / device.freq`.
    /// One number carries both the pitch and the sample-rate conversion, because
    /// they are the same operation.
    double step = 1.0;

    /// Where the gains ARE, as of the last sample of the last buffer.
    float cur_left = 0.0f;
    float cur_right = 0.0f;

    /// Where the gains should get to by the end of the next buffer. §6.
    float tgt_left = 0.0f;
    float tgt_right = 0.0f;

    bool loops = false;

    /// Set by the audio thread when the cursor runs off the end, or by `stop()`.
    /// The voice is still in the table until the main thread's `update()` reaps
    /// it — which is what keeps `pool::remove` off the audio thread.
    bool finished = false;
};

/// Everything the mutex protects, plus the counters. One allocation, made in
/// `open()` and freed in `close()`.
struct mixer::state
{
    /// Guards `voices` and every field inside a voice. Held by `mix_into` for
    /// the length of a buffer and by the setters for a handful of stores.
    ///
    /// **A mutex on a real-time thread is a compromise and this comment is where
    /// it is admitted.** If the main thread is inside `play()` when the callback
    /// fires, the callback WAITS — and on a machine under load the OS can descend
    /// into priority inversion, where the audio thread blocks on a lock held by a
    /// thread that has been descheduled. The correct answer is a lock-free
    /// command queue: the main thread pushes "start voice 7" into a ring buffer
    /// and the audio thread drains it, so neither ever waits. That needs the
    /// memory-ordering vocabulary of Module 9's job system, which is why it is
    /// there and not here. What makes this acceptable in the meantime is that
    /// every critical section on the main thread is a few stores long — §8
    /// measures the longest at well under a microsecond — and that we can
    /// measure it at all.
    SDL_Mutex* lock = nullptr;

    pool<voice> voices;

    /// The callback's working buffer, sized once. `mix_into` writes here and
    /// `SDL_PutAudioStreamData` copies out of it.
    std::vector<float> scratch;

    // ---- Counters ----------------------------------------------------------
    //
    // Atomics rather than plain integers even though most of them are only ever
    // written under the lock, and the reason is `report()`: it reads them WITHOUT
    // taking the lock, on purpose, so that a debug panel drawing at 60 Hz cannot
    // stall the audio thread. A non-atomic read racing a write is undefined
    // behaviour; a relaxed atomic read of a counter is merely possibly one
    // increment out of date, which for a diagnostic is exactly right.
    std::atomic<std::uint64_t> buffers{0};
    std::atomic<std::uint64_t> frames_mixed{0};
    std::atomic<std::uint64_t> voices_started{0};
    std::atomic<std::uint64_t> voices_finished{0};
    std::atomic<std::uint64_t> voices_rejected{0};
    std::atomic<std::uint64_t> stereo_pan_refused{0};
    std::atomic<std::uint64_t> clipped{0};
    std::atomic<std::uint64_t> queue_empty{0};
    std::atomic<std::uint64_t> late{0};
    std::atomic<float> peak{0.0f};
    std::atomic<double> mix_us_total{0.0};
    std::atomic<double> mix_us_max{0.0};

    ~state()
    {
        if (lock != nullptr) { SDL_DestroyMutex(lock); }
    }
};

namespace
{

/// RAII for an `SDL_Mutex*`. Eleven lines, and it exists because the alternative
/// is remembering to unlock on four early returns — which is the bug this course
/// has been calling "the reason RAII exists" since Lesson 1.6, in the one place
/// where forgetting it deadlocks the audio thread rather than leaking a handle.
class lock_guard
{
public:
    explicit lock_guard(SDL_Mutex* m) : m_(m) { if (m_) { SDL_LockMutex(m_); } }
    ~lock_guard() { if (m_) { SDL_UnlockMutex(m_); } }

    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;

private:
    SDL_Mutex* m_ = nullptr;
};

/// Read one source frame with linear interpolation between neighbours.
///
/// The same rule as `sound.cpp`'s resampler — deliberately, since pitching a
/// sound and converting its rate are the same operation — but written for one
/// frame at a time because the mixer's cursor moves by a step that can change
/// between buffers.
///
/// The `i1` clamp at the last frame means the final fraction of a sample
/// interpolates toward itself rather than past the end. For a looping sound that
/// is technically wrong — it should interpolate toward frame 0 — and the error
/// is one sample per loop at a magnitude bounded by the signal's own slope. Named
/// rather than fixed, because fixing it costs a branch in the innermost loop in
/// the engine and the exercise at the end asks you to measure whether it is worth
/// it.
inline void fetch(const sound& s, double cursor, float& l, float& r)
{
    const std::size_t frames = s.frames();
    const std::size_t i0 = static_cast<std::size_t>(cursor);
    if (i0 >= frames) { l = 0.0f; r = 0.0f; return; }

    const std::size_t i1 = std::min(i0 + 1, frames - 1);
    const float t = static_cast<float>(cursor - static_cast<double>(i0));

    if (s.channels == 1)
    {
        const float a = s.samples[i0];
        const float b = s.samples[i1];
        l = a + (b - a) * t;
        r = l;
    }
    else
    {
        const std::size_t ch = static_cast<std::size_t>(s.channels);
        const float a0 = s.samples[i0 * ch];
        const float b0 = s.samples[i1 * ch];
        const float a1 = s.samples[i0 * ch + 1];
        const float b1 = s.samples[i1 * ch + 1];
        l = a0 + (b0 - a0) * t;
        r = a1 + (b1 - a1) * t;
    }
}

/// `std::atomic<float>` has no `fetch_max`, so this is the compare-exchange loop
/// that one would be. Spins only when two threads race to raise the peak, which
/// with one audio thread means never.
inline void atomic_max(std::atomic<float>& target, float value)
{
    float current = target.load(std::memory_order_relaxed);
    while (value > current
           && !target.compare_exchange_weak(current, value, std::memory_order_relaxed))
    {
    }
}

inline void atomic_max(std::atomic<double>& target, double value)
{
    double current = target.load(std::memory_order_relaxed);
    while (value > current
           && !target.compare_exchange_weak(current, value, std::memory_order_relaxed))
    {
    }
}

inline void atomic_add(std::atomic<double>& target, double value)
{
    double current = target.load(std::memory_order_relaxed);
    while (!target.compare_exchange_weak(current, current + value, std::memory_order_relaxed))
    {
    }
}

}   // namespace

mixer::~mixer()
{
    close();
}

bool mixer::open_offline(const mixer_config& cfg)
{
    close();

    cfg_ = cfg;

    if (cfg_.channels != 2)
    {
        ENGINE_LOG_ERROR(log_audio, "mixer: channels must be 2, got %d", cfg_.channels);
        return false;
    }
    cfg_.buffer_frames = std::max(64, cfg_.buffer_frames);
    cfg_.max_voices = std::clamp(cfg_.max_voices, 1, 4096);
    if (cfg_.freq <= 0) { cfg_.freq = k_default_freq; }

    st_ = new state();
    st_->scratch.assign(static_cast<std::size_t>(cfg_.buffer_frames) * 2u, 0.0f);
    st_->lock = SDL_CreateMutex();
    if (st_->lock == nullptr)
    {
        ENGINE_LOG_ERROR(log_audio, "mixer: SDL_CreateMutex: %s", SDL_GetError());
        delete st_;
        st_ = nullptr;
        return false;
    }
    return true;
}

bool mixer::open(const mixer_config& cfg)
{
    close();

    // Refcounted: SDL keeps a count per subsystem, so this composes with an
    // `app_config::extra_subsystems` that already asked for audio and with a
    // second mixer in the same process. The matching QuitSubSystem is in close().
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        ENGINE_LOG_ERROR(log_audio, "mixer: SDL_InitSubSystem(AUDIO): %s", SDL_GetError());
        return false;
    }

    // Ask the device what it runs at when the caller did not care. SDL will
    // happily convert for us, but a conversion we did not ask for is a resampler
    // in the path we did not measure. This must happen BEFORE open_offline,
    // because that is what settles `cfg_.freq`.
    mixer_config wanted = cfg;
    if (wanted.freq <= 0)
    {
        SDL_AudioSpec device_spec{};
        int device_frames = 0;
        if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                     &device_spec, &device_frames)
            && device_spec.freq > 0)
        {
            wanted.freq = device_spec.freq;
        }
    }

    if (!open_offline(wanted))
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    const SDL_AudioSpec want{SDL_AUDIO_F32, cfg_.channels, cfg_.freq};

    // SDL_OpenAudioDeviceStream with a callback is SDL3's pull model: SDL runs
    // `stream_callback` on its own thread whenever the device is hungry. The
    // stream it returns is created PAUSED — SDL3 changed this from SDL2, where a
    // device started running the moment you opened it — so the Resume below is
    // not optional and its absence is a program that is silent with every counter
    // reading zero.
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want,
                                        &mixer::stream_callback, this);
    if (stream_ == nullptr)
    {
        ENGINE_LOG_ERROR(log_audio, "mixer: SDL_OpenAudioDeviceStream: %s", SDL_GetError());
        delete st_;
        st_ = nullptr;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    SDL_ResumeAudioStreamDevice(stream_);

    const SDL_AudioDeviceID dev = SDL_GetAudioStreamDevice(stream_);
    const char* device_name = SDL_GetAudioDeviceName(dev);
    ENGINE_LOG_INFO(log_audio, "mixer: %s  %d Hz  %d ch  %d frames (%.2f ms)  %d voices",
                    device_name ? device_name : "(default)", cfg_.freq, cfg_.channels,
                    cfg_.buffer_frames,
                    1000.0 * cfg_.buffer_frames / static_cast<double>(cfg_.freq),
                    cfg_.max_voices);
    return true;
}

void mixer::close()
{
    if (stream_ != nullptr)
    {
        // Destroying the stream unbinds it from the device and waits for the
        // callback to finish, so after this line no audio thread is looking at
        // `st_`. Deleting `st_` before this would be a use-after-free with a
        // window of one buffer — the classic shutdown race, and the reason the
        // order of these four statements is not arbitrary.
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    // `~state` destroys the mutex, which is why this line is safe to reach with
    // a half-opened mixer: `open()` deletes `st_` on every one of its own failure
    // paths, so either both exist or neither does.
    delete st_;
    st_ = nullptr;
}

voice_id mixer::play(const sound& s, const voice_params& p)
{
    if (st_ == nullptr) { return voice_id{}; }
    if (s.empty() || s.channels <= 0 || s.freq <= 0) { return voice_id{}; }

    voice v;
    v.src = &s;
    v.cursor = 0.0;
    v.step = static_cast<double>(std::max(0.0f, p.pitch))
             * static_cast<double>(s.freq) / static_cast<double>(cfg_.freq);
    v.loops = p.loops;

    stereo_gain g{p.gain, p.gain};
    if (s.mono())
    {
        const stereo_gain pan = pan_constant_power(p.pan);
        g = {p.gain * pan.left, p.gain * pan.right};
    }
    else if (p.pan != 0.0f)
    {
        st_->stereo_pan_refused.fetch_add(1, std::memory_order_relaxed);
    }

    // A voice STARTS at its target gain rather than ramping up from zero. That
    // looks like it contradicts §6 and does not: the ramp exists to get from one
    // known gain to another without a step, and a voice that has never played has
    // no previous gain — its first sample is preceded by silence in the SOURCE,
    // which is the sound's own business (`apply_fade`) and not the mixer's.
    v.cur_left = g.left;
    v.cur_right = g.right;
    v.tgt_left = g.left;
    v.tgt_right = g.right;

    lock_guard guard(st_->lock);

    if (static_cast<int>(st_->voices.size()) >= cfg_.max_voices)
    {
        st_->voices_rejected.fetch_add(1, std::memory_order_relaxed);
        return voice_id{};
    }

    const voice_id id = st_->voices.insert(std::move(v));
    if (id.valid()) { st_->voices_started.fetch_add(1, std::memory_order_relaxed); }
    return id;
}

voice_id mixer::play_spatial(const sound& s, const listener& l, const emitter& e,
                             const voice_params& p)
{
    if (!s.mono() && st_ != nullptr)
    {
        st_->stereo_pan_refused.fetch_add(1, std::memory_order_relaxed);
    }

    voice_params flat = p;
    flat.gain = 1.0f;   // the spatial result carries the gain; do not apply it twice
    flat.pan = 0.0f;

    const voice_id id = play(s, flat);
    if (!id.valid()) { return id; }

    const spatial_result r = spatialise(l, e);
    // Assign rather than ramp: the voice has not produced a sample yet, so there
    // is nothing to ramp FROM. Going through set_gain here would start it at
    // centre and sweep, which is audible on a sound that begins loudly.
    lock_guard guard(st_->lock);
    if (voice* v = st_->voices.get(id))
    {
        v->cur_left = r.gain.left * p.gain;
        v->cur_right = r.gain.right * p.gain;
        v->tgt_left = v->cur_left;
        v->tgt_right = v->cur_right;
    }
    return id;
}

bool mixer::set_gain(voice_id id, stereo_gain g)
{
    if (st_ == nullptr) { return false; }
    lock_guard guard(st_->lock);
    voice* v = st_->voices.get(id);
    if (v == nullptr) { return false; }
    v->tgt_left = g.left;
    v->tgt_right = g.right;
    return true;
}

bool mixer::set_gain_pan(voice_id id, float gain, float pan)
{
    if (st_ == nullptr) { return false; }
    lock_guard guard(st_->lock);
    voice* v = st_->voices.get(id);
    if (v == nullptr) { return false; }

    if (v->src != nullptr && !v->src->mono())
    {
        st_->stereo_pan_refused.fetch_add(1, std::memory_order_relaxed);
        v->tgt_left = gain;
        v->tgt_right = gain;
        return true;
    }

    const stereo_gain p = pan_constant_power(pan);
    v->tgt_left = gain * p.left;
    v->tgt_right = gain * p.right;
    return true;
}

bool mixer::set_pitch(voice_id id, float pitch)
{
    if (st_ == nullptr) { return false; }
    lock_guard guard(st_->lock);
    voice* v = st_->voices.get(id);
    if (v == nullptr || v->src == nullptr) { return false; }
    v->step = static_cast<double>(std::max(0.0f, pitch))
              * static_cast<double>(v->src->freq) / static_cast<double>(cfg_.freq);
    return true;
}

bool mixer::playing(voice_id id) const
{
    if (st_ == nullptr) { return false; }
    lock_guard guard(st_->lock);
    const voice* v = st_->voices.get(id);
    return v != nullptr && !v->finished;
}

bool mixer::stop(voice_id id)
{
    if (st_ == nullptr) { return false; }
    lock_guard guard(st_->lock);
    voice* v = st_->voices.get(id);
    if (v == nullptr) { return false; }
    v->finished = true;
    return true;
}

void mixer::stop_sound(const sound& s)
{
    if (st_ == nullptr) { return; }
    lock_guard guard(st_->lock);
    for (voice& v : st_->voices.items())
    {
        if (v.src == &s) { v.finished = true; v.src = nullptr; }
    }
}

void mixer::stop_all()
{
    if (st_ == nullptr) { return; }
    lock_guard guard(st_->lock);
    for (voice& v : st_->voices.items()) { v.finished = true; v.src = nullptr; }
}

void mixer::update()
{
    if (st_ == nullptr) { return; }
    lock_guard guard(st_->lock);

    // Backwards, because `pool::remove` swaps the last live item into the hole:
    // walking forwards would step past the item that just moved into the slot we
    // are standing on. This is the same iterate-and-erase shape `ecs::registry`
    // uses (5.7), and getting it wrong here retires every other finished voice.
    for (std::size_t d = st_->voices.size(); d-- > 0;)
    {
        if (st_->voices.items()[d].finished)
        {
            st_->voices.remove(st_->voices.handle_at(d));
        }
    }
}

mixer_report mixer::report() const
{
    mixer_report r;
    if (st_ == nullptr) { return r; }

    r.buffers = st_->buffers.load(std::memory_order_relaxed);
    r.frames_mixed = st_->frames_mixed.load(std::memory_order_relaxed);
    r.voices_started = st_->voices_started.load(std::memory_order_relaxed);
    r.voices_finished = st_->voices_finished.load(std::memory_order_relaxed);
    r.voices_rejected = st_->voices_rejected.load(std::memory_order_relaxed);
    r.stereo_pan_refused = st_->stereo_pan_refused.load(std::memory_order_relaxed);
    r.clipped = st_->clipped.load(std::memory_order_relaxed);
    r.queue_empty = st_->queue_empty.load(std::memory_order_relaxed);
    r.late = st_->late.load(std::memory_order_relaxed);
    r.peak = st_->peak.load(std::memory_order_relaxed);
    r.mix_us_total = st_->mix_us_total.load(std::memory_order_relaxed);
    r.mix_us_max = st_->mix_us_max.load(std::memory_order_relaxed);
    r.budget_us = cfg_.freq > 0
                      ? 1e6 * static_cast<double>(cfg_.buffer_frames) / static_cast<double>(cfg_.freq)
                      : 0.0;

    // The ONE field that needs the lock, because it is not a counter: reading a
    // size while another thread inserts is a race, not a stale number.
    {
        lock_guard guard(st_->lock);
        r.live_voices = st_->voices.size();
    }
    return r;
}

void mixer::reset_report()
{
    if (st_ == nullptr) { return; }
    st_->buffers.store(0, std::memory_order_relaxed);
    st_->frames_mixed.store(0, std::memory_order_relaxed);
    st_->voices_started.store(0, std::memory_order_relaxed);
    st_->voices_finished.store(0, std::memory_order_relaxed);
    st_->voices_rejected.store(0, std::memory_order_relaxed);
    st_->stereo_pan_refused.store(0, std::memory_order_relaxed);
    st_->clipped.store(0, std::memory_order_relaxed);
    st_->queue_empty.store(0, std::memory_order_relaxed);
    st_->late.store(0, std::memory_order_relaxed);
    st_->peak.store(0.0f, std::memory_order_relaxed);
    st_->mix_us_total.store(0.0, std::memory_order_relaxed);
    st_->mix_us_max.store(0.0, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// The real-time path
// ---------------------------------------------------------------------------

void mixer::mix_into(float* out, int frames)
{
    if (out == nullptr || frames <= 0) { return; }

    const std::uint64_t t0 = SDL_GetPerformanceCounter();
    const std::size_t samples = static_cast<std::size_t>(frames) * 2u;

    // Silence first, then ADD each voice. Mixing IS addition (§2) and this line
    // is where that is literally true — there is no "first voice" special case,
    // and a mix of zero voices is correctly silent rather than uninitialised.
    std::fill(out, out + samples, 0.0f);

    if (st_ != nullptr)
    {
        lock_guard guard(st_->lock);

        const float inv_frames = 1.0f / static_cast<float>(frames);

        for (voice& v : st_->voices.items())
        {
            if (v.finished || v.src == nullptr || v.src->empty()) { continue; }

            const sound& s = *v.src;
            const auto src_frames = static_cast<double>(s.frames());

            // ---- The gain ramp, and the whole of §6 --------------------------
            //
            // The caller sets a TARGET once a frame — at 60 Hz, once every 800
            // output frames at 48 kHz. Applying it as a step at the buffer
            // boundary injects a discontinuity of (delta gain x the signal's
            // value there), which is a click. Interpolating across the buffer
            // spreads the same change over every sample, so the largest step any
            // one sample sees is 1/frames of it.
            //
            // This is Lesson 7.7's rule, in a subsystem that had never heard of
            // it: A LOSSY TRANSFORMATION MUST BE FITTED WITH ITS CONSUMER'S EXACT
            // RECONSTRUCTION. The gain curve is sampled at 60 Hz and reconstructed
            // at 48 kHz, and the reconstruction is either a zero-order hold or a
            // linear one. One of them is audible.
            const float dl = (v.tgt_left - v.cur_left) * inv_frames;
            const float dr = (v.tgt_right - v.cur_right) * inv_frames;
            float gl = v.cur_left;
            float gr = v.cur_right;

            int f = 0;
            for (; f < frames; ++f)
            {
                if (v.cursor >= src_frames)
                {
                    if (!v.loops) { break; }
                    // SUBTRACT, do not fmod. Lesson 7.7 measured std::fmod's cost
                    // as quotient-dependent — it is not constant time — and the
                    // cursor here moves by less than a frame per sample, so one
                    // subtraction always suffices. `while` rather than `if`
                    // guards the case of a pitch so high that a whole sound fits
                    // inside one output frame.
                    while (v.cursor >= src_frames && src_frames > 0.0)
                    {
                        v.cursor -= src_frames;
                    }
                }

                float sl = 0.0f;
                float sr = 0.0f;
                fetch(s, v.cursor, sl, sr);

                out[static_cast<std::size_t>(f) * 2u]      += sl * gl;
                out[static_cast<std::size_t>(f) * 2u + 1u] += sr * gr;

                gl += dl;
                gr += dr;
                v.cursor += v.step;
            }

            v.cur_left = v.tgt_left;
            v.cur_right = v.tgt_right;

            if (f < frames)
            {
                // Ran off the end of a non-looping sound. Mark it and leave it;
                // `update()` on the main thread does the removal, so no container
                // is mutated here.
                v.finished = true;
                st_->voices_finished.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    // ---- Master gain, peak, and the clamp ---------------------------------
    //
    // The clamp is the LAST resort and not the plan: by the time it does anything
    // the sum has already exceeded full scale, and hard-clipping a waveform
    // flattens its peaks into straight lines, which is audible as a harsh buzz
    // (§3 measures the harmonics it adds). `clipped` climbing is the signal to
    // lower `master_gain`, not to make the clamp cleverer.
    const float master = cfg_.master_gain;
    float local_peak = 0.0f;
    std::uint64_t local_clipped = 0;
    for (std::size_t i = 0; i < samples; ++i)
    {
        float v = out[i] * master;
        const float mag = std::abs(v);
        if (mag > local_peak) { local_peak = mag; }
        if (mag > 1.0f)
        {
            ++local_clipped;
            v = v > 0.0f ? 1.0f : -1.0f;
        }
        out[i] = v;
    }

    if (st_ != nullptr)
    {
        atomic_max(st_->peak, local_peak);
        if (local_clipped != 0)
        {
            st_->clipped.fetch_add(local_clipped, std::memory_order_relaxed);
        }
        st_->buffers.fetch_add(1, std::memory_order_relaxed);
        st_->frames_mixed.fetch_add(static_cast<std::uint64_t>(frames), std::memory_order_relaxed);

        const double us = 1e6 * static_cast<double>(SDL_GetPerformanceCounter() - t0)
                          / static_cast<double>(SDL_GetPerformanceFrequency());
        atomic_add(st_->mix_us_total, us);
        atomic_max(st_->mix_us_max, us);

        // The deadline for THIS chunk, not for a full buffer: the callback is
        // free to ask for a partial one, and charging a 64-frame chunk with a
        // 512-frame budget would hide exactly the overrun this counter exists to
        // find.
        const double chunk_budget_us = cfg_.freq > 0
            ? 1e6 * static_cast<double>(frames) / static_cast<double>(cfg_.freq)
            : 0.0;
        if (chunk_budget_us > 0.0 && us > chunk_budget_us)
        {
            st_->late.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void SDLCALL mixer::stream_callback(void* userdata, SDL_AudioStream* stream,
                                    int additional_amount, int /*total_amount*/)
{
    auto* self = static_cast<mixer*>(userdata);
    if (self == nullptr || self->st_ == nullptr || additional_amount <= 0) { return; }

    // Queue depth on entry, in bytes.
    //
    // THIS WAS WRITTEN AS AN UNDERRUN DETECTOR AND IT IS NOT ONE. The reasoning
    // was: an empty queue means the device has already run out of what we gave it
    // last time, so we are late. The reasoning is wrong for SDL3's pull model,
    // where the callback is invoked BECAUSE the device wants more — an empty
    // queue at that instant is the steady state. Measured on a real device at
    // 0.2% load: 28 of 58 buffers reported "starved" while nothing whatsoever
    // went wrong. Kept, renamed to what it actually observes, and the counter
    // that answers the real question is `late`, below, which compares our own
    // mixing time against our own deadline and needs nothing from the driver.
    if (SDL_GetAudioStreamQueued(stream) == 0
        && self->st_->buffers.load(std::memory_order_relaxed) > 0)
    {
        self->st_->queue_empty.fetch_add(1, std::memory_order_relaxed);
    }

    const int bytes_per_frame = 2 * static_cast<int>(sizeof(float));
    int frames_needed = additional_amount / bytes_per_frame;

    // Produce in fixed-size bites rather than whatever SDL asked for, so that the
    // per-buffer cost §8 measures is a cost per KNOWN number of frames, and so
    // that `scratch` never has to grow on this thread.
    while (frames_needed > 0)
    {
        const int chunk = std::min(frames_needed, self->cfg_.buffer_frames);
        self->mix_into(self->st_->scratch.data(), chunk);
        SDL_PutAudioStreamData(stream, self->st_->scratch.data(),
                               chunk * bytes_per_frame);
        frames_needed -= chunk;
    }
}

}   // namespace engine::audio
