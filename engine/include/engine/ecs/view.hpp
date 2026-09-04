// engine/include/engine/ecs/view.hpp — the query: every entity that has all of these.
//
// Lesson 5.8. A view is the ECS's answer to a `for` loop over a scene graph, and
// it is where Lesson 5.7's RULE 3 lives:
//
//     A VIEW LEADS WITH THE SMALLEST POOL.
//
// That is one comparison, made once, in the constructor — and it is the single
// cheapest correctness-of-design decision in this module. Lesson 5.7 measured it
// at one-in-four selectivity and 100,000 entities: leading with the big pool cost
// 1.73× a real archetype, leading with the small one cost 1.46×, and the code is
// identical apart from which array the outer loop walks. The rarer the component,
// the larger the gap, because the work you skip is proportional to the candidates
// you never look at.
//
// THE SHAPE OF THE LOOP, and why it is the shape it is:
//
//     for each entity e in THE SMALLEST POOL's dense entity list      <- rule 3
//         for each other pool, ask "do you have e?"                   <- O(1) each
//         if all of them said yes, call f(e, components...)
//
// Every part of that is a consequence of what a sparse set is. The outer walk is
// dense and sequential, because that is what a pool's `dense_` array is. The
// inner tests are O(1) array reads, because that is what a sparse array is. And
// the choice of which pool leads is free, because — the small thing that makes it
// possible — every pool can hand back a `std::span<const entity>` regardless of
// what it stores, so the constructor compares four spans' lengths without ever
// caring what is in them.
//
// WHAT IS NOT HERE: GROUPS. Lesson 5.7 measured a group — two pools sorted to
// agree on dense order, so the query reads no sparse entry at all — at 0.99× a
// real archetype on the archetype's own best case, and that measurement is the
// reason this course chose the sparse set at all: it means the migration only
// ever runs one way. Building one NOW would be optimising before there is a
// profile. The hole is named rather than hidden: see §9 of the lesson and
// §5.5 of Lesson 5.7 for exactly what would go here and what it would cost.

#pragma once

#include <engine/core/assert.hpp>
#include <engine/ecs/pool.hpp>

#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace engine::ecs
{

/// Every entity that has all of `Ts`, and its components.
///
/// Constructed by `registry::view<Ts...>()`; there is rarely a reason to build
/// one by hand. A view is a *cursor*, not a container — it holds pointers to
/// pools and computes the answer as it walks, so it is cheap to create, cheap to
/// discard, and must not outlive the registry it came from.
///
/// **The iteration rule, and it is the one hazard in this file.** Do not add or
/// erase components of any type this view names while iterating it. The outer
/// walk is over the lead pool's dense array by position, and both `insert` and
/// `erase` move elements of that array — so a callback that erases the entity it
/// was just handed will skip whichever entity got swapped into its place, and one
/// that inserts may invalidate the array outright. In a debug build the check
/// below catches the first case immediately; in release it is silent, exactly as
/// mutating a `std::vector` while ranging over it is silent. The safe patterns are
/// the same as for a vector: collect the entities you want to change into a small
/// buffer and act after the loop, or make the change through a deferred command
/// list (Module 8 builds one for the job system).
template <typename... Ts>
class view
{
    static_assert(sizeof...(Ts) >= 1, "a view names at least one component type");

public:
    /// Build a view over these pools. A null pool means the component type has
    /// never been used in this world, so nothing can have it, and the view is
    /// empty — which is the right answer rather than a special case.
    explicit view(pool<Ts>*... pools)
        : pools_{pools...}
    {
        if (((pools == nullptr) || ...)) { return; }

        // RULE 3, in five lines. `entities()` returns the same type for every
        // pool no matter what it stores, so the sizes can be compared in a plain
        // loop — no dispatch, no metaprogramming, no runtime type information.
        const std::span<const entity> spans[] = {pools->entities()...};
        const pool_base* bases[] = {static_cast<const pool_base*>(pools)...};

        for (std::size_t i = 1; i < sizeof...(Ts); ++i)
        {
            if (spans[i].size() < spans[lead_].size()) { lead_ = i; }
        }
        lead_pool_ = bases[lead_];
    }

    // ---- Asking ------------------------------------------------------------

    /// Which of `Ts...`, by position, this view will walk. Zero-based.
    ///
    /// Exposed for tests and for the debug UI, not because a caller should care.
    /// It is here because "the smallest pool leads" is a claim about behaviour,
    /// and a claim about behaviour that nothing can observe is a claim nothing can
    /// check — `verify_58` §D reads this and would fail the day the constructor
    /// stopped choosing.
    [[nodiscard]] std::size_t lead() const { return lead_; }

    /// How many entities the walk will CONSIDER — the size of the lead pool.
    ///
    /// An upper bound on how many it will yield, never the answer itself, because
    /// only the intersection is the answer and computing it means doing the walk.
    /// This number is precisely what rule 3 minimises.
    [[nodiscard]] std::size_t size_hint() const
    {
        return lead_pool_ != nullptr ? lead_pool_->entities().size() : 0u;
    }

    [[nodiscard]] bool empty() const { return size_hint() == 0u; }

    // ---- Walking -----------------------------------------------------------

    /// Call `f` for every entity that has all of `Ts`.
    ///
    /// `f` may take either shape, and which one you get is decided at compile
    /// time by `std::is_invocable_v`:
    ///
    ///     v.each([](engine::ecs::entity e, transform& t, velocity& vel) { ... });
    ///     v.each([](transform& t, velocity& vel) { ... });
    ///
    /// The second exists because most systems never use the id, and a parameter
    /// that is always `(void)`-cast is a parameter that should not have been
    /// there. Both compile to the same loop.
    template <typename F>
    void each(F&& f) const
    {
        if (lead_pool_ == nullptr) { return; }
        each_impl(f, std::index_sequence_for<Ts...>{});
    }

private:
    template <typename F, std::size_t... I>
    void each_impl(F& f, std::index_sequence<I...>) const
    {
        // Re-read the span here rather than caching it at construction. A cached
        // span would dangle the moment anything inserted into the lead pool
        // between building the view and walking it — a bug that would reproduce
        // only when a vector happened to reallocate, which is the worst possible
        // schedule for finding it. One virtual call per walk buys the whole class
        // of problem away, and it is one call per walk, not per entity.
        const std::span<const entity> lead = lead_pool_->entities();

        for (std::size_t i = 0; i < lead.size(); ++i)
        {
            const entity e = lead[i];

            // Short-circuit `&&` over the pack: the lead pool is skipped (it
            // contains `e` by construction — that is where `e` came from), and
            // the first pool that says no stops the rest from being asked.
            if (!(... && (I == lead_ || std::get<I>(pools_)->contains(e)))) { continue; }

            if constexpr (std::is_invocable_v<F&, entity, Ts&...>)
            {
                f(e, fetch<I>(e, i)...);
            }
            else
            {
                f(fetch<I>(e, i)...);
            }
        }
    }

    /// The `I`th component of `e`, which is at dense position `i` when pool `I`
    /// is the lead.
    ///
    /// **That shortcut is where the lead pool earns its keep twice.** For the
    /// lead, the dense position is the loop counter — no sparse read, no random
    /// access, just the next element of an array we are already walking. For
    /// every other pool it is a sparse lookup, which is the two-deep dependent
    /// chain Lesson 5.7 spent a figure on. `I == lead_` compares a compile-time
    /// constant against a value fixed for the whole loop, so the branch predicts
    /// perfectly after the first iteration.
    template <std::size_t I>
    [[nodiscard]] auto& fetch(entity e, std::size_t i) const
    {
        auto* p = std::get<I>(pools_);
        if (I == lead_)
        {
            // If this ever fires, something mutated the lead pool during the
            // walk and the dense positions have shifted under us — see the
            // iteration rule in this class's documentation.
            ENGINE_ASSERT_MSG(p->entity_at(i) == e, log_core,
                              "ecs::view: lead pool changed during iteration "
                              "(slot %zu now holds entity %u, expected %u)",
                              i, p->entity_at(i).index(), e.index());
            return p->at(i);
        }
        return *p->get(e);
    }

    std::tuple<pool<Ts>*...> pools_;
    const pool_base* lead_pool_ = nullptr;
    std::size_t lead_ = 0;
};

}   // namespace engine::ecs
