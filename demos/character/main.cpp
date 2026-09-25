// demos/character/main.cpp — a character that obeys the design, beside one that
// obeys Newton.
//
// Lesson 8.13. Twelve demos showed bodies the solver owns. This one shows the
// one body in a game the solver must NOT own: the player. A capsule moved by
// `phys/character.hpp` — casts, slides, steps, snaps and carries — through a
// course built to show each decision, with a kinematic PROXY steered onto it so
// that the crates it walks into are pushed at the speed it is really moving.
//
//   THE VIEW is an orthographic three-quarter view that follows the character.
//     Boxes are wireframes; the character is its capsule's silhouette, WARM when
//     grounded and BLUE in the air. A short line from its feet is the GROUND
//     NORMAL it is standing on — yellow when walkable, red when not (a round
//     bottom on an edge reports the edge-to-centre direction, 8.13 §6).
//
// WHAT TO WATCH FOR, IN ORDER.
//
//   [1] COURSE. Walk it yourself, or press [O] for the autopilot, which walks it
//     at 3 m/s. An 8 cm curb climbed by the round bottom alone; five 18 cm
//     stairs, each a step-up; a 30 cm drop the snap pulls it down; a 30° ramp
//     down with no bounce; two light crates PUSHED (steered proxy) and a heavy
//     one that is a wall; a turntable that carries it round (transform carry —
//     [C] cycles to velocity carry, which drifts outward, and to none); and a
//     1 cm wall it DASHES into at 40 m/s and stops a skin short of. Off the path:
//     a 55° slope it cannot climb, a lift, and an obtuse corner.
//     [K] cycles the clip rule — try the corner with "remainder". [N] turns the
//     snap off (the drop and the ramp become little falls), [T] the step-up
//     (the stairs become a wall), [P] teleports the proxy instead of steering
//     it (the crates, asleep by then, are walked straight through).
//   [2] NEWTON. The same script driven into two characters: a rigid capsule on
//     8.10's solver, rotation locked, friction 0.6, and the controller. Both
//     walk at a 20 cm ledge at 3 m/s; only the controller goes up. Then both
//     jump at a wall and keep pushing into it: the rigid one HANGS there, held
//     by friction; the controller slides down it.
//   [3] CORNERS. Three characters pushing into three 120° corners, one clip rule
//     each: the original motion against every plane (still), the remainder
//     against every plane, the remainder against the last plane (both walk back
//     and forth, 8.13 §4).
//
//   [1]..[3] scenes     [W][A][S][D] / arrows  move     [Shift] run   [F] dash
//   [Space] jump        [O] autopilot           [K] clip rule    [N] snap
//   [T] step-up         [C] carry rule          [P] proxy steer / teleport
//   [R] reset           [.] single step         [Esc] quit
//
//     cmake --build build --target character
//     ./build/demos/character
//     ./build/demos/character --scene 1 --auto --t 6.5 --shot out.ppm
//     ./build/demos/character --scene 2 --t 2.2 --shot out.ppm
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
#include <engine/phys/character.hpp>
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
constexpr int log_demo = 73;

constexpr float k_pi = 3.14159265358979323846f;
[[nodiscard]] float rad(float degrees) { return degrees * (k_pi / 180.0f); }

constexpr float k_g = 9.81f;

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_grid       = engine::pack_argb(36, 40, 50);
constexpr Uint32 k_fixed      = engine::pack_argb(110, 116, 130);
constexpr Uint32 k_moving     = engine::pack_argb(120, 170, 230);
constexpr Uint32 k_light      = engine::pack_argb(150, 190, 120);
constexpr Uint32 k_heavy      = engine::pack_argb(200, 120, 90);
constexpr Uint32 k_asleep     = engine::pack_argb(80, 100, 70);
constexpr Uint32 k_grounded   = engine::pack_argb(240, 170, 110);
constexpr Uint32 k_airborne   = engine::pack_argb(120, 190, 250);
constexpr Uint32 k_rigid      = engine::pack_argb(220, 110, 200);
constexpr Uint32 k_walkable   = engine::pack_argb(255, 210, 90);
constexpr Uint32 k_steep      = engine::pack_argb(240, 70, 60);

constexpr int k_view_x0 = 8;
constexpr int k_view_y0 = 8;
constexpr int k_view_x1 = 631;
constexpr int k_view_y1 = 531;

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

enum class scene
{
    course,
    newton,
    corners
};

const char* name_of(scene s)
{
    switch (s)
    {
    case scene::course:  return "course";
    case scene::newton:  return "Newton vs design";
    case scene::corners: return "corners";
    }
    return "?";
}

const char* name_of(character_config::clip_rule r)
{
    switch (r)
    {
    case character_config::clip_rule::original:   return "the original, every plane";
    case character_config::clip_rule::remainder:  return "the remainder, every plane";
    case character_config::clip_rule::last_plane: return "the remainder, last plane";
    }
    return "?";
}

const char* name_of(character_config::carry_rule r)
{
    switch (r)
    {
    case character_config::carry_rule::transform: return "by transform";
    case character_config::carry_rule::velocity:  return "by velocity (drifts)";
    case character_config::carry_rule::none:      return "none";
    }
    return "?";
}

/// A controller-driven character and the game state that drives it: the
/// velocity is DESIGN — the stick sets the horizontal part, gravity accumulates
/// while airborne, a grounded character does not fall.
struct player
{
    character ch{};
    character_config cfg{};
    vec3 vel{};
    std::uint32_t proxy = k_no_body;
    move_report last{};
    quat facing{};
    double us = 0.0;   ///< the last move_character, in microseconds
};

class character_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char** argv) override
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg{argv[i]};
            if (arg == "--shot" && i + 1 < argc) { shot_path_ = argv[++i]; }
            else if (arg == "--t" && i + 1 < argc) { shot_at_ = static_cast<float>(std::atof(argv[++i])); }
            else if (arg == "--scene" && i + 1 < argc)
            {
                scene_ = static_cast<scene>(std::clamp(std::atoi(argv[++i]) - 1, 0, 2));
            }
            else if (arg == "--auto") { autopilot_ = true; }
            else if (arg == "--no-snap") { snap_ = false; }
            else if (arg == "--no-step") { step_ = false; }
            else if (arg == "--teleport") { steer_ = false; }
            else if (arg == "--clip" && i + 1 < argc)
            {
                const std::string_view c{argv[++i]};
                clip_ = c == "remainder" ? character_config::clip_rule::remainder
                        : c == "last"    ? character_config::clip_rule::last_plane
                                         : character_config::clip_rule::original;
            }
        }
        return {.title = "character — a capsule that obeys the design, not Newton",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    bool on_start() override
    {
        bp_.cell_size = 1.0f;
        reset_scene();
        ENGINE_LOG_INFO(log_demo, "character demo: scene %s, autopilot %s", name_of(scene_),
                        autopilot_ ? "on" : "off");
        return ui_.start(window(), renderer()) || shot_path_ != nullptr;
    }

    void on_event(const SDL_Event& event) override
    {
        (void)ui_.handle_event(event);
        if (event.type != SDL_EVENT_KEY_DOWN) { return; }

        using cr = character_config::clip_rule;
        using ca = character_config::carry_rule;
        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_1: scene_ = scene::course; reset_scene(); break;
        case SDL_SCANCODE_2: scene_ = scene::newton; reset_scene(); break;
        case SDL_SCANCODE_3: scene_ = scene::corners; reset_scene(); break;
        case SDL_SCANCODE_SPACE: jump_ = true; break;
        case SDL_SCANCODE_O: autopilot_ = !autopilot_; break;
        case SDL_SCANCODE_K:
            clip_ = clip_ == cr::original ? cr::remainder : clip_ == cr::remainder ? cr::last_plane : cr::original;
            break;
        case SDL_SCANCODE_N: snap_ = !snap_; break;
        case SDL_SCANCODE_T: step_ = !step_; break;
        case SDL_SCANCODE_C:
            carry_ = carry_ == ca::transform ? ca::velocity : carry_ == ca::velocity ? ca::none : ca::transform;
            break;
        case SDL_SCANCODE_P: steer_ = !steer_; break;
        case SDL_SCANCODE_R: reset_scene(); break;
        case SDL_SCANCODE_PERIOD: running_ = false; single_step_ = true; break;
        case SDL_SCANCODE_PAUSE: running_ = !running_; break;
        default: break;
        }
    }

    void on_input() override { ui_.begin_frame(); }

    void on_fixed_step(float h) override
    {
        if (running_ || single_step_)
        {
            t_ += h;
            advance(h);
            single_step_ = false;
        }
    }

    void on_frame(float alpha) override
    {
        (void)alpha;
        fb().clear(k_background);
        draw_world();
        if (shot_path_ != nullptr && t_ >= shot_at_) { request_quit(engine::save_ppm(fb(), shot_path_)); }
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

    std::uint32_t block(vec3 centre, vec3 half, quat q = quat{})
    {
        rigid_body b = make_fixed(centre);
        b.orientation = q;
        return add(b, box_shape(half));
    }

    /// A fixed block whose top is at `top` over [x0, x1] × [z0, z1], down to y = −1.
    std::uint32_t slab(float x0, float x1, float z0, float z1, float top)
    {
        return block(vec3{0.5f * (x0 + x1), 0.5f * (top - 1.0f), 0.5f * (z0 + z1)},
                     vec3{0.5f * (x1 - x0), 0.5f * (top + 1.0f), 0.5f * (z1 - z0)});
    }

    /// A slab tilted about z by `angle` (rising toward +x), top face through `top`.
    std::uint32_t ramp(float angle, vec3 top, float half_length, float half_width)
    {
        const quat q = engine::quat_z(angle);
        const vec3 half{half_length, 0.4f, half_width};
        return block(top - engine::rotate(q, vec3{0.0f, half.y, 0.0f}), half, q);
    }

    std::uint32_t crate(vec3 at, float mass, float half)
    {
        rigid_body b = make_box(at, mass, vec3{half, half, half});
        return add(b, box_shape(vec3{half, half, half}));
    }

    /// Add a controller-driven character standing at `at` (feet on y = floor).
    std::size_t add_player(vec3 at, float floor_y = 0.0f)
    {
        player p;
        p.cfg = settings();
        p.ch.position = vec3{at.x, floor_y + p.cfg.skin + p.cfg.radius + p.cfg.half_height, at.z};
        p.ch.grounded = true;
        p.proxy = add(make_kinematic(p.ch.position, vec3{}), capsule_shape(p.cfg.radius, p.cfg.half_height));
        players_.push_back(p);
        return players_.size() - 1;
    }

    [[nodiscard]] character_config settings() const
    {
        character_config c;
        c.clip = clip_;
        c.snap_distance = snap_ ? 0.35f : 0.0f;
        c.step_height = step_ ? 0.3f : 0.0f;
        c.carry = carry_;
        return c;
    }

    void reset_scene()
    {
        t_ = 0.0f;
        world_.clear();
        shapes_.clear();
        cache_.clear();
        solver_.clear();
        players_.clear();
        rigid_ = k_no_body;
        turntable_ = lift_ = k_no_body;
        heavy_ = k_no_body;

        add(make_fixed(vec3{0.0f, -0.5f, 0.0f}), box_shape(vec3{60.0f, 0.5f, 30.0f}));

        switch (scene_)
        {
        case scene::course:  build_course(); break;
        case scene::newton:  build_newton(); break;
        case scene::corners: build_corners(); break;
        }
    }

    void build_course()
    {
        // The main path runs along +x at z = 0.
        (void)slab(1.5f, 3.0f, -1.5f, 1.5f, 0.08f);                        // an 8 cm curb
        for (int k = 1; k <= 5; ++k)                                        // five 18 cm stairs
        {
            (void)slab(4.0f + 0.3f * static_cast<float>(k - 1), 9.0f, -1.5f, 1.5f, 0.18f * static_cast<float>(k));
        }
        (void)slab(9.0f, 11.0f, -1.5f, 1.5f, 0.6f);                         // a 30 cm drop
        const float a = rad(30.0f);
        const float run = 0.6f / std::tan(a);
        (void)ramp(-a, vec3{11.0f + 0.5f * run, 0.3f, 0.0f}, 0.5f * run / std::cos(a), 1.5f);  // down to the floor
        (void)crate(vec3{14.0f, 0.3f, 0.0f}, 20.0f, 0.3f);                  // light: pushed
        (void)crate(vec3{15.0f, 0.3f, 0.45f}, 20.0f, 0.3f);
        heavy_ = crate(vec3{17.5f, 0.4f, -1.3f}, 100.0f, 0.4f);             // heavy: a wall

        rigid_body table = make_kinematic(vec3{21.0f, 0.1f, 0.0f}, vec3{});
        table.angular_velocity = vec3{0.0f, 0.8f, 0.0f};
        turntable_ = add(table, box_shape(vec3{1.8f, 0.1f, 1.8f}));         // carried round

        (void)block(vec3{26.005f, 1.25f, 0.0f}, vec3{0.005f, 1.25f, 2.0f}); // a 1 cm wall, dashed into

        // Off the path, for playing.
        (void)ramp(rad(55.0f), vec3{7.0f, 1.0f, -4.5f}, 2.0f, 3.0f);        // too steep to walk
        lift_ = add(make_kinematic(vec3{12.0f, 0.1f, 4.5f}, vec3{}), box_shape(vec3{1.0f, 0.1f, 1.0f}));
        (void)slab(13.0f, 16.0f, 3.5f, 5.5f, 1.6f);                         // where the lift goes
        const float c = std::cos(rad(60.0f));
        const float s = std::sin(rad(60.0f));
        (void)corner_wall(vec3{-6.0f, 0.0f, 4.5f}, vec3{-c, 0.0f, s}, vec3{-s, 0.0f, -c});
        (void)corner_wall(vec3{-6.0f, 0.0f, 4.5f}, vec3{-c, 0.0f, -s}, vec3{-s, 0.0f, c});

        (void)add_player(vec3{-2.0f, 0.0f, 0.0f});
    }

    void build_newton()
    {
        // Two lanes, the same obstacles in each: a 20 cm ledge at x = 2, then a
        // tall wall at x = 5.
        for (float z : {-1.5f, 1.5f})
        {
            (void)slab(2.0f, 4.0f, z - 1.0f, z + 1.0f, 0.2f);
            (void)slab(5.0f, 5.5f, z - 1.0f, z + 1.0f, 4.0f);
        }
        rigid_body b = make_dynamic(vec3{0.0f, 0.9f, -1.5f}, 80.0f);
        b.inv_inertia_local = engine::mat3{vec3{}, vec3{}, vec3{}};
        b.allow_sleep = false;
        rigid_ = add(b, capsule_shape(0.3f, 0.6f));
        (void)add_player(vec3{0.0f, 0.0f, 1.5f});
    }

    std::uint32_t corner_wall(vec3 apex, vec3 dir, vec3 n)
    {
        const vec3 centre = apex + dir * 3.0f - n * 0.1f + vec3{0.0f, 1.5f, 0.0f};
        return block(centre, vec3{3.0f, 1.5f, 0.1f}, engine::quat_y(std::atan2(-dir.z, dir.x)));
    }

    void build_corners()
    {
        const float c = std::cos(rad(60.0f));
        const float s = std::sin(rad(60.0f));
        using cr = character_config::clip_rule;
        const cr rules[3] = {cr::original, cr::remainder, cr::last_plane};
        for (int k = 0; k < 3; ++k)
        {
            const vec3 apex{0.0f, 0.0f, -6.0f + 6.0f * static_cast<float>(k)};
            (void)corner_wall(apex, vec3{-c, 0.0f, s}, vec3{-s, 0.0f, -c});
            (void)corner_wall(apex, vec3{-c, 0.0f, -s}, vec3{-s, 0.0f, c});
            const std::size_t i = add_player(apex + vec3{-2.0f, 0.0f, 0.3f});
            players_[i].cfg.clip = rules[k];
            players_[i].cfg.step_height = 0.0f;
        }
    }

    // -----------------------------------------------------------------------
    // The step
    // -----------------------------------------------------------------------

    /// The stick, as a horizontal velocity: the keyboard, or the autopilot, or
    /// a scene's script.
    [[nodiscard]] vec3 wish_for(std::size_t index)
    {
        if (scene_ == scene::newton) { return newton_script().first; }
        if (scene_ == scene::corners) { return vec3{2.0f, 0.0f, 0.3f}; }

        const player& p = players_[index];
        if (autopilot_)
        {
            // Walk the path at 3 m/s; dash at the thin wall.
            const float speed = p.ch.position.x > 23.0f ? 40.0f : 3.0f;
            return vec3{speed, 0.0f, -0.6f * p.ch.position.z};
        }

        // The view's right and forward, flattened onto the ground.
        const vec3 right = engine::normalised(vec3{right_axis().x, 0.0f, right_axis().z});
        const vec3 fwd = engine::normalised(vec3{-right.z, 0.0f, right.x}) * -1.0f;
        vec3 w{};
        if (in().key_down(SDL_SCANCODE_D) || in().key_down(SDL_SCANCODE_RIGHT)) { w += right; }
        if (in().key_down(SDL_SCANCODE_A) || in().key_down(SDL_SCANCODE_LEFT)) { w -= right; }
        if (in().key_down(SDL_SCANCODE_W) || in().key_down(SDL_SCANCODE_UP)) { w += fwd; }
        if (in().key_down(SDL_SCANCODE_S) || in().key_down(SDL_SCANCODE_DOWN)) { w -= fwd; }
        if (engine::length_squared(w) > 0.0f) { w = engine::normalised(w); }
        float speed = 3.0f;
        if (in().key_down(SDL_SCANCODE_LSHIFT)) { speed = 6.0f; }
        if (in().key_down(SDL_SCANCODE_F)) { speed = 40.0f; }
        return w * speed;
    }

    /// Scene [2]'s script, shared by both characters: walk at the ledge, then
    /// jump at the wall and keep pushing into it. Returns the stick and whether
    /// to jump this step.
    [[nodiscard]] std::pair<vec3, bool> newton_script() const
    {
        const bool jump = t_ >= 2.2f && t_ < 2.2f + 1.0f / 60.0f;
        return {vec3{3.0f, 0.0f, 0.0f}, jump};
    }

    void advance(float h)
    {
        // Gameplay moves the kinematic bodies by velocity, never by position.
        if (lift_ != k_no_body)
        {
            // Up and down between 0.1 and 1.5, a 5 s period: v = dy/dt.
            const float w = 2.0f * k_pi / 5.0f;
            body(lift_).state.velocity = vec3{0.0f, 0.7f * w * std::cos(w * t_), 0.0f};
        }

        // 1. The proxies catch up to where each character was left.
        for (player& p : players_)
        {
            rigid_body& proxy = body(p.proxy);
            if (steer_) { steer_proxy(proxy, p.ch.position, h); }
            else
            {
                proxy.state.position = p.ch.position;
                proxy.state.velocity = vec3{};
            }
        }

        // The rigid character in [2] is driven the way a naive game drives one.
        if (rigid_ != k_no_body)
        {
            const auto [wish, jump] = newton_script();
            rigid_body& b = body(rigid_);
            b.state.velocity.x = wish.x;
            b.state.velocity.z = wish.z;
            if (jump) { b.state.velocity.y = jump_speed(k_g, 1.2f); }
        }

        // 2. The physics step.
        physics_step(h);

        // 3. The controllers, against the world as the step left it.
        for (std::size_t i = 0; i < players_.size(); ++i)
        {
            player& p = players_[i];
            p.cfg.clip = scene_ == scene::corners ? p.cfg.clip : clip_;
            p.cfg.snap_distance = snap_ ? 0.35f : 0.0f;
            p.cfg.step_height = (step_ && scene_ != scene::corners) ? 0.3f : 0.0f;
            p.cfg.carry = carry_;

            const vec3 wish = wish_for(i);
            bool jump = (scene_ == scene::newton) ? newton_script().second : jump_;
            if (p.ch.grounded)
            {
                p.vel.y = 0.0f;
                if (jump) { p.vel.y = jump_speed(k_g, 1.2f); }
            }
            p.vel.x = wish.x;
            p.vel.z = wish.z;
            p.vel.y -= k_g * h;

            character_world view;
            view.bodies = world_.bodies();
            view.shapes = std::span<const shape>{shapes_};
            view.self = p.proxy;
            view.spin = world_.spin();

            const auto t0 = std::chrono::steady_clock::now();
            p.last = move_character(p.ch, p.vel * h, h, view, p.cfg);
            p.us = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() * 1e6;

            p.facing = engine::normalised(p.last.carried_turn * p.facing);
            if (p.ch.grounded && p.vel.y < 0.0f) { p.vel.y = 0.0f; }
            if (p.last.hit_ceiling && p.vel.y > 0.0f) { p.vel.y = 0.0f; }
        }
        jump_ = false;
    }

    /// 8.10's pipeline, unchanged.
    void physics_step(float h)
    {
        solver_config cfg;
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
        for (const broadphase_pair& pr : grid_.pairs())
        {
            if (bodies[pr.a].kind != body_kind::dynamic && bodies[pr.b].kind != body_kind::dynamic) { continue; }
            const placed_shape x = place(shapes_[pr.a], bodies[pr.a].state.position, bodies[pr.a].orientation);
            const placed_shape y = place(shapes_[pr.b], bodies[pr.b].state.position, bodies[pr.b].orientation);
            contact_manifold m = collide_manifold(x.view(), y.view(), mf_);
            if (m.count == 0) { continue; }
            const std::uint64_t key = pair_key(pr.a, pr.b);
            if (const contact_manifold* previous = cache_.find(key)) { carry_impulses(m, *previous); }
            manifolds_.push_back(m);
            keys_.push_back(key);
            pair_a_.push_back(pr.a);
            pair_b_.push_back(pr.b);
        }

        solver_.begin(world_.bodies());
        for (std::size_t i = 0; i < manifolds_.size(); ++i)
        {
            solver_.add(pair_a_[i], pair_b_[i], manifolds_[i], material_);
        }
        (void)solver_.solve(h, cfg, sleep_);
        for (std::size_t i = 0; i < manifolds_.size(); ++i) { cache_.store(keys_[i], manifolds_[i]); }
        cache_.end_frame();

        world_.integrate_positions(h);
    }

    // -----------------------------------------------------------------------
    // Drawing: an orthographic three-quarter view that follows the character
    // -----------------------------------------------------------------------

    /// Pixels per metre: close on the course, wider where there are three
    /// characters to see at once.
    [[nodiscard]] float scale() const { return scene_ == scene::corners ? 34.0f : 64.0f; }

    /// The camera looks along `forward` from the front right and above; +x runs
    /// mostly rightward across the screen, so the course reads left to right.
    [[nodiscard]] static vec3 forward_axis() { return engine::normalised(vec3{-0.3f, -0.55f, -1.0f}); }
    [[nodiscard]] static vec3 right_axis()
    {
        return engine::normalised(engine::cross(forward_axis(), vec3{0.0f, 1.0f, 0.0f}));
    }
    [[nodiscard]] static vec3 up_axis() { return engine::cross(right_axis(), forward_axis()); }

    [[nodiscard]] float px(vec3 p) const
    {
        return 0.5f * static_cast<float>(k_view_x0 + k_view_x1) + engine::dot(p - centre_, right_axis()) * scale();
    }
    [[nodiscard]] float py(vec3 p) const
    {
        return 0.5f * static_cast<float>(k_view_y0 + k_view_y1) + 60.0f
               - engine::dot(p - centre_, up_axis()) * scale();
    }

    /// Liang–Barsky against the view rectangle — `ragdoll`'s, unchanged.
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

    void line(vec3 a, vec3 b, Uint32 colour) { clipped_line(fb(), px(a), py(a), px(b), py(b), colour); }

    void screen_circle(float cx, float cy, float r, Uint32 colour)
    {
        constexpr int k_segments = 20;
        float x0 = cx + r;
        float y0 = cy;
        for (int i = 1; i <= k_segments; ++i)
        {
            const float a = 2.0f * k_pi * static_cast<float>(i) / k_segments;
            const float x1 = cx + r * std::cos(a);
            const float y1 = cy + r * std::sin(a);
            clipped_line(fb(), x0, y0, x1, y1, colour);
            x0 = x1;
            y0 = y1;
        }
    }

    /// A capsule's silhouette in an orthographic view: two circles and the two
    /// tangents between them.
    void draw_capsule(const capsule& c, Uint32 colour)
    {
        const vec3 e0 = c.end(-1);
        const vec3 e1 = c.end(1);
        const float x0 = px(e0);
        const float y0 = py(e0);
        const float x1 = px(e1);
        const float y1 = py(e1);
        const float r = c.radius * scale();
        screen_circle(x0, y0, r, colour);
        screen_circle(x1, y1, r, colour);
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.0f) { return; }
        const float nx = -dy / len * r;
        const float ny = dx / len * r;
        clipped_line(fb(), x0 + nx, y0 + ny, x1 + nx, y1 + ny, colour);
        clipped_line(fb(), x0 - nx, y0 - ny, x1 - nx, y1 - ny, colour);
    }

    void draw_box(const rigid_body& b, const shape& s, Uint32 colour)
    {
        const obb box = world_obb(s, b.state.position, b.orientation);
        vec3 c[8];
        box.corners(c);
        const int edges[12][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7}, {7, 6}, {6, 4},
                                  {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& e : edges) { line(c[e[0]], c[e[1]], colour); }
    }

    [[nodiscard]] Uint32 body_colour(std::uint32_t i) const
    {
        const rigid_body& b = world_.bodies()[i];
        if (b.kind == body_kind::kinematic) { return k_moving; }
        if (b.kind == body_kind::fixed) { return k_fixed; }
        if (b.sleeping) { return k_asleep; }
        return i == heavy_ ? k_heavy : k_light;
    }

    void draw_world()
    {
        if (!players_.empty())
        {
            const vec3 p = players_.front().ch.position;
            centre_ = scene_ == scene::course ? vec3{p.x, 0.9f, p.z} : vec3{2.5f, 0.9f, 0.0f};
            if (scene_ == scene::corners) { centre_ = vec3{-2.0f, 0.9f, 0.0f}; }
        }

        const float gx = std::floor(centre_.x);
        const float gz = std::floor(centre_.z);
        for (int k = -9; k <= 9; ++k)
        {
            const float f = static_cast<float>(k);
            line(vec3{gx - 9.0f, 0.0f, gz + f}, vec3{gx + 9.0f, 0.0f, gz + f}, k_grid);
            line(vec3{gx + f, 0.0f, gz - 9.0f}, vec3{gx + f, 0.0f, gz + 9.0f}, k_grid);
        }

        auto bodies = world_.bodies();
        for (std::uint32_t i = 1; i < bodies.size(); ++i)
        {
            if (shapes_[i].kind == shape_kind::box) { draw_box(bodies[i], shapes_[i], body_colour(i)); }
        }
        if (rigid_ != k_no_body)
        {
            draw_capsule(world_capsule(shapes_[rigid_], bodies[rigid_].state.position, bodies[rigid_].orientation),
                         k_rigid);
        }

        for (const player& p : players_)
        {
            draw_capsule(character_capsule(p.ch.position, p.cfg), p.ch.grounded ? k_grounded : k_airborne);
            if (p.ch.grounded)
            {
                const Uint32 c = walkable(p.ch.ground_normal, p.cfg) ? k_walkable : k_steep;
                line(p.ch.ground_point, p.ch.ground_point + p.ch.ground_normal * 0.5f, c);
            }
            // Where it is facing, turned by whatever it stands on.
            const vec3 foot = p.ch.position - vec3{0.0f, p.cfg.half_height + p.cfg.radius, 0.0f};
            line(foot, foot + engine::rotate(p.facing, vec3{0.6f, 0.0f, 0.0f}), k_grid);
        }
    }

    // -----------------------------------------------------------------------
    // Panel
    // -----------------------------------------------------------------------

    void panel()
    {
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(k_view_x1 + 16), 8.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 520.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("character");

        ImGui::Text("scene: %s   t = %.2f s", name_of(scene_), static_cast<double>(t_));
        ImGui::Text("autopilot            %s", autopilot_ ? "on" : "off");
        ImGui::Separator();
        ImGui::Text("clip rule            %s", name_of(clip_));
        ImGui::Text("snap                 %s", snap_ ? "0.35 m" : "OFF");
        ImGui::Text("step-up              %s", step_ ? "0.30 m" : "OFF");
        ImGui::Text("carry                %s", name_of(carry_));
        ImGui::Text("proxy                %s", steer_ ? "steered (the chord)" : "TELEPORTED");
        ImGui::Separator();

        if (!players_.empty())
        {
            const player& p = players_.front();
            const move_report& r = p.last;
            const float speed = engine::length(vec3{p.ch.velocity.x, 0.0f, p.ch.velocity.z});
            ImGui::Text("grounded             %s", p.ch.grounded ? "yes" : "no");
            if (p.ch.grounded)
            {
                ImGui::Text("ground tilt          %.1f deg%s",
                            static_cast<double>(std::acos(std::clamp(p.ch.ground_normal.y, -1.0f, 1.0f)) * 57.29578f),
                            walkable(p.ch.ground_normal, p.cfg) ? "" : " (steep)");
            }
            ImGui::Text("speed (achieved)     %.2f m/s", static_cast<double>(speed));
            ImGui::Text("sweeps / casts       %d / %d", r.sweeps, r.casts);
            ImGui::Text("newton / gjk         %d / %d", r.cast_iterations, r.gjk_iterations);
            ImGui::Text("slides               %d%s", r.slides, r.blocked ? " (blocked)" : "");
            if (r.stepped) { ImGui::Text("stepped up           %.3f m", static_cast<double>(r.step_rise)); }
            if (r.snapped)
            {
                ImGui::Text("snapped down         %.3f m (rolled %.3f)", static_cast<double>(r.snap_drop),
                            static_cast<double>(r.snap_shift));
            }
            ImGui::Text("move                 %.2f us", p.us);
        }
        ImGui::Separator();
        ImGui::TextWrapped("[1] course  [2] Newton  [3] corners   [O] autopilot");
        ImGui::TextWrapped("[WASD] move  [Shift] run  [F] dash  [Space] jump");
        ImGui::TextWrapped("[K] clip rule  [N] snap  [T] step-up  [C] carry  [P] proxy");
        ImGui::TextWrapped("[R] reset  [.] step  [Esc] quit");
        ImGui::End();
    }

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    engine::debug_ui ui_{};

    body_world world_{};
    std::vector<shape> shapes_{};
    uniform_grid grid_{};
    manifold_cache cache_{};
    constraint_solver solver_{};

    sleep_config sleep_{};
    broadphase_config bp_{};
    manifold_config mf_{};
    contact_material material_{0.0f, 0.6f};

    std::vector<proxy> proxies_{};
    std::vector<contact_manifold> manifolds_{};
    std::vector<std::uint64_t> keys_{};
    std::vector<std::uint32_t> pair_a_{};
    std::vector<std::uint32_t> pair_b_{};

    std::vector<player> players_{};
    std::uint32_t rigid_ = k_no_body;
    std::uint32_t turntable_ = k_no_body;
    std::uint32_t lift_ = k_no_body;
    std::uint32_t heavy_ = k_no_body;

    scene scene_ = scene::course;
    character_config::clip_rule clip_ = character_config::clip_rule::original;
    character_config::carry_rule carry_ = character_config::carry_rule::transform;
    bool snap_ = true;
    bool step_ = true;
    bool steer_ = true;
    bool autopilot_ = false;
    bool jump_ = false;
    bool running_ = true;
    bool single_step_ = false;
    vec3 centre_{0.0f, 0.9f, 0.0f};
    float t_ = 0.0f;

    const char* shot_path_ = nullptr;
    float shot_at_ = 2.0f;
};

}  // namespace

ENGINE_MAIN(character_app)
