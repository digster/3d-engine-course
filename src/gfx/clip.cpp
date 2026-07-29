// src/gfx/clip.cpp — Sutherland–Hodgman against one plane, and what it costs.
//
// The header argues for the algorithm; this file is where the three details that
// are easy to get wrong live:
//
//   1. the crossing parameter is `da / (da - db)`, and it is computed from the
//      SAME two distances the inside test used — not recomputed;
//   2. every component is lerped with that one parameter, `w` included;
//   3. colour is lerped in LINEAR LIGHT, not in stored sRGB values.

#include "gfx/clip.hpp"

#include "gfx/colour.hpp"

namespace engine {

clip_vertex lerp(const clip_vertex& a, const clip_vertex& b, float t)
{
    clip_vertex out;

    // The position, all FOUR components. Interpolating x, y, z and forgetting w is
    // a bug with a strange signature: the new vertex lands in the right place along
    // the edge but with the wrong depth scale, so the triangle is the right shape
    // and the wrong size, and only when it is clipped. Lesson 3.3 §7.
    out.position = a.position + (b.position - a.position) * t;

    out.uv = a.uv + (b.uv - a.uv) * t;

    // Colour, in linear light. `to_linear` undoes the sRGB encoding, the blend
    // happens on actual quantities of light, and `to_encoded` puts it back — the
    // same three steps `fill_triangle` takes per pixel, for the same reason
    // (Lesson 1.6 §3, Lesson 2.4 §3.4). Doing it in stored values instead would
    // darken the new corner, and the darkening would appear only when geometry
    // happened to be clipped, which is the worst kind of bug to chase.
    //
    // The cost is two decodes and an encode per clipped edge. That is per FRAME
    // per clipped triangle, not per pixel, so it does not appear in any profile
    // this course will take.
    const linear_rgb la = to_linear(a.colour);
    const linear_rgb lb = to_linear(b.colour);
    out.colour = to_encoded({la.r + (lb.r - la.r) * t,
                             la.g + (lb.g - la.g) * t,
                             la.b + (lb.b - la.b) * t});

    return out;
}

bool clip_segment_near(clip_vertex& a, clip_vertex& b)
{
    const float da = near_distance(a.position);
    const float db = near_distance(b.position);

    // Both in front: the overwhelmingly common case, and it costs two comparisons.
    // Worth putting first — a renderer where the fast path is the LAST branch is
    // one that pays for the exceptional case on every ordinary one.
    if (da >= 0.0f && db >= 0.0f) { return true; }

    // Both behind: nothing to draw. Note that this is `<` on both, so a segment
    // with an endpoint exactly ON the plane took the branch above.
    if (da < 0.0f && db < 0.0f) { return false; }

    // One of each. Move the outside endpoint onto the plane, leaving the inside
    // one exactly where it was — the visible half of the line must not shift by so
    // much as a rounding, or a wall's edge would crawl as you walked toward it.
    if (da < 0.0f) { a = lerp(a, b, near_crossing(da, db)); }
    else           { b = lerp(b, a, near_crossing(db, da)); }
    return true;
}

std::size_t clip_polygon_near(std::span<const clip_vertex> in, std::span<clip_vertex> out)
{
    if (in.size() < 3) { return 0; }

    std::size_t written = 0;

    // No capacity check inside the loop, and that is a claim rather than an
    // oversight. Emissions are (vertices that end up inside) + (crossings), one
    // plane can produce at most one crossing in each direction on a convex
    // polygon, and a triangle has three vertices — so the worst case is 2 + 2 = 4,
    // which is `k_clip_max_vertices` and which the header requires of `out`. The
    // bound is exact, not a margin. (Lesson 3.2 left a note about this: a limit
    // that silently truncates turns a capacity bug into a rendering bug, so the
    // right move is to make the bound provable, not to clamp.)
    const auto emit = [&](const clip_vertex& v) { out[written++] = v; };

    // Walk the edges: `prev` -> `cur`, for every vertex in order, wrapping. Note
    // that the loop is written around the edge ARRIVING at `cur`, which is what
    // makes "emit `cur` if it is inside" consider each vertex exactly once.
    float d_prev = near_distance(in[in.size() - 1].position);
    for (std::size_t i = 0; i < in.size(); ++i)
    {
        const clip_vertex& prev = in[(i + in.size() - 1) % in.size()];
        const clip_vertex& cur = in[i];
        const float d_cur = near_distance(cur.position);

        // Two independent questions, not four cases. Did the edge cross the plane?
        // Then the crossing point is part of the output, whichever way it crossed.
        // Is the vertex we arrived at inside? Then it is part of the output too.
        //
        // Written as four `if/else if` branches this reads as a table and is easy
        // to typo; written as two tests it is hard to get wrong, and it is the
        // same code. (`(d_prev < 0) != (d_cur < 0)` is "the signs disagree", with
        // zero counting as inside on both sides so a vertex exactly on the plane
        // never manufactures a crossing.)
        if ((d_prev < 0.0f) != (d_cur < 0.0f))
        {
            emit(lerp(prev, cur, near_crossing(d_prev, d_cur)));
        }
        if (d_cur >= 0.0f) { emit(cur); }

        d_prev = d_cur;
    }

    // 0, 3 or 4 — never 1 or 2, because a convex polygon cut by a plane is either
    // empty or has an interior. It CAN come back degenerate: a triangle with one
    // corner exactly on the plane and the other two behind it emits three
    // effectively coincident points. That is not a case to special-case here —
    // `fill_triangle` already rejects a zero-area triangle, which is precisely
    // what this is, and one rejection in one place beats two.
    return written;
}

} // namespace engine
