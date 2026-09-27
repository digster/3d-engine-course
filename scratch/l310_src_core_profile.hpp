// src/core/profile.hpp — measuring the engine, rather than guessing about it.
//
// Every lesson since 3.1 has produced one number on demand: 75x for the bounding
// box, 31.6% for back-face culling, 1.26x for bilinear over nearest. Each of
// those was a one-off answer to a one-off question. This file is the habit turned
// into a tool.
//
// It answers exactly one question — **where does a frame go?** — and it is worth
// being clear about what it therefore cannot answer, because reaching for the
// wrong instrument is how people spend a week optimising 3% of a frame:
//
//   THE FRAME BUDGET (this file).       Which PHASE costs what. Always on,
//                                       coarse, cheap enough to leave in a
//                                       shipping build.
//   THE DIFFERENTIAL BENCHMARK.         What one LINE costs. Two builds that
//                                       differ only in the line under test, run
//                                       offline, thousands of iterations. Lives
//                                       in scratch/verify_310.cpp, not here.
//   THE COUNTER.                        How much WORK there is — pixels,
//                                       triangles, texels. Free, and it is what
//                                       explains the other two. Lessons 3.4 and
//                                       3.9 already ship several.
//
// The instrument you must not build is the fourth one everybody tries first: a
// timer inside the pixel loop. Lesson 3.10 §4.2 measures what that does, and the
// answer is that the measurement becomes larger than the thing measured.

#pragma once

#include <SDL3/SDL.h>

#include <array>

namespace engine {

/// The phases a frame is divided into.
///
/// **These are chosen to be disjoint and to cover the frame**, which is the only
/// property that makes the numbers add up to something. They are also chosen to
/// be *coarse*: each one is tens of microseconds at least, so the cost of reading
/// the clock twice (measured — see `profiler::overhead_ns`) is a rounding error
/// rather than the measurement.
///
/// The order is the order they happen in, so a bar chart drawn straight from this
/// enum reads as a timeline.
enum class zone
{
    build,     ///< Placing objects: transforms, the scene graph such as it is.
    collect,   ///< Model -> world -> view -> clip -> NDC -> screen, plus clipping
               ///< and per-vertex lighting. The "vertex stage".
    sort,      ///< The painter's-algorithm depth sort, when it is running at all.
    fill,      ///< Rasterization. The "fragment stage".
    overlay,   ///< HUD, debug draw, the profiler's own bars.
    present,   ///< Locking the streaming texture, copying, and SDL_RenderPresent.
    count      ///< Not a zone: the number of them. Must stay last.
};

/// How many real zones there are. `zone::count` is the sentinel, never a slot.
inline constexpr int k_zone_count = static_cast<int>(zone::count);

/// A short label for a zone, for HUDs and logs.
[[nodiscard]] const char* zone_name(zone z);

/// Per-frame timings for a fixed set of phases, with a short history.
///
/// Usage is three calls:
///
///     prof.begin_frame();
///     { const scope_timer t{prof, zone::fill}; draw_everything(); }
///     prof.end_frame();
///
/// and then `prof.median_ns(zone::fill)` for a number that does not flicker.
///
/// ---- Why a MEDIAN and not a mean ----
///
/// A frame time is not a measurement of the renderer. It is a measurement of the
/// renderer *plus* whatever else the machine decided to do during those sixteen
/// milliseconds — a page fault, a scheduler decision, another process waking up,
/// the display server compositing. Those events are rare and enormous, which is
/// precisely the shape that ruins a mean: one 40 ms hitch moves the mean of 120
/// frames by a third of a millisecond and moves the median by nothing at all.
///
/// The median is answering the question you actually asked ("what does a typical
/// frame cost?"). The mean answers a different one ("what is the total cost
/// divided by the count?"), which is the right question for a battery budget and
/// the wrong one for finding a hotspot. Lesson 3.10 §2.4.
///
/// ---- Why the history is bounded ----
///
/// `k_history` frames, in a ring. Two seconds at 60 Hz: long enough that the
/// median is stable, short enough that it responds when you press a key and the
/// cost genuinely changes. An unbounded history would take a minute to notice
/// that you switched to nearest filtering, which makes it useless as a tool you
/// steer with.
class profiler
{
public:
    /// Frames kept for the median. 120 = about two seconds at 60 Hz.
    static constexpr int k_history = 120;

    profiler();

    /// Start a frame. Clears this frame's accumulators and starts the total.
    void begin_frame();

    /// End a frame and commit it to the history.
    void end_frame();

    /// Add a measured interval to a zone. Called by `scope_timer`; public because
    /// a phase that is not a lexical scope (a loop body split across functions)
    /// still deserves to be measured.
    void add(zone z, Uint64 ns);

    /// This frame's total, and this frame's cost for one zone.
    [[nodiscard]] Uint64 frame_ns() const { return frame_ns_; }
    [[nodiscard]] Uint64 last_ns(zone z) const { return current_[index(z)]; }

    /// The median over the history — the number to read and to quote.
    [[nodiscard]] Uint64 median_ns(zone z) const;
    [[nodiscard]] Uint64 median_frame_ns() const;

    /// Time inside the frame that no zone claimed.
    ///
    /// **This is the most important number here and it is the one everybody
    /// forgets to display.** If the zones sum to 60% of the frame, then the
    /// largest single item in your budget is the 40% you are not looking at, and
    /// every optimisation you make to the other 60% is bounded by Amdahl's law
    /// before you start. Showing the remainder is what turns a list of timings
    /// into a budget.
    ///
    /// Clamped at zero: it can only go negative if two zones overlapped, which is
    /// a programming error rather than a measurement, and `zones_overlapped()`
    /// reports it separately rather than hiding it in a negative bar.
    [[nodiscard]] Uint64 other_ns() const;

    /// True if this frame's zones summed to more than the frame itself — i.e. two
    /// `scope_timer`s were alive at once. Zones must be disjoint; nesting them
    /// double-counts, and a budget that double-counts is worse than no budget.
    [[nodiscard]] bool zones_overlapped() const { return overlapped_; }

    /// Frames committed since construction.
    [[nodiscard]] Uint64 frames() const { return frames_; }

    /// The clock's own granularity, in nanoseconds — `1e9 / SDL_GetPerformanceFrequency()`.
    ///
    /// **Read this before believing any small number.** A counter ticking at
    /// 24 MHz cannot resolve anything shorter than 42 ns, and a zone reported as
    /// 20 ns from such a clock is not a fast zone, it is a quantisation artifact.
    [[nodiscard]] double resolution_ns() const { return resolution_ns_; }

    /// The measured cost of one `scope_timer` — two counter reads and an add.
    ///
    /// Measured at construction, not assumed, and exposed so a reader can apply
    /// the one rule that keeps instrumentation honest: **never instrument
    /// anything whose cost is within an order of magnitude of this number.**
    [[nodiscard]] double overhead_ns() const { return overhead_ns_; }

    /// Raw counter ticks -> nanoseconds, using this machine's frequency.
    [[nodiscard]] Uint64 to_ns(Uint64 ticks) const;

    /// Read the counter. Public so a caller can time something by hand.
    [[nodiscard]] static Uint64 now_ticks() { return SDL_GetPerformanceCounter(); }

private:
    [[nodiscard]] static constexpr int index(zone z) { return static_cast<int>(z); }

    Uint64 freq_ = 1;
    double resolution_ns_ = 0.0;
    double overhead_ns_ = 0.0;

    Uint64 frame_start_ = 0;
    Uint64 frame_ns_ = 0;
    bool overlapped_ = false;
    Uint64 frames_ = 0;

    /// This frame's accumulators, in nanoseconds. Reset by begin_frame().
    std::array<Uint64, k_zone_count> current_{};

    /// The ring. `history_[z][i]` is zone z's cost in the i-th committed frame;
    /// `frame_history_[i]` is that frame's total.
    std::array<std::array<Uint64, k_history>, k_zone_count> history_{};
    std::array<Uint64, k_history> frame_history_{};
    int write_ = 0;
    int filled_ = 0;
};

/// Times the scope it lives in and adds the result to one zone. RAII, Lesson 0.5.
///
/// The constructor reads the clock, the destructor reads it again and adds the
/// difference. That is the whole class, and the reason it is a class rather than
/// a matched pair of calls is the reason RAII exists at all: an early `return` in
/// the middle of a phase would skip the closing call and silently lose the
/// measurement, whereas a destructor runs on every path out of a scope including
/// the ones you did not think of.
///
/// Non-copyable and non-movable: two copies would each report the interval, so
/// the zone would count it twice. Deleting the operations makes that unwritable
/// rather than merely discouraged.
class scope_timer
{
public:
    scope_timer(profiler& p, zone z)
        : prof_(p), zone_(z), start_(profiler::now_ticks())
    {
    }

    ~scope_timer() { prof_.add(zone_, prof_.to_ns(profiler::now_ticks() - start_)); }

    scope_timer(const scope_timer&) = delete;
    scope_timer& operator=(const scope_timer&) = delete;
    scope_timer(scope_timer&&) = delete;
    scope_timer& operator=(scope_timer&&) = delete;

private:
    profiler& prof_;
    zone zone_;
    Uint64 start_;
};

} // namespace engine
