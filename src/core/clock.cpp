// src/core/clock.cpp — implementation of the frame clock.

#include "core/clock.hpp"

namespace engine {

namespace {

/// Nanoseconds to seconds, via double.
///
/// The division happens in `double` and only the small result is narrowed to
/// `float`. Doing it the other way — converting the nanosecond count to `float`
/// first — would throw away precision before the division: a `float` cannot even
/// represent one second's worth of nanoseconds (1e9) to the nearest integer, so
/// the numerator would already be wrong.
[[nodiscard]] float ns_to_seconds(Uint64 ns)
{
    return static_cast<float>(static_cast<double>(ns) / static_cast<double>(SDL_NS_PER_SECOND));
}

} // namespace

clock::clock(float max_dt_seconds)
    : max_dt_(max_dt_seconds)
{
    // Sample once at construction so the first tick() measures the time from
    // here to the first frame, rather than from SDL's initialisation. Without
    // this, frame one would be handed however long the window and renderer took
    // to create — easily hundreds of milliseconds, and a violent first step.
    start_ns_ = SDL_GetTicksNS();
    prev_ns_ = start_ns_;
    now_ns_ = start_ns_;
}

void clock::tick()
{
    prev_ns_ = now_ns_;
    now_ns_ = SDL_GetTicksNS();

    // The subtraction is unsigned, which would be a bug waiting to happen if the
    // clock could ever run backwards — 0 - 1 in Uint64 is not −1, it is about
    // 18 quintillion, and a single such frame would launch every moving object
    // out of the level. It cannot happen here, because the underlying clock is
    // monotonic (see the header). Relying on that is fine; relying on it
    // *silently* is not, which is why this comment exists.
    const Uint64 delta_ns = now_ns_ - prev_ns_;

    raw_dt_ = ns_to_seconds(delta_ns);

    // The clamp. Everything above the ceiling is discarded, so simulation time
    // falls behind wall time whenever it triggers — the engine effectively enters
    // slow motion rather than taking one enormous step. That is the right trade:
    // a slow frame should degrade smoothly, not teleport the world.
    dt_ = (raw_dt_ > max_dt_) ? max_dt_ : raw_dt_;

    ++frames_;

    // Rolling fps window. Note this counts *real* elapsed nanoseconds, not
    // clamped dt, so the number stays truthful even during a stall.
    fps_window_ns_ += delta_ns;
    ++fps_window_frames_;
    if (fps_window_ns_ >= SDL_NS_PER_SECOND / 2)
    {
        fps_ = static_cast<float>(static_cast<double>(fps_window_frames_)
                                  * static_cast<double>(SDL_NS_PER_SECOND)
                                  / static_cast<double>(fps_window_ns_));
        fps_window_frames_ = 0;
        fps_window_ns_ = 0;
    }
}

double clock::elapsed() const
{
    return static_cast<double>(elapsed_ns()) / static_cast<double>(SDL_NS_PER_SECOND);
}

} // namespace engine
