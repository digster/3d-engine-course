// demos/gimbal/main.cpp — three rings, three knobs, and one of them dying.
//
// Lesson 7.1. Every treatment of gimbal lock shows you the rings. Very few show
// you the NUMBER, and the number is the part you can act on: at pitch = ±90° the
// matrix that turns knob rates into angular velocity loses a dimension, and
// `determinant` of it — a function this engine has had since Lesson 2.6 — tells
// you exactly how close you are. This program puts the picture and the number on
// screen at the same time, so that neither has to be taken on trust.
//
// LESSON 7.2 ADDED THE OTHER HALF, and it is the same rig answering a different
// question. Euler's rotation theorem says the pose the three knobs just built is
// ALSO a single turn about a single line; [A] poses the craft that way instead
// and shows the two agree to six decimals, and the teal line through the
// fuselage is that axis — the one direction the whole rotation leaves alone.
// [B] then runs the blend 7.1 indicted: two ghost craft travel from the same
// start to the same end, one interpolating three angles and one turning steadily
// about that single axis. **Watch what stays still.** The slerp ghost rotates
// rigidly about one fixed teal line for the entire blend; the Euler ghost does
// not, and the difference between the two trails is the 25.24° of detour §9
// measures.
//
//     cmake --build build --target gimbal
//     ./build/demos/gimbal                                play with it
//     ./build/demos/gimbal --pose 40 89 -25               start at a pose
//     ./build/demos/gimbal --shot scratch/l71_shot.ppm    headless, deterministic
//     ./build/demos/gimbal --blend 0.5 --shot out.ppm     the blend, frozen at t
//
// WHAT TO DO WITH IT, in the order that makes the point:
//
//   1. Hold [E] — the JOINT knob, which drives yaw and roll at the same rate.
//      At pitch 0 the craft spins briskly. Press [Up] to climb toward pitch 90
//      and hold [E] again: the rings still turn, the readout still says the
//      knobs are moving, and THE CRAFT STOPS. That is the degree of freedom
//      dying, and it dies gradually rather than at a cliff.
//   2. Watch `|det J|` while you do it. It is 1 when the three knobs are
//      independent and 0 at lock, and it is `cos(pitch)` — which is a fact you
//      can check against the pitch readout beside it.
//   3. Press [K] to snap to pitch 90 and look at the axles. The red axle
//      (pitch) is still its own; the blue axle (roll) has swung round until it
//      lies along the green one (yaw). Two rings, one axis.
//
// THE RULES ARE 5.1'S. Nothing here includes an engine internal, and where the
// public API cannot do something this file does it in the open with a comment
// saying so. There is one such place — §RING below — and it is deliberate.

#include <engine/core/actions.hpp>
#include <engine/ecs/camera.hpp>
#include <engine/ecs/hierarchy.hpp>
#include <engine/ecs/registry.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/debug_draw.hpp>
#include <engine/gfx/debug_lines.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/material.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/renderable.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/axis_angle.hpp>
#include <engine/math/euler.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/rotation.hpp>
#include <engine/math/transform.hpp>
#include <engine/platform/app.hpp>
#include <engine/ui/debug_ui.hpp>

#include <imgui.h>

#include <engine/platform/main.hpp>

#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

namespace {

using engine::ecs::entity;
using engine::euler_angles;
using engine::mat3;
using engine::mat4;
using engine::vec3;

constexpr int k_width = 960;
constexpr int k_height = 540;

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_deg = 180.0f / k_pi;
constexpr float k_rad = k_pi / 180.0f;

/// How fast a held key turns a knob, in degrees per second. Slow enough that you
/// can stop on a chosen pitch, fast enough that a full sweep is not a chore.
constexpr float k_knob_rate = 45.0f;

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_ring_yaw   = engine::k_axis_y_colour;   ///< green: turns about +Y
constexpr Uint32 k_ring_pitch = engine::k_axis_x_colour;   ///< red:   turns about +X
constexpr Uint32 k_ring_roll  = engine::k_axis_z_colour;   ///< blue:  turns about +Z
constexpr Uint32 k_nose       = engine::pack_argb(248, 214, 120);
constexpr Uint32 k_trail      = engine::pack_argb(120, 132, 168);
// DELIBERATELY NOT RED. The first draft drew this arrow in red and it was
// indistinguishable from the pitch ring it crosses — and worse, red MEANS the x
// axis course-wide (Conventions §10), so a red arrow that is not an x axis is a
// convention violation as well as a legibility bug. Magenta is used nowhere else
// in the engine's debug palette.
constexpr Uint32 k_dead       = engine::pack_argb(214, 118, 226);

// LESSON 7.2's THREE. The same constraint applies as to `k_dead` above: red,
// green and blue MEAN x, y and z course-wide (Conventions §10) and the three
// rings already use them, so nothing new may be any of those.
//
// Teal for the rotation axis, which is the one line a rotation leaves alone. It
// is the only cool colour on screen that is not the blue roll ring, and the two
// are never drawn together — [B] hides the rings, and outside a blend the axis
// passes through the fuselage rather than round it.
constexpr Uint32 k_axis         = engine::pack_argb(96, 222, 208);
// The two blend ghosts, deliberately the SAME two colours Lesson 7.1's figure 7
// used for the same two paths: amber is the Euler lerp, pale blue the geodesic.
// A reader who has seen the figure should not have to relearn the code.
constexpr Uint32 k_ghost_euler  = engine::pack_argb(236, 168, 86);
constexpr Uint32 k_ghost_slerp  = engine::pack_argb(126, 188, 248);

// ---------------------------------------------------------------------------
// §RING — the one thing the public API cannot do, done here in the open
// ---------------------------------------------------------------------------
//
// `debug_lines` can draw a line, a ray, a box, a sphere and a wire mesh (5.11).
// It cannot draw a CIRCLE in an arbitrary plane, which is what a gimbal ring is.
// `sphere()` comes close — it is three great circles — but its circles are
// axis-aligned in world space and a gimbal ring is not: the whole point is that
// the ring's plane has been carried by every rotation outside it.
//
// So this file draws its own, out of `line()` calls, and the finding is recorded
// rather than quietly worked around (5.12's rule 2). It is a real gap and a small
// one: an `ellipse(centre, u, v, colour)` taking two in-plane vectors would cover
// rings, orbits, cones and the FOV arcs a camera gizmo wants. Filed for 9.7,
// where the editor's gizmos need exactly this.

/// A circle of `segments` chords, centred at the origin of `frame`, spanned by
/// the two in-plane directions `u` and `v` expressed in `frame`.
///
/// A gimbal ring's plane always CONTAINS its own pivot axis — that is what makes
/// it a ring on bearings rather than a turntable, and it is why the ring visibly
/// tips when its knob turns. So `u` is always the pivot axis and `v` is the next
/// axis down the chain.
void ring(engine::debug_lines& out, const mat3& frame, vec3 u, vec3 v, float radius,
          Uint32 colour, int segments = 64)
{
    vec3 previous = frame * (u * radius);
    for (int i = 1; i <= segments; ++i)
    {
        const float a = 2.0f * k_pi * static_cast<float>(i) / static_cast<float>(segments);
        const vec3 current = frame * ((u * std::cos(a) + v * std::sin(a)) * radius);
        out.line(previous, current, colour);
        previous = current;
    }

    // The axle: the pivot diameter, drawn slightly proud of the ring so that two
    // axles lining up at lock is unmissable. This is the line to watch.
    const vec3 axle = frame * (u * (radius * 1.18f));
    out.line(-axle, axle, colour);
}

// ---------------------------------------------------------------------------
// The measurements, computed from the public API and from nothing else
// ---------------------------------------------------------------------------

/// Everything the readout shows, derived from one pose.
struct conditioning
{
    float det_j = 1.0f;       ///< determinant of the rate Jacobian = -cos(pitch)
    float sigma_min = 1.0f;   ///< weakest gain: body rad/s per unit of knob rad/s
    float cost = 1.0f;        ///< knob rates needed for 1 rad/s in the weak direction
    vec3 weak_knobs{};        ///< the knob combination that does least
    vec3 dead_axis{};         ///< the world axis becoming unreachable, at lock
};

conditioning measure(euler_angles e)
{
    conditioning c;
    const mat3 j = engine::euler_rate_jacobian(e);
    c.det_j = engine::determinant(j);

    // sigma_min = |cos p| / sqrt(1 + |sin p|), and NOT the algebraically equal
    // sqrt(1 - |sin p|). The second subtracts two nearly-equal numbers and hits
    // exactly zero at pitch 89.99°, which is a hundredth of a degree of working
    // range thrown away for nothing — and worse, it throws it away by reporting
    // the value every guard downstream tests for. Lesson 7.1 §6.2 measures both.
    const float sp = std::fabs(std::sin(e.pitch));
    const float cp = std::fabs(std::cos(e.pitch));
    c.sigma_min = cp / std::sqrt(1.0f + sp);
    c.cost = (c.sigma_min > 0.0f) ? 1.0f / c.sigma_min : 0.0f;

    // The weakest knob combination. At pitch > 0 the yaw and roll axles converge,
    // so turning both the same way does least; below zero they diverge and it is
    // turning them opposite ways. Normalised, so the HUD reads as a direction.
    const float same_way = (std::sin(e.pitch) >= 0.0f) ? 1.0f : -1.0f;
    c.weak_knobs = engine::normalised(vec3{1.0f, 0.0f, same_way});

    // Where that combination points once J has acted on it. At lock it is the
    // zero vector — nothing at all — which is exactly the claim; away from lock
    // it is a real axis and the arrow on screen is a live one.
    c.dead_axis = j * c.weak_knobs;
    return c;
}

// ---------------------------------------------------------------------------
// Lesson 7.2 — the single turn, and the two ways to travel it
// ---------------------------------------------------------------------------

/// The two poses the blend runs between, which are **Lesson 7.1's generic pair**,
/// unchanged, so that the numbers on screen are the numbers on the page.
constexpr euler_angles k_blend_from{-70.0f * k_rad, -35.0f * k_rad, 20.0f * k_rad};
constexpr euler_angles k_blend_to{85.0f * k_rad, 55.0f * k_rad, -60.0f * k_rad};

/// Total turning performed along a path, and the minimum it could have been.
///
/// `angle_between_rotations` is the metric (`math/rotation.hpp`), so this is a
/// sum of geodesic steps — the length of the route actually driven. The geodesic
/// between the endpoints is one call to the same function, and the ratio is the
/// detour. Computed ONCE when a blend starts, at a fixed step count, rather than
/// accumulated per frame: a per-frame sum measures the frame rate as much as the
/// path, and two runs of the same demo would print different numbers.
struct blend_cost
{
    float euler_path = 0.0f;
    float slerp_path = 0.0f;
    float geodesic = 0.0f;
};

template <typename Fn>
float path_length(Fn orientation_at, int steps)
{
    float total = 0.0f;
    mat3 previous = orientation_at(0.0f);
    for (int i = 1; i <= steps; ++i)
    {
        const mat3 current = orientation_at(static_cast<float>(i) / static_cast<float>(steps));
        total += engine::angle_between_rotations(previous, current);
        previous = current;
    }
    return total;
}

euler_angles lerp_angles(euler_angles a, euler_angles b, float t)
{
    return {a.yaw + (b.yaw - a.yaw) * t,
            a.pitch + (b.pitch - a.pitch) * t,
            a.roll + (b.roll - a.roll) * t};
}

blend_cost measure_blend()
{
    const mat3 from = engine::rotation_from_euler(k_blend_from);
    const mat3 to = engine::rotation_from_euler(k_blend_to);
    blend_cost c;
    c.euler_path = path_length(
        [&](float t) { return engine::rotation_from_euler(lerp_angles(k_blend_from, k_blend_to, t)); },
        2048);
    c.slerp_path = path_length(
        [&](float t) { return engine::rotation_slerp(from, to, t); }, 2048);
    c.geodesic = engine::angle_between_rotations(from, to);
    return c;
}

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

class gimbal_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char* argv[]) override
    {
        for (int i = 1; i < argc; ++i)
        {
            if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc) { shot_path_ = argv[++i]; }
            else if (SDL_strcmp(argv[i], "--pose") == 0 && i + 3 < argc)
            {
                pose_.yaw = static_cast<float>(SDL_atof(argv[++i])) * k_rad;
                pose_.pitch = static_cast<float>(SDL_atof(argv[++i])) * k_rad;
                pose_.roll = static_cast<float>(SDL_atof(argv[++i])) * k_rad;
            }
            else if (SDL_strcmp(argv[i], "--no-rings") == 0) { rings_ = false; }
            else if (SDL_strcmp(argv[i], "--axis-angle") == 0) { single_turn_ = true; }
            else if (SDL_strcmp(argv[i], "--blend") == 0 && i + 1 < argc)
            {
                // FROZEN at the given t rather than started, so that a `--shot`
                // of a blend is reproducible. A running blend advances in
                // `on_fixed_step`, which a headless run never calls.
                blending_ = true;
                rings_ = false;
                blend_t_ = std::clamp(static_cast<float>(SDL_atof(argv[++i])), 0.0f, 1.0f);
                cost_ = measure_blend();
                // AND THE TRAILS ARE WALKED IN, not left empty. A headless run
                // never calls `on_fixed_step`, so a frozen blend would otherwise
                // draw two aircraft and no history — a picture of a moment rather
                // than of a journey, and the journey is the entire point. Same
                // step count either way, so the still and the live view agree.
                for (int k = 0; k <= 600; ++k)
                {
                    const float t = blend_t_ * static_cast<float>(k) / 600.0f;
                    record_nose(ghost_euler_, euler_at(t));
                    record_nose(ghost_slerp_, slerp_at(t));
                }
            }
        }

        return {.title = "gimbal — watch a degree of freedom die",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        declare_actions();

        mesh_box_ = meshes_.insert(
            engine::with_normals(engine::cube_mesh(), engine::normal_style::flat));

        lights_.key.direction = engine::normalised(vec3{-0.42f, -0.66f, -0.62f});
        lights_.key.colour = {1.0f, 0.97f, 0.92f};
        lights_.key.irradiance = engine::k_reference_irradiance;
        lights_.ambient = {0.13f, 0.14f, 0.19f};

        build_craft();
        build_camera();
        (void)tree_.rebuild_and_resolve(world_);

        (void)ui_.start(window(), renderer());
        return true;
    }

    void on_input() override
    {
        ui_.begin_frame();
        gate_.update(in(), ui_.wants_keyboard(), ui_.wants_mouse());
        actions_.update(gate_);

        if (actions_.pressed(a_quit_))  { request_quit(); }
        if (actions_.pressed(a_reset_)) { pose_ = {}; trail_.clear(); }
        if (actions_.pressed(a_lock_))
        {
            // Snap to the singularity rather than creeping up on it, because the
            // interesting thing about lock is the CONFIGURATION and it is
            // fiddly to hit by hand. Yaw and roll are kept, so the craft's
            // orientation changes as little as the pose allows.
            pose_.pitch = (pose_.pitch >= 0.0f) ? 90.0f * k_rad : -90.0f * k_rad;
            trail_.clear();
        }
        if (actions_.pressed(a_rings_)) { rings_ = !rings_; }
        if (actions_.pressed(a_trail_)) { trail_on_ = !trail_on_; trail_.clear(); }
        if (actions_.pressed(a_single_)) { single_turn_ = !single_turn_; }
        if (actions_.pressed(a_blend_))
        {
            blending_ = !blending_;
            blend_t_ = 0.0f;
            blend_phase_ = 0.0f;
            ghost_euler_.clear();
            ghost_slerp_.clear();
            // The rings belong to the three-knob story and only clutter this one.
            if (blending_) { cost_ = measure_blend(); rings_ = false; }
            if (engine::transform* eye = world_.get<engine::transform>(camera_))
            {
                *eye = camera_placement();
            }
        }
    }

    void on_fixed_step(float h) override
    {
        if (shot_path_ != nullptr) { return; }
        if (blending_) { drive_blend(h); return; }
        drive_knobs(h);
    }

    void on_frame(float alpha) override
    {
        // Not interpolated, for the same reason `collector` gives: the knobs move
        // at 45°/s and the fixed step is 60 Hz, so a frame is at most 0.75° stale.
        (void)alpha;

        fb().clear(k_background);
        depth_.clear();

        // ---- Pose the craft. ONE LINE, and it is the whole lesson ----------
        //
        // `rotation_from_euler` is the entire interface between three numbers a
        // human can type and the matrix the renderer multiplies by. Everything
        // else in this file is a way of looking at what that line just did.
        if (engine::transform* body = world_.get<engine::transform>(craft_))
        {
            // LESSON 7.2's ONE LINE, next to Lesson 7.1's. The two build the same
            // matrix by completely different routes — three elementary turns
            // composed, against one turn about one axis — and the panel prints
            // the angle between them, which is how you know rather than hope.
            // During a blend the craft is the slerp ghost's solid twin.
            body->rotation = blending_ ? slerp_pose()
                           : single_turn_ ? engine::rotation_from_axis_angle(single_turn().value)
                                          : engine::rotation_from_euler(pose_);
        }
        (void)tree_.rebuild_and_resolve(world_);

        const engine::ecs::world_transform* cam_placement =
            world_.get<engine::ecs::world_transform>(camera_);
        const engine::ecs::camera* cam = world_.get<engine::ecs::camera>(camera_);
        if (cam_placement == nullptr || cam == nullptr) { return; }

        const mat4 view = engine::ecs::view_from_camera(*cam_placement);
        const vec3 eye = engine::ecs::eye_of(*cam_placement);
        const engine::projector proj{
            engine::ecs::projection_of(*cam, static_cast<float>(k_width)
                                                 / static_cast<float>(k_height)),
            engine::viewport{0.0f, 0.0f, static_cast<float>(k_width),
                             static_cast<float>(k_height), 0.0f, 1.0f},
            engine::near_mode::clip};

        // NOTHING SOLID DURING A BLEND, and it is not a performance choice. The
        // blend's content is two nose TRAILS a pixel wide, and a solid aircraft
        // sitting between them and the camera hides the part of each trail that
        // passes behind it — including, at t = 1, most of the gap that is the
        // whole measurement. Two wireframes and two trails on an empty ground is
        // also simply the better picture: there is nothing in it that is not the
        // comparison.
        objects_.clear();
        collect_ = blending_ ? engine::renderable_report{}
                             : engine::collect_renderables(world_, meshes_, objects_);

        const engine::render_options opts{.cull = engine::cull_choice::back,
                                          .normals = engine::normal_source::vertex,
                                          .shading = engine::shade_eval::gouraud};
        engine::collect_triangles(triangles_, scratch_, objects_, meshes_, {view, eye}, proj,
                                  lights_, opts);

        const engine::fill_style style{.shade = engine::shading::vertex_colour,
                                       .cull = engine::cull_mode::back,
                                       .eye = eye};
        engine::draw_triangles(fb(), &depth_, triangles_, false, style);

        debug_.clear();
        if (blending_) { queue_blend(); }
        else
        {
            if (rings_) { queue_rings(); }
            queue_nose_and_axis();
            queue_axis(single_turn(), 2.9f);
            if (trail_on_) { queue_trail(); }
        }
        debug_drawn_ = engine::draw_debug_lines(fb(), view, proj, debug_);
        debug_.advance(time().dt());

        if (shot_path_ != nullptr) { write_shot(); }
    }

    void on_overlay() override
    {
        // GUARDED, because a `--shot` run has no window and therefore no ImGui
        // context, and `ImGui::Begin` on no context is a segfault rather than a
        // no-op. `debug_ui`'s own calls are all safe when it never started (5.11
        // made them so on purpose); a panel built by hand between them is not,
        // and the first headless run of this demo found that out.
        if (ui_.running()) { build_panel(); }
        ui_.render();
    }

    void on_event(const SDL_Event& event) override { (void)ui_.handle_event(event); }

    void on_stop() override { ui_.stop(); }

private:
    // ---- Setup -------------------------------------------------------------

    void declare_actions()
    {
        a_yaw_   = actions_.declare("yaw");
        a_pitch_ = actions_.declare("pitch");
        a_roll_  = actions_.declare("roll");
        a_joint_ = actions_.declare("joint");
        a_lock_  = actions_.declare("snap_to_lock");
        a_reset_ = actions_.declare("reset");
        a_rings_ = actions_.declare("rings");
        a_trail_ = actions_.declare("trail");
        a_single_ = actions_.declare("single_turn");
        a_blend_ = actions_.declare("blend");
        a_quit_  = actions_.declare("quit");

        (void)actions_.bind_key(a_yaw_, SDL_SCANCODE_LEFT, +1.0f);
        (void)actions_.bind_key(a_yaw_, SDL_SCANCODE_RIGHT, -1.0f);
        (void)actions_.bind_key(a_pitch_, SDL_SCANCODE_UP, +1.0f);
        (void)actions_.bind_key(a_pitch_, SDL_SCANCODE_DOWN, -1.0f);
        (void)actions_.bind_key(a_roll_, SDL_SCANCODE_Q, +1.0f);
        (void)actions_.bind_key(a_roll_, SDL_SCANCODE_W, -1.0f);
        (void)actions_.bind_key(a_joint_, SDL_SCANCODE_E);
        (void)actions_.bind_key(a_lock_, SDL_SCANCODE_K);
        (void)actions_.bind_key(a_reset_, SDL_SCANCODE_R);
        (void)actions_.bind_key(a_rings_, SDL_SCANCODE_G);
        (void)actions_.bind_key(a_trail_, SDL_SCANCODE_T);
        (void)actions_.bind_key(a_single_, SDL_SCANCODE_A);
        (void)actions_.bind_key(a_blend_, SDL_SCANCODE_B);
        (void)actions_.bind_key(a_quit_, SDL_SCANCODE_ESCAPE);
    }

    /// A craft made of three boxes, parented to one body.
    ///
    /// Three parts rather than one, because a single box has a four-fold symmetry
    /// about each axis and you genuinely cannot see a 90° roll on it. A fuselage,
    /// a wing and a fin break every symmetry there is, and the orientation reads
    /// at a glance — which is the only thing this demo has to get right.
    void build_craft()
    {
        craft_ = world_.create();
        engine::ecs::add_hierarchy_components(world_, craft_, engine::transform{});

        // Sizes are FULL EXTENTS, because `cube_mesh()` spans +/-0.5 and a
        // transform's scale therefore multiplies the whole width. Lesson 5.12
        // got this wrong three times in one file by reaching for the half-extent
        // that everything else in a game has in its hand.
        add_part({0.0f, 0.0f, 0.0f}, {0.46f, 0.46f, 2.30f}, engine::pack_argb(206, 210, 220));
        add_part({0.0f, 0.0f, 0.22f}, {2.50f, 0.14f, 0.62f}, engine::pack_argb(120, 158, 226));
        add_part({0.0f, 0.50f, 0.92f}, {0.11f, 0.82f, 0.50f}, engine::pack_argb(226, 132, 110));
    }

    void add_part(vec3 offset, vec3 size, Uint32 tint)
    {
        const entity e = world_.create();
        engine::ecs::add_hierarchy_components(
            world_, e, engine::transform{.position = offset, .scale = size});
        world_.add<engine::renderable>(
            e, engine::renderable{.mesh = mesh_box_,
                                  .mat = {.tint = tint, .surface = {.roughness = 0.55f}},
                                  .closed = true});
        (void)engine::ecs::set_parent(world_, e, craft_);
        tree_.mark_topology_changed();
    }

    void build_camera()
    {
        camera_ = world_.create();
        engine::ecs::add_hierarchy_components(world_, camera_, camera_placement());
        world_.add<engine::ecs::camera>(camera_, engine::ecs::camera{.fovy = 42.0f * k_rad});
    }

    /// Where the eye sits, which depends on what is being looked at.
    ///
    /// The knob view has to frame the outermost ring at radius 3.05; the blend
    /// view hides the rings and the widest thing in it is a nose trail at 2.05.
    /// Keeping the far distance for both would waste a third of the frame on
    /// background — and the blend's whole content is the GAP between two trails,
    /// which is the first thing a too-small picture loses.
    [[nodiscard]] engine::transform camera_placement() const
    {
        const vec3 eye = blending_ ? vec3{3.55f, 2.45f, 4.70f} : vec3{5.0f, 3.4f, 6.6f};
        return engine::ecs::look_along(eye, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    }

    // ---- Simulation --------------------------------------------------------

    void drive_knobs(float h)
    {
        const float step = k_knob_rate * k_rad * h;

        // THE JOINT KNOB, which is the whole demonstration. It drives yaw and roll
        // at the same rate in the direction the pitch makes weakest — the two
        // axles converge above the horizon and diverge below it, so the sign has
        // to follow the pitch or the effect reverses and looks like a bug.
        if (actions_.held(a_joint_))
        {
            const float same_way = (std::sin(pose_.pitch) >= 0.0f) ? 1.0f : -1.0f;
            pose_.yaw += step;
            pose_.roll += step * same_way;
        }

        pose_.yaw += actions_.value(a_yaw_) * step;
        pose_.roll += actions_.value(a_roll_) * step;

        // Pitch is NOT wrapped and is clamped just past vertical instead, because
        // this demo exists to sit at 90° and look at it. Everything else is
        // wrapped, which is `wrap_angle`'s whole job (§5.1) and is why the yaw
        // readout never grows without bound during a long joint-knob run.
        pose_.pitch = std::clamp(pose_.pitch + actions_.value(a_pitch_) * step,
                                 -95.0f * k_rad, 95.0f * k_rad);
        pose_.yaw = engine::wrap_angle(pose_.yaw);
        pose_.roll = engine::wrap_angle(pose_.roll);

        // The trail records where the NOSE has been, in world space, which turns
        // "the craft stopped responding" from a thing you have to notice into a
        // thing that leaves no mark.
        if (trail_on_)
        {
            const vec3 nose = engine::rotation_from_euler(pose_) * vec3{0.0f, 0.0f, -1.0f};
            if (trail_.empty() || engine::distance(trail_.back(), nose) > 0.01f)
            {
                trail_.push_back(nose);
                if (trail_.size() > 480) { trail_.erase(trail_.begin()); }
            }
        }
    }

    /// Advance the blend, wrap it, and record where each ghost's nose has been.
    ///
    /// The last quarter of the cycle holds at t = 1 rather than snapping straight
    /// back to t = 0, so that the finished pair is on screen long enough to look
    /// at. Both ghosts reach it; only one of them took the short way.
    void drive_blend(float h)
    {
        constexpr float k_cycle = 5.0f;      ///< seconds, including the hold
        constexpr float k_travel = 4.0f;     ///< of which this much is moving

        blend_phase_ += h;
        if (blend_phase_ >= k_cycle)
        {
            blend_phase_ = 0.0f;
            ghost_euler_.clear();
            ghost_slerp_.clear();
        }
        blend_t_ = std::clamp(blend_phase_ / k_travel, 0.0f, 1.0f);

        record_nose(ghost_euler_, euler_pose());
        record_nose(ghost_slerp_, slerp_pose());
    }

    static void record_nose(std::vector<vec3>& trail, const mat3& r)
    {
        const vec3 nose = r * vec3{0.0f, 0.0f, -1.0f};
        if (trail.empty() || engine::distance(trail.back(), nose) > 0.006f)
        {
            trail.push_back(nose);
            if (trail.size() > 900) { trail.erase(trail.begin()); }
        }
    }

    /// The Euler lerp at `t`. Three angles, interpolated independently.
    [[nodiscard]] static mat3 euler_at(float t)
    {
        return engine::rotation_from_euler(lerp_angles(k_blend_from, k_blend_to, t));
    }

    /// The geodesic at `t`. One axis, one angle, scaled.
    [[nodiscard]] static mat3 slerp_at(float t)
    {
        return engine::rotation_slerp(engine::rotation_from_euler(k_blend_from),
                                      engine::rotation_from_euler(k_blend_to), t);
    }

    [[nodiscard]] mat3 euler_pose() const { return euler_at(blend_t_); }
    [[nodiscard]] mat3 slerp_pose() const { return slerp_at(blend_t_); }

    /// The single turn the current pose IS — Euler's rotation theorem, applied.
    [[nodiscard]] engine::axis_angle_extraction single_turn() const
    {
        return engine::axis_angle_from_rotation(engine::rotation_from_euler(pose_));
    }

    /// The single turn that separates the blend's two endpoints.
    ///
    /// **Constant for the whole blend**, which is the entire visual argument of
    /// [B]: the slerp ghost turns about this one line from start to finish, and
    /// the line never moves. Nothing the Euler ghost does can be described that
    /// way at any instant, let alone throughout.
    [[nodiscard]] engine::axis_angle_extraction blend_turn() const
    {
        return engine::axis_angle_from_rotation(
            engine::transpose(engine::rotation_from_euler(k_blend_from))
            * engine::rotation_from_euler(k_blend_to));
    }

    // ---- The debug layer ---------------------------------------------------

    /// The three rings, each in the frame its own knob acts in.
    ///
    /// Read the three lines below and the convention is right there: frame 1 is
    /// the yaw alone, frame 2 is the yaw carrying the pitch, frame 3 is both
    /// carrying the roll. That is what "intrinsic" means, drawn.
    void queue_rings()
    {
        const mat3 f1 = engine::rotation_y(pose_.yaw);
        const mat3 f2 = f1 * engine::rotation_x(pose_.pitch);
        const mat3 f3 = f2 * engine::rotation_z(pose_.roll);

        // Each ring's plane contains its own pivot axis (the first argument) and
        // the next axis down the chain (the second). At pitch 0 the three planes
        // are mutually perpendicular — the picture everybody draws. Turn the
        // pitch to 90 and the blue axle swings onto the green one.
        ring(debug_, f1, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 3.05f, k_ring_yaw);
        ring(debug_, f2, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 2.65f, k_ring_pitch);
        ring(debug_, f3, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, 2.25f, k_ring_roll);
    }

    /// The craft's nose, and the direction that is dying.
    void queue_nose_and_axis()
    {
        const mat3 r = engine::rotation_from_euler(pose_);
        debug_.ray({0.0f, 0.0f, 0.0f}, r * vec3{0.0f, 0.0f, -1.0f} * 2.05f, k_nose);

        // The axis the weakest knob combination produces, drawn at its ACTUAL
        // length rather than normalised. That is the honest picture: away from
        // lock it is a full-length arrow, and as pitch approaches 90° it shrinks
        // to nothing in your hand. A normalised arrow would still point somewhere
        // at the exact moment the claim is that it points nowhere.
        const conditioning c = measure(pose_);
        debug_.ray({0.0f, 0.0f, 0.0f}, c.dead_axis * 2.6f, k_dead);
    }

    /// The line the rotation leaves alone, drawn through the craft.
    ///
    /// Both ways from the origin, because an axis is a LINE and not a direction:
    /// (n, θ) and (−n, −θ) are the same turn, and at θ = 180° the sign is not
    /// determined at all (§8.6). Drawing only the +n half would be asserting a
    /// choice the mathematics does not make.
    ///
    /// It is drawn at a length that scales with the ANGLE, from nothing at the
    /// identity to full length at a half-turn. That is not decoration: near the
    /// identity the axis is genuinely undetermined (the routine says so, with
    /// `axis_route::no_axis`), and a full-length line pointing confidently at the
    /// placeholder +X would be the picture lying. Same discipline as the magenta
    /// arrow above, which is drawn at the true length of the turn it names.
    void queue_axis(const engine::axis_angle_extraction& turn, float base)
    {
        const float reach = base * turn.value.angle / k_pi;
        debug_.line(turn.value.axis * -reach, turn.value.axis * reach, k_axis);
    }

    /// One ghost craft, in wire, at the given orientation.
    ///
    /// Four lines — fuselage, wing, fin — chosen to match the three solid parts
    /// `build_craft` makes, so that the wire ghost and the solid craft read as
    /// the same object. A ghost drawn as a single nose ray would show the
    /// direction and hide the roll, and roll is half of what an Euler lerp gets
    /// wrong.
    /// **Drawn three times, offset**, which is how a line gets a width here.
    ///
    /// `debug_lines` emits one-pixel lines and has no stroke width, deliberately
    /// (5.11). Both craft and both trails are therefore one pixel in the same two
    /// colours, and the first version of this panel was a tangle in which you
    /// could not tell an aircraft from its own history. Dashing the trails was
    /// tried first and did not survive the figure's 3x downsample — the sampler
    /// takes each block's brightest pixel, which fills a two-pixel gap straight
    /// back in. Three copies offset by 25 thousandths along two world axes is
    /// about three pixels at this camera, in every orientation, and downsampling
    /// cannot thin it.
    void queue_ghost(const mat3& r, Uint32 colour)
    {
        const vec3 spread[3] = {{0.0f, 0.0f, 0.0f}, {0.025f, 0.0f, 0.0f}, {0.0f, 0.025f, 0.0f}};
        for (const vec3& d : spread)
        {
            debug_.line(r * vec3{0.0f, 0.0f, 1.15f} + d, r * vec3{0.0f, 0.0f, -1.15f} + d, colour);
            debug_.line(r * vec3{-1.25f, 0.0f, 0.22f} + d, r * vec3{1.25f, 0.0f, 0.22f} + d, colour);
            debug_.line(r * vec3{0.0f, 0.09f, 0.92f} + d, r * vec3{0.0f, 0.91f, 0.92f} + d, colour);
            debug_.line(r * vec3{0.0f, 0.91f, 0.92f} + d, r * vec3{0.0f, 0.09f, 1.15f} + d, colour);
        }
    }

    /// Both blend paths at once, with their trails and the axis one of them uses.
    void queue_blend()
    {
        // The axis is the blend's, not the pose's, and it is expressed in the
        // START frame — which is where the turn happens, since slerp is
        // `A · R(n, tθ)`. Carrying it into world space is one multiply and it is
        // the difference between a line that sits still and one that does not.
        const engine::axis_angle_extraction turn = blend_turn();
        const mat3 from = engine::rotation_from_euler(k_blend_from);
        const vec3 world_axis = from * turn.value.axis;
        debug_.line(world_axis * -3.0f, world_axis * 3.0f, k_axis);

        queue_ghost(euler_pose(), k_ghost_euler);
        queue_ghost(slerp_pose(), k_ghost_slerp);

        queue_path(ghost_euler_, k_ghost_euler);
        queue_path(ghost_slerp_, k_ghost_slerp);
    }

    /// Where a nose has been, at one pixel — thinner than the craft above, on
    /// purpose, so that the two read as "now" and "was".
    void queue_path(const std::vector<vec3>& trail, Uint32 colour)
    {
        for (std::size_t i = 1; i < trail.size(); ++i)
        {
            debug_.line(trail[i - 1] * 2.05f, trail[i] * 2.05f, colour);
        }
    }

    void queue_trail()
    {
        for (std::size_t i = 1; i < trail_.size(); ++i)
        {
            debug_.line(trail_[i - 1] * 2.05f, trail_[i] * 2.05f, k_trail);
        }
    }

    // ---- The readout -------------------------------------------------------

    void build_panel()
    {
        const conditioning c = measure(pose_);
        const engine::euler_extraction back =
            engine::euler_from_rotation(engine::rotation_from_euler(pose_));

        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("gimbal"))
        {
            float yaw_deg = pose_.yaw * k_deg;
            float pitch_deg = pose_.pitch * k_deg;
            float roll_deg = pose_.roll * k_deg;
            if (ImGui::SliderFloat("yaw", &yaw_deg, -180.0f, 180.0f, "%.2f deg"))
            {
                pose_.yaw = yaw_deg * k_rad;
            }
            if (ImGui::SliderFloat("pitch", &pitch_deg, -95.0f, 95.0f, "%.2f deg"))
            {
                pose_.pitch = pitch_deg * k_rad;
            }
            if (ImGui::SliderFloat("roll", &roll_deg, -180.0f, 180.0f, "%.2f deg"))
            {
                pose_.roll = roll_deg * k_rad;
            }

            ImGui::Separator();
            ImGui::Text("|det J|   %.6f      (= |cos pitch| = %.6f)",
                        static_cast<double>(std::fabs(c.det_j)),
                        static_cast<double>(std::fabs(std::cos(pose_.pitch))));
            ImGui::Text("sigma_min %.6f      weakest gain, body rad/s per knob rad/s",
                        static_cast<double>(c.sigma_min));
            if (c.cost > 0.0f)
            {
                ImGui::Text("to turn at 1 rad/s about the weak axis: %.1f rad/s of knob",
                            static_cast<double>(c.cost));
            }
            else
            {
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f),
                                   "LOCKED: no knob rate produces that turn at all");
            }
            ImGui::Text("weak combination: yaw %+.2f, pitch %+.2f, roll %+.2f",
                        static_cast<double>(c.weak_knobs.x),
                        static_cast<double>(c.weak_knobs.y),
                        static_cast<double>(c.weak_knobs.z));
            ImGui::Text("it produces %.4f rad/s of body turn",
                        static_cast<double>(engine::length(c.dead_axis)));

            ImGui::Separator();
            ImGui::Text("extracted: yaw %+.2f  pitch %+.2f  roll %+.2f",
                        static_cast<double>(back.angles.yaw * k_deg),
                        static_cast<double>(back.angles.pitch * k_deg),
                        static_cast<double>(back.angles.roll * k_deg));
            ImGui::Text("cos_pitch %.3e   %s", static_cast<double>(back.cos_pitch),
                        back.degenerate ? "DEGENERATE - roll folded into yaw" : "separable");

            // ---- Lesson 7.2 ------------------------------------------------
            ImGui::Separator();
            const engine::axis_angle_extraction turn = single_turn();
            const char* route = (turn.route == engine::axis_route::no_axis)  ? "no_axis"
                              : (turn.route == engine::axis_route::general)  ? "general"
                                                                            : "reversal";
            ImGui::Text("single turn: %.4f deg about (%+.4f, %+.4f, %+.4f)",
                        static_cast<double>(turn.value.angle * k_deg),
                        static_cast<double>(turn.value.axis.x),
                        static_cast<double>(turn.value.axis.y),
                        static_cast<double>(turn.value.axis.z));
            ImGui::Text("sin(angle) %.3e   route %s", static_cast<double>(turn.sin_angle), route);

            // THE AGREEMENT, PRINTED. Two entirely different constructions of the
            // same orientation — three composed elementary turns, and one
            // Rodrigues turn about a recovered axis — with the metric between
            // them. This is Euler's rotation theorem as a live number rather than
            // a claim, and it is never worse than a few millionths of a degree.
            const float disagreement = engine::angle_between_rotations(
                engine::rotation_from_euler(pose_),
                engine::rotation_from_axis_angle(turn.value));
            ImGui::Text("Euler product vs Rodrigues: %.6f deg apart   %s",
                        static_cast<double>(disagreement * k_deg),
                        single_turn_ ? "[A] posing by AXIS-ANGLE" : "[A] posing by EULER");

            if (blending_)
            {
                ImGui::Separator();
                const float euler_excess =
                    100.0f * (cost_.euler_path / cost_.geodesic - 1.0f);
                const float slerp_excess =
                    100.0f * (cost_.slerp_path / cost_.geodesic - 1.0f);
                ImGui::Text("blend t = %.3f", static_cast<double>(blend_t_));
                ImGui::Text("geodesic         %8.2f deg",
                            static_cast<double>(cost_.geodesic * k_deg));
                ImGui::TextColored(ImVec4(0.93f, 0.66f, 0.34f, 1.0f),
                                   "Euler lerp path  %8.2f deg   %+.2f%%",
                                   static_cast<double>(cost_.euler_path * k_deg),
                                   static_cast<double>(euler_excess));
                ImGui::TextColored(ImVec4(0.49f, 0.74f, 0.97f, 1.0f),
                                   "slerp path       %8.2f deg   %+.2f%%",
                                   static_cast<double>(cost_.slerp_path * k_deg),
                                   static_cast<double>(slerp_excess));
                ImGui::TextUnformatted("the teal line is the slerp's axis, and it never moves");
            }

            ImGui::Separator();
            ImGui::TextUnformatted("arrows yaw/pitch  Q/W roll  [E] joint knob");
            ImGui::TextUnformatted("[K] snap to lock  [T] nose trail  [G] rings  [R] reset");
            ImGui::TextUnformatted("[A] pose by axis-angle  [B] blend: Euler lerp vs slerp");
        }
        ImGui::End();
    }

    void write_shot()
    {
        const conditioning c = measure(pose_);
        // The shot's receipt. Two runs with the same `--pose` print the same
        // numbers or the picture is not reproducible, whatever it looks like.
        const engine::axis_angle_extraction turn = single_turn();
        std::printf("gimbal: pose %.2f %.2f %.2f deg, |det J| %.6f, sigma_min %.6f, "
                    "%zu objects, %zu triangles, %d debug lines\n",
                    static_cast<double>(pose_.yaw * k_deg),
                    static_cast<double>(pose_.pitch * k_deg),
                    static_cast<double>(pose_.roll * k_deg),
                    static_cast<double>(std::fabs(c.det_j)), static_cast<double>(c.sigma_min),
                    collect_.drawn, triangles_.size(), debug_drawn_);
        // The 7.2 half of the receipt. The last field is the one that matters:
        // two runs agreeing on a picture prove nothing if the picture was built
        // from a rotation that disagrees with itself.
        std::printf("gimbal: single turn %.4f deg about (%+.5f, %+.5f, %+.5f), route %d, "
                    "blend t %.3f, Euler-vs-Rodrigues %.6f deg\n",
                    static_cast<double>(turn.value.angle * k_deg),
                    static_cast<double>(turn.value.axis.x),
                    static_cast<double>(turn.value.axis.y),
                    static_cast<double>(turn.value.axis.z),
                    static_cast<int>(turn.route), static_cast<double>(blend_t_),
                    static_cast<double>(k_deg * engine::angle_between_rotations(
                        engine::rotation_from_euler(pose_),
                        engine::rotation_from_axis_angle(turn.value))));
        request_quit(engine::save_ppm(fb(), shot_path_));
    }

    // ---- State -------------------------------------------------------------

    engine::ecs::registry world_;
    engine::ecs::hierarchy tree_;
    engine::mesh_pool meshes_;
    engine::mesh_handle mesh_box_{};
    entity craft_{};
    entity camera_{};

    euler_angles pose_{};
    std::vector<vec3> trail_;
    bool trail_on_ = false;
    bool rings_ = true;
    const char* shot_path_ = nullptr;

    // Lesson 7.2.
    bool single_turn_ = false;        ///< [A]: pose by Rodrigues instead of Euler
    bool blending_ = false;           ///< [B]: the two-path comparison is running
    float blend_t_ = 0.0f;            ///< where along it, in [0, 1]
    float blend_phase_ = 0.0f;        ///< seconds into the cycle, including the hold
    blend_cost cost_{};               ///< measured once per blend, never per frame
    std::vector<vec3> ghost_euler_;
    std::vector<vec3> ghost_slerp_;

    engine::action_map actions_;
    engine::masked_input<engine::input> gate_;
    engine::action_id a_yaw_{}, a_pitch_{}, a_roll_{}, a_joint_{};
    engine::action_id a_lock_{}, a_reset_{}, a_rings_{}, a_trail_{}, a_quit_{};
    engine::action_id a_single_{}, a_blend_{};

    engine::depth_buffer depth_{k_width, k_height};
    engine::lighting lights_;
    std::vector<engine::scene_object> objects_;
    std::vector<engine::raster_triangle> triangles_;
    engine::projection_scratch scratch_;
    engine::renderable_report collect_{};
    engine::debug_lines debug_{8192};
    int debug_drawn_ = 0;
    engine::debug_ui ui_;
};

}  // namespace

ENGINE_MAIN(gimbal_app)
