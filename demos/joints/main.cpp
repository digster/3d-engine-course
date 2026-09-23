// demos/joints/main.cpp — rods, ropes, sockets and hinges, in the loop that
// stacks crates.
//
// Lesson 8.11. `stack` showed what a hundred contacts do to one another. This
// one shows the other kind of constraint the same solver now holds, and every
// scene is one of the lesson's measurements made watchable.
//
//   THE VIEW is from the side, as in `stack`: world x to the right, world y up,
//     and every scene is planar so nothing is hidden. Hinges therefore turn
//     about world z, out of the screen.
//   A JOINT is drawn as a small cross at its anchor, with a thin line from each
//     body's centre to it — the lever arms, which are what the whole lesson
//     turned out to be about. A rope is a line between its anchors, bright
//     when it is taut and dim when it is slack.
//   A LIMIT is drawn as a wedge at the hinge showing the allowed range.
//
// WHAT TO WATCH FOR, IN ORDER.
//
//   [1] PENDULUMS. Three, hung from one bar. On the left a small ball on a
//     1 m rod; in the middle a 2 m plank hung from its top, whose centre is
//     ALSO 1 m below the pin. They start in step and drift apart within a few
//     swings: the plank is 8.3's parallel-axis theorem, audible — its period is
//     2 pi sqrt(I_pivot / (m g d)) with I_pivot = I_cm + m d^2, and the ball's is
//     nearly the point-mass one. On the right a ball on a ROPE, given enough
//     speed to rise above the pivot and not enough to go over: watch the rope
//     go dim at half a length above the bar, and the ball leave the circle.
//   [2] STOPS. Two arms kicked toward alternate stops every 1.2 s, coasting
//     into them. The upper one is a turnstile, hinged through its centre; the
//     lower one a gate, hinged at its end. [S] toggles speculative limits: off,
//     both overshoot their stop by a slice of a step; on, the turnstile stops
//     dead and the gate still BOUNCES — at 7.5% of its speed, and not because
//     of restitution. The limit row sees the inertia about the centre, the gate
//     turns about the pin, and Gauss–Seidel between the two converges at
//     m d^2 / I_pivot = 3/4 per sweep. [Right] twice, to 32 sweeps, and the
//     gate stays on its stop.
//
//     KICKED, NOT MOTORED. The first draft drove the arms with motors, and a
//     motor still pushing into the stop swallows a 7.5% rebound in a few
//     frames — the one thing the scene exists to show. Motors are §11's, and
//     the harness measures them.
//   [3] CHAIN. Ten 1 kg links and an end weight. [W] cycles the weight through
//     1, 10 and 100 kg: at 100 the chain is a stretched spring at the shipped
//     eight sweeps. Hold [Right] and it shortens, slowly. Press [U] for eight
//     sub-steps of one sweep each — the same work — and it snaps nearly true.
//   [4] BRIDGE. Twelve hinged planks between two posts, and crates. Contacts
//     and joints in one loop, one island per bridge-and-its-load. [X] drops a
//     crate. The crates colour by island, as in `stack`.
//
//   [1]..[4]  scenes          [Space] pause   [.] single step   [R] reset
//   [Left] [Right]  velocity iterations      [C] position correction
//   [B] block solve           [S] speculative limits
//   [U] sub-steps (1 / 8)     [W] chain end weight   [X] drop a crate
//   [Esc] quit
//
//     cmake --build build --target joints
//     ./build/demos/joints
//     ./build/demos/joints --scene 3 --weight 100 --substeps 8 --t 4 --shot out.ppm
//
// `engine::engine` directly and NOT `demo_common`, like every physics demo
// before it: it loads nothing and computes everything on screen, so a `--shot`
// run is byte-for-byte reproducible on any machine.

#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/broadphase.hpp>
#include <engine/phys/constraint.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/manifold.hpp>
#include <engine/phys/rigid_body.hpp>
#include <engine/phys/shape.hpp>
#include <engine/phys/solver.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>
#include <engine/ui/debug_ui.hpp>

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace {

using engine::quat;
using engine::vec3;
using namespace engine::phys;

constexpr int k_width = 960;
constexpr int k_height = 540;

/// This demo's own log category, in the range Lesson 5.3 reserved for programs
/// outside the library.
constexpr int log_demo = 71;

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_ground     = engine::pack_argb(52, 58, 72);
constexpr Uint32 k_body       = engine::pack_argb(150, 190, 235);
constexpr Uint32 k_fixed      = engine::pack_argb(110, 116, 130);
constexpr Uint32 k_joint      = engine::pack_argb(255, 200, 80);
constexpr Uint32 k_lever      = engine::pack_argb(120, 104, 70);
constexpr Uint32 k_rope_taut  = engine::pack_argb(235, 225, 160);
constexpr Uint32 k_rope_slack = engine::pack_argb(90, 88, 70);
constexpr Uint32 k_limit      = engine::pack_argb(200, 120, 110);
constexpr Uint32 k_trail      = engine::pack_argb(70, 110, 150);

constexpr int k_view_x0 = 8;
constexpr int k_view_y0 = 8;
constexpr int k_view_x1 = 631;
constexpr int k_view_y1 = 531;

/// The island palette `stack` uses, for the same reason: an island index is a
/// label, not a quantity, so a ramp would be a lie.
constexpr Uint32 k_island_colours[8] = {
    engine::pack_argb(235, 120, 110), engine::pack_argb(120, 200, 235),
    engine::pack_argb(150, 225, 130), engine::pack_argb(240, 190, 100),
    engine::pack_argb(200, 140, 235), engine::pack_argb(120, 230, 200),
    engine::pack_argb(235, 150, 190), engine::pack_argb(180, 200, 120),
};

enum class scene
{
    pendulums,
    stops,
    chain,
    bridge
};

const char* name_of(scene s)
{
    switch (s)
    {
    case scene::pendulums: return "pendulums";
    case scene::stops:     return "stops";
    case scene::chain:     return "chain";
    case scene::bridge:    return "bridge";
    }
    return "?";
}

/// One joint, the two bodies it joins, and whether to draw it as a rope.
struct link
{
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    joint j{};
};

class joints_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char** argv) override
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg{argv[i]};
            if (arg == "--shot" && i + 1 < argc) { shot_path_ = argv[++i]; }
            else if (arg == "--t" && i + 1 < argc)
            {
                shot_at_ = static_cast<float>(std::atof(argv[++i]));
            }
            else if (arg == "--scene" && i + 1 < argc)
            {
                scene_ = static_cast<scene>(std::clamp(std::atoi(argv[++i]) - 1, 0, 3));
            }
            else if (arg == "--iters" && i + 1 < argc)
            {
                cfg_.velocity_iterations = std::clamp(std::atoi(argv[++i]), 1, 128);
            }
            else if (arg == "--substeps" && i + 1 < argc)
            {
                substeps_ = std::clamp(std::atoi(argv[++i]), 1, 16);
            }
            else if (arg == "--no-speculation") { cfg_.joints.speculative_limits = false; }
            else if (arg == "--weight" && i + 1 < argc)
            {
                end_weight_ = std::clamp(static_cast<float>(std::atof(argv[++i])), 0.1f, 1000.0f);
            }
        }
        return {.title = "joints — rods, ropes, sockets and hinges",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    bool on_start() override
    {
        sleep_.enabled = false;
        reset_scene();
        ENGINE_LOG_INFO(log_demo, "joints demo: scene %s, %d iterations, %d sub-steps",
                        name_of(scene_), cfg_.velocity_iterations, substeps_);
        return ui_.start(window(), renderer()) || shot_path_ != nullptr;
    }

    void on_event(const SDL_Event& event) override
    {
        (void)ui_.handle_event(event);
        if (event.type != SDL_EVENT_KEY_DOWN) { return; }

        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_1: scene_ = scene::pendulums; reset_scene(); break;
        case SDL_SCANCODE_2: scene_ = scene::stops;     reset_scene(); break;
        case SDL_SCANCODE_3: scene_ = scene::chain;     reset_scene(); break;
        case SDL_SCANCODE_4: scene_ = scene::bridge;    reset_scene(); break;
        case SDL_SCANCODE_RIGHT:
            cfg_.velocity_iterations = std::min(cfg_.velocity_iterations * 2, 128);
            break;
        case SDL_SCANCODE_LEFT:
            cfg_.velocity_iterations = std::max(cfg_.velocity_iterations / 2, 1);
            break;
        case SDL_SCANCODE_C:
            cfg_.correction =
                static_cast<position_correction>((static_cast<int>(cfg_.correction) + 1) % 3);
            break;
        case SDL_SCANCODE_B: cfg_.joints.block_solve = !cfg_.joints.block_solve; break;
        case SDL_SCANCODE_S:
            cfg_.joints.speculative_limits = !cfg_.joints.speculative_limits;
            reset_scene();
            break;
        case SDL_SCANCODE_U: substeps_ = substeps_ == 1 ? 8 : 1; break;
        case SDL_SCANCODE_W:
            end_weight_ = end_weight_ >= 100.0f ? 1.0f : end_weight_ * 10.0f;
            reset_scene();
            break;
        case SDL_SCANCODE_X: drop_crate(); break;
        case SDL_SCANCODE_R: reset_scene(); break;
        case SDL_SCANCODE_SPACE: running_ = !running_; break;
        case SDL_SCANCODE_PERIOD: running_ = false; single_step_ = true; break;
        default: break;
        }
    }

    void on_input() override { ui_.begin_frame(); }

    void on_fixed_step(float h) override
    {
        t_ += h;
        if (running_ || single_step_)
        {
            advance(h);
            single_step_ = false;
        }
    }

    void on_frame(float alpha) override
    {
        (void)alpha;
        fb().clear(k_background);
        draw_world();

        if (shot_path_ != nullptr && t_ >= shot_at_)
        {
            request_quit(engine::save_ppm(fb(), shot_path_));
        }
    }

    void on_overlay() override
    {
        if (ui_.running()) { panel(); }
        ui_.render();
    }

    void on_stop() override { ui_.stop(); }

private:
    // -----------------------------------------------------------------------
    // Scene setup
    // -----------------------------------------------------------------------

    std::uint32_t add(const rigid_body& b, const shape& s)
    {
        const auto index = static_cast<std::uint32_t>(world_.size());
        world_.add(b);
        shapes_.push_back(s);
        return index;
    }

    [[nodiscard]] rigid_body& body(std::uint32_t i) { return world_.bodies()[i]; }

    /// Add a joint, and — unless it asks otherwise — excuse its two bodies from
    /// colliding with each other. `collision_filter` is the caller's tool because
    /// the narrow phase is the caller's loop.
    void connect(std::uint32_t a, std::uint32_t b, const joint& j, bool rope = false)
    {
        links_.push_back(link{a, b, j});
        rope_.push_back(rope);
        if (!j.collide_connected) { filter_.exclude(a, b); }
    }

    void reset_scene()
    {
        t_ = 0.0f;
        world_.clear();
        shapes_.clear();
        links_.clear();
        rope_.clear();
        filter_.clear();
        cache_.clear();
        solver_.clear();
        manifolds_.clear();
        trail_.clear();
        stats_ = solver_stats{};

        // The floor, in every scene and in no island.
        add(make_fixed(vec3{0.0f, -0.5f, 0.0f}), box_shape(vec3{20.0f, 0.5f, 20.0f}));

        switch (scene_)
        {
        case scene::pendulums: build_pendulums(); break;
        case scene::stops:     build_stops(); break;
        case scene::chain:     build_chain(); break;
        case scene::bridge:    build_bridge(); break;
        }
        filter_.finalize();
    }

    void build_pendulums()
    {
        const float top = 3.6f;
        const std::uint32_t bar = add(make_fixed(vec3{0.0f, top, 0.0f}), box_shape(vec3{3.2f, 0.04f, 0.1f}));
        const float start = 0.35f;
        const quat q = engine::quat_z(start);

        // A ball on a 1 m rod: its centre 1 m below the pin, and almost no
        // inertia of its own, so it keeps nearly the point-mass period.
        const vec3 pin_ball{-2.2f, top, 0.0f};
        const std::uint32_t ball =
            add(make_sphere(pin_ball + engine::rotate(q, vec3{0.0f, -1.0f, 0.0f}), 1.0f, 0.08f),
                sphere_shape(0.08f));
        connect(bar, ball, make_rod(body(bar), body(ball), pin_ball, body(ball).state.position));

        // A 2 m plank hung from its top: its centre is ALSO 1 m below the pin,
        // and its period is 2 pi sqrt(I_pivot / (m g d)), 15% longer.
        const vec3 pin_plank{0.0f, top, 0.0f};
        const vec3 half{0.06f, 1.0f, 0.06f};
        rigid_body plank = make_box(pin_plank + engine::rotate(q, vec3{0.0f, -1.0f, 0.0f}), 1.0f, half);
        plank.orientation = q;
        const std::uint32_t p = add(plank, box_shape(half));
        connect(bar, p, make_ball_socket(body(bar), body(p), pin_plank));

        // A ball on a ROPE, thrown with v0^2 = 3.5 g L: it rises above the pin
        // and the rope goes slack at L/2 above it.
        const vec3 pin_rope{2.2f, top, 0.0f};
        const std::uint32_t bob = add(make_sphere(pin_rope + vec3{0.0f, -1.0f, 0.0f}, 1.0f, 0.08f),
                                      sphere_shape(0.08f));
        body(bob).state.velocity = vec3{std::sqrt(3.5f * 9.81f * 1.0f), 0.0f, 0.0f};
        connect(bar, bob, make_rope(body(bar), body(bob), pin_rope, body(bob).state.position, 1.0f), true);
        trail_body_ = bob;
    }

    void build_stops()
    {
        // Two arms kicked into a ±0.6 rad stop, about z.
        const auto arm = [this](vec3 pin, float offset) {
            const std::uint32_t post = add(make_fixed(pin), box_shape(vec3{0.05f, 0.05f, 0.05f}));
            const vec3 half{0.6f, 0.05f, 0.05f};
            const std::uint32_t a = add(make_box(pin + vec3{offset, 0.0f, 0.0f}, 5.0f, half), box_shape(half));
            joint j = make_hinge(body(post), body(a), pin, vec3{0.0f, 0.0f, 1.0f});
            j.limit.enabled = true;
            j.limit.lower = -0.6f;
            j.limit.upper = 0.6f;
            connect(post, a, j);
            // The world has no gravity torque about a horizontal axis through a
            // turnstile's centre, but a gate would sag; gravity is scaled off
            // on both so the scene isolates the limit.
            body(a).gravity_scale = 0.0f;
        };
        arm(vec3{-1.0f, 2.8f, 0.0f}, 0.0f);   // turnstile: pin through the centre
        arm(vec3{-1.6f, 1.2f, 0.0f}, 0.6f);   // gate: pin at its end, d = W/2
        kick_sign_ = 1.0f;
        kick_arms();
        reverse_at_ = 1.2f;
    }

    /// Set both arms turning at 3 rad/s toward the next stop, as a rigid motion
    /// about each one's own pin so the joint starts the coast satisfied.
    void kick_arms()
    {
        for (const link& l : links_)
        {
            rigid_body& b = body(l.b);
            const vec3 pin = world_point_of(body(l.a), l.j.anchor_a);
            b.angular_velocity = vec3{0.0f, 0.0f, 3.0f * kick_sign_};
            b.state.velocity = engine::cross(b.angular_velocity, b.state.position - pin);
        }
        kick_sign_ = -kick_sign_;
    }

    void build_chain()
    {
        const std::uint32_t ceiling = add(make_fixed(vec3{0.0f, 3.8f, 0.0f}), box_shape(vec3{0.3f, 0.04f, 0.1f}));
        std::uint32_t prev = ceiling;
        for (int i = 0; i < 10; ++i)
        {
            const bool last = i == 9;
            const float y = 3.8f - 0.3f * static_cast<float>(i + 1);
            const float r = last ? 0.12f : 0.07f;
            const std::uint32_t b = add(make_sphere(vec3{0.0f, y, 0.0f}, last ? end_weight_ : 1.0f, r),
                                        sphere_shape(r));
            // Joints midway between the links, hanging straight: §14's fixture.
            // The first draft started the chain a little to the side so that it
            // would swing, and at 100:1 the swinging chain's centripetal load
            // stretched it by METRES and laid the weight on the floor — which is
            // the lesson's point, made so hard that it reads as a crash.
            const vec3 at{0.0f, y + 0.15f, 0.0f};
            connect(prev, b, make_ball_socket(body(prev), body(b), at));
            prev = b;
        }
    }

    void build_bridge()
    {
        const float y = 2.0f;
        const std::uint32_t left = add(make_fixed(vec3{-3.05f, y, 0.0f}), box_shape(vec3{0.05f, 0.3f, 0.4f}));
        const std::uint32_t right = add(make_fixed(vec3{3.05f, y, 0.0f}), box_shape(vec3{0.05f, 0.3f, 0.4f}));
        std::uint32_t prev = left;
        float edge = -3.0f;
        for (int i = 0; i < 12; ++i)
        {
            const float x = -3.0f + 0.25f + 0.5f * static_cast<float>(i);
            const std::uint32_t plank = add(make_box(vec3{x, y, 0.0f}, 2.0f, vec3{0.24f, 0.04f, 0.4f}),
                                            box_shape(vec3{0.24f, 0.04f, 0.4f}));
            connect(prev, plank, make_hinge(body(prev), body(plank), vec3{edge, y, 0.0f}, vec3{0.0f, 0.0f, 1.0f}));
            prev = plank;
            edge = x + 0.25f;
        }
        connect(prev, right, make_hinge(body(prev), body(right), vec3{3.0f, y, 0.0f}, vec3{0.0f, 0.0f, 1.0f}));
        for (int k = 0; k < 3; ++k) { drop_crate(); }
    }

    void drop_crate()
    {
        if (scene_ != scene::bridge) { return; }
        const float x = -2.0f + 1.3f * static_cast<float>(dropped_++ % 4);
        const std::uint32_t c = add(make_box(vec3{x, 3.2f, 0.0f}, 8.0f, vec3{0.2f, 0.2f, 0.2f}),
                                    box_shape(vec3{0.2f, 0.2f, 0.2f}));
        wake(body(c));
    }

    // -----------------------------------------------------------------------
    // The step, which is 8.10's with joints added
    // -----------------------------------------------------------------------

    void advance(float h)
    {
        // Sub-stepping (§14): the same frame cut into `substeps_` smaller steps,
        // each with the whole pipeline. [U] trades eight sweeps of one step for
        // one sweep of each of eight.
        solver_config cfg = cfg_;
        if (substeps_ > 1) { cfg.velocity_iterations = std::max(1, cfg_.velocity_iterations / substeps_); }
        const float hs = h / static_cast<float>(substeps_);

        const auto t0 = std::chrono::steady_clock::now();
        for (int s = 0; s < substeps_; ++s) { step(hs, cfg); }
        ms_step_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() * 1000.0;

        // [2]: kick the arms toward the other stop now and then, so they keep
        // arriving — and so that what happens after each arrival is visible.
        if (scene_ == scene::stops && t_ >= reverse_at_)
        {
            kick_arms();
            reverse_at_ += 1.2f;
        }

        if (scene_ == scene::pendulums)
        {
            trail_.push_back(body(trail_body_).state.position);
            if (trail_.size() > 240) { trail_.erase(trail_.begin()); }
        }

        labels_.assign(solver_.island_of().begin(), solver_.island_of().end());
    }

    void step(float h, const solver_config& cfg_in)
    {
        solver_config cfg = cfg_in;
        cfg.restitution_bias = world_.gravity() * h;

        world_.integrate_velocities(h);

        auto bodies = world_.bodies();
        proxies_.clear();
        for (std::size_t i = 0; i < bodies.size(); ++i)
        {
            proxies_.push_back(proxy{bounds_of(shapes_[i], bodies[i].state.position, bodies[i].orientation),
                                     static_cast<std::uint32_t>(i)});
        }
        grid_.build(proxies_, bp_);

        cache_.begin_frame();
        manifolds_.clear();
        keys_.clear();
        pair_a_.clear();
        pair_b_.clear();
        manifolds_.reserve(grid_.pairs().size());
        for (const broadphase_pair& p : grid_.pairs())
        {
            if (bodies[p.a].kind != body_kind::dynamic && bodies[p.b].kind != body_kind::dynamic) { continue; }

            // Lesson 8.11: a joint's two bodies do not collide with each other.
            if (filter_.excluded(p.a, p.b)) { continue; }

            contact_manifold m = collide_pair(p.a, p.b);
            if (m.count == 0) { continue; }
            const std::uint64_t key = pair_key(p.a, p.b);
            if (const contact_manifold* previous = cache_.find(key)) { carry_impulses(m, *previous); }
            manifolds_.push_back(m);
            keys_.push_back(key);
            pair_a_.push_back(p.a);
            pair_b_.push_back(p.b);
        }

        solver_.begin(world_.bodies());
        for (std::size_t i = 0; i < manifolds_.size(); ++i)
        {
            solver_.add(pair_a_[i], pair_b_[i], manifolds_[i], material_);
        }
        for (link& l : links_) { solver_.add(l.a, l.b, l.j); }
        stats_ = solver_.solve(h, cfg, sleep_);

        for (std::size_t i = 0; i < manifolds_.size(); ++i) { cache_.store(keys_[i], manifolds_[i]); }
        cache_.end_frame();

        world_.integrate_positions(h);
    }

    [[nodiscard]] contact_manifold collide_pair(std::uint32_t ia, std::uint32_t ib) const
    {
        auto bodies = world_.bodies();
        const shape& sa = shapes_[ia];
        const shape& sb = shapes_[ib];
        if (sa.kind == shape_kind::sphere && sb.kind == shape_kind::sphere)
        {
            const auto x = world_sphere(sa, bodies[ia].state.position);
            const auto y = world_sphere(sb, bodies[ib].state.position);
            return collide_manifold(as_convex(x), as_convex(y), mf_);
        }
        if (sa.kind == shape_kind::sphere)
        {
            const auto x = world_sphere(sa, bodies[ia].state.position);
            const auto y = world_obb(sb, bodies[ib].state.position, bodies[ib].orientation);
            return collide_manifold(as_convex(x), as_convex(y), mf_);
        }
        if (sb.kind == shape_kind::sphere)
        {
            const auto x = world_obb(sa, bodies[ia].state.position, bodies[ia].orientation);
            const auto y = world_sphere(sb, bodies[ib].state.position);
            return collide_manifold(as_convex(x), as_convex(y), mf_);
        }
        const auto x = world_obb(sa, bodies[ia].state.position, bodies[ia].orientation);
        const auto y = world_obb(sb, bodies[ib].state.position, bodies[ib].orientation);
        return collide_manifold(as_convex(x), as_convex(y), mf_);
    }

    // -----------------------------------------------------------------------
    // Drawing
    // -----------------------------------------------------------------------

    /// Half the visible width, in metres; the view is the same for every scene
    /// here, because all four are built to fit it.
    static constexpr float k_half_width = 3.6f;
    static constexpr float k_ground_px = 40.0f;

    [[nodiscard]] static int sx(float x)
    {
        const float span = static_cast<float>(k_view_x1 - k_view_x0);
        return k_view_x0 + static_cast<int>((x + k_half_width) / (2.0f * k_half_width) * span);
    }

    [[nodiscard]] static int sy(float y)
    {
        const float span = static_cast<float>(k_view_x1 - k_view_x0);
        const float per_metre = span / (2.0f * k_half_width);
        return k_view_y1 - static_cast<int>(k_ground_px) - static_cast<int>(y * per_metre);
    }

    /// Liang–Barsky against the view rectangle — `stack`'s, unchanged.
    static void clipped_line(engine::framebuffer& f, float x0, float y0, float x1, float y1, Uint32 colour)
    {
        float t0 = 0.0f;
        float t1 = 1.0f;
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float p[4] = {-dx, dx, -dy, dy};
        const float q[4] = {x0 - static_cast<float>(k_view_x0), static_cast<float>(k_view_x1) - x0,
                            y0 - static_cast<float>(k_view_y0), static_cast<float>(k_view_y1) - y0};
        for (int i = 0; i < 4; ++i)
        {
            if (p[i] == 0.0f)
            {
                if (q[i] < 0.0f) { return; }
                continue;
            }
            const float r = q[i] / p[i];
            if (p[i] < 0.0f) { t0 = std::max(t0, r); }
            else             { t1 = std::min(t1, r); }
        }
        if (t0 > t1) { return; }
        engine::draw_line(f, static_cast<int>(x0 + t0 * dx), static_cast<int>(y0 + t0 * dy),
                          static_cast<int>(x0 + t1 * dx), static_cast<int>(y0 + t1 * dy), colour);
    }

    void line(vec3 a, vec3 b, Uint32 colour)
    {
        clipped_line(fb(), static_cast<float>(sx(a.x)), static_cast<float>(sy(a.y)),
                     static_cast<float>(sx(b.x)), static_cast<float>(sy(b.y)), colour);
    }

    [[nodiscard]] Uint32 colour_of(std::size_t index) const
    {
        const rigid_body& b = world_.bodies()[index];
        if (b.kind != body_kind::dynamic) { return k_fixed; }
        if (scene_ != scene::bridge) { return k_body; }
        const int isl = index < labels_.size() ? labels_[index] : -1;
        return isl >= 0 ? k_island_colours[static_cast<std::size_t>(isl) % 8] : k_body;
    }

    void draw_world()
    {
        line(vec3{-40.0f, 0.0f, 0.0f}, vec3{40.0f, 0.0f, 0.0f}, k_ground);

        for (std::size_t i = 1; i < trail_.size(); ++i) { line(trail_[i - 1], trail_[i], k_trail); }

        auto bodies = world_.bodies();
        for (std::size_t i = 1; i < bodies.size(); ++i)
        {
            if (shapes_[i].kind == shape_kind::sphere) { draw_sphere(bodies[i], shapes_[i], colour_of(i)); }
            else                                        { draw_box(bodies[i], shapes_[i], colour_of(i)); }
        }
        draw_joints();
    }

    void draw_box(const rigid_body& b, const shape& s, Uint32 colour)
    {
        const vec3 p = b.state.position;
        const vec3 ex = engine::rotate(b.orientation, vec3{s.half_extents.x, 0.0f, 0.0f});
        const vec3 ey = engine::rotate(b.orientation, vec3{0.0f, s.half_extents.y, 0.0f});
        line(p - ex - ey, p + ex - ey, colour);
        line(p + ex - ey, p + ex + ey, colour);
        line(p + ex + ey, p - ex + ey, colour);
        line(p - ex + ey, p - ex - ey, colour);
    }

    void draw_sphere(const rigid_body& b, const shape& s, Uint32 colour)
    {
        // Sixteen chords and one spoke, so that a spinning ball visibly spins.
        const vec3 c = b.state.position;
        constexpr int k_segments = 16;
        vec3 prev = c + vec3{s.radius, 0.0f, 0.0f};
        for (int i = 1; i <= k_segments; ++i)
        {
            const float a = 6.2831853f * static_cast<float>(i) / k_segments;
            const vec3 next = c + vec3{s.radius * std::cos(a), s.radius * std::sin(a), 0.0f};
            line(prev, next, colour);
            prev = next;
        }
        line(c, c + engine::rotate(b.orientation, vec3{s.radius, 0.0f, 0.0f}), colour);
    }

    void draw_joints()
    {
        for (std::size_t k = 0; k < links_.size(); ++k)
        {
            const link& l = links_[k];
            const rigid_body& a = body(l.a);
            const rigid_body& b = body(l.b);
            const vec3 pa = world_point_of(a, l.j.anchor_a);
            const vec3 pb = world_point_of(b, l.j.anchor_b);

            if (l.j.kind == joint_kind::distance)
            {
                // A rope's colour is its tension: bright while its row pushes, dim
                // once the accumulated impulse has gone to zero and it is slack.
                const bool taut = !rope_[k] || l.j.limit_impulse[1] > 0.0f;
                line(pa, pb, taut ? k_rope_taut : k_rope_slack);
                continue;
            }

            // The lever arms: centre to anchor on each side.
            if (a.kind == body_kind::dynamic) { line(a.state.position, pa, k_lever); }
            if (b.kind == body_kind::dynamic) { line(b.state.position, pb, k_lever); }

            const float s = 0.05f;
            line(pb + vec3{-s, 0.0f, 0.0f}, pb + vec3{s, 0.0f, 0.0f}, k_joint);
            line(pb + vec3{0.0f, -s, 0.0f}, pb + vec3{0.0f, s, 0.0f}, k_joint);

            if (l.j.kind == joint_kind::hinge && l.j.limit.enabled)
            {
                // The allowed range, as a wedge in `a`'s frame, drawn from the
                // pose the joint was authored in.
                const vec3 rest_dir = engine::rotate(a.orientation * l.j.rest, vec3{1.0f, 0.0f, 0.0f});
                const float base = std::atan2(rest_dir.y, rest_dir.x);
                for (float edge : {l.j.limit.lower, l.j.limit.upper})
                {
                    const float ang = base + edge;
                    line(pa, pa + vec3{0.9f * std::cos(ang), 0.9f * std::sin(ang), 0.0f}, k_limit);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Panel
    // -----------------------------------------------------------------------

    void panel()
    {
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(k_view_x1 + 16), 8.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 520.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("joints");

        ImGui::Text("scene: %s   t = %.2f s", name_of(scene_), static_cast<double>(t_));
        ImGui::Separator();

        ImGui::Text("velocity iterations  %d", cfg_.velocity_iterations);
        ImGui::Text("sub-steps            %d%s", substeps_,
                    substeps_ > 1 ? "  (sweeps split across them)" : "");
        ImGui::Text("correction           %s", name_of(cfg_.correction));
        ImGui::Text("block solve          %s", cfg_.joints.block_solve ? "on" : "OFF (rows)");
        ImGui::Text("speculative limits   %s", cfg_.joints.speculative_limits ? "on" : "OFF");
        if (scene_ == scene::chain) { ImGui::Text("end weight           %.0f kg", static_cast<double>(end_weight_)); }
        ImGui::Separator();

        float stretch = 0.0f;
        float worst_angle = 0.0f;
        for (const link& l : links_)
        {
            const joint_error e = measure_joint(l.j, body(l.a), body(l.b));
            stretch += e.linear;
            worst_angle = std::max(worst_angle, e.angular);
        }
        ImGui::Text("joints               %d", stats_.joints);
        ImGui::Text("total joint gap      %.2f mm", static_cast<double>(stretch * 1000.0f));
        ImGui::Text("worst angular error  %.4f deg", static_cast<double>(worst_angle * 57.29578f));
        ImGui::Text("joint residual       %.2e", static_cast<double>(stats_.joint_residual));
        ImGui::Text("contact points       %d", stats_.points);
        ImGui::Text("islands              %d", stats_.islands);
        ImGui::Text("step                 %.3f ms", ms_step_);
        ImGui::Separator();

        ImGui::TextWrapped("[1]..[4] scene   [Left]/[Right] iterations   [C] correction");
        ImGui::TextWrapped("[B] block solve   [S] speculative limits   [U] sub-steps");
        ImGui::TextWrapped("[W] chain end weight   [X] drop a crate");
        ImGui::TextWrapped("[Space] pause   [.] step   [R] reset   [Esc] quit");

        ImGui::End();
    }

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    engine::debug_ui ui_{};

    body_world world_{};
    std::vector<shape> shapes_{};
    std::vector<link> links_{};
    std::vector<bool> rope_{};
    collision_filter filter_{};
    uniform_grid grid_{};
    manifold_cache cache_{};
    constraint_solver solver_{};

    solver_config cfg_{};
    sleep_config sleep_{};
    broadphase_config bp_{};
    manifold_config mf_{};
    contact_material material_{0.0f, 0.6f};

    std::vector<proxy> proxies_{};
    std::vector<contact_manifold> manifolds_{};
    std::vector<std::uint64_t> keys_{};
    std::vector<std::uint32_t> pair_a_{};
    std::vector<std::uint32_t> pair_b_{};
    std::vector<int> labels_{};
    std::vector<vec3> trail_{};

    solver_stats stats_{};
    double ms_step_ = 0.0;

    scene scene_ = scene::pendulums;
    int substeps_ = 1;
    float end_weight_ = 100.0f;
    int dropped_ = 0;
    std::uint32_t trail_body_ = 0;
    float reverse_at_ = 1e30f;
    float kick_sign_ = 1.0f;
    bool running_ = true;
    bool single_step_ = false;
    float t_ = 0.0f;

    const char* shot_path_ = nullptr;
    float shot_at_ = 2.0f;
};

}  // namespace

ENGINE_MAIN(joints_app)
