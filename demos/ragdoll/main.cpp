// demos/ragdoll/main.cpp — a character that is animated, then dropped, then
// animated again.
//
// Lesson 8.12. `joints` showed 8.11's rows one at a time, in a plane. This one
// puts all of them on a skeleton — 7.7's 23-joint humanoid, eleven capsules,
// four hinges and six cone-twist sockets — and hands the character from the
// animation system to the solver and back.
//
//   THE VIEW is an orthographic three-quarter view from the character's front
//     right, following it. A capsule is drawn as its silhouette; the skeleton
//     the skinning code would see is drawn through it in thin grey, INCLUDING
//     the passengers (clavicles, hands, feet, toes) that ride on the parts.
//   A PART'S COLOUR says who owns it: steel blue while the clip does
//     (kinematic, steered), warm while the solver does (dynamic), dimmed while
//     its island sleeps. A joint drawn red is more than two degrees outside a
//     limit.
//
// WHAT TO WATCH FOR, IN ORDER.
//
//   [1] TRIP. The jog, steered — every body lands exactly on its animated pose
//     each step, by velocity, never by teleport — toward a row of crates that
//     fell asleep before the character got there. It runs THROUGH nothing:
//     8.12 fixed the solver so a moving kinematic body wakes what it touches
//     (8.10's islands never let one). At 1.2 s, or on [T], it trips: the bodies
//     become dynamic with the velocities they already had, and the character
//     carries on forward as it falls. [Z] hands over at rest instead — watch it
//     stop dead in mid-stride and then topple. Three seconds later, or on [G],
//     it gets up: the clip is realigned over where the pelvis lies ([A] turns
//     that off, and the whole character slides back to where it tripped), and
//     the pose is blended from the one read off the bodies, so nothing jumps.
//   [M] swaps the jog for 7.7's walk, which bends the knees FORWARD and twists
//     the forearms off their elbows — handed over, the limbs snap back inside
//     their joints. [C] cycles the correction: under Baumgarte they snap back
//     with a real velocity and keep it.
//   [2] HANG. The whole character hanging from one hand: 80 kg through a
//     1.76 kg forearm. At eight sweeps the joints visibly open (the panel reads
//     millimetres); [U] spends the same work as eight sub-steps of one sweep
//     and they close to a micrometre.
//   [3] PILE. Five ragdolls dropped on one another, coloured by island. They
//     settle to millijoules — and do not all fall asleep, which 8.12 §13 says
//     plainly: 8.10's thresholds were tuned on crates.
//
//   [1]..[3]  scenes          [Space] pause   [.] single step   [R] reset
//   [T] trip now   [G] get up now   [M] motion (jog / 7.7's walk)
//   [Z] hand over at rest     [A] realign on return
//   [Left] [Right]  velocity iterations   [C] position correction
//   [U] sub-steps (1 / 8)     [Esc] quit
//
//     cmake --build build --target ragdoll
//     ./build/demos/ragdoll
//     ./build/demos/ragdoll --scene 1 --t 1.6 --shot out.ppm
//     ./build/demos/ragdoll --scene 1 --walk77 --correction baumgarte --t 1.5 --shot out.ppm
//
// `engine::engine` directly and NOT `demo_common`, like every physics demo
// before it: it loads nothing and computes everything on screen, so a `--shot`
// run is byte-for-byte reproducible on any machine. The humanoid, its ragdoll
// description and both motions are the harness's, copied: content, not engine.

#include <engine/anim/skeleton.hpp>
#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/broadphase.hpp>
#include <engine/phys/constraint.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/manifold.hpp>
#include <engine/phys/ragdoll.hpp>
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

using engine::mat4;
using engine::quat;
using engine::transform;
using engine::vec3;
using engine::anim::joint_index;
using engine::anim::k_no_parent;
using engine::anim::skeleton;
using namespace engine::phys;

constexpr int k_width = 960;
constexpr int k_height = 540;

/// This demo's own log category, in the range Lesson 5.3 reserved for programs
/// outside the library.
constexpr int log_demo = 72;

constexpr float k_pi = 3.14159265358979323846f;
[[nodiscard]] float rad(float degrees) { return degrees * (k_pi / 180.0f); }

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_grid       = engine::pack_argb(36, 40, 50);
constexpr Uint32 k_animated   = engine::pack_argb(120, 170, 230);
constexpr Uint32 k_simulated  = engine::pack_argb(240, 170, 110);
constexpr Uint32 k_asleep     = engine::pack_argb(110, 96, 84);
constexpr Uint32 k_bone       = engine::pack_argb(150, 150, 160);
constexpr Uint32 k_joint_ok   = engine::pack_argb(255, 210, 90);
constexpr Uint32 k_joint_bad  = engine::pack_argb(240, 70, 60);
constexpr Uint32 k_crate      = engine::pack_argb(150, 190, 120);
constexpr Uint32 k_crate_zzz  = engine::pack_argb(80, 100, 70);
constexpr Uint32 k_fixed      = engine::pack_argb(110, 116, 130);

constexpr int k_view_x0 = 8;
constexpr int k_view_y0 = 8;
constexpr int k_view_x1 = 631;
constexpr int k_view_y1 = 531;

/// The island palette `stack` and `joints` use: an island index is a label.
constexpr Uint32 k_island_colours[8] = {
    engine::pack_argb(235, 120, 110), engine::pack_argb(120, 200, 235),
    engine::pack_argb(150, 225, 130), engine::pack_argb(240, 190, 100),
    engine::pack_argb(200, 140, 235), engine::pack_argb(120, 230, 200),
    engine::pack_argb(235, 150, 190), engine::pack_argb(180, 200, 120),
};

// ---------------------------------------------------------------------------
// The humanoid, its ragdoll, and two motions — the harness's, copied
// ---------------------------------------------------------------------------

struct joint_spec
{
    const char* name;
    int parent;
    vec3 offset;
};

/// 7.7's 23-joint humanoid: a 1.8 m person, origin between the feet, T-posed.
const joint_spec k_rig[] = {
    {"root", -1, {0.00f, 0.00f, 0.00f}},        {"hips", 0, {0.00f, 0.95f, 0.00f}},
    {"spine1", 1, {0.00f, 0.12f, 0.00f}},       {"spine2", 2, {0.00f, 0.14f, 0.00f}},
    {"chest", 3, {0.00f, 0.16f, 0.00f}},        {"neck", 4, {0.00f, 0.18f, 0.00f}},
    {"head", 5, {0.00f, 0.10f, 0.00f}},         {"clavicle.L", 4, {0.05f, 0.14f, 0.00f}},
    {"upperarm.L", 7, {0.13f, 0.00f, 0.00f}},   {"forearm.L", 8, {0.28f, 0.00f, 0.00f}},
    {"hand.L", 9, {0.25f, 0.00f, 0.00f}},       {"clavicle.R", 4, {-0.05f, 0.14f, 0.00f}},
    {"upperarm.R", 11, {-0.13f, 0.00f, 0.00f}}, {"forearm.R", 12, {-0.28f, 0.00f, 0.00f}},
    {"hand.R", 13, {-0.25f, 0.00f, 0.00f}},     {"thigh.L", 1, {0.09f, -0.05f, 0.00f}},
    {"shin.L", 15, {0.00f, -0.42f, 0.00f}},     {"foot.L", 16, {0.00f, -0.41f, 0.00f}},
    {"toe.L", 17, {0.00f, -0.06f, 0.12f}},      {"thigh.R", 1, {-0.09f, -0.05f, 0.00f}},
    {"shin.R", 19, {0.00f, -0.42f, 0.00f}},     {"foot.R", 20, {0.00f, -0.41f, 0.00f}},
    {"toe.R", 21, {0.00f, -0.06f, 0.12f}},
};
constexpr std::size_t k_joints = sizeof(k_rig) / sizeof(k_rig[0]);

[[nodiscard]] skeleton build_rig()
{
    skeleton sk;
    sk.joints.resize(k_joints);
    for (std::size_t j = 0; j < k_joints; ++j)
    {
        sk.joints[j].name = k_rig[j].name;
        sk.joints[j].parent = k_rig[j].parent < 0 ? k_no_parent : static_cast<joint_index>(k_rig[j].parent);
        sk.joints[j].local_bind.position = k_rig[j].offset;
    }
    engine::anim::bake_inverse_binds(sk);
    return sk;
}

constexpr float k_mass = 80.0f;
constexpr int k_parts = 11;
constexpr int p_fore_l = 4;

/// Eleven parts; Dempster's mass fractions (Winter, Table 4.1). 8.12 §6.
[[nodiscard]] ragdoll_desc humanoid_desc()
{
    ragdoll_desc d;
    d.mass = k_mass;
    auto part = [&](const char* name, joint_index bone, vec3 from, vec3 to, float r, float fraction) {
        ragdoll_part_desc p;
        p.name = name;
        p.bone = bone;
        p.from = from;
        p.to = to;
        p.radius = r;
        p.mass_fraction = fraction;
        d.parts.push_back(p);
        return &d.parts.back();
    };
    auto cone = [](ragdoll_part_desc* p, vec3 twist, vec3 cone_axis, float swing_deg, float twist_deg) {
        p->link = ragdoll_link::cone_twist;
        p->axis = twist;
        p->cone_axis = cone_axis;
        p->swing = rad(swing_deg);
        p->lower = -rad(twist_deg);
        p->upper = rad(twist_deg);
    };
    auto hinge = [](ragdoll_part_desc* p, vec3 axis, float lo_deg, float hi_deg) {
        p->link = ragdoll_link::hinge;
        p->axis = axis;
        p->lower = rad(lo_deg);
        p->upper = rad(hi_deg);
    };
    part("pelvis", 1, {-0.07f, 0.95f, 0.0f}, {0.07f, 0.95f, 0.0f}, 0.11f, 0.142f);
    cone(part("torso", 2, {0.0f, 1.20f, 0.0f}, {0.0f, 1.40f, 0.0f}, 0.15f, 0.355f),
         {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.2f}, 25.0f, 30.0f);
    cone(part("head", 5, {0.0f, 1.64f, 0.0f}, {0.0f, 1.70f, 0.0f}, 0.10f, 0.081f),
         {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.3f}, 35.0f, 60.0f);
    cone(part("upperarm.L", 8, {0.22f, 1.51f, 0.0f}, {0.42f, 1.51f, 0.0f}, 0.045f, 0.028f),
         {1.0f, 0.0f, 0.0f}, {0.7f, -0.7f, 0.35f}, 85.0f, 60.0f);
    hinge(part("forearm.L", 9, {0.505f, 1.51f, 0.0f}, {0.74f, 1.51f, 0.0f}, 0.04f, 0.022f),
          {0.0f, -1.0f, 0.0f}, 0.0f, 145.0f);
    cone(part("upperarm.R", 12, {-0.22f, 1.51f, 0.0f}, {-0.42f, 1.51f, 0.0f}, 0.045f, 0.028f),
         {-1.0f, 0.0f, 0.0f}, {-0.7f, -0.7f, 0.35f}, 85.0f, 60.0f);
    hinge(part("forearm.R", 13, {-0.505f, 1.51f, 0.0f}, {-0.74f, 1.51f, 0.0f}, 0.04f, 0.022f),
          {0.0f, 1.0f, 0.0f}, 0.0f, 145.0f);
    cone(part("thigh.L", 15, {0.09f, 0.83f, 0.0f}, {0.09f, 0.55f, 0.0f}, 0.07f, 0.100f),
         {0.0f, -1.0f, 0.0f}, {0.15f, -1.0f, 0.9f}, 80.0f, 35.0f);
    hinge(part("shin.L", 16, {0.09f, 0.43f, 0.0f}, {0.09f, 0.12f, 0.0f}, 0.05f, 0.061f),
          {1.0f, 0.0f, 0.0f}, 0.0f, 150.0f);
    cone(part("thigh.R", 19, {-0.09f, 0.83f, 0.0f}, {-0.09f, 0.55f, 0.0f}, 0.07f, 0.100f),
         {0.0f, -1.0f, 0.0f}, {-0.15f, -1.0f, 0.9f}, 80.0f, 35.0f);
    hinge(part("shin.R", 20, {-0.09f, 0.43f, 0.0f}, {-0.09f, 0.12f, 0.0f}, 0.05f, 0.061f),
          {1.0f, 0.0f, 0.0f}, 0.0f, 150.0f);
    return d;
}

constexpr float k_jog_cycle = 0.7f;
constexpr float k_jog_speed = 2.5f;

/// The jog written for 8.12: arms hang, elbows and knees bend the right way,
/// and every joint it rotates is a part. It does not travel; the placement does.
[[nodiscard]] transform jog_pose(std::size_t j, float t)
{
    const float w = 2.0f * k_pi * t / k_jog_cycle;
    const float s = std::sin(w);
    transform x;
    x.position = k_rig[j].offset;
    switch (j)
    {
    case 1:
        x.position = x.position + vec3{0.0f, 0.03f * std::sin(2.0f * w), 0.0f};
        x.rotation = engine::quat_y(rad(5.0f) * s);
        break;
    case 2: x.rotation = engine::quat_y(-rad(7.0f) * s) * engine::quat_x(rad(8.0f)); break;
    case 5: x.rotation = engine::quat_x(-rad(6.0f)); break;
    case 8: x.rotation = engine::quat_x(rad(30.0f) * s) * engine::quat_z(-rad(75.0f)); break;
    case 12: x.rotation = engine::quat_x(-rad(30.0f) * s) * engine::quat_z(rad(75.0f)); break;
    case 9:
        x.rotation = engine::quat_from_axis_angle({0.0f, -1.0f, 0.0f}, rad(70.0f + 20.0f * std::sin(w + 0.5f)));
        break;
    case 13:
        x.rotation = engine::quat_from_axis_angle({0.0f, 1.0f, 0.0f}, rad(70.0f - 20.0f * std::sin(w + 0.5f)));
        break;
    case 15: x.rotation = engine::quat_x(-rad(20.0f + 30.0f * s)); break;
    case 19: x.rotation = engine::quat_x(-rad(20.0f - 30.0f * s)); break;
    case 16: x.rotation = engine::quat_x(rad(45.0f + 40.0f * std::sin(w - 1.2f))); break;
    case 20: x.rotation = engine::quat_x(rad(45.0f - 40.0f * std::sin(w - 1.2f))); break;
    default: break;
    }
    return x;
}

/// 7.7's walk, verbatim — knees bent FORWARD, forearms twisted off the elbow.
[[nodiscard]] transform walk77_pose(std::size_t j, float t)
{
    const float w = 2.0f * k_pi * t;
    transform x;
    x.position = k_rig[j].offset;
    auto swing = [&](float amp_deg, float bias_deg, float offset, vec3 axis) {
        return engine::quat_from_axis_angle(engine::normalised(axis), rad(bias_deg + amp_deg * std::sin(w + offset)));
    };
    switch (j)
    {
    case 0: x.position = x.position + vec3{0.0f, 0.035f * std::sin(2.0f * w), 1.2f * t}; break;
    case 1: x.rotation = swing(3.0f, 0.0f, 0.0f, {0, 1, 0}); break;
    case 2: x.rotation = swing(2.0f, 1.0f, k_pi, {1, 0, 0}); break;
    case 3: x.rotation = swing(1.5f, 1.0f, k_pi, {1, 0, 0}); break;
    case 4: x.rotation = swing(2.5f, 0.0f, 0.5f, {0, 1, 0}); break;
    case 5: x.rotation = swing(1.0f, 0.0f, 0.0f, {0, 1, 0}); break;
    case 8: x.rotation = swing(22.0f, 0.0f, k_pi, {1, 0, 0}); break;
    case 9: x.rotation = swing(14.0f, -18.0f, k_pi * 0.5f, {1, 0, 0}); break;
    case 12: x.rotation = swing(22.0f, 0.0f, 0.0f, {1, 0, 0}); break;
    case 13: x.rotation = swing(14.0f, -18.0f, -k_pi * 0.5f, {1, 0, 0}); break;
    case 15: x.rotation = swing(28.0f, 0.0f, 0.0f, {1, 0, 0}); break;
    case 16: x.rotation = swing(24.0f, -24.0f, -1.1f, {1, 0, 0}); break;
    case 17: x.rotation = swing(16.0f, 0.0f, 2.0f, {1, 0, 0}); break;
    case 19: x.rotation = swing(28.0f, 0.0f, k_pi, {1, 0, 0}); break;
    case 20: x.rotation = swing(24.0f, -24.0f, k_pi - 1.1f, {1, 0, 0}); break;
    case 21: x.rotation = swing(16.0f, 0.0f, k_pi + 2.0f, {1, 0, 0}); break;
    default: break;
    }
    return x;
}

using pose_fn = transform (*)(std::size_t, float);

[[nodiscard]] transform bind_pose(std::size_t j, float)
{
    transform x;
    x.position = k_rig[j].offset;
    return x;
}

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

enum class scene
{
    trip,
    hang,
    pile
};

const char* name_of(scene s)
{
    switch (s)
    {
    case scene::trip: return "trip";
    case scene::hang: return "hang";
    case scene::pile: return "pile";
    }
    return "?";
}

/// What the character in [1] is doing.
enum class phase
{
    running,     ///< animated: the clip owns the bodies
    fallen,      ///< simulated: the solver owns them
    getting_up,  ///< animated again, blending from the read-back pose
};

const char* name_of(phase p)
{
    switch (p)
    {
    case phase::running:    return "running (clip owns the bodies)";
    case phase::fallen:     return "fallen (solver owns the bodies)";
    case phase::getting_up: return "getting up (blending from the read-back pose)";
    }
    return "?";
}

/// A placed primitive by value, so `as_convex` has an lvalue to view. The
/// scene dispatch 8.10 and 8.11 wrote for spheres and boxes, with capsules.
struct placed
{
    shape_kind kind = shape_kind::sphere;
    engine::sphere s{};
    obb b{};
    capsule c{};

    [[nodiscard]] convex view() const
    {
        switch (kind)
        {
        case shape_kind::sphere:  return as_convex(s);
        case shape_kind::box:     return as_convex(b);
        case shape_kind::capsule: return as_convex(c);
        }
        return as_convex(s);
    }
};

[[nodiscard]] placed place(const shape& sh, const rigid_body& body)
{
    placed p;
    p.kind = sh.kind;
    switch (sh.kind)
    {
    case shape_kind::sphere:  p.s = world_sphere(sh, body.state.position); break;
    case shape_kind::box:     p.b = world_obb(sh, body.state.position, body.orientation); break;
    case shape_kind::capsule: p.c = world_capsule(sh, body.state.position, body.orientation); break;
    }
    return p;
}

class ragdoll_app final : public engine::app
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
            else if (arg == "--iters" && i + 1 < argc)
            {
                cfg_.velocity_iterations = std::clamp(std::atoi(argv[++i]), 1, 128);
            }
            else if (arg == "--substeps" && i + 1 < argc) { substeps_ = std::clamp(std::atoi(argv[++i]), 1, 16); }
            else if (arg == "--walk77") { motion_ = walk77_pose; }
            else if (arg == "--at-rest") { at_rest_ = true; }
            else if (arg == "--no-realign") { realign_ = false; }
            else if (arg == "--correction" && i + 1 < argc)
            {
                const std::string_view c{argv[++i]};
                cfg_.correction = c == "baumgarte" ? position_correction::baumgarte
                                  : c == "none"    ? position_correction::none
                                                   : position_correction::split_impulse;
            }
        }
        return {.title = "ragdoll — a skeleton handed to the solver and back",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    bool on_start() override
    {
        bp_.cell_size = 0.5f;
        reset_scene();
        ENGINE_LOG_INFO(log_demo, "ragdoll demo: scene %s, %d iterations, %d sub-steps", name_of(scene_),
                        cfg_.velocity_iterations, substeps_);
        return ui_.start(window(), renderer()) || shot_path_ != nullptr;
    }

    void on_event(const SDL_Event& event) override
    {
        (void)ui_.handle_event(event);
        if (event.type != SDL_EVENT_KEY_DOWN) { return; }

        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_1: scene_ = scene::trip; reset_scene(); break;
        case SDL_SCANCODE_2: scene_ = scene::hang; reset_scene(); break;
        case SDL_SCANCODE_3: scene_ = scene::pile; reset_scene(); break;
        case SDL_SCANCODE_T: if (phase_ == phase::running) { trip(); } break;
        case SDL_SCANCODE_G: if (phase_ == phase::fallen) { get_up(); } break;
        case SDL_SCANCODE_M:
            motion_ = motion_ == jog_pose ? walk77_pose : jog_pose;
            reset_scene();
            break;
        case SDL_SCANCODE_Z: at_rest_ = !at_rest_; break;
        case SDL_SCANCODE_A: realign_ = !realign_; break;
        case SDL_SCANCODE_RIGHT: cfg_.velocity_iterations = std::min(cfg_.velocity_iterations * 2, 128); break;
        case SDL_SCANCODE_LEFT: cfg_.velocity_iterations = std::max(cfg_.velocity_iterations / 2, 1); break;
        case SDL_SCANCODE_C:
            cfg_.correction = static_cast<position_correction>((static_cast<int>(cfg_.correction) + 1) % 3);
            break;
        case SDL_SCANCODE_U: substeps_ = substeps_ == 1 ? 8 : 1; break;
        case SDL_SCANCODE_R: reset_scene(); break;
        case SDL_SCANCODE_SPACE: running_ = !running_; break;
        case SDL_SCANCODE_PERIOD: running_ = false; single_step_ = true; break;
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

    /// Where `rd`'s bodies belong for `pose` placed by `place`.
    void targets_for(const ragdoll& rd, pose_fn pose, float t, const mat4& place)
    {
        local_.resize(k_joints);
        for (std::size_t j = 0; j < k_joints; ++j) { local_[j] = pose(j, t); }
        engine::anim::compose_pose(sk_, local_, posed_);
        part_targets(rd, posed_, place, targets_);
    }

    /// Build a ragdoll from the shared description, spawn it kinematic at
    /// `pose` placed by `place`, and give the scene its shapes and filter.
    void add_ragdoll(pose_fn pose, float t, const mat4& place)
    {
        ragdolls_.emplace_back();
        ragdoll& rd = ragdolls_.back();
        (void)build_ragdoll(sk_, humanoid_desc(), rd);
        targets_for(rd, pose, t, place);
        (void)spawn(rd, world_, targets_);
        for (const ragdoll_part& p : rd.parts) { shapes_.push_back(p.collider); }
        exclude_pairs(rd, filter_);
    }

    void reset_scene()
    {
        t_ = 0.0f;
        world_.clear();
        shapes_.clear();
        links_.clear();
        filter_.clear();
        cache_.clear();
        solver_.clear();
        manifolds_.clear();
        ragdolls_.clear();
        ragdolls_.reserve(8);   // `add_joints` keeps pointers into the parts
        stats_ = solver_stats{};
        sleep_ = sleep_config{};
        phase_ = phase::running;
        run_t_ = 0.0f;
        clip_t_ = 0.0f;
        place_ = mat4::identity();

        add(make_fixed(vec3{0.0f, -0.5f, 0.0f}), box_shape(vec3{60.0f, 0.5f, 60.0f}));

        switch (scene_)
        {
        case scene::trip: build_trip(); break;
        case scene::hang: build_hang(); break;
        case scene::pile: build_pile(); break;
        }
        filter_.finalize();
    }

    void build_trip()
    {
        add_ragdoll(motion_, 0.0f, model_now());
        // A row of crates in the path, asleep long before the character
        // arrives — which is exactly what 8.10's islands could not wake.
        for (int k = 0; k < 4; ++k)
        {
            const float z = 2.2f + 2.4f * static_cast<float>(k);
            add(make_box(vec3{(k % 2 == 0 ? 0.0f : 0.2f), 0.25f, z}, 5.0f, vec3{0.25f, 0.25f, 0.25f}),
                box_shape(vec3{0.25f, 0.25f, 0.25f}));
        }
        trip_at_ = 1.2f;
    }

    void build_hang()
    {
        sleep_.enabled = false;
        const mat4 lift = engine::translation(vec3{0.0f, 1.0f, 0.0f});
        add_ragdoll(bind_pose, 0.0f, lift);
        ragdoll& rd = ragdolls_.back();
        simulate(rd, world_.bodies(), local_);
        const rigid_body& fore = body(rd.first_body + p_fore_l);
        const vec3 hand = fore.state.position + engine::rotate(fore.orientation, vec3{0.0f, 0.1175f, 0.0f});
        const std::uint32_t bar = add(make_fixed(hand), box_shape(vec3{0.02f, 0.02f, 0.6f}));
        links_.push_back({bar, rd.first_body + p_fore_l, make_ball_socket(body(bar), body(rd.first_body + p_fore_l), hand)});
        filter_.exclude(bar, rd.first_body + p_fore_l);
        for (std::uint32_t i = 0; i < k_parts; ++i)
        {
            body(rd.first_body + i).damping = 1.0f;
            body(rd.first_body + i).angular_damping = 1.0f;
        }
        phase_ = phase::fallen;
    }

    void build_pile()
    {
        for (int n = 0; n < 5; ++n)
        {
            const float y = 0.2f + 0.45f * static_cast<float>(n);
            const quat lie = engine::quat_y(rad(37.0f * static_cast<float>(n))) * engine::quat_x(rad(-90.0f));
            const mat4 place = engine::translation(vec3{0.1f * static_cast<float>(n % 2), y, 0.0f})
                               * engine::to_mat4(engine::mat3_from_quat(lie))
                               * engine::translation(vec3{0.0f, -0.95f, 0.0f});
            add_ragdoll(bind_pose, 0.0f, place);
            simulate(ragdolls_.back(), world_.bodies(), local_);
        }
        phase_ = phase::fallen;
    }

    [[nodiscard]] mat4 model_now() const
    {
        // The jog does not travel in its pose, so the placement carries it.
        // 7.7's walk carries its own root travel, so its placement stays put.
        return motion_ == jog_pose ? place_ * engine::translation(vec3{0.0f, 0.0f, k_jog_speed * run_t_}) : place_;
    }

    // -----------------------------------------------------------------------
    // The handoffs
    // -----------------------------------------------------------------------

    void trip()
    {
        ragdoll& rd = ragdolls_.front();
        for (std::size_t j = 0; j < k_joints; ++j) { local_[j] = motion_(j, clip_t_); }
        simulate(rd, world_.bodies(), local_);
        if (at_rest_)
        {
            // The control: hand over with no velocity. The character stops
            // dead in mid-stride, then topples.
            for (std::uint32_t i = 0; i < k_parts; ++i)
            {
                body(rd.first_body + i).state.velocity = vec3{};
                body(rd.first_body + i).angular_velocity = vec3{};
            }
        }
        tripped_at_ = model_now();
        phase_ = phase::fallen;
        phase_t_ = 0.0f;
    }

    void get_up()
    {
        ragdoll& rd = ragdolls_.front();

        // The pose to return to, and where to put it: over the pelvis, unless
        // [A] says otherwise.
        target_.resize(k_joints);
        for (std::size_t j = 0; j < k_joints; ++j) { target_[j] = jog_pose(j, 0.0f); }
        engine::anim::compose_pose(sk_, target_, posed_);
        const vec3 hips = engine::translation_of(posed_[1]);
        place_ = realign_ ? realign_model(rd, world_.bodies(), tripped_at_, hips) : tripped_at_;

        read_pose(rd, sk_, world_.bodies(), place_, start_);
        animate(rd, world_.bodies());
        motion_ = jog_pose;   // whatever tripped, the character gets up jogging
        phase_ = phase::getting_up;
        phase_t_ = 0.0f;
    }

    // -----------------------------------------------------------------------
    // The step
    // -----------------------------------------------------------------------

    void advance(float h)
    {
        phase_t_ += h;
        if (scene_ == scene::trip)
        {
            ragdoll& rd = ragdolls_.front();
            switch (phase_)
            {
            case phase::running:
                // Aim at where the clip will be at the END of this step, and
                // let the step carry the bodies there.
                run_t_ += h;
                clip_t_ += h;
                targets_for(rd, motion_, clip_t_, model_now());
                steer(rd, world_.bodies(), targets_, h);
                if (run_t_ >= trip_at_) { trip(); }
                break;
            case phase::fallen:
                if (phase_t_ >= 3.0f) { get_up(); }
                break;
            case phase::getting_up:
            {
                const float w = std::min(1.0f, phase_t_ / k_get_up);
                pose_.resize(k_joints);
                for (std::size_t j = 0; j < k_joints; ++j)
                {
                    pose_[j] = engine::transform_blend_slerp(start_[j], target_[j], w);
                }
                engine::anim::compose_pose(sk_, pose_, posed_);
                part_targets(rd, posed_, place_, targets_);
                steer(rd, world_.bodies(), targets_, h);
                if (w >= 1.0f)
                {
                    phase_ = phase::running;
                    run_t_ = 0.0f;
                    clip_t_ = 0.0f;
                    trip_at_ = 2.5f;
                }
                break;
            }
            }
        }

        const float hs = h / static_cast<float>(substeps_);
        solver_config cfg = cfg_;
        if (substeps_ > 1) { cfg.velocity_iterations = std::max(1, cfg_.velocity_iterations / substeps_); }
        const auto t0 = std::chrono::steady_clock::now();
        for (int s = 0; s < substeps_; ++s) { step(hs, cfg); }
        ms_step_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() * 1000.0;
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
        for (const broadphase_pair& p : grid_.pairs())
        {
            if (bodies[p.a].kind != body_kind::dynamic && bodies[p.b].kind != body_kind::dynamic) { continue; }
            if (filter_.excluded(p.a, p.b)) { continue; }
            const placed x = place(shapes_[p.a], bodies[p.a]);
            const placed y = place(shapes_[p.b], bodies[p.b]);
            contact_manifold m = collide_manifold(x.view(), y.view(), mf_);
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
        for (auto& l : links_) { solver_.add(l.a, l.b, l.j); }
        for (ragdoll& rd : ragdolls_) { add_joints(rd, solver_); }
        stats_ = solver_.solve(h, cfg, sleep_);
        for (std::size_t i = 0; i < manifolds_.size(); ++i) { cache_.store(keys_[i], manifolds_[i]); }
        cache_.end_frame();

        world_.integrate_positions(h);
    }

    // -----------------------------------------------------------------------
    // Drawing: an orthographic three-quarter view that follows the character
    // -----------------------------------------------------------------------

    static constexpr float k_scale = 118.0f;   // pixels per metre

    /// The view's axes: world +z goes mostly right, +y up, and the camera
    /// looks along `forward` from the character's front right.
    [[nodiscard]] static vec3 right_axis() { return engine::normalised(vec3{0.45f, 0.0f, 1.0f}); }
    [[nodiscard]] static vec3 up_axis()
    {
        return engine::normalised(engine::cross(vec3{0.45f, 0.0f, 1.0f}, vec3{1.0f, -0.3f, -0.45f}));
    }

    [[nodiscard]] float px(vec3 p) const
    {
        return 0.5f * static_cast<float>(k_view_x0 + k_view_x1) + engine::dot(p - centre_, right_axis()) * k_scale;
    }
    [[nodiscard]] float py(vec3 p) const
    {
        // The followed point sits a little below the middle of the view: a
        // standing character spans about a metre above and below it.
        return 0.5f * static_cast<float>(k_view_y0 + k_view_y1) + 40.0f
               - engine::dot(p - centre_, up_axis()) * k_scale;
    }

    /// Liang–Barsky against the view rectangle — `stack`'s and `joints`', unchanged.
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

    /// A capsule's silhouette in an orthographic view is a 2D capsule: the two
    /// projected end points, their circles, and the two tangents between them.
    void draw_capsule(const rigid_body& b, const shape& s, Uint32 colour)
    {
        const capsule c = world_capsule(s, b.state.position, b.orientation);
        const vec3 e0 = c.end(-1);
        const vec3 e1 = c.end(1);
        const float x0 = px(e0);
        const float y0 = py(e0);
        const float x1 = px(e1);
        const float y1 = py(e1);
        const float r = c.radius * k_scale;
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

    [[nodiscard]] Uint32 part_colour(std::size_t index) const
    {
        const rigid_body& b = world_.bodies()[index];
        if (b.kind == body_kind::kinematic) { return k_animated; }
        if (b.sleeping) { return k_asleep; }
        if (scene_ == scene::pile)
        {
            const int isl = index < labels_.size() ? labels_[index] : -1;
            return isl >= 0 ? k_island_colours[static_cast<std::size_t>(isl) % 8] : k_simulated;
        }
        return k_simulated;
    }

    void draw_world()
    {
        // Follow the first ragdoll's pelvis in the ground plane.
        if (!ragdolls_.empty())
        {
            const vec3 p = world_.bodies()[ragdolls_.front().first_body].state.position;
            centre_ = vec3{p.x, 0.9f, p.z};
            if (scene_ == scene::hang) { centre_.y = 1.4f; }
        }

        // A one-metre grid, so that motion reads as motion.
        const float gx = std::floor(centre_.x);
        const float gz = std::floor(centre_.z);
        for (int k = -6; k <= 6; ++k)
        {
            const float f = static_cast<float>(k);
            line(vec3{gx - 6.0f, 0.0f, gz + f}, vec3{gx + 6.0f, 0.0f, gz + f}, k_grid);
            line(vec3{gx + f, 0.0f, gz - 6.0f}, vec3{gx + f, 0.0f, gz + 6.0f}, k_grid);
        }

        auto bodies = world_.bodies();
        for (std::size_t i = 1; i < bodies.size(); ++i)
        {
            if (shapes_[i].kind == shape_kind::capsule) { draw_capsule(bodies[i], shapes_[i], part_colour(i)); }
            else if (shapes_[i].kind == shape_kind::box)
            {
                const Uint32 c = bodies[i].kind == body_kind::fixed ? k_fixed
                                 : bodies[i].sleeping               ? k_crate_zzz
                                                                    : k_crate;
                draw_box(bodies[i], shapes_[i], c);
            }
        }

        for (const ragdoll& rd : ragdolls_) { draw_skeleton_and_joints(rd); }
    }

    /// The skeleton the skinning code would see — from the clip while the clip
    /// owns the bodies, and from `read_pose` while the solver does — and each
    /// joint's anchor, red if it is more than two degrees outside a limit.
    void draw_skeleton_and_joints(const ragdoll& rd)
    {
        // While the clip owns the bodies, the skeleton is the clip's own pose.
        // While the solver owns them it is `read_pose`'s, and the frame it is
        // read into cancels out of every world position drawn here (only a
        // joint hung directly off the root would notice, and none is drawn).
        mat4 frame = mat4::identity();
        if (scene_ == scene::trip && phase_ == phase::running)
        {
            frame = model_now();
            drawn_ = local_;
        }
        else
        {
            read_pose(rd, sk_, world_.bodies(), frame, drawn_);
        }
        engine::anim::compose_pose(sk_, drawn_, posed_);
        for (std::size_t j = 1; j < k_joints; ++j)
        {
            const joint_index p = sk_.joints[j].parent;
            if (p == k_no_parent || p == 0) { continue; }
            const vec3 a = engine::translation_of(frame * posed_[p]);
            const vec3 b = engine::translation_of(frame * posed_[j]);
            line(a, b, k_bone);
        }
        for (std::size_t i = 0; i < rd.parts.size(); ++i)
        {
            const ragdoll_part& p = rd.parts[i];
            if (p.kind == ragdoll_link::root) { continue; }
            const rigid_body& a = world_.bodies()[rd.first_body + static_cast<std::size_t>(p.parent)];
            const rigid_body& b = world_.bodies()[rd.first_body + i];
            const vec3 at = world_point_of(b, p.link.anchor_b);
            const bool bad = measure_joint(p.link, a, b).angular > rad(2.0f);
            const float s = 0.03f;
            const Uint32 c = bad ? k_joint_bad : k_joint_ok;
            line(at - vec3{s, 0.0f, 0.0f}, at + vec3{s, 0.0f, 0.0f}, c);
            line(at - vec3{0.0f, s, 0.0f}, at + vec3{0.0f, s, 0.0f}, c);
        }
    }

    // -----------------------------------------------------------------------
    // Panel
    // -----------------------------------------------------------------------

    void panel()
    {
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(k_view_x1 + 16), 8.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 520.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("ragdoll");

        ImGui::Text("scene: %s   t = %.2f s", name_of(scene_), static_cast<double>(t_));
        if (scene_ == scene::trip)
        {
            ImGui::TextWrapped("%s", name_of(phase_));
            ImGui::Text("motion               %s", motion_ == jog_pose ? "the jog" : "7.7's walk");
            ImGui::Text("handoff velocities   %s", at_rest_ ? "ZERO (control)" : "the chord");
            ImGui::Text("realign on return    %s", realign_ ? "on" : "OFF");
        }
        ImGui::Separator();
        ImGui::Text("velocity iterations  %d", cfg_.velocity_iterations);
        ImGui::Text("sub-steps            %d", substeps_);
        ImGui::Text("correction           %s", name_of(cfg_.correction));
        ImGui::Separator();

        joint_error worst{};
        double ke = 0.0;
        for (const ragdoll& rd : ragdolls_)
        {
            const joint_error e = worst_joint_error(rd, world_.bodies());
            worst.linear = std::max(worst.linear, e.linear);
            worst.angular = std::max(worst.angular, e.angular);
            for (std::size_t i = 0; i < rd.parts.size(); ++i)
            {
                ke += static_cast<double>(kinetic_energy(world_.bodies()[rd.first_body + i]));
            }
        }
        for (const auto& l : links_)
        {
            worst.linear = std::max(worst.linear, measure_joint(l.j, body(l.a), body(l.b)).linear);
        }
        ImGui::Text("worst joint gap      %.3f mm", static_cast<double>(worst.linear * 1000.0f));
        ImGui::Text("worst limit error    %.2f deg", static_cast<double>(worst.angular * 57.29578f));
        ImGui::Text("ragdoll KE           %.4f J", ke);
        ImGui::Text("joints               %d", stats_.joints);
        ImGui::Text("contact points       %d", stats_.points);
        ImGui::Text("islands (asleep)     %d (%d)", stats_.islands, stats_.sleeping_islands);
        ImGui::Text("woken by kinematic   %d", stats_.kinematic_wakes);
        ImGui::Text("step                 %.3f ms", ms_step_);
        ImGui::Separator();
        ImGui::TextWrapped("[1] trip  [2] hang  [3] pile   [T] trip  [G] get up");
        ImGui::TextWrapped("[M] motion  [Z] hand over at rest  [A] realign");
        ImGui::TextWrapped("[Left]/[Right] iterations  [C] correction  [U] sub-steps");
        ImGui::TextWrapped("[Space] pause  [.] step  [R] reset  [Esc] quit");
        ImGui::End();
    }

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    struct link
    {
        std::uint32_t a = 0;
        std::uint32_t b = 0;
        joint j{};
    };

    static constexpr float k_get_up = 0.6f;

    engine::debug_ui ui_{};
    skeleton sk_ = build_rig();

    body_world world_{};
    std::vector<shape> shapes_{};
    std::vector<link> links_{};
    std::vector<ragdoll> ragdolls_{};
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

    std::vector<transform> local_{};
    std::vector<transform> pose_{};
    std::vector<transform> start_{};
    std::vector<transform> target_{};
    std::vector<transform> drawn_{};
    std::vector<transform> targets_{};
    std::vector<mat4> posed_{};

    solver_stats stats_{};
    double ms_step_ = 0.0;

    scene scene_ = scene::trip;
    phase phase_ = phase::running;
    pose_fn motion_ = jog_pose;
    mat4 place_ = mat4::identity();
    mat4 tripped_at_ = mat4::identity();
    vec3 centre_{0.0f, 0.9f, 0.0f};
    float run_t_ = 0.0f;
    float clip_t_ = 0.0f;
    float phase_t_ = 0.0f;
    float trip_at_ = 1.2f;
    int substeps_ = 1;
    bool at_rest_ = false;
    bool realign_ = true;
    bool running_ = true;
    bool single_step_ = false;
    float t_ = 0.0f;

    const char* shot_path_ = nullptr;
    float shot_at_ = 2.0f;
};

}  // namespace

ENGINE_MAIN(ragdoll_app)
