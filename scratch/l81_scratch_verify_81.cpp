// scratch/verify_81.cpp — every number Lesson 8.1 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_81.sh
//
// Nine sections, in the lesson's order:
//
//   A  order of accuracy, and why it CANNOT see the bug
//   B  the determinant, measured as an area in phase space
//   C  a spring, run long: three rules, three fates
//   D  smaller h does not save explicit Euler
//   E  gravity, where the choice very nearly does not matter
//   F  the stability limit, found by bisection
//   G  `v *= 0.99f` and the three games it makes
//   H  drag: the step that reverses, and the one that diverges
//   I  the budget, and the free lunch
//
// EVERY SECTION CARRIES A CONTROL, for the reason 7.1 states and every lesson
// since repeats: a check whose degenerate case is a pass is not a check. 7.6
// sharpened it into a question — ask what the control would say if the thing
// were COMPLETELY BROKEN — and 7.8 added the other half after its dropout
// counter spent a whole lesson reporting a healthy run as a failure: ask what it
// would say if the thing were completely FINE.
//
// Two controls here are written directly against that second question. A.4 runs
// the order measurement on a system with NO restoring force, where every rule is
// exact and the reported order must therefore be meaningless rather than good.
// B.4 evaluates the area factor at h = 0, where all three rules are the identity
// and the determinant must be exactly 1 for reasons that have nothing to do with
// whether any of them is symplectic.
//
// PRECISION. The engine integrates in `float`, so this harness does too — there
// is no point measuring a `double` program the student will never run. Where a
// measurement approaches float's own floor the section says so and prints the
// floor beside the number, because a result at the noise floor is a fact about
// the instrument. Section A ends at the floor on purpose, and finding it is part
// of what A measures.
//
// OUTPUT WIDTH IS A CONSTRAINT, not a preference. The lesson quotes this
// program's transcript in <pre> blocks that scroll and never wrap, and the fold
// is at about 66 characters. Nothing below exceeds 66.

#include <engine/phys/integrate.hpp>
#include <engine/math/vec3.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <numbers>
#include <vector>

using engine::vec3;
using engine::phys::apply_drag;
using engine::phys::area_factor;
using engine::phys::damping_factor;
using engine::phys::integrate;
using engine::phys::integrator;
using engine::phys::max_stable_step;
using engine::phys::motion;
using engine::phys::name_of;
using engine::phys::natural_frequency;
using engine::phys::shadow_energy;
using engine::phys::spring_energy;

namespace
{

constexpr float k_pi = std::numbers::pi_v<float>;

/// A 1 Hz spring. Slow, and chosen for exactly that reason: it is the gentlest
/// restoring force anything in a game would ever have (a real contact is
/// hundreds of Hz), so every failure below is a LOWER bound on the trouble.
constexpr float k_omega = 2.0f * k_pi;

const integrator k_rules[3] = {integrator::explicit_euler,
                               integrator::semi_implicit_euler,
                               integrator::velocity_verlet};

/// Short tag, for table columns where `name_of` is too wide.
const char* tag_of(integrator r)
{
    switch (r)
    {
    case integrator::explicit_euler:      return "explicit";
    case integrator::semi_implicit_euler: return "semi-impl";
    case integrator::velocity_verlet:     return "verlet";
    }
    return "?";
}

void rule(const char* title)
{
    std::printf("\n%s\n", title);
    std::printf("------------------------------------------------------------\n");
}

/// The acceleration of a linear spring, a = -w^2 x. One line, and it is the
/// linearisation of every restoring force in Module 8.
struct spring
{
    float w2;
    vec3 operator()(vec3 x) const { return x * -w2; }
};

/// Run `steps` steps of `r` on a spring, from x=(x0,0,0), v=0.
motion run_spring(integrator r, float omega, float h, long steps)
{
    motion m{.position = vec3{1.0f, 0.0f, 0.0f}, .velocity = vec3{}};
    const spring a{omega * omega};
    for (long i = 0; i < steps; ++i) { integrate(m, a, h, r); }
    return m;
}

// ---------------------------------------------------------------------------
// A  order of accuracy, and why it cannot see the bug
// ---------------------------------------------------------------------------
//
// The textbook yardstick. Halve the step, see how much the error at a fixed
// SIMULATED time shrinks, and take the log-2 ratio: 1 means first order, 2 means
// second. It is the number every numerical-methods course teaches first, and
// this section exists to show that it is blind to the thing this lesson is
// about.

/// State error at time `t`, against the closed form x(t) = cos(w t),
/// v(t) = -w sin(w t). Position and velocity are combined with the velocity
/// divided by w so that both terms are lengths and the sum is meaningful.
double error_at(integrator r, float h, double t)
{
    const long steps = std::lround(t / h);
    const motion m = run_spring(r, k_omega, h, steps);
    const double tt = static_cast<double>(steps) * h;
    const double dx = static_cast<double>(m.position.x) - std::cos(k_omega * tt);
    const double dv = static_cast<double>(m.velocity.x) + k_omega * std::sin(k_omega * tt);
    return std::sqrt(dx * dx + dv * dv / (k_omega * k_omega));
}

void section_a()
{
    rule("A  ORDER OF ACCURACY, AND THE QUESTION IT ANSWERS");
    std::printf("A.1  the textbook yardstick: halve h, see what the error\n");
    std::printf("     after ONE PERIOD does. Exact answer is the state we\n");
    std::printf("     started in, so the reference is not approximated.\n\n");
    std::printf("   h          explicit    semi-impl     verlet\n");

    const float hs[] = {1.0f / 60.0f,  1.0f / 120.0f, 1.0f / 240.0f,
                        1.0f / 480.0f, 1.0f / 960.0f, 1.0f / 1920.0f};
    const double period = 2.0 * std::numbers::pi / k_omega;
    for (float hh : hs)
    {
        std::printf("  1/%-6.0f ", 1.0 / hh);
        for (int k = 0; k < 3; ++k)
        {
            std::printf("  %9.3e", error_at(k_rules[k], hh, period));
        }
        std::printf("\n");
    }

    std::printf("\nA.2  measured slopes (1/60 -> 1/240, above the floor)\n");
    for (int k = 0; k < 3; ++k)
    {
        const double e0 = error_at(k_rules[k], 1.0f / 60.0f, period);
        const double e1 = error_at(k_rules[k], 1.0f / 240.0f, period);
        std::printf("   %-10s  slope %.3f\n", tag_of(k_rules[k]),
                    std::log2(e0 / e1) / 2.0);
    }
    std::printf("     The last row of A.1 is the FLOOR, not a result:\n");
    std::printf("     1920 steps of float arithmetic on numbers of\n");
    std::printf("     size 1 cannot resolve below about 1e-6, and the\n");
    std::printf("     verlet column has already reached it.\n");
    std::printf("     Note semi-implicit Euler reads SECOND order here.\n");
    std::printf("     It is a first-order method in general; on this\n");
    std::printf("     problem its amplitude error is exactly zero (that\n");
    std::printf("     is section B) and all that is left is a frequency\n");
    std::printf("     shift, which is O(h^2). A fact about the spring,\n");
    std::printf("     not a promotion.\n");

    std::printf("\nA.3  *** AND NOW THE OTHER QUESTION. Fix h at 1/60 -\n");
    std::printf("     which is what a game does - and vary t instead.\n\n");
    std::printf("   t (s)      explicit    semi-impl     verlet\n");
    for (double t : {1.0, 2.0, 5.0, 10.0, 20.0, 40.0})
    {
        std::printf("  %5.0f    ", t);
        for (int k = 0; k < 3; ++k)
        {
            std::printf("  %9.3e", error_at(k_rules[k], 1.0f / 60.0f, t));
        }
        std::printf("\n");
    }
    std::printf("\n     Order is a statement about h -> 0 at fixed t.\n");
    std::printf("     A game asks the opposite question: t -> infinity\n");
    std::printf("     at fixed h. Down the explicit column the error\n");
    std::printf("     MULTIPLIES; down the other two it adds. No order\n");
    std::printf("     measured in A.1 can see that, because A.1 never\n");
    std::printf("     runs anything for longer than one period.\n");

    std::printf("\nA.4  CONTROL: no restoring force (w = 0, straight line).\n");
    std::printf("     Every rule is EXACT, so whatever is left is the\n");
    std::printf("     instrument rather than the method.\n");
    for (int k = 0; k < 3; ++k)
    {
        motion m{.position = vec3{}, .velocity = vec3{1.0f, 0.0f, 0.0f}};
        const spring a{0.0f};
        for (long i = 0; i < 600; ++i) { integrate(m, a, 1.0f / 600.0f, k_rules[k]); }
        std::printf("   %-10s  x after 1 s = %.9f  (exact 1)\n",
                    tag_of(k_rules[k]), static_cast<double>(m.position.x));
    }
}

// ---------------------------------------------------------------------------
// B  the determinant, measured as an area
// ---------------------------------------------------------------------------
//
// Take three states that form a small parallelogram in the (x, v) plane. Step
// all three with the same rule. Measure the area that comes out. That ratio is
// the determinant of the update matrix, and it is the single number this lesson
// is built on — so it is MEASURED rather than quoted.

double stepped_area(integrator r, float omega, float h)
{
    // HALF A METRE, not an infinitesimal, and that is not sloppiness. The
    // update is a 2x2 MATRIX — it is linear, exactly — so the parallelogram's
    // size cancels out of the ratio algebraically and any size gives the same
    // answer. A tiny one would only throw away precision to differences of
    // nearly equal floats, which is what the first version of this section did.
    const float e = 0.5f;
    motion p0{.position = vec3{0.3f, 0, 0}, .velocity = vec3{0.7f, 0, 0}};
    motion p1 = p0;
    motion p2 = p0;
    p1.position.x += e;
    p2.velocity.x += e;

    const spring a{omega * omega};
    integrate(p0, a, h, r);
    integrate(p1, a, h, r);
    integrate(p2, a, h, r);

    // The two edge vectors of the stepped parallelogram, in (x, v).
    const double ux = static_cast<double>(p1.position.x - p0.position.x);
    const double uv = static_cast<double>(p1.velocity.x - p0.velocity.x);
    const double wx = static_cast<double>(p2.position.x - p0.position.x);
    const double wv = static_cast<double>(p2.velocity.x - p0.velocity.x);
    return std::fabs(ux * wv - uv * wx) / (static_cast<double>(e) * e);
}

void section_b()
{
    rule("B  THE DETERMINANT, MEASURED AS AN AREA IN PHASE SPACE");
    std::printf("B.1  step a small parallelogram; compare the area out\n");
    std::printf("     against the area in. h = 1/60, w = 2pi.\n\n");
    std::printf("   rule         measured      predicted      diff\n");

    const float h = 1.0f / 60.0f;
    for (int k = 0; k < 3; ++k)
    {
        const double got = stepped_area(k_rules[k], k_omega, h);
        const double want = static_cast<double>(area_factor(k_rules[k], k_omega, h));
        std::printf("   %-10s  %.9f   %.9f   %+.2e\n",
                    tag_of(k_rules[k]), got, want, got - want);
    }

    std::printf("\nB.2  the explicit surplus, as a formula. 1 + h^2 w^2:\n");
    const double u = static_cast<double>(h) * h * k_omega * k_omega;
    std::printf("     h^2 w^2 = %.9f, so one step INFLATES phase\n", u);
    std::printf("     space by %.7f%%. Every step. At every h.\n", u * 100.0);

    std::printf("\nB.3  and the same measurement across step sizes.\n\n");
    std::printf("   h         explicit      semi-impl     verlet\n");
    for (float hh : {1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 240.0f, 1.0f / 1000.0f})
    {
        std::printf("  1/%-6.0f", 1.0 / hh);
        for (int k = 0; k < 3; ++k)
        {
            std::printf("  %.9f", stepped_area(k_rules[k], k_omega, hh));
        }
        std::printf("\n");
    }
    std::printf("     explicit never reaches 1. It only approaches it,\n");
    std::printf("     and section D is what that costs.\n");

    std::printf("\nB.4  CONTROL: h = 0. Every rule is the identity map, so\n");
    std::printf("     the area MUST be 1 for reasons unrelated to any of\n");
    std::printf("     this. A meter that failed here would be broken.\n");
    for (int k = 0; k < 3; ++k)
    {
        std::printf("   %-10s  %.9f\n", tag_of(k_rules[k]),
                    stepped_area(k_rules[k], k_omega, 0.0f));
    }
}

// ---------------------------------------------------------------------------
// C  a spring, run long
// ---------------------------------------------------------------------------

void section_c()
{
    rule("C  A 1 Hz SPRING, RUN LONG (h = 1/60, amplitude 1 m)");
    std::printf("C.1  mechanical energy, as a multiple of the energy it\n");
    std::printf("     started with.\n\n");
    std::printf("   t (s)      explicit      semi-impl      verlet\n");

    const float h = 1.0f / 60.0f;
    const float e0 = spring_energy({.position = vec3{1, 0, 0}, .velocity = vec3{}}, k_omega);
    for (long secs : {1L, 5L, 10L, 30L, 60L})
    {
        std::printf("  %5ld   ", secs);
        for (int k = 0; k < 3; ++k)
        {
            const motion m = run_spring(k_rules[k], k_omega, h, secs * 60);
            std::printf("  %12.4e", static_cast<double>(spring_energy(m, k_omega) / e0));
        }
        std::printf("\n");
    }

    std::printf("\nC.2  the same run as an amplitude, in metres. A 1 m\n");
    std::printf("     spring after one minute:\n");
    for (int k = 0; k < 3; ++k)
    {
        const motion m = run_spring(k_rules[k], k_omega, h, 3600);
        const float amp = std::sqrt(2.0f * spring_energy(m, k_omega)) / k_omega;
        std::printf("   %-10s  %.4e m\n", tag_of(k_rules[k]), static_cast<double>(amp));
    }

    std::printf("\nC.3  predicted from the determinant alone. Energy is an\n");
    std::printf("     area, so after N steps it is (1+h^2w^2)^N:\n");
    const double u = static_cast<double>(h) * h * k_omega * k_omega;
    std::printf("     (1+%.9f)^3600 = %.4e\n", u, std::pow(1.0 + u, 3600.0));
    std::printf("     measured above. The determinant is not a\n");
    std::printf("     description of the blow-up; it IS the blow-up.\n");

    std::printf("\nC.4  what semi-implicit Euler actually conserves. The\n");
    std::printf("     true energy wobbles; the SHADOW energy does not.\n\n");
    std::printf("   quantity            min          max      spread\n");
    {
        motion m{.position = vec3{1, 0, 0}, .velocity = vec3{}};
        const spring a{k_omega * k_omega};
        double emin = 1e30, emax = -1e30, smin = 1e30, smax = -1e30;
        for (long i = 0; i < 60 * 60; ++i)
        {
            integrate(m, a, h, integrator::semi_implicit_euler);
            const double e = spring_energy(m, k_omega);
            const double s = shadow_energy(m, k_omega, h);
            emin = std::min(emin, e); emax = std::max(emax, e);
            smin = std::min(smin, s); smax = std::max(smax, s);
        }
        std::printf("   true energy     %.7f  %.7f   %.3e\n",
                    emin, emax, (emax - emin) / ((emax + emin) * 0.5));
        std::printf("   shadow energy   %.7f  %.7f   %.3e\n",
                    smin, smax, (smax - smin) / ((smax + smin) * 0.5));
        std::printf("\n     predicted spread of the true energy is exactly\n");
        std::printf("     h*w = %.6f. Measured %.6f.\n",
                    static_cast<double>(h * k_omega), (emax - emin) / ((emax + emin) * 0.5));
    }

    std::printf("\nC.5  CONTROL: the same shadow quantity under EXPLICIT\n");
    std::printf("     Euler, which does not conserve it either. If the\n");
    std::printf("     meter were simply flat, C.4 would mean nothing.\n");
    {
        motion m{.position = vec3{1, 0, 0}, .velocity = vec3{}};
        const spring a{k_omega * k_omega};
        double smin = 1e30, smax = -1e30;
        for (long i = 0; i < 60 * 60; ++i)
        {
            integrate(m, a, h, integrator::explicit_euler);
            const double s = shadow_energy(m, k_omega, h);
            smin = std::min(smin, s); smax = std::max(smax, s);
        }
        std::printf("   shadow energy   %.4e  %.4e\n", smin, smax);
    }
}

// ---------------------------------------------------------------------------
// D  smaller h does not save explicit Euler
// ---------------------------------------------------------------------------

/// Simulated seconds until the spring's energy has doubled.
double time_to_double(float h)
{
    motion m{.position = vec3{1, 0, 0}, .velocity = vec3{}};
    const spring a{k_omega * k_omega};
    const float e0 = spring_energy(m, k_omega);
    long i = 0;
    const long cap = 200'000'000L;
    while (spring_energy(m, k_omega) < 2.0f * e0 && i < cap)
    {
        integrate(m, a, h, integrator::explicit_euler);
        ++i;
    }
    return static_cast<double>(i) * h;
}

void section_d()
{
    rule("D  SMALLER h DOES NOT SAVE IT (explicit Euler, 1 Hz spring)");
    std::printf("D.1  simulated seconds until the energy has doubled.\n\n");
    std::printf("   rate (Hz)      h        t_double (s)   predicted\n");
    for (float rate : {30.0f, 60.0f, 120.0f, 240.0f, 1000.0f, 4000.0f})
    {
        const float h = 1.0f / rate;
        const double u = static_cast<double>(h) * h * k_omega * k_omega;
        const double want = h * std::log(2.0) / std::log1p(u);
        std::printf("   %7.0f   %.7f   %10.3f   %10.3f\n",
                    static_cast<double>(rate), static_cast<double>(h),
                    time_to_double(h), want);
    }
    std::printf("\n     t_double is proportional to 1/h. Ten times the\n");
    std::printf("     step rate buys ten times the delay, not a fix.\n");
    std::printf("     The 30 Hz row is 5%% off its prediction and the\n");
    std::printf("     rest are not, because the true energy WOBBLES by\n");
    std::printf("     h*w per period (C.4) - 21%% at 30 Hz - so the\n");
    std::printf("     crossing of 2x is caught at an arbitrary phase.\n");

    std::printf("\nD.2  read backwards: what rate would hold the energy to\n");
    std::printf("     within 1%% over a ten-minute level?\n");
    {
        const double target = std::log(1.01);   // total log-growth allowed
        const double T = 600.0;
        // (T/h) * log1p(h^2 w^2) = target  ->  solve for h by bisection.
        double lo = 1e-9, hi = 1e-2;
        for (int i = 0; i < 200; ++i)
        {
            const double mid = 0.5 * (lo + hi);
            const double g = (T / mid) * std::log1p(mid * mid * k_omega * k_omega);
            if (g > target) { hi = mid; } else { lo = mid; }
        }
        const double h = 0.5 * (lo + hi);
        std::printf("     h = %.3e s, i.e. %.0f steps per second.\n", h, 1.0 / h);
        std::printf("     For ONE spring. At 1 Hz. The answer is not a\n");
        std::printf("     smaller step; the answer is a different rule.\n");
    }

    std::printf("\nD.3  CONTROL: the same measurement for semi-implicit\n");
    std::printf("     Euler, which must never reach 2x at all.\n");
    {
        motion m{.position = vec3{1, 0, 0}, .velocity = vec3{}};
        const spring a{k_omega * k_omega};
        const float e0 = spring_energy(m, k_omega);
        double worst = 0.0;
        for (long i = 0; i < 60 * 60 * 60; ++i)   // one simulated hour
        {
            integrate(m, a, 1.0f / 60.0f, integrator::semi_implicit_euler);
            worst = std::max(worst, static_cast<double>(spring_energy(m, k_omega) / e0));
        }
        std::printf("   worst energy ratio over ONE SIMULATED HOUR: %.6f\n", worst);
    }
}

// ---------------------------------------------------------------------------
// E  gravity, where the choice very nearly does not matter
// ---------------------------------------------------------------------------

void section_e()
{
    rule("E  CONSTANT ACCELERATION (g = 9.81, h = 1/60, from rest)");
    std::printf("E.1  height fallen after 1 s. Exact: 0.5*g*t^2 =\n");
    std::printf("     %.6f m, speed %.6f m/s.\n\n", 0.5 * 9.81, 9.81);
    std::printf("   rule          fallen (m)     error (m)    speed\n");

    const vec3 g{0.0f, -9.81f, 0.0f};
    const float h = 1.0f / 60.0f;
    for (int k = 0; k < 3; ++k)
    {
        motion m{};
        for (int i = 0; i < 60; ++i) { integrate(m, g, h, k_rules[k]); }
        const double fallen = -static_cast<double>(m.position.y);
        std::printf("   %-10s  %10.6f   %+10.6f   %8.5f\n",
                    tag_of(k_rules[k]), fallen, fallen - 0.5 * 9.81,
                    -static_cast<double>(m.velocity.y));
    }
    std::printf("\n     predicted error = 0.5*g*h*t = %+.6f m,\n", 0.5 * 9.81 * h * 1.0);
    std::printf("     one rule short by it and one long by it. The\n");
    std::printf("     SPEED is identical to the last bit in all three.\n");

    std::printf("\nE.2  the error is linear in t, not exponential.\n\n");
    std::printf("   t (s)    explicit err   semi-impl err   verlet\n");
    for (int secs : {1, 2, 4, 8})
    {
        std::printf("   %4d ", secs);
        for (int k = 0; k < 3; ++k)
        {
            motion m{};
            for (int i = 0; i < 60 * secs; ++i) { integrate(m, g, h, k_rules[k]); }
            const double exact = -0.5 * 9.81 * secs * secs;
            std::printf("   %+12.6f", static_cast<double>(m.position.y) - exact);
        }
        std::printf("\n");
    }

    std::printf("\nE.3  *** SO YOU WILL NOT SEE THIS BUG IN YOUR FIRST\n");
    std::printf("     DEMO. Gravity alone is forgiving. The moment a\n");
    std::printf("     force depends on POSITION - every contact, every\n");
    std::printf("     joint, every spring - section C is what happens.\n");

    std::printf("\nE.4  CONTROL: velocity Verlet must be EXACT here, not\n");
    std::printf("     merely close, because 0.5*a*h^2 is the whole of\n");
    std::printf("     the missing Taylor term when a is constant.\n");
    {
        motion m{};
        for (int i = 0; i < 60 * 8; ++i)
        {
            integrate(m, g, h, integrator::velocity_verlet);
        }
        const double got = static_cast<double>(m.position.y);
        const double want = -0.5 * 9.81 * 64.0;
        std::printf("   after 8 s: %.6f m   exact: %.6f m\n", got, want);
        std::printf("   residual %.2e m, i.e. %.1f ulp of a float that\n",
                    got - want, (got - want) / 3.0517578125e-05);
        std::printf("   size. EXACT means the ARITHMETIC is exact; the\n");
        std::printf("   floor underneath it is still float's own.\n");
    }
}

// ---------------------------------------------------------------------------
// F  the stability limit
// ---------------------------------------------------------------------------

bool stays_bounded(integrator r, float omega, float h)
{
    motion m{.position = vec3{1, 0, 0}, .velocity = vec3{}};
    const spring a{omega * omega};
    for (long i = 0; i < 20000; ++i)
    {
        integrate(m, a, h, r);
        if (!(std::fabs(m.position.x) < 100.0f)) { return false; }
    }
    return true;
}

void section_f()
{
    rule("F  THE STABILITY LIMIT, FOUND BY BISECTION (w = 2pi)");
    std::printf("F.1  largest h that stays bounded over 20,000 steps.\n\n");
    std::printf("   rule          measured h    2/w         ratio\n");
    for (int k = 1; k < 3; ++k)
    {
        double lo = 1.0e-4, hi = 1.0;
        for (int i = 0; i < 60; ++i)
        {
            const double mid = 0.5 * (lo + hi);
            if (stays_bounded(k_rules[k], k_omega, static_cast<float>(mid))) { lo = mid; }
            else { hi = mid; }
        }
        const double want = static_cast<double>(max_stable_step(k_rules[k], k_omega));
        std::printf("   %-10s  %.7f    %.7f   %.5f\n",
                    tag_of(k_rules[k]), lo, want, lo / want);
    }

    std::printf("\nF.2  what the last 10%% before the limit looks like.\n");
    std::printf("     semi-implicit Euler: the WORST energy excursion\n");
    std::printf("     over 10 s, against 1/(1 - h*w/2), which is where\n");
    std::printf("     the shadow ellipse of C.4 puts its far end.\n\n");
    std::printf("   h*w     steps/period   worst E/E0   predicted\n");
    for (double hw : {0.2, 0.5, 1.0, 1.5, 1.8, 1.95, 1.99})
    {
        const float h = static_cast<float>(hw / k_omega);
        motion m{.position = vec3{1, 0, 0}, .velocity = vec3{}};
        const spring a{k_omega * k_omega};
        const float e0 = spring_energy(m, k_omega);
        double worst = 1.0;
        for (long i = 0; i < std::lround(10.0 / h); ++i)
        {
            integrate(m, a, h, integrator::semi_implicit_euler);
            worst = std::max(worst, static_cast<double>(spring_energy(m, k_omega) / e0));
        }
        std::printf("   %.2f    %10.2f   %10.3f  %10.3f\n", hw,
                    2.0 * std::numbers::pi / hw, worst, 1.0 / (1.0 - hw / 2.0));
    }
    std::printf("     BOUNDED is not the same as RIGHT. Every row here\n");
    std::printf("     is stable and only the first two are usable. The\n");
    std::printf("     measurement lands on the prediction to three\n");
    std::printf("     digits and falls a hair short at the bottom,\n");
    std::printf("     because an orbit sampled three times a period\n");
    std::printf("     does not quite visit its own extremes. Budget a\n");
    std::printf("     factor of five.\n");

    std::printf("\nF.3  CONTROL: explicit Euler, same bisection. There is\n");
    std::printf("     no stable h, so the search must bottom out.\n");
    {
        double lo = 1.0e-6, hi = 1.0;
        for (int i = 0; i < 60; ++i)
        {
            const double mid = 0.5 * (lo + hi);
            if (stays_bounded(integrator::explicit_euler, k_omega,
                              static_cast<float>(mid))) { lo = mid; }
            else { hi = mid; }
        }
        std::printf("   largest 'bounded' h over 20,000 steps: %.3e\n", lo);
        std::printf("   max_stable_step() reports: %.1f\n",
                    static_cast<double>(max_stable_step(integrator::explicit_euler,
                                                        k_omega)));
        std::printf("   ...and 20,000 steps at that h is %.2f simulated\n", lo * 20000.0);
        std::printf("   seconds. The bisection is measuring the clock,\n");
        std::printf("   not the method. That is what 'no limit' looks\n");
        std::printf("   like from inside a test.\n");
    }
}

// ---------------------------------------------------------------------------
// G  `v *= 0.99f`
// ---------------------------------------------------------------------------

void section_g()
{
    rule("G  `v *= 0.99f` AND THE THREE GAMES IT MAKES");
    std::printf("G.1  velocity remaining after one simulated second.\n\n");
    std::printf("   rate      per-step 0.99    pow(r, h)     target\n");

    // The 60 Hz answer is taken as the intended behaviour, because 60 Hz is
    // what the line was written at. That is the whole of the bug: an intent
    // that only exists at one step rate.
    const double target = std::pow(0.99, 60.0);
    for (float rate : {30.0f, 60.0f, 120.0f, 144.0f})
    {
        const float h = 1.0f / rate;
        float naive = 1.0f;
        float fixed = 1.0f;
        const float f = damping_factor(static_cast<float>(target), h);
        for (int i = 0; i < static_cast<int>(rate); ++i) { naive *= 0.99f; fixed *= f; }
        std::printf("   %5.0f Hz   %11.6f    %9.6f   %9.6f\n",
                    static_cast<double>(rate), static_cast<double>(naive),
                    static_cast<double>(fixed), target);
    }
    std::printf("\n     Same constant, same code, three different games.\n");
    std::printf("     A 144 Hz monitor makes the car handle differently.\n");

    std::printf("\nG.2  CONTROL: at exactly 60 Hz the two paths must be\n");
    std::printf("     the same computation, because that is the rate\n");
    std::printf("     the target was derived at. If they differed here\n");
    std::printf("     the fix would be changing the behaviour, not\n");
    std::printf("     preserving it.\n");
    {
        const float h = 1.0f / 60.0f;
        const float f = damping_factor(static_cast<float>(target), h);
        std::printf("   0.99 vs pow(target, 1/60) = %.9f vs %.9f\n",
                    0.99, static_cast<double>(f));
        std::printf("   difference: %.3e\n", std::fabs(0.99 - static_cast<double>(f)));
    }
}

// ---------------------------------------------------------------------------
// H  drag
// ---------------------------------------------------------------------------

void section_h()
{
    rule("H  DRAG: THE STEP THAT REVERSES, AND THE ONE THAT DIVERGES");
    std::printf("H.1  a = -k v for one step of h = 1/60. Explicit Euler\n");
    std::printf("     multiplies v by (1 - h k); the exact answer is\n");
    std::printf("     exp(-h k).\n\n");
    std::printf("   k       h*k      1 - h*k      exp(-h*k)   verdict\n");

    const float h = 1.0f / 60.0f;
    for (float k : {6.0f, 30.0f, 60.0f, 90.0f, 150.0f})
    {
        const double hk = static_cast<double>(h) * k;
        const double euler = 1.0 - hk;
        const double exact = std::exp(-hk);
        // The boundary at h*k = 1 is printed as its own verdict rather than
        // folded into one of the neighbours: it is where the step removes the
        // ENTIRE velocity in one go, which is not drag, it is a handbrake. (The
        // -0.00000 in that row is 1.0f/60.0f*60 falling one ulp short of 1.)
        const char* verdict = (std::fabs(1.0 - hk) < 1e-5) ? "STOPS DEAD"
                              : (hk < 1.0)                 ? "ok"
                              : (hk < 2.0)                 ? "REVERSES"
                                                           : "DIVERGES";
        std::printf("   %5.0f   %.4f   %+9.5f    %9.6f   %s\n",
                    static_cast<double>(k), hk, euler, exact, verdict);
    }

    std::printf("\nH.2  what that looks like over one second at k = 90\n");
    std::printf("     (h*k = 1.5, so the sign flips every step):\n\n");
    std::printf("   step    explicit v      apply_drag v      exact\n");
    {
        float ve = 1.0f;
        motion m{.velocity = vec3{1, 0, 0}};
        for (int i = 1; i <= 5; ++i)
        {
            ve *= (1.0f - h * 90.0f);
            apply_drag(m, 90.0f, h);
            std::printf("   %4d   %+12.6f    %12.6f    %9.6f\n", i,
                        static_cast<double>(ve), static_cast<double>(m.velocity.x),
                        std::exp(-90.0 * h * i));
        }
    }

    std::printf("\nH.3  CONTROL: at a gentle k the two agree, which is\n");
    std::printf("     why this bug survives review. k = 6, h*k = 0.1:\n");
    {
        motion m{.velocity = vec3{1, 0, 0}};
        apply_drag(m, 6.0f, h);
        const double euler = 1.0 - 0.1;
        std::printf("   explicit %.6f   exact %.6f   %.2f%% apart\n",
                    euler, static_cast<double>(m.velocity.x),
                    100.0 * (static_cast<double>(m.velocity.x) - euler)
                        / static_cast<double>(m.velocity.x));
    }
}

// ---------------------------------------------------------------------------
// I  the budget, and the free lunch
// ---------------------------------------------------------------------------

/// BEST OF FIVE, not the mean, and the reason is the one 7.8 ran into from the
/// other end. A mean is the right summary when the thing you are measuring is
/// the distribution — the audio mixer's log line was a syscall, and its tail WAS
/// the finding. Here the distribution is the machine: scheduler, turbo state,
/// another process. The quantity wanted is "what does this loop cost when
/// nothing interferes", and the minimum is the only estimator of it that does
/// not move when the machine gets busier.
template <class Fn>
double time_ns(long reps, Fn&& fn)
{
    double best = 1e30;
    for (int trial = 0; trial < 5; ++trial)
    {
        const auto t0 = std::chrono::steady_clock::now();
        fn();
        const auto t1 = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count()
                                  / static_cast<double>(reps));
    }
    return best;
}

void section_i()
{
    rule("I  THE BUDGET (10,000 bodies x 100 steps, -O2)");

    constexpr int k_bodies = 10000;
    constexpr int k_steps = 100;
    const long reps = static_cast<long>(k_bodies) * k_steps;
    const float h = 1.0f / 60.0f;
    const spring accel{k_omega * k_omega};

    std::vector<motion> bodies(k_bodies);
    auto reset = [&] {
        for (int i = 0; i < k_bodies; ++i)
        {
            bodies[static_cast<std::size_t>(i)] = {
                .position = vec3{1.0f + 0.0001f * static_cast<float>(i), 0, 0},
                .velocity = vec3{}};
        }
    };

    std::printf("I.1  cost per body per step, spring force inlined and\n");
    std::printf("     the RULE A COMPILE-TIME CONSTANT - which is how an\n");
    std::printf("     engine writes it, and what makes the three loops\n");
    std::printf("     comparable at all.\n\n");
    std::printf("   rule            ns/body/step   force evals\n");

    // A template non-type parameter, so each of the three timed loops is a
    // separate monomorphic function with the switch folded away at compile
    // time. Timing the runtime-`integrator` version instead measures whether
    // the optimiser felt like unswitching the loop today, which is a fact about
    // the compiler and not about the method.
    auto timed = [&]<integrator R>() {
        reset();
        return time_ns(reps, [&] {
            for (int st = 0; st < k_steps; ++st)
            {
                for (motion& m : bodies) { integrate(m, accel, h, R); }
            }
        });
    };

    const double ns_exp = timed.template operator()<integrator::explicit_euler>();
    const double ns_sie = timed.template operator()<integrator::semi_implicit_euler>();
    const double ns_ver = timed.template operator()<integrator::velocity_verlet>();
    std::printf("   explicit        %8.3f          1\n", ns_exp);
    std::printf("   semi-impl       %8.3f          1\n", ns_sie);
    std::printf("   verlet          %8.3f          2\n", ns_ver);
    const double base = ns_exp;

    std::printf("\nI.2  *** THE FREE LUNCH. semi-implicit Euler costs the\n");
    std::printf("     same as explicit Euler - same loads, same\n");
    std::printf("     multiplies, two lines in the other order - and is\n");
    std::printf("     unconditionally better. The two differ by %.1f%%,\n",
                100.0 * std::fabs(ns_sie - ns_exp) / ns_exp);
    std::printf("     against a run-to-run spread of the same order.\n");
    std::printf("     Verlet is %.0f%% dearer and buys second order.\n",
                100.0 * (ns_ver - base) / base);

    std::printf("\nI.3  the same loop through a std::function, which is\n");
    std::printf("     what a non-template integrator would cost you.\n");
    {
        reset();
        const std::function<vec3(vec3)> indirect = accel;
        const double ns = time_ns(reps, [&] {
            for (int s = 0; s < k_steps; ++s)
            {
                for (motion& m : bodies)
                {
                    integrate(m, indirect, h, integrator::semi_implicit_euler);
                }
            }
        });
        std::printf("   std::function   %8.3f ns/body/step\n", ns);
    }

    std::printf("\nI.4  and the constant-acceleration overload, which is\n");
    std::printf("     the gravity path and is not a template at all.\n");
    {
        reset();
        const vec3 g{0.0f, -9.81f, 0.0f};
        const double ns = time_ns(reps, [&] {
            for (int s = 0; s < k_steps; ++s)
            {
                for (motion& m : bodies)
                {
                    integrate(m, g, h, integrator::semi_implicit_euler);
                }
            }
        });
        std::printf("   integrate(m, g, ...)  %8.3f ns/body/step\n", ns);
        std::printf("   SLOWER than the template in I.1, and that is the\n");
        std::printf("   whole argument for the template: this one is a\n");
        std::printf("   real call into libengine.a and cannot inline\n");
        std::printf("   across the archive. The simpler-looking path is\n");
        std::printf("   the expensive one.\n");
        std::printf("   10,000 bodies at 60 Hz: %.3f ms of a 16.67 ms\n",
                    ns * k_bodies * 1e-6);
        std::printf("   frame, which is %.2f%% of it.\n",
                    100.0 * ns * k_bodies * 1e-6 / 16.667);
    }

    std::printf("\nI.5  CONTROL: a loop that does nothing but walk the\n");
    std::printf("     same array, so the numbers above are the\n");
    std::printf("     arithmetic and not the memory traffic.\n");
    {
        reset();
        float sink = 0.0f;
        const double ns = time_ns(reps, [&] {
            for (int s = 0; s < k_steps; ++s)
            {
                for (motion& m : bodies) { sink += m.position.x; }
            }
        });
        std::printf("   touch only      %8.3f ns/body/step  (sink %.1f)\n",
                    ns, static_cast<double>(sink));
    }
}

} // namespace

int main()
{
    std::printf("verify_81 — Lesson 8.1, Integrators\n");
    std::printf("float arithmetic throughout, as the engine uses.\n");
    std::printf("spring: w = 2pi rad/s (1 Hz), the gentlest in any game.\n");
    std::printf("natural_frequency(k=100, m=2.5) = %.6f rad/s\n",
                static_cast<double>(natural_frequency(100.0f, 2.5f)));
    std::printf("name_of(semi_implicit_euler) = \"%s\"\n",
                name_of(integrator::semi_implicit_euler));

    section_a();
    section_b();
    section_c();
    section_d();
    section_e();
    section_f();
    section_g();
    section_h();
    section_i();

    std::printf("\ndone.\n");
    return 0;
}
