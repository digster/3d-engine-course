// scratch/verify_813.cpp — every number Lesson 8.13 prints, measured rather
// than asserted.
//
// Build and run:  sh scratch/build_verify_813.sh
//
// Eleven sections, in the lesson's order:
//
//   A  the problem: a character that obeys Newton
//   B  the one query: a cast is Newton's method
//   C  collide and slide
//   D  slopes
//   E  ground, and the round bottom
//   F  snapping to the ground
//   G  stepping up
//   H  tunnelling, revisited
//   I  standing on things that move
//   J  one owner: pushing, and being pushed
//   K  the bill
//
// Section A is the lesson's §1; B to K are §3 to §12. The text a section
// PRINTS uses the lesson's numbers, and the comments use the letters.
//
// EVERY SECTION CARRIES A CONTROL, in the two halves 8.10 settled: what would
// the control say if the thing were COMPLETELY BROKEN, and what would it say if
// it were completely FINE. Seventeen times in Module 8 a section written to
// confirm a claim has refused it instead, so each one here is written to be
// allowed to.
//
// TWO CHARACTERS ARE MEASURED. The CONTROLLER is `phys/character.hpp`, driven
// the way the demo drives it: the game keeps a velocity, gravity accumulates
// while airborne, a grounded character's vertical velocity is zero, and the
// displacement handed to `move_character` is that velocity times the step. The
// RIGID CAPSULE is what §1 argues against: a dynamic capsule with its rotation
// locked, on 8.10's solver, driven the way a naive game drives one — its
// horizontal velocity set from the stick every step.
//
// TIMINGS ARE MINIMA over many repetitions, as in 8.8 through 8.12.

#include <engine/math/mat3.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/broadphase.hpp>
#include <engine/phys/cast.hpp>
#include <engine/phys/character.hpp>
#include <engine/phys/collide.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/gjk.hpp>
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
#include <cstring>
#include <vector>

using engine::cross;
using engine::dot;
using engine::length;
using engine::length_squared;
using engine::mat3;
using engine::normalised;
using engine::quat;
using engine::quat_from_axis_angle;
using engine::rotate;
using engine::vec3;

using namespace engine::phys;

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

/// The same deterministic generator 8.1–8.12 used, for the same reason.
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
        for (;;)
        {
            const vec3 v{signed_unit(), signed_unit(), signed_unit()};
            const float l2 = length_squared(v);
            if (l2 > 1e-4f && l2 <= 1.0f) { return v / std::sqrt(l2); }
        }
    }

    quat rotation() { return quat_from_axis_angle(direction(), range(0.0f, 3.14159f)); }

private:
    std::uint32_t state_;
};

constexpr float k_h = 1.0f / 60.0f;
constexpr float k_g = 9.81f;
constexpr double k_pi = 3.14159265358979323846;
constexpr float k_pif = 3.14159265358979323846f;

[[nodiscard]] float rad(float degrees) { return degrees * (k_pif / 180.0f); }
[[nodiscard]] double deg(double radians) { return radians * (180.0 / k_pi); }

using clock_type = std::chrono::steady_clock;

[[nodiscard]] double seconds_since(clock_type::time_point t0)
{
    return std::chrono::duration<double>(clock_type::now() - t0).count();
}

// ---------------------------------------------------------------------------
// A scene: 8.10's pipeline, unchanged, with the controller beside it
// ---------------------------------------------------------------------------

struct scene
{
    body_world world;
    std::vector<shape> shapes;
    uniform_grid grid;
    manifold_cache cache;
    constraint_solver solver;

    solver_config cfg{};
    sleep_config sleep{};
    broadphase_config bp{};
    manifold_config mf{};
    contact_material material{0.0f, 0.6f};
    float h = k_h;

    std::vector<proxy> proxies;
    std::vector<contact_manifold> manifolds;
    std::vector<std::uint64_t> keys;
    std::vector<std::uint32_t> pair_a;
    std::vector<std::uint32_t> pair_b;

    scene() { bp.cell_size = 1.0f; }

    std::uint32_t add(const rigid_body& b, const shape& s)
    {
        const auto index = static_cast<std::uint32_t>(world.size());
        world.add(b);
        shapes.push_back(s);
        return index;
    }

    [[nodiscard]] rigid_body& body(std::uint32_t i) { return world.bodies()[i]; }

    void collide_all()
    {
        auto bodies = world.bodies();
        proxies.clear();
        for (std::size_t i = 0; i < bodies.size(); ++i)
        {
            proxies.push_back(proxy{bounds_of(shapes[i], bodies[i].state.position, bodies[i].orientation),
                                    static_cast<std::uint32_t>(i)});
        }
        grid.build(proxies, bp);

        cache.begin_frame();
        manifolds.clear();
        keys.clear();
        pair_a.clear();
        pair_b.clear();
        for (const broadphase_pair& p : grid.pairs())
        {
            const rigid_body& a = bodies[p.a];
            const rigid_body& b = bodies[p.b];
            if (a.kind != body_kind::dynamic && b.kind != body_kind::dynamic) { continue; }
            const placed_shape x = place(shapes[p.a], a.state.position, a.orientation);
            const placed_shape y = place(shapes[p.b], b.state.position, b.orientation);
            contact_manifold m = collide_manifold(x.view(), y.view(), mf);
            if (m.count == 0) { continue; }
            const std::uint64_t key = pair_key(p.a, p.b);
            if (const contact_manifold* previous = cache.find(key)) { carry_impulses(m, *previous); }
            manifolds.push_back(m);
            keys.push_back(key);
            pair_a.push_back(p.a);
            pair_b.push_back(p.b);
        }
    }

    /// One physics step: 8.10's order, exactly.
    void step()
    {
        cfg.restitution_bias = world.gravity() * h;
        world.integrate_velocities(h);
        collide_all();
        solver.begin(world.bodies());
        for (std::size_t i = 0; i < manifolds.size(); ++i)
        {
            solver.add(pair_a[i], pair_b[i], manifolds[i], material);
        }
        (void)solver.solve(h, cfg, sleep);
        for (std::size_t k = 0; k < manifolds.size(); ++k) { cache.store(keys[k], manifolds[k]); }
        cache.end_frame();
        world.integrate_positions(h);
    }
};

std::uint32_t add_floor(scene& s, float half_extent = 60.0f)
{
    return s.add(make_fixed(vec3{0.0f, -0.5f, 0.0f}), box_shape(vec3{half_extent, 0.5f, half_extent}));
}

/// A fixed box.
std::uint32_t add_block(scene& s, vec3 centre, vec3 half, quat q = quat{})
{
    rigid_body b = make_fixed(centre);
    b.orientation = q;
    return s.add(b, box_shape(half));
}

/// A slab tilted about z by `angle`, rising toward +x, whose top face passes
/// through `top` at its middle. Downhill is −x.
std::uint32_t add_ramp(scene& s, float angle, vec3 top, float half_length = 8.0f, float half_width = 3.0f)
{
    const quat q = engine::quat_z(angle);
    const vec3 half{half_length, 0.5f, half_width};
    const vec3 centre = top - rotate(q, vec3{0.0f, half.y, 0.0f});
    return add_block(s, centre, half, q);
}

// ---------------------------------------------------------------------------
// The controller, driven as the demo drives it
// ---------------------------------------------------------------------------

/// The capsule's centre height when it stands on a floor at `y`.
[[nodiscard]] float standing_y(const character_config& cfg, float floor_y = 0.0f)
{
    return floor_y + cfg.skin + cfg.radius + cfg.half_height;
}

struct walker
{
    character ch{};
    character_config cfg{};
    vec3 vel{};                    ///< The GAME's velocity: design, not physics.
    std::uint32_t proxy = k_no_body;
    move_report last{};
    float jump_height = 0.0f;      ///< Set to jump on the next grounded frame.

    [[nodiscard]] character_world view(scene& s) const
    {
        character_world w;
        w.bodies = s.world.bodies();
        w.shapes = std::span<const shape>{s.shapes};
        w.self = proxy;
        w.spin = s.world.spin();
        return w;
    }

    /// Stand it on the floor under `at`: placed, probed, grounded.
    void place_at(scene& s, vec3 at)
    {
        ch = character{};
        ch.position = at;
        ch.grounded = true;
        vel = vec3{};
        last = move_character(ch, vec3{0.0f, -k_g * k_h * k_h, 0.0f}, k_h, view(s), cfg);
    }

    /// One frame of game logic: the stick sets the horizontal velocity, gravity
    /// accumulates while airborne, a grounded character does not fall.
    void frame(scene& s, vec3 wish)
    {
        if (ch.grounded)
        {
            vel.y = 0.0f;
            if (jump_height > 0.0f)
            {
                vel.y = jump_speed(k_g, jump_height);
                jump_height = 0.0f;
            }
        }
        vel.x = wish.x;
        vel.z = wish.z;
        vel.y -= k_g * k_h;
        last = move_character(ch, vel * k_h, k_h, view(s), cfg);
        if (ch.grounded && vel.y < 0.0f) { vel.y = 0.0f; }
        if (last.hit_ceiling && vel.y > 0.0f) { vel.y = 0.0f; }
    }
};

// ---------------------------------------------------------------------------
// The rigid capsule §1 argues against
// ---------------------------------------------------------------------------

constexpr float k_person_mass = 80.0f;

/// A dynamic capsule the size of the controller's, its rotation LOCKED — a zero
/// inverse inertia, which is what every engine's "freeze rotation" box does.
std::uint32_t add_rigid_person(scene& s, vec3 at, const character_config& cfg)
{
    rigid_body b = make_dynamic(at, k_person_mass);
    b.inv_inertia_local = mat3{vec3{}, vec3{}, vec3{}};
    b.allow_sleep = false;
    return s.add(b, capsule_shape(cfg.radius, cfg.half_height));
}

// ---------------------------------------------------------------------------
// A: the problem — a character that obeys Newton
// ---------------------------------------------------------------------------

void section_a()
{
    rule("§1  THE PROBLEM: A CHARACTER THAT OBEYS NEWTON");
    const character_config cfg{};

    // --- A1: a 30° ramp, standing still ----------------------------------
    std::printf("  standing still on a 30 deg ramp for 2 s (after 0.5 s to settle)\n");
    std::printf("    %-24s %12s %12s\n", "", "slid (m)", "predicted");
    const float alpha = rad(30.0f);
    for (float mu : {0.5f, 0.6f})
    {
        scene s;
        s.material.friction = mu;
        (void)add_ramp(s, alpha, vec3{0.0f, 0.0f, 0.0f}, 12.0f);
        const vec3 n{-std::sin(alpha), std::cos(alpha), 0.0f};
        const vec3 at = n * (cfg.radius + cfg.half_height + 0.001f);
        const std::uint32_t p = add_rigid_person(s, at, cfg);
        for (int i = 0; i < 30; ++i) { s.step(); }
        const vec3 x0 = s.body(p).state.position;
        for (int i = 0; i < 120; ++i) { s.step(); }
        const float slid = length(s.body(p).state.position - x0);
        const float a = k_g * (std::sin(alpha) - mu * std::cos(alpha));
        const float t0 = 0.5f;
        const float pred = a > 0.0f ? 0.5f * a * ((t0 + 2.0f) * (t0 + 2.0f) - t0 * t0) : 0.0f;
        std::printf("    rigid capsule, mu = %.1f %12.4f %12.4f\n", static_cast<double>(mu),
                    static_cast<double>(slid), static_cast<double>(pred));
        if (mu < 0.55f) { check(std::fabs(slid - pred) < 0.05f * pred, "A1: mu 0.5 slides at g(sin a - mu cos a)"); }
        else { check(slid < 1e-3f, "A1: mu 0.6 holds on 30 deg (tan 30 = 0.577)"); }
    }
    {
        scene s;
        (void)add_ramp(s, alpha, vec3{0.0f, 0.0f, 0.0f}, 12.0f);
        walker w;
        w.place_at(s, vec3{0.0f, 1.5f, 0.0f});
        for (int i = 0; i < 30; ++i) { w.frame(s, vec3{}); }
        const vec3 x0 = w.ch.position;
        for (int i = 0; i < 120; ++i) { w.frame(s, vec3{}); }
        const float slid = length(w.ch.position - x0);
        std::printf("    %-24s %12.4f %12s\n", "the controller", static_cast<double>(slid), "0 (no mu)");
        check(slid < 1e-4f && w.ch.grounded, "A1: the controller stands on 30 deg with no friction at all");
    }

    // --- A2: the wall hang ------------------------------------------------
    // A capsule in the air, its horizontal velocity SET toward a wall every
    // step, as a stick sets it. Each step the contact must cancel `v_push`, so
    // its normal impulse is m·v_push, and friction may then hold up to
    // mu·m·v_push against gravity's m·g·h. It hangs when mu·v_push ≥ g·h.
    std::printf("\n  pushing into a wall in mid-air (mu = 0.6): speed after 1 s of falling\n");
    const float mu_wall = 0.6f;
    auto hang = [&](float push, float mu) {
        scene s;
        s.material.friction = mu;
        (void)add_block(s, vec3{1.25f, 5.0f, 0.0f}, vec3{0.25f, 6.0f, 4.0f});
        const std::uint32_t p = add_rigid_person(s, vec3{1.0f - cfg.radius - 0.002f, 5.0f, 0.0f}, cfg);
        float vy = 0.0f;
        for (int i = 0; i < 60; ++i)
        {
            s.body(p).state.velocity.x = push;
            s.step();
            vy = s.body(p).state.velocity.y;
        }
        return vy;
    };
    const float threshold = k_g * k_h / mu_wall;
    std::printf("    %-34s %12s\n", "", "v_y (m/s)");
    for (float push : {3.0f, 1.0f, 0.5f, 0.2f})
    {
        const float vy = hang(push, mu_wall);
        std::printf("    rigid, stick %.1f m/s into the wall %12.4f\n", static_cast<double>(push),
                    static_cast<double>(vy));
    }
    const float vy_free = hang(3.0f, 0.0f);
    std::printf("    rigid, mu = 0 (the wall's fix)     %12.4f   free fall: %.4f\n", static_cast<double>(vy_free),
                static_cast<double>(-k_g * 1.0f));
    // Bisect the stick speed at which it starts to hang.
    float lo = 0.05f;
    float hi = 1.0f;
    for (int it = 0; it < 30; ++it)
    {
        const float mid = 0.5f * (lo + hi);
        if (hang(mid, mu_wall) > -0.05f) { hi = mid; }
        else { lo = mid; }
    }
    std::printf("    hangs above a stick speed of %.4f m/s; predicted g*h/mu = %.4f\n", static_cast<double>(hi),
                static_cast<double>(threshold));
    check(hang(3.0f, mu_wall) > -0.01f, "A2: a rigid capsule pushed into a wall hangs there");
    check(std::fabs(hi - threshold) < 0.01f * threshold + 2e-3f, "A2: the hang threshold is g*h/mu");
    check(std::fabs(vy_free + k_g) < 0.02f, "A2: with mu = 0 it falls freely");
    {
        scene s;
        (void)add_floor(s);
        (void)add_block(s, vec3{1.25f, 5.0f, 0.0f}, vec3{0.25f, 6.0f, 4.0f});
        walker w;
        w.ch.position = vec3{1.0f - cfg.radius - cfg.skin, 8.0f, 0.0f};
        for (int i = 0; i < 60; ++i) { w.frame(s, vec3{3.0f, 0.0f, 0.0f}); }
        std::printf("    the controller, stick 3 m/s          %12.4f   (fell %.4f m; free fall %.4f)\n",
                    static_cast<double>(w.vel.y), static_cast<double>(8.0f - w.ch.position.y),
                    static_cast<double>(0.5f * k_g * 61.0f * 60.0f * k_h * k_h));
        check(std::fabs(w.vel.y + k_g) < 0.01f && std::fabs(8.0f - w.ch.position.y - 0.5f * k_g * 61.0f * 60.0f * k_h * k_h) < 1e-3f,
              "A2: the controller slides down the wall in free fall");
    }

    // --- A3: letting go of the stick ---------------------------------------
    std::printf("\n  letting go of the stick at 6 m/s on flat ground (mu = 0.6)\n");
    {
        scene s;
        (void)add_floor(s);
        const std::uint32_t p = add_rigid_person(s, vec3{0.0f, cfg.radius + cfg.half_height, 0.0f}, cfg);
        for (int i = 0; i < 30; ++i)
        {
            s.body(p).state.velocity.x = 6.0f;
            s.step();
        }
        const float x0 = s.body(p).state.position.x;
        int steps = 0;
        while (s.body(p).state.velocity.x > 1e-4f && steps < 600)
        {
            s.step();
            ++steps;
        }
        const float slid = s.body(p).state.position.x - x0;
        const float pred = stopping_distance(6.0f, 0.6f, k_g);
        // The discrete stop: the last driven step already lost mu*g*h, and each
        // step after it moves h*v at the velocity it ENDS with (8.1's rule).
        const double dv = 0.6 * static_cast<double>(k_g) * static_cast<double>(k_h);
        double v = 6.0 - dv;
        double sum = 0.0;
        while (v - dv > 0.0) { v -= dv; sum += v * static_cast<double>(k_h); }
        std::printf("    rigid capsule slides %.4f m in %.3f s\n", static_cast<double>(slid),
                    static_cast<double>(steps) * static_cast<double>(k_h));
        std::printf("    v^2/(2 mu g) = %.4f m; the sum of the steps, from where the stick let go = %.4f m\n",
                    static_cast<double>(pred), sum);
        check(std::fabs(static_cast<double>(slid) - sum) < 2e-3, "A3: the rigid capsule's stop is the discrete v^2/(2 mu g)");
    }
    {
        scene s;
        (void)add_floor(s);
        walker w;
        w.place_at(s, vec3{0.0f, standing_y(cfg), 0.0f});
        for (int i = 0; i < 30; ++i) { w.frame(s, vec3{6.0f, 0.0f, 0.0f}); }
        const float x0 = w.ch.position.x;
        for (int i = 0; i < 30; ++i) { w.frame(s, vec3{}); }
        const float slid = w.ch.position.x - x0;
        std::printf("    the controller stops in %.4f m: the stick said stop\n", static_cast<double>(slid));
        check(slid < 1e-4f, "A3: the controller stops when the stick stops");
    }

    // --- A4: a 30 cm step --------------------------------------------------
    std::printf("\n  walking into a ledge (stick speed held every step, mu = 0.6), 3 s\n");
    std::printf("    %-10s", "ledge");
    const float speeds[] = {0.5f, 1.0f, 2.0f, 3.0f, 5.0f, 8.0f};
    for (float v : speeds) { std::printf(" %7.1f", static_cast<double>(v)); }
    std::printf("   m/s\n");
    int speed_dependent = 0;
    for (float ledge : {0.10f, 0.20f, 0.30f})
    {
        std::printf("    %4.2f m   ", static_cast<double>(ledge));
        int up = 0;
        for (float v : speeds)
        {
            scene s;
            (void)add_floor(s);
            (void)add_block(s, vec3{4.0f, 0.5f * ledge, 0.0f}, vec3{2.0f, 0.5f * ledge, 3.0f});
            const std::uint32_t p = add_rigid_person(s, vec3{0.0f, cfg.radius + cfg.half_height, 0.0f}, cfg);
            float peak = 0.0f;
            for (int i = 0; i < 180; ++i)
            {
                s.body(p).state.velocity.x = v;
                s.step();
                peak = std::max(peak, s.body(p).state.position.y - (cfg.radius + cfg.half_height));
            }
            const bool on_top = s.body(p).state.position.x > 2.0f + cfg.radius &&
                                s.body(p).state.position.y > ledge + cfg.half_height;
            up += on_top ? 1 : 0;
            std::printf(" %7s", on_top ? "up" : "no");
        }
        std::printf("\n");
        if (up > 0 && up < 6) { ++speed_dependent; }
    }
    std::printf("    (the controller, step_height 0.3: \"up\" at every speed — §8)\n");
    check(speed_dependent >= 1, "A4: whether a rigid capsule climbs a ledge depends on its speed");

    // --- A5: the jump a designer asks for ------------------------------------
    {
        scene s;
        (void)add_floor(s);
        walker w;
        w.place_at(s, vec3{0.0f, standing_y(cfg), 0.0f});
        const float height = 1.2f;
        const float v0 = jump_speed(k_g, height);
        w.jump_height = height;
        float peak = 0.0f;
        int airborne = 0;
        for (int i = 0; i < 90; ++i)
        {
            w.frame(s, vec3{});
            peak = std::max(peak, w.ch.position.y - standing_y(cfg));
            if (!w.ch.grounded) { ++airborne; }
        }
        // Semi-implicit Euler: the first step already moves at v0 - g h, so the
        // apex is the sum of (v0 - k g h) h while that is positive.
        double apex = 0.0;
        double vy = static_cast<double>(v0);
        for (;;)
        {
            vy -= static_cast<double>(k_g) * static_cast<double>(k_h);
            if (vy <= 0.0) { break; }
            apex += vy * static_cast<double>(k_h);
        }
        std::printf("\n  a jump the design asks to be %.2f m: launched at sqrt(2 g H) = %.4f m/s\n",
                    static_cast<double>(height), static_cast<double>(v0));
        std::printf("    apex %.4f m (the steps sum to %.4f; v0^2/2g - v0 h/2 = %.4f), %d frames in the air\n",
                    static_cast<double>(peak), apex,
                    static_cast<double>(height - 0.5f * v0 * k_h), airborne);
        check(std::fabs(static_cast<double>(peak) - apex) < 1e-3, "A5: the apex is the discrete sum, not H");
    }
}

// ---------------------------------------------------------------------------
// B: the one query
// ---------------------------------------------------------------------------

/// The reference a cast is checked against: the FIRST time the gap falls to
/// `skin`, found without Newton. `f` is convex, so its minimum on [0, 1] is
/// found by golden section; if the minimum is above `skin` the path is clear,
/// and otherwise the crossing lies in [0, t_min] and bisection finds it.
struct reference_toi
{
    bool hit = false;
    float t = 1.0f;
};

/// The gap at `t`, measured TIGHTLY — GJK at a thousandth of its default
/// tolerance — because this is the instrument the cast is judged by, and an
/// instrument as loose as the thing it measures cannot see its errors.
[[nodiscard]] float gap_at(const convex& mover, vec3 d, const convex& obstacle, float t)
{
    convex at = mover;
    at.origin = mover.origin + d * t;
    gjk_config tight;
    tight.tolerance = 1e-7f;
    tight.max_iterations = 256;
    const gjk_result g = gjk_distance(at, obstacle, tight);
    return g.status == gjk_status::separated ? g.distance : -1.0f;
}

[[nodiscard]] reference_toi reference(const convex& mover, vec3 d, const convex& obstacle, float skin)
{
    reference_toi out;
    double a = 0.0;
    double b = 1.0;
    const double phi = 0.6180339887498949;
    for (int i = 0; i < 60; ++i)
    {
        const double c = b - phi * (b - a);
        const double e = a + phi * (b - a);
        if (gap_at(mover, d, obstacle, static_cast<float>(c)) < gap_at(mover, d, obstacle, static_cast<float>(e)))
        {
            b = e;
        }
        else { a = c; }
    }
    const float t_min = static_cast<float>(0.5 * (a + b));
    if (gap_at(mover, d, obstacle, t_min) > skin) { return out; }
    float lo = 0.0f;
    float hi = t_min;
    for (int i = 0; i < 60; ++i)
    {
        const float mid = 0.5f * (lo + hi);
        if (gap_at(mover, d, obstacle, mid) > skin) { lo = mid; }
        else { hi = mid; }
    }
    out.hit = true;
    out.t = lo;
    return out;
}

/// A random convex obstacle, by value.
struct random_shape
{
    shape s{};
    vec3 at{};
    quat q{};
};

[[nodiscard]] random_shape any_shape(rng& r, vec3 at)
{
    random_shape out;
    out.at = at;
    out.q = r.rotation();
    const int kind = static_cast<int>(r.next() % 3u);
    if (kind == 0) { out.s = sphere_shape(r.range(0.1f, 0.8f)); }
    else if (kind == 1) { out.s = box_shape(vec3{r.range(0.1f, 1.0f), r.range(0.1f, 1.0f), r.range(0.1f, 1.0f)}); }
    else { out.s = capsule_shape(r.range(0.1f, 0.5f), r.range(0.0f, 0.8f)); }
    return out;
}

/// **The control for §3: the same cast, stepping from GJK's `distance`** — the
/// gap between two witness points, an UPPER bound — as a first draft of
/// `cast.cpp` did. Kept here, private, so the fix can be measured against it.
[[nodiscard]] cast_result cast_from_upper_bound(const convex& mover, vec3 d, const convex& obstacle, float skin)
{
    cast_result out;
    const float len = length(d);
    convex at = mover;
    gjk_config gcfg;
    float t = 0.0f;
    for (int i = 0; i < 32; ++i)
    {
        at.origin = mover.origin + d * t;
        const gjk_result g = gjk_distance(at, obstacle, gcfg);
        out.iterations = i + 1;
        if (g.status != gjk_status::separated)
        {
            out.status = i == 0 ? cast_status::started_inside : cast_status::hit;
            return out;
        }
        const float approach = dot(d, g.direction);
        out.t = t;
        out.distance = g.distance;
        if (approach <= 1e-4f * len) { out.status = cast_status::clear; out.t = 1.0f; return out; }
        const float excess = g.distance - skin;
        if (excess <= 1e-4f * len) { out.status = cast_status::hit; return out; }
        t += excess / approach;
        if (t >= 1.0f) { out.status = cast_status::clear; out.t = 1.0f; return out; }
        gcfg.initial_direction = g.direction;
    }
    out.status = cast_status::iteration_limit;
    out.t = t;
    return out;
}

void section_b()
{
    rule("§3  THE ONE QUERY: A CAST IS NEWTON'S METHOD");

    // --- B1: the worked example, by hand and by the engine -----------------
    {
        // By hand, in double precision, with the distance in closed form:
        // f(t) = |(4t - 3, -0.6)| - 1, and f'(t) = -dot(d, n).
        std::printf("  two balls, r = 0.5: from the origin along (4, 0, 0) toward one at (3, 0.6, 0)\n");
        std::printf("  the exact answer: |(4t - 3, -0.6)| = 1 at t = 0.55\n");
        std::printf("    %-4s %14s %14s %14s %12s %12s\n", "n", "t", "gap", "approach", "error", "err/prev^2");
        double t = 0.0;
        double prev = 0.0;
        for (int n = 0; n < 5; ++n)
        {
            const double px = 4.0 * t - 3.0;
            const double py = -0.6;
            const double len = std::sqrt(px * px + py * py);
            const double gap = len - 1.0;
            const double approach = 4.0 * (-px / len);   // d . n, n from the mover toward the obstacle
            const double err = 0.55 - t;
            std::printf("    %-4d %14.10f %14.10f %14.10f %12.3e", n, t, gap, approach, err);
            if (n >= 1) { std::printf(" %12.5f", err / (prev * prev)); }
            std::printf("\n");
            prev = err;
            if (err < 1e-12) { break; }
            t += gap / approach;
        }
        std::printf("  the ratio tends to f''/(2|f'|) = 5.76/6.4 = 0.9: quadratic convergence.\n");

        const engine::sphere a{vec3{0.0f, 0.0f, 0.0f}, 0.5f};
        const engine::sphere b{vec3{3.0f, 0.6f, 0.0f}, 0.5f};
        const vec3 d{4.0f, 0.0f, 0.0f};
        cast_config cc;
        const cast_result r0 = cast(as_convex(a), d, as_convex(b), cc);
        std::printf("  cast(), skin 0:    t = %.7f  (%d Newton steps, %d GJK iterations) — %.2f mm short\n",
                    static_cast<double>(r0.t), r0.iterations, r0.gjk_iterations,
                    (0.55 - static_cast<double>(r0.t)) * 4000.0);
        cc.skin = 0.01f;
        const cast_result r1 = cast(as_convex(a), d, as_convex(b), cc);
        const double exact1 = (3.0 - std::sqrt(1.01 * 1.01 - 0.36)) / 4.0;
        std::printf("  cast(), skin 1 cm: t = %.7f  (%d Newton steps, %d GJK iterations); exact %.7f\n",
                    static_cast<double>(r1.t), r1.iterations, r1.gjk_iterations, exact1);
        // With no skin, the step that would land ON the contact lands where GJK
        // reports the balls as intersecting (they touch, to within its margin),
        // so the cast has to report the iterate BEFORE it. §3 turns this into
        // the argument for the skin.
        check(r0.t < 0.5499f && r0.t > 0.549f, "B1: with no skin the cast stops one Newton step short");
        check(std::fabs(static_cast<double>(r1.t) - exact1) * 4.0 < 1e-4 * 4.0 + 1e-6,
              "B1: with a skin it lands on the skin, to tolerance");
    }

    // --- B2: a flat face, head on ------------------------------------------
    {
        const engine::sphere a{vec3{0.0f, 2.0f, 0.0f}, 0.5f};
        obb floor;
        floor.centre = vec3{0.0f, -0.5f, 0.0f};
        floor.half_extents = vec3{5.0f, 0.5f, 5.0f};
        cast_config cc;
        cc.skin = 0.01f;
        const cast_result r = cast(as_convex(a), vec3{0.3f, -3.0f, 0.1f}, as_convex(floor), cc);
        const float exact = (2.0f - 0.5f - 0.01f) / 3.0f;
        std::printf("\n  a ball falling onto a floor at an angle: t = %.7f (exact %.7f), %d Newton steps\n",
                    static_cast<double>(r.t), static_cast<double>(exact), r.iterations);
        std::printf("  — one step lands, and the second GJK call only confirms it. That first step IS\n"
                    "    1.8's swept test: the gap over the closing speed.\n");
        check(r.iterations == 2 && std::fabs(r.t - exact) * 3.03f < 1e-4f * 3.03f,
              "B2: a flat face is hit in one Newton step");
    }

    // --- B3: ten thousand random casts against the reference ---------------
    {
        rng r(8130u);
        const int n = 10000;
        int hits = 0;
        int missed = 0;         // the cast said clear, the reference found a hit: unsafe
        int early = 0;          // the cast said hit, the reference said clear: a graze
        float worst_graze = 0.0f;
        int started = 0;
        int limits = 0;
        int late = 0;           // finished AFTER the reference's crossing: unsafe
        float worst_excess_rel = 0.0f;
        float worst_below = 0.0f;
        int beyond = 0;
        int beyond_explained = 0;
        float worst_loose = 0.0f;
        float worst_beyond = 0.0f;
        int upper_hits = 0;
        int upper_late = 0;
        float upper_below = 0.0f;
        long long steps = 0;
        long long gjk_steps = 0;
        int hist[12] = {};
        int worst_iters = 0;
        const float skin = 0.01f;
        for (int i = 0; i < n; ++i)
        {
            const shape ms = capsule_shape(r.range(0.1f, 0.5f), r.range(0.0f, 0.8f));
            const vec3 m_at{0.0f, 0.0f, 0.0f};
            const quat m_q = r.rotation();
            const random_shape ob = any_shape(r, r.direction() * r.range(2.0f, 4.0f));
            // Aim roughly at the obstacle, with a spread, so about half hit.
            const vec3 aim = normalised(ob.at - m_at + r.direction() * r.range(0.0f, 1.6f));
            const vec3 d = aim * r.range(1.0f, 6.0f);
            const placed_shape pm = place(ms, m_at, m_q);
            const placed_shape po = place(ob.s, ob.at, ob.q);
            const convex cm = pm.view();
            const convex co = po.view();
            if (gap_at(cm, d, co, 0.0f) <= skin) { ++started; continue; }

            cast_config cc;
            cc.skin = skin;
            const cast_result c = cast(cm, d, co, cc);
            const reference_toi ref = reference(cm, d, co, skin);
            const float len = length(d);

            // The control arm: stepped from the upper bound.
            const cast_result u = cast_from_upper_bound(cm, d, co, skin);
            if (u.hit() && ref.hit)
            {
                ++upper_hits;
                if (u.t > ref.t + 1e-6f) { ++upper_late; }
                upper_below = std::max(upper_below, skin - gap_at(cm, d, co, u.t));
            }
            steps += c.iterations;
            gjk_steps += c.gjk_iterations;
            ++hist[std::min(c.iterations, 11)];
            worst_iters = std::max(worst_iters, c.iterations);
            if (c.status == cast_status::iteration_limit) { ++limits; }
            if (!c.hit() && ref.hit) { ++missed; continue; }
            if (c.hit() && !ref.hit)
            {
                ++early;
                worst_graze = std::max(worst_graze, (gap_at(cm, d, co, c.t) - skin) / len);
                continue;
            }
            if (!c.hit()) { continue; }
            ++hits;
            if (c.t > ref.t + 1e-6f) { ++late; }
            const float g = gap_at(cm, d, co, c.t);
            // Past the tolerance. The cast stopped because the LOWER bound it
            // was using had reached the skin, so its own `distance` should say
            // so — and the gap between that bound and the truth is how loose
            // GJK's certificate was at a centimetre.
            if (g - skin > 1e-4f * len + 1e-6f)
            {
                ++beyond;
                if (c.distance <= skin + 1e-4f * len + 1e-6f) { ++beyond_explained; }
                worst_loose = std::max(worst_loose, g - c.distance);
                worst_beyond = std::max(worst_beyond, g - skin);
            }
            worst_excess_rel = std::max(worst_excess_rel, (g - skin) / len);
            worst_below = std::max(worst_below, skin - g);
        }
        const int cast_count = n - started;
        std::printf("\n  %d random casts (capsule against box / ball / capsule, any orientation), skin 1 cm\n",
                    cast_count);
        std::printf("    hits %d, clear %d\n", hits + early, cast_count - hits - early);
        std::printf("    missed hits (clear, but the reference crosses the skin)   %d\n", missed);
        std::printf("    late hits (past the reference's crossing)                 %d\n", late);
        std::printf("    grazes counted as hits (the path passes within tolerance) %d, worst %.2e of |d|\n",
                    early, static_cast<double>(worst_graze));
        std::printf("    worst gap left above the skin at a hit                    %.2e of |d|  (tolerance 1e-4)\n",
                    static_cast<double>(worst_excess_rel));
        std::printf("    hits stopped beyond the tolerance %d of %d; the cast's own lower bound explains %d\n"
                    "      (it was up to %.3f mm below the truth there; worst stop %.3f mm above the skin)\n",
                    beyond, hits, beyond_explained, 1000.0 * static_cast<double>(worst_loose),
                    1000.0 * static_cast<double>(worst_beyond));
        std::printf("    worst finish BELOW the skin                               %.2e m\n",
                    static_cast<double>(worst_below));
        std::printf("    CONTROL, stepping from GJK's distance (an upper bound): %d of %d hits late,\n"
                    "      worst %.4f mm inside the skin\n",
                    upper_late, upper_hits, 1000.0 * static_cast<double>(upper_below));
        std::printf("    Newton steps: mean %.2f, worst %d; GJK iterations per cast %.2f; limits hit %d\n",
                    static_cast<double>(steps) / cast_count, worst_iters,
                    static_cast<double>(gjk_steps) / cast_count, limits);
        std::printf("    steps:");
        for (int k = 1; k < 12; ++k) { std::printf(" %d:%d", k, hist[k]); }
        std::printf("\n");
        check(missed == 0, "B3: no cast misses a hit the reference finds");
        check(late == 0, "B3: no cast finishes after the reference's crossing");
        check(worst_graze <= 1e-4f, "B3: every graze counted as a hit is within the tolerance");
        check(beyond == beyond_explained, "B3: every stop beyond the tolerance is a loose lower bound, nothing else");
        check(worst_beyond < 2e-3f, "B3: ...and none stops more than 2 mm early");
        check(worst_below <= 1e-5f, "B3: no cast ever finishes inside the skin");
        check(upper_late > 0 && upper_below > 1e-5f, "B3: the control, stepped from the upper bound, does");
        check(limits == 0, "B3: no cast hits the iteration limit");
    }

    // --- B4: the control that breaks convexity: one cast against a union ----
    {
        // Two obstacles, one near the path and passed at a grazing angle, one
        // square in it farther on. Newton on min(f1, f2) takes its step from
        // whichever is nearer — the grazing one, whose closing speed is small —
        // and the step can carry the mover straight through the other.
        rng r(8131u);
        const int n = 20000;
        int union_through = 0;
        int split_through = 0;
        int usable = 0;
        const float skin = 0.01f;
        for (int i = 0; i < n; ++i)
        {
            const engine::sphere m{vec3{0.0f, 0.0f, 0.0f}, 0.4f};
            const vec3 d{r.range(3.0f, 8.0f), 0.0f, 0.0f};
            const engine::sphere near{vec3{r.range(0.6f, 2.0f), r.range(0.95f, 1.6f), r.range(-0.3f, 0.3f)},
                                      r.range(0.3f, 0.6f)};
            obb wall;
            wall.centre = vec3{r.range(2.0f, 6.0f), 0.0f, 0.0f};
            wall.half_extents = vec3{r.range(0.02f, 0.3f), 2.0f, 2.0f};
            const convex cm = as_convex(m);
            const convex c1 = as_convex(near);
            const convex c2 = as_convex(wall);
            if (gap_at(cm, d, c1, 0.0f) <= skin || gap_at(cm, d, c2, 0.0f) <= skin) { continue; }
            ++usable;

            // The union: one conservative advancement on min(f1, f2).
            float t = 0.0f;
            for (int k = 0; k < 64; ++k)
            {
                convex at = cm;
                at.origin = cm.origin + d * t;
                const gjk_result g1 = gjk_distance(at, c1);
                const gjk_result g2 = gjk_distance(at, c2);
                const gjk_result& g = (g1.distance < g2.distance) ? g1 : g2;
                if (g.status != gjk_status::separated) { break; }
                const float approach = dot(d, g.direction);
                if (approach <= 1e-4f * length(d)) { t = 1.0f; break; }
                if (g.distance - skin <= 1e-4f * length(d)) { break; }
                t += (g.distance - skin) / approach;
                if (t >= 1.0f) { t = 1.0f; break; }
            }
            // Through the wall = the mover's centre ended past its far face.
            const float end_x = d.x * t;
            if (end_x > wall.centre.x) { ++union_through; }

            // The split: one cast each, keep the smaller t.
            cast_config cc;
            cc.skin = skin;
            const float t1 = cast(cm, d, c1, cc).t;
            const float t2 = cast(cm, d, c2, cc).t;
            if (d.x * std::min(t1, t2) > wall.centre.x) { ++split_through; }
        }
        std::printf("\n  %d moves past a ball at a grazing angle toward a wall behind it\n", usable);
        std::printf("    ONE cast against the union (min of two distances): through the wall %d times\n",
                    union_through);
        std::printf("    one cast PER obstacle, smallest t kept:            through the wall %d times\n",
                    split_through);
        check(union_through > 0, "B4: Newton on a union (not convex) overshoots");
        check(split_through == 0, "B4: one cast per obstacle never does");
    }

    // --- B5: sphere tracing, the other conservative step --------------------
    {
        std::printf("\n  approaching a flat face at a grazing angle: Newton steps to converge\n");
        std::printf("    %-10s %16s %22s\n", "angle", "cast (Newton)", "sphere tracing (gap/|d|)");
        obb floor;
        floor.centre = vec3{0.0f, -0.5f, 0.0f};
        floor.half_extents = vec3{400.0f, 0.5f, 400.0f};
        int worst_newton = 0;
        int trace_at_1deg = 0;
        for (float a : {90.0f, 30.0f, 10.0f, 3.0f, 1.0f})
        {
            const engine::sphere m{vec3{0.0f, 1.0f, 0.0f}, 0.5f};
            const float ar = rad(a);
            const vec3 d = vec3{std::cos(ar), -std::sin(ar), 0.0f} * (1.2f / std::sin(ar));
            cast_config cc;
            cc.skin = 0.01f;
            const cast_result c = cast(as_convex(m), d, as_convex(floor), cc);
            // Sphere tracing: step by the gap along the motion — conservative
            // for ANY shape, because nothing is nearer than the gap, but it
            // divides by |d| where Newton divides by the closing speed.
            const convex cm = as_convex(m);
            const convex cf = as_convex(floor);
            float t = 0.0f;
            int k = 0;
            for (; k < 100000; ++k)
            {
                const float g = gap_at(cm, d, cf, t);
                if (g - cc.skin <= 1e-4f * length(d)) { break; }
                t += (g - cc.skin) / length(d);
            }
            std::printf("    %6.0f deg %16d %22d\n", static_cast<double>(a), c.iterations, k + 1);
            worst_newton = std::max(worst_newton, c.iterations);
            if (a < 2.0f) { trace_at_1deg = k + 1; }
        }
        check(worst_newton <= 2, "B5: Newton hits a face in one step at every angle");
        check(trace_at_1deg > 100, "B5: sphere tracing crawls at a grazing angle");
    }

    // --- B6: why the skin --------------------------------------------------
    {
        // Cast to contact, then ask the next question from where it stopped: a
        // slide along the surface. How close did the cast leave the two shapes,
        // and can GJK — whose contact margin (8.5 §F.6) is a hair wide — still
        // say which way is out?
        std::printf("\n  cast, then slide from where it stopped\n");
        std::printf("    %-10s %14s %14s %22s\n", "skin (m)", "mean gap (mm)", "max gap (mm)", "next cast starts inside");
        int inside_at[4] = {};
        int tried_at[4] = {};
        int col = 0;
        for (float skin : {0.0f, 1e-4f, 1e-3f, 1e-2f})
        {
            rng r(8132u);
            int tried = 0;
            int inside = 0;
            double gap_sum = 0.0;
            double gap_max = 0.0;
            for (int i = 0; i < 4000; ++i)
            {
                const shape ms = capsule_shape(0.3f, 0.6f);
                const placed_shape pm = place(ms, vec3{}, quat{});
                const random_shape ob = any_shape(r, r.direction() * r.range(2.0f, 3.0f));
                const placed_shape po = place(ob.s, ob.at, ob.q);
                const vec3 d = normalised(ob.at + r.direction() * 0.3f) * 0.2f * r.range(10.0f, 25.0f);
                cast_config cc;
                cc.skin = skin;
                if (gap_at(pm.view(), d, po.view(), 0.0f) <= skin) { continue; }
                const cast_result c = cast(pm.view(), d, po.view(), cc);
                if (!c.hit()) { continue; }
                ++tried;
                convex at = pm.view();
                at.origin = at.origin + d * c.t;
                const double g = static_cast<double>(gap_at(pm.view(), d, po.view(), c.t));
                gap_sum += g;
                gap_max = std::max(gap_max, g);
                const vec3 slide = slide_along(d * (1.0f - c.t), c.surface_normal);
                const cast_result next = cast(at, slide, po.view(), cc);
                if (next.status == cast_status::started_inside) { ++inside; }
            }
            std::printf("    %-10.4f %14.4f %14.4f %13d of %4d (%.1f%%)\n", static_cast<double>(skin),
                        1000.0 * gap_sum / std::max(tried, 1), 1000.0 * gap_max, inside, tried,
                        100.0 * inside / std::max(tried, 1));
            inside_at[col] = inside;
            tried_at[col] = tried;
            ++col;
        }
        check(inside_at[1] > inside_at[0], "B6: a skin thinner than GJK's margin is WORSE than none");
        check(inside_at[2] == 0 && inside_at[3] == 0, "B6: a skin of a millimetre or more never starts inside");
        check(tried_at[0] > 1000, "B6: enough hits to mean something");
    }
}

// ---------------------------------------------------------------------------
// C: collide and slide
// ---------------------------------------------------------------------------

/// A thin wall whose FACE contains the vertical line through `apex` and runs
/// along `dir` (horizontal, unit) for `length`, facing `n` (horizontal, unit).
std::uint32_t add_wall(scene& s, vec3 apex, vec3 dir, vec3 n, float length, float height = 1.5f)
{
    const float thick = 0.1f;
    const vec3 centre = apex + dir * (0.5f * length) - n * thick + vec3{0.0f, height, 0.0f};
    const float theta = std::atan2(-dir.z, dir.x);   // quat_y(θ) turns +x onto (cos θ, 0, −sin θ)
    return add_block(s, centre, vec3{0.5f * length, height, thick}, engine::quat_y(theta));
}

struct corner_run
{
    float spread = 0.0f;      ///< max − min of the position over the last second, horizontal
    float travel = 0.0f;      ///< path length over the last second
    int blocked = 0;          ///< moves that ran out of slides
    float slides = 0.0f;      ///< mean surfaces hit per move
};

/// Walk into a V-shaped corner of opening `opening` (radians), apex at the
/// origin, open toward −x, at `heading` from +x, for three seconds.
corner_run corner(float opening, float heading, character_config::clip_rule rule_)
{
    scene s;
    (void)add_floor(s);
    const float c = std::cos(0.5f * opening);
    const float sn = std::sin(0.5f * opening);
    const vec3 dir_a{-c, 0.0f, sn};
    const vec3 dir_b{-c, 0.0f, -sn};
    const vec3 n_a{-sn, 0.0f, -c};
    const vec3 n_b{-sn, 0.0f, c};
    (void)add_wall(s, vec3{}, dir_a, n_a, 6.0f);
    (void)add_wall(s, vec3{}, dir_b, n_b, 6.0f);

    walker w;
    w.cfg.clip = rule_;
    w.cfg.step_height = 0.0f;
    w.place_at(s, vec3{-3.0f, standing_y(w.cfg), 0.0f});
    const vec3 wish = vec3{std::cos(heading), 0.0f, std::sin(heading)} * 2.0f;
    corner_run out;
    float lo_x = 1e9f, hi_x = -1e9f, lo_z = 1e9f, hi_z = -1e9f;
    int slides = 0;
    for (int i = 0; i < 300; ++i)
    {
        const vec3 before = w.ch.position;
        w.frame(s, wish);
        if (i >= 240)
        {
            const vec3 p = w.ch.position;
            lo_x = std::min(lo_x, p.x); hi_x = std::max(hi_x, p.x);
            lo_z = std::min(lo_z, p.z); hi_z = std::max(hi_z, p.z);
            out.travel += length(vec3{p.x - before.x, 0.0f, p.z - before.z});
            out.blocked += w.last.blocked ? 1 : 0;
            slides += w.last.slides;
        }
    }
    out.spread = std::max(hi_x - lo_x, hi_z - lo_z);
    out.slides = static_cast<float>(slides) / 60.0f;
    return out;
}

void section_c()
{
    rule("§4  COLLIDE AND SLIDE");

    std::printf("  walking into a V-shaped corner for 5 s, 2 m/s; the last second measured\n");
    std::printf("    %-9s %-8s %-30s %12s %12s %9s %8s\n", "opening", "heading", "the rest of the move is", "spread (mm)",
                "travel (mm)", "slides", "blocked");
    using cr = character_config::clip_rule;
    auto rule_name = [](cr r) {
        return r == cr::original ? "the ORIGINAL, every plane" : r == cr::remainder ? "the remainder, every plane"
                                                                                    : "the remainder, last plane";
    };
    float worst[3] = {};
    int blocked[3] = {};
    for (float opening : {60.0f, 90.0f, 120.0f})
    {
        for (float heading : {0.0f, 12.0f})
        {
            for (cr rule_ : {cr::last_plane, cr::remainder, cr::original})
            {
                const corner_run r = corner(rad(opening), rad(heading), rule_);
                std::printf("    %5.0f deg %5.0f deg %-30s %12.3f %12.3f %9.2f %8d\n", static_cast<double>(opening),
                            static_cast<double>(heading), rule_name(rule_), 1000.0 * static_cast<double>(r.spread),
                            1000.0 * static_cast<double>(r.travel), static_cast<double>(r.slides), r.blocked);
                const int k = static_cast<int>(rule_);
                worst[k] = std::max(worst[k], r.travel);
                blocked[k] += r.blocked;
            }
        }
    }
    check(worst[0] < 1e-3f && blocked[0] == 0, "C1: clipping the original against every plane, a cornered character is still");
    check(worst[1] > 0.1f, "C1: clipping the remainder, it walks back and forth in an obtuse corner");
    check(worst[2] > 0.1f && blocked[2] > 0, "C1: the last plane only: it jitters, and runs out of slides");

    // --- C2: running PARALLEL to a wall at the skin distance ----------------
    std::printf("\n  running along a wall at its skin distance, 3 m/s for 2 s\n");
    for (float min_approach : {0.0f, 1e-4f})
    {
        for (float lean : {0.0f, 0.02f})
        {
            scene s;
            (void)add_floor(s);
            (void)add_block(s, vec3{1.1f, 1.5f, 0.0f}, vec3{0.1f, 1.5f, 40.0f});
            walker w;
            w.cfg.cast.min_approach = min_approach;
            w.cfg.step_height = 0.0f;
            w.place_at(s, vec3{1.0f - w.cfg.radius - w.cfg.skin, standing_y(w.cfg), 0.0f});
            const float z0 = w.ch.position.z;
            int stalls = 0;
            for (int i = 0; i < 120; ++i)
            {
                const float before = w.ch.position.z;
                w.frame(s, vec3{lean * 3.0f, 0.0f, 3.0f});
                if (w.ch.position.z - before < 0.9f * 3.0f * k_h) { ++stalls; }
            }
            const float speed = (w.ch.position.z - z0) / (120.0f * k_h);
            std::printf("    min_approach %-6.0e  stick %s   speed along the wall %.4f m/s, %3d stalled steps\n",
                        static_cast<double>(min_approach), lean > 0.0f ? "2% into it" : "parallel  ",
                        static_cast<double>(speed), stalls);
            if (min_approach > 0.0f) { check(stalls == 0, "C2: with min_approach, sliding along a wall never stalls"); }
        }
    }
}

// ---------------------------------------------------------------------------
// D: slopes
// ---------------------------------------------------------------------------

void section_d()
{
    rule("§5  SLOPES");

    // --- D1: walkable ramps, horizontal speed kept ---------------------------
    std::printf("  walking up a ramp at 3 m/s (stick), measured between x = 1 and x = 4\n");
    std::printf("    %-8s %14s %14s %16s\n", "slope", "horiz. speed", "along slope", "rise per metre");
    bool kept = true;
    for (float a : {10.0f, 20.0f, 30.0f, 40.0f})
    {
        scene s;
        (void)add_floor(s);
        const float ar = rad(a);
        (void)add_ramp(s, ar, vec3{8.0f * std::cos(ar), 8.0f * std::sin(ar), 0.0f});
        walker w;
        w.place_at(s, vec3{-2.0f, standing_y(w.cfg), 0.0f});
        float x1 = 0, t1 = 0, y1 = 0, x4 = 0, t4 = 0, y4 = 0;
        for (int i = 0; i < 180; ++i)
        {
            w.frame(s, vec3{3.0f, 0.0f, 0.0f});
            const float t = static_cast<float>(i + 1) * k_h;
            if (x1 == 0.0f && w.ch.position.x >= 1.0f) { x1 = w.ch.position.x; t1 = t; y1 = w.ch.position.y; }
            if (x4 == 0.0f && w.ch.position.x >= 4.0f) { x4 = w.ch.position.x; t4 = t; y4 = w.ch.position.y; }
        }
        const float vh = (x4 - x1) / (t4 - t1);
        const float rise = (y4 - y1) / (x4 - x1);
        std::printf("    %4.0f deg %14.4f %14.4f %16.4f   (tan = %.4f)\n", static_cast<double>(a),
                    static_cast<double>(vh), static_cast<double>(vh / std::cos(ar)), static_cast<double>(rise),
                    static_cast<double>(std::tan(ar)));
        if (std::fabs(vh - 3.0f) > 1e-3f || std::fabs(rise - std::tan(ar)) > 1e-3f) { kept = false; }
    }
    check(kept, "D1: on every walkable ramp the horizontal speed is the stick's, and the climb is tan(a)");

    // --- D2: too steep -------------------------------------------------------
    std::printf("\n  a slope too steep to walk (45 deg limit), 3 m/s into it for 3 s: how far in the centre gets (x)\n");
    std::printf("    %-8s %-30s %14s %14s\n", "slope", "", "flattened", "NOT flattened");
    {
        // Three ways in — walking with the move laid along the floor, walking
        // with it horizontal, and hopping 0.5 m into the slope whenever
        // grounded — measured by how far UP THE SLOPE the character's centre
        // gets, which a hop's apex alone cannot fake.
        for (float a : {50.0f, 60.0f})
        {
            for (int way = 0; way < 3; ++way)
            {
                float reach[2] = {-1e9f, -1e9f};
                for (int k = 0; k < 2; ++k)
                {
                    scene s;
                    (void)add_floor(s);
                    const float ar = rad(a);
                    (void)add_ramp(s, ar, vec3{8.0f * std::cos(ar), 8.0f * std::sin(ar), 0.0f});
                    walker w;
                    w.cfg.flatten_walls = (k == 0);
                    w.cfg.step_height = 0.0f;
                    w.cfg.lay_along_ground = (way != 1);
                    w.place_at(s, vec3{-1.0f, standing_y(w.cfg), 0.0f});
                    for (int i = 0; i < 180; ++i)
                    {
                        if (way == 2 && w.ch.grounded) { w.jump_height = 0.5f; }
                        w.frame(s, vec3{3.0f, 0.0f, 0.0f});
                        reach[k] = std::max(reach[k], w.ch.position.x);
                    }
                }
                const char* how = way == 0 ? "walking, laid along the floor" : way == 1 ? "walking, horizontal move"
                                                                                        : "hopping 0.5 m into it";
                // Where the centre rests with the capsule against the foot of the
                // slope: r' = r + skin from the plane, r' above the floor.
                const float rest = -(0.31f) * std::tan(0.5f * rad(a));
                std::printf("    %4.0f deg %-30s %+11.3f m  %+11.3f m   (resting at the foot: %+.3f)\n",
                            static_cast<double>(a), how, static_cast<double>(reach[0]), static_cast<double>(reach[1]),
                            static_cast<double>(rest));
                // Walking horizontally is REPORTED, not checked: the slide up the
                // face is real (v h sin a cos a a step) but gravity's slide back
                // down it cancels it within a few steps, so it barely shows.
                if (way == 0) { check(reach[1] - reach[0] < 0.01f, "D2: laid along the floor, the slide cannot climb either way"); }
                if (way == 2) { check(reach[1] - reach[0] > 0.2f, "D2: in the air, only flattening stops the climb"); }
            }
        }
    }

    // --- D3: standing still, and creeping ------------------------------------
    std::printf("\n  standing still on a slope for 2 s\n");
    std::printf("    %-8s %22s %22s %14s\n", "slope", "stop on ground (m/s)", "slide gravity (m/s)", "g h sin a");
    bool creep_ok = true;
    for (float a : {10.0f, 20.0f, 30.0f, 40.0f})
    {
        float speed[2] = {};
        for (int k = 0; k < 2; ++k)
        {
            scene s;
            const float ar = rad(a);
            (void)add_ramp(s, ar, vec3{0.0f, 0.0f, 0.0f}, 20.0f);
            walker w;
            w.cfg.stop_on_ground = (k == 0);
            const vec3 n{-std::sin(ar), std::cos(ar), 0.0f};
            w.place_at(s, n * (w.cfg.radius + w.cfg.skin) + vec3{0.0f, w.cfg.half_height, 0.0f});
            for (int i = 0; i < 30; ++i) { w.frame(s, vec3{}); }
            const vec3 x0 = w.ch.position;
            for (int i = 0; i < 120; ++i) { w.frame(s, vec3{}); }
            speed[k] = length(w.ch.position - x0) / (120.0f * k_h);
        }
        const float pred = slope_creep_speed(k_g, k_h, rad(a));
        std::printf("    %4.0f deg %22.6f %22.6f %14.6f\n", static_cast<double>(a), static_cast<double>(speed[0]),
                    static_cast<double>(speed[1]), static_cast<double>(pred));
        if (speed[0] > 1e-5f || std::fabs(speed[1] - pred) > 0.02f * pred) { creep_ok = false; }
    }
    check(creep_ok, "D3: stopped on the ground it stands still; slid along it, it creeps at g h sin a");

    // --- D4: sliding down what cannot be stood on ----------------------------
    {
        scene s;
        const float ar = rad(60.0f);
        (void)add_ramp(s, ar, vec3{0.0f, 0.0f, 0.0f}, 20.0f);
        walker w;
        const vec3 n{-std::sin(ar), std::cos(ar), 0.0f};
        w.ch.position = n * (w.cfg.radius + w.cfg.skin) + vec3{0.0f, w.cfg.half_height, 0.0f};
        const vec3 x0 = w.ch.position;
        for (int i = 0; i < 30; ++i) { w.frame(s, vec3{}); }
        const vec3 x1 = w.ch.position;
        w.frame(s, vec3{});
        const float speed = length(w.ch.position - x1) / k_h;
        const float pred = k_g * (31.0f * k_h) * std::sin(ar);
        std::printf("\n  dropped on a 60 deg slope: along it at %.4f m/s after 0.5 s; g t sin a = %.4f\n"
                    "    (the fall, projected onto the slope, IS a frictionless incline: no force anywhere)\n",
                    static_cast<double>(speed), static_cast<double>(pred));
        (void)x0;
        check(std::fabs(speed - pred) < 0.01f * pred, "D4: a steep slope is slid down at g sin a");
    }
}

// ---------------------------------------------------------------------------
// E: ground, and the round bottom
// ---------------------------------------------------------------------------

/// A block whose top is at `top`, spanning [x0, x1] in x and ±3 in z, down to y = −4.
std::uint32_t add_slab(scene& s, float x0, float x1, float top, float half_z = 3.0f)
{
    const float bottom = -4.0f;
    return add_block(s, vec3{0.5f * (x0 + x1), 0.5f * (top + bottom), 0.0f},
                     vec3{0.5f * (x1 - x0), 0.5f * (top - bottom), half_z});
}

void section_e()
{
    rule("§6  GROUND, AND THE ROUND BOTTOM");
    const character_config base{};
    const float rs = base.radius + base.skin;

    // --- E1: the tallest curb climbed with no step-up -----------------------
    auto climbs_curb = [&](float height) {
        scene s;
        (void)add_floor(s);
        (void)add_slab(s, 1.0f, 12.0f, height);
        walker w;
        w.cfg.step_height = 0.0f;
        w.place_at(s, vec3{0.0f, standing_y(w.cfg), 0.0f});
        for (int i = 0; i < 120; ++i) { w.frame(s, vec3{1.0f, 0.0f, 0.0f}); }
        return w.ch.position.x > 1.5f && w.ch.position.y > standing_y(w.cfg, height) - 1e-3f;
    };
    float lo = 0.02f;
    float hi = 0.2f;
    for (int it = 0; it < 24; ++it)
    {
        const float mid = 0.5f * (lo + hi);
        if (climbs_curb(mid)) { lo = mid; }
        else { hi = mid; }
    }
    const float curb = free_curb_height(rs, base.max_slope);
    std::printf("  walking into a curb with the step-up OFF: climbs up to %.5f m\n", static_cast<double>(lo));
    std::printf("    predicted (r + skin)(1 - cos 45) = %.5f m — the round bottom makes a ramp of the edge\n",
                static_cast<double>(curb));
    // GJK's contact normal at a centimetre is good to about 0.2 degrees (§E3),
    // and 0.2 degrees at the threshold moves the curb by (r + skin) sin 45
    // times 0.0035 rad = 0.8 mm. That is the tolerance, and it is GJK's.
    check(std::fabs(lo - curb) < 1.5e-3f, "E1: the free curb is (r + skin)(1 - cos max_slope), to GJK's precision");

    // --- E2: how far past an edge it can stand ------------------------------
    // By placement: rest the capsule ON the edge with its centre `x` past it,
    // take one standing frame, and see whether it is still grounded.
    auto grounded_over = [&](float x) {
        scene s;
        (void)add_slab(s, -10.0f, 0.0f, 1.0f);
        walker w;
        w.cfg.snap_distance = 0.0f;
        const float lift = x < rs ? std::sqrt(rs * rs - x * x) : 0.0f;
        w.ch.position = vec3{x, 1.0f + lift + w.cfg.half_height, 0.0f};
        w.ch.grounded = true;
        w.frame(s, vec3{});
        return w.ch.grounded;
    };
    lo = 0.0f;
    hi = rs;
    for (int it = 0; it < 24; ++it)
    {
        const float mid = 0.5f * (lo + hi);
        if (grounded_over(mid)) { lo = mid; }
        else { hi = mid; }
    }
    const float over = edge_overhang(rs, base.max_slope);
    std::printf("\n  standing on an edge: grounded up to %.5f m past it; (r + skin) sin 45 = %.5f m\n",
                static_cast<double>(lo), static_cast<double>(over));
    check(std::fabs(lo - over) < 1e-3f, "E2: a capsule stands until its centre is (r + skin) sin 45 past an edge");

    // Walking off, slowly, so that the same number shows up as a behaviour.
    {
        scene s;
        (void)add_slab(s, -10.0f, 0.0f, 1.0f);
        (void)add_floor(s);
        walker w;
        w.cfg.snap_distance = 0.0f;
        w.place_at(s, vec3{-1.0f, standing_y(w.cfg, 1.0f), 0.0f});
        float lost = -1.0f;
        for (int i = 0; i < 400 && lost < 0.0f; ++i)
        {
            const float x = w.ch.position.x;
            w.frame(s, vec3{0.3f, 0.0f, 0.0f});
            if (!w.ch.grounded) { lost = x; }
        }
        std::printf("  walking off at 0.3 m/s (5 mm a step): last grounded at %.4f m past the edge\n",
                    static_cast<double>(lost));
        check(std::fabs(lost - over) < 0.006f, "E2: walking off, it loses the ground at the same overhang");
    }

    // --- E3: the contact normal on an edge is not the surface's -------------
    std::printf("\n  the ground normal a round bottom reports on an edge\n");
    std::printf("    %-12s %16s %22s\n", "past edge", "tilt (deg)", "asin(x / (r + skin))");
    float worst = 0.0f;
    for (float x : {0.0f, 0.05f, 0.10f, 0.15f, 0.20f, 0.25f})
    {
        scene s;
        (void)add_slab(s, -10.0f, 0.0f, 1.0f);
        const float lift = std::sqrt(rs * rs - x * x);
        const vec3 at{x, 1.0f + lift + base.half_height + 0.002f, 0.0f};
        walker w;
        const character_hit hit = sweep(at, vec3{0.0f, -0.01f, 0.0f}, w.view(s), base, sweep_filter::everything);
        const double tilt = deg(std::acos(std::min(1.0, static_cast<double>(hit.normal.y))));
        const double pred = deg(std::asin(static_cast<double>(x / rs)));
        std::printf("    %8.3f m %16.3f %22.3f\n", static_cast<double>(x), tilt, pred);
        worst = std::max(worst, static_cast<float>(std::fabs(tilt - pred)));
    }
    std::printf("    (the worst %.3f deg is GJK's: a curved shape against an edge, a centimetre apart, stalls —\n"
                "     8.5 F.5 — and a tighter tolerance changes nothing, because it stops before it gets there)\n",
                static_cast<double>(worst));
    check(worst < 0.25f, "E3: on an edge the normal points from the edge to the capsule's centre, to 0.25 deg");

    // --- E4: how far above its skin it actually stands ------------------------
    // The cast steps from GJK's certified LOWER bound: the width of the slab
    // along GJK's direction. When that direction is off by a small angle e, the
    // slab's far face is set by the obstacle's far corners, and the bound is low
    // by roughly (the obstacle's size) x e. So the character stops short by an
    // amount that scales with the SIZE of what it stands on — not with GJK's
    // tolerance, which cannot go below what floats allow (8.5 F.5).
    std::printf("\n  walking down a 30 deg ramp at 1 m/s, landing by a vertical cast every step:\n"
                "  the gap it actually stands at (skin %.3f m)\n",
                static_cast<double>(base.skin));
    std::printf("    %-24s %14s %14s %14s\n", "ramp half-length", "GJK tol 1e-4", "", "GJK tol 1e-7");
    float spread_big = 0.0f;
    float spread_small = 0.0f;
    for (float half : {3.0f, 30.0f})
    {
        float lo_hi[2][2] = {};
        int col = 0;
        for (float tol : {1e-4f, 1e-7f})
        {
            const float alpha = rad(30.0f);
            scene s;
            (void)add_ramp(s, alpha, vec3{0.0f, 0.0f, 0.0f}, half);
            walker w;
            w.cfg.cast.gjk.tolerance = tol;
            w.cfg.lay_along_ground = false;   // so that every step LANDS, by a vertical cast
            const vec3 n{-std::sin(alpha), std::cos(alpha), 0.0f};
            const vec3 at{0.5f * half, 0.5f * half * std::tan(alpha), 0.0f};
            w.place_at(s, at + n * (w.cfg.radius + w.cfg.skin) + vec3{0.0f, w.cfg.half_height, 0.0f});
            float g_lo = 1.0f;
            float g_hi = 0.0f;
            for (int i = 0; i < 40; ++i)
            {
                w.frame(s, vec3{-1.0f, 0.0f, 0.0f});
                const vec3 foot = w.ch.position - vec3{0.0f, w.cfg.half_height, 0.0f};
                const float gap = dot(foot, n) - w.cfg.radius;
                g_lo = std::min(g_lo, gap);
                g_hi = std::max(g_hi, gap);
            }
            lo_hi[col][0] = g_lo;
            lo_hi[col][1] = g_hi;
            ++col;
        }
        std::printf("    %8.0f m               %6.2f..%6.2f mm %14s %6.2f..%6.2f mm\n", static_cast<double>(half),
                    1000.0 * static_cast<double>(lo_hi[0][0]), 1000.0 * static_cast<double>(lo_hi[0][1]), "",
                    1000.0 * static_cast<double>(lo_hi[1][0]), 1000.0 * static_cast<double>(lo_hi[1][1]));
        const float spread = lo_hi[0][1] - base.skin;
        if (half > 10.0f) { spread_big = spread; } else { spread_small = spread; }
        check(std::fabs(lo_hi[0][1] - lo_hi[1][1]) < 1e-4f, "E4: a tighter GJK tolerance changes nothing");
    }
    check(spread_big > 10.0f * spread_small && spread_small < 2e-4f,
          "E4: it stands above its skin by an amount that scales with the ramp's size");
}

// ---------------------------------------------------------------------------
// F: snapping to the ground
// ---------------------------------------------------------------------------

void section_f()
{
    rule("§7  SNAPPING TO THE GROUND");
    const character_config base{};
    const float alpha = rad(30.0f);

    // --- F1: down a uniform slope, gravity alone -----------------------------
    const float reach = 2.0f * base.skin;
    const float v_skip = skip_speed(k_g, k_h, alpha, reach);
    std::printf("  walking DOWN a uniform 30 deg slope for 1 s: frames spent in the air\n");
    std::printf("    %-8s %28s %28s\n", "speed", "horizontal + gravity, no snap", "laid along the ground");
    bool skip_ok = true;
    bool laid_ok = true;
    for (float v : {0.5f, 1.0f, 2.0f, 2.3f, 2.45f, 3.0f, 6.0f})
    {
        int air[2] = {};
        for (int k = 0; k < 2; ++k)
        {
            scene s;
            (void)add_ramp(s, alpha, vec3{0.0f, 0.0f, 0.0f}, 30.0f);
            walker w;
            w.cfg.lay_along_ground = (k == 1);
            w.cfg.snap_distance = 0.0f;
            const vec3 n{-std::sin(alpha), std::cos(alpha), 0.0f};
            const vec3 foot{12.0f, 12.0f * std::tan(alpha), 0.0f};
            w.place_at(s, foot + n * (w.cfg.radius + w.cfg.skin) + vec3{0.0f, w.cfg.half_height, 0.0f});
            for (int i = 0; i < 60; ++i)
            {
                w.frame(s, vec3{-v, 0.0f, 0.0f});
                if (!w.ch.grounded) { ++air[k]; }
            }
        }
        std::printf("    %4.2f m/s %28d %28d\n", static_cast<double>(v), air[0], air[1]);
        if ((v < 0.97f * v_skip && air[0] != 0) || (v > 1.03f * v_skip && air[0] == 0)) { skip_ok = false; }
        if (air[1] != 0) { laid_ok = false; }
    }
    std::printf("    predicted: it leaves the slope above (g h^2 + 2 skin) / (h tan a) = %.4f m/s\n",
                static_cast<double>(v_skip));
    std::printf("    (with no probe reach at all that is g h / tan a = %.4f m/s)\n",
                static_cast<double>(skip_speed(k_g, k_h, alpha)));
    // The onset, bisected, on a ramp too short for §E4's looseness to matter
    // and on the long one above.
    auto skips = [&](float v, float half) {
        scene s;
        (void)add_ramp(s, alpha, vec3{0.0f, 0.0f, 0.0f}, half);
        walker w;
        w.cfg.lay_along_ground = false;
        w.cfg.snap_distance = 0.0f;
        const vec3 n{-std::sin(alpha), std::cos(alpha), 0.0f};
        const vec3 at{0.75f * half, 0.75f * half * std::tan(alpha), 0.0f};
        w.place_at(s, at + n * (w.cfg.radius + w.cfg.skin) + vec3{0.0f, w.cfg.half_height, 0.0f});
        for (int i = 0; i < 60; ++i)
        {
            w.frame(s, vec3{-v, 0.0f, 0.0f});
            if (!w.ch.grounded) { return true; }
        }
        return false;
    };
    float onset[2] = {};
    int k = 0;
    for (float half : {4.0f, 30.0f})
    {
        float lo = 1.5f;
        float hi = 3.0f;
        for (int it = 0; it < 20; ++it)
        {
            const float mid = 0.5f * (lo + hi);
            if (skips(mid, half)) { hi = mid; }
            else { lo = mid; }
        }
        onset[k++] = hi;
    }
    std::printf("    the onset, bisected: %.4f m/s on an 8 m ramp, %.4f m/s on a 60 m one — the long ramp's\n"
                "    looser certificate (§E4) leaves the character higher and eats the probe's reach\n",
                static_cast<double>(onset[0]), static_cast<double>(onset[1]));
    check(skip_ok, "F1: a gravity-only character skips down a slope above the predicted speed");
    check(std::fabs(onset[0] - v_skip) < 0.005f * v_skip, "F1: on a short ramp the onset is the prediction");
    check(onset[1] < onset[0], "F1: on a long one it comes earlier");
    check(laid_ok, "F1: laid along the ground, it never leaves a uniform slope");

    // --- F2: over a crest ----------------------------------------------------
    // Flat, then a 30 deg downhill, then flat again 12 m further down. Frames
    // in the air are counted only over the slope itself.
    const float rs = base.radius + base.skin;
    const float probe = k_g * k_h * k_h + 2.0f * base.skin;
    const float roll = std::sqrt(rs * rs - (rs - probe) * (rs - probe));
    std::printf("\n  running over a crest onto a 30 deg downhill\n");
    std::printf("    the round bottom rolls over the edge while one step's travel keeps the drop to it within\n"
                "    the probe's reach: v h < sqrt((r+s)^2 - (r+s-reach)^2) = %.4f m, so v < %.3f m/s\n",
                static_cast<double>(roll), static_cast<double>(roll / k_h));
    std::printf("    %-9s %22s %22s %18s\n", "speed", "no snap: in the air", "projectile 2v tan a/g", "snap: in the air");
    bool crest_ok = true;
    for (float v : {3.0f, 6.0f, 7.5f, 10.0f, 14.0f})
    {
        int air[2] = {};
        for (int k = 0; k < 2; ++k)
        {
            scene s;
            (void)add_slab(s, -20.0f, 0.0f, 0.0f);
            const float run = 60.0f;
            (void)add_ramp(s, -alpha, vec3{0.5f * run * std::cos(alpha), -0.5f * run * std::sin(alpha), 0.0f},
                           0.5f * run);
            (void)add_slab(s, run * std::cos(alpha), 80.0f, -run * std::sin(alpha));
            walker w;
            w.cfg.snap_distance = k == 0 ? 0.0f : base.snap_distance;
            w.place_at(s, vec3{-2.0f, standing_y(w.cfg), 0.0f});
            for (int i = 0; i < 240; ++i)
            {
                w.frame(s, vec3{v, 0.0f, 0.0f});
                const float x = w.ch.position.x;
                if (x > 0.0f && x < 0.9f * run * std::cos(alpha) && !w.ch.grounded) { ++air[k]; }
            }
        }
        const float pred = 2.0f * v * std::tan(alpha) / k_g;
        std::printf("    %5.1f m/s %15d (%.3f s) %20.3f s %18d\n", static_cast<double>(v), air[0],
                    static_cast<double>(air[0]) * k_h, static_cast<double>(pred), air[1]);
        if (air[1] != 0) { crest_ok = false; }
        if (v * k_h < 0.95f * roll && air[0] != 0) { crest_ok = false; }
        if (v * k_h > 1.05f * roll && std::fabs(static_cast<float>(air[0]) * k_h - pred) > 0.1f * pred + 2.0f * k_h)
        {
            crest_ok = false;
        }
    }
    check(crest_ok, "F2: below the roll speed a crest is free; above it, a projectile — unless it snaps");

    // --- F3: down a staircase ------------------------------------------------
    std::printf("\n  walking DOWN eight 18 cm stairs (30 cm treads) at 2 m/s\n");
    for (float snap : {0.0f, base.snap_distance})
    {
        scene s;
        (void)add_slab(s, -10.0f, 0.0f, 0.0f);
        for (int k = 1; k <= 8; ++k)
        {
            (void)add_slab(s, 0.3f * static_cast<float>(k - 1), 0.3f * static_cast<float>(k), -0.18f * static_cast<float>(k));
        }
        (void)add_slab(s, 2.4f, 20.0f, -1.44f - 0.18f);
        walker w;
        w.cfg.snap_distance = snap;
        w.place_at(s, vec3{-1.0f, standing_y(w.cfg), 0.0f});
        int air = 0;
        int snaps = 0;
        float lurch = 0.0f;
        for (int i = 0; i < 120; ++i)
        {
            const vec3 before = w.ch.position;
            w.frame(s, vec3{2.0f, 0.0f, 0.0f});
            if (!w.ch.grounded) { ++air; }
            if (w.last.snapped) { ++snaps; }
            lurch = std::max(lurch, w.ch.position.x - before.x - 2.0f * k_h);
        }
        std::printf("    snap %.2f m: %3d frames in the air, %2d snaps, worst extra forward in one step %.4f m\n",
                    static_cast<double>(snap), air, snaps, static_cast<double>(lurch));
        if (snap > 0.0f)
        {
            check(air == 0, "F3: with the snap, a staircase is walked down without leaving it");
            check(lurch <= step_forward_min(base.radius + base.skin, base.max_slope) + 1e-3f,
                  "F3: rolling off each edge costs no more than (r + skin)(1 - sin 45)");
        }
        else { check(air > 10, "F3: without it, every step is a small fall"); }
    }

    // --- F4: a ledge is not a step -------------------------------------------
    std::printf("\n  walking off a ledge at 2 m/s\n");
    for (float drop : {0.3f, 0.5f})
    {
        scene s;
        (void)add_slab(s, -10.0f, 0.0f, 0.0f);
        (void)add_slab(s, 0.0f, 20.0f, -drop);
        walker w;
        w.place_at(s, vec3{-1.0f, standing_y(w.cfg), 0.0f});
        int air = 0;
        float drop_seen = 0.0f;
        for (int i = 0; i < 90; ++i)
        {
            w.frame(s, vec3{2.0f, 0.0f, 0.0f});
            if (!w.ch.grounded) { ++air; }
            if (w.last.snapped) { drop_seen = std::max(drop_seen, w.last.snap_drop); }
        }
        std::printf("    a %.2f m drop (snap reaches %.2f): %2d frames in the air, largest snap %.4f m\n",
                    static_cast<double>(drop), static_cast<double>(base.snap_distance), air,
                    static_cast<double>(drop_seen));
        if (drop < base.snap_distance) { check(air == 0, "F4: a drop inside the snap distance is snapped down"); }
        else { check(air > 5 && drop_seen < 0.1f, "F4: a drop beyond it is fallen, not snapped"); }
    }
}

// ---------------------------------------------------------------------------
// G: stepping up
// ---------------------------------------------------------------------------

void section_g()
{
    rule("§8  STEPPING UP");
    const character_config base{};
    const float rs = base.radius + base.skin;

    // --- G1: a staircase -----------------------------------------------------
    {
        scene s;
        (void)add_floor(s);
        for (int k = 1; k <= 8; ++k)
        {
            (void)add_slab(s, 0.3f * static_cast<float>(k - 1), 30.0f, 0.18f * static_cast<float>(k));
        }
        walker w;
        w.place_at(s, vec3{-1.0f, standing_y(w.cfg), 0.0f});
        int steps = 0;
        float lurch = 0.0f;
        float top_t = -1.0f;
        int air = 0;
        for (int i = 0; i < 180; ++i)
        {
            const vec3 before = w.ch.position;
            w.frame(s, vec3{2.0f, 0.0f, 0.0f});
            if (w.last.stepped) { ++steps; }
            if (!w.ch.grounded) { ++air; }
            lurch = std::max(lurch, w.ch.position.x - before.x - 2.0f * k_h);
            if (top_t < 0.0f && w.ch.position.y >= standing_y(w.cfg, 1.44f) - 1e-3f)
            {
                top_t = static_cast<float>(i + 1) * k_h;
            }
        }
        const float fmin = step_forward_min(rs, base.max_slope);
        std::printf("  up eight 18 cm stairs at 2 m/s: %d step-ups, %d frames in the air, at the top after %.3f s\n",
                    steps, air, static_cast<double>(top_t));
        std::printf("    worst extra forward in one step %.4f m: a step-up carries at least (r + skin)(1 - sin 45)\n"
                    "    = %.4f m, where a frame's walk is %.4f m\n",
                    static_cast<double>(lurch), static_cast<double>(fmin), static_cast<double>(2.0f * k_h));
        check(top_t > 0.0f && air == 0, "G1: the staircase is climbed without leaving the ground");
        check(lurch <= fmin + 1e-3f, "G1: no step lurches further than the least landing forward");
    }

    // --- G2: the tallest ledge -----------------------------------------------
    auto climbs = [&](float height) {
        scene s;
        (void)add_floor(s);
        (void)add_slab(s, 1.0f, 12.0f, height);
        walker w;
        w.place_at(s, vec3{0.0f, standing_y(w.cfg), 0.0f});
        for (int i = 0; i < 120; ++i) { w.frame(s, vec3{1.0f, 0.0f, 0.0f}); }
        return w.ch.position.x > 1.5f && w.ch.position.y > standing_y(w.cfg, height) - 1e-3f;
    };
    float lo = 0.1f;
    float hi = 0.5f;
    for (int it = 0; it < 24; ++it)
    {
        const float mid = 0.5f * (lo + hi);
        if (climbs(mid)) { lo = mid; }
        else { hi = mid; }
    }
    const float tallest = base.step_height + free_curb_height(rs, base.max_slope);
    std::printf("\n  the tallest ledge climbed with step_height = %.2f m: %.5f m\n", static_cast<double>(base.step_height),
                static_cast<double>(lo));
    std::printf("    — not step_height: after the step-up the round bottom rolls over what is left, and the two\n"
                "      add: step_height + (r + skin)(1 - cos 45) = %.5f m\n", static_cast<double>(tallest));
    check(std::fabs(lo - tallest) < 1.5e-3f, "G2: the tallest ledge climbed is step_height plus the free curb");

    // --- G3: how far forward a step has to go --------------------------------
    std::printf("\n  up, across by f, down: the least f that lands with a walkable normal\n");
    std::printf("    %-8s %14s %14s %14s\n", "riser", "short of edge", "least f", "predicted");
    float worst = 0.0f;
    for (float riser : {0.15f, 0.25f, 0.40f})
    {
        scene s;
        (void)add_floor(s);
        (void)add_slab(s, 1.0f, 12.0f, riser);
        walker w;
        w.cfg.step_height = 0.0f;
        w.place_at(s, vec3{0.0f, standing_y(w.cfg), 0.0f});
        for (int i = 0; i < 90; ++i) { w.frame(s, vec3{1.0f, 0.0f, 0.0f}); }
        const vec3 at = w.ch.position;
        const float short_of = 1.0f - at.x;                   // centre to the edge, horizontally
        const float up = riser + 0.05f;
        auto normal_y = [&](float f) {
            character_config cfg = base;
            const character_world view = w.view(s);
            vec3 p = at;
            const character_hit u = sweep(p, vec3{0.0f, up, 0.0f}, view, cfg, sweep_filter::everything);
            p.y += up * u.t;
            const character_hit a = sweep(p, vec3{f, 0.0f, 0.0f}, view, cfg, sweep_filter::everything);
            p.x += f * a.t;
            const character_hit d = sweep(p, vec3{0.0f, -up - 0.05f, 0.0f}, view, cfg, sweep_filter::everything);
            return d.hit ? d.normal.y : -1.0f;
        };
        float flo = 0.0f;
        float fhi = 0.3f;
        for (int it = 0; it < 30; ++it)
        {
            const float mid = 0.5f * (flo + fhi);
            if (walkable(vec3{0.0f, normal_y(mid), 0.0f}, base)) { fhi = mid; }
            else { flo = mid; }
        }
        const float pred = short_of - rs * std::sin(base.max_slope);
        std::printf("    %4.2f m %14.5f %14.5f %14.5f\n", static_cast<double>(riser), static_cast<double>(short_of),
                    static_cast<double>(fhi), static_cast<double>(pred));
        worst = std::max(worst, std::fabs(fhi - pred));
    }
    std::printf("    worst case, any riser at least r + skin tall: %.5f m\n",
                static_cast<double>(step_forward_min(rs, base.max_slope)));
    check(worst < 1e-3f, "G3: the least forward is the distance short of the edge less (r + skin) sin 45");

    // --- G4: a steep slope is not a step --------------------------------------
    {
        scene s;
        (void)add_floor(s);
        const float ar = rad(60.0f);
        (void)add_ramp(s, ar, vec3{8.0f * std::cos(ar), 8.0f * std::sin(ar), 0.0f});
        walker w;
        w.place_at(s, vec3{-1.0f, standing_y(w.cfg), 0.0f});
        float peak = 0.0f;
        int tried = 0;
        for (int i = 0; i < 120; ++i)
        {
            w.frame(s, vec3{3.0f, 0.0f, 0.0f});
            peak = std::max(peak, w.ch.position.y - standing_y(w.cfg));
            tried += w.last.stepped ? 1 : 0;
        }
        std::printf("\n  pushing into a 60 deg slope with the step-up on: highest %.4f m, %d steps taken\n",
                    static_cast<double>(peak), tried);
        check(tried == 0 && peak < 2e-3f, "G4: a step that lands on a steep slope is refused");
    }
}

// ---------------------------------------------------------------------------
// H: tunnelling, revisited
// ---------------------------------------------------------------------------

void section_h()
{
    rule("§9  TUNNELLING, REVISITED");
    const character_config base{};

    // --- H1: the rigid capsule, sampled at 200 phases per speed --------------
    const float wall = 0.1f;
    std::printf("  a rigid capsule (r = %.1f) flung at a %.0f cm wall, 200 phases of the step each\n",
                static_cast<double>(base.radius), 100.0 * static_cast<double>(wall));
    std::printf("    %-9s %12s %24s\n", "speed", "through", "1 - (w/2 + r)/(v h)");
    float worst = 0.0f;
    for (float v : {10.0f, 20.0f, 25.0f, 30.0f, 42.0f, 60.0f, 100.0f})
    {
        int through = 0;
        const int phases = 200;
        for (int k = 0; k < phases; ++k)
        {
            scene s;
            s.world.set_gravity(vec3{});
            (void)add_block(s, vec3{0.5f * wall, 0.0f, 0.0f}, vec3{0.5f * wall, 5.0f, 5.0f});
            const float phase = (static_cast<float>(k) + 0.5f) / static_cast<float>(phases);
            const float x0 = -(base.radius + 1e-3f) - phase * v * k_h;
            const std::uint32_t p = add_rigid_person(s, vec3{x0, 0.0f, 0.0f}, base);
            s.body(p).state.velocity = vec3{v, 0.0f, 0.0f};
            const int n = static_cast<int>(3.0f / (v * k_h)) + 4;
            for (int i = 0; i < n; ++i) { s.step(); }
            if (s.body(p).state.position.x > 0.5f * wall) { ++through; }
        }
        const float frac = static_cast<float>(through) / 200.0f;
        const float pred = std::max(0.0f, 1.0f - (0.5f * wall + base.radius) / (v * k_h));
        std::printf("    %5.0f m/s %11.1f%% %23.1f%%\n", static_cast<double>(v), 100.0 * static_cast<double>(frac),
                    100.0 * static_cast<double>(pred));
        worst = std::max(worst, std::fabs(frac - pred));
    }
    check(worst < 0.011f, "H1: a rigid capsule tunnels exactly as often as its arrival phase says");

    // --- H2: the controller ---------------------------------------------------
    std::printf("\n  the controller at a 1 cm wall\n");
    int passed = 0;
    float worst_gap = 0.0f;
    for (float v : {10.0f, 100.0f, 1000.0f, 10000.0f})
    {
        scene s;
        (void)add_floor(s, 2000.0f);
        (void)add_block(s, vec3{0.005f, 2.0f, 0.0f}, vec3{0.005f, 2.0f, 5.0f});
        walker w;
        w.place_at(s, vec3{-2.0f, standing_y(w.cfg), 0.0f});
        for (int i = 0; i < 30; ++i) { w.frame(s, vec3{v, 0.0f, 0.0f}); }
        const float gap = 0.0f - (w.ch.position.x + w.cfg.radius);
        std::printf("    %6.0f m/s (%.3f m a step): stopped %.6f m from the face (skin %.3f)\n",
                    static_cast<double>(v), static_cast<double>(v * k_h), static_cast<double>(gap),
                    static_cast<double>(w.cfg.skin));
        if (w.ch.position.x > 0.0f) { ++passed; }
        worst_gap = std::max(worst_gap, std::fabs(gap - w.cfg.skin));
    }
    check(passed == 0, "H2: the controller passes no wall at any speed");
    check(worst_gap < 1e-4f, "H2: and stops a skin from its face");
}

// ---------------------------------------------------------------------------
// I: standing on things that move
// ---------------------------------------------------------------------------

[[nodiscard]] const char* name_of(character_config::carry_rule r)
{
    switch (r)
    {
    case character_config::carry_rule::transform: return "by transform";
    case character_config::carry_rule::velocity:  return "by velocity";
    case character_config::carry_rule::none:      return "not at all";
    }
    return "?";
}

/// The angle a vector in the ground plane makes about +y, the way `quat_y` turns.
[[nodiscard]] double yaw_of(vec3 v) { return std::atan2(-static_cast<double>(v.z), static_cast<double>(v.x)); }

void section_i()
{
    rule("§10  STANDING ON THINGS THAT MOVE");

    // --- I1: a turntable, one revolution -------------------------------------
    std::printf("  a rider 1.5 m from the axis of a turntable, one revolution\n");
    std::printf("    %-8s %-14s %12s %14s %14s %12s\n", "omega", "carried", "radius (m)", "growth", "predicted",
                "left behind");
    for (float omega : {1.0f, 2.0f})
    {
        // One linearised step turns by 2 atan(w h / 2), not w h (8.3 §7).
        const double turn = 2.0 * std::atan(0.5 * static_cast<double>(omega) * static_cast<double>(k_h));
        const int n = static_cast<int>(std::lround(2.0 * k_pi / turn));
        for (auto rule_ : {character_config::carry_rule::transform, character_config::carry_rule::velocity,
                           character_config::carry_rule::none})
        {
            scene s;
            rigid_body plate = make_kinematic(vec3{0.0f, 0.1f, 0.0f}, vec3{});
            plate.angular_velocity = vec3{0.0f, omega, 0.0f};
            (void)s.add(plate, box_shape(vec3{3.5f, 0.1f, 3.5f}));
            walker w;
            w.cfg.carry = rule_;
            w.place_at(s, vec3{1.5f, standing_y(w.cfg, 0.2f), 0.0f});
            quat facing{};
            double turned = 0.0;          // the rider's own yaw, unwrapped, summed step by step
            double yaw_prev = yaw_of(w.ch.position);
            for (int i = 0; i < n; ++i)
            {
                s.step();
                w.frame(s, vec3{});
                facing = normalised(w.last.carried_turn * facing);
                const double yaw = yaw_of(w.ch.position);
                turned += std::remainder(yaw - yaw_prev, 2.0 * k_pi);
                yaw_prev = yaw;
            }
            const vec3 p = w.ch.position;
            const float radius = length(vec3{p.x, 0.0f, p.z});
            const double plate_angle = static_cast<double>(n) * turn;
            const double lag = deg(plate_angle - turned);
            const float pred = rule_ == character_config::carry_rule::velocity ? velocity_carry_growth(omega, k_h) : 1.0f;
            std::printf("    %4.1f /s %-14s %12.5f %14.5f %14.5f %12.3f\n", static_cast<double>(omega), name_of(rule_),
                        static_cast<double>(radius), static_cast<double>(radius / 1.5f), static_cast<double>(pred),
                        lag);
            if (rule_ == character_config::carry_rule::transform)
            {
                check(std::fabs(radius - 1.5f) < 1e-3f && std::fabs(lag) < 0.05,
                      "I1: carried by transform, a rider stays where it stood");
                // Half-angle, and a full turn is q = -1: compare modulo a turn.
                const double face = 2.0 * std::atan2(static_cast<double>(facing.v.y), static_cast<double>(facing.w));
                const double face_err = deg(std::remainder(face - plate_angle, 2.0 * k_pi));
                check(std::fabs(face_err) < 0.05, "I1: and turns with the platform");
            }
            if (rule_ == character_config::carry_rule::velocity)
            {
                check(std::fabs(radius / 1.5f - pred) < 2e-3f * pred, "I1: carried by velocity it spirals out at e^(pi w h)");
            }
            if (rule_ == character_config::carry_rule::none)
            {
                check(std::fabs(lag - deg(plate_angle)) < 0.01, "I1: not carried, the platform turns under it and it does not move");
            }
        }
    }

    // --- I2: a lift that only translates -------------------------------------
    std::printf("\n  a lift moving (2, 1.5, 0) m/s for 2 s: the rider's drift from its spot\n");
    for (auto rule_ : {character_config::carry_rule::transform, character_config::carry_rule::velocity,
                       character_config::carry_rule::none})
    {
        scene s;
        const std::uint32_t lift = s.add(make_kinematic(vec3{0.0f, 0.1f, 0.0f}, vec3{2.0f, 1.5f, 0.0f}),
                                         box_shape(vec3{2.0f, 0.1f, 2.0f}));
        walker w;
        w.cfg.carry = rule_;
        w.place_at(s, vec3{0.5f, standing_y(w.cfg, 0.2f), 0.0f});
        const vec3 offset0 = w.ch.position - s.body(lift).state.position;
        for (int i = 0; i < 120; ++i)
        {
            s.step();
            w.frame(s, vec3{});
        }
        const vec3 drift = w.ch.position - s.body(lift).state.position - offset0;
        std::printf("    %-14s drift (%8.5f, %8.5f, %8.5f) m%s\n", name_of(rule_), static_cast<double>(drift.x),
                    static_cast<double>(drift.y), static_cast<double>(drift.z),
                    w.ch.grounded ? "" : "   — and it is no longer on the lift");
        if (rule_ != character_config::carry_rule::none)
        {
            check(length(drift) < 1e-3f, "I2: a pure translation is carried exactly by either rule");
        }
    }
}

// ---------------------------------------------------------------------------
// J: one owner — pushing, and being pushed
// ---------------------------------------------------------------------------

/// A crate pushed by a character at 2 m/s for 1.5 s, then left.
struct push_run
{
    float speed_during = 0.0f;   ///< the crate's REAL velocity, mean over the last half second of pushing
    float moved_during = 0.0f;   ///< how far the crate moved in that half second, over the time: its apparent speed
    float overlap = 0.0f;        ///< mean proxy-into-crate depth over the same half second
    float coast = 0.0f;          ///< how far the crate travels after the character stops
    float release_speed = 0.0f;
    bool through = false;        ///< the character ended up past the crate's centre
    bool slept = false;          ///< the crate was asleep when the character reached it
};

enum class proxy_mode { steered, teleported, teleported_baumgarte };

push_run push(proxy_mode mode, float crate_mass, bool sleep = true)
{
    scene s;
    (void)add_floor(s);
    s.sleep.enabled = sleep;
    if (mode == proxy_mode::teleported_baumgarte) { s.cfg.correction = position_correction::baumgarte; }
    const vec3 half{0.3f, 0.3f, 0.3f};
    const std::uint32_t crate = s.add(make_box(vec3{1.0f, 0.3f, 0.0f}, crate_mass, half), box_shape(half));
    walker w;
    w.place_at(s, vec3{0.0f, standing_y(w.cfg), 0.0f});
    w.proxy = s.add(make_kinematic(w.ch.position, vec3{}), capsule_shape(w.cfg.radius, w.cfg.half_height));
    for (int i = 0; i < 20; ++i) { s.step(); }   // the crate settles

    push_run out;
    float release_x = 0.0f;
    int samples = 0;
    float x_start = 0.0f;
    for (int i = 0; i < 240; ++i)
    {
        rigid_body& proxy = s.body(w.proxy);
        if (mode == proxy_mode::steered) { steer_proxy(proxy, w.ch.position, k_h); }
        else
        {
            proxy.state.position = w.ch.position;
            proxy.state.velocity = vec3{};
        }
        s.step();
        const bool pushing = i < 90;
        w.frame(s, pushing ? vec3{2.0f, 0.0f, 0.0f} : vec3{});

        const rigid_body& c = s.body(crate);
        if (i == 12) { out.slept = c.sleeping; }
        if (i == 89) { out.through = w.ch.position.x > c.state.position.x; }
        if (i >= 60 && i < 90)
        {
            if (i == 60) { x_start = c.state.position.x; }
            out.speed_during += c.state.velocity.x;
            const placed_shape a = place(s.shapes[w.proxy], s.body(w.proxy).state.position, quat{});
            const placed_shape b = place(s.shapes[crate], c.state.position, c.orientation);
            const separation sep = collide(a.view(), b.view());
            out.overlap += std::max(0.0f, sep.depth);
            ++samples;
        }
        if (i == 89)
        {
            out.moved_during = (c.state.position.x - x_start) / (29.0f * k_h);
            release_x = c.state.position.x;
            out.release_speed = c.state.velocity.x;
        }
    }
    out.speed_during /= static_cast<float>(samples);
    out.overlap /= static_cast<float>(samples);
    out.coast = s.body(crate).state.position.x - release_x;
    return out;
}

void section_j()
{
    rule("§11  ONE OWNER: PUSHING, AND BEING PUSHED");
    const character_config base{};

    // --- J1: a 20 kg crate, four ways ----------------------------------------
    std::printf("  a 20 kg crate pushed at 2 m/s for 1.5 s, then left (mu = 0.6)\n");
    std::printf("    %-34s %11s %12s %10s %10s  %s\n", "the proxy", "crate v", "crate moves", "overlap", "coast", "");
    const push_run steered = push(proxy_mode::steered, 20.0f);
    const push_run tele = push(proxy_mode::teleported, 20.0f);
    const push_run tele_awake = push(proxy_mode::teleported, 20.0f, false);
    const push_run tele_b = push(proxy_mode::teleported_baumgarte, 20.0f, false);
    auto row = [](const char* name, const push_run& r) {
        std::printf("    %-34s %9.4f/s %10.4f/s %7.2f mm %8.4f m  %s%s\n", name, static_cast<double>(r.speed_during),
                    static_cast<double>(r.moved_during), 1000.0 * static_cast<double>(r.overlap),
                    static_cast<double>(r.coast), r.slept ? "(asleep when reached) " : "",
                    r.through ? "WALKED THROUGH IT" : "");
    };
    row("steered (the chord)", steered);
    row("teleported", tele);
    row("teleported, crate kept awake", tele_awake);
    row("teleported, awake, Baumgarte", tele_b);

    // The coast: the discrete v^2/(2 mu g) from the release speed — plus ONE
    // STEP at that speed, because the proxy is steered to where the character
    // WAS, and so lets go of the crate a step after the character stops.
    const double dv = 0.6 * static_cast<double>(k_g) * static_cast<double>(k_h);
    double v = static_cast<double>(steered.release_speed);
    double sum = 0.0;
    while (v - dv > 0.0) { v -= dv; sum += v * static_cast<double>(k_h); }
    const double lagged = sum + static_cast<double>(steered.release_speed) * static_cast<double>(k_h);
    std::printf("    steered: released at %.4f m/s; the discrete stop is %.4f m, and the proxy's one-step lag\n"
                "    adds v h: %.4f m\n", static_cast<double>(steered.release_speed), sum, lagged);
    std::printf("    steered, the overlap is the solver's penetration slop, 5 mm (8.10 §8)\n");
    check(std::fabs(steered.speed_during - 2.0f) < 0.02f, "J1: steered, the crate's velocity is the character's");
    check(std::fabs(static_cast<double>(steered.coast) - lagged) < 2e-3, "J1: steered, it coasts v^2/(2 mu g), one step late");
    check(tele.slept && tele.through, "J1: teleported, a sleeping crate never wakes, and is walked through");
    check(std::fabs(tele_awake.speed_during) < 0.05f && tele_awake.moved_during > 1.9f,
          "J1: teleported and awake, the crate moves at 2 m/s with a velocity of zero");
    // Teleported, the crate is moved ONLY by the position pass, which moves it
    // beta·(d − slop) a step (8.10 §8). To move it v·h a step the proxy must
    // start each step v·h/beta + slop deep, and ends it v·h shallower.
    const solver_config sc{};
    const float vh = 2.0f * k_h;
    const float sunk = sc.penetration_slop + vh / sc.baumgarte - vh;
    std::printf("    teleported: sunk slop + v h / beta - v h = %.2f mm into it; when the character stops the\n"
                "    crate is pushed out of the excess, about v h / beta = %.4f m, and has no velocity to keep\n",
                1000.0 * static_cast<double>(sunk), static_cast<double>(vh / sc.baumgarte));
    check(std::fabs(tele_awake.overlap - sunk) < 1e-3f, "J1: teleported, the character sinks v h / beta into it");
    check(std::fabs(tele_awake.release_speed) < 0.01f && std::fabs(tele_awake.coast - vh / sc.baumgarte) < 0.05f * vh / sc.baumgarte,
          "J1: teleported, it is pushed out of the overlap and no further");
    check(std::fabs(tele_b.speed_during - 2.0f) < 0.02f && std::fabs(tele_b.overlap - sunk) < 1e-3f,
          "J1: under Baumgarte the crate gets its velocity back, and the character is sunk just as deep");

    // --- J2: too heavy to push -----------------------------------------------
    {
        const push_run heavy = push(proxy_mode::steered, 100.0f);
        std::printf("\n  a 100 kg crate (push limit %.0f kg): crate v %.5f m/s, moved %.5f m/s — a wall\n",
                    static_cast<double>(base.push_mass_limit), static_cast<double>(heavy.speed_during),
                    static_cast<double>(heavy.moved_during));
        check(std::fabs(heavy.moved_during) < 1e-3f, "J2: a body over the push limit is a wall");
    }

    // --- J3: standing on a pushable crate -------------------------------------
    {
        scene s;
        (void)add_floor(s);
        const vec3 half{0.4f, 0.3f, 0.4f};
        const std::uint32_t crate = s.add(make_box(vec3{0.0f, 0.3f, 0.0f}, 20.0f, half), box_shape(half));
        walker w;
        w.ch.position = vec3{0.0f, 2.5f, 0.0f};
        w.proxy = s.add(make_kinematic(w.ch.position, vec3{}), capsule_shape(w.cfg.radius, w.cfg.half_height));
        for (int i = 0; i < 90; ++i)
        {
            steer_proxy(s.body(w.proxy), w.ch.position, k_h);
            s.step();
            w.frame(s, vec3{});
        }
        const float top = s.body(crate).state.position.y + half.y;
        std::printf("\n  dropped onto a 20 kg crate: grounded %s, on body %u (the crate is %u), %.4f m above its top\n"
                    "    — the skin; and the crate carries none of the character's weight, because nothing touches it\n",
                    w.ch.grounded ? "yes" : "no", w.ch.ground_body, crate,
                    static_cast<double>(w.ch.position.y - w.cfg.half_height - w.cfg.radius - top));
        check(std::fabs(w.ch.position.y - w.cfg.half_height - w.cfg.radius - top - w.cfg.skin) < 1e-3f,
              "J3: it stands a skin above the crate's top");
        check(w.ch.grounded && w.ch.ground_body == crate, "J3: a pushable body is still something to stand on");
    }

    // --- J4: being pushed ----------------------------------------------------
    {
        scene s;
        (void)add_floor(s);
        rigid_body ball = make_sphere(vec3{-3.0f, 0.5f, 0.0f}, 200.0f, 0.5f);
        ball.state.velocity = vec3{5.0f, 0.0f, 0.0f};
        const std::uint32_t boulder = s.add(ball, sphere_shape(0.5f));
        walker w;
        w.place_at(s, vec3{0.0f, standing_y(w.cfg), 0.0f});
        w.proxy = s.add(make_kinematic(w.ch.position, vec3{}), capsule_shape(w.cfg.radius, w.cfg.half_height));
        const vec3 start = w.ch.position;
        for (int i = 0; i < 90; ++i)
        {
            steer_proxy(s.body(w.proxy), w.ch.position, k_h);
            s.step();
            w.frame(s, vec3{});
        }
        const float speed = length(s.body(boulder).state.velocity);
        std::printf("  a 200 kg boulder at 5 m/s into a standing character: after 1.5 s it moves at %.4f m/s,\n"
                    "    and the character has moved %.5f m. The proxy is kinematic: infinitely heavy.\n",
                    static_cast<double>(speed), static_cast<double>(length(w.ch.position - start)));
        check(speed < 0.2f && length(w.ch.position - start) < 1e-3f,
              "J4: nothing pushes the character: the coupling is one way");
    }
}

// ---------------------------------------------------------------------------
// K: the bill
// ---------------------------------------------------------------------------

struct bill
{
    double us_per_move = 0.0;
    double sweeps = 0.0;
    double candidates = 0.0;
    double casts = 0.0;
    double newton = 0.0;
    double gjk = 0.0;
};

bill time_walk(scene& s, const walker& start, vec3 wish, int frames, int reps)
{
    bill out;
    double best = 1e30;
    for (int r = 0; r < reps; ++r)
    {
        walker w = start;
        long long sweeps = 0, cand = 0, casts = 0, newton = 0, gjk = 0;
        const clock_type::time_point t0 = clock_type::now();
        for (int i = 0; i < frames; ++i)
        {
            w.frame(s, wish);
            sweeps += w.last.sweeps;
            cand += w.last.candidates;
            casts += w.last.casts;
            newton += w.last.cast_iterations;
            gjk += w.last.gjk_iterations;
        }
        const double us = seconds_since(t0) * 1e6 / frames;
        if (us < best)
        {
            best = us;
            out.sweeps = static_cast<double>(sweeps) / frames;
            out.candidates = static_cast<double>(cand) / frames;
            out.casts = static_cast<double>(casts) / frames;
            out.newton = static_cast<double>(newton) / frames;
            out.gjk = static_cast<double>(gjk) / frames;
        }
    }
    out.us_per_move = best;
    return out;
}

void section_k()
{
    rule("§12  THE BILL");
    std::printf("    %-34s %9s %8s %11s %7s %8s %8s\n", "scene", "us/move", "sweeps", "candidates", "casts",
                "newton", "gjk");
    auto row = [](const char* name, const bill& b) {
        std::printf("    %-34s %9.3f %8.2f %11.2f %7.2f %8.2f %8.2f\n", name, b.us_per_move, b.sweeps, b.candidates,
                    b.casts, b.newton, b.gjk);
    };

    bill flat;
    {
        scene s;
        (void)add_floor(s);
        walker w;
        w.place_at(s, vec3{-20.0f, standing_y(w.cfg), 0.0f});
        flat = time_walk(s, w, vec3{3.0f, 0.0f, 0.0f}, 600, 20);
        row("walking on a flat floor", flat);
    }
    {
        scene s;
        (void)add_floor(s);
        for (int k = 1; k <= 8; ++k)
        {
            (void)add_slab(s, 0.3f * static_cast<float>(k - 1), 30.0f, 0.18f * static_cast<float>(k));
        }
        walker w;
        w.place_at(s, vec3{-1.0f, standing_y(w.cfg), 0.0f});
        row("up eight stairs, 1.5 s", time_walk(s, w, vec3{2.0f, 0.0f, 0.0f}, 90, 50));
    }
    {
        scene s;
        (void)add_floor(s);
        (void)add_wall(s, vec3{}, vec3{-0.5f, 0.0f, 0.866f}, vec3{-0.866f, 0.0f, -0.5f}, 6.0f);
        (void)add_wall(s, vec3{}, vec3{-0.5f, 0.0f, -0.866f}, vec3{-0.866f, 0.0f, 0.5f}, 6.0f);
        walker w;
        w.place_at(s, vec3{-0.8f, standing_y(w.cfg), 0.1f});
        row("pressed into a 60 deg corner", time_walk(s, w, vec3{2.0f, 0.0f, 0.3f}, 600, 20));
    }

    std::printf("\n  walking a clear corridor through N boxes (every body is AABB-tested every sweep)\n");
    double us_1000 = 0.0;
    double us_10000 = 0.0;
    double cand_max = 0.0;
    for (int n : {10, 100, 1000, 10000})
    {
        scene s;
        (void)add_floor(s, 200.0f);
        rng r(8133u);
        for (int i = 0; i < n; ++i)
        {
            float z = r.range(-60.0f, 60.0f);
            if (std::fabs(z) < 2.0f) { z += z < 0.0f ? -2.0f : 2.0f; }
            (void)add_block(s, vec3{r.range(-60.0f, 60.0f), 0.25f, z}, vec3{0.25f, 0.25f, 0.25f});
        }
        walker w;
        w.place_at(s, vec3{-20.0f, standing_y(w.cfg), 0.0f});
        const bill b = time_walk(s, w, vec3{3.0f, 0.0f, 0.0f}, 300, 5);
        char name[64];
        std::snprintf(name, sizeof(name), "%5d boxes", n);
        row(name, b);
        if (n == 1000) { us_1000 = b.us_per_move; }
        if (n == 10000) { us_10000 = b.us_per_move; }
        cand_max = std::max(cand_max, b.candidates);
    }
    std::printf("    10000 boxes against 1000: %.2fx the time, with the same candidates — the cull is linear\n",
                us_10000 / us_1000);
    check(flat.casts < 6.0 && flat.us_per_move < 20.0, "K1: a flat walk is a handful of casts and microseconds");
    check(cand_max < 4.0, "K3: the cull leaves the same few candidates however many bodies there are");
    check(us_10000 / us_1000 > 4.0, "K3: and pays for every body it rejects");
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strncmp(argv[i], "--only=", 7) == 0) { only = argv[i] + 7; }
    }
    auto want = [&](char c) { return only == nullptr || std::strchr(only, c) != nullptr; };

    const clock_type::time_point t0 = clock_type::now();
    if (want('a')) { section_a(); }
    if (want('b')) { section_b(); }
    if (want('c')) { section_c(); }
    if (want('d')) { section_d(); }
    if (want('e')) { section_e(); }
    if (want('f')) { section_f(); }
    if (want('g')) { section_g(); }
    if (want('h')) { section_h(); }
    if (want('i')) { section_i(); }
    if (want('j')) { section_j(); }
    if (want('k')) { section_k(); }

    std::printf("\n%s: %d checks, %d failures  (%.2f s)\n", g_failures == 0 ? "PASS" : "FAIL", g_checks,
                g_failures, seconds_since(t0));
    return g_failures == 0 ? 0 : 1;
}
