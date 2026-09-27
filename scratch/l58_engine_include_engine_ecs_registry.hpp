// engine/include/engine/ecs/registry.hpp — the world: one id space, N component pools.
//
// Lesson 5.8. `entity.hpp` mints ids and knows nothing about components;
// `pool.hpp` stores components and knows nothing about which ids exist. Neither
// is usable alone, and that is deliberate — each is testable without the other.
// This file is the twenty lines that make them one thing:
//
//     registry
//       ├── entity_allocator          which ids are live
//       └── vector<unique_ptr<pool_base>>   one pool per component TYPE,
//                                           indexed by that type's component id
//
// THE ONE HARD PROBLEM IN THIS FILE is that vector. It holds pools of different
// types — `pool<transform>`, `pool<velocity>`, `pool<material>` — and it must be
// able to say "erase this entity from all of you" without knowing what any of
// them stores. That is type erasure, and C++ offers two ways to do it:
//
//   1. RTTI: store `std::type_index` keys, recover types with `dynamic_cast`.
//      NOT AVAILABLE — the engine core is built with RTTI off (Lesson 5.1), and
//      that was not a decision made in order to be difficult: `dynamic_cast` is
//      a runtime type-graph walk, and a lookup keyed by `type_index` is a hash of
//      a string-ish key on a path that should be an array index.
//   2. A number we assign ourselves. `component_id_of<T>()` hands each component
//      type a small integer the first time it is asked, that integer indexes the
//      vector, and the concrete type is recovered with `static_cast` — safe
//      because the id is what CREATED the pool, so the type at that slot was
//      established by construction rather than guessed at use.
//
// The second is what is below, it is what every production ECS does, and its
// entire cost is one function-local static per component type.

#pragma once

#include <engine/core/assert.hpp>
#include <engine/ecs/entity.hpp>
#include <engine/ecs/pool.hpp>
#include <engine/ecs/view.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace engine::ecs
{

/// A component type's index into the registry's pool vector.
using component_id = std::uint32_t;

namespace detail
{

/// The counter behind `component_id_of`. One per PROGRAM, not one per registry —
/// see the note there.
[[nodiscard]] inline component_id& component_id_counter()
{
    static component_id next = 0;
    return next;
}

}   // namespace detail

/// The id of component type `T`, assigned on first use and stable thereafter.
///
/// **The ids are global to the program, not to a registry**, and that is worth
/// stating because it has one visible consequence: a registry that uses only the
/// tenth type ever registered still allocates a pool vector eleven entries long,
/// ten of which are null pointers. Eighty wasted bytes, once, per registry — and
/// in exchange the lookup is `pools_[id]` rather than a hash. The alternative
/// (per-registry ids) needs a map from type to id inside every registry, which
/// puts a hash on the path this design exists to keep as an array index.
///
/// The `static` initialiser inside a function template is one object across every
/// translation unit — the language guarantees that, and it is what makes the id
/// the same number in the demo and in the test harness. It is initialised on
/// first call, thread-safely (C++11 magic statics), but the counter increment
/// itself is not atomic: **register your component types from one thread**, which
/// in practice means "touch each type once before the job system starts", and is
/// the same constraint every container in this engine carries until Module 9.
template <typename T>
[[nodiscard]] inline component_id component_id_of()
{
    static const component_id id = detail::component_id_counter()++;
    return id;
}

/// The world. Creates entities, attaches components, answers queries.
///
/// This is the public face of the ECS and the only class most code should touch.
/// It is a plain value: construct one on the stack, hold several at once, throw
/// one away. **Not a singleton**, for the reason Lesson 5.5's asset store was not
/// one — the verification harness holds four registries in a single function, and
/// a design that cannot do that is a design that cannot be tested.
///
/// **Not thread-safe.**
class registry
{
public:
    registry() = default;
    ~registry() = default;

    registry(const registry&) = delete;
    registry& operator=(const registry&) = delete;
    registry(registry&&) = default;
    registry& operator=(registry&&) = default;

    // ---- Entities ----------------------------------------------------------

    /// Mint an entity with no components. Null if the id space is exhausted.
    [[nodiscard]] entity create() { return entities_.create(); }

    /// Destroy `e` and every component filed under it. False if `e` was already
    /// null or stale.
    ///
    /// **This is the operation the type erasure exists for.** Erasing an entity
    /// means visiting every pool in the world, because the registry has no record
    /// of which components a given entity holds — that record would be a second
    /// source of truth, and the pools are the first. The cost is one virtual call
    /// per component TYPE, not per component: a world with ten types pays ten
    /// calls to destroy an entity that had two components, and eight of those
    /// return immediately from a single array read. That is the price of not
    /// keeping a per-entity component mask, and at ten types it is nothing; an
    /// engine with three hundred component types would keep the mask and Lesson
    /// 5.7 §9 says so.
    bool destroy(entity e)
    {
        if (!entities_.alive(e)) { return false; }
        for (std::unique_ptr<pool_base>& p : pools_)
        {
            if (p) { p->erase(e); }
        }
        return entities_.destroy(e);
    }

    /// Is `e` a live entity of this world?
    [[nodiscard]] bool alive(entity e) const { return entities_.alive(e); }

    /// How many entities are live.
    [[nodiscard]] std::size_t size() const { return entities_.size(); }

    [[nodiscard]] bool empty() const { return entities_.empty(); }

    /// Destroy everything: all components, then all ids. Generations are kept, so
    /// every entity anyone is still holding goes stale rather than starting to
    /// resolve to a new occupant.
    void clear()
    {
        for (std::unique_ptr<pool_base>& p : pools_)
        {
            if (p) { p->clear(); }
        }
        entities_.clear();
    }

    /// The allocator, for diagnostics — slot counts, free counts, generation
    /// wraps. Const, because minting an id behind the registry's back would file
    /// components under an entity the registry does not know it has.
    [[nodiscard]] const entity_allocator& entities() const { return entities_; }

    // ---- Components --------------------------------------------------------

    /// Attach a `T` to `e`. Returns a pointer to it, or nullptr if `e` is not
    /// alive.
    ///
    /// Adding to a dead entity asserts in a debug build and is REFUSED in a
    /// release one. Refusing rather than filing it is the important half: a
    /// component filed under a dead id is invisible to every query (nothing will
    /// ever name that id again) and is never erased (`destroy` already ran), so
    /// it is a leak whose only symptom is a pool that keeps growing.
    template <typename T>
    T* add(entity e, T value)
    {
        ENGINE_ASSERT_MSG(entities_.alive(e), log_core,
                          "ecs::registry::add: entity %u is not alive", e.index());
        if (!entities_.alive(e)) { return nullptr; }
        return storage<T>().insert(e, std::move(value));
    }

    /// Detach `e`'s `T`. False if it did not have one, or is not alive.
    template <typename T>
    bool remove(entity e)
    {
        if (!entities_.alive(e)) { return false; }
        pool<T>* p = storage_if<T>();
        return p != nullptr && p->erase(e);
    }

    /// Does `e` have a `T`?
    ///
    /// Note the deliberate absence of an aliveness check: a stale entity fails
    /// this anyway, because `pool<T>::contains` compares the full entity word and
    /// a stale id never matches a live row. Adding an `alive()` call would be a
    /// second, slower way to reach the same answer on the hottest path there is.
    template <typename T>
    [[nodiscard]] bool has(entity e) const
    {
        const pool<T>* p = storage_if<T>();
        return p != nullptr && p->contains(e);
    }

    /// `e`'s `T`, or nullptr if it has none.
    ///
    /// **Valid until the next change to that pool, and not one instruction
    /// longer** — Lesson 5.4's rule, unchanged: resolve late, use immediately,
    /// never store.
    template <typename T>
    [[nodiscard]] T* get(entity e)
    {
        pool<T>* p = storage_if<T>();
        return p != nullptr ? p->get(e) : nullptr;
    }

    /// Const overload; same rules.
    template <typename T>
    [[nodiscard]] const T* get(entity e) const
    {
        const pool<T>* p = storage_if<T>();
        return p != nullptr ? p->get(e) : nullptr;
    }

    // ---- Storage and queries -----------------------------------------------

    /// The pool for `T`, created if this world has never seen the type.
    ///
    /// Public because a system that walks one component type wants the dense
    /// array directly — `for (velocity& v : world.storage<velocity>().components())`
    /// is the fastest loop this design can produce, and hiding it behind a
    /// one-type view would only add a layer. It is also how a test reaches
    /// `sparse_extent()` to check the memory story.
    template <typename T>
    [[nodiscard]] pool<T>& storage()
    {
        const component_id id = component_id_of<T>();
        if (id >= pools_.size()) { pools_.resize(id + 1u); }
        if (!pools_[id]) { pools_[id] = std::make_unique<pool<T>>(); }

        // Safe because `id` is what created this slot: the pool at index
        // `component_id_of<T>()` was constructed as a `pool<T>` and nothing else
        // can ever be stored there. This is the `static_cast` the RTTI-free
        // design buys — a downcast justified by construction rather than checked
        // at runtime.
        return *static_cast<pool<T>*>(pools_[id].get());
    }

    /// The pool for `T`, or nullptr if this world has never used the type.
    ///
    /// The non-creating half of `storage()`, and every read path uses it. A query
    /// for a component nobody has ever attached must not allocate a pool as a
    /// side effect of asking — that would make `has<T>()` a mutating operation and
    /// would grow the pool vector by simply looking.
    template <typename T>
    [[nodiscard]] pool<T>* storage_if()
    {
        const component_id id = component_id_of<T>();
        if (id >= pools_.size() || !pools_[id]) { return nullptr; }
        return static_cast<pool<T>*>(pools_[id].get());
    }

    /// Const overload.
    template <typename T>
    [[nodiscard]] const pool<T>* storage_if() const
    {
        const component_id id = component_id_of<T>();
        if (id >= pools_.size() || !pools_[id]) { return nullptr; }
        return static_cast<const pool<T>*>(pools_[id].get());
    }

    /// Every entity that has all of `Ts`.
    ///
    ///     world.view<transform, velocity>().each([](transform& t, velocity& v) {
    ///         t.position += v.linear;
    ///     });
    ///
    /// Uses `storage_if`, so a view naming a type nobody has ever attached is
    /// empty rather than a pool-creating side effect. The view picks the smallest
    /// of the named pools to lead with — Lesson 5.7's rule 3, implemented in
    /// `view.hpp`'s constructor.
    template <typename... Ts>
    [[nodiscard]] engine::ecs::view<Ts...> view()
    {
        return engine::ecs::view<Ts...>{storage_if<Ts>()...};
    }

    /// How many component types this world has actually created a pool for.
    /// Diagnostics: the gap between this and the global id counter is how many
    /// types some *other* registry in the program uses.
    [[nodiscard]] std::size_t pool_count() const
    {
        std::size_t n = 0;
        for (const std::unique_ptr<pool_base>& p : pools_)
        {
            if (p) { ++n; }
        }
        return n;
    }

    /// How many components are stored across every pool — the world's total row
    /// count, which is what a memory report wants.
    [[nodiscard]] std::size_t component_count() const
    {
        std::size_t n = 0;
        for (const std::unique_ptr<pool_base>& p : pools_)
        {
            if (p) { n += p->size(); }
        }
        return n;
    }

private:
    entity_allocator entities_;

    /// Indexed by `component_id_of<T>()`. Sparse — a null entry is a type this
    /// world has never used — and never shrinks, because the ids never move.
    std::vector<std::unique_ptr<pool_base>> pools_;
};

}   // namespace engine::ecs
