// scratch/verify_58.cpp — Lesson 5.8's harness: the ECS runtime, and the claims made for it.
//
//   §A  entity ids: minting, retiring, reuse, and the staleness that survives it
//   §B  the component pool: swap-and-pop, the sparse patch, and the last element
//   §C  the registry: one id space across N pools, and what destroy() reaches
//   §D  views: the intersection is right, and the SMALLEST POOL LEADS
//   §E  churn: 200 frames against an independent shadow model
//   §F  what the ECS costs in memory, measured rather than asserted
//   §G  the golden is still byte-identical
//
// §D is the section that checks a DESIGN decision rather than a behaviour, and it
// is the one worth reading first. "A view leads with the smallest pool" is Lesson
// 5.7's rule 3, bought with a measurement (1.73x -> 1.46x at one-in-four
// selectivity); a rule nothing observes is a rule that quietly stops being true,
// so `view::lead()` exists and §D reads it.
//
// §E is the one modelled on Lesson 5.7 §E, which is where a real bug was found.
// A single-frame test of swap-and-pop passes forever; the failure needs a moved
// element to be the element being removed, which is one iteration in N, so the
// test churns two hundred frames and then walks every invariant.
//
// Build and run:  sh scratch/build_verify_58.sh

#include <engine/ecs/registry.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

using engine::ecs::entity;
using engine::ecs::make_entity;
using engine::ecs::null_entity;

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
    std::string out(static_cast<const char*>(data), size);
    SDL_free(data);
    return out;
}

// ---- The component types this harness uses --------------------------------
//
// Deliberately different sizes and deliberately not the demo's, so that nothing
// here can pass by accidentally agreeing with `ecs_swarm`.

struct position { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct velocity { float x = 0.0f, y = 0.0f, z = 0.0f; };
struct label    { int id = 0; };
struct heavy    { double pad[8] = {}; int id = 0; };

// ===========================================================================
//  §A — ENTITY IDS
// ===========================================================================

void section_a_ids()
{
    std::printf("\n=== A. Entity ids ===\n");

    check(!null_entity.valid(), "the null entity is not valid");
    check(null_entity.bits == 0u, "…and it is all-bits-zero, so a zeroed struct is null for free");
    check(entity{} == null_entity, "a default-constructed entity IS the null entity");

    const entity fabricated = make_entity(1234u, 7u);
    checkf(fabricated.index() == 1234u && fabricated.generation() == 7u,
           "make_entity round-trips: index %u, generation %u",
           fabricated.index(), fabricated.generation());

    engine::ecs::entity_allocator ids;

    const entity a = ids.create();
    const entity b = ids.create();
    const entity c = ids.create();

    checkf(a.index() == 0u && b.index() == 1u && c.index() == 2u,
           "fresh ids come off the high-water mark in order: %u, %u, %u",
           a.index(), b.index(), c.index());
    check(a.generation() == 1u, "the first generation is 1, not 0 — 0 is reserved for null");
    check(ids.alive(a) && ids.alive(b) && ids.alive(c), "all three are alive");
    checkf(ids.size() == 3u && ids.slot_count() == 3u && ids.free_count() == 0u,
           "size 3, slots 3, free 0");

    // Retiring, and the generation bump that makes the old id stale.
    check(ids.destroy(b), "destroy(b) succeeds");
    check(!ids.alive(b), "…and b is no longer alive");
    check(!ids.destroy(b), "destroying it twice fails rather than double-freeing the slot");
    checkf(ids.size() == 2u && ids.slot_count() == 3u && ids.free_count() == 1u,
           "size 2, slots STILL 3 (the high-water mark never falls), free 1");

    // LIFO reuse: the next id takes b's slot, with a bumped generation.
    const entity b2 = ids.create();
    checkf(b2.index() == b.index(), "the next id reuses the freed slot (%u) rather than growing",
           b2.index());
    checkf(b2.generation() == b.generation() + 1u,
           "…with generation %u, one past the id it replaces (%u)",
           b2.generation(), b.generation());
    check(ids.alive(b2), "the new id is alive");
    check(!ids.alive(b), "and THE OLD ONE IS NOT — same slot, different occupant, and the "
                         "generation is what tells them apart");
    check(ids.slot_count() == 3u, "reuse did not grow the id space, which is why the sparse "
                                  "arrays are sized by PEAK LIVE and not by total ever created");

    // Foreign and fabricated ids.
    check(!ids.alive(make_entity(99u, 1u)), "an index past the high-water mark is not alive");
    check(!ids.alive(make_entity(a.index(), 4000u)), "a wild generation on a real slot is not alive");
    check(!ids.alive(null_entity), "the null entity is never alive");

    // clear() retires everything and keeps generations, so outstanding ids go
    // stale rather than starting to resolve again.
    ids.clear();
    check(ids.empty(), "clear() retires every id");
    check(!ids.alive(a) && !ids.alive(c) && !ids.alive(b2),
          "…and every outstanding id is stale, not re-resolving");
    const entity after = ids.create();
    check(after.generation() > 1u, "a slot reused after clear() carries a fresh generation");

    // The generation budget, exercised. 4,095 usable generations per slot, and a
    // LIFO free list hammers ONE slot, which is the case core/handle.hpp did the
    // arithmetic for.
    engine::ecs::entity_allocator churn;
    entity e = churn.create();
    for (int i = 0; i < 4096; ++i)
    {
        churn.destroy(e);
        e = churn.create();
    }
    checkf(churn.slot_count() == 1u, "4,096 create/destroy pairs used ONE slot (%zu)",
           churn.slot_count());
    checkf(churn.generation_wraps() == 1u,
           "…and the generation wrapped exactly once (%zu), counted rather than assumed",
           churn.generation_wraps());
}

// ===========================================================================
//  §B — THE COMPONENT POOL
// ===========================================================================

void section_b_pool()
{
    std::printf("\n=== B. The component pool ===\n");

    engine::ecs::entity_allocator ids;
    engine::ecs::pool<position> pos;

    std::vector<entity> es;
    for (int i = 0; i < 8; ++i) { es.push_back(ids.create()); }

    for (int i = 0; i < 8; ++i)
    {
        const float f = static_cast<float>(i);
        pos.insert(es[static_cast<std::size_t>(i)], position{f, f * 2.0f, f * 3.0f});
    }
    checkf(pos.size() == 8u, "8 components inserted (%zu)", pos.size());
    check(pos.entities().size() == pos.components().size(),
          "the dense entity list and the dense data are the same length, always");

    bool all_found = true;
    for (int i = 0; i < 8; ++i)
    {
        const position* p = pos.get(es[static_cast<std::size_t>(i)]);
        if (p == nullptr || p->x != static_cast<float>(i)) { all_found = false; }
    }
    check(all_found, "every entity gets its own component back");

    // ERASE FROM THE MIDDLE: swap-and-pop, and the sparse entry of the entity
    // that MOVED has to be patched. This is the line whose absence produces a
    // corruption that surfaces frames later, in a different system.
    const entity last_before = pos.entities().back();
    check(pos.erase(es[3]), "erase from the middle succeeds");
    check(!pos.contains(es[3]), "…the erased entity is gone");
    checkf(pos.size() == 7u, "…the pool shrank to %zu", pos.size());
    {
        const position* moved = pos.get(last_before);
        check(moved != nullptr && moved->x == 7.0f,
              "…AND THE ENTITY THAT WAS SWAPPED INTO THE HOLE STILL RESOLVES, to its own "
              "component and not to the dead one's — the sparse patch");
    }

    // ERASE THE LAST ELEMENT: the edge case where the entity that "moves" is the
    // entity being erased. Nothing should be patched, and nothing should break.
    const entity tail = pos.entities().back();
    check(pos.erase(tail), "erase the last element succeeds");
    check(!pos.contains(tail), "…and it is gone");
    checkf(pos.size() == 6u, "…leaving %zu", pos.size());
    {
        bool survivors_ok = true;
        for (entity s : pos.entities())
        {
            if (pos.get(s) == nullptr) { survivors_ok = false; }
        }
        check(survivors_ok, "…and every survivor still resolves");
    }

    check(!pos.erase(es[3]), "erasing something already erased returns false");
    check(!pos.contains(null_entity), "the null entity is never in a pool");
    check(pos.get(null_entity) == nullptr, "…and never resolves");

    // THE GENERATION CHECK, which is what `dense_` storing the full entity word
    // buys. A recycled id must not inherit its predecessor's component.
    const entity victim = pos.entities().front();
    const entity impostor = make_entity(victim.index(), victim.generation() + 1u);
    check(pos.contains(victim), "the live entity has its component");
    check(!pos.contains(impostor),
          "an id with the SAME INDEX and a newer generation does NOT — store a bare index in "
          "the dense array and this test cannot be written");

    // sparse_extent: the memory story, measured.
    checkf(pos.sparse_extent() == 8u,
           "the sparse array is %zu entries long — the highest entity index ever filed, plus "
           "one, not the number of components held (%zu)", pos.sparse_extent(), pos.size());

    pos.clear();
    check(pos.empty() && pos.size() == 0u, "clear() empties the pool");
    check(pos.sparse_extent() == 8u, "…and keeps the sparse array, because the entities it is "
                                     "sized for have not gone anywhere");
    {
        bool none_left = true;
        for (entity s : es)
        {
            if (pos.contains(s)) { none_left = false; }
        }
        check(none_left, "…and nothing resolves through it any more");
    }
}

// ===========================================================================
//  §C — THE REGISTRY
// ===========================================================================

void section_c_registry()
{
    std::printf("\n=== C. The registry ===\n");

    engine::ecs::registry w;

    const entity a = w.create();
    const entity b = w.create();
    const entity c = w.create();

    w.add<position>(a, {1.0f, 0.0f, 0.0f});
    w.add<velocity>(a, {0.5f, 0.0f, 0.0f});
    w.add<label>(a, {10});

    w.add<position>(b, {2.0f, 0.0f, 0.0f});
    w.add<velocity>(b, {0.0f, 1.0f, 0.0f});

    w.add<position>(c, {3.0f, 0.0f, 0.0f});

    checkf(w.size() == 3u, "3 entities (%zu)", w.size());
    checkf(w.component_count() == 6u, "6 components across all pools (%zu)", w.component_count());
    checkf(w.pool_count() == 3u, "3 pools created — one per component TYPE (%zu)",
           w.pool_count());

    check(w.has<position>(a) && w.has<velocity>(a) && w.has<label>(a), "a has all three");
    check(w.has<position>(b) && w.has<velocity>(b) && !w.has<label>(b), "b has two of three");
    check(w.has<position>(c) && !w.has<velocity>(c), "c has one");

    check(w.get<position>(a) != nullptr && w.get<position>(a)->x == 1.0f,
          "get() returns the right component");
    check(w.get<label>(b) == nullptr, "get() of a component an entity lacks is nullptr, not a crash");

    // ONE SHARED ID SPACE — the whole point of the module. The same id opens
    // every pool, which is the question `engine::pool<T>`'s per-pool keys could
    // not even express.
    check(w.get<position>(a) != nullptr && w.get<velocity>(a) != nullptr
              && w.get<label>(a) != nullptr,
          "ONE id resolves in three different pools — the property Lesson 5.4's pool could "
          "not offer, because each of its pools mints its own keys");

    // remove()
    check(w.remove<velocity>(b), "remove() succeeds");
    check(!w.has<velocity>(b), "…and the component is gone");
    check(!w.remove<velocity>(b), "…removing it twice returns false");
    check(w.has<position>(b), "…and the entity's other components are untouched");

    // destroy() must reach every pool.
    const std::size_t before = w.component_count();
    check(w.destroy(a), "destroy(a) succeeds");
    checkf(w.component_count() == before - 3u,
           "…and it erased ALL THREE of a's components (%zu -> %zu), which is what the type "
           "erasure exists for", before, w.component_count());
    check(!w.alive(a), "…a is not alive");
    check(!w.has<position>(a) && !w.has<velocity>(a) && !w.has<label>(a),
          "…and none of its components resolve");
    check(!w.destroy(a), "destroying it twice returns false");

    // A recycled id must not inherit anything.
    const entity a2 = w.create();
    checkf(a2.index() == a.index(), "the next entity reuses a's slot (%u)", a2.index());
    check(!w.has<position>(a2) && !w.has<velocity>(a2) && !w.has<label>(a2),
          "…and inherits NOTHING, because destroy() erased the rows AND the generation "
          "moved — two independent defences, either of which would do");

    // Adding to a dead entity is refused rather than filed under an id nothing
    // will ever look up again. (Release build: the assertion is compiled out and
    // the refusal is what remains. Debug: it fires first. Both are correct.)
    if (!engine::debug_assertions_enabled())
    {
        const std::size_t held = w.component_count();
        check(w.add<label>(a, {99}) == nullptr, "add() to a dead entity returns nullptr");
        check(w.component_count() == held,
              "…and files nothing, so a stale id cannot leak a row into a pool");
    }
    else
    {
        std::printf("  [SKIP] add-to-dead-entity refusal (debug build asserts first — "
                    "run with ENGINE_CFLAGS='-O2 -DNDEBUG')\n");
    }

    // storage<T>() is the direct route, and a type nobody has used has no pool.
    check(w.storage_if<heavy>() == nullptr,
          "a component type this world has never used has NO pool — asking must not create one");
    check(w.storage<heavy>().empty(), "storage<T>() creates it on demand");
    check(w.storage_if<heavy>() != nullptr, "…and now it exists");

    w.clear();
    check(w.empty() && w.component_count() == 0u, "clear() empties everything");
    check(!w.alive(b) && !w.alive(c), "…and every outstanding id is stale");
}

// ===========================================================================
//  §D — VIEWS
// ===========================================================================

/// The answer, computed the slow obvious way: ask every live entity whether it
/// has all three. A test whose reference implementation shares code with the
/// thing under test is not a test.
template <typename... Ts>
[[nodiscard]] std::set<std::uint32_t> brute_force(engine::ecs::registry& w,
                                                  const std::vector<entity>& all)
{
    std::set<std::uint32_t> out;
    for (entity e : all)
    {
        if (w.alive(e) && (... && w.has<Ts>(e))) { out.insert(e.bits); }
    }
    return out;
}

void section_d_views()
{
    std::printf("\n=== D. Views ===\n");

    engine::ecs::registry w;
    std::vector<entity> all;

    // 60 entities. Every one gets a position; one in two gets a velocity; one in
    // five gets a label. So the pools are 60 / 30 / 12, which is the spread rule
    // 3 exists for.
    for (int i = 0; i < 60; ++i)
    {
        const entity e = w.create();
        all.push_back(e);
        w.add<position>(e, {static_cast<float>(i), 0.0f, 0.0f});
        if (i % 2 == 0) { w.add<velocity>(e, {1.0f, 0.0f, 0.0f}); }
        if (i % 5 == 0) { w.add<label>(e, {i}); }
    }
    checkf(w.storage<position>().size() == 60u && w.storage<velocity>().size() == 30u
               && w.storage<label>().size() == 12u,
           "pools are %zu / %zu / %zu", w.storage<position>().size(),
           w.storage<velocity>().size(), w.storage<label>().size());

    // --- Two components ---
    {
        std::set<std::uint32_t> seen;
        int visits = 0;
        w.view<position, velocity>().each([&](entity e, position& p, velocity& v) {
            seen.insert(e.bits);
            ++visits;
            p.x += v.x;
        });
        const std::set<std::uint32_t> want = brute_force<position, velocity>(w, all);
        checkf(seen == want && visits == static_cast<int>(want.size()),
               "view<position, velocity> yields exactly the intersection: %d visits, %zu wanted",
               visits, want.size());
    }

    // --- Three components ---
    {
        std::set<std::uint32_t> seen;
        w.view<position, velocity, label>().each([&](entity e, position&, velocity&, label&) {
            seen.insert(e.bits);
        });
        const std::set<std::uint32_t> want = brute_force<position, velocity, label>(w, all);
        checkf(seen == want, "view<position, velocity, label> yields exactly the intersection "
                             "(%zu entities)", want.size());
    }

    // --- RULE 3: THE SMALLEST POOL LEADS ---
    //
    // The claim is not "the answer is right" — the two blocks above already
    // establish that, and a view that walked the biggest pool would also be
    // right. The claim is that the walk is SHORT, and it is checkable because
    // `lead()` and `size_hint()` say which pool was chosen and how many
    // candidates it implies.
    {
        const auto v2 = w.view<position, velocity>();
        checkf(v2.lead() == 1u, "view<position(60), velocity(30)> leads with pool %zu — velocity, "
               "the smaller", v2.lead());
        checkf(v2.size_hint() == 30u, "…so it considers %zu candidates, not 60", v2.size_hint());

        const auto v2r = w.view<velocity, position>();
        check(v2r.lead() == 0u && v2r.size_hint() == 30u,
              "…and naming them the other way round picks the same pool: the decision is by "
              "SIZE, not by argument order");

        const auto v3 = w.view<position, velocity, label>();
        checkf(v3.lead() == 2u && v3.size_hint() == 12u,
               "view over all three leads with label (pool %zu, %zu candidates) — the rarest "
               "component, which is where the skipped work is",
               v3.lead(), v3.size_hint());

        // And the number that makes it worth doing: leading with the big pool
        // would mean touching 60 sparse entries to find 6 matches.
        checkf(v3.size_hint() * 5u == w.storage<position>().size(),
               "leading with position would have considered %zu candidates instead of %zu — "
               "5x the work for the same answer, which is Lesson 5.7's 1.73x vs 1.46x",
               w.storage<position>().size(), v3.size_hint());
    }

    // --- A view over a component type nobody has ---
    {
        int visits = 0;
        w.view<position, heavy>().each([&](position&, heavy&) { ++visits; });
        check(visits == 0, "a view naming a type this world has never used is empty, not a crash");
        check(w.storage_if<heavy>() == nullptr,
              "…and building it did NOT create the pool as a side effect");
    }

    // --- Both callback shapes ---
    {
        int with_entity = 0;
        int without_entity = 0;
        w.view<position, label>().each([&](entity, position&, label&) { ++with_entity; });
        w.view<position, label>().each([&](position&, label&) { ++without_entity; });
        checkf(with_entity == without_entity && with_entity == 12,
               "a callback may take the entity or not; both walk the same %d entities",
               with_entity);
    }

    // --- A one-component view is just the dense array ---
    {
        int n = 0;
        w.view<label>().each([&](label&) { ++n; });
        checkf(n == 12, "view<label> walks the whole pool (%d)", n);
    }

    // --- The view sees changes made through the registry ---
    {
        w.remove<label>(all[0]);
        const auto v = w.view<position, label>();
        checkf(v.size_hint() == 11u,
               "removing one label shortens the very next view's walk to %zu — a view is a "
               "cursor computed on demand, not a cached result", v.size_hint());
    }
}

// ===========================================================================
//  §E — CHURN
// ===========================================================================
//
// Two hundred frames of the operations a real world performs, cross-checked
// against an independent model built out of std::map. Lesson 5.7's equivalent
// section is where the archetype's row-map bug was found, and it was found only
// because the check ran after HUNDREDS of frames — the failure needs the moved
// element to be the element being removed, which is one iteration in N.

void section_e_churn()
{
    std::printf("\n=== E. Churn ===\n");

    engine::ecs::registry w;

    // The shadow model. Nothing in it shares a line of code with the ECS.
    std::map<std::uint32_t, position> want_pos;
    std::map<std::uint32_t, velocity> want_vel;
    std::map<std::uint32_t, label> want_lab;
    std::vector<entity> live;

    std::uint32_t rng = 0xC0FFEEu;
    auto roll = [&rng](std::uint32_t n) {
        rng = rng * 1664525u + 1013904223u;
        return (rng >> 8) % n;
    };

    int created = 0;
    int destroyed = 0;
    int added = 0;
    int removed = 0;

    for (int frame = 0; frame < 200; ++frame)
    {
        // Spawn a few.
        for (std::uint32_t i = 0, n = roll(6); i < n; ++i)
        {
            const entity e = w.create();
            live.push_back(e);
            ++created;

            const position p{static_cast<float>(e.bits & 0xFFu), 0.0f, 0.0f};
            w.add<position>(e, p);
            want_pos[e.bits] = p;
            ++added;

            if (roll(2) == 0)
            {
                const velocity v{0.0f, static_cast<float>(e.index() % 13u), 0.0f};
                w.add<velocity>(e, v);
                want_vel[e.bits] = v;
                ++added;
            }
        }

        if (live.empty()) { continue; }

        // Add or remove a component on a few random entities.
        for (std::uint32_t i = 0, n = roll(5); i < n; ++i)
        {
            const entity e = live[roll(static_cast<std::uint32_t>(live.size()))];
            if (w.has<label>(e))
            {
                w.remove<label>(e);
                want_lab.erase(e.bits);
                ++removed;
            }
            else
            {
                const label l{static_cast<int>(e.index())};
                w.add<label>(e, l);
                want_lab[e.bits] = l;
                ++added;
            }
        }

        // Kill a few.
        for (std::uint32_t i = 0, n = roll(4); i < n && !live.empty(); ++i)
        {
            const std::uint32_t at = roll(static_cast<std::uint32_t>(live.size()));
            const entity e = live[at];
            live[at] = live.back();
            live.pop_back();

            w.destroy(e);
            want_pos.erase(e.bits);
            want_vel.erase(e.bits);
            want_lab.erase(e.bits);
            ++destroyed;
        }
    }

    std::printf("  … %d created, %d destroyed, %d components added, %d removed, %zu live\n",
                created, destroyed, added, removed, live.size());

    checkf(w.size() == live.size(), "the world holds exactly the live entities (%zu vs %zu)",
           w.size(), live.size());
    checkf(w.component_count() == want_pos.size() + want_vel.size() + want_lab.size(),
           "component_count() matches the model: %zu vs %zu", w.component_count(),
           want_pos.size() + want_vel.size() + want_lab.size());

    // Every component the model says exists, exists, with the right value.
    {
        bool ok = true;
        for (const auto& [bits, p] : want_pos)
        {
            const position* got = w.get<position>(entity{bits});
            if (got == nullptr || got->x != p.x) { ok = false; }
        }
        for (const auto& [bits, v] : want_vel)
        {
            const velocity* got = w.get<velocity>(entity{bits});
            if (got == nullptr || got->y != v.y) { ok = false; }
        }
        for (const auto& [bits, l] : want_lab)
        {
            const label* got = w.get<label>(entity{bits});
            if (got == nullptr || got->id != l.id) { ok = false; }
        }
        check(ok, "EVERY component the model holds resolves to the right value after 200 frames "
                  "of swap-and-pop — this is the check that catches a missing sparse patch");
    }

    // And nothing the model does not know about is in there.
    {
        bool ok = true;
        for (entity e : w.storage<position>().entities())
        {
            if (want_pos.find(e.bits) == want_pos.end()) { ok = false; }
        }
        for (entity e : w.storage<label>().entities())
        {
            if (want_lab.find(e.bits) == want_lab.end()) { ok = false; }
        }
        check(ok, "…and no pool holds a row the model does not know about — no leaked "
                  "components from destroyed entities");
    }

    // Every dense entity is alive. A row filed under a dead id is invisible to
    // every query and never erased, so it is a leak with no symptom; this is the
    // check that would see it.
    {
        bool ok = true;
        for (entity e : w.storage<position>().entities())
        {
            if (!w.alive(e)) { ok = false; }
        }
        for (entity e : w.storage<velocity>().entities())
        {
            if (!w.alive(e)) { ok = false; }
        }
        check(ok, "every entity named in a dense array is still alive");
    }

    // The view still agrees with brute force after all that churn.
    {
        std::set<std::uint32_t> seen;
        w.view<position, velocity>().each([&](entity e, position&, velocity&) {
            seen.insert(e.bits);
        });
        std::set<std::uint32_t> want;
        for (const auto& [bits, v] : want_vel)
        {
            (void)v;
            if (want_pos.find(bits) != want_pos.end()) { want.insert(bits); }
        }
        checkf(seen == want, "the view's intersection still matches the model (%zu entities)",
               want.size());
    }

    // The high-water mark. This is the number rule 4 rests on: because the free
    // list reuses slots, the id space is sized by PEAK LIVE, not by the 600-odd
    // entities this loop created.
    checkf(w.entities().slot_count() < static_cast<std::size_t>(created),
           "the id space is %zu slots after creating %d entities — sized by peak live, which "
           "is why the sparse arrays do not need paging (Lesson 5.7 rule 4)",
           w.entities().slot_count(), created);
    checkf(w.entities().generation_wraps() == 0u,
           "no generation wrapped (%zu) — the 12-bit budget is monitored, not assumed",
           w.entities().generation_wraps());
}

// ===========================================================================
//  §F — WHAT IT COSTS
// ===========================================================================

void section_f_memory()
{
    std::printf("\n=== F. Memory ===\n");

    engine::ecs::registry w;
    std::vector<entity> es;
    constexpr int k_n = 10000;

    for (int i = 0; i < k_n; ++i)
    {
        const entity e = w.create();
        es.push_back(e);
        w.add<position>(e, {});
        if (i % 10 == 0) { w.add<label>(e, {i}); }
    }

    const std::size_t pos_sparse = w.storage<position>().sparse_extent();
    const std::size_t lab_sparse = w.storage<label>().sparse_extent();
    const std::size_t lab_rows = w.storage<label>().size();

    checkf(pos_sparse == static_cast<std::size_t>(k_n),
           "the position pool's sparse array is %zu entries for %zu components — one per "
           "entity, dense", pos_sparse, w.storage<position>().size());
    checkf(lab_sparse == static_cast<std::size_t>(k_n) - 9u && lab_rows == 1000u,
           "the LABEL pool's sparse array is also ~%zu entries — for %zu components. "
           "%.0f%% of it means \"absent\", and that is the sparse set's memory story",
           lab_sparse, lab_rows,
           100.0 * (1.0 - static_cast<double>(lab_rows) / static_cast<double>(lab_sparse)));

    std::printf("  … 10,000 entities, 2 component types: %zu KB of sparse index\n",
                (pos_sparse + lab_sparse) * sizeof(std::uint32_t) / 1024u);
    std::printf("  … extrapolated to Lesson 5.7's revisit point (32 types x 1e5): %zu MB\n",
                (32u * 100000u * sizeof(std::uint32_t)) / (1024u * 1024u));

    // Destroy nine tenths and confirm the sparse arrays do NOT shrink — and that
    // the next 9,000 entities reuse those slots rather than extending them.
    for (int i = 0; i < k_n; ++i)
    {
        if (i % 10 != 0) { w.destroy(es[static_cast<std::size_t>(i)]); }
    }
    checkf(w.size() == 1000u, "1,000 entities left (%zu)", w.size());
    check(w.entities().slot_count() == static_cast<std::size_t>(k_n),
          "the id space did not shrink — a high-water mark never falls");

    for (int i = 0; i < 9000; ++i) { (void)w.create(); }
    checkf(w.entities().slot_count() == static_cast<std::size_t>(k_n),
           "…and 9,000 fresh entities reused the freed slots rather than growing it (%zu)",
           w.entities().slot_count());
}

// ===========================================================================
//  §G — THE GOLDEN
// ===========================================================================

void section_g_golden()
{
    std::printf("\n=== G. The golden ===\n");

    const char* path = "build/demos/verify_58.ppm";
    check(demo::write_reference_shot(path) == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file(path);
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(), "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL to the golden written in Lesson 5.1 — EIGHT lessons now. The ECS is "
          "purely additive: nothing renders through it yet, so the reference picture cannot "
          "have moved, and if it had we would know the change was not where we thought");
}

}   // namespace

int main()
{
    std::printf("verify_58 — Lesson 5.8: the ECS runtime\n");
    std::printf("(debug assertions %s)\n",
                engine::debug_assertions_enabled() ? "ON" : "OFF");

    section_a_ids();
    section_b_pool();
    section_c_registry();
    section_d_views();
    section_e_churn();
    section_f_memory();
    section_g_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
