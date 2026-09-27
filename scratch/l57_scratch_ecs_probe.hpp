// scratch/ecs_probe.hpp — the two candidate ECS storage shapes, cut down to the
// one question that separates them.
//
// Lesson 5.7. Shared by `bench_57.cpp`, which TIMES these shapes, and by
// `verify_57.cpp`, which checks that they describe the SAME WORLD — the same
// discipline `scene_layouts.hpp` follows for 5.6, and for the same reason: a
// benchmark transcribed into its own test has tested the transcription.
//
// WHAT IS AND IS NOT HERE. This is not an ECS and must not grow into one; 5.8
// builds the runtime. There is no entity manager, no component registry, no
// type erasure, no view, no system scheduler. What is here is exactly the two
// ACCESS PATTERNS the design question turns on, because those can be measured
// before either design is committed to — which is the whole point of taking the
// measurement now rather than after writing three thousand lines:
//
//   ARCHETYPE   entities with the same component SET live together, so the K
//               components a query reads are K parallel dense arrays walked by
//               the SAME index. No indirection, no per-entity test.
//               (Unity DOTS, Flecs, Unreal Mass.)
//
//   SPARSE SET  one dense array per component TYPE, plus a sparse map from
//               entity to dense index. A query walks one pool densely and
//               reaches the others through `data[sparse[e]]` — two dependent
//               loads per extra component.
//               (EnTT.)
//
// Everything else about the two designs — how views are built, how systems are
// scheduled, how a component type is registered — is orthogonal to the question
// "what does a query cost and what does adding a component cost", and including
// it would put a thousand lines of noise between us and two numbers.
//
// A NOTE ON `engine::pool<T>`, WHICH THIS FILE DELIBERATELY DOES NOT USE.
// Lesson 5.4's pool already has three arrays in exactly the sparse-set shape:
// `slots_` sparse and stable, `items_` dense and packed, `owners_` the way back.
// That is a hint and it is not an argument. A pool mints its OWN slot indices,
// so a handle from the mesh pool means nothing to the texture pool, and an ECS
// needs every pool keyed by ONE id. More to the point: choosing sparse sets
// because we happen to own something shaped like one is choosing an architecture
// by what is convenient. The numbers decide, and then §6 asks what `pool<T>`
// would have to become.

#pragma once

#include "scene_layouts.hpp"

#include <engine/math/transform.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace ecs_probe {

/// An entity is a bare index here. The real one is 5.4's generational handle;
/// the extra 32 bits change nothing about the access patterns being measured, and
/// leaving them out keeps the sparse map's cost honest at four bytes an entry.
using entity = std::uint32_t;

inline constexpr std::uint32_t k_none = 0xFFFFFFFFu;

// ---- The components -------------------------------------------------------
//
// The lead component is the engine's own `engine::transform` — 60 bytes, and the
// same 60 bytes Lesson 5.6 timed — so that 5.7's "real" workload IS 5.6's
// full-transform workload and the two lessons' numbers sit on the same scale.
// The other three are sixteen bytes each, which is what a small component
// actually weighs, and identical to each other in size so that K is the only
// thing changing when the query widens.

struct velocity
{
    engine::vec3 v{};
    float damping = 0.0f;
};

struct bounds
{
    engine::vec3 centre{};
    float radius = 0.0f;
};

struct material
{
    std::uint32_t albedo = 0u;
    float roughness = 0.0f;
    float metallic = 0.0f;
    float ior = 0.0f;
};

/// Filler for the "wide entity" structural test — eight more sixteen-byte
/// components that no query reads. A real entity has ten to thirty components;
/// four would flatter one of the two designs and it is not the sparse set.
struct pad16
{
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 0.0f;
};

/// Deterministic component values from the entity id — the same integer hash
/// `layouts::make_transform` uses, so every arm holds bit-identical data and the
/// accumulated answers can be compared exactly rather than within a tolerance.
[[nodiscard]] inline float frand(std::uint32_t x, std::uint32_t salt)
{
    x = (x ^ salt) * 2654435761u;
    x ^= x >> 15;
    return static_cast<float>(x % 4096u) / 4096.0f;
}

[[nodiscard]] inline velocity make_velocity(entity e)
{
    return {{frand(e, 11u) * 2.0f - 1.0f, frand(e, 12u) * 2.0f - 1.0f,
             frand(e, 13u) * 2.0f - 1.0f},
            0.5f + frand(e, 14u)};
}

[[nodiscard]] inline bounds make_bounds(entity e)
{
    return {{frand(e, 21u), frand(e, 22u), frand(e, 23u)}, 0.25f + frand(e, 24u)};
}

[[nodiscard]] inline material make_material(entity e)
{
    return {e * 2654435761u, frand(e, 31u), frand(e, 32u), 1.0f + frand(e, 33u)};
}

[[nodiscard]] inline pad16 make_pad(entity e, std::uint32_t k)
{
    return {frand(e, 41u + k), frand(e, 51u + k), frand(e, 61u + k), frand(e, 71u + k)};
}

// ---- The per-entity work --------------------------------------------------
//
// BOTH BODIES ARE SHARED BY EVERY ARM, so the only difference between the arms is
// HOW the components were reached. That is the whole experiment; a body written
// twice is a body that can differ, and then the ratio measures the difference.

/// Almost nothing: one float out of each component. Isolates the cost of REACHING
/// the data by making the cost of using it negligible.
template <int K>
[[nodiscard]] inline double cheap_body(const engine::transform& t, const velocity* v,
                                       const bounds* b, const material* m, double bias)
{
    double acc = static_cast<double>(t.position.x) + bias;
    if constexpr (K >= 2) { acc += static_cast<double>(v->v.x); }
    if constexpr (K >= 3) { acc += static_cast<double>(b->radius); }
    if constexpr (K >= 4) { acc += static_cast<double>(m->roughness); }
    return acc;
}

/// What a render query actually does: build the model matrix, then read the rest.
/// `layouts::sample` reads all sixteen entries for 5.6's reason — reading five of
/// them lets an inlined arm skip work a non-inlined one cannot.
template <int K>
[[nodiscard]] inline double real_body(const engine::transform& t, const velocity* v,
                                      const bounds* b, const material* m, double bias)
{
    double acc = layouts::sample(engine::parent_from_local(t)) + bias;
    if constexpr (K >= 2) { acc += static_cast<double>(v->v.x); }
    if constexpr (K >= 3) { acc += static_cast<double>(b->radius); }
    if constexpr (K >= 4) { acc += static_cast<double>(m->roughness); }
    return acc;
}

// ===========================================================================
//  ARM A — THE ARCHETYPE
// ===========================================================================

/// One archetype: the columns for every entity whose component set is exactly
/// this one. Parallel arrays, so row `i` is one entity across all of them.
///
/// `columns` says how many of the four the archetype actually has: 1 means
/// transform only, 4 means all of them. A query for K components visits an
/// archetype if and only if `columns >= K`, which is the archetype's structural
/// advantage in one line — non-matching entities are never touched, not even to
/// be rejected.
struct chunk
{
    std::vector<entity> ids;
    std::vector<engine::transform> xf;
    std::vector<velocity> vel;
    std::vector<bounds> bnd;
    std::vector<material> mat;
    std::array<std::vector<pad16>, 8> extra;

    int columns = 4;
    std::size_t extras = 0;

    void push(entity e, std::size_t n_extras)
    {
        ids.push_back(e);
        xf.push_back(layouts::make_transform(e));
        if (columns >= 2) { vel.push_back(make_velocity(e)); }
        if (columns >= 3) { bnd.push_back(make_bounds(e)); }
        if (columns >= 4) { mat.push_back(make_material(e)); }
        for (std::size_t k = 0; k < n_extras; ++k)
        {
            extra[k].push_back(make_pad(e, static_cast<std::uint32_t>(k)));
        }
        extras = n_extras;
    }
};

/// `n` entities split evenly across `n_chunks` archetypes, in id order.
///
/// Walking the chunks in order therefore visits entities 0..n-1 in order, which
/// is the same order the sparse arms visit them in — so the two arms sum the same
/// numbers in the same sequence and `bench_ab::agree` can demand bit equality
/// rather than a tolerance. 5.6 could not make that check; here it is free.
struct archetype_world
{
    std::vector<chunk> chunks;

    archetype_world(std::size_t n, std::size_t n_chunks = 1, std::size_t extras = 0)
    {
        chunks.resize(n_chunks);
        for (std::size_t c = 0; c < n_chunks; ++c)
        {
            const std::size_t lo = n * c / n_chunks;
            const std::size_t hi = n * (c + 1) / n_chunks;
            chunks[c].xf.reserve(hi - lo);
            for (std::size_t e = lo; e < hi; ++e)
            {
                chunks[c].push(static_cast<entity>(e), extras);
            }
        }
    }
};

/// The selective world: `keep_num/keep_den` of the entities carry a velocity, the
/// rest carry only a transform, and they are TWO archetypes. Component sets are
/// what an archetype is, so selectivity costs it nothing but a second chunk.
[[nodiscard]] inline archetype_world make_selective_archetypes(std::size_t n,
                                                               std::size_t keep_num,
                                                               std::size_t keep_den)
{
    archetype_world w(0);
    w.chunks.resize(2);
    w.chunks[0].columns = 2;   // transform + velocity — the queried one
    w.chunks[1].columns = 1;   // transform only
    for (std::size_t e = 0; e < n; ++e)
    {
        const bool keep = (e % keep_den) < keep_num;
        w.chunks[keep ? 0 : 1].push(static_cast<entity>(e), 0);
    }
    return w;
}

template <int K, bool Real>
[[nodiscard]] inline double archetype_query(const archetype_world& w, double bias)
{
    double acc = 0.0;
    for (const chunk& c : w.chunks)
    {
        if (c.columns < K) { continue; }
        const std::size_t n = c.xf.size();
        for (std::size_t i = 0; i < n; ++i)
        {
            // The index is the same for every column. That IS the archetype.
            const velocity* v = (K >= 2) ? c.vel.data() + i : nullptr;
            const bounds* b = (K >= 3) ? c.bnd.data() + i : nullptr;
            const material* m = (K >= 4) ? c.mat.data() + i : nullptr;
            acc += Real ? real_body<K>(c.xf[i], v, b, m, bias)
                        : cheap_body<K>(c.xf[i], v, b, m, bias);
        }
    }
    return acc;
}

// ===========================================================================
//  ARM B — THE SPARSE SET
// ===========================================================================

/// One component type's storage: sparse map, dense entity list, dense data.
///
/// The sparse array is indexed by entity id and sized by the HIGHEST id ever
/// seen, not by how many entities have this component — four bytes per entity per
/// component type, whether or not the component is there. Real implementations
/// page it; §5 of the lesson costs both.
template <typename T>
struct pool
{
    std::vector<std::uint32_t> sparse;
    std::vector<entity> dense;
    std::vector<T> data;

    void reserve_entities(std::size_t n) { sparse.assign(n, k_none); }

    void insert(entity e, const T& v)
    {
        sparse[e] = static_cast<std::uint32_t>(dense.size());
        dense.push_back(e);
        data.push_back(v);
    }

    /// Swap-and-pop, and the sparse entry of the entity that MOVED has to be
    /// patched. Two arrays touched, one extra sparse write, no data copied except
    /// the one component. This is the operation the archetype cannot match.
    void erase(entity e)
    {
        const std::uint32_t at = sparse[e];
        const std::uint32_t last = static_cast<std::uint32_t>(dense.size() - 1u);
        dense[at] = dense[last];
        data[at] = data[last];
        sparse[dense[at]] = at;
        dense.pop_back();
        data.pop_back();
        sparse[e] = k_none;
    }

    [[nodiscard]] bool contains(entity e) const { return sparse[e] != k_none; }
    [[nodiscard]] const T& get(entity e) const { return data[sparse[e]]; }
    [[nodiscard]] std::size_t size() const { return dense.size(); }
};

/// Four pools over one shared id space.
///
/// `scramble` is the axis that matters and the one most treatments omit. A pool's
/// dense order is its INSERTION order, so in a world built all at once every
/// pool's dense order is the same and `sparse[e]` walks forward through memory
/// alongside the lead pool — the lookups are sequential and nearly free. In a
/// world that has been running for ten minutes, with entities spawning and
/// components being added at different times, the orders diverge and the same
/// code performs random access. Both are real; the truth is between them, and a
/// design argument that quotes only the first is not an argument.
struct sparse_world
{
    pool<engine::transform> xf;
    pool<velocity> vel;
    pool<bounds> bnd;
    pool<material> mat;
    std::array<pool<pad16>, 8> extra;
    std::size_t extras = 0;

    sparse_world(std::size_t n, bool scramble, std::size_t n_extras = 0)
    {
        extras = n_extras;
        xf.reserve_entities(n);
        vel.reserve_entities(n);
        bnd.reserve_entities(n);
        mat.reserve_entities(n);
        for (std::size_t k = 0; k < n_extras; ++k) { extra[k].reserve_entities(n); }

        // The LEAD pool is always in id order: the query walks it, so its order
        // is the visitation order and must match the archetype arm's.
        for (std::size_t e = 0; e < n; ++e)
        {
            xf.insert(static_cast<entity>(e), layouts::make_transform(static_cast<entity>(e)));
        }

        std::vector<entity> order(n);
        std::iota(order.begin(), order.end(), entity{0});

        const auto fill = [&](auto& p, auto make, std::uint32_t seed) {
            if (scramble)
            {
                std::mt19937 rng(seed);
                std::shuffle(order.begin(), order.end(), rng);
            }
            for (const entity e : order) { p.insert(e, make(e)); }
        };

        fill(vel, make_velocity, 1u);
        fill(bnd, make_bounds, 2u);
        fill(mat, make_material, 3u);
        for (std::size_t k = 0; k < n_extras; ++k)
        {
            fill(extra[k], [k](entity e) { return make_pad(e, static_cast<std::uint32_t>(k)); },
                 static_cast<std::uint32_t>(10u + k));
        }
    }
};

template <int K, bool Real>
[[nodiscard]] inline double sparse_query(const sparse_world& w, double bias)
{
    double acc = 0.0;
    const std::size_t n = w.xf.dense.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        // Two loads per extra component: the redirect, then the payload. The
        // redirect's address is known (it is `sparse[e]`, and `e` came off a
        // dense array), so several of these can be in flight at once — 5.6's
        // memory-level parallelism, and the reason this is the 2.4x shape rather
        // than the 6.5x one.
        const entity e = w.xf.dense[i];
        const velocity* v = (K >= 2) ? w.vel.data.data() + w.vel.sparse[e] : nullptr;
        const bounds* b = (K >= 3) ? w.bnd.data.data() + w.bnd.sparse[e] : nullptr;
        const material* m = (K >= 4) ? w.mat.data.data() + w.mat.sparse[e] : nullptr;
        acc += Real ? real_body<K>(w.xf.data[i], v, b, m, bias)
                    : cheap_body<K>(w.xf.data[i], v, b, m, bias);
    }
    return acc;
}

/// The selective query, done the way a good sparse-set implementation does it:
/// LEAD WITH THE SMALLEST POOL. Walking `vel` densely and looking `xf` up means
/// no entity is visited only to be rejected — the skipping problem that a naive
/// implementation has, and that is usually presented as inherent, is a choice.
template <bool Real>
[[nodiscard]] inline double sparse_query_lead_vel(const sparse_world& w, double bias)
{
    double acc = 0.0;
    const std::size_t n = w.vel.dense.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const entity e = w.vel.dense[i];
        const engine::transform& t = w.xf.data[w.xf.sparse[e]];
        acc += Real ? real_body<2>(t, &w.vel.data[i], nullptr, nullptr, bias)
                    : cheap_body<2>(t, &w.vel.data[i], nullptr, nullptr, bias);
    }
    return acc;
}

/// The same query written the NAIVE way: lead with the big pool and skip. Kept
/// because the difference between it and the one above is the single cheapest
/// implementation decision in either design.
template <bool Real>
[[nodiscard]] inline double sparse_query_lead_xf(const sparse_world& w, double bias)
{
    double acc = 0.0;
    const std::size_t n = w.xf.dense.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        const entity e = w.xf.dense[i];
        const std::uint32_t at = w.vel.sparse[e];
        if (at == k_none) { continue; }
        acc += Real ? real_body<2>(w.xf.data[i], &w.vel.data[at], nullptr, nullptr, bias)
                    : cheap_body<2>(w.xf.data[i], &w.vel.data[at], nullptr, nullptr, bias);
    }
    return acc;
}

/// The move that decides this lesson's recommendation, and the reason it can be
/// MEASURED rather than promised.
///
/// A sparse set's dense order is its own business — nothing outside the pool
/// depends on it. So a pair of pools can be SORTED INTO A COMMON ORDER, putting
/// the entities that have both components at the front of both dense arrays, in
/// the same sequence. Once that holds, a query over the pair needs no sparse
/// read at all: index `i` of one array and index `i` of the other are the same
/// entity, which is precisely what an archetype chunk guarantees.
///
/// EnTT calls this a GROUP. It is not a different storage design; it is a
/// maintenance obligation laid on top of one, paid on insert and erase. The
/// point for us is the DIRECTION of the migration: a sparse set can be given an
/// archetype's query later, incrementally, one group at a time, without changing
/// what a component is. An archetype cannot be given a sparse set's O(1)
/// structural change at all.
template <bool Real>
[[nodiscard]] inline double sparse_query_grouped(const sparse_world& w, double bias)
{
    double acc = 0.0;
    const std::size_t n = w.vel.dense.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        // No `sparse[e]`. The two arrays agree on index, by construction.
        acc += Real ? real_body<2>(w.xf.data[i], &w.vel.data[i], nullptr, nullptr, bias)
                    : cheap_body<2>(w.xf.data[i], &w.vel.data[i], nullptr, nullptr, bias);
    }
    return acc;
}

/// A sparse world where only some entities carry a velocity, built so that both
/// selective arms see the same set. `xf` still holds every entity in id order.
[[nodiscard]] inline sparse_world make_selective_sparse(std::size_t n, std::size_t keep_num,
                                                        std::size_t keep_den, bool scramble)
{
    sparse_world w(0, false);
    w.xf.reserve_entities(n);
    w.vel.reserve_entities(n);
    w.bnd.reserve_entities(n);
    w.mat.reserve_entities(n);
    for (std::size_t e = 0; e < n; ++e)
    {
        w.xf.insert(static_cast<entity>(e), layouts::make_transform(static_cast<entity>(e)));
    }

    std::vector<entity> kept;
    for (std::size_t e = 0; e < n; ++e)
    {
        if ((e % keep_den) < keep_num) { kept.push_back(static_cast<entity>(e)); }
    }
    if (scramble)
    {
        std::mt19937 rng(7u);
        std::shuffle(kept.begin(), kept.end(), rng);
    }
    for (const entity e : kept) { w.vel.insert(e, make_velocity(e)); }
    return w;
}

/// The same world with the group maintained: the entities that have both
/// components occupy the front of BOTH dense arrays, in the same order.
///
/// Note what this costs to build and what it does not change: the sparse maps are
/// still there, `contains` still works for every entity, and an entity outside the
/// group is stored exactly as before. Only the ORDER moved.
[[nodiscard]] inline sparse_world make_grouped_sparse(std::size_t n, std::size_t keep_num,
                                                      std::size_t keep_den)
{
    sparse_world w(0, false);
    w.xf.reserve_entities(n);
    w.vel.reserve_entities(n);
    w.bnd.reserve_entities(n);
    w.mat.reserve_entities(n);

    for (std::size_t e = 0; e < n; ++e)
    {
        if ((e % keep_den) < keep_num)
        {
            w.xf.insert(static_cast<entity>(e), layouts::make_transform(static_cast<entity>(e)));
            w.vel.insert(static_cast<entity>(e), make_velocity(static_cast<entity>(e)));
        }
    }
    for (std::size_t e = 0; e < n; ++e)
    {
        if ((e % keep_den) >= keep_num)
        {
            w.xf.insert(static_cast<entity>(e), layouts::make_transform(static_cast<entity>(e)));
        }
    }
    return w;
}

// ===========================================================================
//  THE STRUCTURAL CHANGE
// ===========================================================================
//
// Adding a component to an entity is where the two designs stop resembling each
// other. For the sparse set it is a push_back and a sparse write, and it touches
// ONE component type. For the archetype it is a MOVE between archetypes: every
// column the entity has must be copied out of the source chunk and into the
// destination, and the source must be re-packed. Its cost is therefore
// proportional to the entity's total width, which is why `extras` exists.

/// The archetype side: two chunks, `with` and `without` a material, plus the
/// bookkeeping every archetype ECS needs — which archetype an entity is in and
/// which row of it. Both maps have to be patched on every move, and one of them
/// has to be patched for a SECOND entity, the one swap-and-pop displaced.
struct archetype_churn
{
    chunk with;
    chunk without;
    std::vector<std::uint8_t> in_with;
    std::vector<std::uint32_t> row;
    std::size_t extras = 0;

    archetype_churn(std::size_t n, std::size_t n_extras)
        : in_with(n, 1u), row(n, 0u), extras(n_extras)
    {
        with.columns = 4;
        without.columns = 3;
        for (std::size_t e = 0; e < n; ++e)
        {
            row[e] = static_cast<std::uint32_t>(with.ids.size());
            with.push(static_cast<entity>(e), n_extras);
        }
    }

    static void pop_row(chunk& c, std::uint32_t at, std::vector<std::uint32_t>& row,
                        std::size_t extras)
    {
        const std::uint32_t last = static_cast<std::uint32_t>(c.ids.size() - 1u);
        c.ids[at] = c.ids[last];
        c.xf[at] = c.xf[last];
        if (c.columns >= 2) { c.vel[at] = c.vel[last]; }
        if (c.columns >= 3) { c.bnd[at] = c.bnd[last]; }
        if (c.columns >= 4) { c.mat[at] = c.mat[last]; }
        for (std::size_t k = 0; k < extras; ++k) { c.extra[k][at] = c.extra[k][last]; }
        row[c.ids[at]] = at;

        c.ids.pop_back();
        c.xf.pop_back();
        if (c.columns >= 2) { c.vel.pop_back(); }
        if (c.columns >= 3) { c.bnd.pop_back(); }
        if (c.columns >= 4) { c.mat.pop_back(); }
        for (std::size_t k = 0; k < extras; ++k) { c.extra[k].pop_back(); }
    }

    /// Move `e` out of the archetype that has a material and into the one that
    /// does not. Every column travels; the material is dropped.
    void remove_material(entity e)
    {
        const std::uint32_t at = row[e];
        without.ids.push_back(e);
        without.xf.push_back(with.xf[at]);
        without.vel.push_back(with.vel[at]);
        without.bnd.push_back(with.bnd[at]);
        for (std::size_t k = 0; k < extras; ++k)
        {
            without.extra[k].push_back(with.extra[k][at]);
        }
        // ORDER MATTERS, and getting it wrong is silent. `pop_row` patches the
        // row of whichever entity swap-and-pop displaced — and when `e` was the
        // last row, that entity IS `e`. Writing `row[e]` first and re-packing
        // afterwards therefore clobbers the value we just wrote with a stale one.
        // Re-pack first, then record where `e` went.
        const std::uint32_t landed = static_cast<std::uint32_t>(without.ids.size() - 1u);
        pop_row(with, at, row, extras);
        row[e] = landed;
        in_with[e] = 0u;
    }

    void add_material(entity e)
    {
        const std::uint32_t at = row[e];
        with.ids.push_back(e);
        with.xf.push_back(without.xf[at]);
        with.vel.push_back(without.vel[at]);
        with.bnd.push_back(without.bnd[at]);
        with.mat.push_back(make_material(e));
        for (std::size_t k = 0; k < extras; ++k)
        {
            with.extra[k].push_back(without.extra[k][at]);
        }
        const std::uint32_t landed = static_cast<std::uint32_t>(with.ids.size() - 1u);
        pop_row(without, at, row, extras);
        row[e] = landed;
        in_with[e] = 1u;
    }
};

/// The sparse side of the same experiment. `extras` is carried so the two arms
/// hold the same world, and it is never touched by add or remove — which is the
/// finding, not an oversight.
struct sparse_churn
{
    sparse_world w;

    sparse_churn(std::size_t n, std::size_t n_extras) : w(n, false, n_extras) {}

    void remove_material(entity e) { w.mat.erase(e); }
    void add_material(entity e) { w.mat.insert(e, make_material(e)); }
};

/// The entities touched in one "frame": `n/100` of them, at a stride that walks a
/// different set each frame without needing a random number in the timed loop.
[[nodiscard]] inline std::vector<entity> churn_set(std::size_t n, std::size_t frame)
{
    const std::size_t count = std::max<std::size_t>(1u, n / 100u);
    std::vector<entity> out;
    out.reserve(count);
    // A stride coprime with any n we use, so the set rotates rather than repeats.
    const std::size_t stride = 97u;
    for (std::size_t i = 0; i < count; ++i)
    {
        out.push_back(static_cast<entity>((frame * count + i * stride) % n));
    }
    // Deduplicate: adding a component twice is not a structural change, it is a
    // bug, and a benchmark that performs one is measuring the bug.
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

}   // namespace ecs_probe
