// engine/src/phys/collide.cpp — fifteen candidates, and the first one that
// separates wins.

#include <engine/phys/collide.hpp>

#include <engine/phys/epa.hpp>

#include <engine/phys/gjk.hpp>

#include <algorithm>
#include <cmath>

namespace engine::phys
{
namespace
{

/// The index a `separation` carries when there is no axis at all: an empty box,
/// or a degenerate input. `-1` already means "radial", which is a real axis.
constexpr int k_no_axis = -2;

/// The index a `separation` carries when GJK produced it (Lesson 8.5). Not a
/// candidate axis: the direction came from two witness points on two surfaces,
/// and there was never a list to be the n-th element of.
constexpr int k_witness = -3;

/// The cyclic successors of 0, 1, 2. `nxt[i]` is `(i+1) % 3` and `prv[i]` is
/// `(i+2) % 3`, written as tables because the nine edge-edge tests below index
/// with them three times each and `%` in that inner loop is a division.
constexpr int nxt[3] = {1, 2, 0};
constexpr int prv[3] = {2, 0, 1};

/// Component `i` of a vector, for the loops that genuinely want an index.
[[nodiscard]] float at(vec3 v, int i)
{
    return (i == 0) ? v.x : (i == 1) ? v.y : v.z;
}

/// A separation result meaning "these cannot touch, and there is no witness to
/// quote". Used only for empty boxes.
[[nodiscard]] separation no_overlap()
{
    separation s;
    s.axis = vec3{0.0f, 1.0f, 0.0f};
    s.depth = -1e30f;
    s.axis_index = k_no_axis;
    s.axes_tested = 0;
    return s;
}

}   // namespace

// ---------------------------------------------------------------------------
// The answer
// ---------------------------------------------------------------------------

const char* name_of(axis_source source)
{
    switch (source)
    {
        case axis_source::radial:    return "radial";
        case axis_source::face_a:    return "face A";
        case axis_source::face_b:    return "face B";
        case axis_source::edge_edge: return "edge-edge";
        case axis_source::witness:   return "witness";
        case axis_source::none:      return "none";
    }
    return "?";
}

axis_source source_of(const separation& s)
{
    if (s.axis_index >= 6) { return axis_source::edge_edge; }
    if (s.axis_index >= 3) { return axis_source::face_b; }
    if (s.axis_index >= 0) { return axis_source::face_a; }
    if (s.axis_index == -1) { return axis_source::radial; }
    if (s.axis_index == k_no_axis) { return axis_source::none; }
    if (s.axis_index == k_witness) { return axis_source::witness; }
    return axis_source::none;
}

separation flip(const separation& s)
{
    separation out = s;
    out.axis = -s.axis;
    return out;
}

// ---------------------------------------------------------------------------
// Projection
// ---------------------------------------------------------------------------

float interval_overlap(interval a, interval b)
{
    return std::min(a.hi, b.hi) - std::max(a.lo, b.lo);
}

float projected_radius(const obb& box, vec3 unit_axis)
{
    // Σᵢ hᵢ |uᵢ · L|. The three absolute values are the whole formula: each one
    // says "take this axis's contribution at its most helpful sign", which is
    // what choosing the farthest corner means.
    return box.half_extents.x * std::fabs(dot(box.axes.c0, unit_axis)) +
           box.half_extents.y * std::fabs(dot(box.axes.c1, unit_axis)) +
           box.half_extents.z * std::fabs(dot(box.axes.c2, unit_axis));
}

interval project(const obb& box, vec3 unit_axis)
{
    const float c = dot(box.centre, unit_axis);
    const float r = projected_radius(box, unit_axis);
    return interval{c - r, c + r};
}

interval project(const engine::sphere& s, vec3 unit_axis)
{
    const float c = dot(s.centre, unit_axis);
    return interval{c - s.radius, c + s.radius};
}

float gap_on_axis(const obb& a, const obb& b, vec3 unit_axis)
{
    // The two projections overlap exactly when the distance between their
    // centres, measured along the axis, is less than the sum of their radii.
    // Working with the offset rather than with two absolute positions is not
    // only shorter: it is the difference between subtracting two numbers of the
    // size of the boxes and subtracting two numbers of the size of the WORLD,
    // which §9 measures at four decimal digits of precision a kilometre out.
    const vec3 t = b.centre - a.centre;
    return std::fabs(dot(t, unit_axis)) -
           (projected_radius(a, unit_axis) + projected_radius(b, unit_axis));
}

// ---------------------------------------------------------------------------
// Spheres
// ---------------------------------------------------------------------------

separation collide(const engine::sphere& a, const engine::sphere& b)
{
    separation out;
    out.axis_index = -1;
    out.axes_tested = 1;

    const vec3 t = b.centre - a.centre;
    const float d2 = length_squared(t);
    const float r = a.radius + b.radius;

    if (d2 > 0.0f)
    {
        const float d = std::sqrt(d2);
        out.axis = t / d;
        out.depth = r - d;
        return out;
    }

    // Concentric. There is no direction between two things at the same point, so
    // we invent one — a legal unit vector rather than a zero, because every
    // caller downstream will multiply by it and some will divide.
    out.axis = vec3{0.0f, 1.0f, 0.0f};
    out.depth = r;
    return out;
}

// ---------------------------------------------------------------------------
// Axis-aligned boxes
// ---------------------------------------------------------------------------

separation collide(const aabb& a, const aabb& b)
{
    if (a.empty() || b.empty()) { return no_overlap(); }

    const vec3 t = b.centre() - a.centre();
    const vec3 ha = a.extent() * 0.5f;
    const vec3 hb = b.extent() * 0.5f;

    separation out;
    out.depth = 1e30f;

    // Three axes, and this loop IS `collide(obb, obb)` with twelve candidates
    // deleted and every dot product replaced by a component read, because the
    // axes are the world's own.
    for (int i = 0; i < 3; ++i)
    {
        ++out.axes_tested;

        const float centre_gap = at(t, i);
        const float overlap = (at(ha, i) + at(hb, i)) - std::fabs(centre_gap);

        vec3 axis{};
        if (i == 0) { axis = vec3{1.0f, 0.0f, 0.0f}; }
        else if (i == 1) { axis = vec3{0.0f, 1.0f, 0.0f}; }
        else { axis = vec3{0.0f, 0.0f, 1.0f}; }
        if (centre_gap < 0.0f) { axis = -axis; }

        if (overlap < 0.0f)
        {
            // A witness. Nothing examined later can make them overlap again, so
            // the remaining axes are not merely unnecessary — asking them would
            // be asking a question whose answer cannot matter.
            out.axis = axis;
            out.depth = overlap;
            out.axis_index = i;
            return out;
        }

        if (overlap < out.depth)
        {
            out.axis = axis;
            out.depth = overlap;
            out.axis_index = i;
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// Oriented boxes: the SAT
// ---------------------------------------------------------------------------

separation collide(const obb& a, const obb& b)
{
    const vec3 t = b.centre - a.centre;

    separation out;
    out.depth = 1e30f;
    out.axis_index = k_no_axis;

    // One candidate: project both boxes, compare, keep it if it is the tightest
    // overlap so far. Returns true when the axis separates, which ends the search
    // with a complete proof.
    const auto consider = [&](vec3 L, int index) -> bool
    {
        ++out.axes_tested;

        const float centre_gap = dot(t, L);
        const float r = projected_radius(a, L) + projected_radius(b, L);
        const float overlap = r - std::fabs(centre_gap);

        // THE SIGN CONVENTION, applied in the one place it can be applied: a
        // candidate axis has no inherent direction — `L` and `−L` are the same
        // shadow — so we point it from A toward B, which is what makes
        // `b + axis*depth` the separating translation.
        const vec3 directed = (centre_gap < 0.0f) ? -L : L;

        if (overlap < 0.0f)
        {
            out.axis = directed;
            out.depth = overlap;
            out.axis_index = index;
            return true;
        }

        if (overlap < out.depth)
        {
            out.axis = directed;
            out.depth = overlap;
            out.axis_index = index;
        }
        return false;
    };

    // --- six face normals ---------------------------------------------------
    //
    // Face normals first, and the ordering is a performance decision backed by
    // §12's histogram rather than a habit: real scenes separate on a face normal
    // the overwhelming majority of the time, and every candidate skipped is
    // fifteen multiplies not performed.
    for (int i = 0; i < 3; ++i)
    {
        if (consider(a.axis(i), i)) { return out; }
    }
    for (int j = 0; j < 3; ++j)
    {
        if (consider(b.axis(j), 3 + j)) { return out; }
    }

    // --- nine edge-edge axes ------------------------------------------------
    //
    // `a.axis(i) × b.axis(j)` is perpendicular to one edge direction from each
    // box, which is the direction in which an edge of A can slide past an edge
    // of B without either face noticing. §7.3's crossed boxes are the
    // configuration where nothing else finds the gap.
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            const vec3 c = cross(a.axis(i), b.axis(j));
            const float len2 = length_squared(c);

            // The degeneracy guard. `len2` is `sin²θ` between the two edges, so
            // this is "these edges are parallel to within 0.06°", which two
            // objects standing on the same floor are at every yaw.
            //
            // **IT IS NOT LOAD-BEARING HERE**, and §F is the measurement: with
            // it removed, a sweep of degenerate configurations produces ZERO
            // false separations, because a normalised axis is a real direction
            // however arbitrary it is, and no direction separates overlapping
            // convex bodies. It skips redundant work — a parallel edge pair
            // spans no face of the Minkowski sum, so anything it could separate
            // is already separated by a face normal — and it keeps an arbitrary
            // direction from winning the minimum. `overlaps` is where the same
            // idea is genuinely holding the algorithm up.
            if (len2 < k_parallel_sin2) { continue; }

            // NORMALISED, and this is the square root the boolean-only version
            // avoids. The overlap TEST would be correct without it — both sides
            // of the comparison scale with |L| — but the DEPTH would come back
            // in units of |L| = sin θ, and comparing that against a face axis's
            // honest metres picks the wrong winner and hands the solver a normal
            // that is too short.
            if (consider(c / std::sqrt(len2), 6 + 3 * i + j)) { return out; }
        }
    }

    return out;
}

bool overlaps(const obb& a, const obb& b)
{
    // R[i][j] = dot(a.axis(i), b.axis(j)): B's axes written in A's frame, one
    // column each. Everything below is expressed in these nine numbers, which is
    // why no cross product is ever formed.
    float R[3][3];
    float AbsR[3][3];
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            R[i][j] = dot(a.axis(i), b.axis(j));
            // THE EPSILON THAT IS LOAD-BEARING. Nothing here is normalised, so
            // on a near-parallel edge pair every term of that test scales with
            // |aᵢ × bⱼ| while its rounding error does not, and the comparison
            // becomes noise against noise. Measured in §F on two crates meeting
            // at a corner: 188 false separations in 400 without this addition,
            // zero with it. Inflating every |R| term biases the comparison
            // toward reporting an overlap, which is the safe direction — a false
            // overlap costs one narrow-phase call, a false separation costs an
            // object falling through the floor.
            AbsR[i][j] = std::fabs(R[i][j]) + k_parallel_sin2;
        }
    }

    const vec3 tw = b.centre - a.centre;
    const float t[3] = {dot(tw, a.axes.c0), dot(tw, a.axes.c1), dot(tw, a.axes.c2)};

    const float ae[3] = {a.half_extents.x, a.half_extents.y, a.half_extents.z};
    const float be[3] = {b.half_extents.x, b.half_extents.y, b.half_extents.z};

    // A's three face normals. In A's own frame the projection of A onto axis i
    // is just ae[i] — no dot products survive — and B's is its half extents
    // against the |R| row.
    for (int i = 0; i < 3; ++i)
    {
        const float ra = ae[i];
        const float rb = be[0] * AbsR[i][0] + be[1] * AbsR[i][1] + be[2] * AbsR[i][2];
        if (std::fabs(t[i]) > ra + rb) { return false; }
    }

    // B's three face normals, which is the same paragraph read down the columns.
    for (int j = 0; j < 3; ++j)
    {
        const float ra = ae[0] * AbsR[0][j] + ae[1] * AbsR[1][j] + ae[2] * AbsR[2][j];
        const float rb = be[j];
        if (std::fabs(t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j]) > ra + rb)
        {
            return false;
        }
    }

    // The nine edge pairs, derived rather than transcribed. For L = Aᵢ × Bⱼ:
    //
    //   Aₖ · L = det(Aₖ, Aᵢ, Bⱼ)  which is zero for k = i and ±R[·][j] otherwise
    //   Bₖ · L = Aᵢ · (Bⱼ × Bₖ)   which is ±R[i][·] by the right-handedness of B
    //   t  · L = t[i+2]·R[i+1][j] − t[i+1]·R[i+2][j]
    //
    // so every entry is a pair of |R| terms picked out by the cyclic index — and
    // the table you find printed in books is this loop unrolled.
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            const float ra = ae[nxt[i]] * AbsR[prv[i]][j] + ae[prv[i]] * AbsR[nxt[i]][j];
            const float rb = be[nxt[j]] * AbsR[i][prv[j]] + be[prv[j]] * AbsR[i][nxt[j]];
            const float lhs = t[prv[i]] * R[nxt[i]][j] - t[nxt[i]] * R[prv[i]][j];
            if (std::fabs(lhs) > ra + rb) { return false; }
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Closest points, and the sphere-versus-box tests built on them
// ---------------------------------------------------------------------------

vec3 closest_point(const aabb& box, vec3 p)
{
    return vec3{std::clamp(p.x, box.min.x, box.max.x),
                std::clamp(p.y, box.min.y, box.max.y),
                std::clamp(p.z, box.min.z, box.max.z)};
}

vec3 closest_point(const obb& box, vec3 p)
{
    // Into the box's axes — `dot(d, uᵢ)` is the transpose of the axis matrix,
    // which is its inverse because the axes are orthonormal — clamp, and back.
    const vec3 d = p - box.centre;
    const vec3 h = box.half_extents;
    const float lx = std::clamp(dot(d, box.axes.c0), -h.x, h.x);
    const float ly = std::clamp(dot(d, box.axes.c1), -h.y, h.y);
    const float lz = std::clamp(dot(d, box.axes.c2), -h.z, h.z);
    return box.centre + box.axes.c0 * lx + box.axes.c1 * ly + box.axes.c2 * lz;
}

float distance_squared_to(const aabb& box, vec3 p)
{
    return length_squared(closest_point(box, p) - p);
}

float distance_squared_to(const obb& box, vec3 p)
{
    return length_squared(closest_point(box, p) - p);
}

separation collide(const obb& box, const engine::sphere& s)
{
    separation out;
    out.axis_index = -1;
    out.axes_tested = 1;

    // The sphere's centre, in the box's own axes, where the box is an AABB.
    const vec3 d = s.centre - box.centre;
    const vec3 h = box.half_extents;
    const vec3 local{dot(d, box.axes.c0), dot(d, box.axes.c1), dot(d, box.axes.c2)};
    const vec3 clamped{std::clamp(local.x, -h.x, h.x),
                       std::clamp(local.y, -h.y, h.y),
                       std::clamp(local.z, -h.z, h.z)};

    const vec3 delta = local - clamped;       // still in box axes
    const float d2 = length_squared(delta);

    if (d2 > 0.0f)
    {
        const float dist = std::sqrt(d2);
        const vec3 n_local = delta / dist;    // box surface -> sphere centre
        out.axis = box.axes * n_local;        // and back out to the world
        out.depth = s.radius - dist;
        return out;
    }

    // THE CENTRE IS INSIDE THE BOX. The clamp did nothing, so there is no
    // direction to read off it — and this is not an exotic case, it is what a
    // fast sphere looks like one frame after it tunnelled into a wall.
    //
    // The answer is the nearest face: push the centre out to the closest of the
    // six planes and then clear the radius on top. It is the shallowest of the
    // ways out, which is the same minimum-translation rule the SAT uses, applied
    // to six candidates instead of fifteen.
    int face = 0;
    float best_inset = h.x - std::fabs(local.x);
    const float inset_y = h.y - std::fabs(local.y);
    const float inset_z = h.z - std::fabs(local.z);
    if (inset_y < best_inset) { best_inset = inset_y; face = 1; }
    if (inset_z < best_inset) { best_inset = inset_z; face = 2; }

    const float sign = (at(local, face) < 0.0f) ? -1.0f : 1.0f;
    vec3 n_local{};
    if (face == 0) { n_local = vec3{sign, 0.0f, 0.0f}; }
    else if (face == 1) { n_local = vec3{0.0f, sign, 0.0f}; }
    else { n_local = vec3{0.0f, 0.0f, sign}; }

    out.axis = box.axes * n_local;
    out.depth = s.radius + best_inset;
    return out;
}

separation collide(const engine::sphere& s, const obb& box)
{
    return flip(collide(box, s));
}

separation collide(const aabb& box, const engine::sphere& s)
{
    if (box.empty()) { return no_overlap(); }
    return collide(as_obb(box), s);
}

separation collide(const engine::sphere& s, const aabb& box)
{
    return flip(collide(box, s));
}

// ---------------------------------------------------------------------------
// Lesson 8.5: the general test
// ---------------------------------------------------------------------------

separation collide(const convex& a, const convex& b)
{
    const gjk_result g = gjk_distance(a, b);

    separation out;
    out.axis_index = k_witness;
    out.axes_tested = g.iterations;

    if (g.status == gjk_status::separated)
    {
        // The one case where this function beats everything above it: `depth` is
        // the TRUE distance, not the largest gap some enumerated axis happened
        // to find. Negative, because the sign convention is 8.4's and has not
        // moved.
        out.axis = g.direction;
        out.depth = -g.distance;
        return out;
    }

    // Overlapping, or out of iterations, which a caller must treat the same way.
    //
    // LESSON 8.6 FILLS IN THE PLACEHOLDER 8.5 LEFT HERE. The comment this
    // replaced said `depth` was `+0` and called it a placeholder in as many
    // words; now the terminal simplex GJK carried through its whole search is
    // handed to EPA, which walks it out to the boundary of the same Minkowski
    // difference and returns the minimum translation.
    //
    // NOTE WHAT IS NOT PAID TWICE. EPA starts from `g.terminal`, so the search
    // GJK already did is reused rather than repeated — which is the entire
    // reason `gjk_result` is 180 bytes rather than 32, and the reason these two
    // algorithms are usually described as one.
    const epa_result e = epa_penetration(a, b, g.terminal);
    out.axis = (length_squared(e.normal) > 0.0f) ? e.normal : g.direction;
    out.depth = e.depth;
    out.axes_tested = g.iterations + e.iterations;
    return out;
}

} // namespace engine::phys
