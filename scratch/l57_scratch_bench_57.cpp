// scratch/bench_57.cpp — Lesson 5.7's experiment.
//
// The design question is archetype vs sparse set, and it is decidable BEFORE
// either is built, because the two differ in exactly two operations:
//
//   QUERY              archetype: K parallel dense arrays, one shared index.
//                      sparse set: one dense walk plus K-1 redirects.
//   STRUCTURAL CHANGE  archetype: move every column of the entity to another
//                      chunk. sparse set: push_back on one pool.
//
// Four measurements, in the order the lesson uses them:
//
//   1  QUERY       K = 1..4, two body costs, two sparse alignments, five sizes.
//   2  CHURN       add + remove a component on 1% of entities per frame, at two
//                  entity widths, because the archetype's cost scales with width
//                  and the sparse set's does not.
//   3  FRAGMENT    the archetype's own downside: the same query with the matching
//                  entities split across 1..4096 archetypes.
//   4  SELECT      a query that matches a quarter of the world — the case the
//                  archetype is supposed to win outright, and the case a sparse
//                  set answers by leading with the smallest pool.
//
// Output is machine-readable, one line per (measurement, arm, parameters):
//
//   q   <body> <arm> <K> <n> <median> <min> <max> <agree> <rel>
//   s   <width> <arm> <n> ...
//   f   <chunks> <n> ...
//   sel <body> <arm> <n> ...
//   mem <n> <arch_bytes> <sparse_bytes> <sparse_index_bytes>

#include <engine/core/bench.hpp>
#include "ecs_probe.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace {

constexpr int k_reps = 25;

/// 5.6's answer to a timer coarser than the work: repeat the pass until a sample
/// is worth roughly a hundred microseconds, then divide back out.
/// `SDL_GetPerformanceFrequency()` is 24 MHz here, so one tick is 41.67 ns and a
/// pass over four entities finishes inside one.
[[nodiscard]] int inner_passes(std::size_t n)
{
    constexpr std::size_t k_target_items = 200000;
    return static_cast<int>(std::max<std::size_t>(1u, k_target_items / std::max<std::size_t>(n, 1u)));
}

void row(const char* kind, const char* a, const char* b, std::size_t n,
         const engine::bench_result& r, const engine::bench_ab& ab)
{
    std::printf("%s %s %s %zu %.4f %.4f %.4f %d %.3e\n", kind, a, b, n,
                r.median_ns, r.min_ns, r.max_ns, static_cast<int>(ab.agree),
                ab.max_rel_diff);
}

/// Wrap a single pass so one timed sample contains `inner` of them, each with a
/// different `bias` so the pass is not a pure function of nothing — 5.6's second
/// lie, where the compiler hoisted an entire loop out and reported 0.166 ns for
/// four matrix builds.
template <typename F>
[[nodiscard]] auto many(int inner, F pass)
{
    return [inner, pass]() {
        double acc = 0.0;
        for (int i = 0; i < inner; ++i) { acc += pass(static_cast<double>(i) * 1e-3); }
        return acc;
    };
}

// ===========================================================================
//  1 — QUERY
// ===========================================================================

template <int K, bool Real>
void query_at(std::size_t n, const ecs_probe::archetype_world& arch,
              const ecs_probe::sparse_world& aligned,
              const ecs_probe::sparse_world& scrambled)
{
    const int inner = inner_passes(n);
    const std::size_t items = n * static_cast<std::size_t>(inner);
    const char* body = Real ? "real" : "cheap";

    // EVERY ARM IS PAIRED AGAINST ITS OWN ARCHETYPE BASELINE, alternately, and
    // the number to read is the RATIO within a pairing. 5.6 found that the
    // baseline's own absolute moved by 1.9x depending on which arm shared the
    // cache with it; only the ratio is a quantity both arms paid for equally.
    const auto vs = [&](const char* name, const ecs_probe::sparse_world& w) {
        const engine::bench_ab ab = engine::bench_compare(
            items, k_reps,
            many(inner, [&](double b) { return ecs_probe::archetype_query<K, Real>(arch, b); }),
            many(inner, [&](double b) { return ecs_probe::sparse_query<K, Real>(w, b); }));
        std::printf("q %s arch %d %zu %.4f %.4f %.4f %d %.3e\n", body, K, n,
                    ab.a.median_ns, ab.a.min_ns, ab.a.max_ns,
                    static_cast<int>(ab.agree), ab.max_rel_diff);
        std::printf("q %s %s %d %zu %.4f %.4f %.4f %d %.3e\n", body, name, K, n,
                    ab.b.median_ns, ab.b.min_ns, ab.b.max_ns,
                    static_cast<int>(ab.agree), ab.max_rel_diff);
    };

    vs("aligned", aligned);
    vs("scrambled", scrambled);
}

// ===========================================================================
//  2 — STRUCTURAL CHANGE
// ===========================================================================
//
// One "frame" removes the component from 1% of entities and adds it straight
// back, so the world is in its starting state at the end of every frame and
// repeated frames are identical work. The set rotates, so no frame repeats
// another's addresses.

void churn_at(std::size_t n, std::size_t extras, const char* width)
{
    const std::size_t count = ecs_probe::churn_set(n, 0).size();
    const int inner = std::max(1, inner_passes(n) / 8);
    const std::size_t ops = 2u * count * static_cast<std::size_t>(inner);

    ecs_probe::archetype_churn arch(n, extras);
    ecs_probe::sparse_churn sparse(n, extras);

    const auto arch_arm = [&]() {
        double acc = 0.0;
        for (int f = 0; f < inner; ++f)
        {
            const std::vector<ecs_probe::entity> set = ecs_probe::churn_set(n, static_cast<std::size_t>(f));
            for (const ecs_probe::entity e : set) { arch.remove_material(e); }
            for (const ecs_probe::entity e : set) { arch.add_material(e); }
            // Read one field back, so "the component is there and holds the right
            // value" is part of what both arms are timed doing. ~1% of the work.
            for (const ecs_probe::entity e : set)
            {
                acc += static_cast<double>(arch.with.mat[arch.row[e]].roughness);
            }
        }
        return acc;
    };

    const auto sparse_arm = [&]() {
        double acc = 0.0;
        for (int f = 0; f < inner; ++f)
        {
            const std::vector<ecs_probe::entity> set = ecs_probe::churn_set(n, static_cast<std::size_t>(f));
            for (const ecs_probe::entity e : set) { sparse.remove_material(e); }
            for (const ecs_probe::entity e : set) { sparse.add_material(e); }
            for (const ecs_probe::entity e : set)
            {
                acc += static_cast<double>(sparse.w.mat.get(e).roughness);
            }
        }
        return acc;
    };

    const engine::bench_ab ab = engine::bench_compare(ops, k_reps, arch_arm, sparse_arm);
    row("s", width, "arch", n, ab.a, ab);
    row("s", width, "sparse", n, ab.b, ab);
}

// ===========================================================================
//  3 — ARCHETYPE FRAGMENTATION
// ===========================================================================

void fragment_at(std::size_t n, std::size_t n_chunks,
                 const ecs_probe::archetype_world& one)
{
    if (n_chunks > n) { return; }
    const ecs_probe::archetype_world many_chunks(n, n_chunks);
    const int inner = inner_passes(n);
    const std::size_t items = n * static_cast<std::size_t>(inner);

    const engine::bench_ab ab = engine::bench_compare(
        items, k_reps,
        many(inner, [&](double b) { return ecs_probe::archetype_query<4, true>(one, b); }),
        many(inner, [&](double b) { return ecs_probe::archetype_query<4, true>(many_chunks, b); }));

    char label[32];
    std::snprintf(label, sizeof label, "%zu", n_chunks);
    row("f", "1", "-", n, ab.a, ab);
    row("f", label, "-", n, ab.b, ab);
}

// ===========================================================================
//  4 — SELECTIVITY
// ===========================================================================

template <bool Real>
void select_at(std::size_t n)
{
    constexpr std::size_t k_num = 1;
    constexpr std::size_t k_den = 4;
    const std::size_t matching = n / k_den;
    if (matching == 0) { return; }

    const ecs_probe::archetype_world arch = ecs_probe::make_selective_archetypes(n, k_num, k_den);
    const ecs_probe::sparse_world aligned = ecs_probe::make_selective_sparse(n, k_num, k_den, false);
    const ecs_probe::sparse_world scrambled = ecs_probe::make_selective_sparse(n, k_num, k_den, true);
    const ecs_probe::sparse_world grouped = ecs_probe::make_grouped_sparse(n, k_num, k_den);

    const int inner = inner_passes(matching);
    const std::size_t items = matching * static_cast<std::size_t>(inner);
    const char* body = Real ? "real" : "cheap";

    const auto vs = [&](const char* name, auto&& arm) {
        const engine::bench_ab ab = engine::bench_compare(
            items, k_reps,
            many(inner, [&](double b) { return ecs_probe::archetype_query<2, Real>(arch, b); }),
            many(inner, arm));
        std::printf("sel %s arch %zu %.4f %.4f %.4f %d %.3e\n", body, n,
                    ab.a.median_ns, ab.a.min_ns, ab.a.max_ns,
                    static_cast<int>(ab.agree), ab.max_rel_diff);
        std::printf("sel %s %s %zu %.4f %.4f %.4f %d %.3e\n", body, name, n,
                    ab.b.median_ns, ab.b.min_ns, ab.b.max_ns,
                    static_cast<int>(ab.agree), ab.max_rel_diff);
    };

    vs("lead_small", [&](double b) { return ecs_probe::sparse_query_lead_vel<Real>(aligned, b); });
    vs("lead_small_scr", [&](double b) { return ecs_probe::sparse_query_lead_vel<Real>(scrambled, b); });
    vs("lead_big", [&](double b) { return ecs_probe::sparse_query_lead_xf<Real>(aligned, b); });
    vs("grouped", [&](double b) { return ecs_probe::sparse_query_grouped<Real>(grouped, b); });
}

// ===========================================================================
//  DISPATCH
// ===========================================================================

void query_all(std::size_t n, const ecs_probe::archetype_world& arch,
               const ecs_probe::sparse_world& aligned,
               const ecs_probe::sparse_world& scrambled)
{
    query_at<1, false>(n, arch, aligned, scrambled);
    query_at<2, false>(n, arch, aligned, scrambled);
    query_at<3, false>(n, arch, aligned, scrambled);
    query_at<4, false>(n, arch, aligned, scrambled);
    query_at<1, true>(n, arch, aligned, scrambled);
    query_at<2, true>(n, arch, aligned, scrambled);
    query_at<3, true>(n, arch, aligned, scrambled);
    query_at<4, true>(n, arch, aligned, scrambled);
}

}   // namespace

int main()
{
    std::printf("sizeof transform %zu\n", sizeof(engine::transform));
    std::printf("sizeof velocity %zu\n", sizeof(ecs_probe::velocity));
    std::printf("sizeof bounds %zu\n", sizeof(ecs_probe::bounds));
    std::printf("sizeof material %zu\n", sizeof(ecs_probe::material));
    std::printf("sizeof pad16 %zu\n", sizeof(ecs_probe::pad16));
    std::printf("sizeof entity %zu\n", sizeof(ecs_probe::entity));

    const std::size_t sizes[] = {4u, 100u, 1000u, 10000u, 100000u};

    // MEMORY, counted rather than timed. The archetype pays one entity id per
    // row; the sparse set pays, per component type, one dense id AND one sparse
    // slot per ENTITY IN THE WORLD — present or not. That second term is the
    // sparse set's structural cost and it is the one nobody quotes.
    for (const std::size_t n : sizes)
    {
        const std::size_t payload = sizeof(engine::transform) + sizeof(ecs_probe::velocity)
                                  + sizeof(ecs_probe::bounds) + sizeof(ecs_probe::material);
        const std::size_t arch_bytes = n * (payload + sizeof(ecs_probe::entity));
        const std::size_t index_bytes = n * 4u * sizeof(std::uint32_t);
        const std::size_t sparse_bytes =
            n * (payload + 4u * sizeof(ecs_probe::entity)) + index_bytes;
        std::printf("mem %zu %zu %zu %zu\n", n, arch_bytes, sparse_bytes, index_bytes);
    }

    for (const std::size_t n : sizes)
    {
        const ecs_probe::archetype_world arch(n);
        const ecs_probe::sparse_world aligned(n, false);
        const ecs_probe::sparse_world scrambled(n, true);
        query_all(n, arch, aligned, scrambled);
    }

    for (const std::size_t n : sizes)
    {
        churn_at(n, 0, "narrow");
        churn_at(n, 8, "wide");
    }

    for (const std::size_t n : {std::size_t{10000}, std::size_t{100000}})
    {
        const ecs_probe::archetype_world one(n);
        for (const std::size_t c : {std::size_t{8}, std::size_t{64}, std::size_t{512},
                                    std::size_t{4096}})
        {
            fragment_at(n, c, one);
        }
    }

    for (const std::size_t n : sizes)
    {
        select_at<false>(n);
        select_at<true>(n);
    }
    return 0;
}
