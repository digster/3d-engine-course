// engine/include/engine/core/pool.hpp — storage that hands out handles instead of pointers.
//
// Lesson 5.4. `core/handle.hpp` argues why a reference into an engine should be
// an index plus a generation; this file is the container that issues them and is
// the only thing in the program allowed to turn one back into an object.
//
// THE SHAPE, in one picture. Two arrays and a free list:
//
//     slots_    [ gen 3, dense 1 ][ gen 1, dense - ][ gen 2, dense 0 ]  ...
//                     |                                    |
//                     +---------------+                    |
//                                     v                    v
//     items_    [ the mesh in slot 2 ][ the mesh in slot 0 ]
//     owners_   [        2           ][        0           ]
//
//   - `slots_` is SPARSE and STABLE. One entry per slot ever created, indexed by
//     a handle's index field, and entries never move. It holds the generation
//     (which decides staleness) and where the item currently lives.
//   - `items_` is DENSE and MOBILE. Live objects only, packed with no holes, in
//     whatever order removals have left them. It is a plain `std::vector`, so it
//     reallocates when it grows and every element moves — and that is a FEATURE
//     here, not a hazard, because nothing outside this class holds an address.
//   - `owners_` is the way back: `owners_[d]` is the slot that `items_[d]`
//     belongs to. Removal needs it, and it is the only reason removal can be
//     O(1) instead of O(n).
//
// WHY DENSE, when a simple "one item per slot" array is fifteen lines shorter?
// Two reasons, and neither is elegance:
//
//   1. ITERATION. `items()` hands back a contiguous span of exactly the live
//      objects. A slot-indexed array is full of holes; walking it means testing
//      every slot and touching cache lines belonging to nothing. Module 5's ECS
//      lives or dies on that distinction and Lesson 5.6 measures it.
//   2. THE PROOF. Removing an item MOVES the last item into its place, which
//      means an object's address changes underneath a handle that keeps working.
//      If a handle survived only because nothing ever moved, it would be a
//      pointer with extra steps. Here it demonstrably is not.
//
// WHAT THIS IS NOT. It is not thread-safe (Module 9's job system revisits every
// shared container). It does not sort, and the order of `items()` is
// deliberately unspecified — depending on it is depending on the history of your
// removals. And `get()` returns a raw pointer, which is a real hazard with a
// simple rule: RESOLVE LATE, USE IMMEDIATELY, NEVER STORE. Any insert or remove
// may move every item; the handle survives that and a pointer does not, which is
// the whole reason this file exists.

#pragma once

#include <engine/core/handle.hpp>
#include <engine/core/log.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace engine {

/// A generational slot map: `insert` gives you a handle, `get` gives it back.
///
/// `T` must be default-constructible (removal assigns a default-constructed `T`
/// over the vacated element so its memory is released with it) and movable.
/// `mesh_data` is both, as are all four of the resource types Module 5 stores.
///
/// Complexity: `insert`, `remove`, `get` and `contains` are all O(1), with no
/// allocation at all once the vectors have grown to their high-water mark.
template <typename T>
class pool
{
public:
    using value_type = T;
    using handle_type = handle<T>;

    // ---- Adding and removing ----------------------------------------------

    /// Store `value` and return a handle to it.
    ///
    /// Reuses a previously freed slot when one exists (LIFO — the most recently
    /// freed, which is the warmest in cache), and grows otherwise. Returns the
    /// NULL HANDLE if the pool is full, which happens at 1,048,576 live items;
    /// callers should test it exactly as they would test a failed allocation.
    ///
    /// **Invalidates every pointer previously returned by `get()`**, because
    /// `items_` may reallocate. Handles are unaffected. That sentence is the
    /// entire contract of this class.
    [[nodiscard]] handle_type insert(T value)
    {
        std::uint32_t slot_index = 0;

        if (!free_slots_.empty())
        {
            // The freed slot already carries its bumped generation — `remove()`
            // did that on the way out — so nothing here has to touch it. A slot
            // spends its whole free lifetime holding a generation that no
            // outstanding handle can match, which is why "is this slot free?"
            // never needs a separate flag.
            slot_index = free_slots_.back();
            free_slots_.pop_back();
        }
        else
        {
            if (slots_.size() > k_handle_max_index)
            {
                // 20 bits of index, all spent. Returning null rather than
                // asserting: running out of slots is something the WORLD can do
                // to a correct program (load a scene with too much in it), and
                // Lesson 5.3's rule says the world's failures are errors, not
                // assertions.
                ENGINE_LOG_ERROR(log_core, "pool: out of slots (%u max)",
                                 k_handle_max_index + 1u);
                return handle_type{};
            }
            slot_index = static_cast<std::uint32_t>(slots_.size());
            slots_.push_back(slot{k_handle_first_generation, k_no_dense});
        }

        slots_[slot_index].dense = static_cast<std::uint32_t>(items_.size());
        items_.push_back(std::move(value));
        owners_.push_back(slot_index);

        return make_handle<T>(slot_index, slots_[slot_index].generation);
    }

    /// Destroy the item `h` names. Returns false if `h` was null or already stale.
    ///
    /// Two things happen, and the order matters:
    ///
    ///   1. **Swap and patch.** The last live item is moved into the hole and its
    ///      OWNING SLOT is patched to point at the new location. This is the step
    ///      that keeps `items_` packed, and it is why `owners_` exists.
    ///   2. **Bump the generation.** From this instant every handle that named
    ///      this slot — including `h` itself — is stale, and `contains()` will
    ///      say so.
    ///
    /// Removing an item **invalidates pointers to the item that moved**, not just
    /// to the one that died. This surprises people. It is also unavoidable in any
    /// packed container, and it is exactly what handles exist to make survivable.
    bool remove(handle_type h)
    {
        if (!contains(h)) { return false; }

        const std::uint32_t slot_index = h.index();
        const std::uint32_t dead = slots_[slot_index].dense;
        const std::uint32_t last = static_cast<std::uint32_t>(items_.size() - 1u);

        if (dead != last)
        {
            items_[dead] = std::move(items_[last]);
            owners_[dead] = owners_[last];
            slots_[owners_[dead]].dense = dead;
        }

        // Assigning a default-constructed T releases whatever the dead item
        // owned. `pop_back` alone would shrink the vector, but a moved-from
        // `mesh_data` is not required to have given up its buffers, and a pool
        // that quietly holds a freed mesh's four megabytes is a leak with a
        // clean bill of health.
        items_[last] = T{};
        items_.pop_back();
        owners_.pop_back();

        std::uint32_t next = slots_[slot_index].generation + 1u;
        if (next > k_handle_max_generation)
        {
            // 4,095 reuses of this one slot. The old handles from the first cycle
            // are about to become indistinguishable from new ones — see the bit
            // budget in handle.hpp. We count it rather than assume it away,
            // because an assumption nobody measures is a bug with a long fuse.
            next = k_handle_first_generation;
            ++wraps_;
            ENGINE_LOG_WARN(log_core, "pool: generation wrapped on slot %u (wrap #%zu)",
                            slot_index, wraps_);
        }
        slots_[slot_index].generation = next;
        slots_[slot_index].dense = k_no_dense;
        free_slots_.push_back(slot_index);

        return true;
    }

    /// Destroy every item, keeping the slots and their generations.
    ///
    /// **Every outstanding handle becomes stale**, which is the point: this is
    /// what an asset system calls between levels. Generations are kept rather
    /// than reset, because resetting them would make yesterday's handles start
    /// resolving again — the one thing this whole design exists to prevent.
    void clear()
    {
        for (std::size_t d = 0; d < items_.size(); ++d)
        {
            const std::uint32_t slot_index = owners_[d];
            std::uint32_t next = slots_[slot_index].generation + 1u;
            if (next > k_handle_max_generation) { next = k_handle_first_generation; ++wraps_; }
            slots_[slot_index].generation = next;
            slots_[slot_index].dense = k_no_dense;
            free_slots_.push_back(slot_index);
        }
        items_.clear();
        owners_.clear();
    }

    // ---- Resolving ---------------------------------------------------------

    /// Does `h` still name a live item in THIS pool?
    ///
    /// Three tests, and each one rejects a different failure from §2 of the
    /// lesson: the range test rejects a handle from another pool or a fabricated
    /// one, the `dense` test rejects a slot that is currently free, and the
    /// generation test rejects the interesting case — a slot that is live again
    /// with a DIFFERENT occupant. Null handles fail the range test on an empty
    /// pool and the generation test everywhere else, since no live slot ever
    /// carries generation 0.
    [[nodiscard]] bool contains(handle_type h) const
    {
        if (h.index() >= slots_.size()) { return false; }
        const slot& s = slots_[h.index()];
        return s.dense != k_no_dense && s.generation == h.generation();
    }

    /// The item `h` names, or `nullptr` if the handle is null, stale, or foreign.
    ///
    /// **The returned pointer is valid only until the next `insert` or `remove`.**
    /// Resolve late, use immediately, never store. A `T*` cached across a call
    /// that mutates the pool is exactly the bug this class was built to abolish,
    /// and caching one puts it back.
    [[nodiscard]] T* get(handle_type h)
    {
        return contains(h) ? &items_[slots_[h.index()].dense] : nullptr;
    }

    /// Const overload; same rules.
    [[nodiscard]] const T* get(handle_type h) const
    {
        return contains(h) ? &items_[slots_[h.index()].dense] : nullptr;
    }

    // ---- Iterating ---------------------------------------------------------

    /// Every live item, contiguously, with no holes and no order you may rely on.
    ///
    /// This is what the dense layout buys, and what a slot-indexed array cannot
    /// offer: a `for (auto& m : meshes.items())` touches only cache lines that
    /// contain something. Lesson 5.6 turns that sentence into a measurement.
    [[nodiscard]] std::span<T> items() { return items_; }
    [[nodiscard]] std::span<const T> items() const { return items_; }

    /// The handle for the item at dense position `d`, so an iteration can name
    /// what it is looking at. Returns null if `d` is out of range.
    [[nodiscard]] handle_type handle_at(std::size_t d) const
    {
        if (d >= owners_.size()) { return handle_type{}; }
        const std::uint32_t slot_index = owners_[d];
        return make_handle<T>(slot_index, slots_[slot_index].generation);
    }

    // ---- Facts about the pool ----------------------------------------------

    /// How many items are live.
    [[nodiscard]] std::size_t size() const { return items_.size(); }

    [[nodiscard]] bool empty() const { return items_.empty(); }

    /// How many slots have ever been created. Never shrinks, and the gap between
    /// this and `size()` is how many handles' worth of history the pool is
    /// carrying — the price of being able to say "no" to a stale handle.
    [[nodiscard]] std::size_t slot_count() const { return slots_.size(); }

    /// Slots currently free and awaiting reuse.
    [[nodiscard]] std::size_t free_count() const { return free_slots_.size(); }

    /// How many times a slot's generation has wrapped past 4,095. In a healthy
    /// program this is zero forever; if it is not, the bit budget in handle.hpp
    /// needs revisiting for this pool's churn rate.
    [[nodiscard]] std::size_t generation_wraps() const { return wraps_; }

    /// The generation slot `i` currently carries — for diagnostics and for tests
    /// that need to fabricate a stale handle deliberately. Returns 0 for an
    /// out-of-range slot, which is never a live generation.
    [[nodiscard]] std::uint32_t generation_of(std::uint32_t i) const
    {
        return i < slots_.size() ? slots_[i].generation : 0u;
    }

private:
    /// `dense` when a slot holds nothing. Not a valid index into `items_` for any
    /// pool that fits in memory, and cheaper than a parallel array of booleans.
    static constexpr std::uint32_t k_no_dense = 0xFFFFFFFFu;

    struct slot
    {
        std::uint32_t generation = k_handle_first_generation;
        std::uint32_t dense = k_no_dense;
    };

    std::vector<slot> slots_;              ///< sparse, stable, indexed by handle
    std::vector<T> items_;                 ///< dense, packed, mobile
    std::vector<std::uint32_t> owners_;    ///< items_[d] belongs to slots_[owners_[d]]

    /// Freed slots awaiting reuse, LIFO.
    ///
    /// LIFO because the most recently freed slot is the one still in cache, and
    /// because a vector used as a stack is the cheapest structure there is. The
    /// cost is that a hot allocate/free pair hammers ONE slot's generation, which
    /// is what makes wrap-around reachable at all; a FIFO free list spreads reuse
    /// across every slot and multiplies the time to wrap by the number of slots.
    /// If `generation_wraps()` ever leaves zero, that is the first fix to try.
    std::vector<std::uint32_t> free_slots_;

    std::size_t wraps_ = 0;
};

}   // namespace engine
