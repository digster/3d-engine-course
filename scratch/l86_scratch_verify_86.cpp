// scratch/verify_86.cpp — every number Lesson 8.6 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_86.sh
//
// Nine sections, in the lesson's order:
//
//   A  the placeholder, and what a guessed normal costs
//   B  the minimum translation, verified by performing it
//   C  depth is a minimum over directions
//   D  the seed: what GJK actually hands over
//   E  containment, and which triangle to keep
//   F  the polytope stays a polytope
//   G  polytopes terminate, curves do not
//   H  accuracy, and where it gives out
//   I  the budget
//
// EVERY SECTION CARRIES A CONTROL — 8.1 through 8.5's rule, in two halves: ask
// what the control would say if the thing were COMPLETELY BROKEN, and what it
// would say if the thing were completely FINE.
//
// THE INSTRUMENT, AND WHY IT IS NOT A SECOND EPA. A penetration depth certifies
// itself from the output alone, because the definition is a minimum over
// directions:
//
//     depth = min over unit n of  h(n),   h(n) = support_{A⊖B}(n)
//
// so for the normal EPA returns, `h(normal)` is computable in two support calls
// and must EQUAL the reported depth — that is the achievability half, and a
// claimed depth larger than it is provably wrong. The other half is minimality,
// and it is the hard one: no reachable certificate proves that no other
// direction does better. Two instruments are used instead, and they fail in
// opposite ways:
//
//   * A FALSIFIER. Sample thousands of directions and take the smallest `h`.
//     Being a minimum over a subset, it is always an UPPER bound on the truth,
//     so it can convict EPA of being too large and can never acquit it.
//   * AN EXACT REFERENCE, for boxes only. The minimum of `h` over all directions
//     is attained on a FACE NORMAL of `A ⊖ B`, and for two boxes those are
//     exactly 8.4's fifteen candidate axes, in both signs. So the SAT's MTV is
//     the true penetration depth for boxes — which makes 8.4, written before any
//     of this existed, the reference 8.6 is checked against.
//
// PRECISION. The engine collides in `float`, so this harness does too.
// References and certificates are evaluated in `double`, the only honest way
// round: a reference computed at the same precision as the thing it checks
// cannot tell you which of the two is wrong.
//
// OUTPUT WIDTH IS A CONSTRAINT, not a preference. The lesson quotes this
// program's transcript in <pre> blocks that scroll and never wrap, and the fold
// is at about 66 characters. Nothing below exceeds 66.

#include <engine/core/bench.hpp>
#include <engine/math/bounds.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/collide.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/epa.hpp>
#include <engine/phys/gjk.hpp>
#include <engine/phys/shape.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

using engine::aabb;
using engine::mat3;
using engine::quat;
using engine::quat_from_axis_angle;
using engine::vec3;
using engine::bench_ab;
using engine::bench_compare;
using engine::bench_result;
using engine::bench_run;
using engine::phys::as_convex;
using engine::phys::box_shape;
using engine::phys::capsule;
using engine::phys::capsule_shape;
using engine::phys::collide;
using engine::phys::convex;
using engine::phys::cso_support;
using engine::phys::depth_along;
using engine::phys::epa_config;
using engine::phys::epa_penetration;
using engine::phys::epa_result;
using engine::phys::epa_status;
using engine::phys::gjk_config;
using engine::phys::gjk_distance;
using engine::phys::gjk_intersects;
using engine::phys::gjk_result;
using engine::phys::gjk_status;
using engine::phys::hull;
using engine::phys::k_epa_max_faces;
using engine::phys::k_epa_max_vertices;
using engine::phys::name_of;
using engine::phys::obb;
using engine::phys::separation;
using engine::phys::shape;
using engine::phys::simplex;
using engine::phys::sphere_shape;
using engine::phys::support;
using engine::phys::support_local;
using engine::phys::world_capsule;
using engine::phys::world_hull;
using engine::phys::world_obb;
using engine::phys::world_sphere;

// ---------------------------------------------------------------------------
// Allocation counting, for §I
// ---------------------------------------------------------------------------
//
// 6.17 §9 made the same measurement the same way, and it is the only way to make
// "this allocates nothing" a fact rather than a claim: replace the global
// operator and count. A narrow phase that allocates once per pair takes a lock
// and a cache miss per pair, and neither shows up in a profile as itself.

namespace
{
std::size_t g_allocs = 0;
}

void* operator new(std::size_t n)
{
    ++g_allocs;
    void* p = std::malloc(n ? n : 1);
    if (!p) { throw std::bad_alloc(); }
    return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace
{

constexpr double k_pi = 3.14159265358979323846;

void rule(const char* title)
{
    std::printf("\n%s\n", title);
    for (int i = 0; i < 66; ++i) { std::putchar('-'); }
    std::putchar('\n');
}

/// The same deterministic generator 8.1-8.5 used, for the same reason:
/// `std::uniform_real_distribution` is not specified to produce the same
/// sequence on two standard libraries, and a harness whose numbers move when you
/// change compiler is a harness the lesson cannot quote.
class rng
{
public:
    explicit rng(std::uint32_t seed) : state_(seed | 1u) {}

    std::uint32_t next()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    float signed_unit() { return unit() * 2.0f - 1.0f; }
    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }

    vec3 direction()
    {
        for (int guard = 0; guard < 64; ++guard)
        {
            const vec3 v{signed_unit(), signed_unit(), signed_unit()};
            const float len2 = length_squared(v);
            if (len2 > 1e-4f && len2 <= 1.0f) { return v / std::sqrt(len2); }
        }
        return vec3{0.0f, 1.0f, 0.0f};
    }

    quat rotation()
    {
        return quat_from_axis_angle(direction(), range(0.0f, 2.0f * 3.14159265f));
    }

    quat yaw() { return quat_from_axis_angle(vec3{0, 1, 0}, range(0.0f, 2.0f * 3.14159265f)); }

private:
    std::uint32_t state_;
};

// ---------------------------------------------------------------------------
// Double-precision arithmetic, for references
// ---------------------------------------------------------------------------

struct dvec3
{
    double x = 0.0, y = 0.0, z = 0.0;
};

dvec3 promote(vec3 v)
{
    return dvec3{static_cast<double>(v.x), static_cast<double>(v.y),
                 static_cast<double>(v.z)};
}

double ddot(dvec3 a, dvec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
dvec3 dmul(dvec3 a, double s) { return dvec3{a.x * s, a.y * s, a.z * s}; }
double dlen(dvec3 a) { return std::sqrt(ddot(a, a)); }
dvec3 dcross(dvec3 a, dvec3 b)
{
    return dvec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

/// `h_{A⊖B}(n)` for two boxes, in double, from float box data.
///
/// The support of a box is `c·n + Σ hᵢ|uᵢ·n|`, which is 8.4 §6's projected
/// radius — so the whole quantity is the two shadows' overlap along `n`, and
/// nothing is subtracted that was not already going to be.
double box_depth_along_double(const obb& a, const obb& b, dvec3 n)
{
    const dvec3 ca = promote(a.centre);
    const dvec3 cb = promote(b.centre);
    double ra = 0.0;
    double rb = 0.0;
    for (int i = 0; i < 3; ++i)
    {
        ra += std::fabs(ddot(promote(a.axis(i)), n))
              * static_cast<double>(i == 0 ? a.half_extents.x
                                           : (i == 1 ? a.half_extents.y : a.half_extents.z));
        rb += std::fabs(ddot(promote(b.axis(i)), n))
              * static_cast<double>(i == 0 ? b.half_extents.x
                                           : (i == 1 ? b.half_extents.y : b.half_extents.z));
    }
    // max over A of dot(x, n) minus min over B of dot(y, n).
    return (ddot(ca, n) + ra) - (ddot(cb, n) - rb);
}

/// **The exact penetration depth of two boxes, and the direction of it.**
///
/// THE REFERENCE, AND THE REASON IT IS EXACT RATHER THAN MERELY GOOD. The
/// minimum of `h` over all unit directions is attained on a face normal of the
/// Minkowski difference `A ⊖ B`, because `h` is piecewise linear in `n` over the
/// normal fan of that polytope and a minimum of a concave-on-each-cone function
/// sits at a cone's edge. The faces of a difference of two boxes are: the faces
/// of A, the faces of B, and one face per pair of edge directions. Their normals
/// are `±aᵢ`, `±bⱼ` and `±(aᵢ × bⱼ)` — **8.4's fifteen candidate axes, in both
/// signs**, and nothing else.
///
/// So the Separating Axis Theorem's minimum-overlap axis IS the true minimum
/// translation for boxes. 8.4 shipped that a lesson before EPA existed, and it
/// is now the thing EPA is checked against.
struct exact_mtv
{
    double depth = 0.0;
    dvec3 normal{};
};

exact_mtv box_mtv_double(const obb& a, const obb& b)
{
    dvec3 axes[15];
    int n = 0;
    for (int i = 0; i < 3; ++i) { axes[n++] = promote(a.axis(i)); }
    for (int j = 0; j < 3; ++j) { axes[n++] = promote(b.axis(j)); }
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j) { axes[n++] = dcross(promote(a.axis(i)), promote(b.axis(j))); }
    }

    exact_mtv best;
    best.depth = 1e300;
    for (int i = 0; i < n; ++i)
    {
        const double len = dlen(axes[i]);
        if (len <= 1e-9) { continue; }   // a parallel edge pair spans no face
        const dvec3 u = dmul(axes[i], 1.0 / len);
        for (int s = 0; s < 2; ++s)
        {
            const dvec3 d = (s == 0) ? u : dmul(u, -1.0);
            const double h = box_depth_along_double(a, b, d);
            if (h < best.depth) { best.depth = h; best.normal = d; }
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

enum class kind
{
    box,
    ball,
    cap,
    poly,
};

/// A shape placed in the world, with stable storage for a hull's vertices.
///
/// NOT COPYABLE IN PRACTICE: `hull::points` is a span into `pts`, so a copy
/// would leave the copy's hull pointing at the original's vector. Every
/// generator below fills one of these through a reference for that reason, and
/// `refresh()` re-seats the span whenever the storage moves.
struct placed
{
    kind k = kind::box;
    obb b{};
    engine::sphere s{};
    capsule c{};
    hull h{};
    std::vector<vec3> pts;

    void refresh() { h.points = std::span<const vec3>(pts.data(), pts.size()); }

    [[nodiscard]] convex view() const
    {
        switch (k)
        {
            case kind::box:  return as_convex(b);
            case kind::ball: return as_convex(s);
            case kind::cap:  return as_convex(c);
            case kind::poly: return as_convex(h);
        }
        return as_convex(b);
    }

    [[nodiscard]] float size() const
    {
        switch (k)
        {
            case kind::box: return length(b.half_extents);
            case kind::ball: return s.radius;
            case kind::cap: return c.half_height + c.radius;
            case kind::poly: return 1.0f;
        }
        return 1.0f;
    }
};

void make_box(placed& p, vec3 centre, vec3 half, quat q)
{
    p.k = kind::box;
    p.b = world_obb(box_shape(half), centre, q);
}

void make_ball(placed& p, vec3 centre, float radius)
{
    p.k = kind::ball;
    p.s = world_sphere(sphere_shape(radius), centre);
}

void make_capsule(placed& p, vec3 centre, float radius, float half_height, quat q)
{
    p.k = kind::cap;
    p.c = world_capsule(capsule_shape(radius, half_height), centre, q);
}

void make_poly(placed& p, rng& r, vec3 centre, int n, float radius, quat q)
{
    p.k = kind::poly;
    p.pts.clear();
    for (int i = 0; i < n; ++i) { p.pts.push_back(r.direction() * radius); }
    p.h = world_hull(std::span<const vec3>(p.pts.data(), p.pts.size()), centre, q);
    p.refresh();
}

/// The naive `convex`: support in WORLD space, origin at nothing.
///
/// §H.3's second arm, and note it is not a second implementation — it is the
/// SAME epa.cpp handed a differently-shaped view, so `delta` is zero and every
/// `w = pa − pb` subtracts two world-sized numbers on every iteration. That is
/// exactly the formulation `convex.hpp` exists to avoid, and making it an
/// adapter rather than a fork is what makes the comparison mean something.
convex as_convex_world(const obb& box)
{
    return convex{
        +[](const void* d, vec3 dir) { return support(*static_cast<const obb*>(d), dir); },
        &box,
        vec3{},
    };
}

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failures; std::printf("  *** FAILED: %s\n", what); }
}

/// GJK then EPA, the way `collide(convex, convex)` does it.
struct query
{
    gjk_result g;
    epa_result e;
};

query penetrate(const convex& a, const convex& b, const epa_config& cfg = {})
{
    query q;
    q.g = gjk_distance(a, b);
    q.e = epa_penetration(a, b, q.g.terminal, cfg);
    return q;
}

/// A percentile of a sorted-in-place sample.
double pct(std::vector<double>& v, double p)
{
    if (v.empty()) { return 0.0; }
    std::sort(v.begin(), v.end());
    const std::size_t i = static_cast<std::size_t>(p * static_cast<double>(v.size() - 1) + 0.5);
    return v[i];
}

double mean_of(const std::vector<double>& v)
{
    if (v.empty()) { return 0.0; }
    double s = 0.0;
    for (double x : v) { s += x; }
    return s / static_cast<double>(v.size());
}

// ===========================================================================
// A. The placeholder, and what a guessed normal costs
// ===========================================================================

void section_a()
{
    rule("A. WHAT 8.5 LEFT BEHIND, AND WHAT GUESSING COSTS");

    // ---- A.1 one pair, worked through ------------------------------------
    placed a;
    placed b;
    make_box(a, vec3{0, 0, 0}, vec3{0.5f, 0.5f, 0.5f}, quat{});
    make_box(b, vec3{0.60f, 0.15f, 0.00f}, vec3{0.5f, 0.5f, 0.5f}, quat{});

    const convex ca = a.view();
    const convex cb = b.view();

    const gjk_result g = gjk_distance(ca, cb);
    const epa_result e = epa_penetration(ca, cb, g.terminal);
    const separation sat = collide(a.b, b.b);
    const exact_mtv ref = box_mtv_double(a.b, b.b);

    std::printf("A.1  two unit cubes, centres 0.60 m apart in x, 0.15 in y\n\n");
    std::printf("     GJK      %-14s distance %.6f\n", name_of(g.status), g.distance);
    std::printf("              direction (%+.4f %+.4f %+.4f)\n",
                static_cast<double>(g.direction.x), static_cast<double>(g.direction.y),
                static_cast<double>(g.direction.z));
    std::printf("     EPA      %-14s depth    %.6f\n", name_of(e.status),
                static_cast<double>(e.depth));
    std::printf("              normal    (%+.4f %+.4f %+.4f)\n",
                static_cast<double>(e.normal.x), static_cast<double>(e.normal.y),
                static_cast<double>(e.normal.z));
    std::printf("     SAT MTV  depth %.6f  axis (%+.4f %+.4f %+.4f)\n",
                static_cast<double>(sat.depth), static_cast<double>(sat.axis.x),
                static_cast<double>(sat.axis.y), static_cast<double>(sat.axis.z));
    std::printf("     exact    depth %.6f  (double, over 30 signed axes)\n", ref.depth);
    std::printf("     bracket  lower %.6f  upper %.6f  slack %.2e\n",
                static_cast<double>(e.lower), static_cast<double>(e.upper),
                static_cast<double>(e.slack()));
    std::printf("     seed     simplex %d pts, %d found, %d iterations\n",
                g.terminal.count, e.seed_vertices, e.iterations);

    check(std::fabs(static_cast<double>(e.depth) - ref.depth) < 1e-4, "A.1 depth vs exact");
    check(std::fabs(static_cast<double>(sat.depth) - ref.depth) < 1e-5, "A.1 SAT vs exact");

    // ---- A.2 the population ----------------------------------------------
    //
    // 8.5 shipped GJK's last search direction as the axis when two shapes
    // overlapped, and its doc comment called it "a reasonable guess at a contact
    // normal and nothing more". THE COMMENT WAS WRONG, and this counts how
    // wrong: `gjk_result::direction` is only written on the SEPARATED path, so
    // on an overlap it is the zero vector every time. What 8.5 actually shipped
    // was not a poor normal but no normal at all.
    //
    // So the guess measured here is the one a naive implementation reaches for
    // instead: centre to centre.
    rng r(0x8601u);
    std::vector<double> angles;
    std::vector<double> ref_angles;
    std::vector<double> depth_err;
    std::vector<double> guess_excess;
    int pairs = 0;
    int zero_direction = 0;
    const int wanted = 200000;

    for (int guard = 0; guard < 40 * wanted && pairs < wanted; ++guard)
    {
        placed pa;
        placed pb;
        const vec3 ha{r.range(0.3f, 1.2f), r.range(0.3f, 1.2f), r.range(0.3f, 1.2f)};
        const vec3 hb{r.range(0.3f, 1.2f), r.range(0.3f, 1.2f), r.range(0.3f, 1.2f)};
        make_box(pa, vec3{}, ha, r.rotation());
        make_box(pb, r.direction() * r.range(0.0f, 1.4f), hb, r.rotation());

        const convex va = pa.view();
        const convex vb = pb.view();
        const gjk_result gg = gjk_distance(va, vb);
        if (gg.status == gjk_status::separated) { continue; }

        const epa_result ee = epa_penetration(va, vb, gg.terminal);
        if (ee.status != epa_status::proven) { continue; }
        ++pairs;
        if (length_squared(gg.direction) == 0.0f) { ++zero_direction; }

        const exact_mtv x = box_mtv_double(pa.b, pb.b);
        depth_err.push_back(std::fabs(static_cast<double>(ee.depth) - x.depth));

        const double ca_dot = std::fmin(1.0, std::fmax(-1.0, ddot(promote(ee.normal), x.normal)));
        ref_angles.push_back(std::acos(ca_dot) * 180.0 / k_pi);

        const vec3 guess = normalised_or(pb.b.centre - pa.b.centre, vec3{0, 1, 0});
        const double gd =
            std::fmin(1.0, std::fmax(-1.0, ddot(promote(guess), promote(ee.normal))));
        angles.push_back(std::acos(gd) * 180.0 / k_pi);
        // What a solver would push by if it trusted the guess: the translation
        // along the guessed direction that actually separates.
        const double along = static_cast<double>(depth_along(va, vb, guess));
        guess_excess.push_back(along / std::fmax(1e-9, static_cast<double>(ee.depth)));
    }

    std::printf("\nA.2  %d overlapping box pairs, random orientations\n\n", pairs);
    std::printf("     8.5's axis (gjk_result::direction) was zero:\n");
    std::printf("       %d / %d  = %.1f%%\n", zero_direction, pairs,
                100.0 * zero_direction / std::fmax(1.0, static_cast<double>(pairs)));
    std::printf("\n     centre-to-centre as a contact normal, vs the truth\n");
    std::printf("       mean %6.2f deg   p50 %6.2f   p95 %6.2f   max %6.2f\n",
                mean_of(angles), pct(angles, 0.50), pct(angles, 0.95), pct(angles, 1.0));
    std::printf("       push distance it implies, as a multiple of the MTV\n");
    std::printf("       mean %6.2fx       p50 %6.2fx  p95 %6.1fx  max %7.0fx\n",
                mean_of(guess_excess), pct(guess_excess, 0.50), pct(guess_excess, 0.95),
                pct(guess_excess, 1.0));
    std::printf("\n     CONTROL — EPA's normal vs the same exact reference\n");
    std::printf("       mean %6.4f deg   p95 %6.4f   p99.9 %6.4f\n", mean_of(ref_angles),
                pct(ref_angles, 0.95), pct(ref_angles, 0.999));
    std::printf("       depth error: mean %.3e  max %.3e m\n", mean_of(depth_err),
                pct(depth_err, 1.0));

    check(zero_direction == pairs, "A.2 8.5's axis was always zero on an overlap");
    check(pct(ref_angles, 0.999) < 2.0, "A.2 EPA normal agrees with the exact MTV");
    check(pct(depth_err, 1.0) < 2e-3, "A.2 EPA depth agrees with the exact MTV");
    check(mean_of(angles) > 5.0, "A.2 the guessed normal is measurably wrong");
}

// ===========================================================================
// B. The minimum translation, verified by performing it
// ===========================================================================

void section_b()
{
    rule("B. THE DEFINITION, CHECKED BY MOVING THE BOX");

    // depth is defined as the SHORTEST translation that separates. That is a
    // statement about what happens when you translate, so it is checked by
    // translating: push slightly further than the answer and they must come
    // apart; push slightly less and they must not.
    //
    // THE TOLERANCE HAS TO BE TIGHTER THAN THE TEST. A 1% nudge on a 1 mm depth
    // is 10 microns, and the default tolerance admits an error of
    // `tol * size ≈ 1.7e-4` m — which would make this section measure the
    // tolerance rather than the definition. So it runs at 1e-6.
    rng r(0x8602u);
    const float delta = 0.01f;   // 1% either side
    epa_config cfg;
    cfg.tolerance = 1e-6f;
    cfg.max_iterations = 64;

    // AND SO DOES GJK'S. 8.5 §F.6 measured its contact margin at
    // `2.11 * tolerance * size`, which at the default is 0.4 mm — wider than the
    // 1% nudge on a 1 cm depth, so the default GJK would report the separated
    // pair as touching and this section would measure ITS tolerance instead.
    gjk_config gcfg;
    gcfg.tolerance = 1e-6f;

    int pairs = 0;
    int over_separates = 0;
    int under_still_hits = 0;
    int other_dir_still_hits = 0;
    int other_dir_tests = 0;
    std::vector<double> gap_after;
    std::vector<double> other_ratio;

    for (int guard = 0; guard < 400000 && pairs < 50000; ++guard)
    {
        placed pa;
        placed pb;
        make_box(pa, vec3{}, vec3{r.range(0.3f, 1.0f), r.range(0.3f, 1.0f), r.range(0.3f, 1.0f)},
                 r.rotation());
        make_box(pb, r.direction() * r.range(0.0f, 1.2f),
                 vec3{r.range(0.3f, 1.0f), r.range(0.3f, 1.0f), r.range(0.3f, 1.0f)},
                 r.rotation());

        const convex va = pa.view();
        const convex vb = pb.view();
        const query q = penetrate(va, vb, cfg);
        if (q.g.status == gjk_status::separated || q.e.status != epa_status::proven) { continue; }
        if (q.e.depth < 1e-3f) { continue; }   // below the tolerance's own floor
        ++pairs;

        const vec3 n = q.e.normal;
        const float d = q.e.depth;

        placed moved = pb;
        moved.b.centre = pb.b.centre + n * (d * (1.0f + delta));
        const convex vm = moved.view();
        const gjk_result apart = gjk_distance(va, vm, gcfg);
        if (apart.status == gjk_status::separated)
        {
            ++over_separates;
            gap_after.push_back(static_cast<double>(apart.distance)
                                / static_cast<double>(d * delta));
        }

        placed shy = pb;
        shy.b.centre = pb.b.centre + n * (d * (1.0f - delta));
        const convex vs = shy.view();
        if (gjk_intersects(va, vs, gcfg)) { ++under_still_hits; }

        // CONTROL — the same distance, a different direction. `depth` is the
        // MINIMUM over directions, so the translation needed along any other
        // direction is at least as long, and travelling only `depth` that way
        // must leave the pair overlapping.
        const vec3 other = r.direction();
        if (dot(other, n) < 0.9f)
        {
            ++other_dir_tests;
            other_ratio.push_back(static_cast<double>(depth_along(va, vb, other))
                                  / static_cast<double>(d));
            placed side = pb;
            side.b.centre = pb.b.centre + other * (d * (1.0f + delta));
            const convex vo = side.view();
            if (gjk_intersects(va, vo, gcfg)) { ++other_dir_still_hits; }
        }
    }

    std::printf("B.1  %d overlapping pairs, moved by the reported MTV\n\n", pairs);
    std::printf("     pushed by 1.01 x depth -> separated   %6d / %d\n", over_separates, pairs);
    std::printf("       residual gap / expected: mean %.4f  p05 %.4f\n", mean_of(gap_after),
                pct(gap_after, 0.05));
    std::printf("     pushed by 0.99 x depth -> still hit   %6d / %d\n", under_still_hits, pairs);
    std::printf("\n     CONTROL — same distance, a different direction\n");
    std::printf("     pushed 1.01 x depth elsewhere -> hit  %6d / %d\n", other_dir_still_hits,
                other_dir_tests);
    std::printf("     the translation that direction really needs,\n");
    std::printf("     as a multiple of the MTV: min %.4f  mean %.2f\n", pct(other_ratio, 0.0),
                mean_of(other_ratio));

    check(over_separates == pairs, "B.1 pushing past the depth separates");
    check(under_still_hits == pairs, "B.1 pushing short of the depth does not");
    check(pct(other_ratio, 0.0) >= 0.999, "B.1 no other direction is shorter");
}

// ===========================================================================
// C. Depth is a minimum over directions
// ===========================================================================

void section_c()
{
    rule("C. A MINIMUM OVER DIRECTIONS, SAMPLED AND ENUMERATED");

    rng r(0x8603u);
    const int pairs = 2000;
    const int samples = 4096;

    int beaten = 0;
    double worst_margin = 0.0;
    std::vector<double> sampled_ratio;
    std::vector<double> exact_ratio;

    for (int i = 0; i < pairs;)
    {
        placed pa;
        placed pb;
        make_box(pa, vec3{}, vec3{r.range(0.4f, 1.0f), r.range(0.4f, 1.0f), r.range(0.4f, 1.0f)},
                 r.rotation());
        make_box(pb, r.direction() * r.range(0.0f, 1.0f),
                 vec3{r.range(0.4f, 1.0f), r.range(0.4f, 1.0f), r.range(0.4f, 1.0f)},
                 r.rotation());

        const convex va = pa.view();
        const convex vb = pb.view();
        const query q = penetrate(va, vb);
        if (q.g.status == gjk_status::separated || q.e.status != epa_status::proven) { continue; }
        ++i;

        const double d = static_cast<double>(q.e.depth);

        // The falsifier. Any sampled direction with a smaller `h` is a shorter
        // translation that separates, which would convict EPA outright.
        double best_sampled = 1e300;
        for (int s = 0; s < samples; ++s)
        {
            const vec3 n = r.direction();
            const double h = static_cast<double>(depth_along(va, vb, n));
            if (h < best_sampled) { best_sampled = h; }
        }
        if (best_sampled < d - 1e-4)
        {
            ++beaten;
            worst_margin = std::fmax(worst_margin, d - best_sampled);
        }
        sampled_ratio.push_back(best_sampled / std::fmax(1e-9, d));

        const exact_mtv x = box_mtv_double(pa.b, pb.b);
        exact_ratio.push_back(x.depth / std::fmax(1e-9, d));

        // The certificate: h along EPA's OWN normal must equal the depth.
        const double own = static_cast<double>(depth_along(va, vb, q.e.normal));
        check(std::fabs(own - d) < 1e-3, "C certificate: h(normal) == depth");
    }

    std::printf("C.1  %d pairs x %d sampled directions = %d h() calls\n\n", pairs, samples,
                pairs * samples);
    std::printf("     directions beating EPA by > 1e-4 m:  %d\n", beaten);
    std::printf("     worst margin:                        %.3e m\n", worst_margin);
    std::printf("\n     best sampled h / EPA depth (>= 1 if EPA is right)\n");
    std::printf("       mean %.5f   p05 %.5f   min %.5f\n", mean_of(sampled_ratio),
                pct(sampled_ratio, 0.05), pct(sampled_ratio, 0.0));
    std::printf("\n     CONTROL — the exact minimum over the 30 face normals\n");
    std::printf("       mean %.6f   p05 %.6f   min %.6f\n", mean_of(exact_ratio),
                pct(exact_ratio, 0.05), pct(exact_ratio, 0.0));

    check(beaten == 0, "C.1 no sampled direction beats EPA");
    check(pct(sampled_ratio, 0.0) >= 0.999, "C.1 sampling never goes below EPA");
    check(std::fabs(mean_of(exact_ratio) - 1.0) < 1e-3, "C.1 EPA matches the enumeration");
}

// ===========================================================================
// D. The seed: what GJK actually hands over
// ===========================================================================

struct seed_stats
{
    const char* name = "";
    int pairs = 0;
    int simplex_count[5] = {0, 0, 0, 0, 0};
    int tetra_with_volume = 0;
    int seed_added[4] = {0, 0, 0, 0};
    int touching = 0;
    int degenerate = 0;
    int proven = 0;
    int stalled = 0;
    int iter_limit = 0;
    int capacity = 0;
};

void gather_seeds(seed_stats& st, rng& r, int wanted, int fixture)
{
    for (int guard = 0; guard < 60 * wanted && st.pairs < wanted; ++guard)
    {
        placed pa;
        placed pb;
        switch (fixture)
        {
            case 0:   // two crates standing on the same floor: shared up axis
                make_box(pa, vec3{0, 0.5f, 0}, vec3{0.5f, 0.5f, 0.5f}, r.yaw());
                make_box(pb, vec3{r.range(-0.9f, 0.9f), 0.5f, r.range(-0.9f, 0.9f)},
                         vec3{0.5f, 0.5f, 0.5f}, r.yaw());
                break;
            case 1:   // the same, one crate tilted a single degree
            {
                const quat tilt = quat_from_axis_angle(vec3{1, 0, 0},
                                                       static_cast<float>(k_pi / 180.0));
                make_box(pa, vec3{0, 0.5f, 0}, vec3{0.5f, 0.5f, 0.5f}, r.yaw());
                make_box(pb, vec3{r.range(-0.9f, 0.9f), 0.5f, r.range(-0.9f, 0.9f)},
                         vec3{0.5f, 0.5f, 0.5f}, r.yaw() * tilt);
                break;
            }
            case 2:   // free orientations
                make_box(pa, vec3{}, vec3{0.5f, 0.5f, 0.5f}, r.rotation());
                make_box(pb, r.direction() * r.range(0.0f, 1.0f), vec3{0.5f, 0.5f, 0.5f},
                         r.rotation());
                break;
            case 3:   // a ball in a crate
                make_box(pa, vec3{}, vec3{0.5f, 0.5f, 0.5f}, r.rotation());
                make_ball(pb, r.direction() * r.range(0.0f, 0.9f), 0.4f);
                break;
            case 4:   // a capsule against a crate
                make_box(pa, vec3{}, vec3{0.5f, 0.5f, 0.5f}, r.rotation());
                make_capsule(pb, r.direction() * r.range(0.0f, 1.0f), 0.25f, 0.5f, r.rotation());
                break;
            default:  // two hulls
                make_poly(pa, r, vec3{}, 12, 0.5f, r.rotation());
                make_poly(pb, r, r.direction() * r.range(0.0f, 0.9f), 12, 0.5f, r.rotation());
                break;
        }

        const convex va = pa.view();
        const convex vb = pb.view();
        const gjk_result g = gjk_distance(va, vb);
        if (g.status == gjk_status::separated) { continue; }
        ++st.pairs;

        st.simplex_count[g.terminal.count] += 1;
        if (g.terminal.count == 4)
        {
            const vec3 p0 = g.terminal.v[0].w;
            const float vol6 = dot(cross(g.terminal.v[1].w - p0, g.terminal.v[2].w - p0),
                                   g.terminal.v[3].w - p0);
            if (std::fabs(vol6) > 1e-9f) { ++st.tetra_with_volume; }
        }

        const epa_result e = epa_penetration(va, vb, g.terminal);
        st.seed_added[std::min(3, e.seed_vertices)] += 1;
        switch (e.status)
        {
            case epa_status::touching:        ++st.touching; break;
            case epa_status::degenerate:      ++st.degenerate; break;
            case epa_status::proven:          ++st.proven; break;
            case epa_status::stalled:         ++st.stalled; break;
            case epa_status::iteration_limit: ++st.iter_limit; break;
            case epa_status::capacity:        ++st.capacity; break;
        }
    }
}

void section_d()
{
    rule("D. THE SEED: WHAT GJK ACTUALLY HANDS OVER");

    static const char* names[6] = {"crates on a floor", "  ...one tilted 1 deg",
                                   "free orientations ", "ball in a crate   ",
                                   "capsule vs crate  ", "hull vs hull      "};
    seed_stats st[6];
    for (int f = 0; f < 6; ++f)
    {
        rng r(0x86040000u + static_cast<std::uint32_t>(f) * 7919u);
        st[f].name = names[f];
        gather_seeds(st[f], r, 50000, f);
    }

    std::printf("D.1  terminal simplex handed to EPA, by arrangement\n\n");
    std::printf("     fixture              pairs   1   2    3      4  tetra\n");
    for (int f = 0; f < 6; ++f)
    {
        std::printf("     %-19s %6d %3d %3d %4d %6d %6d\n", st[f].name, st[f].pairs,
                    st[f].simplex_count[1], st[f].simplex_count[2], st[f].simplex_count[3],
                    st[f].simplex_count[4], st[f].tetra_with_volume);
    }
    std::printf("\n     'tetra' is a 4-point simplex WITH VOLUME: the only\n");
    std::printf("     input a textbook EPA can start from.\n");

    std::printf("\nD.2  extra CSO vertices found, and how it ended\n\n");
    std::printf("     fixture          +0    +2 proven touch degen stall limit\n");
    for (int f = 0; f < 6; ++f)
    {
        std::printf("     %-15s %5d %5d %6d %5d %5d %5d %5d\n", st[f].name, st[f].seed_added[0],
                    st[f].seed_added[2], st[f].proven, st[f].touching, st[f].degenerate,
                    st[f].stalled, st[f].iter_limit + st[f].capacity);
    }

    std::printf("\n     CONTROL — a textbook EPA refuses everything but 'tetra'\n");
    for (int f = 0; f < 6; ++f)
    {
        const double refused = st[f].pairs > 0
                                   ? 100.0 * (st[f].pairs - st[f].tetra_with_volume)
                                         / static_cast<double>(st[f].pairs)
                                   : 0.0;
        std::printf("     %-19s %6.2f%% refused\n", st[f].name, refused);
    }

    check(st[0].tetra_with_volume == 0, "D.1 crates on a floor: no tetrahedron, ever");
    check(st[1].tetra_with_volume > st[0].tetra_with_volume, "D.1 one degree changes it");
    check(st[0].capacity == 0, "D.2 no floor pair runs out of room");
}

// ===========================================================================
// E. Containment, and which triangle to keep
// ===========================================================================

void section_e()
{
    rule("E. THE FLAT CASE, AND WHY THERE ARE TWO APEXES");

    // D.1 measured what a shared up axis produces, and it is not the flat
    // TETRAHEDRON the literature warns about — it is a flat TRIANGLE, because
    // `reduce_simplex` throws away any vertex the closest point does not use and
    // three points in a plane already span it.
    //
    // A triangle containing the origin, given one apex, is a tetrahedron with
    // the origin ON ITS BASE. This section measures what that costs, by building
    // both seeds by hand from the public `cso_support`.
    rng r(0x8605u);
    int flat = 0;
    int covers_origin = 0;
    std::vector<double> one_apex_depth;
    std::vector<double> two_apex_depth;
    std::vector<double> truth_depth;
    int one_apex_zero = 0;
    int non_convex = 0;

    for (int guard = 0; guard < 400000 && flat < 30000; ++guard)
    {
        placed pa;
        placed pb;
        make_box(pa, vec3{0, 0.5f, 0}, vec3{0.5f, 0.5f, 0.5f}, r.yaw());
        make_box(pb, vec3{r.range(-0.9f, 0.9f), 0.5f, r.range(-0.9f, 0.9f)},
                 vec3{0.5f, 0.5f, 0.5f}, r.yaw());

        const convex va = pa.view();
        const convex vb = pb.view();
        const gjk_result g = gjk_distance(va, vb);
        if (g.status == gjk_status::separated || g.terminal.count != 3) { continue; }
        ++flat;

        const vec3 q0 = g.terminal.v[0].w;
        const vec3 q1 = g.terminal.v[1].w;
        const vec3 q2 = g.terminal.v[2].w;

        // Does the triangle GJK reduced to actually cover the origin? It should,
        // because that is what "the closest point uses all three vertices"
        // means — but it is checked rather than believed.
        const vec3 nrm = cross(q1 - q0, q2 - q0);
        const float n2 = length_squared(nrm);
        if (n2 > 0.0f)
        {
            const float inv = 1.0f / n2;
            const float w0 = dot(cross(q2 - q1, -q1), nrm) * inv;
            const float w1 = dot(cross(q0 - q2, -q2), nrm) * inv;
            const float w2 = dot(cross(q1 - q0, -q0), nrm) * inv;
            if (w0 >= -1e-5f && w1 >= -1e-5f && w2 >= -1e-5f) { ++covers_origin; }
        }

        const vec3 plane = normalised_or(nrm, vec3{0, 1, 0});
        const vec3 delta = vb.origin - va.origin;
        const vec3 up = cso_support(va, vb, delta, plane).w;
        const vec3 down = cso_support(va, vb, delta, -plane).w;

        // ONE APEX: the tetrahedron q0 q1 q2 up. Its four outward face planes,
        // and the distance from the origin to the nearest of them.
        const vec3 tet1[4] = {q0, q1, q2, up};
        static constexpr int faces1[4][3] = {{0, 1, 2}, {0, 3, 1}, {0, 2, 3}, {1, 3, 2}};
        double best1 = 1e300;
        for (auto& f : faces1)
        {
            const vec3 nn = cross(tet1[f[1]] - tet1[f[0]], tet1[f[2]] - tet1[f[0]]);
            const float l2 = length_squared(nn);
            if (l2 <= 0.0f) { continue; }
            const double dd = static_cast<double>(dot(nn, tet1[f[0]])) / std::sqrt(l2);
            best1 = std::fmin(best1, std::fabs(dd));
        }
        one_apex_depth.push_back(best1);
        if (best1 < 1e-6) { ++one_apex_zero; }

        // TWO APEXES: the bipyramid. Its closest face, the same way.
        const vec3 bip[5] = {q0, q1, q2, up, down};
        static constexpr int faces2[6][3] = {{0, 1, 3}, {1, 2, 3}, {2, 0, 3},
                                             {1, 0, 4}, {2, 1, 4}, {0, 2, 4}};
        double best2 = 1e300;
        for (auto& f : faces2)
        {
            const vec3 nn = cross(bip[f[1]] - bip[f[0]], bip[f[2]] - bip[f[0]]);
            const float l2 = length_squared(nn);
            if (l2 <= 0.0f) { continue; }
            const double dd = static_cast<double>(dot(nn, bip[f[0]])) / std::sqrt(l2);
            best2 = std::fmin(best2, dd);
        }
        two_apex_depth.push_back(best2);

        // IS THE HAND-STITCHED BIPYRAMID EVEN CONVEX? Six faces from a triangle
        // to two apexes is a convex solid only when each apex projects INSIDE
        // the triangle, and a support point along the plane normal has no reason
        // to. Convexity is checkable: every vertex must be on the inner side of
        // every face plane.
        bool convex_ok = true;
        for (auto& f : faces2)
        {
            const vec3 nn = cross(bip[f[1]] - bip[f[0]], bip[f[2]] - bip[f[0]]);
            const float l2 = length_squared(nn);
            if (l2 <= 0.0f) { continue; }
            const float inv = 1.0f / std::sqrt(l2);
            const float dd = dot(nn, bip[f[0]]) * inv;
            for (const vec3& v : bip)
            {
                if (dot(nn, v) * inv > dd + 1e-5f) { convex_ok = false; }
            }
        }
        if (!convex_ok) { ++non_convex; }

        truth_depth.push_back(box_mtv_double(pa.b, pb.b).depth);
    }

    std::printf("E.1  %d FLAT terminal simplices, crates on a floor\n\n", flat);
    std::printf("     all three are a TRIANGLE, not a tetrahedron, and it\n");
    std::printf("     covers the origin: %d / %d\n", covers_origin, flat);

    std::printf("\nE.2  the starting lower bound each seed gives\n\n");
    std::printf("     seed              mean      p95       max\n");
    std::printf("     one apex     %.3e %.3e %.3e\n", mean_of(one_apex_depth),
                pct(one_apex_depth, 0.95), pct(one_apex_depth, 1.0));
    std::printf("     bipyramid    %.3e %.3e %.3e\n", mean_of(two_apex_depth),
                pct(two_apex_depth, 0.95), pct(two_apex_depth, 1.0));
    std::printf("     true depth   %.3e %.3e %.3e\n", mean_of(truth_depth),
                pct(truth_depth, 0.95), pct(truth_depth, 1.0));
    std::printf("\n     CONTROL — one apex puts the origin ON the base, so\n");
    std::printf("     its closest face is at zero: %d / %d below 1e-6 m.\n", one_apex_zero,
                flat);

    std::printf("\nE.3  and the bipyramid must not be stitched by hand\n\n");
    std::printf("     six faces from a triangle to two apexes is convex only\n");
    std::printf("     when each apex projects INSIDE the triangle:\n\n");
    std::printf("       NOT convex: %d / %d  (%.2f%%)\n", non_convex, flat,
                100.0 * non_convex / std::fmax(1.0, static_cast<double>(flat)));
    std::printf("\n     every one of those puts a face plane on the wrong side\n");
    std::printf("     of the origin, so the lower bound goes NEGATIVE and the\n");
    std::printf("     query stalls. Building the tetrahedron on one apex and\n");
    std::printf("     adding the other through the same beneath-and-beyond step\n");
    std::printf("     the main loop uses gives the convex hull by construction.\n");
    check(non_convex > 0, "E.3 hand-stitching really does produce non-convex seeds");

    check(covers_origin > flat * 99 / 100, "E.1 GJK's triangle covers the origin");
    check(one_apex_zero > flat * 99 / 100, "E.2 one apex starts at zero, every time");
    check(mean_of(two_apex_depth) > 1e-4, "E.2 the bipyramid starts somewhere useful");
}

// ===========================================================================
// F. The polytope stays a polytope
// ===========================================================================

void section_f()
{
    rule("F. EULER'S FORMULA AS A TEST, NOT A FACT");

    rng r(0x8606u);
    int pairs = 0;
    int manifold_ok = 0;
    int max_faces = 0;
    int max_vertices = 0;
    int max_swallowed = 0;

    // Two populations: ordinary crates, and SLIVERS — boxes with a 1000:1 aspect
    // ratio, whose support points are nearly collinear and whose faces therefore
    // have normals made of rounding error.
    for (int pop = 0; pop < 2; ++pop)
    {
        int n = 0;
        int ok = 0;
        int st = 0;
        int sw = 0;
        int mf = 0;
        for (int guard = 0; guard < 600000 && n < 100000; ++guard)
        {
            placed pa;
            placed pb;
            if (pop == 0)
            {
                make_box(pa, vec3{}, vec3{0.5f, 0.5f, 0.5f}, r.rotation());
                make_box(pb, r.direction() * r.range(0.0f, 1.0f), vec3{0.5f, 0.5f, 0.5f},
                         r.rotation());
            }
            else
            {
                make_box(pa, vec3{}, vec3{1.0f, 0.001f, 1.0f}, r.rotation());
                make_box(pb, r.direction() * r.range(0.0f, 1.2f), vec3{1.0f, 0.001f, 1.0f},
                         r.rotation());
            }

            const convex va = pa.view();
            const convex vb = pb.view();
            const query q = penetrate(va, vb);
            if (q.g.status == gjk_status::separated) { continue; }
            if (q.e.status == epa_status::degenerate) { continue; }
            ++n;

            if (q.e.manifold()) { ++ok; }
            if (q.e.status == epa_status::stalled) { ++st; }
            const int swallow = q.e.vertices - q.e.surface_vertices;
            if (swallow > 0) { ++sw; }
            max_swallowed = std::max(max_swallowed, swallow);
            mf = std::max(mf, q.e.faces);
            max_vertices = std::max(max_vertices, q.e.vertices);

            if (!q.e.manifold())
            {
                std::printf("  *** non-manifold: V=%d Vs=%d F=%d %s\n", q.e.vertices,
                            q.e.surface_vertices, q.e.faces, name_of(q.e.status));
            }
        }
        std::printf(pop == 0 ? "F.1  ordinary crates\n"
                             : "\nF.2  SLIVERS: 2 m x 2 mm x 2 m plates\n");
        std::printf("       queries                      %8d\n", n);
        std::printf("       F == 2*Vs - 4 at exit        %8d\n", ok);
        std::printf("       stopped early (stalled)      %8d\n", st);
        std::printf("       queries that swallowed a vertex  %5d\n", sw);
        std::printf("       largest polytope             %3d faces\n", mf);
        pairs += n;
        manifold_ok += ok;
        max_faces = std::max(max_faces, mf);
    }

    std::printf("\n     capacity is %d vertices / %d faces; the worst\n", k_epa_max_vertices,
                k_epa_max_faces);
    std::printf("     seen was %d vertices / %d faces.\n", max_vertices, max_faces);
    std::printf("     most vertices swallowed by one query: %d\n", max_swallowed);

    check(manifold_ok == pairs, "F Euler's formula holds at every exit");
    check(max_faces <= k_epa_max_faces, "F capacity never exceeded");

    // ---- F.3 the control: the textbook visibility test --------------------
    //
    // THE FAILURE IS NOT EXOTIC. A support point that lands EXACTLY on several
    // of the polytope's face planes — which is what an axis-aligned or
    // face-to-face contact produces constantly — leaves `dot(n, w)` and `d` as
    // the same number computed two different ways, and rounding decides which
    // side of the comparison each face falls on. Scattered across the polytope,
    // sharing no edges, they make a horizon that comes back in pieces.
    std::printf("\nF.3  CONTROL — visibility without the flood fill\n");

    epa_config plain;
    plain.flood_fill_visible = false;

    static const char* pop_names[3] = {"crates on a floor", "free orientations",
                                       "hull vs hull     "};
    for (int pop = 0; pop < 3; ++pop)
    {
        rng rr(0x860633u + static_cast<std::uint32_t>(pop) * 5171u);
        int n = 0;
        int disagree = 0;
        int torn = 0;
        double worst = 0.0;
        double worst_good = 0.0;
        double worst_bad = 0.0;
        for (int guard = 0; guard < 400000 && n < 50000; ++guard)
        {
            placed pa;
            placed pb;
            if (pop == 0)
            {
                make_box(pa, vec3{0, 0.5f, 0}, vec3{0.5f, 0.5f, 0.5f}, rr.yaw());
                make_box(pb, vec3{rr.range(-0.9f, 0.9f), 0.5f, rr.range(-0.9f, 0.9f)},
                         vec3{0.5f, 0.5f, 0.5f}, rr.yaw());
            }
            else if (pop == 1)
            {
                make_box(pa, vec3{}, vec3{0.5f, 0.5f, 0.5f}, rr.rotation());
                make_box(pb, rr.direction() * rr.range(0.0f, 1.0f), vec3{0.5f, 0.5f, 0.5f},
                         rr.rotation());
            }
            else
            {
                make_poly(pa, rr, vec3{}, 12, 0.5f, rr.rotation());
                make_poly(pb, rr, rr.direction() * rr.range(0.0f, 0.9f), 12, 0.5f,
                          rr.rotation());
            }

            const convex va = pa.view();
            const convex vb = pb.view();
            const gjk_result g = gjk_distance(va, vb);
            if (g.status == gjk_status::separated) { continue; }
            ++n;
            const epa_result good = epa_penetration(va, vb, g.terminal);
            const epa_result bad = epa_penetration(va, vb, g.terminal, plain);
            if (bad.status == epa_status::stalled) { ++torn; }
            const double rel = std::fabs(static_cast<double>(good.depth - bad.depth))
                               / std::fmax(1e-9, static_cast<double>(good.depth));
            if (rel > 1e-3) { ++disagree; }
            if (rel > worst)
            {
                worst = rel;
                worst_good = static_cast<double>(good.depth);
                worst_bad = static_cast<double>(bad.depth);
            }
        }
        std::printf("\n     %s  (%d overlapping pairs)\n", pop_names[pop], n);
        std::printf("       textbook horizon torn:      %6d  (%.2f%%)\n", torn,
                    100.0 * torn / std::fmax(1.0, static_cast<double>(n)));
        std::printf("       depth wrong by > 0.1%%:      %6d  (%.2f%%)\n", disagree,
                    100.0 * disagree / std::fmax(1.0, static_cast<double>(n)));
        std::printf("       worst: %.6f against %.6f  (%.1f%% low)\n", worst_bad, worst_good,
                    100.0 * worst);
        if (pop == 0) { check(torn > 0, "F.3 the textbook formulation really does tear"); }
    }
    std::printf("\n     every torn case is the same shape of failure: the\n");
    std::printf("     expansion stops early, so `depth` is whatever face the\n");
    std::printf("     polytope happened to have — a valid LOWER bound on the\n");
    std::printf("     truth, and nothing more.\n");
}

// ===========================================================================
// G. Polytopes terminate, curves do not
// ===========================================================================

void section_g()
{
    rule("G. WHAT CONVERGES, AND WHAT SIMPLY STOPS");

    static const char* names[5] = {"box vs box    ", "hull vs hull  ", "capsule vs box",
                                   "sphere vs box ", "sphere/sphere "};
    static const float tols[5] = {1e-2f, 1e-3f, 1e-4f, 1e-5f, 1e-6f};

    std::printf("G.1  mean EPA iterations as the tolerance tightens\n\n");
    std::printf("     shape pair      1e-2   1e-3   1e-4   1e-5   1e-6\n");

    double last_row[5][5] = {};
    for (int s = 0; s < 5; ++s)
    {
        std::printf("     %-14s", names[s]);
        for (int t = 0; t < 5; ++t)
        {
            rng r(0x8607u + static_cast<std::uint32_t>(s) * 131u);
            epa_config cfg;
            cfg.tolerance = tols[t];
            cfg.max_iterations = 64;

            double total = 0.0;
            int n = 0;
            for (int guard = 0; guard < 200000 && n < 4000; ++guard)
            {
                placed pa;
                placed pb;
                make_box(pa, vec3{}, vec3{0.5f, 0.5f, 0.5f}, r.rotation());
                switch (s)
                {
                    case 0:
                        make_box(pb, r.direction() * r.range(0.0f, 0.9f),
                                 vec3{0.5f, 0.5f, 0.5f}, r.rotation());
                        break;
                    case 1:
                        make_poly(pa, r, vec3{}, 14, 0.5f, r.rotation());
                        make_poly(pb, r, r.direction() * r.range(0.0f, 0.8f), 14, 0.5f,
                                  r.rotation());
                        break;
                    case 2:
                        make_capsule(pb, r.direction() * r.range(0.0f, 0.9f), 0.25f, 0.4f,
                                     r.rotation());
                        break;
                    case 3: make_ball(pb, r.direction() * r.range(0.0f, 0.8f), 0.4f); break;
                    default:
                        make_ball(pa, vec3{}, 0.5f);
                        make_ball(pb, r.direction() * r.range(0.0f, 0.9f), 0.4f);
                        break;
                }

                const convex va = pa.view();
                const convex vb = pb.view();
                const gjk_result g = gjk_distance(va, vb);
                if (g.status == gjk_status::separated) { continue; }
                const epa_result e = epa_penetration(va, vb, g.terminal, cfg);
                if (e.status == epa_status::degenerate) { continue; }
                ++n;
                total += e.iterations;
            }
            last_row[s][t] = n > 0 ? total / n : 0.0;
            std::printf(" %6.2f", last_row[s][t]);
        }
        std::putchar('\n');
    }

    std::printf("\n     CONTROL — 8.5 ran this table for GJK and found the\n");
    std::printf("     SPHERE flat at ONE iteration, because a ball's nearest\n");
    std::printf("     point to an outside point is on the line to its centre.\n");
    std::printf("     Here it is the WORST row. Same shape, same support\n");
    std::printf("     function, opposite behaviour: GJK walks TO a ball's\n");
    std::printf("     surface and EPA has to cover it with flat triangles.\n\n");

    // GJK on two separated spheres, for the contrast.
    {
        rng r(0x860712u);
        double total = 0.0;
        int n = 0;
        for (int guard = 0; guard < 200000 && n < 4000; ++guard)
        {
            placed pa;
            placed pb;
            make_ball(pa, vec3{}, 0.5f);
            make_ball(pb, r.direction() * r.range(1.1f, 2.0f), 0.4f);
            const convex va = pa.view();
            const convex vb = pb.view();
            const gjk_result g = gjk_distance(va, vb);
            if (g.status != gjk_status::separated) { continue; }
            ++n;
            total += g.iterations;
        }
        std::printf("     GJK, sphere vs sphere, separated: %.2f iterations\n",
                    n > 0 ? total / n : 0.0);
        std::printf("     EPA, sphere vs sphere, at 1e-6:   %.2f\n", last_row[4][4]);
        std::printf("\n     which is why 8.4 ships collide(sphere, sphere) as a\n");
        std::printf("     subtraction, and why an engine keeps it.\n");
    }

    // ---- G.2 what the cap costs on a shape that cannot terminate ----------
    std::printf("\nG.2  the cap, on the shape that never finishes\n\n");
    std::printf("     a ball deep inside a crate, tolerance 1e-4\n\n");
    std::printf("     cap    hit the cap   mean depth error\n");
    static const int caps[4] = {12, 20, 32, 48};
    for (int c = 0; c < 4; ++c)
    {
        rng r(0x860722u);
        epa_config cfg;
        cfg.tolerance = 1e-4f;
        cfg.max_iterations = caps[c];
        int n = 0;
        int capped = 0;
        double err = 0.0;
        for (int guard = 0; guard < 200000 && n < 20000; ++guard)
        {
            placed pa;
            placed pb;
            make_box(pa, vec3{}, vec3{0.5f, 0.5f, 0.5f}, r.rotation());
            const vec3 c0 = r.direction() * r.range(0.0f, 0.8f);
            make_ball(pb, c0, 0.4f);
            const convex va = pa.view();
            const convex vb = pb.view();
            const gjk_result g = gjk_distance(va, vb);
            if (g.status == gjk_status::separated) { continue; }
            const epa_result e = epa_penetration(va, vb, g.terminal, cfg);
            if (e.status == epa_status::degenerate) { continue; }
            ++n;
            if (e.status == epa_status::iteration_limit) { ++capped; }
            // The closed form: a sphere against a box is 8.4's clamp, and it
            // knows the answer exactly.
            const separation exact = collide(pa.b, pb.s);
            err += std::fabs(static_cast<double>(e.depth) - static_cast<double>(exact.depth));
        }
        std::printf("     %3d    %5.2f%%        %.3e m\n", caps[c],
                    100.0 * capped / std::fmax(1.0, static_cast<double>(n)),
                    err / std::fmax(1.0, static_cast<double>(n)));
    }
    std::printf("\n     CONTROL — 8.4's collide(obb, sphere) is the closed form\n");
    std::printf("     these are measured against, and it is a clamp and a\n");
    std::printf("     subtraction. A ball is the one shape where the general\n");
    std::printf("     algorithm is strictly worse than knowing what you hold.\n");

    check(last_row[0][4] < last_row[4][4], "G.1 a box terminates sooner than a sphere");
    check(last_row[4][4] > last_row[4][0] + 1.0, "G.1 the sphere genuinely converges");
    check(last_row[0][4] - last_row[0][0] < 1.0, "G.1 a box does not converge, it finishes");
}

// ===========================================================================
// H. Accuracy, and where it gives out
// ===========================================================================

void section_h()
{
    rule("H. HOW CLOSE, AND WHAT SETS THE FLOOR");

    // ---- H.1 error against the exact reference, vs tolerance --------------
    static const float tols[5] = {1e-2f, 1e-3f, 1e-4f, 1e-5f, 1e-6f};
    std::printf("H.1  |EPA - exact| for 1 m crates, against tolerance\n\n");
    std::printf("     tol     mean err     max err    err/(tol*size)\n");

    for (int t = 0; t < 5; ++t)
    {
        rng r(0x8608u);
        epa_config cfg;
        cfg.tolerance = tols[t];
        cfg.max_iterations = 64;

        std::vector<double> errs;
        for (int guard = 0; guard < 200000 && errs.size() < 20000; ++guard)
        {
            placed pa;
            placed pb;
            make_box(pa, vec3{}, vec3{0.5f, 0.5f, 0.5f}, r.rotation());
            make_box(pb, r.direction() * r.range(0.0f, 0.9f), vec3{0.5f, 0.5f, 0.5f},
                     r.rotation());
            const convex va = pa.view();
            const convex vb = pb.view();
            const gjk_result g = gjk_distance(va, vb);
            if (g.status == gjk_status::separated) { continue; }
            const epa_result e = epa_penetration(va, vb, g.terminal, cfg);
            if (e.status != epa_status::proven) { continue; }
            const exact_mtv x = box_mtv_double(pa.b, pb.b);
            errs.push_back(std::fabs(static_cast<double>(e.depth) - x.depth));
        }
        const double size = std::sqrt(3.0);   // the 1 m cube's largest CSO vertex scale
        std::printf("     %.0e  %.3e  %.3e      %8.3f\n", static_cast<double>(tols[t]),
                    mean_of(errs), pct(errs, 1.0),
                    pct(errs, 1.0) / (static_cast<double>(tols[t]) * size));
    }

    // ---- H.2 the shallow end ---------------------------------------------
    std::printf("\nH.2  two 1 m cubes, face to face, walked into contact\n\n");
    std::printf("     overlap      status      depth        error\n");

    static const float overlaps[8] = {1e-1f, 1e-2f, 1e-3f, 1e-4f, 1e-5f, 1e-6f, 1e-7f, 0.0f};
    for (float ov : overlaps)
    {
        placed pa;
        placed pb;
        make_box(pa, vec3{}, vec3{0.5f, 0.5f, 0.5f}, quat{});
        make_box(pb, vec3{1.0f - ov, 0.0f, 0.0f}, vec3{0.5f, 0.5f, 0.5f}, quat{});
        const convex va = pa.view();
        const convex vb = pb.view();
        const query q = penetrate(va, vb);
        const double truth = static_cast<double>(ov);
        std::printf("     %.0e  %-12s %.3e  %+.3e\n", static_cast<double>(ov),
                    name_of(q.e.status), static_cast<double>(q.e.depth),
                    static_cast<double>(q.e.depth) - truth);
    }

    // ---- H.3 the world origin --------------------------------------------
    //
    // 8.4 §11 found the wall; 8.5 §11 built the cure into `convex`, by taking
    // every support point relative to its own shape's centre so the one
    // world-sized subtraction happens once and is exact by Sterbenz's lemma. EPA
    // inherits it by inheriting `cso_support`, and this is the check that it
    // really did — two arms, the SAME epa.cpp, differing only in the `convex`
    // view they are handed.
    std::printf("\nH.3  the same overlap, walked away from the world origin\n\n");
    std::printf("     distance    depth (rel)  err rel    err naive\n");

    // THE HALF EXTENTS ARE DELIBERATELY UGLY. A half extent of 0.5 is a multiple
    // of the float grid spacing at every scale below 2^21, so `centre + half`
    // stays EXACT even a million metres out and the naive arm never suffers —
    // an earlier version of this fixture used 0.5 and measured a difference of
    // exactly zero at every distance, which says nothing about the arithmetic
    // and everything about the fixture. 0.37 is not a multiple of anything.
    static const float bases[6] = {1.0f, 16.0f, 1024.0f, 16384.0f, 131072.0f, 1048576.0f};
    for (float base : bases)
    {
        const vec3 origin{base, base, base};
        placed pa;
        placed pb;
        make_box(pa, origin, vec3{0.37f, 0.53f, 0.41f}, quat{});
        make_box(pb, origin + vec3{0.61f, 0.13f, 0.07f}, vec3{0.37f, 0.53f, 0.41f}, quat{});

        const convex va = pa.view();
        const convex vb = pb.view();
        const query rel = penetrate(va, vb);

        const convex wa = as_convex_world(pa.b);
        const convex wb = as_convex_world(pb.b);
        const query nai = penetrate(wa, wb);

        // The reference is computed in double from the SAME float box data, so
        // it measures the arithmetic rather than the geometry: whatever the
        // positions rounded to, this is the depth those boxes really have.
        const exact_mtv x = box_mtv_double(pa.b, pb.b);
        const double e_rel = std::fabs(static_cast<double>(rel.e.depth) - x.depth);
        const double e_nai = std::fabs(static_cast<double>(nai.e.depth) - x.depth);
        std::printf("     %9.0f m %.6e  %.2e  %.2e\n", static_cast<double>(base * 1.7320508f),
                    static_cast<double>(rel.e.depth), e_rel, e_nai);
    }
    std::printf("\n     the reference is the exact MTV of the boxes as FLOAT\n");
    std::printf("     stored them, so this measures the arithmetic. What\n");
    std::printf("     float positions already threw away is gone either way.\n");

    check(true, "H sections printed");
}

// ===========================================================================
// I. The budget
// ===========================================================================

void section_i()
{
    rule("I. WHAT IT COSTS");

    rng r(0x8609u);
    const int n = 20000;
    std::vector<placed> as(static_cast<std::size_t>(n));
    std::vector<placed> bs(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        make_box(as[static_cast<std::size_t>(i)], vec3{}, vec3{0.5f, 0.5f, 0.5f}, r.rotation());
        make_box(bs[static_cast<std::size_t>(i)], r.direction() * r.range(0.0f, 0.9f),
                 vec3{0.5f, 0.5f, 0.5f}, r.rotation());
    }

    const bench_result sat = bench_run(static_cast<std::size_t>(n), 9, [&] {
        double acc = 0.0;
        for (int i = 0; i < n; ++i)
        {
            acc += collide(as[static_cast<std::size_t>(i)].b, bs[static_cast<std::size_t>(i)].b)
                       .depth;
        }
        return acc;
    });

    const bench_result gjk_only = bench_run(static_cast<std::size_t>(n), 9, [&] {
        double acc = 0.0;
        for (int i = 0; i < n; ++i)
        {
            acc += gjk_distance(as[static_cast<std::size_t>(i)].view(),
                                bs[static_cast<std::size_t>(i)].view())
                       .distance;
        }
        return acc;
    });

    const bench_result both = bench_run(static_cast<std::size_t>(n), 9, [&] {
        double acc = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const convex va = as[static_cast<std::size_t>(i)].view();
            const convex vb = bs[static_cast<std::size_t>(i)].view();
            const gjk_result g = gjk_distance(va, vb);
            if (g.status == gjk_status::separated) { acc -= g.distance; }
            else { acc += epa_penetration(va, vb, g.terminal).depth; }
        }
        return acc;
    });

    std::printf("I.1  %d box pairs, mixed overlapping and apart\n\n", n);
    std::printf("     SAT  collide(obb, obb)     %8.2f ns  (spread %.2f)\n", sat.median_ns,
                sat.spread());
    std::printf("     GJK  gjk_distance          %8.2f ns  (spread %.2f)\n", gjk_only.median_ns,
                gjk_only.spread());
    std::printf("     GJK + EPA                  %8.2f ns  (spread %.2f)\n", both.median_ns,
                both.spread());
    std::printf("     EPA's share                %8.2f ns\n",
                both.median_ns - gjk_only.median_ns);

    // ---- I.2 allocations --------------------------------------------------
    {
        const std::size_t before = g_allocs;
        double acc = 0.0;
        for (int i = 0; i < 1000; ++i)
        {
            const convex va = as[static_cast<std::size_t>(i)].view();
            const convex vb = bs[static_cast<std::size_t>(i)].view();
            const gjk_result g = gjk_distance(va, vb);
            acc += static_cast<double>(epa_penetration(va, vb, g.terminal).depth);
        }
        const std::size_t after = g_allocs;
        std::printf("\nI.2  1000 queries, heap allocations: %zu\n", after - before);
        std::printf("     (global operator new replaced and counted)\n");
        check(after == before, "I.2 EPA allocates nothing");
        (void)acc;
    }

    // ---- I.3 cost against depth ------------------------------------------
    std::printf("\nI.3  cost against how deep the overlap is\n\n");
    std::printf("     overlap    iterations   vertices   ns/query\n");
    static const float seps[5] = {0.95f, 0.75f, 0.50f, 0.25f, 0.05f};
    for (float sep : seps)
    {
        rng rr(0x860903u);
        std::vector<placed> xa(2000);
        std::vector<placed> xb(2000);
        for (int i = 0; i < 2000; ++i)
        {
            make_box(xa[static_cast<std::size_t>(i)], vec3{}, vec3{0.5f, 0.5f, 0.5f},
                     rr.rotation());
            make_box(xb[static_cast<std::size_t>(i)], rr.direction() * sep,
                     vec3{0.5f, 0.5f, 0.5f}, rr.rotation());
        }
        double it = 0.0;
        double vt = 0.0;
        int m = 0;
        for (int i = 0; i < 2000; ++i)
        {
            const convex va = xa[static_cast<std::size_t>(i)].view();
            const convex vb = xb[static_cast<std::size_t>(i)].view();
            const query q = penetrate(va, vb);
            if (q.e.status != epa_status::proven) { continue; }
            ++m;
            it += q.e.iterations;
            vt += q.e.vertices;
        }
        const bench_result t = bench_run(2000, 7, [&] {
            double acc = 0.0;
            for (int i = 0; i < 2000; ++i)
            {
                const convex va = xa[static_cast<std::size_t>(i)].view();
                const convex vb = xb[static_cast<std::size_t>(i)].view();
                const gjk_result g = gjk_distance(va, vb);
                acc += static_cast<double>(epa_penetration(va, vb, g.terminal).depth);
            }
            return acc;
        });
        std::printf("     %.2f m      %6.2f     %6.2f    %7.1f\n", static_cast<double>(sep),
                    m > 0 ? it / m : 0.0, m > 0 ? vt / m : 0.0, t.median_ns);
    }

    check(both.median_ns > gjk_only.median_ns, "I.1 EPA costs something");
}

} // namespace

int main()
{
    std::printf("verify_86 — Lesson 8.6, EPA: penetration depth\n");
    std::printf("sizeof(epa_result) = %zu bytes\n", sizeof(epa_result));

    section_a();
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();
    section_g();
    section_h();
    section_i();

    std::printf("\n");
    for (int i = 0; i < 66; ++i) { std::putchar('='); }
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
