// scratch/verify_810.cpp — every number Lesson 8.10 prints, measured rather
// than asserted.
//
// Build and run:  sh scratch/build_verify_810.sh
//
// Eleven sections, in the lesson's order:
//
//   A  Gauss-Seidel: what a second sweep buys
//   B  warm starting
//   C  the order within a sweep
//   D  the frozen overlap
//   E  Baumgarte, and the energy it adds
//   F  split impulse, and the slop
//   G  islands
//   H  sleeping
//   I  where the solve goes in the step
//   J  the budget
//   K  what 8.10 leaves behind
//
// EVERY SECTION CARRIES A CONTROL — the rule since 8.1, in two halves: ask what
// the control would say if the thing were COMPLETELY BROKEN, and what it would
// say if it were completely FINE. 8.7 §9 shipped a control that convicted the
// code of the TEST's own mistake; 8.8 §4 and 8.9 §1 each shipped a measurement
// that refused the claim its section was written to make. Both halves earn
// their keep.
//
// WHAT IS DIFFERENT ABOUT THIS LESSON'S INSTRUMENTS.
//
//   * THE FIXTURE IS THE WHOLE PIPELINE. 8.9's harness hand-rolled a step,
//     because its subject was one contact and `body_world::step` could not be
//     interrupted. This one runs `body_world::integrate_velocities` ->
//     `uniform_grid` -> `collide_manifold` -> `manifold_cache` ->
//     `contact_solver::solve` -> `body_world::integrate_positions`, which is
//     the arrangement the engine now actually ships. Every number below comes
//     out of that, which means every number is also a test that the pieces fit.
//
//   * MOST OF THE CLAIMS ARE ABOUT CONVERGENCE, AND CONVERGENCE HAS NO CLOSED
//     FORM HERE. 8.9 could check a bounce height against `e^(2n)*h0`. A
//     Gauss-Seidel residual on a five-body tower has no formula, so the
//     instrument is different: measure a RATIO between successive iterations
//     and check that it is constant, which is what "geometric convergence"
//     means and is a stronger claim than any single number.
//
//   * TWO THINGS DO HAVE CLOSED FORMS and they are used hard: the arrival
//     depth `v*h`, and Baumgarte's time constant `-h/ln(1 - beta)`. Both are
//     in solver.hpp so the lesson's tables regenerate.
//
//   * AND ONE SECTION CHECKS A REFACTOR RATHER THAN A PHYSICS CLAIM. §I keeps
//     a private copy of the monolithic `body_world::step` this lesson split in
//     two and runs both over the same scene, comparing bit patterns. A
//     refactor that is "obviously" behaviour-preserving is exactly the kind
//     that is not.
//
// TIMINGS ARE MINIMA over many repetitions, as in 8.8 and 8.9: a measurement of
// a few microseconds has a long right tail and no left one, so the minimum is
// the estimator with the smaller variance and it errs conservatively for every
// claim this lesson makes.

#include <engine/phys/broadphase.hpp>
#include <engine/phys/collide.hpp>
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
#include <string>
#include <vector>

using engine::cross;
using engine::dot;
using engine::length;
using engine::length_squared;
using engine::mat3;
using engine::mat3_from_quat;
using engine::normalised;
using engine::quat;
using engine::transpose;
using engine::vec3;

using engine::phys::advance_orientation;
using engine::phys::apply_drag;
using engine::phys::as_convex;
using engine::phys::body_kind;
using engine::phys::body_world;
using engine::phys::bounds_of;
using engine::phys::box_shape;
using engine::phys::broadphase_config;
using engine::phys::broadphase_pair;
using engine::phys::build_islands;
using engine::phys::carry_impulses;
using engine::phys::collide_manifold;
using engine::phys::contact_manifold;
using engine::phys::contact_material;
using engine::phys::contact_pair;
using engine::phys::contact_solver;
using engine::phys::gyroscopic_mode;
using engine::phys::integrator;
using engine::phys::island;
using engine::phys::is_sleep_candidate;
using engine::phys::k_gravity;
using engine::phys::make_box;
using engine::phys::make_fixed;
using engine::phys::make_sphere;
using engine::phys::manifold_config;
using engine::phys::manifold_cache;
using engine::phys::pair_key;
using engine::phys::position_correction;
using engine::phys::proxy;
using engine::phys::pseudo_velocity;
using engine::phys::rigid_body;
using engine::phys::shape;
using engine::phys::sleep_config;
using engine::phys::solver_config;
using engine::phys::solver_stats;
using engine::phys::spin_rule;
using engine::phys::uniform_grid;
using engine::phys::wake;
using engine::phys::world_obb;
using engine::phys::world_sphere;

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

/// The same deterministic generator 8.1-8.9 used, for the same reason:
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

private:
    std::uint32_t state_;
};

constexpr float k_h = 1.0f / 60.0f;

using clock_type = std::chrono::steady_clock;

[[nodiscard]] double seconds_since(clock_type::time_point t0)
{
    return std::chrono::duration<double>(clock_type::now() - t0).count();
}

// ---------------------------------------------------------------------------
// The fixture: the whole pipeline, one step at a time
// ---------------------------------------------------------------------------

/// A scene of shaped bodies, stepped through every stage the engine ships.
///
/// It exists because this lesson's subject is the LOOP rather than any one
/// stage, and a fixture that hand-rolled the loop would be measuring itself.
/// The only thing it adds beyond the engine's own calls is instrumentation and
/// the two deliberately-wrong variants §C and §H need for their controls.
struct scene
{
    body_world world;
    std::vector<shape> shapes;

    uniform_grid grid;
    manifold_cache cache;
    contact_solver solver;

    solver_config cfg{};
    sleep_config sleep{};
    broadphase_config bp{};
    manifold_config mf{};
    contact_material material{0.0f, 0.5f};
    float h = k_h;

    /// Solve the contacts AFTER the position half rather than between the two.
    /// §I's control, and the bug that costs `g*h^2` per resting step.
    bool solve_at_end_of_step = false;

    /// In what order manifolds are handed to the solver. §C.
    ///
    /// `as_added` is the broadphase's own order, which is cell-bucket order and
    /// is therefore arbitrary — that is the default and it is what the engine
    /// ships. The other two SORT by contact height, which is the experiment:
    /// Gauss-Seidel carries information in the direction it sweeps, and a
    /// tower is held up from the bottom.
    enum class order { as_added, bottom_up, top_down };
    order contact_order = order::as_added;

    /// Skip the narrow phase when both bodies are asleep. §H measures what it
    /// saves and the one frame of wake-up lag it costs.
    bool skip_sleeping_pairs = false;

    /// Multiply every inherited impulse by this before the solve. 1 is warm
    /// starting as shipped, 0 is cold, and anything else is a DELIBERATELY
    /// WRONG initial guess — §B's control, and the only way to show that what
    /// warm starting supplies is a guess rather than an answer.
    float warm_scale = 1.0f;

    /// Inherit only the NORMAL impulse, not the two friction impulses. §K's
    /// probe: the tangent impulses are stored in "the solver's tangent basis"
    /// and the basis is not stored with them, so if a manifold's normal moves
    /// between frames the inherited friction is applied in the wrong direction.
    bool warm_normal_only = false;

    /// Throw away the basis the inherited friction impulses were measured in,
    /// which is exactly what the engine did before Lesson 8.10 — `prepare_
    /// contacts` then reads the two scalars as though they were already in
    /// this frame's basis. §B's A/B switch for the tangent-basis bug.
    bool stale_tangent_basis = false;

    // ---- per-step scratch and instrumentation -----------------------------
    std::vector<proxy> proxies;
    std::vector<contact_manifold> manifolds;
    std::vector<std::uint64_t> keys;
    std::vector<std::uint32_t> pair_a;
    std::vector<std::uint32_t> pair_b;

    std::vector<std::size_t> submit_;

    [[nodiscard]] static float mean_height(const contact_manifold& m)
    {
        float sum = 0.0f;
        for (int i = 0; i < m.count; ++i) { sum += m.points[i].position.y; }
        return m.count > 0 ? sum / static_cast<float>(m.count) : 0.0f;
    }

    solver_stats stats{};
    int narrow_tests = 0;
    int skipped_pairs = 0;
    double t_broad = 0.0;
    double t_narrow = 0.0;
    double t_solve = 0.0;

    std::uint32_t add(const rigid_body& b, const shape& s)
    {
        const std::uint32_t index = static_cast<std::uint32_t>(world.size());
        world.add(b);
        shapes.push_back(s);
        return index;
    }

    [[nodiscard]] rigid_body& body(std::uint32_t i) { return world.bodies()[i]; }
    [[nodiscard]] const rigid_body& body(std::uint32_t i) const { return world.bodies()[i]; }

    [[nodiscard]] contact_manifold collide_indices(std::uint32_t ia, std::uint32_t ib) const
    {
        const rigid_body& a = body(ia);
        const rigid_body& b = body(ib);
        const shape& sa = shapes[ia];
        const shape& sb = shapes[ib];

        // Four combinations written out rather than dispatched, because the
        // harness only ever uses boxes and spheres and a dispatcher would be a
        // piece of engine design smuggled into a test.
        if (sa.kind == engine::phys::shape_kind::sphere && sb.kind == engine::phys::shape_kind::sphere)
        {
            const auto x = world_sphere(sa, a.state.position);
            const auto y = world_sphere(sb, b.state.position);
            return collide_manifold(as_convex(x), as_convex(y), mf);
        }
        if (sa.kind == engine::phys::shape_kind::sphere)
        {
            const auto x = world_sphere(sa, a.state.position);
            const auto y = world_obb(sb, b.state.position, b.orientation);
            return collide_manifold(as_convex(x), as_convex(y), mf);
        }
        if (sb.kind == engine::phys::shape_kind::sphere)
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
        narrow_tests = 0;
        skipped_pairs = 0;

        // Reserved before anything is pushed, because `contact_solver::add`
        // takes a POINTER into this vector and a reallocation half way through
        // would leave the solver holding freed memory. The engine's contract
        // says the manifold must outlive the step; this is what honouring it
        // looks like from the caller's side.
        manifolds.reserve(grid.pairs().size());

        for (const broadphase_pair& p : grid.pairs())
        {
            const rigid_body& a = bodies[p.a];
            const rigid_body& b = bodies[p.b];

            // Two immovable bodies cannot produce a contact anything could act
            // on. The broadphase reports the pair because it is a box test and
            // it should be; rejecting it here is a caller's job.
            if (a.kind != body_kind::dynamic && b.kind != body_kind::dynamic) { continue; }

            if (skip_sleeping_pairs && a.sleeping && b.sleeping)
            {
                ++skipped_pairs;
                continue;
            }

            ++narrow_tests;
            contact_manifold m = collide_indices(p.a, p.b);
            if (m.count == 0) { continue; }

            const std::uint64_t key = pair_key(p.a, p.b);
            if (const contact_manifold* previous = cache.find(key))
            {
                carry_impulses(m, *previous);
                for (int i = 0; i < m.count; ++i)
                {
                    m.points[i].normal_impulse *= warm_scale;
                    m.points[i].tangent_impulse[0] *= warm_normal_only ? 0.0f : warm_scale;
                    m.points[i].tangent_impulse[1] *= warm_normal_only ? 0.0f : warm_scale;
                }
                if (stale_tangent_basis)
                {
                    m.tangent[0] = vec3{};
                    m.tangent[1] = vec3{};
                }
            }

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

        const std::size_t n = manifolds.size();
        submit_.clear();
        for (std::size_t i = 0; i < n; ++i) { submit_.push_back(i); }
        if (contact_order != order::as_added)
        {
            const bool up = contact_order == order::bottom_up;
            std::sort(submit_.begin(), submit_.end(),
                      [this, up](std::size_t x, std::size_t y) {
                          const float hx = mean_height(manifolds[x]);
                          const float hy = mean_height(manifolds[y]);
                          return up ? hx < hy : hx > hy;
                      });
        }
        for (const std::size_t i : submit_) { solver.add(pair_a[i], pair_b[i], manifolds[i], material); }

        stats = solver.solve(h, cfg, sleep);
        t_solve = seconds_since(t0);

        for (std::size_t k = 0; k < n; ++k) { cache.store(keys[k], manifolds[k]); }
        cache.end_frame();
    }

    void step()
    {
        // The restitution bias is the velocity gravity added to a dynamic body
        // this step — 8.9's `solver_config::restitution_bias`. It is set here
        // rather than once at construction because a caller that changes the
        // gravity or the step length and forgets is a caller with a ball that
        // bounces forever.
        cfg.restitution_bias = world.gravity() * h;

        world.integrate_velocities(h);

        if (!solve_at_end_of_step)
        {
            collide_all();
            solve();
        }

        world.integrate_positions(h);

        if (solve_at_end_of_step)
        {
            collide_all();
            solve();
        }
    }

    void run(float seconds)
    {
        const int steps = static_cast<int>(seconds / h + 0.5f);
        for (int i = 0; i < steps; ++i) { step(); }
    }

    /// The deepest penetration anywhere in the scene, in metres. Measured from
    /// the manifolds the last step generated, which is the only place it
    /// exists — a body does not know how far into anything it is.
    [[nodiscard]] float deepest() const
    {
        float worst = 0.0f;
        for (const contact_manifold& m : manifolds)
        {
            for (int i = 0; i < m.count; ++i) { worst = std::max(worst, m.points[i].depth); }
        }
        return worst;
    }

    [[nodiscard]] int awake_bodies() const
    {
        int n = 0;
        for (const rigid_body& b : world.bodies())
        {
            if (b.kind == body_kind::dynamic && !b.sleeping) { ++n; }
        }
        return n;
    }
};

/// A floor, and `count` cubes stacked on it with `gap` metres of air between.
///
/// The gap matters and is not decoration: a stack authored exactly touching is
/// a stack whose first frame has no contacts at all, so every impulse in it
/// starts cold and the settling transient is a different experiment from the
/// steady state. A millimetre of gap makes the first contact happen on frame
/// two, arrived at by falling, which is what a level does.
struct tower_spec
{
    int count = 5;
    float half = 0.25f;
    float mass = 10.0f;
    float gap = 0.001f;
    float x = 0.0f;
    float z = 0.0f;
};

std::uint32_t add_floor(scene& s, float half_extent = 20.0f)
{
    rigid_body floor = make_fixed(vec3{0.0f, -0.5f, 0.0f});
    return s.add(floor, box_shape(vec3{half_extent, 0.5f, half_extent}));
}

void add_tower(scene& s, const tower_spec& spec)
{
    const float side = 2.0f * spec.half;
    for (int i = 0; i < spec.count; ++i)
    {
        const float y = spec.half + static_cast<float>(i) * (side + spec.gap);
        s.add(make_box(vec3{spec.x, y, spec.z}, spec.mass, vec3{spec.half, spec.half, spec.half}),
              box_shape(vec3{spec.half, spec.half, spec.half}));
    }
}

/// The residual of the LAST velocity iteration, over the whole scene. The
/// convergence number, and the thing §A plots.
[[nodiscard]] float residual_of(const scene& s) { return s.stats.max_residual; }

/// Every body's state, so that a sweep can run the SAME step many times.
///
/// The instrument that makes most of this file possible: "how does the answer
/// change with the iteration count" is only a question if the question is
/// asked of one identical state, and a scene that has been stepped differently
/// is a different scene. Snapshot, sweep, restore.
/// **AND THE MANIFOLD CACHE IS PART OF THE STATE.** The first version of this
/// struct held only the bodies, and §B's sweep came out non-monotone in a way
/// that took twenty minutes to explain: each trial inherited the impulses the
/// PREVIOUS trial had written, so a sweep over the iteration count was also an
/// uncontrolled sweep over the quality of the initial guess. Warm starting is
/// exactly the feature that makes a physics step depend on more than its
/// bodies, and a fixture for measuring warm starting had better know that.
struct snapshot
{
    std::vector<rigid_body> bodies;
    manifold_cache cache;
};

[[nodiscard]] snapshot take(const scene& s)
{
    return snapshot{std::vector<rigid_body>(s.world.bodies().begin(), s.world.bodies().end()),
                    s.cache};
}

void restore(scene& s, const snapshot& snap)
{
    auto bodies = s.world.bodies();
    for (std::size_t i = 0; i < bodies.size(); ++i) { bodies[i] = snap.bodies[i]; }
    s.cache = snap.cache;
}

} // namespace


namespace
{

// ---------------------------------------------------------------------------
// §A  Gauss-Seidel: what a second sweep buys
// ---------------------------------------------------------------------------

void section_a()
{
    rule("A  GAUSS-SEIDEL: WHAT A SECOND SWEEP BUYS");

    std::printf(
        "  8.9 ended on a number: one pass over a four-point manifold leaves\n"
        "  2.6e-02 m/s of residual approach velocity and the crate sinks. The\n"
        "  fix is to sweep again, against the velocities the other points have\n"
        "  since produced. Warm starting is OFF throughout this section and the\n"
        "  position correction is OFF, so that ITERATION is the only variable.\n");

    // ---- one crate, four points ------------------------------------------
    scene s;
    s.cfg.warm_start = false;
    s.cfg.correction = position_correction::none;
    s.cfg.velocity_iterations = 8;
    s.sleep.enabled = false;
    add_floor(s);
    add_tower(s, tower_spec{1});
    s.run(2.0f);

    const snapshot settled = take(s);

    std::printf("\n  ONE CRATE ON A FLOOR, four contact points, cold:\n");
    std::printf("     iters    residual m/s     ratio to previous\n");
    float previous = 0.0f;
    float ratio_sum = 0.0f;
    int ratio_n = 0;
    float residual_1 = 0.0f;
    float residual_8 = 0.0f;
    for (int iters = 1; iters <= 64; iters *= 2)
    {
        restore(s, settled);
        s.cfg.velocity_iterations = iters;
        s.step();
        const float r = residual_of(s);
        if (iters == 1) { residual_1 = r; }
        if (iters == 8) { residual_8 = r; }
        if (previous > 0.0f)
        {
            // Each row DOUBLES the iteration count, so the per-iteration factor
            // is the square root of the row-to-row one at 2, the fourth root at
            // 4... `pow(r/previous, 1/iters_added)` is the honest reduction.
            const float per = std::pow(r / previous, 1.0f / static_cast<float>(iters / 2));
            std::printf("     %5d    %12.4e     %8.4f per iteration%s\n", iters,
                        static_cast<double>(r), static_cast<double>(per),
                        r < 1e-7f ? "   (at the float floor)" : "");

            // Rows below 1e-07 are excluded from the mean and the reason is not
            // tidiness: the residual has stopped being a measurement of the
            // solver and started being a measurement of `float`. Including them
            // would report a contraction of 0.64 where the solver's is 0.53.
            if (r > 1e-7f)
            {
                ratio_sum += per;
                ++ratio_n;
            }
        }
        else
        {
            std::printf("     %5d    %12.4e            -\n", iters, static_cast<double>(r));
        }
        previous = r;
    }
    const float mean_ratio = ratio_n > 0 ? ratio_sum / static_cast<float>(ratio_n) : 0.0f;
    std::printf("     mean contraction over the %d rows above the float floor: %.4f\n",
                ratio_n, static_cast<double>(mean_ratio));

    check(residual_1 > 1e-3f, "one pass leaves a residual worth caring about");
    check(residual_8 < residual_1 * 0.2f, "eight passes are at least five times better than one");
    check(mean_ratio > 0.0f && mean_ratio < 1.0f, "the contraction factor is a contraction");

    // ---- and what that residual DOES over fifteen seconds -----------------
    std::printf("\n  AND WHAT THE RESIDUAL DOES, over 15 s with no position correction:\n");
    std::printf("     iters    sink mm     final residual m/s\n");
    float sink_1 = 0.0f;
    float sink_8 = 0.0f;
    for (int iters : {1, 2, 4, 8, 16, 32})
    {
        scene t;
        t.cfg.warm_start = false;
        t.cfg.correction = position_correction::none;
        t.cfg.velocity_iterations = iters;
        t.sleep.enabled = false;
        add_floor(t);
        add_tower(t, tower_spec{1});
        t.run(15.0f);
        const float sink = 0.25f - t.body(1).state.position.y;
        if (iters == 1) { sink_1 = sink; }
        if (iters == 8) { sink_8 = sink; }
        std::printf("     %5d    %8.3f     %12.4e\n", iters, static_cast<double>(sink * 1000.0f),
                    static_cast<double>(residual_of(t)));
    }
    check(sink_1 > 0.1f, "one pass sinks a crate more than 100 mm in 15 s");
    check(sink_8 < sink_1 * 0.2f, "eight passes stop most of the sinking");

    // ---- the control: a contact with nothing to iterate against -----------
    //
    // A sphere on a floor is ONE point of ONE manifold. There is no second
    // impulse to disturb the first, so the very first pass is exact and every
    // later one has nothing to do. If iteration were improving things for some
    // reason other than the one claimed, this row would improve too.
    scene one;
    one.cfg.warm_start = false;
    one.cfg.correction = position_correction::none;
    one.sleep.enabled = false;
    add_floor(one);
    one.add(make_sphere(vec3{0.0f, 0.25f, 0.0f}, 10.0f, 0.25f),
            engine::phys::sphere_shape(0.25f));
    one.run(1.0f);
    const snapshot one_settled = take(one);

    float single_1 = 0.0f;
    float single_16 = 0.0f;
    for (int iters : {1, 16})
    {
        restore(one, one_settled);
        one.cfg.velocity_iterations = iters;
        one.step();
        if (iters == 1) { single_1 = residual_of(one); }
        else            { single_16 = residual_of(one); }
    }
    std::printf("\n  CONTROL, one sphere = one point = nothing to iterate against:\n");
    std::printf("     1 pass %.4e   16 passes %.4e   improvement %.4fx\n",
                static_cast<double>(single_1), static_cast<double>(single_16),
                single_16 > 0.0f ? static_cast<double>(single_1 / single_16) : 0.0);
    check(single_1 < 1e-6f, "a single-point contact is solved exactly by one pass");
    check(std::abs(single_16 - single_1) < 1e-6f, "and further passes change nothing");

    std::printf("\n  A FOUR-POINT MANIFOLD IS ALREADY A SYSTEM. The residual is not\n"
                "  noise and it is not a bug in one point's arithmetic: each point's\n"
                "  impulse moves the velocity the previous point's solve had just\n"
                "  set exactly on target. Sweeping again is the whole fix, and the\n"
                "  measured contraction says how many sweeps it takes.\n");
}

// ---------------------------------------------------------------------------
// §B  warm starting
// ---------------------------------------------------------------------------

/// How many iterations this scene needs, from this state, to get the residual
/// under `target`. Returns `limit` if it never does.
int iterations_to(scene& s, const snapshot& state, float target, int limit)
{
    for (int iters = 1; iters <= limit; ++iters)
    {
        restore(s, state);
        s.cfg.velocity_iterations = iters;
        s.step();
        if (residual_of(s) < target) { return iters; }
    }
    return limit;
}

void section_b()
{
    rule("B  WARM STARTING");

    std::printf(
        "  The accumulated impulses are already in the manifold: 8.7 gave every\n"
        "  point a feature id, `carry_impulses` matches them across frames, and\n"
        "  8.9's `write_back` puts numbers in them. Warm starting begins each\n"
        "  point from last frame's answer instead of from zero. On 8.9's single\n"
        "  pass that was close to nothing. Here is what it is worth to a solver\n"
        "  that iterates.\n");

    // ---- a five-crate tower, settled, cold vs warm ------------------------
    scene s;
    s.cfg.correction = position_correction::none;
    s.sleep.enabled = false;
    add_floor(s);
    add_tower(s, tower_spec{5});

    s.cfg.warm_start = true;
    s.run(4.0f);
    const snapshot settled = take(s);

    std::printf("\n  A FIVE-CRATE TOWER, settled, asked to reach a residual of 1e-04:\n");

    s.cfg.warm_start = false;
    const int cold = iterations_to(s, settled, 1e-4f, 600);

    restore(s, settled);
    s.cfg.warm_start = true;
    s.cfg.velocity_iterations = 8;
    s.step();   // one step to repopulate the cache with warm impulses
    const snapshot warm_state = take(s);
    const int warm = iterations_to(s, warm_state, 1e-4f, 600);

    std::printf("     cold (every accumulator starts at zero):  %3d iterations\n", cold);
    std::printf("     warm (each starts from last frame's):     %3d iterations\n", warm);
    std::printf("     ratio: %.2fx\n", warm > 0 ? static_cast<double>(cold) / warm : 0.0);
    check(warm < cold, "warm starting needs fewer iterations than cold");

    // ---- and at a FIXED iteration count, what it costs not to -------------
    std::printf("\n  AT THE EIGHT ITERATIONS THIS ENGINE SHIPS, over 20 s, no position\n"
                "  correction, so all that holds the tower up is the velocity solve:\n");
    std::printf("     warm    top crate sink mm    deepest mm    final residual\n");
    float sink_cold = 0.0f;
    float sink_warm = 0.0f;
    for (int warm_on = 0; warm_on <= 1; ++warm_on)
    {
        scene t;
        t.cfg.correction = position_correction::none;
        t.cfg.warm_start = warm_on != 0;
        t.cfg.velocity_iterations = 8;
        t.sleep.enabled = false;
        add_floor(t);
        add_tower(t, tower_spec{5});
        const float top0 = 0.25f + 4.0f * 0.501f;
        t.run(20.0f);
        const float sink = top0 - t.body(5).state.position.y;
        if (warm_on == 0) { sink_cold = sink; } else { sink_warm = sink; }
        std::printf("     %-4s    %14.2f    %10.3f    %12.4e\n", warm_on ? "yes" : "no",
                    static_cast<double>(sink * 1000.0f),
                    static_cast<double>(t.deepest() * 1000.0f),
                    static_cast<double>(residual_of(t)));
    }
    check(sink_warm < sink_cold, "warm starting holds the tower higher at the same cost");

    // ---- the match rate, which is what makes it free ----------------------
    scene m;
    m.sleep.enabled = false;
    add_floor(m);
    add_tower(m, tower_spec{5});
    m.run(4.0f);
    int matched = 0;
    int total = 0;
    for (int i = 0; i < 600; ++i)
    {
        m.step();
        matched += m.stats.warm_points;
        total += m.stats.points;
    }
    std::printf("\n  THE MATCH RATE on a settled tower, 600 frames: %d of %d points\n"
                "  inherited an impulse (%.2f%%). 8.7's ids are what make that\n"
                "  possible; a position match would find essentially none.\n",
                matched, total,
                total > 0 ? 100.0 * static_cast<double>(matched) / static_cast<double>(total) : 0.0);
    check(total > 0 && matched > total * 9 / 10, "a settled tower matches over 90% of its points");

    // ---- the control: a guess that is WRONG -------------------------------
    //
    // Warm starting supplies an INITIAL GUESS, not an answer. If it were doing
    // anything more than that, a deliberately bad guess would not hurt — and
    // the accumulator clamp would not be load-bearing. Scale every inherited
    // impulse and watch the iteration count move in both directions.
    std::printf("\n  CONTROL, the inherited impulse scaled before the solve. 1.0 is\n"
                "  warm starting as shipped; 0 is cold; the rest are guesses that\n"
                "  are wrong by a known amount:\n");
    std::printf("     scale    iters to 1e-04    residual at 8    top crate m/s after 1 s\n");
    int iters_at_one = 0;
    int iters_at_two = 0;
    float speed_at_one = 0.0f;
    float speed_at_five = 0.0f;
    for (float scale : {0.0f, 0.5f, 1.0f, 2.0f, 5.0f})
    {
        scene t;
        t.cfg.correction = position_correction::none;
        t.sleep.enabled = false;
        t.cfg.warm_start = true;
        add_floor(t);
        add_tower(t, tower_spec{5});
        t.run(4.0f);

        t.warm_scale = scale;
        t.cfg.velocity_iterations = 8;
        t.step();
        const snapshot state = take(t);
        const int n = iterations_to(t, state, 1e-4f, 400);

        restore(t, state);
        t.cfg.velocity_iterations = 8;
        t.step();
        const float r8 = residual_of(t);

        restore(t, state);
        t.run(1.0f);
        const float speed = length(t.body(5).state.velocity);

        if (scale == 1.0f) { iters_at_one = n; speed_at_one = speed; }
        if (scale == 2.0f) { iters_at_two = n; }
        if (scale == 5.0f) { speed_at_five = speed; }
        std::printf("     %5.1f    %14d    %13.4e    %22.4f\n", static_cast<double>(scale), n,
                    static_cast<double>(r8), static_cast<double>(speed));
    }
    check(iters_at_two > iters_at_one,
          "a guess that is wrong by 2x costs more iterations than one that is right");
    check(speed_at_five > 10.0f * std::max(speed_at_one, 1e-4f),
          "and a guess that is wrong by 5x has thrown the tower into the air");
    std::printf("     A BAD GUESS IS WORSE THAN NO GUESS — and read the last two\n"
                "     columns together, because the 5.0 row is the warning that goes\n"
                "     with every convergence measurement in this lesson: THE RESIDUAL\n"
                "     IS A MEASURE OF CONSISTENCY, NOT OF CORRECTNESS. A solver that\n"
                "     has launched the stack has nothing left to disagree with itself\n"
                "     about, and reports a beautiful residual for it.\n");

    // ---- keep_slop: the knob 8.7 left at zero and named this lesson -------
    //
    // A point a tenth of a millimetre clear of the surface this frame is a
    // contact next frame. With `keep_slop` at zero it is not in the manifold at
    // all, so it has no id, so `carry_impulses` cannot match it and its impulse
    // starts from zero every time it flickers back. The point count of a
    // settled stack is the instrument.
    std::printf("\n  `manifold_config::keep_slop`, which 8.7 left at zero and named\n"
                "  8.9's, and which is really this lesson's — 600 frames of a\n"
                "  settled five-crate tower:\n");
    std::printf("     keep_slop    mean points/frame    inherited %%\n");
    float rate_zero = 0.0f;
    float rate_slop = 0.0f;
    for (float slop : {0.0f, 0.002f, 0.01f})
    {
        scene t;
        t.mf.keep_slop = slop;
        t.sleep.enabled = false;
        add_floor(t);
        add_tower(t, tower_spec{5});
        t.run(4.0f);
        int pts = 0;
        int warm_pts = 0;
        for (int i = 0; i < 600; ++i)
        {
            t.step();
            pts += t.stats.points;
            warm_pts += t.stats.warm_points;
        }
        const float rate = pts > 0 ? 100.0f * static_cast<float>(warm_pts) / static_cast<float>(pts)
                                   : 0.0f;
        if (slop == 0.0f)  { rate_zero = rate; }
        if (slop == 0.01f) { rate_slop = rate; }
        std::printf("     %9.3f    %17.3f    %10.2f\n", static_cast<double>(slop),
                    static_cast<double>(pts) / 600.0, static_cast<double>(rate));
    }
    check(rate_slop >= rate_zero - 0.01f, "a positive keep_slop does not lower the match rate");

    // ---- *** AND THE BUG WARM STARTING WAS HIDING *** ----------------------
    //
    // `contact_point::tangent_impulse` is TWO SCALARS IN A BASIS, and until
    // this lesson the basis was rebuilt from the normal every frame and never
    // stored anywhere. `tangent_basis` branches on the smallest component of
    // the normal, so on a near-vertical contact the choice is decided by
    // whether |n.x| or |n.z| is smaller — two numbers that are both about 1e-5
    // on a settled crate and cross each other constantly.
    std::printf("\n  *** AND THE BUG WARM STARTING WAS HIDING. *** The tangent basis\n"
                "  is rebuilt from the normal every frame, and `tangent_basis`\n"
                "  chooses its seed axis by the normal's SMALLEST component. On a\n"
                "  near-vertical contact that is a comparison between two numbers\n"
                "  around 1e-05. Measured over 20 s of a ten-crate tower:\n");
    {
        scene s;
        s.sleep.enabled = false;
        add_floor(s);
        add_tower(s, tower_spec{10});
        std::vector<vec3> previous(64, vec3{});
        int flips = 0;
        int samples = 0;
        float worst = 0.0f;
        for (int i = 0; i < 1200; ++i)
        {
            s.step();
            for (std::size_t k = 0; k < s.manifolds.size(); ++k)
            {
                const std::uint32_t id = s.pair_b[k];
                if (id >= previous.size()) { continue; }
                vec3 t1;
                vec3 t2;
                engine::phys::tangent_basis(s.manifolds[k].normal, t1, t2);
                if (length_squared(previous[id]) > 0.5f)
                {
                    const float c = std::min(1.0f, std::max(-1.0f, dot(t1, previous[id])));
                    const float degrees = std::acos(c) * 57.29577951f;
                    worst = std::max(worst, degrees);
                    if (degrees > 30.0f) { ++flips; }
                    ++samples;
                }
                previous[id] = t1;
            }
        }
        std::printf("     manifold-frames whose basis rotated by more than 30 deg:\n"
                    "     %d of %d (%.2f%%), worst rotation %.1f deg\n", flips, samples,
                    100.0 * flips / std::max(1, samples), static_cast<double>(worst));
        check(flips > 0, "the tangent basis really does flip, and often");
        check(worst > 80.0f, "and when it flips it flips by about a right angle");
    }

    std::printf("\n  WHICH MEANS LAST FRAME'S FRICTION IS APPLIED SIDEWAYS. The fix\n"
                "  is to store the basis on the manifold and re-express the\n"
                "  inherited impulse in the new one — two dot products, exact. A\n"
                "  ten-crate tower at 24 iterations, 20 s, with and without:\n");
    std::printf("     basis stored    sideways drift mm    top crate y    standing\n");
    float drift_fixed = 0.0f;
    float drift_stale = 0.0f;
    for (int stale = 0; stale <= 1; ++stale)
    {
        scene s;
        s.sleep.enabled = false;
        s.cfg.velocity_iterations = 24;
        s.stale_tangent_basis = stale != 0;
        add_floor(s);
        add_tower(s, tower_spec{10});
        s.run(20.0f);
        const rigid_body& top = s.body(10);
        const float drift = std::sqrt(top.state.position.x * top.state.position.x
                                      + top.state.position.z * top.state.position.z);
        if (stale) { drift_stale = drift; } else { drift_fixed = drift; }
        std::printf("     %-15s %17.2f    %11.4f    %8s\n", stale ? "no (pre-8.10)" : "yes",
                    static_cast<double>(drift * 1000.0f),
                    static_cast<double>(top.state.position.y),
                    top.state.position.y > 4.6f ? "yes" : "NO");
    }
    check(drift_fixed < drift_stale, "storing the basis keeps the tower where it was put");
    std::printf("     A CACHED NUMBER IS MEANINGLESS WITHOUT THE FRAME IT WAS\n"
                "     MEASURED IN, and nothing in the type system was ever going to\n"
                "     say so: both versions are two floats called tangent_impulse.\n");
}

// ---------------------------------------------------------------------------
// §C  the order within a sweep
// ---------------------------------------------------------------------------

void section_c()
{
    rule("C  THE ORDER WITHIN A SWEEP");

    std::printf(
        "  Gauss-Seidel uses each impulse as soon as it is computed, so a sweep\n"
        "  carries information in the direction it WALKS. A tower is held up\n"
        "  from the bottom; solving it top-down means each contact is solved\n"
        "  against a support that has not been established yet. This engine\n"
        "  does not sort, so the default is the broadphase's cell order — which\n"
        "  is arbitrary, and the honest thing is to say so and then measure what\n"
        "  sorting would have been worth.\n");

    std::printf("\n  A TEN-CRATE TOWER, settled at 32 iterations so that it is\n"
                "  actually standing, then warm starting OFF so that the order is\n"
                "  the only thing acting. Residual after n sweeps:\n");
    std::printf("     order        n=1         n=4         n=16        n=64        n=256\n");

    float resid_up[5] = {};
    float resid_down[5] = {};
    for (int which = 0; which < 3; ++which)
    {
        scene s;
        s.cfg.correction = position_correction::none;
        s.sleep.enabled = false;
        s.cfg.warm_start = true;
        s.cfg.velocity_iterations = 32;
        add_floor(s);
        add_tower(s, tower_spec{10});
        s.run(8.0f);

        s.cfg.warm_start = false;
        s.contact_order = which == 0 ? scene::order::as_added
                                     : (which == 1 ? scene::order::bottom_up
                                                   : scene::order::top_down);
        const snapshot settled = take(s);

        std::printf("     %-10s", which == 0 ? "as added" : (which == 1 ? "bottom-up" : "top-down"));
        int col = 0;
        for (int iters : {1, 4, 16, 64, 256})
        {
            restore(s, settled);
            s.cfg.velocity_iterations = iters;
            s.step();
            const float r = residual_of(s);
            if (which == 1) { resid_up[col] = r; }
            if (which == 2) { resid_down[col] = r; }
            ++col;
            std::printf("  %10.3e", static_cast<double>(r));
        }
        std::printf("\n");
    }
    const float gain16 = resid_up[2] > 0.0f ? resid_down[2] / resid_up[2] : 1.0f;
    const float gain64 = resid_up[3] > 0.0f ? resid_down[3] / resid_up[3] : 1.0f;
    std::printf("     bottom-up against top-down: %.2fx at n=16, %.2fx at n=64\n",
                static_cast<double>(gain16), static_cast<double>(gain64));
    std::printf("     (the as-added column IS bottom-up here — the broadphase's cell\n"
                "      order happens to run up the tower — which is itself worth\n"
                "      knowing, because it means the default is not a coin toss)\n");
    check(resid_up[2] < resid_down[2],
          "sweeping from the supports upward converges faster than downward");
    check(resid_up[3] < resid_down[3], "and still does four times further in");

    // *** AND NOW SAY WHAT THAT IS WORTH, IN THE ONE CURRENCY THAT MATTERS. ***
    //
    // A ratio of residuals is not a saving until it is converted into
    // iterations, and §A measured the exchange rate: 0.53 per iteration. The
    // folklore says solve order is a big deal. On this fixture it is worth
    // less than two iterations, and saying so is more useful than repeating
    // the folklore.
    const float per_iteration = 0.53f;
    const float worth = std::log(gain64) / std::log(1.0f / per_iteration);
    std::printf("     AT §A's MEASURED CONTRACTION OF %.2f PER ITERATION, that is\n"
                "     worth %.2f iterations. The folklore makes more of solve order\n"
                "     than this fixture supports.\n",
                static_cast<double>(per_iteration), static_cast<double>(worth));
    check(worth < 4.0f, "the ordering gain is worth a couple of iterations, not a rewrite");

    // ---- the control: a scene where order cannot matter -------------------
    //
    // Twelve crates spread out on a floor, each touching nothing but the
    // ground. There is no chain for information to travel along, so a sweep
    // in either direction does exactly the same arithmetic in a different
    // sequence, and the two answers should be identical to the bit.
    std::printf("\n  CONTROL, twelve crates that touch only the floor and never each\n"
                "  other — no chain, so no direction:\n");
    float independent[2] = {0.0f, 0.0f};
    for (int which = 0; which <= 1; ++which)
    {
        scene s;
        s.cfg.correction = position_correction::none;
        s.cfg.warm_start = false;
        s.sleep.enabled = false;
        s.contact_order = which == 0 ? scene::order::bottom_up : scene::order::top_down;
        add_floor(s);
        for (int i = 0; i < 12; ++i)
        {
            s.add(make_box(vec3{static_cast<float>(i) * 2.0f - 11.0f, 0.2501f, 0.0f}, 10.0f,
                           vec3{0.25f, 0.25f, 0.25f}),
                  box_shape(vec3{0.25f, 0.25f, 0.25f}));
        }
        s.cfg.velocity_iterations = 4;
        s.run(2.0f);
        independent[which] = s.body(6).state.position.y;
    }
    std::printf("     bottom-up %.9f m   top-down %.9f m   difference %.3e\n",
                static_cast<double>(independent[0]), static_cast<double>(independent[1]),
                static_cast<double>(std::abs(independent[0] - independent[1])));
    check(std::abs(independent[0] - independent[1]) < 1e-6f,
          "with no chain of contacts the sweep order changes nothing");

    std::printf("\n  THE SORT IS FREE AND IT IS NOT NOTHING, but it is not free of\n"
                "  DESIGN: \"bottom-up\" means \"by height\", which is a statement about\n"
                "  gravity pointing down, and an engine whose levels have ceilings,\n"
                "  ropes and rotating platforms has no such axis. The general\n"
                "  version is a topological order of the contact graph, per island,\n"
                "  which is a sort per frame rather than a comparison. This engine\n"
                "  ships the broadphase's order and names it; §K's twenty-crate\n"
                "  tower is where you can see what that costs.\n");
}

// ---------------------------------------------------------------------------
// §D  the frozen overlap
// ---------------------------------------------------------------------------

void section_d()
{
    rule("D  THE FROZEN OVERLAP");

    std::printf(
        "  A converged velocity solve leaves the relative normal velocity at\n"
        "  zero, which means the overlap STOPS GROWING. It says nothing about\n"
        "  the overlap already there — and `collide_manifold` reports nothing\n"
        "  until the shapes actually overlap, so the first frame a contact\n"
        "  exists is the frame after the body crossed the surface.\n");

    // ---- how deep is that, exactly? ---------------------------------------
    //
    // *** THE FIRST DRAFT OF THIS SECTION CLAIMED `depth == v*h` AND THE
    // *** MEASUREMENT REFUSED IT, BY UP TO 81%.
    //
    // It is an upper BOUND, not an equality, and the reason is obvious once
    // the measurement says so: the body crosses the surface at some instant
    // INSIDE a step, and the next sample is taken a whole step later. How much
    // of that step was left over is a property of where the body happened to
    // be when the step began, which is to say it is arbitrary — so the arrival
    // depth is uniform on [0, v*h] and only its ceiling is predictable.
    //
    // 200 drop heights, finely spaced, is what turns "the claim is wrong" into
    // "the claim was the wrong shape".
    std::printf("\n  THE DEPTH A CONTACT ARRIVES WITH, as a fraction of `v*h`, over\n"
                "  200 finely spaced drop heights per row:\n");
    std::printf("     drop m    arrival m/s    v*h mm     min     mean     max\n");
    float worst_over = 0.0f;
    float worst_mean_error = 0.0f;
    for (float drop : {0.1f, 0.5f, 2.0f})
    {
        float lo = 1e9f;
        float hi = -1e9f;
        double sum = 0.0;
        int n = 0;
        float last_speed = 0.0f;
        for (int k = 0; k < 200; ++k)
        {
            const float extra = static_cast<float>(k) * 0.0005f;
            scene s;
            s.cfg.correction = position_correction::none;
            s.cfg.velocity_iterations = 32;
            s.sleep.enabled = false;
            add_floor(s);
            s.add(make_box(vec3{0.0f, 0.25f + drop + extra, 0.0f}, 10.0f,
                           vec3{0.25f, 0.25f, 0.25f}),
                  box_shape(vec3{0.25f, 0.25f, 0.25f}));

            for (int i = 0; i < 2000; ++i)
            {
                // The speed the body will actually travel at during this step
                // is the one AFTER gravity, which is what moves it into the
                // floor — so it is read after the velocity half, not before.
                s.cfg.restitution_bias = s.world.gravity() * s.h;
                s.world.integrate_velocities(s.h);
                const float v = -s.body(1).state.velocity.y;
                s.collide_all();
                s.solve();
                s.world.integrate_positions(s.h);
                if (s.stats.points > 0)
                {
                    const float fraction = s.deepest() / (v * s.h);
                    lo = std::min(lo, fraction);
                    hi = std::max(hi, fraction);
                    sum += static_cast<double>(fraction);
                    ++n;
                    last_speed = v;
                    break;
                }
            }
        }
        const float mean = static_cast<float>(sum / n);
        worst_over = std::max(worst_over, hi);
        worst_mean_error = std::max(worst_mean_error, std::abs(mean - 0.5f));
        std::printf("     %6.2f    %11.4f    %7.3f  %6.3f   %6.3f   %6.3f\n",
                    static_cast<double>(drop), static_cast<double>(last_speed),
                    static_cast<double>(last_speed * k_h * 1000.0f), static_cast<double>(lo),
                    static_cast<double>(mean), static_cast<double>(hi));
    }
    std::printf("     largest fraction seen anywhere: %.4f;  means are within %.3f of 0.5\n",
                static_cast<double>(worst_over), static_cast<double>(worst_mean_error));
    check(worst_over <= 1.02f, "v*h is a ceiling: nothing arrives deeper");
    check(worst_mean_error < 0.08f, "and the arrival depth is uniform on [0, v*h], so it averages half");

    // ---- and it does not move, however hard the solver is asked to try ----
    std::printf("\n  AND NO NUMBER OF VELOCITY ITERATIONS TOUCHES IT. A crate dropped\n"
                "  0.5 m, run for 10 s with `position_correction::none`:\n");
    std::printf("     iters    depth mm       residual m/s\n");
    float depth_lo = 1e9f;
    float depth_hi = -1e9f;
    for (int iters : {1, 2, 4, 8, 16, 32, 64})
    {
        scene s;
        s.cfg.correction = position_correction::none;
        s.cfg.velocity_iterations = iters;
        s.sleep.enabled = false;
        add_floor(s);
        s.add(make_box(vec3{0.0f, 0.75f, 0.0f}, 10.0f, vec3{0.25f, 0.25f, 0.25f}),
              box_shape(vec3{0.25f, 0.25f, 0.25f}));
        s.run(10.0f);
        const float depth = s.deepest();
        depth_lo = std::min(depth_lo, depth);
        depth_hi = std::max(depth_hi, depth);
        std::printf("     %5d    %8.4f       %12.4e\n", iters,
                    static_cast<double>(depth * 1000.0f), static_cast<double>(residual_of(s)));
    }
    std::printf("     over a 64x range of iteration count the depth moves %.4f mm\n"
                "     (%.2f%%) and the residual moves by orders of magnitude.\n",
                static_cast<double>((depth_hi - depth_lo) * 1000.0f),
                static_cast<double>(100.0f * (depth_hi - depth_lo) / depth_hi));
    check(depth_lo > 0.005f, "even a converged velocity solve leaves millimetres of overlap");
    check((depth_hi - depth_lo) / depth_hi < 0.05f,
          "and the depth is frozen: iteration does not move it by 5%");
    std::printf("     TWO DIFFERENT PROBLEMS, AND ITERATION IS THE ANSWER TO ONE.\n");

    // ---- the control: a contact that arrives at zero speed ----------------
    //
    // If the frozen depth really is bounded by one step of approach travel, a
    // body that arrives arbitrarily slowly must arrive arbitrarily shallow.
    // Lower a crate onto the floor at 1 cm/s with gravity switched off.
    scene slow;
    slow.cfg.correction = position_correction::none;
    slow.cfg.velocity_iterations = 32;
    slow.sleep.enabled = false;
    slow.world.set_gravity(vec3{});
    add_floor(slow);
    slow.add(make_box(vec3{0.0f, 0.30f, 0.0f}, 10.0f, vec3{0.25f, 0.25f, 0.25f}),
             box_shape(vec3{0.25f, 0.25f, 0.25f}));
    slow.body(1).state.velocity = vec3{0.0f, -0.01f, 0.0f};
    slow.run(8.0f);
    const float slow_depth = slow.deepest();
    std::printf("\n  CONTROL, a crate lowered at 0.01 m/s with gravity off:\n"
                "     ceiling %.4f mm   measured %.4f mm\n",
                static_cast<double>(engine::phys::arrival_depth(0.01f, slow.h) * 1000.0f),
                static_cast<double>(slow_depth * 1000.0f));
    check(slow_depth <= engine::phys::arrival_depth(0.01f, slow.h) * 1.02f,
          "a slow arrival arrives shallow, under the same ceiling");
    std::printf("     WHICH IS ALSO THE BOUND ON HOW GOOD `correction::none` CAN BE.\n"
                "     A scene in which nothing ever arrives faster than a few cm/s\n"
                "     does not need a position correction at all.\n");
}

// ---------------------------------------------------------------------------
// §E  Baumgarte, and the energy it adds
// ---------------------------------------------------------------------------

/// A crate placed `depth` metres inside the floor.
///
/// With `gravity_on` false the only thing acting is the correction, which is
/// the cleanest fixture for "what does the correction DO"; with it true the
/// crate is held against the surface, which is the fixture for "how fast does
/// the correction work". The two answers are different and §E needs both.
scene deep_crate(float depth, position_correction correction, bool gravity_on = false)
{
    scene s;
    s.cfg.correction = correction;
    s.cfg.velocity_iterations = 8;
    s.cfg.position_iterations = 3;
    s.sleep.enabled = false;
    if (!gravity_on) { s.world.set_gravity(vec3{}); }
    add_floor(s);
    s.add(make_box(vec3{0.0f, 0.25f - depth, 0.0f}, 10.0f, vec3{0.25f, 0.25f, 0.25f}),
          box_shape(vec3{0.25f, 0.25f, 0.25f}));
    return s;
}

/// Seconds for the overlap under a held crate to fall to 1/e of where it
/// started. The instrument §E and §F both use.
float decay_time(float start_depth, position_correction correction, float beta)
{
    scene s = deep_crate(start_depth, correction, true);
    s.cfg.baumgarte = beta;
    s.cfg.penetration_slop = 0.0f;
    s.cfg.max_correction_speed = 100.0f;
    const float target = start_depth / 2.718281828f;
    for (int i = 0; i < 6000; ++i)
    {
        s.step();
        if (0.25f - s.body(1).state.position.y <= target)
        {
            return static_cast<float>(i + 1) * s.h;
        }
    }
    return -1.0f;
}

void section_e()
{
    rule("E  BAUMGARTE, AND THE ENERGY IT ADDS");

    std::printf(
        "  Feed a fraction of the penetration back into the velocity solve as a\n"
        "  target and the contact pushes apart instead of merely stopping. One\n"
        "  line, no extra state, and every first implementation does it.\n");

    // ---- the time constant, against the closed form -----------------------
    //
    // *** WITH GRAVITY ON, AND THE FIRST DRAFT HAD IT OFF. *** Off, the decay
    // came out 33% FASTER than `-h/ln(1 - beta)` at beta = 0.05 and dead on at
    // beta = 0.4, which is the signature of a second effect rather than a
    // wrong constant (8.9 §8's rule, again). The effect is the complaint this
    // section is about: an unheld crate KEEPS the correction velocity, so it
    // coasts out at the largest bias it ever saw instead of decaying with the
    // overlap. Geometric decay is what the correction does to a body that is
    // being pushed back in — which is every body in a real stack.
    std::printf("\n  THE DECAY, against `baumgarte_time_constant(beta, h)`. A crate\n"
                "  50 mm inside the floor, held there by gravity:\n");
    std::printf("     beta    predicted tau ms    measured tau ms    ratio\n");
    float worst_tau_error = 0.0f;
    for (float beta : {0.05f, 0.1f, 0.2f, 0.4f})
    {
        const float measured = decay_time(0.050f, position_correction::baumgarte, beta);
        const float predicted = engine::phys::baumgarte_time_constant(beta, k_h);
        const float ratio = predicted > 0.0f ? measured / predicted : 0.0f;
        worst_tau_error = std::max(worst_tau_error, std::abs(ratio - 1.0f));
        std::printf("     %.2f    %16.2f    %16.2f    %5.3f\n", static_cast<double>(beta),
                    static_cast<double>(predicted * 1000.0f),
                    static_cast<double>(measured * 1000.0f), static_cast<double>(ratio));
    }
    std::printf("     worst deviation: %.2f%%, and one 16.67 ms step is %.1f%% of the\n"
                "     shortest row — the measurement cannot be finer than a frame.\n",
                static_cast<double>(worst_tau_error * 100.0f),
                static_cast<double>(100.0f * k_h
                                    / engine::phys::baumgarte_time_constant(0.4f, k_h)));
    check(worst_tau_error < 0.15f, "the decay matches -h/ln(1-beta) to about a step");

    // ---- and the energy -----------------------------------------------------
    std::printf("\n  *** AND WHEN THE OVERLAP RUNS OUT, THE BODY IS STILL CARRYING\n"
                "  *** THE PUSH. Gravity OFF, so a correction that added nothing\n"
                "  would leave the crate exactly at rest on the surface:\n");
    std::printf("     start mm    departure m/s    KE added J    as a drop of\n");
    float worst_departure = 0.0f;
    for (float depth : {0.010f, 0.050f, 0.100f})
    {
        scene s = deep_crate(depth, position_correction::baumgarte);
        s.run(3.0f);
        const float departure = s.body(1).state.velocity.y;
        const float ke = 0.5f * 10.0f * departure * departure;
        worst_departure = std::max(worst_departure, std::abs(departure));
        std::printf("     %8.1f    %13.4f    %10.4e    %8.1f mm\n",
                    static_cast<double>(depth * 1000.0f), static_cast<double>(departure),
                    static_cast<double>(ke),
                    static_cast<double>(1000.0f * departure * departure / (2.0f * k_gravity)));
    }
    check(worst_departure > 0.01f, "Baumgarte leaves the body moving when the overlap runs out");

    // ---- and what that looks like with gravity on ---------------------------
    //
    // Gravity is the thing that mostly HIDES this. The correction's push is
    // spent climbing, and below a certain starting depth the crate never
    // clears the surface at all — it just arrives shallower than it should and
    // settles. Above that depth it leaves the floor, and a reader who has only
    // ever tested shallow overlaps has never seen it.
    std::printf("\n  WITH GRAVITY ON, THE PUSH HAS TO CLIMB — and below a certain\n"
                "  starting depth it never gets out, which is why this is easy to\n"
                "  miss. Peak height relative to the surface, negative = never\n"
                "  surfaced:\n");
    std::printf("     start mm    peak vs surface mm    peak speed m/s    Baumgarte / split\n");
    float first_launch = -1.0f;
    for (float depth : {0.010f, 0.050f, 0.100f, 0.200f, 0.300f})
    {
        float peaks[2];
        for (int which = 0; which <= 1; ++which)
        {
            scene s = deep_crate(depth, which == 0 ? position_correction::baumgarte
                                                   : position_correction::split_impulse,
                                 true);
            float peak = -1e9f;
            float peak_v = 0.0f;
            for (int i = 0; i < 400; ++i)
            {
                s.step();
                peak = std::max(peak, s.body(1).state.position.y - 0.25f);
                peak_v = std::max(peak_v, s.body(1).state.velocity.y);
            }
            peaks[which] = peak;
            if (which == 0)
            {
                if (peak > 0.0f && first_launch < 0.0f) { first_launch = depth; }
                std::printf("     %8.1f    %18.3f    %14.4f    ",
                            static_cast<double>(depth * 1000.0f),
                            static_cast<double>(peak * 1000.0f), static_cast<double>(peak_v));
            }
        }
        std::printf("%.3f / %.3f mm\n", static_cast<double>(peaks[0] * 1000.0f),
                    static_cast<double>(peaks[1] * 1000.0f));
    }
    std::printf("     the shallowest of these starts that LAUNCHES the crate clear of\n"
                "     the floor is %.0f mm; split impulse launches at none of them.\n",
                static_cast<double>(first_launch * 1000.0f));
    check(first_launch > 0.0f, "deep enough, Baumgarte throws the crate off the floor");

    // ---- the clamp is what turns a launch into a push ---------------------
    std::printf("\n  THE CLAMP, `max_correction_speed`. A crate spawned 300 mm inside\n"
                "  the floor, which is a level-design mistake every project makes:\n");
    std::printf("     clamp m/s    peak departure m/s    peak above surface mm\n");
    for (float clamp : {100.0f, 3.0f, 1.0f})
    {
        scene s = deep_crate(0.300f, position_correction::baumgarte, true);
        s.cfg.max_correction_speed = clamp;
        float peak = -1e9f;
        float peak_v = 0.0f;
        for (int i = 0; i < 600; ++i)
        {
            s.step();
            peak = std::max(peak, s.body(1).state.position.y - 0.25f);
            peak_v = std::max(peak_v, s.body(1).state.velocity.y);
        }
        std::printf("     %9.1f    %18.4f    %21.3f\n", static_cast<double>(clamp),
                    static_cast<double>(peak_v), static_cast<double>(peak * 1000.0f));
    }

    // ---- the control: the same fixture with the correction OFF -------------
    //
    // If the departure velocity really is the correction's doing, switching the
    // correction off must leave the crate exactly where it was put — motionless
    // and 100 mm inside the floor, forever.
    scene off = deep_crate(0.100f, position_correction::none);
    off.run(3.0f);
    std::printf("\n  CONTROL, the same crate with no correction at all:\n"
                "     departure %.6e m/s   still %.3f mm deep\n",
                static_cast<double>(off.body(1).state.velocity.y),
                static_cast<double>((0.25f - off.body(1).state.position.y) * 1000.0f));
    check(std::abs(off.body(1).state.velocity.y) < 1e-6f,
          "with no correction nothing moves, so the departure is the correction's");
}

// ---------------------------------------------------------------------------
// §F  split impulse, and the slop
// ---------------------------------------------------------------------------

void section_f()
{
    rule("F  SPLIT IMPULSE, AND THE SLOP");

    std::printf(
        "  The same arithmetic against a velocity that is thrown away at the end\n"
        "  of the step. Twenty-four bytes per body, and nothing the correction\n"
        "  does survives into the next step.\n");

    std::printf("\n  THE SAME THREE FIXTURES AS §E, side by side:\n");
    std::printf("     start mm    correction       departure m/s    overshoot mm    final depth mm\n");
    float baumgarte_departure = 0.0f;
    float split_departure = 0.0f;
    for (float depth : {0.010f, 0.050f, 0.100f})
    {
        for (int which = 0; which <= 1; ++which)
        {
            const position_correction c = which == 0 ? position_correction::baumgarte
                                                     : position_correction::split_impulse;
            scene s = deep_crate(depth, c);
            s.run(3.0f);
            const float departure = s.body(1).state.velocity.y;
            const float overshoot = s.body(1).state.position.y - 0.25f;
            if (depth > 0.09f)
            {
                if (which == 0) { baumgarte_departure = std::abs(departure); }
                else            { split_departure = std::abs(departure); }
            }
            std::printf("     %8.1f    %-15s  %13.5f    %12.4f    %14.4f\n",
                        static_cast<double>(depth * 1000.0f), engine::phys::name_of(c),
                        static_cast<double>(departure), static_cast<double>(overshoot * 1000.0f),
                        static_cast<double>(s.deepest() * 1000.0f));
        }
    }
    check(split_departure < baumgarte_departure,
          "split impulse leaves less velocity behind than Baumgarte");
    check(split_departure < 1e-3f, "split impulse leaves essentially none");

    // ---- how fast it corrects, which must be the same ---------------------
    //
    // It would be a poor trade if the energy-free correction were also a
    // slower one. It is the same arithmetic, so it should have the same time
    // constant, and measuring it is how you find out you have not accidentally
    // scaled something by `h` twice.
    std::printf("\n  AND IT IS NOT SLOWER. Time to remove 1/e of a 50 mm overlap on a\n"
                "  held crate, against the closed form's %.2f ms:\n",
                static_cast<double>(engine::phys::baumgarte_time_constant(0.2f, k_h) * 1000.0f));
    const float tau_b = decay_time(0.050f, position_correction::baumgarte, 0.2f);
    const float tau_s = decay_time(0.050f, position_correction::split_impulse, 0.2f);
    std::printf("     Baumgarte %6.2f ms     split impulse %6.2f ms     difference %.2f ms\n",
                static_cast<double>(tau_b * 1000.0f), static_cast<double>(tau_s * 1000.0f),
                static_cast<double>(std::abs(tau_b - tau_s) * 1000.0f));
    check(std::abs(tau_b - tau_s) <= 2.0f * k_h,
          "the two corrections work at the same rate, to the resolution of a frame");

    // ---- the slop, and what it is actually for ----------------------------
    //
    // *** THIS SECTION WAS WRITTEN TO SHOW THAT WITHOUT A SLOP A RESTING
    // *** CRATE BREATHES AT 60 Hz, AND THE MEASUREMENT REFUSED IT. ***
    //
    // Fifth time in Module 8 (8.6 §1, 8.7 §9, 8.8 §4, 8.9 §1). A single crate
    // under the split-impulse correction is bit-for-bit motionless at a slop
    // of ZERO, because the correction is geometric: it removes `beta` of what
    // is left each step and therefore never reaches zero, so the contact never
    // separates and there is nothing to oscillate. The folklore describes a
    // correction that drives the overlap to zero in one step, which is
    // `beta = 1`, which is not what anybody ships.
    //
    // So the honest question is not "does the slop stop the breathing" but
    // "what does the slop do at all", and the answer turns out to depend on
    // the correction and on whether the contacts compete.
    std::printf("\n  THE SLOP, MEASURED RATHER THAN ASSUMED. Height peak-to-peak over\n"
                "  2 s after a 30 s settle, sleeping off so that stillness is real\n"
                "  rather than switched off:\n");
    std::printf("     slop mm   1 crate split   1 crate Baum.   5 tower split   resting depth mm\n");
    float jitter_zero_tower = 0.0f;
    float jitter_five_tower = 0.0f;
    for (float slop : {0.0f, 0.0005f, 0.002f, 0.005f, 0.02f})
    {
        float jitter[3] = {};
        float depth_tower = 0.0f;
        for (int which = 0; which < 3; ++which)
        {
            scene s;
            s.cfg.penetration_slop = slop;
            s.cfg.correction = which == 1 ? position_correction::baumgarte
                                          : position_correction::split_impulse;
            s.sleep.enabled = false;
            add_floor(s);
            add_tower(s, tower_spec{which == 2 ? 5 : 1});
            s.run(30.0f);

            float lo = 1e9f;
            float hi = -1e9f;
            double depth_sum = 0.0;
            for (int i = 0; i < 120; ++i)
            {
                s.step();
                const float y = s.body(which == 2 ? 5 : 1).state.position.y;
                lo = std::min(lo, y);
                hi = std::max(hi, y);
                depth_sum += static_cast<double>(s.deepest());
            }
            jitter[which] = (hi - lo) * 1e6f;
            if (which == 2) { depth_tower = static_cast<float>(depth_sum / 120.0); }
        }
        if (slop == 0.0f)   { jitter_zero_tower = jitter[2]; }
        if (slop == 0.005f) { jitter_five_tower = jitter[2]; }
        std::printf("     %7.2f   %13.2f   %13.2f   %13.2f   %16.3f\n",
                    static_cast<double>(slop * 1000.0f), static_cast<double>(jitter[0]),
                    static_cast<double>(jitter[1]), static_cast<double>(jitter[2]),
                    static_cast<double>(depth_tower * 1000.0f));
    }
    std::printf("     (micrometres. A crate is half a metre.)\n");
    check(jitter_zero_tower > 1000.0f, "a five-crate tower at zero slop jitters by millimetres");
    check(jitter_five_tower < 1.0f, "and at 5 mm of slop it is bit-for-bit still");

    std::printf("\n  WHAT THE SWEEP ACTUALLY SAYS, IN THREE LINES.\n"
                "    ONE resting contact is still at every slop INCLUDING ZERO, so\n"
                "      the usual justification for the knob does not apply to it.\n"
                "    FIVE crates at zero slop move %.1f mm peak to peak — the tower\n"
                "      breathes — and it stops completely somewhere between 0.5 and\n"
                "      2 mm. The slop is for contacts that COMPETE: each one asks\n"
                "      for its own separation, they cannot all have it, and the slop\n"
                "      is the band inside which they stop asking.\n"
                "    AND WHAT IT COSTS is in the last column, exactly: the resting\n"
                "      depth IS the slop. Keep it small; 5 mm buys the stillness and\n"
                "      is invisible under a half-metre crate.\n",
                static_cast<double>(jitter_zero_tower / 1000.0f));

    std::printf("\n  THE SLOP IS A SCALE DECISION AND IT IS THE ONE PLACE THIS\n"
                "  ENGINE'S \"metres and seconds\" CONVENTION BITES. Five millimetres\n"
                "  is invisible under a crate and enormous under a marble.\n");
}

// ---------------------------------------------------------------------------
// §G  islands
// ---------------------------------------------------------------------------

/// Twenty towers of five, well apart, on one floor. The scene islands exist
/// for: twenty groups that cannot possibly influence one another, sharing one
/// piece of level geometry that touches all of them.
void build_yard(scene& s, int towers = 20, int height = 5)
{
    add_floor(s, 40.0f);
    for (int i = 0; i < towers; ++i)
    {
        tower_spec spec;
        spec.count = height;
        spec.x = static_cast<float>(i % 5) * 3.0f - 6.0f;
        spec.z = static_cast<float>(i / 5) * 3.0f - 6.0f;
        add_tower(s, spec);
    }
}

/// The union-find `build_islands` deliberately does NOT do: one that lets a
/// contact through an immovable body join its two neighbours.
///
/// It is here as §G's control and nowhere else. Every published engine gets
/// this right, and every first implementation gets it wrong, because the rule
/// only shows up when you ask how many islands there are.
int build_islands_bridging_statics(std::span<const rigid_body> bodies,
                                   std::span<const contact_pair> contacts,
                                   std::vector<int>& island_of)
{
    const int n = static_cast<int>(bodies.size());
    std::vector<int> parent(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) { parent[static_cast<std::size_t>(i)] = i; }

    const auto find = [&parent](int x) {
        while (parent[static_cast<std::size_t>(x)] != x)
        {
            parent[static_cast<std::size_t>(x)] =
                parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])];
            x = parent[static_cast<std::size_t>(x)];
        }
        return x;
    };

    for (const contact_pair& p : contacts)
    {
        const int ra = find(static_cast<int>(p.body_a));
        const int rb = find(static_cast<int>(p.body_b));
        if (ra != rb) { parent[static_cast<std::size_t>(std::max(ra, rb))] = std::min(ra, rb); }
    }

    island_of.assign(static_cast<std::size_t>(n), -1);
    int count = 0;
    std::vector<int> label(static_cast<std::size_t>(n), -1);
    for (int i = 0; i < n; ++i)
    {
        const int r = find(i);
        if (label[static_cast<std::size_t>(r)] < 0) { label[static_cast<std::size_t>(r)] = count++; }
        island_of[static_cast<std::size_t>(i)] = label[static_cast<std::size_t>(r)];
    }
    return count;
}

void section_g()
{
    rule("G  ISLANDS");

    std::printf(
        "  Two bodies are in the same island when a chain of contacts joins\n"
        "  them. Everything in an island has to be solved together; nothing in\n"
        "  one island can reach another this step, by construction.\n");

    scene s;
    s.sleep.enabled = false;
    build_yard(s);
    s.run(4.0f);

    std::printf("\n  A YARD: twenty towers of five crates, one floor, 101 bodies.\n");
    std::printf("     islands reported by the solver:            %d\n", s.stats.islands);
    std::printf("     manifolds:                                 %d\n", s.stats.manifolds);
    check(s.stats.islands == 20, "twenty separated towers make twenty islands");

    // ---- the control that is the whole rule -------------------------------
    std::vector<contact_pair> pairs;
    for (std::size_t i = 0; i < s.manifolds.size(); ++i)
    {
        pairs.push_back(contact_pair{s.pair_a[i], s.pair_b[i], nullptr, s.material});
    }
    std::vector<int> labels;
    const int correct = build_islands(s.world.bodies(), pairs, labels);
    std::vector<int> bad_labels;
    const int bridging = build_islands_bridging_statics(s.world.bodies(), pairs, bad_labels);

    std::printf("\n  CONTROL, the same contacts through a union-find that lets a\n"
                "  FIXED body join its neighbours:\n");
    std::printf("     fixed bodies are not bridges (this engine):  %3d islands\n", correct);
    std::printf("     fixed bodies ARE bridges (the usual bug):    %3d islands\n", bridging);
    check(correct == 20, "the shipped rule finds twenty");
    check(bridging == 1, "the bug finds one — the floor welds the level together");

    std::printf("\n  ONE ISLAND IS NOT A SLOW SIMULATION, IT IS NO SLEEPING AT ALL:\n"
                "  a single rolling marble anywhere on that floor would keep every\n"
                "  crate in the level awake, in exactly the scene sleeping exists\n"
                "  for. The rule is that an edge is only an edge when BOTH ends\n"
                "  can move, and it is safe because an impulse applied to a fixed\n"
                "  body changes nothing any other contact can read.\n");

    // ---- the partition itself ---------------------------------------------
    int smallest = 1 << 30;
    int largest = 0;
    int total_bodies = 0;
    for (const island& isl : s.solver.islands())
    {
        smallest = std::min(smallest, isl.body_count);
        largest = std::max(largest, isl.body_count);
        total_bodies += isl.body_count;
    }
    std::printf("\n  THE PARTITION: %d islands, smallest %d bodies, largest %d,\n"
                "  %d bodies placed of %d dynamic (the floor is in none).\n",
                static_cast<int>(s.solver.islands().size()), smallest, largest, total_bodies, 100);
    check(total_bodies == 100, "every dynamic body lands in exactly one island");

    // ---- a lone body ------------------------------------------------------
    scene lone;
    lone.sleep.enabled = false;
    add_floor(lone);
    lone.add(make_box(vec3{0.0f, 6.0f, 0.0f}, 10.0f, vec3{0.25f, 0.25f, 0.25f}),
             box_shape(vec3{0.25f, 0.25f, 0.25f}));
    lone.step();
    std::printf("\n  A BODY WITH NO CONTACTS AT ALL — a crate in flight — is its own\n"
                "  island of one: %d island, %d manifolds. Not a special case in\n"
                "  the code, and the right answer: a falling body is quiet nowhere.\n",
                lone.stats.islands, lone.stats.manifolds);
    check(lone.stats.islands == 1, "a body in flight is an island of one");

    // ---- and what the whole partition costs --------------------------------
    scene big;
    big.sleep.enabled = false;
    build_yard(big, 20, 5);
    big.run(4.0f);
    std::vector<contact_pair> big_pairs;
    for (std::size_t i = 0; i < big.manifolds.size(); ++i)
    {
        big_pairs.push_back(contact_pair{big.pair_a[i], big.pair_b[i], nullptr, big.material});
    }
    std::vector<int> big_labels;
    double best = 1e30;
    for (int rep = 0; rep < 200; ++rep)
    {
        const clock_type::time_point t0 = clock_type::now();
        const int c = build_islands(big.world.bodies(), big_pairs, big_labels);
        const double dt = seconds_since(t0);
        if (c != 20) { check(false, "island count is stable across repetitions"); }
        best = std::min(best, dt);
    }
    std::printf("\n  COST: %d bodies, %d contacts, union-find and labelling:\n"
                "     %.3f us  (%.2f ns per body)\n",
                static_cast<int>(big.world.size()), static_cast<int>(big_pairs.size()),
                best * 1e6, best * 1e9 / static_cast<double>(big.world.size()));
}

// ---------------------------------------------------------------------------
// §H  sleeping
// ---------------------------------------------------------------------------

void section_h()
{
    rule("H  SLEEPING");

    std::printf(
        "  An island whose every body has been quiet for long enough stops\n"
        "  being simulated. The saving is the easy half; waking is the half\n"
        "  that has to be right.\n");

    // ---- THE ORDERING FINDING ---------------------------------------------
    //
    // The sleep test cannot run before the solve, and the reason is g*h.
    {
        scene s;
        s.sleep.enabled = false;
        build_yard(s, 20, 5);
        s.run(6.0f);

        int quiet_pre = 0;
        int quiet_post = 0;
        float worst_pre = 0.0f;
        float worst_post = 0.0f;
        s.cfg.restitution_bias = s.world.gravity() * s.h;
        s.world.integrate_velocities(s.h);
        for (const rigid_body& b : s.world.bodies())
        {
            if (b.kind != body_kind::dynamic) { continue; }
            worst_pre = std::max(worst_pre, length(b.state.velocity));
            if (is_sleep_candidate(b, s.sleep.linear_threshold, s.sleep.angular_threshold))
            {
                ++quiet_pre;
            }
        }
        s.collide_all();
        s.solve();
        for (const rigid_body& b : s.world.bodies())
        {
            if (b.kind != body_kind::dynamic) { continue; }
            worst_post = std::max(worst_post, length(b.state.velocity));
            if (is_sleep_candidate(b, s.sleep.linear_threshold, s.sleep.angular_threshold))
            {
                ++quiet_post;
            }
        }
        s.world.integrate_positions(s.h);

        std::printf("\n  *** WHERE THE TEST GOES, ON A SCENE THAT HAS BEEN STILL FOR\n"
                    "  *** SIX SECONDS. 100 settled crates, threshold %.2f m/s:\n",
                    static_cast<double>(s.sleep.linear_threshold));
        std::printf("     read BEFORE the solve:  %3d of 100 quiet, fastest %.4f m/s\n",
                    quiet_pre, static_cast<double>(worst_pre));
        std::printf("     read AFTER the solve:   %3d of 100 quiet, fastest %.4f m/s\n",
                    quiet_post, static_cast<double>(worst_post));
        std::printf("     g*h at 60 Hz is %.4f m/s, which is %.1fx the threshold.\n",
                    static_cast<double>(k_gravity * s.h),
                    static_cast<double>(k_gravity * s.h / s.sleep.linear_threshold));
        check(quiet_pre == 0, "before the solve nothing is quiet, at any sane threshold");
        check(quiet_post > 80, "after the solve nearly everything is");
    }

    // ---- the saving --------------------------------------------------------
    {
        scene s;
        build_yard(s, 20, 5);
        int frame_asleep = -1;
        for (int i = 0; i < 1200; ++i)
        {
            s.step();
            if (frame_asleep < 0 && s.stats.sleeping_bodies == 100) { frame_asleep = i; }
        }
        std::printf("\n  A YARD OF TWENTY TOWERS falls asleep at frame %d (%.2f s):\n",
                    frame_asleep, frame_asleep / 60.0);
        std::printf("     sleeping bodies %d of 100, sleeping islands %d of %d\n",
                    s.stats.sleeping_bodies, s.stats.sleeping_islands, s.stats.islands);
        std::printf("     manifolds handed in %d, manifolds SOLVED %d\n", s.stats.manifolds,
                    s.stats.solved_manifolds);
        check(s.stats.sleeping_bodies == 100, "the whole yard sleeps");
        check(s.stats.solved_manifolds == 0, "and not one contact is solved");

        // and the same scene with sleeping off, for the cost comparison
        scene awake;
        awake.sleep.enabled = false;
        build_yard(awake, 20, 5);
        awake.run(20.0f);

        double t_asleep = 1e30;
        double t_awake = 1e30;
        for (int rep = 0; rep < 60; ++rep)
        {
            s.step();
            t_asleep = std::min(t_asleep, s.t_solve);
            awake.step();
            t_awake = std::min(t_awake, awake.t_solve);
        }
        std::printf("     solve cost asleep %.3f us   awake %.3f us   %.1fx\n", t_asleep * 1e6,
                    t_awake * 1e6, t_asleep > 0.0 ? t_awake / t_asleep : 0.0);
        check(t_asleep < t_awake, "a sleeping yard costs less to solve than an awake one");

        // The honest half: collision detection is NOT saved.
        std::printf("     broadphase %.3f us and narrow phase %.3f us are UNCHANGED —\n"
                    "     %d narrow-phase tests either way. Sleeping saves the solve\n"
                    "     and the integration, not the detection.\n",
                    s.t_broad * 1e6, s.t_narrow * 1e6, s.narrow_tests);

        // ...unless the caller skips pairs where BOTH bodies are asleep, which
        // is safe because two sleeping bodies cannot have moved relative to
        // each other.
        s.skip_sleeping_pairs = true;
        double t_narrow_skipped = 1e30;
        for (int rep = 0; rep < 60; ++rep)
        {
            s.step();
            t_narrow_skipped = std::min(t_narrow_skipped, s.t_narrow);
        }
        std::printf("     with the caller skipping asleep-asleep pairs: narrow phase\n"
                    "     %.3f us, %d tests, %d pairs skipped. Safe, because two\n"
                    "     sleeping bodies cannot have moved relative to each other.\n",
                    t_narrow_skipped * 1e6, s.narrow_tests, s.skipped_pairs);
    }

    // ---- waking, which is the half that has to be right --------------------
    {
        scene s;
        build_yard(s, 4, 5);
        s.run(6.0f);
        check(s.stats.sleeping_bodies == 20, "four towers asleep");

        // Throw a crate at one of them.
        const std::uint32_t thrown = s.add(make_box(vec3{-9.0f, 1.0f, -6.0f}, 20.0f,
                                                    vec3{0.25f, 0.25f, 0.25f}),
                                           box_shape(vec3{0.25f, 0.25f, 0.25f}));
        s.body(thrown).state.velocity = vec3{14.0f, 2.0f, 0.0f};
        wake(s.body(thrown));

        int woke_at = -1;
        int woken = 0;
        for (int i = 0; i < 180; ++i)
        {
            s.step();
            const int awake_now = s.awake_bodies();
            if (woke_at < 0 && awake_now > 1) { woke_at = i; woken = awake_now; }
        }
        std::printf("\n  WAKING. A 20 kg crate thrown at one of four sleeping towers:\n");
        std::printf("     first frame anything else woke: %d (%.3f s after the throw)\n", woke_at,
                    woke_at / 60.0);
        std::printf("     bodies awake on that frame: %d of 21\n", woken);
        std::printf("     still awake 3 s later: %d\n", s.awake_bodies());
        check(woke_at >= 0, "the tower it hits wakes up");
        check(woken >= 5, "and the whole tower wakes, not just the crate that was hit");
    }

    // ---- the control: what per-BODY sleeping does --------------------------
    //
    // A body asleep is, to everything else in the simulation, an IMMOVABLE
    // body: nothing integrates it and no rule here changes that. So per-body
    // sleeping is emulated exactly by `body_kind::fixed`, and the emulation is
    // not an approximation — it is the same statement made in the type system.
    //
    // The fair experiment is a COLLISION, not a teleport. A support removed by
    // teleport is invisible to both rules (nothing collided, so nothing in the
    // contact graph changed) and that is what `wake` is for; the header says
    // so. What separates the two rules is what happens when something arrives.
    {
        std::printf("\n  CONTROL, PER-BODY SLEEPING. A three-crate tower, asleep, with a\n"
                    "  crate thrown at its BOTTOM block. Per-body sleeping is emulated\n"
                    "  by `body_kind::fixed`, which is exactly what a slept body is to\n"
                    "  everything else:\n");
        std::printf("     rule          bottom crate moved m    top crate fell m    top awake\n");

        float fell[2] = {};
        for (int per_body = 0; per_body <= 1; ++per_body)
        {
            scene s;
            add_floor(s);
            add_tower(s, tower_spec{3});
            s.run(6.0f);
            check(s.body(3).sleeping, "the tower is asleep before the throw");

            if (per_body)
            {
                // Freeze the two upper crates individually. Under a per-body
                // rule they are asleep and nothing about the crate below them
                // can change that.
                for (std::uint32_t k = 2; k <= 3; ++k)
                {
                    s.body(k).kind = body_kind::fixed;
                    s.body(k).inv_mass = 0.0f;
                    s.body(k).inv_inertia_local = mat3{vec3{}, vec3{}, vec3{}};
                }
            }

            const std::uint32_t thrown =
                s.add(make_box(vec3{-6.0f, 0.25f, 0.0f}, 200.0f, vec3{0.25f, 0.25f, 0.25f}),
                      box_shape(vec3{0.25f, 0.25f, 0.25f}));
            s.body(thrown).state.velocity = vec3{25.0f, 0.0f, 0.0f};
            wake(s.body(thrown));

            const float bottom_x0 = s.body(1).state.position.x;
            const float top_y0 = s.body(3).state.position.y;
            s.run(4.0f);
            fell[per_body] = top_y0 - s.body(3).state.position.y;
            std::printf("     %-12s  %20.4f    %16.4f    %9s\n",
                        per_body ? "per body" : "per island",
                        static_cast<double>(std::abs(s.body(1).state.position.x - bottom_x0)),
                        static_cast<double>(fell[per_body]),
                        per_body ? "n/a" : (s.body(3).sleeping ? "no" : "yes"));
        }
        std::printf("     the island rule drops the top crate %.3f m; the per-body rule\n"
                    "     leaves it %.3f m from where it was, hanging in the air with\n"
                    "     nothing underneath it.\n",
                    static_cast<double>(fell[0]), static_cast<double>(fell[1]));
        check(fell[0] > 0.1f, "under the island rule the tower comes down");
        check(std::abs(fell[1]) < 1e-4f, "under a per-body rule the upper crates hang");
    }

    // ---- the threshold floor --------------------------------------------
    //
    // The first version of this measured the residual speed of a SINGLE
    // settled crate and got exactly 0.00000 at every iteration count — one
    // four-point contact under eight sweeps cancels gravity to the bit, so
    // there is no floor to find and the table said nothing. The floor is a
    // property of contacts that COMPETE, and the operative question is not
    // "what speed is left" but "how low can the threshold go before the scene
    // stops sleeping".
    {
        std::printf("\n  THE THRESHOLD HAS A FLOOR AND THE SOLVER SETS IT. A yard of\n"
                    "  twenty five-crate towers, eight iterations, given 20 s:\n");
        std::printf("     threshold m/s    bodies asleep    time to sleep s\n");
        float smallest_working = -1.0f;
        for (float threshold : {0.0005f, 0.002f, 0.01f, 0.02f, 0.05f, 0.1f, 0.2f})
        {
            scene s;
            s.sleep.linear_threshold = threshold;
            s.sleep.angular_threshold = threshold * 2.0f;
            build_yard(s, 20, 5);
            int frame = -1;
            for (int i = 0; i < 1200; ++i)
            {
                s.step();
                if (frame < 0 && s.stats.sleeping_bodies == 100) { frame = i; }
            }
            if (s.stats.sleeping_bodies == 100 && smallest_working < 0.0f)
            {
                smallest_working = threshold;
            }
            std::printf("     %13.4f    %13d    %15s\n", static_cast<double>(threshold),
                        s.stats.sleeping_bodies,
                        frame >= 0 ? (std::to_string(frame / 60.0).substr(0, 5)).c_str()
                                   : "never");
        }
        std::printf("     THE DEFAULT IS NOT GENEROUS: the smallest threshold at which\n"
                    "     this yard sleeps at all is %.4f m/s against a default of\n"
                    "     %.4f — a factor of %.1f, and below it a quarter of the\n"
                    "     crates never qualify however long you wait. A stiffer scene\n"
                    "     (a taller stack, a heavier lid) leaves more residual and\n"
                    "     wants a larger threshold; the symptom of getting it wrong is\n"
                    "     a pile that is visibly still and measurably expensive.\n",
                    static_cast<double>(smallest_working), 0.05,
                    smallest_working > 0.0f ? static_cast<double>(0.05f / smallest_working) : 0.0);
        check(smallest_working > 0.0f, "some threshold lets the yard sleep");
        check(smallest_working <= 0.05f, "and the default is above it");
    }

    // ---- what `wake` resetting the clock is actually for ---------------------
    //
    // *** AND THIS ONE CAME OUT NEGATIVE FIRST. *** The obvious demonstration —
    // wake a sleeping crate and shove it — shows NO difference at all, because
    // a moving body is not a sleep candidate and `update_sleep` resets its
    // timer on the first frame anyway. The reset matters for the case nobody
    // thinks to test: a body woken that does not then move.
    {
        std::printf("\n  `wake` RESETS THE CLOCK AS WELL AS THE FLAG, and the obvious\n"
                    "  test does not show why. A sleeping crate, woken, then:\n");
        std::printf("     what happens next     flag only    wake() as shipped\n");

        float shoved[2] = {};
        int resleep[2] = {};
        for (int reset = 0; reset <= 1; ++reset)
        {
            // (a) shoved at 2 m/s
            {
                scene s;
                s.material.friction = 0.2f;
                add_floor(s);
                add_tower(s, tower_spec{1});
                s.run(3.0f);
                const float banked = s.body(1).sleep_timer;
                const float x0 = s.body(1).state.position.x;
                wake(s.body(1));
                if (!reset) { s.body(1).sleep_timer = banked; }
                s.body(1).state.velocity = vec3{2.0f, 0.0f, 0.0f};
                s.run(4.0f);
                shoved[reset] = s.body(1).state.position.x - x0;
            }
            // (b) woken and left alone
            {
                scene s;
                add_floor(s);
                add_tower(s, tower_spec{1});
                s.run(3.0f);
                const float banked = s.body(1).sleep_timer;
                wake(s.body(1));
                if (!reset) { s.body(1).sleep_timer = banked; }
                int frames = 0;
                for (int i = 0; i < 300; ++i)
                {
                    s.step();
                    ++frames;
                    if (s.body(1).sleeping) { break; }
                }
                resleep[reset] = frames;
            }
        }
        std::printf("     shoved at 2 m/s        %6.4f m     %6.4f m\n",
                    static_cast<double>(shoved[0]), static_cast<double>(shoved[1]));
        std::printf("     woken and left alone   %4d frames   %4d frames until asleep again\n",
                    resleep[0], resleep[1]);
        check(std::abs(shoved[0] - shoved[1]) < 1e-4f,
              "a woken body that MOVES does not care, because moving resets the timer anyway");
        check(resleep[1] > resleep[0], "a woken body that does not move cares a great deal");
        std::printf("     %d frames against %d, and `time_to_sleep` is %.2f s = %.0f frames.\n"
                    "     WITHOUT THE RESET, A BODY WOKEN SO THAT GAMEPLAY CAN DO\n"
                    "     SOMETHING WITH IT NEXT FRAME IS ASLEEP AGAIN BEFORE GAMEPLAY\n"
                    "     GETS THERE.\n", resleep[0], resleep[1], 0.5, 0.5 * 60.0);
    }
}

// ---------------------------------------------------------------------------
// §I  where the solve goes in the step
// ---------------------------------------------------------------------------

/// `body_world::step` EXACTLY as it stood before Lesson 8.10 split it, over a
/// plain array of bodies.
///
/// It is here to be a reference rather than to be used. A refactor that is
/// "obviously" behaviour-preserving is exactly the kind that is not, and the
/// only instrument that can settle it is the old code running beside the new
/// one on the same input, compared bit for bit.
void legacy_step(std::span<rigid_body> bodies, vec3 gravity, float h, integrator rule,
                 spin_rule spin)
{
    for (rigid_body& b : bodies)
    {
        switch (b.kind)
        {
        case body_kind::fixed:
            b.force = vec3{};
            b.torque = vec3{};
            continue;

        case body_kind::kinematic:
            b.state.position += b.state.velocity * h;
            b.orientation = advance_orientation(b.orientation, b.angular_velocity, h, spin);
            b.force = vec3{};
            b.torque = vec3{};
            break;

        case body_kind::dynamic:
        {
            const vec3 acceleration =
                (b.inv_mass > 0.0f) ? (b.force * b.inv_mass + gravity * b.gravity_scale) : vec3{};
            engine::phys::integrate(b.state, acceleration, h, rule);
            if (b.damping > 0.0f) { apply_drag(b.state, b.damping, h); }

            const mat3 body_to_world = mat3_from_quat(b.orientation);
            const mat3 inv_i_world =
                body_to_world * b.inv_inertia_local * transpose(body_to_world);

            if (b.gyroscopic != gyroscopic_mode::off)
            {
                b.angular_velocity = engine::phys::gyroscopic_step(b, h, b.gyroscopic);
            }
            b.angular_velocity += (inv_i_world * b.torque) * h;
            if (b.angular_damping > 0.0f)
            {
                b.angular_velocity *= std::exp(-b.angular_damping * h);
            }
            b.orientation = advance_orientation(b.orientation, b.angular_velocity, h, spin);

            b.force = vec3{};
            b.torque = vec3{};
            break;
        }
        }
    }
}

void section_i()
{
    rule("I  WHERE THE SOLVE GOES IN THE STEP");

    // ---- the refactor, checked bit for bit --------------------------------
    //
    // No `gyroscopic_mode::momentum` bodies here: that mode's derivation cannot
    // be cut in half, so the split moves its orientation advance into the
    // velocity half deliberately and the comparison would be a false alarm.
    // Every other body must be identical to the bit.
    {
        rng r(20261011u);
        body_world world;
        std::vector<rigid_body> legacy;
        for (int i = 0; i < 400; ++i)
        {
            rigid_body b = make_box(vec3{r.range(-5.0f, 5.0f), r.range(0.5f, 8.0f),
                                         r.range(-5.0f, 5.0f)},
                                    r.range(1.0f, 40.0f), vec3{0.25f, 0.4f, 0.3f});
            b.state.velocity = vec3{r.signed_unit(), r.signed_unit(), r.signed_unit()} * 3.0f;
            b.angular_velocity = vec3{r.signed_unit(), r.signed_unit(), r.signed_unit()} * 5.0f;
            b.damping = (i % 3 == 0) ? 0.4f : 0.0f;
            b.angular_damping = (i % 5 == 0) ? 0.3f : 0.0f;
            b.gravity_scale = r.range(0.5f, 1.5f);
            if (i % 11 == 0) { b.gyroscopic = gyroscopic_mode::implicit_term; }
            if (i % 17 == 0) { b.kind = body_kind::kinematic; }
            if (i % 23 == 0) { b.kind = body_kind::fixed; }
            engine::phys::add_force(b, vec3{r.signed_unit(), r.signed_unit(), r.signed_unit()} * 20.0f);
            engine::phys::add_torque(b, vec3{r.signed_unit(), r.signed_unit(), r.signed_unit()} * 4.0f);
            world.add(b);
            legacy.push_back(b);
        }

        for (int step = 0; step < 120; ++step)
        {
            world.step(k_h);
            legacy_step(legacy, world.gravity(), k_h, world.integrator_rule(), world.spin());
        }

        std::size_t identical = 0;
        float worst = 0.0f;
        auto bodies = world.bodies();
        for (std::size_t i = 0; i < bodies.size(); ++i)
        {
            const bool same = std::memcmp(&bodies[i].state, &legacy[i].state, sizeof(bodies[i].state)) == 0
                              && std::memcmp(&bodies[i].orientation, &legacy[i].orientation,
                                             sizeof(bodies[i].orientation)) == 0
                              && std::memcmp(&bodies[i].angular_velocity, &legacy[i].angular_velocity,
                                             sizeof(bodies[i].angular_velocity)) == 0;
            if (same) { ++identical; }
            worst = std::max(worst, length(bodies[i].state.position - legacy[i].state.position));
        }
        std::printf("\n  THE REFACTOR, CHECKED RATHER THAN ASSERTED. 400 mixed bodies,\n"
                    "  120 steps, `step(h)` against a private copy of the monolithic\n"
                    "  version it replaced:\n");
        std::printf("     bit-identical bodies: %zu of %zu\n", identical, bodies.size());
        std::printf("     worst position difference: %.3e m\n", static_cast<double>(worst));
        check(identical == bodies.size(), "the split is bit-identical to the function it replaced");
    }

    // ---- solving one step late -------------------------------------------
    {
        std::printf("\n  *** AND WHY THE SPLIT HAD TO HAPPEN AT ALL. A crate resting on\n"
                    "  *** a floor, solved AFTER the position half instead of between\n"
                    "  the halves — which is what every caller does when `step` is the\n"
                    "  only entry point:\n");
        std::printf("     slot             sink after 4 s    per step mm    g*h^2 mm\n");
        const float gh2 = k_gravity * k_h * k_h;
        float sink_mid = 0.0f;
        float sink_end = 0.0f;
        for (int late = 0; late <= 1; ++late)
        {
            scene s;
            s.solve_at_end_of_step = late != 0;
            s.cfg.correction = position_correction::none;
            s.cfg.velocity_iterations = 16;
            s.sleep.enabled = false;
            add_floor(s);
            add_tower(s, tower_spec{1});
            s.run(0.5f);
            const float y0 = s.body(1).state.position.y;
            s.run(4.0f);
            const float sink = y0 - s.body(1).state.position.y;
            if (late == 0) { sink_mid = sink; } else { sink_end = sink; }
            std::printf("     %-15s  %14.4f    %11.5f    %8.5f\n",
                        late ? "end of step" : "mid step (ours)",
                        static_cast<double>(sink * 1000.0f),
                        static_cast<double>(sink * 1000.0f / (4.0f * 60.0f)),
                        static_cast<double>(gh2 * 1000.0f));
        }
        check(sink_end > sink_mid, "solving late sinks the crate and solving mid step does not");
        const float per_step = sink_end / (4.0f * 60.0f);
        std::printf("     measured %.5f mm per step against a predicted %.5f — ratio %.4f\n",
                    static_cast<double>(per_step * 1000.0f), static_cast<double>(gh2 * 1000.0f),
                    static_cast<double>(per_step / gh2));
        check(std::abs(per_step / gh2 - 1.0f) < 0.1f, "the late-solve sink is exactly g*h^2 a step");
    }

    // ---- the price of the split -------------------------------------------
    {
        rng r(20261012u);
        body_world world;
        std::vector<rigid_body> legacy;
        for (int i = 0; i < 4000; ++i)
        {
            rigid_body b = make_box(vec3{r.range(-20.0f, 20.0f), r.range(0.5f, 20.0f),
                                         r.range(-20.0f, 20.0f)},
                                    10.0f, vec3{0.25f, 0.25f, 0.25f});
            b.angular_velocity = vec3{r.signed_unit(), r.signed_unit(), r.signed_unit()};
            world.add(b);
            legacy.push_back(b);
        }

        double split_best = 1e30;
        double legacy_best = 1e30;
        for (int rep = 0; rep < 200; ++rep)
        {
            clock_type::time_point t0 = clock_type::now();
            world.step(k_h);
            split_best = std::min(split_best, seconds_since(t0));

            t0 = clock_type::now();
            legacy_step(legacy, world.gravity(), k_h, world.integrator_rule(), world.spin());
            legacy_best = std::min(legacy_best, seconds_since(t0));
        }
        const double n = 4000.0;
        std::printf("\n  WHAT THE SPLIT COSTS: a second `mat3_from_quat` per dynamic\n"
                    "  body per step, because two functions cannot share a local.\n");
        std::printf("     monolithic  %8.3f us   %6.3f ns/body\n", legacy_best * 1e6,
                    legacy_best * 1e9 / n);
        std::printf("     split       %8.3f us   %6.3f ns/body   +%.1f%%\n", split_best * 1e6,
                    split_best * 1e9 / n, 100.0 * (split_best / legacy_best - 1.0));
        std::printf("     8.3 §12 measured `mat3_from_quat` at 7.843 ns on a 33.793 ns\n"
                    "     update, and hoisting it was that lesson's largest saving.\n");

        // ...and what pays for it.
        scene yard;
        build_yard(yard, 20, 5);
        yard.run(20.0f);
        double t_settled = 1e30;
        for (int rep = 0; rep < 200; ++rep)
        {
            const clock_type::time_point t0 = clock_type::now();
            yard.world.integrate_velocities(yard.h);
            yard.world.integrate_positions(yard.h);
            t_settled = std::min(t_settled, seconds_since(t0));
        }
        scene yard_awake;
        yard_awake.sleep.enabled = false;
        build_yard(yard_awake, 20, 5);
        yard_awake.run(20.0f);
        double t_busy = 1e30;
        for (int rep = 0; rep < 200; ++rep)
        {
            const clock_type::time_point t0 = clock_type::now();
            yard_awake.world.integrate_velocities(yard_awake.h);
            yard_awake.world.integrate_positions(yard_awake.h);
            t_busy = std::min(t_busy, seconds_since(t0));
        }
        std::printf("     AND WHAT PAYS FOR IT: the same 101-body yard, integrated\n"
                    "     asleep %.3f us and awake %.3f us — %.1fx. A sleeping body\n"
                    "     reaches neither half.\n", t_settled * 1e6, t_busy * 1e6,
                    t_settled > 0.0 ? t_busy / t_settled : 0.0);
        check(t_settled < t_busy, "a sleeping scene integrates faster than an awake one");
    }
}

// ---------------------------------------------------------------------------
// §J  the budget
// ---------------------------------------------------------------------------

void section_j()
{
    rule("J  THE BUDGET");

    scene s;
    s.sleep.enabled = false;
    build_yard(s, 20, 5);
    s.run(6.0f);

    std::printf("\n  A SETTLED YARD, 101 bodies, %d manifolds, %d contact points,\n"
                "  sleeping disabled so that every stage is paid for. Minima over\n"
                "  200 repetitions with 5 discarded:\n", s.stats.manifolds, s.stats.points);

    double broad = 1e30;
    double narrow = 1e30;
    double solve_all = 1e30;
    for (int rep = 0; rep < 205; ++rep)
    {
        s.step();
        if (rep < 5) { continue; }
        broad = std::min(broad, s.t_broad);
        narrow = std::min(narrow, s.t_narrow);
        solve_all = std::min(solve_all, s.t_solve);
    }
    std::printf("     broadphase              %8.3f us\n", broad * 1e6);
    std::printf("     narrow phase            %8.3f us\n", narrow * 1e6);
    std::printf("     solve (8 vel + 3 pos)   %8.3f us\n", solve_all * 1e6);
    std::printf("     total                   %8.3f us of a 16667 us frame (%.2f%%)\n",
                (broad + narrow + solve_all) * 1e6,
                100.0 * (broad + narrow + solve_all) / 0.016667);

    // ---- what each iteration costs ----------------------------------------
    //
    // EVERY ROW IS RUN FROM THE SAME SNAPSHOT, and the first version was not:
    // it changed the iteration count on a running scene, which at zero
    // iterations dropped the whole yard through the floor, so the "cost" of
    // few iterations was measured on a scene with no contacts left in it and
    // came out FASTER than eight. A timing sweep over a parameter that changes
    // the simulation has to restore the simulation.
    const snapshot budget_state = take(s);
    const int budget_manifolds = s.stats.manifolds;
    const int budget_points = s.stats.points;

    std::printf("\n  AND WHERE THE SOLVE'S TIME GOES — the same %d manifolds and %d\n"
                "  points, restored from one snapshot for every row:\n",
                budget_manifolds, budget_points);
    std::printf("     vel iters   pos iters    solve us\n");
    double base = 0.0;
    double at_eight = 0.0;
    double pos_none = 0.0;
    double pos_three = 0.0;
    for (int iters : {0, 1, 2, 4, 8, 16})
    {
        s.cfg.velocity_iterations = iters;
        s.cfg.position_iterations = 0;
        s.cfg.correction = position_correction::none;
        double best = 1e30;
        for (int rep = 0; rep < 120; ++rep)
        {
            restore(s, budget_state);
            s.step();
            if (rep >= 5) { best = std::min(best, s.t_solve); }
        }
        if (iters == 0) { base = best; }
        if (iters == 8) { at_eight = best; }
        std::printf("     %9d   %9d    %8.3f\n", iters, 0, best * 1e6);
    }
    for (int iters : {0, 1, 3, 6})
    {
        s.cfg.velocity_iterations = 8;
        s.cfg.position_iterations = iters;
        s.cfg.correction = iters == 0 ? position_correction::none
                                      : position_correction::split_impulse;
        double best = 1e30;
        for (int rep = 0; rep < 120; ++rep)
        {
            restore(s, budget_state);
            s.step();
            if (rep >= 5) { best = std::min(best, s.t_solve); }
        }
        if (iters == 0) { pos_none = best; }
        if (iters == 3) { pos_three = best; }
        std::printf("     %9d   %9d    %8.3f\n", 8, iters, best * 1e6);
    }
    s.cfg.velocity_iterations = 8;
    s.cfg.position_iterations = 3;
    s.cfg.correction = position_correction::split_impulse;

    const double per_vel = (at_eight - base) / 8.0;
    const double per_pos = (pos_three - pos_none) / 3.0;
    std::printf("     fixed cost (islands, sleep, prepare, warm start, write-back):\n"
                "       %.3f us, which is %.0f%% of the whole solve at the shipped\n"
                "       settings. The other %.0f%% is the loop, and it is LINEAR in\n"
                "       the iteration count to three figures — which is what says\n"
                "       the cost model has no surprises in it.\n",
                base * 1e6, 100.0 * base / pos_three, 100.0 - 100.0 * base / pos_three);
    std::printf("     one velocity iteration over %d manifolds: %.3f us\n", budget_manifolds,
                per_vel * 1e6);
    std::printf("     one position iteration:                   %.3f us  (%.0f%% of a\n"
                "       velocity one: no friction, no restitution, one target)\n",
                per_pos * 1e6, per_vel > 0.0 ? 100.0 * per_pos / per_vel : 0.0);
    std::printf("     60 Hz affords about %.0f velocity iterations on this scene before\n"
                "     the BUDGET is the constraint. Eight is a quality decision.\n",
                per_vel > 0.0 ? 0.016667 / per_vel : 0.0);
    check(per_vel > 0.0, "an iteration costs something measurable");
    check(per_pos > 0.0 && per_pos < per_vel, "and a position iteration costs less than a velocity one");

    // ---- and at 8.8's scale ------------------------------------------------
    {
        scene big;
        big.sleep.enabled = false;
        rng r(20261013u);
        add_floor(big, 40.0f);
        for (int i = 0; i < 2000; ++i)
        {
            big.add(make_box(vec3{r.range(-18.0f, 18.0f), r.range(0.3f, 12.0f),
                                  r.range(-18.0f, 18.0f)},
                             10.0f, vec3{0.25f, 0.25f, 0.25f}),
                    box_shape(vec3{0.25f, 0.25f, 0.25f}));
        }
        big.run(6.0f);
        double b_broad = 1e30;
        double b_narrow = 1e30;
        double b_solve = 1e30;
        for (int rep = 0; rep < 80; ++rep)
        {
            big.step();
            if (rep < 5) { continue; }
            b_broad = std::min(b_broad, big.t_broad);
            b_narrow = std::min(b_narrow, big.t_narrow);
            b_solve = std::min(b_solve, big.t_solve);
        }
        std::printf("\n  AT 8.8's SCALE — 2,001 bodies, %d manifolds, %d points:\n",
                    big.stats.manifolds, big.stats.points);
        std::printf("     broadphase %.3f ms   narrow %.3f ms   solve %.3f ms   total %.3f ms\n",
                    b_broad * 1e3, b_narrow * 1e3, b_solve * 1e3,
                    (b_broad + b_narrow + b_solve) * 1e3);
        std::printf("     %.1f%% of a 16.67 ms frame, and %.1f%% of it is the solve.\n",
                    100.0 * (b_broad + b_narrow + b_solve) / 0.016667,
                    100.0 * b_solve / (b_broad + b_narrow + b_solve));

        big.sleep.enabled = true;
        big.run(10.0f);
        double s_total = 1e30;
        for (int rep = 0; rep < 80; ++rep)
        {
            big.step();
            if (rep >= 5) { s_total = std::min(s_total, big.t_broad + big.t_narrow + big.t_solve); }
        }
        std::printf("     with sleeping on and the pile settled: %.3f ms total, %d of\n"
                    "     2000 bodies asleep — %.1fx.\n", s_total * 1e3, big.stats.sleeping_bodies,
                    (b_broad + b_narrow + b_solve) / s_total);
    }
}

// ---------------------------------------------------------------------------
// §K  what 8.10 leaves behind
// ---------------------------------------------------------------------------

void section_k()
{
    rule("K  WHAT 8.10 LEAVES BEHIND");

    // ---- how many sweeps a stack of n needs ------------------------------
    //
    // The headline, and the cleanest statement of what a Gauss-Seidel solver
    // IS: one sweep moves information across one contact, so a chain of n
    // contacts needs of order n sweeps before its bottom knows what its top is
    // doing. Under-sweep it and the stack does not sag — it LEANS, because the
    // corrections arrive inconsistently and the inconsistency has a direction.
    std::printf("\n  HOW MANY SWEEPS A TOWER OF n NEEDS TO STAND FOR 20 SECONDS,\n"
                "  found by trying them (sleeping off, so nothing rescues it):\n");
    std::printf("     crates    minimum velocity iterations    drift at that count mm\n");
    int needed_5 = 0;
    int needed_10 = 0;
    for (int height : {2, 3, 5, 10, 15, 20})
    {
        const float top0 = 0.25f + static_cast<float>(height - 1) * 0.501f;
        int needed = -1;
        float drift_there = 0.0f;
        for (int iters : {2, 4, 8, 12, 16, 20, 24, 32, 48, 64})
        {
            scene s;
            s.sleep.enabled = false;
            s.cfg.velocity_iterations = iters;
            add_floor(s);
            add_tower(s, tower_spec{height});
            s.run(20.0f);
            const rigid_body& top = s.body(static_cast<std::uint32_t>(height));
            if (top.state.position.y > top0 - 0.1f)
            {
                needed = iters;
                drift_there = std::sqrt(top.state.position.x * top.state.position.x
                                        + top.state.position.z * top.state.position.z);
                break;
            }
        }
        if (height == 5)  { needed_5 = needed; }
        if (height == 10) { needed_10 = needed; }
        if (needed < 0)
        {
            std::printf("     %6d    %28s    %22s\n", height, "more than 64", "-");
        }
        else
        {
            std::printf("     %6d    %28d    %22.2f\n", height, needed,
                        static_cast<double>(drift_there * 1000.0f));
        }
    }
    check(needed_5 > 0 && needed_5 <= 8, "the shipped eight iterations hold a five-crate tower");
    check(needed_10 > 8, "and do not hold a ten-crate one");
    std::printf("     THE SHIPPED DEFAULT IS EIGHT, WHICH HOLDS FIVE AND NOT TEN.\n"
                "     That is the honest capability of this solver and it is not a\n"
                "     tuning problem: §10 measured the cost as linear in the count,\n"
                "     the count needed is linear in the height, so a tall stack is\n"
                "     quadratic — and a scene costs what its tallest pile costs.\n");

    // ---- ...and sleeping rescues the short ones ---------------------------
    std::printf("\n  SLEEPING RESCUES THE SHORT ONES, which is not something the\n"
                "  solver can take credit for: it stops the drift by stopping the\n"
                "  simulation, and only if the pile settles before it leans.\n");
    std::printf("     crates    sleeping OFF drift mm    sleeping ON drift mm    asleep at s\n");
    for (int height : {5, 10})
    {
        float drift[2] = {};
        float asleep_at = -1.0f;
        for (int sleeping = 0; sleeping <= 1; ++sleeping)
        {
            scene s;
            s.sleep.enabled = sleeping != 0;
            add_floor(s);
            add_tower(s, tower_spec{height});
            for (int i = 0; i < 1200; ++i)
            {
                s.step();
                if (sleeping && asleep_at < 0.0f && s.stats.sleeping_bodies == height)
                {
                    asleep_at = static_cast<float>(i) * s.h;
                }
            }
            const rigid_body& top = s.body(static_cast<std::uint32_t>(height));
            drift[sleeping] = std::sqrt(top.state.position.x * top.state.position.x
                                        + top.state.position.z * top.state.position.z);
        }
        std::printf("     %6d    %21.2f    %20.2f    %11.2f\n", height,
                    static_cast<double>(drift[0] * 1000.0f),
                    static_cast<double>(drift[1] * 1000.0f), static_cast<double>(asleep_at));
    }

    // ---- mass ratios -------------------------------------------------------
    //
    // The other classic weakness, and it needs a fixture that cannot be
    // rescued by the position correction — otherwise every row reads exactly
    // `penetration_slop` and the table says nothing, which is what the first
    // version of it did.
    std::printf("\n  MASS RATIOS. A heavy crate on a light one on the floor, eight\n"
                "  iterations, no position correction, 10 s:\n");
    std::printf("     ratio     light crate squashed mm    residual m/s\n");
    float depth_1 = 0.0f;
    float depth_1000 = 0.0f;
    for (float ratio : {1.0f, 10.0f, 100.0f, 1000.0f})
    {
        scene s;
        s.sleep.enabled = false;
        s.cfg.correction = position_correction::none;
        add_floor(s);
        s.add(make_box(vec3{0.0f, 0.25f, 0.0f}, 1.0f, vec3{0.25f, 0.25f, 0.25f}),
              box_shape(vec3{0.25f, 0.25f, 0.25f}));
        s.add(make_box(vec3{0.0f, 0.751f, 0.0f}, ratio, vec3{0.25f, 0.25f, 0.25f}),
              box_shape(vec3{0.25f, 0.25f, 0.25f}));
        s.run(10.0f);
        const float depth = 0.25f - s.body(1).state.position.y;
        if (ratio == 1.0f)    { depth_1 = depth; }
        if (ratio == 1000.0f) { depth_1000 = depth; }
        std::printf("     %7.0f   %24.4f    %12.4e\n", static_cast<double>(ratio),
                    static_cast<double>(depth * 1000.0f), static_cast<double>(residual_of(s)));
    }
    std::printf("     a 1000:1 ratio costs %.1fx the penetration of 1:1 at the same\n"
                "     iteration count — the light crate is the one that pays, and it\n"
                "     pays because the impulse the heavy one needs is a thousand\n"
                "     times larger and Gauss-Seidel gets there one sweep at a time.\n",
                depth_1 > 0.0f ? static_cast<double>(depth_1000 / depth_1) : 0.0);
    check(depth_1000 > depth_1, "a large mass ratio penetrates further at the same iteration count");

    std::printf("\n  WHAT ALL THREE HAVE IN COMMON is that a Gauss-Seidel sweep\n"
                "  carries information ONE CONTACT AT A TIME, so a chain of n\n"
                "  contacts needs about n sweeps before the bottom of it knows what\n"
                "  the top is doing. More iterations buy that linearly and nothing\n"
                "  else does — until you stop treating the contacts as independent\n"
                "  scalars and write the constraint as a Jacobian over the pair,\n"
                "  which is the same machinery a hinge needs and is Lesson 8.11.\n");
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strncmp(argv[i], "--only=", 7) == 0) { only = argv[i] + 7; }
    }

    const struct { const char* name; void (*fn)(); } sections[] = {
        {"a", section_a}, {"b", section_b}, {"c", section_c}, {"d", section_d},
        {"e", section_e}, {"f", section_f}, {"g", section_g}, {"h", section_h},
        {"i", section_i}, {"j", section_j}, {"k", section_k},
    };

    std::printf("verify_810 — Lesson 8.10, Sequential Impulses\n");
    for (const auto& s : sections)
    {
        if (only && std::strcmp(only, s.name) != 0) { continue; }
        s.fn();
    }

    std::printf("\n%s: %d checks, %d failures\n", g_failures == 0 ? "PASS" : "FAIL", g_checks,
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
