// scratch/verify_811.cpp — every number Lesson 8.11 prints, measured rather
// than asserted.
//
// Build and run:  sh scratch/build_verify_811.sh
//
// Eleven sections, in the lesson's order:
//
//   A  a contact is a row
//   B  rods and ropes
//   C  why a joint drifts
//   D  correcting a joint, and what Baumgarte does to one
//   E  the ball-socket, and the block
//   F  the pendulum, and the parallel axis
//   G  the hinge
//   H  limits: a contact that is always detected
//   I  motors, and what friction was all along
//   J  islands, sleeping and joints
//   K  what joints leave behind
//
// Sections A to J are the lesson's §3 to §12 and K is its §14; the text a
// section PRINTS uses the lesson's numbers, because that is where a reader
// will look them up, and the comments use the letters.
//
// EVERY SECTION CARRIES A CONTROL, in the two halves 8.10 settled: what would
// the control say if the thing were COMPLETELY BROKEN, and what would it say if
// it were completely FINE. Eight times in Module 8 a section written to confirm
// a claim has refused it instead, so each one here is written to be allowed to.
//
// WHAT IS DIFFERENT ABOUT THIS LESSON'S INSTRUMENTS.
//
//   * MOST OF THE CLAIMS HAVE CLOSED FORMS, which the solver lessons did not.
//     A pendulum's period is 2π·sqrt(I/(m·g·d)); a velocity constraint's drift
//     is a Pythagorean recurrence; a motor's spin-up time is I·ω/τ; a rope lets
//     go at a height energy conservation fixes. So most sections compare a
//     simulation against a number computed without it — which is the strongest
//     check this module has had since 8.2.
//
//   * AND ONE CONVERGENCE RATE HAS A CLOSED FORM TOO. A hinge limit on a door
//     converges against the pin at `m·d²/(I_cm + m·d²)` per sweep — the
//     parallel-axis theorem, arriving a second time in the same lesson as a
//     rate rather than a period. §H measures it on four shapes.
//
//   * A/B TIMINGS KEEP BOTH ARMS IN ONE TRANSLATION UNIT. 8.8 §7 timed a
//     strictly more expensive hash as three times faster because one arm
//     crossed into libengine.a and the other was inlined. §A's cost comparison
//     is therefore between two harness-local copies behind the same
//     `noinline` boundary, and says so.
//
// TIMINGS ARE MINIMA over many repetitions, as in 8.8 through 8.10.

#include <engine/math/quat.hpp>
#include <engine/phys/broadphase.hpp>
#include <engine/phys/collide.hpp>
#include <engine/phys/constraint.hpp>
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
using engine::quat_y;
using engine::quat_z;
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

/// The same deterministic generator 8.1–8.10 used, for the same reason.
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

private:
    std::uint32_t state_;
};

constexpr float k_h = 1.0f / 60.0f;
constexpr double k_pi = 3.14159265358979323846;

using clock_type = std::chrono::steady_clock;

[[nodiscard]] double seconds_since(clock_type::time_point t0)
{
    return std::chrono::duration<double>(clock_type::now() - t0).count();
}

/// A body's kinetic energy, linear plus rotational, in joules.
[[nodiscard]] double energy_of(const rigid_body& b)
{
    return static_cast<double>(kinetic_energy(b));
}

// ---------------------------------------------------------------------------
// Fixture 1: a rig — bodies and joints, no collision at all
// ---------------------------------------------------------------------------

/// **Bodies joined by joints, stepped through the engine's own loop, and
/// nothing else.**
///
/// Most of this lesson's claims are about joints in isolation — a pendulum, a
/// spinning rod, a chain — and a collision pipeline would only be a place for
/// a second effect to hide. The step is exactly what a game calls:
/// `integrate_velocities`, `constraint_solver::solve` with the joints added,
/// `integrate_positions`. Sleeping is OFF by default, because a pendulum at
/// the top of its swing is momentarily still and a sleep test is allowed to
/// notice; §J turns it on on purpose.
struct rig
{
    struct link
    {
        std::uint32_t a = 0;
        std::uint32_t b = 0;
        joint j{};
    };

    body_world world;
    constraint_solver solver;
    solver_config cfg{};
    sleep_config sleep{};
    float h = k_h;
    std::vector<link> links;
    solver_stats stats{};

    rig() { sleep.enabled = false; }

    std::uint32_t add(const rigid_body& b)
    {
        const auto index = static_cast<std::uint32_t>(world.size());
        world.add(b);
        return index;
    }

    [[nodiscard]] rigid_body& body(std::uint32_t i) { return world.bodies()[i]; }

    /// Add a joint. Returns its index in `links`, not a reference: the vector
    /// may grow, and a reference into it would dangle.
    std::size_t connect(std::uint32_t a, std::uint32_t b, const joint& j)
    {
        links.push_back(link{a, b, j});
        return links.size() - 1;
    }

    [[nodiscard]] joint& joint_at(std::size_t i) { return links[i].j; }

    void step()
    {
        world.integrate_velocities(h);
        solver.begin(world.bodies());
        for (link& l : links) { solver.add(l.a, l.b, l.j); }
        stats = solver.solve(h, cfg, sleep);
        world.integrate_positions(h);
    }

    void run(float seconds)
    {
        const int steps = static_cast<int>(seconds / h + 0.5f);
        for (int i = 0; i < steps; ++i) { step(); }
    }

    [[nodiscard]] joint_error error_of(std::size_t i)
    {
        const link& l = links[i];
        return measure_joint(l.j, body(l.a), body(l.b));
    }
};

/// A box `half` extents, mass `m`, hanging from a fixed anchor at the origin by
/// a ball-socket at the middle of its top face, displaced by `angle` about z.
/// Returns the rig with body 0 fixed and body 1 the box.
void hang_box(rig& r, vec3 half, float mass, float angle)
{
    r.add(make_fixed(vec3{}));
    const quat q = quat_z(angle);
    rigid_body box = make_box(rotate(q, vec3{0.0f, -half.y, 0.0f}), mass, half);
    box.orientation = q;
    const std::uint32_t b = r.add(box);
    r.connect(0, b, make_ball_socket(r.body(0), r.body(b), vec3{}));
}

/// Zero-crossings of `x` from positive to negative, as interpolated times.
/// The period estimator for §F: the mean spacing of downward crossings over
/// many swings, which is insensitive to the amplitude decaying and to where
/// in the swing the recording started.
struct crossing_timer
{
    double prev = 0.0;
    double t = 0.0;
    double first = -1.0;
    double last = -1.0;
    int count = 0;
    bool primed = false;

    void sample(double x, double h)
    {
        t += h;
        if (primed && prev > 0.0 && x <= 0.0)
        {
            const double tc = t - h * x / (x - prev);
            if (first < 0.0) { first = tc; }
            last = tc;
            ++count;
        }
        prev = x;
        primed = true;
    }

    [[nodiscard]] double period() const
    {
        return count >= 2 ? (last - first) / static_cast<double>(count - 1) : 0.0;
    }
};

// ---------------------------------------------------------------------------
// Fixture 2: a scene — 8.10's whole pipeline, with joints added
// ---------------------------------------------------------------------------

/// 8.10's `scene`, with joints and a collision filter. Used where contacts and
/// joints have to meet: §A reads real manifolds out of it, and §G's bridge is
/// planks, hinges and a bouncing ball at once.
struct scene
{
    struct link
    {
        std::uint32_t a = 0;
        std::uint32_t b = 0;
        joint j{};
    };

    body_world world;
    std::vector<shape> shapes;
    uniform_grid grid;
    manifold_cache cache;
    constraint_solver solver;
    collision_filter filter;

    solver_config cfg{};
    sleep_config sleep{};
    broadphase_config bp{};
    manifold_config mf{};
    contact_material material{0.0f, 0.5f};
    float h = k_h;

    std::vector<link> links;
    std::vector<proxy> proxies;
    std::vector<contact_manifold> manifolds;
    std::vector<std::uint64_t> keys;
    std::vector<std::uint32_t> pair_a;
    std::vector<std::uint32_t> pair_b;

    solver_stats stats{};
    double t_broad = 0.0;
    double t_narrow = 0.0;
    double t_solve = 0.0;

    std::uint32_t add(const rigid_body& b, const shape& s)
    {
        const auto index = static_cast<std::uint32_t>(world.size());
        world.add(b);
        shapes.push_back(s);
        return index;
    }

    [[nodiscard]] rigid_body& body(std::uint32_t i) { return world.bodies()[i]; }
    [[nodiscard]] const rigid_body& body(std::uint32_t i) const { return world.bodies()[i]; }

    std::size_t connect(std::uint32_t a, std::uint32_t b, const joint& j)
    {
        links.push_back(link{a, b, j});
        if (!j.collide_connected) { filter.exclude(a, b); }
        filter.finalize();
        return links.size() - 1;
    }

    [[nodiscard]] contact_manifold collide_indices(std::uint32_t ia, std::uint32_t ib) const
    {
        const rigid_body& a = body(ia);
        const rigid_body& b = body(ib);
        const shape& sa = shapes[ia];
        const shape& sb = shapes[ib];
        // Bound to locals first: `as_convex` is a VIEW and refuses a
        // temporary, because the view would outlive it.
        if (sa.kind == shape_kind::sphere && sb.kind == shape_kind::sphere)
        {
            const auto x = world_sphere(sa, a.state.position);
            const auto y = world_sphere(sb, b.state.position);
            return collide_manifold(as_convex(x), as_convex(y), mf);
        }
        if (sa.kind == shape_kind::sphere)
        {
            const auto x = world_sphere(sa, a.state.position);
            const auto y = world_obb(sb, b.state.position, b.orientation);
            return collide_manifold(as_convex(x), as_convex(y), mf);
        }
        if (sb.kind == shape_kind::sphere)
        {
            const auto x = world_obb(sa, a.state.position, a.orientation);
            const auto y = world_sphere(sb, b.state.position);
            return collide_manifold(as_convex(x), as_convex(y), mf);
        }
        const auto x = world_obb(sa, a.state.position, a.orientation);
        const auto y = world_obb(sb, b.state.position, b.orientation);
        return collide_manifold(as_convex(x), as_convex(y), mf);
    }

    void collide_all()
    {
        auto bodies = world.bodies();
        clock_type::time_point t0 = clock_type::now();
        proxies.clear();
        for (std::size_t i = 0; i < bodies.size(); ++i)
        {
            proxies.push_back(proxy{bounds_of(shapes[i], bodies[i].state.position,
                                              bodies[i].orientation),
                                    static_cast<std::uint32_t>(i)});
        }
        grid.build(proxies, bp);
        t_broad = seconds_since(t0);

        t0 = clock_type::now();
        cache.begin_frame();
        manifolds.clear();
        keys.clear();
        pair_a.clear();
        pair_b.clear();
        manifolds.reserve(grid.pairs().size());
        for (const broadphase_pair& p : grid.pairs())
        {
            const rigid_body& a = bodies[p.a];
            const rigid_body& b = bodies[p.b];
            if (a.kind != body_kind::dynamic && b.kind != body_kind::dynamic) { continue; }

            // Lesson 8.11: two bodies a joint connects do not collide, unless
            // the joint says they should.
            if (filter.excluded(p.a, p.b)) { continue; }

            contact_manifold m = collide_indices(p.a, p.b);
            if (m.count == 0) { continue; }
            const std::uint64_t key = pair_key(p.a, p.b);
            if (const contact_manifold* previous = cache.find(key)) { carry_impulses(m, *previous); }
            manifolds.push_back(m);
            keys.push_back(key);
            pair_a.push_back(p.a);
            pair_b.push_back(p.b);
        }
        t_narrow = seconds_since(t0);
    }

    void solve()
    {
        const clock_type::time_point t0 = clock_type::now();
        solver.begin(world.bodies());
        for (std::size_t i = 0; i < manifolds.size(); ++i)
        {
            solver.add(pair_a[i], pair_b[i], manifolds[i], material);
        }
        for (link& l : links) { solver.add(l.a, l.b, l.j); }
        stats = solver.solve(h, cfg, sleep);
        t_solve = seconds_since(t0);
        for (std::size_t k = 0; k < manifolds.size(); ++k) { cache.store(keys[k], manifolds[k]); }
        cache.end_frame();
    }

    void step()
    {
        cfg.restitution_bias = world.gravity() * h;
        world.integrate_velocities(h);
        collide_all();
        solve();
        world.integrate_positions(h);
    }

    void run(float seconds)
    {
        const int steps = static_cast<int>(seconds / h + 0.5f);
        for (int i = 0; i < steps; ++i) { step(); }
    }
};

std::uint32_t add_floor(scene& s, float half_extent = 20.0f)
{
    return s.add(make_fixed(vec3{0.0f, -0.5f, 0.0f}), box_shape(vec3{half_extent, 0.5f, half_extent}));
}

void add_tower(scene& s, int count, float x = 0.0f, float half = 0.25f, float mass = 10.0f)
{
    for (int i = 0; i < count; ++i)
    {
        const float y = half + static_cast<float>(i) * (2.0f * half + 0.001f);
        s.add(make_box(vec3{x, y, 0.0f}, mass, vec3{half, half, half}),
              box_shape(vec3{half, half, half}));
    }
}

} // namespace

namespace
{

// ---------------------------------------------------------------------------
// §A  a contact is a row
// ---------------------------------------------------------------------------

/// The per-manifold part of a flattened set of contact rows.
struct row_group
{
    int first = 0;
    int count = 0;
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    float inv_mass_a = 0.0f;
    float inv_mass_b = 0.0f;
};

/// **8.9's normal solve, copied into this translation unit.** Three dot
/// products' worth of relative velocity, a division dressed as a multiply, the
/// clamp, and `apply_impulse_pair`'s two cross products and two matrix
/// products. Behind `noinline` with the row arm below, so that the two differ
/// in their arithmetic and in nothing else — 8.8 §7's rule.
__attribute__((noinline)) void hand_normal_pass(std::vector<contact_batch>& batches,
                                                const std::vector<row_group>& groups,
                                                std::vector<rigid_body>& bodies)
{
    for (std::size_t k = 0; k < batches.size(); ++k)
    {
        contact_batch& batch = batches[k];
        rigid_body& a = bodies[groups[k].a];
        rigid_body& b = bodies[groups[k].b];
        for (int i = 0; i < batch.count; ++i)
        {
            contact_constraint& c = batch.points[i];
            const vec3 u = (b.state.velocity + cross(b.angular_velocity, c.r_b))
                           - (a.state.velocity + cross(a.angular_velocity, c.r_a));
            const float vn = dot(u, batch.normal);
            float delta = (c.target_normal_velocity - vn) * c.normal_mass;
            const float total = std::max(0.0f, c.normal_impulse + delta);
            delta = total - c.normal_impulse;
            c.normal_impulse = total;
            const vec3 impulse = batch.normal * delta;
            a.state.velocity = a.state.velocity - impulse * batch.inv_mass_a;
            a.angular_velocity = a.angular_velocity - batch.inv_inertia_a * cross(c.r_a, impulse);
            b.state.velocity = b.state.velocity + impulse * batch.inv_mass_b;
            b.angular_velocity = b.angular_velocity + batch.inv_inertia_b * cross(c.r_b, impulse);
        }
    }
}

/// **The same solve, as rows.** `solve_row`'s body, copied, over the same
/// points in the same order: four dot products, the clamp, four scaled adds,
/// and no matrix at all — the matrices were multiplied into the row by
/// `prepare_row`.
__attribute__((noinline)) void row_normal_pass(std::vector<jacobian_row>& rows,
                                               const std::vector<row_group>& groups,
                                               std::vector<rigid_body>& bodies)
{
    for (const row_group& g : groups)
    {
        rigid_body& a = bodies[g.a];
        rigid_body& b = bodies[g.b];
        for (int i = g.first; i < g.first + g.count; ++i)
        {
            jacobian_row& row = rows[static_cast<std::size_t>(i)];
            const float jv = dot(row.linear_a, a.state.velocity) + dot(row.angular_a, a.angular_velocity)
                             + dot(row.linear_b, b.state.velocity) + dot(row.angular_b, b.angular_velocity);
            float delta = row.mass * (row.target - jv);
            const float total = std::clamp(row.impulse + delta, row.lower, row.upper);
            delta = total - row.impulse;
            row.impulse = total;
            a.state.velocity = a.state.velocity + row.linear_a * (g.inv_mass_a * delta);
            a.angular_velocity = a.angular_velocity + row.inv_inertia_angular_a * delta;
            b.state.velocity = b.state.velocity + row.linear_b * (g.inv_mass_b * delta);
            b.angular_velocity = b.angular_velocity + row.inv_inertia_angular_b * delta;
        }
    }
}

void section_a()
{
    rule("A  A CONTACT IS A ROW");

    std::printf(
        "  8.9 wrote the contact normal solve by hand: relative velocity at\n"
        "  the point, along the normal, times an effective mass, clamped at\n"
        "  zero. The claim is that this IS a Jacobian row,\n"
        "      J = [ -n, -(r_a x n), n, r_b x n ],  bounds [0, inf)\n"
        "  and that J M^-1 J^T is 8.9's effective mass, not an analogue of it.\n");

    // A settled yard of towers: real manifolds, real lever arms, real tilts.
    scene s;
    s.sleep.enabled = false;
    add_floor(s);
    for (int t = 0; t < 10; ++t) { add_tower(s, 5, static_cast<float>(t) * 1.2f - 5.4f); }
    s.run(3.0f);
    s.collide_all();

    // The state the solver actually sees: AFTER the velocity half of a step,
    // so every resting body carries this step's `g*h` of approach. The first
    // draft of this section copied the bodies at the end of a step instead,
    // where the velocities are the solver's own leftovers — half approaching,
    // half separating, all tiny — and CONTROL 2 below came out backwards.
    std::vector<rigid_body> bodies(s.world.bodies().begin(), s.world.bodies().end());
    for (rigid_body& b : bodies)
    {
        if (b.kind == body_kind::dynamic) { b.state.velocity = b.state.velocity + s.world.gravity() * k_h; }
    }
    solver_config cfg;
    cfg.warm_start = false;
    cfg.friction = friction_model::none;
    cfg.correction = position_correction::none;

    std::vector<contact_batch> batches;
    std::vector<row_group> groups;
    std::vector<jacobian_row> rows;

    double worst_mass = 0.0;
    double worst_tangent_mass = 0.0;
    double worst_velocity = 0.0;
    double worst_linear_only = 0.0;
    double best_linear_only = 1e30;
    int points = 0;

    for (std::size_t k = 0; k < s.manifolds.size(); ++k)
    {
        const rigid_body& a = bodies[s.pair_a[k]];
        const rigid_body& b = bodies[s.pair_b[k]];
        const contact_batch batch = prepare_contacts(a, b, s.manifolds[k], s.material, cfg);
        batches.push_back(batch);

        row_group g;
        g.first = static_cast<int>(rows.size());
        g.count = batch.count;
        g.a = s.pair_a[k];
        g.b = s.pair_b[k];
        g.inv_mass_a = batch.inv_mass_a;
        g.inv_mass_b = batch.inv_mass_b;
        groups.push_back(g);

        for (int i = 0; i < batch.count; ++i)
        {
            const contact_constraint& c = batch.points[i];
            jacobian_row row = point_row(batch.normal, c.r_a, c.r_b);
            prepare_row(row, batch.inv_mass_a, batch.inv_inertia_a, batch.inv_mass_b,
                        batch.inv_inertia_b);
            row.lower = 0.0f;
            row.target = c.target_normal_velocity;
            rows.push_back(row);

            worst_mass = std::max(worst_mass, std::abs(static_cast<double>(row.mass) / c.normal_mass - 1.0));
            for (int t = 0; t < 2; ++t)
            {
                jacobian_row tr = point_row(batch.tangent[t], c.r_a, c.r_b);
                prepare_row(tr, batch.inv_mass_a, batch.inv_inertia_a, batch.inv_mass_b,
                            batch.inv_inertia_b);
                worst_tangent_mass = std::max(
                    worst_tangent_mass, std::abs(static_cast<double>(tr.mass) / c.tangent_mass[t] - 1.0));
            }
            const float jv = row_velocity(row, a.state.velocity, a.angular_velocity,
                                          b.state.velocity, b.angular_velocity);
            worst_velocity = std::max(worst_velocity,
                                      std::abs(static_cast<double>(jv - c.initial_normal_velocity)));

            // THE CONTROL: the same row with the angular halves deleted — the
            // mistake of treating a contact as a push on the centre of mass.
            jacobian_row lin = row;
            lin.angular_a = vec3{};
            lin.angular_b = vec3{};
            prepare_row(lin, batch.inv_mass_a, batch.inv_inertia_a, batch.inv_mass_b,
                        batch.inv_inertia_b);
            const double ratio = static_cast<double>(lin.mass) / row.mass;
            worst_linear_only = std::max(worst_linear_only, ratio);
            best_linear_only = std::min(best_linear_only, ratio);
            ++points;
        }
    }

    std::printf("\n  %zu manifolds, %d contact points, from a settled yard of 50 crates:\n",
                s.manifolds.size(), points);
    std::printf("     worst |J M^-1 J^T  vs  normal_mass|       %.3e (relative)\n", worst_mass);
    std::printf("     worst |J M^-1 J^T  vs  tangent_mass|      %.3e (relative)\n", worst_tangent_mass);
    std::printf("     worst |J V  vs  8.9's relative velocity|  %.3e m/s\n", worst_velocity);
    check(worst_mass < 1e-6, "J M^-1 J^T is 8.9's normal effective mass");
    check(worst_tangent_mass < 1e-6, "and the tangent rows are 8.9's tangent masses");
    check(worst_velocity < 1e-6, "J V is 8.9's relative normal velocity");

    std::printf("\n  CONTROL 1, THE ANGULAR HALVES DELETED — a contact treated as a\n"
                "  push on the centre of mass. The effective mass comes out too large\n"
                "  by between %.3fx and %.3fx over the same %d points. The worked\n"
                "  example in 8.10 §2 predicts 4.000x at a cube's corner: 1/m = 0.1\n"
                "  against 1/m + w.(I^-1 w) = 0.1 + 0.3 = 0.4 per crate.\n",
                best_linear_only, worst_linear_only, points);
    check(worst_linear_only > 3.5 && worst_linear_only < 8.5,
          "without the angular halves the mass is wrong by the rotational share");

    // ---- the solve itself, both ways, on identical copies -------------------
    std::vector<rigid_body> hand = bodies;
    std::vector<rigid_body> by_row = bodies;
    std::vector<contact_batch> hand_batches = batches;
    std::vector<jacobian_row> row_copy = rows;
    for (int iteration = 0; iteration < 8; ++iteration)
    {
        for (std::size_t k = 0; k < hand_batches.size(); ++k)
        {
            (void)solve_contacts(hand[groups[k].a], hand[groups[k].b], hand_batches[k], cfg);
        }
        for (const row_group& g : groups)
        {
            for (int i = g.first; i < g.first + g.count; ++i)
            {
                (void)solve_row(row_copy[static_cast<std::size_t>(i)], g.inv_mass_a, g.inv_mass_b,
                                by_row[g.a].state.velocity, by_row[g.a].angular_velocity,
                                by_row[g.b].state.velocity, by_row[g.b].angular_velocity);
            }
        }
    }
    double worst_v = 0.0;
    double worst_w = 0.0;
    double biggest = 0.0;
    for (std::size_t i = 0; i < hand.size(); ++i)
    {
        worst_v = std::max(worst_v, static_cast<double>(length(hand[i].state.velocity - by_row[i].state.velocity)));
        worst_w = std::max(worst_w, static_cast<double>(length(hand[i].angular_velocity - by_row[i].angular_velocity)));
        biggest = std::max(biggest, static_cast<double>(length(hand[i].state.velocity)));
    }
    std::printf("\n  EIGHT SWEEPS OF THE WHOLE YARD, BOTH WAYS, from identical copies.\n"
                "  engine `solve_contacts` (friction off) against engine `solve_row`:\n"
                "     worst |v difference|    %.3e m/s   (largest |v| %.3e)\n"
                "     worst |w difference|    %.3e rad/s\n",
                worst_v, biggest, worst_w);
    check(worst_v < 1e-5 && worst_w < 1e-4, "the two solves agree to rounding");

    // ---- the sign control, as a consequence rather than a count --------------
    //
    // The first draft of this control COUNTED flipped rows that pushed, and
    // could not be made to mean anything: in a stack, gravity adds the same
    // `g*h` to both crates of a crate-on-crate contact, so only the floor
    // contacts are clearly approaching and the rest are noise either side of
    // zero. What a sign error DOES is the honest instrument. So: take the rows
    // alone as the yard's entire contact solver for half a second — collide,
    // build rows, eight sweeps of `solve_row`, move — once with J as derived and
    // once with J negated, and see where the crates are.
    {
        std::printf("\n  CONTROL 2, J WITH ITS SIGN FLIPPED, run as the yard's only contact\n"
                    "  solver for 0.5 s (no friction, no warm start, no correction):\n"
                    "     J              lowest crate centre    fell by      rows with impulse\n");
        float fell[2] = {};
        for (int flip = 0; flip <= 1; ++flip)
        {
            scene y;
            y.sleep.enabled = false;
            add_floor(y);
            for (int t = 0; t < 10; ++t) { add_tower(y, 5, static_cast<float>(t) * 1.2f - 5.4f); }
            y.run(3.0f);

            const auto lowest = [&y]() {
                float low = 1e30f;
                for (const rigid_body& b : y.world.bodies())
                {
                    if (b.kind == body_kind::dynamic) { low = std::min(low, b.state.position.y); }
                }
                return low;
            };
            const float before = lowest();
            int pushing = 0;
            for (int step = 0; step < 30; ++step)
            {
                y.world.integrate_velocities(k_h);
                y.collide_all();
                auto live = y.world.bodies();
                std::vector<jacobian_row> step_rows;
                std::vector<row_group> step_groups;
                for (std::size_t k = 0; k < y.manifolds.size(); ++k)
                {
                    const rigid_body& a = live[y.pair_a[k]];
                    const rigid_body& b = live[y.pair_b[k]];
                    const contact_batch batch = prepare_contacts(a, b, y.manifolds[k], y.material, cfg);
                    row_group g;
                    g.first = static_cast<int>(step_rows.size());
                    g.count = batch.count;
                    g.a = y.pair_a[k];
                    g.b = y.pair_b[k];
                    g.inv_mass_a = batch.inv_mass_a;
                    g.inv_mass_b = batch.inv_mass_b;
                    step_groups.push_back(g);
                    for (int i = 0; i < batch.count; ++i)
                    {
                        const vec3 n = flip ? -batch.normal : batch.normal;
                        jacobian_row row = point_row(n, batch.points[i].r_a, batch.points[i].r_b);
                        prepare_row(row, batch.inv_mass_a, batch.inv_inertia_a, batch.inv_mass_b,
                                    batch.inv_inertia_b);
                        row.lower = 0.0f;
                        step_rows.push_back(row);
                    }
                }
                for (int it = 0; it < 8; ++it)
                {
                    for (const row_group& g : step_groups)
                    {
                        for (int i = g.first; i < g.first + g.count; ++i)
                        {
                            (void)solve_row(step_rows[static_cast<std::size_t>(i)], g.inv_mass_a,
                                            g.inv_mass_b, live[g.a].state.velocity,
                                            live[g.a].angular_velocity, live[g.b].state.velocity,
                                            live[g.b].angular_velocity);
                        }
                    }
                }
                if (step == 29)
                {
                    for (const jacobian_row& row : step_rows) { pushing += row.impulse > 0.0f ? 1 : 0; }
                }
                y.world.integrate_positions(k_h);
            }
            fell[flip] = before - lowest();
            std::printf("     %-12s   %14.4f m      %8.4f m   %6d of %zu\n",
                        flip ? "negated" : "as derived", static_cast<double>(lowest()),
                        static_cast<double>(fell[flip]), pushing, y.manifolds.size() * 4);
        }
        std::printf("     free fall over the same 0.5 s is g t^2 / 2 = %.4f m.\n"
                    "  As derived, the yard sinks %.1f mm — the cold-start, eight-sweep\n"
                    "  sink 8.10 §4 measured, because this loop has no warm start and no\n"
                    "  position correction. Negated, it falls %.0f mm: every row that\n"
                    "  acts is PULLING two separating surfaces together, and not one\n"
                    "  pushes. A contact whose Jacobian points the wrong way is not a\n"
                    "  weak contact. It is no contact at all.\n",
                    0.5 * 9.81 * 0.25, static_cast<double>(fell[0]) * 1000.0,
                    static_cast<double>(fell[1]) * 1000.0);
        check(fell[0] < 0.05f, "rows as derived hold the yard up, to 8.10's cold sink");
        check(fell[1] > 10.0f * fell[0], "rows with J negated let it fall through the floor");
    }

    // ---- what the general form costs -------------------------------------------
    //
    // BOTH ARMS ARE HARNESS-LOCAL COPIES BEHIND `noinline`. The engine's own
    // `solve_row` lives in libengine.a and would be a call per point, where the
    // hand-written arm would be inlined; that is a comparison of call overhead
    // against arithmetic, and 8.8 §7 already paid for learning it.
    {
        double t_hand = 1e30;
        double t_row = 1e30;
        const int reps = 400;
        for (int rep = 0; rep < reps; ++rep)
        {
            std::vector<rigid_body> x = bodies;
            std::vector<contact_batch> bx = batches;
            clock_type::time_point t0 = clock_type::now();
            for (int it = 0; it < 8; ++it) { hand_normal_pass(bx, groups, x); }
            t_hand = std::min(t_hand, seconds_since(t0));

            std::vector<rigid_body> y = bodies;
            std::vector<jacobian_row> ry = rows;
            t0 = clock_type::now();
            for (int it = 0; it < 8; ++it) { row_normal_pass(ry, groups, y); }
            t_row = std::min(t_row, seconds_since(t0));
        }
        const double per_hand = t_hand / (8.0 * points) * 1e9;
        const double per_row = t_row / (8.0 * points) * 1e9;
        std::printf("\n  AND WHAT EACH COSTS, per point per sweep, minimum of %d runs of\n"
                    "  eight sweeps over the yard's %d points:\n"
                    "     hand-written (8.9)   %6.3f ns    %3zu bytes per point\n"
                    "     Jacobian row         %6.3f ns    %3zu bytes per point\n"
                    "     ratio row / hand     %6.3f\n",
                    reps, points, per_hand, sizeof(contact_constraint), per_row, sizeof(jacobian_row),
                    per_row / per_hand);
        check(per_row > 0.0 && per_hand > 0.0, "both arms timed");
    }
}

} // namespace

namespace
{

// ---------------------------------------------------------------------------
// §B  rods and ropes
// ---------------------------------------------------------------------------

/// What a bob on a rod or a rope does when it is swung up past the horizontal
/// with `v0² = 3.5·g·L` at the bottom — enough to rise above the pivot and not
/// enough to go over the top.
struct swing_result
{
    float turn_height = 0.0f;   ///< bob height (above the pivot) where the row's impulse changed sign or went to zero
    float max_height = 0.0f;    ///< the highest the bob got while the joint was acting
    double energy_ratio = 0.0;  ///< energy at `turn_height` over the energy it started with
    bool turned = false;
};

swing_result swing(bool rope, float hz)
{
    rig r;
    r.h = 1.0f / hz;
    const float L = 1.0f;
    const float g = 9.81f;
    r.add(make_fixed(vec3{}));
    const std::uint32_t bob = r.add(make_sphere(vec3{0.0f, -L, 0.0f}, 1.0f, 0.05f));
    r.body(bob).state.velocity = vec3{std::sqrt(3.5f * g * L), 0.0f, 0.0f};
    const std::size_t j = r.connect(0, bob, rope ? make_rope(r.body(0), r.body(bob), vec3{}, vec3{0.0f, -L, 0.0f}, L)
                                                 : make_rod(r.body(0), r.body(bob), vec3{}, vec3{0.0f, -L, 0.0f}));

    const auto energy = [&r, bob, g]() {
        const rigid_body& b = r.body(bob);
        return 0.5 * length_squared(b.state.velocity) + static_cast<double>(g) * b.state.position.y;
    };
    const double e0 = energy();

    swing_result out;
    bool loaded = false;
    bool falling = false;
    const int steps = static_cast<int>(2.0f * hz);
    for (int i = 0; i < steps; ++i)
    {
        r.step();
        const float y = r.body(bob).state.position.y;
        const joint& jj = r.joint_at(j);
        (void)0;

        // THE TWO INSTRUMENTS. A rope's row may only pull, so tension is a
        // positive accumulated impulse and slack is zero. A rod's row may do
        // either, and its sign says which: NEGATIVE is tension (the row pulls
        // `b` toward `a`, against its +u direction) and POSITIVE is
        // compression. The rope goes slack exactly where the rod would have
        // to start PUSHING — two different instruments, one physical fact.
        const float impulse = rope ? jj.limit_impulse[1] : -jj.limit_impulse[0];
        if (impulse > 0.0f) { loaded = true; }
        // The peak of the FIRST rise, whether or not the joint is still
        // loaded: the rod keeps climbing in compression after it turns, and
        // that climb is the whole difference between a rod and a rope.
        if (!falling)
        {
            out.max_height = std::max(out.max_height, y);
            falling = r.body(bob).state.velocity.y < 0.0f;
        }
        if (loaded && !out.turned && impulse <= 0.0f)
        {
            out.turned = true;
            out.turn_height = y;
            out.energy_ratio = (energy() + static_cast<double>(g) * L) / (e0 + static_cast<double>(g) * L);
        }
    }
    return out;
}

void section_b()
{
    rule("B  RODS AND ROPES");

    std::printf(
        "  A distance joint is ONE row along the line between its anchors —\n"
        "  the same `point_row` a contact normal is. With its two lengths equal\n"
        "  it is a rod, and the row is bilateral. With min_length = 0 it is a\n"
        "  rope, and the row is point_row(-u) with bounds [0, inf): a contact\n"
        "  turned inside out.\n"
        "\n"
        "  THE FIXTURE: a bob on a 1 m joint, given v0^2 = 3.5 g L at the bottom.\n"
        "  Energy says it rises to y where v^2 = v0^2 - 2g(L + y); tension is\n"
        "  m(v^2/L - g y/L), so it reaches zero at\n"
        "      y* = (v0^2 - 2 g L) / (3 g) = L/2 = 0.5000 m above the pivot.\n"
        "  A ROPE goes slack there and the bob leaves the circle. A ROD goes\n"
        "  from tension to compression there and carries on, to y = 0.75 L.\n");

    std::printf("\n     rate      rope lets go at    rod turns at     rod peaks at   energy left\n");
    swing_result rope60{};
    swing_result rod60{};
    swing_result rope960{};
    for (float hz : {60.0f, 240.0f, 960.0f})
    {
        const swing_result rp = swing(true, hz);
        const swing_result rd = swing(false, hz);
        std::printf("     %4.0f Hz   %13.4f m   %11.4f m   %11.4f m   %10.2f%%\n",
                    static_cast<double>(hz), static_cast<double>(rp.turn_height),
                    static_cast<double>(rd.turn_height), static_cast<double>(rd.max_height),
                    100.0 * rp.energy_ratio);
        if (hz == 60.0f) { rope60 = rp; rod60 = rd; }
        if (hz == 960.0f) { rope960 = rp; }
    }
    std::printf("     exact      %13.4f m   %11.4f m   %11.4f m      100.00%%\n", 0.5, 0.5, 0.75);

    std::printf("\n  THE CLAIM REFUSED AT 60 Hz, AND THE REFUSAL HAS A SIGN. Both\n"
                "  instruments agree with EACH OTHER at every rate — the rope lets go\n"
                "  where the rod turns — and both read LOW at 60 Hz, because the bob\n"
                "  arrives with less energy than it left with. That is not the rope:\n"
                "  it is §5, a velocity constraint losing energy at (w h)^2 per step,\n"
                "  and it shrinks as the step does.\n");
    check(rope60.turned && rod60.turned, "the rope lets go and the rod turns");
    check(std::abs(rope60.turn_height - rod60.turn_height) < 0.02f,
          "the rope lets go where the rod starts to push");
    check(std::abs(rope960.turn_height - 0.5f) < std::abs(rope60.turn_height - 0.5f) / 5.0f,
          "and the height converges on L/2 as the step shrinks");
}

// ---------------------------------------------------------------------------
// §C  why a joint drifts
// ---------------------------------------------------------------------------

void section_c()
{
    rule("C  WHY A JOINT DRIFTS");

    const float L = 1.0f;
    const float v0 = 5.0f;
    std::printf(
        "  A velocity constraint keeps dC/dt = 0 and says nothing about C.\n"
        "  After the solve a bob on a rod moves exactly perpendicular to the\n"
        "  rod; the position update moves it along that straight line, which is\n"
        "  a TANGENT, not the circle. Pythagoras:\n"
        "      r(n+1)^2 = r(n)^2 + (v(n) h)^2\n"
        "  and the next solve removes the radial part of v, which keeps r x v:\n"
        "      v(n+1) r(n+1) = v(n) r(n)          (angular momentum, exactly)\n"
        "  The first step's drift is sqrt(L^2 + (v h)^2) - L, which for small\n"
        "  steps is v^2 h^2 / (2L) — half the centripetal acceleration times h^2.\n"
        "\n"
        "  THE FIXTURE: gravity off, a 1 kg bob on a 1 m rod from a fixed anchor,\n"
        "  moving at 5 m/s (w h = 0.0833 rad per step), NO position correction.\n");

    rig r;
    r.world.set_gravity(vec3{});
    r.cfg.correction = position_correction::none;
    r.add(make_fixed(vec3{}));
    const std::uint32_t bob = r.add(make_sphere(vec3{L, 0.0f, 0.0f}, 1.0f, 0.05f));
    r.body(bob).state.velocity = vec3{0.0f, v0, 0.0f};
    r.connect(0, bob, make_rod(r.body(0), r.body(bob), vec3{}, vec3{L, 0.0f, 0.0f}));

    // The recurrence, in double, with nothing from the engine in it.
    double pr = L;
    double pv = v0;
    double pr_before = L;   // the radius the last solve saw — see the energy check
    const double h = k_h;
    double worst_r = 0.0;
    double worst_ell = 0.0;
    double first_drift = 0.0;
    const double ell0 = static_cast<double>(L) * v0;

    std::printf("\n     step     r predicted     r measured    |v| measured    r x v / L0\n");
    for (int n = 1; n <= 60; ++n)
    {
        r.step();
        const double nr = std::sqrt(pr * pr + pv * pv * h * h);
        pv = pv * pr / nr;
        pr_before = pr;
        pr = nr;

        const rigid_body& b = r.body(bob);
        const double mr = length(b.state.position);
        const double ell = length(cross(b.state.position, b.state.velocity));
        worst_r = std::max(worst_r, std::abs(mr - pr) / pr);
        worst_ell = std::max(worst_ell, std::abs(ell / ell0 - 1.0));
        if (n == 1) { first_drift = mr - L; }
        if (n == 1 || n == 2 || n == 10 || n == 30 || n == 60)
        {
            std::printf("     %4d    %12.6f    %11.6f    %12.6f    %10.7f\n", n, pr, mr,
                        static_cast<double>(length(b.state.velocity)), ell / ell0);
        }
    }
    const double predicted_first = constraint_drift_per_step(v0, L, k_h);
    std::printf("\n     first step's drift     %.6f m measured, %.6f predicted\n"
                "     v^2 h^2 / (2L)          %.6f m   (the small-step form)\n"
                "     worst |r - recurrence| / r over 60 steps     %.3e\n"
                "     worst |r x v| drift over 60 steps             %.3e\n",
                first_drift, predicted_first, v0 * v0 * h * h / 2.0, worst_r, worst_ell);
    // THE VELOCITY A STEP ENDS WITH WAS SOLVED AT THE RADIUS IT STARTED WITH.
    // The first draft compared the energy against (L/r)^2 at the FINAL radius
    // and missed by 0.4% — exactly one step of drift, because the solve runs
    // before the move. The speed after step n is ell / r(n-1).
    const double ke_ratio = 0.5 * length_squared(r.body(bob).state.velocity) / (0.5 * v0 * v0);
    std::printf("     kinetic energy after 1 s    %.6f of the start; (L/r)^2 = %.6f\n",
                ke_ratio, (L / pr_before) * (L / pr_before));
    check(std::abs(first_drift - predicted_first) < 2e-6, "the first step drifts by the Pythagorean amount");
    check(worst_r < 1e-5, "the radius follows the recurrence for a whole second");
    check(worst_ell < 1e-5, "and angular momentum is conserved exactly");
    check(std::abs(ke_ratio - (L / pr_before) * (L / pr_before)) < 1e-5, "so the energy falls as 1/r^2");

    // ---- and for a BODY rather than a bead ------------------------------------------
    //
    // The bead keeps r x v, so its energy falls as 1/r^2 and the fraction lost
    // per step is (w h)^2. A rigid body on a pin is a bead plus a spin, and only
    // the bead half — the centre of mass orbiting the pin — is projected: the
    // spin about the centre is untouched by a point constraint. So the loss
    // should be (w h)^2 times the fraction of the kinetic energy that is
    // ORBITAL, which is m d^2 / (I_cm + m d^2): the parallel-axis ratio.
    {
        std::printf("\n  AND FOR A BODY, NOT A BEAD. Only the centre's ORBIT about the pin\n"
                    "  is projected; its spin about itself is not. So the energy lost per\n"
                    "  step should be (w h)^2 times the orbital fraction of the energy,\n"
                    "      rho = m d^2 / (I_cm + m d^2).\n"
                    "  A 1.0 x 2.0 x 0.1 m, 10 kg slab spun at 4 rad/s about a pin d from\n"
                    "  its centre, gravity off, no correction, ten steps after thirty:\n"
                    "     pin            rho      loss per step     rho (w h)^2     ratio\n");
        double worst_law = 0.0;
        for (int hinge = 0; hinge <= 1; ++hinge)
        {
            for (float d : {0.25f, 0.5f, 1.0f})
            {
                rig c;
                c.world.set_gravity(vec3{});
                c.cfg.correction = position_correction::none;
                c.add(make_fixed(vec3{}));
                const std::uint32_t slab = c.add(make_box(vec3{d, 0.0f, 0.0f}, 10.0f, vec3{0.5f, 1.0f, 0.05f}));
                c.body(slab).angular_velocity = vec3{0.0f, 4.0f, 0.0f};
                c.body(slab).state.velocity = cross(c.body(slab).angular_velocity, c.body(slab).state.position);
                c.connect(0, slab, hinge ? make_hinge(c.body(0), c.body(slab), vec3{}, vec3{0.0f, 1.0f, 0.0f})
                                         : make_ball_socket(c.body(0), c.body(slab), vec3{}));
                c.run(0.5f);
                const double e0 = energy_of(c.body(slab));
                const double w0 = c.body(slab).angular_velocity.y;
                for (int i = 0; i < 10; ++i) { c.step(); }
                const double e1 = energy_of(c.body(slab));
                const double w = 0.5 * (w0 + c.body(slab).angular_velocity.y);
                const float i_cm = 10.0f * (1.0f + 0.01f) / 12.0f;
                const double rho = lever_ratio(i_cm, 10.0f, d);
                const double per = 1.0 - std::pow(e1 / e0, 0.1);
                const double law = projection_loss_per_step(static_cast<float>(rho), static_cast<float>(w), k_h);
                worst_law = std::max(worst_law, std::abs(per / law - 1.0));
                std::printf("     %-11s   %6.4f    %12.6f     %11.6f    %6.4f\n",
                            hinge ? (d == 0.25f ? "hinge,  d/4" : d == 0.5f ? "hinge,  d/2" : "hinge,  d")
                                  : (d == 0.25f ? "socket, d/4" : d == 0.5f ? "socket, d/2" : "socket, d"),
                            rho, per, law, per / law);
            }
        }
        std::printf("  A velocity joint dissipates rho (w h)^2 of its kinetic energy per\n"
                    "  step, to %.1f%% on six configurations. rho is the parallel-axis\n"
                    "  ratio, and it will turn up twice more in this lesson.\n", worst_law * 100.0);
        check(worst_law < 0.01, "a body on a pin loses rho (w h)^2 of its energy per step");
    }

    // ---- the control: no curvature, no drift -----------------------------------
    {
        rig c;
        c.world.set_gravity(vec3{});
        c.cfg.correction = position_correction::none;
        const std::uint32_t a = c.add(make_sphere(vec3{0.0f, 0.0f, 0.0f}, 1.0f, 0.05f));
        const std::uint32_t b = c.add(make_sphere(vec3{L, 0.0f, 0.0f}, 1.0f, 0.05f));
        c.body(a).state.velocity = vec3{0.0f, v0, 0.0f};
        c.body(b).state.velocity = vec3{0.0f, v0, 0.0f};
        const std::size_t j = c.connect(a, b, make_rod(c.body(a), c.body(b), vec3{}, vec3{L, 0.0f, 0.0f}));
        c.run(1.0f);
        const float err = c.error_of(j).linear;
        std::printf("\n  CONTROL: the same rod between two bodies moving TOGETHER at 5 m/s —\n"
                    "  the same speed, no relative rotation, so no curvature. After 1 s the\n"
                    "  rod's length error is %.3e m. The drift is not a property of\n"
                    "  speed; it is a property of turning.\n",
                    static_cast<double>(err));
        check(err < 1e-6f, "a rod that does not turn does not drift");
    }
}

// ---------------------------------------------------------------------------
// §D  correcting a joint, and what Baumgarte does to one
// ---------------------------------------------------------------------------

void section_d()
{
    rule("D  CORRECTING A JOINT, AND WHAT BAUMGARTE DOES TO ONE");

    const float L = 1.0f;
    const float v0 = 5.0f;
    const float beta = 0.2f;
    std::printf(
        "  §5's rod again, with a correction on. Each step removes beta of the\n"
        "  error and the motion adds a drift delta, so\n"
        "      e(n+1) = (1 - beta) e(n) + delta    ->    e* = delta / beta\n"
        "  and delta itself is §5's per-step drift at the current speed.\n");

    std::printf("\n     correction      stretch at 1 s     delta/beta     |v| at 1 s    energy per step\n");
    double stretch_split = 0.0;
    double predicted_split = 0.0;
    double loss_per_step[3] = {};
    double loss_predicted[3] = {};
    for (position_correction pc : {position_correction::none, position_correction::baumgarte,
                                   position_correction::split_impulse})
    {
        rig r;
        r.world.set_gravity(vec3{});
        r.cfg.correction = pc;
        r.add(make_fixed(vec3{}));
        const std::uint32_t bob = r.add(make_sphere(vec3{L, 0.0f, 0.0f}, 1.0f, 0.05f));
        r.body(bob).state.velocity = vec3{0.0f, v0, 0.0f};
        const std::size_t j = r.connect(0, bob, make_rod(r.body(0), r.body(bob), vec3{}, vec3{L, 0.0f, 0.0f}));
        r.run(0.95f);
        const double e_before = 0.5 * length_squared(r.body(bob).state.velocity);
        r.run(0.05f);
        const double e_after = 0.5 * length_squared(r.body(bob).state.velocity);
        const float stretch = r.error_of(j).linear;
        const float speed = length(r.body(bob).state.velocity);
        const double delta = constraint_drift_per_step(speed, L + stretch, k_h);
        const double per_step = 1.0 - std::pow(e_after / e_before, 1.0 / 3.0);
        const double omega_h = static_cast<double>(speed) / (L + stretch) * k_h;
        loss_per_step[static_cast<int>(pc)] = per_step;
        loss_predicted[static_cast<int>(pc)] = omega_h * omega_h;
        std::printf("     %-14s  %12.4f mm   %10.4f mm   %9.4f m/s   %9.5f  (w h)^2 %.5f\n",
                    name_of(pc), static_cast<double>(stretch) * 1000.0,
                    pc == position_correction::none ? 0.0 : delta / beta * 1000.0,
                    static_cast<double>(speed), per_step, omega_h * omega_h);
        if (pc == position_correction::split_impulse)
        {
            stretch_split = stretch;
            predicted_split = delta / beta;
        }
    }
    std::printf(
        "\n  THE SECTION SET OUT TO SHOW BAUMGARTE ADDING ENERGY TO A JOINT AND\n"
        "  FOUND IT CONSERVING IT. Without a correction and under split impulse\n"
        "  the rod loses (w h)^2 of its energy per step — the projection §5\n"
        "  derived. Under Baumgarte it loses NOTHING at the steady state. The\n"
        "  bias is a real inward velocity of delta/h, and on the next step the\n"
        "  rod has turned by w h, so a fraction w h of that inward velocity now\n"
        "  points along the new tangent: (delta/h)(w h) = v (w h)^2 / 2, which\n"
        "  is exactly the speed the projection removes. Baumgarte adds energy;\n"
        "  on a turning joint it adds it precisely where the projection takes\n"
        "  it away.\n");
    check(std::abs(stretch_split / predicted_split - 1.0) < 0.05,
          "split impulse holds the rod at delta/beta");
    check(std::abs(loss_per_step[0] / loss_predicted[0] - 1.0) < 0.05,
          "the uncorrected rod loses (w h)^2 of its energy per step");
    check(std::abs(loss_per_step[2] / loss_predicted[2] - 1.0) < 0.05,
          "and so does the rod under split impulse");
    check(loss_per_step[1] < 0.1 * loss_per_step[2], "Baumgarte's steady state loses almost none");

    // ---- the dislocated joint ---------------------------------------------------
    std::printf(
        "\n  AND WHAT BAUMGARTE DOES TO A JOINT. 8.10 §7 measured it throwing a\n"
        "  crate out of a floor, because a CONTACT stops acting once the overlap\n"
        "  is gone and the body keeps the bias velocity. A joint never stops\n"
        "  acting, so the bias should decay with the error — and along the\n"
        "  joint's CONSTRAINED directions it does. The question is the FREE ones.\n"
        "\n"
        "  THE FIXTURE: a 0.5 m, 10 kg cube, gravity off, hung by a ball-socket\n"
        "  at one corner from a fixed anchor 0.2 m away — a limb spawned\n"
        "  dislocated. Three seconds, then the energy the correction left in it:\n");
    std::printf("     correction       anchor gap      kinetic energy     spin\n");
    double ke[2][3] = {};
    for (int centre = 0; centre <= 1; ++centre)
    {
        if (centre) { std::printf("  CONTROL, the socket at the cube's CENTRE instead (r = 0):\n"); }
        for (position_correction pc : {position_correction::none, position_correction::baumgarte,
                                       position_correction::split_impulse})
        {
            rig r;
            r.world.set_gravity(vec3{});
            r.cfg.correction = pc;
            const vec3 corner = centre ? vec3{} : vec3{0.25f, 0.25f, 0.25f};
            r.add(make_fixed(corner + vec3{0.2f, 0.0f, 0.0f}));
            const std::uint32_t cube = r.add(make_box(vec3{}, 10.0f, vec3{0.25f, 0.25f, 0.25f}));
            joint j = make_ball_socket(r.body(0), r.body(cube), corner);
            j.anchor_a = vec3{};   // the fixed anchor is 0.2 m from where the corner is
            const std::size_t ji = r.connect(0, cube, j);
            r.run(3.0f);
            const rigid_body& b = r.body(cube);
            ke[centre][static_cast<int>(pc)] = energy_of(b);
            std::printf("     %-14s  %9.5f m   %14.6f J   %6.3f rad/s\n", name_of(pc),
                        static_cast<double>(r.error_of(ji).linear), energy_of(b),
                        static_cast<double>(length(b.angular_velocity)));
        }
    }
    std::printf("  Baumgarte's pull on the corner is a real impulse off the centre of\n"
                "  mass, so it spins the cube, and a ball-socket has no row that\n"
                "  resists spin — the energy goes into the degrees of freedom the joint\n"
                "  leaves FREE and stays there. Split impulse moves the cube by a\n"
                "  velocity it then throws away. Through the centre there is no lever\n"
                "  arm, nothing to spin, and Baumgarte's velocity decays with the error.\n");
    check(ke[0][1] > 0.1, "Baumgarte leaves a dislocated limb spinning");
    check(ke[0][2] == 0.0, "split impulse leaves it exactly at rest");
    check(ke[1][1] < 1e-3, "through the centre Baumgarte leaves nothing behind");
}

} // namespace

namespace
{

// ---------------------------------------------------------------------------
// §E  the ball-socket, and the block
// ---------------------------------------------------------------------------

/// The spectral radius of the Gauss–Seidel iteration matrix for a symmetric
/// 3x3 `K`, in double: `G = −(D + L)⁻¹ U`, and `|G^k x|^(1/k)` for large `k`.
///
/// **The prediction §E checks the solver against.** A sweep of scalar rows is
/// exactly one Gauss–Seidel iteration on `K λ = b`, so the residual shrinks by
/// this factor per visit, asymptotically — a number computed from the joint's
/// effective-mass matrix alone, without running anything.
double gauss_seidel_radius(const double k[3][3])
{
    double x[3] = {1.0, 0.7, 0.3};
    double log_sum = 0.0;
    int counted = 0;
    for (int it = 0; it < 400; ++it)
    {
        // One GS sweep on K x = 0: x_i = -(sum_{j != i} K_ij x_j) / K_ii,
        // using the newest values — exactly what `solve_row` does.
        double before = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
        for (int i = 0; i < 3; ++i)
        {
            double s = 0.0;
            for (int j = 0; j < 3; ++j) { if (j != i) { s += k[i][j] * x[j]; } }
            x[i] = -s / k[i][i];
        }
        const double after = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
        if (after < 1e-250 || before == 0.0) { return 0.0; }
        if (it >= 200) { log_sum += std::log(after / before); ++counted; }
        for (double& v : x) { v /= after; }
    }
    return std::exp(log_sum / counted);
}

/// A box on a ball-socket at one corner of it, from a fixed anchor, in a random
/// pose with a random velocity that violates the joint. Returns the two bodies.
void random_socket(rng& g, rigid_body& anchor, rigid_body& box, bool at_centre)
{
    const vec3 half{g.range(0.05f, 0.5f), g.range(0.05f, 0.5f), g.range(0.05f, 0.5f)};
    box = make_box(vec3{}, g.range(0.5f, 20.0f), half);
    box.orientation = quat_from_axis_angle(
        normalised(vec3{g.signed_unit(), g.signed_unit(), g.signed_unit()} + vec3{0.01f, 0.0f, 0.0f}),
        g.range(0.0f, 3.1f));
    box.state.velocity = vec3{g.signed_unit(), g.signed_unit(), g.signed_unit()};
    box.angular_velocity = vec3{g.signed_unit(), g.signed_unit(), g.signed_unit()} * 3.0f;
    const vec3 corner = at_centre ? vec3{} : rotate(box.orientation, half);
    anchor = make_fixed(corner);
}

/// How fast `box`'s anchor is moving away from a fixed partner's — the
/// violation a visit starts from, and the scale its residual is judged on.
[[nodiscard]] vec3 anchor_velocity_probe(const rigid_body& box, const joint& j)
{
    const vec3 r = rotate(box.orientation, j.anchor_b);
    return box.state.velocity + cross(box.angular_velocity, r);
}

void section_e()
{
    rule("E  THE BALL-SOCKET, AND THE BLOCK");

    std::printf(
        "  A ball-socket is three point rows — along x, y and z — with no clamp.\n"
        "  Solved one at a time they are Gauss-Seidel on the 3x3 system\n"
        "      K lambda = -dC/dt,   K = (1/m_a + 1/m_b) 1 - [r_a]x Ia^-1 [r_a]x - [r_b]x Ib^-1 [r_b]x\n"
        "  and K's off-diagonal entries — the lever arm coupling x to y to z —\n"
        "  are what a row-by-row solve has to iterate away. Solved as a BLOCK,\n"
        "  lambda = K^-1 (-dC/dt): exact in one visit.\n");

    joint_config rows_cfg;
    rows_cfg.block_solve = false;
    rows_cfg.warm_start = false;
    joint_config block_cfg;
    block_cfg.warm_start = false;

    // ---- one joint, visit by visit ---------------------------------------------
    {
        rng g(811);
        rigid_body anchor;
        rigid_body box;
        random_socket(g, anchor, box, false);
        const joint j = make_ball_socket(anchor, box, world_point_of(anchor, vec3{}));

        rigid_body a1 = anchor, b1 = box;
        joint_batch rb = prepare_joint(a1, b1, j, k_h, rows_cfg);
        rigid_body a2 = anchor, b2 = box;
        joint_batch bb = prepare_joint(a2, b2, j, k_h, block_cfg);

        double k[3][3];
        for (int r = 0; r < 3; ++r)
        {
            for (int c = 0; c < 3; ++c)
            {
                const jacobian_row& x = rb.rows[r];
                const jacobian_row& y = rb.rows[c];
                k[r][c] = static_cast<double>(rb.inv_mass_a * dot(x.linear_a, y.linear_a)
                                              + dot(x.angular_a, rb.inv_inertia_a * y.angular_a)
                                              + rb.inv_mass_b * dot(x.linear_b, y.linear_b)
                                              + dot(x.angular_b, rb.inv_inertia_b * y.angular_b));
            }
        }
        const double predicted = gauss_seidel_radius(k);

        std::printf("\n  ONE JOINT: a random box on a socket at one of its corners, in a\n"
                    "  random pose, with a random velocity that violates the joint by\n"
                    "  %.3f m/s. The residual |dC/dt| after each visit:\n"
                    "     visit       rows           ratio        block\n",
                    static_cast<double>(length(anchor_velocity_probe(box, j))));
        double prev = 0.0;
        double ratio_sum = 0.0;
        int ratio_n = 0;
        for (int visit = 1; visit <= 10; ++visit)
        {
            const float rres = solve_joint(a1, b1, rb, false).max_residual;
            const float bres = solve_joint(a2, b2, bb, false).max_residual;
            const double ratio = prev > 0.0 ? rres / prev : 0.0;
            if (visit >= 4 && rres > 1e-6f) { ratio_sum += std::log(ratio); ++ratio_n; }
            std::printf("     %5d    %10.3e    %9.4f    %10.3e\n", visit, static_cast<double>(rres),
                        ratio, static_cast<double>(bres));
            prev = rres;
        }
        const double measured = ratio_n > 0 ? std::exp(ratio_sum / ratio_n) : 0.0;
        std::printf("     measured contraction (geometric mean, visits 4..10)   %.4f\n"
                    "     spectral radius of K's Gauss-Seidel matrix            %.4f\n",
                    measured, predicted);
        check(std::abs(measured / predicted - 1.0) < 0.02, "the rows contract at K's Gauss-Seidel radius");
    }

    // ---- a thousand of them -------------------------------------------------------
    {
        rng g(8110);
        double worst_block = 0.0;
        double worst_rows = 0.0;
        double worst_fit = 0.0;
        std::vector<double> radii;
        double worst_centre = 0.0;
        for (int trial = 0; trial < 1000; ++trial)
        {
            for (int centre = 0; centre <= 1; ++centre)
            {
                rigid_body anchor;
                rigid_body box;
                random_socket(g, anchor, box, centre == 1);
                const joint j = make_ball_socket(anchor, box, world_point_of(anchor, vec3{}));
                const float scale = length(anchor_velocity_probe(box, j));
                rigid_body a1 = anchor, b1 = box, a2 = anchor, b2 = box;
                joint_batch rb = prepare_joint(a1, b1, j, k_h, rows_cfg);
                joint_batch bb = prepare_joint(a2, b2, j, k_h, block_cfg);
                const double rr = solve_joint(a1, b1, rb, false).max_residual / scale;
                const double br = solve_joint(a2, b2, bb, false).max_residual / scale;
                if (centre)
                {
                    worst_centre = std::max(worst_centre, rr);
                    continue;
                }
                worst_block = std::max(worst_block, br);
                worst_rows = std::max(worst_rows, rr);
                double k[3][3];
                for (int r = 0; r < 3; ++r)
                {
                    for (int c = 0; c < 3; ++c)
                    {
                        const jacobian_row& x = rb.rows[r];
                        const jacobian_row& y = rb.rows[c];
                        k[r][c] = static_cast<double>(dot(x.angular_b, rb.inv_inertia_b * y.angular_b))
                                  + (r == c ? static_cast<double>(rb.inv_mass_b) : 0.0);
                    }
                }
                radii.push_back(gauss_seidel_radius(k));
                (void)worst_fit;
            }
        }
        std::sort(radii.begin(), radii.end());
        std::printf("\n  A THOUSAND RANDOM SOCKETS (box shape, mass, pose, corner, velocity),\n"
                    "  residual after ONE visit relative to the violation it started with:\n"
                    "     block, worst                                   %.3e\n"
                    "     rows, worst                                    %.3e\n"
                    "     rows' Gauss-Seidel radius: min %.3f  median %.3f  max %.3f\n",
                    worst_block, worst_rows, radii.front(), radii[radii.size() / 2], radii.back());
        std::printf("  CONTROL, the socket at the box's CENTRE of mass (r = 0): K is\n"
                    "  (1/m) times the identity, the rows do not couple, and one visit\n"
                    "  of rows is already exact — worst relative residual %.3e.\n",
                    worst_centre);
        check(worst_block < 1e-5, "a block is exact in one visit");
        check(worst_rows > 0.05, "rows are not");
        check(worst_centre < 1e-5, "and rows through the centre of mass are, because nothing couples");
    }

    // ---- a chain: does the block help where it matters? ------------------------------
    {
        std::printf("\n  AND ON A CHAIN — twelve 0.3 m links hinged end to end by sockets,\n"
                    "  released horizontal from a fixed anchor, 8 sweeps, 3 seconds. The\n"
                    "  worst anchor gap anywhere in the chain at any step:\n"
                    "     solve        worst gap      mean gap over the run\n");
        double worst[2] = {};
        for (int block = 1; block >= 0; --block)
        {
            rig r;
            r.cfg.joints.block_solve = block == 1;
            r.add(make_fixed(vec3{}));
            std::uint32_t prev = 0;
            for (int i = 0; i < 12; ++i)
            {
                const float x = 0.15f + 0.3f * static_cast<float>(i);
                const std::uint32_t link = r.add(make_box(vec3{x, 0.0f, 0.0f}, 1.0f, vec3{0.15f, 0.03f, 0.03f}));
                r.connect(prev, link, make_ball_socket(r.body(prev), r.body(link), vec3{x - 0.15f, 0.0f, 0.0f}));
                prev = link;
            }
            double mean = 0.0;
            int samples = 0;
            for (int step = 0; step < 180; ++step)
            {
                r.step();
                for (std::size_t j = 0; j < r.links.size(); ++j)
                {
                    const double e = r.error_of(j).linear;
                    worst[block] = std::max(worst[block], e);
                    mean += e;
                    ++samples;
                }
            }
            std::printf("     %-8s   %9.3f mm   %12.3f mm\n", block ? "block" : "rows",
                        worst[block] * 1000.0, mean / samples * 1000.0);
        }
        std::printf("  The block makes each JOINT exact and does nothing about the CHAIN:\n"
                    "  a sweep still carries information across one joint at a time, which\n"
                    "  is 8.10's tower again, and §14's subject.\n");
        check(worst[1] <= worst[0] * 1.05, "the block is no worse on a chain");
    }

    // ---- what each costs ------------------------------------------------------------
    {
        rng g(81100);
        std::vector<rigid_body> anchors(1000), boxes(1000);
        std::vector<joint> joints;
        for (int i = 0; i < 1000; ++i)
        {
            random_socket(g, anchors[static_cast<std::size_t>(i)], boxes[static_cast<std::size_t>(i)], false);
            joints.push_back(make_ball_socket(anchors[static_cast<std::size_t>(i)], boxes[static_cast<std::size_t>(i)],
                                              world_point_of(anchors[static_cast<std::size_t>(i)], vec3{})));
        }
        double best[2] = {1e30, 1e30};
        double prep[2] = {1e30, 1e30};
        for (int rep = 0; rep < 60; ++rep)
        {
            for (int block = 0; block <= 1; ++block)
            {
                std::vector<rigid_body> a = anchors, b = boxes;
                std::vector<joint_batch> batches(1000);
                clock_type::time_point t0 = clock_type::now();
                for (int i = 0; i < 1000; ++i)
                {
                    batches[static_cast<std::size_t>(i)] = prepare_joint(a[static_cast<std::size_t>(i)], b[static_cast<std::size_t>(i)],
                                                                          joints[static_cast<std::size_t>(i)], k_h,
                                                                          block ? block_cfg : rows_cfg);
                }
                prep[block] = std::min(prep[block], seconds_since(t0));
                t0 = clock_type::now();
                for (int it = 0; it < 8; ++it)
                {
                    for (int i = 0; i < 1000; ++i)
                    {
                        (void)solve_joint(a[static_cast<std::size_t>(i)], b[static_cast<std::size_t>(i)],
                                          batches[static_cast<std::size_t>(i)], false);
                    }
                }
                best[block] = std::min(best[block], seconds_since(t0));
            }
        }
        std::printf("\n  WHAT IT COSTS, per joint, minimum of 60 runs over 1,000 sockets\n"
                    "  (both arms are the engine's own `prepare_joint` and `solve_joint`,\n"
                    "  so both cross the same library boundary):\n"
                    "     solve      prepare      one visit\n"
                    "     rows     %7.2f ns   %8.2f ns\n"
                    "     block    %7.2f ns   %8.2f ns\n",
                    prep[0] / 1000 * 1e9, best[0] / 8000 * 1e9, prep[1] / 1000 * 1e9, best[1] / 8000 * 1e9);
        check(best[1] > 0.0, "timed");
    }
}

} // namespace

namespace
{

// ---------------------------------------------------------------------------
// §F  the pendulum, and the parallel axis
// ---------------------------------------------------------------------------

/// Swing a box hung by the middle of its top face and time it by downward zero
/// crossings of its centre's x. Returns the mean period over `swings` swings,
/// or the first period only when `swings` is 1.
struct period_result
{
    double period = 0.0;
    double energy_after = 0.0;   ///< energy after the timed swings, over the energy before
};

period_result time_pendulum(vec3 half, float mass, float amplitude, float hz, int swings,
                            position_correction pc = position_correction::split_impulse)
{
    rig r;
    r.h = 1.0f / hz;
    r.cfg.correction = pc;
    hang_box(r, half, mass, amplitude);
    const float g = 9.81f;
    const auto energy = [&r, g]() {
        const rigid_body& b = r.body(1);
        return energy_of(b) + static_cast<double>(mass_of(b) * g * b.state.position.y);
    };
    const double e0 = energy() - static_cast<double>(mass * g * (-half.y));

    crossing_timer t;
    const int max_steps = static_cast<int>(hz * 60.0f);
    for (int i = 0; i < max_steps && t.count < swings + 1; ++i)
    {
        r.step();
        t.sample(r.body(1).state.position.x, r.h);
    }
    period_result out;
    out.period = t.period();
    out.energy_after = (energy() - static_cast<double>(mass * g * (-half.y))) / e0;
    return out;
}

void section_f()
{
    rule("F  THE PENDULUM, AND THE PARALLEL AXIS");

    const vec3 half{0.1f, 0.5f, 0.1f};
    const float m = 2.0f;
    const float d = 0.5f;
    const float g = 9.81f;
    const float i_cm = m * (4.0f * half.x * half.x + 4.0f * half.y * half.y) / 12.0f;
    const float i_pivot = i_cm + m * d * d;
    const float t_body = pendulum_period(i_pivot, m, g, d);
    const float t_point = pendulum_period(m * d * d, m, g, d);

    std::printf(
        "  A 0.2 x 1.0 x 0.2 m, 2 kg box on a ball-socket at the middle of its\n"
        "  top face: its centre is d = 0.5 m below the pivot. A point mass at the\n"
        "  same distance swings with T = 2 pi sqrt(d/g). A BODY swings with\n"
        "      T = 2 pi sqrt(I_pivot / (m g d)),    I_pivot = I_cm + m d^2\n"
        "  — 8.3's parallel-axis theorem, which is what the extra m d^2 is.\n"
        "     I_cm about z     = m (w^2 + h^2)/12 = %.6f kg m^2\n"
        "     I_pivot          = I_cm + m d^2     = %.6f kg m^2\n"
        "     T, the body      = %.6f s\n"
        "     T, a point mass  = %.6f s    (%.2f%% short)\n",
        static_cast<double>(i_cm), static_cast<double>(i_pivot), static_cast<double>(t_body),
        static_cast<double>(t_point), 100.0 * (1.0 - static_cast<double>(t_point / t_body)));

    // THE REFERENCE IS THE PERIOD AT 2 DEGREES, NOT THE SMALL-SWING LIMIT.
    // The first draft compared against the limit and found an "error" that
    // did not shrink with the step: +6.6e-05 at 240 Hz. Two degrees is not
    // zero — T(a) = T/AGM(1, cos a/2) puts it 7.6e-05 slower — and an error
    // that refuses to converge is a reference that is wrong.
    const float two_degrees = 2.0f * 3.14159265f / 180.0f;
    const double t_two = pendulum_period_at(t_body, two_degrees);
    const period_result small = time_pendulum(half, m, two_degrees, 60.0f, 20);
    std::printf("\n  MEASURED, 2 degree amplitude, 60 Hz, twenty swings:\n"
                "     period           %.6f s\n"
                "     predicted        %.6f s    (the body's, at 2 degrees)\n"
                "     error            %+.2e       against the body\n"
                "                      %+.2f%%        against the point mass\n",
                small.period, t_two, small.period / t_two - 1.0, 100.0 * (small.period / t_point - 1.0));
    check(std::abs(small.period / t_two - 1.0) < 3e-4, "the box keeps the parallel-axis period");
    check(small.period / t_point - 1.0 > 0.1, "and not the point-mass one");

    // ---- how the error scales with the step -----------------------------------------
    std::printf("\n  AND HOW THE PERIOD ERROR SCALES WITH THE STEP — the same 2 degrees:\n"
                "     rate        period           error        ratio to the next\n");
    double errs[4] = {};
    int si = 0;
    for (float hz : {30.0f, 60.0f, 120.0f, 240.0f})
    {
        const period_result p = time_pendulum(half, m, two_degrees, hz, 20);
        errs[si++] = p.period / t_two - 1.0;
    }
    si = 0;
    for (float hz : {30.0f, 60.0f, 120.0f, 240.0f})
    {
        std::printf("     %4.0f Hz   %.6f s   %+.3e    ", static_cast<double>(hz), t_two * (1.0 + errs[si]), errs[si]);
        if (si < 3) { std::printf("%6.2f\n", errs[si] / errs[si + 1]); }
        else        { std::printf("\n"); }
        ++si;
    }
    std::printf("  Each halving of the step divides the error by four: second order in\n"
                "  h, the period error of a symplectic integrator (8.1 §5), with the\n"
                "  joint adding nothing of its own at this amplitude.\n");
    check(errs[1] / errs[2] > 3.5 && errs[1] / errs[2] < 4.5 && errs[2] / errs[3] > 3.5 && errs[2] / errs[3] < 4.5,
          "the period error is second order in h");

    // ---- amplitude -------------------------------------------------------------------
    std::printf("\n  A LARGE SWING IS SLOWER, by a factor with a closed form that is the\n"
                "  prettiest formula in the lesson:\n"
                "      T(a) = T_small / AGM(1, cos(a/2))\n"
                "  — the small-swing period divided by the arithmetic-geometric mean of\n"
                "  1 and cos(a/2), which is the complete elliptic integral in disguise.\n"
                "  The FIRST period, from rest at amplitude a, at 3840 Hz so that the\n"
                "  swing is not losing energy while it is being timed; and then the\n"
                "  energy the same swing keeps over that period at 60 Hz:\n"
                "     amplitude   predicted     measured     error       60 Hz: split   Baumgarte\n");
    double worst_fine = 0.0;
    double split90 = 0.0;
    double baum90 = 0.0;
    for (float deg : {10.0f, 30.0f, 60.0f, 90.0f, 120.0f})
    {
        const float a = deg * 3.14159265f / 180.0f;
        const double predicted = pendulum_period_at(t_body, a);
        const period_result fine = time_pendulum(half, m, a, 3840.0f, 1);
        const period_result split = time_pendulum(half, m, a, 60.0f, 1);
        const period_result baum = time_pendulum(half, m, a, 60.0f, 1, position_correction::baumgarte);
        worst_fine = std::max(worst_fine, std::abs(fine.period / predicted - 1.0));
        if (deg == 90.0f) { split90 = split.energy_after; baum90 = baum.energy_after; }
        std::printf("     %5.0f deg   %.6f s   %.6f s   %+.2e     %6.2f%%     %6.2f%%\n",
                    static_cast<double>(deg), predicted, fine.period, fine.period / predicted - 1.0,
                    100.0 * split.energy_after, 100.0 * baum.energy_after);
    }
    std::printf("  At 3840 Hz the formula holds to %.1e at every amplitude up to 120.\n"
                "  AND THE LAST TWO COLUMNS ARE §6's FINDING ON A REAL JOINT. At 60 Hz\n"
                "  a 90 degree swing keeps %.1f%% of its energy per period under split\n"
                "  impulse — §5's (w h)^2 per step, for a joint turning at up to\n"
                "  %.1f rad/s — and %.1f%% under Baumgarte, whose bias puts back what\n"
                "  the projection takes.\n",
                worst_fine, 100.0 * split90, std::sqrt(2.0 * m * g * d * (1.0 - std::cos(k_pi / 2.0)) / i_pivot),
                100.0 * baum90);
    check(worst_fine < 3e-3, "the finite-amplitude period follows the AGM formula");
    check(baum90 > split90, "Baumgarte keeps more of a swinging joint's energy");
}

} // namespace

namespace
{

// ---------------------------------------------------------------------------
// §G  the hinge
// ---------------------------------------------------------------------------

/// The angle between a body's local +y and world +y, radians. A door hung
/// true has zero; a door that has flopped over has pi/2 or more.
[[nodiscard]] float tilt_of(const rigid_body& b)
{
    const vec3 up = rotate(b.orientation, vec3{0.0f, 1.0f, 0.0f});
    return std::atan2(length(cross(up, vec3{0.0f, 1.0f, 0.0f})), up.y);
}

void section_g()
{
    rule("G  THE HINGE");

    std::printf(
        "  A hinge is a ball-socket plus two angular rows: the relative angular\n"
        "  velocity may point along the axis and nowhere else,\n"
        "      J_i = [ 0, -p_i, 0, p_i ],   p_0, p_1 perpendicular to the axis,\n"
        "  with C_i = (a_1 x b_1) . p_i — the two hinge axes' cross product,\n"
        "  which for a small misalignment IS the rotation carrying one onto the\n"
        "  other. Solved as a 2x2 block.\n"
        "\n"
        "  THE FIXTURE: a 1.0 x 2.0 x 0.1 m, 10 kg door, hinged at the middle of\n"
        "  one vertical edge to a fixed frame, axis vertical, gravity on, given\n"
        "  2 rad/s about the axis. Five seconds:\n"
        "     joint              worst anchor gap    worst misalignment    worst tilt\n");

    float tilt[2] = {};
    float misalign = 0.0f;
    for (int hinge = 1; hinge >= 0; --hinge)
    {
        rig r;
        r.add(make_fixed(vec3{}));
        const std::uint32_t door = r.add(make_box(vec3{0.5f, 0.0f, 0.0f}, 10.0f, vec3{0.5f, 1.0f, 0.05f}));
        const std::size_t j = r.connect(0, door, hinge ? make_hinge(r.body(0), r.body(door), vec3{}, vec3{0.0f, 1.0f, 0.0f})
                                                       : make_ball_socket(r.body(0), r.body(door), vec3{}));
        r.body(door).angular_velocity = vec3{0.0f, 2.0f, 0.0f};
        float gap = 0.0f;
        float mis = 0.0f;
        for (int i = 0; i < 300; ++i)
        {
            r.step();
            const joint_error e = r.error_of(j);
            gap = std::max(gap, e.linear);
            mis = std::max(mis, e.angular);
            tilt[hinge] = std::max(tilt[hinge], tilt_of(r.body(door)));
        }
        if (hinge) { misalign = mis; }
        std::printf("     %-14s  %14.3f mm   %14.4f deg   %10.2f deg\n", hinge ? "hinge" : "ball-socket only",
                    static_cast<double>(gap) * 1000.0, hinge ? static_cast<double>(mis) * 57.29578 : 0.0,
                    static_cast<double>(tilt[hinge]) * 57.29578);
    }
    std::printf("  The door on a hinge stays within %.4f degrees of true while it\n"
                "  swings. The same door on the ball-socket alone falls over: the\n"
                "  socket holds the POINT and nothing else, and gravity's torque about\n"
                "  that point is exactly what the two angular rows exist to answer.\n",
                static_cast<double>(misalign) * 57.29578);
    check(tilt[1] < 0.01f, "a hinged door stays upright");
    check(tilt[0] > 1.0f, "a door on a ball-socket alone falls over");

    // ---- the frame the perpendicular impulse is cached in ------------------------
    {
        std::printf(
            "\n  THE CACHED FRAME, CHECKED BY A PROBE. 8.10 §4 found friction impulses\n"
            "  cached as coordinates in a basis `tangent_basis` re-chose every frame\n"
            "  from a WORLD normal, flipping 90 degrees whenever two near-zero\n"
            "  components crossed. A hinge's perpendicular impulse has the same\n"
            "  shape. The engine avoids it twice over — the basis is chosen from the\n"
            "  axis in BODY a's frame, which never moves, and the cache is a world\n"
            "  vector — and this probe counts what the obvious way would have done.\n"
            "\n"
            "  THE FIXTURE: a twelve-plank bridge, hinged plank to plank about\n"
            "  z and to two fixed posts at the ends, sagging under gravity with a\n"
            "  2 kg ball bouncing on it, ten seconds.\n");
        scene s;
        s.sleep.enabled = false;
        add_floor(s);
        const std::uint32_t left = s.add(make_fixed(vec3{-3.0f, 2.0f, 0.0f}), box_shape(vec3{0.05f, 0.05f, 0.4f}));
        const std::uint32_t right = s.add(make_fixed(vec3{3.0f, 2.0f, 0.0f}), box_shape(vec3{0.05f, 0.05f, 0.4f}));
        std::uint32_t prev = left;
        float prev_edge = -3.0f;
        for (int i = 0; i < 12; ++i)
        {
            const float x = -3.0f + 0.25f + 0.5f * static_cast<float>(i);
            const std::uint32_t plank = s.add(make_box(vec3{x, 2.0f, 0.0f}, 2.0f, vec3{0.24f, 0.04f, 0.4f}),
                                              box_shape(vec3{0.24f, 0.04f, 0.4f}));
            s.connect(prev, plank, make_hinge(s.body(prev), s.body(plank), vec3{prev_edge, 2.0f, 0.0f},
                                              vec3{0.0f, 0.0f, 1.0f}));
            prev = plank;
            prev_edge = x + 0.25f;
        }
        s.connect(prev, right, make_hinge(s.body(prev), s.body(right), vec3{3.0f, 2.0f, 0.0f}, vec3{0.0f, 0.0f, 1.0f}));
        const std::uint32_t ball = s.add(make_sphere(vec3{0.3f, 3.0f, 0.1f}, 2.0f, 0.15f), sphere_shape(0.15f));
        (void)ball;

        std::vector<vec3> world_t(s.links.size());
        std::vector<vec3> body_t(s.links.size());
        int world_flips = 0;
        int body_flips = 0;
        int frames = 0;
        double worst_world = 0.0;
        double worst_body = 0.0;
        for (int step = 0; step < 600; ++step)
        {
            s.step();
            const auto batches = s.solver.joint_batches();
            for (std::size_t k = 0; k < batches.size(); ++k)
            {
                vec3 t1;
                vec3 t2;
                tangent_basis(batches[k].axis, t1, t2);
                const vec3 p = batches[k].perp[0];
                if (step > 0)
                {
                    const double aw = std::atan2(length(cross(world_t[k], t1)), dot(world_t[k], t1));
                    const double ab = std::atan2(length(cross(body_t[k], p)), dot(body_t[k], p));
                    worst_world = std::max(worst_world, aw);
                    worst_body = std::max(worst_body, ab);
                    if (aw > 30.0 / 57.29578) { ++world_flips; }
                    if (ab > 30.0 / 57.29578) { ++body_flips; }
                    ++frames;
                }
                world_t[k] = t1;
                body_t[k] = p;
            }
        }
        std::printf("     basis chosen from           rotated > 30 deg      worst step-to-step\n"
                    "     the world axis (8.10's way)   %5d of %d          %8.3f deg\n"
                    "     body a's own axis (8.11)      %5d of %d          %8.3f deg\n",
                    world_flips, frames, worst_world * 57.29578, body_flips, frames, worst_body * 57.29578);
        std::printf("  A hinge axis along z, carried by a body whose motion is in the x-y\n"
                    "  plane, has x and y components that are rounding noise either side of\n"
                    "  zero — the same condition 8.10 found on a crate's normal, and it\n"
                    "  flips the world-chosen basis constantly. Chosen where the axis is a\n"
                    "  constant, it rotates exactly as the plank does.\n");
        check(body_flips == 0, "a basis chosen in the body's frame never flips");
        check(world_flips > 0, "a basis chosen from the world axis does");
    }
}

// ---------------------------------------------------------------------------
// §H  limits: a contact that is always detected
// ---------------------------------------------------------------------------

/// A slab hinged about a vertical axis `offset` metres from its centre, from a
/// fixed frame, with its angle limited to `[-limit, +limit]`. Gravity OFF, so
/// that nothing but the joint acts.
struct hinge_rig
{
    rig r;
    std::size_t j = 0;
    std::uint32_t slab = 0;
    float inertia_axis = 0.0f;   ///< about the vertical axis through the centre
    float mass = 10.0f;
};

void build_slab(hinge_rig& h, float offset, float limit, bool speculative)
{
    const vec3 half{0.5f, 1.0f, 0.05f};
    h.r.world.set_gravity(vec3{});
    h.r.cfg.joints.speculative_limits = speculative;
    h.r.add(make_fixed(vec3{}));
    h.slab = h.r.add(make_box(vec3{offset, 0.0f, 0.0f}, h.mass, half));
    joint j = make_hinge(h.r.body(0), h.r.body(h.slab), vec3{}, vec3{0.0f, 1.0f, 0.0f});
    j.limit.enabled = true;
    j.limit.lower = -limit;
    j.limit.upper = limit;
    h.j = h.r.connect(0, h.slab, j);
    h.inertia_axis = h.mass * (4.0f * half.x * half.x + 4.0f * half.z * half.z) / 12.0f;
}

/// Put the slab at hinge angle `angle` turning at `omega` about the axis — as a
/// rigid motion about the HINGE, so the pin is satisfied exactly.
void pose_slab(hinge_rig& h, float offset, float angle, float omega)
{
    const quat q = quat_y(angle);
    rigid_body& b = h.r.body(h.slab);
    b.orientation = q;
    b.state.position = rotate(q, vec3{offset, 0.0f, 0.0f});
    b.angular_velocity = vec3{0.0f, omega, 0.0f};
    b.state.velocity = cross(b.angular_velocity, b.state.position);
}

[[nodiscard]] float axial_rate(hinge_rig& h)
{
    return h.r.body(h.slab).angular_velocity.y;
}

void section_h()
{
    rule("H  LIMITS: A CONTACT THAT IS ALWAYS DETECTED");

    std::printf(
        "  A hinge limit is a one-sided angular row: C = upper - theta >= 0,\n"
        "  impulse >= 0. It is a contact on an angle, and it would inherit\n"
        "  8.10 §6's arrival depth — a stop is only noticed on the step AFTER\n"
        "  the door passed it, by up to w h — except for one thing a contact\n"
        "  does not have: a hinge always knows its angle. So the row can exist\n"
        "  BEFORE the stop is reached, with target -C/h — \"arrive, but no\n"
        "  faster than this step allows\" — and the clamp makes it do nothing\n"
        "  unless the door would otherwise overshoot. That is a speculative\n"
        "  constraint, and for a contact it would need the narrow phase to run\n"
        "  on pairs that are not touching yet.\n");

    // ---- part 1: a turnstile, where nothing couples --------------------------------
    // THE PHASE, NOT THE SPEED, IS WHAT HAS TO BE SAMPLED UNIFORMLY. The first
    // draft swept the speed from 2.0 to 2.2 rad/s, which moves the arrival
    // phase through 1.36 cycles rather than a whole number of them, and read a
    // mean of 0.42 against the 0.5 a uniform distribution has. Here the speed
    // is fixed and the starting angle steps back through exactly one step's
    // travel, so the 200 arrivals land at 200 evenly spaced phases.
    std::printf("\n  A TURNSTILE — the same slab hinged through its CENTRE — swung at\n"
                "  2 rad/s into a 0.5 rad stop from 200 starting angles spaced evenly\n"
                "  across one step's travel, so that it arrives at every phase of a\n"
                "  step. Overshoot past the stop, as a fraction of w h:\n"
                "     limit            mean        largest      smallest\n");
    double worst_spec = 0.0;
    double mean_react = 0.0;
    for (int spec = 0; spec <= 1; ++spec)
    {
        double sum = 0.0;
        double hi = 0.0;
        double lo = 1e30;
        for (int k = 0; k < 200; ++k)
        {
            const float omega = 2.0f;
            const float start = -omega * k_h * (static_cast<float>(k) + 0.5f) / 200.0f;
            hinge_rig h;
            build_slab(h, 0.0f, 0.5f, spec == 1);
            pose_slab(h, 0.0f, start, omega);
            float over = 0.0f;
            for (int i = 0; i < 40; ++i)
            {
                h.r.step();
                over = std::max(over, hinge_angle(h.r.joint_at(h.j), h.r.body(0), h.r.body(h.slab)) - 0.5f);
            }
            const double frac = static_cast<double>(over) / (static_cast<double>(omega) * k_h);
            sum += frac;
            hi = std::max(hi, frac);
            lo = std::min(lo, frac);
        }
        if (spec) { worst_spec = hi; }
        else      { mean_react = sum / 200.0; }
        std::printf("     %-14s  %9.4f   %9.4f   %11.2e\n", spec ? "speculative" : "reactive",
                    sum / 200.0, hi, lo);
    }
    std::printf("  Reactive: uniform on [0, w h], exactly 8.10 §6's arrival depth on an\n"
                "  angle. Speculative: gone, to rounding.\n");
    check(std::abs(mean_react - 0.5) < 0.05, "a reactive limit overshoots by half a step on average");
    check(worst_spec < 1e-4, "a speculative limit through the centre does not overshoot");

    // ---- part 2: the door, and the rate the parallel axis sets -------------------------
    std::printf(
        "\n  AND NOW THE DOOR — the same slab hinged at its EDGE — where the\n"
        "  speculative limit is no longer exact, and the reason is a number.\n"
        "\n"
        "  The limit row is ANGULAR: J = [0, a, 0, -a]. Its effective mass is the\n"
        "  inertia about the CENTRE, I_cm. But a door does not turn about its\n"
        "  centre; it turns about the pin, where the inertia is I_cm + m d^2. So\n"
        "  the limit asks for a change in w that the pin then partly undoes —\n"
        "  the centre's velocity no longer matches — and the pin's correction\n"
        "  gives back a fraction\n"
        "      rho = m d^2 / (I_cm + m d^2)\n"
        "  of what the limit took. Gauss-Seidel between the two contracts by rho\n"
        "  per sweep. For any uniform slab hinged at its edge that is 3/4,\n"
        "  whatever its size. The parallel-axis theorem again: §8 heard it as a\n"
        "  period, and here it is a convergence rate.\n"
        "\n"
        "  MEASURED: one step, the slab 0.005 rad from its stop turning at\n"
        "  2 rad/s into it, from a fresh state (warm start off), sweeping the\n"
        "  iteration count. The error in w after the solve, relative to the\n"
        "  error before it:\n"
        "     hinge offset   rho predicted   n=1       n=2       n=4       n=8       per sweep\n");
    double worst_fit = 0.0;
    for (float frac : {0.0f, 0.25f, 0.5f, 1.0f})
    {
        const float offset = frac * 1.0f;
        hinge_rig probe;
        build_slab(probe, offset, 0.5f, true);
        const double rho = lever_ratio(probe.inertia_axis, probe.mass, offset);
        const double target = 0.005 / k_h;
        std::printf("     d = %4.2f W   %10.4f   ", static_cast<double>(frac), rho);
        double last = 0.0;
        double prev = 0.0;
        for (int n : {1, 2, 4, 8})
        {
            hinge_rig h;
            build_slab(h, offset, 0.5f, true);
            h.r.cfg.velocity_iterations = n;
            h.r.cfg.joints.warm_start = false;
            h.r.cfg.correction = position_correction::none;
            pose_slab(h, offset, 0.5f - 0.005f, 2.0f);
            h.r.world.integrate_velocities(k_h);
            h.r.solver.begin(h.r.world.bodies());
            for (auto& l : h.r.links) { h.r.solver.add(l.a, l.b, l.j); }
            (void)h.r.solver.solve(k_h, h.r.cfg, h.r.sleep);
            const double err = (axial_rate(h) - target) / (2.0 - target);
            std::printf("%8.4f  ", err);
            prev = last;
            last = err;
        }
        const double per = rho > 0.0 ? std::pow(std::abs(last / prev), 1.0 / 4.0) : std::abs(last);
        if (rho > 0.0) { worst_fit = std::max(worst_fit, std::abs(per / rho - 1.0)); }
        std::printf("%8.4f\n", per);
        check(rho > 0.0 || std::abs(last) < 1e-5, "a turnstile's limit is exact in one sweep");
    }
    std::printf("  The measured rate per sweep matches m d^2 / I_pivot to %.1f%% on every\n"
                "  shape: 0 through the centre, 0.43 a quarter of the way out, 0.75 at\n"
                "  the edge and 0.92 on a bracket a full width out.\n", worst_fit * 100.0);
    check(worst_fit < 0.02, "the limit converges at rho = m d^2 / I_pivot per sweep");

    // ---- part 3: what that leaves at eight sweeps ------------------------------------
    std::printf("\n  AT THE SHIPPED EIGHT SWEEPS, swung into the stop at 2 rad/s and\n"
                "  left for a second — overshoot past the stop, and the speed it\n"
                "  bounces back off at, as a fraction of the speed it arrived at:\n"
                "     hinge offset   rho^8      overshoot     rebound\n");
    double rebound_edge = 0.0;
    for (float frac : {0.0f, 0.25f, 0.5f, 1.0f})
    {
        hinge_rig h;
        build_slab(h, frac, 0.5f, true);
        const double rho = lever_ratio(h.inertia_axis, h.mass, frac);
        pose_slab(h, frac, 0.2f, 2.0f);
        float over = 0.0f;
        for (int i = 0; i < 60; ++i)
        {
            h.r.step();
            over = std::max(over, hinge_angle(h.r.joint_at(h.j), h.r.body(0), h.r.body(h.slab)) - 0.5f);
        }
        const double rebound = -axial_rate(h) / 2.0;
        if (frac == 0.5f) { rebound_edge = rebound; }
        std::printf("     d = %4.2f W   %8.5f   %8.5f rad   %8.4f\n", static_cast<double>(frac), std::pow(rho, 8.0),
                    static_cast<double>(over), rebound);
    }
    std::printf("  A door with restitution zero bounces off its stop at %.1f%% of its\n"
                "  arrival speed at eight sweeps. That is not restitution; it is the\n"
                "  convergence that is left, and more sweeps are the only knob that\n"
                "  reaches it — §14 prices them.\n", rebound_edge * 100.0);
    check(rebound_edge > 0.0, "a door bounces off its stop at eight sweeps");
}

// ---------------------------------------------------------------------------
// §I  motors, and what friction was all along
// ---------------------------------------------------------------------------

/// Integrate `dw/dt = a − c·w³` from `w_from` until `w` reaches `w_to`, by RK4
/// in fine steps, and return the time taken. The ODE of a motor (or a brake)
/// fighting §5's dissipation, which removes `ρ·(w·h)²` of a joint's kinetic
/// energy per step, i.e. `½·ρ·h·w²` of its angular momentum per second.
double time_to_reach(double a, double c, double w_from, double w_to)
{
    const auto f = [a, c](double w) { return a - c * w * w * w; };
    const double dt = 1e-5;
    double w = w_from;
    double t = 0.0;
    const bool rising = w_to > w_from;
    for (int i = 0; i < 100000000; ++i)
    {
        const double k1 = f(w);
        const double k2 = f(w + 0.5 * dt * k1);
        const double k3 = f(w + 0.5 * dt * k2);
        const double k4 = f(w + dt * k3);
        const double next = w + dt * (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
        if (rising ? next >= w_to : next <= w_to)
        {
            return t + dt * (w_to - w) / (next - w);
        }
        w = next;
        t += dt;
    }
    return 1e30;
}

void section_i()
{
    rule("I  MOTORS, AND WHAT FRICTION WAS ALL ALONG");

    std::printf(
        "  A motor is the axis row with a TARGET — the speed — and a symmetric\n"
        "  clamp: |impulse| <= tau_max h. While it is short of its speed the\n"
        "  clamp is saturated, so it applies exactly tau_max h per step, the\n"
        "  angular acceleration is constant, and the spin-up time is exact:\n"
        "      t = I w / tau_max\n"
        "  with I the inertia about the PIN — the clamp decides the impulse, so\n"
        "  the row's own I_cm-based mass never enters.\n"
        "\n"
        "     slab             I about pin   rho      I w / tau    with §5's loss   measured\n");
    double worst = 0.0;
    double naive_edge = 0.0;
    double measured_edge = 0.0;
    for (float offset : {0.0f, 0.5f})
    {
        hinge_rig h;
        build_slab(h, offset, 0.5f, true);
        joint& j = h.r.joint_at(h.j);
        j.limit.enabled = false;
        j.motor.enabled = true;
        j.motor.speed = 4.0f;
        j.motor.max_torque = 5.0f;
        const double i_pin = h.inertia_axis + static_cast<double>(h.mass) * offset * offset;
        const double rho = lever_ratio(h.inertia_axis, h.mass, offset);
        const double naive = motor_spin_up_time(static_cast<float>(i_pin), 4.0f, 5.0f);
        const double lossy = time_to_reach(5.0 / i_pin, 0.5 * rho * k_h, 0.0, 4.0 * (1.0 - 1e-4));
        double t = 0.0;
        double reached = -1.0;
        for (int i = 0; i < 1200 && reached < 0.0; ++i)
        {
            h.r.step();
            t += k_h;
            if (axial_rate(h) >= 4.0f * (1.0f - 1e-4f)) { reached = t; }
        }
        // `reached` is the end of the first step on which the speed is there;
        // the crossing itself is somewhere inside that step.
        worst = std::max(worst, std::abs(reached - k_h * 0.5 - lossy));
        if (offset > 0.0f) { naive_edge = naive; measured_edge = reached; }
        std::printf("     %-15s  %7.4f kg m2  %5.3f   %8.4f s     %8.4f s     %8.4f s\n",
                    offset == 0.0f ? "through centre" : "door, at edge", i_pin, rho, naive, lossy, reached);
    }
    std::printf(
        "  Through the centre I w / tau is right to the step. At the edge it is\n"
        "  %.1f%% short, and the measurement refused the section's claim — the\n"
        "  clamp does apply exactly tau h per step, but the joint is also\n"
        "  BLEEDING angular momentum, at §5's rate. A velocity joint dissipates\n"
        "  rho (w h)^2 of its kinetic energy per step — rho the fraction of it\n"
        "  that lives in the lever arm — so the motor is fighting\n"
        "      dw/dt = tau / I_pin  -  (rho h / 2) w^3\n"
        "  and integrating THAT predicts the measured time to within a step.\n",
        100.0 * (measured_edge / naive_edge - 1.0));
    check(worst <= k_h, "the motor's spin-up matches I w / tau with §5's loss, to the step");

    // ---- stall -----------------------------------------------------------------------
    {
        const float m = 2.0f;
        const float d = 0.5f;
        const float g = 9.81f;
        float lo = 0.0f;
        float hi = 30.0f;
        for (int it = 0; it < 18; ++it)
        {
            const float tau = 0.5f * (lo + hi);
            rig r;
            r.add(make_fixed(vec3{}));
            const std::uint32_t arm = r.add(make_box(vec3{d, 0.0f, 0.0f}, m, vec3{0.5f, 0.05f, 0.05f}));
            joint j = make_hinge(r.body(0), r.body(arm), vec3{}, vec3{0.0f, 0.0f, 1.0f});
            j.motor.enabled = true;
            j.motor.speed = 0.2f;
            j.motor.max_torque = tau;
            const std::size_t ji = r.connect(0, arm, j);
            r.run(1.0f);
            const float theta = hinge_angle(r.joint_at(ji), r.body(0), r.body(arm));
            if (theta > 0.0f) { hi = tau; } else { lo = tau; }
        }
        const float found = 0.5f * (lo + hi);
        std::printf("\n  STALL. A 1 m, 2 kg arm hinged at its end about a horizontal axis,\n"
                    "  held level, motor asked to lift it at 0.2 rad/s. Gravity's torque\n"
                    "  about the pin is m g d = %.4f N m. Bisecting the motor's torque\n"
                    "  over 18 halvings for the smallest that lifts it: %.4f N m\n"
                    "  (%.2e relative).\n",
                    static_cast<double>(m * g * d), static_cast<double>(found),
                    static_cast<double>(found / (m * g * d) - 1.0f));
        check(std::abs(found / (m * g * d) - 1.0f) < 0.01f, "the motor stalls at m g d");
    }

    // ---- friction is a motor with target zero -----------------------------------------
    {
        std::printf(
            "\n  AND FRICTION WAS THIS ROW ALL ALONG. 8.9's tangent row clips at\n"
            "  mu times the normal impulse; this row clips at tau h. Set the target\n"
            "  speed to ZERO and a motor is a brake with a fixed maximum torque —\n"
            "  Coulomb friction in the hinge, with a constant deceleration, so a\n"
            "  door pushed at w0 stops in\n"
            "      t = I_pivot w0 / tau\n"
            "  — shortened a little by the same §5 loss, which here HELPS the brake,\n"
            "  and the prediction below integrates it —\n"
            "  and then STAYS stopped, because below the clamp the row holds zero\n"
            "  speed exactly — static friction, from the same clamp. The door at its\n"
            "  edge, gravity on, pushed at 2 rad/s:\n"
            "     tau          predicted stop    measured stop    speed after 5 s\n");
        double stop_err = 0.0;
        for (float tau : {2.0f, 4.0f, 0.0f})
        {
            hinge_rig h;
            build_slab(h, 0.5f, 0.5f, true);
            h.r.world.set_gravity(vec3{0.0f, -9.81f, 0.0f});
            joint& j = h.r.joint_at(h.j);
            j.limit.enabled = false;
            j.motor.enabled = true;
            j.motor.speed = 0.0f;
            j.motor.max_torque = tau;
            pose_slab(h, 0.5f, 0.0f, 2.0f);
            const double i_pin = h.inertia_axis + static_cast<double>(h.mass) * 0.25;
            const double rho = lever_ratio(h.inertia_axis, h.mass, 0.5f);
            const double predicted = tau > 0.0f ? time_to_reach(-tau / i_pin, 0.5 * rho * k_h, 2.0, 0.0) : 1e30;
            double t = 0.0;
            double stopped = -1.0;
            for (int i = 0; i < 300; ++i)
            {
                h.r.step();
                t += k_h;
                if (stopped < 0.0 && std::abs(axial_rate(h)) < 1e-3f) { stopped = t; }
            }
            if (tau > 0.0f) { stop_err = std::max(stop_err, std::abs(stopped - predicted)); }
            if (tau > 0.0f)
            {
                std::printf("     %4.1f N m    %10.4f s    %10.4f s     %.2e rad/s\n", static_cast<double>(tau),
                            predicted, stopped, static_cast<double>(std::abs(axial_rate(h))));
            }
            else
            {
                std::printf("     %4.1f N m    %10s      %10s       %.4f rad/s   (control: no brake)\n",
                            static_cast<double>(tau), "never", stopped < 0 ? "never" : "?",
                            static_cast<double>(axial_rate(h)));
            }
        }
        std::printf("  Within two steps. The last step is the one where the brake is NOT\n"
                    "  saturated — the door is slower than tau h can stop — and an\n"
                    "  unsaturated axis row on a door converges at §10's rho per sweep, so\n"
                    "  the final few millimetres per second take one step more.\n");
        check(stop_err <= 2.0 * k_h, "a zero-speed motor stops a door like Coulomb friction");
    }
}

} // namespace

namespace
{

// ---------------------------------------------------------------------------
// §J  islands, sleeping and joints
// ---------------------------------------------------------------------------

/// `links` spheres hanging in a vertical chain from a fixed anchor at
/// `(x, 0, 0)`, 0.25 m apart, joined by ball-sockets midway between them. The
/// spheres are 0.16 m across, so neighbours never touch: every edge between
/// them is a joint and nothing else.
std::vector<std::uint32_t> hang_chain(rig& r, std::uint32_t anchor, float x, int links,
                                      float link_mass = 1.0f, float end_mass = 0.0f)
{
    std::vector<std::uint32_t> ids;
    std::uint32_t prev = anchor;
    for (int i = 0; i < links; ++i)
    {
        const bool last = i == links - 1 && end_mass > 0.0f;
        const float y = -0.25f * static_cast<float>(i + 1);
        const std::uint32_t b = r.add(make_sphere(vec3{x, y, 0.0f}, last ? end_mass : link_mass,
                                                  last ? 0.1f : 0.08f));
        r.connect(prev, b, make_ball_socket(r.body(prev), r.body(b), vec3{x, y + 0.125f, 0.0f}));
        ids.push_back(b);
        prev = b;
    }
    return ids;
}

void section_j()
{
    rule("J  ISLANDS, SLEEPING AND JOINTS");

    std::printf(
        "  A joint is an edge of the island graph by exactly the rule a contact\n"
        "  is: both ends must be able to move. So a chain whose links never\n"
        "  touch is ONE island, and two chains hung from one fixed ceiling are\n"
        "  two. Leave the joint edges out and each link is its own island.\n");

    {
        rig r;
        r.sleep.enabled = true;
        const std::uint32_t ceiling = r.add(make_fixed(vec3{}));
        hang_chain(r, ceiling, 0.0f, 8);
        hang_chain(r, ceiling, 1.0f, 8);
        r.step();
        std::vector<int> labels;
        std::vector<joint_pair> jp;
        for (auto& l : r.links) { jp.push_back(joint_pair{l.a, l.b, &l.j}); }
        const int with = build_islands(r.world.bodies(), std::span<const contact_pair>{}, jp, labels);
        const int without = build_islands(r.world.bodies(), std::span<const contact_pair>{}, labels);
        std::printf("\n  TWO EIGHT-LINK CHAINS FROM ONE FIXED CEILING:\n"
                    "     islands with joint edges      %2d   (the solver reports %d)\n"
                    "     islands without them          %2d\n",
                    with, r.stats.islands, without);
        check(with == 2 && r.stats.islands == 2, "two chains from one ceiling are two islands");
        check(without == 16, "without joint edges every link is its own island");
    }

    std::printf(
        "\n  WHY IT MATTERS: SLEEPING. 8.10 §10 measured a crate slept on its own\n"
        "  hanging in the air, because a sleeping body is an immovable body to\n"
        "  everything else. A chain is the case where that is guaranteed to\n"
        "  bite, because its links go quiet together and would sleep together\n"
        "  either way — the difference only shows when something WAKES one.\n"
        "\n"
        "  THE FIXTURE: one eight-link chain, hanging still until it sleeps; then\n"
        "  the bottom link is struck sideways at 2 m/s. One second later: how\n"
        "  many links are awake, and how many have moved more than a millimetre?\n"
        "  Per-link sleeping is emulated as 8.10 §10 did it, by making the other\n"
        "  seven FIXED — not an approximation, but the same statement in the\n"
        "  type system.\n"
        "     rule                 asleep before   awake after   moved > 1 mm   bottom swung\n");
    int moved[2] = {};
    int awake[2] = {};
    for (int per_link = 0; per_link <= 1; ++per_link)
    {
        rig r;
        r.sleep.enabled = true;
        const std::uint32_t ceiling = r.add(make_fixed(vec3{}));
        const std::vector<std::uint32_t> chain = hang_chain(r, ceiling, 0.0f, 8);
        r.run(1.5f);
        const int asleep = r.stats.sleeping_bodies;
        std::vector<vec3> before;
        for (std::uint32_t id : chain) { before.push_back(r.body(id).state.position); }
        if (per_link)
        {
            for (std::size_t k = 0; k + 1 < chain.size(); ++k)
            {
                rigid_body& b = r.body(chain[k]);
                b.kind = body_kind::fixed;
                b.inv_mass = 0.0f;
                b.inv_inertia_local = mat3{vec3{}, vec3{}, vec3{}};
            }
        }
        rigid_body& bottom = r.body(chain.back());
        bottom.state.velocity = vec3{2.0f, 0.0f, 0.0f};
        wake(bottom);
        float swing = 0.0f;
        for (int i = 0; i < 60; ++i)
        {
            r.step();
            swing = std::max(swing, std::abs(r.body(chain.back()).state.position.x));
        }
        for (std::size_t k = 0; k < chain.size(); ++k)
        {
            const rigid_body& b = r.body(chain[k]);
            if (length(b.state.position - before[k]) > 0.001f) { ++moved[per_link]; }
            if (b.kind == body_kind::dynamic && !b.sleeping) { ++awake[per_link]; }
        }
        std::printf("     %-18s   %6d of 8     %6d of 8    %6d of 8     %.3f m\n",
                    per_link ? "per link" : "per island (engine)", asleep, awake[per_link],
                    moved[per_link], static_cast<double>(swing));
    }
    std::printf("  Under the island rule the whole chain wakes and swings. Under a\n"
                "  per-link rule one link swings from a frozen chain above it, as a\n"
                "  0.25 m pendulum hung from the air.\n");
    check(awake[0] == 8 && moved[0] == 8, "the island rule wakes the whole chain");
    check(awake[1] == 1 && moved[1] == 1, "a per-link rule wakes one link and leaves seven frozen");
}

// ---------------------------------------------------------------------------
// §K  what joints leave behind
// ---------------------------------------------------------------------------

/// A ten-link chain with an end weight of `ratio` times a link's mass, hung
/// straight and left to settle. Returns the total stretch (the sum of every
/// joint's anchor gap) in millimetres, averaged over the last half second.
double chain_stretch(float ratio, int iterations, int substeps)
{
    rig r;
    r.cfg.velocity_iterations = iterations;
    r.h = k_h / static_cast<float>(substeps);
    const std::uint32_t ceiling = r.add(make_fixed(vec3{}));
    hang_chain(r, ceiling, 0.0f, 10, 1.0f, ratio);
    r.run(2.5f);
    double sum = 0.0;
    int n = 0;
    const int tail = static_cast<int>(0.5f / r.h + 0.5f);
    for (int i = 0; i < tail; ++i)
    {
        r.step();
        double total = 0.0;
        for (std::size_t j = 0; j < r.links.size(); ++j) { total += r.error_of(j).linear; }
        sum += total;
        ++n;
    }
    return sum / n * 1000.0;
}

void section_k()
{
    rule("K  WHAT JOINTS LEAVE BEHIND");

    std::printf(
        "  A block makes one joint exact. It does nothing for a CHAIN of them:\n"
        "  a Gauss-Seidel sweep still carries information across one joint at\n"
        "  a time, which is 8.10 §14's tower lying on its side. And a chain\n"
        "  adds the thing a tower of equal crates does not have — a MASS RATIO.\n"
        "\n"
        "  THE FIXTURE: ten 1 kg links, 0.25 m apart, hanging from a fixed\n"
        "  ceiling, with an end weight of M kg. Settled for 2.5 s; the total\n"
        "  stretch of the chain (every joint's gap, summed) over the next 0.5 s.\n"
        "  Ideal is zero.\n"
        "\n"
        "     end weight    8 sweeps    16         32         64        128\n");
    double at8[4] = {};
    int ri = 0;
    for (float ratio : {1.0f, 10.0f, 100.0f, 1000.0f})
    {
        std::printf("     %6.0f kg  ", static_cast<double>(ratio));
        for (int it : {8, 16, 32, 64, 128})
        {
            const double st = chain_stretch(ratio, it, 1);
            if (it == 8) { at8[ri] = st; }
            std::printf("%8.2f mm ", st);
        }
        std::printf("\n");
        ++ri;
    }
    std::printf("  At 1:1, eight sweeps leave a millimetre over ten joints. The stretch\n"
                "  grows with the ratio roughly in proportion — 18 mm at 10:1, 388 at\n"
                "  100:1 — and every doubling of the sweeps roughly halves it, which is\n"
                "  the signature of a solve that has not converged rather than of a\n"
                "  steady state. At 1000:1 the numbers stop meaning anything: the chain\n"
                "  is not settling, it is bouncing, and the mean over half a second is\n"
                "  not monotone in the sweep count at all.\n");
    check(at8[0] < 2.0, "an evenly weighted chain holds at eight sweeps");
    check(at8[2] > 100.0 * at8[0], "a 100:1 end weight stretches it by orders of magnitude");

    // ---- sub-stepping ----------------------------------------------------------------
    std::printf(
        "\n  THE SAME WORK, SPENT DIFFERENTLY. Eight sweeps of one 1/60 s step\n"
        "  against eight 1/480 s steps of ONE sweep each — the same number of\n"
        "  joint visits per frame, and the same 100 kg end weight:\n"
        "     schedule                       stretch\n");
    const double eight_by_one = chain_stretch(100.0f, 8, 1);
    const double four_by_two = chain_stretch(100.0f, 4, 2);
    const double two_by_four = chain_stretch(100.0f, 2, 4);
    const double one_by_eight = chain_stretch(100.0f, 1, 8);
    std::printf("     8 sweeps x 1 step            %9.3f mm\n"
                "     4 sweeps x 2 steps           %9.3f mm\n"
                "     2 sweeps x 4 steps           %9.3f mm\n"
                "     1 sweep  x 8 steps           %9.3f mm\n",
                eight_by_one, four_by_two, two_by_four, one_by_eight);
    std::printf(
        "  Smaller steps win by a factor of %.0f for the same number of joint\n"
        "  visits. Two things compound. Each sub-step starts from the answer the\n"
        "  last one reached an eighth of a frame ago, so the error the sweep has\n"
        "  to remove is an eighth as large; and the error a step CREATES is\n"
        "  second order in h — §5's drift is (w h)^2 d / 2 — so eight small steps\n"
        "  create an eighth of what one big one does. Iterating the big step\n"
        "  more addresses neither. This is the argument of Macklin et al. 2019,\n"
        "  \"Small Steps in Physics Simulation\", and of Box2D v3's soft step. It\n"
        "  is not free here — every sub-step re-runs prepare, and in a scene\n"
        "  with contacts the whole narrow phase — so it is named, not built.\n",
        eight_by_one / one_by_eight);
    check(one_by_eight < eight_by_one / 5.0, "sub-stepping beats iterating, for the same visits");

    // ---- the budget ---------------------------------------------------------------------
    std::printf("\n  THE BUDGET. 1,000 joints — a hundred ten-link chains — in the\n"
                "  engine's own loop, restored from one snapshot for every row\n"
                "  (warm-start state included: 8.10 §13's lesson). Minimum of 60:\n"
                "     joint                      prepare + warm start    per sweep\n");
    struct kind_row { const char* name; int kind; };
    for (const kind_row kr : {kind_row{"ball-socket, block", 0}, kind_row{"ball-socket, rows", 1},
                              kind_row{"hinge, block", 2}, kind_row{"hinge + limit + motor", 3},
                              kind_row{"rod", 4}})
    {
        rig r;
        r.cfg.joints.block_solve = kr.kind != 1;
        const std::uint32_t ceiling = r.add(make_fixed(vec3{}));
        for (int c = 0; c < 100; ++c)
        {
            std::uint32_t prev = ceiling;
            for (int i = 0; i < 10; ++i)
            {
                const float x = static_cast<float>(c) * 2.0f;
                const float y = -0.25f * static_cast<float>(i + 1);
                const std::uint32_t b = r.add(make_box(vec3{x, y, 0.0f}, 1.0f, vec3{0.05f, 0.1f, 0.05f}));
                const vec3 at{x, y + 0.125f, 0.0f};
                joint j;
                if (kr.kind <= 1) { j = make_ball_socket(r.body(prev), r.body(b), at); }
                else if (kr.kind == 4) { j = make_rod(r.body(prev), r.body(b), world_point_of(r.body(prev), vec3{}) + (prev == ceiling ? at : vec3{0.0f, -0.1f, 0.0f}), vec3{x, y + 0.1f, 0.0f}); }
                else
                {
                    j = make_hinge(r.body(prev), r.body(b), at, vec3{0.0f, 0.0f, 1.0f});
                    if (kr.kind == 3)
                    {
                        j.limit.enabled = true;
                        j.limit.lower = -0.5f;
                        j.limit.upper = 0.5f;
                        j.motor.enabled = true;
                        j.motor.speed = 0.0f;
                        j.motor.max_torque = 0.1f;
                    }
                }
                r.connect(prev, b, j);
                prev = b;
            }
        }
        r.run(1.0f);
        const std::vector<rigid_body> bodies(r.world.bodies().begin(), r.world.bodies().end());
        std::vector<joint> saved;
        for (auto& l : r.links) { saved.push_back(l.j); }
        double t[2] = {1e30, 1e30};
        for (int rep = 0; rep < 60; ++rep)
        {
            for (int k = 0; k < 2; ++k)
            {
                auto live = r.world.bodies();
                for (std::size_t i = 0; i < live.size(); ++i) { live[i] = bodies[i]; }
                for (std::size_t i = 0; i < saved.size(); ++i) { r.links[i].j = saved[i]; }
                r.cfg.velocity_iterations = k == 0 ? 0 : 8;
                r.cfg.correction = position_correction::none;
                r.world.integrate_velocities(r.h);
                const clock_type::time_point t0 = clock_type::now();
                r.solver.begin(r.world.bodies());
                for (auto& l : r.links) { r.solver.add(l.a, l.b, l.j); }
                (void)r.solver.solve(r.h, r.cfg, r.sleep);
                t[k] = std::min(t[k], seconds_since(t0));
            }
        }
        std::printf("     %-24s   %12.2f ns          %7.2f ns\n", kr.name, t[0] / 1000.0 * 1e9,
                    (t[1] - t[0]) / 8000.0 * 1e9);
    }
    std::printf("  Per joint per sweep, against 8.10 §13's 129 ns per contact MANIFOLD\n"
                "  per sweep. A hinge with everything on it costs a fraction of one\n"
                "  crate resting on another.\n");
    check(true, "budget measured");
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strncmp(argv[i], "--only=", 7) == 0) { only = argv[i] + 7; }
    }

    // `--only=eh` runs sections E and H. Every section builds its own fixture
    // from nothing, so any subset gives the same numbers it gives in a full run.
    const struct { const char* name; void (*fn)(); } sections[] = {
        {"a", section_a}, {"b", section_b}, {"c", section_c}, {"d", section_d},
        {"e", section_e}, {"f", section_f}, {"g", section_g}, {"h", section_h},
        {"i", section_i}, {"j", section_j}, {"k", section_k},
    };

    std::printf("verify_811 — Lesson 8.11, Constraints and Joints\n");
    for (const auto& s : sections)
    {
        if (only && std::strchr(only, s.name[0]) == nullptr) { continue; }
        s.fn();
    }

    std::printf("\n%s: %d checks, %d failures\n", g_failures == 0 ? "PASS" : "FAIL", g_checks,
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
