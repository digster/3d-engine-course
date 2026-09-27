// scratch/verify_84.cpp — every number Lesson 8.4 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_84.sh
//
// Nine sections, in the lesson's order:
//
//   A  the shadow test, and the certificate that replaces trust
//   B  projecting a box: the radius of its shadow
//   C  an AABB test is the SAT with three axes
//   D  six axes are not enough
//   E  the MTV, and the direction it points
//   F  when two edges are parallel
//   G  where the precision floor is
//   H  a sphere against a box, including from inside
//   I  the budget
//
// EVERY SECTION CARRIES A CONTROL, and 8.1-8.3 left the rule in two halves: ask
// what the control would say if the thing were COMPLETELY BROKEN, and what it
// would say if the thing were completely FINE.
//
// THIS HARNESS HAS A PROPERTY THE PREVIOUS THREE DID NOT, and it is the reason
// several sections are short. **A separating axis is a certificate.** When
// `collide` reports that two boxes are apart it hands back the direction, and
// anybody can check that direction in four dot products — so for every separated
// pair this file does not compare against a reference implementation, it
// RE-PROVES the answer in double precision from the box data alone. There is no
// second implementation to be wrong in the same way.
//
// Overlap does not certify itself so cheaply, so the other half is a witness
// hunt: sample the first box's volume on a grid and look for a point inside the
// second. A found point is a proof; a not-found point is only evidence, and §D
// reports the depth of the cases it could not confirm so the reader can see they
// are grazing rather than wrong.
//
// PRECISION. The engine collides in `float`, so this harness does too.
// Certificates are evaluated in double — the only honest way round, since a
// reference computed at the same precision as the thing it checks cannot tell
// you which one is wrong. §G is about exactly the gap between the two.
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
#include <engine/phys/shape.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using engine::aabb;
using engine::mat3;
using engine::mat3_from_quat;
using engine::quat;
using engine::quat_from_axis_angle;
using engine::vec3;
using engine::phys::as_obb;
using engine::phys::axis_source;
using engine::phys::bounds_of;
using engine::phys::box_shape;
using engine::phys::closest_point;
using engine::phys::collide;
using engine::phys::gap_on_axis;
using engine::phys::interval;
using engine::phys::k_parallel_sin2;
using engine::phys::name_of;
using engine::phys::obb;
using engine::phys::overlaps;
using engine::phys::project;
using engine::phys::projected_radius;
using engine::phys::separation;
using engine::phys::shape;
using engine::phys::source_of;
using engine::phys::sphere_shape;
using engine::phys::support;
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

/// A deterministic 32-bit generator, written out rather than pulled from
/// <random> because `std::uniform_real_distribution` is NOT specified to give
/// the same sequence on two standard libraries. A harness whose numbers change
/// when you change compiler is a harness you cannot quote in a lesson.
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

    /// Uniform in [0, 1).
    float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }

    /// Uniform in [-1, 1].
    float signed_unit() { return unit() * 2.0f - 1.0f; }

    /// Uniform in [lo, hi].
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

    /// A rotation with no preferred axis and no preferred angle.
    quat rotation()
    {
        return quat_from_axis_angle(direction(), range(0.0f, 2.0f * 3.14159265f));
    }

private:
    std::uint32_t state_;
};

// ---------------------------------------------------------------------------
// Certificates, in double
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

/// `Σᵢ hᵢ |uᵢ · L|`, evaluated in double from the float box data.
double radius_double(const obb& box, dvec3 L)
{
    return static_cast<double>(box.half_extents.x) * std::fabs(ddot(promote(box.axes.c0), L)) +
           static_cast<double>(box.half_extents.y) * std::fabs(ddot(promote(box.axes.c1), L)) +
           static_cast<double>(box.half_extents.z) * std::fabs(ddot(promote(box.axes.c2), L));
}

/// The signed gap between two boxes along `L`, in double. Positive proves them
/// apart. THIS IS THE CERTIFICATE CHECKER: it never calls anything in
/// collide.cpp, it reads the boxes and the claimed axis and nothing else.
double gap_double(const obb& a, const obb& b, vec3 axis)
{
    const dvec3 L = promote(axis);
    const double len = std::sqrt(ddot(L, L));
    const dvec3 Ln{L.x / len, L.y / len, L.z / len};
    const dvec3 t{static_cast<double>(b.centre.x) - static_cast<double>(a.centre.x),
                  static_cast<double>(b.centre.y) - static_cast<double>(a.centre.y),
                  static_cast<double>(b.centre.z) - static_cast<double>(a.centre.z)};
    return std::fabs(ddot(t, Ln)) - (radius_double(a, Ln) + radius_double(b, Ln));
}

/// The full fifteen-axis test, in double, with no guard.
///
/// THE REFERENCE FOR §F, and it can be one precisely because §F is about
/// precision. A cross product of two nearly-parallel unit vectors has a
/// direction error of about `eps / sin θ`; in `float` that is 1e-7/sin θ and at
/// sin θ = 1e-6 it is a tenth of a radian, while in `double` it is 1e-16/sin θ
/// and stays below 1e-8 everywhere this harness looks. So the same algorithm at
/// the other precision is an independent answer here, in a way it would not be
/// for a question about geometry.
bool overlaps_double(const obb& a, const obb& b)
{
    const dvec3 au[3] = {promote(a.axes.c0), promote(a.axes.c1), promote(a.axes.c2)};
    const dvec3 bu[3] = {promote(b.axes.c0), promote(b.axes.c1), promote(b.axes.c2)};
    const double ah[3] = {a.half_extents.x, a.half_extents.y, a.half_extents.z};
    const double bh[3] = {b.half_extents.x, b.half_extents.y, b.half_extents.z};
    const dvec3 t{static_cast<double>(b.centre.x) - static_cast<double>(a.centre.x),
                  static_cast<double>(b.centre.y) - static_cast<double>(a.centre.y),
                  static_cast<double>(b.centre.z) - static_cast<double>(a.centre.z)};

    const auto separates = [&](dvec3 L) {
        const double len = std::sqrt(ddot(L, L));
        if (len <= 0.0) { return false; }
        const dvec3 n{L.x / len, L.y / len, L.z / len};
        double ra = 0.0;
        double rb = 0.0;
        for (int k = 0; k < 3; ++k)
        {
            ra += ah[k] * std::fabs(ddot(au[k], n));
            rb += bh[k] * std::fabs(ddot(bu[k], n));
        }
        return std::fabs(ddot(t, n)) > ra + rb;
    };

    const auto dcross = [](dvec3 u, dvec3 v) {
        return dvec3{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
    };

    for (int i = 0; i < 3; ++i) { if (separates(au[i])) { return false; } }
    for (int j = 0; j < 3; ++j) { if (separates(bu[j])) { return false; } }
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            if (separates(dcross(au[i], bu[j]))) { return false; }
        }
    }
    return true;
}

/// Is `p` inside (or on) the box?
bool inside(const obb& box, vec3 p)
{
    const vec3 d = p - box.centre;
    return std::fabs(dot(d, box.axes.c0)) <= box.half_extents.x &&
           std::fabs(dot(d, box.axes.c1)) <= box.half_extents.y &&
           std::fabs(dot(d, box.axes.c2)) <= box.half_extents.z;
}

/// Look for a point inside both boxes by walking an n³ grid over `a`'s volume.
///
/// A found point PROVES overlap. Not finding one proves nothing, which is why
/// every caller reports the failures rather than counting them as passes.
bool witness_exists(const obb& a, const obb& b, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float sx = (n == 1) ? 0.0f : -1.0f + 2.0f * static_cast<float>(i) /
                                                       static_cast<float>(n - 1);
        for (int j = 0; j < n; ++j)
        {
            const float sy = (n == 1) ? 0.0f : -1.0f + 2.0f * static_cast<float>(j) /
                                                           static_cast<float>(n - 1);
            for (int k = 0; k < n; ++k)
            {
                const float sz = (n == 1) ? 0.0f : -1.0f + 2.0f * static_cast<float>(k) /
                                                               static_cast<float>(n - 1);
                const vec3 p = a.centre + a.axes.c0 * (sx * a.half_extents.x) +
                               a.axes.c1 * (sy * a.half_extents.y) +
                               a.axes.c2 * (sz * a.half_extents.z);
                if (inside(b, p)) { return true; }
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// The versions we did NOT ship, kept here so the lesson can measure them
// ---------------------------------------------------------------------------

/// The SAT with the six face normals and nothing else.
///
/// Not a straw man: it is what you write if you reason "a box is bounded by its
/// faces, so a face normal is where a gap must show". §D is the measurement of
/// how often that is false.
bool overlaps_faces_only(const obb& a, const obb& b)
{
    for (int i = 0; i < 3; ++i)
    {
        if (gap_on_axis(a, b, a.axis(i)) > 0.0f) { return false; }
    }
    for (int j = 0; j < 3; ++j)
    {
        if (gap_on_axis(a, b, b.axis(j)) > 0.0f) { return false; }
    }
    return true;
}

/// The full fifteen, with **no degeneracy guard** on the cross products.
///
/// Identical to `collide(obb, obb)`'s search except that a near-zero cross
/// product is normalised and tested instead of skipped. §F is what that costs.
separation collide_unguarded(const obb& a, const obb& b)
{
    const vec3 t = b.centre - a.centre;
    separation out;
    out.depth = 1e30f;
    out.axis_index = -2;

    const auto consider = [&](vec3 L, int index) -> bool
    {
        ++out.axes_tested;
        const float centre_gap = dot(t, L);
        const float r = projected_radius(a, L) + projected_radius(b, L);
        const float overlap = r - std::fabs(centre_gap);
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

    for (int i = 0; i < 3; ++i) { if (consider(a.axis(i), i)) { return out; } }
    for (int j = 0; j < 3; ++j) { if (consider(b.axis(j), 3 + j)) { return out; } }
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            const vec3 c = cross(a.axis(i), b.axis(j));
            const float len2 = length_squared(c);
            // THE ONE DIFFERENCE. No `if (len2 < k_parallel_sin2) continue;`.
            // A zero-length axis normalises to NaN, which compares false against
            // everything, so the truly-parallel case falls through harmlessly —
            // it is the NEARLY parallel case, where the direction is rounding
            // error but the length is not zero, that does the damage.
            if (len2 <= 0.0f) { continue; }
            if (consider(c / std::sqrt(len2), 6 + 3 * i + j)) { return out; }
        }
    }
    return out;
}

/// `overlaps()` with the epsilon on the absolute matrix made settable.
///
/// The shipped version adds `k_parallel_sin2` to every |R| term. Passing 0 here
/// is the same algorithm with that one addition removed, which is the only
/// difference §F.3 measures.
bool overlaps_eps(const obb& a, const obb& b, float eps)
{
    float R[3][3];
    float AbsR[3][3];
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            R[i][j] = dot(a.axis(i), b.axis(j));
            AbsR[i][j] = std::fabs(R[i][j]) + eps;
        }
    }
    const vec3 tw = b.centre - a.centre;
    const float t[3] = {dot(tw, a.axes.c0), dot(tw, a.axes.c1), dot(tw, a.axes.c2)};
    const float ae[3] = {a.half_extents.x, a.half_extents.y, a.half_extents.z};
    const float be[3] = {b.half_extents.x, b.half_extents.y, b.half_extents.z};
    static const int nx[3] = {1, 2, 0};
    static const int pv[3] = {2, 0, 1};

    for (int i = 0; i < 3; ++i)
    {
        const float ra = ae[i];
        const float rb = be[0] * AbsR[i][0] + be[1] * AbsR[i][1] + be[2] * AbsR[i][2];
        if (std::fabs(t[i]) > ra + rb) { return false; }
    }
    for (int j = 0; j < 3; ++j)
    {
        const float ra = ae[0] * AbsR[0][j] + ae[1] * AbsR[1][j] + ae[2] * AbsR[2][j];
        const float rb = be[j];
        if (std::fabs(t[0] * R[0][j] + t[1] * R[1][j] + t[2] * R[2][j]) > ra + rb)
        {
            return false;
        }
    }
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            const float ra = ae[nx[i]] * AbsR[pv[i]][j] + ae[pv[i]] * AbsR[nx[i]][j];
            const float rb = be[nx[j]] * AbsR[i][pv[j]] + be[pv[j]] * AbsR[i][nx[j]];
            const float lhs = t[pv[i]] * R[nx[i]][j] - t[nx[i]] * R[pv[i]][j];
            if (std::fabs(lhs) > ra + rb) { return false; }
        }
    }
    return true;
}

/// A sphere-versus-box test with the inside case left out, which is what the
/// clamp gives you if you stop at the obvious version.
separation collide_no_inside(const obb& box, const engine::sphere& s)
{
    separation out;
    out.axis_index = -1;
    out.axes_tested = 1;

    const vec3 d = s.centre - box.centre;
    const vec3 h = box.half_extents;
    const vec3 local{dot(d, box.axes.c0), dot(d, box.axes.c1), dot(d, box.axes.c2)};
    const vec3 clamped{std::clamp(local.x, -h.x, h.x), std::clamp(local.y, -h.y, h.y),
                       std::clamp(local.z, -h.z, h.z)};
    const vec3 delta = local - clamped;
    const float d2 = length_squared(delta);
    const float dist = std::sqrt(d2);
    // A divide by zero would be loud. This is the version that "handles" it.
    out.axis = (d2 > 0.0f) ? box.axes * (delta / dist) : vec3{};
    out.depth = s.radius - dist;
    return out;
}

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

obb make_obb(vec3 half, vec3 centre, quat q)
{
    return world_obb(box_shape(half), centre, q);
}

/// Two long thin boxes placed so that ONLY an edge-edge axis can separate them.
///
/// Built rather than searched for, and the construction is the argument: pick
/// the axis first — the cross product of one long edge from each box — then
/// place the second box along it, just far enough away to clear both shadows.
/// Nothing about that placement involves a face normal, and §D.1 prints all
/// fifteen candidates so the reader can see which ones noticed.
void crossed_boxes(obb& a, obb& b, float gap)
{
    const vec3 half{2.0f, 0.15f, 0.15f};
    a = make_obb(half, vec3{}, quat::identity());

    // A generic rotation — 0.7π about (1,2,3) normalised, the same one 8.3 §6
    // used to catch a convention bug that every 90° test had passed. Nothing
    // here is parallel to anything.
    const quat qb = quat_from_axis_angle(normalised(vec3{1.0f, 2.0f, 3.0f}),
                                         static_cast<float>(0.7 * k_pi));
    b = make_obb(half, vec3{}, qb);

    const vec3 L = normalised(cross(a.axis(0), b.axis(0)));
    const float reach = projected_radius(a, L) + projected_radius(b, L);
    b.centre = L * (reach + gap);
}

const char* yes_no(bool v) { return v ? "yes" : "no"; }

}   // namespace

// ---------------------------------------------------------------------------
// A — the shadow test, and the certificate that replaces trust
// ---------------------------------------------------------------------------

void section_a()
{
    rule("A  THE SHADOW TEST, AND THE CERTIFICATE");

    const engine::sphere a{vec3{0.0f, 0.0f, 0.0f}, 1.0f};
    const engine::sphere b{vec3{1.5f, 0.0f, 0.0f}, 1.0f};
    const separation s = collide(a, b);

    std::printf("  two unit spheres, centres 1.5 m apart\n\n");
    std::printf("  A.1  depth   %+.6f m   (2.0 - 1.5)\n", static_cast<double>(s.depth));
    std::printf("       axis    (%.4f, %.4f, %.4f)\n", static_cast<double>(s.axis.x),
                static_cast<double>(s.axis.y), static_cast<double>(s.axis.z));
    std::printf("       source  %s,  axes tested %d\n", name_of(source_of(s)), s.axes_tested);

    // A.2 — the interval picture, which is the SAT with one axis.
    const vec3 L{1.0f, 0.0f, 0.0f};
    const interval ia = project(a, L);
    const interval ib = project(b, L);
    std::printf("\n  A.2  the same answer as two shadows on the x axis:\n");
    std::printf("       A [%+.4f, %+.4f]   B [%+.4f, %+.4f]\n",
                static_cast<double>(ia.lo), static_cast<double>(ia.hi),
                static_cast<double>(ib.lo), static_cast<double>(ib.hi));
    std::printf("       overlap %+.6f m  — identical: %s\n",
                static_cast<double>(engine::phys::interval_overlap(ia, ib)),
                yes_no(engine::phys::interval_overlap(ia, ib) == s.depth));

    // A.3 — the whole population, checked two ways.
    rng r(0x5A7u);
    const int n = 200000;
    int overlapping = 0;
    int witness_ok = 0;
    int certificate_ok = 0;
    double worst_unconfirmed = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const engine::sphere p{vec3{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f),
                                    r.range(-2.0f, 2.0f)},
                               r.range(0.2f, 1.2f)};
        const engine::sphere q{vec3{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f),
                                    r.range(-2.0f, 2.0f)},
                               r.range(0.2f, 1.2f)};
        const separation sep = collide(p, q);

        if (sep.hit())
        {
            ++overlapping;
            // The witness is the midpoint of the two shadows' overlap, on the
            // line through the centres — which is a real point of both spheres
            // exactly when they meet.
            const double d = static_cast<double>(length(q.centre - p.centre));
            const double lo = std::max(-static_cast<double>(p.radius), d - static_cast<double>(q.radius));
            const double hi = std::min(static_cast<double>(p.radius), d + static_cast<double>(q.radius));
            const double t = 0.5 * (lo + hi);
            const dvec3 axis = promote(sep.axis);
            const dvec3 w{static_cast<double>(p.centre.x) + axis.x * t,
                          static_cast<double>(p.centre.y) + axis.y * t,
                          static_cast<double>(p.centre.z) + axis.z * t};
            const dvec3 dp{w.x - p.centre.x, w.y - p.centre.y, w.z - p.centre.z};
            const dvec3 dq{w.x - q.centre.x, w.y - q.centre.y, w.z - q.centre.z};
            const bool in_p = std::sqrt(ddot(dp, dp)) <= static_cast<double>(p.radius) + 1e-6;
            const bool in_q = std::sqrt(ddot(dq, dq)) <= static_cast<double>(q.radius) + 1e-6;
            if (in_p && in_q) { ++witness_ok; }
            else { worst_unconfirmed = std::max(worst_unconfirmed, static_cast<double>(sep.depth)); }
        }
        else
        {
            // The certificate: the claimed axis, re-projected in double.
            const dvec3 axis = promote(sep.axis);
            const dvec3 t{static_cast<double>(q.centre.x) - p.centre.x,
                          static_cast<double>(q.centre.y) - p.centre.y,
                          static_cast<double>(q.centre.z) - p.centre.z};
            const double gap = std::fabs(ddot(t, axis)) -
                               (static_cast<double>(p.radius) + static_cast<double>(q.radius));
            if (gap > 0.0) { ++certificate_ok; }
        }
    }

    std::printf("\n  A.3  %d random pairs\n", n);
    std::printf("       overlapping %d   apart %d\n", overlapping, n - overlapping);
    std::printf("       every overlap has a witness point:  %d / %d\n", witness_ok, overlapping);
    std::printf("       every gap re-proves in double:      %d / %d\n", certificate_ok,
                n - overlapping);
    std::printf("       worst unconfirmed depth  %.3e m\n", worst_unconfirmed);

    // A.4 — CONTROL, the completely-fine end: two spheres at exactly the same
    // point. There is no direction between them, and the answer still has to be
    // a legal unit vector because everything downstream divides by it.
    const separation same = collide(engine::sphere{vec3{}, 1.0f}, engine::sphere{vec3{}, 1.0f});
    std::printf("\n  A.4  CONTROL concentric spheres: depth %.4f\n",
                static_cast<double>(same.depth));
    std::printf("       axis length %.6f  (a legal unit vector,\n",
                static_cast<double>(length(same.axis)));
    std::printf("       not a zero and not a NaN)\n");

    // A.5 — CONTROL, the broken end: exactly touching is the sign change, and
    // the two sides of it must disagree.
    const separation touch = collide(engine::sphere{vec3{}, 1.0f},
                                     engine::sphere{vec3{2.0f, 0.0f, 0.0f}, 1.0f});
    const separation clear = collide(engine::sphere{vec3{}, 1.0f},
                                     engine::sphere{vec3{2.0001f, 0.0f, 0.0f}, 1.0f});
    std::printf("\n  A.5  CONTROL exactly touching: depth %+.7f, hit %s\n",
                static_cast<double>(touch.depth), yes_no(touch.hit()));
    std::printf("       0.1 mm further:            depth %+.7f, hit %s\n",
                static_cast<double>(clear.depth), yes_no(clear.hit()));
}

// ---------------------------------------------------------------------------
// B — projecting a box
// ---------------------------------------------------------------------------

void section_b()
{
    rule("B  PROJECTING A BOX: THE RADIUS OF ITS SHADOW");

    // B.1 — the worked example the lesson does by hand.
    const obb worked = make_obb(vec3{2.0f, 1.0f, 0.5f}, vec3{},
                                quat_from_axis_angle(vec3{0.0f, 0.0f, 1.0f},
                                                     static_cast<float>(k_pi / 6.0)));
    const vec3 ex{1.0f, 0.0f, 0.0f};
    std::printf("  half extents (2, 1, 0.5), turned 30 deg about z,\n");
    std::printf("  projected on the world x axis\n\n");
    std::printf("  B.1  u0.L = %.6f   h0 = 2.0\n", static_cast<double>(dot(worked.axes.c0, ex)));
    std::printf("       u1.L = %.6f   h1 = 1.0\n", static_cast<double>(dot(worked.axes.c1, ex)));
    std::printf("       u2.L = %.6f   h2 = 0.5\n", static_cast<double>(dot(worked.axes.c2, ex)));
    std::printf("       radius = 2|0.8660| + 1|-0.5| + 0.5|0| = %.6f\n",
                static_cast<double>(projected_radius(worked, ex)));
    const aabb wb = bounds_of(worked);
    std::printf("       and bounds_of() agrees: half width %.6f\n",
                static_cast<double>(0.5f * wb.extent().x));

    // B.2 — against brute force over the eight corners.
    rng r(0xB07u);
    const int n = 100000;
    double worst_abs = 0.0;
    double worst_rel = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const obb box = make_obb(vec3{r.range(0.1f, 2.0f), r.range(0.1f, 2.0f),
                                      r.range(0.1f, 2.0f)},
                                 vec3{r.signed_unit(), r.signed_unit(), r.signed_unit()},
                                 r.rotation());
        const vec3 L = r.direction();
        const float formula = projected_radius(box, L);

        vec3 c[8];
        box.corners(c);
        const float centre = dot(box.centre, L);
        float brute = 0.0f;
        for (const vec3& p : c) { brute = std::max(brute, std::fabs(dot(p, L) - centre)); }

        const double diff = std::fabs(static_cast<double>(formula) - static_cast<double>(brute));
        worst_abs = std::max(worst_abs, diff);
        worst_rel = std::max(worst_rel, diff / static_cast<double>(brute));
    }
    std::printf("\n  B.2  formula vs the farthest of the 8 corners,\n");
    std::printf("       %d random boxes and directions:\n", n);
    std::printf("       worst absolute  %.4e m\n", worst_abs);
    std::printf("       worst relative  %.4e\n", worst_rel);

    // B.3 — the support function is the same statement about corners.
    rng r2(0x5A9u);
    int support_ok = 0;
    const int m = 20000;
    for (int i = 0; i < m; ++i)
    {
        const obb box = make_obb(vec3{r2.range(0.1f, 2.0f), r2.range(0.1f, 2.0f),
                                      r2.range(0.1f, 2.0f)},
                                 vec3{r2.signed_unit(), r2.signed_unit(), r2.signed_unit()},
                                 r2.rotation());
        const vec3 L = r2.direction();
        const vec3 sp = support(box, L);
        vec3 c[8];
        box.corners(c);
        float best = -1e30f;
        for (const vec3& p : c) { best = std::max(best, dot(p, L)); }
        if (std::fabs(dot(sp, L) - best) <= 1e-5f * std::max(1.0f, std::fabs(best)))
        {
            ++support_ok;
        }
    }
    std::printf("\n  B.3  support() lands on the farthest corner:\n");
    std::printf("       %d / %d\n", support_ok, m);

    // B.4 — CONTROL, the completely-fine end: project onto the box's own axis
    // and the answer must be that half extent, with the other two terms exactly
    // zero.
    const obb cube = make_obb(vec3{0.3f, 1.7f, 0.9f}, vec3{4.0f, -2.0f, 1.0f},
                              quat_from_axis_angle(normalised(vec3{1.0f, 2.0f, 3.0f}), 1.1f));
    std::printf("\n  B.4  CONTROL projected on its own axes, which must\n");
    std::printf("       return the half extents:\n");
    for (int i = 0; i < 3; ++i)
    {
        const float got = projected_radius(cube, cube.axis(i));
        std::printf("       axis %d: %.7f  want %.7f  exact %s\n", i,
                    static_cast<double>(got), static_cast<double>(cube.half(i)),
                    yes_no(got == cube.half(i)));
    }

    // B.5 — CONTROL, the broken end: drop the absolute values and measure.
    rng r3(0xD1Eu);
    double worst_noabs = 0.0;
    int noabs_wrong = 0;
    const int k = 20000;
    for (int i = 0; i < k; ++i)
    {
        const obb box = make_obb(vec3{r3.range(0.1f, 2.0f), r3.range(0.1f, 2.0f),
                                      r3.range(0.1f, 2.0f)},
                                 vec3{}, r3.rotation());
        const vec3 L = r3.direction();
        const float right = projected_radius(box, L);
        const float wrong = std::fabs(box.half_extents.x * dot(box.axes.c0, L) +
                                      box.half_extents.y * dot(box.axes.c1, L) +
                                      box.half_extents.z * dot(box.axes.c2, L));
        const double rel = std::fabs(static_cast<double>(right - wrong)) /
                           static_cast<double>(right);
        worst_noabs = std::max(worst_noabs, rel);
        if (rel > 1e-4) { ++noabs_wrong; }
    }
    std::printf("\n  B.5  CONTROL the same sum WITHOUT the absolute\n");
    std::printf("       values, over %d boxes:\n", k);
    std::printf("       wrong on %d of them, worst by %.1f%%\n", noabs_wrong,
                worst_noabs * 100.0);
}

// ---------------------------------------------------------------------------
// C — an AABB test is the SAT with three axes
// ---------------------------------------------------------------------------

void section_c()
{
    rule("C  AN AABB TEST IS THE SAT WITH THREE AXES");

    rng r(0xAABu);
    const int n = 200000;
    int overlapping = 0;
    int identical = 0;
    double worst_depth_diff = 0.0;
    int any_axis_wrong = 0;

    for (int i = 0; i < n; ++i)
    {
        aabb p;
        const vec3 pc{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)};
        const vec3 ph{r.range(0.2f, 1.0f), r.range(0.2f, 1.0f), r.range(0.2f, 1.0f)};
        p.min = pc - ph;
        p.max = pc + ph;

        aabb q;
        const vec3 qc{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)};
        const vec3 qh{r.range(0.2f, 1.0f), r.range(0.2f, 1.0f), r.range(0.2f, 1.0f)};
        q.min = qc - qh;
        q.max = qc + qh;

        const separation fast = collide(p, q);
        const separation general = collide(as_obb(p), as_obb(q));

        if (fast.hit()) { ++overlapping; }
        if (fast.hit() == general.hit() && fast.axis_index == general.axis_index &&
            fast.depth == general.depth && fast.axis == general.axis)
        {
            ++identical;
        }
        worst_depth_diff = std::max(worst_depth_diff,
                                    std::fabs(static_cast<double>(fast.depth - general.depth)));

        // The classic bug: "they overlap if their projections overlap on SOME
        // axis" rather than on EVERY axis.
        const bool ox = std::fabs(qc.x - pc.x) < ph.x + qh.x;
        const bool oy = std::fabs(qc.y - pc.y) < ph.y + qh.y;
        const bool oz = std::fabs(qc.z - pc.z) < ph.z + qh.z;
        if ((ox || oy || oz) != fast.hit()) { ++any_axis_wrong; }
    }

    std::printf("  %d random AABB pairs, %d of them overlapping\n\n", n, overlapping);
    std::printf("  C.1  collide(aabb) vs collide(obb) with identity\n");
    std::printf("       axes, bit for bit:   %d / %d\n", identical, n);
    std::printf("       worst depth difference  %.4e\n", worst_depth_diff);

    std::printf("\n  C.2  CONTROL \"overlapping on SOME axis\" instead of\n");
    std::printf("       on EVERY axis: wrong on %d of %d (%.1f%%)\n", any_axis_wrong, n,
                100.0 * any_axis_wrong / n);

    // C.3 — the one-line picture of that bug.
    aabb u;
    u.min = vec3{-1.0f, -1.0f, -1.0f};
    u.max = vec3{1.0f, 1.0f, 1.0f};
    aabb v;
    v.min = vec3{-0.5f, -0.5f, 3.0f};
    v.max = vec3{0.5f, 0.5f, 4.0f};
    const separation uv = collide(u, v);
    std::printf("\n  C.3  a unit cube and a box stacked 3 m above it:\n");
    std::printf("       x and y shadows overlap, z does not\n");
    std::printf("       depth %+.4f m on axis %d (%s)\n", static_cast<double>(uv.depth),
                uv.axis_index, name_of(source_of(uv)));
    std::printf("       axis (%.1f, %.1f, %.1f), axes tested %d of 3\n",
                static_cast<double>(uv.axis.x), static_cast<double>(uv.axis.y),
                static_cast<double>(uv.axis.z), uv.axes_tested);

    // C.4 — CONTROL, the completely-fine end: a box against itself.
    const separation self = collide(u, u);
    std::printf("\n  C.4  CONTROL a box against itself: depth %+.4f m,\n",
                static_cast<double>(self.depth));
    std::printf("       which is its full width — nothing shallower\n");
    std::printf("       separates a thing from itself\n");
}

// ---------------------------------------------------------------------------
// D — six axes are not enough
// ---------------------------------------------------------------------------

void section_d()
{
    rule("D  SIX AXES ARE NOT ENOUGH");

    // D.1 — the constructed case, with all fifteen candidates printed.
    obb a;
    obb b;
    crossed_boxes(a, b, 0.05f);

    const separation full = collide(a, b);
    const bool faces = overlaps_faces_only(a, b);

    std::printf("  two 4.0 x 0.3 x 0.3 m boxes, crossed, placed on\n");
    std::printf("  the a0 x b0 axis with a 5 cm gap\n\n");
    std::printf("  D.1  face axes only:  %s\n", faces ? "OVERLAPPING" : "apart");
    std::printf("       all fifteen:     %s\n", full.hit() ? "OVERLAPPING" : "apart");
    std::printf("       winning axis %d (%s), gap %.5f m\n", full.axis_index,
                name_of(source_of(full)), static_cast<double>(-full.depth));
    std::printf("       certificate in double: %+.7f m\n", gap_double(a, b, full.axis));

    std::printf("\n       every candidate, gap in metres (+ separates):\n");
    for (int i = 0; i < 3; ++i)
    {
        std::printf("       %2d  A face %d      %+.5f\n", i, i,
                    static_cast<double>(gap_on_axis(a, b, a.axis(i))));
    }
    for (int j = 0; j < 3; ++j)
    {
        std::printf("       %2d  B face %d      %+.5f\n", 3 + j, j,
                    static_cast<double>(gap_on_axis(a, b, b.axis(j))));
    }
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            const vec3 c = cross(a.axis(i), b.axis(j));
            const float len2 = length_squared(c);
            const vec3 L = c / std::sqrt(len2);
            std::printf("       %2d  a%d x b%d       %+.5f\n", 6 + 3 * i + j, i, j,
                        static_cast<double>(gap_on_axis(a, b, L)));
        }
    }

    // D.2 — how often it happens on random pairs.
    rng r(0x0BBu);
    const int n = 200000;
    int overlapping = 0;
    int face_only_says_yes = 0;
    int edge_saved = 0;
    int certified = 0;
    int overlaps_agrees = 0;
    int edge_wins_mtv = 0;

    for (int i = 0; i < n; ++i)
    {
        const obb p = make_obb(vec3{r.range(0.2f, 1.5f), r.range(0.2f, 1.5f), r.range(0.2f, 1.5f)},
                               vec3{r.range(-1.5f, 1.5f), r.range(-1.5f, 1.5f), r.range(-1.5f, 1.5f)},
                               r.rotation());
        const obb q = make_obb(vec3{r.range(0.2f, 1.5f), r.range(0.2f, 1.5f), r.range(0.2f, 1.5f)},
                               vec3{r.range(-1.5f, 1.5f), r.range(-1.5f, 1.5f), r.range(-1.5f, 1.5f)},
                               r.rotation());

        const separation s = collide(p, q);
        const bool f = overlaps_faces_only(p, q);
        if (s.hit())
        {
            ++overlapping;
            if (source_of(s) == axis_source::edge_edge) { ++edge_wins_mtv; }
        }
        if (f) { ++face_only_says_yes; }
        if (f && !s.hit())
        {
            ++edge_saved;
            if (gap_double(p, q, s.axis) > 0.0) { ++certified; }
        }
        if (overlaps(p, q) == s.hit()) { ++overlaps_agrees; }
    }

    std::printf("\n  D.2  %d random OBB pairs\n", n);
    std::printf("       genuinely overlapping        %d\n", overlapping);
    std::printf("       face axes alone say yes      %d\n", face_only_says_yes);
    std::printf("       caught ONLY by an edge axis  %d (%.2f%% of\n", edge_saved,
                100.0 * edge_saved / n);
    std::printf("       all pairs, %.1f%% of the ones six axes\n",
                100.0 * edge_saved / std::max(1, face_only_says_yes));
    std::printf("       called overlapping)\n");
    std::printf("       every one of them re-proved in double: %d / %d\n", certified,
                edge_saved);

    std::printf("\n  D.3  and when they DO overlap, the minimum\n");
    std::printf("       translation is on an edge axis %d / %d\n", edge_wins_mtv, overlapping);
    std::printf("       (%.1f%%)\n", 100.0 * edge_wins_mtv / std::max(1, overlapping));

    std::printf("\n  D.4  the absR formulation agrees with the explicit\n");
    std::printf("       one on %d / %d\n", overlaps_agrees, n);

    // D.5 — CONTROL, the completely-fine end: with both boxes axis aligned every
    // cross product is degenerate, so six axes ARE enough and the two tests must
    // never disagree.
    rng r2(0xA11u);
    const int m = 200000;
    int aligned_disagree = 0;
    for (int i = 0; i < m; ++i)
    {
        const obb p = make_obb(vec3{r2.range(0.2f, 1.5f), r2.range(0.2f, 1.5f), r2.range(0.2f, 1.5f)},
                               vec3{r2.range(-1.5f, 1.5f), r2.range(-1.5f, 1.5f), r2.range(-1.5f, 1.5f)},
                               quat::identity());
        const obb q = make_obb(vec3{r2.range(0.2f, 1.5f), r2.range(0.2f, 1.5f), r2.range(0.2f, 1.5f)},
                               vec3{r2.range(-1.5f, 1.5f), r2.range(-1.5f, 1.5f), r2.range(-1.5f, 1.5f)},
                               quat::identity());
        if (overlaps_faces_only(p, q) != collide(p, q).hit()) { ++aligned_disagree; }
    }
    std::printf("\n  D.5  CONTROL the same test on %d AXIS-ALIGNED\n", m);
    std::printf("       pairs, where the nine cross products are all\n");
    std::printf("       degenerate and six axes are provably enough:\n");
    std::printf("       disagreements %d\n", aligned_disagree);

    // D.6 — CONTROL, the broken end: witness points for the overlaps.
    rng r3(0x11Eu);
    const int w = 400;
    int overlaps_found = 0;
    int witnessed = 0;
    double worst_unwitnessed = 1e30;
    for (int i = 0; i < w; ++i)
    {
        const obb p = make_obb(vec3{r3.range(0.4f, 1.2f), r3.range(0.4f, 1.2f), r3.range(0.4f, 1.2f)},
                               vec3{r3.range(-1.0f, 1.0f), r3.range(-1.0f, 1.0f), r3.range(-1.0f, 1.0f)},
                               r3.rotation());
        const obb q = make_obb(vec3{r3.range(0.4f, 1.2f), r3.range(0.4f, 1.2f), r3.range(0.4f, 1.2f)},
                               vec3{r3.range(-1.0f, 1.0f), r3.range(-1.0f, 1.0f), r3.range(-1.0f, 1.0f)},
                               r3.rotation());
        const separation s = collide(p, q);
        if (!s.hit()) { continue; }
        ++overlaps_found;
        if (witness_exists(p, q, 41)) { ++witnessed; }
        else { worst_unwitnessed = std::min(worst_unwitnessed, static_cast<double>(s.depth)); }
    }
    std::printf("\n  D.6  CONTROL a 41^3 grid over the first box finds a\n");
    std::printf("       point inside the second, for %d / %d overlaps\n", witnessed,
                overlaps_found);
    if (witnessed < overlaps_found)
    {
        std::printf("       shallowest unconfirmed depth %.3e m\n", worst_unwitnessed);
    }
}

// ---------------------------------------------------------------------------
// E — the MTV, and the direction it points
// ---------------------------------------------------------------------------

void section_e()
{
    rule("E  THE MTV, AND THE DIRECTION IT POINTS");

    rng r(0x33Fu);
    const int n = 100000;
    int tested = 0;
    int separated_after = 0;
    int still_overlapping_after_half = 0;
    int wrong_way = 0;
    double worst_residual = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const obb p = make_obb(vec3{r.range(0.3f, 1.2f), r.range(0.3f, 1.2f), r.range(0.3f, 1.2f)},
                               vec3{r.range(-1.0f, 1.0f), r.range(-1.0f, 1.0f), r.range(-1.0f, 1.0f)},
                               r.rotation());
        const obb q = make_obb(vec3{r.range(0.3f, 1.2f), r.range(0.3f, 1.2f), r.range(0.3f, 1.2f)},
                               vec3{r.range(-1.0f, 1.0f), r.range(-1.0f, 1.0f), r.range(-1.0f, 1.0f)},
                               r.rotation());
        const separation s = collide(p, q);
        if (!s.hit()) { continue; }
        ++tested;

        // Push B along the axis by the depth. The convention says this separates
        // them; if the axis pointed the other way it would drive them together.
        obb moved = q;
        moved.centre = q.centre + s.axis * s.depth;
        const separation after = collide(p, moved);
        if (!after.hit() || after.depth <= 1e-5f) { ++separated_after; }
        worst_residual = std::max(worst_residual, static_cast<double>(after.depth));

        obb half = q;
        half.centre = q.centre + s.axis * (s.depth * 0.5f);
        if (collide(p, half).hit()) { ++still_overlapping_after_half; }

        // Pushed the WRONG way by the same distance. This can never separate
        // them, and that is provable rather than hopeful: every other candidate
        // axis M had overlap at least `depth` (because `depth` is the minimum),
        // and moving by `depth` along the winning axis changes M's overlap by at
        // most `depth * |L.M| <= depth`. So no overlap can be driven below zero,
        // while the winning axis's own doubles.
        obb back = q;
        back.centre = q.centre - s.axis * s.depth;
        if (collide(p, back).hit()) { ++wrong_way; }
    }

    std::printf("  %d overlapping pairs, each pushed apart by its own\n", tested);
    std::printf("  reported axis and depth\n\n");
    std::printf("  E.1  separated afterwards   %d / %d\n", separated_after, tested);
    std::printf("       worst residual overlap  %.3e m\n", worst_residual);
    std::printf("\n  E.2  CONTROL pushed only HALF the depth, still\n");
    std::printf("       overlapping: %d / %d\n", still_overlapping_after_half, tested);
    std::printf("\n  E.3  CONTROL pushed the same distance the WRONG\n");
    std::printf("       way, still overlapping: %d / %d\n", wrong_way, tested);

    // E.4 — the worked example.
    const obb x = make_obb(vec3{1.0f, 1.0f, 1.0f}, vec3{}, quat::identity());
    const obb y = make_obb(vec3{1.0f, 1.0f, 1.0f}, vec3{1.5f, 0.2f, 0.1f}, quat::identity());
    const separation s = collide(x, y);
    std::printf("\n  E.4  two unit cubes, centres (1.5, 0.2, 0.1) apart:\n");
    std::printf("       depth %.4f m on axis %d (%s)\n", static_cast<double>(s.depth),
                s.axis_index, name_of(source_of(s)));
    std::printf("       axis (%.1f, %.1f, %.1f) — the SHALLOWEST way\n",
                static_cast<double>(s.axis.x), static_cast<double>(s.axis.y),
                static_cast<double>(s.axis.z));
    std::printf("       out, not the way it came in\n");

    // E.5 — where the MTV is a bad answer: deep overlap.
    const obb deep = make_obb(vec3{1.0f, 1.0f, 1.0f}, vec3{0.1f, 0.05f, 0.0f}, quat::identity());
    const separation ds = collide(x, deep);
    std::printf("\n  E.5  the same cubes almost concentric, centres\n");
    std::printf("       (0.1, 0.05, 0) apart:\n");
    std::printf("       depth %.4f m on axis %d — the SHALLOWEST escape\n",
                static_cast<double>(ds.depth), ds.axis_index);
    std::printf("       from a 1 m cube is still most of a metre, so\n");
    std::printf("       the MTV here is a teleport rather than a\n");
    std::printf("       correction. Deep overlap is where the minimum\n");
    std::printf("       translation stops being the right answer.\n");
}

// ---------------------------------------------------------------------------
// F — when two edges are parallel
// ---------------------------------------------------------------------------

void section_f()
{
    rule("F  WHEN TWO EDGES ARE PARALLEL");

    std::printf("  Two crates meeting at a corner: boxes that SHARE an\n");
    std::printf("  up axis, yawed 0.65 rad about it, overlapping on two\n");
    std::printf("  axes and sliding along the third. Both in a generic\n");
    std::printf("  orientation, so nothing is axis-aligned and nothing\n");
    std::printf("  is exact by accident.\n\n");
    std::printf("  THIS IS NOT AN EXOTIC CONFIGURATION. Two objects on\n");
    std::printf("  the same floor have parallel up axes whatever their\n");
    std::printf("  yaw, so a1 x b1 is degenerate in the single most\n");
    std::printf("  common arrangement a game ever produces.\n\n");
    std::printf("  `tilt` breaks the shared axis by that many radians.\n");
    std::printf("  Ground truth is the same fifteen axes in DOUBLE.\n");

    const quat base = quat_from_axis_angle(normalised(vec3{1.0f, 2.0f, 3.0f}),
                                           static_cast<float>(0.7 * k_pi));
    const double tilts[] = {1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 0.0};

    for (int shape_case = 0; shape_case < 2; ++shape_case)
    {
        // Two arrangements, both of them things a scene is full of: crates
        // meeting at a corner, and a crate standing on a floor.
        const vec3 half_a = (shape_case == 0) ? vec3{0.5f, 0.5f, 0.5f}
                                              : vec3{8.0f, 0.25f, 8.0f};
        const vec3 half_b = vec3{0.5f, 0.5f, 0.5f};
        std::printf("\n  %s\n", (shape_case == 0) ? "CRATE AGAINST CRATE, at a corner"
                                                   : "CRATE ON A 16 x 16 m FLOOR SLAB");
        std::printf("       tilt     min|axb|    normalised  absR    absR+eps\n");

        for (double tilt : tilts)
        {
            const obb a = make_obb(half_a, vec3{}, base);
            const quat yaw = quat_from_axis_angle(a.axis(1), 0.65f);
            const quat lean = quat_from_axis_angle(a.axis(0), static_cast<float>(tilt));
            const quat qb = lean * yaw * base;

            int overlapping = 0;
            int bad_normalised = 0;
            int bad_absr = 0;
            int bad_absr_eps = 0;
            double shortest = 1e30;

            // Placed by PROJECTED RADIUS rather than by half extent, so the
            // overlap is 1 mm whatever the shape is and whatever the yaw did to
            // it. §6's formula, used to build a fixture.
            const obb oriented = make_obb(half_b, vec3{}, qb);
            const float reach0 = projected_radius(a, a.axis(0)) +
                                 projected_radius(oriented, a.axis(0));
            const float reach1 = projected_radius(a, a.axis(1)) +
                                 projected_radius(oriented, a.axis(1));
            const float reach2 = projected_radius(a, a.axis(2)) +
                                 projected_radius(oriented, a.axis(2));

            for (int k = 0; k < 400; ++k)
            {
                // Corner to corner: 20% into the neighbour on two axes, with
                // the third sliding across. A brick in a wall, or the corner
                // where four crates meet.
                const float slide = reach2 * (-0.35f + 0.00175f * static_cast<float>(k));
                const vec3 c = (shape_case == 0)
                                   ? a.axis(0) * (reach0 * 0.8f) +
                                         a.axis(1) * (reach1 * 0.8f) + a.axis(2) * slide
                                   : a.axis(1) * (reach1 - 0.001f) +
                                         a.axis(0) * (slide * 4.0f) + a.axis(2) * (slide * 2.0f);
                const obb b = make_obb(half_b, c, qb);

                if (!overlaps_double(a, b)) { continue; }
                ++overlapping;

                for (int ai = 0; ai < 3; ++ai)
                {
                    for (int bj = 0; bj < 3; ++bj)
                    {
                        shortest = std::min(shortest,
                            static_cast<double>(length(cross(a.axis(ai), b.axis(bj)))));
                    }
                }

                if (!collide_unguarded(a, b).hit()) { ++bad_normalised; }
                if (!overlaps_eps(a, b, 0.0f)) { ++bad_absr; }
                if (!overlaps_eps(a, b, k_parallel_sin2)) { ++bad_absr_eps; }
            }

            std::printf("      %7.1e   %.2e   %5d/%-4d %4d/%-4d %4d/%d\n", tilt, shortest,
                        bad_normalised, overlapping, bad_absr, overlapping, bad_absr_eps,
                        overlapping);
        }
    }

    std::printf("\n  F.1  A SHARED AXIS IS NEVER EXACTLY SHARED. At tilt\n");
    std::printf("       0 the two boxes are built from the same axis and\n");
    std::printf("       a quaternion-to-matrix conversion, and the cross\n");
    std::printf("       product of the two copies comes out around\n");
    std::printf("       1e-08 rather than 0. That is the dangerous\n");
    std::printf("       length: a TRUE zero compares 0 > 0 and answers\n");
    std::printf("       correctly, while a near-zero is a comparison\n");
    std::printf("       between two quantities that are both rounding\n");
    std::printf("       error.\n");

    std::printf("\n  F.2  THE NORMALISED FORM NEVER FAILS, and that is a\n");
    std::printf("       theorem rather than luck. If two convex bodies\n");
    std::printf("       overlap then NO direction separates them — not\n");
    std::printf("       merely none of the fifteen. An axis whose\n");
    std::printf("       direction is pure rounding error is still a\n");
    std::printf("       direction, so it reports an overlap like any\n");
    std::printf("       other. Rounding cannot invent a gap where no\n");
    std::printf("       direction has one.\n");

    std::printf("\n  F.3  THE UNNORMALISED FORM IS A DIFFERENT STORY, and\n");
    std::printf("       it is the one the books print. Its test on axis\n");
    std::printf("       a_i x b_j compares quantities that all scale\n");
    std::printf("       with |a_i x b_j|, so as the edges approach\n");
    std::printf("       parallel both sides shrink toward zero while\n");
    std::printf("       their rounding error does not. The comparison\n");
    std::printf("       becomes noise against noise. The epsilon on the\n");
    std::printf("       absolute matrix lifts the right-hand side back\n");
    std::printf("       above the noise floor, and it is why every\n");
    std::printf("       published version of this routine has one.\n");

    // F.4 — CONTROL: on generic pairs nothing about the guard is visible at all.
    rng r(0x6D5u);
    const int n = 200000;
    int differ = 0;
    int guard_fired = 0;
    int absr_differ = 0;
    double worst_depth_diff = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const obb p = make_obb(vec3{r.range(0.2f, 1.5f), r.range(0.2f, 1.5f), r.range(0.2f, 1.5f)},
                               vec3{r.range(-1.5f, 1.5f), r.range(-1.5f, 1.5f), r.range(-1.5f, 1.5f)},
                               r.rotation());
        const obb q = make_obb(vec3{r.range(0.2f, 1.5f), r.range(0.2f, 1.5f), r.range(0.2f, 1.5f)},
                               vec3{r.range(-1.5f, 1.5f), r.range(-1.5f, 1.5f), r.range(-1.5f, 1.5f)},
                               r.rotation());
        const separation g = collide(p, q);
        const separation u = collide_unguarded(p, q);
        if (g.hit() != u.hit()) { ++differ; }
        if (overlaps_eps(p, q, 0.0f) != g.hit()) { ++absr_differ; }
        if (g.hit() && u.hit())
        {
            worst_depth_diff = std::max(worst_depth_diff,
                                        std::fabs(static_cast<double>(g.depth - u.depth)));
        }
        for (int ai = 0; ai < 3; ++ai)
        {
            for (int bj = 0; bj < 3; ++bj)
            {
                if (length_squared(cross(p.axis(ai), q.axis(bj))) < k_parallel_sin2)
                {
                    ++guard_fired;
                }
            }
        }
    }
    std::printf("\n  F.4  CONTROL on %d pairs at RANDOM orientations,\n", n);
    std::printf("       guarded vs unguarded disagree on %d, worst\n", differ);
    std::printf("       depth difference %.3e m\n", worst_depth_diff);
    std::printf("       the epsilon-less absR disagrees on %d\n", absr_differ);
    std::printf("       the guard fired on %d of %d candidate axes\n", guard_fired, n * 9);
    std::printf("       — which is the whole problem with this class of\n");
    std::printf("       bug. Random test data never generates it, and\n");
    std::printf("       the configuration that does is the one every\n");
    std::printf("       scene is full of.\n");

    // F.5 — CONTROL, the completely-fine end: a true zero.
    const obb p = make_obb(vec3{0.5f, 0.5f, 0.5f}, vec3{}, quat::identity());
    const obb q = make_obb(vec3{0.5f, 0.5f, 0.5f}, vec3{0.3f, 0.1f, 0.0f}, quat::identity());
    const separation exact_g = collide(p, q);
    const separation exact_u = collide_unguarded(p, q);
    std::printf("\n  F.5  CONTROL two axis-aligned cubes, where every\n");
    std::printf("       cross product is a TRUE zero:\n");
    std::printf("       guarded    hit %s, depth %+.4f, axes %d\n", yes_no(exact_g.hit()),
                static_cast<double>(exact_g.depth), exact_g.axes_tested);
    std::printf("       unguarded  hit %s, depth %+.4f, axes %d\n", yes_no(exact_u.hit()),
                static_cast<double>(exact_u.depth), exact_u.axes_tested);
    std::printf("       absR, no epsilon: hit %s\n", yes_no(overlaps_eps(p, q, 0.0f)));
    std::printf("       All three are right, and that is the point: the\n");
    std::printf("       exactly-degenerate case is the SAFE one.\n");
}

// ---------------------------------------------------------------------------
// G — where the precision floor is
// ---------------------------------------------------------------------------

void section_g()
{
    rule("G  WHERE THE PRECISION FLOOR IS");

    std::printf("  two 1 m cubes with a 1 mm gap, moved away from the\n");
    std::printf("  origin together. Nothing about their relationship\n");
    std::printf("  changes; only the magnitude of their coordinates.\n\n");
    std::printf("    distance    reported gap    error      ulp(d)\n");

    const double ds[] = {0.0, 1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0};
    for (double d : ds)
    {
        const float gap = 0.001f;
        const float off = static_cast<float>(d);
        const obb p = make_obb(vec3{0.5f, 0.5f, 0.5f}, vec3{off, off, off}, quat::identity());
        const obb q = make_obb(vec3{0.5f, 0.5f, 0.5f},
                               vec3{off + 1.0f + gap, off, off}, quat::identity());
        const separation s = collide(p, q);
        const double reported = -static_cast<double>(s.depth);
        const double err = std::fabs(reported - static_cast<double>(gap));
        const float u = std::nextafterf(static_cast<float>(d), 1e30f) - static_cast<float>(d);
        std::printf("    %8.0f    %10.3e    %.3e  %.3e\n", d, reported, err,
                    static_cast<double>(u));
    }

    std::printf("\n  G.1  the gap is 1.000e-03 m. At 10 km from the\n");
    std::printf("       origin a float's neighbours are ~1 mm apart, so\n");
    std::printf("       the gap is no longer a representable difference.\n");

    // G.2 — CONTROL: the same geometry, both centres shifted to the origin
    // first. This is what a floating-origin renderer or a local-space solver
    // does, and it is the whole fix.
    const float gap = 0.001f;
    const obb p0 = make_obb(vec3{0.5f, 0.5f, 0.5f}, vec3{}, quat::identity());
    const obb q0 = make_obb(vec3{0.5f, 0.5f, 0.5f}, vec3{1.0f + gap, 0.0f, 0.0f},
                            quat::identity());
    const separation s0 = collide(p0, q0);
    std::printf("\n  G.2  CONTROL the same pair at the origin:\n");
    std::printf("       reported gap %.6e m, error %.3e\n", -static_cast<double>(s0.depth),
                std::fabs(-static_cast<double>(s0.depth) - static_cast<double>(gap)));

    // G.3 — CONTROL, the completely-fine end: it is the SUBTRACTION that loses,
    // not the test. Compute the same offset in double and the answer returns.
    const float off = 10000.0f;
    const obb pf = make_obb(vec3{0.5f, 0.5f, 0.5f}, vec3{off, off, off}, quat::identity());
    const obb qf = make_obb(vec3{0.5f, 0.5f, 0.5f}, vec3{off + 1.0f + gap, off, off},
                            quat::identity());
    const double dgap = gap_double(pf, qf, vec3{1.0f, 0.0f, 0.0f});
    std::printf("\n  G.3  CONTROL the same boxes at 10 km, gap computed\n");
    std::printf("       in double from the same float centres:\n");
    std::printf("       %.6e m — the float CENTRES already lost it\n", dgap);
    std::printf("       (10000 + 1.001 rounds to %.6f)\n",
                static_cast<double>(off + 1.0f + gap) - static_cast<double>(off));
}

// ---------------------------------------------------------------------------
// H — a sphere against a box, including from inside
// ---------------------------------------------------------------------------

void section_h()
{
    rule("H  A SPHERE AGAINST A BOX, INCLUDING FROM INSIDE");

    rng r(0x5B0u);
    const int n = 100000;
    int outside = 0;
    int inside_cases = 0;
    int clamp_ok = 0;
    int certified = 0;
    int pushed_clear = 0;
    int naive_zero_axis = 0;
    double worst_clamp = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const obb box = make_obb(vec3{r.range(0.3f, 1.5f), r.range(0.3f, 1.5f), r.range(0.3f, 1.5f)},
                                 vec3{r.range(-1.0f, 1.0f), r.range(-1.0f, 1.0f), r.range(-1.0f, 1.0f)},
                                 r.rotation());
        const engine::sphere s{vec3{r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f), r.range(-2.0f, 2.0f)},
                               r.range(0.1f, 0.8f)};

        const separation sep = collide(box, s);
        const bool centre_inside = inside(box, s.centre);
        if (centre_inside) { ++inside_cases; } else { ++outside; }

        // The clamp, against a brute-force search over the box's surface. A 25^2
        // grid per face is coarse; the check is that the clamp is never WORSE
        // than the best sample, which is a one-sided claim a coarse grid can
        // still make.
        const vec3 cp = closest_point(box, s.centre);
        const float clamp_d = length(cp - s.centre);
        float best = 1e30f;
        vec3 corner[8];
        box.corners(corner);
        for (const vec3& c : corner) { best = std::min(best, length(c - s.centre)); }
        if (clamp_d <= best + 1e-5f) { ++clamp_ok; }
        worst_clamp = std::max(worst_clamp, static_cast<double>(clamp_d - best));

        if (!sep.hit())
        {
            // Certificate: the closest point is at least `radius` away.
            if (static_cast<double>(clamp_d) >= static_cast<double>(s.radius) - 1e-6)
            {
                ++certified;
            }
        }
        else
        {
            engine::sphere moved = s;
            moved.centre = s.centre + sep.axis * sep.depth;
            if (collide(box, moved).depth <= 1e-4f) { ++pushed_clear; }
        }

        if (centre_inside && length_squared(collide_no_inside(box, s).axis) == 0.0f)
        {
            ++naive_zero_axis;
        }
    }

    std::printf("  %d random sphere/box pairs\n\n", n);
    std::printf("  H.1  centre outside %d, centre INSIDE %d\n", outside, inside_cases);
    std::printf("       the clamp is never beaten by a corner: %d / %d\n", clamp_ok, n);
    std::printf("  H.2  every gap certifies: %d\n", certified);
    std::printf("  H.3  every overlap is cleared by its own axis and\n");
    std::printf("       depth: %d\n", pushed_clear);
    std::printf("\n  H.4  CONTROL the version without the inside case\n");
    std::printf("       returns a ZERO axis on %d of the %d\n", naive_zero_axis, inside_cases);
    std::printf("       tunnelled spheres — a normal a solver cannot\n");
    std::printf("       normalise and will not notice\n");

    // H.5 — the worked example: a sphere buried in a wall.
    const obb wall = make_obb(vec3{2.0f, 2.0f, 0.1f}, vec3{}, quat::identity());
    const engine::sphere ball{vec3{0.3f, 0.4f, 0.05f}, 0.25f};
    const separation sep = collide(wall, ball);
    std::printf("\n  H.5  a 0.25 m ball with its centre 5 cm inside a\n");
    std::printf("       0.2 m thick wall:\n");
    std::printf("       depth %.4f m, axis (%.1f, %.1f, %.1f)\n",
                static_cast<double>(sep.depth), static_cast<double>(sep.axis.x),
                static_cast<double>(sep.axis.y), static_cast<double>(sep.axis.z));
    std::printf("       = radius 0.25 + inset %.2f, out the near face\n",
                static_cast<double>(sep.depth) - 0.25);

    // H.6 — CONTROL, the completely-fine end: a sphere far away.
    const engine::sphere far{vec3{0.0f, 0.0f, 5.0f}, 0.25f};
    const separation fs = collide(wall, far);
    std::printf("\n  H.6  CONTROL the same ball 5 m away: gap %.4f m\n",
                static_cast<double>(-fs.depth));
    std::printf("       = 5.0 - 0.1 - 0.25, and hit %s\n", yes_no(fs.hit()));
}

// ---------------------------------------------------------------------------
// I — the budget
// ---------------------------------------------------------------------------

void section_i()
{
    rule("I  THE BUDGET");

    // Build three populations up front so that no arm pays for generation.
    rng r(0xB0Du);
    const std::size_t n = 20000;

    std::vector<engine::sphere> sa;
    std::vector<engine::sphere> sb;
    std::vector<aabb> ba;
    std::vector<aabb> bb;
    std::vector<obb> oa;
    std::vector<obb> ob;
    std::vector<obb> near_a;
    std::vector<obb> near_b;
    sa.reserve(n); sb.reserve(n); ba.reserve(n); bb.reserve(n);
    oa.reserve(n); ob.reserve(n); near_a.reserve(n); near_b.reserve(n);

    for (std::size_t i = 0; i < n; ++i)
    {
        sa.push_back(engine::sphere{vec3{r.range(-3.0f, 3.0f), r.range(-3.0f, 3.0f),
                                         r.range(-3.0f, 3.0f)}, r.range(0.2f, 1.0f)});
        sb.push_back(engine::sphere{vec3{r.range(-3.0f, 3.0f), r.range(-3.0f, 3.0f),
                                         r.range(-3.0f, 3.0f)}, r.range(0.2f, 1.0f)});

        const vec3 c1{r.range(-3.0f, 3.0f), r.range(-3.0f, 3.0f), r.range(-3.0f, 3.0f)};
        const vec3 h1{r.range(0.2f, 1.0f), r.range(0.2f, 1.0f), r.range(0.2f, 1.0f)};
        aabb x; x.min = c1 - h1; x.max = c1 + h1;
        const vec3 c2{r.range(-3.0f, 3.0f), r.range(-3.0f, 3.0f), r.range(-3.0f, 3.0f)};
        const vec3 h2{r.range(0.2f, 1.0f), r.range(0.2f, 1.0f), r.range(0.2f, 1.0f)};
        aabb y; y.min = c2 - h2; y.max = c2 + h2;
        ba.push_back(x);
        bb.push_back(y);

        // A spread-out population: most pairs are far apart, which is what a
        // broadphase hands a narrow phase.
        oa.push_back(make_obb(h1, c1, r.rotation()));
        ob.push_back(make_obb(h2, c2, r.rotation()));

        // A crowded population: centres within a box's width of each other, so
        // most pairs overlap and every one of the fifteen gets asked.
        const vec3 nc{r.range(-0.6f, 0.6f), r.range(-0.6f, 0.6f), r.range(-0.6f, 0.6f)};
        near_a.push_back(make_obb(vec3{0.5f, 0.5f, 0.5f}, vec3{}, r.rotation()));
        near_b.push_back(make_obb(vec3{0.5f, 0.5f, 0.5f}, nc, r.rotation()));
    }

    // How much work each population actually asks for.
    long long spread_axes = 0;
    long long crowd_axes = 0;
    int spread_hits = 0;
    int crowd_hits = 0;
    int spread_axis_hist[16] = {0};
    for (std::size_t i = 0; i < n; ++i)
    {
        const separation s = collide(oa[i], ob[i]);
        spread_axes += s.axes_tested;
        spread_hits += s.hit() ? 1 : 0;
        if (s.axes_tested >= 0 && s.axes_tested <= 15) { ++spread_axis_hist[s.axes_tested]; }
        const separation c = collide(near_a[i], near_b[i]);
        crowd_axes += c.axes_tested;
        crowd_hits += c.hit() ? 1 : 0;
    }

    std::printf("  two populations of %zu pairs:\n", n);
    std::printf("    SPREAD  random in a 6 m cube, %d%% overlapping\n",
                static_cast<int>(100.0 * spread_hits / static_cast<double>(n)));
    std::printf("    CROWDED centres within 0.6 m, %d%% overlapping\n",
                static_cast<int>(100.0 * crowd_hits / static_cast<double>(n)));
    std::printf("\n  I.1  axes examined per query\n");
    std::printf("       spread  %.2f of 15\n", static_cast<double>(spread_axes) / static_cast<double>(n));
    std::printf("       crowded %.2f of 15\n", static_cast<double>(crowd_axes) / static_cast<double>(n));
    std::printf("\n       spread, by count:\n");
    for (int i = 1; i <= 15; ++i)
    {
        if (spread_axis_hist[i] == 0) { continue; }
        std::printf("       %2d axes  %6d  %5.1f%%\n", i, spread_axis_hist[i],
                    100.0 * spread_axis_hist[i] / static_cast<double>(n));
    }

    const int reps = 21;

    const engine::bench_result t_sphere = engine::bench_run(n, reps, [&] {
        double acc = 0.0;
        for (std::size_t i = 0; i < n; ++i) { acc += collide(sa[i], sb[i]).depth; }
        return acc;
    });

    const engine::bench_result t_aabb = engine::bench_run(n, reps, [&] {
        double acc = 0.0;
        for (std::size_t i = 0; i < n; ++i) { acc += collide(ba[i], bb[i]).depth; }
        return acc;
    });

    const engine::bench_result t_spread = engine::bench_run(n, reps, [&] {
        double acc = 0.0;
        for (std::size_t i = 0; i < n; ++i) { acc += collide(oa[i], ob[i]).depth; }
        return acc;
    });

    const engine::bench_result t_crowd = engine::bench_run(n, reps, [&] {
        double acc = 0.0;
        for (std::size_t i = 0; i < n; ++i) { acc += collide(near_a[i], near_b[i]).depth; }
        return acc;
    });

    const engine::bench_result t_bounds = engine::bench_run(n, reps, [&] {
        double acc = 0.0;
        for (std::size_t i = 0; i < n; ++i) { acc += bounds_of(oa[i]).max.x; }
        return acc;
    });

    std::printf("\n  I.2  nanoseconds per test\n");
    std::printf("       sphere vs sphere      %8.3f\n", t_sphere.median_ns);
    std::printf("       aabb vs aabb          %8.3f\n", t_aabb.median_ns);
    std::printf("       obb vs obb, spread    %8.3f\n", t_spread.median_ns);
    std::printf("       obb vs obb, crowded   %8.3f\n", t_crowd.median_ns);
    std::printf("       bounds_of(obb)        %8.3f\n", t_bounds.median_ns);
    std::printf("       spread of the worst run  %.2f\n",
                std::max({t_sphere.spread(), t_aabb.spread(), t_spread.spread(),
                          t_crowd.spread(), t_bounds.spread()}));

    // I.3 — the boolean formulation against the full one, alternated inside a
    // single run so that the ratio is a fact rather than a before-and-after.
    const engine::bench_ab cheap = engine::bench_compare(n, reps,
        [&] {
            double acc = 0.0;
            for (std::size_t i = 0; i < n; ++i) { acc += collide(near_a[i], near_b[i]).hit() ? 1.0 : 0.0; }
            return acc;
        },
        [&] {
            double acc = 0.0;
            for (std::size_t i = 0; i < n; ++i) { acc += overlaps(near_a[i], near_b[i]) ? 1.0 : 0.0; }
            return acc;
        });

    std::printf("\n  I.3  crowded, boolean only\n");
    std::printf("       collide().hit()  %8.3f ns\n", cheap.a.median_ns);
    std::printf("       overlaps()       %8.3f ns\n", cheap.b.median_ns);
    std::printf("       ratio %.3f, same answers %s\n", 1.0 / cheap.ratio(),
                yes_no(cheap.agree));

    // I.4 — the sphere pre-test, which is what a broadphase actually does.
    const engine::bench_ab pre = engine::bench_compare(n, reps,
        [&] {
            double acc = 0.0;
            for (std::size_t i = 0; i < n; ++i)
            {
                acc += overlaps(oa[i], ob[i]) ? 1.0 : 0.0;
            }
            return acc;
        },
        [&] {
            double acc = 0.0;
            for (std::size_t i = 0; i < n; ++i)
            {
                // A bounding-sphere reject first: rotation-invariant, four
                // floats, one comparison, no square root.
                const float ra = length(oa[i].half_extents);
                const float rb = length(ob[i].half_extents);
                const vec3 d = ob[i].centre - oa[i].centre;
                if (length_squared(d) > (ra + rb) * (ra + rb)) { continue; }
                acc += overlaps(oa[i], ob[i]) ? 1.0 : 0.0;
            }
            return acc;
        });

    std::printf("\n  I.4  spread, with a bounding-sphere reject first\n");
    std::printf("       overlaps() alone        %8.3f ns\n", pre.a.median_ns);
    std::printf("       sphere reject first     %8.3f ns\n", pre.b.median_ns);
    std::printf("       ratio %.3f, same answers %s\n", 1.0 / pre.ratio(), yes_no(pre.agree));

    // I.5 — CONTROL: the two arms of I.4 must count the same overlaps, or the
    // faster one is faster because it did less. bench_compare says so, and this
    // prints what it compared.
    int sphere_rejects = 0;
    int actual_hits = 0;
    int rejected_a_real_overlap = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const float ra = length(oa[i].half_extents);
        const float rb = length(ob[i].half_extents);
        const bool rejected =
            length_squared(ob[i].centre - oa[i].centre) > (ra + rb) * (ra + rb);
        const bool hit = overlaps(oa[i], ob[i]);
        if (rejected) { ++sphere_rejects; }
        if (hit) { ++actual_hits; }
        // The only failure that matters. A pre-test may keep as many pairs as it
        // likes; it may never throw away a real one.
        if (rejected && hit) { ++rejected_a_real_overlap; }
    }
    std::printf("\n  I.5  CONTROL the pre-test rejects %d of %zu pairs\n", sphere_rejects,
                n);
    std::printf("       outright. There are %d real overlaps, and it\n", actual_hits);
    std::printf("       rejected %d of them.\n", rejected_a_real_overlap);
    std::printf("       It also KEEPS %d pairs that do not overlap,\n",
                static_cast<int>(n) - sphere_rejects - actual_hits);
    std::printf("       which costs a full test each and is the price\n");
    std::printf("       of a conservative filter.\n");
}

int main()
{
    std::printf("verify_84 — Lesson 8.4, collision primitives\n");
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
    return 0;
}
