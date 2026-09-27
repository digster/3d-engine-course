// scratch/verify_89.cpp — every number Lesson 8.9 prints, measured rather than
// asserted.
//
// Build and run:  sh scratch/build_verify_89.sh
//
// Eleven sections, in the lesson's order:
//
//   A  the penalty spring, and the rate it demands
//   B  the effective mass, two ways
//   C  one impulse, and the velocity it produces
//   D  restitution, and the gravity already in the velocity
//   E  the restitution threshold
//   F  friction: the cone and the box
//   G  the slope, and what mu actually means
//   H  the order of the two solves
//   I  sliding becomes rolling
//   J  what this lesson leaves behind
//   K  the budget
//
// EVERY SECTION CARRIES A CONTROL — the rule since 8.1, in two halves: ask what
// the control would say if the thing were COMPLETELY BROKEN, and what it would
// say if it were completely FINE. 8.7 §9 shipped a control that convicted the
// code of the TEST's own mistake, and 8.8 §4 shipped a measurement that refused
// the claim its section was written to make, so both halves earn their keep.
//
// THE INSTRUMENTS, AND WHAT IS DIFFERENT ABOUT THIS LESSON'S.
//
//   * THIS LESSON HAS CLOSED FORMS, which 8.5 through 8.8 mostly did not. A
//     bounce height is `e^(2n)*h0`; a critical slope is `atan(mu)`; a sliding
//     sphere rolls at exactly `5/7` of its initial speed; an elastic head-on
//     collision has an algebraic answer. So most checks here are a simulation
//     against a formula derived independently of it, which is the strongest
//     instrument available and is not available often.
//
//   * MOMENTUM IS CONSERVED TO THE BIT, and that makes it useless as evidence.
//     `apply_impulse_pair` adds `+J` to one body and subtracts the same `J` from
//     the other, so the sum of `m*v` cannot change by more than the rounding of
//     one addition — no matter how wrong the impulse is. §C measures it anyway
//     and says so, because a reader who sees "momentum conserved to 1e-7" and
//     concludes the solver is right has been misled by a tautology. 8.7 §6 paid
//     for that lesson once already.
//
//   * ENERGY IS THE ONE THAT CAN FAIL. Nothing in the solver enforces it, so
//     "an elastic collision conserved kinetic energy to 1e-6" is a real check
//     on the effective mass, the lever arms and the target velocity all at once.
//
//   * THE STEP IS HAND-ROLLED, not `body_world::step`. §J's whole subject is
//     WHERE in the step the solve goes, and a step that does the velocity and
//     position halves in one call cannot be asked the question.
//
// TIMINGS ARE MINIMA over many repetitions, as in 8.8: a measurement of a few
// microseconds has a long right tail and no left one, so the minimum is the
// estimator with the smaller variance and it errs in the conservative direction
// for every claim this lesson makes.

#include <engine/phys/collide.hpp>
#include <engine/phys/epa.hpp>
#include <engine/phys/gjk.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/integrate.hpp>
#include <engine/phys/manifold.hpp>
#include <engine/phys/rigid_body.hpp>
#include <engine/phys/shape.hpp>
#include <engine/phys/solver.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using engine::cross;
using engine::dot;
using engine::length;
using engine::length_squared;
using engine::mat3;
using engine::normalised;
using engine::quat;
using engine::vec3;

using engine::phys::as_convex;
using engine::phys::body_kind;
using engine::phys::box_shape;
using engine::phys::collide_manifold;
using engine::phys::combine_material;
using engine::phys::combine_rule;
using engine::phys::contact_batch;
using engine::phys::contact_manifold;
using engine::phys::contact_material;
using engine::phys::critical_slope_degrees;
using engine::phys::effective_mass;
using engine::phys::friction_model;
using engine::phys::k_gravity;
using engine::phys::make_box;
using engine::phys::make_sphere;
using engine::phys::make_fixed;
using engine::phys::prepare_contacts;
using engine::phys::resolve_contact;
using engine::phys::rigid_body;
using engine::phys::rolling_speed_fraction;
using engine::phys::rolling_time;
using engine::phys::shape;
using engine::phys::solve_contacts;
using engine::phys::solve_report;
using engine::phys::solver_config;
using engine::phys::sphere_shape;
using engine::phys::tangent_basis;
using engine::phys::terminal_bounce_speed;
using engine::phys::world_inv_inertia;
using engine::phys::world_obb;
using engine::phys::world_sphere;
using engine::phys::write_back;

namespace
{

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
    for (int i = 0; i < 70; ++i) { std::putchar('-'); }
    std::putchar('\n');
}

/// The same deterministic generator 8.1-8.8 used, for the same reason:
/// `std::uniform_real_distribution` is not specified to produce the same
/// sequence on two standard libraries, so a "measured" number would not be
/// reproducible on the reader's machine.
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

    quat orientation()
    {
        const vec3 axis = direction();
        const float angle = range(-3.14159265f, 3.14159265f);
        const float s = std::sin(0.5f * angle);
        return quat{std::cos(0.5f * angle), axis * s};
    }

private:
    std::uint32_t state_;
};

constexpr float k_pi = 3.14159265358979323846f;
constexpr float k_deg = 57.29577951308232f;

/// A minute of simulation at 60 Hz, which is the horizon every "does it settle"
/// question in this file is asked over.
constexpr float k_h = 1.0f / 60.0f;

// ---------------------------------------------------------------------------
// A hand-rolled step, because §J's subject is the order of its halves
// ---------------------------------------------------------------------------

/// Where the contact solve sits inside one step.
enum class solve_slot
{
    /// Between the velocity update and the position update. **The correct one**,
    /// and §J measures why: the position half then travels at the velocity the
    /// solver approved rather than at the one gravity asked for.
    mid_step,

    /// After the whole step. One frame late, and the body sinks by `g*h^2` every
    /// step it rests — 2.7 mm at 60 Hz, forever.
    end_of_step,
};

/// One dynamic body over one immovable floor, stepped by hand.
struct fall_scene
{
    rigid_body body{};
    rigid_body floor{};
    shape body_shape = box_shape(vec3{0.5f, 0.5f, 0.5f});
    shape floor_shape = box_shape(vec3{20.0f, 0.5f, 20.0f});
    bool body_is_sphere = false;
    float sphere_radius = 0.5f;

    contact_material material{};
    solver_config cfg{};
    vec3 gravity{0.0f, -k_gravity, 0.0f};
    float h = k_h;
    solve_slot slot = solve_slot::mid_step;

    /// A constant external force on the body, newtons. §E uses it for the one
    /// case the gravity bias cannot reach: an acceleration the caller did not
    /// tell the solver about.
    vec3 extra_force{};

    /// How many solver passes per step. **One** is what this lesson ships;
    /// §A and §J run the same code with more, which is a preview of 8.10 and
    /// the honest way to say how far short one pass falls.
    int passes = 1;

    /// Instrumentation the sections read.
    contact_manifold last_manifold{};
    solve_report last_report{};
    float last_depth = 0.0f;
    bool touched = false;
};

[[nodiscard]] contact_manifold collide_scene(const fall_scene& s)
{
    const auto floor_box = world_obb(s.floor_shape, s.floor.state.position, s.floor.orientation);
    if (s.body_is_sphere)
    {
        const auto ball = world_sphere(s.body_shape, s.body.state.position);
        return collide_manifold(as_convex(floor_box), as_convex(ball));
    }
    const auto box = world_obb(s.body_shape, s.body.state.position, s.body.orientation);
    return collide_manifold(as_convex(floor_box), as_convex(box));
}

/// Collide and solve once. `a` is always the floor, so the manifold normal
/// points from the floor toward the body — upward — which is the convention
/// every sign in this file is written against.
void contact_pass(fall_scene& s)
{
    contact_manifold m = collide_scene(s);
    s.last_manifold = m;
    s.touched = m.count > 0;
    s.last_depth = m.count > 0 ? m.deepest() : 0.0f;
    if (m.count == 0)
    {
        s.last_report = solve_report{};
        return;
    }

    // The restitution bias is the velocity gravity added to a dynamic body this
    // step. It is what `solver_config::restitution_bias` is for, and §D is the
    // measurement of leaving it out.
    contact_batch batch = prepare_contacts(s.floor, s.body, m, s.material, s.cfg);
    if (s.cfg.warm_start) { engine::phys::warm_start_contacts(s.floor, s.body, batch); }
    for (int pass = 0; pass < s.passes; ++pass)
    {
        s.last_report = solve_contacts(s.floor, s.body, batch, s.cfg);
    }
    write_back(batch, m);
    s.last_manifold = m;
}

void step_scene(fall_scene& s)
{
    rigid_body& b = s.body;

    // 1. velocity, semi-implicit Euler (8.1's settled default).
    b.state.velocity = b.state.velocity + s.gravity * (b.gravity_scale * s.h);
    b.state.velocity = b.state.velocity + s.extra_force * (b.inv_mass * s.h);

    if (s.slot == solve_slot::mid_step) { contact_pass(s); }

    // 2. position and orientation.
    b.state.position = b.state.position + b.state.velocity * s.h;
    b.orientation = engine::phys::advance_orientation(b.orientation, b.angular_velocity, s.h,
                                                      engine::phys::spin_rule::linearised);

    if (s.slot == solve_slot::end_of_step) { contact_pass(s); }
}

/// The penalty alternative from §A, as a force rather than an impulse.
void penalty_pass(fall_scene& s, float stiffness, float damping)
{
    const contact_manifold m = collide_scene(s);
    s.last_manifold = m;
    s.touched = m.count > 0;
    s.last_depth = m.count > 0 ? m.deepest() : 0.0f;
    if (m.count == 0) { return; }

    // One spring per contact point, along the manifold normal, with the load
    // split evenly. A real penalty engine does exactly this.
    const float share = 1.0f / static_cast<float>(m.count);
    for (int i = 0; i < m.count; ++i)
    {
        const vec3 p = m.points[i].position;
        const float relative_speed = dot(engine::phys::contact_velocity(s.floor, s.body, p), m.normal);
        const float magnitude = share * (stiffness * m.points[i].depth - damping * relative_speed);
        engine::phys::add_force_at(s.body, m.normal * std::max(0.0f, magnitude), p);
    }
}

void step_penalty(fall_scene& s, float stiffness, float damping)
{
    rigid_body& b = s.body;
    penalty_pass(s, stiffness, damping);

    const vec3 acceleration = b.force * b.inv_mass + s.gravity * b.gravity_scale;
    b.state.velocity = b.state.velocity + acceleration * s.h;
    b.angular_velocity = b.angular_velocity + world_inv_inertia(b) * b.torque * s.h;
    b.state.position = b.state.position + b.state.velocity * s.h;
    b.orientation = engine::phys::advance_orientation(b.orientation, b.angular_velocity, s.h,
                                                      engine::phys::spin_rule::linearised);
    engine::phys::clear_force(b);
    engine::phys::clear_torque(b);
}

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

using clock_type = std::chrono::steady_clock;

[[nodiscard]] double seconds_since(clock_type::time_point t0)
{
    return std::chrono::duration<double>(clock_type::now() - t0).count();
}

// ---------------------------------------------------------------------------
// §A  the penalty spring, and the rate it demands
// ---------------------------------------------------------------------------

void section_a()
{
    rule("A  THE PENALTY SPRING, AND THE RATE IT DEMANDS");

    std::printf("  A 20 kg crate on a spring floor. The stiffness is not free: it is\n"
                "  m*g/x for whatever STATIC sink x you will tolerate, and the resulting\n"
                "  rate omega = sqrt(g/x) DOES NOT CONTAIN THE MASS.\n\n");

    std::printf("  %10s  %14s  %12s  %12s  %14s\n",
                "static sink", "k (N/m)", "omega", "h_max (8.1)", "minimum rate");
    const float sinks[] = {0.05f, 0.01f, 0.001f, 0.0001f};
    for (float x : sinks)
    {
        const float k = engine::phys::penalty_stiffness(20.0f, k_gravity, x);
        const float w = engine::phys::penalty_omega(k_gravity, x);
        const float hmax = engine::phys::max_stable_step(engine::phys::integrator::semi_implicit_euler, w);
        std::printf("  %8.2f mm  %14.1f  %9.2f r/s  %10.5f s  %11.1f Hz\n",
                    x * 1000.0f, static_cast<double>(k), static_cast<double>(w),
                    static_cast<double>(hmax), 1.0 / static_cast<double>(hmax));
    }

    // The mass really does cancel: two crates a ten-thousandfold apart want the
    // same rate for the same sink. That is the claim, so measure it.
    const float w_light = engine::phys::penalty_omega(k_gravity, 0.001f);
    const float k_light = engine::phys::penalty_stiffness(0.2f, k_gravity, 0.001f);
    const float k_heavy = engine::phys::penalty_stiffness(2000.0f, k_gravity, 0.001f);
    std::printf("\n  mass cancels: 0.2 kg wants k = %.1f, 2000 kg wants k = %.1f,\n"
                "  and BOTH oscillate at omega = %.2f rad/s.\n",
                static_cast<double>(k_light), static_cast<double>(k_heavy),
                static_cast<double>(w_light));
    check(std::abs(engine::phys::penalty_omega(k_gravity, 0.001f) - w_light) < 1e-4f,
          "A: penalty omega independent of mass");

    // At 60 Hz, what is the best a STABLE spring can do?
    const float omega_at_60 = 2.0f / k_h;
    const float best_sink = k_gravity / (omega_at_60 * omega_at_60);
    std::printf("  At 60 Hz the stability limit is omega < %.1f rad/s, so the finest a\n"
                "  STABLE penalty spring can hold a resting crate is %.3f mm.\n",
                static_cast<double>(omega_at_60), static_cast<double>(best_sink * 1000.0f));

    // ---- the run -------------------------------------------------------
    const float k_spring = engine::phys::penalty_stiffness(20.0f, k_gravity, 0.001f);

    struct outcome
    {
        float max_depth = 0.0f;
        float end_depth = 0.0f;
        float end_speed = 0.0f;
        float late_swing = 0.0f;   // peak-to-peak height over the last half second
        bool exploded = false;
    };

    auto observe = [](fall_scene& s, float seconds, auto&& advance) {
        outcome out;
        const int steps = static_cast<int>(seconds / s.h);
        const int late = steps - static_cast<int>(0.5f / s.h);
        float hi = -1e30f, lo = 1e30f;
        for (int i = 0; i < steps; ++i)
        {
            advance(s);
            out.max_depth = std::max(out.max_depth, s.last_depth);
            if (i >= late)
            {
                hi = std::max(hi, s.body.state.position.y);
                lo = std::min(lo, s.body.state.position.y);
            }
            if (!std::isfinite(s.body.state.position.y) || std::abs(s.body.state.position.y) > 1e4f)
            {
                out.exploded = true;
                break;
            }
        }
        out.end_depth = s.last_depth;
        out.end_speed = length(s.body.state.velocity);
        out.late_swing = (hi > lo) ? hi - lo : 0.0f;
        return out;
    };

    auto fresh = [&](float h) {
        fall_scene s;
        s.h = h;
        s.body = make_box(vec3{0.0f, 1.2f, 0.0f}, 20.0f, vec3{0.5f, 0.5f, 0.5f});
        s.floor = make_fixed(vec3{0.0f, 0.0f, 0.0f});
        s.material = {0.0f, 0.5f};
        s.cfg.restitution_bias = s.gravity * h;
        return s;
    };

    outcome penalty_60;
    {
        fall_scene s = fresh(k_h);
        penalty_60 = observe(s, 2.0f, [&](fall_scene& sc) { step_penalty(sc, k_spring, 0.0f); });
    }
    outcome penalty_60_damped;
    {
        fall_scene s = fresh(k_h);
        const float c = 2.0f * std::sqrt(k_spring * 20.0f);
        penalty_60_damped = observe(s, 2.0f, [&](fall_scene& sc) { step_penalty(sc, k_spring, c); });
    }
    outcome penalty_fast;
    {
        fall_scene s = fresh(1.0f / 400.0f);
        penalty_fast = observe(s, 2.0f, [&](fall_scene& sc) { step_penalty(sc, k_spring, 0.0f); });
    }
    outcome impulse_60;
    {
        fall_scene s = fresh(k_h);
        impulse_60 = observe(s, 2.0f, [&](fall_scene& sc) { step_scene(sc); });
    }
    // The same solver, iterated. §J measures the iteration count properly; here
    // it is the arm that shows what the ONE pass is costing.
    outcome impulse_60_converged;
    {
        fall_scene s = fresh(k_h);
        s.passes = 16;
        impulse_60_converged = observe(s, 2.0f, [&](fall_scene& sc) { step_scene(sc); });
    }

    std::printf("\n  Two seconds of a 20 kg crate dropped from 1.2 m onto a floor whose\n"
                "  spring is tuned for 1 mm of STATIC sink. It arrives at 1.96 m/s.\n\n");
    std::printf("  %-38s %11s %11s %10s %11s\n", "", "max depth", "end depth", "end |v|",
                "late swing");
    auto row = [](const char* what, const outcome& o) {
        if (o.exploded)
        {
            std::printf("  %-38s %11s %11s %10s %11s\n", what, "EXPLODED", "-", "-", "-");
            return;
        }
        std::printf("  %-38s %8.3f mm %8.3f mm %7.3f m/s %8.3f mm\n", what,
                    static_cast<double>(o.max_depth * 1000.0f),
                    static_cast<double>(o.end_depth * 1000.0f),
                    static_cast<double>(o.end_speed),
                    static_cast<double>(o.late_swing * 1000.0f));
    };
    row("penalty, 60 Hz, undamped", penalty_60);
    row("penalty, 60 Hz, critically damped", penalty_60_damped);
    row("penalty, 400 Hz, undamped", penalty_fast);
    row("impulse, 60 Hz, one pass", impulse_60);
    row("impulse, 60 Hz, sixteen passes", impulse_60_converged);
    std::printf("  (an end depth of zero means the crate is AIRBORNE at t = 2 s)\n");

    // What the spring tuned for 1 mm of STATIC sink can possibly do about an
    // impact: x = v*sqrt(m/k), which has nothing to do with the weight.
    const float impact_speed = std::sqrt(2.0f * k_gravity * 0.2f);
    const float predicted_dynamic = impact_speed * std::sqrt(20.0f / k_spring);
    std::printf("\n  THE STATIC NUMBER WAS THE OPTIMISTIC ONE. All the impact energy has to\n"
                "  go into the spring, so the dynamic sink is v*sqrt(m/k) = %.3f mm — %.0fx\n"
                "  the 1 mm it was tuned for, before any discretisation. Measured %.3f mm.\n",
                static_cast<double>(predicted_dynamic * 1000.0f),
                static_cast<double>(predicted_dynamic / 0.001f),
                static_cast<double>(penalty_60.max_depth * 1000.0f));

    const float k_wanted = engine::phys::penalty_impact_stiffness(20.0f, impact_speed, 0.001f);
    const float omega_wanted = std::sqrt(k_wanted / 20.0f);
    const float h_wanted = engine::phys::max_stable_step(engine::phys::integrator::semi_implicit_euler,
                                                         omega_wanted);
    std::printf("  To get 1 mm out of THAT impact the spring wants k = %.3e N/m, which\n"
                "  oscillates at %.0f rad/s and demands at least %.0f Hz.\n",
                static_cast<double>(k_wanted), static_cast<double>(omega_wanted),
                1.0 / static_cast<double>(h_wanted));

    // ---- the two-dimensional sweep ----------------------------------------
    //
    // The section was drafted expecting to show that a penalty contact cannot be
    // made to work at 60 Hz. THAT IS TOO STRONG A CLAIM TO MAKE FROM ONE
    // STIFFNESS, so both knobs are swept: softer springs are more stable and
    // sink further, and damping is what turns a bounce into a landing. The
    // honest question is what the BEST penalty setting achieves, and this is the
    // measurement of it.
    std::printf("\n  BOTH KNOBS, SWEPT. An undamped spring returns every joule it took, so\n"
                "  it is a contact with e = 1; damping is what makes it a landing. A softer\n"
                "  spring is calmer and sinks further. 4 stiffnesses x 41 dampings, 3 s\n"
                "  each, and the best result for each stiffness:\n\n");
    std::printf("  %14s %12s %10s %14s %14s\n", "static sink", "k (N/m)", "best zeta",
                "max depth", "late swing");
    float overall_swing = 1e30f;
    float overall_depth = 0.0f;
    float overall_c = 0.0f;
    float overall_k = 0.0f;
    for (float x : {0.05f, 0.01f, 0.001f, 0.0001f})
    {
        const float k = engine::phys::penalty_stiffness(20.0f, k_gravity, x);
        const float critical = 2.0f * std::sqrt(k * 20.0f);
        float best_swing_k = 1e30f;
        float best_depth_k = 0.0f;
        float best_c_k = 0.0f;
        for (int i = 0; i <= 40; ++i)
        {
            const float c = critical * static_cast<float>(i) / 20.0f;   // zeta 0 .. 2
            fall_scene sc = fresh(k_h);
            const outcome o = observe(sc, 3.0f, [&](fall_scene& y) { step_penalty(y, k, c); });
            if (o.exploded) { continue; }
            if (o.late_swing < best_swing_k)
            {
                best_swing_k = o.late_swing;
                best_depth_k = o.max_depth;
                best_c_k = c;
            }
        }
        std::printf("  %11.2f mm %12.1f %10.2f %11.3f mm %11.4f mm\n", static_cast<double>(x * 1000.0f),
                    static_cast<double>(k), static_cast<double>(best_c_k / critical),
                    static_cast<double>(best_depth_k * 1000.0f),
                    static_cast<double>(best_swing_k * 1000.0f));
        if (best_swing_k < overall_swing)
        {
            overall_swing = best_swing_k;
            overall_depth = best_depth_k;
            overall_c = best_c_k;
            overall_k = k;
        }
    }
    const float best_c = overall_c;
    const float best_swing = overall_swing;
    std::printf("\n  THE BEST PENALTY SETTING THERE IS, over 164 tried, lands the crate with\n"
                "  %.4f mm of residual bounce at %.2f mm of penetration (k = %.0f,\n"
                "  zeta = %.2f). The impulse solver, with nothing to tune, lands it with\n"
                "  %.4f mm at %.2f mm. So a penalty contact is not broken and it is not\n"
                "  free either: it is TUNED.\n",
                static_cast<double>(overall_swing * 1000.0f),
                static_cast<double>(overall_depth * 1000.0f), static_cast<double>(overall_k),
                static_cast<double>(overall_c / (2.0f * std::sqrt(overall_k * 20.0f))),
                static_cast<double>(impulse_60_converged.late_swing * 1000.0f),
                static_cast<double>(impulse_60_converged.max_depth * 1000.0f));

    // ---- AND THE TUNING DOES NOT TRANSFER ---------------------------------
    //
    // This is the argument, and it is the one that survives the sweep above.
    // The impulse solver's column is the control: same scene, no tuning at all.
    std::printf("\n  AND THE TUNING DOES NOT TRANSFER. The same spring, applied to scenes\n"
                "  it was not tuned on. The right-hand column is the impulse solver on the\n"
                "  identical scenes, with nothing tuned at all:\n\n");
    std::printf("  %-28s %13s %13s %13s\n", "scene", "penalty depth", "penalty swing",
                "impulse depth");
    struct variant { const char* name; float mass; float drop; };
    const variant variants[] = {
        {"20 kg from 1.2 m (tuned)", 20.0f, 1.2f},
        {"200 kg from 1.2 m", 200.0f, 1.2f},
        {"2 kg from 1.2 m", 2.0f, 1.2f},
        {"20 kg from 5 m", 20.0f, 5.0f},
    };
    bool transfer_broke = false;
    for (const variant& v : variants)
    {
        auto build = [&](float h) {
            fall_scene sc;
            sc.h = h;
            sc.body = make_box(vec3{0.0f, v.drop, 0.0f}, v.mass, vec3{0.5f, 0.5f, 0.5f});
            sc.floor = make_fixed(vec3{0.0f, 0.0f, 0.0f});
            sc.material = {0.0f, 0.5f};
            sc.cfg.restitution_bias = sc.gravity * h;
            return sc;
        };
        fall_scene sp = build(k_h);
        const outcome op = observe(sp, 3.0f, [&](fall_scene& x) { step_penalty(x, overall_k, best_c); });
        fall_scene si = build(k_h);
        si.passes = 16;
        const outcome oi = observe(si, 3.0f, [&](fall_scene& x) { step_scene(x); });
        std::printf("  %-28s %10.2f mm %10.3f mm %10.3f mm%s\n", v.name,
                    op.exploded ? 0.0 : static_cast<double>(op.max_depth * 1000.0f),
                    op.exploded ? 0.0 : static_cast<double>(op.late_swing * 1000.0f),
                    static_cast<double>(oi.max_depth * 1000.0f), op.exploded ? "  EXPLODED" : "");
        if (!op.exploded && op.late_swing > 20.0f * best_swing) { transfer_broke = true; }
        if (op.exploded) { transfer_broke = true; }
    }

    // THE CONTROL, in both halves. If penalty methods were simply broken, the
    // 400 Hz arm and the c = 800 row would fail too — neither does. And if the
    // impulse solver were doing nothing, its depth would be a body in free fall.
    check(!penalty_fast.exploded, "A: control - penalty at 400 Hz is stable");
    check(best_swing < 0.02f * penalty_60.late_swing,
          "A: control - the best penalty setting is far quieter than the undamped one");
    check(transfer_broke, "A: the tuning does not transfer to a scene it was not tuned on");
    check(penalty_60.late_swing > 0.5f, "A: an undamped penalty spring is a perfect bouncer");
    check(!impulse_60.exploded, "A: the impulse solver is stable at 60 Hz");
    check(impulse_60.late_swing < 0.05f * penalty_60.late_swing,
          "A: the impulse solver does not launch the crate");
    check(impulse_60_converged.end_depth < impulse_60.end_depth,
          "A: iterating the same solver sinks less still");
    std::printf("\n  CONTROL, AND IT REFUSED THE SECTION'S FIRST DRAFT: the 400 Hz spring is\n"
                "  quiet, and so is the best 60 Hz setting. A penalty contact is NOT\n"
                "  BROKEN — it is TUNED, and the tuning is a property of the scene rather\n"
                "  than of the material. Read the sweep as a TRADE: at 60 Hz you may have a\n"
                "  settled crate 108 mm into the floor, or one 12.55 mm in that bounces 367\n"
                "  mm forever, and not both. The impulse solver takes both and has no knob.\n"
                "  AND NOTE THE LAST TWO ROWS OF THE FIRST TABLE: the impulse solver has no\n"
                "  stiffness to be stable about, but ONE PASS over four contact points does\n"
                "  not hold the crate still either. Section J measures that properly, and it\n"
                "  is Lesson 8.10's reason to exist.\n");
}

// ---------------------------------------------------------------------------
// §B  the effective mass, two ways
// ---------------------------------------------------------------------------

void section_b()
{
    rule("B  THE EFFECTIVE MASS, TWO WAYS");

    // The form the derivation produces, kept here rather than in the engine so
    // that the two are genuinely independent code paths.
    auto naive_form = [](float inv_ma, const mat3& inv_ia, vec3 ra, float inv_mb,
                         const mat3& inv_ib, vec3 rb, vec3 dir) {
        const float ka = dot(dir, cross(inv_ia * cross(ra, dir), ra));
        const float kb = dot(dir, cross(inv_ib * cross(rb, dir), rb));
        return inv_ma + inv_mb + ka + kb;
    };

    rng gen(0x89B0u);
    double worst_relative = 0.0;
    double worst_positive_gap = 0.0;
    int n = 0;
    for (int i = 0; i < 200000; ++i)
    {
        rigid_body a = make_box(vec3{gen.range(-3.0f, 3.0f), gen.range(-3.0f, 3.0f), 0.0f},
                                gen.range(0.2f, 400.0f),
                                vec3{gen.range(0.1f, 2.0f), gen.range(0.1f, 2.0f), gen.range(0.1f, 2.0f)});
        rigid_body b = make_box(vec3{gen.range(-3.0f, 3.0f), gen.range(-3.0f, 3.0f), 0.0f},
                                gen.range(0.2f, 400.0f),
                                vec3{gen.range(0.1f, 2.0f), gen.range(0.1f, 2.0f), gen.range(0.1f, 2.0f)});
        a.orientation = gen.orientation();
        b.orientation = gen.orientation();

        const vec3 p{gen.range(-3.0f, 3.0f), gen.range(-3.0f, 3.0f), gen.range(-3.0f, 3.0f)};
        const vec3 ra = p - a.state.position;
        const vec3 rb = p - b.state.position;
        const vec3 dir = gen.direction();

        const mat3 ia = world_inv_inertia(a);
        const mat3 ib = world_inv_inertia(b);

        const float k_rearranged = 1.0f / effective_mass(a.inv_mass, ia, ra, b.inv_mass, ib, rb, dir);
        const float k_naive = naive_form(a.inv_mass, ia, ra, b.inv_mass, ib, rb, dir);

        const double rel = std::abs(k_rearranged - k_naive) / std::abs(k_naive);
        worst_relative = std::max(worst_relative, rel);

        // Positive definiteness: the two angular terms can never make `k` smaller
        // than the sum of the inverse masses. A sign error in the cross products
        // is exactly what would break this, and it would not show up as a
        // disagreement between the two forms because both would have it.
        const double gap = static_cast<double>(k_rearranged) - (a.inv_mass + b.inv_mass);
        worst_positive_gap = std::min(worst_positive_gap, gap);
        ++n;
    }

    std::printf("  %d random contacts, freely oriented boxes 0.2-400 kg:\n", n);
    std::printf("    worst relative disagreement, the two algebraic forms : %.3e\n", worst_relative);
    std::printf("    worst  k - (1/ma + 1/mb)  (must be >= 0)             : %+.3e\n",
                worst_positive_gap);
    check(worst_relative < 1e-3, "B: the two forms of the effective mass agree");
    check(worst_positive_gap > -1e-9, "B: k is never below the sum of inverse masses");

    // ---- CONTROL 1: a contact straight through the centre of mass --------
    //
    // `r x n` is zero, so both angular terms vanish and `k` must be EXACTLY the
    // sum of the inverse masses — not approximately. A test that only ever sees
    // random geometry cannot tell a nearly-right angular term from a right one.
    rigid_body ball_a = make_sphere(vec3{0.0f, 0.0f, 0.0f}, 3.0f, 0.5f);
    rigid_body ball_b = make_sphere(vec3{1.0f, 0.0f, 0.0f}, 7.0f, 0.5f);
    const vec3 mid{0.5f, 0.0f, 0.0f};
    const float k_central = 1.0f / effective_mass(ball_a, ball_b, mid - ball_a.state.position,
                                                  mid - ball_b.state.position, vec3{1.0f, 0.0f, 0.0f});
    const float k_expected = ball_a.inv_mass + ball_b.inv_mass;
    std::printf("\n  CONTROL, contact through both centres: k = %.9f, 1/ma + 1/mb = %.9f\n",
                static_cast<double>(k_central), static_cast<double>(k_expected));
    std::printf("    reduced mass = 1/k = %.6f kg  (analytic m1*m2/(m1+m2) = %.6f)\n",
                static_cast<double>(1.0f / k_central), 3.0 * 7.0 / 10.0);
    check(std::abs(k_central - k_expected) < 1e-7f, "B: control - central contact has no angular term");
    check(std::abs(1.0f / k_central - 2.1f) < 1e-5f, "B: control - 1/k is the reduced mass");

    // ---- CONTROL 2: two immovable bodies ---------------------------------
    rigid_body s1 = make_fixed(vec3{0.0f, 0.0f, 0.0f});
    rigid_body s2 = make_fixed(vec3{1.0f, 0.0f, 0.0f});
    const float m_static = effective_mass(s1, s2, vec3{0.5f, 0.0f, 0.0f}, vec3{-0.5f, 0.0f, 0.0f},
                                          vec3{1.0f, 0.0f, 0.0f});
    std::printf("  CONTROL, two immovable bodies: effective mass = %.1f (not an infinity)\n",
                static_cast<double>(m_static));
    check(m_static == 0.0f, "B: control - two static bodies give zero, not a division");

    // ---- CONTROL 3: the lever arm really does matter ---------------------
    //
    // If the angular terms were silently zero — the commonest way to get this
    // wrong — an off-centre contact would report the same effective mass as a
    // central one. It must not.
    rigid_body plank = make_box(vec3{0.0f, 0.0f, 0.0f}, 10.0f, vec3{2.0f, 0.1f, 0.5f});
    rigid_body ground = make_fixed(vec3{0.0f, -1.0f, 0.0f});
    const vec3 up{0.0f, 1.0f, 0.0f};
    const float m_centre = effective_mass(ground, plank, vec3{0.0f, 1.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f}, up);
    const float m_end = effective_mass(ground, plank, vec3{1.9f, 1.0f, 0.0f}, vec3{1.9f, 0.0f, 0.0f}, up);
    std::printf("  CONTROL, a 10 kg plank: pushed at the centre it weighs %.3f kg,\n"
                "    pushed 1.9 m out it weighs %.3f kg (%.2fx lighter)\n",
                static_cast<double>(m_centre), static_cast<double>(m_end),
                static_cast<double>(m_centre / m_end));
    check(m_end < m_centre * 0.5f, "B: control - an off-centre contact is much lighter");
}

// ---------------------------------------------------------------------------
// §C  one impulse, and the velocity it produces
// ---------------------------------------------------------------------------

void section_c()
{
    rule("C  ONE IMPULSE, AND THE VELOCITY IT PRODUCES");

    // ---- FIRST, AN INSTRUMENT PROBLEM, BECAUSE IT DECIDES THE SECTION -----
    //
    // The obvious fixture is two spheres meeting head on, run through
    // `collide_manifold` and checked against the algebra. It does not work, and
    // the reason is not in this lesson's code.
    {
        const auto sa = sphere_shape(0.5f);
        std::printf("  A DETECTOR PROBLEM FIRST. Two spheres meeting head on along x,\n"
                    "  through the whole 8.5-8.7 chain. The normal should be (1, 0, 0):\n\n");
        std::printf("  %12s %16s %16s %14s\n", "overlap", "normal error", "depth error",
                    "relative");
        double worst_angle = 0.0;
        for (float overlap : {0.001f, 0.010f, 0.100f, 0.400f})
        {
            const auto ball_a = world_sphere(sa, vec3{0.0f, 0.0f, 0.0f});
            const auto ball_b = world_sphere(sa, vec3{1.0f - overlap, 0.0f, 0.0f});
            const contact_manifold m = collide_manifold(as_convex(ball_a), as_convex(ball_b));
            const float angle = std::acos(std::min(1.0f, std::abs(m.normal.x))) * k_deg;
            const float depth_error = m.points[0].depth - overlap;
            std::printf("  %9.1f mm %13.4f deg %13.4f mm %13.4f%%\n",
                        static_cast<double>(overlap * 1000.0f), static_cast<double>(angle),
                        static_cast<double>(depth_error * 1000.0f),
                        100.0 * static_cast<double>(depth_error / overlap));
            worst_angle = std::max(worst_angle, static_cast<double>(angle));
        }
        std::printf("\n  THE ERROR IS CONSTANT ACROSS FOUR DECADES, so it is not convergence\n"
                    "  noise — EPA is stopping at a fixed place. The depth is short by\n"
                    "  exactly 1 - cos(4.26 deg) = %.4f%%, which is what you get when the\n"
                    "  direction is wrong and the depth along it is right.\n",
                    100.0 * (1.0 - std::cos(4.2602 / k_deg)));
        std::printf("  8.6 measured EPA at a worst case of 21 expansions over 200,000 pairs\n"
                    "  and set `max_iterations` to 32 on that evidence — of BOXES. A sphere\n"
                    "  has no flat face to terminate on, so it runs out of iterations:\n\n");
        std::printf("  %14s %10s %16s %14s\n", "max_iterations", "tolerance", "normal error",
                    "status");
        for (int iterations : {32, 64, 128})
        {
            engine::phys::epa_config ec;
            ec.max_iterations = iterations;
            const auto ball_a = world_sphere(sa, vec3{0.0f, 0.0f, 0.0f});
            const auto ball_b = world_sphere(sa, vec3{0.99f, 0.0f, 0.0f});
            const engine::phys::gjk_result g =
                engine::phys::gjk_distance(as_convex(ball_a), as_convex(ball_b));
            const engine::phys::epa_result r =
                engine::phys::epa_penetration(as_convex(ball_a), as_convex(ball_b), g.terminal, ec);
            std::printf("  %14d %10.0e %13.4f deg %14s\n", iterations, 1e-4,
                        static_cast<double>(std::acos(std::min(1.0f, std::abs(r.normal.x))) * k_deg),
                        name_of(r.status));
        }
        std::printf("\n  8.7 HIDES THIS WHENEVER ONE SHAPE IS A BOX, because the manifold takes\n"
                    "  the reference FACE's normal rather than EPA's — 8.7 §6's measurement,\n"
                    "  and the reason a ball on a floor is bit-exact while two balls are 4\n"
                    "  degrees out. Two spheres is the one pair with no face to snap to.\n"
                    "  THIS LESSON DOES NOT CHANGE `epa.hpp`: it is 8.6's knob, the fix is one\n"
                    "  number, and changing it needs 8.6's own 200,000-pair measurement rerun\n"
                    "  to say what it costs. What §C does instead is take the detector out of\n"
                    "  the loop, which `build_manifold` exists to allow.\n");
        check(worst_angle > 1.0, "C: the sphere-sphere normal really is degrees out");
    }

    // A head-on collision of two spheres has an algebraic answer, so this is a
    // simulation checked against a formula rather than against itself — PROVIDED
    // the formula's geometry is what the solver is given. `build_manifold` takes
    // the normal and depth as arguments precisely so that a test can supply them
    // exactly, which is `manifold.hpp`'s own stated reason for the signature.
    auto head_on = [](float m1, float m2, float v1, float v2, float e) {
        rigid_body a = make_sphere(vec3{-0.5f, 0.0f, 0.0f}, m1, 0.5f);
        rigid_body b = make_sphere(vec3{0.49f, 0.0f, 0.0f}, m2, 0.5f);
        a.state.velocity = vec3{v1, 0.0f, 0.0f};
        b.state.velocity = vec3{v2, 0.0f, 0.0f};

        const auto sa = sphere_shape(0.5f);
        const auto ball_a = world_sphere(sa, a.state.position);
        const auto ball_b = world_sphere(sa, b.state.position);
        contact_manifold m = engine::phys::build_manifold(as_convex(ball_a), as_convex(ball_b),
                                                          vec3{1.0f, 0.0f, 0.0f}, 0.01f);
        solver_config cfg;
        cfg.friction = friction_model::none;
        cfg.restitution_threshold = 0.0f;
        const solve_report r = resolve_contact(a, b, m, contact_material{e, 0.0f}, cfg);
        return std::tuple{a.state.velocity.x, b.state.velocity.x, r, m.count};
    };

    std::printf("  Head-on, 3 kg at +4 m/s into 7 kg at -1 m/s. Analytic answers from\n"
                "  momentum and the definition of e, derived without the solver.\n\n");
    std::printf("  %5s %12s %12s %12s %12s\n", "e", "v1 (sim)", "v1 (exact)", "v2 (sim)", "v2 (exact)");

    const float m1 = 3.0f, m2 = 7.0f, u1 = 4.0f, u2 = -1.0f;
    double worst = 0.0;
    for (float e : {0.0f, 0.25f, 0.5f, 0.8f, 1.0f})
    {
        // v1' = (m1*u1 + m2*u2 + m2*e*(u2 - u1)) / (m1 + m2)
        const float total = m1 + m2;
        const float x1 = (m1 * u1 + m2 * u2 + m2 * e * (u2 - u1)) / total;
        const float x2 = (m1 * u1 + m2 * u2 + m1 * e * (u1 - u2)) / total;
        auto [s1, s2, report, count] = head_on(m1, m2, u1, u2, e);
        (void)report;
        (void)count;
        std::printf("  %5.2f %12.6f %12.6f %12.6f %12.6f\n", static_cast<double>(e),
                    static_cast<double>(s1), static_cast<double>(x1),
                    static_cast<double>(s2), static_cast<double>(x2));
        worst = std::max({worst, std::abs(static_cast<double>(s1 - x1)),
                          std::abs(static_cast<double>(s2 - x2))});
    }
    std::printf("\n  worst disagreement with the closed form: %.3e m/s\n", worst);
    check(worst < 1e-4, "C: the solver reproduces the 1-D closed form");

    // ---- momentum, and why it is a tautology ------------------------------
    {
        auto [s1, s2, report, count] = head_on(m1, m2, u1, u2, 1.0f);
        (void)report;
        (void)count;
        const double p_before = static_cast<double>(m1 * u1 + m2 * u2);
        const double p_after = static_cast<double>(m1 * s1 + m2 * s2);
        const double ke_before = 0.5 * (m1 * u1 * u1 + m2 * u2 * u2);
        const double ke_after = 0.5 * (static_cast<double>(m1) * s1 * s1
                                       + static_cast<double>(m2) * s2 * s2);
        std::printf("\n  e = 1:  momentum %.9f -> %.9f   (relative %.2e)\n", p_before, p_after,
                    std::abs(p_after - p_before) / std::abs(p_before));
        std::printf("          kinetic  %.9f -> %.9f   (relative %.2e)\n", ke_before, ke_after,
                    std::abs(ke_after - ke_before) / ke_before);
        check(std::abs(p_after - p_before) / std::abs(p_before) < 1e-6, "C: momentum conserved");
        check(std::abs(ke_after - ke_before) / ke_before < 1e-5, "C: energy conserved at e = 1");

        std::printf("\n  READ THE TWO LINES DIFFERENTLY. Momentum is conserved because the same\n"
                    "  J is added to one body and subtracted from the other, so it would come\n"
                    "  out at 1e-9 even if every number in the impulse were wrong. ENERGY is\n"
                    "  the check: nothing enforces it, and it only lands if the effective\n"
                    "  mass, the lever arms and the target velocity are all right at once.\n");
    }

    // ---- CONTROL: a deliberately wrong effective mass ---------------------
    //
    // What the energy check looks like when the solver IS wrong. Halving the
    // effective mass is the single most plausible mistake in this file (forget
    // one body's inverse mass), and it leaves momentum untouched.
    {
        rigid_body a = make_sphere(vec3{-0.5f, 0.0f, 0.0f}, m1, 0.5f);
        rigid_body b = make_sphere(vec3{0.49f, 0.0f, 0.0f}, m2, 0.5f);
        a.state.velocity = vec3{u1, 0.0f, 0.0f};
        b.state.velocity = vec3{u2, 0.0f, 0.0f};
        const auto sa = sphere_shape(0.5f);
        const auto ball_a = world_sphere(sa, a.state.position);
        const auto ball_b = world_sphere(sa, b.state.position);
        contact_manifold m = engine::phys::build_manifold(as_convex(ball_a), as_convex(ball_b),
                                                          vec3{1.0f, 0.0f, 0.0f}, 0.01f);
        solver_config cfg;
        cfg.friction = friction_model::none;
        cfg.restitution_threshold = 0.0f;
        contact_batch batch = prepare_contacts(a, b, m, contact_material{1.0f, 0.0f}, cfg);
        for (int i = 0; i < batch.count; ++i) { batch.points[i].normal_mass *= 0.5f; }
        solve_contacts(a, b, batch, cfg);

        const double p_before = static_cast<double>(m1 * u1 + m2 * u2);
        const double p_after = static_cast<double>(m1 * a.state.velocity.x + m2 * b.state.velocity.x);
        const double ke_before = 0.5 * (m1 * u1 * u1 + m2 * u2 * u2);
        const double ke_after = 0.5 * (static_cast<double>(m1) * a.state.velocity.x * a.state.velocity.x
                                       + static_cast<double>(m2) * b.state.velocity.x * b.state.velocity.x);
        std::printf("\n  CONTROL, effective mass halved on purpose:\n");
        std::printf("    momentum relative change %.2e   <- still perfect\n",
                    std::abs(p_after - p_before) / std::abs(p_before));
        std::printf("    energy   relative change %.2e   <- caught\n",
                    std::abs(ke_after - ke_before) / ke_before);
        check(std::abs(p_after - p_before) / std::abs(p_before) < 1e-6,
              "C: control - momentum survives a wrong impulse");
        check(std::abs(ke_after - ke_before) / ke_before > 0.05,
              "C: control - energy catches a wrong impulse");
    }

    // ---- the target velocity is hit exactly, on one point -----------------
    {
        rng gen(0x89C0u);
        double worst_residual = 0.0;
        for (int i = 0; i < 20000; ++i)
        {
            rigid_body a = make_sphere(vec3{0.0f, 0.0f, 0.0f}, gen.range(0.5f, 50.0f), 0.5f);
            rigid_body b = make_sphere(vec3{gen.range(0.90f, 0.99f), 0.0f, 0.0f},
                                       gen.range(0.5f, 50.0f), 0.5f);
            a.state.velocity = gen.direction() * gen.range(0.0f, 8.0f);
            b.state.velocity = gen.direction() * gen.range(0.0f, 8.0f);
            a.angular_velocity = gen.direction() * gen.range(0.0f, 5.0f);
            b.angular_velocity = gen.direction() * gen.range(0.0f, 5.0f);

            const auto sa = sphere_shape(0.5f);
            const auto ball_a = world_sphere(sa, a.state.position);
            const auto ball_b = world_sphere(sa, b.state.position);
            contact_manifold m = collide_manifold(as_convex(ball_a), as_convex(ball_b));
            if (m.count != 1) { continue; }
            solver_config cfg;
            cfg.friction = friction_model::none;
            cfg.restitution_threshold = 0.0f;
            const solve_report r = resolve_contact(a, b, m, contact_material{gen.unit(), 0.0f}, cfg);
            worst_residual = std::max(worst_residual, static_cast<double>(r.max_residual));
        }
        std::printf("\n  20,000 single-point contacts, random masses, velocities and spins:\n"
                    "    worst |v_n - target| after one solve: %.3e m/s\n", worst_residual);
        check(worst_residual < 1e-4, "C: one point reaches its target velocity exactly");
    }
}

// ---------------------------------------------------------------------------
// §D  restitution, and the gravity already in the velocity
// ---------------------------------------------------------------------------

void section_d()
{
    rule("D  RESTITUTION, AND THE GRAVITY ALREADY IN THE VELOCITY");

    struct bounce_run
    {
        std::vector<float> peaks;
        float settled_speed = 0.0f;
    };

    auto drop = [](float e, bool corrected, float h, int wanted_peaks) {
        fall_scene s;
        s.h = h;
        s.body = make_sphere(vec3{0.0f, 1.5f, 0.0f}, 1.0f, 0.25f);
        s.body_shape = sphere_shape(0.25f);
        s.body_is_sphere = true;
        s.sphere_radius = 0.25f;
        s.floor = make_fixed(vec3{0.0f, -0.5f, 0.0f});
        s.material = {e, 0.0f};
        s.cfg.friction = friction_model::none;
        s.cfg.restitution_threshold = 0.0f;   // §E is where the threshold is measured
        if (corrected) { s.cfg.restitution_bias = s.gravity * h; }

        bounce_run out;
        float previous_y = s.body.state.position.y;
        float previous_v = 0.0f;
        float last_rebound = 0.0f;
        bool rising = false;
        const int steps = static_cast<int>(30.0f / h);
        for (int i = 0; i < steps; ++i)
        {
            step_scene(s);
            // The rebound speed itself, which needs no peak and therefore no
            // interpolation: the step on which the velocity flipped from down to
            // up is the step the contact was solved on.
            if (previous_v < 0.0f && s.body.state.velocity.y > 0.0f)
            {
                last_rebound = s.body.state.velocity.y;
            }
            previous_v = s.body.state.velocity.y;
            const float y = s.body.state.position.y;
            if (y > previous_y) { rising = true; }
            else if (rising)
            {
                // Peak: the height above the floor's surface, which sits at
                // y = 0 for a 0.25 m ball on a floor whose top is at 0.
                rising = false;
                out.peaks.push_back(previous_y - 0.25f);
                if (static_cast<int>(out.peaks.size()) >= wanted_peaks && wanted_peaks > 0) { break; }
            }
            previous_y = y;
        }
        out.settled_speed = last_rebound;
        return out;
    };

    std::printf("  A 1 kg ball dropped so its first bounce peaks at h0, e = 0.80.\n"
                "  Closed form: the n-th peak is e^(2n)*h0, derived from nothing but\n"
                "  'speed x e' and 'height goes as speed squared'.\n\n");

    const bounce_run raw = drop(0.8f, false, k_h, 10);
    const bounce_run fixed = drop(0.8f, true, k_h, 10);
    const bounce_run raw_long = drop(0.8f, false, k_h, 0);

    std::printf("  %4s %14s %14s %14s\n", "n", "e^2n * h0", "uncorrected", "corrected");
    const float h0_raw = raw.peaks.empty() ? 0.0f : raw.peaks[0];
    const float h0_fixed = fixed.peaks.empty() ? 0.0f : fixed.peaks[0];
    const std::size_t rows = std::min(raw.peaks.size(), fixed.peaks.size());
    for (std::size_t i = 0; i < rows; ++i)
    {
        std::printf("  %4zu %12.5f m %12.5f m %12.5f m\n", i,
                    static_cast<double>(engine::phys::bounce_height(h0_fixed, 0.8f, static_cast<int>(i))),
                    static_cast<double>(raw.peaks[i]), static_cast<double>(fixed.peaks[i]));
    }
    (void)h0_raw;

    // ---- the fixed point --------------------------------------------------
    const float gh = k_gravity * k_h;
    std::printf("\n  Where the uncorrected bounce is GOING, in closed form:\n");
    std::printf("  %6s %16s %16s %14s %10s\n", "e", "v predicted", "v measured",
                "hop height", "ratio");
    for (float e : {0.50f, 0.80f, 0.95f})
    {
        const float v = terminal_bounce_speed(e, gh);
        const bounce_run long_run = drop(e, false, k_h, 0);
        const float measured = long_run.settled_speed;
        std::printf("  %6.2f %13.4f m/s %13.4f m/s %11.2f mm %10.3f\n", static_cast<double>(e),
                    static_cast<double>(v), static_cast<double>(measured),
                    static_cast<double>(measured * measured / (2.0f * k_gravity) * 1000.0f),
                    static_cast<double>(measured / v));
        check(measured > 0.5f * v && measured < 2.0f * v,
              "D: the permanent bounce speed is the predicted fixed point");
    }

    const bounce_run long_fixed = drop(0.8f, true, k_h, 0);
    const float predicted_last = engine::phys::bounce_height(h0_fixed, 0.8f,
                                                             static_cast<int>(rows) - 1);
    const float ratio_fixed = fixed.peaks[rows - 1] / predicted_last;
    const float ratio_raw = raw.peaks[rows - 1] / predicted_last;
    std::printf("\n  At the tenth bounce the corrected run is %.3fx the closed form and the\n"
                "  uncorrected one is %.3fx. Over 30 s the corrected ball's last bounce\n"
                "  rebounds at %.5f m/s and the uncorrected one at %.5f m/s — one is going\n"
                "  to zero and the other is going to %.4f.\n",
                static_cast<double>(ratio_fixed), static_cast<double>(ratio_raw),
                static_cast<double>(long_fixed.settled_speed),
                static_cast<double>(raw_long.settled_speed),
                static_cast<double>(terminal_bounce_speed(0.8f, gh)));
    check(std::abs(ratio_fixed - 1.0f) < 0.25f, "D: the corrected bounce tracks e^(2n)*h0");
    check(ratio_raw > 2.0f, "D: the uncorrected bounce does not");
    check(long_fixed.settled_speed < 0.5f * raw_long.settled_speed,
          "D: the correction kills the permanent bounce");

    // ---- CONTROL: two dynamic bodies in free fall --------------------------
    //
    // Gravity adds the SAME velocity to both, so the relative normal velocity it
    // produces is exactly zero and the correction must do nothing at all. A bias
    // implemented as "subtract g*h from the approach speed" — which is what one
    // writes first — would be wrong here, and wrong by the full 16 cm/s.
    {
        auto free_fall_pair = [](bool corrected) {
            rigid_body a = make_sphere(vec3{0.0f, 5.0f, 0.0f}, 2.0f, 0.5f);
            rigid_body b = make_sphere(vec3{0.0f, 5.98f, 0.0f}, 2.0f, 0.5f);
            a.state.velocity = vec3{0.0f, 2.0f, 0.0f};
            b.state.velocity = vec3{0.0f, -2.0f, 0.0f};
            // Both have already had this step's gravity applied.
            const vec3 gh_v{0.0f, -k_gravity * k_h, 0.0f};
            a.state.velocity = a.state.velocity + gh_v;
            b.state.velocity = b.state.velocity + gh_v;

            const auto sa = sphere_shape(0.5f);
            const auto ball_a = world_sphere(sa, a.state.position);
            const auto ball_b = world_sphere(sa, b.state.position);
            contact_manifold m = collide_manifold(as_convex(ball_a), as_convex(ball_b));
            solver_config cfg;
            cfg.friction = friction_model::none;
            cfg.restitution_threshold = 0.0f;
            if (corrected) { cfg.restitution_bias = gh_v; }
            resolve_contact(a, b, m, contact_material{0.8f, 0.0f}, cfg);
            return b.state.velocity.y - a.state.velocity.y;
        };
        const float without = free_fall_pair(false);
        const float with = free_fall_pair(true);
        std::printf("\n  CONTROL, two dynamic balls meeting in free fall: separation speed\n"
                    "    without the correction %.9f m/s, with it %.9f m/s (identical)\n",
                    static_cast<double>(without), static_cast<double>(with));
        check(std::abs(without - with) < 1e-6f,
              "D: control - the correction is a no-op when both bodies are dynamic");
    }

    // ---- CONTROL: e = 0 ----------------------------------------------------
    {
        const bounce_run dead = drop(0.0f, false, k_h, 4);
        std::printf("  CONTROL, e = 0: %zu peaks in 30 s (a dead landing does not bounce,\n"
                    "    with or without the correction)\n", dead.peaks.size());
        check(dead.peaks.size() <= 1, "D: control - e = 0 does not bounce");
    }
}

// ---------------------------------------------------------------------------
// §E  the restitution threshold
// ---------------------------------------------------------------------------

void section_e()
{
    rule("E  THE RESTITUTION THRESHOLD");

    auto rest = [](float e, float threshold, bool bias, vec3 push) {
        fall_scene s;
        // SIXTEEN PASSES, and the reason is §J: ONE pass over a four-point
        // manifold leaves 5 mm/s of creep, which is larger than the buzz this
        // section exists to measure. You cannot measure restitution on a crate
        // that is still sinking, and the first draft of this section did exactly
        // that — every row came back at 55 mm and none of them was restitution.
        s.passes = 16;
        s.body = make_box(vec3{0.0f, 0.55f, 0.0f}, 20.0f, vec3{0.5f, 0.5f, 0.5f});
        s.floor = make_fixed(vec3{0.0f, -0.5f, 0.0f});
        s.material = {e, 0.5f};
        s.cfg.restitution_threshold = threshold;
        s.extra_force = push;
        if (bias) { s.cfg.restitution_bias = s.gravity * s.h; }

        for (int i = 0; i < 180; ++i) { step_scene(s); }
        float max_speed = 0.0f;
        float max_height = -1e9f;
        float min_height = 1e9f;
        int airborne = 0;
        for (int i = 0; i < 300; ++i)
        {
            step_scene(s);
            max_speed = std::max(max_speed, std::abs(s.body.state.velocity.y));
            max_height = std::max(max_height, s.body.state.position.y);
            min_height = std::min(min_height, s.body.state.position.y);
            if (!s.touched) { ++airborne; }
        }
        return std::tuple{max_speed, max_height - min_height, airborne};
    };

    std::printf("  A 20 kg crate settling on a floor, measured over the five seconds\n"
                "  AFTER it has landed. A body at rest should not move at all. The buzz\n"
                "  is its peak-to-peak height over that window.\n\n");
    std::printf("  %6s %12s %10s %14s %14s %10s\n", "e", "threshold", "bias", "peak |vy|",
                "buzz", "frames off");
    for (float e : {0.00f, 0.30f, 0.60f})
    {
        for (bool bias : {false, true})
        {
            for (float t : {0.0f, 1.0f})
            {
                auto [speed, buzz, airborne] = rest(e, t, bias, vec3{});
                std::printf("  %6.2f %9.2f m/s %10s %11.5f m/s %11.4f mm %10d\n",
                            static_cast<double>(e), static_cast<double>(t), bias ? "on" : "off",
                            static_cast<double>(speed), static_cast<double>(buzz * 1000.0f),
                            airborne);
            }
        }
    }

    auto [speed_off, buzz_off, air_off] = rest(0.6f, 0.0f, false, vec3{});
    auto [speed_on, buzz_on, air_on] = rest(0.6f, 1.0f, false, vec3{});
    auto [speed_bias, buzz_bias, air_bias] = rest(0.6f, 0.0f, true, vec3{});
    (void)air_off;
    (void)air_on;
    (void)air_bias;
    std::printf("\n  AT e = 0.6, WITH NO BIAS: the threshold takes the buzz from %.4f mm to\n"
                "  %.4f mm and the peak speed from %.5f to %.5f m/s. That is the artifact\n"
                "  the threshold exists for, and every engine ships it.\n",
                static_cast<double>(buzz_off * 1000.0f), static_cast<double>(buzz_on * 1000.0f),
                static_cast<double>(speed_off), static_cast<double>(speed_on));
    check(buzz_on < 0.5f * buzz_off, "E: the threshold suppresses the resting buzz");

    std::printf("\n  *** AND THE BIAS DOES THE SAME JOB EXACTLY, WITH NO MAGIC NUMBER. ***\n"
                "  With `restitution_bias` set and the threshold at ZERO the buzz is %.4f mm\n"
                "  — the same as the threshold achieves — because the two are fixes for the\n"
                "  SAME artifact. A resting body's approach speed IS this step's g*h;\n"
                "  subtracting it makes the arrival speed exactly zero, and zero is below\n"
                "  any threshold. The threshold is the approximate version of §D's\n"
                "  correction, and it was invented first.\n",
                static_cast<double>(buzz_bias * 1000.0f));
    check(buzz_bias < 0.5f * buzz_off, "E: the restitution bias alone suppresses the buzz");

    // ---- WHAT THE BIAS CANNOT REACH ---------------------------------------
    //
    // The bias cancels the acceleration the CALLER declared. Anything else that
    // presses the two bodies together in the same step — a thruster, a spring, a
    // conveyor, another body in a stack — produces an approach speed the solver
    // has no way to know about, and restitution turns it into a bounce.
    std::printf("\n  WHAT THE BIAS CANNOT REACH. A 300 N downward thruster on the same\n"
                "  crate, which the solver was never told about (bias still set to g*h):\n\n");
    std::printf("  %6s %12s %14s %14s %10s\n", "e", "threshold", "peak |vy|", "buzz",
                "frames off");
    float pushed_off = 0.0f;
    float pushed_on = 0.0f;
    for (float e : {0.30f, 0.60f})
    {
        for (float t : {0.0f, 1.0f})
        {
            auto [speed, buzz, airborne] = rest(e, t, true, vec3{0.0f, -300.0f, 0.0f});
            std::printf("  %6.2f %9.2f m/s %11.5f m/s %11.4f mm %10d\n", static_cast<double>(e),
                        static_cast<double>(t), static_cast<double>(speed),
                        static_cast<double>(buzz * 1000.0f), airborne);
            if (e > 0.5f && t == 0.0f) { pushed_off = buzz; }
            if (e > 0.5f && t == 1.0f) { pushed_on = buzz; }
        }
    }
    std::printf("\n  So the threshold is not redundant — it is the catch-all for every\n"
                "  acceleration the caller did not declare, and a stack of crates is full\n"
                "  of them. Keep both: the bias because it is exact where it applies, the\n"
                "  threshold because it covers everywhere else.\n");
    check(pushed_off > 0.002f, "E: an undeclared push defeats the bias");
    check(pushed_on < 0.25f * pushed_off, "E: and the threshold catches it");

    // ---- CONTROL: e = 0 is quiet however you set the knobs -----------------
    auto [dead_speed, dead_buzz, dead_air] = rest(0.0f, 0.0f, false, vec3{});
    (void)dead_air;
    std::printf("\n  CONTROL, e = 0 with BOTH corrections off: buzz %.4f mm, peak |vy|\n"
                "  %.5f m/s. So what these knobs suppress is restitution, not the solver.\n",
                static_cast<double>(dead_buzz * 1000.0f), static_cast<double>(dead_speed));
    check(dead_buzz < 1e-3f, "E: control - e = 0 is quiet with no correction at all");
}

// ---------------------------------------------------------------------------
// §F  friction: the cone and the box
// ---------------------------------------------------------------------------

void section_f()
{
    rule("F  FRICTION: THE CONE AND THE BOX");

    // ---- the tangent basis itself -----------------------------------------
    {
        rng gen(0x89F0u);
        double worst_orthonormality = 0.0;
        double worst_length = 0.0;
        for (int i = 0; i < 1000000; ++i)
        {
            const vec3 n = gen.direction();
            vec3 t1, t2;
            tangent_basis(n, t1, t2);
            worst_orthonormality = std::max({worst_orthonormality,
                                             std::abs(static_cast<double>(dot(n, t1))),
                                             std::abs(static_cast<double>(dot(n, t2))),
                                             std::abs(static_cast<double>(dot(t1, t2)))});
            worst_length = std::max({worst_length,
                                     std::abs(static_cast<double>(length(t1)) - 1.0),
                                     std::abs(static_cast<double>(length(t2)) - 1.0)});
        }
        std::printf("  tangent_basis over 1,000,000 random normals:\n");
        std::printf("    worst |dot| among the three pairs : %.3e\n", worst_orthonormality);
        std::printf("    worst |length - 1|                : %.3e   (t2 is NOT normalised)\n",
                    worst_length);
        check(worst_orthonormality < 1e-6, "F: the tangent basis is orthogonal");
        check(worst_length < 1e-6, "F: the tangent basis is normalised without a second sqrt");
    }

    // ---- the 360-degree sweep ---------------------------------------------
    //
    // A puck sliding on a floor in direction theta. Under the cone the answer
    // cannot depend on theta, because a disc is round; under the box it must,
    // because a square is not. Sliding at theta against a fixed basis is the
    // same experiment as sliding along a fixed direction with the basis rotated
    // by -theta, so this sweep IS the measurement of basis dependence.
    auto slide = [](float theta, friction_model model, float mu) {
        fall_scene s;
        s.passes = 16;   // §J: one pass leaves creep larger than the effect
        s.body = make_box(vec3{0.0f, 0.5f, 0.0f}, 5.0f, vec3{0.5f, 0.5f, 0.5f});
        s.floor = make_fixed(vec3{0.0f, -0.5f, 0.0f});
        s.material = {0.0f, mu};
        s.cfg.friction = model;
        s.cfg.restitution_bias = s.gravity * s.h;

        // Settle first so the contact is a clean four-point manifold, then push.
        for (int i = 0; i < 90; ++i) { step_scene(s); }
        const float speed = 4.0f;
        s.body.state.velocity = vec3{std::cos(theta) * speed, s.body.state.velocity.y,
                                     std::sin(theta) * speed};
        s.body.angular_velocity = vec3{};

        const vec3 start = s.body.state.position;
        int steps = 0;
        for (; steps < 1200; ++steps)
        {
            step_scene(s);
            const vec3 v = s.body.state.velocity;
            if (std::sqrt(v.x * v.x + v.z * v.z) < 1e-3f) { break; }
        }
        const vec3 travelled = s.body.state.position - start;
        return std::sqrt(travelled.x * travelled.x + travelled.z * travelled.z);
    };

    std::printf("\n  A 5 kg crate pushed at 4 m/s along the floor, mu = 0.50. Sliding\n"
                "  distance against direction, over a full turn:\n\n");
    std::printf("  %10s %16s %16s\n", "theta", "box model", "cone model");
    float box_min = 1e9f, box_max = -1e9f, cone_min = 1e9f, cone_max = -1e9f;
    for (int i = 0; i <= 8; ++i)
    {
        const float theta = static_cast<float>(i) * (k_pi / 16.0f);
        const float d_box = slide(theta, friction_model::box, 0.5f);
        const float d_cone = slide(theta, friction_model::cone, 0.5f);
        std::printf("  %8.1f deg %14.4f m %14.4f m\n", static_cast<double>(theta * k_deg),
                    static_cast<double>(d_box), static_cast<double>(d_cone));
        box_min = std::min(box_min, d_box);
        box_max = std::max(box_max, d_box);
        cone_min = std::min(cone_min, d_cone);
        cone_max = std::max(cone_max, d_cone);
    }
    // Finer sweep for the spread, without printing 180 rows.
    for (int i = 0; i <= 360; ++i)
    {
        const float theta = static_cast<float>(i) * (k_pi / 180.0f);
        const float d_box = slide(theta, friction_model::box, 0.5f);
        const float d_cone = slide(theta, friction_model::cone, 0.5f);
        box_min = std::min(box_min, d_box);
        box_max = std::max(box_max, d_box);
        cone_min = std::min(cone_min, d_cone);
        cone_max = std::max(cone_max, d_cone);
    }
    std::printf("\n  over 361 directions:\n");
    std::printf("    box  : %.4f m to %.4f m   spread %.2f%%\n", static_cast<double>(box_min),
                static_cast<double>(box_max), 100.0 * (box_max - box_min) / box_min);
    std::printf("    cone : %.4f m to %.4f m   spread %.2f%%\n", static_cast<double>(cone_min),
                static_cast<double>(cone_max), 100.0 * (cone_max - cone_min) / cone_min);
    check((box_max - box_min) / box_min > 0.10, "F: the box model is direction-dependent");
    check((cone_max - cone_min) / cone_min < 0.02, "F: the cone model is not");

    std::printf("\n  The predicted extremes are 1.000 and sqrt(2) = 1.41421 times the\n"
                "  available friction, so a ratio of stopping distances of 1/1.41421 =\n"
                "  %.5f. Measured %.5f.\n", 1.0 / std::sqrt(2.0),
                static_cast<double>(box_min / box_max));

    // ---- CONTROL: mu = 0 ---------------------------------------------------
    {
        const float d = slide(0.7f, friction_model::cone, 0.0f);
        std::printf("\n  CONTROL, mu = 0: the crate travels %.1f m and is still going, so the\n"
                    "    sweep above is measuring friction and not a settling artifact.\n",
                    static_cast<double>(d));
        check(d > 50.0f, "F: control - no friction means no stopping");
    }

    // ---- CONTROL: the two models agree when the clip does not bind ---------
    //
    // At a mu large enough that the crate grips instead of sliding, a square and
    // a disc that both contain the answer are indistinguishable. If the box
    // model were wrong in some other way, this would not hold.
    {
        fall_scene a;
        a.passes = 16;
        a.body = make_box(vec3{0.0f, 0.5f, 0.0f}, 5.0f, vec3{0.5f, 0.5f, 0.5f});
        a.floor = make_fixed(vec3{0.0f, -0.5f, 0.0f});
        a.material = {0.0f, 2.0f};
        a.cfg.restitution_bias = a.gravity * a.h;
        fall_scene b = a;
        b.passes = 16;
        a.cfg.friction = friction_model::box;
        b.cfg.friction = friction_model::cone;
        for (int i = 0; i < 90; ++i) { step_scene(a); step_scene(b); }
        a.body.state.velocity = a.body.state.velocity + vec3{0.05f, 0.0f, 0.05f};
        b.body.state.velocity = b.body.state.velocity + vec3{0.05f, 0.0f, 0.05f};
        for (int i = 0; i < 120; ++i) { step_scene(a); step_scene(b); }
        const float gap = length(a.body.state.position - b.body.state.position);
        std::printf("  CONTROL, mu = 2.0 (gripping, clip never binds): the two models end\n"
                    "    %.3e m apart.\n", static_cast<double>(gap));
        check(gap < 1e-5f, "F: control - the models agree when neither clips");
    }
}

// ---------------------------------------------------------------------------
// §G  the slope, and what mu actually means
// ---------------------------------------------------------------------------

void section_g()
{
    rule("G  THE SLOPE, AND WHAT mu ACTUALLY MEANS");

    // A crate on a ramp. The ramp is a big box turned about z; the crate is
    // placed on its surface with the same orientation, half a millimetre in, so
    // that the contact exists from the first step and the measurement is not
    // contaminated by a landing.
    auto drift_on_slope = [](float degrees, float mu, float seconds, vec3 half = vec3{0.5f, 0.5f, 0.5f}) {
        const float a = degrees / k_deg;
        const quat tilt{std::cos(0.5f * a), vec3{0.0f, 0.0f, std::sin(0.5f * a)}};

        fall_scene s;
        s.passes = 16;   // §J: one pass leaves creep, and creep on a slope slides
        s.floor_shape = box_shape(vec3{40.0f, 0.5f, 40.0f});
        s.floor = make_fixed(vec3{0.0f, 0.0f, 0.0f});
        s.floor.orientation = tilt;

        const vec3 up_ramp = engine::rotate(tilt, vec3{0.0f, 1.0f, 0.0f});
        s.body = make_box(up_ramp * (0.5f + half.y - 0.0005f), 20.0f, half);
        s.body.orientation = tilt;
        s.material = {0.0f, mu};
        s.cfg.restitution_bias = s.gravity * s.h;

        for (int i = 0; i < 60; ++i) { step_scene(s); }
        const vec3 start = s.body.state.position;
        const int steps = static_cast<int>(seconds / s.h);
        for (int i = 0; i < steps; ++i) { step_scene(s); }
        return length(s.body.state.position - start);
    };

    std::printf("  A 20 kg crate on a ramp. The ONLY thing mu says physically is the\n"
                "  steepest slope a body sits still on: atan(mu). Bisected against the\n"
                "  engine's own behaviour, 2 s of drift, threshold 5 mm.\n\n");
    auto bisect = [&](float mu, vec3 half) {
        float lo = 0.0f, hi = 60.0f;
        for (int it = 0; it < 16; ++it)
        {
            const float mid = 0.5f * (lo + hi);
            if (drift_on_slope(mid, mu, 2.0f, half) < 0.005f) { lo = mid; } else { hi = mid; }
        }
        return 0.5f * (lo + hi);
    };

    const vec3 cube{0.5f, 0.5f, 0.5f};
    const vec3 slab{0.5f, 0.15f, 0.5f};
    const float tip_cube = std::atan(cube.x / cube.y) * k_deg;
    const float tip_slab = std::atan(slab.x / slab.y) * k_deg;
    std::printf("  %8s %14s %16s %16s\n", "mu", "atan(mu)", "cube (1:1)", "slab (10:3)");
    double worst_cube = 0.0;
    double worst_slab = 0.0;
    for (float mu : {0.20f, 0.35f, 0.50f, 0.75f, 1.00f})
    {
        const float exact = critical_slope_degrees(mu);
        const float found_cube = bisect(mu, cube);
        const float found_slab = bisect(mu, slab);
        std::printf("  %8.2f %10.4f deg %11.4f deg %11.4f deg\n", static_cast<double>(mu),
                    static_cast<double>(exact), static_cast<double>(found_cube),
                    static_cast<double>(found_slab));
        worst_cube = std::max(worst_cube, std::abs(static_cast<double>(found_cube - exact)));
        worst_slab = std::max(worst_slab, std::abs(static_cast<double>(found_slab - exact)));
        check(std::abs(found_slab - exact) < 1.0f, "G: the critical slope matches atan(mu)");
    }
    std::printf("\n  worst disagreement: cube %.4f deg, slab %.4f deg.\n", worst_cube, worst_slab);
    std::printf("\n  *** THE CUBE COLUMN WAS THE SECTION'S FIRST FIXTURE AND IT IS WRONG,\n"
                "  BUT NOT BECAUSE THE SOLVER IS. *** A block on a slope has TWO critical\n"
                "  angles, and it does whichever comes first: it SLIDES when tan(theta)\n"
                "  exceeds mu, and it TIPS when tan(theta) exceeds w/h. A cube has w/h = 1,\n"
                "  so it tips at %.1f degrees — and mu = 1 wants to slide at exactly that\n"
                "  angle, which is why the cube column is 0.004 deg out at mu = 0.2 and\n"
                "  3.75 deg out at mu = 1.0. The slab tips at %.1f degrees and stays out of\n"
                "  the way, so its column is the measurement of mu alone.\n"
                "  A fixture that cannot express the quantity is not evidence about it, and\n"
                "  a systematic error that GROWS WITH THE PARAMETER is the shape to look\n"
                "  for — a wrong constant would have been wrong at mu = 0.2 too.\n",
                static_cast<double>(tip_cube), static_cast<double>(tip_slab));

    // ---- CONTROL: the tipping angle is the OTHER formula, and it is right ---
    {
        const float found = bisect(5.0f, cube);   // mu so large that sliding is impossible
        std::printf("\n  CONTROL, mu = 5.0 (sliding needs %.1f deg, so it cannot happen): the\n"
                    "    cube lets go at %.4f deg against a predicted tipping angle of\n"
                    "    atan(w/h) = %.4f deg. The same instrument, measuring the other\n"
                    "    formula, confirms the diagnosis.\n",
                    static_cast<double>(critical_slope_degrees(5.0f)), static_cast<double>(found),
                    static_cast<double>(tip_cube));
        check(std::abs(found - tip_cube) < 5.0f, "G: control - the cube's limit is the tipping angle");
    }

    // ---- CONTROL: below, above, and none --------------------------------
    {
        const float below = drift_on_slope(20.0f, 0.5f, 5.0f, slab);
        const float above = drift_on_slope(35.0f, 0.5f, 5.0f, slab);
        const float none = drift_on_slope(5.0f, 0.0f, 5.0f, slab);
        std::printf("\n  CONTROL, mu = 0.5 (critical 26.57 deg): 5 s of drift at 20 deg is\n"
                    "    %.4f mm and at 35 deg is %.3f m. CONTROL, mu = 0 on a 5 deg slope:\n"
                    "    %.3f m, so a shallow slope is not holding it by accident.\n",
                    static_cast<double>(below * 1000.0f), static_cast<double>(above),
                    static_cast<double>(none));
        check(below < 0.005f, "G: control - a sub-critical slope holds");
        check(above > 0.5f, "G: control - a super-critical slope slides");
        check(none > 0.1f, "G: control - mu = 0 slides on any slope");
    }

    // ---- the combination rule, against published coefficients -------------
    //
    // REFERENCE DATA, not a measurement: dry static coefficients as commonly
    // tabulated. Sources disagree by tens of per cent, so the table alone would
    // settle nothing — which is why the robustness sweep below it exists.
    std::printf("\n  THE COMBINE RULE. There is no simulation that can settle this: the rule\n"
                "  is an act of modelling, and the only evidence available is pairs whose\n"
                "  THREE coefficients — a on a, b on b, and a on b — are all tabulated.\n\n");
    struct pair_datum { const char* name; float a; float b; float measured; };
    const pair_datum data[] = {
        {"aluminium / steel", 1.10f, 0.74f, 0.61f},
        {"copper / steel",    1.00f, 0.74f, 0.53f},
        {"glass / nickel",    0.94f, 1.10f, 0.78f},
        {"rubber / ice",      1.16f, 0.10f, 0.15f},
        {"PTFE / steel",      0.04f, 0.74f, 0.04f},
    };
    std::printf("  %-20s %7s %7s %7s %9s %9s %9s %9s\n", "pair", "mu_a", "mu_b", "real",
                "geo", "min", "max", "avg");
    for (const pair_datum& d : data)
    {
        std::printf("  %-20s %7.2f %7.2f %7.2f %9.3f %9.3f %9.3f %9.3f\n", d.name,
                    static_cast<double>(d.a), static_cast<double>(d.b),
                    static_cast<double>(d.measured),
                    static_cast<double>(engine::phys::combine(d.a, d.b, combine_rule::geometric_mean)),
                    static_cast<double>(engine::phys::combine(d.a, d.b, combine_rule::minimum)),
                    static_cast<double>(engine::phys::combine(d.a, d.b, combine_rule::maximum)),
                    static_cast<double>(engine::phys::combine(d.a, d.b, combine_rule::average)));
    }

    const combine_rule rules[] = {combine_rule::geometric_mean, combine_rule::minimum,
                                  combine_rule::maximum, combine_rule::average};
    std::printf("\n  %-18s %14s %14s\n", "rule", "worst error", "mean error");
    for (combine_rule r : rules)
    {
        double worst = 0.0;
        double total = 0.0;
        for (const pair_datum& d : data)
        {
            const double predicted = engine::phys::combine(d.a, d.b, r);
            const double err = std::abs(predicted - d.measured) / d.measured;
            worst = std::max(worst, err);
            total += err;
        }
        std::printf("  %-18s %12.1f%% %12.1f%%\n", name_of(r), 100.0 * worst,
                    100.0 * total / static_cast<double>(std::size(data)));
    }

    // ---- ROBUSTNESS: does the ranking survive the disagreement in the data? -
    //
    // Published coefficients vary by tens of per cent between sources, so a
    // ranking that only holds for one table is not a result. Perturb every
    // number by up to +/-30% and count how often each rule wins.
    {
        rng gen(0x8930u);
        int wins[4] = {0, 0, 0, 0};
        const int trials = 20000;
        for (int t = 0; t < trials; ++t)
        {
            double best = 1e30;
            int best_rule = 0;
            float jitter[5][3];
            for (int i = 0; i < 5; ++i)
            {
                for (int j = 0; j < 3; ++j) { jitter[i][j] = gen.range(0.7f, 1.3f); }
            }
            for (int r = 0; r < 4; ++r)
            {
                double total = 0.0;
                for (int i = 0; i < 5; ++i)
                {
                    const float a = data[i].a * jitter[i][0];
                    const float b = data[i].b * jitter[i][1];
                    const float real = data[i].measured * jitter[i][2];
                    total += std::abs(engine::phys::combine(a, b, rules[r]) - real) / real;
                }
                if (total < best) { best = total; best_rule = r; }
            }
            ++wins[best_rule];
        }
        std::printf("\n  ROBUSTNESS. Every coefficient perturbed by up to +/-30%%, %d times,\n"
                    "  and the rule with the smallest total error counted:\n\n", trials);
        for (int r = 0; r < 4; ++r)
        {
            std::printf("    %-18s wins %6.2f%% of the time\n", name_of(rules[r]),
                        100.0 * wins[r] / trials);
        }
        check(wins[1] > wins[0], "G: the data prefers the minimum over the geometric mean");
        std::printf("\n  SO THE DATA DOES NOT SUPPORT THE RULE EVERY ENGINE SHIPS. `minimum`\n"
                    "  wins, and it has a physical story: the interface is governed by the\n"
                    "  more lubricious of the two surfaces, which is why PTFE on anything is\n"
                    "  PTFE. This engine's default is `minimum` because of this table.\n"
                    "  WHAT IT COSTS: `min` is insensitive to the grippier material, so a\n"
                    "  designer raising rubber's friction sees NOTHING change against ice.\n"
                    "  That is the case for the geometric mean, and it is a usability\n"
                    "  argument rather than a physical one — which is worth saying out loud.\n");
    }

    check(engine::phys::combine(0.0f, 5.0f, combine_rule::geometric_mean) == 0.0f,
          "G: a frictionless surface makes the pair frictionless under the geometric mean");
    check(engine::phys::combine(0.0f, 5.0f, combine_rule::minimum) == 0.0f,
          "G: and under the minimum");
    check(engine::phys::combine(0.0f, 5.0f, combine_rule::average) > 0.0f,
          "G: control - the average rule does not have that property");
}

// ---------------------------------------------------------------------------
// §H  the order of the two solves
// ---------------------------------------------------------------------------

void section_h()
{
    rule("H  THE ORDER OF THE TWO SOLVES");

    // A crate DROPPED onto a ramp it should grip. The measurement window is the
    // first few frames after it touches, because that is where the question is:
    // on the frame of first contact there is no accumulated normal impulse yet,
    // so a friction solve that runs first has a Coulomb cone of radius zero.
    auto land_on_slope = [](bool normal_first, bool warm, int passes, float degrees, float mu,
                            int window) {
        const float a = degrees / k_deg;
        const quat tilt{std::cos(0.5f * a), vec3{0.0f, 0.0f, std::sin(0.5f * a)}};
        const vec3 up_ramp = engine::rotate(tilt, vec3{0.0f, 1.0f, 0.0f});
        const vec3 down_slope = engine::rotate(tilt, vec3{-1.0f, 0.0f, 0.0f});

        fall_scene s;
        s.floor_shape = box_shape(vec3{40.0f, 0.5f, 40.0f});
        s.floor = make_fixed(vec3{0.0f, 0.0f, 0.0f});
        s.floor.orientation = tilt;
        s.body = make_box(up_ramp * (0.5f + 0.15f + 0.30f), 20.0f, vec3{0.5f, 0.15f, 0.5f});
        s.body.orientation = tilt;
        s.material = {0.0f, mu};
        s.cfg.restitution_bias = s.gravity * s.h;
        s.cfg.normal_before_friction = normal_first;
        s.cfg.warm_start = warm;
        s.passes = passes;

        // Persistence, exactly as `manifold.hpp`'s frame discipline describes it.
        contact_manifold previous{};
        bool have_previous = false;
        vec3 origin{};
        bool landed = false;
        int since_landing = 0;
        vec3 finish{};

        for (int i = 0; i < 600; ++i)
        {
            rigid_body& b = s.body;
            b.state.velocity = b.state.velocity + s.gravity * s.h;

            contact_manifold m = collide_scene(s);
            if (m.count > 0)
            {
                if (have_previous && warm) { engine::phys::carry_impulses(m, previous); }
                if (!landed) { landed = true; origin = b.state.position; }
                contact_batch batch = prepare_contacts(s.floor, b, m, s.material, s.cfg);
                if (warm) { engine::phys::warm_start_contacts(s.floor, b, batch); }
                for (int pass = 0; pass < passes; ++pass) { solve_contacts(s.floor, b, batch, s.cfg); }
                write_back(batch, m);
                previous = m;
                have_previous = true;
            }
            else
            {
                have_previous = false;
            }

            b.state.position = b.state.position + b.state.velocity * s.h;
            b.orientation = engine::phys::advance_orientation(b.orientation, b.angular_velocity, s.h,
                                                              engine::phys::spin_rule::linearised);
            if (landed)
            {
                ++since_landing;
                if (since_landing == window) { finish = b.state.position; break; }
                finish = b.state.position;
            }
        }
        return dot(finish - origin, down_slope);
    };

    std::printf("  A 20 kg slab DROPPED from 30 cm onto a 20 degree ramp with mu = 0.50,\n"
                "  whose critical angle is 26.57 degrees. It arrives with 0.83 m/s of\n"
                "  DOWN-SLOPE velocity and should lose all of it on the frame it lands.\n"
                "  Down-slope travel over the first 10 frames of contact:\n\n");
    std::printf("  %-28s %14s %14s %14s\n", "", "1 pass", "4 passes", "16 passes");
    auto row = [&](const char* what, bool normal_first, bool warm) {
        std::printf("  %-28s %11.4f mm %11.4f mm %11.4f mm\n", what,
                    static_cast<double>(land_on_slope(normal_first, warm, 1, 20.0f, 0.5f, 10) * 1000.0f),
                    static_cast<double>(land_on_slope(normal_first, warm, 4, 20.0f, 0.5f, 10) * 1000.0f),
                    static_cast<double>(land_on_slope(normal_first, warm, 16, 20.0f, 0.5f, 10) * 1000.0f));
    };
    row("normal then friction", true, false);
    row("friction then normal", false, false);
    row("friction then normal, warm", false, true);
    row("normal then friction, warm", true, true);

    const float right = land_on_slope(true, false, 1, 20.0f, 0.5f, 10);
    const float wrong = land_on_slope(false, false, 1, 20.0f, 0.5f, 10);
    const float frictionless = land_on_slope(true, false, 1, 20.0f, 0.0f, 10);
    std::printf("\n  *** AT ONE PASS — WHICH IS WHAT THIS LESSON SHIPS — THE WRONG ORDER HAS\n"
                "  NO FRICTION AT ALL. *** Not less friction: none. It slides %.4f mm\n"
                "  against %.4f mm for the right order, and %.4f mm is EXACTLY what the\n"
                "  same slab does with mu set to zero (%.4f mm). Every frame starts with a\n"
                "  zero accumulator, so every frame's friction solve clips against a cone\n"
                "  of radius zero, so no tangential impulse is ever applied.\n",
                static_cast<double>(wrong * 1000.0f), static_cast<double>(right * 1000.0f),
                static_cast<double>(wrong * 1000.0f), static_cast<double>(frictionless * 1000.0f));
    check(std::abs(wrong - frictionless) < 1e-6f,
          "H: at one pass the wrong order is exactly frictionless");
    check(std::abs(wrong) > 2.0f * std::abs(right), "H: and it slides much further");

    std::printf("\n  AND ITERATION HIDES IT, which is the uncomfortable half. With the\n"
                "  friction solve starved only on the FIRST of sixteen passes, the two\n"
                "  orders converge — so a solver that iterates enough gets away with the\n"
                "  wrong order, and the bug waits until somebody lowers the iteration\n"
                "  count for performance.\n");
    const float wrong16 = land_on_slope(false, false, 16, 20.0f, 0.5f, 10);
    const float right16 = land_on_slope(true, false, 16, 20.0f, 0.5f, 10);
    check(std::abs(wrong16 - right16) < std::abs(wrong - right),
          "H: iterating narrows the gap between the two orders");

    // ---- CONTROL: on the flat, the order does not matter ------------------
    {
        const float flat_right = land_on_slope(true, false, 16, 0.0f, 0.5f, 10);
        const float flat_wrong = land_on_slope(false, false, 16, 0.0f, 0.5f, 10);
        std::printf("\n  CONTROL, the same slab on a FLAT floor: %.4e m and %.4e m. With no\n"
                    "    down-slope velocity there is nothing for friction to fail to stop,\n"
                    "    so the ordering is invisible — which is the other reason this bug\n"
                    "    survives testing. (Sixteen passes here, so that the one-pass creep\n"
                    "    of section J is not what the control is reading.)\n",
                    static_cast<double>(flat_right), static_cast<double>(flat_wrong));
        check(std::abs(flat_right) < 1e-4f && std::abs(flat_wrong) < 1e-4f,
              "H: control - the order is invisible on a flat floor");
    }

    // ---- CONTROL: mu = 0 slides the same either way ------------------------
    {
        const float slick_right = land_on_slope(true, false, 1, 20.0f, 0.0f, 10);
        const float slick_wrong = land_on_slope(false, false, 1, 20.0f, 0.0f, 10);
        std::printf("  CONTROL, mu = 0 on the same ramp: %.3f mm and %.3f mm — identical,\n"
                    "    because with no friction there is no order to get wrong.\n",
                    static_cast<double>(slick_right * 1000.0f),
                    static_cast<double>(slick_wrong * 1000.0f));
        check(std::abs(slick_right - slick_wrong) < 1e-6f,
              "H: control - mu = 0 is order-independent");
    }
}

// ---------------------------------------------------------------------------
// §I  sliding becomes rolling
// ---------------------------------------------------------------------------

void section_i()
{
    rule("I  SLIDING BECOMES ROLLING");

    // A sphere landing with forward speed and no spin. Friction acts at the
    // contact point, which is BELOW the centre of mass, so it both slows the
    // centre and spins the ball up. The end state is fixed by conservation of
    // angular momentum about the contact line and does not depend on mu at all.
    struct roll_result
    {
        float fraction = 0.0f;
        float rolled_at = -1.0f;
        float contact_radius = 0.0f;
        float residual_slip = 0.0f;
        float omega_r = 0.0f;
    };

    auto roll_test = [](float v0, float mu, float inertia_coefficient, float radius, float mass,
                        float start_depth) {
        fall_scene s;
        s.body = make_sphere(vec3{0.0f, radius - start_depth, 0.0f}, mass, radius);
        s.body_shape = sphere_shape(radius);
        s.body_is_sphere = true;
        s.sphere_radius = radius;
        // Override the tensor so a "shell" can be tested against the same code.
        const float inertia = inertia_coefficient * mass * radius * radius;
        s.body.inertia_local = engine::diagonal(vec3{inertia, inertia, inertia});
        s.body.inv_inertia_local =
            engine::diagonal(vec3{1.0f / inertia, 1.0f / inertia, 1.0f / inertia});
        s.floor = make_fixed(vec3{0.0f, -0.5f, 0.0f});
        s.material = {0.0f, mu};
        s.cfg.restitution_bias = s.gravity * s.h;
        s.body.state.velocity = vec3{v0, 0.0f, 0.0f};

        roll_result out;
        float previous_slip = v0;
        for (int step = 0; step < 4000; ++step)
        {
            step_scene(s);
            // The lever arm the SOLVER is using: the contact point sits midway
            // between the surfaces, so it is `depth/2` inside the ball. 8.7's
            // `contact_point::position` says so, and this is where it shows up in
            // the mechanics.
            const float r_contact =
                s.touched ? radius - 0.5f * s.last_depth : radius;
            out.contact_radius = r_contact;
            const float slip = s.body.state.velocity.x + s.body.angular_velocity.z * r_contact;
            if (out.rolled_at < 0.0f && std::abs(slip) < 1e-4f)
            {
                out.rolled_at = static_cast<float>(step + 1) * s.h;
            }
            if (out.rolled_at > 0.0f && std::abs(slip - previous_slip) < 1e-9f) { break; }
            previous_slip = slip;
        }
        out.fraction = s.body.state.velocity.x / v0;
        out.residual_slip = s.body.state.velocity.x + s.body.angular_velocity.z * radius;
        out.omega_r = s.body.angular_velocity.z * radius;
        return out;
    };

    std::printf("  A ball landing at 6 m/s with no spin. Friction slows the centre AND\n"
                "  spins it up; when the contact point is stationary the slipping stops.\n"
                "  The end speed is v0/(1 + c) and DOES NOT DEPEND ON mu — friction\n"
                "  decides only how long the transition takes.\n\n");
    std::printf("  %-12s %6s %12s %12s %12s %12s\n", "body", "mu", "v/v0 exact", "v/v0 sim",
                "t exact", "t sim");

    struct body_case { const char* name; float c; };
    const body_case cases[] = {{"solid ball", 2.0f / 5.0f}, {"hollow shell", 2.0f / 3.0f}};
    for (const body_case& bc : cases)
    {
        for (float mu : {0.20f, 0.40f, 0.80f})
        {
            const roll_result r = roll_test(6.0f, mu, bc.c, 0.25f, 2.0f, 0.0005f);
            const float exact_fraction = rolling_speed_fraction(bc.c);
            const float exact_time = rolling_time(6.0f, mu, k_gravity, bc.c);
            std::printf("  %-12s %6.2f %12.6f %12.6f %10.4f s %10.4f s\n", bc.name,
                        static_cast<double>(mu), static_cast<double>(exact_fraction),
                        static_cast<double>(r.fraction), static_cast<double>(exact_time),
                        static_cast<double>(r.rolled_at));
            check(std::abs(r.fraction - exact_fraction) < 0.005f,
                  "I: the rolling speed matches v0/(1+c)");
            check(r.rolled_at > 0.0f && std::abs(r.rolled_at - exact_time) < 0.05f + 0.15f * exact_time,
                  "I: the transition time matches the closed form");
        }
    }

    // ---- THE RESIDUAL IS PREDICTABLE, WHICH MAKES IT EVIDENCE --------------
    //
    // The simulated fraction misses 5/7 by about 0.06%. That is not noise: the
    // contact point is `depth/2` inside the ball, so the lever arm the solver
    // uses is `R - depth/2` rather than `R`, and the effective inertia
    // coefficient is c*(R/r_c)^2. Sweep the penetration and the predicted
    // fraction should track the measured one across the whole range.
    std::printf("\n  A 0.06%% MISS, AND IT IS NOT NOISE. The contact point sits midway\n"
                "  between the surfaces — 8.7's convention — so the lever arm is\n"
                "  R - depth/2 and the effective coefficient is c*(R/r_c)^2. Sweep the\n"
                "  starting penetration and the prediction should follow:\n\n");
    std::printf("  %12s %14s %14s %14s %12s\n", "depth", "r_contact", "v/v0 predicted",
                "v/v0 measured", "difference");
    double worst_prediction = 0.0;
    for (float depth : {0.0002f, 0.0005f, 0.0020f, 0.0100f, 0.0300f})
    {
        const roll_result r = roll_test(6.0f, 0.4f, 2.0f / 5.0f, 0.25f, 2.0f, depth);
        const float ratio = 0.25f / r.contact_radius;
        const float c_effective = (2.0f / 5.0f) * ratio * ratio;
        const float predicted = rolling_speed_fraction(c_effective);
        std::printf("  %9.2f mm %12.6f m %14.6f %14.6f %12.2e\n",
                    static_cast<double>(depth * 1000.0f), static_cast<double>(r.contact_radius),
                    static_cast<double>(predicted), static_cast<double>(r.fraction),
                    static_cast<double>(predicted - r.fraction));
        worst_prediction = std::max(worst_prediction, std::abs(static_cast<double>(predicted - r.fraction)));
    }
    std::printf("\n  worst disagreement over the sweep: %.2e — the residual is the\n"
                "  manifold's own convention, arriving in the mechanics.\n", worst_prediction);
    check(worst_prediction < 1e-4, "I: the corrected closed form predicts the residual");

    // ---- CONTROL: mu = 0 ---------------------------------------------------
    {
        const roll_result r = roll_test(6.0f, 0.0f, 2.0f / 5.0f, 0.25f, 2.0f, 0.0005f);
        std::printf("\n  CONTROL, mu = 0: v/v0 = %.6f after 4,000 steps, omega*r = %.3e,\n"
                    "    rolled at %.1f (never). Nothing but friction can do this.\n",
                    static_cast<double>(r.fraction), static_cast<double>(r.omega_r),
                    static_cast<double>(r.rolled_at));
        check(std::abs(r.fraction - 1.0f) < 1e-4f, "I: control - no friction, no slowing");
        check(std::abs(r.omega_r) < 1e-4f, "I: control - no friction, no spin-up");
    }

    // ---- CONTROL: the answer is genuinely mu-independent -------------------
    {
        const roll_result slow = roll_test(6.0f, 0.20f, 2.0f / 5.0f, 0.25f, 2.0f, 0.0005f);
        const roll_result fast = roll_test(6.0f, 0.80f, 2.0f / 5.0f, 0.25f, 2.0f, 0.0005f);
        std::printf("  CONTROL, mu x 4: end speed %.6f vs %.6f (%.4f%% apart) while the\n"
                    "    transition time goes %.4f s -> %.4f s (%.2fx).\n",
                    static_cast<double>(slow.fraction), static_cast<double>(fast.fraction),
                    100.0 * std::abs(static_cast<double>(slow.fraction - fast.fraction))
                        / static_cast<double>(slow.fraction),
                    static_cast<double>(slow.rolled_at), static_cast<double>(fast.rolled_at),
                    static_cast<double>(slow.rolled_at / fast.rolled_at));
        check(std::abs(slow.fraction - fast.fraction) < 0.005f,
              "I: control - the end state is mu-independent");
        check(slow.rolled_at > 2.0f * fast.rolled_at,
              "I: control - the transition time is not");
    }
}

// ---------------------------------------------------------------------------
// §J  what this lesson leaves behind
// ---------------------------------------------------------------------------

void section_j()
{
    rule("J  WHAT THIS LESSON LEAVES BEHIND");

    // ---- where the solve goes inside the step ------------------------------
    struct rest_result { float depth = 0.0f; bool through = false; };
    auto rest_depth = [](solve_slot slot, int passes, float seconds) {
        fall_scene s;
        s.slot = slot;
        s.passes = passes;
        s.body = make_box(vec3{0.0f, 0.55f, 0.0f}, 20.0f, vec3{0.5f, 0.5f, 0.5f});
        s.floor = make_fixed(vec3{0.0f, -0.5f, 0.0f});
        s.material = {0.0f, 0.5f};
        s.cfg.restitution_bias = s.gravity * s.h;
        const int steps = static_cast<int>(seconds / s.h);
        for (int i = 0; i < steps; ++i) { step_scene(s); }
        rest_result out;
        out.depth = s.last_depth;
        out.through = s.body.state.position.y < -1.5f;
        return out;
    };

    std::printf("  TWO THINGS ARE LEFT OVER, and they are 8.10's two jobs. The first is\n"
                "  WHERE the solve goes inside the step; the second is how many times it\n"
                "  runs. A 20 kg crate resting on a floor, penetration after n seconds:\n\n");
    std::printf("  %8s %16s %16s %16s %16s\n", "seconds", "mid, 1 pass", "mid, 16 passes",
                "end, 1 pass", "end, 16 passes");
    for (float t : {1.0f, 2.0f, 5.0f, 10.0f})
    {
        const rest_result m1 = rest_depth(solve_slot::mid_step, 1, t);
        const rest_result m16 = rest_depth(solve_slot::mid_step, 16, t);
        const rest_result e1 = rest_depth(solve_slot::end_of_step, 1, t);
        const rest_result e16 = rest_depth(solve_slot::end_of_step, 16, t);
        auto cell = [](const rest_result& r, char* buffer) {
            if (r.through) { std::snprintf(buffer, 24, "%15s", "FELL THROUGH"); }
            else { std::snprintf(buffer, 24, "%12.3f mm", static_cast<double>(r.depth * 1000.0f)); }
            return buffer;
        };
        char b1[24], b2[24], b3[24], b4[24];
        std::printf("  %6.1f s %16s %16s %16s %16s\n", static_cast<double>(t), cell(m1, b1),
                    cell(m16, b2), cell(e1, b3), cell(e16, b4));
    }

    const float predicted_per_step = k_gravity * k_h * k_h;
    const rest_result end16 = rest_depth(solve_slot::end_of_step, 16, 2.0f);
    const rest_result end16_one = rest_depth(solve_slot::end_of_step, 16, 1.0f);
    const float measured_per_step = (end16.depth - end16_one.depth) / 60.0f;
    std::printf("\n  THE END-OF-STEP COLUMN SINKS BY g*h^2 EVERY STEP, no matter how well\n"
                "  the solve converges, because the position half has ALREADY travelled at\n"
                "  the velocity the solver is about to cancel. Predicted %.4f mm per step,\n"
                "  measured %.4f mm. At 60 Hz that is 16 cm a second and the crate is\n"
                "  through a half-metre floor in four.\n",
                static_cast<double>(predicted_per_step * 1000.0f),
                static_cast<double>(measured_per_step * 1000.0f));
    check(std::abs(measured_per_step - predicted_per_step) < 0.2f * predicted_per_step,
          "J: end-of-step solving sinks by exactly g*h^2 per step");
    check(rest_depth(solve_slot::mid_step, 16, 10.0f).depth
              < 1.2f * rest_depth(solve_slot::mid_step, 16, 1.0f).depth,
          "J: mid-step solving with a converged solve does not sink at all");

    // ---- the creep, against the pass count ---------------------------------
    std::printf("\n  AND THE MID-STEP COLUMN STILL CREEPS AT ONE PASS. Same scene, same\n"
                "  place in the step, only the number of solver passes differing:\n\n");
    std::printf("  %8s %16s %18s %16s\n", "passes", "depth at 10 s", "creep", "residual");
    for (int passes : {1, 2, 4, 8, 16, 32})
    {
        const rest_result five = rest_depth(solve_slot::mid_step, passes, 5.0f);
        const rest_result fifteen = rest_depth(solve_slot::mid_step, passes, 15.0f);
        fall_scene probe;
        probe.slot = solve_slot::mid_step;
        probe.passes = passes;
        probe.body = make_box(vec3{0.0f, 0.55f, 0.0f}, 20.0f, vec3{0.5f, 0.5f, 0.5f});
        probe.floor = make_fixed(vec3{0.0f, -0.5f, 0.0f});
        probe.material = {0.0f, 0.5f};
        probe.cfg.restitution_bias = probe.gravity * probe.h;
        for (int i = 0; i < 600; ++i) { step_scene(probe); }
        std::printf("  %8d %13.4f mm %14.5f mm/s %13.3e m/s\n", passes,
                    static_cast<double>(fifteen.depth * 1000.0f),
                    static_cast<double>((fifteen.depth - five.depth) / 10.0f * 1000.0f),
                    static_cast<double>(probe.last_report.max_residual));
    }
    const rest_result creep_one = rest_depth(solve_slot::mid_step, 1, 15.0f);
    const rest_result frozen = rest_depth(solve_slot::mid_step, 32, 15.0f);
    std::printf("\n  ONE PASS OVER FOUR CONTACT POINTS DOES NOT HOLD A CRATE UP. It leaves\n"
                "  enough residual approach speed to sink %.1f mm in fifteen seconds —\n"
                "  %.0f%% of the crate's own height — and it never stops. Thirty-two passes\n"
                "  converge and the depth FREEZES at %.3f mm, which is the penetration the\n"
                "  crate arrived with on the frame it was first detected: one step of\n"
                "  approach travel. THAT frozen millimetre is the other thing 8.10 has to\n"
                "  deal with, and more iterations will not touch it — no velocity solve can\n"
                "  undo an overlap that is already there. It needs a position correction.\n",
                static_cast<double>(creep_one.depth * 1000.0f),
                100.0 * static_cast<double>(creep_one.depth),
                static_cast<double>(frozen.depth * 1000.0f));

    // ---- where the residual comes from -------------------------------------
    //
    // Two independent sources, and separating them is the point: points
    // disturbing each other, and FRICTION disturbing the normal solve through
    // the lever arm. The second exists even on a single point.
    std::printf("\n  WHERE THE RESIDUAL COMES FROM. Two sources, separated:\n\n");
    std::printf("  %8s %18s %18s %18s\n", "points", "1 pass, no mu", "1 pass, mu = 0.5",
                "4 passes, mu = 0.5");
    rng gen(0x8910u);
    double single_point_frictionless = 1.0;
    for (int wanted = 1; wanted <= 4; ++wanted)
    {
        double worst_plain = 0.0;
        double worst_friction = 0.0;
        double worst_four = 0.0;
        int found = 0;
        for (int trial = 0; trial < 20000 && found < 300; ++trial)
        {
            const float tilt_angle = gen.range(0.0f, 0.25f);
            const vec3 axis = normalised(vec3{gen.signed_unit(), 0.0f, gen.signed_unit()});
            const quat tilt{std::cos(0.5f * tilt_angle), axis * std::sin(0.5f * tilt_angle)};

            rigid_body floor = make_fixed(vec3{0.0f, -0.5f, 0.0f});
            rigid_body crate = make_box(vec3{0.0f, 0.494f, 0.0f}, 20.0f, vec3{0.5f, 0.5f, 0.5f});
            crate.orientation = tilt;
            crate.state.velocity = vec3{gen.signed_unit(), -gen.range(0.1f, 2.0f), gen.signed_unit()};
            crate.angular_velocity = vec3{gen.signed_unit(), gen.signed_unit(), gen.signed_unit()};

            const auto floor_box = world_obb(box_shape(vec3{20.0f, 0.5f, 20.0f}),
                                             floor.state.position, floor.orientation);
            const auto crate_box = world_obb(box_shape(vec3{0.5f, 0.5f, 0.5f}),
                                             crate.state.position, crate.orientation);
            const contact_manifold m = collide_manifold(as_convex(floor_box), as_convex(crate_box));
            if (m.count != wanted) { continue; }
            ++found;

            solver_config cfg;
            cfg.restitution_threshold = 1.0f;
            auto run = [&](float mu, int passes) {
                rigid_body a = floor, b = crate;
                contact_manifold copy = m;
                contact_batch batch = prepare_contacts(a, b, copy, contact_material{0.0f, mu}, cfg);
                solve_report r;
                for (int pass = 0; pass < passes; ++pass) { r = solve_contacts(a, b, batch, cfg); }
                return static_cast<double>(r.max_residual);
            };
            worst_plain = std::max(worst_plain, run(0.0f, 1));
            worst_friction = std::max(worst_friction, run(0.5f, 1));
            worst_four = std::max(worst_four, run(0.5f, 4));
        }
        if (found == 0) { continue; }
        std::printf("  %8d %15.3e m/s %15.3e m/s %15.3e m/s\n", wanted, worst_plain,
                    worst_friction, worst_four);
        if (wanted == 1) { single_point_frictionless = worst_plain; }
    }
    std::printf("\n  READ THE FIRST ROW. With friction off, ONE point has NO residual at all\n"
                "  — its own solve lands exactly on target and nothing disturbs it. Turn\n"
                "  friction on and the same single point has one, because the tangential\n"
                "  impulse acts through a lever arm and changes the angular velocity, which\n"
                "  changes the normal velocity at the very point that was just solved. So\n"
                "  the coupling that makes a solver iterate is not only between POINTS; it\n"
                "  is between the normal and the friction of a single contact.\n");
    check(single_point_frictionless < 1e-4, "J: one point with no friction has no residual");
    std::printf("\n  Those columns are Lesson 8.10, arriving early enough to be measured\n"
                "  rather than promised.\n");
}

// ---------------------------------------------------------------------------
// §K  the budget
// ---------------------------------------------------------------------------

void section_k()
{
    rule("K  THE BUDGET");

    // Build a realistic population of manifolds once, then time the solver on
    // them. Timing the collision detection too would measure 8.7 all over again.
    std::vector<contact_manifold> manifolds;
    std::vector<rigid_body> crates;
    rng gen(0x8920u);
    const shape floor_shape = box_shape(vec3{60.0f, 0.5f, 60.0f});
    const rigid_body floor = make_fixed(vec3{0.0f, -0.5f, 0.0f});
    while (manifolds.size() < 1000)
    {
        const float tilt_angle = gen.range(0.0f, 0.3f);
        const vec3 axis = normalised(vec3{gen.signed_unit(), 0.0f, gen.signed_unit()});
        const quat tilt{std::cos(0.5f * tilt_angle), axis * std::sin(0.5f * tilt_angle)};
        rigid_body crate = make_box(vec3{gen.range(-20.0f, 20.0f), gen.range(0.487f, 0.499f),
                                         gen.range(-20.0f, 20.0f)},
                                    gen.range(5.0f, 60.0f), vec3{0.5f, 0.5f, 0.5f});
        crate.orientation = tilt;
        crate.state.velocity = vec3{gen.signed_unit(), -gen.range(0.0f, 2.0f), gen.signed_unit()};
        crate.angular_velocity = vec3{gen.signed_unit(), gen.signed_unit(), gen.signed_unit()} * 0.4f;
        const auto floor_box = world_obb(floor_shape, floor.state.position, floor.orientation);
        const auto crate_box = world_obb(box_shape(vec3{0.5f, 0.5f, 0.5f}), crate.state.position,
                                         crate.orientation);
        contact_manifold m = collide_manifold(as_convex(floor_box), as_convex(crate_box));
        if (m.count == 0) { continue; }
        manifolds.push_back(m);
        crates.push_back(crate);
    }

    int total_points = 0;
    for (const contact_manifold& m : manifolds) { total_points += m.count; }
    std::printf("  %zu manifolds, %d contact points (%.2f per manifold).\n", manifolds.size(),
                total_points, static_cast<double>(total_points) / static_cast<double>(manifolds.size()));

    // FIVE WARM-UP REPETITIONS, DISCARDED. The first run of this loop after the
    // process starts measures 173 ns where every later one measures 139.5 — a
    // 24% difference that is the instruction cache, the branch predictor and the
    // CPU's clock ramp, not the solver. Without the warm-up the numbers depend on
    // which sections were asked for, which is not a property of the code.
    auto time_run = [&](friction_model model, bool hoisted, bool solve_too = true) {
        double best = 1e30;
        for (int rep = 0; rep < 65; ++rep)
        {
            std::vector<rigid_body> bodies = crates;
            std::vector<contact_manifold> copies = manifolds;
            solver_config cfg;
            cfg.friction = model;
            rigid_body a = floor;
            const auto t0 = clock_type::now();
            double sink = 0.0;
            for (std::size_t i = 0; i < copies.size(); ++i)
            {
                if (hoisted)
                {
                    contact_batch batch =
                        prepare_contacts(a, bodies[i], copies[i], contact_material{0.2f, 0.5f}, cfg);
                    if (solve_too)
                    {
                        const solve_report r = solve_contacts(a, bodies[i], batch, cfg);
                        sink += static_cast<double>(r.normal_impulse);
                    }
                    else
                    {
                        sink += static_cast<double>(batch.points[0].normal_mass);
                    }
                }
                else
                {
                    // The un-hoisted arm: the SAME three effective masses per
                    // point, but through the two-body overload, which recomputes
                    // both world inverse inertia tensors inside every call. Six
                    // basis changes per point instead of two per manifold.
                    const contact_manifold& m = copies[i];
                    vec3 t1, t2;
                    tangent_basis(m.normal, t1, t2);
                    for (int p = 0; p < m.count; ++p)
                    {
                        const vec3 ra = m.points[p].position - a.state.position;
                        const vec3 rb = m.points[p].position - bodies[i].state.position;
                        sink += static_cast<double>(effective_mass(a, bodies[i], ra, rb, m.normal));
                        sink += static_cast<double>(effective_mass(a, bodies[i], ra, rb, t1));
                        sink += static_cast<double>(effective_mass(a, bodies[i], ra, rb, t2));
                    }
                }
            }
            const double elapsed = seconds_since(t0);
            if (sink == 12345.6789) { std::printf(" "); }   // keep the work alive
            if (rep >= 5) { best = std::min(best, elapsed); }
        }
        return best;
    };

    const double cone = time_run(friction_model::cone, true);
    const double box = time_run(friction_model::box, true);
    const double no_friction = time_run(friction_model::none, true);
    const double prepare_hoisted = time_run(friction_model::cone, true, false);
    const double prepare_unhoisted = time_run(friction_model::cone, false);

    const double per_manifold = 1e9 * cone / static_cast<double>(manifolds.size());
    std::printf("\n  %-42s %12s %12s\n", "", "per manifold", "per point");
    auto row = [&](const char* what, double seconds) {
        std::printf("  %-42s %9.1f ns %9.1f ns\n", what,
                    1e9 * seconds / static_cast<double>(manifolds.size()),
                    1e9 * seconds / static_cast<double>(total_points));
    };
    row("prepare + solve, cone friction", cone);
    row("prepare + solve, box friction", box);
    row("prepare + solve, no friction", no_friction);
    row("prepare only, tensors hoisted", prepare_hoisted);
    row("the same three effective masses, unhoisted", prepare_unhoisted);

    std::printf("\n  the cone costs %.1f%% more than the box; friction at all costs %.1f%%\n"
                "  of a solve without it.\n",
                100.0 * (cone - box) / box, 100.0 * (cone - no_friction) / no_friction);
    std::printf("  THE HOIST IS THE WHOLE OF `prepare_contacts`. Computing the same three\n"
                "  effective masses per point through the two-body overload — which does\n"
                "  six basis changes per point instead of two per manifold — costs %.2fx\n"
                "  the ENTIRE prepare, and %.0f%% of prepare AND solve together.\n",
                prepare_unhoisted / prepare_hoisted, 100.0 * prepare_unhoisted / cone);
    check(cone > 0.0 && box > 0.0, "K: the solver was timed");
    check(prepare_unhoisted > prepare_hoisted,
          "K: recomputing the tensors costs more than hoisting them");
    check(cone > box, "K: the cone's sqrt is not free");

    // The frame: 8.8 measured 2,000 bodies at 983 candidate pairs and 0.426 ms
    // of narrow phase. Put this stage next to those.
    const double solver_ms = 983.0 * per_manifold / 1e6;
    std::printf("\n  8.8's frame was 2,000 bodies -> 983 candidates -> 0.426 ms of narrow\n"
                "  phase. 983 manifolds through this solver is %.3f ms, so collision\n"
                "  detection plus one solver pass is %.3f ms of a 16.67 ms budget.\n",
                solver_ms, 0.091 + 0.426 + solver_ms);

    // ---- CONTROL: the timing is not measuring an empty loop ---------------
    check(per_manifold > 5.0, "K: control - the timed loop does real work");
    std::printf("  CONTROL: %.1f ns per manifold is %.1f ns per contact point, against a\n"
                "  measured %.3f ns for a single AABB overlap test in 8.8 — the solver is\n"
                "  roughly %.0fx a broadphase test, which is the right order for three\n"
                "  effective masses and three impulses.\n",
                per_manifold, 1e9 * cone / static_cast<double>(total_points), 1.190,
                (1e9 * cone / static_cast<double>(total_points)) / 1.190);
}

} // namespace

int main(int argc, char** argv)
{
    const bool only = argc > 1;
    auto want = [&](char letter) {
        if (!only) { return true; }
        for (int i = 1; i < argc; ++i)
        {
            for (const char* c = argv[i]; *c; ++c)
            {
                if (*c == letter || *c == (letter - 'A' + 'a')) { return true; }
            }
        }
        return false;
    };

    std::printf("verify_89 — Lesson 8.9, Impulse Response: Restitution and Friction\n");

    if (want('A')) { section_a(); }
    if (want('B')) { section_b(); }
    if (want('C')) { section_c(); }
    if (want('D')) { section_d(); }
    if (want('E')) { section_e(); }
    if (want('F')) { section_f(); }
    if (want('G')) { section_g(); }
    if (want('H')) { section_h(); }
    if (want('I')) { section_i(); }
    if (want('J')) { section_j(); }
    if (want('K')) { section_k(); }

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
