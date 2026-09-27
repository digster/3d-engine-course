// scratch/hier_probe.hpp — Lesson 5.9's probe: four ways to resolve one forest.
//
// THE QUESTION, and it is a real one rather than a warm-up.
//
// A transform hierarchy imposes an ORDER: a parent's world matrix must exist
// before a child's can be computed. Lesson 5.7 established that a component
// pool's dense order is nobody's business but the pool's — it is insertion
// order, disturbed by every swap-and-pop. Those two facts are in tension, and
// the whole of Lesson 5.9 is what it costs to reconcile them.
//
// There are four plausible answers, they differ ONLY in the order they visit
// rows and where they keep the ordering information, and every one of them
// computes exactly the same matrices. So they can be raced.
//
//   A  RECURSE FROM ROOTS. Build a children list, walk depth-first, compose on
//      the way down. The parent's matrix was written one call ago, so it is
//      certainly in L1 — the best temporal locality any arm can have. Pays for
//      it in call overhead and in a write pattern that jumps.
//
//   B  LEVEL ORDER, VIA AN INDEX. Bucket rows by DEPTH (which is a topological
//      order: a parent's depth is always less than its child's), then run one
//      flat loop per level. No recursion, and every level is independent of
//      everything except the level above — which is what makes it parallelisable
//      in Module 9. The arrays are NOT moved, so both reads and writes are
//      scattered through them.
//
//   C  LEVEL ORDER, ARRAYS PERMUTED. The same order as B, but the rows are
//      actually rearranged so that each level is contiguous. Reads of `local`
//      and writes of `world` become sequential; only the parent lookup still
//      jumps. This is Lesson 5.7's GROUP idea generalised from "two pools agree
//      on index" to "one pool obeys an ordering constraint" — and 5.7 predicted
//      it would be available precisely because dense order was never promised.
//
//   D  DIRTY SUBTREES ONLY. B, but skipping every row whose subtree did not
//      move this frame. Reduces WORK rather than improving LOCALITY, which is a
//      different axis, and it is the only arm whose cost depends on what the
//      game did rather than on what the scene is.
//
// WHAT THE PROBE DELIBERATELY DOES NOT DO is use the ECS. `registry` exists and
// works (5.8), and building the experiment on it would tie four timings to one
// container's incidental behaviour. The measured quantity is a VISIT ORDER over
// three parallel arrays; that is what these arms differ in, and it is all they
// need to be.
//
// FAIRNESS NOTES, because the arms are not automatically comparable:
//
//   - `scramble` is the axis most treatments omit. In a world built all at once,
//     row order IS creation order, so parents naturally sit before children and
//     every arm walks nearly forwards. In a world that has been running for ten
//     minutes — spawning, dying, swap-and-popping — row order is arbitrary. Both
//     are real. Quoting only the first is not an argument.
//   - Arm C's PERMUTATION IS NOT FREE and is not timed inside the resolve. It is
//     a cost the world pays on structural change, measured separately in §4 of
//     the lesson, and pretending otherwise would be the classic benchmark lie.
//   - Every arm accumulates a checksum IN VISIT ORDER, and on Lesson 5.6's
//     evidence that should make `bench_ab::agree` false — floating-point addition
//     is not associative. It does not, and the reason is worth knowing: each
//     addend is a `float` accumulated into a `double`, and a running sum of 1e5
//     values of magnitude ~1 never needs more than 41 significant bits, so
//     nothing is ever rounded and an exact sum is order-independent. Swap the
//     accumulator to `float` and the agreement vanishes; that knob is what proves
//     the explanation. Arm D is the exception and disagrees legitimately — it
//     sums fewer rows because it touched fewer. The EXACT equality of the world
//     MATRICES is checked in verify_59, outside any timing loop, element by
//     element, which is where an exactness claim belongs.

#pragma once

#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace hier {

/// "This row has no parent."
inline constexpr std::uint32_t k_root = 0xFFFFFFFFu;

// ---------------------------------------------------------------------------
//  The world
// ---------------------------------------------------------------------------

/// A forest, in the layout every arm shares.
///
/// Three parallel arrays indexed by ROW — which is exactly what a component
/// pool's dense storage is, minus the entity ids that would only add noise here.
/// Everything else in this struct is an *index* built on top of them, and which
/// index an arm uses is the whole experiment.
struct world
{
    std::vector<engine::transform> local;    ///< the authored placement, per row
    std::vector<std::uint32_t> parent_row;   ///< k_root, or the row of the parent
    std::vector<engine::mat4> out;           ///< the resolved world matrix, per row

    // ---- Arm A's index: children as CSR -----------------------------------
    //
    // Compressed sparse row: `children[child_start[r] .. child_start[r+1])` are
    // the rows whose parent is `r`. A vector<vector<uint32_t>> would be the
    // obvious spelling and would give the recursive arm n separate allocations
    // to chase, which is a strawman rather than a competitor.
    std::vector<std::uint32_t> child_start;  ///< size n + 1
    std::vector<std::uint32_t> children;     ///< size n - roots
    std::vector<std::uint32_t> roots;

    // ---- Arms B, C and D's index: rows bucketed by depth -------------------
    std::vector<std::uint32_t> order;        ///< rows, parents strictly before children
    std::vector<std::uint32_t> level_start;  ///< level i is order[level_start[i] .. [i+1])
    std::vector<std::uint32_t> depth;        ///< per row, for diagnostics and D

    [[nodiscard]] std::size_t size() const { return local.size(); }
    [[nodiscard]] std::size_t levels() const
    {
        return level_start.empty() ? 0u : level_start.size() - 1u;
    }
};

/// A tiny LCG, so a shape is reproducible on every machine.
class rng
{
public:
    explicit constexpr rng(std::uint32_t seed) : state_(seed) {}

    [[nodiscard]] std::uint32_t next()
    {
        state_ = state_ * 1664525u + 1013904223u;
        return state_ >> 8;
    }

    [[nodiscard]] float unit() { return static_cast<float>(next()) / 16777216.0f; }

private:
    std::uint32_t state_;
};

// ---------------------------------------------------------------------------
//  Building a shape
// ---------------------------------------------------------------------------

/// Rebuild every index from `parent_row` alone.
///
/// Called after the parent links are decided (and again after a permutation), so
/// that no arm can be accidentally handed a stale index. Two passes for the CSR,
/// one counting sort for the levels.
inline void reindex(world& w)
{
    const std::uint32_t n = static_cast<std::uint32_t>(w.local.size());

    // ---- depth, by memoised chase ----------------------------------------
    //
    // Each row's depth is its parent's plus one. Walking up from every row would
    // be O(n * depth); remembering the answer makes it O(n) amortised, and the
    // explicit stack means an adversarial chain cannot blow the C++ stack.
    w.depth.assign(n, 0xFFFFFFFFu);
    std::vector<std::uint32_t> stack;
    for (std::uint32_t r = 0; r < n; ++r)
    {
        if (w.depth[r] != 0xFFFFFFFFu) { continue; }
        stack.clear();
        std::uint32_t cur = r;
        while (cur != k_root && w.depth[cur] == 0xFFFFFFFFu)
        {
            stack.push_back(cur);
            cur = w.parent_row[cur];
        }
        std::uint32_t d = (cur == k_root) ? 0xFFFFFFFFu : w.depth[cur];
        while (!stack.empty())
        {
            d = (d == 0xFFFFFFFFu) ? 0u : d + 1u;
            w.depth[stack.back()] = d;
            stack.pop_back();
        }
    }

    std::uint32_t max_depth = 0;
    for (std::uint32_t r = 0; r < n; ++r) { max_depth = std::max(max_depth, w.depth[r]); }

    // ---- level buckets, by counting sort ---------------------------------
    w.level_start.assign(max_depth + 2u, 0u);
    for (std::uint32_t r = 0; r < n; ++r) { ++w.level_start[w.depth[r] + 1u]; }
    for (std::size_t i = 1; i < w.level_start.size(); ++i)
    {
        w.level_start[i] += w.level_start[i - 1u];
    }
    w.order.assign(n, 0u);
    {
        std::vector<std::uint32_t> cursor(w.level_start.begin(), w.level_start.end() - 1);
        for (std::uint32_t r = 0; r < n; ++r) { w.order[cursor[w.depth[r]]++] = r; }
    }

    // ---- children, as CSR -------------------------------------------------
    w.child_start.assign(n + 1u, 0u);
    w.roots.clear();
    for (std::uint32_t r = 0; r < n; ++r)
    {
        if (w.parent_row[r] == k_root) { w.roots.push_back(r); }
        else { ++w.child_start[w.parent_row[r] + 1u]; }
    }
    for (std::uint32_t r = 0; r < n; ++r) { w.child_start[r + 1u] += w.child_start[r]; }
    w.children.assign(w.child_start[n], 0u);
    {
        std::vector<std::uint32_t> cursor(w.child_start.begin(), w.child_start.end() - 1);
        for (std::uint32_t r = 0; r < n; ++r)
        {
            if (w.parent_row[r] != k_root) { w.children[cursor[w.parent_row[r]]++] = r; }
        }
    }
}

/// A forest of `n` rows, `depth` levels deep, with the branching factor implied.
///
/// `scramble` decides whether row order is creation order (a world built all at
/// once, parents naturally before children) or arbitrary (a world that has been
/// running). Both are real worlds and they measure differently; see the fairness
/// note at the top of the file.
[[nodiscard]] inline world make_forest(std::size_t n, int depth, bool scramble,
                                       std::uint32_t seed = 0xA11CEu)
{
    world w;
    w.local.resize(n);
    w.parent_row.assign(n, k_root);
    w.out.assign(n, engine::mat4::identity());

    rng r{seed};

    // Lay the tree out in creation order first: level 0 is the first slice, and
    // every later row picks a parent uniformly from the level above. The result
    // is a forest with `depth` levels and a roughly even branching factor,
    // without having to solve for one.
    const std::size_t per_level = (depth > 0) ? (n / static_cast<std::size_t>(depth)) : n;
    std::vector<std::uint32_t> level_lo(static_cast<std::size_t>(depth) + 1u, 0u);
    for (int d = 0; d <= depth; ++d)
    {
        level_lo[static_cast<std::size_t>(d)] =
            static_cast<std::uint32_t>(std::min(n, per_level * static_cast<std::size_t>(d)));
    }
    level_lo[static_cast<std::size_t>(depth)] = static_cast<std::uint32_t>(n);

    for (int d = 1; d < depth; ++d)
    {
        const std::uint32_t lo = level_lo[static_cast<std::size_t>(d)];
        const std::uint32_t hi = level_lo[static_cast<std::size_t>(d) + 1u];
        const std::uint32_t plo = level_lo[static_cast<std::size_t>(d) - 1u];
        const std::uint32_t phi = lo;
        if (phi <= plo) { continue; }
        for (std::uint32_t i = lo; i < hi; ++i)
        {
            w.parent_row[i] = plo + (r.next() % (phi - plo));
        }
    }

    // Something for the arms to actually multiply. Small angles and offsets, so
    // the composed matrices stay in a sane numeric range at depth 16.
    for (std::size_t i = 0; i < n; ++i)
    {
        const float a = r.unit() * 0.7f;
        w.local[i].position = {r.unit() * 2.0f - 1.0f, r.unit() * 2.0f - 1.0f, r.unit() * 0.5f};
        w.local[i].rotation = engine::rotation_y(a) * engine::rotation_x(a * 0.5f);
        w.local[i].scale = {1.0f, 1.0f, 1.0f};
    }

    if (scramble)
    {
        // A Fisher-Yates permutation of the ROWS, with the parent links carried
        // along. The tree is unchanged; only where each node lives is.
        std::vector<std::uint32_t> new_row(n);
        for (std::uint32_t i = 0; i < n; ++i) { new_row[i] = i; }
        for (std::uint32_t i = static_cast<std::uint32_t>(n); i > 1u; --i)
        {
            const std::uint32_t j = r.next() % i;
            std::swap(new_row[i - 1u], new_row[j]);
        }
        // new_row[old] = the row `old` now lives at.
        std::vector<engine::transform> local2(n);
        std::vector<std::uint32_t> parent2(n, k_root);
        for (std::uint32_t old = 0; old < n; ++old)
        {
            const std::uint32_t nw = new_row[old];
            local2[nw] = w.local[old];
            parent2[nw] = (w.parent_row[old] == k_root) ? k_root : new_row[w.parent_row[old]];
        }
        w.local.swap(local2);
        w.parent_row.swap(parent2);
    }

    reindex(w);
    return w;
}

/// Permute the rows into level order — arm C's precondition.
///
/// **This is not free and it is not timed inside the resolve.** It is the cost a
/// world pays when its shape changes, and §4 of the lesson prices it separately.
/// A benchmark that hid it here would be measuring a world that never spawns
/// anything, which is not a world.
inline void permute_to_level_order(world& w)
{
    const std::uint32_t n = static_cast<std::uint32_t>(w.local.size());
    std::vector<std::uint32_t> new_row(n);
    for (std::uint32_t i = 0; i < n; ++i) { new_row[w.order[i]] = i; }

    std::vector<engine::transform> local2(n);
    std::vector<std::uint32_t> parent2(n, k_root);
    for (std::uint32_t old = 0; old < n; ++old)
    {
        const std::uint32_t nw = new_row[old];
        local2[nw] = w.local[old];
        parent2[nw] = (w.parent_row[old] == k_root) ? k_root : new_row[w.parent_row[old]];
    }
    w.local.swap(local2);
    w.parent_row.swap(parent2);
    reindex(w);
}

// ---------------------------------------------------------------------------
//  The four arms
// ---------------------------------------------------------------------------

/// The composition every arm performs, spelled once so no arm can differ in it.
///
/// `world_from_local = world_from_parent * parent_from_local`. Sixty-four
/// multiplies and forty-eight adds, identical operands in identical order in
/// every arm — which is why the OUTPUT is bit-identical across arms even though
/// the running checksum is not.
[[nodiscard]] inline engine::mat4 compose(const engine::mat4& parent_world,
                                          const engine::transform& local)
{
    return parent_world * engine::parent_from_local(local);
}

/// ARM A — recurse from the roots, depth first.
///
/// An explicit stack rather than actual recursion, for two reasons that both
/// matter: a 16-deep tree is fine but a pathological one is not, and a hand-rolled
/// stack removes call-frame overhead from the comparison so that what is left is
/// the ACCESS PATTERN, which is the thing under test.
[[nodiscard]] inline double resolve_recursive(world& w)
{
    double acc = 0.0;
    std::vector<std::uint32_t> stack;
    stack.reserve(64);

    for (const std::uint32_t root : w.roots)
    {
        w.out[root] = engine::parent_from_local(w.local[root]);
        acc += static_cast<double>(w.out[root].c3.x);
        stack.push_back(root);

        while (!stack.empty())
        {
            const std::uint32_t r = stack.back();
            stack.pop_back();
            for (std::uint32_t k = w.child_start[r]; k < w.child_start[r + 1u]; ++k)
            {
                const std::uint32_t c = w.children[k];
                w.out[c] = compose(w.out[r], w.local[c]);
                acc += static_cast<double>(w.out[c].c3.x);
                stack.push_back(c);
            }
        }
    }
    return acc;
}

/// ARM B — level order, through an index array, arrays left where they are.
///
/// One flat loop per level, and the only thing the loop needs to know is that
/// everything it reads was written by an EARLIER level. No recursion, no stack,
/// and — the part that pays off in Module 9 — every iteration within a level is
/// independent of every other, so a level is a `parallel_for` waiting to happen.
[[nodiscard]] inline double resolve_levels(world& w)
{
    double acc = 0.0;
    for (std::size_t lvl = 0; lvl < w.levels(); ++lvl)
    {
        const std::uint32_t lo = w.level_start[lvl];
        const std::uint32_t hi = w.level_start[lvl + 1u];
        for (std::uint32_t i = lo; i < hi; ++i)
        {
            const std::uint32_t r = w.order[i];
            const std::uint32_t p = w.parent_row[r];
            w.out[r] = (p == k_root) ? engine::parent_from_local(w.local[r])
                                     : compose(w.out[p], w.local[r]);
            acc += static_cast<double>(w.out[r].c3.x);
        }
    }
    return acc;
}

/// ARM C — level order with the rows already permuted into it.
///
/// Identical arithmetic to B; the difference is that `order[i] == i`, so the loop
/// reads `local` and writes `out` STRAIGHT DOWN the arrays. Only the parent
/// lookup still jumps, and it jumps backwards into rows the previous level just
/// touched.
///
/// **Requires `permute_to_level_order(w)` first**, and asserting that in a
/// benchmark would cost the thing being measured, so verify_59 asserts it instead.
[[nodiscard]] inline double resolve_levels_packed(world& w)
{
    double acc = 0.0;
    const std::uint32_t n = static_cast<std::uint32_t>(w.local.size());
    for (std::uint32_t r = 0; r < n; ++r)
    {
        const std::uint32_t p = w.parent_row[r];
        w.out[r] = (p == k_root) ? engine::parent_from_local(w.local[r])
                                 : compose(w.out[p], w.local[r]);
        acc += static_cast<double>(w.out[r].c3.x);
    }
    return acc;
}

/// ARM D — level order, skipping rows whose subtree did not move.
///
/// `dirty` is per row and means "this row's LOCAL changed". A row must also be
/// recomputed when an ancestor moved, and because the walk is in level order that
/// falls out for free: mark a child dirty as you pass its parent, and the parent
/// has always been visited first.
///
/// This is the only arm whose cost depends on what the GAME did rather than on
/// what the scene is, which is why it is measured against a fraction rather than
/// against a size.
[[nodiscard]] inline double resolve_dirty(world& w, std::vector<std::uint8_t>& dirty)
{
    double acc = 0.0;
    for (std::size_t lvl = 0; lvl < w.levels(); ++lvl)
    {
        const std::uint32_t lo = w.level_start[lvl];
        const std::uint32_t hi = w.level_start[lvl + 1u];
        for (std::uint32_t i = lo; i < hi; ++i)
        {
            const std::uint32_t r = w.order[i];
            if (dirty[r] == 0u) { continue; }
            const std::uint32_t p = w.parent_row[r];
            w.out[r] = (p == k_root) ? engine::parent_from_local(w.local[r])
                                     : compose(w.out[p], w.local[r]);
            acc += static_cast<double>(w.out[r].c3.x);
            for (std::uint32_t k = w.child_start[r]; k < w.child_start[r + 1u]; ++k)
            {
                dirty[w.children[k]] = 1u;
            }
        }
    }
    return acc;
}

/// Mark `fraction` of rows dirty, deterministically. The propagation to children
/// is `resolve_dirty`'s job, not the caller's — which is the point of the design.
inline void mark_dirty(const world& w, std::vector<std::uint8_t>& dirty, double fraction,
                       std::uint32_t seed = 0xD117u)
{
    const std::uint32_t n = static_cast<std::uint32_t>(w.local.size());
    dirty.assign(n, 0u);
    rng r{seed};
    const std::uint32_t threshold = static_cast<std::uint32_t>(fraction * 16777216.0);
    for (std::uint32_t i = 0; i < n; ++i)
    {
        if (r.next() < threshold) { dirty[i] = 1u; }
    }
}

/// How many rows arm D will actually touch, given `dirty` — computed the slow
/// obvious way, so a table can report work done rather than work requested.
[[nodiscard]] inline std::size_t dirty_reach(const world& w, std::vector<std::uint8_t> dirty)
{
    std::size_t n_touched = 0;
    for (std::size_t lvl = 0; lvl < w.levels(); ++lvl)
    {
        for (std::uint32_t i = w.level_start[lvl]; i < w.level_start[lvl + 1u]; ++i)
        {
            const std::uint32_t r = w.order[i];
            if (dirty[r] == 0u) { continue; }
            ++n_touched;
            for (std::uint32_t k = w.child_start[r]; k < w.child_start[r + 1u]; ++k)
            {
                dirty[w.children[k]] = 1u;
            }
        }
    }
    return n_touched;
}

}   // namespace hier
