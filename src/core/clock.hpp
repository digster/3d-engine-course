// src/core/clock.hpp — the engine's measurement of time.
//
// One job: answer "how long since the last frame?" honestly, and expose enough
// of its own workings that the answer can be questioned. Everything that moves
// will be scaled by the number this produces, so a lie told here is a lie told
// everywhere.

#pragma once

#include <SDL3/SDL.h>

namespace engine {

/// Per-frame timing, measured from a monotonic clock.
///
/// Call tick() exactly once per frame, at the top, and read dt() for the rest of
/// the frame. Between ticks the answers do not change, which is the same
/// frame-coherence guarantee `input` makes — for the same reason. A simulation
/// that asked the clock twice and got two different answers would be integrating
/// with numbers that disagree about how long a frame is.
///
/// Two deliberate design choices, both explained at length in Lesson 1.3:
///
/// 1. **Absolute time is kept as `Uint64` nanoseconds; only the small delta is
///    ever converted to `float`.** A `float` seconds counter loses so much
///    precision as it grows that after about a day of uptime, adding a short
///    frame to it rounds to no change at all and time silently stops advancing.
///    An integer nanosecond counter is exact and does not overflow for 584 years.
///
/// 2. **dt() is clamped; raw_dt() is not.** The clamp is a lie we tell on
///    purpose, so that a breakpoint or a stalled disk cannot hand the simulation
///    a forty-second frame. Anything that needs the truth — a profiler, a frame
///    graph, a log line — asks raw_dt() instead.
class clock
{
public:
    /// @param max_dt_seconds Longest frame the simulation is allowed to see.
    ///        0.25 s is the conventional value: long enough never to trigger in
    ///        normal running, short enough that the worst single step is
    ///        survivable. See was_clamped() for detecting when it bites.
    explicit clock(float max_dt_seconds = 0.25f);

    /// Sample the clock. Call once per frame, before anything reads dt().
    void tick();

    /// Seconds since the previous tick, **clamped** to the configured maximum.
    /// This is the number simulation should use.
    [[nodiscard]] float dt() const { return dt_; }

    /// Seconds since the previous tick, unclamped — what actually happened.
    /// For diagnostics and profiling, never for simulation.
    [[nodiscard]] float raw_dt() const { return raw_dt_; }

    /// True when the last frame was long enough that dt() had to lie about it.
    /// Worth logging: it means the engine's clock and the wall clock have drifted
    /// apart, and it is usually the first evidence of a real performance problem.
    [[nodiscard]] bool was_clamped() const { return raw_dt_ > dt_; }

    /// Seconds since the clock was constructed. `double`, not `float`: this one
    /// grows without bound, and `double` has enough mantissa to stay precise for
    /// far longer than any session will last.
    [[nodiscard]] double elapsed() const;

    /// Nanoseconds since the clock was constructed — the exact, integer form.
    [[nodiscard]] Uint64 elapsed_ns() const { return now_ns_ - start_ns_; }

    /// Frames completed since construction.
    [[nodiscard]] Uint64 frame_count() const { return frames_; }

    /// Frames per second, averaged over roughly half a second.
    ///
    /// **For display only.** An instantaneous 1/dt jitters far too much to read,
    /// so this is smoothed — which makes it a number about the recent past, not
    /// about this frame. Never simulate with it.
    [[nodiscard]] float fps() const { return fps_; }

private:
    // Absolute times, always integer nanoseconds. SDL_GetTicksNS() is built on
    // SDL_GetPerformanceCounter(), which uses CLOCK_MONOTONIC_RAW / mach_absolute_time
    // / QueryPerformanceCounter depending on platform — all monotonic. It therefore
    // cannot run backwards when the user changes their system clock or when NTP
    // adjusts it, which is exactly why we do not use a wall clock here.
    Uint64 start_ns_ = 0;
    Uint64 prev_ns_ = 0;
    Uint64 now_ns_ = 0;

    float dt_ = 0.0f;
    float raw_dt_ = 0.0f;
    float max_dt_ = 0.25f;

    Uint64 frames_ = 0;

    // The fps display window: count frames until half a second of real time has
    // passed, then divide. Simple, stable, and honest about being an average.
    Uint64 fps_window_ns_ = 0;
    Uint64 fps_window_frames_ = 0;
    float fps_ = 0.0f;
};

} // namespace engine
