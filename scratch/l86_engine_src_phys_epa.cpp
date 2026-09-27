// engine/src/phys/epa.cpp — the expanding polytope, and the seed it has to build
// for itself.
//
// Lesson 8.6. The loop at the bottom of this file is twenty lines and reads like
// the four-step sketch in the header. Everything above it is `seed_polytope`,
// which is the part nobody's blog post writes out — because every description of
// EPA assumes GJK hands over a tetrahedron, and 8.5 §9 measured that on two
// boxes standing on a floor it never does.

#include <engine/phys/epa.hpp>

#include <engine/core/assert.hpp>

#include <cmath>

namespace engine::phys
{
namespace
{

/// A vertex closer than `sqrt` of this to one the polytope already has, relative
/// to the largest vertex seen, is the same vertex.
///
/// 8.5's `k_duplicate_rel2` exactly, and for its reason: this test exists to
/// catch a support function returning a point already present, which on a
/// POLYTOPE is how the expansion finishes exactly — the vertex set is finite, so
/// once the boundary face has been reached there is nothing new to find. Setting
/// it to the user's tolerance instead is the bug 8.5 measured at 2.4% slack on
/// curved shapes, where a support point moves continuously and a genuine small
/// step reads as a repeat.
constexpr float k_duplicate_rel2 = 1e-12f;

/// A tetrahedron whose squared volume is below this fraction of `scale³` counts
/// as flat, and is rebuilt rather than used.
///
/// Squared, because the volume is a triple product — a difference of products of
/// nearly equal numbers on exactly the configurations that produce it — so the
/// sign is noise long before the magnitude is. Comparing `vol²` against
/// `k_flat_rel2 * scale⁶` is the same relative test `outside_plane` makes in
/// `gjk.cpp`, one dimension up.
constexpr float k_flat_rel2 = 1e-14f;

// ---------------------------------------------------------------------------
// The polytope
// ---------------------------------------------------------------------------

/// One triangle of the expanding polytope, with its plane cached.
///
/// **`n` and `d` are stored rather than recomputed**, and that is correctness
/// rather than speed. The closest face is selected by comparing `d`; the
/// visibility test for a new vertex compares `dot(n, w)` against the SAME `d`.
/// If the two used independently recomputed planes they could disagree by a few
/// ulps, and a face could be simultaneously "the closest" and "not visible from
/// the point we are expanding toward" — which is a loop that cannot terminate
/// and cannot be debugged, because each half of it is correct.
/// **NO MEMBER INITIALISERS, AND THAT IS A MEASUREMENT RATHER THAN A STYLE.**
/// `polytope` below holds 128 of these, and a default member initialiser on any
/// one member makes the whole type non-trivially-default-constructible — which
/// makes `polytope p;` write four kilobytes of zeroes that the very next line
/// overwrites. `expand`'s horizon array is three more. 8.6 §10 measures the
/// difference on a real query mix; every field here is assigned before it is
/// read, and `add_face` is the only thing that builds one.
struct face
{
    int v[3];      ///< Indices into `polytope::vert`. Wound CCW from outside.
    vec3 n;        ///< Unit outward normal. Zero for a degenerate sliver.
    float d;       ///< Plane offset: `dot(n, vert[v[0]].w)`.
    bool alive;
};

/// THE THIRD ARRAY IS LEFT ALONE ON PURPOSE. `gjk_vertex` carries member
/// initialisers of its own, so `vert` below is 2.3 KB that `polytope p;` zeroes —
/// the same waste `face` had. Giving this file a trivially-constructible mirror
/// type and converting at the boundary was tried and measured back to back:
/// **1054.4 ns against 1053.1**, inside the noise, because the compiler can see
/// that every slot below `vertex_count` is written before it is read and the
/// array is touched once per query rather than once per pass. A change that buys
/// nothing still costs a reader, so it was reverted.
struct polytope
{
    gjk_vertex vert[k_epa_max_vertices];
    face faces[k_epa_max_faces];
    int vertex_count = 0;
    int face_count = 0;

    /// The largest squared vertex magnitude seen: the length the tolerances are
    /// relative to, and the same quantity `gjk.cpp` calls `scale2`.
    float scale2 = 0.0f;
};

/// Append a vertex. Returns its index, or `-1` at capacity.
int add_vertex(polytope& p, const gjk_vertex& w)
{
    if (p.vertex_count >= k_epa_max_vertices) { return -1; }
    const int index = p.vertex_count++;
    p.vert[index] = w;
    const float m2 = length_squared(w.w);
    if (m2 > p.scale2) { p.scale2 = m2; }
    return index;
}

/// Append a triangle, computing its plane. Returns `false` at capacity.
///
/// A DEGENERATE TRIANGLE IS KEPT, NOT DROPPED, and the choice matters. Three
/// nearly-collinear vertices give a cross product that is pure rounding error,
/// so its direction means nothing — but the triangle is still part of the
/// polytope's surface, and dropping it would leave a hole through which the
/// horizon walk in `expand` would fall. It is stored with a zero normal and an
/// infinite offset instead, which makes it invisible to both the closest-face
/// search (nothing is ever nearer than infinity) and the visibility test
/// (`dot(0, w) = 0` is never greater than infinity), so it is carried along
/// inertly until some later expansion removes it.
bool add_face(polytope& p, int i, int j, int k)
{
    if (p.face_count >= k_epa_max_faces) { return false; }

    face f;
    f.v[0] = i;
    f.v[1] = j;
    f.v[2] = k;

    const vec3 a = p.vert[i].w;
    const vec3 normal = cross(p.vert[j].w - a, p.vert[k].w - a);
    const float n2 = length_squared(normal);

    // Relative degeneracy test: `|cross|` has the units of an AREA, so compare
    // its square against `scale⁴` rather than against a constant in metres.
    if (n2 <= k_flat_rel2 * p.scale2 * p.scale2)
    {
        f.n = vec3{};
        f.d = 3.4028235e38f;   // FLT_MAX: never closest, never visible.
    }
    else
    {
        f.n = normal * (1.0f / std::sqrt(n2));
        f.d = dot(f.n, a);
    }

    f.alive = true;
    p.faces[p.face_count++] = f;
    return true;
}

/// The index of the live face nearest the origin, or `-1` if there is none.
int closest_face(const polytope& p)
{
    int best = -1;
    float best_d = 3.4028235e38f;
    for (int i = 0; i < p.face_count; ++i)
    {
        if (!p.faces[i].alive) { continue; }
        if (p.faces[i].d < best_d)
        {
            best_d = p.faces[i].d;
            best = i;
        }
    }
    return best;
}

/// How many vertices are still ON the surface.
///
/// Not the same as how many were found. An expansion deletes every face that
/// sees the new vertex, and if a vertex's whole fan is deleted it is swallowed:
/// still in the array, no longer on the hull. Euler's formula counts the hull's
/// vertices, so `manifold()` needs this number rather than the array's.
int surface_vertex_count(const polytope& p)
{
    bool used[k_epa_max_vertices] = {};
    for (int i = 0; i < p.face_count; ++i)
    {
        for (int e = 0; e < 3; ++e) { used[p.faces[i].v[e]] = true; }
    }
    int n = 0;
    for (int i = 0; i < p.vertex_count; ++i) { n += used[i] ? 1 : 0; }
    return n;
}

// ---------------------------------------------------------------------------
// The expansion
// ---------------------------------------------------------------------------

/// One horizon edge, as a pair of vertex indices in winding order.
struct edge
{
    int from;
    int to;
};

/// Do two faces share an edge? Two triangles of a closed surface do exactly when
/// they have two vertices in common.
bool share_edge(const face& f, const face& g)
{
    int common = 0;
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            if (f.v[i] == g.v[j]) { ++common; break; }
        }
    }
    return common >= 2;
}

/// Add `w` to the polytope: delete every face that can see it, then stitch the
/// hole shut with triangles from its rim.
///
/// ---- WHY THE HORIZON IS FOUND BY CANCELLATION -----------------------------
///
/// The faces that "see" `w` — those whose outward plane `w` is beyond — form a
/// connected cap, and the rim of that cap is a single closed loop of edges. The
/// obvious way to find the loop is to store face adjacency and walk it. The
/// cheaper way, and the one here, uses an identity instead: **every edge
/// INTERIOR to the cap is shared by two deleted faces and appears twice, once in
/// each direction; every edge on the rim appears once.** So push each deleted
/// face's three edges onto a list, and whenever an edge's reverse is already
/// there, remove both. What survives is the horizon, in winding order, with no
/// adjacency structure to build or keep correct.
///
/// ---- AND WHY THE VISIBLE SET IS FLOOD-FILLED FIRST ------------------------
///
/// "The visible set is connected" is a theorem about convex polytopes and
/// exterior points — in exact arithmetic. 8.6 §7 measures the case that breaks
/// it in float, and it is not exotic: **two cubes meeting face to face with a
/// tenth of a millimetre of overlap.** The support point that arrives is
/// *exactly coplanar* with four of the polytope's existing faces, so `dot(n, w)`
/// and `d` are the same number computed two different ways, and which side of
/// the comparison each lands on is decided by the last few bits. Measured: three
/// faces classified visible, scattered around the polytope, sharing no edge —
/// nine horizon edges with nothing to cancel. Stitching a fan across that
/// produces a surface that is not a polytope at all, and everything after it is
/// silently wrong.
///
/// So the candidates are gathered, and then a flood fill from the CLOSEST face —
/// the one face that is certainly visible, because the caller only got here by
/// finding `dot(n, w)` well beyond its `d` — keeps just the connected component
/// containing it. In exact arithmetic that changes nothing, because the set was
/// already connected. In float it projects a scattered classification back onto
/// the answer exact arithmetic would have given.
///
/// The loop check that follows is kept anyway, and it is cheap: on a single
/// closed rim, every vertex starts exactly one edge and ends exactly one edge.
/// **Nothing is modified until both tests pass**, so a failure costs the caller
/// only the iteration, not the answer.
///
/// ---- AND WHY DEAD FACES ARE RECLAIMED RATHER THAN FLAGGED -----------------
///
/// The live face count obeys Euler's formula exactly — `F = 2V − 4` — so 64
/// vertices can never need more than 124 live faces. The number CREATED over a
/// whole query is a different quantity: each pass deletes `k` faces and stitches
/// `k + 2` new ones, so a query that deletes a five-face cap twenty times has
/// created 140 faces while never holding more than a few dozen. Leaving the dead
/// ones in the array turns `k_epa_max_faces` from a bound on the polytope into a
/// bound on the total work, and the difference is an unnecessary `capacity`
/// failure on exactly the deep overlaps that matter most. The compaction below
/// is one sweep over an array of at most 128 elements, per pass.
bool expand(polytope& p, const gjk_vertex& w, int closest, bool flood_fill)
{
    // ---- gather, and change nothing yet ------------------------------------
    int cand[k_epa_max_faces];
    int cand_count = 0;
    int seed_slot = -1;
    for (int i = 0; i < p.face_count; ++i)
    {
        if (dot(p.faces[i].n, w.w) > p.faces[i].d)
        {
            if (i == closest) { seed_slot = cand_count; }
            cand[cand_count++] = i;
        }
    }

    // No face saw the new vertex. The caller's termination test should already
    // have stopped, so reaching here means the two tests disagree — refuse
    // rather than loop forever on a polytope that never changes.
    if (cand_count == 0 || seed_slot < 0) { return false; }

    int visible[k_epa_max_faces];
    int visible_count = 0;
    if (flood_fill)
    {
        bool taken[k_epa_max_faces];
        for (int i = 0; i < cand_count; ++i) { taken[i] = false; }

        int queue[k_epa_max_faces];
        int head = 0;
        int tail = 0;
        queue[tail++] = seed_slot;
        taken[seed_slot] = true;
        while (head < tail)
        {
            const int slot = queue[head++];
            visible[visible_count++] = cand[slot];
            for (int j = 0; j < cand_count; ++j)
            {
                if (taken[j]) { continue; }
                if (share_edge(p.faces[cand[slot]], p.faces[cand[j]]))
                {
                    taken[j] = true;
                    queue[tail++] = j;
                }
            }
        }
    }
    else
    {
        // The textbook formulation, kept because §7 measures what it costs.
        for (int i = 0; i < cand_count; ++i) { visible[visible_count++] = cand[i]; }
    }

    edge horizon[3 * k_epa_max_faces];
    int horizon_count = 0;
    for (int f = 0; f < visible_count; ++f)
    {
        const face& dead = p.faces[visible[f]];
        for (int e = 0; e < 3; ++e)
        {
            const int from = dead.v[e];
            const int to = dead.v[(e + 1) % 3];

            int found = -1;
            for (int h = 0; h < horizon_count; ++h)
            {
                if (horizon[h].from == to && horizon[h].to == from) { found = h; break; }
            }

            if (found >= 0)
            {
                horizon[found] = horizon[--horizon_count];   // interior: cancel
            }
            else
            {
                horizon[horizon_count].from = from;
                horizon[horizon_count].to = to;
                ++horizon_count;
            }
        }
    }

    if (horizon_count < 3) { return false; }

    // The loop check.
    for (int h = 0; h < horizon_count; ++h)
    {
        int starts = 0;
        int ends = 0;
        for (int g = 0; g < horizon_count; ++g)
        {
            if (horizon[g].from == horizon[h].from) { ++starts; }
            if (horizon[g].to == horizon[h].from) { ++ends; }
        }
        if (starts != 1 || ends != 1) { return false; }
    }

    if (p.face_count - visible_count + horizon_count > k_epa_max_faces) { return false; }

    const int index = add_vertex(p, w);
    if (index < 0) { return false; }

    // ---- commit ------------------------------------------------------------
    for (int f = 0; f < visible_count; ++f) { p.faces[visible[f]].alive = false; }

    int write = 0;
    for (int i = 0; i < p.face_count; ++i)
    {
        if (p.faces[i].alive) { p.faces[write++] = p.faces[i]; }
    }
    p.face_count = write;

    // Stitch. The new triangles inherit their winding from the rim, so their
    // normals come out outward without anything being tested or flipped — which
    // is why the seed below works so hard to get the initial winding right.
    for (int h = 0; h < horizon_count; ++h)
    {
        if (!add_face(p, horizon[h].from, horizon[h].to, index)) { return false; }
    }
    return true;
}

// ---------------------------------------------------------------------------
// The seed
// ---------------------------------------------------------------------------

/// Build a four-face tetrahedron from four points, fixing the winding so that
/// every face normal points outward.
///
/// A positive triple product puts `q[3]` on the side the normal of face (0,1,2)
/// points to, which is INWARD; swapping two vertices reverses every face at
/// once. Shared by both seeding paths, because "is this wound the right way" is
/// a question with one right answer and no reason to have two spellings.
bool build_tetrahedron(polytope& p, gjk_vertex q[4])
{
    const float vol6 =
        dot(cross(q[1].w - q[0].w, q[2].w - q[0].w), q[3].w - q[0].w);
    if (vol6 > 0.0f) { const gjk_vertex t = q[1]; q[1] = q[2]; q[2] = t; }

    for (int i = 0; i < 4; ++i) { add_vertex(p, q[i]); }
    return add_face(p, 0, 1, 2) && add_face(p, 0, 3, 1) && add_face(p, 0, 2, 3)
           && add_face(p, 1, 3, 2);
}

/// Barycentric coordinates of `p` — assumed to lie in the triangle's plane —
/// with respect to triangle `abc`. Returns `false` if the triangle has no area.
bool barycentric(vec3 a, vec3 b, vec3 c, vec3 p, float w[3])
{
    const vec3 n = cross(b - a, c - a);
    const float n2 = length_squared(n);
    if (n2 <= 0.0f) { return false; }

    // 2.3's signed-area construction, for the third time in this engine: the
    // weight of a vertex is the area of the sub-triangle OPPOSITE it, signed
    // against the whole triangle's normal.
    const float inv = 1.0f / n2;
    w[0] = dot(cross(c - b, p - b), n) * inv;
    w[1] = dot(cross(a - c, p - c), n) * inv;
    w[2] = dot(cross(b - a, p - a), n) * inv;
    return true;
}

/// Does the origin's projection onto the plane of `abc` land inside it?
bool triangle_covers_origin(vec3 a, vec3 b, vec3 c)
{
    float w[3];
    if (!barycentric(a, b, c, vec3{}, w)) { return false; }

    // The projection of the ORIGIN is what matters, and the barycentric weights
    // of the origin are the weights of its projection: both are the same point
    // as far as in-plane coordinates are concerned, because the difference
    // between them is along the normal and the normal has zero weight.
    return w[0] >= 0.0f && w[1] >= 0.0f && w[2] >= 0.0f;
}

/// Build the starting polytope from GJK's terminal simplex.
///
/// **This is the section every EPA write-up skips, and the measurement that says
/// it cannot be skipped is 8.5 §9: on two boxes standing on the same floor, GJK
/// produces a tetrahedron ZERO times in 100,000 pairs.** The simplex handed over
/// is flat, because a shared up axis puts `y = 0` on every difference vertex the
/// search visits. The set is not flat; the search never left its equator.
///
/// Five inputs, and they collapse to two outputs:
///
///   * **A tetrahedron with volume** — GJK's good case, and the only one the
///     textbooks describe. Used as is, with the winding fixed.
///   * **Anything else** — a flat tetrahedron, a triangle, a segment, a single
///     point. Extended to a triangle that covers the origin, then given two
///     apexes found by one support call each along ±normal: a BIPYRAMID.
///
/// A bipyramid and not another tetrahedron, and the reason is containment. The
/// origin lies in the flat simplex; dropping a vertex to make a triangle can
/// drop the origin out of it, and a starting polytope that does not contain the
/// origin makes the lower bound meaningless. So the triangle is chosen to be one
/// that still covers the origin — of the four a flat tetrahedron offers, at
/// least two do, because either diagonal cuts the quadrilateral into two — and
/// the two apexes then straddle the plane it lies in.
///
/// `added` counts the CSO vertices this had to find, which is the histogram §5
/// reports. Returns `true` when `p` is a closed polytope ready to expand; on
/// `false`, `failure` says why and `flat_normal` carries the plane direction,
/// which is still the right normal to hand a solver.
bool seed_polytope(const convex& a, const convex& b, vec3 delta,
                   const simplex& start, polytope& p,
                   int& added, int& support_calls, vec3& flat_normal,
                   epa_status& failure)
{
    added = 0;
    flat_normal = vec3{};
    failure = epa_status::degenerate;

    gjk_vertex pts[4];
    int count = start.count;
    for (int i = 0; i < count && i < 4; ++i)
    {
        pts[i] = start.v[i];
        const float m2 = length_squared(pts[i].w);
        if (m2 > p.scale2) { p.scale2 = m2; }
    }
    if (count <= 0) { return false; }

    const float scale = std::sqrt(p.scale2);

    // ---- the good case: a tetrahedron that already has volume ---------------
    if (count == 4)
    {
        const float vol6 = dot(cross(pts[1].w - pts[0].w, pts[2].w - pts[0].w),
                               pts[3].w - pts[0].w);
        if (vol6 * vol6 > k_flat_rel2 * p.scale2 * p.scale2 * p.scale2)
        {
            const bool ok = build_tetrahedron(p, pts);
            if (!ok) { failure = epa_status::capacity; }
            return ok;
        }
        // Flat. Fall through and treat the four points as a quadrilateral.
    }

    // ---- extend up to a triangle -------------------------------------------
    //
    // A single point means the origin IS that point of the difference set, and a
    // segment means it lies on that segment. Either way the origin stays inside
    // whatever we grow, because growing only ever adds.
    if (count == 1)
    {
        // Six axis directions, and the best of them wins rather than the first.
        // A first-match loop picks a direction that happens to be barely
        // non-degenerate and hands the next step a segment made of noise.
        static const vec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                                     {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        gjk_vertex best;
        float best_m2 = 0.0f;
        for (const vec3& d : dirs)
        {
            const gjk_vertex w = cso_support(a, b, delta, d);
            support_calls += 2;
            const float m2 = length_squared(w.w - pts[0].w);
            if (m2 > best_m2) { best_m2 = m2; best = w; }
        }
        if (best_m2 <= 0.0f) { return false; }
        pts[1] = best;
        count = 2;
        ++added;
    }

    if (count == 2)
    {
        const vec3 e = pts[1].w - pts[0].w;
        // A direction perpendicular to the segment, built from whichever basis
        // axis is least aligned with it — so the cross product is never taken
        // between two nearly parallel vectors, which is where its direction
        // would be noise.
        const vec3 ax = (std::fabs(e.x) <= std::fabs(e.y) && std::fabs(e.x) <= std::fabs(e.z))
                            ? vec3{1, 0, 0}
                            : (std::fabs(e.y) <= std::fabs(e.z) ? vec3{0, 1, 0} : vec3{0, 0, 1});
        const vec3 t0 = normalised_or(cross(e, ax), vec3{1, 0, 0});
        const vec3 t1 = normalised_or(cross(e, t0), vec3{0, 1, 0});

        gjk_vertex best;
        float best_area2 = 0.0f;
        // Four directions around the segment rather than one: the difference set
        // can be thin in any particular perpendicular direction, and a triangle
        // with no area is a plane normal made of rounding error.
        const vec3 around[4] = {t0, -t0, t1, -t1};
        for (const vec3& d : around)
        {
            const gjk_vertex w = cso_support(a, b, delta, d);
            support_calls += 2;
            const float area2 = length_squared(cross(e, w.w - pts[0].w));
            if (area2 > best_area2) { best_area2 = area2; best = w; }
        }
        if (best_area2 <= k_flat_rel2 * p.scale2 * p.scale2) { return false; }
        pts[2] = best;
        count = 3;
        ++added;
    }

    // ---- choose the triangle that still covers the origin -------------------
    int tri[3] = {0, 1, 2};
    if (count == 4)
    {
        static constexpr int candidates[4][3] = {{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}};
        int chosen = -1;
        float chosen_area2 = 0.0f;
        int widest = 0;
        float widest_area2 = 0.0f;
        for (int c = 0; c < 4; ++c)
        {
            const vec3 v0 = pts[candidates[c][0]].w;
            const vec3 v1 = pts[candidates[c][1]].w;
            const vec3 v2 = pts[candidates[c][2]].w;
            const float area2 = length_squared(cross(v1 - v0, v2 - v0));
            if (area2 > widest_area2) { widest_area2 = area2; widest = c; }
            if (area2 > chosen_area2 && triangle_covers_origin(v0, v1, v2))
            {
                chosen_area2 = area2;
                chosen = c;
            }
        }
        // Falling back to the widest triangle when none covers the origin is not
        // a guess: it happens when the origin is a hair OUTSIDE the flat
        // simplex, which is GJK's contact-margin exit, and the containment check
        // at the end of this function then reports `touching` — the right answer
        // for two shapes that are touching.
        const int use = (chosen >= 0) ? chosen : widest;
        tri[0] = candidates[use][0];
        tri[1] = candidates[use][1];
        tri[2] = candidates[use][2];
    }

    const vec3 q0 = pts[tri[0]].w;
    const vec3 q1 = pts[tri[1]].w;
    const vec3 q2 = pts[tri[2]].w;
    const vec3 plane = normalised_or(cross(q1 - q0, q2 - q0), vec3{0, 1, 0});
    flat_normal = plane;

    // ---- two apexes, one support call each ---------------------------------
    const gjk_vertex up = cso_support(a, b, delta, plane);
    const gjk_vertex down = cso_support(a, b, delta, -plane);
    support_calls += 4;
    added += 2;

    const float height_up = dot(up.w - q0, plane);
    const float height_down = dot(q0 - down.w, plane);

    // THE SET ITSELF IS FLAT. Not the simplex — the difference. Its support
    // function has no extent along `plane`, so `h(plane) = 0`, and since the
    // depth is the minimum of `h` over all directions it is zero too: the shapes
    // touch and do not overlap. A face resting exactly on a face produces this,
    // and so does a point against a plane.
    const float flat_eps = 1e-7f * scale;
    if (height_up <= flat_eps || height_down <= flat_eps)
    {
        return false;
    }

    // ---- a tetrahedron, and then the SAME expansion the loop uses ---------
    //
    // **A BIPYRAMID BUILT BY HAND IS NOT ALWAYS CONVEX, AND IT IS NOT EVEN
    // USUALLY CONVEX.** Six faces stitched from the triangle to two apexes is a
    // convex solid only when each apex projects INSIDE the triangle, and a
    // support point along the plane normal has no reason to. Measured on crates
    // standing on a floor: **52.9% of seeds were not convex**, and 2.3% of
    // queries went on to stall with a negative lower bound.
    //
    // The gap between those two numbers is why this survived: half the seeds
    // were not polytopes and nineteen in twenty of those still came out with the
    // right answer, because the expansion usually repaired itself. Only a status
    // histogram over fifty thousand pairs made it visible at all.
    //
    // The repair is to stop hand-building it. Take the tetrahedron on the first
    // apex — always convex, because a tetrahedron is — and add the second apex
    // through `expand`, which is beneath-and-beyond and therefore produces the
    // convex hull by construction. The seed and the loop now share one notion of
    // what adding a vertex means.
    gjk_vertex tet[4] = {pts[tri[0]], pts[tri[1]], pts[tri[2]], up};
    if (!build_tetrahedron(p, tet)) { failure = epa_status::capacity; return false; }

    // The face `down` is beyond is the base — the one whose normal points away
    // from `up`, which after the winding fix is face 0. Handing it to `expand`
    // as the certainly-visible seed is exactly what the main loop does with its
    // closest face.
    if (!expand(p, down, 0, true)) { failure = epa_status::degenerate; return false; }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// The public pieces
// ---------------------------------------------------------------------------

const char* name_of(epa_status status)
{
    switch (status)
    {
        case epa_status::proven:          return "proven";
        case epa_status::touching:        return "touching";
        case epa_status::stalled:         return "stalled";
        case epa_status::iteration_limit: return "iteration limit";
        case epa_status::capacity:        return "capacity";
        case epa_status::degenerate:      return "degenerate";
    }
    return "?";
}

float depth_along(const convex& a, const convex& b, vec3 unit_axis)
{
    // `h_{A⊖B}(n) = max over the set of dot(x, n)`, and the support identity
    // says that is `support_A(n) − support_B(−n)`. One subtraction of world
    // positions, formed here and nowhere else, for `convex.hpp`'s reason.
    const vec3 delta = b.origin - a.origin;
    const vec3 sa = a.support(a.data, unit_axis);
    const vec3 sb = b.support(b.data, -unit_axis);
    return dot(sa - sb - delta, unit_axis);
}

// ---------------------------------------------------------------------------
// The expansion
// ---------------------------------------------------------------------------

epa_result epa_penetration(const convex& a, const convex& b, const simplex& start,
                           const epa_config& cfg)
{
    epa_result r;

    // THE ONE LARGE SUBTRACTION, taken once — 8.5 §11's finding, and EPA
    // inherits it intact because it inherits `cso_support`.
    const vec3 delta = b.origin - a.origin;

    polytope p;
    vec3 flat_normal{};
    epa_status failure = epa_status::degenerate;
    if (!seed_polytope(a, b, delta, start, p, r.seed_vertices, r.support_calls, flat_normal,
                       failure))
    {
        // A flat difference set has zero depth, and the plane normal is still
        // the right direction for a solver: it is the one along which the two
        // shapes have no overlap at all.
        r.status = failure;
        r.normal = normalised_or(flat_normal, vec3{0.0f, 1.0f, 0.0f});
        r.vertices = p.vertex_count;
        r.faces = p.face_count;
        r.surface_vertices = surface_vertex_count(p);
        return r;
    }

    const float scale = std::sqrt(p.scale2);

    // WHETHER THE ORIGIN IS INSIDE IS DECIDED AT THE END, NOT HERE, AND THAT
    // ORDERING IS A BUG FIX RATHER THAN a preference.
    //
    // The lower bound — "the nearest face of an inner polytope is no farther
    // than the set's boundary" — needs the origin inside the polytope to mean
    // anything, and GJK does not promise that: it reports `intersecting` when
    // the origin is within ITS contact margin of the simplex, which 8.5 §F.6
    // measured at `2.11 * tolerance * size` and is 0.4 mm on metre-sized crates
    // at the default. Worse, GJK can terminate on a SEGMENT through the origin
    // on a pair that overlaps deeply — the origin lying on a chord of the
    // difference set says nothing about how far inside it is — and the seed then
    // puts the origin on an EDGE of the polytope, where the incident faces'
    // offsets are zero give or take GJK's own margin.
    //
    // The first version of this function tested those offsets up front and
    // returned `touching` when any was negative. It reported a depth of ZERO for
    // spheres seventy centimetres inside a crate, 19 times in 20,000 — and the
    // second version, which scaled the threshold by `cfg.tolerance`, made the
    // failure come BACK when the tolerance was tightened, because EPA's
    // tolerance is not the one that set the margin. There is no constant here
    // that is right, because the question is being asked too early.
    //
    // So it is not asked. The expansion runs either way — beneath-and-beyond
    // does not care where the origin is — and whether the polytope ever got the
    // origin inside is read off `lower` at the end, where it is a measurement
    // rather than a guess.

    const float tol = cfg.tolerance;
    float upper = 3.4028235e38f;

    // The best proven answer so far, kept separately from the polytope so that a
    // failed expansion costs nothing. 8.5's monotonicity block restored a
    // 160-byte simplex; a polytope is seven kilobytes, so the answer is snapshot
    // instead of the state — which is the same discipline and a hundredth of the
    // copy.
    const int seed_face = closest_face(p);
    ENGINE_ASSERT(seed_face >= 0 && "seed_polytope returned a polytope with no live face");
    const face& seed_best = p.faces[seed_face];
    float lower = seed_best.d;
    vec3 best_normal = seed_best.n;
    gjk_vertex best_tri[3] = {p.vert[seed_best.v[0]], p.vert[seed_best.v[1]],
                              p.vert[seed_best.v[2]]};

    epa_status status = epa_status::iteration_limit;

    for (int it = 0; it < cfg.max_iterations; ++it)
    {
        r.iterations = it + 1;

        const int index = closest_face(p);
        if (index < 0) { status = epa_status::stalled; break; }

        const face f = p.faces[index];

        // MONOTONICITY IS A THEOREM HERE, EXACTLY AS |v| WAS IN GJK — and it is
        // checked for the same two reasons. Every expansion adds a vertex, so
        // the new polytope CONTAINS the old one, so its distance to the boundary
        // cannot decrease. A step that decreases it is not slow convergence; it
        // is a corrupted surface, which is what a torn horizon produces.
        if (f.d + tol * scale < lower)
        {
            status = epa_status::stalled;
            break;
        }

        lower = f.d;
        best_normal = f.n;
        best_tri[0] = p.vert[f.v[0]];
        best_tri[1] = p.vert[f.v[1]];
        best_tri[2] = p.vert[f.v[2]];

        const gjk_vertex w = cso_support(a, b, delta, f.n);
        r.support_calls += 2;

        // The sandwich. `f.d` is the distance from the origin to the nearest
        // face of a polytope INSIDE the difference set, so nothing in the set is
        // nearer than it once the origin is enclosed — a lower bound.
        // `dot(w, n)` is the supporting plane of the WHOLE set in that same
        // direction, so the depth is at most that — an upper bound, and it can
        // only improve, so the minimum over all directions tried is kept.
        const float h = dot(w.w, f.n);
        if (h < upper) { upper = h; }

        // Proven, to a tolerance relative to the SIZE OF THE SHAPES. Not
        // relative to the depth: a resting contact has a depth of microns or of
        // exactly zero, and a test relative to that would never terminate on the
        // commonest contact in the engine.
        if (h - f.d <= tol * scale)
        {
            status = epa_status::proven;
            break;
        }

        bool duplicate = false;
        for (int i = 0; i < p.vertex_count; ++i)
        {
            if (length_squared(p.vert[i].w - w.w) <= k_duplicate_rel2 * p.scale2)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
        {
            // On a polytope this is how the expansion FINISHES exactly: the
            // boundary face has been reached and there is no vertex beyond it.
            // The bound is therefore tight even though the tolerance test did
            // not fire, which is why this is `proven` and not `stalled`.
            status = epa_status::proven;
            break;
        }

        if (!expand(p, w, index, cfg.flood_fill_visible))
        {
            status = (p.vertex_count >= k_epa_max_vertices || p.face_count >= k_epa_max_faces)
                         ? epa_status::capacity
                         : epa_status::stalled;
            break;
        }
    }

    // ---- read the answer off the winning face ------------------------------
    //
    // The nearest point of the boundary is the origin projected onto that face's
    // plane, `n * d`. Its barycentric weights on the triangle are also the
    // weights of the contact points on the two ORIGINAL surfaces — the same
    // combination applied to three different vertex sets, which is 8.5's witness
    // trick used a second time and the reason `gjk_vertex` carries `pa` and `pb`
    // into a file that never looks at a shape.
    // THE ANSWER, AND THE TOUCHING TEST PROMISED ABOVE. A non-positive lower
    // bound means no expansion ever got the origin strictly inside, which is
    // what "these two are touching rather than overlapping" looks like from in
    // here. The normal is still the best direction found, and that is exactly
    // what a solver wants at a resting contact: zero to push out, and a
    // direction to push along when the next frame presses them together.
    if (lower <= 0.0f) { status = epa_status::touching; }

    r.status = status;
    r.lower = lower;
    r.upper = (upper < 3.4028235e38f) ? upper : lower;
    r.depth = std::fmax(0.0f, lower);
    r.normal = best_normal;
    r.vertices = p.vertex_count;
    r.faces = p.face_count;
    r.surface_vertices = surface_vertex_count(p);

    float w3[3] = {1.0f, 0.0f, 0.0f};
    if (barycentric(best_tri[0].w, best_tri[1].w, best_tri[2].w, best_normal * lower, w3))
    {
        // Clamp and renormalise. The projection of the origin lands inside the
        // globally closest face in exact arithmetic — if it did not, some edge
        // or neighbouring face would be nearer — so a negative weight here is
        // rounding at the rim, of order the tolerance, and clamping is the
        // repair that keeps the contact point ON the shapes.
        float sum = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            w3[i] = std::fmax(0.0f, w3[i]);
            sum += w3[i];
        }
        if (sum > 0.0f) { for (float& x : w3) { x /= sum; } }
        else { w3[0] = 1.0f; w3[1] = 0.0f; w3[2] = 0.0f; }
    }

    vec3 pa{};
    vec3 pb{};
    for (int i = 0; i < 3; ++i)
    {
        pa += best_tri[i].pa * w3[i];
        pb += best_tri[i].pb * w3[i];
    }
    r.point_a = a.origin + pa;
    r.point_b = b.origin + pb;
    return r;
}

} // namespace engine::phys
