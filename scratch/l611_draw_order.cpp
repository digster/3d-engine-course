// engine/src/gfx/draw_order.cpp — twenty lines, and the argument is all in the header.

#include <engine/gfx/draw_order.hpp>

#include <algorithm>

namespace engine {

order_report order_draws(std::span<draw_key> keys)
{
    order_report rep{};

    // ---- The partition -----------------------------------------------------
    //
    // `std::stable_partition` and not a two-pass copy, because the caller's
    // opaque ordering is worth preserving (see the header) and because this is
    // the operation the standard library has a name for. The predicate is
    // "everything that is NOT blended", which puts masked geometry with the
    // opaque geometry — the single most important line in this file, and the one
    // most likely to be written the other way by somebody who reads
    // `alpha_mode` as a scale from solid to see-through.
    const auto first_blended = std::stable_partition(
        keys.begin(), keys.end(),
        [](const draw_key& k) { return k.mode != alpha_mode::blend; });

    for (auto it = keys.begin(); it != first_blended; ++it)
    {
        ++rep.opaque;
        if (it->mode == alpha_mode::mask) { ++rep.masked; }
    }
    rep.blended = static_cast<int>(keys.end() - first_blended);

    // ---- What the sort was worth, measured before it runs ------------------
    //
    // One pass, adjacent pairs only. See `order_report::out_of_order` for what
    // this number does and does not claim.
    for (auto it = first_blended; it != keys.end() && it + 1 != keys.end(); ++it)
    {
        if ((it + 1)->depth > it->depth) { ++rep.out_of_order; }
    }

    // ---- Back to front -----------------------------------------------------
    //
    // `>` gives descending depth: the farthest object first, so that everything
    // nearer composites over it. Stable, for the equal-depth case.
    std::stable_sort(first_blended, keys.end(),
                     [](const draw_key& a, const draw_key& b) { return a.depth > b.depth; });

    return rep;
}

} // namespace engine
