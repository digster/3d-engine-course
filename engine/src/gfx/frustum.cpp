// engine/src/gfx/frustum.cpp — six planes, and the walk that uses them.
//
// Lesson 6.16. The header carries the derivation; this file carries the two
// things that are easier to get wrong than to explain: which rows combine into
// which plane, and which corner of a box to test against each one.

#include <engine/gfx/frustum.hpp>

#include <engine/core/assert.hpp>

namespace engine {

const char* name_of_plane(int index)
{
    switch (index)
    {
    case frustum::k_left:   return "left";
    case frustum::k_right:  return "right";
    case frustum::k_bottom: return "bottom";
    case frustum::k_top:    return "top";
    case frustum::k_near_z: return "near";
    case frustum::k_far_z:  return "far";
    default: break;
    }
    return "?";
}

const char* name_of(visibility v)
{
    switch (v)
    {
    case visibility::outside:      return "outside";
    case visibility::intersecting: return "intersecting";
    case visibility::inside:       return "inside";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Extraction
// ---------------------------------------------------------------------------

namespace {

/// Row `r` of `m` as four floats, in WRITTEN notation.
///
/// `mat4::at(row, col)` is the only spelling in this engine that reads a matrix
/// the way the algebra writes one — the storage is column-major (conventions §3)
/// and reaching into `c0.x` by hand is how a transpose gets introduced exactly
/// once, silently, three lessons before anyone notices.
struct row4 { float x, y, z, w; };

[[nodiscard]] row4 row_of(const mat4& m, int r)
{
    return {m.at(r, 0), m.at(r, 1), m.at(r, 2), m.at(r, 3)};
}

/// `a + s*b` as a plane. `s` MULTIPLIES b, which is not a free choice.
///
/// The first draft of this put the sign on `a` instead, turning `row3 - row0`
/// into `row0 - row3`. That is the same plane with its normal reversed, so every
/// point reports the negative of its true distance, the left and right planes
/// come out as exact negations of one another, and the culler rejects the entire
/// scene. **Exact negation between two opposite planes is the tell** — worth
/// knowing, because the picture (nothing at all) looks identical to a dozen
/// other mistakes.
[[nodiscard]] plane combine(const row4& a, const row4& b, float s)
{
    return {vec3{a.x + s * b.x, a.y + s * b.y, a.z + s * b.z}, a.w + s * b.w};
}

} // namespace

frustum frustum_of(const mat4& clip_from_world)
{
    const row4 r0 = row_of(clip_from_world, 0);
    const row4 r1 = row_of(clip_from_world, 1);
    const row4 r2 = row_of(clip_from_world, 2);
    const row4 r3 = row_of(clip_from_world, 3);

    frustum f;

    // The four side planes, from the two-sided inequalities `-w <= x <= w` and
    // `-w <= y <= w`. Each side of each inequality, rearranged to `something >= 0`,
    // IS a plane:
    //
    //     x >= -w   <=>   x + w >= 0   <=>   (row0 + row3) . p >= 0
    //     x <=  w   <=>   w - x >= 0   <=>   (row3 - row0) . p >= 0
    //
    // Note that the two are NOT negatives of each other — `row3` appears with a
    // plus sign in both — which is exactly what makes them two distinct planes
    // meeting at the apex rather than one plane counted twice.
    f.planes[frustum::k_left]   = combine(r3, r0,  1.0f);
    f.planes[frustum::k_right]  = combine(r3, r0, -1.0f);
    f.planes[frustum::k_bottom] = combine(r3, r1,  1.0f);
    f.planes[frustum::k_top]    = combine(r3, r1, -1.0f);

    // THE NEAR PLANE IS ROW 2 ALONE, AND THIS LINE IS THE CONVENTION.
    //
    // SDL_GPU's clip volume is `0 <= z <= w` (conventions §4), so the near
    // condition is `z >= 0`, which is `row2 . p >= 0` with nothing added. Every
    // OpenGL-derived article on the internet writes `row2 + row3` here, because
    // OpenGL's volume is `-w <= z <= w`. Copy that line into this engine and the
    // near plane lands at the FAR plane's distance behind the camera, so nothing
    // within `far` units of the eye is ever culled and the bug looks like the
    // culler simply being weak.
    //
    // It also has a happy side effect worth naming: being a single row rather
    // than a difference of two, the near plane suffers none of the cancellation
    // that costs the far plane its last four digits (see the header's worked
    // example). Measured: near is accurate to 1.8e-7, far to 1.4e-3.
    f.planes[frustum::k_near_z] = {vec3{r2.x, r2.y, r2.z}, r2.w};

    // `z <= w`, the same rearrangement as the right and top planes.
    f.planes[frustum::k_far_z]  = combine(r3, r2, -1.0f);

    // NORMALISED, ALWAYS, even though the box test does not need it.
    //
    // The sign of a dot product survives any positive scale, so a pure
    // inside/outside test is correct with raw rows. Two things are not: the
    // sphere test compares a distance against a RADIUS, and every diagnostic that
    // prints a distance is meaningless in unknown units. Normalising once at
    // extraction — six square roots per camera per frame — buys both, and makes
    // `signed_distance` mean metres everywhere in the engine rather than in the
    // half of it that remembered.
    for (plane& pl : f.planes) { pl = normalised(pl); }
    return f;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

namespace {

/// The box corner FURTHEST along `n` — the "positive vertex".
///
/// Per axis, take `max` when the normal's component is positive and `min` when it
/// is negative. That is the corner with the largest `dot(n, corner)`, so it is
/// the LAST one to fall outside a plane: if it is outside, all eight are, and the
/// box is rejected having looked at one corner instead of eight.
[[nodiscard]] vec3 positive_vertex(const aabb& box, vec3 n)
{
    return {n.x >= 0.0f ? box.max.x : box.min.x,
            n.y >= 0.0f ? box.max.y : box.min.y,
            n.z >= 0.0f ? box.max.z : box.min.z};
}

/// The opposite corner — the FIRST to fall outside. Inside every plane means the
/// whole box is inside, which is `visibility::inside`.
[[nodiscard]] vec3 negative_vertex(const aabb& box, vec3 n)
{
    return {n.x >= 0.0f ? box.min.x : box.max.x,
            n.y >= 0.0f ? box.min.y : box.max.y,
            n.z >= 0.0f ? box.min.z : box.max.z};
}

} // namespace

bool intersects(const frustum& f, const aabb& box)
{
    if (box.empty()) { return false; }
    for (const plane& pl : f.planes)
    {
        if (signed_distance(pl, positive_vertex(box, pl.normal)) < 0.0f) { return false; }
    }
    return true;
}

bool intersects(const frustum& f, const sphere& s)
{
    if (s.empty()) { return false; }
    for (const plane& pl : f.planes)
    {
        // `-radius` and not `0`: the sphere is outside only once its CENTRE is
        // further outside than the radius reaches back. Comparing against zero
        // here rejects any sphere whose centre has crossed the plane, which
        // clips objects the moment they touch the screen edge — a classic and
        // very visible bug, because it happens at the edge of the frame where
        // the eye is drawn to motion.
        if (signed_distance(pl, s.centre) < -s.radius) { return false; }
    }
    return true;
}

visibility classify(const frustum& f, const aabb& box)
{
    if (box.empty()) { return visibility::outside; }

    bool fully = true;
    for (const plane& pl : f.planes)
    {
        if (signed_distance(pl, positive_vertex(box, pl.normal)) < 0.0f)
        {
            return visibility::outside;
        }
        if (signed_distance(pl, negative_vertex(box, pl.normal)) < 0.0f)
        {
            // Straddles this plane. Keep going — a later plane may still reject
            // the box outright, and `outside` is a strictly more useful answer
            // than `intersecting`.
            fully = false;
        }
    }
    return fully ? visibility::inside : visibility::intersecting;
}

// ---------------------------------------------------------------------------
// The walk
// ---------------------------------------------------------------------------

int cull_visible(const frustum& f, std::span<const aabb> bounds, std::span<int> out,
                 cull_report* report, bool sphere_prefilter, bool classify_fully)
{
    ENGINE_ASSERT_MSG(out.size() >= bounds.size(), log_gfx,
                      "cull_visible: out holds %zu, needs %zu",
                      out.size(), bounds.size());
    if (out.size() < bounds.size()) { return 0; }

    cull_report local;
    int written = 0;

    for (std::size_t i = 0; i < bounds.size(); ++i)
    {
        const aabb& box = bounds[i];
        ++local.tested;

        if (box.empty())
        {
            ++local.empty;
            ++local.culled;
            continue;
        }

        // ---- The optional cheap first question -----------------------------
        //
        // Loose by construction (bounds.hpp), so a rejection here is one the box
        // test below would also have made. It can only save time, never change
        // the answer — which is why it is safe to have as a runtime flag at all.
        if (sphere_prefilter)
        {
            const sphere s = bounding_sphere(box);
            bool out_by_sphere = false;
            for (int p = 0; p < frustum::k_count; ++p)
            {
                ++local.plane_tests;
                if (signed_distance(f.planes[p], s.centre) < -s.radius)
                {
                    ++local.rejected_by[p];
                    out_by_sphere = true;
                    break;
                }
            }
            if (out_by_sphere) { ++local.culled; continue; }
        }

        // ---- The box test, inlined so the report can count plane evaluations -
        //
        // `intersects(f, box)` says the same thing in one line. It is spelled out
        // here because the counters need the loop: `plane_tests` is the cost in
        // the unit the cost is paid in, and `rejected_by` is the diagnostic that
        // tells a far plane set too close from a scene that is genuinely behind
        // the camera. Instrumentation may duplicate the QUESTION, never the
        // ANSWER — the rule `is_front_facing` set in Lesson 3.4 — so this loop is
        // byte-for-byte the one in `intersects` and §6 of the lesson checks the
        // two agree on every object of every frame rather than assuming it.
        bool rejected = false;
        bool fully = true;
        for (int p = 0; p < frustum::k_count; ++p)
        {
            const plane& pl = f.planes[p];
            ++local.plane_tests;
            if (signed_distance(pl, positive_vertex(box, pl.normal)) < 0.0f)
            {
                ++local.rejected_by[p];
                rejected = true;
                break;
            }
            if (classify_fully && fully
                && signed_distance(pl, negative_vertex(box, pl.normal)) < 0.0f)
            {
                fully = false;
            }
        }

        if (rejected) { ++local.culled; continue; }

        if (classify_fully && fully) { ++local.fully_inside; }
        out[static_cast<std::size_t>(written)] = static_cast<int>(i);
        ++written;
        ++local.visible;
    }

    if (report != nullptr) { *report = local; }
    return written;
}

} // namespace engine
