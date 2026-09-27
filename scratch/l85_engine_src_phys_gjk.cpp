// engine/src/phys/gjk.cpp — the search, and the simplex solver underneath it.
//
// Lesson 8.5. The loop in `gjk_distance` is fifteen lines and reads like the
// four-step sketch in the header. Everything else in this file is
// `reduce_simplex`, which is the part nobody's blog post writes out: given up to
// four points, find the one closest to the origin and throw away the vertices
// that do not help. It is where GJK's correctness lives and where its bugs live.

#include <engine/phys/gjk.hpp>

#include <engine/core/assert.hpp>

#include <cmath>

namespace engine::phys
{
namespace
{

/// Two simplex vertices closer than `sqrt` of this, relative to the largest
/// vertex seen, are the same vertex.
///
/// **NOT the user's tolerance, and the difference is a lesson.** This test
/// exists to catch a support function returning a point the simplex already
/// has, which on a POLYTOPE is how the search finishes exactly: the vertex set
/// is finite, so once the best one repeats there is nothing left to find.
///
/// The first version used `cfg.tolerance` here, which is 1e-4 by default, and it
/// was a quiet disaster on curved shapes. A capsule's support point moves
/// continuously with the direction, so two consecutive iterations produce points
/// a fraction of a millimetre apart — "duplicates" by a 1e-4 relative measure —
/// and the loop exited before the bounds had converged. Measured: capsule gaps
/// reported with a certificate slack of **2.4% where the tolerance promised
/// 0.01%**, on results that were not flagged as stalled and therefore looked
/// trustworthy. The threshold belongs at float's own resolution, where an exact
/// repeat still trips it and a genuine small step does not.
constexpr float k_duplicate_rel2 = 1e-12f;

/// Is `p` on the opposite side of plane `abc` from `d`?
///
/// The reference point `d` is what turns a plane into a HALF-SPACE without
/// anybody having to know which way round the tetrahedron was wound. `n` is a
/// normal of the plane; `d` is the vertex not on it, so `dot(d − a, n)` is the
/// sign of "inside"; and `p` is outside when its sign disagrees.
///
/// THE COPLANAR CASE IS THE ONE THAT MATTERS AND IT IS NOT AN EDGE CASE. GJK
/// generates degenerate tetrahedra routinely — two boxes resting face to face
/// put four support points on one plane — and there `dot(d − a, n)` is zero or
/// noise, so the sign it supplies is meaningless. Reporting "outside" in that
/// case is the conservative answer and it is also the CORRECT one: a flat
/// tetrahedron has no interior, so nothing is inside it, and the caller goes on
/// to find a closest point on the face instead of claiming containment.
[[nodiscard]] bool outside_plane(vec3 p, vec3 a, vec3 b, vec3 c, vec3 d)
{
    const vec3 n = cross(b - a, c - a);
    const float sp = dot(p - a, n);
    const float sd = dot(d - a, n);

    // Relative degeneracy test: `sd` has the units of |n| * |d − a|, so compare
    // its square against that product rather than against a constant in metres.
    // 8.4 §11 is the lesson — a threshold with units must be scaled by something
    // with the same units or it means a different thing on every scene.
    const float scale = length_squared(n) * length_squared(d - a);
    if (sd * sd <= 1e-12f * scale) { return true; }

    return sp * sd < 0.0f;
}

/// Closest point on segment `v[0]v[1]` to the origin. Reduces and weights.
void reduce_segment(simplex& s, vec3& closest, float w[4])
{
    const vec3 a = s.v[0].w;
    const vec3 b = s.v[1].w;
    const vec3 ab = b - a;

    const float denom = length_squared(ab);
    if (denom <= 0.0f)
    {
        // The two points coincide. Keep one; the duplicate guard in the main
        // loop normally prevents this, and "normally" is not a reason to divide
        // by zero here.
        s.count = 1;
        closest = a;
        w[0] = 1.0f;
        return;
    }

    // The origin projected onto the line: t = dot(0 − a, ab) / |ab|².
    const float t = dot(-a, ab) / denom;

    if (t <= 0.0f)
    {
        s.count = 1;                       // keep a
        closest = a;
        w[0] = 1.0f;
    }
    else if (t >= 1.0f)
    {
        s.v[0] = s.v[1];                   // keep b
        s.count = 1;
        closest = b;
        w[0] = 1.0f;
    }
    else
    {
        closest = a + ab * t;
        w[0] = 1.0f - t;
        w[1] = t;
    }
}

/// Closest point on triangle `v[0]v[1]v[2]` to the origin. Reduces and weights.
///
/// FOUR CANDIDATES, AND THE MINIMUM WINS. The closest point of a triangle to any
/// point is either in its interior or on one of its three edges — and a CLAMPED
/// edge already includes its own two endpoints, so three edges cover all six of
/// the boundary regions. Evaluate the interior projection, evaluate the three
/// edges, take the smallest. There is no case analysis, because there is nothing
/// to decide: every candidate is computed and compared.
///
/// THE TEXTBOOK VERSION DECIDES INSTEAD, and this one does not, for a measured
/// reason. Ericson's `ClosestPtPointTriangle` — the one every implementation
/// copies, this course's first draft included — identifies which of the seven
/// Voronoi regions the query point is in using six dot products and three 2x2
/// determinants, then computes only that region's answer. It is genuinely faster
/// and it is correct in exact arithmetic. But the three determinants are signed
/// AREAS, differences of nearly equal products, and on a thin triangle they are
/// noise — so the region it selects can be the wrong one, and the "closest"
/// point it returns is then the foot of the perpendicular on the triangle's
/// PLANE, which can be arbitrarily nearer the origin than the triangle is.
///
/// THAT FAILURE IS NOT THEORETICAL AND IT IS NOT RARE. GJK's whole job is to
/// drive the simplex onto the closest feature, so by the last iteration its
/// triangles are as thin as the problem allows — the degenerate case is where
/// the algorithm SPENDS ITS TIME. Measured on a sphere against a turned box:
/// eighteen iterations converging cleanly to 0.0233827 m against a true
/// 0.0233828, then one reduction returning 0.0674522, three times too far.
/// 8.5 §C.5 measures both the disagreement rate and the price of not deciding.
///
/// The interior candidate is skipped when its barycentric coordinates come out
/// negative, which covers both "the origin projects outside the triangle" and
/// "the triangle is too thin to have an inside". Either way the edges answer.
void reduce_triangle(simplex& s, vec3& closest, float w[4])
{
    const gjk_vertex vs[3] = {s.v[0], s.v[1], s.v[2]};
    const vec3 a = vs[0].w;
    const vec3 b = vs[1].w;
    const vec3 c = vs[2].w;

    gjk_vertex keep[3];
    float keep_w[3] = {0.0f, 0.0f, 0.0f};
    int keep_n = 0;
    vec3 best{};
    float best_d2 = 0.0f;
    bool have = false;

    // ---- candidate 0: the interior ---------------------------------------
    //
    // The barycentric coordinates of the origin's projection onto the plane,
    // from SIGNED AREAS — 2.3's construction exactly, with the origin as the
    // query point so that `p − x` is just `−x` and the three cross products
    // collapse to `b × c`, `c × a`, `a × b`. Their sum is `n · n`, which is
    // the identity that makes the division a single reciprocal.
    const vec3 n = cross(b - a, c - a);
    const float n2 = length_squared(n);
    if (n2 > 0.0f)
    {
        const float la = dot(cross(b, c), n);
        const float lb = dot(cross(c, a), n);
        const float lc = dot(cross(a, b), n);
        if (la >= 0.0f && lb >= 0.0f && lc >= 0.0f)
        {
            const float inv = 1.0f / n2;
            best = a * (la * inv) + b * (lb * inv) + c * (lc * inv);
            best_d2 = length_squared(best);
            have = true;
            keep_n = 3;
            keep[0] = vs[0]; keep[1] = vs[1]; keep[2] = vs[2];
            keep_w[0] = la * inv; keep_w[1] = lb * inv; keep_w[2] = lc * inv;
        }
    }

    // ---- candidates 1-3: the three clamped edges -------------------------
    static constexpr int k_edges[3][2] = {{0, 1}, {0, 2}, {1, 2}};
    for (int k = 0; k < 3; ++k)
    {
        const vec3 p0 = vs[k_edges[k][0]].w;
        const vec3 p1 = vs[k_edges[k][1]].w;
        const vec3 d = p1 - p0;
        const float dd = length_squared(d);

        float t = (dd > 0.0f) ? dot(-p0, d) / dd : 0.0f;
        t = std::fmin(1.0f, std::fmax(0.0f, t));

        const vec3 p = p0 + d * t;
        const float d2 = length_squared(p);
        if (have && d2 >= best_d2) { continue; }

        best = p;
        best_d2 = d2;
        have = true;
        keep_n = 2;
        keep[0] = vs[k_edges[k][0]];
        keep[1] = vs[k_edges[k][1]];
        keep_w[0] = 1.0f - t;
        keep_w[1] = t;
    }

    // ---- write back, dropping any vertex that contributes nothing --------
    //
    // A weight of exactly zero means the closest point does not use that vertex
    // at all, so keeping it would carry a useless point into the next iteration
    // — where it would occupy one of only four slots and make the next triangle
    // thinner than it needs to be.
    s.count = 0;
    for (int i = 0; i < 4; ++i) { w[i] = 0.0f; }
    for (int i = 0; i < keep_n; ++i)
    {
        if (keep_w[i] == 0.0f) { continue; }
        w[s.count] = keep_w[i];
        s.v[s.count] = keep[i];
        ++s.count;
    }
    if (s.count == 0)   // every weight zero: only possible if the point is a vertex
    {
        s.v[0] = keep[0];
        s.count = 1;
        w[0] = 1.0f;
    }
    closest = best;
}

/// Closest point on the tetrahedron to the origin. Returns `true` if it is
/// inside, in which case the simplex is left whole and the closest point is the
/// origin itself.
bool reduce_tetrahedron(simplex& s, vec3& closest, float w[4])
{
    static constexpr int k_faces[4][3] = {{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}};
    static constexpr int k_opposite[4] = {3, 2, 1, 0};

    const vec3 origin{};

    simplex best{};
    float best_w[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    vec3 best_point{};
    float best_d2 = 0.0f;
    bool any_outside = false;

    for (int f = 0; f < 4; ++f)
    {
        const int i = k_faces[f][0];
        const int j = k_faces[f][1];
        const int k = k_faces[f][2];

        if (!outside_plane(origin, s.v[i].w, s.v[j].w, s.v[k].w, s.v[k_opposite[f]].w))
        {
            continue;
        }

        simplex face;
        face.v[0] = s.v[i];
        face.v[1] = s.v[j];
        face.v[2] = s.v[k];
        face.count = 3;

        float fw[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        vec3 q{};
        reduce_triangle(face, q, fw);

        const float d2 = length_squared(q);
        if (!any_outside || d2 < best_d2)
        {
            any_outside = true;
            best = face;
            best_point = q;
            best_d2 = d2;
            for (int n = 0; n < 4; ++n) { best_w[n] = fw[n]; }
        }
    }

    if (!any_outside)
    {
        // Inside every one of the four half-spaces: the origin is enclosed, the
        // distance is zero, and this tetrahedron is what 8.6's EPA expands.
        closest = origin;
        for (int n = 0; n < 4; ++n) { w[n] = 0.25f; }
        return true;
    }

    s = best;
    closest = best_point;
    for (int n = 0; n < 4; ++n) { w[n] = best_w[n]; }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// The public pieces
// ---------------------------------------------------------------------------

const char* name_of(gjk_status status)
{
    switch (status)
    {
        case gjk_status::separated:       return "separated";
        case gjk_status::intersecting:    return "intersecting";
        case gjk_status::iteration_limit: return "iteration limit";
    }
    return "?";
}

gjk_vertex cso_support(const convex& a, const convex& b, vec3 delta, vec3 d)
{
    gjk_vertex out;
    out.pa = a.support(a.data, d);
    out.pb = b.support(b.data, -d);
    // The Minkowski-difference identity, and the one place `delta` is used. Both
    // supports are shape-sized, `delta` was formed once by the caller, and no
    // world-sized number appears in this subtraction.
    out.w = out.pa - out.pb - delta;
    return out;
}

bool reduce_simplex(simplex& s, vec3& closest, float weights[4])
{
    for (int i = 0; i < 4; ++i) { weights[i] = 0.0f; }

    switch (s.count)
    {
        case 1:
            closest = s.v[0].w;
            weights[0] = 1.0f;
            return false;
        case 2:
            reduce_segment(s, closest, weights);
            return false;
        case 3:
            reduce_triangle(s, closest, weights);
            return false;
        case 4:
            return reduce_tetrahedron(s, closest, weights);
        default:
            ENGINE_ASSERT(false && "reduce_simplex: empty simplex");
            closest = vec3{};
            return false;
    }
}

// ---------------------------------------------------------------------------
// The search
// ---------------------------------------------------------------------------

gjk_result gjk_distance(const convex& a, const convex& b, const gjk_config& cfg)
{
    gjk_result r;

    // THE ONE LARGE SUBTRACTION, taken once. Everything after this line works on
    // differences, which is 8.4 §11's finding turned into a line of code.
    const vec3 delta = b.origin - a.origin;

    // THE SEED, AND ITS SIGN IS NOT ARBITRARY. `A ⊖ B` is centred near `−delta`,
    // so the direction from the difference set TOWARD THE ORIGIN is `+delta` and
    // the support point that way is already close to the answer. Seeding with
    // `−delta` instead picks the point of the difference FARTHEST from the
    // origin, which is a perfectly legal start and costs one whole iteration on
    // every query — measured at 4.52 passes against 3.53 over 117,413 box pairs,
    // and at two against one for spheres, where the second point IS the answer.
    vec3 dir = cfg.initial_direction;
    if (length_squared(dir) <= 0.0f) { dir = delta; }
    if (length_squared(dir) <= 0.0f) { dir = vec3{1.0f, 0.0f, 0.0f}; }

    simplex s;
    gjk_vertex first = cso_support(a, b, delta, dir);
    r.support_calls += 2;
    s.push(first);

    vec3 v = first.w;
    float weights[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    // The scale the tolerances are relative to: the largest CSO vertex seen. It
    // is on the order of the two shapes' sizes plus their separation, which is
    // the only length this query has any business comparing against.
    float scale2 = length_squared(first.w);

    // THE BEST LOWER BOUND PROVEN SO FAR, squared. Kept because it is not an
    // estimate — it is a THEOREM the loop has already earned, and §E's whole
    // instrument is built on it.
    //
    // `w` is the support in direction `−v`, so it MINIMISES `dot(x, v)` over the
    // whole difference set: every point of `A ⊖ B` satisfies `dot(x, v) ≥
    // dot(w, v)`. If that number is positive then no point of the set has
    // `dot(x, v) = 0`, and the origin is one such point. A single positive
    // `dot(v, w)` is therefore a complete proof that the shapes are APART, and
    // no later containment test may overrule it. Below, one does try.
    float lower2 = 0.0f;

    const float tol = cfg.tolerance;

    for (int it = 0; it < cfg.max_iterations; ++it)
    {
        r.iterations = it + 1;

        const float v2 = length_squared(v);

        // The origin is (numerically) on the simplex: the shapes touch, within
        // the contact margin `tol * |w_max|` that §F.6 measures.
        if (v2 <= tol * tol * scale2 && lower2 <= tol * tol * scale2)
        {
            r.status = gjk_status::intersecting;
            r.terminal = s;
            return r;
        }

        const gjk_vertex w = cso_support(a, b, delta, -v);
        r.support_calls += 2;

        // THE TERMINATION TEST, AND IT IS THE WHOLE ALGORITHM'S HONESTY.
        //
        //   |v|                 is an UPPER bound: v is a real point of A ⊖ B.
        //   dot(v, w) / |v|     is a LOWER bound: the supporting plane with
        //                       normal v/|v| through w has all of A ⊖ B beyond
        //                       it, so nothing in the set is nearer than that.
        //
        // Their difference is (|v|² − dot(v,w)) / |v|. Stop when it is below
        // `tol * |v|`, i.e. when the answer is proven to a RELATIVE tolerance —
        // multiply the scene by a thousand and this test says exactly the same
        // thing, which an absolute one does not.
        const float vw = dot(v, w.w);
        if (v2 - vw <= tol * v2)
        {
            break;
        }

        // Bank the proof. `vw / |v|` is the distance from the origin to the
        // supporting plane; squaring keeps a divide instead of a square root in
        // the hot loop, and the sign is carried by the `vw > 0` test.
        if (vw > 0.0f)
        {
            const float lb2 = (vw * vw) / v2;
            if (lb2 > lower2) { lower2 = lb2; }
        }

        // NO PROGRESS. The support function returned a vertex the simplex
        // already has, so the next iteration would compute the same thing
        // forever. On a polytope this is the normal way to finish exactly; on a
        // curved shape it is float arithmetic running out of room.
        bool duplicate = false;
        for (int i = 0; i < s.count; ++i)
        {
            if (length_squared(s.v[i].w - w.w) <= k_duplicate_rel2 * scale2) { duplicate = true; }
        }
        if (duplicate) { break; }

        // The state to fall back to. `s`, `v` and `weights` agree with each
        // other right now, and the next two lines may destroy that agreement.
        const simplex previous = s;
        const vec3 previous_v = v;
        float previous_weights[4];
        for (int i = 0; i < 4; ++i) { previous_weights[i] = weights[i]; }

        s.push(w);
        const float w2 = length_squared(w.w);
        if (w2 > scale2) { scale2 = w2; }

        if (reduce_simplex(s, v, weights))
        {
            // THE CONTAINMENT TEST LOSES TO THE PROOF, and this is the second
            // place a decision is refused in favour of evidence already in hand.
            //
            // `reduce_simplex` decides containment from the SIGN of a signed
            // volume, and by the last iteration GJK's tetrahedra are as thin as
            // the problem allows — the degenerate case is where the algorithm
            // spends its time, not an exotic corner. Measured on a sphere and a
            // turned box 55 mm apart: fourteen clean iterations converging to
            // 0.0553297 m, and then a tetrahedron whose four face tests all
            // reported "the origin is on the inside", turning a 55 mm gap into a
            // contact.
            //
            // But by then the loop had ALREADY PROVEN a lower bound of 0.0553,
            // and a proof does not become false because a later determinant
            // changed sign. So: if a meaningful lower bound exists, the
            // containment is rejected, the last good state is restored, and the
            // result is reported as separated-but-unproven.
            if (lower2 > tol * tol * scale2)
            {
                s = previous;
                v = previous_v;
                for (int i = 0; i < 4; ++i) { weights[i] = previous_weights[i]; }
                r.stalled = true;
                break;
            }

            r.status = gjk_status::intersecting;
            r.terminal = s;
            return r;
        }

        // NO PROGRESS, AND THIS TEST IS A THEOREM RATHER THAN A HEURISTIC. The
        // new simplex CONTAINS the old reduced one, so its closest point to the
        // origin cannot be farther away: |v| is monotonically non-increasing for
        // the whole search, by construction. A step that fails to decrease it
        // therefore cannot be the algorithm converging slowly.
        //
        // IT CATCHES TWO DIFFERENT FAILURES, and the second one is why this
        // block restores state instead of merely breaking.
        //
        //   * THE LOOP CYCLING, on ordinary input rather than adversarial: two
        //     3 m boxes 2 mm apart reached the iteration cap with the distance
        //     already correct to six digits, because the termination test's
        //     threshold falls as the gap SQUARED while its rounding error falls
        //     only as the gap. 8.5 §F.5 measures that floor.
        //
        //   * THE SIMPLEX SOLVER GOING BACKWARDS. On a very thin triangle the
        //     signed areas `va`, `vb`, `vc` in `reduce_triangle` are differences
        //     of nearly equal products, so the Voronoi region they select can be
        //     the wrong one and the "closest" point lands somewhere else
        //     entirely. Measured on a sphere against a turned box: eighteen
        //     iterations converging cleanly to 0.0233827 m against a true
        //     0.0233828, and then one reduction returning **0.0674522** — three
        //     times too far, from a triangle two of whose vertices agreed to six
        //     digits. Without the restore this function would return that.
        //
        // An invariant you can CHECK is worth more than one you believe, and it
        // costs one comparison and a 160-byte copy against two support calls.
        const float improved = length_squared(v);
        if (improved >= v2)
        {
            s = previous;
            v = previous_v;
            for (int i = 0; i < 4; ++i) { weights[i] = previous_weights[i]; }
            r.stalled = true;
            break;
        }

        if (it + 1 == cfg.max_iterations)
        {
            r.status = gjk_status::iteration_limit;
            r.terminal = s;
            r.distance = std::sqrt(length_squared(v));
            return r;
        }
    }

    // Separated. The barycentric weights of `v` on the reduced simplex are also
    // the weights of the witness points on their own shapes — same combination,
    // three different sets of vertices — which is the entire reason `gjk_vertex`
    // carries `pa` and `pb` through the loop.
    vec3 pa{};
    vec3 pb{};
    for (int i = 0; i < s.count; ++i)
    {
        pa += s.v[i].pa * weights[i];
        pb += s.v[i].pb * weights[i];
    }

    r.status = gjk_status::separated;
    r.terminal = s;
    r.point_a = a.origin + pa;
    r.point_b = b.origin + pb;

    const float d2 = length_squared(v);
    r.distance = std::sqrt(d2);
    // `v` runs from B's witness to A's, so the axis FROM a TOWARD b is its
    // negative — conventions.html §9e, the same direction 8.4's `separation`
    // carries, so the two tests can be swapped without a sign hunt.
    r.direction = (d2 > 0.0f) ? -v * (1.0f / r.distance) : vec3{};
    return r;
}

bool gjk_intersects(const convex& a, const convex& b, const gjk_config& cfg)
{
    const vec3 delta = b.origin - a.origin;

    vec3 dir = cfg.initial_direction;
    if (length_squared(dir) <= 0.0f) { dir = delta; }     // toward the origin
    if (length_squared(dir) <= 0.0f) { dir = vec3{1.0f, 0.0f, 0.0f}; }

    simplex s;
    gjk_vertex first = cso_support(a, b, delta, dir);
    s.push(first);
    vec3 v = first.w;
    float scale2 = length_squared(first.w);
    float weights[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    const float tol = cfg.tolerance;

    for (int it = 0; it < cfg.max_iterations; ++it)
    {
        const float v2 = length_squared(v);
        if (v2 <= tol * tol * scale2) { return true; }

        const gjk_vertex w = cso_support(a, b, delta, -v);

        // THE CHEAP TEST, AND THE ONLY DIFFERENCE FROM `gjk_distance`. The
        // supporting plane with outward normal −v passes through w. If w does
        // not even reach the origin's side of it — dot(w, −v) < 0 — then no
        // point of A ⊖ B does, so the origin is outside and the answer is no.
        // This is a PROOF of separation and it arrives with no square root, no
        // refinement and usually in one or two passes. What it does not give you
        // is a number, which is exactly what `gjk_distance` spends its extra
        // iterations buying.
        if (dot(w.w, -v) < 0.0f) { return false; }

        bool duplicate = false;
        for (int i = 0; i < s.count; ++i)
        {
            if (length_squared(s.v[i].w - w.w) <= k_duplicate_rel2 * scale2) { duplicate = true; }
        }
        if (duplicate) { return false; }

        s.push(w);
        const float w2 = length_squared(w.w);
        if (w2 > scale2) { scale2 = w2; }

        const float before = v2;
        if (reduce_simplex(s, v, weights)) { return true; }

        // The same monotonicity argument as `gjk_distance`. Here a stall means
        // the origin is as close to the simplex as float can show and no
        // supporting plane put it outside, so the honest answer is "touching".
        if (length_squared(v) >= before) { return true; }
    }

    // Ran out of iterations without proving separation: the bounds never came
    // apart, so the shapes are at worst a tolerance from touching. Say yes,
    // because a false overlap costs one narrow-phase call and a false gap costs
    // an object through the floor — 8.4's `overlaps` made the same trade.
    return true;
}

// ---------------------------------------------------------------------------
// The certificate
// ---------------------------------------------------------------------------

gjk_bounds certify(const convex& a, const convex& b, const gjk_result& r)
{
    gjk_bounds out;

    // Upper: two points on two surfaces. Computed from the RESULT alone, the way
    // a caller holding only `gjk_result` would compute it.
    out.upper = length(r.point_b - r.point_a);

    const vec3 n = r.direction;
    if (length_squared(n) <= 0.0f) { return out; }

    // Lower: the width of the slab between the two supporting planes with normal
    // `n`. A's farthest point along +n and B's farthest along −n bound the
    // shapes on the inside faces of the gap, so their separation along n cannot
    // exceed the true distance.
    const vec3 delta = b.origin - a.origin;
    const vec3 sa = a.support(a.data, n);
    const vec3 sb = b.support(b.data, -n);
    out.lower = dot(delta + sb - sa, n);
    return out;
}

} // namespace engine::phys
