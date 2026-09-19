// engine/src/phys/broadphase.cpp — the grid, in four linear passes.
//
// Lesson 8.8. Read `broadphase.hpp` first; it carries the argument, and this
// file carries the loops. What is worth watching here is how little of it is
// about geometry: one AABB test, one floor, and everything else is a counting
// sort and an index.

#include <engine/phys/broadphase.hpp>

#include <engine/phys/collide.hpp>

#include <algorithm>   // std::max

namespace engine::phys
{
namespace
{

/// The next power of two at or above `v`, with a floor of 16.
///
/// A power-of-two table lets the hash reduce with `& (n - 1)` instead of `%`,
/// which is a couple of nanoseconds per entry and matters at a hundred thousand
/// entries a frame. The floor keeps a four-object scene from thrashing a
/// two-bucket table.
std::uint32_t round_up_pow2(std::uint32_t v)
{
    if (v < 16u) { return 16u; }
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1u;
}

} // namespace

// ---------------------------------------------------------------------------
// The reference
// ---------------------------------------------------------------------------

int brute_force_pairs(std::span<const proxy> proxies, std::vector<broadphase_pair>& out)
{
    out.clear();
    const std::size_t n = proxies.size();

    // `j = i + 1` is the whole of why this is n(n-1)/2 and not n²: a pair is
    // unordered, and starting the inner loop past the outer index visits each
    // one once, in a canonical order that already satisfies `a < b` when the
    // proxies are given in index order. They may not be, so `emit` still sorts.
    for (std::size_t i = 0; i < n; ++i)
    {
        for (std::size_t j = i + 1; j < n; ++j)
        {
            if (!overlaps(proxies[i].box, proxies[j].box)) { continue; }

            const std::uint32_t a = proxies[i].index;
            const std::uint32_t b = proxies[j].index;
            out.push_back(a < b ? broadphase_pair{a, b} : broadphase_pair{b, a});
        }
    }
    return static_cast<int>(out.size());
}

// ---------------------------------------------------------------------------
// Tuning
// ---------------------------------------------------------------------------

float auto_cell_size(std::span<const proxy> proxies)
{
    if (proxies.empty()) { return 1.0f; }

    double total = 0.0;
    for (const proxy& p : proxies)
    {
        if (p.box.empty()) { continue; }
        const vec3 e = p.box.extent();
        total += static_cast<double>(std::max(e.x, std::max(e.y, e.z)));
    }

    const double mean = total / static_cast<double>(proxies.size());

    // A scene of points — every box an exact plane or a single vertex — has a
    // mean extent of zero and would divide by it. One metre is arbitrary and is
    // supposed to be: there is no information in the input to choose better, and
    // the grid still works, it just puts everything in one cell.
    if (!(mean > 1e-6)) { return 1.0f; }

    // ONE AND A HALF, and the number came out of §5's cost model rather than out
    // of a book. The model's optimum sits at 2.13x the mean side on a sparse
    // scene and 1.32x on one four times denser, because the "cell too large"
    // wing of the cost grows with crowding and the "cell too small" wing does
    // not. 1.5 is between them, and the floor is flat enough that being between
    // them is worth more than being exactly on either.
    return static_cast<float>(mean * 1.5);
}

// ---------------------------------------------------------------------------
// The grid
// ---------------------------------------------------------------------------

void uniform_grid::clear()
{
    boxes_.clear();
    ids_.clear();
    ranges_.clear();
    big_.clear();
    big_flag_.clear();
    entries_.clear();
    starts_.clear();
    fill_.clear();
    pairs_.clear();
    boxes_.shrink_to_fit();
    entries_.shrink_to_fit();
    starts_.shrink_to_fit();
    pairs_.shrink_to_fit();
    stats_ = {};
}

void uniform_grid::emit(std::uint32_t item_a, std::uint32_t item_b)
{
    const std::uint32_t a = ids_[item_a];
    const std::uint32_t b = ids_[item_b];
    pairs_.push_back(a < b ? broadphase_pair{a, b} : broadphase_pair{b, a});
}

void uniform_grid::build(std::span<const proxy> proxies, const broadphase_config& cfg)
{
    // Every vector here is `clear()`ed rather than reconstructed, which is what
    // makes a rebuilt-every-frame grid allocation-free after the first few
    // frames: `clear` on a vector of trivially destructible elements keeps the
    // capacity. §10 checks that claim with a counting `operator new` rather than
    // asserting it — zero allocations over two thousand rebuilds.
    boxes_.clear();
    ids_.clear();
    ranges_.clear();
    big_.clear();
    big_flag_.clear();
    entries_.clear();
    pairs_.clear();
    stats_ = {};

    stats_.proxies = static_cast<int>(proxies.size());
    if (proxies.empty()) { return; }

    cell_ = cfg.cell_size > 0.0f ? cfg.cell_size : auto_cell_size(proxies);
    stats_.cell_size = cell_;

    // ---- Pass 1: fatten, classify, and count the entries -------------------
    //
    // Two lists come out of this pass: the gridded proxies, with their cell
    // ranges, and the oversized ones. Nothing is inserted yet, because the table
    // has to be sized against the total first.
    std::size_t wanted_entries = 0;
    for (const proxy& p : proxies)
    {
        if (p.box.empty()) { continue; }

        aabb box = p.box;
        if (cfg.margin > 0.0f) { box.grow(cfg.margin); }

        cell_range r;
        std::int64_t cells = 1;
        const vec3 lo = box.min;
        const vec3 hi = box.max;
        const float lo_a[3] = {lo.x, lo.y, lo.z};
        const float hi_a[3] = {hi.x, hi.y, hi.z};
        for (int axis = 0; axis < 3; ++axis)
        {
            r.lo[axis] = cell_of(lo_a[axis], cell_);
            r.hi[axis] = cell_of(hi_a[axis], cell_);
            cells *= static_cast<std::int64_t>(r.hi[axis] - r.lo[axis]) + 1;
        }

        boxes_.push_back(box);
        ids_.push_back(p.index);
        ranges_.push_back(r);

        // THE TEAPOT GUARD. Past the limit the proxy is not gridded at all —
        // note that it still goes into `boxes_`/`ids_`, because the oversized
        // list is tested against EVERY proxy including the other oversized ones,
        // and dropping it here would be the one thing the contract forbids.
        if (cells > static_cast<std::int64_t>(cfg.max_cells_per_proxy))
        {
            big_.push_back(static_cast<std::uint32_t>(boxes_.size() - 1));
            big_flag_.push_back(1u);
        }
        else
        {
            big_flag_.push_back(0u);
            wanted_entries += static_cast<std::size_t>(cells);
        }
    }

    stats_.oversized = static_cast<int>(big_.size());
    stats_.entries = static_cast<int>(wanted_entries);

    // ---- Pass 2: size the table and count each bucket ----------------------
    //
    // Two entries per bucket on average, and §7 measures why that is the right
    // number rather than asserting it. On a lattice of crates the buckets
    // holding more than one cell go 346 at 1:1, **254 at 1:2**, 176 at 1:4 and
    // 156 at 1:8 — so the second doubling buys 31% for twice the memory and the
    // third buys 11% for twice again. Two is where the return per byte stops
    // being worth a colder table.
    //
    // The same table also shows the xor hash's signature, which is worth seeing
    // once: random hashing would give 323, 202, 113 and 60, so the GAP widens as
    // the table grows. A bigger table cannot fix a hash that is not spreading
    // the low bits, and buying one is how people conclude that hashing is
    // expensive.
    const std::uint32_t buckets =
        wanted_entries == 0 ? 16u : round_up_pow2(static_cast<std::uint32_t>(wanted_entries * 2));
    const std::uint32_t mask = buckets - 1u;
    stats_.buckets = static_cast<int>(buckets);

    starts_.assign(static_cast<std::size_t>(buckets) + 1u, 0u);

    const std::size_t gridded = boxes_.size();
    entries_.resize(wanted_entries);

    // A first walk that only counts. It repeats the triple loop below, which
    // looks wasteful and is not: the alternative is a vector per bucket, which
    // allocates `buckets` times and scatters the grid across the heap. Two cheap
    // passes over contiguous memory beat one pass that allocates, every time.
    for (std::size_t item = 0; item < gridded; ++item)
    {
        if (big_flag_[item] != 0u) { continue; }

        const cell_range& r = ranges_[item];
        for (std::int32_t z = r.lo[2]; z <= r.hi[2]; ++z)
        {
            for (std::int32_t y = r.lo[1]; y <= r.hi[1]; ++y)
            {
                for (std::int32_t x = r.lo[0]; x <= r.hi[0]; ++x)
                {
                    ++starts_[(hash_cell(x, y, z) & mask) + 1u];
                }
            }
        }
    }

    // ---- Pass 3: prefix sum, then scatter ----------------------------------
    //
    // A counting sort. After the prefix sum, `starts_[b]` is where bucket `b`
    // begins in `entries_` and `starts_[b + 1]` is where it ends, so the whole
    // grid is ONE contiguous array in which every bucket's members are adjacent
    // — which is the property the pair loop needs and the reason this is not a
    // vector of vectors.
    for (std::uint32_t b = 0; b < buckets; ++b) { starts_[b + 1u] += starts_[b]; }

    fill_.assign(starts_.begin(), starts_.end() - 1);

    for (std::size_t item = 0; item < gridded; ++item)
    {
        if (big_flag_[item] != 0u) { continue; }

        const cell_range& r = ranges_[item];
        for (std::int32_t z = r.lo[2]; z <= r.hi[2]; ++z)
        {
            for (std::int32_t y = r.lo[1]; y <= r.hi[1]; ++y)
            {
                for (std::int32_t x = r.lo[0]; x <= r.hi[0]; ++x)
                {
                    const std::uint32_t b = hash_cell(x, y, z) & mask;
                    entries_[fill_[b]++] = entry{x, y, z, static_cast<std::uint32_t>(item)};
                }
            }
        }
    }

    // ---- Pass 4: the pairs -------------------------------------------------
    for (std::uint32_t b = 0; b < buckets; ++b)
    {
        const std::uint32_t begin = starts_[b];
        const std::uint32_t end = starts_[b + 1u];
        const std::uint32_t n = end - begin;
        if (n == 0u) { continue; }

        ++stats_.occupied_buckets;
        stats_.largest_bucket = std::max(stats_.largest_bucket, static_cast<int>(n));

        // Does this bucket hold more than one cell? A hash collision, counted
        // here because it is free here — the entries are already in hand.
        bool mixed = false;
        for (std::uint32_t i = begin + 1u; i < end && !mixed; ++i)
        {
            mixed = entries_[i].cx != entries_[begin].cx || entries_[i].cy != entries_[begin].cy ||
                    entries_[i].cz != entries_[begin].cz;
        }
        if (mixed) { ++stats_.colliding_buckets; }

        for (std::uint32_t i = begin; i < end; ++i)
        {
            const entry& ea = entries_[i];
            for (std::uint32_t j = i + 1u; j < end; ++j)
            {
                const entry& eb = entries_[j];
                ++stats_.bucket_tests;

                // 1. SAME CELL? Two entries in one bucket may be from unrelated
                //    cells that happened to hash together. Rejecting the pair
                //    here is safe: if the two proxies really do share a cell,
                //    that cell's own bucket will produce them.
                if (ea.cx != eb.cx || ea.cy != eb.cy || ea.cz != eb.cz)
                {
                    ++stats_.cell_rejects;
                    continue;
                }

                // 2. IS THIS CELL THE OWNER? The minimum corner of the overlap
                //    of the two cell ranges, which is the one cell in which this
                //    pair is allowed to be reported. Three `max` calls and the
                //    duplicates are gone. See `owner_cell`'s comment.
                const cell_range& ra = ranges_[ea.item];
                const cell_range& rb = ranges_[eb.item];
                if (ea.cx != owner_cell(ra.lo[0], rb.lo[0]) ||
                    ea.cy != owner_cell(ra.lo[1], rb.lo[1]) ||
                    ea.cz != owner_cell(ra.lo[2], rb.lo[2]))
                {
                    ++stats_.owner_rejects;
                    continue;
                }

                // 3. DO THE BOXES ACTUALLY OVERLAP? Sharing a cell means being
                //    within a cell of each other, which is not the same thing.
                //    Six comparisons to avoid a 395 ns narrow-phase call.
                if (!overlaps(boxes_[ea.item], boxes_[eb.item]))
                {
                    ++stats_.box_rejects;
                    continue;
                }

                emit(ea.item, eb.item);
            }
        }
    }

    // ---- The oversized list ------------------------------------------------
    //
    // Everything on it, against everything. Linear in `big_.size() * gridded`,
    // which is the price of admitting the grid cannot help — and note the inner
    // loop runs over ALL items, so two oversized proxies test each other twice.
    // The `item_b > item_a` guard below is what stops that, and it is why the
    // loop starts at zero rather than at `item_a + 1`: a big proxy must still
    // see the small ones that come before it in the array.
    for (const std::uint32_t item_a : big_)
    {
        for (std::uint32_t item_b = 0; item_b < gridded; ++item_b)
        {
            if (item_b == item_a) { continue; }

            if (big_flag_[item_b] != 0u && item_b < item_a) { continue; }  // counted from the other side

            if (!overlaps(boxes_[item_a], boxes_[item_b])) { continue; }

            emit(item_a, item_b);
            ++stats_.oversized_pairs;
        }
    }

    stats_.pairs = static_cast<int>(pairs_.size());
}

} // namespace engine::phys
