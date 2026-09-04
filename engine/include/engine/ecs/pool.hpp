// engine/include/engine/ecs/pool.hpp — one component type's storage: a sparse set.
//
// Lesson 5.8, building what Lesson 5.7 decided. Three arrays:
//
//     sparse_   [ 2 ][ - ][ 0 ][ - ][ 1 ]        indexed by ENTITY INDEX
//                 |         |         |          value = where that entity's
//                 |         |         |                  component sits below
//                 +----+    +--+   +--+
//                      |       |   |
//                      v       v   v
//     dense_    [ entity 2 ][ entity 4 ][ entity 0 ]   who owns each component
//     data_     [    T     ][    T     ][    T     ]   the components themselves
//
//   - `sparse_` is indexed by an entity's INDEX FIELD and is as long as the
//     allocator's high-water mark, whether or not those entities have this
//     component. Four bytes per entity per component type; Lesson 5.7 §5.4 costs
//     that honestly (12.8 MB at 32 types and 100,000 entities, ~80% of it
//     meaning "absent") and rules that paging it can wait.
//   - `dense_` and `data_` are packed, in lockstep, with no holes. A system that
//     wants every velocity walks `data_` and touches nothing else — which is
//     Lesson 5.6's whole finding, in a container.
//   - `dense_[i]` is the FULL entity word, generation included, not just an
//     index. That is the one line that makes a stale id fail a lookup instead of
//     quietly reading somebody else's component.
//
// A NAME COLLISION, ON PURPOSE. `engine::pool<T>` in core/pool.hpp is a
// DIFFERENT CONTAINER with the same name, and the difference is the whole of
// Lesson 5.8 §2:
//
//     engine::pool<T>       mints its own keys. Ask it to store something and it
//                           gives you a handle<T> that only it can read. One
//                           container, one id space, no relationship to any other
//                           pool in the program.
//     engine::ecs::pool<T>  is KEYED BY AN ID IT DID NOT MINT. It cannot create
//                           an entity and has no opinion about which ones exist;
//                           it is a map from somebody else's id to a value, and
//                           that is exactly what lets ten of them describe one
//                           entity between them.
//
// Same three arrays, same swap-and-pop, same generational staleness check —
// opposite jobs. Giving this one a different name (`component_storage`,
// `component_pool`) would hide a relationship the course spends two lessons
// drawing out; the namespace keeps them apart without pretending they are
// unrelated. Inside `engine::ecs`, plain `pool` is this one.

#pragma once

#include <engine/core/assert.hpp>
#include <engine/ecs/entity.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace engine::ecs
{

/// Everything the registry can do to a pool without knowing what it stores.
///
/// **Type erasure, and no RTTI was harmed.** `registry` holds a vector of these
/// so that `destroy(e)` can walk every pool in the world and erase `e`'s row from
/// each — an operation that must not know, and cannot know, what any of those
/// pools contain. The engine forbids RTTI (Lesson 5.1), so `dynamic_cast` is not
/// available and is not wanted: the registry recovers the concrete type from the
/// component id that indexed the pointer, which is a fact it established when it
/// created the pool, not a guess it makes when it uses it.
///
/// **Every function here is cold, and that is a constraint rather than a
/// coincidence.** Lesson 5.6 measured virtual dispatch on an iteration at 1.5–1.7×
/// *at every world size, including four objects in L1*, and concluded that ECS
/// iteration must not be virtual. So nothing on the hot path goes through this
/// interface: a view holds concrete `pool<T>*` and calls non-virtual members on
/// it. What is left here runs once per pool per entity destruction, and once per
/// pool at teardown. `pool<T>` is `final` besides, so even these calls devirtualise
/// wherever the static type is known.
class pool_base
{
public:
    pool_base() = default;
    virtual ~pool_base() = default;

    pool_base(const pool_base&) = delete;
    pool_base& operator=(const pool_base&) = delete;

    /// Erase `e`'s component if it has one. False if it did not.
    virtual bool erase(entity e) = 0;

    /// Drop every component, keeping the sparse array's capacity.
    virtual void clear() = 0;

    /// How many components this pool holds.
    [[nodiscard]] virtual std::size_t size() const = 0;

    /// Which entities own them, in dense order.
    ///
    /// Type-INDEPENDENT, which is the property the view is built on: every pool,
    /// whatever it stores, can hand back the same `std::span<const entity>`. That
    /// is what lets a view over four component types pick the smallest of the
    /// four with a plain loop over an array of spans instead of a pile of
    /// template machinery. Cheap enough to be virtual — a view calls it once at
    /// construction, never per entity.
    [[nodiscard]] virtual std::span<const entity> entities() const = 0;
};

/// One component type's storage, keyed by entity.
///
/// `T` must be movable. It does NOT have to be default-constructible, trivially
/// copyable or small — but Lesson 5.7's rule 1 says it should be small and
/// single-purpose anyway, and the reason is mechanical rather than stylistic: the
/// entire model is that a system pays only for the pools it names, and a fat
/// component drags unread bytes through cache exactly as Lesson 5.6's 96-byte
/// `scene_object` did. Two components of 16 bytes are strictly better than one of
/// 32 whenever any system wants only one of them.
///
/// Complexity: `insert`, `erase`, `get` and `contains` are O(1). `insert` may
/// grow two vectors and, when it sees a new high-water entity index, the sparse
/// array as well.
///
/// **Not thread-safe.**
template <typename T>
class pool final : public pool_base
{
public:
    using value_type = T;

    // ---- Adding and removing ----------------------------------------------

    /// File `value` under `e`. Returns a pointer to the stored component, or
    /// nullptr if `e` is null.
    ///
    /// A pointer rather than a reference, because there is a case in which there
    /// is nothing to refer to, and Lesson 5.3's error strategy says to return the
    /// failure rather than invent a value or throw. Most call sites ignore it.
    ///
    /// **Adding a component an entity already has is a programmer error**, so it
    /// asserts — a correct program knows what it has attached. In a release build
    /// the assertion is gone and the new value REPLACES the old one, which is the
    /// least surprising of the available behaviours and is documented rather than
    /// discovered.
    ///
    /// This pool cannot tell whether `e` is alive; it has never met the
    /// allocator. `registry::add` checks first, and going around the registry to
    /// call this directly is how you file a component under a dead id.
    T* insert(entity e, T value)
    {
        ENGINE_ASSERT(e.valid());
        if (!e.valid()) { return nullptr; }

        if (T* existing = get(e))
        {
            ENGINE_ASSERT_MSG(false, log_core,
                              "ecs::pool::insert: entity %u already has this component",
                              e.index());
            *existing = std::move(value);
            return existing;
        }

        // The sparse array is indexed by entity index, so it has to be at least
        // as long as the largest index ever filed here. It never shrinks, which
        // is the memory Lesson 5.7 §5.4 accounted for.
        if (e.index() >= sparse_.size()) { sparse_.resize(e.index() + 1u, k_none); }

        sparse_[e.index()] = static_cast<std::uint32_t>(dense_.size());
        dense_.push_back(e);
        data_.push_back(std::move(value));
        return &data_.back();
    }

    /// Erase `e`'s component. False if it did not have one.
    ///
    /// **Swap and pop, and the patch is the part people get wrong.** The last
    /// component is moved into the hole, which means the entity that owned the
    /// last component now owns a different dense slot — and its sparse entry
    /// still points at the old one. Patching that entry is the second line below,
    /// and forgetting it produces a corruption that shows up an arbitrary number
    /// of frames later, in a different system, as one entity reading another's
    /// data. Lesson 5.7 §7 hit exactly this bug in the archetype arm and it took
    /// a 200-frame churn test to find.
    ///
    /// The edge case is the one worth stepping through: when the erased component
    /// IS the last one, `moved` is the entity being erased, and the patch writes
    /// its sparse entry back to where it already was — immediately before the
    /// final line marks it absent. Harmless, and the order is what makes it so.
    bool erase(entity e) override
    {
        if (!contains(e)) { return false; }

        const std::uint32_t at = sparse_[e.index()];
        const std::uint32_t last = static_cast<std::uint32_t>(dense_.size() - 1u);

        if (at != last)
        {
            dense_[at] = dense_[last];
            data_[at] = std::move(data_[last]);
            sparse_[dense_[at].index()] = at;   // the patch
        }

        dense_.pop_back();
        data_.pop_back();
        sparse_[e.index()] = k_none;
        return true;
    }

    /// Drop every component. The sparse array keeps its length (and so its
    /// capacity), because the entities it is sized for have not gone anywhere.
    void clear() override
    {
        for (entity e : dense_) { sparse_[e.index()] = k_none; }
        dense_.clear();
        data_.clear();
    }

    // ---- Resolving ---------------------------------------------------------

    /// Does `e` own a component in this pool?
    ///
    /// Three tests, and the third is the one that costs a line and buys the
    /// design. The range test rejects an entity this pool has never seen; the
    /// `k_none` test rejects one whose component was erased; and
    /// `dense_[at] == e` compares the FULL entity word, so an id that names the
    /// same slot with an older generation — a stale entity, or a recycled one
    /// looking for its predecessor's data — is rejected. Store bare indices in
    /// `dense_` and that third test cannot be written at all, which is Lesson
    /// 5.4's "aliasing" failure with a new coat on.
    [[nodiscard]] bool contains(entity e) const
    {
        if (e.index() >= sparse_.size()) { return false; }
        const std::uint32_t at = sparse_[e.index()];
        return at != k_none && dense_[at] == e;
    }

    /// `e`'s component, or nullptr if it has none.
    ///
    /// **The returned pointer is valid until the next `insert` or `erase` on this
    /// pool, and not one instruction longer** — the same rule `engine::pool` set
    /// in Lesson 5.4, for the same reason: both vectors may reallocate and a
    /// removal moves the last element regardless. Resolve late, use immediately,
    /// never store.
    [[nodiscard]] T* get(entity e)
    {
        return contains(e) ? &data_[sparse_[e.index()]] : nullptr;
    }

    /// Const overload; same rules.
    [[nodiscard]] const T* get(entity e) const
    {
        return contains(e) ? &data_[sparse_[e.index()]] : nullptr;
    }

    // ---- Iterating ---------------------------------------------------------

    /// Every component, contiguously, in an order that is nobody's business but
    /// this pool's.
    ///
    /// Depending on that order is depending on the history of your insertions and
    /// removals — with one exception, and it is the interesting one. Lesson 5.7
    /// measured that if two pools are *sorted to agree* on dense order, a query
    /// over the pair needs no sparse lookup at all and runs at 0.99× a real
    /// archetype. That is what a GROUP is, and it is a thing this class could be
    /// taught to maintain later precisely because the order was never promised to
    /// anyone. See §9 for why it is not here yet.
    [[nodiscard]] std::span<T> components() { return data_; }
    [[nodiscard]] std::span<const T> components() const { return data_; }

    /// Who owns each component, in the same order.
    [[nodiscard]] std::span<const entity> entities() const override { return dense_; }

    /// The component at dense position `d`. No bounds check in release; this is
    /// the accessor a view uses once it already knows `d` is in range because it
    /// is the loop counter.
    [[nodiscard]] T& at(std::size_t d)
    {
        ENGINE_ASSERT(d < data_.size());
        return data_[d];
    }

    [[nodiscard]] const T& at(std::size_t d) const
    {
        ENGINE_ASSERT(d < data_.size());
        return data_[d];
    }

    /// Who owns the component at dense position `d`.
    [[nodiscard]] entity entity_at(std::size_t d) const
    {
        ENGINE_ASSERT(d < dense_.size());
        return dense_[d];
    }

    // ---- Facts -------------------------------------------------------------

    [[nodiscard]] std::size_t size() const override { return data_.size(); }
    [[nodiscard]] bool empty() const { return data_.empty(); }

    /// How long the sparse array is — the largest entity index ever filed here,
    /// plus one. The gap between this and `size()` is the pool's memory overhead
    /// in entries, and it is the number Lesson 5.7 §5.4 multiplies by four bytes
    /// and by the component-type count.
    [[nodiscard]] std::size_t sparse_extent() const { return sparse_.size(); }

private:
    /// "No component here." Not a reachable dense index for any pool that fits in
    /// memory, and cheaper than a parallel array of flags — the same trick
    /// `engine::pool::k_no_dense` plays.
    static constexpr std::uint32_t k_none = 0xFFFFFFFFu;

    std::vector<std::uint32_t> sparse_;   ///< entity index -> dense position, or k_none
    std::vector<entity> dense_;           ///< dense position -> owning entity (full word)
    std::vector<T> data_;                 ///< dense position -> the component
};

}   // namespace engine::ecs
