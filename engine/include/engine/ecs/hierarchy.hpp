// engine/include/engine/ecs/hierarchy.hpp — a turret on a tank, and the order that costs.
//
// Lesson 5.9. Lesson 5.8 built a flat world: every `transform` is a placement in
// WORLD space, which is fine for a swarm orbiting an origin and useless the
// moment one thing has to move with another. A hierarchy fixes that with one
// component and one composition rule:
//
//     world_from_local(child) = world_from_local(parent) * parent_from_local(child)
//
// That rule is the whole of a scene graph, and `math/transform.hpp` has been
// written for it since Lesson 2.8 — which is why `parent_from_local` is called
// `parent_from_local` and not `world_from_local`. Nothing in that file changes
// today; only the meaning of "parent" widens from "the world" to "whoever is
// above you".
//
// THE PROBLEM IS NOT THE MATHS. IT IS THE ORDER.
//
// A parent's world matrix must exist before its children can be computed. That
// is an ordering constraint on the *graph* — and Lesson 5.7 established that a
// component pool's dense order is nobody's business but the pool's: it is
// insertion order, disturbed by every swap-and-pop. Walk `pool<transform>` in
// dense order and you will meet children before parents, roughly half the time,
// with no warning at all.
//
// Lesson 5.9 measures the three ways out (docs/lessons/05-09-transform-hierarchy.html
// §4) and takes the middle one:
//
//   RECURSE FROM ROOTS            rejected. Depth-DEPENDENT: at 100,000 entities
//                                 it ran 3.0 ns/entity at depth 1 and 13.3 at
//                                 depth 16, while level order ran 3.4 and 4.9.
//   LEVEL ORDER  <- what this is  entities bucketed by DEPTH, which is a
//                                 topological order because a parent's depth is
//                                 always less than its child's. One flat loop per
//                                 level, nearly flat in depth, and every entity
//                                 within a level is independent of every other —
//                                 which is what will let Module 8 run a level as
//                                 a parallel_for.
//   PERMUTE THE POOL              deferred, with the number attached. Physically
//                                 packing rows into level order buys another 30%
//                                 at 100,000 entities in a world whose row order
//                                 has decayed, and costs about 1.6 resolves every
//                                 time the shape changes. Below 10,000 entities
//                                 it is worth nothing. See the lesson's §7.
//
// AND THE HONEST SCALE. At the demo's entity count one resolve is under a
// microsecond — a few thousandths of a percent of a 60 Hz frame. None of the
// above matters here and the lesson says so; what matters is that the ORDER IS
// CORRECT, and that is not a performance question at any scale.

#pragma once

#include <engine/core/assert.hpp>
#include <engine/core/log.hpp>
#include <engine/ecs/registry.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace engine::ecs
{

// ---- The components --------------------------------------------------------

/// "My placement is relative to this entity."
///
/// One entity, four bytes. Note what is NOT here: no child list, and no ordering
/// information of any kind. Children are derivable from parents — this file does
/// exactly that, once per rebuild — and a stored child list would be a second
/// source of truth that has to be kept in step with the first. That is the same
/// argument `registry::destroy` makes for not keeping a per-entity component
/// mask, and it costs the same thing: a scan where an index would have been O(1).
/// The lesson's §7 prices it.
///
/// An entity with no `parent` component is a ROOT: its `transform` is already in
/// world space. That is exactly the rule that held before this lesson, which is
/// why Lesson 5.8's flat world keeps working with nothing changed.
struct parent
{
    entity value;
};

/// The composed result: this entity's local-to-world matrix.
///
/// **Written by `hierarchy::resolve`, and by nothing else.** It is a component
/// rather than a return value because everything downstream — the renderer, the
/// culler, Module 7's physics — wants to look it up by entity, and because
/// Module 8's serializer will want to skip it precisely BECAUSE it is derived
/// data that can be recomputed from the things that are not.
///
/// An entity with a `transform` but no `world_transform` is simply not resolved.
/// `resolve` never creates components, so the shape of the world stays the
/// caller's decision; `add_hierarchy_components` below is the convenience that
/// attaches the pair.
struct world_transform
{
    mat4 matrix = mat4::identity();
};

// ---- What a rebuild found --------------------------------------------------

/// Facts about the last `rebuild()`.
///
/// The shape Lesson 5.3 settled on, minus the status: a rebuild cannot fail, but
/// there are diagnostics worth having on success, which is exactly the case rule
/// 1 says deserves a report.
struct hierarchy_report
{
    std::size_t entities = 0;   ///< entities with a `transform` — the rows resolved
    std::size_t roots = 0;      ///< …of which these have no live parent
    std::size_t levels = 0;     ///< the deepest chain, counted in levels (1 = flat)

    /// Entities whose `parent` names something that is no longer alive.
    ///
    /// **The orphan policy, and it is a decision rather than an oversight: an
    /// orphan becomes a ROOT.** Its local transform is used unchanged, so it stops
    /// following anything and stays where it last was. The alternative —
    /// destroying the subtree — is a policy a *game* may want and a transform
    /// system must not impose: silently deleting entities during a resolve pass is
    /// a surprise nobody asked for, and `destroy_subtree()` below is the explicit
    /// tool for callers who do want it. Counted here so that "why did my turret
    /// stop following the tank" has an answer on a HUD.
    std::size_t orphans = 0;

    /// Entities found in a parent cycle. **Treated as roots, and the count is the
    /// alarm.**
    ///
    /// `set_parent()` refuses to create a cycle, but `parent` is a public
    /// component and nothing stops a caller writing one directly — so the
    /// resolver has to be defensive rather than trusting. A hierarchy that can
    /// loop is not a wrong picture, it is a hang, and a hang is the one failure a
    /// frame loop cannot survive.
    std::size_t cycles = 0;
};

// ---- Editing the shape -----------------------------------------------------

/// Why `set_parent` refused.
enum class parent_status
{
    ok,           ///< the link was made (or removed)
    dead_child,   ///< `child` is not a live entity
    dead_parent,  ///< `new_parent` is not a live entity
    cycle,        ///< `new_parent` IS `child`, or is below it
};

/// Is `maybe_ancestor` at or above `e` in the tree?
///
/// O(depth), and it terminates on a malformed graph: the walk is bounded by the
/// number of live entities rather than trusted to reach a root, so a pre-existing
/// cycle makes this return false instead of hanging. Exposed because it is
/// exactly the check an editor's "can I drop this here" needs — Module 8's scene
/// hierarchy panel is the first caller.
[[nodiscard]] inline bool is_ancestor_of(registry& world, entity maybe_ancestor, entity e)
{
    if (!maybe_ancestor.valid() || !e.valid()) { return false; }

    std::size_t guard = world.size() + 1u;
    entity cur = e;
    while (cur.valid() && guard-- > 0u)
    {
        if (cur == maybe_ancestor) { return true; }
        const parent* p = world.get<parent>(cur);
        if (p == nullptr) { return false; }
        cur = p->value;
    }
    return false;
}

/// Re-parent `child` under `new_parent`, or detach it by passing `null_entity`.
///
/// **Refuses to create a cycle**, which is the one invariant this module cannot
/// recover from cheaply. The test is a single walk up from `new_parent`: if
/// `child` is anywhere on that path, the new link would close a loop.
///
/// The caller must tell the resolver afterwards — `hierarchy::mark_topology_changed()`.
/// This function deliberately does not, because re-parenting comes in batches (a
/// squad boards a vehicle) and rebuilding once per link would be N rebuilds for
/// one logical change.
inline parent_status set_parent(registry& world, entity child, entity new_parent)
{
    if (!world.alive(child)) { return parent_status::dead_child; }

    if (!new_parent.valid())
    {
        world.remove<parent>(child);
        return parent_status::ok;
    }

    if (!world.alive(new_parent)) { return parent_status::dead_parent; }
    if (new_parent == child || is_ancestor_of(world, child, new_parent))
    {
        return parent_status::cycle;
    }

    if (parent* existing = world.get<parent>(child)) { existing->value = new_parent; }
    else { world.add<parent>(child, parent{new_parent}); }
    return parent_status::ok;
}

/// Destroy `e` and everything beneath it. Returns how many entities died.
///
/// **The explicit alternative to the orphan policy**, and the reason that policy
/// can afford to be permissive: a caller who wants a subtree gone says so, in a
/// function whose name says it.
///
/// The links are gathered and sorted once — O(n log n) — rather than rescanning
/// the parent pool per node, which would be O(subtree × n). That is the cost of
/// `parent` not keeping a child index, paid here where it is visible instead of
/// being paid every frame by a second source of truth.
///
/// **Collect first, destroy after**, which is Lesson 5.8's iteration rule
/// arriving exactly where it was predicted to: destroying an entity erases rows
/// from every pool, and that swap-and-pops the very array a view is walking.
inline std::size_t destroy_subtree(registry& world, entity e)
{
    if (!world.alive(e)) { return 0; }

    // (parent bits, child) for every link in the world, sorted so a parent's
    // children are a contiguous range.
    std::vector<std::pair<std::uint32_t, entity>> links;
    world.view<parent>().each([&links](entity child, const parent& p) {
        links.emplace_back(p.value.bits, child);
    });
    std::sort(links.begin(), links.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::vector<entity> doomed{e};
    for (std::size_t head = 0; head < doomed.size(); ++head)
    {
        const std::uint32_t key = doomed[head].bits;
        const auto lo = std::lower_bound(links.begin(), links.end(), key,
                                         [](const auto& l, std::uint32_t k) { return l.first < k; });
        for (auto it = lo; it != links.end() && it->first == key; ++it)
        {
            doomed.push_back(it->second);
        }
    }

    for (const entity victim : doomed) { world.destroy(victim); }
    return doomed.size();
}

/// Give `e` the pair of components a resolved entity needs.
///
/// Convenience only — there is nothing magic about having both, and an entity
/// may legitimately have a `transform` and no `world_transform`, in which case it
/// is simply not resolved.
inline void add_hierarchy_components(registry& world, entity e, const transform& local)
{
    world.add<transform>(e, local);
    world.add<world_transform>(e, world_transform{});
}

// ---- The resolver ----------------------------------------------------------

/// Computes every `world_transform`, parents before children.
///
/// Two operations with very different frequencies, and keeping them apart is the
/// single most useful thing this class does:
///
///   `rebuild()`  reads every `parent` link and produces a LEVEL ORDER. Costs
///                about as much as one resolve. Needed only when the SHAPE
///                changes — an entity gains or loses a transform, a parent link
///                is set or cleared, an entity is destroyed.
///   `resolve()`  walks that order and composes. Needed every frame.
///
/// A game re-parents rarely and moves things constantly, so this split is where
/// the real saving lives — far more than any choice of visit order. Calling
/// `rebuild()` every frame is *correct* and merely wasteful; calling it too
/// rarely is a bug, so `resolve()` asserts what it can about the order's
/// freshness before walking it.
///
/// **Not thread-safe**, like every container in this engine until Module 8.
class hierarchy
{
public:
    /// Rebuild the level order from the registry's current parent links.
    ///
    /// Three passes, all O(n): compute each entity's depth by a memoised chase up
    /// its ancestors, counting-sort the entities into depth buckets, and total up
    /// the report. **The memoisation is what keeps it O(n) rather than
    /// O(n × depth)** — without it, a 16-deep chain of 100,000 entities walks 1.6
    /// million links instead of 100,000, and the difference is measurable at
    /// exactly the sizes where anybody cares.
    ///
    /// `depth_` is a SPARSE ARRAY keyed by entity index, sized by the allocator's
    /// high-water mark — the same shape, and the same memory story, as every
    /// component pool's sparse array (Lesson 5.7 §5.4). A hash map keyed by entity
    /// would have been the obvious spelling and would have put a hash on the one
    /// path this whole module exists to keep as an array index.
    hierarchy_report rebuild(registry& world)
    {
        report_ = hierarchy_report{};
        order_.clear();
        level_start_.clear();
        scratch_.clear();

        pool<transform>* transforms = world.storage_if<transform>();
        if (transforms == nullptr || transforms->empty())
        {
            level_start_.push_back(0u);
            topology_dirty_ = false;
            return report_;
        }

        pool<parent>* parents = world.storage_if<parent>();
        const std::span<const entity> rows = transforms->entities();
        report_.entities = rows.size();

        depth_.assign(world.entities().slot_count(), k_unvisited);

        for (const entity start : rows)
        {
            if (depth_[start.index()] != k_unvisited) { continue; }

            // Walk up until we reach a depth already known, a root, or a loop.
            scratch_.clear();
            entity cur = start;
            std::uint32_t known = k_unvisited;
            bool cyclic = false;

            for (;;)
            {
                const std::uint32_t d = depth_[cur.index()];
                if (d == k_in_progress) { cyclic = true; break; }
                if (d != k_unvisited) { known = d; break; }

                depth_[cur.index()] = k_in_progress;
                scratch_.push_back(cur);

                const parent* p = (parents != nullptr) ? parents->get(cur) : nullptr;
                if (p == nullptr) { break; }                    // a root by construction

                // A parent that is dead, or that has no transform of its own, is
                // not a parent this pass can compose through. Both make the child
                // a root — the orphan policy, and it is one branch because the
                // two cases have the same answer.
                if (!world.alive(p->value) || !transforms->contains(p->value))
                {
                    ++report_.orphans;
                    break;
                }
                cur = p->value;
            }

            if (cyclic)
            {
                // Everything on this chain is inside a loop or hanging off one.
                // Break it by declaring the chain's top a root: the picture will
                // be wrong, the count and the log line will say so, and the frame
                // will not hang.
                ++report_.cycles;
                ENGINE_LOG_WARN(log_core,
                                "ecs::hierarchy: parent cycle reached through entity %u — "
                                "treating it as a root (%zu so far)",
                                scratch_.back().index(), report_.cycles);
            }

            // …and unwind, assigning depths on the way back down.
            std::uint32_t d = known;
            while (!scratch_.empty())
            {
                d = (d == k_unvisited) ? 0u : d + 1u;
                depth_[scratch_.back().index()] = d;
                scratch_.pop_back();
            }
        }

        // ---- counting sort into level buckets -----------------------------
        //
        // Depth IS a topological order: a parent's depth is always exactly one
        // less than its child's, so sorting by it puts every parent before every
        // child. Within a level the order is arbitrary and — this is the useful
        // part — it does not matter, because nothing in a level depends on
        // anything else in it.
        std::uint32_t max_depth = 0;
        for (const entity e : rows)
        {
            max_depth = std::max(max_depth, depth_[e.index()]);
        }

        level_start_.assign(static_cast<std::size_t>(max_depth) + 2u, 0u);
        for (const entity e : rows) { ++level_start_[depth_[e.index()] + 1u]; }
        for (std::size_t i = 1; i < level_start_.size(); ++i)
        {
            level_start_[i] += level_start_[i - 1u];
        }

        order_.assign(rows.size(), null_entity);
        cursor_.assign(level_start_.begin(), level_start_.end() - 1);
        for (const entity e : rows) { order_[cursor_[depth_[e.index()]]++] = e; }

        report_.roots = level_start_[1];
        report_.levels = static_cast<std::size_t>(max_depth) + 1u;

        topology_dirty_ = false;
        return report_;
    }

    /// Compose every `world_transform`, parents before children.
    ///
    /// **The loop is deliberately dull, and that is the finding.** No recursion,
    /// no stack, no visited set, and no "has my parent been done yet" test —
    /// because the ORDER already guarantees it. Everything a level reads was
    /// written by an earlier level, so within a level the iterations are
    /// completely independent of one another. That is the property Module 8 turns
    /// into a `parallel_for`, and it is worth noticing that it arrived free with
    /// the choice of visit order rather than being designed in.
    void resolve(registry& world)
    {
        ENGINE_ASSERT_MSG(!topology_dirty_, log_core,
                          "ecs::hierarchy::resolve: the shape changed since the last "
                          "rebuild() — call rebuild() first");

        pool<transform>* transforms = world.storage_if<transform>();
        pool<world_transform>* worlds = world.storage_if<world_transform>();
        if (transforms == nullptr || worlds == nullptr) { return; }

        // The strongest freshness check available without a version counter, and
        // it is worth naming what it cannot see: an add and a remove between two
        // resolves leaves the count identical and the order stale. That is the
        // honest cost of `mark_topology_changed()` being a flag the caller sets
        // rather than a signal the registry emits.
        ENGINE_ASSERT_MSG(order_.size() == transforms->size(), log_core,
                          "ecs::hierarchy::resolve: the order holds %zu entries and the "
                          "transform pool holds %zu — rebuild() was not called",
                          order_.size(), transforms->size());

        pool<parent>* parents = world.storage_if<parent>();

        for (const entity e : order_)
        {
            const transform* local = transforms->get(e);
            world_transform* out = worlds->get(e);
            if (local == nullptr || out == nullptr) { continue; }

            const mat4 local_matrix = parent_from_local(*local);
            const parent* p = (parents != nullptr) ? parents->get(e) : nullptr;

            // A root — no parent component, a dead parent, or a parent with no
            // world transform of its own — is already in world space. All three
            // collapse to the same line, which is why the orphan policy is cheap
            // as well as defensible.
            const world_transform* above =
                (p != nullptr) ? worlds->get(p->value) : nullptr;

            out->matrix = (above != nullptr) ? above->matrix * local_matrix : local_matrix;
        }
    }

    /// `rebuild()` then `resolve()`. Always correct; wasteful on the frames when
    /// the shape did not change, which is most of them.
    hierarchy_report rebuild_and_resolve(registry& world)
    {
        const hierarchy_report r = rebuild(world);
        resolve(world);
        return r;
    }

    /// Tell the resolver that the shape changed and a rebuild is required.
    ///
    /// A flag rather than an automatic hook, and that is a real trade stated
    /// plainly: the registry notifies nobody about anything (Lesson 5.8 ships no
    /// signals), so this is the seam where a caller takes responsibility. Forget
    /// it and a debug build asserts on the next `resolve()` whenever the entity
    /// count moved; a release build resolves a stale order, which shows up as one
    /// object failing to follow its parent. Signals would close the footgun and
    /// add a mechanism nobody has yet asked for — Module 8 revisits it with the
    /// editor, which is the first caller that genuinely needs one.
    void mark_topology_changed() { topology_dirty_ = true; }

    [[nodiscard]] bool topology_changed() const { return topology_dirty_; }

    // ---- What the order looks like ----------------------------------------

    /// Every resolved entity, parents strictly before children.
    [[nodiscard]] std::span<const entity> order() const { return order_; }

    [[nodiscard]] std::size_t levels() const
    {
        return level_start_.empty() ? 0u : level_start_.size() - 1u;
    }

    /// The entities at depth `i`. Level 0 is the roots.
    ///
    /// Exposed because it is **the unit of parallelism**: a caller may hand one
    /// level to a job system knowing that nothing in it depends on anything else
    /// in it. It is also how a test checks that the order really is topological
    /// rather than merely producing the right answer by luck — verify_59 §C walks
    /// it and asserts that every entity's parent appeared in an earlier level.
    [[nodiscard]] std::span<const entity> level(std::size_t i) const
    {
        if (i + 1u >= level_start_.size()) { return {}; }
        return std::span<const entity>(order_).subspan(level_start_[i],
                                                       level_start_[i + 1u] - level_start_[i]);
    }

    /// The depth `rebuild()` assigned to `e`, or 0 for an entity it never saw.
    [[nodiscard]] std::uint32_t depth_of(entity e) const
    {
        if (e.index() >= depth_.size()) { return 0u; }
        const std::uint32_t d = depth_[e.index()];
        return (d == k_unvisited || d == k_in_progress) ? 0u : d;
    }

    [[nodiscard]] const hierarchy_report& last_report() const { return report_; }

private:
    static constexpr std::uint32_t k_unvisited = 0xFFFFFFFFu;
    static constexpr std::uint32_t k_in_progress = 0xFFFFFFFEu;

    std::vector<entity> order_;              ///< parents strictly before children
    std::vector<std::uint32_t> level_start_; ///< level i is order_[start[i] .. start[i+1])
    std::vector<std::uint32_t> depth_;       ///< sparse, keyed by entity INDEX
    std::vector<std::uint32_t> cursor_;      ///< counting-sort scratch, kept to avoid reallocating
    std::vector<entity> scratch_;            ///< the ancestor chase's stack
    hierarchy_report report_;
    bool topology_dirty_ = true;
};

}   // namespace engine::ecs
