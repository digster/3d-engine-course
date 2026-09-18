// engine/src/phys/manifold.cpp — clipping one face against another.
//
// Lesson 8.7. The header argues for the manifold; this file builds it. Four
// steps, and the third of them is an algorithm this course already taught:
//
//   1. ask each shape for the feature it presents along the contact normal
//      (`shape.hpp`'s `support_face`, new this lesson);
//   2. pick the more face-like of the two as the REFERENCE;
//   3. clip the other against the reference face's side planes — Sutherland and
//      Hodgman, which Lesson 3.3 wrote for the near plane and which is the same
//      algorithm here with a different set of planes;
//   4. keep what is actually penetrating, and reduce to four.
//
// ---- EVERYTHING IS RELATIVE TO `a.origin` ---------------------------------
//
// 8.4 §11's precision wall, fixed the same way GJK and EPA fix it and worth
// restating because it is easy to undo. Both faces arrive relative to their own
// shape's centre. The ONE world-sized subtraction — `b.origin - a.origin` — is
// formed here, once, and every clip, dot product and interpolation below runs on
// crate-sized numbers. `a.origin` is added back exactly once, at the end, when a
// contact point becomes a world position.
//
// Write `fb.v[i] + b.origin` anywhere in the middle of this file and a manifold
// a thousand kilometres from the world origin loses six digits of the only
// quantity a solver cares about.

#include <engine/phys/manifold.hpp>

#include <engine/core/assert.hpp>
#include <engine/phys/gjk.hpp>

#include <algorithm>
#include <cmath>

namespace engine::phys
{

namespace
{

/// A vertex of the polygon being clipped, plus everything needed to name it.
///
/// **TWO SETS OF FIELDS, AND CONFLATING THEM IS THE BUG THIS STRUCT PREVENTS.**
/// The `inc_vertex` / `cut_*` / `ref_vertex` fields say what MADE this point and
/// never change once written — they become its `contact_id`. The `out_*` fields
/// say which feature the segment LEAVING this point lies along, and every clip
/// pass rewrites them. One field cannot be both: a point cut by the second side
/// plane would be attributed to the first, and the ids would come out stable,
/// plausible and wrong — the worst combination there is, because nothing
/// downstream can tell. A warm start would then land on the wrong contact every
/// time a face is cut on two sides at once, which §7 measures at a third of all
/// the contact points a real mix produces.
struct clip_vertex
{
    vec3 p;                          ///< Relative to `a.origin`.

    std::uint8_t inc_vertex = 0xff;  ///< This IS incident polygon vertex n.
    std::uint8_t cut_inc_edge = 0xff;///< Made by cutting incident edge n.
    std::uint8_t cut_ref_edge = 0xff;///< ...with reference side plane n.
    std::uint8_t ref_vertex = 0xff;  ///< This IS reference polygon vertex n.

    std::uint8_t out_inc_edge = 0xff;///< The outgoing segment lies on incident edge n.
    std::uint8_t out_ref_edge = 0xff;///< The outgoing segment lies on reference plane n.
};

/// The reference polygon vertex shared by two side planes, or `0xff`.
///
/// Side plane `e` carries edge `e`, which runs from vertex `e` to vertex `e+1`.
/// Two planes therefore share a vertex only when their indices are adjacent, and
/// the shared vertex is the later one. A point produced by cutting a segment
/// that lay on plane `p` with plane `q` is exactly that shared corner — which is
/// how a REFERENCE vertex ends up inside the incident face, and how it gets an
/// id that names it rather than an id that names the two planes.
std::uint8_t shared_vertex(std::uint8_t p, std::uint8_t q, int n)
{
    if (p == 0xff || q == 0xff) { return 0xff; }
    const int pi = static_cast<int>(p);
    const int qi = static_cast<int>(q);
    if (qi == (pi + 1) % n) { return static_cast<std::uint8_t>(qi); }
    if (pi == (qi + 1) % n) { return static_cast<std::uint8_t>(pi); }
    return 0xff;
}

/// The closest points of two segments, as parameters along each.
///
/// Ericson's `ClosestPtSegmentSegment`, and it is the one place in this file
/// where the answer is a continuous function of the input rather than a discrete
/// choice — which is also why an edge-edge contact has no stable id beyond the
/// two edges themselves.
void closest_on_segments(vec3 p1, vec3 q1, vec3 p2, vec3 q2, float& s, float& t)
{
    const vec3 d1 = q1 - p1;
    const vec3 d2 = q2 - p2;
    const vec3 r = p1 - p2;
    const float a = length_squared(d1);
    const float e = length_squared(d2);
    const float f = dot(d2, r);

    constexpr float k_tiny = 1e-20f;
    if (a <= k_tiny && e <= k_tiny) { s = 0.0f; t = 0.0f; return; }
    if (a <= k_tiny)
    {
        s = 0.0f;
        t = std::fmin(1.0f, std::fmax(0.0f, f / e));
        return;
    }
    const float c = dot(d1, r);
    if (e <= k_tiny)
    {
        t = 0.0f;
        s = std::fmin(1.0f, std::fmax(0.0f, -c / a));
        return;
    }

    const float b = dot(d1, d2);
    const float denom = a * e - b * b;
    s = (denom > k_tiny) ? std::fmin(1.0f, std::fmax(0.0f, (b * f - c * e) / denom)) : 0.0f;
    t = (b * s + f) / e;
    if (t < 0.0f)
    {
        t = 0.0f;
        s = std::fmin(1.0f, std::fmax(0.0f, -c / a));
    }
    else if (t > 1.0f)
    {
        t = 1.0f;
        s = std::fmin(1.0f, std::fmax(0.0f, (b - c) / a));
    }
}

/// Twice the signed area of a triangle, measured about `n`.
float signed_area2(vec3 a, vec3 b, vec3 c, vec3 n)
{
    return dot(cross(b - a, c - a), n);
}

/// **Reduce a clipped contact set to at most four points.**
///
/// The clip can produce up to sixteen, and a solver can use four. Which four is
/// not a matter of taste: the manifold's job is to span the largest support
/// polygon it can, because that polygon is what resists rotation (see
/// `contact_manifold::support_area`). So:
///
///   1. the DEEPEST point, because it is the one the solver most has to fix;
///   2. the point FARTHEST from it, which fixes the longest axis of the set;
///   3. and 4. the points maximising triangle area on each SIDE of that axis,
///      which is what stops the four collapsing onto a line.
///
/// Every tie is broken by the lower index, so the result is a deterministic
/// function of the clip output — which it has to be, because a manifold that
/// reorders itself when nothing moved produces four new `contact_id`s and throws
/// away every warm start.
///
/// The output is left in CYCLIC order (`0, 2, 1, 3`) rather than selection
/// order, so the four points trace the polygon rather than a bowtie. That costs
/// nothing and makes `support_area` and every debug drawing correct by
/// construction.
int reduce_to_four(contact_point* pts, int n, vec3 normal)
{
    if (n <= k_max_manifold_points) { return n; }

    int i0 = 0;
    for (int i = 1; i < n; ++i)
    {
        if (pts[i].depth > pts[i0].depth) { i0 = i; }
    }

    int i1 = -1;
    float far2 = -1.0f;
    for (int i = 0; i < n; ++i)
    {
        if (i == i0) { continue; }
        const float d2 = length_squared(pts[i].position - pts[i0].position);
        if (d2 > far2) { far2 = d2; i1 = i; }
    }
    if (i1 < 0) { return 1; }

    int i2 = -1;
    int i3 = -1;
    float best_pos = 0.0f;
    float best_neg = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        if (i == i0 || i == i1) { continue; }
        const float area = signed_area2(pts[i0].position, pts[i1].position, pts[i].position, normal);
        if (area > best_pos) { best_pos = area; i2 = i; }
        if (area < best_neg) { best_neg = area; i3 = i; }
    }

    // ONE SIDE EMPTY IS NOT A REASON TO RETURN THREE POINTS, and the first
    // version of this function did. It happens when the deepest point and the
    // one farthest from it are ADJACENT on the contact polygon rather than
    // opposite, which leaves every remaining candidate on one side of the line
    // between them — and the three points kept then span half the polygon.
    // The fix is four lines: when a side has no candidate, take the best of what
    // is left by ABSOLUTE area instead. Measured over 61,455 manifolds that
    // clipped past four, the mean area kept went from 88.27% of the true contact
    // polygon to 88.87% — a small number for four lines, and not the reason to
    // keep them. The reason is that a three-point manifold where four points
    // exist resists rotation about one axis for no reason at all, which is a
    // SHAPE of failure rather than a magnitude of one. §8 measures the whole
    // reduction at 96.3% of the best four points there are.
    if (i2 < 0 || i3 < 0)
    {
        int& empty = (i2 < 0) ? i2 : i3;
        float best = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if (i == i0 || i == i1 || i == i2 || i == i3) { continue; }
            const float area = std::fabs(
                signed_area2(pts[i0].position, pts[i1].position, pts[i].position, normal));
            if (area > best) { best = area; empty = i; }
        }
    }

    contact_point keep[k_max_manifold_points];
    int m = 0;
    keep[m++] = pts[i0];
    if (i2 >= 0) { keep[m++] = pts[i2]; }
    keep[m++] = pts[i1];
    if (i3 >= 0) { keep[m++] = pts[i3]; }

    // A degenerate set — every candidate collinear with the first two — still
    // leaves only two points, and that is the honest answer: the contact really
    // is a line, and padding it out with a collinear third would report a
    // support polygon of zero area as though it had some.
    for (int i = 0; i < m; ++i) { pts[i] = keep[i]; }
    return m;
}

/// **The edge of a polygon that faces most nearly along `d`.**
///
/// Called when a shape presents a polygon but the contact normal is too far off
/// its plane to clip against — two boxes meeting edge to edge, where each one's
/// `support_face` dutifully returns a whole face and the contact is on one of
/// its four edges. Using `v[0]` and `v[1]` here instead, which is the obvious
/// spelling, takes an ARBITRARY edge of that face, and the contact point then
/// lands wherever those two vertices happen to be.
///
/// Find the vertex that reaches farthest along `d`; the contacting edge is one
/// of the two meeting there. Choose the one more PERPENDICULAR to `d`, because
/// an edge lying along the contact normal is pointing into the other shape while
/// an edge across it is the one lying against it. Ties go to the incoming edge,
/// deterministically.
///
/// The returned face keeps the polygon's winding, so `v[0]` is the edge's start
/// and `id[0]` names it: in a convex polygon every vertex starts exactly one
/// edge, so a start vertex is a unique name for an edge within its face.
contact_face supporting_edge(const contact_face& f, vec3 d)
{
    if (f.count < 3) { return f; }

    int k = 0;
    float best = dot(f.v[0], d);
    for (int i = 1; i < f.count; ++i)
    {
        const float v = dot(f.v[i], d);
        if (v > best) { best = v; k = i; }
    }

    const int prev = (k - 1 + f.count) % f.count;
    const int next = (k + 1) % f.count;
    const vec3 in_edge = f.v[k] - f.v[prev];
    const vec3 out_edge = f.v[next] - f.v[k];
    const float in_align = std::fabs(dot(normalised_or(in_edge, d), d));
    const float out_align = std::fabs(dot(normalised_or(out_edge, d), d));

    contact_face e;
    e.normal = f.normal;
    e.feature = f.feature;
    e.truncated = f.truncated;
    e.count = 2;
    const int start = (in_align <= out_align) ? prev : k;
    const int end = (in_align <= out_align) ? k : next;
    e.v[0] = f.v[start];
    e.v[1] = f.v[end];
    e.id[0] = f.id[start];
    e.id[1] = f.id[end];
    return e;
}

/// The extent of a face: how far its farthest vertex is from its shape's centre.
/// The length every relative tolerance in this file is relative to.
float face_extent(const contact_face& f)
{
    float best = 0.0f;
    for (int i = 0; i < f.count; ++i)
    {
        const float m2 = length_squared(f.v[i]);
        if (m2 > best) { best = m2; }
    }
    return std::sqrt(best);
}

} // namespace

// ---------------------------------------------------------------------------
// Names
// ---------------------------------------------------------------------------

const char* name_of(contact_feature feature)
{
    switch (feature)
    {
    case contact_feature::incident_vertex: return "incident vertex";
    case contact_feature::crossing: return "crossing";
    case contact_feature::reference_vertex: return "reference vertex";
    case contact_feature::edge_pair: return "edge pair";
    case contact_feature::point: return "point";
    }
    return "point";
}

const char* name_of(manifold_status status)
{
    switch (status)
    {
    case manifold_status::none: return "none";
    case manifold_status::face: return "face";
    case manifold_status::edge: return "edge";
    case manifold_status::point: return "point";
    case manifold_status::clip_empty: return "clip empty";
    }
    return "none";
}

// ---------------------------------------------------------------------------
// contact_manifold
// ---------------------------------------------------------------------------

float contact_manifold::deepest() const
{
    float best = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        if (points[i].depth > best) { best = points[i].depth; }
    }
    return best;
}

float contact_manifold::support_area() const
{
    if (count < 3) { return 0.0f; }
    if (count == 3)
    {
        return 0.5f
               * std::fabs(signed_area2(points[0].position, points[1].position,
                                        points[2].position, normal));
    }

    // FOUR POINTS, AND THE ORDER MIGHT NOT BE A POLYGON. `reduce_to_four` leaves
    // them cyclic, but a manifold built with `reduce` turned off keeps whatever
    // order the clipper produced, and this function is the instrument §7 uses to
    // compare the two. So take the largest of the three distinct cyclic orders,
    // which for four points IS the area of their convex hull: a bowtie ordering
    // always measures smaller than the hull it crosses.
    const vec3 p0 = points[0].position;
    const vec3 p1 = points[1].position;
    const vec3 p2 = points[2].position;
    const vec3 p3 = points[3].position;
    const float a = std::fabs(signed_area2(p0, p1, p2, normal))
                    + std::fabs(signed_area2(p0, p2, p3, normal));
    const float b = std::fabs(signed_area2(p0, p1, p3, normal))
                    + std::fabs(signed_area2(p0, p3, p2, normal));
    const float c = std::fabs(signed_area2(p0, p2, p1, normal))
                    + std::fabs(signed_area2(p0, p1, p3, normal));
    return 0.5f * std::fmax(a, std::fmax(b, c));
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

contact_manifold build_manifold(const convex& a, const convex& b, vec3 normal, float depth,
                                const manifold_config& cfg)
{
    contact_manifold m;
    const vec3 n = normalised_or(normal, vec3{0.0f, 1.0f, 0.0f});
    m.normal = n;

    // THE ONE LARGE SUBTRACTION, taken once. See the file header.
    const vec3 delta = b.origin - a.origin;

    // ---- 1. what feature does each shape present? --------------------------
    //
    // `a` looks along `+n` because the normal points from `a` toward `b`; `b`
    // looks back along `-n`. Getting one of these backwards returns the far side
    // of the shape and produces a manifold whose points are a whole diameter
    // away — which looks, on screen, exactly like a body being flung across the
    // level by a solver that is working perfectly.
    const contact_face fa = a.face_of(n);
    const contact_face fb = b.face_of(-n);
    m.truncated = fa.truncated || fb.truncated;

    const float scale = std::fmax(face_extent(fa), face_extent(fb));

    // ---- 2. reference and incident ----------------------------------------
    //
    // A feature counts as a FACE only if it has three or more vertices AND its
    // own plane normal agrees with the contact normal to `face_cos`. The second
    // condition is the one that matters: `support_face` always returns a box's
    // dominant face, even when the contact is on an edge, and clipping against a
    // face that is thirty degrees off the contact normal produces four points
    // none of which is on the surface.
    const float cos_a = dot(fa.normal, n);
    const float cos_b = dot(fb.normal, -n);
    const bool a_is_face = (fa.count >= 3) && (cos_a >= cfg.face_cos);
    const bool b_is_face = (fb.count >= 3) && (cos_b >= cfg.face_cos);

    if (a_is_face || b_is_face)
    {
        // WHICH ONE IS THE REFERENCE. It supplies the clipping planes and the
        // plane every depth is measured from, so the choice changes the answer —
        // not by much on a face-to-face contact, but it changes every
        // `contact_id`, because `flipped` is part of the identity.
        //
        // **A BARE `>` IS SAFE HERE AND IT IS WORTH KNOWING WHY**, because the
        // same line in a 2D engine is not. On a face contact both cosines are
        // exactly `1.0f` — each face really is perpendicular to the contact
        // normal — so the tie is EXACT, and an exact tie is resolved
        // deterministically: `a` wins, this frame and every frame. What would be
        // unsafe is comparing the two candidate SEPARATIONS instead, which are
        // genuinely different floats that a nanometre of motion can reorder.
        // 8.7 §9 measures it: **zero flips in 208,000 pairs**, over freely
        // rotated boxes, crates stacked on crates, and hexagonal prisms whose
        // face normals come out of Newell's method rather than out of a rotation
        // matrix. Its control forces a flip by swapping the two arguments and
        // measures what one would cost: every warm start on the pair.
        const bool ref_on_b = (a_is_face && b_is_face) ? (cos_b > cos_a) : b_is_face;

        const contact_face& ref = ref_on_b ? fb : fa;
        const contact_face& inc = ref_on_b ? fa : fb;

        // Both polygons into one frame, relative to `a.origin`.
        vec3 rv[k_max_face_vertices];
        vec3 iv[k_max_face_vertices];
        for (int i = 0; i < ref.count; ++i) { rv[i] = ref_on_b ? ref.v[i] + delta : ref.v[i]; }
        for (int i = 0; i < inc.count; ++i) { iv[i] = ref_on_b ? inc.v[i] : inc.v[i] + delta; }
        const vec3 rn = ref.normal;

        // ---- 3. Sutherland-Hodgman, against the reference face's sides -----
        //
        // 3.3 clipped a triangle against the near plane by walking its edges and
        // asking, for each, whether it crosses. This is the same walk against
        // `ref.count` planes instead of one, and the planes are built from the
        // polygon rather than given: for edge `v[e] -> v[e+1]` the outward
        // in-plane direction is `cross(edge, normal)`, which points AWAY from a
        // polygon wound counter-clockwise about that normal — 8.7 §7 draws it,
        // and `shape.hpp`'s winding rule exists for this line.
        //
        // The side normal is deliberately NOT normalised. Sutherland-Hodgman
        // needs only the sign of each offset and the ratio of two of them, and
        // both are invariant under a positive scale, so a square root per plane
        // per pair would buy exactly nothing.
        clip_vertex buf[2][k_max_clip_points];
        int cur = 0;
        int count = inc.count;
        for (int i = 0; i < inc.count; ++i)
        {
            clip_vertex& v = buf[0][i];
            v.p = iv[i];
            v.inc_vertex = static_cast<std::uint8_t>(i);
            v.cut_inc_edge = 0xff;
            v.cut_ref_edge = 0xff;
            v.ref_vertex = 0xff;
            v.out_inc_edge = static_cast<std::uint8_t>(i);
            v.out_ref_edge = 0xff;
        }

        for (int e = 0; e < ref.count && count > 0; ++e)
        {
            const vec3 a0 = rv[e];
            const vec3 a1 = rv[(e + 1) % ref.count];
            const vec3 side = cross(a1 - a0, rn);
            const float offset = dot(side, a0);

            const int dst = cur ^ 1;
            int out = 0;
            for (int i = 0; i < count; ++i)
            {
                const clip_vertex& c = buf[cur][i];
                const clip_vertex& nx = buf[cur][(i + 1) % count];
                const float dc = dot(side, c.p) - offset;
                const float dn = dot(side, nx.p) - offset;

                if (dc <= 0.0f && out < k_max_clip_points) { buf[dst][out++] = c; }

                if ((dc < 0.0f) != (dn < 0.0f) && out < k_max_clip_points)
                {
                    const float t = dc / (dc - dn);
                    clip_vertex x;
                    x.p = c.p + (nx.p - c.p) * t;
                    x.inc_vertex = 0xff;

                    // IDENTITY COMES FROM THE SEGMENT BEING CUT, not from the
                    // vertex that starts it. A segment leaving `c` lies either
                    // on an incident edge (an ordinary crossing) or along an
                    // earlier reference plane (in which case cutting it produces
                    // a CORNER of the reference face).
                    if (c.out_inc_edge != 0xff)
                    {
                        x.cut_inc_edge = c.out_inc_edge;
                        x.cut_ref_edge = static_cast<std::uint8_t>(e);
                        x.ref_vertex = 0xff;
                    }
                    else
                    {
                        x.cut_inc_edge = 0xff;
                        x.cut_ref_edge = 0xff;
                        x.ref_vertex = shared_vertex(c.out_ref_edge,
                                                     static_cast<std::uint8_t>(e), ref.count);
                    }

                    // The outgoing segment: leaving the half-space means the
                    // next stretch runs along plane `e`; entering it means the
                    // stretch continues along whatever `c` was on.
                    const bool leaving = (dc <= 0.0f);
                    x.out_inc_edge = leaving ? 0xff : c.out_inc_edge;
                    x.out_ref_edge = leaving ? static_cast<std::uint8_t>(e) : c.out_ref_edge;

                    buf[dst][out++] = x;
                }
            }
            cur = dst;
            count = out;
        }

        // ---- 4. keep what is penetrating, and name it ----------------------
        contact_point kept[k_max_clip_points];
        int kept_count = 0;
        const float keep = cfg.keep_slop * scale;
        for (int i = 0; i < count; ++i)
        {
            const clip_vertex& v = buf[cur][i];
            const float sep = dot(rn, v.p - rv[0]);
            if (sep > keep) { continue; }

            contact_point& cp = kept[kept_count++];
            cp = contact_point{};
            cp.depth = -sep;
            // Halfway between the incident point and its shadow on the
            // reference plane. See `contact_point::position`.
            cp.position = a.origin + v.p - rn * (sep * 0.5f);

            contact_id id;
            id.reference_face = ref.feature;
            id.incident_face = inc.feature;
            id.flipped = ref_on_b ? 1u : 0u;
            if (v.inc_vertex != 0xff)
            {
                id.kind = contact_feature::incident_vertex;
                id.reference_index = 0xff;
                id.incident_index = inc.id[v.inc_vertex];
            }
            else if (v.cut_inc_edge != 0xff)
            {
                id.kind = contact_feature::crossing;
                id.reference_index = ref.id[v.cut_ref_edge];
                id.incident_index = inc.id[v.cut_inc_edge];
            }
            else if (v.ref_vertex != 0xff)
            {
                id.kind = contact_feature::reference_vertex;
                id.reference_index = ref.id[v.ref_vertex];
                id.incident_index = 0xff;
            }
            else
            {
                // A cut of a reference-plane stretch by a NON-adjacent plane.
                // Convex polygons do not have those, so this is rounding at a
                // corner; it is kept, with an id that says only which faces met.
                id.kind = contact_feature::crossing;
                id.reference_index = 0xff;
                id.incident_index = 0xff;
            }
            cp.id = id;
        }

        if (kept_count > 0)
        {
            m.clipped = kept_count;
            // THE STATUS DESCRIBES THE CONTACT, NOT THE CODE PATH. A ball on a
            // floor goes through this branch — the floor really does supply a
            // reference face and the ball's one support point really is clipped
            // against it — and calling the result a face contact would tell a
            // reader the contact has an area. It does not. What decides is the
            // INCIDENT feature: a single vertex against a plane is a point.
            m.status = (inc.count >= 2) ? manifold_status::face : manifold_status::point;
            m.reference_on_b = ref_on_b;
            // THE MANIFOLD NORMAL IS THE REFERENCE FACE'S, oriented from `a`
            // toward `b`. It is exact where EPA's is converged-to, because it is
            // a property of a real shape rather than of a polytope approximating
            // the difference set.
            m.normal = ref_on_b ? -rn : rn;
            const int n_kept =
                cfg.reduce ? reduce_to_four(kept, kept_count, m.normal)
                           : std::min(kept_count,
                                      std::min(cfg.max_points_unreduced, k_max_clip_points));
            for (int i = 0; i < n_kept; ++i) { m.points[i] = kept[i]; }
            m.count = n_kept;
            return m;
        }

        // THE CLIP KEPT NOTHING, WHICH SHOULD BE IMPOSSIBLE and happens about
        // once in two hundred thousand random pairs. Two convex shapes that
        // overlap must have overlapping projections onto the contact plane, so
        // an empty result means the reference face was not the face the contact
        // is on — a near-tie between two faces resolved the wrong way, or a
        // shape whose `support_face` and `support` disagree at a corner.
        //
        // WHAT TO FALL BACK TO IS THE INTERESTING PART. The obvious answer is
        // the pair of support points along the normal, and it is a bad one: on a
        // shallow corner contact those are two DIFFERENT corners, tens of
        // centimetres apart in the tangent plane, and their midpoint is near
        // neither surface — measured at 0.30 m on the one case in 189,282 that
        // reached it. If the clipper produced geometry at all, the least
        // separated of its points is on both faces by construction and is a far
        // better answer than a fabricated one.
        m.status = manifold_status::clip_empty;
        if (count > 0)
        {
            int best = 0;
            float best_sep = 3.4028235e38f;
            for (int i = 0; i < count; ++i)
            {
                const float sep = dot(rn, buf[cur][i].p - rv[0]);
                if (sep < best_sep) { best_sep = sep; best = i; }
            }
            const clip_vertex& v = buf[cur][best];
            contact_point& cp = m.points[0];
            cp = contact_point{};
            cp.depth = std::fmax(0.0f, -best_sep);
            cp.position = a.origin + v.p - rn * (best_sep * 0.5f);
            cp.id.reference_face = ref.feature;
            cp.id.incident_face = inc.feature;
            cp.id.kind = contact_feature::point;
            cp.id.flipped = ref_on_b ? 1u : 0u;
            m.normal = ref_on_b ? -rn : rn;
            m.reference_on_b = ref_on_b;
            m.count = 1;
            m.clipped = 1;
            return m;
        }
    }

    // ---- the cases that are genuinely not faces ----------------------------

    // A POLYGON THAT WAS NOT PARALLEL ENOUGH IS STILL TOUCHING ALONG ONE OF ITS
    // EDGES, and using `fa.v[0]` and `fa.v[1]` here — an arbitrary edge of the
    // face — was the first draft's most embarrassing bug: two boxes meeting
    // corner to corner reported a contact point on the far side of both.
    const contact_face ea = supporting_edge(fa, n);
    const contact_face eb = supporting_edge(fb, -n);

    if (ea.count >= 2 && eb.count >= 2 && m.status != manifold_status::clip_empty)
    {
        // TWO EDGES CROSSING TOUCH AT EXACTLY ONE PLACE, and reporting one point
        // is not a shortfall — it is the answer. Two pencils laid across each
        // other meet at a point, and a manifold claiming four there would let a
        // solver resist a rotation that nothing is resisting.
        float s = 0.0f;
        float t = 0.0f;
        closest_on_segments(ea.v[0], ea.v[1], eb.v[0] + delta, eb.v[1] + delta, s, t);
        const vec3 pa = ea.v[0] + (ea.v[1] - ea.v[0]) * s;
        const vec3 pb = (eb.v[0] + delta) + (eb.v[1] - eb.v[0]) * t;

        contact_point& cp = m.points[0];
        cp = contact_point{};
        cp.position = a.origin + (pa + pb) * 0.5f;
        cp.depth = std::fmax(0.0f, depth);
        cp.id.reference_face = ea.feature;
        cp.id.incident_face = eb.feature;
        cp.id.reference_index = ea.id[0];
        cp.id.incident_index = eb.id[0];
        cp.id.kind = contact_feature::edge_pair;
        m.count = 1;
        m.clipped = 1;
        m.status = manifold_status::edge;
        return m;
    }

    // A curved surface, or a clip that produced nothing. One point, taken from
    // the support function rather than from either face, because the faces are
    // exactly what has just been ruled out.
    {
        const vec3 pa = a.support(a.data, n);
        const vec3 pb = b.support(b.data, -n) + delta;
        contact_point& cp = m.points[0];
        cp = contact_point{};
        cp.position = a.origin + (pa + pb) * 0.5f;
        cp.depth = std::fmax(0.0f, depth);
        cp.id.reference_face = fa.feature;
        cp.id.incident_face = fb.feature;
        cp.id.kind = contact_feature::point;
        m.count = 1;
        m.clipped = m.clipped > 0 ? m.clipped : 1;
        if (m.status != manifold_status::clip_empty) { m.status = manifold_status::point; }
        return m;
    }
}

contact_manifold collide_manifold(const convex& a, const convex& b, const manifold_config& cfg)
{
    const gjk_result g = gjk_distance(a, b);
    if (g.status == gjk_status::separated)
    {
        // No speculative margin. See the header: a pair that is a micron short
        // of touching gets no manifold, and 8.9 is where that changes.
        return contact_manifold{};
    }

    const epa_result e = epa_penetration(a, b, g.terminal);
    if (e.status == epa_status::degenerate) { return contact_manifold{}; }
    return build_manifold(a, b, e.normal, e.depth, cfg);
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

int carry_impulses(contact_manifold& fresh, const contact_manifold& previous)
{
    int matched = 0;
    for (int i = 0; i < fresh.count; ++i)
    {
        for (int j = 0; j < previous.count; ++j)
        {
            if (!(fresh.points[i].id == previous.points[j].id)) { continue; }
            fresh.points[i].normal_impulse = previous.points[j].normal_impulse;
            fresh.points[i].tangent_impulse[0] = previous.points[j].tangent_impulse[0];
            fresh.points[i].tangent_impulse[1] = previous.points[j].tangent_impulse[1];
            fresh.points[i].warm = true;
            ++matched;
            break;
        }
    }
    fresh.warm_points = matched;
    return matched;
}

std::uint64_t pair_key(std::uint32_t a, std::uint32_t b)
{
    const std::uint32_t lo = (a < b) ? a : b;
    const std::uint32_t hi = (a < b) ? b : a;
    return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
}

namespace
{

/// SplitMix64's finaliser. Two multiplies and three shifts.
///
/// A pair key is two small integers side by side, so its low bits are one body's
/// index and its high bits the other's — and a table indexed by `key & mask`
/// would put every pair involving the same body into the same handful of slots.
/// Every body in a stack collides with its neighbour, so that is not a
/// hypothetical: it is the arrangement the cache exists for.
std::uint64_t mix(std::uint64_t x)
{
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

} // namespace

void manifold_cache::begin_frame()
{
    ++frame_;
}

const contact_manifold* manifold_cache::find(std::uint64_t key) const
{
    if (slots_.empty()) { return nullptr; }
    const std::size_t mask = slots_.size() - 1;
    std::size_t i = static_cast<std::size_t>(mix(key)) & mask;
    for (std::size_t probe = 0; probe < slots_.size(); ++probe)
    {
        const slot& s = slots_[i];
        if (!s.used) { return nullptr; }
        if (s.key == key) { return &s.value; }
        i = (i + 1) & mask;
    }
    return nullptr;
}

void manifold_cache::store(std::uint64_t key, const contact_manifold& m)
{
    // GROW AT HALF FULL. Linear probing degrades sharply past about 70% — the
    // expected probe length is `(1 + 1/(1-load)^2)/2`, which is 2 at half full
    // and 13 at 90% — and a physics cache is walked once per pair per frame, so
    // the probe length is multiplied by everything touching everything.
    if (slots_.empty() || static_cast<std::size_t>(count_ + 1) * 2 > slots_.size())
    {
        rehash(slots_.empty() ? 64 : slots_.size() * 2);
    }

    const std::size_t mask = slots_.size() - 1;
    std::size_t i = static_cast<std::size_t>(mix(key)) & mask;
    for (std::size_t probe = 0; probe < slots_.size(); ++probe)
    {
        slot& s = slots_[i];
        if (!s.used)
        {
            s.used = true;
            s.key = key;
            s.value = m;
            s.stamp = frame_;
            ++count_;
            return;
        }
        if (s.key == key)
        {
            s.value = m;
            s.stamp = frame_;
            return;
        }
        i = (i + 1) & mask;
    }
    ENGINE_ASSERT(false && "manifold_cache: table full after growth");
}

int manifold_cache::end_frame()
{
    if (slots_.empty()) { return 0; }

    // NO TOMBSTONES. Deleting from an open-addressed table means either a
    // deleted marker — which accumulates until a rehash, and this table deletes
    // on most frames because pairs are born and die as things move — or a
    // rebuild. The rebuild is O(capacity) once per frame, against O(pairs) of
    // work the caller has already done walking them, so it is free in the only
    // sense that matters.
    int dropped = 0;
    std::vector<slot> live;
    live.reserve(static_cast<std::size_t>(count_));
    for (const slot& s : slots_)
    {
        if (!s.used) { continue; }
        if (s.stamp == frame_) { live.push_back(s); }
        else { ++dropped; }
    }
    if (dropped == 0) { return 0; }

    std::size_t wanted = 64;
    while (wanted < live.size() * 2) { wanted *= 2; }
    slots_.assign(wanted, slot{});
    count_ = 0;
    // `store` stamps each entry with the current frame, which is what these
    // already were: they are exactly the ones that survived it.
    for (const slot& s : live) { store(s.key, s.value); }
    return dropped;
}

void manifold_cache::rehash(std::size_t wanted)
{
    std::vector<slot> old;
    old.swap(slots_);
    slots_.assign(wanted, slot{});
    const int old_count = count_;
    count_ = 0;
    for (const slot& s : old)
    {
        if (!s.used) { continue; }
        const std::size_t mask = slots_.size() - 1;
        std::size_t i = static_cast<std::size_t>(mix(s.key)) & mask;
        while (slots_[i].used) { i = (i + 1) & mask; }
        slots_[i] = s;
        ++count_;
    }
    ENGINE_ASSERT(count_ == old_count && "manifold_cache: lost entries in rehash");
    (void)old_count;
}

void manifold_cache::clear()
{
    slots_.clear();
    count_ = 0;
    frame_ = 1;
}

} // namespace engine::phys
