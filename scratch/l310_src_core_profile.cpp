// src/core/profile.cpp — the frame budget.

#include "core/profile.hpp"

#include <algorithm>

namespace engine {

const char* zone_name(zone z)
{
    switch (z)
    {
    case zone::build:    return "build";
    case zone::collect:  return "collect";
    case zone::sort:     return "sort";
    case zone::fill:     return "fill";
    case zone::overlay:  return "overlay";
    case zone::present:  return "present";
    case zone::count:    break;
    }
    return "?";
}

profiler::profiler()
{
    // SDL_GetPerformanceFrequency() is how many counter ticks make a second on
    // THIS machine, and it is not a constant across machines: it is 1e9 on macOS
    // and modern Linux, 1e7 on Windows, and has historically been the raw TSC
    // rate. Hard-coding any of those would make a profiler that reports plausible
    // wrong numbers on someone else's computer — the worst kind.
    freq_ = SDL_GetPerformanceFrequency();
    if (freq_ == 0) { freq_ = 1; }   // cannot happen; makes the divide below total
    resolution_ns_ = 1.0e9 / static_cast<double>(freq_);

    // ---- Calibrate the instrument against itself ---------------------------
    //
    // Time an empty scope_timer, many times, and divide. This is the number that
    // decides what may legitimately be instrumented: a zone costing 50 ns
    // measured with a 40 ns instrument is not a measurement, it is a coin toss.
    //
    // Note the shape, because every microbenchmark in this course has it: time a
    // BATCH and divide, never one iteration. One iteration is shorter than the
    // clock's own granularity (`resolution_ns_`), so timing it measures the
    // clock. Lesson 3.10 §2.2.
    constexpr int k_calibration_reps = 100000;
    const Uint64 t0 = now_ticks();
    for (int i = 0; i < k_calibration_reps; ++i)
    {
        const scope_timer t{*this, zone::build};
    }
    const Uint64 t1 = now_ticks();
    overhead_ns_ = static_cast<double>(to_ns(t1 - t0)) / k_calibration_reps;

    // The calibration ran real `add` calls, so `current_` is now full of
    // measurement noise. Clear it: a profiler must not report its own setup.
    current_.fill(0);
}

Uint64 profiler::to_ns(Uint64 ticks) const
{
    // Multiply first, then divide. The other order — `ticks / freq_ * 1e9` —
    // truncates to whole seconds before scaling, so every interval shorter than a
    // second reports as zero. It is the single most common way to write this
    // wrong, and it fails silently and completely.
    //
    // Overflow: this is safe for intervals up to about 18 seconds when the
    // counter runs at 1 GHz (2^64 / 1e9 ticks), which is far longer than any
    // frame that is still worth profiling.
    return (ticks * 1000000000ull) / freq_;
}

void profiler::begin_frame()
{
    current_.fill(0);
    overlapped_ = false;
    frame_start_ = now_ticks();
}

void profiler::add(zone z, Uint64 ns)
{
    current_[static_cast<std::size_t>(index(z))] += ns;
}

void profiler::end_frame()
{
    frame_ns_ = to_ns(now_ticks() - frame_start_);

    Uint64 sum = 0;
    for (int i = 0; i < k_zone_count; ++i)
    {
        history_[static_cast<std::size_t>(i)][static_cast<std::size_t>(write_)] =
            current_[static_cast<std::size_t>(i)];
        sum += current_[static_cast<std::size_t>(i)];
    }
    overlapped_ = sum > frame_ns_;

    frame_history_[static_cast<std::size_t>(write_)] = frame_ns_;
    write_ = (write_ + 1) % k_history;
    filled_ = std::min(filled_ + 1, k_history);
    ++frames_;
}

namespace {

/// The median of the first `n` entries of a ring, without disturbing the ring.
///
/// Copies into a scratch array and uses `std::nth_element`, which is a partial
/// sort: it puts the k-th element where it belongs and leaves everything else
/// merely partitioned around it. That is O(n) rather than the O(n log n) a full
/// sort would cost, and the ordering of the rest is information we would throw
/// away anyway.
[[nodiscard]] Uint64 median_of(const std::array<Uint64, profiler::k_history>& ring, int n)
{
    if (n <= 0) { return 0; }

    std::array<Uint64, profiler::k_history> scratch{};
    std::copy(ring.begin(), ring.begin() + n, scratch.begin());

    const auto mid = scratch.begin() + n / 2;
    std::nth_element(scratch.begin(), mid, scratch.begin() + n);
    return *mid;
}

} // namespace

Uint64 profiler::median_ns(zone z) const
{
    return median_of(history_[static_cast<std::size_t>(index(z))], filled_);
}

Uint64 profiler::median_frame_ns() const
{
    return median_of(frame_history_, filled_);
}

Uint64 profiler::other_ns() const
{
    // Medians, not this frame's values, so the remainder is drawn on the same
    // footing as the bars beside it. Note that a sum of medians is NOT the median
    // of the sum — they differ whenever the zones peak on different frames — so
    // this can disagree slightly with `median_frame_ns()` minus the parts. The
    // disagreement is small and honest; pretending otherwise would mean deriving
    // one of the numbers from the others instead of measuring it.
    Uint64 sum = 0;
    for (int i = 0; i < k_zone_count; ++i)
    {
        sum += median_ns(static_cast<zone>(i));
    }
    const Uint64 total = median_frame_ns();
    return (total > sum) ? (total - sum) : 0;
}

} // namespace engine
