// scratch/verify_85.cpp — every number Lesson 8.5 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_85.sh
//
// Nine sections, in the lesson's order:
//
//   A  the number the SAT could not give
//   B  the support function IS the shape
//   C  the simplex solver
//   D  does it agree with the SAT about the sign?
//   E  the certificate: two bounds that squeeze
//   F  polytopes terminate, curves converge
//   G  warm starting
//   H  where the origin is
//   I  the budget
//
// EVERY SECTION CARRIES A CONTROL — 8.1 through 8.4's rule, in two halves: ask
// what the control would say if the thing were COMPLETELY BROKEN, and what it
// would say if the thing were completely FINE.
//
// THE VERIFICATION INSTRUMENT IS STRONGER THAN 8.4'S, and it is worth saying why
// before any of it runs. 8.4 checked a BOOLEAN by re-proving a separating axis:
// a certificate that says "apart" and nothing else. GJK returns a NUMBER, and
// that number certifies itself from two directions at once —
//
//   UPPER   |point_a − point_b|. Both are real points of real shapes, so the
//           true distance cannot be larger.
//   LOWER   the slab between the two supporting planes with normal `direction`.
//           Nothing of A reaches past one, nothing of B reaches past the other,
//           so the true distance cannot be smaller.
//
// Two support calls and four dot products, computed in double from the float
// shape data, and `lower <= truth <= upper` holds whatever produced the result.
// There is no reference GJK anywhere in this file, because a reference would
// only tell us the two agreed.
//
// WHERE A CLOSED FORM EXISTS, IT IS USED ANYWAY, because a certificate proves
// the answer is in a range and a closed form proves which number it is. Four of
// them appear below — AABB/AABB, sphere/sphere, sphere/OBB and sphere/capsule —
// and each checks a case GJK does not know is special.
//
// PRECISION. The engine collides in `float`, so this harness does too.
// References and certificates are evaluated in `double`, the only honest way
// round: a reference computed at the same precision as the thing it checks
// cannot tell you which of the two is wrong. §H is about exactly that gap.
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
#include <engine/phys/gjk.hpp>
#include <engine/phys/shape.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
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
using engine::phys::as_obb;
using engine::phys::box_shape;
using engine::phys::capsule;
using engine::phys::capsule_shape;
using engine::phys::certify;
using engine::phys::closest_point;
using engine::phys::collide;
using engine::phys::convex;
using engine::phys::cso_support;
using engine::phys::gap_on_axis;
using engine::phys::gjk_bounds;
using engine::phys::gjk_config;
using engine::phys::gjk_distance;
using engine::phys::gjk_intersects;
using engine::phys::gjk_result;
using engine::phys::gjk_status;
using engine::phys::gjk_vertex;
using engine::phys::hull;
using engine::phys::obb;
using engine::phys::overlaps;
using engine::phys::reduce_simplex;
using engine::phys::separation;
using engine::phys::shape;
using engine::phys::simplex;
using engine::phys::sphere_shape;
using engine::phys::support;
using engine::phys::support_local;
using engine::phys::world_capsule;
using engine::phys::world_hull;
using engine::phys::world_obb;

namespace
{

constexpr double k_pi = 3.14159265358979323846;

void rule(const char* title)
{
    std::printf("\n%s\n", title);
    for (int i = 0; i < 66; ++i) { std::putchar('-'); }
    std::putchar('\n');
}

/// The same deterministic generator 8.1-8.4 used, for the same reason:
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

private:
    std::uint32_t state_;
};

// ---------------------------------------------------------------------------
// Double-precision arithmetic, for references and certificates
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
dvec3 dsub(dvec3 a, dvec3 b) { return dvec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
dvec3 dadd(dvec3 a, dvec3 b) { return dvec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
dvec3 dmul(dvec3 a, double s) { return dvec3{a.x * s, a.y * s, a.z * s}; }
double dlen(dvec3 a) { return std::sqrt(ddot(a, a)); }

/// The farthest point of a box in direction `d`, in double, from float data.
dvec3 support_double(const obb& box, dvec3 d)
{
    const dvec3 u0 = promote(box.axes.c0);
    const dvec3 u1 = promote(box.axes.c1);
    const dvec3 u2 = promote(box.axes.c2);
    const double sx = (ddot(u0, d) >= 0.0 ? 1.0 : -1.0) * static_cast<double>(box.half_extents.x);
    const double sy = (ddot(u1, d) >= 0.0 ? 1.0 : -1.0) * static_cast<double>(box.half_extents.y);
    const double sz = (ddot(u2, d) >= 0.0 ? 1.0 : -1.0) * static_cast<double>(box.half_extents.z);
    return dadd(promote(box.centre),
                dadd(dmul(u0, sx), dadd(dmul(u1, sy), dmul(u2, sz))));
}

// ---------------------------------------------------------------------------
// Closed forms. Each one knows a case GJK does not know is special.
// ---------------------------------------------------------------------------

/// The exact distance between two axis-aligned boxes given as CENTRE and HALF
/// EXTENT, with every subtraction taken in double.
///
/// §H's reference, and the promotion is the whole point of it. Writing
/// `centre - half` in `float` first would round the corner to the float grid,
/// and the reference would then agree with whichever arm happened to round the
/// same way — which is exactly the bias 8.4 §3 found in its own 87% figure.
/// `centre` and `half` are exact floats; their difference in double is exact;
/// so these are the corners the boxes REALLY have.
double centre_box_distance_double(vec3 ca, vec3 ha, vec3 cb, vec3 hb)
{
    const dvec3 pa = promote(ca);
    const dvec3 qa = promote(ha);
    const dvec3 pb = promote(cb);
    const dvec3 qb = promote(hb);

    const double gx = std::fmax(0.0, std::fabs(pb.x - pa.x) - (qa.x + qb.x));
    const double gy = std::fmax(0.0, std::fabs(pb.y - pa.y) - (qa.y + qb.y));
    const double gz = std::fmax(0.0, std::fabs(pb.z - pa.z) - (qa.z + qb.z));
    return std::sqrt(gx * gx + gy * gy + gz * gz);
}

/// The exact distance between two axis-aligned boxes.
///
/// Separable, so it is three independent one-dimensional gaps and a length —
/// the same "a box is a product of three intervals" that made 8.4's
/// `closest_point` a clamp.
double aabb_distance_double(const aabb& a, const aabb& b)
{
    const double gx = std::fmax(0.0, std::fmax(static_cast<double>(a.min.x) - b.max.x,
                                               static_cast<double>(b.min.x) - a.max.x));
    const double gy = std::fmax(0.0, std::fmax(static_cast<double>(a.min.y) - b.max.y,
                                               static_cast<double>(b.min.y) - a.max.y));
    const double gz = std::fmax(0.0, std::fmax(static_cast<double>(a.min.z) - b.max.z,
                                               static_cast<double>(b.min.z) - a.max.z));
    return std::sqrt(gx * gx + gy * gy + gz * gz);
}

// ---------------------------------------------------------------------------
// The SAT's best answer, for §A
// ---------------------------------------------------------------------------

/// The LARGEST gap over all fifteen candidate axes: the tightest lower bound the
/// Separating Axis Theorem can produce for two boxes.
///
/// `collide(obb, obb)` returns on the FIRST separating axis, because one witness
/// is a complete proof and the remaining candidates cannot change the verdict.
/// That makes its `depth` a valid lower bound but not the best one, so §A quotes
/// both: what the shipped function reports, and what the SAT could report if it
/// were asked to try harder. Neither reaches the distance, and the gap between
/// them is the point.
float sat_best_gap(const obb& a, const obb& b)
{
    float best = -1e30f;
    for (int i = 0; i < 3; ++i) { best = std::fmax(best, gap_on_axis(a, b, a.axis(i))); }
    for (int j = 0; j < 3; ++j) { best = std::fmax(best, gap_on_axis(a, b, b.axis(j))); }
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            const vec3 c = cross(a.axis(i), b.axis(j));
            const float len2 = length_squared(c);
            if (len2 <= 1e-12f) { continue; }
            best = std::fmax(best, gap_on_axis(a, b, c / std::sqrt(len2)));
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// The certificate, in double
// ---------------------------------------------------------------------------

/// Re-derive GJK's two bounds in double, from the BOX DATA and the claimed
/// direction alone. Nothing in gjk.cpp is consulted.
struct dbounds
{
    double lower = 0.0;
    double upper = 0.0;
};

dbounds certify_double(const obb& a, const obb& b, const gjk_result& r)
{
    dbounds out;
    out.upper = dlen(dsub(promote(r.point_b), promote(r.point_a)));

    const dvec3 n0 = promote(r.direction);
    const double nl = dlen(n0);
    if (nl <= 0.0) { return out; }
    const dvec3 n = dmul(n0, 1.0 / nl);

    const dvec3 sa = support_double(a, n);
    const dvec3 sb = support_double(b, dmul(n, -1.0));
    out.lower = ddot(dsub(sb, sa), n);
    return out;
}

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

/// A convex point set: points on a sphere, so every one is a hull vertex.
std::vector<vec3> make_hull_points(rng& r, int n, float radius)
{
    std::vector<vec3> pts;
    pts.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) { pts.push_back(r.direction() * radius); }
    return pts;
}

/// The naive `convex`: support in WORLD space, origin at nothing.
///
/// §H's second arm, and note that it is not a second implementation of GJK — it
/// is the SAME gjk.cpp handed a differently-shaped view, so `delta` is zero and
/// every `w = pa − pb` subtracts two world-sized numbers on every iteration.
/// That is exactly the formulation `convex.hpp` exists to avoid, and making it
/// an adapter rather than a fork is what makes the comparison mean something.
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

// ===========================================================================
// A  THE NUMBER THE SAT COULD NOT GIVE
// ===========================================================================

void section_a()
{
    rule("A  THE NUMBER THE SAT COULD NOT GIVE");

    // ---- A.1  the worked example, by hand ---------------------------------
    {
        const shape cube = box_shape(vec3{0.5f, 0.5f, 0.5f});
        const obb A = world_obb(cube, vec3{0.0f, 0.0f, 0.0f}, quat::identity());
        const obb B = world_obb(cube, vec3{2.0f, 2.0f, 2.0f}, quat::identity());

        const separation sat = collide(A, B);
        const float best = sat_best_gap(A, B);
        const gjk_result g = gjk_distance(as_convex(A), as_convex(B));

        std::printf("  two unit cubes, centres (2, 2, 2) apart\n\n");
        std::printf("  A.1  SAT, as shipped   gap %.7f m  (axis %d)\n",
                    -sat.depth, sat.axis_index);
        std::printf("       SAT, best of 15   gap %.7f m\n", best);
        std::printf("       GJK               dst %.7f m  (%d iters)\n",
                    static_cast<double>(g.distance), g.iterations);
        std::printf("       sqrt(3)              %.7f m\n", std::sqrt(3.0));
        std::printf("       the SAT is low by %.1f%%;  ratio %.6f = 1/sqrt(3)\n",
                    100.0 * (1.0 - static_cast<double>(best) / std::sqrt(3.0)),
                    static_cast<double>(best) / static_cast<double>(g.distance));

        check(std::fabs(static_cast<double>(g.distance) - std::sqrt(3.0)) < 1e-5,
              "A.1 GJK reproduces sqrt(3)");
        check(std::fabs(static_cast<double>(best) - 1.0) < 1e-5,
              "A.1 the SAT's best of fifteen is 1.0");

        // The witness points are the two corners, and they can be named.
        std::printf("       witness A (%.4f, %.4f, %.4f)\n",
                    static_cast<double>(g.point_a.x), static_cast<double>(g.point_a.y),
                    static_cast<double>(g.point_a.z));
        std::printf("       witness B (%.4f, %.4f, %.4f)\n",
                    static_cast<double>(g.point_b.x), static_cast<double>(g.point_b.y),
                    static_cast<double>(g.point_b.z));
    }

    // ---- A.2  how often, and how badly, on random pairs -------------------
    {
        rng r(0x8501u);
        const int n = 200000;
        int apart = 0;
        int low_10 = 0;      // the SAT's best is more than 10% low
        int low_1 = 0;
        double worst_ratio = 1.0;
        double sum_ratio = 0.0;
        double worst_shipped = 1.0;

        for (int i = 0; i < n; ++i)
        {
            const shape sa = box_shape(vec3{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f),
                                            r.range(0.2f, 1.2f)});
            const shape sb = box_shape(vec3{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f),
                                            r.range(0.2f, 1.2f)});
            const obb A = world_obb(sa, vec3{}, r.rotation());
            const obb B = world_obb(sb, r.direction() * r.range(0.5f, 4.0f), r.rotation());

            const gjk_result g = gjk_distance(as_convex(A), as_convex(B));
            if (g.status != gjk_status::separated || g.distance < 1e-4f) { continue; }
            ++apart;

            const double d = static_cast<double>(g.distance);
            const double best = static_cast<double>(sat_best_gap(A, B));
            const double ratio = best / d;
            sum_ratio += ratio;
            if (ratio < worst_ratio) { worst_ratio = ratio; }
            if (ratio < 0.90) { ++low_10; }
            if (ratio < 0.99) { ++low_1; }

            const separation sat = collide(A, B);
            const double shipped = static_cast<double>(-sat.depth) / d;
            if (shipped < worst_shipped) { worst_shipped = shipped; }
        }

        std::printf("\n  A.2  %d random OBB pairs, %d of them apart\n", n, apart);
        std::printf("       SAT best-of-15 / true distance:\n");
        std::printf("         mean  %.4f    worst  %.4f\n",
                    sum_ratio / apart, worst_ratio);
        std::printf("         more than  1%% low: %d  (%.1f%%)\n",
                    low_1, 100.0 * low_1 / apart);
        std::printf("         more than 10%% low: %d  (%.1f%%)\n",
                    low_10, 100.0 * low_10 / apart);
        std::printf("       and the SHIPPED first-axis answer, worst  %.4f\n",
                    worst_shipped);
        check(worst_ratio <= 1.0 + 1e-4, "A.2 the SAT never exceeds the distance");
    }

    // ---- A.3  CONTROL: two AABBs have a closed form -----------------------
    {
        rng r(0x8502u);
        const int n = 50000;
        int apart = 0;
        int wrong_status = 0;
        double worst = 0.0;

        for (int i = 0; i < n; ++i)
        {
            aabb A;
            A.min = vec3{r.range(-2.0f, 0.0f), r.range(-2.0f, 0.0f), r.range(-2.0f, 0.0f)};
            A.max = A.min + vec3{r.range(0.2f, 2.0f), r.range(0.2f, 2.0f), r.range(0.2f, 2.0f)};
            aabb B;
            B.min = vec3{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)};
            B.max = B.min + vec3{r.range(0.2f, 2.0f), r.range(0.2f, 2.0f), r.range(0.2f, 2.0f)};

            const obb oa = as_obb(A);
            const obb ob = as_obb(B);
            const gjk_result g = gjk_distance(as_convex(oa), as_convex(ob));
            const double truth = aabb_distance_double(A, B);
            if (truth <= 1e-3) { continue; }
            ++apart;
            if (g.status != gjk_status::separated) { ++wrong_status; continue; }
            worst = std::fmax(worst, std::fabs(static_cast<double>(g.distance) - truth) / truth);
        }

        std::printf("\n  A.3  CONTROL vs the AABB closed form, %d apart\n", apart);
        std::printf("       by more than 1 mm:\n");
        std::printf("       reported as touching anyway  %d\n", wrong_status);
        std::printf("       worst relative error  %.4e\n", worst);
        check(worst < 1e-4, "A.3 GJK matches the AABB closed form");
        check(wrong_status == 0, "A.3 no separated pair is called touching");
    }

    // ---- A.4  CONTROL: two spheres ----------------------------------------
    {
        rng r(0x8503u);
        const int n = 50000;
        int apart = 0;
        int wrong_status = 0;
        double worst = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const engine::sphere A{vec3{}, r.range(0.2f, 1.0f)};
            const engine::sphere B{r.direction() * r.range(0.5f, 5.0f), r.range(0.2f, 1.0f)};
            const double truth = dlen(dsub(promote(B.centre), promote(A.centre))) -
                                 static_cast<double>(A.radius) - static_cast<double>(B.radius);
            if (truth <= 1e-3) { continue; }
            ++apart;
            const gjk_result g = gjk_distance(as_convex(A), as_convex(B));
            if (g.status != gjk_status::separated) { ++wrong_status; continue; }
            worst = std::fmax(worst, std::fabs(static_cast<double>(g.distance) - truth) / truth);
        }

        std::printf("\n  A.4  CONTROL vs the sphere closed form, %d apart:\n", apart);
        std::printf("       reported as touching anyway  %d\n", wrong_status);
        std::printf("       worst relative error  %.4e\n", worst);
        std::printf("       (the tolerance is 1e-4 and this is a CURVED\n");
        std::printf("        shape, so this is convergence, not exactness\n");
        std::printf("        — see section F)\n");
        check(worst < 1e-3, "A.4 GJK matches the sphere closed form");
        check(wrong_status == 0, "A.4 no separated sphere pair is called touching");
    }

    // ---- A.5  CONTROL: sphere vs box, against 8.4's own answer ------------
    {
        rng r(0x8504u);
        const int n = 50000;
        int apart = 0;
        int wrong_status = 0;
        int stalled = 0;
        double worst_below = 0.0;
        double worst_false_contact = 0.0;
        double worst = 0.0;
        double worst_abs = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const shape sb = box_shape(vec3{r.range(0.3f, 1.2f), r.range(0.3f, 1.2f),
                                            r.range(0.3f, 1.2f)});
            const obb B = world_obb(sb, vec3{}, r.rotation());
            const engine::sphere S{r.direction() * r.range(0.5f, 4.0f), r.range(0.2f, 0.8f)};

            const separation s84 = collide(B, S);
            const double truth = static_cast<double>(-s84.depth);
            if (s84.hit() || truth <= 0.0) { continue; }
            const gjk_result g = gjk_distance(as_convex(B), as_convex(S));
            if (g.status != gjk_status::separated)
            {
                ++wrong_status;
                worst_false_contact = std::fmax(worst_false_contact, truth);
                continue;
            }

            const double d = static_cast<double>(g.distance);
            worst_abs = std::fmax(worst_abs, std::fabs(d - truth));
            if (g.stalled) { ++stalled; }
            worst_below = std::fmax(worst_below, truth - d);
            if (truth <= 0.01) { continue; }
            ++apart;
            worst = std::fmax(worst, std::fabs(d - truth) / truth);
        }

        std::printf("\n  A.5  CONTROL vs 8.4's sphere-box clamp.\n");
        std::printf("       %d pairs apart by more than 1 cm:\n", apart);
        std::printf("       worst relative error  %.4e\n", worst);
        std::printf("       — and THIS one is exact on 8.4's side, because a\n");
        std::printf("         sphere-box gap IS the distance: the closest\n");
        std::printf("         point is found, not bounded.\n\n");
        std::printf("       over ALL separated pairs including the hairline\n");
        std::printf("       ones, which is where GJK stops being exact:\n");
        std::printf("       reported as touching       %d\n", wrong_status);
        std::printf("       widest gap so reported     %.4e m\n", worst_false_contact);
        std::printf("       stalled (see F.5)          %d\n", stalled);
        std::printf("       worst ABSOLUTE error       %.4e m\n", worst_abs);
        std::printf("       worst shortfall vs 8.4     %.4e m\n", worst_below);
        std::printf("       — the last line is the one to read. A stalled\n");
        std::printf("         GJK returns |point_a - point_b| for two real\n");
        std::printf("         surface points, so it is still an UPPER\n");
        std::printf("         bound: the error has a direction, and it is\n");
        std::printf("         the safe one for a collision test.\n");
        check(worst < 2e-2, "A.5 GJK matches the sphere-box clamp above 1 cm");
        // NOT zero, and saying so is the point. A false CONTACT costs one
        // narrow-phase call; a false GAP costs an object through the floor, and
        // 8.4's `overlaps` made the same trade in the other direction. What has
        // to be bounded is how wide a gap can be mistaken for a touch.
        check(worst_false_contact < 0.05, "A.5 false contacts stay within a margin");
        check(worst_below < 1e-5, "A.5 no answer falls meaningfully below the truth");
    }
}

// ===========================================================================
// B  THE SUPPORT FUNCTION IS THE SHAPE
// ===========================================================================

void section_b()
{
    rule("B  THE SUPPORT FUNCTION IS THE SHAPE");

    // ---- B.1  the Minkowski identity, against a built difference ----------
    {
        rng r(0x8510u);
        const int trials = 2000;
        double worst = 0.0;
        int checked = 0;

        for (int t = 0; t < trials; ++t)
        {
            // Two small point sets, and the difference set built EXPLICITLY:
            // every a minus every b, 12 x 12 = 144 points. Then the claim
            // support_{A-B}(d) = support_A(d) - support_B(-d) is checked against
            // the maximum over that built set, which is the definition.
            std::vector<vec3> pa = make_hull_points(r, 12, 1.0f);
            std::vector<vec3> pb = make_hull_points(r, 12, 0.7f);
            const hull A = world_hull(pa, vec3{0.3f, -0.2f, 0.1f}, r.rotation());
            const hull B = world_hull(pb, vec3{-1.1f, 0.4f, 0.6f}, r.rotation());

            const vec3 d = r.direction();
            const vec3 identity_pt = support(A, d) - support(B, -d);

            float brute = -1e30f;
            for (const vec3& x : pa)
            {
                for (const vec3& y : pb)
                {
                    const vec3 diff = (A.centre + A.axes * x) - (B.centre + B.axes * y);
                    brute = std::fmax(brute, dot(diff, d));
                }
            }
            ++checked;
            worst = std::fmax(worst, std::fabs(static_cast<double>(dot(identity_pt, d)) -
                                               static_cast<double>(brute)));
        }

        std::printf("  B.1  support_{A-B}(d) = support_A(d) - support_B(-d),\n");
        std::printf("       against the difference set built by hand\n");
        std::printf("       (12 x 12 = 144 points), %d trials:\n", checked);
        std::printf("       worst absolute disagreement  %.4e m\n", worst);
        check(worst < 1e-5, "B.1 the Minkowski identity holds");
    }

    // ---- B.2  a capsule is a segment plus a ball --------------------------
    {
        rng r(0x8511u);
        const int trials = 2000;
        double worst = 0.0;
        bool all_outside = true;

        for (int t = 0; t < trials; ++t)
        {
            const shape cs = capsule_shape(r.range(0.1f, 0.8f), r.range(0.0f, 1.5f));
            const capsule C = world_capsule(cs, r.direction() * 2.0f, r.rotation());
            const vec3 d = r.direction();

            // The support point, and the maximum of dot(x, d) over a DENSE
            // sampling of the capsule's actual surface: rings around the two
            // hemispherical caps and the cylinder wall.
            const float claimed = dot(support(C, d), d);

            // An orthonormal pair perpendicular to the spine, built once.
            vec3 t0 = cross(C.axis, vec3{0.0f, 0.0f, 1.0f});
            if (length_squared(t0) < 1e-3f) { t0 = cross(C.axis, vec3{1.0f, 0.0f, 0.0f}); }
            t0 = normalised(t0);
            const vec3 t1 = cross(C.axis, t0);

            float brute = -1e30f;
            for (int i = 0; i <= 24; ++i)
            {
                const double theta = k_pi * static_cast<double>(i) / 24.0;   // 0..pi
                const double sy = std::cos(theta);
                const double sr = std::sin(theta);
                for (int j = 0; j < 32; ++j)
                {
                    const double phi = 2.0 * k_pi * static_cast<double>(j) / 32.0;
                    const vec3 off = C.axis * static_cast<float>(sy * C.radius) +
                                     t0 * static_cast<float>(sr * std::cos(phi) * C.radius) +
                                     t1 * static_cast<float>(sr * std::sin(phi) * C.radius);
                    const vec3 p0 = C.end(+1) + off;
                    const vec3 p1 = C.end(-1) + off;
                    brute = std::fmax(brute, std::fmax(dot(p0, d), dot(p1, d)));
                }
            }
            worst = std::fmax(worst, static_cast<double>(claimed - brute));
            if (claimed < brute - 1e-4f) { all_outside = false; }
        }

        std::printf("\n  B.2  capsule support vs 1,600 sampled surface points,\n");
        std::printf("       %d capsules:\n", trials);
        std::printf("       worst excess of the formula over the samples\n");
        std::printf("         %.4e m   (positive, and small: a finite\n", worst);
        std::printf("         sampling can only UNDERSHOOT the true max)\n");
        check(all_outside, "B.2 the formula never lands inside the surface");
    }

    // ---- B.3  interior points are invisible to a support function ---------
    {
        rng r(0x8512u);
        const int trials = 5000;
        int differed = 0;
        double worst = 0.0;

        for (int t = 0; t < trials; ++t)
        {
            std::vector<vec3> outer = make_hull_points(r, 16, 1.0f);
            std::vector<vec3> padded = outer;
            for (int k = 0; k < 40; ++k)
            {
                // A random CONVEX COMBINATION of the outer points, which is
                // inside their hull by the definition of a hull rather than by
                // a radius argument. The first draft of this test used points
                // inside a sphere of radius 0.3 and it FAILED once in 5,000:
                // sixteen random points on a sphere can cluster, and when they
                // do the inscribed radius of their hull is nowhere near 0.3.
                // The point that escaped changed a support value by 0.16 m.
                vec3 p{};
                float total = 0.0f;
                for (const vec3& q : outer)
                {
                    const float wt = r.unit();
                    p += q * wt;
                    total += wt;
                }
                padded.push_back(p / total);
            }
            const hull A{vec3{}, mat3::identity(), outer};
            const hull B{vec3{}, mat3::identity(), padded};

            const vec3 d = r.direction();
            const float sa = dot(support(A, d), d);
            const float sb = dot(support(B, d), d);
            if (std::fabs(sa - sb) > 1e-5f) { ++differed; }
            worst = std::fmax(worst, std::fabs(static_cast<double>(sa - sb)));
        }

        std::printf("\n  B.3  40 interior points added to a 16-point hull,\n");
        std::printf("       %d trials:\n", trials);
        std::printf("       support values that changed  %d\n", differed);
        std::printf("       worst change                 %.4e m\n", worst);
        std::printf("       — so `hull` need not BE a hull. It is the\n");
        std::printf("         support function of its own convex hull\n");
        std::printf("         whatever you put in it, and the extra points\n");
        std::printf("         cost time and nothing else.\n");
        check(differed == 0, "B.3 interior points change no support value");
    }

    // ---- B.4  CONTROL: a capsule of zero height IS a sphere ---------------
    {
        rng r(0x8513u);
        const int trials = 20000;
        int identical = 0;
        double worst = 0.0;

        for (int t = 0; t < trials; ++t)
        {
            const float rad = r.range(0.2f, 1.5f);
            const shape cs = capsule_shape(rad, 0.0f);
            const capsule C = world_capsule(cs, vec3{}, r.rotation());
            const engine::sphere S{vec3{}, rad};
            const vec3 d = r.direction();

            const vec3 pc = support(C, d);
            const vec3 ps = support(S, d);
            if (pc == ps) { ++identical; }
            worst = std::fmax(worst, dlen(dsub(promote(pc), promote(ps))));
        }

        std::printf("\n  B.4  CONTROL a capsule with half_height 0, against\n");
        std::printf("       the sphere of the same radius, %d trials:\n", trials);
        std::printf("       bit-identical support points  %d / %d\n", identical, trials);
        std::printf("       worst distance apart          %.4e m\n", worst);
        check(identical == trials, "B.4 a zero-height capsule is exactly a sphere");
    }
}

// ===========================================================================
// C  THE SIMPLEX SOLVER
// ===========================================================================

void section_c()
{
    rule("C  THE SIMPLEX SOLVER");

    // ---- C.1  the weights are barycentric, and they reconstruct -----------
    {
        rng r(0x8520u);
        const int trials = 200000;
        double worst_sum = 0.0;
        double worst_recon = 0.0;
        double worst_negative = 0.0;

        for (int t = 0; t < trials; ++t)
        {
            simplex s;
            const int n = 1 + static_cast<int>(r.next() % 4u);
            for (int i = 0; i < n; ++i)
            {
                gjk_vertex v;
                v.w = vec3{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)};
                v.pa = v.w;
                v.pb = vec3{};
                s.push(v);
            }

            vec3 closest{};
            float w[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            const bool inside = reduce_simplex(s, closest, w);

            double sum = 0.0;
            dvec3 recon{};
            for (int i = 0; i < s.count; ++i)
            {
                sum += static_cast<double>(w[i]);
                recon = dadd(recon, dmul(promote(s.v[i].w), static_cast<double>(w[i])));
                worst_negative = std::fmin(worst_negative, static_cast<double>(w[i]));
            }
            worst_sum = std::fmax(worst_sum, std::fabs(sum - 1.0));
            if (!inside)
            {
                worst_recon = std::fmax(worst_recon, dlen(dsub(recon, promote(closest))));
            }
        }

        std::printf("  C.1  %d random simplices of 1 to 4 points:\n", trials);
        std::printf("       worst |sum of weights - 1|     %.4e\n", worst_sum);
        std::printf("       most negative weight           %.4e\n", worst_negative);
        std::printf("       worst |sum w_i v_i - closest|  %.4e m\n", worst_recon);
        std::printf("       — the second line is what makes the FIRST\n");
        std::printf("         one mean something: weights that sum to 1\n");
        std::printf("         but go negative are an AFFINE combination,\n");
        std::printf("         which lands outside the simplex.\n");
        check(worst_sum < 1e-5, "C.1 weights sum to one");
        check(worst_negative > -1e-6, "C.1 weights are non-negative");
        check(worst_recon < 1e-5, "C.1 the weights reconstruct the closest point");
    }

    // ---- C.2  the triangle case, against a dense sampling -----------------
    {
        rng r(0x8521u);
        const int trials = 4000;
        double worst = 0.0;
        int solver_lower = 0;

        for (int t = 0; t < trials; ++t)
        {
            const vec3 a{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)};
            const vec3 b{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)};
            const vec3 c{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)};

            simplex s;
            s.push(gjk_vertex{a, a, vec3{}});
            s.push(gjk_vertex{b, b, vec3{}});
            s.push(gjk_vertex{c, c, vec3{}});
            vec3 closest{};
            float w[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            (void)reduce_simplex(s, closest, w);
            const double solved = dlen(promote(closest));

            // Every barycentric point on a 60 x 60 lattice.
            double brute = 1e30;
            for (int i = 0; i <= 60; ++i)
            {
                for (int j = 0; i + j <= 60; ++j)
                {
                    const double u = static_cast<double>(i) / 60.0;
                    const double v = static_cast<double>(j) / 60.0;
                    const dvec3 p = dadd(dmul(promote(a), 1.0 - u - v),
                                         dadd(dmul(promote(b), u), dmul(promote(c), v)));
                    brute = std::fmin(brute, dlen(p));
                }
            }
            if (solved <= brute + 1e-6) { ++solver_lower; }
            worst = std::fmax(worst, solved - brute);
        }

        std::printf("\n  C.2  triangle closest point vs a 1,891-point\n");
        std::printf("       barycentric lattice, %d triangles:\n", trials);
        std::printf("       solver never worse than the lattice: %d / %d\n",
                    solver_lower, trials);
        std::printf("       worst excess over the lattice  %.4e m\n", worst);
        check(solver_lower == trials, "C.2 the solver beats a dense lattice every time");
    }

    // ---- C.3  the coplanar tetrahedron, and the guard that catches it -----
    {
        // Four points on the plane y = 1, with the origin below it. A flat
        // tetrahedron has no interior, so nothing can be inside it — but the
        // "which side is the fourth vertex on?" test has no side to report,
        // because the fourth vertex is ON every face's plane.
        simplex s;
        s.push(gjk_vertex{vec3{-1.0f, 1.0f, -1.0f}, vec3{}, vec3{}});
        s.push(gjk_vertex{vec3{ 1.0f, 1.0f, -1.0f}, vec3{}, vec3{}});
        s.push(gjk_vertex{vec3{ 1.0f, 1.0f,  1.0f}, vec3{}, vec3{}});
        s.push(gjk_vertex{vec3{-1.0f, 1.0f,  1.0f}, vec3{}, vec3{}});

        vec3 closest{};
        float w[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        const bool inside = reduce_simplex(s, closest, w);

        std::printf("\n  C.3  a FLAT tetrahedron: four points on y = 1,\n");
        std::printf("       origin one metre below\n");
        std::printf("       reported inside?  %s   (must be no)\n", inside ? "yes" : "NO");
        std::printf("       closest point (%.4f, %.4f, %.4f), |v| = %.6f\n",
                    static_cast<double>(closest.x), static_cast<double>(closest.y),
                    static_cast<double>(closest.z), dlen(promote(closest)));
        std::printf("       reduced to %d vertices\n", s.count);
        check(!inside, "C.3 a degenerate tetrahedron encloses nothing");
        check(std::fabs(dlen(promote(closest)) - 1.0) < 1e-5, "C.3 the distance is 1 m");

        // CONTROL: the same test WITHOUT the degeneracy guard. `sd` is exactly
        // zero for every face, `sp * sd` is exactly zero, `0 < 0` is false, so
        // every face reports "not outside" and the origin is declared INSIDE a
        // tetrahedron with no volume.
        int faces_called_inside = 0;
        static constexpr int k_faces[4][3] = {{0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}};
        static constexpr int k_opp[4] = {3, 2, 1, 0};
        const vec3 pts[4] = {vec3{-1.0f, 1.0f, -1.0f}, vec3{1.0f, 1.0f, -1.0f},
                             vec3{1.0f, 1.0f, 1.0f}, vec3{-1.0f, 1.0f, 1.0f}};
        for (int f = 0; f < 4; ++f)
        {
            const vec3 A = pts[k_faces[f][0]];
            const vec3 B = pts[k_faces[f][1]];
            const vec3 C = pts[k_faces[f][2]];
            const vec3 D = pts[k_opp[f]];
            const vec3 nrm = cross(B - A, C - A);
            const float sp = dot(vec3{} - A, nrm);
            const float sd = dot(D - A, nrm);
            if (!(sp * sd < 0.0f)) { ++faces_called_inside; }
        }
        std::printf("       CONTROL without the guard, faces reporting\n");
        std::printf("         \"origin not outside\":  %d of 4\n", faces_called_inside);
        std::printf("       — 4 of 4 is the bug: the shapes would be\n");
        std::printf("         reported as touching when they are a metre\n");
        std::printf("         apart, and 8.4's SAT could never do this\n");
        std::printf("         because it never builds a tetrahedron.\n");
        check(faces_called_inside == 4, "C.3 CONTROL the unguarded test does fail");
    }

    // ---- C.5  the textbook triangle solver, as a control ------------------
    {
        // Ericson's `ClosestPtPointTriangle`, written out here rather than
        // shipped: six dot products and three signed areas decide which of seven
        // Voronoi regions the origin is in, and only that region is evaluated.
        // It is the version every implementation copies. It is also the version
        // this lesson's first draft shipped, and §C's opening paragraph explains
        // why it does not any more.
        auto ericson = [](vec3 a, vec3 b, vec3 c) -> vec3 {
            const vec3 ab = b - a;
            const vec3 ac = c - a;
            const float d1 = dot(ab, -a);
            const float d2 = dot(ac, -a);
            if (d1 <= 0.0f && d2 <= 0.0f) { return a; }
            const float d3 = dot(ab, -b);
            const float d4 = dot(ac, -b);
            if (d3 >= 0.0f && d4 <= d3) { return b; }
            const float vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) { return a + ab * (d1 / (d1 - d3)); }
            const float d5 = dot(ab, -c);
            const float d6 = dot(ac, -c);
            if (d6 >= 0.0f && d5 <= d6) { return c; }
            const float vb = d5 * d2 - d1 * d6;
            if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) { return a + ac * (d2 / (d2 - d6)); }
            const float va = d3 * d6 - d5 * d4;
            if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
            {
                const float t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                return b + (c - b) * t;
            }
            const float denom = 1.0f / (va + vb + vc);
            return a + ab * (vb * denom) + ac * (vc * denom);
        };

        // Two populations. FAT triangles are what a textbook test uses; THIN
        // ones are what GJK actually produces, because the algorithm's whole job
        // is to drive the simplex onto the closest feature.
        for (int pop = 0; pop < 2; ++pop)
        {
            rng r(0x8523u + static_cast<std::uint32_t>(pop));
            const int trials = 200000;
            int worse = 0;
            double worst_excess = 0.0;
            double worst_rel = 0.0;

            for (int t = 0; t < trials; ++t)
            {
                vec3 a{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)};
                vec3 b{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)};
                vec3 c{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)};
                if (pop == 1)
                {
                    // THE GEOMETRY GJK ACTUALLY PRODUCES, and it takes two
                    // steps to build. First squash `c` onto the `ab` line, so
                    // the triangle is a sliver — that is what a simplex looks
                    // like once the search has driven it onto the closest
                    // feature. Then TRANSLATE the whole triangle so that its
                    // closest point sits a fraction of a millimetre from the
                    // origin, because in GJK the origin is never far: |v| is the
                    // gap, and the gap is small by the last iteration.
                    const vec3 ab = b - a;
                    const float u = r.range(0.0f, 1.0f);
                    const float squash = std::pow(10.0f, r.range(-7.0f, -4.0f));
                    c = a + ab * u + (c - (a + ab * u)) * squash;

                    simplex probe;
                    probe.push(gjk_vertex{a, vec3{}, vec3{}});
                    probe.push(gjk_vertex{b, vec3{}, vec3{}});
                    probe.push(gjk_vertex{c, vec3{}, vec3{}});
                    vec3 q{};
                    float qw[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                    (void)reduce_simplex(probe, q, qw);

                    const vec3 shift = r.direction() * std::pow(10.0f, r.range(-5.0f, -2.0f)) - q;
                    a += shift;
                    b += shift;
                    c += shift;
                }

                simplex s3;
                s3.push(gjk_vertex{a, a, vec3{}});
                s3.push(gjk_vertex{b, b, vec3{}});
                s3.push(gjk_vertex{c, c, vec3{}});
                vec3 mine{};
                float w[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                (void)reduce_simplex(s3, mine, w);

                const vec3 theirs = ericson(a, b, c);
                const double dm = dlen(promote(mine));
                const double dt = dlen(promote(theirs));
                if (dt < dm * 0.999 - 1e-9)
                {
                    // The textbook version claims a CLOSER point. On a triangle
                    // that is only possible if its point is not on the triangle.
                    ++worse;
                    worst_excess = std::fmax(worst_excess, dm - dt);
                    worst_rel = std::fmax(worst_rel, (dm - dt) / std::fmax(dm, 1e-9));
                }
            }

            std::printf("\n  C.5  %s, %d of them:\n",
                        pop == 0 ? "FAT triangles, origin anywhere"
                                 : "SLIVERS with the origin 10um-10mm away",
                        trials);
            std::printf("       textbook answer claims to beat ours  %d\n", worse);
            std::printf("       worst claimed improvement  %.4e m\n", worst_excess);
            std::printf("       worst relative             %.4e\n", worst_rel);
        }
        std::printf("       — a triangle's closest point cannot be beaten\n");
        std::printf("         by another point OF THE TRIANGLE, so every\n");
        std::printf("         one of those is the textbook version\n");
        std::printf("         returning a point that is not on it. On fat\n");
        std::printf("         triangles it never happens, which is why the\n");
        std::printf("         bug survives every test suite written\n");
        std::printf("         against uniform random data — and thin\n");
        std::printf("         triangles are not an edge case in GJK, they\n");
        std::printf("         are the last two iterations of every query.\n");
    }

    // ---- C.6  and what refusing to decide costs ---------------------------
    {
        rng r(0x8524u);
        const std::size_t m = 200000;
        std::vector<vec3> pts;
        pts.reserve(m * 3);
        for (std::size_t i = 0; i < m * 3; ++i)
        {
            pts.push_back(vec3{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)});
        }

        const bench_result mine = bench_run(m, 9, [&]() {
            double acc = 0.0;
            for (std::size_t i = 0; i < m; ++i)
            {
                simplex s3;
                s3.push(gjk_vertex{pts[i * 3 + 0], vec3{}, vec3{}});
                s3.push(gjk_vertex{pts[i * 3 + 1], vec3{}, vec3{}});
                s3.push(gjk_vertex{pts[i * 3 + 2], vec3{}, vec3{}});
                vec3 q{};
                float w[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                (void)reduce_simplex(s3, q, w);
                acc += static_cast<double>(q.x);
            }
            return acc;
        });

        std::printf("\n  C.6  four candidates, evaluated and compared:\n");
        std::printf("       %.3f ns per triangle  (spread %.2f)\n",
                    mine.median_ns, mine.spread());
        std::printf("       — against roughly 115 ns for the whole box-box\n");
        std::printf("         query in I.1, of which this runs two or\n");
        std::printf("         three times. Refusing to decide is a few\n");
        std::printf("         percent of a collision test, and it buys an\n");
        std::printf("         answer that cannot be wrong.\n");
    }

    // ---- C.4  what 8.6 will actually be handed ---------------------------
    {
        // 8.6's EPA expands the simplex GJK leaves behind, and every description
        // of EPA begins "start from the tetrahedron GJK terminated with". This
        // measures whether it gets one.
        const int n = 100000;
        for (int tilted = 0; tilted < 2; ++tilted)
        {
            rng r(0x8522u);
            int counts[5] = {0, 0, 0, 0, 0};
            int deep = 0;
            int deep_counts[5] = {0, 0, 0, 0, 0};
            int y_zero = 0;

            for (int i = 0; i < n; ++i)
            {
                // 8.4 §F's arrangement, and it called it the default one for a
                // reason: two objects standing on the same floor share an up
                // axis whatever their yaw.
                const shape sa = box_shape(vec3{0.5f, 0.5f, 0.5f});
                const shape sb = box_shape(vec3{0.5f, 0.5f, 0.5f});
                quat qa = quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f}, r.range(0.0f, 6.283f));
                quat qb = quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f}, r.range(0.0f, 6.283f));
                if (tilted)
                {
                    // One degree of tilt on one of them. That is the control.
                    qb = normalised(quat_from_axis_angle(vec3{1.0f, 0.0f, 0.0f},
                                                         1.0f * 3.14159265f / 180.0f) * qb);
                }
                const obb A = world_obb(sa, vec3{}, qa);
                const obb B = world_obb(sb, vec3{r.range(-1.2f, 1.2f), 0.0f, r.range(-1.2f, 1.2f)},
                                        qb);

                const gjk_result g = gjk_distance(as_convex(A), as_convex(B));
                ++counts[g.terminal.count];

                bool all_flat = true;
                for (int k = 0; k < g.terminal.count; ++k)
                {
                    if (std::fabs(static_cast<double>(g.terminal.v[k].w.y)) > 1e-7) { all_flat = false; }
                }
                if (all_flat) { ++y_zero; }

                const separation sat = collide(A, B);
                if (sat.hit() && sat.depth > 0.1f)
                {
                    ++deep;
                    ++deep_counts[g.terminal.count];
                }
            }

            std::printf("\n  C.4  %s, %d pairs\n",
                        tilted ? "CONTROL the same, one box tilted 1 degree"
                               : "two boxes on the same floor, any yaw",
                        n);
            std::printf("       terminal simplex   1:%6d  2:%6d\n", counts[1], counts[2]);
            std::printf("                          3:%6d  4:%6d\n", counts[3], counts[4]);
            std::printf("       DEEP overlaps (MTV > 10 cm): %d\n", deep);
            std::printf("         of those, tetrahedra:  %d\n", deep_counts[4]);
            std::printf("       simplices lying entirely in y = 0:  %d\n", y_zero);
        }

        std::printf("\n       THE SEARCH IS TWO-DIMENSIONAL AND NOTHING\n");
        std::printf("       IN IT KNOWS THAT. Both boxes have the same up\n");
        std::printf("       axis, so their y half-extents are equal and\n");
        std::printf("       `support_local` picks the same sign on that\n");
        std::printf("       axis for both — every vertex of the difference\n");
        std::printf("       comes out with y EXACTLY zero. The simplex is\n");
        std::printf("       confined to a plane, so it can never be a\n");
        std::printf("       tetrahedron, and the origin is found inside a\n");
        std::printf("       TRIANGLE.\n\n");
        std::printf("       This is 8.4 F's finding again from the other\n");
        std::printf("       side. There a shared axis made one of the nine\n");
        std::printf("       cross products degenerate; here it collapses\n");
        std::printf("       the whole search by a dimension. A shared axis\n");
        std::printf("       is not a coincidence in a game — it is what\n");
        std::printf("       \"standing on the floor\" means — and both\n");
        std::printf("       algorithms have to survive it.\n\n");
        std::printf("       THE CONSEQUENCE IS 8.6'S, and it is worth\n");
        std::printf("       naming now: EPA is always described as\n");
        std::printf("       starting from the tetrahedron GJK terminated\n");
        std::printf("       with, and on the commonest arrangement in a\n");
        std::printf("       game there is no tetrahedron. 8.6 will have to\n");
        std::printf("       build one.\n");
    }

}

// ===========================================================================
// D  DOES IT AGREE WITH THE SAT ABOUT THE SIGN?
// ===========================================================================

void section_d()
{
    rule("D  DOES IT AGREE WITH THE SAT ABOUT THE SIGN?");

    // ---- D.1  gjk_intersects vs overlaps, 200,000 pairs -------------------
    {
        rng r(0x8530u);
        const int n = 200000;
        int overlapping = 0;
        int disagree = 0;
        double worst_disagreement_gap = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const shape sa = box_shape(vec3{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f),
                                            r.range(0.2f, 1.2f)});
            const shape sb = box_shape(vec3{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f),
                                            r.range(0.2f, 1.2f)});
            const obb A = world_obb(sa, vec3{}, r.rotation());
            const obb B = world_obb(sb, r.direction() * r.range(0.3f, 3.0f), r.rotation());

            const bool sat = overlaps(A, B);
            const bool gjk = gjk_intersects(as_convex(A), as_convex(B));
            if (sat) { ++overlapping; }
            if (sat != gjk)
            {
                ++disagree;
                const gjk_result g = gjk_distance(as_convex(A), as_convex(B));
                worst_disagreement_gap =
                    std::fmax(worst_disagreement_gap, static_cast<double>(g.distance));
            }
        }

        std::printf("  D.1  %d random OBB pairs\n", n);
        std::printf("       SAT says overlapping        %d\n", overlapping);
        std::printf("       GJK disagrees with the SAT  %d\n", disagree);
        std::printf("       worst true gap at a disagreement  %.4e m\n",
                    worst_disagreement_gap);
        std::printf("       — the two algorithms share no code, no axis\n");
        std::printf("         list and no arithmetic. Agreement here is\n");
        std::printf("         two independent proofs of the same fact.\n");
        check(disagree == 0 || worst_disagreement_gap < 1e-4,
              "D.1 any disagreement is a grazing contact");
    }

    // ---- D.2  the shipped collide, both ways ------------------------------
    {
        rng r(0x8531u);
        const int n = 100000;
        int disagree = 0;

        for (int i = 0; i < n; ++i)
        {
            const shape sa = box_shape(vec3{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f),
                                            r.range(0.2f, 1.2f)});
            const shape sb = box_shape(vec3{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f),
                                            r.range(0.2f, 1.2f)});
            const obb A = world_obb(sa, vec3{}, r.rotation());
            const obb B = world_obb(sb, r.direction() * r.range(0.3f, 3.0f), r.rotation());

            const separation sat = collide(A, B);
            const separation gjk = collide(as_convex(A), as_convex(B));
            const bool sat_hit = sat.hit();
            const bool gjk_hit = gjk.depth >= 0.0f;
            if (sat_hit != gjk_hit) { ++disagree; }
        }

        std::printf("\n  D.2  the same question through the two `collide`\n");
        std::printf("       overloads, %d pairs:  %d disagreements\n", n, disagree);
        std::printf("       — and note what the GENERAL overload gives up:\n");
        std::printf("         `hit()` is true on a +0 depth, so a caller\n");
        std::printf("         that reads `depth` rather than the status\n");
        std::printf("         gets a penetration of zero on every overlap.\n");
        std::printf("         Named in the header, fixed in 8.6.\n");
    }

    // ---- D.3  shapes the SAT cannot do at all -----------------------------
    {
        rng r(0x8532u);
        const int n = 50000;
        int apart = 0;
        int certified = 0;
        int stalled = 0;
        int stalled_above_1cm = 0;
        double worst_slack = 0.0;
        double worst_slack_clean = 0.0;
        double worst_slack_abs = 0.0;
        double worst_viol = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const shape ca = capsule_shape(r.range(0.1f, 0.5f), r.range(0.2f, 1.0f));
            const shape cb = capsule_shape(r.range(0.1f, 0.5f), r.range(0.2f, 1.0f));
            const capsule A = world_capsule(ca, vec3{}, r.rotation());
            const capsule B = world_capsule(cb, r.direction() * r.range(0.5f, 3.0f), r.rotation());

            const gjk_result g = gjk_distance(as_convex(A), as_convex(B));
            if (g.status != gjk_status::separated || g.distance < 1e-4f) { continue; }
            ++apart;
            if (g.stalled) { ++stalled; }

            const gjk_bounds bd = certify(as_convex(A), as_convex(B), g);
            const double slack = static_cast<double>(bd.slack());
            if (g.distance > 0.01f)
            {
                worst_slack = std::fmax(worst_slack, slack / static_cast<double>(g.distance));
                if (g.stalled) { stalled_above_1cm += 1; }
                else { worst_slack_clean = std::fmax(worst_slack_clean,
                                                     slack / static_cast<double>(g.distance)); }
            }
            worst_slack_abs = std::fmax(worst_slack_abs, slack);
            const double d = static_cast<double>(g.distance);
            const double viol = std::fmax(static_cast<double>(bd.lower) - d,
                                          d - static_cast<double>(bd.upper));
            worst_viol = std::fmax(worst_viol, viol);
            if (viol <= 1e-3 * d + 1e-5) { ++certified; }
        }

        std::printf("\n  D.3  two CAPSULES, which the SAT cannot test at\n");
        std::printf("       all (a capsule has no finite face list):\n");
        std::printf("       %d apart, %d of them certified\n", apart, certified);
        std::printf("       stalled  %d   (of those, above 1 cm: %d)\n", stalled, stalled_above_1cm);
        std::printf("       worst bracket violation  %.4e m\n", worst_viol);
        std::printf("       worst ABSOLUTE slack     %.4e m\n", worst_slack_abs);
        std::printf("       worst RELATIVE slack above 1 cm\n");
        std::printf("         all results      %.4e\n", worst_slack);
        std::printf("         excluding stalls %.4e\n", worst_slack_clean);
        std::printf("       — the split is the finding. A capsule is the\n");
        std::printf("         one shape here that genuinely CONVERGES\n");
        std::printf("         rather than finishing (F.2), so it is also\n");
        std::printf("         the one that can run out of float before it\n");
        std::printf("         runs out of tolerance.\n");
        check(worst_viol < 1e-4, "D.3 capsule brackets hold to 0.1 mm");
    }

    // ---- D.4  a hull against a box, by certificate ------------------------
    {
        rng r(0x8533u);
        const int n = 20000;
        int apart = 0;
        int certified = 0;
        double worst_slack = 0.0;

        for (int i = 0; i < n; ++i)
        {
            std::vector<vec3> pts = make_hull_points(r, 20, r.range(0.3f, 0.9f));
            const hull H = world_hull(pts, vec3{}, r.rotation());
            const shape sb = box_shape(vec3{r.range(0.2f, 1.0f), r.range(0.2f, 1.0f),
                                            r.range(0.2f, 1.0f)});
            const obb B = world_obb(sb, r.direction() * r.range(0.5f, 3.0f), r.rotation());

            const gjk_result g = gjk_distance(as_convex(H), as_convex(B));
            if (g.status != gjk_status::separated || g.distance < 1e-4f) { continue; }
            ++apart;
            const gjk_bounds bd = certify(as_convex(H), as_convex(B), g);
            const double slack = static_cast<double>(bd.slack());
            if (g.distance > 0.01f)
            {
                worst_slack = std::fmax(worst_slack, slack / static_cast<double>(g.distance));
            }
            if (bd.lower <= g.distance + 1e-5f && g.distance <= bd.upper + 1e-5f) { ++certified; }
        }

        std::printf("\n  D.4  a 20-vertex HULL against a box, %d apart:\n", apart);
        std::printf("       certified  %d\n", certified);
        std::printf("       worst RELATIVE slack above 1 cm  %.4e\n", worst_slack);
        std::printf("       — one algorithm, four shape types, ten pairings,\n");
        std::printf("         and gjk.cpp names none of them.\n");
        check(certified == apart, "D.4 every hull-box gap certifies");
    }
}

// ===========================================================================
// E  THE CERTIFICATE: TWO BOUNDS THAT SQUEEZE
// ===========================================================================

void section_e()
{
    rule("E  THE CERTIFICATE: TWO BOUNDS THAT SQUEEZE");

    rng r(0x8540u);
    const int n = 200000;
    int apart = 0;
    int bracketed = 0;
    double worst_slack_abs = 0.0;
    double worst_slack_rel = 0.0;
    double worst_below = 0.0;
    double worst_above = 0.0;

    std::vector<obb> keep_a;
    std::vector<obb> keep_b;
    std::vector<gjk_result> keep_g;

    for (int i = 0; i < n; ++i)
    {
        const shape sa = box_shape(vec3{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f),
                                        r.range(0.2f, 1.2f)});
        const shape sb = box_shape(vec3{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f),
                                        r.range(0.2f, 1.2f)});
        const obb A = world_obb(sa, vec3{}, r.rotation());
        const obb B = world_obb(sb, r.direction() * r.range(0.3f, 4.0f), r.rotation());

        const gjk_result g = gjk_distance(as_convex(A), as_convex(B));
        if (g.status != gjk_status::separated || g.distance < 1e-4f) { continue; }
        ++apart;

        // IN DOUBLE, from the box data and the claimed direction only.
        const dbounds bd = certify_double(A, B, g);
        const double d = static_cast<double>(g.distance);

        // Counted with a tolerance PROPORTIONAL to the shapes, because
        // `certify_double` reads float box data and its own inputs carry half an
        // ulp — about 6e-8 m per term at metre scale. The MAGNITUDES below are
        // the real result; the count is a convenience.
        const double slop = 1e-5 * std::sqrt(static_cast<double>(length_squared(B.centre)) + 4.0);
        if (bd.lower <= d + slop && d <= bd.upper + slop) { ++bracketed; }
        worst_below = std::fmax(worst_below, bd.lower - d);
        worst_above = std::fmax(worst_above, d - bd.upper);

        const double slack = bd.upper - bd.lower;
        worst_slack_abs = std::fmax(worst_slack_abs, slack);
        if (d > 0.01) { worst_slack_rel = std::fmax(worst_slack_rel, slack / d); }

        if (keep_a.size() < 20000u) { keep_a.push_back(A); keep_b.push_back(B); keep_g.push_back(g); }
    }

    std::printf("  E.1  %d random OBB pairs, %d of them apart\n", n, apart);
    std::printf("       lower <= reported <= upper:  %d / %d\n", bracketed, apart);
    std::printf("       worst violation below  %.4e m\n", worst_below);
    std::printf("       worst violation above  %.4e m\n", worst_above);
    std::printf("       — both are single-ulp quantities at metre scale,\n");
    std::printf("         and they are the honest answer rather than a\n");
    std::printf("         zero: `certify_double` is exact arithmetic on\n");
    std::printf("         INEXACT INPUTS, so it brackets the answer to\n");
    std::printf("         the precision of the box data and no further.\n");
    std::printf("\n  E.2  how tight the proof is:\n");
    std::printf("       worst ABSOLUTE slack             %.4e m\n", worst_slack_abs);
    std::printf("       worst RELATIVE slack above 1 cm  %.4e\n", worst_slack_rel);
    std::printf("       — the band matters and saying so is the point.\n");
    std::printf("         Below a centimetre the relative slack runs\n");
    std::printf("         away, because the numerator is bounded by\n");
    std::printf("         float and the denominator is not. F.5 measures\n");
    std::printf("         that floor directly; here it would only look\n");
    std::printf("         like an outlier.\n");
    check(bracketed == apart, "E.1 every reported distance is bracketed");
    check(worst_below < 1e-5 && worst_above < 1e-5,
          "E.1 bracket violations stay at float's noise floor");

    // ---- E.3  CONTROL: a wrong direction fails to certify -----------------
    {
        int still_bracketed = 0;
        double worst_slack = 0.0;
        const int m = static_cast<int>(keep_g.size());

        for (int i = 0; i < m; ++i)
        {
            gjk_result bad = keep_g[static_cast<std::size_t>(i)];
            // Tilt the reported direction by five degrees about an arbitrary
            // axis. The DISTANCE is untouched — only the claimed direction is
            // wrong, which is precisely the bug a certificate must catch.
            vec3 perp = cross(bad.direction, vec3{0.0f, 1.0f, 0.0f});
            if (length_squared(perp) < 1e-3f) { perp = cross(bad.direction, vec3{1.0f, 0.0f, 0.0f}); }
            const quat tilt = quat_from_axis_angle(normalised(perp), 5.0f * 3.14159265f / 180.0f);
            bad.direction = rotate(tilt, bad.direction);

            const dbounds bd = certify_double(keep_a[static_cast<std::size_t>(i)],
                                              keep_b[static_cast<std::size_t>(i)], bad);
            const double d = static_cast<double>(bad.distance);
            if (bd.lower <= d + 1e-6 && d <= bd.upper + 1e-6) { ++still_bracketed; }
            worst_slack = std::fmax(worst_slack, bd.upper - bd.lower);
        }

        std::printf("\n  E.3  CONTROL the same results with the direction\n");
        std::printf("       tilted 5 degrees, %d pairs:\n", m);
        std::printf("       still bracketed  %d  (%.1f%%)\n",
                    still_bracketed, 100.0 * still_bracketed / m);
        std::printf("       worst slack      %.4e m   (was %.4e)\n",
                    worst_slack, worst_slack_abs);
        std::printf("       slack grew by    %.0fx\n", worst_slack / worst_slack_abs);
        std::printf("       — the bracket still HOLDS, and it must: a\n");
        std::printf("         worse lower bound is still a lower bound,\n");
        std::printf("         so no certificate can ever be violated by a\n");
        std::printf("         wrong direction. What it loses is its\n");
        std::printf("         POWER. The upper bound does not move — the\n");
        std::printf("         witnesses are still real points — while the\n");
        std::printf("         lower one collapses, and a proof that\n");
        std::printf("         brackets the answer between 0 and 0.38 m is\n");
        std::printf("         not a proof of anything. THAT is what a\n");
        std::printf("         certificate detects: not a false answer,\n");
        std::printf("         but an unproven one.\n");
        check(worst_slack > 10.0 * worst_slack_abs, "E.3 a tilted direction loses the proof");
    }

    // ---- E.4  the witness points are ON the shapes ------------------------
    {
        double worst_a = 0.0;
        double worst_b = 0.0;
        const int m = static_cast<int>(keep_g.size());

        for (int i = 0; i < m; ++i)
        {
            const obb& A = keep_a[static_cast<std::size_t>(i)];
            const obb& B = keep_b[static_cast<std::size_t>(i)];
            const gjk_result& g = keep_g[static_cast<std::size_t>(i)];
            const dvec3 n = promote(g.direction);

            // A supporting plane touches A at point_a: nothing in A reaches
            // further along +n than point_a does.
            const double reach_a = ddot(support_double(A, n), n);
            const double at_a = ddot(promote(g.point_a), n);
            worst_a = std::fmax(worst_a, at_a - reach_a);

            const double reach_b = ddot(support_double(B, dmul(n, -1.0)), dmul(n, -1.0));
            const double at_b = ddot(promote(g.point_b), dmul(n, -1.0));
            worst_b = std::fmax(worst_b, at_b - reach_b);
        }

        std::printf("\n  E.4  the witnesses sit on the supporting planes,\n");
        std::printf("       %d pairs:\n", m);
        std::printf("       worst overshoot on A  %.4e m\n", worst_a);
        std::printf("       worst overshoot on B  %.4e m\n", worst_b);
        check(worst_a < 1e-4 && worst_b < 1e-4, "E.4 the witnesses are on the surfaces");
    }
}

// ===========================================================================
// F  POLYTOPES TERMINATE, CURVES CONVERGE
// ===========================================================================

void section_f()
{
    rule("F  POLYTOPES TERMINATE, CURVES CONVERGE");

    // ---- F.1  the iteration histogram for two boxes -----------------------
    {
        rng r(0x8550u);
        const int n = 200000;
        int hist[16] = {0};
        int over = 0;
        int max_it = 0;
        long long total = 0;
        int apart = 0;

        for (int i = 0; i < n; ++i)
        {
            const shape sa = box_shape(vec3{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f),
                                            r.range(0.2f, 1.2f)});
            const shape sb = box_shape(vec3{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f),
                                            r.range(0.2f, 1.2f)});
            const obb A = world_obb(sa, vec3{}, r.rotation());
            const obb B = world_obb(sb, r.direction() * r.range(0.3f, 4.0f), r.rotation());
            const gjk_result g = gjk_distance(as_convex(A), as_convex(B));
            if (g.status != gjk_status::separated) { continue; }
            ++apart;
            total += g.iterations;
            max_it = std::max(max_it, g.iterations);
            if (g.iterations < 16) { ++hist[g.iterations]; } else { ++over; }
        }

        std::printf("  F.1  %d separated box pairs, iterations:\n", apart);
        for (int k = 1; k < 12; ++k)
        {
            if (hist[k] == 0) { continue; }
            const double pct = 100.0 * hist[k] / apart;
            std::printf("       %2d  %7d  %5.1f%%  ", k, hist[k], pct);
            const int bars = static_cast<int>(pct / 3.0 + 0.5);
            for (int b = 0; b < bars; ++b) { std::putchar('#'); }
            std::putchar('\n');
        }
        std::printf("       mean %.2f   max %d   over 15: %d\n",
                    static_cast<double>(total) / apart, max_it, over);
        check(max_it <= 16, "F.1 box-box terminates quickly");
    }

    // ---- F.2  iterations against tolerance, three shape pairs -------------
    {
        const float tols[6] = {1e-2f, 1e-3f, 1e-4f, 1e-5f, 1e-6f, 1e-7f};
        std::printf("\n  F.2  mean iterations as the tolerance tightens,\n");
        std::printf("       2,000 pairs each:\n");
        std::printf("       tol      box-box   sph-sph   cap-cap   hull-hull\n");

        for (int ti = 0; ti < 6; ++ti)
        {
            gjk_config cfg;
            cfg.tolerance = tols[ti];
            cfg.max_iterations = 200;

            double sums[4] = {0.0, 0.0, 0.0, 0.0};
            int counts[4] = {0, 0, 0, 0};
            rng r(0x8551u);

            for (int i = 0; i < 2000; ++i)
            {
                const vec3 off = r.direction() * r.range(1.5f, 4.0f);
                const quat qa = r.rotation();
                const quat qb = r.rotation();

                const obb A = world_obb(box_shape(vec3{0.6f, 0.4f, 0.5f}), vec3{}, qa);
                const obb B = world_obb(box_shape(vec3{0.5f, 0.7f, 0.3f}), off, qb);
                const gjk_result g0 = gjk_distance(as_convex(A), as_convex(B), cfg);
                if (g0.status == gjk_status::separated) { sums[0] += g0.iterations; ++counts[0]; }

                const engine::sphere SA{vec3{}, 0.5f};
                const engine::sphere SB{off, 0.4f};
                const gjk_result g1 = gjk_distance(as_convex(SA), as_convex(SB), cfg);
                if (g1.status == gjk_status::separated) { sums[1] += g1.iterations; ++counts[1]; }

                const capsule CA = world_capsule(capsule_shape(0.3f, 0.5f), vec3{}, qa);
                const capsule CB = world_capsule(capsule_shape(0.25f, 0.4f), off, qb);
                const gjk_result g2 = gjk_distance(as_convex(CA), as_convex(CB), cfg);
                if (g2.status == gjk_status::separated) { sums[2] += g2.iterations; ++counts[2]; }

                std::vector<vec3> ha = make_hull_points(r, 24, 0.5f);
                std::vector<vec3> hb = make_hull_points(r, 24, 0.5f);
                const hull HA{vec3{}, mat3::identity(), ha};
                const hull HB{off, mat3::identity(), hb};
                const gjk_result g3 = gjk_distance(as_convex(HA), as_convex(HB), cfg);
                if (g3.status == gjk_status::separated) { sums[3] += g3.iterations; ++counts[3]; }
            }

            std::printf("       %.0e   %6.2f    %6.2f    %6.2f    %6.2f\n",
                        static_cast<double>(tols[ti]),
                        sums[0] / std::max(1, counts[0]), sums[1] / std::max(1, counts[1]),
                        sums[2] / std::max(1, counts[2]), sums[3] / std::max(1, counts[3]));
        }
        std::printf("       — THREE columns are flat and ONE is not, and\n");
        std::printf("         the odd one out is not the one you would\n");
        std::printf("         guess. Box and hull are POLYTOPES: finitely\n");
        std::printf("         many vertices, the simplex can only improve\n");
        std::printf("         finitely often, so GJK does not converge on\n");
        std::printf("         them, it FINISHES — and the tolerance is\n");
        std::printf("         never what stopped it. The SPHERE is flat at\n");
        std::printf("         ONE for a different reason: a ball's closest\n");
        std::printf("         point to an outside point is on the line to\n");
        std::printf("         its centre, so the very first support call\n");
        std::printf("         in direction `delta` lands ON the answer.\n");
        std::printf("         Only the CAPSULE genuinely converges, and it\n");
        std::printf("         costs about ONE MORE PASS PER DECADE. A\n");
        std::printf("         capsule difference is a parallelogram\n");
        std::printf("         rounded by a ball, so the closest point can\n");
        std::printf("         sit on the curved part where neither\n");
        std::printf("         operand decides the answer alone.\n");
    }

    // ---- F.3  and the error follows the same split ------------------------
    {
        const float tols[6] = {1e-2f, 1e-3f, 1e-4f, 1e-5f, 1e-6f, 1e-7f};
        std::printf("\n  F.3  worst RELATIVE error against a closed form,\n");
        std::printf("       2,000 pairs each:\n");
        std::printf("       tol       AABB (exact)    sphere (curved)\n");

        for (int ti = 0; ti < 6; ++ti)
        {
            gjk_config cfg;
            cfg.tolerance = tols[ti];
            cfg.max_iterations = 200;
            double worst_box = 0.0;
            double worst_sph = 0.0;
            rng r(0x8552u);

            for (int i = 0; i < 2000; ++i)
            {
                aabb A;
                A.min = vec3{-0.6f, -0.4f, -0.5f};
                A.max = vec3{0.6f, 0.4f, 0.5f};
                const vec3 off = r.direction() * r.range(2.0f, 4.0f);
                aabb B;
                B.min = off - vec3{0.5f, 0.7f, 0.3f};
                B.max = off + vec3{0.5f, 0.7f, 0.3f};
                const obb OA = as_obb(A);
                const obb OB = as_obb(B);
                const gjk_result gb = gjk_distance(as_convex(OA), as_convex(OB), cfg);
                const double tb = aabb_distance_double(A, B);
                if (tb > 1e-3)
                {
                    worst_box = std::fmax(worst_box,
                                          std::fabs(static_cast<double>(gb.distance) - tb) / tb);
                }

                const engine::sphere SA{vec3{}, 0.5f};
                const engine::sphere SB{off, 0.4f};
                const gjk_result gs = gjk_distance(as_convex(SA), as_convex(SB), cfg);
                const double ts = dlen(promote(off)) - 0.9;
                if (ts > 1e-3)
                {
                    worst_sph = std::fmax(worst_sph,
                                          std::fabs(static_cast<double>(gs.distance) - ts) / ts);
                }
            }
            std::printf("       %.0e    %.4e      %.4e\n",
                        static_cast<double>(tols[ti]), worst_box, worst_sph);
        }
        std::printf("       — both columns go flat at float's noise floor\n");
        std::printf("         rather than at the tolerance, which is the\n");
        std::printf("         same statement F.2 made in iterations. The\n");
        std::printf("         only row where the tolerance is visible is\n");
        std::printf("         1e-2 on the boxes, and 1.8e-4 is exactly\n");
        std::printf("         what a 1e-2 relative tolerance permits.\n");
    }

    // ---- F.5  the resolution floor, and the margin it implies -------------
    {
        std::printf("\n  F.5  THE RESOLUTION FLOOR. Two 2 m cubes turned to\n");
        std::printf("       generic orientations so the contact is corner\n");
        std::printf("       to corner, walked together from 1 m apart to\n");
        std::printf("       one micrometre. The gap is set by bisection\n");
        std::printf("       and the reference is the certificate's UPPER\n");
        std::printf("       bound, which is two real surface points and\n");
        std::printf("       needs no algorithm to believe.\n\n");
        std::printf("       gap (m)   reported (m)  slack (m)  stall  status\n");

        const double gaps[9] = {1.0, 1e-1, 1e-2, 3e-3, 1e-3, 3e-4, 1e-4, 1e-5, 1e-6};
        rng rr(0x8554u);
        const quat qa = rr.rotation();
        const quat qb = rr.rotation();
        const shape cube = box_shape(vec3{1.0f, 1.0f, 1.0f});
        const obb A = world_obb(cube, vec3{}, qa);
        const vec3 dir{0.5773503f, 0.5773503f, 0.5773503f};

        for (int i = 0; i < 9; ++i)
        {
            // Bisect the offset until the CERTIFICATE's upper bound hits the
            // target gap. Bisecting on the certificate rather than on GJK's
            // reported distance keeps the fixture honest: the thing being
            // measured does not get to decide where it is measured.
            float lo = 2.0f;
            float hi = 8.0f;
            for (int b = 0; b < 60; ++b)
            {
                const float mid = 0.5f * (lo + hi);
                const obb B = world_obb(cube, dir * mid, qb);
                gjk_config tight;
                tight.tolerance = 1e-6f;
                const gjk_result g = gjk_distance(as_convex(A), as_convex(B), tight);
                const dbounds bd = certify_double(A, B, g);
                if (bd.upper > gaps[i]) { hi = mid; } else { lo = mid; }
            }
            const obb B = world_obb(cube, dir * (0.5f * (lo + hi)), qb);
            const gjk_result g = gjk_distance(as_convex(A), as_convex(B));
            const dbounds bd = certify_double(A, B, g);
            std::printf("       %.0e   %.6e  %.2e   %-4s   %s\n", gaps[i],
                        static_cast<double>(g.distance), bd.upper - bd.lower,
                        g.stalled ? "yes" : "no", name_of(g.status));
        }

        std::printf("\n       TWO SEPARATE THINGS GIVE OUT, at two\n");
        std::printf("       different gaps, and they are worth keeping\n");
        std::printf("       apart.\n\n");
        std::printf("       1. THE PROOF goes first. The test asks whether\n");
        std::printf("          |v|^2 - dot(v,w) <= tol*|v|^2. The left side\n");
        std::printf("          carries rounding error of order eps*|v|*|w|\n");
        std::printf("          and the right side is tol*|v|^2, so the\n");
        std::printf("          threshold falls as the gap SQUARED while\n");
        std::printf("          the noise falls only as the gap. Below\n\n");
        std::printf("            |v|  ~  eps * |w| / tol\n\n");
        std::printf("          the comparison is noise against noise. For\n");
        std::printf("          eps = 1.19e-07, |w| = 4 m and tol = 1e-4\n");
        std::printf("          that is %.2e m, and the `stall` column\n",
                    1.1920929e-7 * 4.0 / 1e-4);
        std::printf("          turns over there. The ANSWER is still\n");
        std::printf("          right; what is gone is the certificate.\n\n");
        std::printf("       2. THE ANSWER goes second, at a gap where GJK\n");
        std::printf("          stops calling them apart at all. That is\n");
        std::printf("          the `|v|^2 <= tol^2 * |w|^2` test, which is\n");
        std::printf("          `|v| <= tol * |w|` — a CONTACT MARGIN of\n");
        std::printf("          %.2e m for these shapes, and a\n", 1e-4 * 4.0);
        std::printf("          deliberate one. Every physics engine has\n");
        std::printf("          one; 8.7's manifolds will keep contacts\n");
        std::printf("          that are within a margin rather than\n");
        std::printf("          touching, for exactly this reason.\n\n");
        std::printf("       8.4 11 found the same wall measured from the\n");
        std::printf("       WORLD ORIGIN. This one is measured from the\n");
        std::printf("       SHAPES, and no amount of recentring moves it.\n");
    }

    // ---- F.6  where the margin actually sits ------------------------------
    {
        std::printf("\n  F.6  the margin, measured rather than derived.\n");
        std::printf("       Cubes of side S at generic orientations,\n");
        std::printf("       bisected for the largest gap GJK still calls\n");
        std::printf("       a contact:\n\n");
        std::printf("       side S    margin (m)   margin / (tol * S)\n");

        const float sides[5] = {0.1f, 1.0f, 10.0f, 100.0f, 1000.0f};
        rng rr(0x8555u);
        const quat qa = rr.rotation();
        const quat qb = rr.rotation();
        const vec3 dir{0.5773503f, 0.5773503f, 0.5773503f};

        for (int i = 0; i < 5; ++i)
        {
            const float h = 0.5f * sides[i];
            const shape cube = box_shape(vec3{h, h, h});
            const obb A = world_obb(cube, vec3{}, qa);

            // Bisect the CENTRE OFFSET, and measure the resulting gap with the
            // certificate rather than with GJK, so the thing under test does not
            // get to say where the boundary is.
            float lo = 0.0f;                        // certainly overlapping
            float hi = 4.0f * sides[i];             // certainly apart
            for (int b = 0; b < 60; ++b)
            {
                const float mid = 0.5f * (lo + hi);
                const obb B = world_obb(cube, dir * mid, qb);
                const gjk_result g = gjk_distance(as_convex(A), as_convex(B));
                if (g.status == gjk_status::separated && g.distance > 0.0f) { hi = mid; }
                else { lo = mid; }
            }
            const obb B = world_obb(cube, dir * hi, qb);
            const gjk_result g = gjk_distance(as_convex(A), as_convex(B));
            const dbounds bd = certify_double(A, B, g);

            std::printf("       %7.1f   %.4e   %.3f\n", static_cast<double>(sides[i]),
                        bd.upper, bd.upper / (1e-4 * static_cast<double>(sides[i])));
        }
        std::printf("       — the last column is the finding: the margin\n");
        std::printf("         is a fixed multiple of tolerance times SIZE\n");
        std::printf("         across four decades. It is a RELATIVE\n");
        std::printf("         quantity, which is what makes one tolerance\n");
        std::printf("         usable on a marble and on a cathedral — and\n");
        std::printf("         it is also why `tolerance` is not a distance\n");
        std::printf("         and must not be set as one.\n");
    }

    // ---- F.4  CONTROL: does the cap ever fire? ----------------------------
    {
        rng r(0x8553u);
        const int n = 200000;
        int limited = 0;
        gjk_config tight;
        tight.tolerance = 1e-7f;
        tight.max_iterations = 64;

        for (int i = 0; i < n; ++i)
        {
            const capsule A = world_capsule(capsule_shape(r.range(0.1f, 0.6f),
                                                          r.range(0.0f, 1.0f)),
                                            vec3{}, r.rotation());
            const capsule B = world_capsule(capsule_shape(r.range(0.1f, 0.6f),
                                                          r.range(0.0f, 1.0f)),
                                            r.direction() * r.range(0.5f, 3.0f), r.rotation());
            const gjk_result g = gjk_distance(as_convex(A), as_convex(B), tight);
            if (g.status == gjk_status::iteration_limit) { ++limited; }
        }

        std::printf("\n  F.4  CONTROL capsules at tolerance 1e-7, cap 64:\n");
        std::printf("       hit the iteration limit  %d of %d\n", limited, n);
        std::printf("       — a cap that never fires at the DEFAULT\n");
        std::printf("         tolerance is still the only reason this\n");
        std::printf("         function returns on a curved shape. 8.4's\n");
        std::printf("         SAT had fifteen candidates and no loop.\n");
    }
}

// ===========================================================================
// G  WARM STARTING
// ===========================================================================

void section_g()
{
    rule("G  WARM STARTING");

    rng r(0x8560u);
    const int pairs = 2000;
    const int steps = 120;

    long long cold = 0;
    long long warm = 0;
    long long rand_start = 0;
    long long counted = 0;

    for (int p = 0; p < pairs; ++p)
    {
        // A pair drifting past each other, 120 frames, the way a real scene
        // moves: a small step per frame rather than a teleport.
        const shape sa = box_shape(vec3{r.range(0.3f, 0.8f), r.range(0.3f, 0.8f),
                                        r.range(0.3f, 0.8f)});
        const shape sb = box_shape(vec3{r.range(0.3f, 0.8f), r.range(0.3f, 0.8f),
                                        r.range(0.3f, 0.8f)});
        const quat qa = r.rotation();
        quat qb = r.rotation();
        const vec3 start = r.direction() * 3.0f;
        const vec3 vel = r.direction() * 0.05f;
        const quat spin = quat_from_axis_angle(r.direction(), 0.01f);

        const obb A = world_obb(sa, vec3{}, qa);
        vec3 previous{};

        for (int t = 0; t < steps; ++t)
        {
            qb = normalised(spin * qb);
            const obb B = world_obb(sb, start + vel * static_cast<float>(t), qb);

            gjk_config c0;                         // cold: centre to centre
            const gjk_result g0 = gjk_distance(as_convex(A), as_convex(B), c0);

            gjk_config c1;                         // warm: last frame's answer
            c1.initial_direction = previous;
            const gjk_result g1 = gjk_distance(as_convex(A), as_convex(B), c1);

            gjk_config c2;                         // control: a random direction
            c2.initial_direction = r.direction();
            const gjk_result g2 = gjk_distance(as_convex(A), as_convex(B), c2);

            if (g0.status == gjk_status::separated && g1.status == gjk_status::separated &&
                g2.status == gjk_status::separated)
            {
                cold += g0.iterations;
                warm += g1.iterations;
                rand_start += g2.iterations;
                ++counted;
                // The direction GJK just found. It has the same sense as the
                // cold seed `b.origin - a.origin`, so warm and cold differ only
                // in how good a guess they are, which is the whole comparison.
                previous = g1.direction;
            }
        }
    }

    const double c = static_cast<double>(cold) / counted;
    const double w = static_cast<double>(warm) / counted;
    const double rs = static_cast<double>(rand_start) / counted;

    std::printf("  G.1  %lld queries, %d pairs drifting over %d frames\n",
                counted, pairs, steps);
    std::printf("       mean iterations\n");
    std::printf("         cold  (centre to centre)   %.3f\n", c);
    std::printf("         warm  (last frame's axis)  %.3f   %.2fx\n", w, c / w);
    std::printf("         CONTROL (random axis)      %.3f   %.2fx\n", rs, c / rs);
    std::printf("       — READ THE CONTROL FIRST. A random seed costs\n");
    std::printf("         %.0f%% more passes than centre-to-centre, so\n", 100.0 * (rs / c - 1.0));
    std::printf("         the seed matters a great deal. And then warm\n");
    std::printf("         starting buys only %.0f%% on top, because the\n", 100.0 * (c / w - 1.0));
    std::printf("         free heuristic was ALREADY nearly right: two\n");
    std::printf("         bodies that have not moved much have not moved\n");
    std::printf("         their centre line much either.\n");
    std::printf("       The honest conclusion is that warm starting is\n");
    std::printf("       worth having and is not where the win is. 8.7's\n");
    std::printf("       manifold cache will want it for a different\n");
    std::printf("       reason — keeping the same CONTACT FEATURE frame\n");
    std::printf("       to frame, which is about stability, not speed.\n");
    check(w <= c + 1e-9, "G.1 warm starting never costs more");
    check(rs > c, "G.1 CONTROL a random seed is measurably worse");
}

// ===========================================================================
// H  WHERE THE ORIGIN IS
// ===========================================================================

void section_h()
{
    rule("H  WHERE THE ORIGIN IS");

    std::printf("  ONE PAIR OF BOXES at a fixed DIAGONAL offset, walked\n");
    std::printf("  away from the world origin along (1,1,1)/sqrt(3). The\n");
    std::printf("  distance is the same at every stop by construction, so\n");
    std::printf("  any change in the reported number is arithmetic rather\n");
    std::printf("  than geometry — and because the boxes are axis aligned\n");
    std::printf("  there is an EXACT reference: three one-dimensional gaps\n");
    std::printf("  and a square root, evaluated in double from the boxes\n");
    std::printf("  AS FLOAT ACTUALLY STORES THEM.\n\n");
    std::printf("  TWO ARMS, ONE ALGORITHM. `gjk.cpp` is the same code in\n");
    std::printf("  both columns; only the `convex` view differs. The\n");
    std::printf("  RELATIVE arm hands it support points measured from each\n");
    std::printf("  shape's own centre and one `delta` formed once; the\n");
    std::printf("  NAIVE arm hands it WORLD support points and a zero\n");
    std::printf("  origin, so every iteration subtracts two world-sized\n");
    std::printf("  numbers. That is the formulation `convex.hpp` exists to\n");
    std::printf("  avoid, and making it an adapter rather than a fork is\n");
    std::printf("  what makes the comparison mean anything.\n\n");

    const vec3 ha{0.6f, 0.4f, 0.5f};
    const vec3 hb{0.5f, 0.7f, 0.3f};
    const vec3 offset{2.5f, 1.7f, 1.1f};

    std::printf("   from 0 (m)   relative (m)   naive (m)      exact (m)\n");

    const double scales[7] = {0.0, 1.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0};
    double worst_rel = 0.0;
    double worst_naive = 0.0;

    for (int i = 0; i < 7; ++i)
    {
        const float d = static_cast<float>(scales[i] / std::sqrt(3.0));
        const vec3 base{d, d, d};
        const obb A = world_obb(box_shape(ha), base, quat::identity());
        const obb B = world_obb(box_shape(hb), base + offset, quat::identity());

        const gjk_result rel = gjk_distance(as_convex(A), as_convex(B));
        const convex wa = as_convex_world(A);
        const convex wb = as_convex_world(B);
        const gjk_result naive = gjk_distance(wa, wb);

        const double truth = centre_box_distance_double(A.centre, ha, B.centre, hb);

        std::printf("   %10.0f   %.6e   %.6e   %.6e\n", scales[i],
                    static_cast<double>(rel.distance), static_cast<double>(naive.distance), truth);

        worst_rel = std::fmax(worst_rel, std::fabs(static_cast<double>(rel.distance) - truth));
        worst_naive = std::fmax(worst_naive, std::fabs(static_cast<double>(naive.distance) - truth));
    }

    std::printf("\n  H.1  worst departure from the exact distance\n");
    std::printf("       between the boxes as stored:\n");
    std::printf("         relative form  %.4e m\n", worst_rel);
    std::printf("         naive form     %.4e m   %.0fx\n", worst_naive,
                worst_naive / std::fmax(worst_rel, 1e-12));
    std::printf("       — note what the reference is and is not. It is\n");
    std::printf("         the distance between the boxes FLOAT HOLDS, not\n");
    std::printf("         the distance between the boxes the program\n");
    std::printf("         meant. Those diverge too, and no arrangement of\n");
    std::printf("         the arithmetic can fix it: at 1000 km a float\n");
    std::printf("         position cannot name a centimetre. What the\n");
    std::printf("         relative form buys is that GJK ADDS NOTHING OF\n");
    std::printf("         ITS OWN to a degradation the input already has,\n");
    std::printf("         which is everything an algorithm can buy. 8.4\n");
    std::printf("         11 measured the same wall from the other side.\n");
    check(worst_rel <= worst_naive, "H.1 the relative form is never worse");

    // ---- H.2  where each arm's error comes from --------------------------
    {
        std::printf("\n  H.2  the mechanism, counted rather than argued.\n");
        std::printf("       A box's support point is `centre + axes*s`,\n");
        std::printf("       three additions. Done in WORLD space each one\n");
        std::printf("       rounds to half an ulp of the world POSITION,\n");
        std::printf("       and there are two support points per\n");
        std::printf("       iteration. Done RELATIVE they round to half an\n");
        std::printf("       ulp of the SHAPE, and the single world-sized\n");
        std::printf("       subtraction `b.origin - a.origin` is EXACT\n");
        std::printf("       whenever the two are within a factor of two of\n");
        std::printf("       each other — Sterbenz's lemma, and two things\n");
        std::printf("       about to collide always are.\n\n");
        std::printf("       from 0 (m)   ulp(pos)    naive err   rel err\n");

        for (int i = 2; i < 7; ++i)
        {
            const float d = static_cast<float>(scales[i] / std::sqrt(3.0));
            const vec3 base{d, d, d};
            const obb A = world_obb(box_shape(ha), base, quat::identity());
            const obb B = world_obb(box_shape(hb), base + offset, quat::identity());
            const convex wa = as_convex_world(A);
            const convex wb = as_convex_world(B);
            const gjk_result nv = gjk_distance(wa, wb);

            const double truth = centre_box_distance_double(A.centre, ha, B.centre, hb);

            const double ulp = static_cast<double>(
                std::nextafter(d, std::numeric_limits<float>::infinity()) - d);
            const gjk_result rl = gjk_distance(as_convex(A), as_convex(B));
            const double err = std::fabs(static_cast<double>(nv.distance) - truth);
            const double err_rel = std::fabs(static_cast<double>(rl.distance) - truth);
            std::printf("       %10.0f   %.3e   %.3e   %.3e\n", scales[i], ulp, err, err_rel);
        }
        std::printf("       — the naive column tracks ulp(position) and\n");
        std::printf("         the relative column does not track anything:\n");
        std::printf("         it stays at the shape's own resolution\n");
        std::printf("         wherever the shapes are. The error is not\n");
        std::printf("         chaotic and it is not a bug — it is the\n");
        std::printf("         number of world-sized additions each arm\n");
        std::printf("         performs, six per iteration against zero.\n");
    }
}

// ===========================================================================
// I  THE BUDGET
// ===========================================================================

void section_i()
{
    rule("I  THE BUDGET");

    rng r(0x8570u);
    const std::size_t n = 20000;

    std::vector<obb> as;
    std::vector<obb> bs;
    as.reserve(n);
    bs.reserve(n);
    int overlapping = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const shape sa = box_shape(vec3{r.range(0.3f, 1.0f), r.range(0.3f, 1.0f),
                                        r.range(0.3f, 1.0f)});
        const shape sb = box_shape(vec3{r.range(0.3f, 1.0f), r.range(0.3f, 1.0f),
                                        r.range(0.3f, 1.0f)});
        as.push_back(world_obb(sa, vec3{}, r.rotation()));
        bs.push_back(world_obb(sb, r.direction() * r.range(0.4f, 3.0f), r.rotation()));
        if (overlaps(as.back(), bs.back())) { ++overlapping; }
    }

    std::printf("  %zu box pairs, %d of them overlapping\n\n", n, overlapping);

    const bench_result b_overlaps = bench_run(n, 9, [&]() {
        double acc = 0.0;
        for (std::size_t i = 0; i < n; ++i) { acc += overlaps(as[i], bs[i]) ? 1.0 : 0.0; }
        return acc;
    });
    const bench_result b_sat = bench_run(n, 9, [&]() {
        double acc = 0.0;
        for (std::size_t i = 0; i < n; ++i) { acc += static_cast<double>(collide(as[i], bs[i]).depth); }
        return acc;
    });
    const bench_result b_gjk_bool = bench_run(n, 9, [&]() {
        double acc = 0.0;
        for (std::size_t i = 0; i < n; ++i)
        {
            acc += gjk_intersects(as_convex(as[i]), as_convex(bs[i])) ? 1.0 : 0.0;
        }
        return acc;
    });
    const bench_result b_gjk_dist = bench_run(n, 9, [&]() {
        double acc = 0.0;
        for (std::size_t i = 0; i < n; ++i)
        {
            acc += static_cast<double>(gjk_distance(as_convex(as[i]), as_convex(bs[i])).distance);
        }
        return acc;
    });

    std::printf("  I.1  ns per pair\n");
    std::printf("       overlaps (SAT, bool)     %7.3f\n", b_overlaps.median_ns);
    std::printf("       collide  (SAT, MTV)      %7.3f\n", b_sat.median_ns);
    std::printf("       gjk_intersects (bool)    %7.3f   %.2fx\n",
                b_gjk_bool.median_ns, b_gjk_bool.median_ns / b_overlaps.median_ns);
    std::printf("       gjk_distance (metres)    %7.3f   %.2fx\n",
                b_gjk_dist.median_ns, b_gjk_dist.median_ns / b_sat.median_ns);
    std::printf("       spread %.2f / %.2f / %.2f / %.2f\n",
                b_overlaps.spread(), b_sat.spread(), b_gjk_bool.spread(), b_gjk_dist.spread());
    std::printf("       — GJK is SLOWER on boxes, and it should be: the\n");
    std::printf("         SAT knows the shape and GJK searches for it.\n");
    std::printf("         What GJK buys is the metres and the generality,\n");
    std::printf("         not speed, and an engine keeps BOTH for that\n");
    std::printf("         reason. 8.8's broadphase will call the cheap\n");
    std::printf("         one first whichever pair of shapes it holds.\n");

    // ---- I.2  what a hull's support costs, by vertex count ---------------
    {
        std::printf("\n  I.2  hull support cost by vertex count\n");
        std::printf("       verts   ns/support   ns/gjk_distance\n");
        const int counts[5] = {8, 16, 32, 64, 128};
        for (int ci = 0; ci < 5; ++ci)
        {
            rng rr(0x8571u + static_cast<std::uint32_t>(ci));
            std::vector<std::vector<vec3>> sets_a;
            std::vector<std::vector<vec3>> sets_b;
            std::vector<vec3> offsets;
            const std::size_t m = 2000;
            for (std::size_t i = 0; i < m; ++i)
            {
                sets_a.push_back(make_hull_points(rr, counts[ci], 0.5f));
                sets_b.push_back(make_hull_points(rr, counts[ci], 0.5f));
                offsets.push_back(rr.direction() * rr.range(1.5f, 3.0f));
            }
            std::vector<hull> ha;
            std::vector<hull> hb;
            for (std::size_t i = 0; i < m; ++i)
            {
                ha.push_back(hull{vec3{}, mat3::identity(), sets_a[i]});
                hb.push_back(hull{offsets[i], mat3::identity(), sets_b[i]});
            }
            const vec3 d{0.577f, 0.577f, 0.577f};
            const bench_result bs = bench_run(m, 9, [&]() {
                double acc = 0.0;
                for (std::size_t i = 0; i < m; ++i) { acc += static_cast<double>(support_local(ha[i], d).x); }
                return acc;
            });
            const bench_result bg = bench_run(m, 9, [&]() {
                double acc = 0.0;
                for (std::size_t i = 0; i < m; ++i)
                {
                    acc += static_cast<double>(gjk_distance(as_convex(ha[i]), as_convex(hb[i])).distance);
                }
                return acc;
            });
            std::printf("       %5d   %8.3f     %8.3f\n", counts[ci], bs.median_ns, bg.median_ns);
        }
        std::printf("       — linear in the vertex count, as promised, and\n");
        std::printf("         `gjk_distance` is that line times roughly\n");
        std::printf("         twice the iteration count. A hill-climbing\n");
        std::printf("         support would bend the first column toward\n");
        std::printf("         sqrt(n); it is named in section 13 and not\n");
        std::printf("         built, because it needs an adjacency list\n");
        std::printf("         this engine does not have.\n");
    }

    // ---- I.3  what the indirect call costs -------------------------------
    {
        const vec3 d{0.577f, 0.577f, 0.577f};
        const bench_ab ab = bench_compare(n, 11,
            [&]() {
                double acc = 0.0;
                for (std::size_t i = 0; i < n; ++i)
                {
                    acc += static_cast<double>(support_local(as[i], d).x);
                }
                return acc;
            },
            [&]() {
                double acc = 0.0;
                for (std::size_t i = 0; i < n; ++i)
                {
                    const convex c = as_convex(as[i]);
                    acc += static_cast<double>(c.support(c.data, d).x);
                }
                return acc;
            });

        std::printf("\n  I.3  the price of the function pointer\n");
        std::printf("       direct  support_local(obb)  %7.3f ns\n", ab.a.median_ns);
        std::printf("       through convex::support     %7.3f ns   %.2fx\n",
                    ab.b.median_ns, ab.ratio());
        std::printf("       answers agree bit for bit:  %s\n", ab.agree ? "yes" : "NO");
        std::printf("       — the two arms compute the same float and the\n");
        std::printf("         comparison is void if they do not, which is\n");
        std::printf("         why `agree` is printed rather than assumed.\n");
        check(ab.agree, "I.3 the two arms agree bit for bit");
    }
}

} // namespace

int main()
{
    std::printf("verify_85 — Lesson 8.5, GJK\n");

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
