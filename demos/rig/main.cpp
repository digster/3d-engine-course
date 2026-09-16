// demos/rig/main.cpp — one surface, six joints, and the pinch nobody warns you about.
//
// Lesson 7.6. Every other demo in this repository draws RIGID things: a cube, an
// aircraft, a rover, a swarm of drones. Each of them is a `transform` and a mesh,
// and Lesson 5.9's hierarchy is enough to make a turret follow a hull. This one
// draws the case that breaks — a single continuous tube whose vertices are
// shared between joints, so that no assignment of whole objects to whole matrices
// can bend it.
//
// WHAT TO DO WITH IT, in the order that makes the point:
//
//   1. Hold [Left] / [Right] and watch the tube CURL. The skeleton drawn through
//      it is the same six joints the whole time; what changed is that each vertex
//      is being moved by two of them at once, blended by weight. Every vertex on
//      screen went through four multiply-adds and nothing else.
//   2. Press [N]. That is the whole of skinning done WITHOUT the inverse bind
//      matrices — the single most common first-attempt bug — and the tube leaves
//      the frame. It is not subtly wrong: the mesh's coordinates were never in
//      any joint's space, so multiplying them by a joint's matrix asks a question
//      with no meaning. Press it again to come back.
//   3. Press [T] for the twist, and turn it up to 180 degrees with [Left] /
//      [Right]. The tube pinches to a POINT, and the readout says the radius is
//      zero rather than small. That is the candy wrapper, and it is exactly
//      `cos(theta/2)`: the average of two points a half-turn apart on a circle is
//      the centre of the circle.
//   4. Still in [T], press [2], [3], [4]. The same 180 degrees spread over more
//      joints, and the pinch opens up to cos(90/n) — 0.707, 0.866, 0.924. That
//      is what a forearm twist bone IS, and it is why rigs carry more joints than
//      skeletons do.
//   5. Press [W] to see the weighting itself: each ring drawn in the blend of its
//      two joints' colours. The pinch always lands where the two are equal.
//
//     cmake --build build --target rig
//     ./build/demos/rig                                      play with it
//     ./build/demos/rig --bend 70                            start curled
//     ./build/demos/rig --twist 180 --segments 1             the worst case
//     ./build/demos/rig --twist 180 --segments 4             …and the fix
//     ./build/demos/rig --no-bind --shot scratch/x.ppm       the classic bug
//     ./build/demos/rig --weights --shot scratch/x.ppm       headless, deterministic
//
// THE RULES ARE 5.1'S. Nothing here includes an engine internal. Note in
// particular that the deformation is `engine::anim::skin_into` writing into a
// `mesh_data` that the engine's own `mesh_pool` owns — a demo may reach a mesh's
// arrays through `pool::get`, which is exactly the "resolve late, use
// immediately" contract `core/pool.hpp` documents, and nothing here needed a new
// public function to do it.

#include <engine/anim/skeleton.hpp>
#include <engine/anim/skin.hpp>
#include <engine/core/actions.hpp>
#include <engine/ecs/camera.hpp>
#include <engine/ecs/hierarchy.hpp>
#include <engine/ecs/registry.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/debug_draw.hpp>
#include <engine/gfx/debug_lines.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/image.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/material.hpp>
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/renderable.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/transform.hpp>
#include <engine/math/vec3.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>
#include <engine/ui/debug_ui.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

using engine::mat4;
using engine::quat;
using engine::transform;
using engine::vec3;
using engine::anim::joint_index;
using engine::anim::k_max_influences;
using engine::anim::k_no_parent;
using engine::ecs::entity;

namespace {

constexpr int k_width = 960;
constexpr int k_height = 540;

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_rad = k_pi / 180.0f;
constexpr float k_deg = 180.0f / k_pi;

/// Six joints, one unit apart, so the tube is five units long and the numbers on
/// the panel are in units the reader can count off the picture.
constexpr int k_joints = 6;

/// The tube. Enough rings that the pinch is a smooth waist rather than a crease,
/// and enough segments around that a circle looks like one at this radius.
constexpr int k_rings = 21;
constexpr int k_around = 24;
constexpr float k_radius = 0.38f;

/// The first joint the twist is applied to. Chosen so the pinch lands in the
/// MIDDLE of the tube rather than at its root, where the camera would have to
/// look past the skeleton's origin marker to see it.
constexpr int k_twist_first = 2;

/// **Darker than gimbal's and plane's (16, 18, 24), and EVERY channel matters.**
/// The lesson's figures turn a render into inline SVG by run-length encoding it
/// against a fixed palette, and that encoder treats a pixel as BACKGROUND — and
/// emits nothing at all for it — only when red, green AND blue are each below 14.
/// The obvious first attempt here was (12, 13, 17), which is darker than the
/// others on two channels and 17 on the third, and 17 is not below 14: the
/// background went through the quantiser like any other colour and came back at
/// (79, 84, 98), a mid-grey slab with the subject barely visible on it.
///
/// A demo choosing its clear colour to suit a figure generator is a tail wagging
/// a dog, and it is still the right call here: nothing a reader can see on screen
/// changes, and the alternative is editing `figs_45.py`'s threshold, which every
/// render figure since Lesson 4.5 depends on.
const Uint32 k_background = engine::pack_argb(11, 12, 13);

/// One colour per joint, for the weight view and for the skeleton.
///
/// **Deliberately none of red, green or blue at full strength.** The Conventions
/// page fixes those for the x/y/z axes and this demo draws axis triads on the
/// joints; a joint tinted pure green beside a green y axis is a picture that
/// lies about which thing is which. These are six well-separated hues taken from
/// the warm/cool sides of the wheel instead.
const Uint32 k_joint_colour[k_joints] = {
    engine::pack_argb(242, 186,  92),   // amber
    engine::pack_argb(236, 122, 140),   // rose
    engine::pack_argb(158, 132, 232),   // violet
    engine::pack_argb( 96, 196, 206),   // teal
    engine::pack_argb(214, 214,  96),   // citron
    engine::pack_argb(146, 200, 148),   // sage
};

/// Blend two packed colours. The weight view needs it and nothing in `colour.hpp`
/// blends packed pixels — that file works in linear `vec3`, which is right for
/// shading and heavy-handed for a debug line.
[[nodiscard]] Uint32 mix_argb(Uint32 a, Uint32 b, float t)
{
    const float u = 1.0f - t;
    auto chan = [&](int shift) {
        const float ca = static_cast<float>((a >> shift) & 0xFFu);
        const float cb = static_cast<float>((b >> shift) & 0xFFu);
        return static_cast<Uint32>(std::clamp(ca * u + cb * t, 0.0f, 255.0f));
    };
    return 0xFF000000u | (chan(16) << 16) | (chan(8) << 8) | chan(0);
}

// ---- The rig ---------------------------------------------------------------

/// A straight chain along +y, one unit per link.
///
/// **The bind pose is deliberately straight and un-rotated**, which is not how a
/// real character is authored — an arm is modelled bent, a leg is modelled in an
/// A-pose — but it makes every number on the panel readable off the picture. The
/// harness (`scratch/verify_76.cpp`) uses a chain with awkward rotations and a
/// non-uniform scale in it precisely because this one is too tidy to catch a
/// transpose written backwards.
[[nodiscard]] engine::anim::skeleton build_skeleton()
{
    engine::anim::skeleton sk;
    sk.joints.resize(k_joints);
    for (int j = 0; j < k_joints; ++j)
    {
        engine::anim::joint& jt = sk.joints[static_cast<std::size_t>(j)];
        jt.parent = (j == 0) ? k_no_parent : static_cast<joint_index>(j - 1);
        jt.name = "joint" + std::to_string(j);
        jt.local_bind.position = (j == 0) ? vec3{0.0f, 0.0f, 0.0f} : vec3{0.0f, 1.0f, 0.0f};
    }
    engine::anim::bake_inverse_binds(sk);
    return sk;
}

/// A tube around the chain, weighted to the two joints it lies between.
///
/// THE WEIGHTING IS THE SIMPLEST ONE THAT WORKS and it is worth saying what it
/// is not: real content is weighted by a rigger, or by an automatic bind that
/// solves a heat equation over the surface, and either produces falloffs that
/// are smooth in two dimensions rather than linear in one. This is linear in the
/// distance between two joints — which is exactly the case the candy-wrapper
/// derivation assumes, so the picture and the formula are talking about the same
/// thing.
[[nodiscard]] engine::anim::skinned_mesh build_tube()
{
    engine::anim::skinned_mesh m;
    const float span = static_cast<float>(k_joints - 1);

    for (int r = 0; r < k_rings; ++r)
    {
        const float t = static_cast<float>(r) / static_cast<float>(k_rings - 1);
        const float y = t * span;

        for (int a = 0; a < k_around; ++a)
        {
            const float phi = 2.0f * k_pi * static_cast<float>(a) / static_cast<float>(k_around);
            const float cx = std::cos(phi);
            const float cz = std::sin(phi);
            m.bind.vertices.push_back({k_radius * cx, y, k_radius * cz});
            m.bind.normals.push_back({cx, 0.0f, cz});

            const int lower = std::min(static_cast<int>(y), k_joints - 1);
            const int upper = std::min(lower + 1, k_joints - 1);
            const float frac = y - static_cast<float>(lower);

            engine::anim::skin_influence inf;
            inf.joints[0] = static_cast<joint_index>(lower);
            inf.joints[1] = static_cast<joint_index>(upper);
            inf.weights[0] = 1.0f - frac;
            inf.weights[1] = frac;
            inf.weights[2] = 0.0f;
            inf.weights[3] = 0.0f;
            m.influences.push_back(inf);
        }
    }

    // Two triangles per quad, wound so the outside faces out — which back-face
    // culling then relies on, and which is the one thing in this generator that
    // is easy to get backwards and invisible until the tube renders inside-out.
    for (int r = 0; r + 1 < k_rings; ++r)
    {
        for (int a = 0; a < k_around; ++a)
        {
            const auto i0 = static_cast<std::uint16_t>(r * k_around + a);
            const auto i1 = static_cast<std::uint16_t>(r * k_around + (a + 1) % k_around);
            const auto i2 = static_cast<std::uint16_t>(i0 + k_around);
            const auto i3 = static_cast<std::uint16_t>(i1 + k_around);
            m.bind.indices.insert(m.bind.indices.end(), {i0, i2, i1, i1, i2, i3});
        }
    }
    return m;
}

/// What a ring of the deformed tube looks like: its centre, and the SMALLEST
/// distance of its vertices from that centre.
///
/// The readout that makes the candy wrapper a NUMBER rather than a shape, and it
/// took two wrong instruments to arrive at. **Both earlier versions measured the
/// radius PERPENDICULAR TO THE CHAIN**, which sounds obviously right — a ring on
/// a bent arm is not perpendicular to anything else, so measure against the bone
/// and the foreshortening cancels. It does not work, twice over:
///
///   1. The tip ring is weighted entirely to the last joint, so there is no
///      "segment" through it and the axis fell back to world +y. On a limb bent
///      90 degrees that ring is edge-on to +y and the perpendicular radius read
///      **0.241 instead of 0.380** — a pinch reported at the one place on the
///      tube where there are not two joints to pinch between.
///   2. Clamping the axis to the last real segment fixed that and left a
///      smaller version of the same error: the tip ring is rigidly rotated by
///      one more joint angle than the segment below it, so its plane is TILTED
///      against the measuring axis, and at a 120-degree bend it read 0.364.
///
/// A rigid rotation does not change the distance between two points, so a
/// distance **from the ring's own centroid** is invariant under every rotation
/// the skeleton can apply and drops only when the blend actually pulls the
/// vertices in. It needs no axis, which is what makes it right: it cannot be
/// fooled by the orientation of anything.
///
/// **BOTH WRONG VERSIONS REPORTED A SMALLER NUMBER**, which is the direction
/// this measurement is hunting in — so both of them looked like a discovery.
struct ring_measure
{
    float radius = 0.0f;
    vec3 centre{};
};

[[nodiscard]] ring_measure measure_ring(const std::vector<vec3>& skinned, int ring)
{
    ring_measure out;
    const int base = ring * k_around;
    for (int a = 0; a < k_around; ++a)
    {
        out.centre = out.centre + skinned[static_cast<std::size_t>(base + a)];
    }
    out.centre = out.centre / static_cast<float>(k_around);

    // THE MINIMUM RATHER THAN THE MEAN, and the difference is a fact about the
    // two artifacts rather than a choice of statistic. A TWIST collapses the
    // whole ring uniformly, so every vertex lands at `r cos(theta/2)` and the
    // two agree. A BEND rotates about an axis ACROSS the tube, so the vertices
    // that lie along the bend axis do not move at all and the ring becomes an
    // ELLIPSE with semi-axes `r cos(theta/2)` and `r`. Its mean radius is then
    // some average of the two — which is a number about the shape of an ellipse,
    // and its minor axis is the number about skinning.
    out.radius = engine::length(skinned[static_cast<std::size_t>(base)] - out.centre);
    for (int a = 1; a < k_around; ++a)
    {
        out.radius = std::min(out.radius,
                              engine::length(skinned[static_cast<std::size_t>(base + a)]
                                             - out.centre));
    }
    return out;
}

}   // namespace

// ---- The program -----------------------------------------------------------

class rig_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char* argv[]) override
    {
        for (int i = 1; i < argc; ++i)
        {
            // ONE NAMED LOCAL PER ARGUMENT. `SDL_clamp` is a macro that expands
            // its argument three times, which cost Lesson 7.3's demo an
            // afternoon of a headless run opening a window; `std::clamp` is a
            // function, and writing the local first survives either.
            if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc) { shot_path_ = argv[++i]; }
            else if (SDL_strcmp(argv[i], "--bend") == 0 && i + 1 < argc)
            {
                const double given = SDL_atof(argv[++i]);
                bend_ = std::clamp(static_cast<float>(given), -120.0f, 120.0f) * k_rad;
            }
            else if (SDL_strcmp(argv[i], "--twist") == 0 && i + 1 < argc)
            {
                const double given = SDL_atof(argv[++i]);
                twisting_ = true;
                twist_ = std::clamp(static_cast<float>(given), 0.0f, 180.0f) * k_rad;
            }
            else if (SDL_strcmp(argv[i], "--segments") == 0 && i + 1 < argc)
            {
                const int given = SDL_atoi(argv[++i]);
                segments_ = std::clamp(given, 1, k_joints - k_twist_first);
            }
            else if (SDL_strcmp(argv[i], "--no-bind") == 0) { use_inverse_binds_ = false; }
            else if (SDL_strcmp(argv[i], "--weights") == 0) { weight_view_ = true; }
            else if (SDL_strcmp(argv[i], "--ghost") == 0) { ghost_ = true; }
        }

        return {.title = "rig — one surface, six joints",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        declare_actions();

        skeleton_ = build_skeleton();
        tube_ = build_tube();
        report_ = engine::anim::validate(skeleton_);
        skin_report_ = engine::anim::validate(tube_, skeleton_.size());

        // The deformed copy lives in the engine's own mesh pool, and it starts
        // as the bind mesh so that the very first frame — before any pose has
        // been composed — draws the character as modelled rather than as
        // nothing. `skin_into` overwrites the positions and normals from here on
        // and never touches the indices again.
        skin_handle_ = meshes_.insert(tube_.bind);

        lights_.key.direction = engine::normalised(vec3{-0.45f, -0.62f, -0.65f});
        lights_.key.colour = {1.0f, 0.97f, 0.92f};
        lights_.key.irradiance = engine::k_reference_irradiance;
        lights_.ambient = {0.14f, 0.15f, 0.20f};

        limb_ = world_.create();
        engine::ecs::add_hierarchy_components(world_, limb_, engine::transform{});
        world_.add<engine::renderable>(
            limb_, engine::renderable{.mesh = skin_handle_,
                                      .mat = {.tint = engine::pack_argb(198, 202, 212),
                                              .surface = {.roughness = 0.62f}},
                                      .closed = true});

        camera_ = world_.create();
        engine::ecs::add_hierarchy_components(world_, camera_, camera_placement());
        world_.add<engine::ecs::camera>(camera_, engine::ecs::camera{.fovy = 40.0f * k_rad});

        (void)tree_.rebuild_and_resolve(world_);
        (void)ui_.start(window(), renderer());
        return true;
    }

    void on_input() override
    {
        ui_.begin_frame();
        gate_.update(in(), ui_.wants_keyboard(), ui_.wants_mouse());
        actions_.update(gate_);

        if (actions_.pressed(a_quit_)) { request_quit(); }
        if (actions_.pressed(a_reset_)) { bend_ = 0.0f; twist_ = 0.0f; }
        if (actions_.pressed(a_twist_)) { twisting_ = !twisting_; }
        if (actions_.pressed(a_bind_))  { use_inverse_binds_ = !use_inverse_binds_; }
        if (actions_.pressed(a_weights_)) { weight_view_ = !weight_view_; }
        if (actions_.pressed(a_ghost_)) { ghost_ = !ghost_; }
        for (int n = 0; n < 4; ++n)
        {
            if (actions_.pressed(a_seg_[n])) { segments_ = std::min(n + 1, k_joints - k_twist_first); }
        }
    }

    void on_fixed_step(float h) override
    {
        // A headless run never gets here, which is why every mode is reachable
        // from a flag as well as from a key. Same rule `gimbal` follows, and for
        // the same reason: a `--shot` has to be able to name any picture the
        // program can draw.
        if (shot_path_ != nullptr) { return; }

        const float rate = 45.0f * k_rad * h;
        const float dir = actions_.value(a_angle_);
        if (twisting_) { twist_ = std::clamp(twist_ + rate * dir, 0.0f, k_pi); }
        else { bend_ = std::clamp(bend_ + rate * dir, -120.0f * k_rad, 120.0f * k_rad); }
    }

    void on_frame(float alpha) override
    {
        (void)alpha;   // the pose moves at 45 deg/s; a frame is at most 0.75 deg stale

        fb().clear(k_background);
        depth_.clear();

        // ---- The three lines that are the lesson --------------------------
        //
        // Build a pose, compose it into joint matrices, turn those into skinning
        // matrices, and deform. Everything else in this file is a way of looking
        // at what these four calls just did.
        build_pose();
        engine::anim::skinning_palette(skeleton_, pose_, posed_, palette_);

        // [N] — the whole of the classic bug, expressed as one substitution. The
        // posed joint matrices ARE `model_from_joint`; using them as if they were
        // skinning matrices asks the mesh's model-space coordinates to pretend
        // they were in joint space all along.
        const std::vector<mat4>& use = use_inverse_binds_ ? palette_ : posed_;

        if (engine::mesh_data* dst = meshes_.get(skin_handle_))
        {
            engine::anim::skin_into(tube_, use, *dst);
            deformed_ = dst->vertices;           // kept for the measurements below
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

        objects_.clear();
        // NOTHING SOLID IN THE WEIGHT VIEW. Its content is the ring outlines, and
        // a solid tube in front of them hides exactly the half of each ring that
        // shows how the two colours meet.
        collect_ = weight_view_ ? engine::renderable_report{}
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
        queue_skeleton();
        if (weight_view_) { queue_weight_rings(); }
        if (ghost_) { queue_bind_ghost(); }
        measure();
        debug_drawn_ = engine::draw_debug_lines(fb(), view, proj, debug_);
        debug_.advance(time().dt());

        if (shot_path_ != nullptr) { write_shot(); }
    }

    void on_overlay() override
    {
        // GUARDED: a `--shot` run has no window and therefore no ImGui context,
        // and `ImGui::Begin` on no context is a segfault rather than a no-op.
        if (ui_.running()) { build_panel(); }
        ui_.render();
    }

    void on_event(const SDL_Event& event) override { (void)ui_.handle_event(event); }

    void on_stop() override { ui_.stop(); }

private:
    // ---- Setup -------------------------------------------------------------

    void declare_actions()
    {
        a_quit_ = actions_.declare("quit");
        a_reset_ = actions_.declare("reset");
        a_angle_ = actions_.declare("angle");
        a_twist_ = actions_.declare("twist");
        a_bind_ = actions_.declare("bind");
        a_weights_ = actions_.declare("weights");
        a_ghost_ = actions_.declare("ghost");
        for (int n = 0; n < 4; ++n)
        {
            a_seg_[n] = actions_.declare(k_seg_names[n]);
            (void)actions_.bind_key(a_seg_[n], k_seg_keys[n]);
        }

        // Both directions on ONE action with opposite scales, which is what
        // `action_map` is for: `value(a_angle_)` is then -1, 0 or +1 and the
        // simulation multiplies it by a rate. Two separate actions would work and
        // would put the subtraction in the caller, where Lesson 5.10 §3 spends a
        // page explaining why it does not belong.
        (void)actions_.bind_key(a_angle_, SDL_SCANCODE_RIGHT, +1.0f);
        (void)actions_.bind_key(a_angle_, SDL_SCANCODE_LEFT, -1.0f);
        (void)actions_.bind_key(a_quit_, SDL_SCANCODE_ESCAPE);
        (void)actions_.bind_key(a_reset_, SDL_SCANCODE_R);
        (void)actions_.bind_key(a_twist_, SDL_SCANCODE_T);
        (void)actions_.bind_key(a_bind_, SDL_SCANCODE_N);
        (void)actions_.bind_key(a_weights_, SDL_SCANCODE_W);
        (void)actions_.bind_key(a_ghost_, SDL_SCANCODE_B);
    }

    [[nodiscard]] engine::transform camera_placement() const
    {
        // Aimed at the middle of the tube rather than at the origin: the chain
        // spans y = 0 to 5 and a camera on the origin spends the top half of the
        // frame on the subject and the bottom half on nothing.
        return engine::ecs::look_along({6.2f, 3.4f, 6.6f}, {0.0f, 2.3f, 0.0f},
                                       {0.0f, 1.0f, 0.0f});
    }

    // ---- The pose ----------------------------------------------------------

    /// Turn the two sliders into a local transform per joint.
    ///
    /// **This is the only place in the program that knows what "a pose" means**,
    /// and it is deliberately tiny: Lesson 7.7 replaces it with a clip sampler
    /// and nothing else in this file changes, because everything downstream takes
    /// `std::span<const transform>` and does not care where the transforms came
    /// from.
    void build_pose()
    {
        engine::anim::rest_pose(skeleton_, pose_);

        if (twisting_)
        {
            // The same total twist, divided among `segments_` joints. Each pair
            // of adjacent joints therefore differs by `twist_ / segments_`, and
            // that difference — not the total — is what sets the pinch.
            const float per = twist_ / static_cast<float>(segments_);
            for (int n = 0; n < segments_; ++n)
            {
                const std::size_t j = static_cast<std::size_t>(k_twist_first + n);
                if (j < pose_.size())
                {
                    pose_[j].rotation = engine::quat_from_axis_angle({0.0f, 1.0f, 0.0f}, per);
                }
            }
            return;
        }

        // A curl: every joint above the root turns by the same amount about +z,
        // so the accumulated bend at the tip is five times the per-joint angle.
        const float per = bend_ / static_cast<float>(k_joints - 1);
        for (std::size_t j = 1; j < pose_.size(); ++j)
        {
            pose_[j].rotation = engine::quat_from_axis_angle({0.0f, 0.0f, 1.0f}, per);
        }
    }

    // ---- What the picture is claiming --------------------------------------

    /// The narrowest ring, and what the derivation says it should be.
    void measure()
    {
        narrowest_ = k_radius;
        narrowest_ring_ = 0;
        if (deformed_.size() != tube_.bind.vertices.size()) { return; }

        for (int r = 0; r < k_rings; ++r)
        {
            const ring_measure m = measure_ring(deformed_, r);
            if (m.radius < narrowest_)
            {
                narrowest_ = m.radius;
                narrowest_ring_ = r;
            }
        }
    }

    /// `r cos(delta / 2)` — the radius the derivation predicts, where `delta` is
    /// the angle between the two joints a ring is weighted BETWEEN.
    ///
    /// **The same formula governs both modes, which is not obvious and is the
    /// best evidence the derivation is about the blend rather than about
    /// twisting.** A twist collapses the whole ring to `r cos(delta/2)`; a bend
    /// flattens it into an ellipse whose MINOR axis is `r cos(delta/2)` and whose
    /// major axis is untouched, because the vertices lying along the bend's own
    /// rotation axis do not move. Measuring the smallest radius therefore reads
    /// the same law in both — 0.37532 against 0.375321 at a 90-degree bend, and
    /// 0.26870 against 0.268702 at a 180-degree twist over two segments.
    ///
    /// What differs is only how the total angle is divided. A bend spreads itself
    /// over every joint above the root; a twist over `segments_` of them.
    [[nodiscard]] float predicted_radius() const
    {
        const float delta = twisting_ ? twist_ / static_cast<float>(segments_)
                                      : bend_ / static_cast<float>(k_joints - 1);
        return k_radius * std::cos(delta * 0.5f);
    }

    // ---- Drawing -----------------------------------------------------------

    /// The skeleton itself: a bone line per joint, and a triad at each.
    void queue_skeleton()
    {
        for (int j = 0; j < k_joints; ++j)
        {
            const vec3 here = engine::translation_of(posed_[static_cast<std::size_t>(j)]);
            const Uint32 tint = k_joint_colour[j];

            if (j > 0)
            {
                const vec3 up = engine::translation_of(posed_[static_cast<std::size_t>(j - 1)]);
                debug_.line(up, here, tint);
            }
            // Short triads, because a full-length one at every joint turns the
            // picture into a thicket. Long enough to read the twist off, which
            // is the whole reason they are here in [T].
            debug_.axes(posed_[static_cast<std::size_t>(j)], 0.30f);
        }
    }

    /// Each ring of the tube, drawn in the blend of its two joints' colours.
    ///
    /// This IS the weighting, drawn: a ring at `w = 0.5` between amber and rose
    /// is the colour halfway between them, and that is also exactly the ring
    /// that pinches furthest. Seeing the two facts in one picture is the point.
    void queue_weight_rings()
    {
        if (deformed_.size() != tube_.bind.vertices.size()) { return; }

        for (int r = 0; r < k_rings; ++r)
        {
            const engine::anim::skin_influence& inf =
                tube_.influences[static_cast<std::size_t>(r * k_around)];
            const Uint32 tint = mix_argb(k_joint_colour[inf.joints[0] % k_joints],
                                         k_joint_colour[inf.joints[1] % k_joints],
                                         inf.weights[1]);
            for (int a = 0; a < k_around; ++a)
            {
                const auto i0 = static_cast<std::size_t>(r * k_around + a);
                const auto i1 = static_cast<std::size_t>(r * k_around + (a + 1) % k_around);
                debug_.line(deformed_[i0], deformed_[i1], tint);
            }
        }
    }

    /// The bind pose, in outline, so that "where it started" is on screen next to
    /// "where it is".
    void queue_bind_ghost()
    {
        const Uint32 tint = engine::pack_argb(96, 102, 118);
        for (int r = 0; r < k_rings; r += 4)
        {
            for (int a = 0; a < k_around; ++a)
            {
                const auto i0 = static_cast<std::size_t>(r * k_around + a);
                const auto i1 = static_cast<std::size_t>(r * k_around + (a + 1) % k_around);
                debug_.line(tube_.bind.vertices[i0], tube_.bind.vertices[i1], tint);
            }
        }
    }

    void build_panel()
    {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(376.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("rig"))
        {
            ImGui::Text("mode          %s", twisting_ ? "TWIST [T]" : "BEND [T]");
            ImGui::Text("angle         %7.2f deg", static_cast<double>(
                            (twisting_ ? twist_ : bend_) * k_deg));
            if (twisting_)
            {
                ImGui::Text("segments      %d   ([1]-[4])", segments_);
                ImGui::Text("per joint     %7.2f deg", static_cast<double>(
                                twist_ * k_deg / static_cast<float>(segments_)));
            }

            ImGui::Separator();
            ImGui::Text("min ring radius %6.4f  (ring %d)",
                        static_cast<double>(narrowest_), narrowest_ring_);
            ImGui::Text("r cos(d/2) says %6.4f", static_cast<double>(predicted_radius()));
            ImGui::Text("bind radius     %6.4f", static_cast<double>(k_radius));

            ImGui::Separator();
            ImGui::Text("inverse binds  %s  [N]", use_inverse_binds_ ? "ON" : "OFF — the bug");
            ImGui::Text("bind residual  %.3e", static_cast<double>(report_.worst_bind_residual));
            ImGui::Text("joints %zu, depth %zu, roots %zu",
                        report_.joints, report_.depth, report_.roots);
            ImGui::Text("vertices %zu, unnormalised %zu",
                        skin_report_.vertices, skin_report_.unnormalised);
            ImGui::Text("%zu objects, %zu triangles, %d lines",
                        collect_.drawn, triangles_.size(), debug_drawn_);

            ImGui::Separator();
            ImGui::TextUnformatted("left/right angle  [T] twist  [N] binds");
            ImGui::TextUnformatted("[W] weights  [B] bind ghost  [R] reset");
        }
        ImGui::End();
    }

    /// The shot's receipt. Two runs with the same flags print the same numbers,
    /// or the picture is not reproducible whatever it looks like.
    ///
    /// FOUR SHORT LINES RATHER THAN TWO LONG ONES. The lesson quotes this inside
    /// a <pre> block that scrolls and never wraps, and the fold is at about 66
    /// characters — Lesson 7.3 lost fourteen numbers off the right-hand edge
    /// before anybody noticed.
    void write_shot()
    {
        std::printf("rig: %s %.2f deg, %d segments, binds %s\n",
                    twisting_ ? "twist" : "bend",
                    static_cast<double>((twisting_ ? twist_ : bend_) * k_deg),
                    segments_, use_inverse_binds_ ? "on" : "OFF");
        std::printf("rig: min ring radius %.5f at %d, r cos(d/2) %.5f\n",
                    static_cast<double>(narrowest_), narrowest_ring_,
                    static_cast<double>(predicted_radius()));
        std::printf("rig: bind residual %.3e, %zu joints, depth %zu\n",
                    static_cast<double>(report_.worst_bind_residual),
                    report_.joints, report_.depth);
        std::printf("rig: %zu objects, %zu triangles, %d debug lines\n",
                    collect_.drawn, triangles_.size(), debug_drawn_);
        request_quit(engine::save_ppm(fb(), shot_path_));
    }

    // ---- State -------------------------------------------------------------

    static constexpr const char* k_seg_names[4] = {"seg1", "seg2", "seg3", "seg4"};
    static constexpr SDL_Scancode k_seg_keys[4] = {SDL_SCANCODE_1, SDL_SCANCODE_2,
                                                   SDL_SCANCODE_3, SDL_SCANCODE_4};

    const char* shot_path_ = nullptr;

    float bend_ = 0.0f;
    float twist_ = 0.0f;
    bool twisting_ = false;
    int segments_ = 1;
    bool use_inverse_binds_ = true;
    bool weight_view_ = false;
    bool ghost_ = false;

    engine::anim::skeleton skeleton_;
    engine::anim::skinned_mesh tube_;
    engine::anim::skeleton_report report_{};
    engine::anim::skin_report skin_report_{};

    std::vector<transform> pose_;
    std::vector<mat4> posed_;
    std::vector<mat4> palette_;
    std::vector<vec3> deformed_;

    float narrowest_ = 0.0f;
    int narrowest_ring_ = 0;

    engine::ecs::registry world_;
    engine::ecs::hierarchy tree_;
    entity limb_{};
    entity camera_{};

    engine::mesh_pool meshes_;
    engine::mesh_handle skin_handle_{};

    engine::lighting lights_{};
    engine::depth_buffer depth_{k_width, k_height};
    std::vector<engine::scene_object> objects_;
    std::vector<engine::raster_triangle> triangles_;
    engine::projection_scratch scratch_;
    engine::renderable_report collect_{};
    engine::debug_lines debug_;
    int debug_drawn_ = 0;

    engine::masked_input<engine::input> gate_;
    engine::action_map actions_;
    engine::action_id a_quit_{}, a_reset_{}, a_angle_{};
    engine::action_id a_twist_{}, a_bind_{}, a_weights_{}, a_ghost_{};
    engine::action_id a_seg_[4]{};

    engine::debug_ui ui_;
};

ENGINE_MAIN(rig_app)
