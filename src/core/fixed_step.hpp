// src/core/fixed_step.hpp — the simulation's own clock.
//
// Lesson 1.3 established that the frame rate must not decide how far the
// simulation advances. This is the thing that separates them: it takes real
// elapsed time in, and hands out simulation steps of a fixed, chosen size.

#pragma once

#include <SDL3/SDL.h>

namespace engine {

/// An accumulator that converts variable real time into fixed simulation steps.
///
/// The whole idea in three lines of usage:
///
///     stepper.begin_frame(clk.dt());
///     while (stepper.next_step()) { previous = current; simulate(current, stepper.h()); }
///     render(lerp(previous, current, stepper.alpha()));
///
/// Real time goes into an accumulator. Whole steps of `h` seconds are taken out
/// of it for as long as there is a whole step's worth left. Whatever remains is
/// less than one step, is carried into the next frame, and — expressed as a
/// fraction in `alpha()` — is what the renderer uses to interpolate.
///
/// Why this is a class rather than four lines in the loop: the accumulator has an
/// invariant (after the step loop, `0 <= accumulator < h`) that is maintained by
/// exactly one subtraction. Written inline, that subtraction is one `continue`
/// away from being skipped, and the failure mode is an infinite loop inside a
/// frame. Making the type own it means the invariant cannot be violated from
/// outside.
class fixed_step
{
public:
    /// @param rate_hz Simulation steps per second. 60 is the usual choice; see
    ///        the lesson for the tradeoff against 30 and 120.
    /// @param max_steps_per_frame The spiral-of-death guard. If one frame ever
    ///        needs more steps than this, the excess is discarded rather than
    ///        simulated — the game falls behind real time instead of locking up.
    explicit fixed_step(float rate_hz = 60.0f, int max_steps_per_frame = 8);

    /// Add one frame's real elapsed time. Call once per frame, before stepping.
    /// Pass the **clamped** dt: an unclamped 40-second frame after a breakpoint
    /// would ask for 2400 steps and immediately trip the guard below.
    void begin_frame(float dt_seconds);

    /// Take one step out of the accumulator. Returns false when less than a whole
    /// step remains, or when this frame's step cap has been reached.
    [[nodiscard]] bool next_step();

    /// The fixed step size, in seconds. This is the `dt` simulation must use —
    /// never the frame's real duration.
    [[nodiscard]] float h() const { return h_; }

    [[nodiscard]] float rate_hz() const { return 1.0f / h_; }

    /// How far the renderer sits between the previous step and the current one,
    /// in [0, 1]. Feed this to a lerp between the two most recent states.
    [[nodiscard]] float alpha() const;

    [[nodiscard]] int steps_this_frame() const { return steps_; }

    /// Total steps ever taken. Multiply by h() for the simulation's own clock,
    /// which is the number to compare against real elapsed time when looking for
    /// drift.
    [[nodiscard]] Uint64 total_steps() const { return total_steps_; }

    /// Seconds thrown away by the step cap this frame. Non-zero means the machine
    /// could not keep up and the simulation has permanently fallen that far
    /// behind real time. Worth surfacing: it is silent otherwise.
    [[nodiscard]] float dropped_seconds() const { return dropped_; }

    /// Change the simulation rate. Whatever is already in the accumulator is real
    /// time that is still owed, so it carries over and is simply diced differently
    /// from here on. Call it before begin_frame(), not in the middle of stepping.
    void set_rate(float rate_hz);

private:
    float h_;
    int max_steps_;

    // The accumulator is bounded — the step loop drains it below h_ every frame —
    // so unlike absolute time in `clock`, a float is entirely safe here. The rule
    // from Lesson 1.3 is about quantities that grow without bound, and this is
    // deliberately not one of them.
    float accumulator_ = 0.0f;

    int steps_ = 0;
    Uint64 total_steps_ = 0;
    float dropped_ = 0.0f;
};

} // namespace engine
