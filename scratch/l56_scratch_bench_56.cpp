// scratch/bench_56.cpp — Lesson 5.6's experiment.
//
// Six ways to store a scene and two ways to walk it, timed against each other on
// this engine's own `transform` and `parent_from_local`.
//
//   WORKLOAD 1  FULL TRANSFORM — read position, rotation and scale (60 of the
//               96 bytes) and build a model matrix. What collect_triangles does.
//   WORKLOAD 2  PARTIAL — read only the position (12 of 96) and test it against
//               a sphere. What frustum culling does.
//
// Two workloads, because the interesting question is not "is contiguous faster"
// (it is) but "how much of the struct do you have to touch before the layout
// stops mattering", and one workload cannot answer that.
//
// Output is machine-readable: one line per (workload, layout, n).
//
//   row <workload> <layout> <n> <median_ns> <min_ns> <max_ns> <agree>

#include <engine/core/bench.hpp>
#include "scene_layouts.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>

namespace {

constexpr int k_reps = 25;

/// How many passes over the scene one timed sample should contain.
///
/// **The first run of this benchmark reported 0.0000 ns/item at n = 4**, which is
/// not a fast layout, it is a timer. `SDL_GetPerformanceFrequency()` is 24 MHz on
/// this machine, so one tick is 41.67 ns and a pass over four objects finishes
/// inside one — every sample read either 0 or 1 ticks. Repeating the pass until a
/// sample is worth roughly a hundred microseconds puts the measurement three
/// orders of magnitude above the granularity, and the per-item figure divides it
/// back out.
[[nodiscard]] int inner_passes(std::size_t n)
{
    constexpr std::size_t k_target_items = 200000;
    return static_cast<int>(std::max<std::size_t>(1u, k_target_items / std::max<std::size_t>(n, 1u)));
}

// ---------------------------------------------------------------------------

void row(const char* workload, const char* layout, std::size_t n,
         const engine::bench_result& r, const engine::bench_ab& ab)
{
    std::printf("row %s %s %zu %.4f %.4f %.4f %d %.3e\n", workload, layout, n,
                r.median_ns, r.min_ns, r.max_ns, static_cast<int>(ab.agree),
                ab.max_rel_diff);
}

/// Report where the pointer arm's objects actually landed.
///
/// **Printed alongside the timings, in the same process, at the same n**, because
/// a benchmark that claims to measure scattered access has to show that its
/// objects are scattered — and cannot assume it, because the answer depends on the
/// allocator AND on what the process allocated earlier. Measured here rather than
/// asserted somewhere else, so every published ratio carries its own provenance.
void report_layout(std::size_t n, const layouts::pointer_scene& p)
{
    const char* lo = reinterpret_cast<const char*>(p.ordered[0]);
    const char* hi = lo;
    long adjacent = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const char* a = reinterpret_cast<const char*>(p.ordered[i]);
        if (a < lo) { lo = a; }
        if (a > hi) { hi = a; }
        if (i > 0 && a - reinterpret_cast<const char*>(p.ordered[i - 1])
                     == static_cast<std::ptrdiff_t>(sizeof(engine::scene_object)))
        {
            ++adjacent;
        }
    }
    std::printf("alloc %zu %ld %.3f\n", n, adjacent,
                static_cast<double>(hi - lo) / 1048576.0);
}

void run(std::size_t n)
{
    const layouts::flat_scene flat(n);
    const layouts::pointer_scene ptrs(n);
    report_layout(n, ptrs);
    const layouts::soa_scene soa(n);
    const layouts::virtual_scene virt(n);
    const layouts::tree_scene tree(n);

    const int inner = inner_passes(n);
    const std::size_t items = n * static_cast<std::size_t>(inner);

    // A pass repeated `inner` times inside one timed sample. The accumulation is
    // summed across passes so both arms still produce comparable values.
    const auto many = [inner](auto&& pass) {
        return [inner, pass]() {
            double acc = 0.0;
            for (int i = 0; i < inner; ++i)
            {
                acc += pass(static_cast<double>(i) * 1e-3);
            }
            return acc;
        };
    };

    // EVERY ARM IS PAIRED AGAINST ITS OWN FLAT BASELINE, alternately, and the
    // number to read is the RATIO within a pairing rather than either absolute.
    //
    // That is not fastidiousness, it is a real effect this benchmark found: at
    // n = 10,000 the flat scene is 960 KB, and whichever arm runs alongside it
    // evicts it. So `flat`'s own median moved between 0.98 and 1.83 ns/item
    // depending on who it was measured against — a 1.9x swing in an arm that did
    // not change. Alternation protects against thermal drift and INTRODUCES cache
    // interference; the ratio within one pairing is the quantity both arms paid
    // the same price for.
    const auto vs = [&](const char* name, auto&& other) {
        const engine::bench_ab ab = engine::bench_compare(
            items, k_reps, many([&](double b) { return flat.full(b); }), many(other));
        row("full", "flat", n, ab.a, ab);
        row("full", name, n, ab.b, ab);
    };

    vs("ptr_ordered", [&](double b) { return layouts::pointer_scene::full(ptrs.ordered, b); });
    vs("ptr_shuffled", [&](double b) { return layouts::pointer_scene::full(ptrs.shuffled, b); });
    vs("soa", [&](double b) { return soa.full(b); });
    vs("virtual_mono", [&](double b) { return layouts::virtual_scene::full(virt.mono, b); });
    vs("virtual_poly", [&](double b) { return layouts::virtual_scene::full(virt.poly, b); });
    vs("tree_fresh", [&](double b) { return tree.full(b); });
    vs("tree_shuffled", [&](double b) { return tree.full_shuffled(b); });

    // Workload 2: the same scene, touching 12 bytes per object instead of 60.
    const engine::bench_ab cull = engine::bench_compare(
        items, k_reps,
        many([&](double b) { return layouts::cull_flat(flat.objects, b); }),
        many([&](double b) { return layouts::cull_soa(soa.position, b); }));
    row("cull", "flat", n, cull.a, cull);
    row("cull", "soa", n, cull.b, cull);
}

}   // namespace

int main()
{
    std::printf("sizeof scene_object %zu\n", sizeof(engine::scene_object));
    std::printf("sizeof transform %zu\n", sizeof(engine::transform));
    std::printf("sizeof vec3 %zu\n", sizeof(engine::vec3));
    std::printf("sizeof node_ptr %zu\n", sizeof(void*));

    for (const std::size_t n : {std::size_t{4}, std::size_t{100}, std::size_t{1000},
                                std::size_t{10000}, std::size_t{100000}})
    {
        run(n);
    }
    return 0;
}
