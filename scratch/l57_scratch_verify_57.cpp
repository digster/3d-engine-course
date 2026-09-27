// scratch/verify_57.cpp — Lesson 5.7's harness: the probe, and the claims made from it.
//
//   §A  the sparse set's invariants, including after erase
//   §B  the archetype world and the sparse worlds hold THE SAME COMPONENTS
//   §C  every query arm computes the same answer
//   §D  splitting an archetype into 4,096 chunks does not change the answer
//   §E  a structural change leaves both worlds consistent, and the values survive it
//   §F  the selective arms agree, and the "grouped" world really is grouped
//   §G  the golden is still byte-identical
//
// §B and §C are the sections that make the tables mean anything. Every ratio in
// this lesson is a comparison, and a comparison between two arms that compute
// different answers is not a measurement — it is a bug report with a stopwatch
// attached. §E is the one that caught a real bug: the archetype's row map has to
// be written AFTER the source chunk is re-packed, because when the moved entity
// was the last row, re-packing patches that entity's own row entry.
//
// Build and run:  sh scratch/build_verify_57.sh

#include <engine/core/bench.hpp>

#include "ecs_probe.hpp"
#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
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

/// Byte equality, because these components are trivially copyable and "the same
/// component" should mean the same bits, not the same to within a tolerance.
template <typename T>
[[nodiscard]] bool same_bits(const T& a, const T& b)
{
    return std::memcmp(&a, &b, sizeof(T)) == 0;
}

constexpr std::size_t k_n = 5000;

// ---------------------------------------------------------------------------

void section_a_pool()
{
    std::printf("\n=== A. The sparse set's invariants ===\n");

    ecs_probe::pool<ecs_probe::velocity> p;
    p.reserve_entities(64);
    check(p.size() == 0, "a fresh pool is empty");
    check(!p.contains(7), "…and contains nothing");

    for (ecs_probe::entity e = 0; e < 16; ++e) { p.insert(e, ecs_probe::make_velocity(e)); }
    checkf(p.size() == 16, "16 inserts -> size 16 (got %zu)", p.size());

    bool round_trip = true;
    for (std::size_t i = 0; i < p.dense.size(); ++i)
    {
        if (p.sparse[p.dense[i]] != i) { round_trip = false; }
    }
    check(round_trip, "sparse[dense[i]] == i for every i — the whole invariant");

    bool values_ok = true;
    for (ecs_probe::entity e = 0; e < 16; ++e)
    {
        if (!same_bits(p.get(e), ecs_probe::make_velocity(e))) { values_ok = false; }
    }
    check(values_ok, "every entity's component reads back bit-identical");

    // Erase from the MIDDLE, which is the case swap-and-pop exists for and the
    // case that moves another entity behind the caller's back.
    const ecs_probe::entity moved = p.dense.back();
    p.erase(3);
    check(!p.contains(3), "erased entity is gone");
    checkf(p.size() == 15, "…and the pool shrank by one (got %zu)", p.size());
    check(p.contains(moved) && p.sparse[moved] == 3,
          "the entity that swap-and-pop displaced had its sparse entry patched");
    check(same_bits(p.get(moved), ecs_probe::make_velocity(moved)),
          "…and its component travelled with it, unchanged");

    // Erase the LAST element: the degenerate case where the moved entity is the
    // erased one. It is where an ordering bug hides.
    const ecs_probe::entity last = p.dense.back();
    p.erase(last);
    check(!p.contains(last), "erasing the last element is not a special case");

    round_trip = true;
    for (std::size_t i = 0; i < p.dense.size(); ++i)
    {
        if (p.sparse[p.dense[i]] != i) { round_trip = false; }
    }
    check(round_trip, "the invariant survives both erases");
}

void section_b_same_world()
{
    std::printf("\n=== B. The archetype world and the sparse worlds hold the same components ===\n");

    const ecs_probe::archetype_world arch(k_n);
    const ecs_probe::sparse_world aligned(k_n, false);
    const ecs_probe::sparse_world scrambled(k_n, true);

    checkf(arch.chunks.size() == 1 && arch.chunks[0].ids.size() == k_n,
           "the archetype world is one chunk of %zu entities", k_n);
    checkf(aligned.xf.size() == k_n && aligned.mat.size() == k_n,
           "the aligned sparse world has %zu of each component", k_n);

    bool ids_in_order = true;
    for (std::size_t i = 0; i < k_n; ++i)
    {
        if (arch.chunks[0].ids[i] != i || aligned.xf.dense[i] != i
            || scrambled.xf.dense[i] != i)
        {
            ids_in_order = false;
        }
    }
    check(ids_in_order,
          "all three arms visit entities in the SAME order — which is why the "
          "benchmark can demand bit equality instead of a tolerance");

    int mismatches = 0;
    for (std::size_t i = 0; i < k_n; ++i)
    {
        const ecs_probe::entity e = static_cast<ecs_probe::entity>(i);
        if (!same_bits(arch.chunks[0].xf[i], aligned.xf.get(e))) { ++mismatches; }
        if (!same_bits(arch.chunks[0].vel[i], aligned.vel.get(e))) { ++mismatches; }
        if (!same_bits(arch.chunks[0].bnd[i], aligned.bnd.get(e))) { ++mismatches; }
        if (!same_bits(arch.chunks[0].mat[i], aligned.mat.get(e))) { ++mismatches; }
        if (!same_bits(arch.chunks[0].xf[i], scrambled.xf.get(e))) { ++mismatches; }
        if (!same_bits(arch.chunks[0].vel[i], scrambled.vel.get(e))) { ++mismatches; }
        if (!same_bits(arch.chunks[0].bnd[i], scrambled.bnd.get(e))) { ++mismatches; }
        if (!same_bits(arch.chunks[0].mat[i], scrambled.mat.get(e))) { ++mismatches; }
    }
    checkf(mismatches == 0, "%zu entities x 4 components x 2 sparse worlds agree bit for bit "
                            "(%d mismatches)", k_n, mismatches);

    // …and the scrambled world is actually scrambled, or the arm measures nothing.
    std::size_t in_place = 0;
    for (std::size_t i = 0; i < k_n; ++i)
    {
        if (scrambled.vel.dense[i] == i) { ++in_place; }
    }
    checkf(in_place < k_n / 100u,
           "the scrambled world's velocity pool really is permuted: %zu of %zu entities "
           "sit at their own index", in_place, k_n);
    check(aligned.vel.dense[k_n / 2] == k_n / 2,
          "…and the aligned world's really is not");
}

void section_c_queries()
{
    std::printf("\n=== C. Every query arm computes the same answer ===\n");

    const ecs_probe::archetype_world arch(k_n);
    const ecs_probe::sparse_world aligned(k_n, false);
    const ecs_probe::sparse_world scrambled(k_n, true);

    const double bias = 0.125;

    const auto compare = [&](const char* label, double a, double b, double c) {
        checkf(a == b && b == c, "%s: archetype %.9f, aligned %.9f, scrambled %.9f", label, a, b, c);
    };

    compare("K=1 cheap",
            ecs_probe::archetype_query<1, false>(arch, bias),
            ecs_probe::sparse_query<1, false>(aligned, bias),
            ecs_probe::sparse_query<1, false>(scrambled, bias));
    compare("K=2 cheap",
            ecs_probe::archetype_query<2, false>(arch, bias),
            ecs_probe::sparse_query<2, false>(aligned, bias),
            ecs_probe::sparse_query<2, false>(scrambled, bias));
    compare("K=3 cheap",
            ecs_probe::archetype_query<3, false>(arch, bias),
            ecs_probe::sparse_query<3, false>(aligned, bias),
            ecs_probe::sparse_query<3, false>(scrambled, bias));
    compare("K=4 cheap",
            ecs_probe::archetype_query<4, false>(arch, bias),
            ecs_probe::sparse_query<4, false>(aligned, bias),
            ecs_probe::sparse_query<4, false>(scrambled, bias));
    compare("K=4 real",
            ecs_probe::archetype_query<4, true>(arch, bias),
            ecs_probe::sparse_query<4, true>(aligned, bias),
            ecs_probe::sparse_query<4, true>(scrambled, bias));

    // K must actually change the work, or the "K" axis is decoration.
    check(ecs_probe::archetype_query<1, false>(arch, bias)
              != ecs_probe::archetype_query<2, false>(arch, bias),
          "widening the query from K=1 to K=2 changes the answer — the axis is real");
    check(ecs_probe::archetype_query<4, false>(arch, bias)
              != ecs_probe::archetype_query<4, true>(arch, bias),
          "the cheap body and the real body are different work");
}

void section_d_fragmentation()
{
    std::printf("\n=== D. Splitting the archetype does not change the answer ===\n");

    const ecs_probe::archetype_world one(k_n, 1);
    const double bias = 0.25;
    const double reference = ecs_probe::archetype_query<4, true>(one, bias);

    for (const std::size_t chunks : {std::size_t{8}, std::size_t{64}, std::size_t{512},
                                     std::size_t{4096}})
    {
        const ecs_probe::archetype_world split(k_n, chunks);
        std::size_t total = 0;
        for (const ecs_probe::chunk& c : split.chunks) { total += c.ids.size(); }
        checkf(total == k_n && split.chunks.size() == chunks,
               "%zu chunks still hold all %zu entities", chunks, k_n);
        checkf(ecs_probe::archetype_query<4, true>(split, bias) == reference,
               "…and the query over %zu chunks is bit-identical to the query over one", chunks);
    }
}

void section_e_churn()
{
    std::printf("\n=== E. A structural change leaves both worlds consistent ===\n");

    constexpr std::size_t n = 2000;
    ecs_probe::archetype_churn arch(n, 3);
    ecs_probe::sparse_churn sparse(n, 3);

    checkf(arch.with.ids.size() == n && sparse.w.mat.size() == n,
           "both worlds start with all %zu entities carrying a material", n);

    // One frame: remove from 1% and add straight back.
    const std::vector<ecs_probe::entity> set = ecs_probe::churn_set(n, 0);
    checkf(set.size() == n / 100u, "the churn set is 1%% of the world (%zu of %zu)",
           set.size(), n);
    bool unique = true;
    for (std::size_t i = 1; i < set.size(); ++i)
    {
        if (set[i] == set[i - 1]) { unique = false; }
    }
    check(unique, "…and holds no entity twice — adding a component twice is a bug, not a change");

    for (const ecs_probe::entity e : set) { arch.remove_material(e); sparse.remove_material(e); }
    checkf(arch.with.ids.size() == n - set.size() && arch.without.ids.size() == set.size(),
           "after the removals the entities moved archetype: %zu with, %zu without",
           arch.with.ids.size(), arch.without.ids.size());
    check(sparse.w.mat.size() == n - set.size(),
          "…and the sparse world's material pool shrank by exactly as many");
    check(sparse.w.xf.size() == n && sparse.w.vel.size() == n,
          "…while its OTHER pools were not touched at all — the whole argument, in one check");

    for (const ecs_probe::entity e : set) { arch.add_material(e); sparse.add_material(e); }
    checkf(arch.with.ids.size() == n && arch.without.ids.empty(),
           "adding them back restores the archetypes (%zu with, %zu without)",
           arch.with.ids.size(), arch.without.ids.size());
    check(sparse.w.mat.size() == n, "…and the material pool");

    // Now churn hard, and check both worlds still describe the same thing.
    for (std::size_t f = 0; f < 200; ++f)
    {
        const std::vector<ecs_probe::entity> s = ecs_probe::churn_set(n, f);
        for (const ecs_probe::entity e : s) { arch.remove_material(e); sparse.remove_material(e); }
        for (const ecs_probe::entity e : s) { arch.add_material(e); sparse.add_material(e); }
    }

    bool rows_ok = true;
    for (std::size_t i = 0; i < arch.with.ids.size(); ++i)
    {
        if (arch.row[arch.with.ids[i]] != i || arch.in_with[arch.with.ids[i]] != 1u)
        {
            rows_ok = false;
        }
    }
    check(rows_ok, "after 200 frames the archetype's row map still points at the right rows "
                   "— the check that caught the ordering bug in add/remove");

    bool sparse_ok = true;
    for (std::size_t i = 0; i < sparse.w.mat.dense.size(); ++i)
    {
        if (sparse.w.mat.sparse[sparse.w.mat.dense[i]] != i) { sparse_ok = false; }
    }
    check(sparse_ok, "…and the sparse set's invariant still holds");

    int drift = 0;
    for (std::size_t e = 0; e < n; ++e)
    {
        const ecs_probe::entity id = static_cast<ecs_probe::entity>(e);
        const std::uint32_t at = arch.row[id];
        if (!same_bits(arch.with.xf[at], layouts::make_transform(id))) { ++drift; }
        if (!same_bits(arch.with.mat[at], ecs_probe::make_material(id))) { ++drift; }
        if (!same_bits(sparse.w.xf.get(id), layouts::make_transform(id))) { ++drift; }
        if (!same_bits(sparse.w.mat.get(id), ecs_probe::make_material(id))) { ++drift; }
    }
    checkf(drift == 0, "every component of every entity survived 400 moves unchanged "
                       "(%d drifted)", drift);
}

void section_f_selectivity()
{
    std::printf("\n=== F. The selective arms, and the group ===\n");

    const ecs_probe::archetype_world arch = ecs_probe::make_selective_archetypes(k_n, 1, 4);
    const ecs_probe::sparse_world aligned = ecs_probe::make_selective_sparse(k_n, 1, 4, false);
    const ecs_probe::sparse_world scrambled = ecs_probe::make_selective_sparse(k_n, 1, 4, true);
    const ecs_probe::sparse_world grouped = ecs_probe::make_grouped_sparse(k_n, 1, 4);

    checkf(arch.chunks.size() == 2 && arch.chunks[0].ids.size() == k_n / 4,
           "selectivity costs the archetype one extra chunk: %zu with velocity, %zu without",
           arch.chunks[0].ids.size(), arch.chunks[1].ids.size());
    checkf(aligned.vel.size() == k_n / 4 && aligned.xf.size() == k_n,
           "…and costs the sparse set nothing structural: %zu velocities over %zu transforms",
           aligned.vel.size(), aligned.xf.size());

    const double bias = 0.5;
    const double a = ecs_probe::archetype_query<2, true>(arch, bias);
    const double small = ecs_probe::sparse_query_lead_vel<true>(aligned, bias);
    const double big = ecs_probe::sparse_query_lead_xf<true>(aligned, bias);
    const double grp = ecs_probe::sparse_query_grouped<true>(grouped, bias);

    checkf(a == small, "leading with the small pool: %.9f vs archetype %.9f", small, a);
    checkf(a == big, "leading with the big pool and skipping: %.9f", big);
    checkf(a == grp, "the grouped walk, which reads no sparse entry at all: %.9f", grp);

    // The scrambled arm sums the same numbers in a DIFFERENT ORDER, and floating
    // point addition is not associative. Exactness is the wrong demand here; the
    // magnitude of the difference is the right one.
    const double scr = ecs_probe::sparse_query_lead_vel<true>(scrambled, bias);
    const double rel = std::abs(scr - a) / std::abs(a);
    checkf(rel < 1e-12, "the scrambled arm agrees to %.1e — a reordered sum, not different work",
           rel);

    // …and the grouped world really is grouped, or the arm is measuring a lie.
    bool group_ok = true;
    for (std::size_t i = 0; i < grouped.vel.dense.size(); ++i)
    {
        if (grouped.xf.dense[i] != grouped.vel.dense[i]) { group_ok = false; }
    }
    check(group_ok, "in the grouped world the two dense arrays agree on index for every "
                    "member — which is exactly what an archetype chunk guarantees");
    check(grouped.xf.size() == k_n,
          "…and the non-members are still in the transform pool, behind them");
}

void section_g_golden()
{
    std::printf("\n=== G. The golden ===\n");

    const char* path = "build/demos/verify_57.ppm";
    check(demo::write_reference_shot(path) == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file(path);
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(), "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL to the golden written in Lesson 5.1 — seven lessons now, and "
          "this one changed no engine code at all");
}

}   // namespace

int main()
{
    std::printf("verify_57 — Lesson 5.7: ECS storage design\n");

    section_a_pool();
    section_b_same_world();
    section_c_queries();
    section_d_fragmentation();
    section_e_churn();
    section_f_selectivity();
    section_g_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
