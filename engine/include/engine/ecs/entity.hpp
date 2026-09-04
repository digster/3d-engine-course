// engine/include/engine/ecs/entity.hpp — the id every pool is keyed by.
//
// Lesson 5.8. Lesson 5.7 chose the sparse set by measurement and left four rules
// behind; this file is RULE 2, and it is the only thing the engine did not
// already own.
//
//     ONE SHARED ID SPACE.
//
// That sentence is the entire difference between what `core/pool.hpp` has been
// doing since Lesson 5.4 and what an ECS does. Read 5.4's pool again with the
// right eyes and it IS a sparse set — `slots_` is the sparse array, `items_` is
// the dense data, `owners_` is the dense-to-sparse way back. It has generations,
// it has swap-and-pop removal, it has O(1) everything. What it does not have is
// an id anyone else can use:
//
//     mesh_pool.insert(...)     ->  handle<mesh_data>{ index 7, generation 1 }
//     texture_pool.insert(...)  ->  handle<texture>  { index 7, generation 1 }
//
// Two sevens that mean nothing to each other, because each pool mints its own
// keys off its own free list. Ask "does the thing in mesh slot 7 also have a
// texture?" and there is no answer — not a slow answer, no answer, because the
// question is not well formed. An ECS is exactly the arrangement in which it IS:
// the id is minted ONCE, by the allocator below, and every component pool is
// then a map from that id to a value.
//
// So this file mints ids and nothing else. It stores no components, knows about
// no pools, and could not tell you whether an entity has a transform. It knows
// which ids are live, which are dead, and — this is the part that has to be got
// right — which dead ones are safe to hand out again.
//
// WHY NOT JUST REUSE handle<T>?
//
// It was the first thing tried, and the reason it is not here is worth a minute,
// because it is not "handles are wrong". Everything `handle<T>` argues in
// core/handle.hpp is argued again here: an index plus a generation, twenty bits
// and twelve, generation zero reserved so the null value is all-bits-zero, the
// generation bumped on removal so a stale id can never match. Those constants are
// literally included from that header rather than restated, so the bit budget has
// one home.
//
// What differs is what the type MEANS, and a type that means something different
// should be a different type:
//
//     handle<mesh_data>   names an item IN a pool<mesh_data>.  One container.
//     entity              names a row ACROSS every pool there is. No container
//                         owns it, and the thing it names may not exist anywhere.
//
// Making `entity` an alias for `handle<entity_tag>` would have compiled, and then
// `pool<T>::get(handle<T>)` and a component lookup would be spelled the same way
// while meaning opposite things. The phantom-parameter trick in handle.hpp exists
// precisely so that two ids which must not mix cannot mix; using it to make them
// mix would be an odd way to spend it.
//
// See docs/lessons/05-08-ecs-runtime.html for the derivation.

#pragma once

#include <engine/core/assert.hpp>
#include <engine/core/handle.hpp>
#include <engine/core/log.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

/// The entity-component-system runtime.
///
/// **A nested namespace, and one name in it is deliberately shadowed.**
/// `engine::ecs::pool<T>` (ecs/pool.hpp) and `engine::pool<T>` (core/pool.hpp)
/// are different containers with the same name, and that is not an oversight —
/// they are the same *shape* used for opposite jobs, and calling one of them
/// something else would hide the relationship this module spends a lesson
/// drawing out. The nesting keeps them apart: inside `engine::ecs`, plain `pool`
/// is this one; from outside, both spellings are explicit. The one arrangement
/// that breaks is `using namespace engine; using namespace engine::ecs;` in the
/// same scope, which makes bare `pool` ambiguous — and an ambiguity the compiler
/// reports is the good kind.
namespace engine::ecs
{

// ---- The id ----------------------------------------------------------------

/// A thing in the world: an index, a generation, and no data whatsoever.
///
/// Four bytes. Trivially copyable, comparable, hashable by its `bits`, and
/// meaningful only in the `registry` that minted it. An entity is not an object
/// and does not own its components — it is the *key* under which components are
/// filed, and a world in which nothing has been added to it is a perfectly
/// legitimate live entity with nothing in it.
///
/// The bit layout is `handle<T>`'s, from `core/handle.hpp`, for the reasons
/// argued there and not restated here: 20 bits of index (1,048,576 slots), 12 of
/// generation (4,095 usable, generation 0 reserved so a value-initialised
/// `entity` is null for free).
struct entity
{
    /// Generation in the high 12 bits, index in the low 20. Public because an
    /// entity is a value, and because serialization (Module 8) wants this word.
    std::uint32_t bits = 0;

    /// Which slot of the allocator this entity occupies. Meaningless when
    /// `!valid()`, and — this is the one that matters — it is also the index
    /// into every component pool's sparse array.
    [[nodiscard]] constexpr std::uint32_t index() const { return bits & k_handle_index_mask; }

    /// Which occupant of that slot. Zero means "no occupant, ever".
    [[nodiscard]] constexpr std::uint32_t generation() const { return bits >> k_handle_index_bits; }

    /// Is this anything at all?
    ///
    /// **Not the same question as `registry::alive()`.** A well-formed entity can
    /// name something destroyed three seconds ago; only the allocator knows. The
    /// names differ on purpose — see the identical warning in `handle::valid()`.
    [[nodiscard]] constexpr bool valid() const { return generation() != 0; }

    /// `if (e)` — the same question as `valid()`. `explicit`, so an entity cannot
    /// decay into an integer in arithmetic.
    [[nodiscard]] explicit constexpr operator bool() const { return valid(); }

    /// Two entities are equal when they are the same occupant of the same slot.
    /// One 32-bit compare, which is what makes the pool's staleness check free.
    [[nodiscard]] friend constexpr bool operator==(entity, entity) = default;
};

static_assert(sizeof(entity) == 4, "an entity is one 32-bit word");

/// The entity that names nothing. All bits zero, so this is also what you get
/// from `entity{}`, from a default-constructed vector element, and from a
/// `memset` — see `k_handle_first_generation` for why that is free.
inline constexpr entity null_entity{};

/// Assemble an entity from its two fields.
///
/// Normally only `entity_allocator` calls this. Tests call it to fabricate stale
/// or foreign ids on purpose, which is the only honest way to check that the
/// staleness machinery works — a test that cannot construct a bad id cannot prove
/// the good ones are being told apart.
[[nodiscard]] constexpr entity make_entity(std::uint32_t index, std::uint32_t generation)
{
    return entity{((generation & k_handle_max_generation) << k_handle_index_bits)
                  | (index & k_handle_index_mask)};
}

// ---- The allocator ---------------------------------------------------------

/// Mints entity ids, retires them, and answers "is this one still live?".
///
/// This is `core/pool.hpp`'s slot machinery with the payload removed. There is no
/// `items_` here and no `owners_`, because an entity has no value — the value is
/// spread across however many component pools happen to hold a row for it, and
/// this class deliberately knows about none of them. `registry` is what ties the
/// two halves together; keeping them apart is what makes each testable on its own.
///
/// Complexity: `create`, `destroy` and `alive` are O(1) with no allocation once
/// the slot vector has reached its high-water mark.
///
/// **Not thread-safe**, exactly as `engine::pool` is not. Module 8's job system
/// revisits every shared container in the engine at once.
class entity_allocator
{
public:
    // ---- Minting and retiring ---------------------------------------------

    /// Hand out an id, reusing a retired slot when one is available.
    ///
    /// Reuse is LIFO — the most recently destroyed slot comes back first — which
    /// is `engine::pool`'s policy for `engine::pool`'s reason: the warmest slot
    /// is the one just touched. It also has a consequence that Lesson 5.7 leaned
    /// on when it ruled out paged sparse arrays: because a freed slot is reused
    /// before a new one is created, the id space's high-water mark is *peak live
    /// entities*, not *entities ever created*. A game that spawns and kills a
    /// thousand bullets a second for an hour still has an id space the size of
    /// the most bullets that were ever alive at one instant, and every component
    /// pool's sparse array is sized by that number.
    ///
    /// Returns `null_entity` if the id space is exhausted at 1,048,576 live
    /// entities. That is an ERROR rather than an assertion by Lesson 5.3's test:
    /// a correct program on a working machine CAN meet it, by loading a scene
    /// with too much in it, so it is the world's doing and it ships.
    [[nodiscard]] entity create()
    {
        std::uint32_t slot_index = 0;

        if (!free_slots_.empty())
        {
            // The generation was bumped on the way out, so the slot already
            // carries a value no outstanding id can match. Nothing to do here
            // but take it — see `destroy` for the other half of the argument.
            slot_index = free_slots_.back();
            free_slots_.pop_back();
        }
        else
        {
            if (slots_.size() > k_handle_max_index)
            {
                ENGINE_LOG_ERROR(log_core, "ecs: out of entity ids (%u max live)",
                                 k_handle_max_index + 1u);
                return null_entity;
            }
            slot_index = static_cast<std::uint32_t>(slots_.size());
            slots_.push_back(slot{});
        }

        slots_[slot_index].live = true;
        ++live_;
        return make_entity(slot_index, slots_[slot_index].generation);
    }

    /// Retire `e`. Returns false if it was already null or stale.
    ///
    /// **This does not touch a single component**, and it cannot — the allocator
    /// has never heard of a component. `registry::destroy` is the operation that
    /// erases an entity's rows from every pool and then calls this; calling this
    /// one directly on an entity that still has components leaves those rows
    /// filed under an id nothing will ever look up again, which is a leak with no
    /// symptom. The registry is the public door for exactly that reason.
    ///
    /// The generation is bumped HERE, on removal, never on creation. That is the
    /// rule `core/handle.hpp` derives: a retired slot immediately holds a value
    /// no outstanding id carries, so "is this slot free?" needs no separate
    /// question and no window exists in which a stale id matches.
    bool destroy(entity e)
    {
        if (!alive(e)) { return false; }

        slot& s = slots_[e.index()];
        s.live = false;

        std::uint32_t next = s.generation + 1u;
        if (next > k_handle_max_generation)
        {
            // 4,095 reuses of this one slot. From here, ids from the first cycle
            // become indistinguishable from fresh ones. Counted rather than
            // assumed away — `core/handle.hpp` does the arithmetic (68 seconds at
            // one recycle per frame) and names the two fixes, and entities are
            // the case that arithmetic was worried about.
            next = k_handle_first_generation;
            ++wraps_;
            ENGINE_LOG_WARN(log_core, "ecs: entity generation wrapped on slot %u (wrap #%zu)",
                            e.index(), wraps_);
        }
        s.generation = next;

        free_slots_.push_back(e.index());
        --live_;
        return true;
    }

    /// Retire every id. Generations are KEPT, so every outstanding entity goes
    /// stale rather than silently starting to resolve again — the same argument
    /// `engine::pool::clear()` makes, for the same reason.
    void clear()
    {
        for (std::uint32_t i = 0; i < slots_.size(); ++i)
        {
            if (!slots_[i].live) { continue; }
            slots_[i].live = false;
            std::uint32_t next = slots_[i].generation + 1u;
            if (next > k_handle_max_generation) { next = k_handle_first_generation; ++wraps_; }
            slots_[i].generation = next;
            free_slots_.push_back(i);
        }
        live_ = 0;
    }

    // ---- Asking -----------------------------------------------------------

    /// Is `e` a live id in THIS allocator?
    ///
    /// Three tests, one per failure mode from Lesson 5.4 §2. The range test
    /// rejects a foreign or fabricated id; the `live` flag rejects a slot that is
    /// currently retired; the generation test rejects the interesting one — a
    /// slot that is live again with a *different* occupant. A null entity fails
    /// the generation test everywhere, since no live slot ever carries zero.
    [[nodiscard]] bool alive(entity e) const
    {
        if (e.index() >= slots_.size()) { return false; }
        const slot& s = slots_[e.index()];
        return s.live && s.generation == e.generation();
    }

    /// How many ids are live right now.
    [[nodiscard]] std::size_t size() const { return live_; }

    [[nodiscard]] bool empty() const { return live_ == 0; }

    /// How many slots have ever existed — the high-water mark of peak live
    /// entities, and therefore the length every component pool's sparse array
    /// grows to. This is the number Lesson 5.7 §5.4 budgeted against when it
    /// ruled that paging the sparse arrays can wait.
    [[nodiscard]] std::size_t slot_count() const { return slots_.size(); }

    /// Slots retired and awaiting reuse.
    [[nodiscard]] std::size_t free_count() const { return free_slots_.size(); }

    /// How many times a slot's generation has run past 4,095. Zero forever in a
    /// healthy program; if it is not, `core/handle.hpp`'s bit budget needs
    /// revisiting for this world's churn rate.
    [[nodiscard]] std::size_t generation_wraps() const { return wraps_; }

    /// The generation slot `i` currently carries — diagnostics, and tests that
    /// need to fabricate a stale id deliberately. Zero for an out-of-range slot,
    /// which is never a live generation.
    [[nodiscard]] std::uint32_t generation_of(std::uint32_t i) const
    {
        return i < slots_.size() ? slots_[i].generation : 0u;
    }

private:
    /// One slot: the generation it currently carries, and whether it is occupied.
    ///
    /// **Eight bytes for five bits of information, and that is a choice with an
    /// arithmetic behind it.** The generation needs 12 bits and `live` needs one,
    /// so a packed slot fits in two bytes and this one wastes six. At the 10,000
    /// peak entities Lesson 5.7 budgeted for, the difference is 80 KB against
    /// 20 KB — both of which vanish beside the 400 KB of component sparse arrays
    /// that the same 10,000 entities imply across ten component types. Packing
    /// this would buy 0.4 KB per thousand entities and cost every reader of this
    /// file a shift and a mask. If a profile ever disagrees, the two fields are
    /// private and the change is local to this class.
    struct slot
    {
        std::uint32_t generation = k_handle_first_generation;
        bool live = false;
    };

    std::vector<slot> slots_;                ///< indexed by an entity's index field
    std::vector<std::uint32_t> free_slots_;  ///< retired slots awaiting reuse, LIFO
    std::size_t live_ = 0;
    std::size_t wraps_ = 0;
};

}   // namespace engine::ecs
