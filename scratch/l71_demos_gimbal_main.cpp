// demos/gimbal/main.cpp — three rings, three knobs, and one of them dying.
//
// Lesson 7.1. Every treatment of gimbal lock shows you the rings. Very few show
// you the NUMBER, and the number is the part you can act on: at pitch = ±90° the
// matrix that turns knob rates into angular velocity loses a dimension, and
// `determinant` of it — a function this engine has had since Lesson 2.6 — tells
// you exactly how close you are. This program puts the picture and the number on
// screen at the same time, so that neither has to be taken on trust.
//
//     cmake --build build --target gimbal
//     ./build/demos/gimbal                                play with it
//     ./build/demos/gimbal --pose 40 89 -25               start at a pose
//     ./build/demos/gimbal --shot scratch/l71_shot.ppm    headless, deterministic
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
#include <engine/math/euler.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
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
    }

    void on_fixed_step(float h) override
    {
        if (shot_path_ != nullptr) { return; }
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
            body->rotation = engine::rotation_from_euler(pose_);
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

        collect_ = engine::collect_renderables(world_, meshes_, objects_);

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
        if (rings_) { queue_rings(); }
        queue_nose_and_axis();
        if (trail_on_) { queue_trail(); }
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
        engine::ecs::add_hierarchy_components(
            world_, camera_,
            engine::ecs::look_along({5.0f, 3.4f, 6.6f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}));
        world_.add<engine::ecs::camera>(camera_, engine::ecs::camera{.fovy = 42.0f * k_rad});
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

            ImGui::Separator();
            ImGui::TextUnformatted("arrows yaw/pitch  Q/W roll  [E] joint knob");
            ImGui::TextUnformatted("[K] snap to lock  [T] nose trail  [G] rings  [R] reset");
        }
        ImGui::End();
    }

    void write_shot()
    {
        const conditioning c = measure(pose_);
        // The shot's receipt. Two runs with the same `--pose` print the same
        // numbers or the picture is not reproducible, whatever it looks like.
        std::printf("gimbal: pose %.2f %.2f %.2f deg, |det J| %.6f, sigma_min %.6f, "
                    "%zu objects, %zu triangles, %d debug lines\n",
                    static_cast<double>(pose_.yaw * k_deg),
                    static_cast<double>(pose_.pitch * k_deg),
                    static_cast<double>(pose_.roll * k_deg),
                    static_cast<double>(std::fabs(c.det_j)), static_cast<double>(c.sigma_min),
                    collect_.drawn, triangles_.size(), debug_drawn_);
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

    engine::action_map actions_;
    engine::masked_input<engine::input> gate_;
    engine::action_id a_yaw_{}, a_pitch_{}, a_roll_{}, a_joint_{};
    engine::action_id a_lock_{}, a_reset_{}, a_rings_{}, a_trail_{}, a_quit_{};

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
