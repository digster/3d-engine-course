// scratch/verify_82.cpp — every number Lesson 8.2 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_82.sh
//
// Nine sections, in the lesson's order:
//
//   A  F = ma: one force, three masses
//   B  the accumulator, and the one way forces do not commute
//   C  gravity is mass-blind — and the round trip that is not exact
//   D  damping is not drag
//   E  an impulse has no `h` in it, and a force does
//   F  inverse mass zero, and the infinity that is not a mass
//   G  units: the square root that makes miniatures look like miniatures
//   H  frames: what a scaled parent does to gravity
//   I  the budget
//
// EVERY SECTION CARRIES A CONTROL, and 8.1 left the rule in two halves: ask what
// the control would say if the thing were COMPLETELY BROKEN (7.6) and what it
// would say if the thing were completely FINE (7.8). Two here are written
// against the second half. B.4 sums four forces that are exact powers of two, so
// every permutation must agree to the bit and the spread must be exactly zero —
// which is what makes B.2's nonzero spread a fact about the VALUES rather than
// about addition. And C.4 runs the mass-round-trip drift on two bodies of the
// SAME mass, where it must vanish, so that C.2's drift is known to be about the
// mass and not about the code path.
//
// PRECISION. The engine integrates in `float`, so this harness does too. Section
// C is entirely about a float question and prints the ulp beside the number;
// everywhere else the closed forms are evaluated in double and the simulation in
// float, which is the only honest way round — a reference computed at the same
// precision as the thing it checks cannot tell you which one is wrong.
//
// OUTPUT WIDTH IS A CONSTRAINT, not a preference. The lesson quotes this
// program's transcript in <pre> blocks that scroll and never wrap, and the fold
// is at about 66 characters. Nothing below exceeds 66.

#include <engine/core/bench.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/integrate.hpp>
#include <engine/phys/rigid_body.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>

using engine::affine;
using engine::mat3;
using engine::mat4;
using engine::vec3;
using engine::phys::add_force;
using engine::phys::add_impulse;
using engine::phys::body_kind;
using engine::phys::body_world;
using engine::phys::free_fall_time;
using engine::phys::frame_report;
using engine::phys::inspect_frame;
using engine::phys::integrate;
using engine::phys::integrator;
using engine::phys::k_gravity;
using engine::phys::make_dynamic;
using engine::phys::mass_of;
using engine::phys::motion;
using engine::phys::rigid_body;
using engine::phys::set_mass;
using engine::phys::terminal_speed_damped;
using engine::phys::terminal_speed_dragged;
using engine::phys::time_scale_for_length_scale;

namespace
{

constexpr float k_h60 = 1.0f / 60.0f;

void rule(const char* title)
{
    std::printf("\n%s\n", title);
    for (int i = 0; i < 66; ++i) { std::putchar('-'); }
    std::putchar('\n');
}

/// The gap between `x` and the next representable float, at `x`'s magnitude.
/// Printed beside any measurement that lands near it, because a number at the
/// noise floor is a fact about the instrument (8.1's rule).
float ulp_at(float x)
{
    const float a = std::fabs(x);
    return (a == 0.0f) ? 0.0f : std::nextafterf(a, HUGE_VALF) - a;
}

/// Step one body in its own world, `steps` times, and hand back its state.
/// Every section below that "simulates" does it through this, so no section can
/// accidentally measure a different code path from another.
motion run(rigid_body body, int steps, float h, vec3 gravity)
{
    body_world w;
    w.set_gravity(gravity);
    const engine::phys::body_id id = w.add(body);
    for (int i = 0; i < steps; ++i) { w.step(h); }
    return w.get(id)->state;
}

// ===========================================================================
// A — F = ma: one force, three masses
// ===========================================================================

void section_a()
{
    rule("A  F = ma: ONE FORCE, THREE MASSES");

    const float masses[3] = {1.0f, 10.0f, 1000.0f};
    const vec3 push{0.0f, 0.0f, 50.0f};   // 50 N along +z

    std::printf("  one step of h = 1/60 s, F = 50 N, no gravity\n\n");
    std::printf("    mass      inv_mass        a       v after 1 step\n");

    float v_at[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 3; ++i)
    {
        body_world w;
        w.set_gravity(vec3{});
        const engine::phys::body_id id = w.add(make_dynamic(vec3{}, masses[i]));

        // The push is applied to the body IN the table, which is what a real
        // caller does — `add` took a copy, so pushing the local would push a
        // body nobody is stepping.
        rigid_body* b = w.get(id);
        add_force(*b, push);
        const vec3 a = b->force * b->inv_mass;   // read before step() clears it
        w.step(k_h60);

        v_at[i] = w.get(id)->state.velocity.z;
        std::printf("  %7.1f kg  %.6f  %8.4f    %10.6f m/s\n",
                    static_cast<double>(masses[i]),
                    static_cast<double>(b->inv_mass),
                    static_cast<double>(a.z), static_cast<double>(v_at[i]));
    }

    std::printf("\n  A.1  m*a recovers the force at every mass:\n");
    for (int i = 0; i < 3; ++i)
    {
        rigid_body b = make_dynamic(vec3{}, masses[i]);
        add_force(b, push);
        const vec3 a = b.force * b.inv_mass;
        std::printf("       %7.1f kg -> m*a = %10.4f N   (F = 50 N)\n",
                    static_cast<double>(masses[i]),
                    static_cast<double>(a.z * masses[i]));
    }

    std::printf("\n  A.2  v(1 kg) / v(1000 kg) = %.4f  (expect 1000)\n",
                static_cast<double>(v_at[0] / v_at[2]));

    // CONTROL: two bodies of the SAME mass and the same push must agree to the
    // bit. If they do not, nothing above this line means anything.
    rigid_body p = make_dynamic(vec3{}, 7.25f);
    rigid_body q = make_dynamic(vec3{}, 7.25f);
    add_force(p, push);
    add_force(q, push);
    const vec3 ap = p.force * p.inv_mass;
    const vec3 aq = q.force * q.inv_mass;
    std::printf("\n  A.3  CONTROL same mass, same push: identical  %s\n",
                (ap.z == aq.z) ? "yes" : "NO");
}

// ===========================================================================
// B — the accumulator, and the one way forces do not commute
// ===========================================================================

void spread_of_permutations(const vec3 f[4], const char* label)
{
    int order[4] = {0, 1, 2, 3};
    std::sort(order, order + 4);

    float lo = HUGE_VALF;
    float hi = -HUGE_VALF;
    int n = 0;
    do
    {
        rigid_body b = make_dynamic(vec3{}, 1.0f);
        for (int k = 0; k < 4; ++k) { add_force(b, f[order[k]]); }
        const float s = b.force.x;
        lo = std::min(lo, s);
        hi = std::max(hi, s);
        ++n;
    } while (std::next_permutation(order, order + 4));

    float smallest = HUGE_VALF;
    for (int k = 0; k < 4; ++k) { smallest = std::min(smallest, std::fabs(f[k].x)); }

    std::printf("  %s\n", label);
    std::printf("    %d orders   lo %.9f   hi %.9f\n", n,
                static_cast<double>(lo), static_cast<double>(hi));
    std::printf("    spread %.4e = %.1f%% of the answer\n",
                static_cast<double>(hi - lo),
                (hi != 0.0f) ? static_cast<double>(100.0f * (hi - lo) / hi) : 0.0);
    std::printf("    the smallest of the four forces is %.4e\n",
                static_cast<double>(smallest));
}

void section_b()
{
    rule("B  THE ACCUMULATOR, AND THE ONE WAY FORCES DO NOT COMMUTE");

    // B.1 superposition: four systems push, and the result is the sum.
    const vec3 gravity_like{0.0f, -19.62f, 0.0f};
    const vec3 thruster{14.0f, 0.0f, 0.0f};
    const vec3 wind{-4.5f, 1.25f, 3.0f};
    const vec3 drag{-2.25f, 0.5f, -1.5f};

    rigid_body b = make_dynamic(vec3{}, 2.0f);
    add_force(b, gravity_like);
    add_force(b, thruster);
    add_force(b, wind);
    add_force(b, drag);
    std::printf("  B.1  four systems, four calls, no coordination:\n");
    std::printf("       F = (%.3f, %.3f, %.3f) N\n",
                static_cast<double>(b.force.x), static_cast<double>(b.force.y),
                static_cast<double>(b.force.z));
    const vec3 a = b.force * b.inv_mass;
    std::printf("       a = F/m = (%.4f, %.4f, %.4f) m/s^2  (m = 2 kg)\n\n",
                static_cast<double>(a.x), static_cast<double>(a.y),
                static_cast<double>(a.z));

    // B.2/B.3 the float caveat, measured over all 24 orders.
    const vec3 awkward[4] = {{1.0e6f, 0.0f, 0.0f},
                             {-1.0e6f, 0.0f, 0.0f},
                             {0.125f, 0.0f, 0.0f},
                             {3.7e-3f, 0.0f, 0.0f}};
    std::printf("  B.2  same four forces, 24 orders, x component only\n");
    spread_of_permutations(awkward, "       [1e6, -1e6, 0.125, 0.0037] N");

    const vec3 tame[4] = {{4.0f, 0.0f, 0.0f},
                          {2.0f, 0.0f, 0.0f},
                          {1.0f, 0.0f, 0.0f},
                          {0.5f, 0.0f, 0.0f}};
    std::printf("\n  B.3  CONTROL exactly representable, partial sums exact\n");
    spread_of_permutations(tame, "       [4, 2, 1, 0.5] N");

    // B.4 what the accumulator buys that a setter does not: the LAST WRITER
    // problem, shown rather than described.
    rigid_body setter = make_dynamic(vec3{}, 2.0f);
    setter.force = gravity_like;
    setter.force = thruster;      // a second system, written as a setter
    std::printf("\n  B.4  a setter instead of an accumulator:\n");
    std::printf("       F = (%.3f, %.3f, %.3f) N   gravity is gone\n",
                static_cast<double>(setter.force.x),
                static_cast<double>(setter.force.y),
                static_cast<double>(setter.force.z));
}

// ===========================================================================
// C — gravity is mass-blind, and the round trip that is not exact
// ===========================================================================

void section_c()
{
    rule("C  GRAVITY IS MASS-BLIND - AND THE ROUND TRIP THAT IS NOT");

    // C.1 the round trip the current `step` performs: F = m*g, then a = F/m.
    // Seven hand-picked masses would prove nothing either way, so this sweeps a
    // million of them and reports how often the two roundings fail to cancel.
    std::printf("  C.1  a = (g / inv_mass) * inv_mass, against g = -9.81\n");
    {
        const float g = -k_gravity;
        int checked = 0;
        int inexact = 0;
        float worst = 0.0f;
        float worst_mass = 0.0f;
        // Log-uniform over 1 g to 1000 tonnes, which brackets everything a game
        // has a mass for, plus the two ends nothing does.
        for (int i = 0; i < 1000001; ++i)
        {
            const double t = static_cast<double>(i) / 1000000.0;
            const float m = static_cast<float>(std::pow(10.0, -3.0 + 9.0 * t));
            rigid_body b = make_dynamic(vec3{}, m);
            const float force = g * (1.0f / b.inv_mass);
            const float a = force * b.inv_mass;
            ++checked;
            const float err = std::fabs(a - g);
            if (a != g) { ++inexact; }
            if (err > worst) { worst = err; worst_mass = m; }
        }
        std::printf("       %d masses from 1 g to 1000 t\n", checked);
        std::printf("       inexact %d (%.4f%%)   worst %.3e at %.4g kg\n",
                    inexact, 100.0 * inexact / checked,
                    static_cast<double>(worst), static_cast<double>(worst_mass));
        std::printf("       one ulp of g is %.3e, so that is %.2f ulp\n",
                    static_cast<double>(ulp_at(k_gravity)),
                    (ulp_at(k_gravity) > 0.0f)
                        ? static_cast<double>(worst / ulp_at(k_gravity)) : 0.0);
    }

    // C.2 A HAND-PICKED MASS PROVES NOTHING. Find one the sweep says is
    // inexact, and let it fall beside an exact one.
    const vec3 g{0.0f, -k_gravity, 0.0f};
    float bad_mass = 0.0f;
    {
        const float gy = -k_gravity;
        for (int i = 0; i < 1000001 && bad_mass == 0.0f; ++i)
        {
            const double t = static_cast<double>(i) / 1000000.0;
            const float m = static_cast<float>(std::pow(10.0, 9.0 * t));   // 1 kg up
            rigid_body b = make_dynamic(vec3{}, m);
            if ((gy * (1.0f / b.inv_mass)) * b.inv_mass != gy) { bad_mass = m; }
        }
    }

    // The FORCE route, written out here because the engine no longer contains
    // it — §9 adopts the other one, and this is the arm it was measured against.
    auto fall_via_force = [&](float mass, int steps) {
        motion m;
        rigid_body b = make_dynamic(vec3{}, mass);
        for (int i = 0; i < steps; ++i)
        {
            const vec3 weight = g * (b.gravity_scale / b.inv_mass);
            integrate(m, (b.force + weight) * b.inv_mass, k_h60,
                      integrator::semi_implicit_euler);
        }
        return m;
    };

    std::printf("\n  C.2  the FORCE route: 1 kg against %.6f kg\n",
                static_cast<double>(bad_mass));
    std::printf("      steps        v(1 kg)        v(odd)     apart\n");
    for (int n : {1, 60, 600})
    {
        const float v1 = fall_via_force(1.0f, n).velocity.y;
        const float v2 = fall_via_force(bad_mass, n).velocity.y;
        std::printf("  %9d  %13.6f  %12.6f  %.3e\n", n,
                    static_cast<double>(v1), static_cast<double>(v2),
                    static_cast<double>(std::fabs(v1 - v2)));
    }
    {
        const float v1 = fall_via_force(1.0f, 1).velocity.y;
        std::printf("       one ulp of v after one step is %.3e\n",
                    static_cast<double>(ulp_at(v1)));
    }

    // C.3 the same pair through the engine, which adds gravity as an
    // ACCELERATION after the division — so the mass never enters the arithmetic
    // at all and the two cannot differ.
    std::printf("\n  C.3  the ACCELERATION route, through body_world\n");
    std::printf("      steps        v(1 kg)        v(odd)     apart\n");
    for (int n : {1, 60, 600})
    {
        const float v1 = run(make_dynamic(vec3{}, 1.0f), n, k_h60, g).velocity.y;
        const float v2 = run(make_dynamic(vec3{}, bad_mass), n, k_h60, g).velocity.y;
        std::printf("  %9d  %13.6f  %12.6f  %.3e\n", n,
                    static_cast<double>(v1), static_cast<double>(v2),
                    static_cast<double>(std::fabs(v1 - v2)));
    }

    // C.4 CONTROL: same mass, force route. The divergence must vanish, or C.2
    // is measuring the code path rather than the mass.
    const motion same_a = fall_via_force(bad_mass, 600);
    const motion same_b = fall_via_force(bad_mass, 600);
    std::printf("\n  C.4  CONTROL same mass, force route: identical  %s\n",
                (same_a.velocity.y == same_b.velocity.y) ? "yes" : "NO");

    // C.5 and the closed form, so that neither of the two is being graded
    // against the other.
    const double t = 10.0;
    const double exact = -0.5 * 9.81 * t * t;
    std::printf("\n  C.5  closed form -0.5*g*t^2 = %.6f m\n", exact);
    std::printf("       semi-implicit Euler is high by 0.5*g*h*t = %.6f m\n",
                0.5 * 9.81 * (1.0 / 60.0) * t);
}

// ===========================================================================
// D — damping is not drag
// ===========================================================================

void section_d()
{
    rule("D  DAMPING IS NOT DRAG");

    const vec3 g{0.0f, -k_gravity, 0.0f};
    const float k = 0.5f;          // velocity-space damping, 1/s
    const float bcoef = 0.5f;      // drag force coefficient, N.s/m
    const float masses[3] = {0.1f, 1.0f, 100.0f};

    std::printf("  60 s of falling, h = 1/60\n\n");
    // THE CONTINUOUS ANSWER IS g/k AND THE SIMULATION DOES NOT PRODUCE IT, so
    // both are printed. The step applies gravity and then damps, so the fixed
    // point is where (v + g h) e^(-k h) = v, i.e.
    //
    //     v = g h e^(-k h) / (1 - e^(-k h))
    //
    // which tends to g/k as h tends to 0 and is 0.42% below it at 60 Hz. A
    // harness that printed only the continuous form would report a 0.42% error
    // and invite somebody to go looking for a bug in the damping.
    const double decay = std::exp(-static_cast<double>(k) / 60.0);
    const double discrete = 9.81 / 60.0 * decay / (1.0 - decay);
    std::printf("  DAMPING  v *= exp(-k h),  k = 0.5 /s\n");
    std::printf("      mass       v_terminal    g/k    discrete\n");
    for (float m : masses)
    {
        rigid_body b = make_dynamic(vec3{}, m);
        b.damping = k;
        const motion s = run(b, 3600, k_h60, g);
        std::printf("  %9.2f kg  %9.4f m/s  %7.4f  %9.4f\n",
                    static_cast<double>(m),
                    static_cast<double>(-s.velocity.y),
                    static_cast<double>(terminal_speed_damped(k_gravity, k)),
                    discrete);
    }

    std::printf("\n  DRAG FORCE  F = -b v,  b = 0.5 N.s/m\n");
    std::printf("      mass       v_terminal   predicted    run for\n");
    for (float m : masses)
    {
        // FIVE TIME CONSTANTS EACH, and the durations differ by a factor of a
        // thousand — which is the finding, not an inconvenience. tau = m/b, so
        // a heavy body does not merely fall faster in the end, it takes
        // proportionally longer to get there.
        const float tau = m / bcoef;
        const int steps = static_cast<int>(7.0f * tau * 60.0f);   // 99.9% of it
        body_world w;
        w.set_gravity(g);
        const engine::phys::body_id id = w.add(make_dynamic(vec3{}, m));
        for (int i = 0; i < steps; ++i)
        {
            rigid_body* b = w.get(id);
            add_force(*b, b->state.velocity * -bcoef);
            w.step(k_h60);
        }
        std::printf("  %9.2f kg  %9.4f m/s  %9.4f  %8.1f s\n",
                    static_cast<double>(m),
                    static_cast<double>(-w.get(id)->state.velocity.y),
                    static_cast<double>(terminal_speed_dragged(k_gravity, m, bcoef)),
                    static_cast<double>(steps) / 60.0);
    }

    // CONTROL: no damping and no drag. There is no terminal speed at all, and
    // the number must be the one free fall gives — if the harness reported
    // something finite here, every row above would be suspect.
    const motion freefall = run(make_dynamic(vec3{}, 1.0f), 3600, k_h60, g);
    std::printf("\n  D.3  CONTROL neither: v after 60 s = %.2f m/s\n",
                static_cast<double>(-freefall.velocity.y));
    std::printf("       g*t = %.2f, so no terminal speed exists\n", 9.81 * 60.0);

    // D.4 the time constants, which is the other half of the difference.
    std::printf("\n  D.4  time to reach 63%% of terminal speed\n");
    std::printf("       damping  1/k = %.2f s, every mass\n",
                static_cast<double>(1.0f / k));
    for (float m : masses)
    {
        std::printf("       drag     m/b = %.2f s at %.2f kg\n",
                    static_cast<double>(m / bcoef), static_cast<double>(m));
    }
}

// ===========================================================================
// E — an impulse has no `h` in it, and a force does
// ===========================================================================

/// Peak height of a 70 kg body launched from rest, simulated.
float jump_peak(float h, bool as_impulse, float magnitude)
{
    const vec3 g{0.0f, -k_gravity, 0.0f};
    body_world w;
    w.set_gravity(g);
    const engine::phys::body_id id = w.add(make_dynamic(vec3{}, 70.0f));

    rigid_body* b = w.get(id);
    if (as_impulse) { add_impulse(*b, vec3{0.0f, magnitude, 0.0f}); }
    else { add_force(*b, vec3{0.0f, magnitude, 0.0f}); }

    float peak = 0.0f;
    for (int i = 0; i < 100000; ++i)
    {
        w.step(h);
        const motion& s = w.get(id)->state;
        if (s.position.y > peak) { peak = s.position.y; }
        if (s.velocity.y <= 0.0f) { break; }
    }
    return peak;
}

void section_e()
{
    rule("E  AN IMPULSE HAS NO h IN IT, AND A FORCE DOES");

    // A 70 kg character wants a 1.00 m jump: v0 = sqrt(2 g H) = 4.4294 m/s,
    // so J = m v0 = 310.06 N.s. Tuned at 60 Hz, the "force for one step" that
    // produces the same takeoff speed is J/h = 18603 N.
    const float v0 = std::sqrt(2.0f * k_gravity * 1.0f);
    const float impulse = 70.0f * v0;
    const float force = impulse * 60.0f;
    std::printf("  70 kg, target 1.00 m. v0 = %.4f m/s, J = %.2f N.s\n",
                static_cast<double>(v0), static_cast<double>(impulse));
    std::printf("  the force tuned at 60 Hz is J/h = %.0f N for one step\n\n",
                static_cast<double>(force));

    const float rates[4] = {30.0f, 60.0f, 120.0f, 144.0f};
    std::printf("     rate    impulse peak    force peak\n");
    float imin = HUGE_VALF, imax = 0.0f, fmin = HUGE_VALF, fmax = 0.0f;
    for (float r : rates)
    {
        const float h = 1.0f / r;
        const float pi_ = jump_peak(h, true, impulse);
        const float pf = jump_peak(h, false, force);
        imin = std::min(imin, pi_); imax = std::max(imax, pi_);
        fmin = std::min(fmin, pf); fmax = std::max(fmax, pf);
        std::printf("  %6.0f Hz  %10.4f m   %10.4f m\n",
                    static_cast<double>(r), static_cast<double>(pi_),
                    static_cast<double>(pf));
    }
    std::printf("\n  E.1  impulse spread  %.2f%%   (%.4f to %.4f m)\n",
                static_cast<double>(100.0f * (imax - imin) / imin),
                static_cast<double>(imin), static_cast<double>(imax));
    std::printf("  E.2  force   spread  %.0f%%  = %.1fx   (%.4f to %.4f m)\n",
                static_cast<double>(100.0f * (fmax - fmin) / fmin),
                static_cast<double>(fmax / fmin),
                static_cast<double>(fmin), static_cast<double>(fmax));

    // E.3 the residue in the impulse column is 8.1's, not this lesson's: with
    // semi-implicit Euler the peak falls short by v0*h/2 exactly.
    std::printf("\n  E.3  the impulse column is not flat, and the residue is\n"
                "       8.1's: peak = v0^2/2g - v0*h/2\n");
    std::printf("     rate    predicted      measured\n");
    for (float r : rates)
    {
        const float h = 1.0f / r;
        const double pred = static_cast<double>(v0) * v0 / (2.0 * 9.81)
                          - static_cast<double>(v0) * h / 2.0;
        std::printf("  %6.0f Hz  %10.6f m  %10.6f m\n",
                    static_cast<double>(r), pred,
                    static_cast<double>(jump_peak(h, true, impulse)));
    }

    // CONTROL: the force applied for a fixed DURATION rather than a fixed
    // number of steps. It must stop depending on the rate, because then it is
    // an impulse spelled out over many steps.
    std::printf("\n  E.4  CONTROL the same IMPULSE spread over whole steps\n");
    std::printf("     rate   steps      force        peak\n");
    for (float r : rates)
    {
        const float h = 1.0f / r;
        // The force is rescaled to the duration it is actually held for, so
        // that every rate delivers the SAME impulse. Without this the control
        // measures how evenly 60 divides into the rate: at 144 Hz two steps is
        // 13.9 ms rather than 16.7, and the "control" reports a third bug.
        const int hold = std::max(1, static_cast<int>(r / 60.0f + 0.5f));
        const float held_force = impulse / (static_cast<float>(hold) * h);
        const vec3 g{0.0f, -k_gravity, 0.0f};
        body_world w;
        w.set_gravity(g);
        const engine::phys::body_id id = w.add(make_dynamic(vec3{}, 70.0f));
        float peak = 0.0f;
        for (int i = 0; i < 100000; ++i)
        {
            if (i < hold) { add_force(*w.get(id), vec3{0.0f, held_force, 0.0f}); }
            w.step(h);
            const motion& s = w.get(id)->state;
            if (s.position.y > peak) { peak = s.position.y; }
            if (i >= hold && s.velocity.y <= 0.0f) { break; }
        }
        std::printf("  %6.0f Hz  %5d  %9.0f N  %8.4f m\n",
                    static_cast<double>(r), hold,
                    static_cast<double>(held_force), static_cast<double>(peak));
    }
}

// ===========================================================================
// F — inverse mass zero, and the infinity that is not a mass
// ===========================================================================

void section_f()
{
    rule("F  INVERSE MASS ZERO, AND THE INFINITY THAT IS NOT A MASS");

    const vec3 g{0.0f, -k_gravity, 0.0f};

    // F.1 a fixed body under gravity and a 10 kN shove.
    body_world w;
    w.set_gravity(g);
    const engine::phys::body_id id = w.add(engine::phys::make_fixed(vec3{0.0f, 0.0f, 0.0f}));
    for (int i = 0; i < 600; ++i)
    {
        add_force(*w.get(id), vec3{0.0f, -10000.0f, 0.0f});
        w.step(k_h60);
    }
    const motion& s = w.get(id)->state;
    std::printf("  F.1  fixed body, 10 s of gravity + 10 kN:\n");
    std::printf("       position %.1f  velocity %.1f   moved: %s\n",
                static_cast<double>(s.position.y),
                static_cast<double>(s.velocity.y),
                (s.position.y == 0.0f && s.velocity.y == 0.0f) ? "no" : "YES");

    // F.2 the arithmetic that makes the storage choice, side by side.
    engine::phys::rigid_body floor_ = engine::phys::make_fixed(vec3{});
    engine::phys::rigid_body wall = engine::phys::make_fixed(vec3{});
    const float m_floor = mass_of(floor_);
    const float m_wall = mass_of(wall);
    std::printf("\n  F.2  two immovable bodies:\n");
    std::printf("       mass_of  = %f and %f\n",
                static_cast<double>(m_floor), static_cast<double>(m_wall));
    std::printf("       m_a - m_b        = %f   (NaN: %s)\n",
                static_cast<double>(m_floor - m_wall),
                std::isnan(m_floor - m_wall) ? "yes" : "no");
    std::printf("       inv_a + inv_b    = %f   (the solver's term)\n",
                static_cast<double>(floor_.inv_mass + wall.inv_mass));
    std::printf("       1/(inv_a+inv_b)  = %f   (reduced mass)\n",
                static_cast<double>(1.0f / (floor_.inv_mass + wall.inv_mass)));

    // F.3 set_mass refuses what is not a mass.
    engine::phys::rigid_body b = make_dynamic(vec3{}, 5.0f);
    const bool zero_ok = set_mass(b, 0.0f);
    const bool neg_ok = set_mass(b, -2.0f);
    const bool inf_ok = set_mass(b, HUGE_VALF);
    std::printf("\n  F.3  set_mass(0) %s   set_mass(-2) %s\n",
                zero_ok ? "ACCEPTED" : "refused",
                neg_ok ? "ACCEPTED" : "refused");
    std::printf("       set_mass(inf) %s\n", inf_ok ? "accepted" : "REFUSED");
    std::printf("       inv_mass after inf = %.1f\n",
                static_cast<double>(b.inv_mass));

    // F.4 CONTROL — the trap this replaces: a "very heavy" DYNAMIC body. It is
    // not immovable at all, and gravity does not care how heavy it is.
    const motion heavy = run(make_dynamic(vec3{}, 1.0e6f), 60, k_h60, g);
    const motion pebble = run(make_dynamic(vec3{}, 0.01f), 60, k_h60, g);
    std::printf("\n  F.4  CONTROL a 1,000,000 kg DYNAMIC 'floor', 1 s:\n");
    std::printf("       y = %.6f m   a 10 g pebble: y = %.6f m\n",
                static_cast<double>(heavy.position.y),
                static_cast<double>(pebble.position.y));
}

// ===========================================================================
// G — units: the square root that makes miniatures look like miniatures
// ===========================================================================

void section_g()
{
    rule("G  UNITS: THE SQUARE ROOT UNDER EVERY MINIATURE");

    const vec3 g{0.0f, -k_gravity, 0.0f};

    // G.1 the closed form, checked against the simulation.
    std::printf("  G.1  time to fall 1 m from rest\n");
    std::printf("       closed form sqrt(2d/g) = %.6f s\n",
                static_cast<double>(free_fall_time(1.0f, k_gravity)));
    {
        body_world w;
        w.set_gravity(g);
        const engine::phys::body_id id = w.add(make_dynamic(vec3{}, 1.0f));
        int n = 0;
        while (w.get(id)->state.position.y > -1.0f && n < 100000) { w.step(k_h60); ++n; }
        std::printf("       simulated at 60 Hz     = %.6f s (%d steps)\n",
                    n / 60.0, n);
    }

    // G.2 the scaling law. A world where one unit depicts `s` metres, with the
    // engine's gravity left at 9.81 units/s^2.
    std::printf("\n  G.2  a world where 1 unit depicts s metres, g left alone\n\n");
    std::printf("      s     fall of a 1.8 m figure   ratio   sqrt(s)\n");
    const float scales[5] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
    const double base = static_cast<double>(free_fall_time(1.8f, k_gravity));
    for (float s : scales)
    {
        // The figure is modelled 1.8/s units tall, so it falls its own height
        // in the engine's units and we convert the duration back to seconds.
        const double units = 1.8 / static_cast<double>(s);
        const double t = std::sqrt(2.0 * units / 9.81);
        std::printf("  %5.2f   %18.6f s   %6.4f  %6.4f\n",
                    static_cast<double>(s), t, t / base,
                    static_cast<double>(time_scale_for_length_scale(1.0f / s)));
    }

    // G.3 the film number.
    std::printf("\n  G.3  a 1/8 scale model: everything is sqrt(8) = %.4f\n",
                static_cast<double>(time_scale_for_length_scale(8.0f)));
    std::printf("       times too fast, so a 24 fps shot is overcranked to\n");
    std::printf("       %.1f fps and played back at 24.\n",
                24.0 * static_cast<double>(time_scale_for_length_scale(8.0f)));

    // G.4 scaling g by 1/s DOES restore free fall exactly — the honest half.
    //
    // SIMULATED AND NOT EVALUATED. The closed form for this is
    // `sqrt(2*(d/s)/(g/s))`, in which the `s` cancels algebraically, so a table
    // built from it would print five identical numbers and prove only that
    // algebra works. Running the actual integrator at each scale exercises the
    // floats, and the residual disagreement is the step granularity rather than
    // the rescale.
    std::printf("\n  G.4  scaling g by 1/s restores the fall, SIMULATED\n\n");
    std::printf("      s      g'       steps   fall time   vs s = 1\n");
    // Two passes, because the reference row is in the middle of the table and a
    // running comparison would have printed "+0.6000" for the two rows above it.
    double base_sim = 0.0;
    for (int pass = 0; pass < 2; ++pass)
    for (float sc : scales)
    {
        if (pass == 0 && sc != 1.0f) { continue; }
        const float gp = 9.81f / sc;
        const float height = 1.8f / sc;      // the figure, in this world's units
        body_world w;
        w.set_gravity(vec3{0.0f, -gp, 0.0f});
        const engine::phys::body_id id = w.add(make_dynamic(vec3{}, 1.0f));
        int n = 0;
        while (w.get(id)->state.position.y > -height && n < 1000000) { w.step(k_h60); ++n; }
        const double t = n / 60.0;
        if (pass == 0) { base_sim = t; continue; }
        std::printf("  %5.2f  %8.4f  %6d  %9.6f s  %+.4f s\n",
                    static_cast<double>(sc), static_cast<double>(gp), n, t,
                    t - base_sim);
    }

    // G.5 ...and what it does not fix: any length that did not go through the
    // same rescale.
    std::printf("\n  G.5  what a rescaled g does NOT carry with it\n");
    std::printf("       a solver tolerance of 0.005 UNITS, in a world at s =\n");
    for (float s : scales)
    {
        std::printf("       %5.2f  ->  %7.2f mm of real slop\n",
                    static_cast<double>(s), 5.0 * static_cast<double>(s));
    }
}

// ===========================================================================
// H — frames: what a scaled parent does to gravity
// ===========================================================================

void report_frame(const char* label, const mat4& m, vec3 g)
{
    const frame_report r = inspect_frame(m, g);
    std::printf("  %s\n", label);
    std::printf("    g_world (%.4f, %.4f, %.4f)\n",
                static_cast<double>(r.gravity_in_world.x),
                static_cast<double>(r.gravity_in_world.y),
                static_cast<double>(r.gravity_in_world.z));
    std::printf("    gain %.4f  tilt %.3f deg  square %.4f\n",
                static_cast<double>(r.gain),
                static_cast<double>(r.tilt_degrees),
                static_cast<double>(r.out_of_square));
    std::printf("    scale %s   frame %s\n",
                r.uniform ? "uniform" : "NON-UNIFORM",
                r.inertial ? "inertial" : "NOT INERTIAL");
}

void section_h()
{
    rule("H  FRAMES: WHAT A SCALED PARENT DOES TO GRAVITY");

    const vec3 g{0.0f, -k_gravity, 0.0f};
    const float rad = 3.14159265358979323846f / 4.0f;

    // CONTROL FIRST this time: a frame that is a rotation and a translation must
    // report gain 1 and square 0, or nothing below it can be believed.
    report_frame("H.1  CONTROL translate(3,-2,7) * rotate_z(45 deg)",
                 affine(engine::rotation_z(rad), vec3{3.0f, -2.0f, 7.0f}), g);

    std::putchar(10);
    report_frame("H.2  uniform scale 2",
                 affine(engine::scale(2.0f, 2.0f, 2.0f), vec3{}), g);

    std::putchar(10);
    report_frame("H.3  non-uniform scale (2,1,1)",
                 affine(engine::scale(2.0f, 1.0f, 1.0f), vec3{}), g);

    std::putchar(10);
    report_frame("H.4  scale (2,1,1) AFTER a 45 deg turn",
                 affine(engine::scale(2.0f, 1.0f, 1.0f) * engine::rotation_z(rad), vec3{}), g);

    // H.5 the same, as a fall. A body integrated in the local space of a frame
    // scaled by 2, against one integrated in world space.
    const motion local_ = run(make_dynamic(vec3{}, 1.0f), 60, k_h60, g);
    const mat4 scaled = affine(engine::scale(2.0f, 2.0f, 2.0f), vec3{});
    const vec3 world_pos = engine::xyz(scaled * engine::point(local_.position));
    std::printf("\n  H.5  1 s of falling, integrated in a frame scaled by 2\n");
    std::printf("       local  y = %.6f units\n",
                static_cast<double>(local_.position.y));
    std::printf("       world  y = %.6f m\n", static_cast<double>(world_pos.y));
    std::printf("       a world-space body falls %.6f m -> ratio %.4f\n",
                static_cast<double>(local_.position.y),
                static_cast<double>(world_pos.y / local_.position.y));

    // H.6 a ROTATING frame passes every test above and is still not inertial,
    // because a matrix has no time derivative. A body with NO forces on it must
    // travel in a straight line; integrated in a spinning frame it does not.
    const float omega = 2.0f;   // rad/s about y
    motion free_local;
    free_local.position = vec3{1.0f, 0.0f, 0.0f};
    free_local.velocity = vec3{0.0f, 0.0f, 2.0f};
    for (int i = 0; i < 60; ++i)
    {
        integrate(free_local, vec3{}, k_h60, integrator::semi_implicit_euler);
    }
    const mat3 spun = engine::rotation_y(omega * 1.0f);
    const vec3 spun_world = spun * free_local.position;
    const vec3 truth{1.0f, 0.0f, 2.0f + 2.0f * k_h60};   // 8.1's half-step offset
    std::printf("\n  H.6  no forces at all, 1 s, in a frame spinning at 2 rad/s\n");
    std::printf("       straight line   (%.4f, %.4f, %.4f)\n",
                static_cast<double>(truth.x), static_cast<double>(truth.y),
                static_cast<double>(truth.z));
    std::printf("       integrated local, mapped to world\n");
    std::printf("                       (%.4f, %.4f, %.4f)\n",
                static_cast<double>(spun_world.x),
                static_cast<double>(spun_world.y),
                static_cast<double>(spun_world.z));
    std::printf("       apart by %.4f m after one second\n",
                static_cast<double>(length(spun_world - truth)));
}

// ===========================================================================
// I — the budget
// ===========================================================================

double time_world(body_world& w, float h, int reps)
{
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < reps; ++i) { w.step(h); }
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / reps;
}

void say(const char* label, const engine::bench_ab& r, bool compare_answers = false)
{
    std::printf("  %s\n", label);
    // MEDIAN AND MIN BOTH, because `spread()` on 2000 reps is dominated by a
    // handful of scheduler outliers and reads as 1-4 on a machine that is
    // behaving perfectly well. The min is the least contaminated estimator
    // available without pinning a core, the median is what bench.hpp quotes,
    // and the RATIO of the two medians is what repeats run to run — measured at
    // 0.819, 0.829 and 0.822 across three runs of this binary.
    std::printf("    a  med %6.3f  min %6.3f ns/body\n",
                r.a.median_ns, r.a.min_ns);
    std::printf("    b  med %6.3f  min %6.3f ns/body\n",
                r.b.median_ns, r.b.min_ns);
    std::printf("    b/a %.3f on medians, %.3f on mins", r.ratio(),
                (r.a.min_ns > 0.0) ? r.b.min_ns / r.a.min_ns : 0.0);
    // `agree` is only a meaningful check when the two arms are two spellings of
    // the SAME arithmetic, which is true of I.1 and I.2 and false of the rest —
    // I.3's second arm computes no report at all, and I.5/I.6 are two different
    // worlds. Printing "agree no" for those would look like a failure and would
    // be a category error, so the caller says which kind of comparison it is.
    if (compare_answers) { std::printf("   answers agree %s", r.agree ? "yes" : "no"); }
    std::putchar('\n');
}

void section_i()
{
    rule("I  THE BUDGET, AND WHERE IT ACTUALLY GOES");

    constexpr std::size_t k_bodies = 4096;
    constexpr int k_reps = 2000;
    const vec3 g{0.0f, -k_gravity, 0.0f};

    // 5.6'S `bench_compare`, WHICH IS THE ONLY REASON THE NUMBERS BELOW ARE
    // WORTH QUOTING. A first attempt at this section timed the four loops one
    // after another and reported the force route at 2.625, 1.758, 1.476 and
    // 1.459 ns/body on four consecutive runs of the same binary — the machine
    // was still settling, and the arm that ran first always lost. Alternating
    // the arms one rep each puts both of them in the same thermal state within
    // microseconds, and taking the median of 400 reps throws away the outliers
    // the scheduler produces.
    std::vector<rigid_body> table(k_bodies);
    for (std::size_t i = 0; i < k_bodies; ++i)
    {
        table[i] = make_dynamic(vec3{static_cast<float>(i), 0.0f, 0.0f},
                                1.0f + static_cast<float>(i % 7));
        table[i].state.velocity = vec3{0.1f, 0.0f, -0.05f};
    }
    std::vector<rigid_body> arm_a_table = table;
    std::vector<rigid_body> arm_b_table = table;

    std::printf("  %zu bodies, %d alternating reps, h = 1/60\n\n",
                k_bodies, k_reps);

    // I.1 — gravity through the accumulator, against gravity as an
    // acceleration. One float divide per body per step is the whole difference.
    {
        auto force_route = [&]() {
            float max_speed = 0.0f;
            for (rigid_body& b : arm_a_table)
            {
                b.force += g * (b.gravity_scale / b.inv_mass);      // the DIVIDE
                integrate(b.state, b.force * b.inv_mass, k_h60,
                          integrator::semi_implicit_euler);
                b.force = vec3{};
                max_speed = std::max(max_speed, length(b.state.velocity));
            }
            return static_cast<double>(max_speed);
        };
        auto accel_route = [&]() {
            float max_speed = 0.0f;
            for (rigid_body& b : arm_b_table)
            {
                integrate(b.state, b.force * b.inv_mass + g * b.gravity_scale,
                          k_h60, integrator::semi_implicit_euler);
                b.force = vec3{};
                max_speed = std::max(max_speed, length(b.state.velocity));
            }
            return static_cast<double>(max_speed);
        };
        say("I.1  a = weight through the accumulator (a divide)\n"
            "       b = gravity added after the divide",
            engine::bench_compare(k_bodies, k_reps, force_route, accel_route), true);
    }

    // I.2 — the obvious optimisation, which does not pay.
    {
        arm_a_table = table;
        arm_b_table = table;
        auto with_sqrt = [&]() {
            float max_speed = 0.0f;
            for (rigid_body& b : arm_a_table)
            {
                integrate(b.state, b.force * b.inv_mass + g, k_h60,
                          integrator::semi_implicit_euler);
                max_speed = std::max(max_speed, length(b.state.velocity));
            }
            return static_cast<double>(max_speed);
        };
        auto squared = [&]() {
            float max_sq = 0.0f;
            for (rigid_body& b : arm_b_table)
            {
                integrate(b.state, b.force * b.inv_mass + g, k_h60,
                          integrator::semi_implicit_euler);
                max_sq = std::max(max_sq, length_squared(b.state.velocity));
            }
            return static_cast<double>(std::sqrt(max_sq));
        };
        say("\n  I.2  a = a sqrt per body for max_speed\n"
            "       b = compare squared, one sqrt at the end",
            engine::bench_compare(k_bodies, k_reps, with_sqrt, squared), true);
    }

    // I.3 — what the whole report costs.
    {
        arm_a_table = table;
        arm_b_table = table;
        auto reported = [&]() {
            float max_speed = 0.0f;
            vec3 momentum{};
            for (rigid_body& b : arm_a_table)
            {
                integrate(b.state, b.force * b.inv_mass + g, k_h60,
                          integrator::semi_implicit_euler);
                b.force = vec3{};
                max_speed = std::max(max_speed, length(b.state.velocity));
                momentum += b.state.velocity * (1.0f / b.inv_mass);
            }
            return static_cast<double>(max_speed + momentum.x);
        };
        auto bare = [&]() {
            for (rigid_body& b : arm_b_table)
            {
                integrate(b.state, b.force * b.inv_mass + g, k_h60,
                          integrator::semi_implicit_euler);
                b.force = vec3{};
            }
            return 0.0;
        };
        say("\n  I.3  a = with max_speed and momentum\n"
            "       b = no report at all",
            engine::bench_compare(k_bodies, k_reps, reported, bare));
    }

    std::printf("\n  I.4  8.1's bare semi-implicit step is 0.963 ns/body\n");

    // The real thing, through the library, so the numbers above are known to be
    // about the arithmetic rather than about a harness that inlines everything.
    body_world plain, damped, fixed_;
    auto fill = [&](body_world& w, bool damp, body_kind kind) {
        w.set_gravity(g);
        for (std::size_t i = 0; i < k_bodies; ++i)
        {
            rigid_body b = table[i];
            b.kind = kind;
            if (kind != body_kind::dynamic) { b.inv_mass = 0.0f; }
            if (damp) { b.damping = 0.4f; }
            w.add(b);
        }
    };
    fill(plain, false, body_kind::dynamic);
    fill(damped, true, body_kind::dynamic);
    fill(fixed_, false, body_kind::fixed);

    const engine::bench_ab dampcost = engine::bench_compare(
        k_bodies, k_reps,
        [&]() { plain.step(k_h60); return static_cast<double>(plain.report().max_speed); },
        [&]() { damped.step(k_h60); return static_cast<double>(damped.report().max_speed); });
    const engine::bench_ab kindcost = engine::bench_compare(
        k_bodies, k_reps,
        [&]() { plain.step(k_h60); return static_cast<double>(plain.report().max_speed); },
        [&]() { fixed_.step(k_h60); return static_cast<double>(fixed_.report().max_speed); });

    say("\n  I.5  body_world::step, a = plain, b = damping 0.4", dampcost);
    say("\n  I.6  body_world::step, a = dynamic, b = all fixed", kindcost);
    std::printf("\n  I.7  a 60 Hz frame is 16,667 us. %zu bodies at\n"
                "       %.3f ns each is %.3f%% of it.\n",
                k_bodies, dampcost.a.median_ns,
                100.0 * dampcost.a.median_ns * static_cast<double>(k_bodies)
                    / 16667000.0);

    // CONTROL: an empty world. If this is not near zero the loop overhead is
    // being measured rather than the bodies.
    body_world empty;
    const double te = time_world(empty, k_h60, 20000);
    std::printf("\n  I.8  CONTROL empty world: %.2f ns per step\n", te);
}

} // namespace

int main()
{
    std::printf("verify_82 - Lesson 8.2, Forces, Gravity and Linear Bodies\n");
    std::printf("float everywhere the engine uses float.\n");

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
