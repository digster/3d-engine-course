// engine/include/engine/phys/broadphase.hpp — n² is a promise you cannot keep.
//
// Lesson 8.8. Four lessons of narrow phase have built something precise: 8.4's
// SAT, 8.5's GJK, 8.6's EPA and 8.7's manifold together answer, for ONE pair of
// shapes, whether they touch, how deep, which way, and at which points. 8.7 §9
// measured the whole chain at 395 ns, and 8.8 §1 re-measures it on 8.7's own
// fixture at **432 ns** — the same number, on the same machine, a lesson later.
//
// That number is the problem. A pair test costs 432 ns; a scene of `n` bodies
// has `n(n-1)/2` pairs; and `n(n-1)/2 * 432 ns` passes a 16.7 ms frame at
// **n = 278**. Not at ten thousand, not at "some large number" — at two hundred
// and seventy-eight boxes, which is a small pile of crates. The quadratic does
// not arrive later. It is already here.
//
// ---- THE SHAPE OF THE FIX --------------------------------------------------
//
// Almost every one of those pairs is two objects at opposite ends of the room,
// and the arithmetic that establishes they are far apart is the same arithmetic
// that would establish they are touching. A broadphase is the stage that answers
// the cheap question — *could these two possibly touch?* — for every pair, and
// hands the narrow phase only the survivors.
//
// It gets to be cheap because it is allowed to be wrong in ONE direction:
//
//     A BROADPHASE MAY REPORT PAIRS THAT DO NOT TOUCH.
//     A BROADPHASE MAY NOT OMIT A PAIR THAT DOES.
//
// That asymmetry is the entire design, and every decision in this file falls out
// of it. A false positive costs one narrow-phase call, which is 432 ns and a
// correct answer. A false negative is an object falling through the floor, with
// nothing downstream able to notice. So every approximation here is made in the
// conservative direction, deliberately, and 8.8 §3 checks it by running the
// grid's output against `brute_force_pairs` on every frame of a test scene and
// requiring one to be a superset of the other.
//
// ---- WHY A GRID -----------------------------------------------------------
//
// The three broadphases you will meet in real engines:
//
//   SWEEP AND PRUNE keeps the proxies sorted along an axis and exploits the fact
//     that a sorted list barely changes between frames. Excellent for scenes
//     that are wide in one axis and shallow in the others — which is most
//     shooters, where everything stands on a floor. Its worst case is a scene
//     with a shared coordinate on the sweep axis, and "everything standing on a
//     floor" is also exactly that.
//   A BVH (a tree of boxes) is what a static world uses, because a tree you
//     build once and never move is the cheapest query there is. Refitting one
//     every frame for moving bodies is where it stops being free.
//   A UNIFORM GRID cuts space into equal boxes and asks each object which boxes
//     it overlaps. Build is O(n) with no comparisons and no tree, the query is
//     an array index, and there is nothing to keep sorted or rebalanced.
//
// This engine builds the grid, for the reason the curriculum gives it: it is the
// cheapest possible fix, and the interesting question is not how to write it but
// **when it stops being enough**, which is a measurement rather than an opinion.
// 8.8 §9 finds the exact scene that breaks it and §10 measures the crossover
// below which the grid is SLOWER than the quadratic it replaces — because it is,
// and the number is larger than most people guess.
//
// ---- THE TWO THINGS THAT ARE ACTUALLY HARD ---------------------------------
//
// Writing "hash the cell, put the object in it" takes twenty minutes. The two
// problems that take the rest of the lesson:
//
//   DUPLICATES. An object spanning four cells meets its neighbour in all four,
//     and reports the pair four times. A `std::set` of pair keys fixes it and
//     costs **45% of what the entire broadphase costs**. `owner_cell` below
//     fixes it in three `max` calls and no memory at all — see its comment,
//     which is the piece of this file worth reading twice. §5 predicts the
//     duplicate count in closed form and checks it against the counter the grid
//     keeps: **3271 and 3271**, exactly.
//   THE TEAPOT IN THE STADIUM. One object a kilometre wide in a scene of
//     one-metre crates occupies a million cells on its own. A uniform grid has
//     no answer to this; it is the grid's defining weakness and the reason
//     hierarchical structures exist. What it has instead is a GUARD:
//     `max_cells_per_proxy`, past which a proxy stops being gridded and is
//     tested against everything. §9 measures both halves — the explosion with
//     the guard off, and what the guard costs with it on.
//
// ---- WHAT THIS FILE PROMISES TO 8.9 AND 8.10 -------------------------------
//
// `broadphase_pair` carries `a < b` always, and that is a contract rather than a
// tidiness preference. 8.7's `pair_key(a, b)` is order-INDEPENDENT and its
// manifold is NOT: generate a manifold for `(7, 3)` on one frame and `(3, 7)` on
// the next and the normal reverses, the reference face moves to the other shape,
// and every warm start on that pair is lost — a stack that mysteriously will not
// settle, three lessons downstream from the line that caused it. Sorting the
// pair here, once, at the only place pairs are made, is the cheapest possible
// place to fix that.

#pragma once

#include <engine/math/bounds.hpp>
#include <engine/math/vec3.hpp>

#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace engine::phys
{

// ---------------------------------------------------------------------------
// What goes in, what comes out
// ---------------------------------------------------------------------------

/// **One body, as the broadphase sees it: a world-space box and an index.**
///
/// Not a shape, not a body, not a pointer to either. The broadphase's whole job
/// is to be cheap, and the way it stays cheap is by knowing as little as
/// possible: six floats and a number that means something to the caller. Give it
/// a `rigid_body*` and it would be tempted to look inside one.
///
/// `index` is whatever the caller uses to name a body — an ECS entity's index, a
/// slot in an array, a handle's index half (5.4). The broadphase never
/// dereferences it and never assumes it is contiguous or small; it only ever
/// copies it into a pair and compares two of them for order.
///
/// **The box is world space and it is the caller's job to keep it current.**
/// `bounds_of(shape, centre, orientation)` (8.4) is the usual way to get one.
/// See `broadphase_config::margin` for why you might want it slightly too big.
struct proxy
{
    aabb box{};
    std::uint32_t index = 0;
};

/// A candidate pair, **with `a < b` always**.
///
/// The ordering is what lets `pair_key(p.a, p.b)` index 8.7's `manifold_cache`
/// and lets `collide_manifold` be called with the same shape as `a` every frame.
/// See this file's header for what happens when it is not.
struct broadphase_pair
{
    std::uint32_t a = 0;
    std::uint32_t b = 0;

    [[nodiscard]] friend bool operator==(const broadphase_pair& x, const broadphase_pair& y)
    {
        return x.a == y.a && x.b == y.b;
    }
};

// ---------------------------------------------------------------------------
// Tuning
// ---------------------------------------------------------------------------

/// The upper bound on cells a single proxy may occupy before the grid gives up
/// on it. See `broadphase_config::max_cells_per_proxy`.
///
/// 4096 is 16³, so a proxy sixteen cells wide on every axis still grids. The
/// number is a guard against pathology rather than a tuning knob, and §9
/// measures the cliff it is standing in front of: at a 1 m cell a 200 m ground
/// plane wants **80,802 cells for itself**, nearly three times what a whole
/// scene of two thousand crates wants, and it is alone in almost all of them.
inline constexpr int k_max_cells_per_proxy = 4096;

/// **How the grid is tuned, which is really one number.**
struct broadphase_config
{
    /// Metres. **Zero means "choose one", via `auto_cell_size`.**
    ///
    /// The single decision that matters, and it is a genuine trade with a
    /// measurable minimum rather than a preference:
    ///
    ///   TOO SMALL and every object spans many cells. Insertion cost grows as
    ///     the CUBE of the ratio — halve the cell size and an object occupies
    ///     eight times as many cells — and the duplicate work grows with it.
    ///   TOO LARGE and every cell holds many objects, and the pair loop inside a
    ///     cell is quadratic in its occupancy. In the limit of one enormous cell
    ///     the grid is exactly the brute force it replaced, plus a hash.
    ///
    /// §6 sweeps it across three decades on two scenes and fits a two-term cost
    /// model to the counts. The minimum is broad — both wings of the curve are
    /// cubic, so the optimum moves as the SIXTH ROOT of the scene's density —
    /// which is the useful practical result: this number needs sanity rather
    /// than tuning. `auto_cell_size` supplies the sanity.
    float cell_size = 0.0f;

    /// Metres to push every face of every box out by before inserting it.
    ///
    /// Zero here, and 8.9 is who will want it non-zero. Two separate uses, and
    /// they are worth keeping apart:
    ///
    ///   SPECULATION. A pair a micron short of touching produces no manifold at
    ///     all (8.7's `manifold_status::none`), so a solver that wants to stop a
    ///     collision BEFORE it happens has to be told about pairs that have not
    ///     happened yet. `gjk.hpp` said this lesson would want a margin; this is
    ///     it.
    ///   HYSTERESIS. A proxy fattened by more than one frame's motion can be
    ///     left alone across frames in which the body barely moved, which is the
    ///     door to an incremental broadphase. This grid rebuilds from scratch
    ///     every frame and does not use it that way — see `build`.
    ///
    /// It is conservative in the allowed direction: a bigger box reports more
    /// pairs and never fewer — §8 checks that containment at every margin it
    /// measures. What it costs is the cube of the fattened MINKOWSKI SUM: a pair
    /// overlaps when the difference of its centres lies in a box of extent
    /// `e_a + e_b`, and fattening each box by `m` adds `4m` to that, so the pair
    /// count goes as `((E + 4m)/E)³`. Measured within 12% of that at every
    /// margin from 1 cm to 50 cm — at 50 cm on 1.5 m crates, **4.53x the
    /// pairs.**
    float margin = 0.0f;

    /// Past this many cells, a proxy is not gridded at all — it goes on the
    /// oversized list and is tested against every other proxy directly.
    ///
    /// **This is the teapot-in-the-stadium guard and it is not elegant.** It is
    /// what a uniform grid can do about an object far larger than its cell,
    /// which is: notice, and stop pretending. The contract survives — an
    /// oversized proxy is tested against *everything*, so no pair is lost — and
    /// the cost is linear in the number of oversized proxies times `n`, which is
    /// fine for the one ground plane and one terrain chunk a real scene has, and
    /// is a disaster if half the scene trips it. §9 measures the shape of that
    /// disaster.
    int max_cells_per_proxy = k_max_cells_per_proxy;
};

// ---------------------------------------------------------------------------
// What it did
// ---------------------------------------------------------------------------

/// **Where the frame went.** Every field is a count the grid already had to
/// keep, or one line to keep.
///
/// This struct exists because of 3.10's rule and because a broadphase is the
/// system in this engine where intuition is least reliable: the difference
/// between a good cell size and a bad one is invisible in the output — the pair
/// list is identical — and visible only here, in `entries` and `bucket_tests`.
struct broadphase_stats
{
    int proxies = 0;               ///< How many went in.
    float cell_size = 0.0f;        ///< What was used, after `auto_cell_size`.

    /// Total (proxy, cell) insertions. **The cost of the build**, and the number
    /// that explodes when the cell size is too small.
    int entries = 0;

    int buckets = 0;               ///< Hash table size. A power of two.
    int occupied_buckets = 0;      ///< How many hold at least one entry.
    int largest_bucket = 0;        ///< The worst one. Its square is the worst cell's cost.

    /// Buckets holding entries from **more than one distinct cell** — i.e. hash
    /// collisions. Harmless (the cell test below rejects those pairs) but not
    /// free, and §7 measures them, because the textbook hash is worse at this
    /// than it looks — and then measures what that is actually worth, which is
    /// less than the collision count suggests.
    int colliding_buckets = 0;

    /// Pairs of entries examined inside buckets. **The cost of the query.**
    long long bucket_tests = 0;

    /// Of those, how many were rejected by the owner-cell rule — i.e. how many
    /// duplicates the rule removed for free.
    long long owner_rejects = 0;

    /// Rejected because the two entries sat in different cells of the same
    /// bucket. Exactly the cost of hash collisions.
    long long cell_rejects = 0;

    /// Rejected by the AABB overlap test, having survived everything else.
    long long box_rejects = 0;

    int pairs = 0;                 ///< Reported. `pairs()` has this many.
    int oversized = 0;             ///< Proxies that tripped `max_cells_per_proxy`.

    /// Pairs that came from the oversized list rather than from the grid.
    int oversized_pairs = 0;
};

// ---------------------------------------------------------------------------
// The reference
// ---------------------------------------------------------------------------

/// **Every pair whose boxes overlap, found by testing all of them.**
///
/// `n(n-1)/2` AABB tests, in index order, `a < b`. This is here for three
/// reasons and only the third is about production code:
///
///   1. It is the ORACLE. A broadphase is a thing that is allowed to be wrong,
///      so the only way to check one is against something that cannot be. §3
///      runs both on every frame of a tumbling scene and asserts the grid's
///      output contains this one's.
///   2. It is the BASELINE. "The grid is faster" is not a claim until there is a
///      number it is faster than.
///   3. It is the RIGHT ANSWER BELOW SOME n, and the n is not small. An AABB
///      overlap test is six comparisons with no memory traffic at all; a grid
///      build touches every proxy twice and then chases a hash table. §10
///      measures the crossover on this engine, and it is not small: the grid
///      does not start winning until **around a hundred proxies**.
///
/// Returns the number of pairs, which is also `out.size()`. `out` is cleared.
int brute_force_pairs(std::span<const proxy> proxies, std::vector<broadphase_pair>& out);

/// **A cell size chosen from the proxies: one and a half times the mean box's
/// longest side.**
///
/// The rule of thumb every source states is "about the size of an average
/// object". §6 replaces the vague version with a two-term cost model —
/// `a * entries + b * pair tests`, whose two constants are solved exactly from
/// two rows of a sweep and come out the SAME on scenes of different density
/// (8.1 and 5.4 nanoseconds here) because they are properties of the machine
/// rather than of the scene. Minimising it puts the optimum at **2.13x** the
/// mean side on a sparse scene and **1.32x** on one four times denser.
///
/// The optimum moves with density and the formula does not look at density,
/// which sounds like a flaw and is a measurement: both wings of the cost curve
/// are CUBIC in the cell size, so the minimum goes as the SIXTH ROOT of the
/// crowd and barely moves. 1.5 sits between the two measured optima and is
/// within a few percent of each.
///
/// **The LONGEST side and not the mean side**, because a cell has to hold a box
/// in its worst direction. **The MEAN and not the maximum**, because the maximum
/// is one ground plane away from being useless — the same observation
/// `max_cells_per_proxy` is built on, seen from the other end.
///
/// Returns 1.0 for an empty span or for proxies with no extent at all, because
/// a cell size of zero divides.
[[nodiscard]] float auto_cell_size(std::span<const proxy> proxies);

// ---------------------------------------------------------------------------
// The grid
// ---------------------------------------------------------------------------

/// **A hashed uniform grid, rebuilt from scratch every frame.**
///
/// ---- WHY HASHED, AND NOT AN ARRAY OF CELLS -------------------------------
///
/// The obvious grid is a 3-D array indexed by cell coordinate. It needs the
/// world's extent, and it allocates for every cell whether or not anything is in
/// it: a 1 km world at a 1 m cell is 10⁹ cells, which is four gigabytes of
/// indices for a scene of forty crates. Worse, it has a BOUNDARY — an object
/// that walks off the edge of the array has to be clamped, wrapped, or rejected,
/// and the first two are silent false-negative machines.
///
/// Hashing the cell coordinate removes both problems at once. The table is sized
/// against the number of OCCUPIED cells rather than the number of possible ones,
/// so memory follows the scene rather than the world; and the coordinate is an
/// unbounded `int32`, so there is no edge to fall off and no world bounds to
/// declare. A scene that happens to be a kilometre across costs exactly what a
/// scene that happens to be a metre across costs.
///
/// The price is collisions: two unrelated cells can land in the same bucket, and
/// their objects then get compared. That costs a comparison and produces no
/// wrong answers, because every pair carries its cell coordinate and a pair from
/// two different cells is rejected. §7 measures the collision rate, and it is
/// the one place in this file where the textbook answer measured worse than
/// expected — **25.7% above the balls-in-bins prediction** on a lattice of
/// crates, against 3.4% for a stronger mix.
///
/// ---- WHY REBUILT, AND NOT UPDATED -----------------------------------------
///
/// An incremental grid moves only the proxies that changed cell, which sounds
/// strictly better and is not: it needs a per-proxy record of which cells it is
/// in, removal from a hash bucket is the expensive operation in every open
/// scheme, and a physics scene's proxies mostly ALL move. A rebuild is two
/// linear passes over contiguous memory with no removals, no tombstones and no
/// pointer chasing, and it is O(n) rather than O(n log n).
///
/// The same argument 8.7's `manifold_cache` makes, one lesson later and in the
/// same words: a structure that is thrown away every frame never rots.
///
/// ---- USE ------------------------------------------------------------------
///
///     uniform_grid grid;
///     grid.build(proxies, {});                 // cell size chosen for you
///     for (const broadphase_pair& p : grid.pairs()) {
///         // p.a < p.b, and each pair appears exactly once
///     }
///
/// The vectors inside are kept across calls, so a grid rebuilt every frame
/// allocates on the first few frames and then never again. §10 measures that:
/// **zero allocations** over a two-thousand-frame run.
class uniform_grid
{
public:
    /// **Rebuild the grid over these proxies and generate every candidate pair.**
    ///
    /// Four passes, all linear in the entries:
    ///
    ///   1. Fatten each box by `margin`, compute its cell range, and count the
    ///      cells it wants. Proxies past `max_cells_per_proxy` go on the
    ///      oversized list instead.
    ///   2. Size the table, then count how many entries fall in each bucket.
    ///   3. Prefix-sum the counts into offsets and scatter the entries — a
    ///      counting sort, which is how the grid ends up as one contiguous array
    ///      with the members of each bucket adjacent, and no per-cell
    ///      allocation anywhere.
    ///   4. Walk each bucket and emit the surviving pairs, then test every
    ///      oversized proxy against everything.
    ///
    /// `proxies` is not retained; the boxes are copied.
    void build(std::span<const proxy> proxies, const broadphase_config& cfg = {});

    /// This frame's candidate pairs. Valid until the next `build`.
    [[nodiscard]] std::span<const broadphase_pair> pairs() const
    {
        return {pairs_.data(), pairs_.size()};
    }

    [[nodiscard]] const broadphase_stats& stats() const { return stats_; }

    /// Which cell a coordinate is in, at a given cell size.
    ///
    /// **`std::floor` and not a cast to `int`** — and the reason usually given
    /// for that is wrong, which §4 measures rather than repeats. A C++ cast
    /// truncates TOWARD ZERO, so cells −1 and 0 merge into one double-width cell
    /// straddling the origin, and every tutorial that mentions this calls it a
    /// correctness bug. **In a grid that walks an object's whole cell RANGE it
    /// is not one.** The range walk needs its cell map to be MONOTONE and needs
    /// nothing else: if two intervals overlap then their images under a
    /// non-decreasing map overlap too, and truncation is non-decreasing.
    /// Measured on 3000 proxies centred on the origin: **zero pairs lost**.
    ///
    /// What it costs instead is occupancy. Eight cells become one, a cell's pair
    /// loop is quadratic in what is in it, and §4 measures **4.0x** the
    /// comparisons in that one neighbourhood — a footnote, not a bug, and worth
    /// knowing precisely rather than fearing vaguely. Use `floor` anyway: it is
    /// the same instruction count, and a cell map that is uniform everywhere is
    /// one less thing that is only true away from the origin.
    ///
    /// **Defined here rather than in the .cpp, and that is a measurement.** §7
    /// times `hash_cell` at **0.115 ns** inlined and **0.692 ns** behind a call
    /// the compiler is forbidden to inline — the same three multiplies and two
    /// exclusive-ors, **6.0x**, and all of it the call. Inside
    /// `build` it never mattered, because that is the same translation unit; it
    /// matters to everybody else, and these two are the engine's introspection
    /// hooks into the grid, so everybody else is who calls them.
    [[nodiscard]] static std::int32_t cell_of(float x, float cell_size)
    {
        return static_cast<std::int32_t>(std::floor(x / cell_size));
    }

    /// The bucket a cell coordinate hashes to, in a table of `buckets` slots.
    ///
    /// The standard spatial hash (Teschner et al. 2003): multiply each
    /// coordinate by a large prime and exclusive-or the results. It is one
    /// multiply and one xor per axis, which is why everybody uses it.
    ///
    /// **It is also measurably mediocre on exactly the input a physics scene
    /// produces**, which is a lattice of small coordinates rather than random
    /// ones: §7 measures **25.7%** more shared buckets than chance on a lattice
    /// of crates, where a stronger mix is at 3.4%. It stays, and §7 is also why
    /// — the collision excess is worth only **9.4%** of the inner loop, and this
    /// hash is **half the price** of the mix that removes it.
    ///
    /// Inline for the reason `cell_of` above gives.
    [[nodiscard]] static std::uint32_t hash_cell(std::int32_t x, std::int32_t y, std::int32_t z)
    {
        // Cast through `uint32_t` before multiplying: signed overflow is
        // undefined behaviour in C++ and a negative cell coordinate is ordinary
        // here, one metre to the left of the origin.
        const std::uint32_t ux = static_cast<std::uint32_t>(x);
        const std::uint32_t uy = static_cast<std::uint32_t>(y);
        const std::uint32_t uz = static_cast<std::uint32_t>(z);
        return (ux * 73856093u) ^ (uy * 19349663u) ^ (uz * 83492791u);
    }

    /// Release the memory. A rebuild does not need this; a level change does.
    void clear();

private:
    /// One (proxy, cell) insertion. Sixteen bytes, and the layout is deliberate:
    /// the pair loop reads `cx/cy/cz` first (to reject a hash collision) and
    /// `item` only on the ones that survive.
    struct entry
    {
        std::int32_t cx = 0;
        std::int32_t cy = 0;
        std::int32_t cz = 0;
        std::uint32_t item = 0;   ///< Index into `boxes_`/`ranges_`, NOT a body index.
    };

    /// The cell range a proxy occupies, inclusive at both ends.
    struct cell_range
    {
        std::int32_t lo[3]{};
        std::int32_t hi[3]{};
    };

    void emit(std::uint32_t item_a, std::uint32_t item_b);

    std::vector<aabb> boxes_;            ///< Fattened, gridded proxies only.
    std::vector<std::uint32_t> ids_;     ///< Their caller indices.
    std::vector<cell_range> ranges_;
    std::vector<std::uint32_t> big_;     ///< Items on the oversized list.

    /// One byte per item: is it on `big_`? A flag rather than a search, because
    /// both insertion passes ask the question once per item and the oversized
    /// pass asks it once per item per big proxy.
    std::vector<std::uint8_t> big_flag_;

    std::vector<entry> entries_;
    std::vector<std::uint32_t> starts_;  ///< buckets + 1 offsets into `entries_`.
    std::vector<std::uint32_t> fill_;    ///< Scratch for the scatter pass.

    std::vector<broadphase_pair> pairs_;
    broadphase_stats stats_{};
    float cell_ = 1.0f;
};

/// **The cell that owns a pair: the minimum corner of the overlap of their two
/// cell ranges.**
///
/// This is the duplicate fix, and it is worth the paragraph.
///
/// Two proxies whose cell ranges overlap are both present in EVERY cell of the
/// overlap, so a naive walk reports them once per shared cell — four times for
/// two crates meeting at a cell corner, and far worse for large objects. The
/// usual fixes are a `std::set` of pair keys (an allocation and a tree walk per
/// pair) or sorting the pair list and running `unique` (O(p log p) on the thing
/// you were trying to make cheap).
///
/// Neither is needed, because the overlap of two axis-aligned cell RANGES is
/// itself an axis-aligned range, and a range has a unique minimum corner. Report
/// the pair only from that cell and it is reported exactly once — not usually,
/// not with high probability, exactly once — for three `max` calls and no
/// memory. Both proxies are guaranteed to be present in that cell, because it
/// lies in both their ranges by construction.
///
/// **The rule is exact rather than heuristic, and the proof is one line**: the
/// cell it selects is in the overlap, both proxies are inserted into every cell
/// of their own range, and the overlap is a subset of both.
[[nodiscard]] inline std::int32_t owner_cell(std::int32_t lo_a, std::int32_t lo_b)
{
    return lo_a > lo_b ? lo_a : lo_b;
}

} // namespace engine::phys
