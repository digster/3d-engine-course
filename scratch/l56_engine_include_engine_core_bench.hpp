// engine/include/engine/core/bench.hpp — timing two things fairly.
//
// Lesson 5.6. This file exists because the same twenty lines have now been
// written three times — in `measure_53.py`'s probe, in `measure_54.py`'s, and in
// `measure_55.py`'s — each time slightly differently, and Lesson 5.5's rule about
// the search path applies to code as well as to concepts: three copies of one
// idea is the usual sign that the idea has no name.
//
// Its name is A/B TIMING, and the whole of it is four rules that are easy to
// state and easy to get wrong:
//
//   1. ALTERNATE THE ARMS. Not "run A ten thousand times, then B ten thousand
//      times". A machine warms up, throttles, migrates between cores and gets
//      interrupted; every one of those effects lands on whichever arm happens to
//      be running, and running them in blocks hands the entire effect to one of
//      them. Lesson 3.10 discovered this the expensive way — it published a 10%
//      improvement that turned out to be session drift, visible in the data as
//      both columns climbing monotonically as the machine warmed — and the
//      pitfall it wrote ("never compare two numbers taken hours apart") is what
//      this file mechanises.
//   2. REPORT THE MEDIAN, AND THE SPREAD. A single scheduler hiccup moves a mean
//      and cannot move a median. But a median alone hides a bimodal result, so
//      the minimum and maximum come with it: if they straddle the other arm's
//      median, you have not measured a difference, you have measured noise.
//   3. STOP THE OPTIMISER DELETING THE WORK. A loop whose result is unused is a
//      loop a modern compiler is entitled to remove entirely, and a benchmark
//      that measures an empty loop reports beautiful numbers. `bench_keep` is the
//      cheapest portable answer.
//   4. CHECK THAT BOTH ARMS COMPUTED THE SAME ANSWER. This is the rule people
//      skip, and it is the one that makes the rest mean anything: if arm A is
//      faster because it is doing less, the number is not a comparison, it is a
//      bug report. `bench_ab::agree` carries the answer out so a caller can
//      assert on it.
//
// This is a MICROBENCHMARK harness, not a profiler. `engine/core/profile.hpp`
// (Lesson 3.10) is the profiler: it instruments a running frame with named zones
// and reports where a real workload spent its time. This one takes a piece of
// code out of the program, runs it in a loop, and answers "which of these two is
// faster". Both are useful and they answer different questions; reaching for the
// wrong one is how you end up optimising something that was never on the critical
// path.

#pragma once

#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace engine {

// ---- Keeping the optimiser honest ------------------------------------------

/// A `volatile` sink, so that storing to it cannot be optimised away.
///
/// One object across the whole program (`inline` variable, C++17), because it is
/// never read and its value never matters — only the fact that the store must
/// happen.
inline volatile double g_bench_sink = 0.0;

/// Force `v` to be computed. Call it with whatever your loop accumulated.
///
/// The store is to a `volatile`, which the standard requires to happen — so
/// every computation `v` depends on must happen too. That is a guarantee about
/// the abstract machine and it is the strongest portable one available; a
/// compiler doing whole-program optimisation across this header could in
/// principle still see more than we would like, which is why the numbers in
/// Lesson 5.6 are taken without LTO and why that is stated rather than assumed.
inline void bench_keep(double v)
{
    g_bench_sink = v;
}

// ---- What a run reports ----------------------------------------------------

/// The outcome of timing one arm.
///
/// **Per item, not per run**, because the interesting comparisons in this course
/// are between layouts at different N and a total is not comparable across them.
/// `items` is carried so a caller can multiply back.
struct bench_result
{
    double median_ns = 0.0;   ///< the number to quote
    double min_ns = 0.0;      ///< …and the two that say whether to believe it
    double max_ns = 0.0;
    int samples = 0;
    std::size_t items = 0;

    /// How far apart the extremes are, relative to the median. Above ~0.2 the
    /// machine was busy and the run is worth repeating.
    [[nodiscard]] double spread() const
    {
        return median_ns > 0.0 ? (max_ns - min_ns) / median_ns : 0.0;
    }
};

/// Two arms, timed against each other, plus whether they agreed.
struct bench_ab
{
    bench_result a;
    bench_result b;

    /// `true` if both arms produced **bit-identical** accumulated values.
    /// **Check it.** An arm that is faster because it did less has not won a
    /// comparison.
    bool agree = false;

    /// The largest relative difference seen between the arms' answers.
    ///
    /// Exact equality is the right default and it is too strict for one real
    /// case: an arm that visits the same elements in a **different order** sums
    /// the same numbers in a different order, and floating-point addition is not
    /// associative. Lesson 5.6's shuffled-pointer and tree arms are exactly that,
    /// and `agree` correctly reports `false` for them. So the caller gets the
    /// magnitude as well as the verdict and decides which it needs: bit-identical
    /// for two orderings of the same loop, a tolerance for two orders of the same
    /// sum. Anything above ~1e-9 is not rounding, it is different work.
    double max_rel_diff = 0.0;

    /// How much slower `b` is than `a`. Below ~1.05 with any real spread, call it
    /// a tie and say so — a 3% difference between two medians whose ranges
    /// overlap is not a finding.
    [[nodiscard]] double ratio() const
    {
        return a.median_ns > 0.0 ? b.median_ns / a.median_ns : 0.0;
    }
};

// ---- Running ---------------------------------------------------------------

namespace detail {

[[nodiscard]] inline bench_result summarise(std::vector<double>& per_item, std::size_t items)
{
    bench_result r;
    r.samples = static_cast<int>(per_item.size());
    r.items = items;
    if (per_item.empty()) { return r; }

    std::sort(per_item.begin(), per_item.end());
    r.median_ns = per_item[per_item.size() / 2];
    r.min_ns = per_item.front();
    r.max_ns = per_item.back();
    return r;
}

[[nodiscard]] inline double elapsed_ns(Uint64 t0, Uint64 t1)
{
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    return static_cast<double>(t1 - t0) / freq * 1e9;
}

}   // namespace detail

/// Time one arm. `body()` performs one full pass over `items` and returns the
/// value it accumulated.
///
/// A warm-up pass runs first and is discarded: the first execution pays for cold
/// caches, lazy page faults and any one-time work the loop triggers, and none of
/// those are the thing being measured.
template <typename F>
[[nodiscard]] bench_result bench_run(std::size_t items, int reps, F&& body)
{
    bench_keep(body());   // warm-up, discarded

    std::vector<double> per_item;
    per_item.reserve(static_cast<std::size_t>(reps));

    for (int i = 0; i < reps; ++i)
    {
        const Uint64 t0 = SDL_GetPerformanceCounter();
        const double v = body();
        const Uint64 t1 = SDL_GetPerformanceCounter();
        bench_keep(v);
        per_item.push_back(detail::elapsed_ns(t0, t1) / static_cast<double>(items));
    }
    return detail::summarise(per_item, items);
}

/// Time two arms **alternately**, one rep each, `reps` times.
///
/// This is the function the file exists for. Both arms see the same thermal
/// state, the same scheduler, the same neighbours on the machine — because they
/// see them within microseconds of each other rather than seconds apart. Any
/// drift that remains lands on both.
///
/// Each arm returns the value it accumulated, and `agree` reports whether the two
/// matched on every rep. Exact equality, not a tolerance: these are meant to be
/// two ways of arranging the *same* arithmetic, so a difference in the last bit
/// means the arms are not doing the same work and the comparison is void.
template <typename A, typename B>
[[nodiscard]] bench_ab bench_compare(std::size_t items, int reps, A&& arm_a, B&& arm_b)
{
    bench_keep(arm_a());
    bench_keep(arm_b());

    std::vector<double> a_ns;
    std::vector<double> b_ns;
    a_ns.reserve(static_cast<std::size_t>(reps));
    b_ns.reserve(static_cast<std::size_t>(reps));

    bool agree = true;
    double worst_rel = 0.0;

    for (int i = 0; i < reps; ++i)
    {
        const Uint64 t0 = SDL_GetPerformanceCounter();
        const double va = arm_a();
        const Uint64 t1 = SDL_GetPerformanceCounter();
        const double vb = arm_b();
        const Uint64 t2 = SDL_GetPerformanceCounter();

        bench_keep(va);
        bench_keep(vb);
        if (va != vb)
        {
            agree = false;
            const double scale = std::max(std::abs(va), std::abs(vb));
            const double rel = scale > 0.0 ? std::abs(va - vb) / scale : std::abs(va - vb);
            worst_rel = std::max(worst_rel, rel);
        }

        a_ns.push_back(detail::elapsed_ns(t0, t1) / static_cast<double>(items));
        b_ns.push_back(detail::elapsed_ns(t1, t2) / static_cast<double>(items));
    }

    bench_ab out;
    out.a = detail::summarise(a_ns, items);
    out.b = detail::summarise(b_ns, items);
    out.agree = agree;
    out.max_rel_diff = worst_rel;
    return out;
}

}   // namespace engine
