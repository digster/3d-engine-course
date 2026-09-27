// scratch/bench_59.cpp — Lesson 5.9's measurements.
//
//   1  RESOLVE ORDER, full rebuild: recursive vs level-index vs level-packed,
//      at four world sizes x four depths x two row orders.
//   2  DEPTH ALONE, at one size, so the effect can be seen without n moving.
//   3  DIRTY, against the fraction of the world that moved.
//   4  WHAT THE ORDER COSTS TO MAINTAIN: reindex, and the permutation arm C needs.
//
// Every timing obeys the four rules in engine/core/bench.hpp: alternate the arms,
// report median and spread, keep the result alive, and check the arms agreed.
//
// THE FOURTH RULE REPORTS SOMETHING UNEXPECTED HERE, and it is worth knowing why.
// These arms visit the same rows in DIFFERENT ORDERS, so on Lesson 5.6's evidence
// their running checksums should differ in the last bits — floating-point
// addition is not associative. They do not: §1, §2 and §4 all report `agree`,
// exactly. The reason is the widening. Each addend is a `float` (24-bit mantissa)
// accumulated into a `double` (53), and the running sum of 100,000 values of
// magnitude ~1 never exceeds 2^17, so every partial sum needs at most 17 + 24 = 41
// significant bits and NOTHING IS EVER ROUNDED. An exact sum is order-independent.
// Change the accumulator to `float` and the agreement disappears; that is the knob
// which proves the explanation rather than asserting it.
//
// §3 is the honest exception: `DIFFER` there is CORRECT, because the dirty arm is
// supposed to touch fewer rows and therefore sums fewer of them. Its claim is not
// "same checksum" but "same MATRICES", and that is checked element-by-element in
// verify_59, outside any timing loop, which is where an exactness claim belongs.
//
// Build and run:  sh scratch/build_bench_59.sh

#include <engine/core/bench.hpp>

#include <engine/ecs/hierarchy.hpp>

#include "hier_probe.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <vector>

namespace {

constexpr int k_reps = 25;

void heading(const char* title)
{
    std::printf("\n%s\n", title);
    for (const char* p = title; *p != '\0'; ++p) { std::printf("-"); }
    std::printf("\n");
}

/// One row of a ratio table, with the spread of both arms so a reader can tell a
/// finding from a tie.
void row(const char* label, const engine::bench_ab& ab)
{
    std::printf("  %-34s %8.3f %8.3f  %6.2fx   spread %.2f/%.2f  %s\n",
                label, ab.a.median_ns, ab.b.median_ns, ab.ratio(),
                ab.a.spread(), ab.b.spread(),
                ab.agree ? "agree" : "DIFFER");
}

/// How many times a body must repeat before the timer can see it.
///
/// SDL's performance counter ticks at 41.67 ns on this machine, and the course's
/// noise-floor rule wants a measurement to clear 100 ticks — about 4.2 us. A pass
/// over 100 entities is ~1 us, which is BELOW the floor and reads back quantised:
/// the first version of this file reported 10.417 ns/entity for two different
/// arms at n = 100, which is 25 ticks exactly and is the timer talking, not the
/// code. So small worlds are walked repeatedly and the total is divided.
[[nodiscard]] int inner_reps(std::size_t n)
{
    constexpr std::size_t k_min_items = 200000;   // ~1 ms of composes
    return static_cast<int>((k_min_items + n - 1u) / n);
}

// ---------------------------------------------------------------------------
//  1 — resolve order, full rebuild
// ---------------------------------------------------------------------------

void experiment_1()
{
    heading("1. Resolve order, full rebuild  (ns/entity; ratio is B or C relative to A)");
    std::printf("     A = recurse from roots    B = level order via index    "
                "C = level order, rows packed\n");
    std::printf("  %-34s %8s %8s  %7s\n", "", "A ns", "arm ns", "ratio");

    const std::size_t sizes[] = {100, 1000, 10000, 100000};
    const int depths[] = {2, 4, 8, 16};

    for (const bool scramble : {false, true})
    {
        std::printf("\n  --- rows in %s order ---\n",
                    scramble ? "SCRAMBLED (a world that has been running)"
                             : "creation (a world built all at once)");
        for (const int d : depths)
        {
            for (const std::size_t n : sizes)
            {
                hier::world wa = hier::make_forest(n, d, scramble);
                hier::world wb = hier::make_forest(n, d, scramble);
                hier::world wc = hier::make_forest(n, d, scramble);
                hier::permute_to_level_order(wc);

                char label[64];
                const int inner = inner_reps(n);
                const std::size_t items = n * static_cast<std::size_t>(inner);

                const engine::bench_ab ab = engine::bench_compare(
                    items, k_reps,
                    [&wa, inner]() {
                        double v = 0.0;
                        for (int i = 0; i < inner; ++i) { v += hier::resolve_recursive(wa); }
                        return v;
                    },
                    [&wb, inner]() {
                        double v = 0.0;
                        for (int i = 0; i < inner; ++i) { v += hier::resolve_levels(wb); }
                        return v;
                    });
                SDL_snprintf(label, sizeof(label), "depth %2d, n = %6zu   B index", d, n);
                row(label, ab);

                const engine::bench_ab ac = engine::bench_compare(
                    items, k_reps,
                    [&wa, inner]() {
                        double v = 0.0;
                        for (int i = 0; i < inner; ++i) { v += hier::resolve_recursive(wa); }
                        return v;
                    },
                    [&wc, inner]() {
                        double v = 0.0;
                        for (int i = 0; i < inner; ++i) { v += hier::resolve_levels_packed(wc); }
                        return v;
                    });
                SDL_snprintf(label, sizeof(label), "                     C packed");
                row(label, ac);
            }
            std::printf("\n");
        }
    }
}

// ---------------------------------------------------------------------------
//  2 — depth alone
// ---------------------------------------------------------------------------

void experiment_2()
{
    heading("2. Depth alone, n = 100,000, scrambled  (ns/entity)");
    std::printf("     Same entity count every row, so anything that moves is depth.\n");
    std::printf("  %-34s %8s %8s  %7s\n", "", "A ns", "C ns", "ratio");

    for (const int d : {1, 2, 4, 8, 16, 32})
    {
        hier::world wa = hier::make_forest(100000, d, true);
        hier::world wc = hier::make_forest(100000, d, true);
        hier::permute_to_level_order(wc);

        const engine::bench_ab ab = engine::bench_compare(
            100000, k_reps,
            [&wa]() { return hier::resolve_recursive(wa); },
            [&wc]() { return hier::resolve_levels_packed(wc); });

        char label[64];
        SDL_snprintf(label, sizeof(label), "depth %2d  (%zu levels)", d, wa.levels());
        row(label, ab);
    }
}

// ---------------------------------------------------------------------------
//  3 — dirty, against the fraction that moved
// ---------------------------------------------------------------------------

void experiment_3()
{
    heading("3. Dirty subtrees vs a full pass  (ns/entity, n = 100,000, depth 8, scrambled)");
    std::printf("     A = full level pass (arm B)    D = dirty-only\n");
    std::printf("     'reach' is rows D actually touched, INCLUDING descendants of "
                "moved rows.\n");
    std::printf("  %-34s %8s %8s  %7s\n", "", "full ns", "D ns", "ratio");

    for (const double f : {0.001, 0.01, 0.05, 0.10, 0.25, 0.50, 1.00})
    {
        hier::world wf = hier::make_forest(100000, 8, true);
        hier::world wd = hier::make_forest(100000, 8, true);

        std::vector<std::uint8_t> mask;
        hier::mark_dirty(wd, mask, f);
        const std::size_t reach = hier::dirty_reach(wd, mask);

        // The dirty arm consumes its mask (it propagates into it), so each rep
        // must start from the same one. Re-marking inside the arm would time the
        // marking too, and marking is the game's cost, not the resolver's — so
        // the mask is copied instead, and the copy is charged to BOTH arms by
        // giving the full arm the identical copy it does not use.
        std::vector<std::uint8_t> scratch_d = mask;
        std::vector<std::uint8_t> scratch_f = mask;

        const engine::bench_ab ab = engine::bench_compare(
            100000, k_reps,
            [&wf, &scratch_f, &mask]() {
                scratch_f = mask;
                return hier::resolve_levels(wf);
            },
            [&wd, &scratch_d, &mask]() {
                scratch_d = mask;
                return hier::resolve_dirty(wd, scratch_d);
            });

        char label[64];
        SDL_snprintf(label, sizeof(label), "moved %5.1f%%  reach %6.1f%%",
                     f * 100.0, 100.0 * static_cast<double>(reach) / 100000.0);
        row(label, ab);
    }
}

// ---------------------------------------------------------------------------
//  4 — what the order costs to maintain
// ---------------------------------------------------------------------------

void experiment_4()
{
    heading("4. Maintaining the order  (ns/entity — this is the bill arm C does not pay "
            "in §1)");
    std::printf("     reindex  = depth memoisation + counting sort + children CSR\n");
    std::printf("     permute  = reindex, then physically move every row into level order\n");
    std::printf("  %-34s %8s %8s  %7s\n", "", "reidx ns", "perm ns", "ratio");

    for (const std::size_t n : {1000u, 10000u, 100000u})
    {
        hier::world w1 = hier::make_forest(n, 8, true);
        hier::world w2 = hier::make_forest(n, 8, true);

        const int inner = inner_reps(n);
        const std::size_t items = n * static_cast<std::size_t>(inner);

        const engine::bench_ab ab = engine::bench_compare(
            items, k_reps,
            [&w1, inner]() {
                double v = 0.0;
                for (int i = 0; i < inner; ++i) { hier::reindex(w1); v += static_cast<double>(w1.levels()); }
                return v;
            },
            [&w2, inner]() {
                double v = 0.0;
                for (int i = 0; i < inner; ++i) { hier::permute_to_level_order(w2); v += static_cast<double>(w2.levels()); }
                return v;
            });

        char label[64];
        SDL_snprintf(label, sizeof(label), "n = %6zu, depth 8", n);
        row(label, ab);
    }

    std::printf("\n     …and what one resolve costs, for scale:\n");
    for (const std::size_t n : {1000u, 10000u, 100000u})
    {
        hier::world w = hier::make_forest(n, 8, true);
        hier::permute_to_level_order(w);
        const int inner = inner_reps(n);
        const engine::bench_result r = engine::bench_run(
            n * static_cast<std::size_t>(inner), k_reps, [&w, inner]() {
                double v = 0.0;
                for (int i = 0; i < inner; ++i) { v += hier::resolve_levels_packed(w); }
                return v;
            });
        std::printf("  n = %6zu                          %8.3f ns/entity   spread %.2f\n",
                    n, r.median_ns, r.spread());
    }
}

// ---------------------------------------------------------------------------
//  5 — what the container costs on top of the order
// ---------------------------------------------------------------------------
//
// §1-§4 measured a VISIT ORDER over three plain arrays, which is what the four
// candidate designs actually differ in. The shipped resolver walks the same order
// through `registry` and `pool<T>`, so it pays the ECS's own indirection on top:
// three sparse lookups per entity (local, world, parent) instead of three array
// reads. Quoting the probe's numbers for shipped code would be quoting a
// measurement of something else, so this section measures the real thing.

void experiment_5()
{
    heading("5. The shipped resolver vs the probe  (ns/entity, depth 8, scrambled)");
    std::printf("     probe   = hier::resolve_levels  — three ARRAY reads per entity\n");
    std::printf("     engine  = ecs::hierarchy::resolve — three POOL lookups per entity\n");
    std::printf("  %-34s %8s %8s  %7s\n", "", "probe ns", "engine ns", "ratio");

    for (const std::size_t n : {1000u, 10000u, 100000u})
    {
        hier::world probe = hier::make_forest(n, 8, true);

        // The same forest, in a real registry. Entities are created in row order
        // and then linked, so the transform pool's dense order matches the
        // probe's row order — which is what makes the two comparable at all.
        engine::ecs::registry world;
        engine::ecs::hierarchy tree;
        std::vector<engine::ecs::entity> row_entity(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            row_entity[i] = world.create();
            engine::ecs::add_hierarchy_components(world, row_entity[i], probe.local[i]);
        }
        for (std::size_t i = 0; i < n; ++i)
        {
            if (probe.parent_row[i] != hier::k_root)
            {
                world.add<engine::ecs::parent>(
                    row_entity[i], engine::ecs::parent{row_entity[probe.parent_row[i]]});
            }
        }
        const engine::ecs::hierarchy_report r = tree.rebuild(world);

        const int inner = inner_reps(n);
        const std::size_t items = n * static_cast<std::size_t>(inner);

        const engine::bench_ab ab = engine::bench_compare(
            items, k_reps,
            [&probe, inner]() {
                double v = 0.0;
                for (int i = 0; i < inner; ++i) { v += hier::resolve_levels(probe); }
                return v;
            },
            [&world, &tree, inner]() {
                for (int i = 0; i < inner; ++i) { tree.resolve(world); }
                return static_cast<double>(tree.order().size());
            });

        char label[80];
        SDL_snprintf(label, sizeof(label), "n = %6zu  (%zu levels, %zu roots)",
                     n, r.levels, r.roots);
        std::printf("  %-34s %8.3f %8.3f  %6.2fx   spread %.2f/%.2f\n",
                    label, ab.a.median_ns, ab.b.median_ns, ab.ratio(),
                    ab.a.spread(), ab.b.spread());
    }

    std::printf("\n     …and what a rebuild costs on the same worlds:\n");
    for (const std::size_t n : {1000u, 10000u, 100000u})
    {
        hier::world probe = hier::make_forest(n, 8, true);
        engine::ecs::registry world;
        engine::ecs::hierarchy tree;
        std::vector<engine::ecs::entity> row_entity(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            row_entity[i] = world.create();
            engine::ecs::add_hierarchy_components(world, row_entity[i], probe.local[i]);
        }
        for (std::size_t i = 0; i < n; ++i)
        {
            if (probe.parent_row[i] != hier::k_root)
            {
                world.add<engine::ecs::parent>(
                    row_entity[i], engine::ecs::parent{row_entity[probe.parent_row[i]]});
            }
        }

        const int inner = inner_reps(n);
        const engine::bench_result rb = engine::bench_run(
            n * static_cast<std::size_t>(inner), k_reps, [&world, &tree, inner]() {
                double v = 0.0;
                for (int i = 0; i < inner; ++i) { v += static_cast<double>(tree.rebuild(world).levels); }
                return v;
            });
        const engine::bench_result rs = engine::bench_run(
            n * static_cast<std::size_t>(inner), k_reps, [&world, &tree, inner]() {
                for (int i = 0; i < inner; ++i) { tree.resolve(world); }
                return static_cast<double>(tree.order().size());
            });
        std::printf("  n = %6zu   rebuild %7.3f   resolve %7.3f   rebuild costs %.2f resolves\n",
                    n, rb.median_ns, rs.median_ns, rb.median_ns / rs.median_ns);
    }
}

}   // namespace

int main()
{
    std::printf("bench_59 — Lesson 5.9: resolving a transform hierarchy\n");
    std::printf("(release build required for any timing claim)\n");

    experiment_1();
    experiment_2();
    experiment_3();
    experiment_4();
    experiment_5();

    std::printf("\ndone.\n");
    return 0;
}
