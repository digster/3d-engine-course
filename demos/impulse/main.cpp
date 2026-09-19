// demos/impulse/main.cpp — one contact, resolved, with the arithmetic on screen.
//
// Lesson 8.9. Five demos before this one showed collision DETECTION: the SAT's
// axes, GJK's simplex, EPA's growing polytope, the manifold's four points, the
// broadphase's lattice. Every one of them answered a question and changed
// nothing. This is the first demo in the course in which something moves because
// the physics said so.
//
//   THE VIEW is from the side: world x to the right, world y up. Every scene
//     here is planar, so nothing is lost and the reader can see the normal, the
//     lever arms and the impulse all at once.
//   THE ARROWS at each contact point are the two halves of the impulse — the
//     normal impulse along the contact normal, and the friction impulse in the
//     contact plane — drawn to the same scale, so their ratio IS Coulomb's cone
//     and you can watch the clip bind.
//   THE INSET at bottom right is the friction clip itself: the admissible set
//     for the current model (a disc for the cone, a square for the box) with the
//     current tangential impulse plotted inside it. Press [F] and watch the
//     square appear around the disc — the extra area in the corners is the 43%
//     of extra friction that §7 measures, and it is available only in the
//     directions the tangent basis happens to point.
//
// WHAT TO WATCH FOR, IN ORDER.
//
//   [1] THE BOUNCE. A ball dropped, with the closed form e^(2n)*h0 drawn as
//     ghost lines at each predicted peak. Press [B] to turn off the restitution
//     bias and watch the ball climb ABOVE the ghosts and then settle into a hop
//     that never dies — 2 cm at e = 0.8, half a metre at e = 0.95. Lesson 8.9 §6.
//   [2] THE SLOPE. A slab on a ramp. Hold [Up] to raise mu and the slab stops
//     where atan(mu) says it should; the panel prints both angles. Press [O] to
//     solve friction BEFORE the normal impulse and the slab slides as though mu
//     were zero, because a cone of radius zero clips everything. §8 and §10.
//   [3] THE ROLL. A ball lands with forward speed and no spin. Friction spins it
//     up and slows it down until the contact point is stationary, at exactly 5/7
//     of the speed it arrived with, whatever mu is. The panel prints v, omega*r
//     and the ratio. §9.
//   [4] THE CREEP. A crate resting flat, with the pass count on [Left]/[Right].
//     At one pass it sinks, visibly, forever. At sixteen the depth freezes. This
//     is the demo of why Lesson 8.10 exists. §12.
//
//   [1]..[4]     scenes        [Space] pause   [.] single step   [R] reset
//   [E] [Shift+E]  restitution [Up] [Down]  friction coefficient
//   [F]          friction model (none / box / cone)
//   [Left] [Right]  solver passes           [W]  warm starting
//   [B]          restitution bias           [T]  restitution threshold
//   [O]          swap the solve order       [G]  ghost peaks / trace
//   [Esc]        quit
//
//     cmake --build build --target impulse
//     ./build/demos/impulse
//     ./build/demos/impulse --scene 2 --mu 0.3 --t 2.0 --shot out.ppm   headless
//
// `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
// `rig`, `plane`, `collector`, `ecs_swarm`, `audio`, `integrate`, `bodies`,
// `spin`, `collide`, `gjk`, `epa`, `manifold` and `broadphase` make. It loads
// nothing and computes everything on screen, so a `--shot` run is byte-for-byte
// reproducible on any machine.
//
// No `engine_use_assets` and no `engine_use_shaders`.

#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/integrate.hpp>
#include <engine/phys/manifold.hpp>
#include <engine/phys/rigid_body.hpp>
#include <engine/phys/shape.hpp>
#include <engine/phys/solver.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>
#include <engine/ui/debug_ui.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace {

using engine::quat;
using engine::vec3;
using engine::phys::as_convex;
using engine::phys::collide_manifold;
using engine::phys::contact_batch;
using engine::phys::contact_manifold;
using engine::phys::contact_material;
using engine::phys::friction_model;
using engine::phys::k_gravity;
using engine::phys::make_box;
using engine::phys::make_fixed;
using engine::phys::make_sphere;
using engine::phys::prepare_contacts;
using engine::phys::rigid_body;
using engine::phys::shape;
using engine::phys::solve_contacts;
using engine::phys::solve_report;
using engine::phys::solver_config;
using engine::phys::world_obb;
using engine::phys::world_sphere;

constexpr int k_width = 960;
constexpr int k_height = 540;

/// This demo's own log category, in the range Lesson 5.3 reserved for programs
/// outside the library.
constexpr int log_demo = 69;

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_ground     = engine::pack_argb(52, 58, 72);
constexpr Uint32 k_body       = engine::pack_argb(150, 190, 235);
constexpr Uint32 k_body_deep  = engine::pack_argb(235, 120, 110);
constexpr Uint32 k_normal_arr = engine::pack_argb(120, 230, 150);
constexpr Uint32 k_friction   = engine::pack_argb(255, 200, 80);
constexpr Uint32 k_ghost      = engine::pack_argb(70, 78, 96);
constexpr Uint32 k_trace      = engine::pack_argb(110, 130, 170);
constexpr Uint32 k_inset_line = engine::pack_argb(90, 98, 114);
constexpr Uint32 k_slide      = engine::pack_argb(255, 110, 90);

constexpr int k_view_x0 = 8;
constexpr int k_view_y0 = 8;
constexpr int k_view_x1 = 631;
constexpr int k_view_y1 = 531;

/// Metres per screen unit. The view is 6 m wide, which fits every scene here.
constexpr float k_view_half_w = 3.0f;

constexpr float k_pi = 3.14159265358979323846f;
constexpr float k_deg = 57.29577951308232f;

enum class scene
{
    bounce,
    slope,
    roll,
    creep
};

const char* name_of(scene s)
{
    switch (s)
    {
    case scene::bounce: return "bounce";
    case scene::slope:  return "slope";
    case scene::roll:   return "roll";
    case scene::creep:  return "creep";
    }
    return "?";
}

class impulse_app final : public engine::app
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
            else if (arg == "--mu" && i + 1 < argc)
            {
                mu_ = std::clamp(static_cast<float>(std::atof(argv[++i])), 0.0f, 2.0f);
            }
            else if (arg == "--e" && i + 1 < argc)
            {
                restitution_ = std::clamp(static_cast<float>(std::atof(argv[++i])), 0.0f, 1.0f);
            }
            else if (arg == "--passes" && i + 1 < argc)
            {
                passes_ = std::clamp(std::atoi(argv[++i]), 1, 64);
            }
            else if (arg == "--no-bias") { bias_ = false; }
            else if (arg == "--wrong-order") { normal_first_ = false; }
        }
        return {.title = "impulse — one contact, resolved",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    bool on_start() override
    {
        reset_scene();
        ENGINE_LOG_INFO(log_demo, "impulse demo: scene %s, e %.2f, mu %.2f, %d pass(es)",
                        name_of(scene_), static_cast<double>(restitution_),
                        static_cast<double>(mu_), passes_);
        return ui_.start(window(), renderer()) || shot_path_ != nullptr;
    }

    void on_event(const SDL_Event& event) override
    {
        (void)ui_.handle_event(event);
        if (event.type != SDL_EVENT_KEY_DOWN) { return; }

        const bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_1: scene_ = scene::bounce; reset_scene(); break;
        case SDL_SCANCODE_2: scene_ = scene::slope;  reset_scene(); break;
        case SDL_SCANCODE_3: scene_ = scene::roll;   reset_scene(); break;
        case SDL_SCANCODE_4: scene_ = scene::creep;  reset_scene(); break;
        case SDL_SCANCODE_E:
            restitution_ = std::clamp(restitution_ + (shift ? -0.05f : 0.05f), 0.0f, 1.0f);
            break;
        case SDL_SCANCODE_UP:    mu_ = std::min(mu_ + 0.05f, 2.0f); break;
        case SDL_SCANCODE_DOWN:  mu_ = std::max(mu_ - 0.05f, 0.0f); break;
        case SDL_SCANCODE_RIGHT: passes_ = std::min(passes_ * 2, 64); break;
        case SDL_SCANCODE_LEFT:  passes_ = std::max(passes_ / 2, 1); break;
        case SDL_SCANCODE_F:
            model_ = static_cast<friction_model>((static_cast<int>(model_) + 1) % 3);
            break;
        case SDL_SCANCODE_B: bias_ = !bias_; break;
        case SDL_SCANCODE_T: threshold_on_ = !threshold_on_; break;
        case SDL_SCANCODE_O: normal_first_ = !normal_first_; break;
        case SDL_SCANCODE_W: warm_ = !warm_; break;
        case SDL_SCANCODE_G: ghosts_ = !ghosts_; break;
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
        // The camera follows the body along x, held still until the body is
        // more than a third of the way out so that a bouncing ball does not make
        // the whole world slide about.
        const float x = body_.state.position.x;
        const float slack = k_view_half_w / 3.0f;
        camera_x_ = std::clamp(camera_x_, x - slack, x + slack);

        fb().clear(k_background);
        draw_world();
        draw_inset();

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

    void reset_scene()
    {
        t_ = 0.0f;
        trace_.clear();
        peaks_.clear();
        rising_ = false;
        previous_y_ = 0.0f;
        report_ = solve_report{};
        manifold_ = contact_manifold{};
        have_previous_ = false;

        ground_tilt_ = 0.0f;
        ball_ = false;
        camera_x_ = 0.0f;

        switch (scene_)
        {
        case scene::bounce:
            ball_ = true;
            radius_ = 0.20f;
            body_shape_ = engine::phys::sphere_shape(radius_);
            body_ = make_sphere(vec3{-1.6f, 2.00f, 0.0f}, 1.0f, radius_);
            break;

        case scene::slope:
            ground_tilt_ = 20.0f;
            half_ = vec3{0.45f, 0.14f, 0.45f};
            body_shape_ = engine::phys::box_shape(half_);
            // Relative to the ground's own centre, which is at -k_ground_half
            // along world y and then rotated about itself. Placing it relative
            // to the world origin — which the first draft did — floats the slab
            // a ground-thickness too high and it lands with speed it should not
            // have.
            body_ = make_box(vec3{0.0f, -k_ground_half, 0.0f}
                                 + up_ramp() * (k_ground_half + half_.y + 0.08f),
                             20.0f, half_);
            body_.orientation = ground_orientation();
            break;

        case scene::roll:
            ball_ = true;
            radius_ = 0.22f;
            body_shape_ = engine::phys::sphere_shape(radius_);
            body_ = make_sphere(vec3{-2.4f, 0.60f, 0.0f}, 2.0f, radius_);
            body_.state.velocity = vec3{4.0f, 0.0f, 0.0f};
            break;

        case scene::creep:
            half_ = vec3{0.45f, 0.45f, 0.45f};
            body_shape_ = engine::phys::box_shape(half_);
            body_ = make_box(vec3{0.0f, k_ground_half + half_.y + 0.06f, 0.0f}, 20.0f, half_);
            break;
        }

        ground_ = make_fixed(vec3{0.0f, -k_ground_half, 0.0f});
        ground_.orientation = ground_orientation();
        ground_shape_ = engine::phys::box_shape(vec3{8.0f, k_ground_half, 8.0f});
        drop_height_ = body_.state.position.y;
        previous_y_ = body_.state.position.y;
        camera_x_ = body_.state.position.x;
    }

    [[nodiscard]] quat ground_orientation() const
    {
        const float a = ground_tilt_ / k_deg;
        return quat{std::cos(0.5f * a), vec3{0.0f, 0.0f, std::sin(0.5f * a)}};
    }

    [[nodiscard]] vec3 up_ramp() const
    {
        return engine::rotate(ground_orientation(), vec3{0.0f, 1.0f, 0.0f});
    }

    // -----------------------------------------------------------------------
    // The step. THE ORDER IS THE LESSON: velocity, then contacts, then position.
    // -----------------------------------------------------------------------

    void advance(float h)
    {
        const vec3 gravity{0.0f, -k_gravity, 0.0f};
        body_.state.velocity = body_.state.velocity + gravity * (body_.gravity_scale * h);

        const auto ground_box = world_obb(ground_shape_, ground_.state.position, ground_.orientation);
        contact_manifold fresh;
        if (ball_)
        {
            const auto ball = world_sphere(body_shape_, body_.state.position);
            fresh = collide_manifold(as_convex(ground_box), as_convex(ball));
        }
        else
        {
            const auto box = world_obb(body_shape_, body_.state.position, body_.orientation);
            fresh = collide_manifold(as_convex(ground_box), as_convex(box));
        }

        solver_config cfg;
        cfg.friction = model_;
        cfg.restitution_threshold = threshold_on_ ? 1.0f : 0.0f;
        cfg.normal_before_friction = normal_first_;
        cfg.warm_start = warm_;
        if (bias_) { cfg.restitution_bias = gravity * h; }

        if (fresh.count > 0)
        {
            if (warm_ && have_previous_) { engine::phys::carry_impulses(fresh, previous_); }
            contact_batch batch =
                prepare_contacts(ground_, body_, fresh, contact_material{restitution_, mu_}, cfg);
            if (warm_) { engine::phys::warm_start_contacts(ground_, body_, batch); }
            for (int pass = 0; pass < passes_; ++pass)
            {
                report_ = solve_contacts(ground_, body_, batch, cfg);
            }
            engine::phys::write_back(batch, fresh);
            batch_ = batch;
            previous_ = fresh;
            have_previous_ = true;
        }
        else
        {
            report_ = solve_report{};
            have_previous_ = false;
        }
        manifold_ = fresh;

        body_.state.position = body_.state.position + body_.state.velocity * h;
        body_.orientation = engine::phys::advance_orientation(
            body_.orientation, body_.angular_velocity, h, engine::phys::spin_rule::linearised);

        // A peak is a step on which the body stopped rising. Collected for the
        // ghost lines in the bounce scene.
        const float y = body_.state.position.y;
        if (y > previous_y_) { rising_ = true; }
        else if (rising_)
        {
            rising_ = false;
            if (peaks_.size() < 32) { peaks_.push_back(previous_y_); }
        }
        previous_y_ = y;

        if (trace_.size() < 4096) { trace_.push_back(body_.state.position); }
    }

    // -----------------------------------------------------------------------
    // Drawing
    // -----------------------------------------------------------------------

    /// World x to screen x, through a camera that follows the body.
    ///
    /// **The roll scene needs it and the others do not**: a ball landing at
    /// 4 m/s crosses a six-metre view in a second and a half, and the first
    /// version of this demo simply lost it off the right-hand edge. Following
    /// the body costs one subtraction and makes every scene framable.
    [[nodiscard]] int sx(float x) const
    {
        const float span = static_cast<float>(k_view_x1 - k_view_x0);
        return k_view_x0
               + static_cast<int>((x - camera_x_ + k_view_half_w) / (2.0f * k_view_half_w) * span);
    }

    [[nodiscard]] static int sy(float y)
    {
        // One metre is the same number of pixels on both axes, anchored so that
        // y = 0 (the top of a level floor) sits 60 px above the bottom edge.
        const float span = static_cast<float>(k_view_x1 - k_view_x0);
        const float per_metre = span / (2.0f * k_view_half_w);
        return k_view_y1 - 60 - static_cast<int>(y * per_metre);
    }

    [[nodiscard]] static float pixels_per_metre()
    {
        return static_cast<float>(k_view_x1 - k_view_x0) / (2.0f * k_view_half_w);
    }

    /// Draw a line clipped to the view rectangle, Liang-Barsky.
    ///
    /// **The demo needs this and the earlier ones did not**, because this is the
    /// first demo whose world is unbounded: a ground plane eight metres wide, a
    /// ball that can be thrown off the top of the frame, and a slab that slides
    /// away down a ramp. Without it the ground line runs straight across the
    /// ImGui panel and through the friction inset, which is what the first
    /// screenshot of this file showed.
    static void clipped_line(engine::framebuffer& f, float x0, float y0, float x1, float y1,
                             Uint32 colour)
    {
        float t0 = 0.0f;
        float t1 = 1.0f;
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float p[4] = {-dx, dx, -dy, dy};
        const float q[4] = {x0 - static_cast<float>(k_view_x0),
                            static_cast<float>(k_view_x1) - x0,
                            y0 - static_cast<float>(k_view_y0),
                            static_cast<float>(k_view_y1) - y0};
        for (int i = 0; i < 4; ++i)
        {
            if (p[i] == 0.0f)
            {
                if (q[i] < 0.0f) { return; }   // parallel to this edge and outside it
                continue;
            }
            const float r = q[i] / p[i];
            if (p[i] < 0.0f) { t0 = std::max(t0, r); }
            else { t1 = std::min(t1, r); }
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

    void draw_world()
    {
        engine::framebuffer& f = fb();

        // The ground, as its four corners in the x-y plane.
        const quat q = ground_orientation();
        const vec3 c = ground_.state.position;
        const vec3 ex = engine::rotate(q, vec3{8.0f, 0.0f, 0.0f});
        const vec3 ey = engine::rotate(q, vec3{0.0f, k_ground_half, 0.0f});
        line(c - ex + ey, c + ex + ey, k_ground);
        line(c - ex - ey, c + ex - ey, k_ground);
        line(c - ex - ey, c - ex + ey, k_ground);
        line(c + ex - ey, c + ex + ey, k_ground);

        if (ghosts_ && scene_ == scene::bounce) { draw_ghost_peaks(); }
        if (ghosts_) { draw_trace(); }

        // The body. Red when it is penetrating more than a millimetre, which is
        // the creep scene's whole subject.
        const float depth = manifold_.count > 0 ? manifold_.deepest() : 0.0f;
        const Uint32 colour = depth > 0.001f ? k_body_deep : k_body;
        if (ball_) { draw_ball(colour); }
        else { draw_box(colour); }

        draw_contacts();
        (void)f;
    }

    void draw_ball(Uint32 colour)
    {
        engine::framebuffer& f = fb();
        const vec3 p = body_.state.position;
        const int cx = sx(p.x);
        const int cy = sy(p.y);
        const int r = static_cast<int>(radius_ * pixels_per_metre());
        int previous_x = cx + r;
        int previous_y = cy;
        for (int i = 1; i <= 48; ++i)
        {
            const float a = static_cast<float>(i) * (2.0f * k_pi / 48.0f);
            const int x = cx + static_cast<int>(static_cast<float>(r) * std::cos(a));
            const int y = cy - static_cast<int>(static_cast<float>(r) * std::sin(a));
            clipped_line(f, static_cast<float>(previous_x), static_cast<float>(previous_y),
                         static_cast<float>(x), static_cast<float>(y), colour);
            previous_x = x;
            previous_y = y;
        }
        // A spoke, so that the spin is visible. Without it a ball rolling and a
        // ball sliding look identical, which is exactly the distinction scene 3
        // exists to draw.
        const vec3 spoke = engine::rotate(body_.orientation, vec3{radius_, 0.0f, 0.0f});
        clipped_line(f, static_cast<float>(cx), static_cast<float>(cy),
                     static_cast<float>(sx(p.x + spoke.x)), static_cast<float>(sy(p.y + spoke.y)),
                     colour);
    }

    void draw_box(Uint32 colour)
    {
        const vec3 p = body_.state.position;
        const vec3 ex = engine::rotate(body_.orientation, vec3{half_.x, 0.0f, 0.0f});
        const vec3 ey = engine::rotate(body_.orientation, vec3{0.0f, half_.y, 0.0f});
        line(p - ex - ey, p + ex - ey, colour);
        line(p + ex - ey, p + ex + ey, colour);
        line(p + ex + ey, p - ex + ey, colour);
        line(p - ex + ey, p - ex - ey, colour);
    }

    /// The impulses, as arrows from each contact point. Both are drawn at the
    /// same newton-seconds-per-pixel scale, so the picture IS Coulomb's ratio.
    void draw_contacts()
    {
        engine::framebuffer& f = fb();
        constexpr float k_per_newton_second = 0.006f;

        for (int i = 0; i < manifold_.count && i < batch_.count; ++i)
        {
            const vec3 p = manifold_.points[i].position;
            const int px = sx(p.x);
            const int py = sy(p.y);
            if (px < k_view_x0 || px > k_view_x1 || py < k_view_y0 || py > k_view_y1) { continue; }
            f.fill_rect(px - 2, py - 2, 5, 5, k_normal_arr);

            const auto& c = batch_.points[i];
            const vec3 n = batch_.normal * (c.normal_impulse * k_per_newton_second);
            clipped_line(f, static_cast<float>(px), static_cast<float>(py),
                         static_cast<float>(sx(p.x + n.x)), static_cast<float>(sy(p.y + n.y)),
                         k_normal_arr);

            const vec3 tangential = batch_.tangent[0] * c.tangent_impulse[0]
                                    + batch_.tangent[1] * c.tangent_impulse[1];
            const vec3 tv = tangential * k_per_newton_second;
            clipped_line(f, static_cast<float>(px), static_cast<float>(py),
                         static_cast<float>(sx(p.x + tv.x)), static_cast<float>(sy(p.y + tv.y)),
                         c.sliding ? k_slide : k_friction);
        }
    }

    /// The closed form, drawn where the ball should peak. Lesson 8.9 §6: the
    /// n-th peak is e^(2n) times the first, and the uncorrected run climbs above
    /// these lines and then never comes down to them.
    void draw_ghost_peaks()
    {
        engine::framebuffer& f = fb();
        if (peaks_.empty()) { return; }
        const float first = peaks_.front();
        for (int n = 0; n < 12; ++n)
        {
            const float h = engine::phys::bounce_height(first, restitution_, n);
            if (h < 0.004f) { break; }
            const int y = sy(h);
            if (y < k_view_y0 || y > k_view_y1) { continue; }
            for (int x = k_view_x0; x < k_view_x1; x += 6) { f.put_pixel(x, y, k_ghost); }
        }
    }

    void draw_trace()
    {
        engine::framebuffer& f = fb();
        for (std::size_t i = 1; i < trace_.size(); ++i)
        {
            clipped_line(f, static_cast<float>(sx(trace_[i - 1].x)),
                         static_cast<float>(sy(trace_[i - 1].y)),
                         static_cast<float>(sx(trace_[i].x)), static_cast<float>(sy(trace_[i].y)),
                         k_trace);
        }
    }

    /// The friction clip, in the contact plane. The admissible set for the
    /// current model, with the current tangential impulse plotted in it.
    ///
    /// **This is the picture of §7.** The square circumscribes the disc, so the
    /// box model has up to sqrt(2) times the friction available — but only in
    /// the directions the tangent basis happens to point, which came out of
    /// `tangent_basis` and has nothing to do with the scene.
    void draw_inset()
    {
        engine::framebuffer& f = fb();
        constexpr int cx = 548;
        constexpr int cy = 452;
        constexpr int r = 62;

        f.fill_rect(cx - r - 10, cy - r - 10, 2 * (r + 10), 2 * (r + 10),
                    engine::pack_argb(22, 25, 33));
        engine::draw_line(f, cx - r - 4, cy, cx + r + 4, cy, k_inset_line);
        engine::draw_line(f, cx, cy - r - 4, cx, cy + r + 4, k_inset_line);

        // The disc, always drawn, so that the square can be seen to enclose it.
        int previous_x = cx + r;
        int previous_y = cy;
        for (int i = 1; i <= 64; ++i)
        {
            const float a = static_cast<float>(i) * (2.0f * k_pi / 64.0f);
            const int x = cx + static_cast<int>(static_cast<float>(r) * std::cos(a));
            const int y = cy - static_cast<int>(static_cast<float>(r) * std::sin(a));
            engine::draw_line(f, previous_x, previous_y, x, y,
                              model_ == friction_model::cone ? k_friction : k_inset_line);
            previous_x = x;
            previous_y = y;
        }
        if (model_ == friction_model::box)
        {
            engine::draw_line(f, cx - r, cy - r, cx + r, cy - r, k_friction);
            engine::draw_line(f, cx + r, cy - r, cx + r, cy + r, k_friction);
            engine::draw_line(f, cx + r, cy + r, cx - r, cy + r, k_friction);
            engine::draw_line(f, cx - r, cy + r, cx - r, cy - r, k_friction);
        }

        // The current tangential impulse, in units of mu * normal impulse, so
        // the radius of the disc is exactly 1 whatever the numbers are.
        if (batch_.count > 0)
        {
            const auto& c = batch_.points[0];
            const float limit = mu_ * c.normal_impulse;
            if (limit > 1e-6f)
            {
                const float u = std::clamp(c.tangent_impulse[0] / limit, -1.6f, 1.6f);
                const float v = std::clamp(c.tangent_impulse[1] / limit, -1.6f, 1.6f);
                const int px = cx + static_cast<int>(u * static_cast<float>(r));
                const int py = cy - static_cast<int>(v * static_cast<float>(r));
                engine::draw_line(f, cx, cy, px, py, c.sliding ? k_slide : k_body);
                f.fill_rect(px - 2, py - 2, 5, 5, c.sliding ? k_slide : k_body);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Panel
    // -----------------------------------------------------------------------

    void panel()
    {
        ImGui::SetNextWindowPos(ImVec2(646.0f, 8.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(306.0f, 524.0f), ImGuiCond_Always);
        ImGui::Begin("impulse", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse);

        ImGui::Text("scene            %14s", name_of(scene_));
        ImGui::Text("restitution e    %14.2f", static_cast<double>(restitution_));
        ImGui::Text("friction mu      %14.2f", static_cast<double>(mu_));
        ImGui::Text("model            %14s", name_of(model_));
        ImGui::Text("solver passes    %14d", passes_);
        ImGui::Separator();

        ImGui::Text("bias        %s", bias_ ? "ON " : "off");
        ImGui::SameLine();
        ImGui::Text("threshold %s", threshold_on_ ? "ON " : "off");
        ImGui::Text("warm start  %s", warm_ ? "ON " : "off");
        ImGui::SameLine();
        ImGui::TextColored(normal_first_ ? ImVec4(0.6f, 0.8f, 0.6f, 1.0f)
                                         : ImVec4(1.0f, 0.5f, 0.4f, 1.0f),
                           "order %s", normal_first_ ? "N,F" : "F,N");
        ImGui::Separator();

        const float depth = manifold_.count > 0 ? manifold_.deepest() : 0.0f;
        ImGui::Text("contacts         %14d", manifold_.count);
        ImGui::Text("penetration      %11.3f mm", static_cast<double>(depth * 1000.0f));
        ImGui::Text("normal impulse   %11.3f Ns", static_cast<double>(report_.normal_impulse));
        ImGui::Text("friction impulse %11.3f Ns", static_cast<double>(report_.friction_impulse));
        ImGui::Text("sliding points   %14d", report_.sliding_points);
        ImGui::Text("residual         %11.2e m/s", static_cast<double>(report_.max_residual));
        ImGui::Separator();

        const vec3 v = body_.state.velocity;
        ImGui::Text("velocity      %6.2f %6.2f m/s", static_cast<double>(v.x),
                    static_cast<double>(v.y));
        ImGui::Text("omega z          %11.3f r/s", static_cast<double>(body_.angular_velocity.z));

        switch (scene_)
        {
        case scene::bounce: bounce_panel(); break;
        case scene::slope:  slope_panel(); break;
        case scene::roll:   roll_panel(); break;
        case scene::creep:  creep_panel(); break;
        }

        ImGui::Separator();
        ImGui::Text("[1-4] scene  [E/shift-E] e  [Up/Dn] mu");
        ImGui::Text("[F] model [L/R] passes [W] warm [O] order");
        ImGui::Text("[B] bias  [T] threshold  [G] ghosts");
        ImGui::Text("[Space] pause  [.] step  [R] reset");
        ImGui::End();
    }

    void bounce_panel()
    {
        ImGui::Separator();
        ImGui::Text("bounces          %14zu", peaks_.size());
        if (!peaks_.empty())
        {
            const float first = peaks_.front();
            const int n = static_cast<int>(peaks_.size()) - 1;
            const float predicted = engine::phys::bounce_height(first, restitution_, n);
            ImGui::Text("last peak        %11.3f m", static_cast<double>(peaks_.back()));
            ImGui::Text("e^(2n) * h0      %11.3f m", static_cast<double>(predicted));
        }
        const float hop = engine::phys::terminal_bounce_speed(restitution_, k_gravity / 60.0f);
        ImGui::Text("permanent hop    %11.1f mm",
                    static_cast<double>(hop * hop / (2.0f * k_gravity) * 1000.0f));
        ImGui::TextWrapped(bias_ ? "Bias ON: the ball tracks the ghost lines down to nothing."
                                 : "Bias OFF: restitution is applied to a speed that already "
                                   "contains this step's gravity, so the ball settles into a "
                                   "hop that never dies.");
    }

    void slope_panel()
    {
        ImGui::Separator();
        ImGui::Text("slope            %11.2f deg", static_cast<double>(ground_tilt_));
        ImGui::Text("atan(mu)         %11.2f deg",
                    static_cast<double>(engine::phys::critical_slope_degrees(mu_)));
        const vec3 down = engine::rotate(ground_orientation(), vec3{-1.0f, 0.0f, 0.0f});
        ImGui::Text("down-slope speed %11.3f m/s",
                    static_cast<double>(engine::dot(body_.state.velocity, down)));
        ImGui::TextWrapped("Raise mu past the slope angle and it stops. Press [O] to solve "
                           "friction first and it slides as though mu were zero.");
    }

    void roll_panel()
    {
        ImGui::Separator();
        const float omega_r = body_.angular_velocity.z * radius_;
        ImGui::Text("omega * r        %11.3f m/s", static_cast<double>(omega_r));
        ImGui::Text("slip             %11.3f m/s",
                    static_cast<double>(body_.state.velocity.x + omega_r));
        ImGui::Text("v / v0           %14.4f", static_cast<double>(body_.state.velocity.x / 4.0f));
        ImGui::Text("5/7              %14.4f",
                    static_cast<double>(engine::phys::rolling_speed_fraction(0.4f)));
        ImGui::TextWrapped("The end speed does not depend on mu at all — friction only "
                           "decides how long the transition takes.");
    }

    void creep_panel()
    {
        ImGui::Separator();
        ImGui::Text("start height     %11.3f m", static_cast<double>(drop_height_));
        ImGui::TextWrapped(passes_ >= 8
                               ? "Converged: the depth freezes at the penetration the crate "
                                 "arrived with, and no number of passes will repair it. That "
                                 "needs a POSITION correction — Lesson 8.10."
                               : "One pass over four points leaves a few mm/s of approach "
                                 "speed, and the crate sinks forever. Hold [Right].");
    }

    static constexpr float k_ground_half = 0.30f;

    engine::debug_ui ui_{};

    const char* shot_path_ = nullptr;
    float shot_at_ = 0.0f;

    scene scene_ = scene::bounce;
    bool running_ = true;
    bool single_step_ = false;
    bool ghosts_ = true;
    bool ball_ = true;
    bool bias_ = true;
    bool threshold_on_ = true;
    bool normal_first_ = true;
    bool warm_ = false;

    float t_ = 0.0f;
    float restitution_ = 0.70f;
    float mu_ = 0.50f;
    int passes_ = 1;
    friction_model model_ = friction_model::cone;

    float ground_tilt_ = 0.0f;
    float camera_x_ = 0.0f;
    float radius_ = 0.2f;
    float drop_height_ = 0.0f;
    float previous_y_ = 0.0f;
    bool rising_ = false;
    vec3 half_{0.45f, 0.45f, 0.45f};

    rigid_body body_{};
    rigid_body ground_{};
    shape body_shape_ = engine::phys::sphere_shape(0.2f);
    shape ground_shape_ = engine::phys::box_shape(vec3{8.0f, 0.30f, 8.0f});

    contact_manifold manifold_{};
    contact_manifold previous_{};
    bool have_previous_ = false;
    contact_batch batch_{};
    solve_report report_{};

    std::vector<vec3> trace_;
    std::vector<float> peaks_;
};

} // namespace

ENGINE_MAIN(impulse_app)
