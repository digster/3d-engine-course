// scratch/verify_83.cpp — every number Lesson 8.3 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_83.sh
//
// Nine sections, in the lesson's order:
//
//   A  torque is r x F, and the sign our convention fixes
//   B  the inertia tensor, derived and checked two ways
//   C  the closed forms against a brute-force sum
//   D  the parallel-axis theorem
//   E  R I R^T is a basis change, and the plausible wrong one
//   F  an orientation is not a vector
//   G  L is conserved and omega is not
//   H  the tennis racket theorem
//   I  the budget
//
// EVERY SECTION CARRIES A CONTROL, and 8.1/8.2 left the rule in two halves: ask
// what the control would say if the thing were COMPLETELY BROKEN, and what it
// would say if the thing were completely FINE. Three here are written against
// the second half. C.4 runs the brute-force sampler on a SINGLE point mass,
// where it must reproduce `inertia_of_point` to the bit; E.4 runs the basis
// change with the identity rotation, where the tensor must come back bit for
// bit; and G.4 runs the whole torque-free tumble on a SPHERE, whose isotropic
// tensor makes `omega x (I omega)` identically zero — so a sphere that wobbles
// means the instrument is broken and not the physics.
//
// AND ONE SECTION IS A CONTROL FOR A CLAIM MADE IN A COMMENT. C.2 checks the
// grid sampler against the closed form twice: once by summing point masses
// (which is LOW by exactly 1/N^2, predicted before it was measured) and once by
// summing each cell's own box tensor as well (which is exact at any N). The
// second one passes only if the parallel-axis theorem is exactly true rather
// than approximately true, which is the section's actual subject.
//
// PRECISION. The engine integrates in `float`, so this harness does too. Closed
// forms are evaluated in double and simulations in float — the only honest way
// round, since a reference computed at the same precision as the thing it checks
// cannot tell you which one is wrong.
//
// OUTPUT WIDTH IS A CONSTRAINT, not a preference. The lesson quotes this
// program's transcript in <pre> blocks that scroll and never wrap, and the fold
// is at about 66 characters. Nothing below exceeds 66.

#include <engine/core/bench.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/inertia.hpp>
#include <engine/phys/integrate.hpp>
#include <engine/phys/rigid_body.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using engine::mat3;
using engine::mat3_from_quat;
using engine::quat;
using engine::quat_from_axis_angle;
using engine::vec3;
using engine::phys::add_force;
using engine::phys::add_force_at;
using engine::phys::add_impulse_at;
using engine::phys::add_torque;
using engine::phys::advance_orientation;
using engine::phys::angular_momentum;
using engine::phys::body_id;
using engine::phys::body_world;
using engine::phys::inertia_assembly;
using engine::phys::inertia_assembly_result;
using engine::phys::inertia_capsule;
using engine::phys::inertia_of;
using engine::phys::inertia_of_point;
using engine::phys::inertia_of_points;
using engine::phys::inertia_part;
using engine::phys::inertia_report;
using engine::phys::inertia_solid_box;
using engine::phys::inertia_solid_cylinder;
using engine::phys::inertia_solid_sphere;
using engine::phys::inertia_thin_rod;
using engine::phys::inspect_inertia;
using engine::phys::inverse_inertia;
using engine::phys::kinetic_energy;
using engine::phys::make_box;
using engine::phys::make_dynamic;
using engine::phys::make_sphere;
using engine::phys::point_mass;
using engine::phys::point_velocity;
using engine::phys::rigid_body;
using engine::phys::rotate_inertia;
using engine::phys::set_inertia;
using engine::phys::shift_inertia;
using engine::phys::spin_angle_error;
using engine::phys::spin_inflation;
using engine::phys::spin_rule;
using engine::phys::unshift_inertia;
using engine::phys::world_inertia;
using engine::phys::world_inv_inertia;
using engine::phys::world_inverse_inertia;

namespace
{

constexpr float k_h60 = 1.0f / 60.0f;
constexpr double k_pi = 3.14159265358979323846;

void rule(const char* title)
{
    std::printf("\n%s\n", title);
    for (int i = 0; i < 66; ++i) { std::putchar('-'); }
    std::putchar('\n');
}

/// Print a 3x3 in WRITTEN notation — rows across the page — which is not how it
/// is stored. Printing the columns instead is the fastest way to convince
/// yourself a correct tensor is transposed.
void print_mat3(const char* label, const mat3& m, int indent = 4)
{
    for (int row = 0; row < 3; ++row)
    {
        std::printf("%*s", indent, "");
        if (row == 1) { std::printf("%-16s", label); }
        else          { std::printf("%-16s", ""); }
        // NEGATIVE ZERO IS PRINTED AS ZERO. `-m*x*y` with x or y exactly 0
        // is a legitimate -0.0f, and a column of "-0.000000" in a transcript
        // reads as a sign error rather than as the absence of a term.
        auto tidy = [&m](int r, int c) {
            const double v = static_cast<double>(m.at(r, c));
            return (v == 0.0) ? 0.0 : v;
        };
        std::printf("| %10.6f %10.6f %10.6f |\n",
                    tidy(row, 0), tidy(row, 1), tidy(row, 2));
    }
}

/// Largest absolute element-wise difference between two matrices.
float max_diff(const mat3& a, const mat3& b)
{
    float worst = 0.0f;
    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            worst = std::max(worst, std::fabs(a.at(r, c) - b.at(r, c)));
        }
    }
    return worst;
}

bool identical(const mat3& a, const mat3& b)
{
    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            if (a.at(r, c) != b.at(r, c)) { return false; }
        }
    }
    return true;
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
        // xorshift32, Marsaglia. Three shifts, full period, and enough for
        // picking test directions.
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    /// Uniform in [-1, 1].
    float signed_unit()
    {
        return static_cast<float>(next() >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

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

private:
    std::uint32_t state_;
};

// ===========================================================================
// A — torque is r x F, and the sign our convention fixes
// ===========================================================================

void section_a()
{
    rule("A  TORQUE IS r x F, AND THE SIGN OUR CONVENTION FIXES");

    // A 1 kg box, half a metre on a side in x and z, one metre tall, at the
    // origin. Push it on the top face, horizontally.
    rigid_body b = make_box(vec3{}, 1.0f, vec3{0.5f, 1.0f, 0.5f});

    const vec3 push{10.0f, 0.0f, 0.0f};        // 10 N along +x
    const vec3 at{0.0f, 1.0f, 0.0f};           // the top face, 1 m up

    add_force_at(b, push, at);

    std::printf("  a 1 kg box, pushed at (0, 1, 0) with F = (10, 0, 0) N\n\n");
    std::printf("    force   %8.4f %8.4f %8.4f  N\n",
                static_cast<double>(b.force.x), static_cast<double>(b.force.y),
                static_cast<double>(b.force.z));
    std::printf("    torque  %8.4f %8.4f %8.4f  N.m\n",
                static_cast<double>(b.torque.x), static_cast<double>(b.torque.y),
                static_cast<double>(b.torque.z));

    const vec3 by_hand = cross(at - b.state.position, push);
    std::printf("\n  A.1  r x F by hand:  %8.4f %8.4f %8.4f\n",
                static_cast<double>(by_hand.x), static_cast<double>(by_hand.y),
                static_cast<double>(by_hand.z));
    std::printf("       identical to add_force_at: %s\n",
                (by_hand == b.torque) ? "yes" : "NO");

    // The SIGN is a convention question and the convention is
    // conventions.html §10's right-handed basis. Pushing the top of a box in
    // +x tips it over toward +x, which is a rotation about -z.
    std::printf("\n  A.2  the sign: a push at +y in the +x direction\n");
    std::printf("       gives a torque about %s, i.e. the box tips\n",
                (b.torque.z < 0.0f) ? "-z" : "+z");
    std::printf("       toward +x. Right-hand rule, right-handed basis.\n");

    // A force aimed straight at the centre of mass produces NO torque, because
    // r and F are parallel and the cross product of parallel vectors is zero.
    rigid_body c = make_box(vec3{}, 1.0f, vec3{0.5f, 1.0f, 0.5f});
    const vec3 corner{0.5f, 1.0f, 0.5f};
    const vec3 aimed = (vec3{} - corner) * 20.0f;   // from the corner, at the centre
    add_force_at(c, aimed, corner);

    std::printf("\n  A.3  a force AT a corner but AIMED at the centre:\n");
    std::printf("       |torque| = %.4e N.m  (parallel r and F)\n",
                static_cast<double>(length(c.torque)));

    // A COUPLE: two equal and opposite forces at different points. Net force
    // exactly zero, net torque not — which is what a pure `add_torque` models.
    rigid_body d = make_box(vec3{}, 1.0f, vec3{0.5f, 1.0f, 0.5f});
    add_force_at(d, vec3{5.0f, 0.0f, 0.0f}, vec3{0.0f, 1.0f, 0.0f});
    add_force_at(d, vec3{-5.0f, 0.0f, 0.0f}, vec3{0.0f, -1.0f, 0.0f});

    std::printf("\n  A.4  a COUPLE: +5 N at +y, -5 N at -y\n");
    std::printf("       net force  %.4e N   (exactly zero)\n",
                static_cast<double>(length(d.force)));
    std::printf("       net torque %8.4f N.m about z\n",
                static_cast<double>(d.torque.z));

    // CONTROL: the same force applied AT the centre of mass. Torque must be
    // exactly zero — not small, zero — because `r` is the zero vector and every
    // term of the cross product has a factor of it.
    rigid_body e = make_box(vec3{}, 1.0f, vec3{0.5f, 1.0f, 0.5f});
    add_force_at(e, push, e.state.position);
    std::printf("\n  A.5  CONTROL force at the centre: torque == 0 exactly\n");
    std::printf("       %s   force still %6.2f N\n",
                (e.torque == vec3{}) ? "yes" : "NO",
                static_cast<double>(e.force.x));
}

// ===========================================================================
// B — the inertia tensor, derived and checked two ways
// ===========================================================================

void section_b()
{
    rule("B  THE INERTIA TENSOR, DERIVED AND CHECKED TWO WAYS");

    // The derivation's one step: L = sum m (r x v) with v = omega x r, and the
    // triple product turns r x (omega x r) into (|r|^2 I - r (x) r) omega.
    //
    // So: build the matrix, apply it to omega, and compare against the cross
    // products done directly. If these disagree, equation (1) is wrong.
    rng gen(0x8311u);
    float worst_triple = 0.0f;
    bool skew_identical = true;

    for (int trial = 0; trial < 20000; ++trial)
    {
        const vec3 r = gen.direction() * (0.1f + 2.0f * (gen.signed_unit() * 0.5f + 0.5f));
        const vec3 w = gen.direction() * (0.1f + 9.9f * (gen.signed_unit() * 0.5f + 0.5f));

        const vec3 direct = cross(r, cross(w, r));
        const mat3 tensor = mat3::identity() * length_squared(r) - outer(r, r);
        const vec3 through = tensor * w;

        const float scale = std::max(1.0f, length(direct));
        worst_triple = std::max(worst_triple, length(direct - through) / scale);

        // The second route: -[r]x [r]x, the cross-product matrix squared and
        // negated. Same algebra, different operators — and `inertia_of_point`
        // now uses this one, for a reason B.3 measures.
        const mat3 by_skew = -(skew(r) * skew(r));
        if (!identical(by_skew, tensor)) { skew_identical = false; }
    }

    std::printf("  20,000 random (r, omega) pairs\n\n");
    std::printf("  B.1  r x (omega x r)  vs  (|r|^2 I - r(x)r) omega\n");
    std::printf("       worst relative difference  %.4e\n",
                static_cast<double>(worst_triple));

    std::printf("\n  B.2  -[r]x [r]x  vs  |r|^2 I - r(x)r\n");
    std::printf("       algebraically identical; bit-identical in float\n");
    std::printf("       on all 20,000: %s\n", skew_identical ? "yes" : "no");

    // ...and B.3 is why that "no" matters rather than being a curiosity. The
    // derived form's diagonal is `|r|^2 - x^2`: a sum of three squares with one
    // of them subtracted straight off again. For a long thin body the surviving
    // terms are far below the sum's own ulp, and they are simply gone.
    std::printf("\n  B.3  the same two forms on a LONG THIN body,\n");
    std::printf("       a sample at r = (1000, 0.001, 0):\n\n");
    {
        const vec3 thin{1000.0f, 0.001f, 0.0f};
        const mat3 derived = mat3::identity() * length_squared(thin) - outer(thin, thin);
        const mat3 conditioned = inertia_of_point(1.0f, thin);
        const double truth = 1e-6;

        std::printf("       |r|^2 I - r(x)r     Ixx = %.6e\n",
                    static_cast<double>(derived.c0.x));
        std::printf("       inertia_of_point    Ixx = %.6e\n",
                    static_cast<double>(conditioned.c0.x));
        std::printf("       exact                     %.6e\n", truth);
        std::printf("       relative error of the derived form: %.1f%%\n",
                    100.0 * std::fabs(static_cast<double>(derived.c0.x) - truth) / truth);
        std::printf("       A plank, a rail, a sword. The zero it returns is\n");
        std::printf("       a singular tensor and a body that will not spin\n");
        std::printf("       about its own length.\n");
    }

    // A point mass resists rotation about an axis through itself not at all.
    const vec3 r{0.0f, 2.0f, 0.0f};
    const mat3 p = inertia_of_point(3.0f, r);
    print_mat3("B.4 3 kg at", p);
    std::printf("         (0, 2, 0)\n");
    std::printf("       spun about y (through it):  I = %.6f\n",
                static_cast<double>(p.c1.y));
    std::printf("       spun about x (across it) :  I = %.6f  = m r^2\n",
                static_cast<double>(p.c0.x));

    // CONTROL: symmetry. Every tensor equation (1) can build is symmetric,
    // because both of its terms are. A nonzero value here is a bug, not a
    // tolerance.
    float worst_asym = 0.0f;
    for (int trial = 0; trial < 20000; ++trial)
    {
        const vec3 q = gen.direction() * 3.0f;
        worst_asym = std::max(worst_asym, asymmetry(inertia_of_point(2.5f, q)));
    }
    std::printf("\n  B.5  CONTROL asymmetry over 20,000 tensors: %.4e\n",
                static_cast<double>(worst_asym));
}

// ===========================================================================
// C — the closed forms against a brute-force sum
// ===========================================================================

/// Fill a box with an N x N x N grid of equal point masses.
std::vector<point_mass> box_grid(float mass, vec3 half, int n)
{
    std::vector<point_mass> out;
    out.reserve(static_cast<std::size_t>(n) * static_cast<std::size_t>(n)
                * static_cast<std::size_t>(n));

    const float m = mass / static_cast<float>(n * n * n);
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            for (int k = 0; k < n; ++k)
            {
                // Cell CENTRES, not corners: the samples have to be the
                // centroids of equal sub-volumes or the sum is biased toward
                // one face and the whole comparison is meaningless.
                const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
                const float v = (static_cast<float>(j) + 0.5f) / static_cast<float>(n);
                const float w = (static_cast<float>(k) + 0.5f) / static_cast<float>(n);
                out.push_back({vec3{half.x * (2.0f * u - 1.0f),
                                    half.y * (2.0f * v - 1.0f),
                                    half.z * (2.0f * w - 1.0f)},
                               m});
            }
        }
    }
    return out;
}

/// The same grid, clipped to a sphere of `radius` and renormalised so the kept
/// points carry the full mass.
std::vector<point_mass> sphere_grid(float mass, float radius, int n)
{
    std::vector<point_mass> out;
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            for (int k = 0; k < n; ++k)
            {
                const float u = (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
                const float v = (static_cast<float>(j) + 0.5f) / static_cast<float>(n);
                const float w = (static_cast<float>(k) + 0.5f) / static_cast<float>(n);
                const vec3 p{radius * (2.0f * u - 1.0f),
                             radius * (2.0f * v - 1.0f),
                             radius * (2.0f * w - 1.0f)};
                if (length_squared(p) <= radius * radius) { out.push_back({p, 0.0f}); }
            }
        }
    }
    const float m = mass / static_cast<float>(out.size());
    for (point_mass& p : out) { p.mass = m; }
    return out;
}

void section_c()
{
    rule("C  THE CLOSED FORMS AGAINST A BRUTE-FORCE SUM");

    const float mass = 4.0f;
    const vec3 half{0.5f, 1.0f, 1.5f};
    const mat3 closed = inertia_solid_box(mass, half);

    print_mat3("C.1 box, closed", closed);
    std::printf("       m = %.1f kg, half-extents (%.1f, %.1f, %.1f)\n",
                static_cast<double>(mass), static_cast<double>(half.x),
                static_cast<double>(half.y), static_cast<double>(half.z));

    std::printf("\n  C.2  the same box as a grid of point masses\n\n");
    std::printf("      N    points     Ixx(points)   ratio   1 - 1/N^2\n");

    for (int n : {4, 8, 20, 50})
    {
        const std::vector<point_mass> grid = box_grid(mass, half, n);
        const mat3 summed = inertia_of_points(grid);
        const double ratio = static_cast<double>(summed.c0.x)
                           / static_cast<double>(closed.c0.x);
        const double predicted = 1.0 - 1.0 / (static_cast<double>(n) * n);
        std::printf("    %3d  %8zu   %11.6f   %.6f  %.6f\n",
                    n, grid.size(), static_cast<double>(summed.c0.x),
                    ratio, predicted);
    }

    std::printf("\n       The point sum is LOW, by exactly 1/N^2, because each\n");
    std::printf("       cell's own inertia about its own centre is missing.\n");
    std::printf("       Add it back — the parallel-axis theorem, per cell —\n");
    std::printf("       and the sum is exact at ANY N:\n\n");
    std::printf("      N      Ixx(cells)    rel. error\n");

    for (int n : {4, 8, 20})
    {
        const std::vector<point_mass> grid = box_grid(mass, half, n);
        const vec3 cell_half{half.x / static_cast<float>(n),
                             half.y / static_cast<float>(n),
                             half.z / static_cast<float>(n)};
        const float cell_mass = mass / static_cast<float>(n * n * n);
        const mat3 cell_own = inertia_solid_box(cell_mass, cell_half);

        mat3 total{vec3{}, vec3{}, vec3{}};
        for (const point_mass& p : grid)
        {
            total += shift_inertia(cell_own, cell_mass, p.offset);
        }
        const double rel = std::fabs(static_cast<double>(total.c0.x - closed.c0.x))
                         / static_cast<double>(closed.c0.x);
        std::printf("    %3d   %11.6f   %.4e\n", n,
                    static_cast<double>(total.c0.x), rel);
    }

    // The sphere, where there is no clean closed form for the discretisation
    // error, so the evidence is convergence rather than an identity.
    std::printf("\n  C.3  solid sphere, m = 2 kg, R = 1.5 m\n");
    const mat3 sphere = inertia_solid_sphere(2.0f, 1.5f);
    std::printf("       closed form  (2/5) m R^2 = %.6f\n",
                static_cast<double>(sphere.c0.x));
    std::printf("\n      N    points    I(points)   rel. error\n");
    for (int n : {20, 40, 80})
    {
        const std::vector<point_mass> grid = sphere_grid(2.0f, 1.5f, n);
        const mat3 summed = inertia_of_points(grid);
        const double rel = std::fabs(static_cast<double>(summed.c0.x - sphere.c0.x))
                         / static_cast<double>(sphere.c0.x);
        std::printf("    %3d  %8zu  %10.6f   %.4e\n",
                    n, grid.size(), static_cast<double>(summed.c0.x), rel);
    }

    // The capsule's two limits, which are the only practical check on a formula
    // nobody can do in their head.
    std::printf("\n  C.4  the capsule's two limits\n");
    const mat3 cap_sphere = inertia_capsule(3.0f, 0.8f, 0.0f);
    const mat3 as_sphere = inertia_solid_sphere(3.0f, 0.8f);
    std::printf("       H -> 0   capsule %.8f  sphere %.8f\n",
                static_cast<double>(cap_sphere.c0.x),
                static_cast<double>(as_sphere.c0.x));
    std::printf("                max element difference %.4e\n",
                static_cast<double>(max_diff(cap_sphere, as_sphere)));

    const mat3 cap_rod = inertia_capsule(3.0f, 1e-4f, 2.0f);
    const mat3 as_rod = inertia_thin_rod(3.0f, 2.0f);
    std::printf("       R -> 0   capsule %.8f  rod    %.8f\n",
                static_cast<double>(cap_rod.c0.x),
                static_cast<double>(as_rod.c0.x));
    std::printf("                relative difference    %.4e\n",
                std::fabs(static_cast<double>(cap_rod.c0.x - as_rod.c0.x))
                    / static_cast<double>(as_rod.c0.x));

    // CONTROL: the sampler on ONE point mass, where it must reproduce
    // `inertia_of_point` to the bit. If it cannot do that, nothing above it
    // means anything.
    const point_mass one{vec3{0.3f, -1.1f, 2.0f}, 7.0f};
    const mat3 sampled = inertia_of_points({&one, 1});
    const mat3 direct = inertia_of_point(one.mass, one.offset);
    std::printf("\n  C.5  CONTROL sampler on ONE point: identical  %s\n",
                identical(sampled, direct) ? "yes" : "NO");

    // ...and the other half of the control: every tensor built above must pass
    // the four physical tests, including the triangle inequality.
    const inertia_report r = inspect_inertia(closed);
    std::printf("       box report: asym %.2e  positive %d  triangle %d\n",
                static_cast<double>(r.asymmetry), r.positive ? 1 : 0,
                r.triangle ? 1 : 0);
    std::printf("       a tensor that no shape could have:\n");
    const mat3 impossible = diagonal(vec3{1.0f, 1.0f, 5.0f});
    const inertia_report bad = inspect_inertia(impossible);
    std::printf("         diag (1, 1, 5)  triangle %d  usable %d\n",
                bad.triangle ? 1 : 0, bad.usable ? 1 : 0);
}

// ===========================================================================
// D — the parallel-axis theorem
// ===========================================================================

void section_d()
{
    rule("D  THE PARALLEL-AXIS THEOREM");

    const float mass = 4.0f;
    const vec3 half{0.5f, 1.0f, 1.5f};
    const vec3 offset{2.0f, 0.0f, 0.0f};

    const mat3 centred = inertia_solid_box(mass, half);
    const mat3 shifted = shift_inertia(centred, mass, offset);

    // Brute force: the same box, sampled, but with every sample measured from
    // the OFFSET point rather than the centre. No theorem involved.
    std::vector<point_mass> grid = box_grid(mass, half, 40);
    for (point_mass& p : grid) { p.offset -= offset; }
    const mat3 sampled = inertia_of_points(grid);

    // ...and the same 1/N^2 deficit as C.2 applies to the sampled version, so
    // the comparison has to add the cells' own tensors back or it is comparing
    // a theorem against a known-biased estimate.
    const vec3 cell_half{half.x / 40.0f, half.y / 40.0f, half.z / 40.0f};
    const float cell_mass = mass / 64000.0f;
    const mat3 cell_own = inertia_solid_box(cell_mass, cell_half);
    mat3 exact_sum{vec3{}, vec3{}, vec3{}};
    for (const point_mass& p : grid)
    {
        exact_sum += shift_inertia(cell_own, cell_mass, p.offset);
    }

    print_mat3("D.1 shifted", shifted);
    std::printf("       box at (2, 0, 0), theorem\n");
    print_mat3("    sampled", exact_sum);
    std::printf("       the same box, 64,000 cells, no theorem\n");
    std::printf("\n       max element difference  %.4e\n",
                static_cast<double>(max_diff(shifted, exact_sum)));
    std::printf("       (naive point sum would give %.6f for Izz\n",
                static_cast<double>(sampled.c2.z));
    std::printf("        against %.6f — the 1/N^2 of C.2)\n",
                static_cast<double>(shifted.c2.z));

    // Round trip.
    const mat3 back = unshift_inertia(shifted, mass, offset);
    std::printf("\n  D.2  shift then unshift: max difference %.4e\n",
                static_cast<double>(max_diff(back, centred)));

    // A dumbbell: two spheres on a rod. Built by assembly, checked by sampling.
    const float ball_mass = 2.0f;
    const float ball_r = 0.4f;
    const float bar_mass = 0.5f;
    const float bar_len = 3.0f;

    const inertia_part parts[3] = {
        {ball_mass, vec3{0.0f, 1.5f, 0.0f}, inertia_solid_sphere(ball_mass, ball_r)},
        {ball_mass, vec3{0.0f, -1.5f, 0.0f}, inertia_solid_sphere(ball_mass, ball_r)},
        {bar_mass, vec3{}, inertia_solid_cylinder(bar_mass, 0.05f, bar_len)},
    };
    const inertia_assembly_result dumbbell = inertia_assembly({parts, 3});

    std::printf("\n  D.3  a dumbbell: 2 x 2 kg balls, a 0.5 kg bar\n");
    std::printf("       total mass   %.4f kg\n", static_cast<double>(dumbbell.mass));
    std::printf("       centre       (%.4f, %.4f, %.4f)\n",
                static_cast<double>(dumbbell.centre.x),
                static_cast<double>(dumbbell.centre.y),
                static_cast<double>(dumbbell.centre.z));
    print_mat3("       I", dumbbell.inertia, 7);
    std::printf("       Ixx / Iyy = %.4f  (hard across, easy along)\n",
                static_cast<double>(dumbbell.inertia.c0.x / dumbbell.inertia.c1.y));

    // THE SILENT BUG: add the parts' tensors without shifting them.
    mat3 unshifted{vec3{}, vec3{}, vec3{}};
    for (const inertia_part& p : parts) { unshifted += p.inertia; }
    std::printf("\n  D.4  the same parts added WITHOUT shifting:\n");
    std::printf("       Ixx %.6f against %.6f — %.1fx too small,\n",
                static_cast<double>(unshifted.c0.x),
                static_cast<double>(dumbbell.inertia.c0.x),
                static_cast<double>(dumbbell.inertia.c0.x / unshifted.c0.x));
    std::printf("       and it still passes every test in inspect_inertia:\n");
    const inertia_report wrong = inspect_inertia(unshifted);
    std::printf("       asym %.2e  positive %d  triangle %d  usable %d\n",
                static_cast<double>(wrong.asymmetry), wrong.positive ? 1 : 0,
                wrong.triangle ? 1 : 0, wrong.usable ? 1 : 0);

    // CONTROL: a zero offset must leave the tensor bit-identical. If shifting by
    // nothing changes something, the theorem's implementation is wrong.
    const mat3 nudged = shift_inertia(centred, mass, vec3{});
    std::printf("\n  D.5  CONTROL shift by zero: identical  %s\n",
                identical(nudged, centred) ? "yes" : "NO");

    // ...and an assembly of ONE part must return that part untouched.
    const inertia_part solo[1] = {{mass, vec3{}, centred}};
    const inertia_assembly_result alone = inertia_assembly({solo, 1});
    std::printf("       CONTROL assembly of one part: identical  %s\n",
                identical(alone.inertia, centred) ? "yes" : "NO");
}

// ===========================================================================
// E — R I R^T is a basis change, and the plausible wrong one
// ===========================================================================

void section_e()
{
    rule("E  R I R^T IS A BASIS CHANGE, AND THE PLAUSIBLE WRONG ONE");

    const mat3 body = inertia_solid_box(4.0f, vec3{0.5f, 1.0f, 1.5f});
    const quat turn = quat_from_axis_angle(normalised(vec3{1.0f, 2.0f, 3.0f}),
                                           static_cast<float>(0.7 * k_pi));
    const mat3 r = mat3_from_quat(turn);
    const mat3 world = rotate_inertia(r, body);

    print_mat3("E.1 body axes", body);
    print_mat3("    world axes", world);

    std::printf("\n       trace        %12.6f -> %12.6f\n",
                static_cast<double>(trace(body)), static_cast<double>(trace(world)));
    std::printf("       determinant  %12.6f -> %12.6f\n",
                static_cast<double>(determinant(body)),
                static_cast<double>(determinant(world)));
    std::printf("       asymmetry    %12.4e -> %12.4e\n",
                static_cast<double>(asymmetry(body)),
                static_cast<double>(asymmetry(world)));

    // THE PLAUSIBLE WRONG ONE. `transpose(R) I R` is the OTHER basis change —
    // the inverse one — and it is symmetric, has the same trace, the same
    // determinant and the same principal moments. Every cheap test passes.
    const mat3 backwards = transpose(r) * body * r;
    std::printf("\n  E.2  R^T I R instead of R I R^T:\n");
    std::printf("       trace       %12.6f  (same)\n",
                static_cast<double>(trace(backwards)));
    std::printf("       determinant %12.6f  (same)\n",
                static_cast<double>(determinant(backwards)));
    std::printf("       asymmetry   %12.4e  (still symmetric)\n",
                static_cast<double>(asymmetry(backwards)));
    std::printf("       max element difference from R I R^T  %.6f\n",
                static_cast<double>(max_diff(backwards, world)));

    // The test that DOES tell them apart: put the body in a known orientation
    // and ask what the world tensor does to a world-axis spin. Turn the box 90
    // degrees about z, and its long axis (z, half-extent 1.5) is still z — but
    // its x and y axes have swapped. So the world Ixx must become the body Iyy.
    const quat quarter = quat_from_axis_angle(vec3{0.0f, 0.0f, 1.0f},
                                              static_cast<float>(0.5 * k_pi));
    const mat3 rq = mat3_from_quat(quarter);
    const mat3 right = rotate_inertia(rq, body);
    const mat3 wrong = transpose(rq) * body * rq;

    std::printf("\n  E.3  a 90-degree turn about z. The box's x and y axes\n");
    std::printf("       swap, so world Ixx must equal body Iyy.\n");
    std::printf("       body    Ixx %.6f  Iyy %.6f\n",
                static_cast<double>(body.c0.x), static_cast<double>(body.c1.y));
    std::printf("       R I R^T Ixx %.6f  <- matches body Iyy\n",
                static_cast<double>(right.c0.x));
    std::printf("       R^T I R Ixx %.6f\n",
                static_cast<double>(wrong.c0.x));
    std::printf("       For THIS rotation the two happen to agree, because\n");
    std::printf("       a 90-degree turn is its own inverse up to a sign on\n");
    std::printf("       a diagonal tensor. That is why E.2's rotation is a\n");
    std::printf("       generic one: a test built on a right angle would\n");
    std::printf("       have passed with the transpose in either place.\n");

    // The inverse sandwich identity, which is what lets a body store the
    // inverse tensor once and never invert again.
    const mat3 inv_body = inverse_inertia(body);
    const mat3 a = inverse_inertia(world);                 // invert the world tensor
    const mat3 b2 = world_inverse_inertia(turn, inv_body); // sandwich the inverse
    std::printf("\n  E.4  (R I R^T)^-1  vs  R I^-1 R^T\n");
    std::printf("       max element difference  %.4e\n",
                static_cast<double>(max_diff(a, b2)));

    // CONTROL: the identity rotation must leave the tensor bit-identical.
    const mat3 unchanged = rotate_inertia(mat3::identity(), body);
    std::printf("\n  E.5  CONTROL identity rotation: identical  %s\n",
                identical(unchanged, body) ? "yes" : "NO");

    // ...and a SCALE in the sandwich, which is 8.2 §8's frame question arriving
    // in its rotational form: the result is not the inertia tensor of anything.
    const mat3 scaled = engine::scale(2.0f, 1.0f, 1.0f);
    const mat3 sheared = scaled * body * transpose(scaled);
    std::printf("       a SCALE of (2,1,1) in the sandwich:\n");
    std::printf("         trace %.6f -> %.6f, and the body it\n",
                static_cast<double>(trace(body)), static_cast<double>(trace(sheared)));
    std::printf("         describes is a different body.\n");
}

// ===========================================================================
// F — an orientation is not a vector
// ===========================================================================

void section_f()
{
    rule("F  AN ORIENTATION IS NOT A VECTOR");

    // Two 90-degree turns, in both orders. The "sum of rotation vectors" is the
    // same either way; the rotations are not.
    const quat rx = quat_from_axis_angle(vec3{1.0f, 0.0f, 0.0f},
                                         static_cast<float>(0.5 * k_pi));
    const quat ry = quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f},
                                         static_cast<float>(0.5 * k_pi));

    const vec3 probe{0.0f, 0.0f, 1.0f};
    const vec3 xy = rotate(rx * ry, probe);
    const vec3 yx = rotate(ry * rx, probe);

    std::printf("  F.1  90 deg about x, then y — and the other order\n\n");
    std::printf("       (0,0,1) -> x then y: %6.3f %6.3f %6.3f\n",
                static_cast<double>(xy.x), static_cast<double>(xy.y),
                static_cast<double>(xy.z));
    std::printf("       (0,0,1) -> y then x: %6.3f %6.3f %6.3f\n",
                static_cast<double>(yx.x), static_cast<double>(yx.y),
                static_cast<double>(yx.z));
    std::printf("       angle between the two results: %.3f deg\n",
                static_cast<double>(engine::angle_between(rx * ry, ry * rx))
                    * 180.0 / k_pi);
    std::printf("       The two 'rotation vectors' summed are IDENTICAL\n");
    std::printf("       either way — addition commutes. Rotations do not,\n");
    std::printf("       so no vector can be their running total.\n");

    // The inflation of the linearised update, before renormalisation.
    std::printf("\n  F.2  the linearised update inflates. |q| after one\n");
    std::printf("       step, before renormalising:\n\n");
    std::printf("      omega   omega*h    measured      predicted\n");

    for (float w : {1.0f, 10.0f, 30.0f})
    {
        const quat q = quat::identity();
        const vec3 omega{0.0f, w, 0.0f};
        const quat rate = quat::pure(omega) * q;
        const quat stepped{q.w + rate.w * 0.5f * k_h60,
                           q.v + rate.v * 0.5f * k_h60};
        std::printf("    %6.1f  %7.4f   %.9f   %.9f\n",
                    static_cast<double>(w), static_cast<double>(w * k_h60),
                    static_cast<double>(length(stepped)),
                    static_cast<double>(spin_inflation(w, k_h60)));
    }

    std::printf("\n       Compounded without renormalising, 10 rad/s for\n");
    std::printf("       one second (60 steps) gives |q| = ");
    {
        quat q = quat::identity();
        const vec3 omega{0.0f, 10.0f, 0.0f};
        for (int i = 0; i < 60; ++i)
        {
            const quat rate = quat::pure(omega) * q;
            q = quat{q.w + rate.w * 0.5f * k_h60, q.v + rate.v * 0.5f * k_h60};
        }
        std::printf("%.4f\n", static_cast<double>(length(q)));
    }

    // The angle error that SURVIVES renormalisation.
    std::printf("\n  F.3  ...and what renormalising does NOT fix: the angle.\n\n");
    std::printf("      omega*h    asked      achieved    rel. err   -(wh)^2/12\n");
    for (float w : {1.0f, 10.0f, 30.0f})
    {
        const vec3 omega{0.0f, w, 0.0f};
        const quat after = advance_orientation(quat::identity(), omega, k_h60,
                                               spin_rule::linearised);
        const double achieved =
            static_cast<double>(engine::angle_between(quat::identity(), after));
        const double asked = static_cast<double>(w) * static_cast<double>(k_h60);
        const double wh = asked;
        std::printf("    %7.4f  %9.6f  %9.6f  %10.3e  %10.3e\n",
                    wh, asked, achieved, achieved / asked - 1.0,
                    -wh * wh / 12.0);
    }
    std::printf("\n       predicted by spin_angle_error(10, 1/60) = %.4e\n",
                static_cast<double>(spin_angle_error(10.0f, k_h60)));

    // The exponential map, which has neither problem.
    std::printf("\n  F.4  the exponential map, 3,600 steps at 10 rad/s\n");
    for (spin_rule rule_kind : {spin_rule::linearised, spin_rule::exponential})
    {
        quat q = quat::identity();
        const vec3 omega{0.0f, 10.0f, 0.0f};
        for (int i = 0; i < 3600; ++i) { q = advance_orientation(q, omega, k_h60, rule_kind); }

        const double exact_angle = 10.0 * 60.0;                 // radians
        const double turns = exact_angle / (2.0 * k_pi);
        const double wrapped = (turns - std::floor(turns)) * 2.0 * k_pi;
        const quat exact = quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f},
                                                static_cast<float>(wrapped));
        std::printf("       %-12s |q| %.7f   error %9.4f deg\n",
                    name_of(rule_kind), static_cast<double>(length(q)),
                    static_cast<double>(engine::angle_between(q, exact)) * 180.0 / k_pi);
    }

    // The left/right multiply mistake: a body-space omega used as a world-space
    // one. Identical for a spin about a fixed axis; divergent the moment the
    // body is already turned.
    std::printf("\n  F.5  omega on the wrong side (body vs world)\n");
    {
        const quat start = quat_from_axis_angle(normalised(vec3{1.0f, 1.0f, 0.0f}),
                                                static_cast<float>(0.4 * k_pi));
        const vec3 omega{0.0f, 5.0f, 0.0f};

        quat world_side = start;
        quat body_side = start;
        for (int i = 0; i < 60; ++i)
        {
            world_side = advance_orientation(world_side, omega, k_h60,
                                             spin_rule::exponential);
            // The mistake: composing the delta on the right instead.
            const quat delta = quat_from_axis_angle(normalised(omega),
                                                    length(omega) * k_h60);
            body_side = normalised(body_side * delta);
        }
        std::printf("       after 1 s, the two differ by %.3f deg\n",
                    static_cast<double>(engine::angle_between(world_side, body_side))
                        * 180.0 / k_pi);
        std::printf("       Both are unit quaternions, both are smooth, and\n");
        std::printf("       neither reports anything.\n");
    }

    // CONTROL: zero angular velocity must leave the orientation bit-identical
    // under both rules, however many steps run.
    {
        const quat start = quat_from_axis_angle(normalised(vec3{1.0f, -2.0f, 0.5f}),
                                                1.234f);
        quat a = start;
        quat b = start;
        for (int i = 0; i < 10000; ++i)
        {
            a = advance_orientation(a, vec3{}, k_h60, spin_rule::linearised);
            b = advance_orientation(b, vec3{}, k_h60, spin_rule::exponential);
        }
        std::printf("\n  F.6  CONTROL omega = 0, 10,000 steps: bit-identical\n");
        std::printf("       linearised %s   exponential %s\n",
                    (a == start) ? "yes" : "NO", (b == start) ? "yes" : "NO");
    }
}

// ===========================================================================
// G — L is conserved and omega is not
// ===========================================================================

/// Run a torque-free body and report what moved.
struct tumble
{
    double l_first = 0.0;
    double l_last = 0.0;
    double l_drift = 0.0;     ///< max |L(t)| / |L(0)| - 1
    double omega_min = 0.0;
    double omega_max = 0.0;
    double energy_drift = 0.0;
};

tumble run_tumble(rigid_body body, float h, int steps,
                  engine::phys::gyroscopic_mode mode, spin_rule rule_kind)
{
    body.gyroscopic = mode;

    body_world w;
    w.set_gravity(vec3{});
    w.set_spin_rule(rule_kind);
    const body_id id = w.add(body);

    const rigid_body* b = w.get(id);
    tumble out;
    out.l_first = static_cast<double>(length(angular_momentum(*b)));
    const double e0 = static_cast<double>(kinetic_energy(*b));
    out.omega_min = static_cast<double>(length(b->angular_velocity));
    out.omega_max = out.omega_min;

    for (int i = 0; i < steps; ++i)
    {
        w.step(h);
        b = w.get(id);

        const double l = static_cast<double>(length(angular_momentum(*b)));
        out.l_drift = std::max(out.l_drift, std::fabs(l / out.l_first - 1.0));

        const double s = static_cast<double>(length(b->angular_velocity));
        out.omega_min = std::min(out.omega_min, s);
        out.omega_max = std::max(out.omega_max, s);

        const double e = static_cast<double>(kinetic_energy(*b));
        out.energy_drift = std::max(out.energy_drift, std::fabs(e / e0 - 1.0));
    }

    out.l_last = static_cast<double>(length(angular_momentum(*w.get(id))));
    return out;
}

void section_g()
{
    rule("G  L IS CONSERVED AND OMEGA IS NOT");

    using engine::phys::gyroscopic_mode;

    // A 1 kg box, 1 x 2 x 3 metres. Its three principal moments are all
    // different, which is what makes it interesting.
    const vec3 half{0.5f, 1.0f, 1.5f};
    rigid_body box = make_box(vec3{}, 1.0f, half);
    box.angular_velocity = vec3{0.6f, 4.0f, 0.3f};

    const mat3 i_body = inertia_of(box);
    const double ia = static_cast<double>(i_body.c0.x);
    const double ib = static_cast<double>(i_body.c1.y);
    const double ic = static_cast<double>(i_body.c2.z);

    std::printf("  a 1 kg box, half-extents (%.1f, %.1f, %.1f)\n",
                static_cast<double>(half.x), static_cast<double>(half.y),
                static_cast<double>(half.z));
    std::printf("  principal moments  Ix %.6f  Iy %.6f  Iz %.6f\n", ia, ib, ic);
    std::printf("  omega(0) = (0.6, 4.0, 0.3) rad/s, no torque\n");

    const tumble mom = run_tumble(box, k_h60, 3600, gyroscopic_mode::momentum,
                                  spin_rule::exponential);

    std::printf("\n  G.1  60 seconds at 60 Hz, momentum formulation\n");
    std::printf("       |L|      %.6f -> %.6f\n", mom.l_first, mom.l_last);
    std::printf("       worst drift in |L|   %.4e\n", mom.l_drift);
    std::printf("       worst drift in E     %.4e\n", mom.energy_drift);

    // ---- G.2: the bound, DERIVED, then measured against ---------------------
    //
    // A torque-free rigid body has exactly two conserved quantities: |L|^2 and
    // twice the kinetic energy, 2T = sum(L_i^2 / I_i) in body axes. Write
    // u_i = L_i^2 and both are LINEAR in u — so the reachable set is the
    // segment where two planes meet the positive octant, and |omega|^2, which
    // is sum(u_i / I_i^2), is linear on it too.
    //
    // A linear function on a segment takes its extremes at the ENDS, and the
    // ends are where one u_i reaches zero. Three candidates, of which the
    // infeasible ones announce themselves with a negative u.
    const double lx = 0.6 * ia;
    const double ly = 4.0 * ib;
    const double lz = 0.3 * ic;
    const double l2 = lx * lx + ly * ly + lz * lz;
    const double two_t = lx * lx / ia + ly * ly / ib + lz * lz / ic;

    std::printf("\n  G.2  the reachable range of |omega|, DERIVED from the\n");
    std::printf("       two conserved quantities and then measured\n\n");
    std::printf("       |L|^2 = %.6f    2T = %.6f\n\n", l2, two_t);

    double lo = HUGE_VAL;
    double hi = 0.0;
    const double moment[3] = {ia, ib, ic};
    const char* zeroed[3] = {"Lx = 0", "Ly = 0", "Lz = 0"};

    for (int gone = 0; gone < 3; ++gone)
    {
        // The two survivors, p and q.
        const int p = (gone + 1) % 3;
        const int q = (gone + 2) % 3;

        //   u_p + u_q = |L|^2
        //   u_p/I_p + u_q/I_q = 2T
        const double denom = 1.0 / moment[q] - 1.0 / moment[p];
        if (std::fabs(denom) < 1e-12) { continue; }

        const double uq = (two_t - l2 / moment[p]) / denom;
        const double up = l2 - uq;
        if (up < 0.0 || uq < 0.0)
        {
            std::printf("       %s  infeasible\n", zeroed[gone]);
            continue;
        }

        const double w2 = up / (moment[p] * moment[p]) + uq / (moment[q] * moment[q]);
        const double w = std::sqrt(w2);
        lo = std::min(lo, w);
        hi = std::max(hi, w);
        std::printf("       %s  gives |omega| = %.4f\n", zeroed[gone], w);
    }

    std::printf("\n       predicted  %.4f .. %.4f\n", lo, hi);
    std::printf("       measured   %.4f .. %.4f\n", mom.omega_min, mom.omega_max);
    std::printf("       a swing of %.2f%% with NOTHING acting on it.\n",
                100.0 * (mom.omega_max - mom.omega_min) / mom.omega_min);
    std::printf("       L is the conserved quantity; omega is not.\n");

    // What dropping the term entirely does.
    const tumble off = run_tumble(box, k_h60, 3600, gyroscopic_mode::off,
                                  spin_rule::exponential);
    std::printf("\n  G.3  the same run with the term OFF\n");
    std::printf("       |omega|  min %.6f   max %.6f\n",
                off.omega_min, off.omega_max);
    std::printf("       |L| drift %.4e — omega is now CONSTANT, which\n", off.l_drift);
    std::printf("       looks stable and is the wrong physics: the body\n");
    std::printf("       spins about a fixed world axis and never tumbles.\n");

    // CONTROL: a sphere. Its tensor is isotropic, so `omega x (I omega)` is
    // `omega x (k omega)` = 0 identically and `I_world` does not change at all
    // as it turns. Every mode must agree, and the tumble must not happen.
    rigid_body ball = make_sphere(vec3{}, 1.0f, 1.0f);
    ball.angular_velocity = vec3{0.6f, 4.0f, 0.3f};
    const tumble sphere_on = run_tumble(ball, k_h60, 3600, gyroscopic_mode::momentum,
                                        spin_rule::exponential);
    const tumble sphere_off = run_tumble(ball, k_h60, 3600, gyroscopic_mode::off,
                                         spin_rule::exponential);

    std::printf("\n  G.4  CONTROL the same experiment on a SPHERE\n");
    std::printf("       momentum  |omega| swing %.4e\n",
                (sphere_on.omega_max - sphere_on.omega_min) / sphere_on.omega_min);
    std::printf("       off       |omega| swing %.4e\n",
                (sphere_off.omega_max - sphere_off.omega_min) / sphere_off.omega_min);
    std::printf("       |L| drift %.4e and %.4e — the residue is a\n",
                sphere_on.l_drift, sphere_off.l_drift);
    std::printf("       body-frame round trip through two rotations, not\n");
    std::printf("       physics. An isotropic tensor has nothing to say.\n");

    // ---- G.5: the same physics, three ways --------------------------------
    std::printf("\n  G.5  the SAME physics, three ways. Worst drift in |L|\n");
    std::printf("       over 60 torque-free seconds:\n\n");
    std::printf("      rate      explicit      implicit     momentum\n");
    for (float rate : {30.0f, 60.0f, 240.0f, 960.0f})
    {
        const int steps = static_cast<int>(60.0f * rate);
        const tumble ex = run_tumble(box, 1.0f / rate, steps,
                                     gyroscopic_mode::explicit_term,
                                     spin_rule::exponential);
        const tumble im = run_tumble(box, 1.0f / rate, steps,
                                     gyroscopic_mode::implicit_term,
                                     spin_rule::exponential);
        const tumble mo = run_tumble(box, 1.0f / rate, steps,
                                     gyroscopic_mode::momentum,
                                     spin_rule::exponential);
        std::printf("    %5.0f Hz   %.4e    %.4e   %.4e\n",
                    static_cast<double>(rate), ex.l_drift, im.l_drift, mo.l_drift);
    }

    // ---- G.6: the midpoint, and what it is worth --------------------------
    //
    // The momentum formulation conserves `L` by construction whatever it does
    // with the orientation — so `L` alone cannot tell you whether the body is
    // doing the RIGHT thing with it. The second conserved quantity can: a
    // torque-free body's kinetic energy is constant too, and nothing in the
    // formulation protects it.
    //
    // Both arms below integrate `L` identically. They differ only in which
    // angular velocity they hand to `advance_orientation`: the one at the start
    // of the step, or the one at a half step in.
    {
        const mat3 inv_i = box.inv_inertia_local;
        const mat3 i_body = box.inertia_local;

        auto omega_at = [&](quat q, vec3 l) {
            const mat3 r = mat3_from_quat(q);
            return r * (inv_i * (transpose(r) * l));
        };

        std::printf("\n  G.6  the same formulation, with and without a half\n");
        std::printf("       step. Both conserve L; only one is right.\n\n");
        std::printf("      rate    pairing    |omega| range       worst dE\n");

        for (float rate : {60.0f, 240.0f, 960.0f})
        {
            for (int midpoint = 0; midpoint < 2; ++midpoint)
            {
                quat q = box.orientation;
                const mat3 r0 = mat3_from_quat(q);
                const vec3 l = r0 * (i_body * (transpose(r0) * box.angular_velocity));

                vec3 w = box.angular_velocity;
                const double e0 = 0.5 * static_cast<double>(dot(w, l));
                double lo = static_cast<double>(length(w));
                double hi = lo;
                double worst_e = 0.0;

                const float h = 1.0f / rate;
                for (int i = 0; i < static_cast<int>(20.0f * rate); ++i)
                {
                    if (midpoint != 0)
                    {
                        const quat half = advance_orientation(q, omega_at(q, l), 0.5f * h,
                                                              spin_rule::exponential);
                        q = advance_orientation(q, omega_at(half, l), h,
                                                spin_rule::exponential);
                    }
                    else
                    {
                        q = advance_orientation(q, omega_at(q, l), h,
                                                spin_rule::exponential);
                    }

                    w = omega_at(q, l);
                    const double sp = static_cast<double>(length(w));
                    lo = std::min(lo, sp);
                    hi = std::max(hi, sp);
                    worst_e = std::max(worst_e,
                                       std::fabs(0.5 * static_cast<double>(dot(w, l)) / e0 - 1.0));
                }

                std::printf("    %5.0f Hz  %-9s  %6.4f .. %6.4f   %.3e\n",
                            static_cast<double>(rate),
                            (midpoint != 0) ? "midpoint" : "start", lo, hi, worst_e);
            }
        }

        std::printf("\n       The bound from G.2 is %.4f .. %.4f. At 60 Hz\n",
                    lo, hi);
        std::printf("       the start-of-step pairing puts the body outside\n");
        std::printf("       it by nearly a factor of two while conserving L\n");
        std::printf("       beautifully — the momentum was right and the body\n");
        std::printf("       was doing the wrong thing with it. One extra\n");
        std::printf("       rotation matrix and one extra orientation advance\n");
        std::printf("       fix it, and `step` takes them.\n");
    }

    std::printf("\n       The first two columns are 8.1's two determinants:\n");
    std::printf("       explicit GROWS, implicit is its exact reciprocal\n");
    std::printf("       and DAMPS. Both improve as the step shrinks.\n");
    std::printf("\n       *** THE THIRD COLUMN GETS WORSE. *** It is the\n");
    std::printf("       only table in this course where a smaller step is\n");
    std::printf("       a worse answer, and the reason is that its error\n");
    std::printf("       is not truncation at all. Nothing in the momentum\n");
    std::printf("       formulation approximates L; the engine simply does\n");
    std::printf("       not STORE it, and rebuilds it from omega and the\n");
    std::printf("       orientation every step. That round trip loses a\n");
    std::printf("       little each time, so the error grows with the\n");
    std::printf("       NUMBER of steps rather than their size.\n");
}

// ===========================================================================
// H — the tennis racket theorem
// ===========================================================================

void section_h()
{
    rule("H  THE TENNIS RACKET THEOREM");

    using engine::phys::gyroscopic_mode;

    const vec3 half{0.5f, 1.0f, 1.5f};
    rigid_body box = make_box(vec3{}, 1.0f, half);
    const mat3 i_body = inertia_of(box);

    const double ix = static_cast<double>(i_body.c0.x);   // largest
    const double iy = static_cast<double>(i_body.c1.y);   // intermediate
    const double iz = static_cast<double>(i_body.c2.z);   // smallest

    std::printf("  Ix %.6f > Iy %.6f > Iz %.6f\n", ix, iy, iz);
    std::printf("  so y is the INTERMEDIATE axis.\n");

    const double spin = 10.0;
    const double growth = spin * std::sqrt((ix - iy) * (iy - iz) / (iz * ix));
    const double osc_x = spin * std::sqrt((ix - iy) * (ix - iz) / (iy * iz));
    const double osc_z = spin * std::sqrt((ix - iz) * (iy - iz) / (ix * iy));

    // *** EVERYTHING BELOW MEASURES omega IN BODY AXES, AND THE FIRST DRAFT
    // *** DID NOT.
    //
    // Euler's equations are body-frame equations. The engine stores `omega` in
    // world space, and for a body spinning about its own y axis the two
    // PERTURBATION components are carried around that axis at the spin rate —
    // so a world-space `omega.x` oscillates at 10 rad/s while its envelope
    // grows, and sampling it at a threshold crossing samples the oscillation.
    // The first version of H.3 did exactly that and fitted a growth rate of
    // 3.8498 against a predicted 4.8038, a 20% miss that looked like a failed
    // prediction and was a failed frame.
    auto body_omega = [](const rigid_body& b) {
        return transpose(mat3_from_quat(b.orientation)) * b.angular_velocity;
    };

    std::printf("\n  H.1  spin at 10 rad/s about each axis in turn, with a\n");
    std::printf("       1e-3 rad/s nudge on the other two. 20 seconds,\n");
    std::printf("       omega measured in BODY axes.\n\n");
    std::printf("      axis   nudge        grew to      verdict\n");

    const char* names[3] = {"x", "y", "z"};
    for (int axis = 0; axis < 3; ++axis)
    {
        rigid_body b = box;
        b.gyroscopic = gyroscopic_mode::implicit_term;
        vec3 w{1e-3f, 1e-3f, 1e-3f};
        (&w.x)[axis] = static_cast<float>(spin);
        b.angular_velocity = w;

        body_world world;
        world.set_gravity(vec3{});
        world.set_spin_rule(spin_rule::exponential);
        const body_id id = world.add(b);

        float worst_off = 0.0f;
        for (int i = 0; i < 20 * 240; ++i)
        {
            world.step(1.0f / 240.0f);
            const vec3 wv = body_omega(*world.get(id));
            float off = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                if (k != axis) { off += (&wv.x)[k] * (&wv.x)[k]; }
            }
            worst_off = std::max(worst_off, std::sqrt(off));
        }
        std::printf("      %-5s  %.1e     %9.4f     %s\n",
                    names[axis], 1e-3, static_cast<double>(worst_off),
                    (worst_off > 1.0f) ? "UNSTABLE" : "stable");
    }

    std::printf("\n  H.2  the predicted rates, from Euler's equations:\n");
    std::printf("       about x (largest) : oscillates at %.4f rad/s\n", osc_x);
    std::printf("       about y (middle)  : GROWS at e^(%.4f t)\n", growth);
    std::printf("       about z (smallest): oscillates at %.4f rad/s\n", osc_z);

    // Fit the growth over the FIRST decade, where the motion is still linear in
    // the perturbation. Past that it saturates: the body cannot spin faster
    // than |L| allows, so it flips instead and the perturbation comes back down.
    std::printf("\n  H.3  fitted over the first decade of growth, at four\n");
    std::printf("       step rates — the prediction is a continuous-time\n");
    std::printf("       one, so the columns should converge toward it\n\n");
    std::printf("      rate      measured   predicted    ratio\n");

    for (float rate : {120.0f, 480.0f, 1920.0f, 7680.0f})
    {
        rigid_body b = box;
        b.gyroscopic = gyroscopic_mode::implicit_term;
        b.angular_velocity = vec3{1e-4f, static_cast<float>(spin), 0.0f};

        body_world world;
        world.set_gravity(vec3{});
        world.set_spin_rule(spin_rule::exponential);
        const body_id id = world.add(b);

        const float h = 1.0f / rate;
        double t_lo = 0.0;
        double t_hi = 0.0;
        double x_lo = 0.0;
        double x_hi = 0.0;

        const int steps = static_cast<int>(8.0f * rate);
        for (int i = 1; i <= steps; ++i)
        {
            world.step(h);
            const double t = static_cast<double>(i) * static_cast<double>(h);
            const vec3 wv = body_omega(*world.get(id));
            const double x = std::sqrt(static_cast<double>(wv.x * wv.x + wv.z * wv.z));
            if (x_lo == 0.0 && x >= 1e-3) { x_lo = x; t_lo = t; }
            if (x_lo != 0.0 && x_hi == 0.0 && x >= 1e-2) { x_hi = x; t_hi = t; break; }
        }

        const double measured = std::log(x_hi / x_lo) / (t_hi - t_lo);
        std::printf("    %6.0f Hz  %9.4f  %9.4f  %8.5f\n",
                    static_cast<double>(rate), measured, growth, measured / growth);
    }

    // The flip itself: the sign of the body-frame omega_y reverses, over and
    // over, with nothing acting on the body.
    {
        rigid_body b = box;
        b.gyroscopic = gyroscopic_mode::implicit_term;
        b.angular_velocity = vec3{1e-4f, static_cast<float>(spin), 0.0f};

        body_world world;
        world.set_gravity(vec3{});
        world.set_spin_rule(spin_rule::exponential);
        const body_id id = world.add(b);

        const float h = 1.0f / 1920.0f;
        float previous = body_omega(*world.get(id)).y;
        std::vector<double> flips;
        for (int i = 1; i <= static_cast<int>(20 * 1920) && flips.size() < 4; ++i)
        {
            world.step(h);
            const float now = body_omega(*world.get(id)).y;
            if ((previous > 0.0f) != (now > 0.0f))
            {
                flips.push_back(static_cast<double>(i) * static_cast<double>(h));
            }
            previous = now;
        }

        std::printf("\n  H.4  omega_y reverses sign at t =");
        for (double t : flips) { std::printf(" %.3f", t); }
        std::printf(" s\n");
        if (flips.size() >= 2)
        {
            std::printf("       interval %.4f s, and it repeats forever with\n",
                        flips[1] - flips[0]);
            std::printf("       no torque of any kind. This is the Dzhanibekov\n");
            std::printf("       effect, and it is not a bug.\n");
        }
    }

    // CONTROL: turn the term off and nothing can couple the three components at
    // all, so the perturbation cannot grow — the flip is a consequence of the
    // one term most engines drop.
    {
        rigid_body b = box;
        b.gyroscopic = gyroscopic_mode::off;
        b.angular_velocity = vec3{1e-4f, static_cast<float>(spin), 0.0f};

        body_world world;
        world.set_gravity(vec3{});
        world.set_spin_rule(spin_rule::exponential);
        const body_id id = world.add(b);

        float worst = 0.0f;
        for (int i = 0; i < 40 * 240; ++i)
        {
            world.step(1.0f / 240.0f);
            const vec3 wv = body_omega(*world.get(id));
            worst = std::max(worst, std::sqrt(wv.x * wv.x + wv.z * wv.z));
        }
        std::printf("\n  H.5  CONTROL term OFF, 40 s: worst perpendicular\n");
        std::printf("       |omega| = %.4e — it started at 1.0000e-04\n",
                    static_cast<double>(worst));
        std::printf("       and never moved. No flip, ever.\n");
    }
}

// ===========================================================================
// I — the budget
// ===========================================================================

void section_i()
{
    rule("I  THE BUDGET");

    using engine::phys::gyroscopic_mode;

    // 5.6's `bench_compare`, for the reason 8.2 §11 spelled out: four loops run
    // one after another report the machine settling rather than the code, and
    // whichever arm runs first always loses. This runs them one rep each,
    // alternately, and the RATIO of medians is what repeats.
    constexpr std::size_t k_bodies = 4096;
    constexpr int k_reps = 600;

    std::printf("  sizeof(rigid_body) = %zu bytes  (%zu cache lines)\n",
                sizeof(rigid_body), (sizeof(rigid_body) + 63) / 64);
    std::printf("  8.2 measured 60; the angular half added %zu — an\n",
                sizeof(rigid_body) - 60);
    std::printf("  orientation, an angular velocity, a torque, and TWO\n");
    std::printf("  nine-float tensors, which I.2 is the reason for.\n\n");

    // *** THE FIRST VERSION OF THIS SECTION MEASURED A memcpy. ***
    //
    // Each arm was written as `body_world w = seed; ... w.step(h)`, so that both
    // arms started from identical state — which is a reasonable instinct and put
    // a 557 KB copy of the pool INSIDE the timed region. Every arm reported
    // 36-58 ns/body against 8.2's 1-2, the ratios were all near 1, and the
    // conclusion would have been "the angular half is free".
    //
    // So each arm gets its own world, built once, and the timed body is one
    // `step`. The arms then drift apart in state over 600 reps, which is
    // harmless here: there is no gravity and no torque, so every body simply
    // spins, and the WORK is identical whatever the state.
    auto build = [](gyroscopic_mode mode, spin_rule rule_kind) {
        body_world w;
        w.set_gravity(vec3{});
        w.set_spin_rule(rule_kind);

        rng gen(0x83b1u);
        for (std::size_t i = 0; i < k_bodies; ++i)
        {
            rigid_body b = make_box(gen.direction() * 10.0f,
                                    1.0f + 0.01f * static_cast<float>(i % 7),
                                    vec3{0.5f, 1.0f, 1.5f});
            b.angular_velocity = gen.direction() * 4.0f;
            b.orientation = quat_from_axis_angle(gen.direction(), 0.7f);
            b.gyroscopic = mode;
            w.add(b);
        }
        return w;
    };

    body_world lin = build(gyroscopic_mode::off, spin_rule::linearised);
    body_world expo = build(gyroscopic_mode::off, spin_rule::exponential);

    const engine::bench_ab spin_cost = engine::bench_compare(
        k_bodies, k_reps,
        [&lin]() { return static_cast<double>(lin.step(k_h60).max_spin); },
        [&expo]() { return static_cast<double>(expo.step(k_h60).max_spin); });

    std::printf("  I.1  the orientation rule, %zu bodies\n", k_bodies);
    std::printf("       linearised   %6.3f ns/body  (min %6.3f)\n",
                spin_cost.a.median_ns, spin_cost.a.min_ns);
    std::printf("       exponential  %6.3f ns/body  (min %6.3f)\n",
                spin_cost.b.median_ns, spin_cost.b.min_ns);
    std::printf("       ratio b/a    %.3f\n", spin_cost.ratio());

    body_world plain = build(gyroscopic_mode::off, spin_rule::linearised);
    body_world implicit = build(gyroscopic_mode::implicit_term, spin_rule::linearised);
    body_world explicit_arm = build(gyroscopic_mode::explicit_term, spin_rule::linearised);

    const engine::bench_ab gyro_cost = engine::bench_compare(
        k_bodies, k_reps,
        [&plain]() { return static_cast<double>(plain.step(k_h60).max_spin); },
        [&implicit]() { return static_cast<double>(implicit.step(k_h60).max_spin); });

    std::printf("\n  I.2  the gyroscopic term, implicit — two skew\n");
    std::printf("       products, a Jacobian and a 3x3 inverse\n");
    std::printf("       off  %6.3f ns/body   on  %6.3f ns/body\n",
                gyro_cost.a.median_ns, gyro_cost.b.median_ns);
    std::printf("       ratio on/off %.3f\n", gyro_cost.ratio());

    const engine::bench_ab explicit_cost = engine::bench_compare(
        k_bodies, k_reps,
        [&implicit]() { return static_cast<double>(implicit.step(k_h60).max_spin); },
        [&explicit_arm]() { return static_cast<double>(explicit_arm.step(k_h60).max_spin); });

    std::printf("       explicit instead: %6.3f ns/body, ratio %.3f —\n",
                explicit_cost.b.median_ns, explicit_cost.ratio());
    std::printf("       the cheap one is the one that diverges (G.5)\n");

    body_world momentum_arm = build(gyroscopic_mode::momentum, spin_rule::linearised);
    const engine::bench_ab momentum_cost = engine::bench_compare(
        k_bodies, k_reps,
        [&implicit]() { return static_cast<double>(implicit.step(k_h60).max_spin); },
        [&momentum_arm]() { return static_cast<double>(momentum_arm.step(k_h60).max_spin); });

    std::printf("\n       ...and the MOMENTUM formulation, which carries two\n");
    std::printf("       extra rotation matrices and a half step:\n");
    std::printf("       implicit %6.3f   momentum %6.3f   ratio %.3f\n",
                momentum_cost.a.median_ns, momentum_cost.b.median_ns,
                momentum_cost.ratio());
    std::printf("       The accurate one is also the one that costs more\n");
    std::printf("       here — 8.2's gravity result does not repeat. What\n");
    std::printf("       it buys is in G.5, three orders of magnitude.\n");

    // ---- the hoist, measured inside one run -------------------------------
    //
    // The step as shipped builds `mat3_from_quat(q)` ONCE and uses it for the
    // inverse sandwich and for the angular-momentum report. The obvious version
    // calls `world_inv_inertia(b)` and `angular_momentum(b)`, each of which
    // builds its own. Both arms produce the same two quantities.
    {
        std::vector<rigid_body> bodies;
        bodies.reserve(k_bodies);
        rng gen(0x83d3u);
        for (std::size_t i = 0; i < k_bodies; ++i)
        {
            rigid_body b = make_box(vec3{}, 1.0f, vec3{0.5f, 1.0f, 1.5f});
            b.orientation = quat_from_axis_angle(gen.direction(), 0.9f);
            b.angular_velocity = gen.direction() * 4.0f;
            bodies.push_back(b);
        }

        const engine::bench_ab hoist = engine::bench_compare(
            k_bodies, k_reps,
            [&bodies]() {   // twice
                double sum = 0.0;
                for (const rigid_body& b : bodies)
                {
                    sum += static_cast<double>(trace(world_inv_inertia(b)));
                    sum += static_cast<double>(length(angular_momentum(b)));
                }
                return sum;
            },
            [&bodies]() {   // once
                double sum = 0.0;
                for (const rigid_body& b : bodies)
                {
                    const mat3 r = mat3_from_quat(b.orientation);
                    const mat3 rt = transpose(r);
                    sum += static_cast<double>(trace(r * b.inv_inertia_local * rt));
                    sum += static_cast<double>(
                        length(r * (b.inertia_local * (rt * b.angular_velocity))));
                }
                return sum;
            });

        std::printf("\n  I.3  mat3_from_quat, built twice or hoisted\n");
        std::printf("       twice    %6.3f ns/body\n", hoist.a.median_ns);
        std::printf("       hoisted  %6.3f ns/body\n", hoist.b.median_ns);
        std::printf("       ratio hoisted/twice %.3f — and this is why\n",
                    hoist.ratio());
        std::printf("       the step spells the sandwich out instead of\n");
        std::printf("       calling world_inverse_inertia\n");
    }

    // The basis change on its own, out of the step: two matrix products against
    // a cached tensor.
    {
        std::vector<rigid_body> bodies;
        bodies.reserve(k_bodies);
        rng gen(0x83c2u);
        for (std::size_t i = 0; i < k_bodies; ++i)
        {
            rigid_body b = make_box(vec3{}, 1.0f, vec3{0.5f, 1.0f, 1.5f});
            b.orientation = quat_from_axis_angle(gen.direction(), 0.9f);
            bodies.push_back(b);
        }

        std::vector<mat3> cached(k_bodies);
        for (std::size_t i = 0; i < k_bodies; ++i)
        {
            cached[i] = world_inv_inertia(bodies[i]);
        }

        const engine::bench_ab sandwich = engine::bench_compare(
            k_bodies, k_reps,
            [&bodies]() {
                double sum = 0.0;
                for (const rigid_body& b : bodies)
                {
                    sum += static_cast<double>(trace(world_inv_inertia(b)));
                }
                return sum;
            },
            [&cached]() {
                double sum = 0.0;
                for (const mat3& m : cached) { sum += static_cast<double>(trace(m)); }
                return sum;
            });

        std::printf("\n  I.4  R I^-1 R^T rebuilt   %6.3f ns/body\n",
                    sandwich.a.median_ns);
        std::printf("       read from a cache    %6.3f ns/body\n",
                    sandwich.b.median_ns);
        std::printf("       ratio cached/rebuilt %.3f — which is what\n",
                    sandwich.ratio());
        std::printf("       8.10's solver will buy, once per body per\n");
        std::printf("       frame instead of once per contact iteration\n");
    }

    // CONTROL: two identical arms. A ratio that is not close to 1 here means
    // the harness is measuring the machine and every number above is noise.
    body_world same_a = build(gyroscopic_mode::off, spin_rule::linearised);
    body_world same_b = build(gyroscopic_mode::off, spin_rule::linearised);
    const engine::bench_ab same = engine::bench_compare(
        k_bodies, k_reps,
        [&same_a]() { return static_cast<double>(same_a.step(k_h60).max_spin); },
        [&same_b]() { return static_cast<double>(same_b.step(k_h60).max_spin); });

    std::printf("\n  I.5  CONTROL two identical arms: ratio %.3f\n", same.ratio());
    std::printf("       spread a %.2f  b %.2f  (scheduler outliers;\n",
                same.a.spread(), same.b.spread());
    std::printf("       the medians are what repeat)\n");
}

}   // namespace

int main()
{
    std::printf("verify_83 — Lesson 8.3, angular dynamics\n");

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
