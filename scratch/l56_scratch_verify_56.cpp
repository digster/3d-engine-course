// scratch/verify_56.cpp — Lesson 5.6's harness: the instrument, and the experiment.
//
//   §A  bench_result reports a median, a minimum and a maximum
//   §B  bench_compare alternates, and reports agreement honestly
//   §C  bench_keep stores through a volatile, so the work cannot be deleted
//   §D  ALL SEVEN LAYOUTS DESCRIBE THE SAME SCENE
//   §E  …and the ones that claim to be shuffled actually are
//   §F  the golden is still byte-identical
//
// §D is the section that makes Lesson 5.6's tables mean anything. A benchmark is
// a comparison, and a comparison between two arms that compute different answers
// is not a measurement — it is a bug report with a stopwatch attached. Every
// number the lesson publishes rests on these six checks.
//
// Build and run:  sh scratch/build_verify_56.sh

#include <engine/core/bench.hpp>
#include <engine/gfx/scene.hpp>

#include "scene_layouts.hpp"
#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failures; }
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
}

void checkf(bool ok, const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    SDL_vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    check(ok, line);
}

[[nodiscard]] std::string read_file(const char* path)
{
    std::size_t size = 0;
    void* data = SDL_LoadFile(path, &size);
    if (data == nullptr) { return {}; }
    std::string s(static_cast<const char*>(data), size);
    SDL_free(data);
    return s;
}

/// Burn a controllable amount of time without being optimised away.
[[nodiscard]] double spin(int units)
{
    double acc = 0.0;
    for (int i = 0; i < units * 1000; ++i) { acc += static_cast<double>(i) * 1.000001; }
    return acc;
}

// ---------------------------------------------------------------------------

void section_a_summary()
{
    std::printf("\n=== A. What a run reports ===\n");

    const engine::bench_result r = engine::bench_run(100, 11, [] { return spin(20); });

    checkf(r.samples == 11, "%d samples, as asked", r.samples);
    checkf(r.items == 100, "items carried through: %zu", r.items);
    check(r.median_ns > 0.0, "the median is a positive time, not a zero from a coarse timer");
    check(r.min_ns <= r.median_ns && r.median_ns <= r.max_ns,
          "min <= median <= max, which is the whole point of reporting three numbers");
    checkf(r.spread() >= 0.0 && r.spread() < 5.0,
           "spread is %.3f — above ~0.2 the machine was busy and the run wants repeating",
           r.spread());

    // Per ITEM, not per run: the interesting comparisons are across n and a total
    // is not comparable between them.
    const engine::bench_result ten = engine::bench_run(1000, 11, [] { return spin(20); });
    checkf(ten.median_ns < r.median_ns,
           "the same work over 10x the items is ~10x cheaper per item (%.4f vs %.4f)",
           ten.median_ns, r.median_ns);
}

void section_b_compare()
{
    std::printf("\n=== B. Comparing two arms ===\n");

    // Identical work: the ratio should be near 1 and the answers identical.
    const engine::bench_ab same = engine::bench_compare(
        100, 15, [] { return spin(20); }, [] { return spin(20); });
    check(same.agree, "two arms doing identical work agree EXACTLY");
    checkf(same.max_rel_diff == 0.0, "…with a relative difference of %.1e",
           same.max_rel_diff);
    checkf(same.ratio() > 0.7 && same.ratio() < 1.4,
           "…and land within 40%% of each other: ratio %.2f", same.ratio());

    // Different work: caught.
    const engine::bench_ab diff = engine::bench_compare(
        100, 5, [] { return spin(20); }, [] { return spin(20) + 1.0; });
    check(!diff.agree, "an arm computing something DIFFERENT is caught");
    checkf(diff.max_rel_diff > 0.0, "…and the size of the difference is reported: %.1e",
           diff.max_rel_diff);

    // Four times the work: the ratio should see it. This is the check that the
    // timing is measuring the arms rather than the harness.
    const engine::bench_ab four = engine::bench_compare(
        100, 15, [] { return spin(20); }, [] { return spin(80); });
    checkf(four.ratio() > 2.5, "an arm doing 4x the work reads %.2fx slower", four.ratio());
}

void section_c_keep()
{
    std::printf("\n=== C. Keeping the optimiser honest ===\n");

    engine::bench_keep(1234.5);
    checkf(engine::g_bench_sink == 1234.5,
           "bench_keep stores through the volatile sink (read back %.1f)",
           static_cast<double>(engine::g_bench_sink));

    // The real property — that a loop whose result is only passed to bench_keep
    // is not deleted — cannot be asserted from inside the language. What CAN be
    // asserted is that it still takes time, which is the observable consequence.
    const engine::bench_result r = engine::bench_run(1, 9, [] { return spin(50); });
    check(r.median_ns > 1000.0,
          "a 50,000-iteration loop still costs microseconds — it was not deleted");
}

void section_e_same_scene()
{
    std::printf("\n=== E. All seven layouts describe the SAME scene ===\n");

    constexpr std::size_t n = 2000;
    const layouts::flat_scene flat(n);
    const layouts::pointer_scene ptrs(n);
    const layouts::soa_scene soa(n);
    const layouts::virtual_scene virt(n);
    const layouts::tree_scene tree(n);

    constexpr double bias = 0.125;
    const double reference = flat.full(bias);

    checkf(reference != 0.0, "the flat baseline computed something: %.6f", reference);

    // Same order of visitation => bit-identical. Anything else would mean the
    // arms are not doing the same arithmetic.
    check(layouts::pointer_scene::full(ptrs.ordered, bias) == reference,
          "ordered pointers: bit-identical");
    check(soa.full(bias) == reference, "SoA: bit-identical");
    check(layouts::virtual_scene::full(virt.mono, bias) == reference,
          "virtual (monomorphic): bit-identical");
    check(layouts::virtual_scene::full(virt.poly, bias) == reference,
          "virtual (polymorphic): bit-identical");

    // Different order of visitation => the same numbers summed differently.
    // Floating-point addition is not associative, so exact equality is the WRONG
    // test here and a tolerance is the right one — which is why `bench_ab` carries
    // both the verdict and the magnitude.
    const auto close = [&](double v) {
        return std::abs(v - reference) / std::abs(reference) < 1e-9;
    };
    const double shuffled = layouts::pointer_scene::full(ptrs.shuffled, bias);
    const double fresh = tree.full(bias);
    const double stirred = tree.full_shuffled(bias);

    checkf(close(shuffled), "shuffled pointers: same scene, %.1e apart (reordered sum)",
           std::abs(shuffled - reference) / std::abs(reference));
    checkf(close(fresh), "tree in allocation order: same scene, %.1e apart",
           std::abs(fresh - reference) / std::abs(reference));
    checkf(close(stirred), "tree shuffled: same scene, %.1e apart",
           std::abs(stirred - reference) / std::abs(reference));
    // NOT "at least one of them differs" — the first version asserted that and it
    // failed, because at n = 2000 with this data all three reorderings happened to
    // land on the same bits. Whether a reordering differs is a property of the
    // NUMBERS, not of the layout, so asserting it is asserting an accident.
    //
    // What IS worth pinning is the underlying fact the tolerance exists for, and
    // it can be demonstrated in four lines with no layouts at all.
    {
        const double big = 1.0e16;
        const double forward = (big + 1.0) - big;   // 1.0 is lost, then subtracted
        const double backward = (big - big) + 1.0;  // …and here it is not
        checkf(forward != backward,
               "floating-point addition is not associative: (a+b)-a is %.1f and "
               "(a-a)+b is %.1f — which is why a reordering arm needs a tolerance "
               "and not an equality", forward, backward);
    }

    // Workload 2 counts, and a count is an integer, so this one must be exact.
    const double a = layouts::cull_flat(flat.objects, 0.0);
    const double b = layouts::cull_soa(soa.position, 0.0);
    checkf(a == b && a > 0.0 && a < static_cast<double>(n),
           "the cull arms agree exactly: %.0f of %zu objects inside the sphere",
           a, n);
}

/// MUST RUN BEFORE §E, AND THE REASON IS THE FINDING.
///
/// This section asks where the pointer layout's objects landed, and the answer
/// depends on the allocator's state — not only on the code. Run it after §E has
/// built and destroyed two 2,000-object scenes and the 96-byte free list is full,
/// so 500 fresh objects come back recycled and CONTIGUOUS: the first version of
/// this harness ran in that order and reported 497 of 499 adjacent, which looked
/// like the spacers failing and was actually the heap being warm.
///
/// The benchmark does not have this problem — it reports its own layout at every
/// n, in its own process (`bench_56.cpp`'s `alloc` rows, 0 adjacent at every
/// size) — but a check that depends on allocation history has to say so.
void section_d_shapes()
{
    std::printf("\n=== D. The layouts are what they claim to be ===\n");

    constexpr std::size_t n = 500;
    const layouts::flat_scene flat(n);
    const layouts::pointer_scene ptrs(n);
    const layouts::soa_scene soa(n);
    const layouts::tree_scene tree(n);

    checkf(sizeof(engine::scene_object) == 96, "scene_object is %zu bytes (1.5 cache lines)",
           sizeof(engine::scene_object));
    checkf(sizeof(engine::transform) == 60,
           "transform is %zu of them — everything the full workload reads",
           sizeof(engine::transform));

    check(flat.objects.size() == n, "the flat array holds n objects");
    check(soa.position.size() == n && soa.rotation.size() == n && soa.scale.size() == n,
          "the SoA arrays are parallel and all n long");
    check(ptrs.ordered.size() == n && ptrs.shuffled.size() == n,
          "both pointer arrays hold n pointers");

    // The flat array really is contiguous with a 96-byte stride.
    const auto stride = reinterpret_cast<const char*>(&flat.objects[1])
                      - reinterpret_cast<const char*>(&flat.objects[0]);
    checkf(stride == static_cast<std::ptrdiff_t>(sizeof(engine::scene_object)),
           "the flat array strides by exactly sizeof(scene_object): %td", stride);

    // …and the shuffled arm really is shuffled. Counting how many entries sit in
    // their original position: a random permutation of 500 leaves about one.
    int in_place = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        if (ptrs.shuffled[i] == ptrs.ordered[i]) { ++in_place; }
    }
    checkf(in_place < 10, "%d of %zu pointers are still in their original slot — it is "
                          "genuinely shuffled, not nearly sorted", in_place, n);

    // ---- where the objects actually landed ---------------------------------
    //
    // The spacers exist to stop "pointer chasing" from quietly measuring a
    // contiguous walk with an extra load. WHETHER THEY WORK IS NOT SOMETHING THIS
    // CODE DECIDES, and chasing that turned into the most useful thing this
    // harness found.
    //
    // Measured, in three different processes, all with the same layout code:
    //
    //     bench_56, every n from 4 to 100,000     0 adjacent
    //     a standalone probe, n = 100,000         0 adjacent
    //     a standalone probe, n = 500           485 of 499 adjacent
    //     THIS harness, n = 500 and 100,000     486 and 99,410 adjacent
    //
    // Same code, four answers. macOS's allocator hands same-size blocks out of a
    // run and interleaves — or does not — depending on the size class's magazine
    // state, which depends on everything the process allocated earlier. So
    // adjacency is a property of the PROCESS, not of the layout, and a harness
    // cannot assert another program's heap state.
    //
    // The resolution: `bench_56` REPORTS ITS OWN LAYOUT at every n (its `alloc`
    // rows, 0 adjacent throughout), so the published ratios carry their own
    // provenance. What is asserted here is the part this code does control.
    int adjacent = 0;
    for (std::size_t i = 1; i < n; ++i)
    {
        const auto gap = reinterpret_cast<const char*>(ptrs.ordered[i])
                       - reinterpret_cast<const char*>(ptrs.ordered[i - 1]);
        if (gap == static_cast<std::ptrdiff_t>(sizeof(engine::scene_object))) { ++adjacent; }
    }
    std::printf("        (n=%zu: %d of %zu consecutive objects 96 B apart — a property "
                "of this process's heap, reported not asserted)\n",
                n, adjacent, n - 1);

    checkf(ptrs.spacers.size() == 3 * n,
           "%zu spacers for %zu objects — three same-size-class blocks each, which "
           "is the part the layout controls", ptrs.spacers.size(), n);

    // The two tree chains cover the same nodes.
    int fresh_count = 0;
    for (const layouts::tree_node* c = tree.root->first_child; c != nullptr;
         c = c->next_sibling) { ++fresh_count; }
    int stirred_count = 0;
    for (const layouts::tree_node* c = tree.shuffled_root->alt_child; c != nullptr;
         c = c->alt_sibling) { ++stirred_count; }
    checkf(fresh_count == stirred_count && fresh_count == static_cast<int>(n) - 1,
           "both tree chains visit the same %d children — they differ in ORDER and "
           "in nothing else", fresh_count);
}

void section_f_golden()
{
    std::printf("\n=== F. The golden ===\n");

    const char* path = "build/demos/verify_56.ppm";
    check(demo::write_reference_shot(path) == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file(path);
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(), "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL to the golden written in Lesson 5.1 — six lessons now, and "
          "this one changed no rendering code at all");
}

}   // namespace

int main()
{
    std::printf("verify_56 — Lesson 5.6: data-oriented design\n");

    section_a_summary();
    section_b_compare();
    section_c_keep();
    // Shapes BEFORE same-scene: §D measures allocation addresses and §E churns
    // several thousand allocations. See the note on section_d_shapes.
    section_d_shapes();
    section_e_same_scene();
    section_f_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
