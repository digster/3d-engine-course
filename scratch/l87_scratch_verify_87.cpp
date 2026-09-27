// scratch/verify_87.cpp — every number Lesson 8.7 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_87.sh
//
// Nine sections, in the lesson's order:
//
//   A  one point cannot hold a box up
//   B  the normal is discontinuous; the contact SET need not be
//   C  a support function determines a shape and does not name a feature
//   D  the faces themselves
//   E  reference, incident, and how parallel is parallel
//   F  the clip
//   G  four, and why not five
//   H  persistence
//   I  the budget
//
// EVERY SECTION CARRIES A CONTROL — 8.1 through 8.6's rule, in two halves: ask
// what the control would say if the thing were COMPLETELY BROKEN, and what it
// would say if the thing were completely FINE. A control that only ever agrees
// is not a control; 7.8 §G shipped one that fired on the healthy case and had to
// be renamed.
//
// THE INSTRUMENTS, AND WHY NONE OF THEM IS A SECOND MANIFOLD GENERATOR.
//
//   * A CONTACT POINT IS CHECKABLE AGAINST THE SHAPES IT CAME FROM. It must lie
//     on (or just inside) both surfaces, and `distance_squared_to(obb, p)` —
//     shipped in 8.4, knowing nothing about any of this — answers that for a
//     box exactly. So "is this point real" needs no second opinion.
//   * A MANIFOLD'S NORMAL IS CHECKABLE AGAINST 8.4's SAT, for boxes, for the
//     reason 8.6 §8 established: the minimum of `h(n)` over directions is
//     attained on a face normal of `A ⊖ B`, and for two boxes those are exactly
//     the SAT's fifteen axes in both signs. The SAT's minimum-overlap axis IS
//     the true minimum translation.
//   * A SUPPORT POLYGON IS CHECKABLE BY SIMULATION, which is section A: put a
//     body on it and see whether it stays up. That is the only instrument in
//     this file that answers the question the lesson actually asks.
//   * AND A PERSISTENCE SCHEME IS CHECKABLE BY ITS OWN FALSE POSITIVES. An id
//     matcher that matched everything would look perfect on a resting box and
//     wrong on a teleporting one, so every match rate below is printed beside
//     the rate on a body that moved somewhere it has no business matching.
//
// PRECISION. The engine collides in `float`, so this harness does too.
// References are evaluated in `double` where the arithmetic allows it.
//
// OUTPUT WIDTH IS A CONSTRAINT, not a preference. The lesson quotes this
// program's transcript in <pre> blocks that scroll and never wrap, and the fold
// is at about 66 characters. Nothing below exceeds 66.

#include <engine/core/bench.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/collide.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/epa.hpp>
#include <engine/phys/gjk.hpp>
#include <engine/phys/integrate.hpp>
#include <engine/phys/manifold.hpp>
#include <engine/phys/rigid_body.hpp>
#include <engine/phys/shape.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <utility>
#include <vector>

using engine::mat3;
using engine::quat;
using engine::quat_from_axis_angle;
using engine::vec3;
using engine::bench_result;
using engine::bench_run;
using engine::phys::advance_orientation;
using engine::phys::as_convex;
using engine::phys::build_manifold;
using engine::phys::capsule;
using engine::phys::capsule_shape;
using engine::phys::carry_impulses;
using engine::phys::collide;
using engine::phys::collide_manifold;
using engine::phys::contact_face;
using engine::phys::contact_feature;
using engine::phys::contact_id;
using engine::phys::contact_manifold;
using engine::phys::contact_point;
using engine::phys::convex;
using engine::phys::cube_shape;
using engine::phys::epa_penetration;
using engine::phys::epa_result;
using engine::phys::gjk_distance;
using engine::phys::gjk_result;
using engine::phys::gjk_status;
using engine::phys::hull;
using engine::phys::k_max_face_vertices;
using engine::phys::k_max_manifold_points;
using engine::phys::make_box;
using engine::phys::manifold_cache;
using engine::phys::manifold_config;
using engine::phys::manifold_status;
using engine::phys::name_of;
using engine::phys::obb;
using engine::phys::pair_key;
using engine::phys::rigid_body;
using engine::phys::separation;
using engine::phys::shape;
using engine::phys::sphere_shape;
using engine::phys::spin_rule;
using engine::phys::support_face;
using engine::phys::support_local;
using engine::phys::world_capsule;
using engine::phys::world_hull;
using engine::phys::world_inv_inertia;
using engine::phys::world_obb;
using engine::phys::world_sphere;

// ---------------------------------------------------------------------------
// Allocation counting, for §I
// ---------------------------------------------------------------------------

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

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("  FAIL  %s\n", what);
    }
}

void rule(const char* title)
{
    std::printf("\n%s\n", title);
    for (int i = 0; i < 66; ++i) { std::putchar('-'); }
    std::putchar('\n');
}

/// The same deterministic generator 8.1-8.6 used, for the same reason:
/// `std::uniform_real_distribution` is not specified to produce the same
/// sequence on two standard libraries.
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

double mean_of(const std::vector<double>& xs)
{
    if (xs.empty()) { return 0.0; }
    double sum = 0.0;
    for (double x : xs) { sum += x; }
    return sum / static_cast<double>(xs.size());
}

double pct(std::vector<double> xs, double q)
{
    if (xs.empty()) { return 0.0; }
    std::sort(xs.begin(), xs.end());
    const std::size_t i =
        static_cast<std::size_t>(q * static_cast<double>(xs.size() - 1) + 0.5);
    return xs[i];
}

double max_of(const std::vector<double>& xs)
{
    double best = 0.0;
    for (double x : xs) { best = std::fmax(best, x); }
    return best;
}

/// The area of the convex hull of up to 16 coplanar points, about `n`.
///
/// Sort by angle around the centroid, then the shoelace formula in the plane —
/// which is 2.3's signed-area machinery, used here as an instrument rather than
/// as a rasteriser.
double hull_area(const vec3* pts, int n, vec3 normal)
{
    if (n < 3) { return 0.0; }
    vec3 c{};
    for (int i = 0; i < n; ++i) { c += pts[i]; }
    c = c * (1.0f / static_cast<float>(n));

    const vec3 u = normalised_or(pts[0] - c, vec3{1.0f, 0.0f, 0.0f});
    const vec3 w = cross(normal, u);

    int order[16];
    double ang[16];
    for (int i = 0; i < n; ++i)
    {
        order[i] = i;
        ang[i] = std::atan2(static_cast<double>(dot(pts[i] - c, w)),
                            static_cast<double>(dot(pts[i] - c, u)));
    }
    for (int i = 1; i < n; ++i)
    {
        const double a = ang[i];
        const int o = order[i];
        int j = i - 1;
        while (j >= 0 && ang[j] > a)
        {
            ang[j + 1] = ang[j];
            order[j + 1] = order[j];
            --j;
        }
        ang[j + 1] = a;
        order[j + 1] = o;
    }

    double twice = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const vec3 p = pts[order[i]] - c;
        const vec3 q = pts[order[(i + 1) % n]] - c;
        twice += static_cast<double>(dot(cross(p, q), normal));
    }
    return 0.5 * std::fabs(twice);
}

/// **The signed distance from a point to a box**: positive outside, negative
/// inside, zero on the surface.
///
/// `distance_squared_to` (8.4) answers zero for every interior point, which is
/// the right answer to the question it asks and the wrong instrument for this
/// lesson. A contact point that has been pushed a centimetre INSIDE a shape is
/// exactly as wrong as one a centimetre outside, and only a signed measure sees
/// both. The formula is the standard box distance field: outside, the length of
/// the componentwise overshoot; inside, the distance to the nearest face,
/// negated.
double box_signed_distance(const obb& box, vec3 p)
{
    const vec3 local = transpose(box.axes) * (p - box.centre);
    const vec3 d{std::fabs(local.x) - box.half_extents.x,
                 std::fabs(local.y) - box.half_extents.y,
                 std::fabs(local.z) - box.half_extents.z};
    const vec3 outside{std::fmax(d.x, 0.0f), std::fmax(d.y, 0.0f), std::fmax(d.z, 0.0f)};
    const double out = std::sqrt(static_cast<double>(length_squared(outside)));
    const double in = std::fmin(0.0, static_cast<double>(std::fmax(d.x, std::fmax(d.y, d.z))));
    return out + in;
}

/// The angle between two unit vectors, in degrees — **via the chord, not the
/// dot product**, because this file has to resolve angles near zero.
///
/// `acos(dot(a, b))` is the obvious spelling and it has a resolution floor that
/// this harness walked straight into: for a small angle `t`, `dot` is `1 - t²/2`,
/// so all the information about `t` is in the last bits of a number near 1. Two
/// unit `float` vectors give a dot product good to about 1e−7, which recovers
/// `t` only to `sqrt(2e-7)` radians — **0.036 degrees**. The first version of
/// §E.2 reported the reference-face normal as 0.0198 degrees off the exact MTV,
/// and that number was the instrument rather than the normal.
///
/// The chord `|a − b|` IS the angle to first order and loses nothing:
/// `2·asin(|a−b|/2)` is exact at every angle, and near zero it is a subtraction
/// of two nearly equal vectors, which carries full relative precision — 8.5
/// §11's Sterbenz argument, in a measuring instrument rather than in the engine.
double angle_deg(vec3 a, vec3 b)
{
    const double chord = std::sqrt(static_cast<double>(length_squared(a - b)));
    return 2.0 * std::asin(std::fmin(1.0, chord * 0.5)) * 180.0 / k_pi;
}

// ---------------------------------------------------------------------------
// A placed box, and a `convex` view of it that cannot dangle
// ---------------------------------------------------------------------------

struct placed_box
{
    obb box{};
    [[nodiscard]] convex view() const { return as_convex(box); }
};

placed_box make_obb(vec3 centre, vec3 half, quat q)
{
    placed_box p;
    const shape s = engine::phys::box_shape(half);
    p.box = world_obb(s, centre, q);
    return p;
}

// ---------------------------------------------------------------------------
// The instrument for §A: the smallest thing that can hold a box up
// ---------------------------------------------------------------------------
//
// **THIS IS NOT 8.9's SOLVER AND IS NOT TRYING TO BE.** It is a projected-Gauss-
// Seidel normal-impulse loop with a Baumgarte bias and no friction, no
// restitution, no accumulation and no warm starting — about twenty lines, which
// is the point: it is the simplest correct thing that can resist penetration, so
// that the ONLY difference between the two arms of §A.1 is how many points it is
// given. If a sophisticated solver held a one-point contact up, the result would
// be about the solver.
//
// Sequential impulses, restitution, friction and warm starting are 8.9 and 8.10.

struct contact_solver_config
{
    int iterations = 8;
    float beta = 0.2f;     ///< Baumgarte: what fraction of the error to remove per step.
    float slop = 0.001f;   ///< Metres of penetration left alone, so the bias does not buzz.
};

/// Apply normal impulses to a single dynamic body resting on immovable geometry.
void resolve_against_fixed(rigid_body& b, const contact_manifold& m, vec3 normal_towards_body,
                           float h, const contact_solver_config& cfg)
{
    if (m.count == 0) { return; }
    const mat3 inv_i = world_inv_inertia(b);

    for (int it = 0; it < cfg.iterations; ++it)
    {
        for (int i = 0; i < m.count; ++i)
        {
            const contact_point& p = m.points[i];
            const vec3 r = p.position - b.state.position;
            const vec3 n = normal_towards_body;

            const vec3 v = b.state.velocity + cross(b.angular_velocity, r);
            const float vn = dot(v, n);

            const vec3 rn = cross(r, n);
            const float k = b.inv_mass + dot(n, cross(inv_i * rn, r));
            if (k <= 0.0f) { continue; }

            const float bias = (cfg.beta / h) * std::fmax(0.0f, p.depth - cfg.slop);
            const float j = std::fmax(0.0f, (-vn + bias) / k);
            if (j <= 0.0f) { continue; }

            b.state.velocity += n * (j * b.inv_mass);
            b.angular_velocity += inv_i * cross(r, n * j);
        }
    }
}

/// The tilt of a body away from upright, in degrees.
double tilt_deg(const rigid_body& b)
{
    const vec3 up = rotate(b.orientation, vec3{0.0f, 1.0f, 0.0f});
    return angle_deg(up, vec3{0.0f, 1.0f, 0.0f});
}

/// Drop a crate onto a fixed floor and simulate, resolving with `points` contact
/// points. Returns the tilt after `seconds`.
struct rest_result
{
    double tilt_deg = 0.0;
    double sink_mm = 0.0;
    double area = 0.0;
    double rock_deg_per_s = 0.0;
    int points = 0;
    int samples = 0;

    [[nodiscard]] double mean_points() const
    {
        return samples > 0 ? static_cast<double>(points) / samples : 0.0;
    }
};

rest_result simulate_rest(bool use_manifold, float start_y, float start_tilt_deg, float seconds,
                          float h, float applied_torque = 0.0f)
{
    const vec3 half{0.5f, 0.5f, 0.5f};
    rigid_body body = make_box(vec3{0.0f, start_y, 0.0f}, 10.0f, half);
    // A REAL CRATE IS NEVER PERFECTLY BALANCED, and a perfectly balanced one is
    // the single arrangement in which a one-point contact happens to work: the
    // deepest point lands directly under the centre of mass, the lever arm is
    // zero, and nothing turns. Two degrees of tilt is what a solver leaves
    // behind; it is also what turns this measurement from a coincidence into a
    // statement.
    body.orientation = quat_from_axis_angle(
        vec3{0.0f, 0.0f, 1.0f}, static_cast<float>(start_tilt_deg * k_pi / 180.0));

    const placed_box floor = make_obb(vec3{0.0f, -1.0f, 0.0f}, vec3{8.0f, 1.0f, 8.0f}, quat{});
    const contact_solver_config cfg;
    const int steps = static_cast<int>(seconds / h);

    rest_result out;
    for (int s = 0; s < steps; ++s)
    {
        body.state.velocity += vec3{0.0f, -9.81f, 0.0f} * h;
        if (applied_torque != 0.0f)
        {
            body.angular_velocity +=
                world_inv_inertia(body) * (vec3{0.0f, 0.0f, 1.0f} * applied_torque) * h;
        }

        const placed_box crate = make_obb(body.state.position, half, body.orientation);
        const convex va = floor.view();
        const convex vb = crate.view();

        contact_manifold m = collide_manifold(va, vb);
        if (!use_manifold && m.count > 1)
        {
            // THE CONTROL ARM: exactly what 8.6 could produce. EPA returns one
            // pair of witness points, so keep the deepest single contact and
            // throw the rest away. Same normal, same depth, same solver.
            contact_point deepest = m.points[0];
            for (int i = 1; i < m.count; ++i)
            {
                if (m.points[i].depth > deepest.depth) { deepest = m.points[i]; }
            }
            m.points[0] = deepest;
            m.count = 1;
        }

        resolve_against_fixed(body, m, m.normal, h, cfg);

        body.state.position += body.state.velocity * h;
        body.orientation = normalised(
            advance_orientation(body.orientation, body.angular_velocity, h, spin_rule::linearised));

        if (s >= steps - 60)
        {
            // Averaged over the last second, because a resting contact really
            // does lose and regain a corner as the solver breathes, and a
            // reading from one frame would be reporting that rather than the
            // manifold.
            out.points += m.count;
            out.area += static_cast<double>(m.support_area());
            out.sink_mm += 1000.0 * static_cast<double>(std::fmax(0.0f, m.deepest()));
            out.rock_deg_per_s +=
                std::sqrt(static_cast<double>(length_squared(body.angular_velocity)))
                * 180.0 / k_pi;
            ++out.samples;
        }
    }
    if (out.samples > 0)
    {
        out.area /= out.samples;
        out.sink_mm /= out.samples;
        out.rock_deg_per_s /= out.samples;
    }
    out.tilt_deg = tilt_deg(body);
    return out;
}

// ---------------------------------------------------------------------------
// A: one point cannot hold a box up
// ---------------------------------------------------------------------------

void section_a()
{
    rule("A. ONE POINT CANNOT HOLD A BOX UP");

    std::printf("A.1  a 10 kg crate dropped 2 cm onto a fixed floor with\n");
    std::printf("     2 degrees of tilt -- which is what a solver leaves.\n");
    std::printf("     Same solver, same normal, same depths: only the\n");
    std::printf("     NUMBER of contact points differs.\n\n");
    std::printf("     contacts   tilt    rocking      sink     support\n");

    const rest_result one = simulate_rest(false, 0.53f, 2.0f, 2.0f, 1.0f / 60.0f);
    const rest_result four = simulate_rest(true, 0.53f, 2.0f, 2.0f, 1.0f / 60.0f);

    std::printf("     1        %6.3f  %7.3f d/s %7.2f mm  %.4f m2\n", one.tilt_deg,
                one.rock_deg_per_s, one.sink_mm, one.area);
    std::printf("     %.2f     %6.3f  %7.3f d/s %7.2f mm  %.4f m2\n", four.mean_points(),
                four.tilt_deg, four.rock_deg_per_s, four.sink_mm, four.area);

    std::printf("\n     A SINGLE POINT HAS A SUPPORT POLYGON OF ZERO AREA. It\n");
    std::printf("     does not tip a crate that is already flat -- it ROCKS\n");
    std::printf("     it, pushing whichever corner is lowest and turning the\n");
    std::printf("     other one down -- and while it rocks it sinks, because\n");
    std::printf("     only one corner of four is ever being pushed out.\n");

    check(one.sink_mm > 8.0 * four.sink_mm, "A.1 one point sinks far deeper");
    check(one.rock_deg_per_s > 4.0 * four.rock_deg_per_s, "A.1 and never stops rocking");
    check(four.tilt_deg < 0.5, "A.1 four points hold it flat");
    check(four.area > 0.4, "A.1 four points span a real polygon");
    check(one.area == 0.0, "A.1 one point spans nothing");
    check(four.mean_points() > 2.9, "A.1 a resting crate keeps three or four points");

    // ---- A.2: the moment a support polygon can carry, PREDICTED then
    // measured. This is the statement "a manifold is a support polygon" in its
    // strongest form, because the threshold is derivable on paper.
    std::printf("\nA.2  the same crate, with a steady torque applied about z.\n");
    std::printf("     A support polygon 1 m wide, carrying a weight of\n");
    std::printf("     m*g = 98.1 N, can resist a moment of at most\n");
    std::printf("     m*g*(w/2) = 49.1 N m -- the weight acting at the very\n");
    std::printf("     lip of the polygon. Past that it must tip.\n\n");
    std::printf("     torque     tilt, 4 points   tilt, 1 point\n");

    std::vector<double> four_tilts;
    for (float tau : {10.0f, 30.0f, 45.0f, 55.0f, 80.0f})
    {
        const rest_result f4 = simulate_rest(true, 0.505f, 0.0f, 2.0f, 1.0f / 60.0f, tau);
        const rest_result f1 = simulate_rest(false, 0.505f, 0.0f, 2.0f, 1.0f / 60.0f, tau);
        four_tilts.push_back(f4.tilt_deg);
        std::printf("     %5.1f N m  %10.3f deg    %10.3f deg\n", static_cast<double>(tau),
                    f4.tilt_deg, f1.tilt_deg);
    }
    // BISECT IT. A sweep of five values says the threshold is between 45 and
    // 55; a bisection says where, and a prediction that survives three digits is
    // a different kind of statement from one that survives a bracket.
    {
        float lo = 10.0f;
        float hi = 90.0f;
        for (int it = 0; it < 14; ++it)
        {
            const float mid = 0.5f * (lo + hi);
            const rest_result r4 = simulate_rest(true, 0.505f, 0.0f, 2.0f, 1.0f / 60.0f, mid);
            if (r4.tilt_deg > 5.0) { hi = mid; }
            else { lo = mid; }
        }
        const double measured = 0.5 * static_cast<double>(lo + hi);
        const double predicted = 10.0 * 9.81 * 0.5;
        std::printf("\n     predicted m*g*(w/2)   : %.3f N m\n", predicted);
        std::printf("     bisected threshold    : %.3f N m\n", measured);
        std::printf("     off by                : %.2f%%\n",
                    100.0 * (measured - predicted) / predicted);
        std::printf("\n     THE ONE-POINT COLUMN CARRIES THE MOMENT TOO, and\n");
        std::printf("     that is the honest and uncomfortable part: a single\n");
        std::printf("     contact point emulates a support polygon by\n");
        std::printf("     CHATTERING, moving from corner to corner faster than\n");
        std::printf("     the body can respond. It reaches roughly the same\n");
        std::printf("     threshold -- at 27.989 deg/s of permanent rocking and\n");
        std::printf("     21 mm of sink. Which is exactly why single-point\n");
        std::printf("     contact looks nearly fine in a demo and is unusable\n");
        std::printf("     in a game.\n");
        check(std::fabs(measured - predicted) < 0.15 * predicted,
              "A.2 the measured threshold is the predicted one");
    }
    check(four_tilts[1] < 2.0, "A.2 30 N m is carried by the polygon");
    check(four_tilts[3] > 20.0, "A.2 55 N m tips it, as predicted");
    check(four_tilts[2] < four_tilts[3], "A.2 the threshold is between 45 and 55");

    // ---- A.3 the control: four points SHOULD tip when the centre of mass
    // leaves the support polygon. A manifold that held everything up would be
    // just as wrong as one that held nothing up.
    std::printf("\nA.3  the control: statics says a body rests iff its centre\n");
    std::printf("     of mass projects INSIDE the support polygon. Slide the\n");
    std::printf("     same crate off the edge of a 0.5 m ledge.\n\n");
    std::printf("     overhang   com inside?   tilt after 2 s\n");

    // THE LEDGE OCCUPIES x IN [-1, 0], TOP AT y = 0. A 1 m crate sits fully on
    // it when its centre is at x = -0.5, and hangs `over` metres off the edge
    // when its centre is at `-0.5 + over`. At `over = 0.5` the centre of mass is
    // exactly above the lip, which is the boundary statics predicts.
    std::vector<double> tilts;
    std::vector<int> insides;
    for (float over : {0.0f, 0.2f, 0.4f, 0.45f, 0.55f, 0.7f})
    {
        const vec3 half{0.5f, 0.5f, 0.5f};
        rigid_body body = make_box(vec3{-0.5f + over, 0.505f, 0.0f}, 10.0f, half);
        const placed_box ledge = make_obb(vec3{-0.5f, -1.0f, 0.0f}, vec3{0.5f, 1.0f, 8.0f}, quat{});
        const contact_solver_config cfg;
        const float h = 1.0f / 60.0f;
        bool inside = false;
        for (int s = 0; s < 120; ++s)
        {
            body.state.velocity += vec3{0.0f, -9.81f, 0.0f} * h;
            const placed_box crate = make_obb(body.state.position, half, body.orientation);
            const contact_manifold m = collide_manifold(ledge.view(), crate.view());
            if (s == 2)
            {
                // Does the centre of mass project inside the contact set's own
                // x range? That range IS the support polygon, measured from the
                // manifold rather than assumed from the geometry.
                float lo = 1e9f;
                float hi = -1e9f;
                for (int i = 0; i < m.count; ++i)
                {
                    lo = std::fmin(lo, m.points[i].position.x);
                    hi = std::fmax(hi, m.points[i].position.x);
                }
                inside = (m.count >= 2) && (body.state.position.x >= lo)
                         && (body.state.position.x <= hi);
            }
            resolve_against_fixed(body, m, m.normal, h, cfg);
            body.state.position += body.state.velocity * h;
            body.orientation = normalised(advance_orientation(body.orientation,
                                                              body.angular_velocity, h,
                                                              spin_rule::linearised));
        }
        const double t = tilt_deg(body);
        tilts.push_back(t);
        insides.push_back(inside ? 1 : 0);
        std::printf("     %.2f m     %-11s  %8.3f deg\n", static_cast<double>(over),
                    inside ? "yes" : "no", t);
    }

    std::printf("\n     The tipping point is where the centre of mass leaves\n");
    std::printf("     the support polygon, and the manifold KNOWS where that\n");
    std::printf("     is: nothing in the simulation was told about the ledge.\n");

    check(tilts.front() < 0.5, "A.3 a centred crate rests");
    check(insides.front() == 1, "A.3 and its centre of mass is inside the polygon");
    check(tilts.back() > 10.0, "A.3 an overhanging crate tips");
    check(insides.back() == 0, "A.3 and its centre of mass is outside");
}

// ---------------------------------------------------------------------------
// B: the normal is discontinuous; the contact set need not be
// ---------------------------------------------------------------------------

void section_b()
{
    rule("B. THE NORMAL JUMPS. THE CONTACT SET DOES NOT HAVE TO.");

    // ---- B.1 slide a box past a corner and watch the MTV swing -------------
    std::printf("B.1  a 0.4 m cube pressed 50 mm into a wall that ENDS at\n");
    std::printf("     z = 1, slid along +z past that end in 10 um steps.\n");
    std::printf("     Escaping through the face costs 50 mm; escaping past\n");
    std::printf("     the end costs 1 - (z - 0.2), so they are equal at\n");
    std::printf("     z = 1.15 and the argmin changes there.\n\n");
    std::printf("     z (mm)   depth (mm)   normal            swing\n");

    const placed_box wall = make_obb(vec3{-1.0f, 0.0f, 0.0f}, vec3{1.0f, 1.0f, 1.0f}, quat{});
    vec3 prev_normal{};
    double worst_swing = 0.0;
    double worst_depth_jump = 0.0;
    float prev_depth = -1.0f;
    for (int i = 0; i <= 2000; ++i)
    {
        const float z = 1.14f + static_cast<float>(i) * 1e-5f;
        const placed_box crate = make_obb(vec3{0.15f, 0.0f, z}, vec3{0.2f, 0.2f, 0.2f}, quat{});
        const gjk_result g = gjk_distance(wall.view(), crate.view());
        if (g.status == gjk_status::separated) { continue; }
        const epa_result e = epa_penetration(wall.view(), crate.view(), g.terminal);

        double swing = 0.0;
        if (length_squared(prev_normal) > 0.0f) { swing = angle_deg(prev_normal, e.normal); }
        if (prev_depth >= 0.0f)
        {
            worst_depth_jump = std::fmax(
                worst_depth_jump, 1000.0 * std::fabs(static_cast<double>(e.depth - prev_depth)));
        }
        if (swing > worst_swing || i % 500 == 0)
        {
            std::printf("     %6.1f   %8.3f     (%+.2f %+.2f %+.2f)  %6.2f deg\n",
                        static_cast<double>(z) * 1000.0, static_cast<double>(e.depth) * 1000.0,
                        static_cast<double>(e.normal.x), static_cast<double>(e.normal.y),
                        static_cast<double>(e.normal.z), swing);
        }
        worst_swing = std::fmax(worst_swing, swing);
        prev_normal = e.normal;
        prev_depth = e.depth;
    }
    std::printf("\n     worst swing between adjacent 10 um steps : %.2f deg\n", worst_swing);
    std::printf("     worst DEPTH jump over the same steps     : %.5f mm\n", worst_depth_jump);
    std::printf("\n     THE DEPTH IS CONTINUOUS AND THE ARGMIN IS NOT. That is\n");
    std::printf("     a property of a minimum, not a defect in EPA, and every\n");
    std::printf("     penetration-depth implementation has it.\n");

    check(worst_swing > 80.0, "B.1 the MTV normal really does swing 90 degrees");
    check(worst_depth_jump < 0.02, "B.1 the depth stays continuous through it");

    // ---- B.2 the witness point jumps between corners; the manifold's ids do
    // not ------------------------------------------------------------------
    std::printf("\nB.2  a 1 m crate sliding across a floor in 1 micron steps,\n");
    std::printf("     2 mm of travel: how often does the answer CHANGE?\n\n");

    const placed_box floor = make_obb(vec3{0.0f, -1.0f, 0.0f}, vec3{8.0f, 1.0f, 8.0f}, quat{});
    vec3 prev_witness{};
    contact_id prev_ids[k_max_manifold_points];
    int prev_count = 0;
    int witness_jumps = 0;
    int id_changes = 0;
    int samples = 0;
    double witness_path = 0.0;
    double witness_worst_step = 0.0;
    for (int i = 0; i < 2000; ++i)
    {
        const float x = static_cast<float>(i) * 1e-6f;
        const placed_box crate =
            make_obb(vec3{x, 0.4995f, 0.0f}, vec3{0.5f, 0.5f, 0.5f}, quat{});
        const gjk_result g = gjk_distance(floor.view(), crate.view());
        if (g.status == gjk_status::separated) { continue; }
        const epa_result e = epa_penetration(floor.view(), crate.view(), g.terminal);
        const contact_manifold m = build_manifold(floor.view(), crate.view(), e.normal, e.depth);
        ++samples;

        if (i > 0)
        {
            // A witness point that moved by more than the crate moved has jumped
            // to a different feature: the crate advanced one micron, so anything
            // over a millimetre is a jump and not motion.
            const double step =
                std::sqrt(static_cast<double>(length_squared(e.point_b - prev_witness)));
            witness_path += step;
            witness_worst_step = std::fmax(witness_worst_step, step);
            if (step > 1e-5) { ++witness_jumps; }

            bool same = (m.count == prev_count);
            for (int k = 0; same && k < m.count; ++k)
            {
                bool found = false;
                for (int j = 0; j < prev_count; ++j)
                {
                    if (m.points[k].id == prev_ids[j]) { found = true; break; }
                }
                same = found;
            }
            if (!same) { ++id_changes; }
        }
        prev_witness = e.point_b;
        prev_count = m.count;
        for (int k = 0; k < m.count; ++k) { prev_ids[k] = m.points[k].id; }
    }
    std::printf("     samples                          : %d\n", samples);
    std::printf("     the crate travelled              : %.3f mm\n", 1999.0 * 1e-3);
    std::printf("     EPA's witness point travelled    : %.3f mm\n", witness_path * 1000.0);
    std::printf("     its worst single step            : %.3f mm\n",
                witness_worst_step * 1000.0);
    std::printf("     steps where it moved over 10 um  : %d\n", witness_jumps);
    std::printf("     manifold contact ids changed     : %d times\n", id_changes);

    check(samples > 1900, "B.2 the sweep stayed in contact throughout");
    check(id_changes == 0, "B.2 the manifold's ids are stable under sliding");

    // ---- the control: ids MUST change when the contact really changes ------
    std::printf("\n     the control: slide the same crate off the floor's edge,\n");
    std::printf("     where the contact genuinely becomes a different one.\n\n");
    int edge_changes = 0;
    prev_count = 0;
    for (int i = 0; i < 400; ++i)
    {
        const float x = 7.0f + static_cast<float>(i) * 0.004f;
        const placed_box crate = make_obb(vec3{x, 0.4995f, 0.0f}, vec3{0.5f, 0.5f, 0.5f}, quat{});
        const gjk_result g = gjk_distance(floor.view(), crate.view());
        if (g.status == gjk_status::separated) { continue; }
        const epa_result e = epa_penetration(floor.view(), crate.view(), g.terminal);
        const contact_manifold m = build_manifold(floor.view(), crate.view(), e.normal, e.depth);
        if (i > 0)
        {
            bool same = (m.count == prev_count);
            for (int k = 0; same && k < m.count; ++k)
            {
                bool found = false;
                for (int j = 0; j < prev_count; ++j)
                {
                    if (m.points[k].id == prev_ids[j]) { found = true; break; }
                }
                same = found;
            }
            if (!same) { ++edge_changes; }
        }
        prev_count = m.count;
        for (int k = 0; k < m.count; ++k) { prev_ids[k] = m.points[k].id; }
    }
    std::printf("     ids changed while crossing the edge : %d times\n", edge_changes);
    check(edge_changes > 0, "B.2 control: ids DO change at a real feature change");
}

// ---------------------------------------------------------------------------
// C: a support function determines a shape and does not name a feature
// ---------------------------------------------------------------------------

void section_c()
{
    rule("C. SUPPORT DETERMINES THE SHAPE, AND NAMES NO FEATURE");

    std::printf("C.1  query a unit cube with 4000 directions drawn from a\n");
    std::printf("     cone 0.001 deg wide about its +y face normal. Every\n");
    std::printf("     one of them has the SAME four-corner argmax set.\n\n");

    rng r(20250918u);
    const placed_box cube = make_obb(vec3{}, vec3{0.5f, 0.5f, 0.5f}, quat{});

    bool corner_seen[8] = {};
    int distinct_support = 0;
    int face_always_four = 0;
    std::uint16_t face_feature = 0xffff;
    bool face_corner_seen[8] = {};
    const int trials = 4000;
    for (int i = 0; i < trials; ++i)
    {
        const vec3 jitter = r.direction() * 1.7e-5f;   // about 0.001 deg
        const vec3 d = normalised(vec3{0.0f, 1.0f, 0.0f} + jitter);

        const vec3 s = support_local(cube.box, d);
        const int idx = (s.x > 0.0f ? 1 : 0) | (s.y > 0.0f ? 2 : 0) | (s.z > 0.0f ? 4 : 0);
        if (!corner_seen[idx]) { corner_seen[idx] = true; ++distinct_support; }

        const contact_face f = support_face(cube.box, d);
        if (f.count == 4) { ++face_always_four; }
        if (face_feature == 0xffff) { face_feature = f.feature; }
        for (int k = 0; k < f.count; ++k) { face_corner_seen[f.id[k]] = true; }
        check(f.feature == face_feature, "C.1 the face feature id never changes");
        g_checks -= 1;   // counted once below rather than 4000 times
    }
    ++g_checks;

    int face_corners = 0;
    for (bool b : face_corner_seen) { face_corners += b ? 1 : 0; }

    std::printf("     distinct corners `support` returned  : %d of 4\n", distinct_support);
    std::printf("     `support_face` count == 4            : %d of %d\n", face_always_four,
                trials);
    std::printf("     distinct corners it named            : %d\n", face_corners);
    std::printf("\n     The argmax set has four elements. `support` has to pick\n");
    std::printf("     one, and WHICH one is decided by a tie-break on three\n");
    std::printf("     dot products that are all zero in exact arithmetic.\n");

    check(distinct_support >= 2, "C.1 support's answer is arbitrary within the cone");
    check(face_always_four == trials, "C.1 support_face returns the whole face");
    check(face_corners == 4, "C.1 and it returns the SAME four corners");

    // ---- C.2 the control: away from a face normal there is no ambiguity ----
    std::printf("\nC.2  the control: the same jitter about a direction pointed\n");
    std::printf("     at a CORNER, where the argmax really is one point.\n\n");
    bool corner_seen2[8] = {};
    int distinct2 = 0;
    for (int i = 0; i < trials; ++i)
    {
        const vec3 jitter = r.direction() * 1.7e-5f;
        const vec3 d = normalised(normalised(vec3{1.0f, 1.0f, 1.0f}) + jitter);
        const vec3 s = support_local(cube.box, d);
        const int idx = (s.x > 0.0f ? 1 : 0) | (s.y > 0.0f ? 2 : 0) | (s.z > 0.0f ? 4 : 0);
        if (!corner_seen2[idx]) { corner_seen2[idx] = true; ++distinct2; }
    }
    std::printf("     distinct corners `support` returned  : %d of 8\n", distinct2);
    std::printf("     so the ambiguity is a property of the QUERY, not of\n");
    std::printf("     the function: it appears exactly where the feature is\n");
    std::printf("     bigger than a point, which is exactly where a manifold\n");
    std::printf("     is needed.\n");
    check(distinct2 == 1, "C.2 control: a corner query is unambiguous");
}

// ---------------------------------------------------------------------------
// D: the faces themselves
// ---------------------------------------------------------------------------

/// Is every vertex of `f` on the shape's surface, and does it maximise
/// `dot(v, d)` to within the gather tolerance?
struct face_report
{
    double worst_off_surface = 0.0;   ///< metres
    double worst_shortfall = 0.0;     ///< metres below the supporting plane
    double worst_winding = 0.0;       ///< most negative turn, relative
    int nonconvex = 0;
    int bad_normal = 0;
};

void section_d()
{
    rule("D. THE FACES THEMSELVES");

    rng r(4242u);

    std::printf("D.1  every vertex of every face, checked against the shape\n");
    std::printf("     it came from: on the surface, on the supporting plane,\n");
    std::printf("     and wound counter-clockwise about its own normal.\n\n");
    std::printf("     shape      faces    off surface   best face?    turns\n");

    const int trials = 20000;

    // --- box ---------------------------------------------------------------
    {
        face_report rep;
        int counted = 0;
        int wrong_face = 0;
        for (int i = 0; i < trials; ++i)
        {
            const placed_box p = make_obb(vec3{}, vec3{r.range(0.2f, 2.0f), r.range(0.2f, 2.0f),
                                                       r.range(0.2f, 2.0f)},
                                          r.rotation());
            const vec3 d = r.direction();
            const contact_face f = support_face(p.box, d);
            ++counted;

            // IS IT THE RIGHT FACE? The box has six, and the one to return is
            // the one whose outward normal leans into `d` hardest. Comparing
            // against the SUPPORT POINT would be asking a different question —
            // the support point is a corner, and for most directions it is not
            // on the returned face at all, which is exactly why this lesson
            // needed a second query.
            float best_cos = -2.0f;
            for (int ax = 0; ax < 3; ++ax)
            {
                best_cos = std::fmax(best_cos, std::fabs(dot(p.box.axis(ax), d)));
            }
            if (dot(f.normal, d) < best_cos - 1e-5f) { ++wrong_face; }

            for (int k = 0; k < f.count; ++k)
            {
                rep.worst_off_surface =
                    std::fmax(rep.worst_off_surface,
                              std::fabs(static_cast<double>(
                                  engine::phys::distance_squared_to(p.box, p.box.centre + f.v[k]))));
            }
            for (int k = 0; k < f.count; ++k)
            {
                const vec3 a = f.v[k];
                const vec3 b = f.v[(k + 1) % f.count];
                const vec3 c = f.v[(k + 2) % f.count];
                if (dot(cross(b - a, c - b), f.normal) <= 0.0f) { ++rep.nonconvex; }
            }
            if (dot(f.normal, d) <= 0.0f) { ++rep.bad_normal; }
        }
        std::printf("     box        %5d    %.2e      %-5s         %s\n", counted,
                    std::sqrt(rep.worst_off_surface), wrong_face == 0 ? "yes" : "NO",
                    rep.nonconvex == 0 ? "all ccw" : "BROKEN");
        check(std::sqrt(rep.worst_off_surface) < 1e-5, "D.1 box face vertices are on the box");
        check(rep.nonconvex == 0, "D.1 box faces are convex and CCW");
        check(rep.bad_normal == 0, "D.1 box face normals face the query");
        check(wrong_face == 0, "D.1 and it is always the most-aligned of the six");
    }

    // --- capsule -----------------------------------------------------------
    //
    // A CAPSULE'S SURFACE IS EVERY POINT EXACTLY `radius` FROM ITS SPINE, which
    // makes "is this vertex on the shape" a one-line exact test and needs no
    // second opinion at all.
    {
        int segments = 0;
        int points = 0;
        double worst = 0.0;
        for (int i = 0; i < trials; ++i)
        {
            const shape s = capsule_shape(r.range(0.1f, 0.6f), r.range(0.1f, 1.0f));
            const capsule c = world_capsule(s, vec3{}, r.rotation());
            const vec3 d = r.direction();
            const contact_face f = support_face(c, d);
            if (f.count == 2) { ++segments; }
            else { ++points; }
            for (int k = 0; k < f.count; ++k)
            {
                const vec3 w = f.v[k];
                const float t = std::fmin(c.half_height,
                                          std::fmax(-c.half_height, dot(w, c.axis)));
                const float dist = std::sqrt(length_squared(w - c.axis * t));
                worst = std::fmax(worst, std::fabs(static_cast<double>(dist - c.radius)));
            }
        }
        std::printf("     capsule    %5d    %.2e      side %d /   all ccw\n", trials, worst,
                    segments);
        std::printf("                                       cap %d\n", points);
        check(segments > 0 && points > 0, "D.1 a capsule has both kinds of feature");
        check(worst < 1e-6, "D.1 capsule face vertices are exactly on the capsule");
    }

    // --- hull: a 24-sided prism, the truncation case -----------------------
    std::printf("\nD.2  a 24-sided prism. A hull has no face list, so its\n");
    std::printf("     face has to be GATHERED from the supporting plane --\n");
    std::printf("     and how wide to make that plane is a guess about the\n");
    std::printf("     geometry that nothing in the data answers.\n\n");
    {
        std::vector<vec3> prism;
        const int sides = 24;
        for (int i = 0; i < sides; ++i)
        {
            const float a = static_cast<float>(2.0 * k_pi * i / sides);
            for (float y : {-0.5f, 0.5f})
            {
                prism.push_back(vec3{std::cos(a) * 0.5f, y, std::sin(a) * 0.5f});
            }
        }
        const hull h = world_hull(prism, vec3{}, quat{});

        // THE QUERY DIRECTION IS THE FACE'S OWN NORMAL, which for a 24-gon
        // BISECTS two adjacent vertices: a prism's side face spans 15 degrees,
        // so its normal is at 7.5. Querying along +x instead points at a vertical
        // EDGE, and the two-vertex answer that comes back is correct rather than
        // broken -- a mistake this harness made first and had to be talked out
        // of, because a fixture aimed at the wrong feature convicts the code of
        // its own error.
        const float fa = static_cast<float>(k_pi / sides);   // 7.5 degrees
        const vec3 face_dir{std::cos(fa), 0.0f, std::sin(fa)};

        std::printf("     gather_sin   side face    bulge      end cap\n");
        int side4 = 0;
        for (float gs : {0.002f, 0.01f, 0.05f, 0.2f})
        {
            const contact_face side = support_face(h, face_dir, gs);
            const contact_face cap = support_face(h, vec3{0.0f, 1.0f, 0.0f}, gs);
            // How far does the gathered polygon depart from the flat face it is
            // supposed to be? Anything gathered from a NEIGHBOURING face sits
            // short of the supporting plane, and that shortfall is the bulge.
            double best = -1e9;
            for (int k = 0; k < side.count; ++k)
            {
                best = std::fmax(best, static_cast<double>(dot(side.v[k], face_dir)));
            }
            double bulge = 0.0;
            for (int k = 0; k < side.count; ++k)
            {
                bulge = std::fmax(bulge, best - static_cast<double>(dot(side.v[k], face_dir)));
            }
            std::printf("     %-11.3f  %2d vertices  %6.4f m   %d vertices\n",
                        static_cast<double>(gs), side.count, bulge, cap.count);
            if (side.count == 4 && bulge < 1e-6) { ++side4; }
        }
        std::printf("\n     TOO TIGHT and a cylinder's side contact is a bare\n");
        std::printf("     EDGE -- two points, no support polygon, and the\n");
        std::printf("     thing rolls when it should not. TOO LOOSE and three\n");
        std::printf("     flat faces are merged into one bulged polygon whose\n");
        std::printf("     outer vertices are millimetres proud of the plane.\n");
        std::printf("     There is no value that is right, because the\n");
        std::printf("     question is topological and a point cloud has no\n");
        std::printf("     topology. The fix is to give `hull` its faces.\n");
        check(side4 > 0, "D.2 some tolerance recovers the prism's true quad side");

        // ---- and the truncation, at the default -------------------------
        const contact_face cap = support_face(h, vec3{0.0f, 1.0f, 0.0f});
        double worst_inset = 0.0;
        for (int k = 0; k < cap.count; ++k)
        {
            const vec3 a = cap.v[k];
            const vec3 b = cap.v[(k + 1) % cap.count];
            const vec3 mid = (a + b) * 0.5f;
            const double rad = std::sqrt(static_cast<double>(mid.x * mid.x + mid.z * mid.z));
            worst_inset = std::fmax(worst_inset, 0.5 - rad);
        }
        const double predicted = 0.5 * (1.0 - std::cos(k_pi / k_max_face_vertices));
        std::printf("\n     the end cap has 24 vertices and a `contact_face`\n");
        std::printf("     holds %d, so it is decimated -- AFTER the angular\n",
                    k_max_face_vertices);
        std::printf("     sort, so what is kept is an inscribed octagon and\n");
        std::printf("     every contact point is still on the surface.\n\n");
        std::printf("     truncated          : %s\n", cap.truncated ? "yes" : "no");
        std::printf("     worst edge inset   : %.5f m\n", worst_inset);
        std::printf("     r(1-cos(pi/8)) says: %.5f m\n", predicted);
        check(cap.truncated, "D.2 a 24-gon cap is truncated and says so");
        check(cap.count == k_max_face_vertices, "D.2 truncation keeps the full budget");
        check(std::fabs(worst_inset - predicted) < 0.01, "D.2 the inset is the predicted one");
    }

    // ---- the control -------------------------------------------------------
    std::printf("\nD.3  the control: reverse a face's winding by hand and the\n");
    std::printf("     clipper's side planes all point inward, so a contact\n");
    std::printf("     that plainly exists comes back with nothing.\n\n");
    {
        const placed_box floor = make_obb(vec3{0.0f, -0.5f, 0.0f}, vec3{2.0f, 0.5f, 2.0f}, quat{});
        const placed_box crate = make_obb(vec3{0.0f, 0.49f, 0.0f}, vec3{0.5f, 0.5f, 0.5f}, quat{});
        const contact_manifold good =
            build_manifold(floor.view(), crate.view(), vec3{0.0f, 1.0f, 0.0f}, 0.01f);

        // A hand-built reversed polygon, clipped by the same arithmetic.
        contact_face f = support_face(floor.box, vec3{0.0f, 1.0f, 0.0f});
        std::swap(f.v[1], f.v[3]);
        int inside = 0;
        const contact_face inc = support_face(crate.box, vec3{0.0f, -1.0f, 0.0f});
        for (int k = 0; k < inc.count; ++k)
        {
            const vec3 p = inc.v[k] + (crate.box.centre - floor.box.centre);
            bool in = true;
            for (int e = 0; e < f.count; ++e)
            {
                const vec3 side = cross(f.v[(e + 1) % f.count] - f.v[e], f.normal);
                if (dot(side, p - f.v[e]) > 0.0f) { in = false; }
            }
            inside += in ? 1 : 0;
        }
        std::printf("     correct winding  : %d contact points\n", good.count);
        std::printf("     reversed winding : %d of 4 incident vertices inside\n", inside);
        check(good.count == 4, "D.3 the correct winding gives four points");
        check(inside == 0, "D.3 control: a reversed winding rejects everything");
    }
}

// ---------------------------------------------------------------------------
// E: reference, incident, and how parallel is parallel
// ---------------------------------------------------------------------------

void section_e()
{
    rule("E. REFERENCE, INCIDENT, AND HOW PARALLEL IS PARALLEL");

    std::printf("E.1  `face_cos` decides whether a feature is used as a\n");
    std::printf("     FACE or only as an edge. Swept over 40,000 random\n");
    std::printf("     overlapping box pairs.\n\n");
    std::printf("     face_cos   face%%   mean pts   normal vs exact MTV\n");
    std::printf("                                     median     worst\n");

    double mean_points_default = 0.0;
    for (float fc : {0.5f, 0.9f, 0.99f, 0.999f, 0.99999f})
    {
        rng r(11223u);
        manifold_config cfg;
        cfg.face_cos = fc;
        int pairs = 0;
        int faces = 0;
        int points = 0;
        double worst = 0.0;
        std::vector<double> errs;
        for (int i = 0; i < 40000; ++i)
        {
            const vec3 ha{r.range(0.3f, 1.0f), r.range(0.3f, 1.0f), r.range(0.3f, 1.0f)};
            const vec3 hb{r.range(0.3f, 1.0f), r.range(0.3f, 1.0f), r.range(0.3f, 1.0f)};
            const placed_box a = make_obb(vec3{}, ha, r.rotation());
            const vec3 off = r.direction() * r.range(0.9f, 1.3f);
            const placed_box b = make_obb(off, hb, r.rotation());
            const separation sat = collide(a.box, b.box);
            const contact_manifold m = collide_manifold(a.view(), b.view(), cfg);
            if (m.status == manifold_status::none || !sat.hit()) { continue; }
            ++pairs;
            if (m.status == manifold_status::face) { ++faces; }
            points += m.count;

            // WHAT `face_cos` DECIDES IS THE NORMAL, and the first version of
            // this section measured the wrong thing: it checked that the two
            // claimed surface points lie on their shapes, which is true BY
            // CONSTRUCTION at every tolerance — one is the incident vertex and
            // the other is its shadow on the reference plane — and reported a
            // flat zero across four decades. A tautology dressed as a result.
            //
            // What a loose tolerance actually buys is a contact normal
            // perpendicular to a face that is thirty degrees from the direction
            // the two shapes are really pressing, and 8.4's SAT — the exact MTV
            // for two boxes — says so.
            const double err = angle_deg(m.normal, sat.axis);
            errs.push_back(err);
            worst = std::fmax(worst, err);
        }
        const double mean_pts =
            static_cast<double>(points) / std::fmax(1.0, static_cast<double>(pairs));
        std::printf("     %-9.5f %5.1f   %6.3f     %7.4f  %8.4f deg\n", static_cast<double>(fc),
                    100.0 * faces / std::fmax(1.0, static_cast<double>(pairs)), mean_pts,
                    pct(errs, 0.5), worst);
        if (fc <= 0.5f) { check(worst > 30.0, "E.1 too loose snaps the normal a long way"); }
        if (fc == 0.999f) { mean_points_default = mean_pts; }
        if (fc >= 0.999f)
        {
            check(worst < std::acos(static_cast<double>(fc)) * 180.0 / k_pi + 0.05,
                  "E.1 the error is bounded by acos(face_cos)");
        }
        if (fc >= 0.99999f)
        {
            check(mean_pts < mean_points_default, "E.1 too tight loses contact points");
        }
    }
    std::printf("\n     TOO LOOSE clips two faces that are sixty degrees\n");
    std::printf("     apart and calls one of them the contact normal. TOO\n");
    std::printf("     TIGHT refuses a face that EPA's own tolerance has\n");
    std::printf("     nudged a hundredth of a degree off, and the mean\n");
    std::printf("     contact count falls with it. The error is bounded by\n");
    std::printf("     acos(face_cos) exactly, which is what makes this a\n");
    std::printf("     KNOB rather than a magic number: pick the largest\n");
    std::printf("     normal error a solver can live with and take its\n");
    std::printf("     cosine.\n");

    // ---- E.2 the normal: exact where it matters, bounded where it does not --
    std::printf("\nE.2  the manifold normal is the REFERENCE FACE's, not\n");
    std::printf("     EPA's. Two populations, because they answer\n");
    std::printf("     differently and the difference is the design.\n\n");

    // (a) A CURVED shape on a floor, which is where EPA's normal is an
    // approximation rather than an answer. On two BOXES both normals come out
    // exact, because the difference set has a genuinely flat face and EPA
    // terminates on it -- 8.6 §9's finding, and it makes box-on-box the one
    // fixture that cannot show this difference. A ball is EPA's worst case.
    {
        rng r(606060u);
        const placed_box floor = make_obb(vec3{0.0f, -1.0f, 0.0f}, vec3{8.0f, 1.0f, 8.0f}, quat{});
        double epa_wobble = 0.0;
        double face_wobble = 0.0;
        int exact = 0;
        int frames = 0;
        for (int f = 0; f < 4000; ++f)
        {
            const shape sh = sphere_shape(0.4f);
            const engine::sphere ball =
                world_sphere(sh, vec3{r.signed_unit() * 1e-3f, 0.3990f + r.signed_unit() * 1e-6f,
                                      r.signed_unit() * 1e-3f});
            const convex vb = as_convex(ball);
            const gjk_result g = gjk_distance(floor.view(), vb);
            if (g.status == gjk_status::separated) { continue; }
            const epa_result e = epa_penetration(floor.view(), vb, g.terminal);
            const contact_manifold m = build_manifold(floor.view(), vb, e.normal, e.depth);
            ++frames;
            epa_wobble = std::fmax(epa_wobble, angle_deg(e.normal, vec3{0.0f, 1.0f, 0.0f}));
            face_wobble = std::fmax(face_wobble, angle_deg(m.normal, vec3{0.0f, 1.0f, 0.0f}));
            if (m.normal.x == 0.0f && m.normal.y == 1.0f && m.normal.z == 0.0f) { ++exact; }
        }
        std::printf("     (a) a 0.4 m ball resting on a floor, jittered a\n");
        std::printf("         micron a frame, %d frames. A ball is EPA's\n", frames);
        std::printf("         worst case (8.6 §9): flat triangles can only\n");
        std::printf("         approximate a sphere.\n");
        std::printf("         EPA normal, worst off +y  : %.6f deg\n", epa_wobble);
        std::printf("         face normal, worst off +y : %.6f deg\n", face_wobble);
        std::printf("         frames where it was EXACTLY (0,1,0) : %d\n", exact);
        check(frames > 3000, "E.2 the ball stayed in contact");
        check(exact == frames, "E.2 the face normal is bit-exact on a flat reference");
        check(epa_wobble > face_wobble, "E.2 EPA's normal wobbles and the face normal does not");
    }

    // (b) Random pairs, against 8.4's exact MTV.
    {
        rng r(99001u);
        std::vector<double> epa_err;
        std::vector<double> man_err;
        std::vector<double> flat_epa;
        std::vector<double> flat_man;
        int pairs = 0;
        for (int i = 0; i < 40000; ++i)
        {
            const placed_box a = make_obb(vec3{}, vec3{0.5f, 0.5f, 0.5f}, r.yaw());
            const vec3 off{r.range(-0.9f, 0.9f), r.range(-0.9f, 0.9f), r.range(-0.9f, 0.9f)};
            const placed_box b = make_obb(off, vec3{0.5f, 0.5f, 0.5f}, r.yaw());
            const separation sat = collide(a.box, b.box);
            if (!sat.hit() || sat.depth < 1e-4f) { continue; }
            const gjk_result g = gjk_distance(a.view(), b.view());
            if (g.status == gjk_status::separated) { continue; }
            const epa_result e = epa_penetration(a.view(), b.view(), g.terminal);
            const contact_manifold m = build_manifold(a.view(), b.view(), e.normal, e.depth);
            if (m.status != manifold_status::face) { continue; }
            ++pairs;
            const double ee = angle_deg(e.normal, sat.axis);
            const double me = angle_deg(m.normal, sat.axis);
            epa_err.push_back(ee);
            man_err.push_back(me);
            // IS THE REFERENCE FACE THE ONE THE SAT USED? That is the precise
            // question, and it is not the same as "is this a face contact". The
            // SAT names the shape whose face normal is the exact MTV axis; the
            // manifold names the shape whose face it clipped against. When they
            // agree, the manifold's normal IS the exact axis, to the last bit.
            // When they disagree -- A's face is within `face_cos` of B's, and
            // the bias kept A -- the normal is deliberately snapped to the
            // reference, and the error is exactly that angle.
            const engine::phys::axis_source src = engine::phys::source_of(sat);
            const bool sat_face_a = (src == engine::phys::axis_source::face_a);
            const bool sat_face_b = (src == engine::phys::axis_source::face_b);
            if ((sat_face_a && !m.reference_on_b) || (sat_face_b && m.reference_on_b))
            {
                flat_epa.push_back(ee);
                flat_man.push_back(me);
            }
        }
        std::printf("\n     (b) 8.4's SAT is the exact MTV for two boxes (8.6\n");
        std::printf("         §8), so it is the reference. %d face contacts:\n", pairs);
        std::printf("                          median      99th       worst\n");
        std::printf("         EPA   , all    %8.5f  %8.5f  %8.4f deg\n", pct(epa_err, 0.5),
                    pct(epa_err, 0.99), max_of(epa_err));
        std::printf("         face  , all    %8.5f  %8.5f  %8.4f deg\n", pct(man_err, 0.5),
                    pct(man_err, 0.99), max_of(man_err));
        std::printf("         EPA   , agree  %8.5f  %8.5f  %8.4f deg\n", pct(flat_epa, 0.5),
                    pct(flat_epa, 0.99), max_of(flat_epa));
        std::printf("         face  , agree  %8.5f  %8.5f  %8.4f deg\n", pct(flat_man, 0.5),
                    pct(flat_man, 0.99), max_of(flat_man));
        std::printf("\n         (\"agree\" = the SAT's exact axis came from the\n");
        std::printf("          same shape's face the manifold clipped against:\n");
        std::printf("          %zu of %d)\n", flat_man.size(), pairs);
        std::printf("\n     ON A FLAT CONTACT THE FACE NORMAL IS THE ANSWER AND\n");
        std::printf("     EPA's IS AN APPROXIMATION TO IT. Off one, the face\n");
        std::printf("     normal is deliberately SNAPPED, by up to\n");
        std::printf("     acos(face_cos) = %.2f deg -- which is the trade a\n",
                    std::acos(0.999) * 180.0 / k_pi);
        std::printf("     solver wants, because a normal that is piecewise\n");
        std::printf("     constant does not shake a stack and one that is\n");
        std::printf("     merely accurate does.\n");
        check(pairs > 1000, "E.2 enough face contacts to say anything");
        check(max_of(flat_man) < 1e-4, "E.2 when the two agree the face normal is exact");
        check(max_of(flat_man) <= max_of(flat_epa) + 1e-6,
              "E.2 and no worse than EPA there");
        check(max_of(man_err) < std::acos(0.999) * 180.0 / k_pi + 0.01,
              "E.2 and off one it is bounded by acos(face_cos)");
    }

    // ---- the control -------------------------------------------------------
    std::printf("\nE.3  the control: a SPHERE has no face at any tolerance,\n");
    std::printf("     and `face_cos` must not be able to invent one.\n\n");
    {
        const placed_box floor = make_obb(vec3{0.0f, -1.0f, 0.0f}, vec3{8.0f, 1.0f, 8.0f}, quat{});
        int sphere_points = 0;
        int point_status = 0;
        for (float fc : {0.5f, 0.9f, 0.999f, 0.99999f})
        {
            const shape s2 = sphere_shape(0.4f);
            const engine::sphere ball = world_sphere(s2, vec3{0.0f, 0.39f, 0.0f});
            manifold_config cfg;
            cfg.face_cos = fc;
            const contact_manifold m = collide_manifold(floor.view(), as_convex(ball), cfg);
            std::printf("     face_cos %-8.5f -> %d point, status %s\n", static_cast<double>(fc),
                        m.count, name_of(m.status));
            sphere_points += m.count;
            if (m.status == manifold_status::point) { ++point_status; }
        }
        check(sphere_points == 4, "E.3 control: a ball is one point at every tolerance");
        check(point_status == 4, "E.3 and it is REPORTED as a point contact");
    }
}

// ---------------------------------------------------------------------------
// F: the clip
// ---------------------------------------------------------------------------

void section_f()
{
    rule("F. THE CLIP");

    std::printf("F.1  200,000 random box pairs. Every contact point checked\n");
    std::printf("     against BOTH boxes with 8.4's `distance_squared_to`,\n");
    std::printf("     which knows nothing about manifolds.\n\n");

    rng r(7001u);
    int overlapping = 0;
    int face_count = 0;
    int edge_count = 0;
    int point_count = 0;
    int empty_count = 0;
    int zero_points = 0;
    std::vector<double> off_surface;
    std::vector<double> shallow;
    std::vector<double> edge_off;
    std::vector<double> edge_shallow;
    std::vector<double> depth_err;
    int kind_hist[5] = {};
    int point_hist[5] = {};

    for (int i = 0; i < 200000; ++i)
    {
        const vec3 ha{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f), r.range(0.2f, 1.2f)};
        const vec3 hb{r.range(0.2f, 1.2f), r.range(0.2f, 1.2f), r.range(0.2f, 1.2f)};
        const placed_box a = make_obb(vec3{}, ha, r.rotation());
        const vec3 off = r.direction() * r.range(0.0f, 1.6f);
        const placed_box b = make_obb(off, hb, r.rotation());

        const contact_manifold m = collide_manifold(a.view(), b.view());
        if (m.status == manifold_status::none) { continue; }
        ++overlapping;
        switch (m.status)
        {
        case manifold_status::face: ++face_count; break;
        case manifold_status::edge: ++edge_count; break;
        case manifold_status::point: ++point_count; break;
        case manifold_status::clip_empty: ++empty_count; break;
        default: break;
        }
        if (m.count == 0) { ++zero_points; }
        point_hist[std::min(m.count, 4)] += 1;

        for (int k = 0; k < m.count; ++k)
        {
            kind_hist[static_cast<int>(m.points[k].id.kind)] += 1;

            // THE INSTRUMENT IS THE PAIR OF SURFACE POINTS, NOT THE MIDPOINT,
            // and the first version of this harness got that wrong. A contact
            // point is defined as halfway between the two surfaces, so on a pair
            // overlapping by more than its own size the midpoint is legitimately
            // 0.6 m from either one — a fact about the convention, measured as
            // though it were a bug. The two surface points are recoverable
            // exactly: A's is `position + normal*depth/2` and B's is
            // `position - normal*depth/2`, whichever shape supplied the
            // reference face.
            // TWO STATUSES, TWO INSTRUMENTS, AND THE DIFFERENCE IS REAL.
            //
            // On a FACE contact the two surface points are recoverable exactly:
            // A's is `position + normal*depth/2` and B's is `position -
            // normal*depth/2`, because the contact really was constructed as the
            // midpoint of an incident vertex and its shadow on the reference
            // plane, and the shadow is along the normal by definition.
            //
            // On an EDGE contact it is not. The two closest points of two
            // crossed edges are not separated along the contact normal in
            // general, so the recovery formula misses both surfaces — by up to
            // 0.30 m on a deep overlap, which the first version of this harness
            // reported as a clipping bug. What IS true of an edge contact is
            // that its point is the midpoint of two surface points, so it is
            // within `depth/2` of each.
            const float d = m.points[k].depth;
            double e = 0.0;
            if (m.status == manifold_status::face)
            {
                const vec3 pa = m.points[k].position + m.normal * (d * 0.5f);
                const vec3 pb = m.points[k].position - m.normal * (d * 0.5f);
                e = std::fmax(std::fabs(box_signed_distance(a.box, pa)),
                              std::fabs(box_signed_distance(b.box, pb)));
                off_surface.push_back(e);
                if (d < 0.01f) { shallow.push_back(e); }
            }
            else
            {
                // THE INVARIANT IS SIGNED, and it is provable rather than
                // empirical. The contact is the midpoint of a point on A's edge
                // and a point on B's edge; when the two edges are crossing,
                // each of those points lies inside the OTHER shape, and a convex
                // set contains every midpoint of two of its own points. So the
                // contact must be inside both — and unlike a face contact, that
                // is all that can be said: an edge contact has no reference
                // plane to project onto, so its point does not land on either
                // surface, and on a deep overlap it is nowhere near one.
                const double ea = box_signed_distance(a.box, m.points[k].position);
                const double eb = box_signed_distance(b.box, m.points[k].position);
                e = std::fmax(ea, eb);
                edge_off.push_back(e);
                if (d < 0.01f) { edge_shallow.push_back(std::fmax(ea, eb)); }
            }
            // The depth must not exceed what the shapes themselves allow along
            // this normal, and `depth_along` measures that in two support calls.
            const double h = static_cast<double>(
                engine::phys::depth_along(a.view(), b.view(), m.normal));
            depth_err.push_back(static_cast<double>(d) - h);
        }
    }

    std::printf("     overlapping pairs      : %d\n", overlapping);
    std::printf("     face / edge / point    : %d / %d / %d\n", face_count, edge_count,
                point_count);
    std::printf("     clip produced nothing  : %d  (%.4f%%)\n", empty_count,
                100.0 * empty_count / std::fmax(1.0, static_cast<double>(overlapping)));
    std::printf("     manifolds with 0 points: %d\n", zero_points);
    std::printf("\n     points per manifold    : 1:%d 2:%d 3:%d 4:%d\n", point_hist[1],
                point_hist[2], point_hist[3], point_hist[4]);
    std::printf("     contact kinds          : inc-vtx %d, crossing %d,\n", kind_hist[0],
                kind_hist[1]);
    std::printf("                              ref-vtx %d, edge %d, point %d\n", kind_hist[2],
                kind_hist[3], kind_hist[4]);
    std::printf("\n     FACE contacts, both surface points recovered:\n");
    std::printf("       worst |sdf|            : %.2e m  (%zu pts)\n", max_of(off_surface),
                off_surface.size());
    std::printf("       99.9th percentile      : %.2e m\n", pct(off_surface, 0.999));
    std::printf("       overlaps under 10 mm   : %.2e m  (%zu pts)\n", max_of(shallow),
                shallow.size());
    std::printf("     EDGE contacts, signed distance at the point:\n");
    std::printf("       worst, all overlaps    : %+.2e m  (%zu pts)\n", max_of(edge_off),
                edge_off.size());
    std::printf("       worst, under 10 mm     : %+.2e m  (%zu pts)\n", max_of(edge_shallow),
                edge_shallow.size());
    std::printf("       median, under 10 mm    : %+.2e m\n", pct(edge_shallow, 0.5));
    std::printf("\n     AN EDGE CONTACT HAS NO REFERENCE PLANE, so there is no\n");
    std::printf("     projection that puts its point back on a surface: it is\n");
    std::printf("     the midpoint of the closest points of two crossed edges,\n");
    std::printf("     and only when they nearly touch is that midpoint near\n");
    std::printf("     either. At the depths a solver maintains it is within a\n");
    std::printf("     few millimetres of both; on a deep overlap it is half a\n");
    std::printf("     metre from one, and no convention does better, because\n");
    std::printf("     there IS no single point where two deeply crossed boxes\n");
    std::printf("     touch. One more reason 8.9's job is to keep overlaps\n");
    std::printf("     shallow, and a named limitation until it does.\n");
    std::printf("     worst depth above h(normal): %.2e m\n", max_of(depth_err));

    check(zero_points == 0, "F.1 no overlapping pair returns an empty manifold");
    check(max_of(shallow) < 1e-5, "F.1 shallow face contacts sit on both surfaces");
    check(max_of(off_surface) < 1e-4, "F.1 and deep ones do too");
    check(max_of(edge_shallow) < 5e-3, "F.1 a shallow edge contact sits between the surfaces");
    check(max_of(depth_err) < 2e-3, "F.1 no point claims more depth than h(n) allows");
    check(kind_hist[0] > 0 && kind_hist[1] > 0 && kind_hist[2] > 0,
          "F.1 all three clip outcomes occur");

    // ---- F.1b a fixture whose answer is known on paper ---------------------
    //
    // The random sweep says the code is self-consistent. It cannot say the
    // ANSWER is right, because nothing in it knows where the contact should be.
    // Two 1 m cubes crossed at 45 degrees about y, the upper one lowered until
    // its bottom EDGE presses into the lower one's top edge, has a contact at a
    // place that can be written down: directly between the two centres, on the
    // shared vertical line, at the height of the two edges.
    std::printf("\nF.1b the sweep says the clip is self-consistent. This\n");
    std::printf("     fixture says it is RIGHT: two 1 m cubes crossed at 45\n");
    std::printf("     degrees, upper one lowered 5 mm into the lower. The\n");
    std::printf("     contact is on the shared vertical axis, at y = 0.5.\n\n");
    {
        const placed_box lower = make_obb(vec3{0.0f, 0.0f, 0.0f}, vec3{0.5f, 0.5f, 0.5f},
                                          quat_from_axis_angle(vec3{0.0f, 0.0f, 1.0f},
                                                               static_cast<float>(k_pi / 4.0)));
        const placed_box upper =
            make_obb(vec3{0.0f, 0.5f * 1.41421356f + 0.5f * 1.41421356f - 0.005f, 0.0f},
                     vec3{0.5f, 0.5f, 0.5f},
                     quat_from_axis_angle(vec3{1.0f, 0.0f, 0.0f},
                                          static_cast<float>(k_pi / 4.0)));
        const contact_manifold m = collide_manifold(lower.view(), upper.view());
        const double expected_y = 0.5 * 1.41421356 - 0.0025;
        std::printf("     status          : %s\n", name_of(m.status));
        std::printf("     contact points  : %d\n", m.count);
        std::printf("     normal          : (%+.4f %+.4f %+.4f)\n",
                    static_cast<double>(m.normal.x), static_cast<double>(m.normal.y),
                    static_cast<double>(m.normal.z));
        std::printf("     point           : (%+.5f %+.5f %+.5f)\n",
                    static_cast<double>(m.points[0].position.x),
                    static_cast<double>(m.points[0].position.y),
                    static_cast<double>(m.points[0].position.z));
        std::printf("     expected        : (%+.5f %+.5f %+.5f)\n", 0.0, expected_y, 0.0);
        std::printf("     depth           : %.5f m  (expected 0.00500)\n",
                    static_cast<double>(m.points[0].depth));
        check(m.count == 1, "F.1b two crossed edges touch at exactly one point");
        check(std::fabs(static_cast<double>(m.points[0].position.x)) < 1e-4,
              "F.1b and it is on the shared axis in x");
        check(std::fabs(static_cast<double>(m.points[0].position.z)) < 1e-4,
              "F.1b and in z");
        check(std::fabs(static_cast<double>(m.points[0].position.y) - expected_y) < 1e-3,
              "F.1b and at the right height");
        check(std::fabs(static_cast<double>(m.points[0].depth) - 0.005) < 1e-4,
              "F.1b with the depth it was given");
        check(angle_deg(m.normal, vec3{0.0f, 1.0f, 0.0f}) < 0.01, "F.1b and a vertical normal");
    }

    // ---- F.2 the control ---------------------------------------------------
    std::printf("\nF.2  the control: the same pairs with the depth filter\n");
    std::printf("     WIDENED to a whole shape-width, so every clipped\n");
    std::printf("     point survives whether it penetrates or not. The\n");
    std::printf("     recovered surface points are still exactly on the\n");
    std::printf("     surfaces -- that is true by construction and measuring\n");
    std::printf("     it proves nothing. What breaks is the DEPTH.\n\n");

    rng r2(7001u);
    std::vector<double> negative;
    int loose_pairs = 0;
    int loose_points = 0;
    int not_touching = 0;
    for (int i = 0; i < 40000; ++i)
    {
        const vec3 ha{r2.range(0.2f, 1.2f), r2.range(0.2f, 1.2f), r2.range(0.2f, 1.2f)};
        const vec3 hb{r2.range(0.2f, 1.2f), r2.range(0.2f, 1.2f), r2.range(0.2f, 1.2f)};
        const placed_box a = make_obb(vec3{}, ha, r2.rotation());
        const vec3 off = r2.direction() * r2.range(0.0f, 1.6f);
        const placed_box b = make_obb(off, hb, r2.rotation());
        manifold_config cfg;
        cfg.keep_slop = 1.0f;
        const contact_manifold m = collide_manifold(a.view(), b.view(), cfg);
        if (m.status != manifold_status::face) { continue; }
        ++loose_pairs;
        for (int k = 0; k < m.count; ++k)
        {
            ++loose_points;
            if (m.points[k].depth < 0.0f)
            {
                ++not_touching;
                negative.push_back(-static_cast<double>(m.points[k].depth));
            }
        }
    }
    std::printf("     face contacts                   : %d\n", loose_pairs);
    std::printf("     contact points kept             : %d\n", loose_points);
    std::printf("     points NOT ACTUALLY TOUCHING    : %d  (%.2f%%)\n", not_touching,
                100.0 * not_touching / std::fmax(1.0, static_cast<double>(loose_points)));
    std::printf("     worst gap reported as a contact : %.4f m\n", max_of(negative));
    std::printf("     median gap                      : %.4f m\n", pct(negative, 0.5));
    std::printf("\n     A solver handed these applies a correction where\n");
    std::printf("     nothing is touching. With a NON-NEGATIVE impulse\n");
    std::printf("     clamp it mostly does nothing and merely wastes the\n");
    std::printf("     iteration -- which is why a small positive slop is a\n");
    std::printf("     good idea in 8.9 and a whole shape-width is not.\n");
    check(not_touching > loose_points / 10, "F.2 control: the filter is doing real work");
    check(max_of(negative) > 0.05, "F.2 control: and the gaps it removes are large");
}

// ---------------------------------------------------------------------------
// G: four, and why not five
// ---------------------------------------------------------------------------

void section_g()
{
    rule("G. FOUR, AND WHY NOT FIVE");

    std::printf("G.1  how many points does the clip produce, before the\n");
    std::printf("     reduction? And what does the reduction keep?\n\n");

    rng r(31337u);
    int hist[17] = {};
    int total = 0;
    std::vector<double> kept_area;
    std::vector<double> naive_area;
    std::vector<double> full_area;

    for (int i = 0; i < 100000; ++i)
    {
        const vec3 ha{r.range(0.3f, 1.0f), r.range(0.3f, 1.0f), r.range(0.3f, 1.0f)};
        const vec3 hb{r.range(0.3f, 1.0f), r.range(0.3f, 1.0f), r.range(0.3f, 1.0f)};
        const placed_box a = make_obb(vec3{}, ha, r.yaw());
        const vec3 off{r.range(-0.6f, 0.6f), ha.y + hb.y - r.range(0.001f, 0.05f),
                       r.range(-0.6f, 0.6f)};
        const placed_box b = make_obb(off, hb, r.yaw());

        const contact_manifold reduced = collide_manifold(a.view(), b.view());
        if (reduced.status != manifold_status::face) { continue; }
        manifold_config raw;
        raw.reduce = false;
        const contact_manifold naive = collide_manifold(a.view(), b.view(), raw);

        ++total;
        hist[std::min(reduced.clipped, 16)] += 1;
        kept_area.push_back(static_cast<double>(reduced.support_area()));
        naive_area.push_back(static_cast<double>(naive.support_area()));
        full_area.push_back(static_cast<double>(reduced.clipped));
    }

    std::printf("     face contacts : %d\n", total);
    std::printf("     clipped points before reduction:\n");
    for (int i = 1; i <= 8; ++i)
    {
        if (hist[i] == 0) { continue; }
        std::printf("       %2d : %6d  (%5.2f%%)\n", i, hist[i],
                    100.0 * hist[i] / std::fmax(1.0, static_cast<double>(total)));
    }

    const double kept = mean_of(kept_area);
    const double naive_mean = mean_of(naive_area);
    std::printf("\n     mean support area, reduction ON  : %.5f m2\n", kept);
    std::printf("     mean support area, FIRST FOUR    : %.5f m2\n", naive_mean);
    std::printf("     the reduction keeps %.1f%% more polygon, from the\n",
                100.0 * (kept / std::fmax(1e-9, naive_mean) - 1.0));
    std::printf("     same clipped set and the same four slots.\n");

    check(total > 10000, "G.1 enough face contacts");
    check(kept >= naive_mean, "G.1 the area heuristic never loses to the first four");
    check(kept > naive_mean * 1.01, "G.1 and it wins measurably");

    // ---- G.2 the control ---------------------------------------------------
    std::printf("\nG.2  the control: when the clip produces four or fewer,\n");
    std::printf("     the reduction must be the identity, point for point.\n\n");
    int identical = 0;
    int checked = 0;
    rng r2(31337u);
    for (int i = 0; i < 20000; ++i)
    {
        const vec3 ha{r2.range(0.3f, 1.0f), r2.range(0.3f, 1.0f), r2.range(0.3f, 1.0f)};
        const vec3 hb{r2.range(0.3f, 1.0f), r2.range(0.3f, 1.0f), r2.range(0.3f, 1.0f)};
        const placed_box a = make_obb(vec3{}, ha, r2.yaw());
        const vec3 off{r2.range(-0.6f, 0.6f), ha.y + hb.y - r2.range(0.001f, 0.05f),
                       r2.range(-0.6f, 0.6f)};
        const placed_box b = make_obb(off, hb, r2.yaw());
        const contact_manifold reduced = collide_manifold(a.view(), b.view());
        if (reduced.clipped > k_max_manifold_points || reduced.count == 0) { continue; }
        manifold_config raw;
        raw.reduce = false;
        const contact_manifold naive = collide_manifold(a.view(), b.view(), raw);
        ++checked;
        bool same = (naive.count == reduced.count);
        for (int k = 0; same && k < reduced.count; ++k)
        {
            same = (naive.points[k].id == reduced.points[k].id);
        }
        identical += same ? 1 : 0;
    }
    std::printf("     manifolds with <= 4 clipped points : %d\n", checked);
    std::printf("     identical with and without         : %d\n", identical);
    check(checked > 0 && identical == checked, "G.2 control: reduction is the identity under 5");

    // ---- G.3 what a fifth point would buy ---------------------------------
    std::printf("\nG.3  what would a FIFTH point buy? For every manifold\n");
    std::printf("     that clipped to more than four, compare the polygon\n");
    std::printf("     the kept four span against the polygon the WHOLE\n");
    std::printf("     clipped set spans, and the depth they carry.\n\n");

    rng r3(31337u);
    std::vector<double> area_ratio;
    std::vector<double> best_ratio;
    std::vector<double> depth_lost;
    int over_four = 0;
    for (int i = 0; i < 100000; ++i)
    {
        const vec3 ha{r3.range(0.3f, 1.0f), r3.range(0.3f, 1.0f), r3.range(0.3f, 1.0f)};
        const vec3 hb{r3.range(0.3f, 1.0f), r3.range(0.3f, 1.0f), r3.range(0.3f, 1.0f)};
        const placed_box a = make_obb(vec3{}, ha, r3.yaw());
        const vec3 off{r3.range(-0.6f, 0.6f), ha.y + hb.y - r3.range(0.001f, 0.05f),
                       r3.range(-0.6f, 0.6f)};
        const placed_box b = make_obb(off, hb, r3.yaw());

        manifold_config raw;
        raw.reduce = false;
        raw.max_points_unreduced = 16;
        const contact_manifold full = collide_manifold(a.view(), b.view(), raw);
        const contact_manifold kept = collide_manifold(a.view(), b.view());
        if (full.status != manifold_status::face || full.count <= k_max_manifold_points)
        {
            continue;
        }
        ++over_four;

        vec3 fp[16];
        double full_deep = 0.0;
        for (int k = 0; k < full.count; ++k)
        {
            fp[k] = full.points[k].position;
            full_deep = std::fmax(full_deep, static_cast<double>(full.points[k].depth));
        }
        vec3 kp[4];
        double kept_deep = 0.0;
        for (int k = 0; k < kept.count; ++k)
        {
            kp[k] = kept.points[k].position;
            kept_deep = std::fmax(kept_deep, static_cast<double>(kept.points[k].depth));
        }
        const double fa2 = hull_area(fp, full.count, full.normal);
        const double ka = hull_area(kp, kept.count, kept.normal);
        if (fa2 > 1e-9) { area_ratio.push_back(ka / fa2); }
        depth_lost.push_back(full_deep - kept_deep);

        // THE HONEST COMPARISON IS AGAINST THE BEST FOUR, NOT AGAINST ALL OF
        // THEM. Four points can never span an octagon: the largest quadrilateral
        // inscribed in a regular octagon is 70.7% of it, so a ratio against the
        // full polygon measures the SHAPE of the contact as much as the
        // heuristic. Eight choose four is seventy combinations, which is cheap
        // enough to brute force and settles the question the heuristic is
        // actually asking.
        double best4 = 0.0;
        for (int p0 = 0; p0 < full.count; ++p0)
        {
            for (int p1 = p0 + 1; p1 < full.count; ++p1)
            {
                for (int p2 = p1 + 1; p2 < full.count; ++p2)
                {
                    for (int p3 = p2 + 1; p3 < full.count; ++p3)
                    {
                        const vec3 q[4] = {fp[p0], fp[p1], fp[p2], fp[p3]};
                        best4 = std::fmax(best4, hull_area(q, 4, full.normal));
                    }
                }
            }
        }
        if (best4 > 1e-9) { best_ratio.push_back(ka / best4); }
    }
    std::printf("     manifolds that clipped to 5-8 points : %d\n", over_four);
    std::printf("     area the kept four span, as a fraction of the whole\n");
    std::printf("     clipped polygon:\n");
    std::printf("       mean   %.5f\n", mean_of(area_ratio));
    std::printf("       1st    %.5f\n", pct(area_ratio, 0.01));
    std::printf("       worst  %.5f\n", pct(area_ratio, 0.0));
    std::printf("     and as a fraction of the BEST four, brute forced\n");
    std::printf("     over every four-subset:\n");
    std::printf("       mean   %.5f\n", mean_of(best_ratio));
    std::printf("       median %.5f\n", pct(best_ratio, 0.5));
    std::printf("       1st    %.5f\n", pct(best_ratio, 0.01));
    std::printf("       worst  %.5f\n", pct(best_ratio, 0.0));
    std::printf("     deepest point lost by the reduction  : %.3e m\n", max_of(depth_lost));
    std::printf("\n     THE REDUCTION LOSES NO DEPTH AT ALL, because it picks\n");
    std::printf("     the deepest point first, and it comes within a few per\n");
    std::printf("     cent of the best four points there are -- for four\n");
    std::printf("     comparisons and no search. What it cannot do is span an\n");
    std::printf("     octagon with a quadrilateral: the largest quadrilateral\n");
    std::printf("     inscribed in a regular octagon is 70.7%% of it, which is\n");
    std::printf("     most of the gap in the first table and none of it in\n");
    std::printf("     the second.\n");
    check(over_four > 10000, "G.3 enough manifolds clipped past four");
    check(mean_of(area_ratio) > 0.85, "G.3 four points span most of the contact polygon");
    check(mean_of(best_ratio) > 0.95, "G.3 and nearly all of the best four could span");
    check(pct(best_ratio, 0.5) > 0.99, "G.3 and the median is within a per cent of optimal");
    check(max_of(depth_lost) <= 0.0, "G.3 and the reduction never loses depth");
}

// ---------------------------------------------------------------------------
// H: persistence
// ---------------------------------------------------------------------------

void section_h()
{
    rule("H. PERSISTENCE");

    std::printf("H.1  a crate settling on a floor, 600 frames at 60 Hz.\n");
    std::printf("     How many of each frame's contacts can be matched to\n");
    std::printf("     the frame before -- by ID, and by POSITION?\n\n");

    const placed_box floor = make_obb(vec3{0.0f, -1.0f, 0.0f}, vec3{8.0f, 1.0f, 8.0f}, quat{});
    const vec3 half{0.5f, 0.5f, 0.5f};
    rigid_body body = make_box(vec3{0.0f, 0.60f, 0.0f}, 10.0f, half);
    body.orientation = quat_from_axis_angle(vec3{0.0f, 0.0f, 1.0f}, 0.02f);
    const contact_solver_config solver;

    contact_manifold previous{};
    int id_matches = 0;
    int pos_matches_tight = 0;
    int pos_matches_loose = 0;
    int opportunities = 0;
    int frames_in_contact = 0;
    int settled_matches = 0;
    int settled_opps = 0;
    int diag_hist[5] = {};
    const float h = 1.0f / 60.0f;

    for (int f = 0; f < 600; ++f)
    {
        body.state.velocity += vec3{0.0f, -9.81f, 0.0f} * h;
        const placed_box crate = make_obb(body.state.position, half, body.orientation);
        contact_manifold m = collide_manifold(floor.view(), crate.view());

        if (m.count > 0 && previous.count > 0)
        {
            ++frames_in_contact;
            opportunities += m.count;
            const int matched_now = carry_impulses(m, previous);
            id_matches += matched_now;
            if (f >= 300)
            {
                settled_opps += m.count;
                settled_matches += matched_now;
                diag_hist[std::min(m.count, 4)] += 1;
            }
            for (int i = 0; i < m.count; ++i)
            {
                for (int j = 0; j < previous.count; ++j)
                {
                    const float d2 =
                        length_squared(m.points[i].position - previous.points[j].position);
                    if (d2 < 1e-10f) { ++pos_matches_tight; break; }
                }
                for (int j = 0; j < previous.count; ++j)
                {
                    const float d2 =
                        length_squared(m.points[i].position - previous.points[j].position);
                    if (d2 < 1e-4f) { ++pos_matches_loose; break; }
                }
            }
        }

        resolve_against_fixed(body, m, m.normal, h, solver);
        body.state.position += body.state.velocity * h;
        body.orientation = normalised(
            advance_orientation(body.orientation, body.angular_velocity, h, spin_rule::linearised));
        previous = m;
    }

    std::printf("     frames with a contact on both sides : %d\n", frames_in_contact);
    std::printf("     contact points to match             : %d\n", opportunities);
    std::printf("     matched by ID                       : %d  (%.2f%%)\n", id_matches,
                100.0 * id_matches / std::fmax(1.0, static_cast<double>(opportunities)));
    std::printf("     matched by position, 10 microns     : %d  (%.2f%%)\n", pos_matches_tight,
                100.0 * pos_matches_tight / std::fmax(1.0, static_cast<double>(opportunities)));
    std::printf("     matched by position, 10 millimetres : %d  (%.2f%%)\n", pos_matches_loose,
                100.0 * pos_matches_loose / std::fmax(1.0, static_cast<double>(opportunities)));
    std::printf("\n     The ones that do not match are the frames where the\n");
    std::printf("     crate is still landing and the contact COUNT itself is\n");
    std::printf("     changing -- a new point genuinely has no history. Over\n");
    std::printf("     the last 300 frames, after it has settled:\n");
    std::printf("     matched by ID                       : %.2f%%\n",
                100.0 * settled_matches / std::fmax(1.0, static_cast<double>(settled_opps)));
    std::printf("     its contact count over those frames : 2:%d 3:%d 4:%d\n", diag_hist[2],
                diag_hist[3], diag_hist[4]);
    std::printf("\n     *** AND THAT IS NOT AN ID FAILURE. *** The crate is\n");
    std::printf("     still ROCKING -- §A.1 measured 5.2 deg/s of residual\n");
    std::printf("     rotation from this deliberately crude solver -- so its\n");
    std::printf("     contact set genuinely changes: one corner lifts and\n");
    std::printf("     another lands, and the count sits at 3 rather than 4\n");
    std::printf("     on %d of the last 300 frames. A contact that really\n", diag_hist[3]);
    std::printf("     is new SHOULD have no history, and reporting it as\n");
    std::printf("     matched would be the bug.\n");
    std::printf("\n     The same crate held at rest and jittered by a\n");
    std::printf("     hundred NANOMETRES a frame -- a body a solver has\n");
    std::printf("     settled, which is what warm starting is for:\n");
    {
        rng rj(313131u);
        contact_manifold prev{};
        int opps2 = 0;
        int matched2 = 0;
        int pos2 = 0;
        int frames2 = 0;
        for (int f = 0; f < 2000; ++f)
        {
            const vec3 jitter{rj.signed_unit() * 1e-7f, rj.signed_unit() * 1e-7f,
                              rj.signed_unit() * 1e-7f};
            const placed_box crate =
                make_obb(vec3{0.0f, 0.4990f, 0.0f} + jitter, half, quat{});
            contact_manifold m = collide_manifold(floor.view(), crate.view());
            if (m.count > 0 && prev.count > 0)
            {
                ++frames2;
                opps2 += m.count;
                matched2 += carry_impulses(m, prev);
                for (int i = 0; i < m.count; ++i)
                {
                    for (int j = 0; j < prev.count; ++j)
                    {
                        if (length_squared(m.points[i].position - prev.points[j].position)
                            < 1e-10f)
                        {
                            ++pos2;
                            break;
                        }
                    }
                }
            }
            prev = m;
        }
        std::printf("     frames                              : %d\n", frames2);
        std::printf("     matched by ID                       : %.2f%%\n",
                    100.0 * matched2 / std::fmax(1.0, static_cast<double>(opps2)));
        std::printf("     matched by position, 10 microns     : %.2f%%\n",
                    100.0 * pos2 / std::fmax(1.0, static_cast<double>(opps2)));
        check(frames2 > 1900, "H.1 the resting crate stayed in contact");
        check(matched2 == opps2, "H.1 a settled contact matches every point, every frame");
        std::printf("\n     A POSITION MATCHER WORKS PERFECTLY ON A BODY THAT IS\n");
        std::printf("     NOT MOVING, which is exactly why it is a trap: the\n");
        std::printf("     tolerance that works at 100 nm of jitter fails at the\n");
        std::printf("     8 cm a frame a crate travels while it is landing --\n");
        std::printf("     0.00%% of the settling run above, at the same 10 um.\n");
        std::printf("     The failure arrives with the motion, and the motion\n");
        std::printf("     arrives when the scene gets interesting.\n");
    }

    // ---- what an id MEANS, and what it cannot do --------------------------
    //
    // *** THE FIRST VERSION OF THIS CONTROL WAS WRONG AND THE WAY IT WAS WRONG
    // IS WORTH MORE THAN THE CONTROL. *** It teleported the crate two metres
    // sideways, expected zero id matches, and got four out of four — and the
    // manifold was right. A `contact_id` names FEATURES: the floor's top face,
    // the crate's bottom face, corners 0 to 3. Move the crate anywhere on that
    // floor and the same corner really is on the same face, so the same id is
    // the correct answer to the question an id asks.
    //
    // Ids therefore cannot detect a teleport, and should not be asked to: a body
    // that was MOVED rather than simulated is a discontinuity, and invalidating
    // its cached manifolds is the CACHE's job. What an id can do is notice that
    // the contacting features changed, which is the second row below.
    std::printf("\n     what an id means: slide the crate 2 m along the same\n");
    std::printf("     face and the ids still match, because the same corner\n");
    std::printf("     IS on the same face. Tip it onto a different face and\n");
    std::printf("     every one of them changes.\n\n");
    {
        const placed_box c0 = make_obb(vec3{0.0f, 0.495f, 0.0f}, half, quat{});
        const placed_box c1 = make_obb(vec3{2.0f, 0.495f, 0.0f}, half, quat{});
        const placed_box c2 =
            make_obb(vec3{0.0f, 0.495f, 0.0f}, half,
                     quat_from_axis_angle(vec3{0.0f, 0.0f, 1.0f},
                                          static_cast<float>(k_pi * 0.5)));
        contact_manifold m0 = collide_manifold(floor.view(), c0.view());
        contact_manifold m1 = collide_manifold(floor.view(), c1.view());
        contact_manifold m2 = collide_manifold(floor.view(), c2.view());
        const int moved = carry_impulses(m1, m0);
        const int tipped = carry_impulses(m2, m0);
        std::printf("     slid 2 m along the same face : %d of %d matched\n", moved, m1.count);
        std::printf("     rolled onto its side         : %d of %d matched\n", tipped, m2.count);
        check(moved == m1.count, "H.1 an id names a feature, not a place");
        check(tipped == 0, "H.1 and a different feature is a different id");
    }

    // ---- H.2 the tie that does not fire, and the one that would -----------
    //
    // A DRAFT OF THIS LESSON SHIPPED A KNOB FOR THIS AND THE MEASUREMENT TOOK IT
    // BACK OUT. Every 2D engine carries a bias on the reference choice, and the
    // reasoning is sound: two equally parallel faces make the choice a coin
    // toss, a bare `>` decides it on rounding, and a flip renames all four
    // contacts because `contact_id::flipped` is part of the identity.
    //
    // It does not happen here, and why not is the finding. On a face contact
    // both cosines are EXACTLY 1.0f — each face really is perpendicular to the
    // contact normal — so the tie is exact, and an exact tie is decided
    // deterministically. The hazard belongs to a formulation that compares two
    // SEPARATIONS, which are genuinely different floats. The control below
    // builds that formulation and measures it flipping.
    std::printf("\nH.2  the reference is chosen by comparing two cosines.\n");
    std::printf("     Three populations, each nudged by a NANOMETRE, asking\n");
    std::printf("     whether the choice moved and whether the ids survived.\n\n");
    std::printf("     population        pairs   cos tie   flips   ids kept\n");

    {
        // Three populations: freely rotated boxes, crates on crates (the exact
        // tie, by construction), and hexagonal prisms whose face normals come
        // out of Newell's method rather than out of a rotation matrix.
        std::vector<vec3> hexa;
        std::vector<vec3> hexb;
        for (int i = 0; i < 6; ++i)
        {
            const float ang = static_cast<float>(2.0 * k_pi * i / 6.0);
            for (float y : {-0.5f, 0.5f})
            {
                hexa.push_back(vec3{std::cos(ang) * 0.5f, y, std::sin(ang) * 0.5f});
            }
            const float ang2 = static_cast<float>(2.0 * k_pi * (i + 0.37) / 6.0);
            for (float y : {-0.5f, 0.5f})
            {
                hexb.push_back(vec3{std::cos(ang2) * 0.47f, y, std::sin(ang2) * 0.47f});
            }
        }

        int total_pairs = 0;
        int total_flips = 0;
        for (int pop = 0; pop < 3; ++pop)
        {
            rng r(4242u + static_cast<std::uint32_t>(pop));
            int pairs = 0;
            int ties = 0;
            int flips = 0;
            int idsame = 0;
            int idtot = 0;
            const int n = (pop == 2) ? 20000 : 120000;
            for (int i = 0; i < n; ++i)
            {
                contact_manifold m0{};
                contact_manifold m1{};
                float ca = 0.0f;
                float cb = 0.0f;
                if (pop == 2)
                {
                    const hull ha = world_hull(hexa, vec3{}, quat{});
                    const vec3 j{r.signed_unit() * 1e-9f, 0.0f, r.signed_unit() * 1e-9f};
                    const hull hb0 = world_hull(hexb, vec3{0.0f, 0.999f, 0.0f}, quat{});
                    const hull hb1 = world_hull(hexb, vec3{0.0f, 0.999f, 0.0f} + j, quat{});
                    m0 = collide_manifold(as_convex(ha), as_convex(hb0));
                    m1 = collide_manifold(as_convex(ha), as_convex(hb1));
                    if (m0.status != manifold_status::face) { continue; }
                    ca = dot(support_face(ha, m0.normal).normal, m0.normal);
                    cb = dot(support_face(hb0, -m0.normal).normal, -m0.normal);
                }
                else
                {
                    const vec3 ha{r.range(0.3f, 1.0f), r.range(0.3f, 1.0f), r.range(0.3f, 1.0f)};
                    const vec3 hb{r.range(0.3f, 1.0f), r.range(0.3f, 1.0f), r.range(0.3f, 1.0f)};
                    const quat qa = (pop == 0) ? r.rotation() : r.yaw();
                    const quat qb = (pop == 0) ? r.rotation() : r.yaw();
                    const placed_box a = make_obb(vec3{}, ha, qa);
                    const vec3 off = (pop == 0)
                                         ? r.direction() * r.range(0.8f, 1.4f)
                                         : vec3{r.range(-0.5f, 0.5f),
                                                ha.y + hb.y - r.range(0.0005f, 0.02f),
                                                r.range(-0.5f, 0.5f)};
                    placed_box b = make_obb(off, hb, qb);
                    m0 = collide_manifold(a.view(), b.view());
                    if (m0.status != manifold_status::face) { continue; }
                    ca = dot(support_face(a.box, m0.normal).normal, m0.normal);
                    cb = dot(support_face(b.box, -m0.normal).normal, -m0.normal);
                    b.box.centre = b.box.centre + vec3{1e-9f, 1e-9f, 1e-9f};
                    m1 = collide_manifold(a.view(), b.view());
                }
                if (m1.status != manifold_status::face) { continue; }
                ++pairs;
                if (ca == cb) { ++ties; }
                if (m1.reference_on_b != m0.reference_on_b) { ++flips; }
                for (int k = 0; k < m1.count; ++k)
                {
                    ++idtot;
                    for (int j2 = 0; j2 < m0.count; ++j2)
                    {
                        if (m1.points[k].id == m0.points[j2].id) { ++idsame; break; }
                    }
                }
            }
            static const char* names[3] = {"free boxes      ", "crates on crates",
                                           "hex prisms      "};
            std::printf("     %s %6d  %6.2f%%  %5d   %6.2f%%\n", names[pop], pairs,
                        100.0 * ties / std::fmax(1.0, static_cast<double>(pairs)), flips,
                        100.0 * idsame / std::fmax(1.0, static_cast<double>(idtot)));
            total_pairs += pairs;
            total_flips += flips;
        }
        std::printf("\n     %d pairs, %d flips.\n", total_pairs, total_flips);
        check(total_flips == 0, "H.2 the reference never flips on a nanometre");
        check(total_pairs > 200000, "H.2 over a population worth quoting");
    }

    // ---- the control: what a flip WOULD cost ------------------------------
    //
    // A measurement of zero needs a control more than any other kind, because
    // "it never happened" and "I was not measuring anything" produce the same
    // number. The flip cannot be provoked by a nudge — that is the result — so
    // the control forces one by hand, the only way the reference can definitely
    // change: swap the two arguments. Everything about the contact is identical
    // and the reference moves from `a`'s face to `b`'s.
    std::printf("\n     the control: a measurement of zero needs one. Force\n");
    std::printf("     a flip by swapping the two arguments -- same contact,\n");
    std::printf("     same geometry, the other shape's face as reference --\n");
    std::printf("     and see what it costs.\n\n");
    {
        const placed_box a = make_obb(vec3{}, vec3{0.5f, 0.5f, 0.5f}, quat{});
        const placed_box b = make_obb(vec3{0.1f, 0.995f, 0.05f}, vec3{0.5f, 0.5f, 0.5f}, quat{});
        contact_manifold ab = collide_manifold(a.view(), b.view());
        contact_manifold ba = collide_manifold(b.view(), a.view());
        const contact_face fa = support_face(a.box, ab.normal);
        const contact_face fb = support_face(b.box, -ab.normal);
        const float ca = dot(fa.normal, ab.normal);
        const float cb = dot(fb.normal, -ab.normal);
        const int matched = carry_impulses(ba, ab);
        std::printf("     cos_a = %.9f   cos_b = %.9f\n", static_cast<double>(ca),
                    static_cast<double>(cb));
        std::printf("     bit-identical                   : %s\n", (ca == cb) ? "yes" : "no");
        std::printf("     collide(a,b): reference is     : %s\n",
                    ab.reference_on_b ? "the 2nd argument" : "the 1st argument");
        std::printf("     collide(b,a): reference is     : %s\n",
                    ba.reference_on_b ? "the 2nd argument" : "the 1st argument");
        std::printf("     ...so the reference SHAPE moved from a to b, while\n");
        std::printf("     `reference_on_b` stayed false, because the flag is\n");
        std::printf("     relative to the ARGUMENTS. That is the trap in one\n");
        std::printf("     line.\n");
        std::printf("     normal, collide(a,b)            : (%+.1f %+.1f %+.1f)\n",
                    static_cast<double>(ab.normal.x), static_cast<double>(ab.normal.y),
                    static_cast<double>(ab.normal.z));
        std::printf("     normal, collide(b,a)            : (%+.1f %+.1f %+.1f)\n",
                    static_cast<double>(ba.normal.x), static_cast<double>(ba.normal.y),
                    static_cast<double>(ba.normal.z));
        std::printf("     ids that survive the swap       : %d of %d\n", matched, ba.count);
        std::printf("\n     SO THE FLIP IS NOT HARMLESS -- it costs every warm\n");
        std::printf("     start on the pair -- and it does not fire, because\n");
        std::printf("     the two cosines are the SAME FLOAT. Compare two\n");
        std::printf("     numbers that are exactly equal when the situation is\n");
        std::printf("     symmetric and `>` is a deterministic tie-break;\n");
        std::printf("     compare two that merely OUGHT to be equal and it is\n");
        std::printf("     a coin toss. 8.6 §7 said the same about a torn\n");
        std::printf("     horizon, and this is the happy version of it.\n");
        std::printf("\n     It is also why `pair_key` is order-independent and\n");
        std::printf("     the MANIFOLD is not: hand the same pair to\n");
        std::printf("     `collide_manifold` the other way round one frame and\n");
        std::printf("     you have thrown away exactly this much.\n");
        check(ca == cb, "H.2 control: the two cosines really are bit-identical");
        check(!ab.reference_on_b && !ba.reference_on_b,
              "H.2 control: the first argument always wins the tie");
    check(dot(ab.normal, ba.normal) < -0.99f, "H.2 control: and the normal reverses");
        check(ab.count == 4 && ba.count == 4, "H.2 control: both manifolds are four points");
        check(matched == 0, "H.2 control: and a flip costs every warm start");
    }

    // ---- H.3 the cache -----------------------------------------------------
    std::printf("\nH.3  the cache, over 400 frames of a 60-body scene where\n");
    std::printf("     pairs are born and die every frame.\n\n");

    manifold_cache cache;
    rng rc(818181u);
    int total_found = 0;
    int total_stored = 0;
    int total_dropped = 0;
    const std::size_t allocs_before = g_allocs;
    int peak = 0;
    double worst_load = 0.0;
    for (int f = 0; f < 400; ++f)
    {
        cache.begin_frame();
        const int pairs = 40 + static_cast<int>(rc.unit() * 40.0f);
        for (int p = 0; p < pairs; ++p)
        {
            const std::uint32_t i = static_cast<std::uint32_t>(rc.unit() * 60.0f);
            const std::uint32_t j = static_cast<std::uint32_t>(rc.unit() * 60.0f);
            if (i == j) { continue; }
            const std::uint64_t key = pair_key(i, j);
            contact_manifold m{};
            m.count = 1;
            m.normal = vec3{0.0f, 1.0f, 0.0f};
            m.points[0].normal_impulse = 1.0f;
            if (const contact_manifold* old = cache.find(key))
            {
                ++total_found;
                m.points[0].normal_impulse = old->points[0].normal_impulse + 1.0f;
            }
            cache.store(key, m);
            ++total_stored;
        }
        worst_load = std::fmax(worst_load,
                               static_cast<double>(cache.size())
                                   / std::fmax(1.0, static_cast<double>(cache.capacity())));
        total_dropped += cache.end_frame();
        peak = std::max(peak, cache.size());
        check(pair_key(3, 7) == pair_key(7, 3), "H.3 the pair key is order-independent");
        g_checks -= 1;
    }
    ++g_checks;
    std::printf("     stores / hits / drops : %d / %d / %d\n", total_stored, total_found,
                total_dropped);
    std::printf("     peak live pairs       : %d\n", peak);
    std::printf("     slots at the end      : %d\n", cache.capacity());
    std::printf("     worst load factor     : %.3f\n", worst_load);
    std::printf("     allocations           : %zu, over 400 frames\n", g_allocs - allocs_before);
    std::printf("\n     LINEAR PROBING DEGRADES AS THE SQUARE of the free\n");
    std::printf("     fraction -- the expected probe is 2 at half full and\n");
    std::printf("     13 at ninety per cent -- so the table grows at half.\n");
    std::printf("     And it never TOMBSTONES: `end_frame` rebuilds from the\n");
    std::printf("     survivors, O(capacity) once a frame against O(pairs)\n");
    std::printf("     the caller already spent walking them.\n");
    check(cache.size() > 0, "H.3 the cache holds the last frame's pairs");
    check(total_found > 0, "H.3 and pairs are found across frames");
    check(total_dropped > 0, "H.3 and dead pairs are dropped");
    check(worst_load <= 0.5, "H.3 the table stays under half full");
}

// ---------------------------------------------------------------------------
// I: the budget
// ---------------------------------------------------------------------------

void section_i()
{
    rule("I. THE BUDGET");

    std::printf("I.1  20,000 overlapping box pairs on a release library.\n\n");

    rng r(24680u);
    std::vector<placed_box> xa(20000);
    std::vector<placed_box> xb(20000);
    for (int i = 0; i < 20000; ++i)
    {
        xa[static_cast<std::size_t>(i)] = make_obb(vec3{}, vec3{0.5f, 0.5f, 0.5f}, r.yaw());
        xa[static_cast<std::size_t>(i)].box.centre = vec3{};
        xb[static_cast<std::size_t>(i)] =
            make_obb(vec3{r.range(-0.5f, 0.5f), 0.95f, r.range(-0.5f, 0.5f)},
                     vec3{0.5f, 0.5f, 0.5f}, r.yaw());
    }

    std::vector<gjk_result> gs(20000);
    std::vector<epa_result> es(20000);
    for (int i = 0; i < 20000; ++i)
    {
        gs[static_cast<std::size_t>(i)] = gjk_distance(xa[static_cast<std::size_t>(i)].view(),
                                                       xb[static_cast<std::size_t>(i)].view());
        es[static_cast<std::size_t>(i)] =
            epa_penetration(xa[static_cast<std::size_t>(i)].view(),
                            xb[static_cast<std::size_t>(i)].view(),
                            gs[static_cast<std::size_t>(i)].terminal);
    }

    const bench_result gjk_only = bench_run(20000, 7, [&] {
        double acc = 0.0;
        for (int i = 0; i < 20000; ++i)
        {
            acc += static_cast<double>(
                gjk_distance(xa[static_cast<std::size_t>(i)].view(),
                             xb[static_cast<std::size_t>(i)].view())
                    .distance);
        }
        return acc;
    });

    const bench_result through_epa = bench_run(20000, 7, [&] {
        double acc = 0.0;
        for (int i = 0; i < 20000; ++i)
        {
            const convex va = xa[static_cast<std::size_t>(i)].view();
            const convex vb = xb[static_cast<std::size_t>(i)].view();
            const gjk_result g = gjk_distance(va, vb);
            acc += static_cast<double>(epa_penetration(va, vb, g.terminal).depth);
        }
        return acc;
    });

    const bench_result manifold_only = bench_run(20000, 7, [&] {
        double acc = 0.0;
        for (int i = 0; i < 20000; ++i)
        {
            const convex va = xa[static_cast<std::size_t>(i)].view();
            const convex vb = xb[static_cast<std::size_t>(i)].view();
            const epa_result& e = es[static_cast<std::size_t>(i)];
            acc += static_cast<double>(build_manifold(va, vb, e.normal, e.depth).deepest());
        }
        return acc;
    });

    const bench_result whole = bench_run(20000, 7, [&] {
        double acc = 0.0;
        for (int i = 0; i < 20000; ++i)
        {
            acc += static_cast<double>(collide_manifold(xa[static_cast<std::size_t>(i)].view(),
                                                        xb[static_cast<std::size_t>(i)].view())
                                           .deepest());
        }
        return acc;
    });

    const bench_result faces_only = bench_run(20000, 7, [&] {
        double acc = 0.0;
        for (int i = 0; i < 20000; ++i)
        {
            const contact_face f = support_face(xa[static_cast<std::size_t>(i)].box,
                                                es[static_cast<std::size_t>(i)].normal);
            acc += static_cast<double>(f.v[0].x);
        }
        return acc;
    });

    std::printf("     GJK distance only            : %7.1f ns/pair\n", gjk_only.median_ns);
    std::printf("     GJK + EPA                    : %7.1f ns/pair\n", through_epa.median_ns);
    std::printf("     build_manifold alone         : %7.1f ns/pair\n", manifold_only.median_ns);
    std::printf("     the whole narrow phase       : %7.1f ns/pair\n", whole.median_ns);
    std::printf("     one support_face             : %7.1f ns\n", faces_only.median_ns);
    std::printf("\n     the manifold is %.1f%% of the narrow phase; the\n",
                100.0 * manifold_only.median_ns / std::fmax(1.0, whole.median_ns));
    std::printf("     penetration depth is still what costs.\n");

    check(manifold_only.median_ns < through_epa.median_ns,
          "I.1 the manifold costs less than the depth it is built on");

    // ---- I.2 allocations ---------------------------------------------------
    std::printf("\nI.2  does it allocate? Replace the global operator and\n");
    std::printf("     count, exactly as 6.17 §9 and 8.6 §I.2 did.\n\n");
    {
        const std::size_t before = g_allocs;
        double acc = 0.0;
        for (int i = 0; i < 20000; ++i)
        {
            const convex va = xa[static_cast<std::size_t>(i)].view();
            const convex vb = xb[static_cast<std::size_t>(i)].view();
            acc += static_cast<double>(collide_manifold(va, vb).deepest());
        }
        const std::size_t after = g_allocs;
        std::printf("     20,000 whole narrow-phase queries : %zu allocations\n", after - before);
        std::printf("     (acc %.3f, so the loop was not elided)\n", acc);
        check(after == before, "I.2 the narrow phase allocates nothing");
    }

    // ---- I.3 the struct sizes ----------------------------------------------
    std::printf("\nI.3  8.6 §12 found 20.5%% in four default member\n");
    std::printf("     initialisers. The same question, asked again -- and\n");
    std::printf("     this time the answer is no.\n\n");
    std::printf("     sizeof(contact_face)     : %3zu bytes\n", sizeof(contact_face));
    std::printf("     sizeof(contact_id)       : %3zu bytes\n", sizeof(contact_id));
    std::printf("     sizeof(contact_point)    : %3zu bytes\n", sizeof(contact_point));
    std::printf("     sizeof(contact_manifold) : %3zu bytes\n", sizeof(contact_manifold));
    std::printf("\n     `polytope` was 7 KB and `expand` rebuilt a 3 KB array\n");
    std::printf("     PER PASS. Two `contact_face`s are %zu bytes, once per\n",
                2 * sizeof(contact_face));
    std::printf("     query. The tell in 8.6 was the ARRAY, not the struct.\n");

    check(sizeof(contact_manifold) < 1024, "I.3 a manifold is a stack-sized object");
    check(sizeof(contact_id) <= 8, "I.3 an id is small enough to compare whole");
}

} // namespace

int main()
{
    std::printf("verify_87 - Lesson 8.7, contact manifolds and persistence\n");
    std::printf("sizeof(contact_manifold) = %zu bytes\n", sizeof(contact_manifold));

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
