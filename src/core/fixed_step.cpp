// src/core/fixed_step.cpp — implementation of the fixed-timestep accumulator.

#include "core/fixed_step.hpp"

namespace engine {

namespace {

/// Guard against a rate that would produce a zero or negative step. A step of
/// zero is not a slow simulation, it is an infinite loop: the accumulator would
/// never drop below h, so next_step() would never return false.
[[nodiscard]] float clamp_rate(float rate_hz)
{
    constexpr float k_min_rate = 1.0f;
    constexpr float k_max_rate = 1000.0f;
    if (!(rate_hz >= k_min_rate)) { return k_min_rate; }   // also catches NaN
    if (rate_hz > k_max_rate) { return k_max_rate; }
    return rate_hz;
}

} // namespace

fixed_step::fixed_step(float rate_hz, int max_steps_per_frame)
    : h_(1.0f / clamp_rate(rate_hz))
    , max_steps_(max_steps_per_frame > 0 ? max_steps_per_frame : 1)
{
}

void fixed_step::begin_frame(float dt_seconds)
{
    accumulator_ += dt_seconds;
    steps_ = 0;
    dropped_ = 0.0f;
}

bool fixed_step::next_step()
{
    // Not enough real time banked for a whole step. Whatever is left stays in the
    // accumulator and becomes this frame's alpha.
    if (accumulator_ < h_)
    {
        return false;
    }

    // The spiral-of-death guard.
    //
    // If simulating one step costs more real time than the step represents, then
    // every frame ends further behind than it started: more steps owed, more time
    // spent, more steps owed. Frame times grow without bound and the application
    // locks solid while appearing to do useful work.
    //
    // The fix is to refuse. Past the cap we discard the outstanding time instead
    // of simulating it, which means the simulation permanently falls behind the
    // wall clock — the game goes into slow motion rather than freezing. That is a
    // real loss, not a repair, so we record how much was thrown away and let the
    // caller decide whether to complain about it.
    if (steps_ >= max_steps_)
    {
        while (accumulator_ >= h_)
        {
            accumulator_ -= h_;
            dropped_ += h_;
        }
        return false;
    }

    accumulator_ -= h_;
    ++steps_;
    ++total_steps_;
    return true;
}

float fixed_step::alpha() const
{
    const float a = accumulator_ / h_;

    // After the step loop the invariant guarantees a < 1. Reading alpha() *before*
    // the loop — or immediately after set_rate() shortened the step — can exceed
    // it, and handing a value above 1 to a lerp silently turns interpolation into
    // extrapolation: objects overshoot their true position and snap back. Clamping
    // costs one comparison and removes the possibility.
    return (a > 1.0f) ? 1.0f : a;
}

void fixed_step::set_rate(float rate_hz)
{
    // Deliberately does not touch the accumulator. What it holds is real elapsed
    // time that the simulation still owes; changing the step size changes how that
    // debt is divided up, not whether it exists. Discarding it here would make
    // every rate change a small, silent time skip.
    h_ = 1.0f / clamp_rate(rate_hz);
}

} // namespace engine
