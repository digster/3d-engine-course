// demos/collector/main.cpp — the first GAME written against this engine.
//
// Eleven lessons ago, Lesson 5.1 drew a line: `engine/` is a library, `demos/`
// are programs, and a program may include nothing but `<engine/...>`. Every
// program written since has been a DEMONSTRATION — ecs_swarm shows off the ECS,
// hello_cube shows off the renderer, and each was written by the same person who
// had just built the thing it shows off, in the same afternoon.
//
// This one is different in exactly one way, and the whole lesson turns on it:
// **it wants to be a game, and it does not care which subsystem it has to touch
// to be one.** A demo can stop at the edge of the part it is demonstrating. A
// game has to cross every edge there is — load an asset, spawn an entity, parent
// a camera, read a button, move a body, test an overlap, tell the player what
// just happened — and the seams between subsystems are exactly where a public
// API is weakest, because nobody has ever stood on them.
//
// THE RULES OF THE EXERCISE, which are worth stating because they are what makes
// the result mean anything:
//
//   1. Nothing in this file may include an engine internal. That is enforced by
//      the include path (5.1), so it is not a promise — it is a compile error.
//   2. Where the public API cannot do something, this file DOES IT ITSELF, in
//      the open, with a comment naming what was missing. It does not quietly
//      widen the boundary.
//   3. Every such note is a finding, and Lesson 5.12 counts them.
//
// There are six. Four are gaps a later module fills (collision, audio, text,
// the ECS-to-GPU path); one was a promise the engine had already made and not
// kept, and is fixed in this lesson (`engine/gfx/renderable.hpp`); one is a
// const-correctness defect reported and left standing, on purpose.
//
// ---- THE GAME ---------------------------------------------------------------
//
// Drive a rover round an arena and collect twelve orbs before the clock runs
// out. Six sit still and bob; six ride a carousel, so they move while you chase
// them. Eight pillars stand in the way — or rather, they do not, and §8.1 of the
// lesson is about why.
//
//     cmake --build build --target collector
//     ./build/demos/collector                        play it
//     ./build/demos/collector --shot arena.ppm       one deterministic frame
//
// Keys are ACTIONS, declared in `declare_actions()` and bound in one place:
//     [W]/[S] or Up/Down    drive          [A]/[D] or Left/Right   steer
//     [Shift] boost         [R] restart    [Esc] quit
//     [B] camera boom on/off (the lesson's §6)      [G] gameplay gizmos
//     [L] hierarchy links   [P] panels

#include <engine/asset/asset_store.hpp>
#include <engine/core/actions.hpp>
#include <engine/core/log.hpp>
#include <engine/ecs/camera.hpp>
#include <engine/ecs/hierarchy.hpp>
#include <engine/ecs/registry.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/debug_draw.hpp>
#include <engine/gfx/debug_lines.hpp>
#include <engine/gfx/depth_buffer.hpp>
#include <engine/gfx/light.hpp>
#include <engine/gfx/material.hpp>   // 6.5
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/renderable.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>
#include <engine/platform/app.hpp>
#include <engine/ui/debug_ui.hpp>

// TOOLING ONLY. engine/CMakeLists.txt links imgui PUBLIC so a demo may speak the
// vocabulary directly (5.11); not one pixel of the game world is drawn with it.
#include <imgui.h>

#include <engine/platform/main.hpp>

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using engine::ecs::entity;

/// ---- A SMALL FINDING, AND A GOOD ONE --------------------------------------
///
/// Lesson 5.3 gave the engine five log categories and argued for keeping them
/// few. None of them is "the game", and that is correct — they name ENGINE
/// subsystems, and a category called `log_app` inside the library would be the
/// library naming something it knows nothing about.
///
/// What makes it work anyway is the last member of that enum. `log_category_count`
/// is "not a category; the count, for iteration", and it is also exactly where a
/// program's own categories begin — SDL's `SDL_LOG_CATEGORY_CUSTOM + n` scheme,
/// left open by an enum that publishes its own end. A game extends the space
/// without the engine having to anticipate the game.
///
/// THE HALF THAT DOES NOT WORK, reported rather than worked around: the `--log`
/// spec is parsed against `name_of(log_category)`, which returns "?" for
/// anything outside the five. So this category can be filtered in code and
/// cannot be named on the command line. Fixing that means letting a program
/// register a name, which is a real API change and not this lesson's.
constexpr int log_game = engine::log_category_count;

// ---------------------------------------------------------------------------
// Tunables, gathered so the lesson can quote them and the panel can move them
// ---------------------------------------------------------------------------

constexpr int   k_width  = 480;
constexpr int   k_height = 270;
constexpr Uint32 k_background = 0xFF141821u;

constexpr float k_arena_half   = 7.0f;    ///< the floor is 14 x 14 metres
constexpr float k_rover_radius = 0.50f;   ///< for the pickup test, and NOTHING else
constexpr float k_orb_radius   = 0.42f;
constexpr float k_carousel_r   = 4.0f;

/// The rover's box, as a HALF-EXTENT in metres. Named because `update_boom` has
/// to invert it, and two copies of a number that must agree is a bug with a
/// waiting period.
constexpr engine::vec3 k_rover_half{0.30f, 0.22f, 0.42f};
constexpr int   k_orbs_static  = 6;
constexpr int   k_orbs_riding  = 6;
constexpr int   k_pillars      = 8;
constexpr float k_round_time   = 90.0f;

/// How many fixed steps `--shot` runs before drawing, and what it holds down.
///
/// A screenshot of frame zero is a screenshot of a spawn function. These three
/// constants are what make the shot a picture of the GAME — the rover has
/// driven, the carousel has turned, two orbs are gone — and reproducible to the
/// bit, because nothing in the loop below reads a clock.
constexpr int   k_shot_steps = 240;
constexpr float k_shot_drive = 1.0f;
constexpr float k_shot_steer = 0.55f;

/// A transform for a box given its CENTRE and its HALF-EXTENT, both in metres.
///
/// ---- READ THE FACTOR OF TWO, IT IS THE WHOLE FUNCTION ---------------------
///
/// `cube_mesh()` spans **-0.5 to +0.5** on every axis, so a `transform`'s
/// `scale` is the box's FULL SIZE and not its half-extent. Passing a half-extent
/// straight through — which is the natural thing to do, because a half-extent is
/// what a collision test wants and what a debug box takes — builds geometry at
/// HALF the intended size while the gizmo drawn beside it is at full size, and
/// the two disagree by exactly 2x. That is how this file's arena was first
/// built, and the symptom was not "everything is small": it was pillars floating
/// in the void beside a floor a quarter of its intended area, which reads as a
/// camera bug for a good ten minutes.
///
/// **And `icosahedron_mesh()` does not share the convention.** Its vertices sit
/// at distance 1.0 from the origin, so for the orbs `scale` IS the radius, with
/// no factor of two — two primitives in one header, disagreeing about what
/// "unit" means. Lesson 5.12 §7.1 reports it; this function is why the game only
/// had to find out once.
[[nodiscard]] engine::transform box_at(engine::vec3 centre, engine::vec3 half,
                                       engine::quat rotation = engine::quat::identity())
{
    return engine::transform{.position = centre,
                             .rotation = rotation,
                             .scale = {2.0f * half.x, 2.0f * half.y, 2.0f * half.z}};
}

// ---------------------------------------------------------------------------
// The game's own components
// ---------------------------------------------------------------------------
//
// NONE OF THESE ARE THE ENGINE'S, and that is the rule `gfx/renderable.hpp`
// states: the engine defines a component when an ENGINE system reads it, and
// nothing under engine/ will ever look at a `collectible`. The engine supplies
// `transform`, `world_transform`, `parent`, `camera`, `active_camera` and
// `renderable`; the six below are the game.

/// The player. One entity has this.
struct rover
{
    float speed = 0.0f;   ///< signed, along the rover's local +z
    float yaw   = 0.0f;   ///< radians, accumulated
    float bank  = 0.0f;   ///< radians of roll, purely cosmetic — and §6's problem
};

/// "Driving into this scores a point and destroys it."
struct collectible
{
    float radius = k_orb_radius;
};

/// Spin about local y at `rate` rad/s. Orbs use it so a still orb still reads
/// as alive.
struct spinner
{
    float rate = 1.0f;
};

/// Bob up and down about `base_y`. Phase is per-entity so a row of orbs does not
/// pulse in unison, which looks mechanical.
struct bobber
{
    float base_y = 0.0f;
    float phase = 0.0f;
    float amplitude = 0.18f;
};

/// The rotating pivot the riding orbs hang from. One entity has this, and it is
/// INVISIBLE — it has no `renderable`, which is Lesson 5.8's argument arriving
/// as a design rather than a demonstration: the carousel is not a thing you can
/// see, so it does not have the component that makes things visible. There is no
/// flag to set and no branch in the renderer.
struct carousel
{
    float rate = 0.45f;
};

/// A static obstacle. **A tag — no data at all**, and working out why is most of
/// §8.1.
///
/// The first version of this struct carried a `half_extent`, on the reasonable-
/// sounding grounds that a collision test needs one. It does not need one *here*:
/// a pillar is a unit cube under a `world_transform`, so its oriented bounding
/// box is exactly that matrix applied to the cube's own ±0.5 — which means the
/// geometry a solver would want is ALREADY in the registry, in a component the
/// engine defines, put there by the renderer's requirements. Storing it twice
/// would have created the usual second copy that drifts.
///
/// So what is missing from this engine is not the data. It is the solver.
struct pillar
{
};

// ---------------------------------------------------------------------------
// Systems
// ---------------------------------------------------------------------------
//
// Free functions over a registry, the shape ecs_swarm settled on. Note what is
// NOT in any of these signatures: a framebuffer, a renderer, a window. A system
// that needs one of those is a system that cannot be tested, and Lesson 5.11
// paid for that lesson once already with `line3`.

/// Drive the rover. `drive` and `steer` are in [-1, 1] and come from the action
/// map — or, under `--shot`, from two constants. **The system does not know
/// which**, and that is the whole reason it takes floats rather than reading
/// `action_map` itself: a system that reads input directly can only ever be run
/// by a human.
void drive_system(engine::ecs::registry& world, float drive, float steer,
                  bool boost, float h)
{
    constexpr float k_accel = 9.0f;
    constexpr float k_drag = 2.6f;
    constexpr float k_turn = 2.4f;
    constexpr float k_top = 7.5f;
    constexpr float k_bank_max = 0.38f;

    world.view<engine::transform, rover>().each(
        [&](engine::transform& t, rover& r) {
            const float accel = k_accel * (boost ? 1.8f : 1.0f);

            // Semi-implicit Euler: accelerate, THEN integrate with the new
            // velocity. Lesson 1.4 derived the fixed step this runs inside;
            // Module 8 derives why this ordering is the stable one and explicit
            // Euler is not. Here it is simply what a step should look like.
            r.speed += (drive * accel - r.speed * k_drag) * h;
            r.speed = std::fmax(-k_top * 0.45f, std::fmin(k_top, r.speed));

            // TURNING SCALES WITH SPEED, because a vehicle that spins on the
            // spot at a standstill feels like a turret. `tanh`-free version:
            // a plain linear factor, clamped, which is enough for a checkpoint.
            const float authority = std::fmin(1.0f, std::fabs(r.speed) / 2.5f);
            r.yaw -= steer * k_turn * authority * h * (r.speed < 0.0f ? -1.0f : 1.0f);

            // Cosmetic roll into the turn, eased towards its target so it does
            // not snap. This is the field §6 is about: it looks good on the
            // rover and is poison to a camera that inherits it.
            const float bank_target = -steer * k_bank_max * authority;
            r.bank += (bank_target - r.bank) * std::fmin(1.0f, 6.0f * h);

            // LESSON 7.5. `heading` is used two ways on the next two lines and
            // the quaternion is the right storage for exactly one of them:
            // composing with the bank (16 multiplies, and the result is what the
            // transform holds), and rotating a single vector (where a quaternion
            // is 1.57× DEARER than a matrix — 7.4 §10.3). One vector is far below
            // the eight-vector crossover, so it stays a quaternion and the
            // sandwich is paid for once per rover per step.
            const engine::quat heading = engine::quat_y(r.yaw);
            t.rotation = heading * engine::quat_z(r.bank);
            t.position = t.position + heading * engine::vec3{0.0f, 0.0f, r.speed * h};

            // ---- FINDING 1: there is no collision, so the wall is a clamp ----
            //
            // The arena has edges because this loop says so. Six lines, no
            // contact normal, no restitution, no sliding — drive at the corner
            // and the rover stops dead in both axes at once, which is a
            // recognisably wrong-looking thing that a real solver fixes by
            // resolving along ONE normal at a time.
            //
            // Module 8 is thirteen lessons of exactly this. Naming it here is
            // cheaper than pretending the clamp is a design.
            const float limit = k_arena_half - k_rover_radius;
            t.position.x = std::fmax(-limit, std::fmin(limit, t.position.x));
            t.position.z = std::fmax(-limit, std::fmin(limit, t.position.z));
        });
}

/// Turn the carousel. One entity, one line — and every orb parented to it moves,
/// because moving with your parent is not a feature anybody implemented here.
void carousel_system(engine::ecs::registry& world, float h)
{
    world.view<engine::transform, carousel>().each(
        [h](engine::transform& t, carousel& c) {
            // **THE FIRST PLACE IN THIS ENGINE WHERE DRIFT IS A REAL PROBLEM**,
            // and Lesson 7.5 is why the third line is here. This field is
            // *stepped*: every frame multiplies the stored rotation by a small
            // one, so rounding accumulates for as long as the program runs. The
            // `mat3` this replaced had the same disease and a dearer cure —
            // Gram-Schmidt, three normalisations and two projections. One
            // `renormalised_fast` is a subtract and five multiplies, has no
            // `sqrt` and no divide, and Lesson 7.4 §10.4 measured it holding a
            // million compositions at 6e−8 where the unrepaired walk reached
            // 1e−3.
            t.rotation = engine::renormalised_fast(t.rotation * engine::quat_y(c.rate * h));
        });
}

/// Spin the orbs about their own local y.
void spin_system(engine::ecs::registry& world, float h)
{
    world.view<engine::transform, spinner>().each(
        [h](engine::transform& t, spinner& s) {
            t.rotation = engine::renormalised_fast(t.rotation * engine::quat_y(s.rate * h));
        });
}

/// Bob the orbs. Note it advances a PHASE rather than reading a global clock:
/// two orbs created at different times must not share a phase, and a system that
/// asks the clock what time it is cannot give them different ones.
void bob_system(engine::ecs::registry& world, float h)
{
    world.view<engine::transform, bobber>().each(
        [h](engine::transform& t, bobber& b) {
            b.phase += 1.7f * h;
            t.position.y = b.base_y + b.amplitude * std::sin(b.phase);
        });
}

/// What one pickup pass found.
struct pickup_result
{
    int collected = 0;
    float nearest_distance = 1e9f;
    engine::vec3 nearest_point{};
    bool have_nearest = false;
};

/// Collect orbs the rover is touching, and report the nearest one for the gizmo.
///
/// ---- FINDING 2: THE ENGINE HAS NO COLLISION AT ALL -------------------------
///
/// This is a sphere-sphere overlap test written by hand, in a demo, in 2026, in
/// a 3D engine — and it is the correct decision for today. What it is NOT is a
/// small thing: this test is O(rovers x orbs) and there is exactly one rover, so
/// the quadratic is a lie that twelve orbs are hiding. The broadphase that stops
/// it being a lie is Lesson 8.9, and the reason a pickup is the EASY case is
/// that it needs a boolean and nothing else — no contact point, no normal, no
/// penetration depth, no impulse. Compare the pillars in `drive_system`, which
/// need all four and therefore get none.
///
/// **The destruction is deferred**, and that is not a style choice: `view::each`
/// asserts if the lead pool changes during iteration (5.8), so destroying inside
/// the lambda would fire the assertion the ECS was built with. Collect ids, then
/// act.
pickup_result pickup_system(engine::ecs::registry& world,
                            engine::ecs::hierarchy& tree,
                            std::vector<entity>& scratch)
{
    pickup_result out{};

    engine::vec3 rover_pos{};
    bool have_rover = false;
    world.view<engine::ecs::world_transform, rover>().each(
        [&](const engine::ecs::world_transform& w, const rover&) {
            rover_pos = engine::translation_of(w.matrix);
            have_rover = true;
        });
    if (!have_rover) { return out; }

    scratch.clear();
    world.view<engine::ecs::world_transform, collectible>().each(
        [&](entity e, const engine::ecs::world_transform& w, const collectible& c) {
            // The orb's WORLD position — which for a riding orb is a fact about
            // its parent's rotation, computed by `hierarchy::resolve` before this
            // system ran. The pickup test does not know or care that half the
            // orbs are on a carousel.
            const engine::vec3 p = engine::translation_of(w.matrix);
            const engine::vec3 d = p - rover_pos;
            const float dist = engine::length(d);

            if (dist < out.nearest_distance)
            {
                out.nearest_distance = dist;
                out.nearest_point = p;
                out.have_nearest = true;
            }

            // SUM OF RADII, SQUARED COMPARISON AVOIDED ON PURPOSE. The usual
            // trick is to compare squared distances and skip the square root;
            // here the root is already paid for above, because the gizmo wants
            // the real distance in metres to print. Optimising it away would
            // save one `sqrtf` on twelve items and cost the number on screen.
            if (dist <= k_rover_radius + c.radius) { scratch.push_back(e); }
        });

    for (const entity e : scratch)
    {
        // `destroy` removes every component and bumps the slot's generation, so
        // any handle to this orb now resolves to null instead of to whatever
        // lands in the slot next — Lesson 5.4's whole argument, doing its job in
        // the one place a game really does destroy things mid-frame.
        if (world.destroy(e)) { ++out.collected; }
    }
    if (out.collected != 0) { tree.mark_topology_changed(); }

    return out;
}

/// Queue the gameplay gizmos. **No framebuffer in the signature** — Lesson
/// 5.11's rework is what allows a gameplay system to draw.
void gizmo_system(engine::ecs::registry& world, engine::debug_lines& out,
                  const pickup_result& pick)
{
    constexpr Uint32 k_orb_ring = engine::pack_argb(120, 210, 255);
    constexpr Uint32 k_rover_ring = engine::pack_argb(255, 210, 120);
    constexpr Uint32 k_lead = engine::pack_argb(150, 255, 170);
    constexpr Uint32 k_pillar_box = engine::pack_argb(255, 120, 120);

    world.view<engine::ecs::world_transform, collectible>().each(
        [&](const engine::ecs::world_transform& w, const collectible& c) {
            out.sphere(engine::translation_of(w.matrix), c.radius, k_orb_ring);
        });

    world.view<engine::ecs::world_transform, rover>().each(
        [&](const engine::ecs::world_transform& w, const rover&) {
            out.sphere(engine::translation_of(w.matrix), k_rover_radius, k_rover_ring);
            if (pick.have_nearest)
            {
                out.line(engine::translation_of(w.matrix), pick.nearest_point, k_lead);
            }
        });

    // THE BOXES ARE THE POINT OF §8.1. Every pillar gets one, drawn from the same
    // half-extent a collision test would use — so you can SEE that the geometry
    // the engine would need is present and correctly placed, and drive straight
    // through it anyway. What is missing is not the data.
    world.view<engine::ecs::world_transform, pillar>().each(
        [&](const engine::ecs::world_transform& w, const pillar&) {
            // ±0.5 AND NOT THE PILLAR'S HALF-EXTENT IN METRES — the third time
            // the unit cube's convention bites in this one file, and the first
            // two are commented on `box_at`. `world_from_local` ALREADY carries
            // the scale, so the half-extent passed here is in the cube's own
            // space, where it is 0.5 by definition. Passing metres draws a box
            // exactly twice the size of the thing it is meant to outline — which
            // looks like a collision margin, and was shipped as one for about
            // twenty minutes.
            out.box(w.matrix, {0.5f, 0.5f, 0.5f}, k_pillar_box);
        });
}

// ---------------------------------------------------------------------------
// The program
// ---------------------------------------------------------------------------

class collector_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char* argv[]) override
    {
        for (int i = 1; i < argc; ++i)
        {
            if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc) { shot_path_ = argv[++i]; }
            if (SDL_strcmp(argv[i], "--no-boom") == 0) { boom_ = false; }
            if (SDL_strcmp(argv[i], "--shot-steps") == 0 && i + 1 < argc)
            {
                shot_steps_ = SDL_atoi(argv[++i]);
            }
            if (SDL_strcmp(argv[i], "--gizmos") == 0) { gizmos_ = true; }
        }

        return {.title = "collector — a game on the public API",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        declare_actions();

        // Three generated meshes through the asset store, because generated
        // content and a file on disk must go through the same door (5.5). The
        // game never loads a file, and that is precisely why it is worth putting
        // its meshes here: if generated content needed a second mechanism, the
        // first one would be wrong.
        mesh_box_ = assets_.insert_mesh(
            "box", engine::with_normals(engine::cube_mesh(), engine::normal_style::flat));
        mesh_orb_ = assets_.insert_mesh(
            "orb", engine::with_normals(engine::icosahedron_mesh(),
                                        engine::normal_style::smooth));

        lights_.key.direction = engine::normalised(engine::vec3{-0.45f, -0.78f, -0.44f});
        lights_.key.colour = {1.0f, 0.96f, 0.88f};
        // LESSON 6.2. Was `intensity = 1.0f`. Same light, stated honestly: the
        // pi that used to live inside the shading now lives here.
        lights_.key.irradiance = engine::k_reference_irradiance;
        lights_.ambient = {0.11f, 0.12f, 0.16f};

        build_world();

        // ImGui is optional infrastructure: a failure to start it means no
        // panels, not no game. `start` logs which backend it chose and every
        // later call is a safe no-op.
        (void)ui_.start(window(), renderer());

        if (shot_path_ != nullptr) { run_scripted_opening(); }
        return true;
    }

    void on_input() override
    {
        // FIRST, before anything reads a key. The capture flags are computed
        // inside NewFrame, so a `begin_frame` placed next to the panels leaks one
        // frame of every keystroke into the game (5.11).
        ui_.begin_frame();

        // The keyboard, minus whatever ImGui is claiming. `masked_input`
        // satisfies 5.10's `input_snapshot` concept, so `action_map::update`
        // takes it without knowing anything has changed.
        gate_.update(in(), ui_.wants_keyboard(), ui_.wants_mouse());
        actions_.update(gate_);

        // FRAME-SCOPED EDGES BELONG HERE and nowhere else — `on_input` runs
        // exactly once per frame, which is what makes `pressed()` mean "once".
        if (actions_.pressed(a_quit_))    { request_quit(); }
        if (actions_.pressed(a_restart_)) { build_world(); }
        if (actions_.pressed(a_boom_))    { boom_ = !boom_; }
        if (actions_.pressed(a_gizmos_))  { gizmos_ = !gizmos_; }
        if (actions_.pressed(a_links_))   { links_ = !links_; }
        if (actions_.pressed(a_panels_))  { panels_ = !panels_; }
    }

    void on_fixed_step(float h) override
    {
        if (shot_path_ != nullptr) { return; }   // the shot already ran its steps
        if (won_ || time_left_ <= 0.0f)
        {
            // Still run the cosmetic systems, so a finished round is a scene
            // rather than a freeze-frame. The rover stops because nothing drives
            // it, not because anything was disabled.
            step_world(0.0f, 0.0f, false, h);
            return;
        }

        time_left_ = std::fmax(0.0f, time_left_ - h);
        step_world(actions_.value(a_drive_), actions_.value(a_steer_),
                   actions_.held(a_boost_), h);
    }

    void on_frame(float alpha) override
    {
        // `alpha` IS DELIBERATELY UNUSED, and saying so beats silently taking
        // it. Lesson 1.4 derived the interpolation factor so that a renderer can
        // draw a moment between two simulation states; using it means keeping
        // the PREVIOUS state of everything you draw, which for an ECS means a
        // second copy of every transform. At a 60 Hz fixed step against a 60 Hz
        // display the two states are one step apart and the stutter is invisible;
        // on a 144 Hz display it would not be. The honest position is that this
        // game does not interpolate, that Pong does (demos/common/pong.cpp), and
        // that doing it here is Exercise 3.
        (void)alpha;

        fb().clear(k_background);
        depth_.clear();

        const engine::ecs::world_transform* cam_placement =
            world_.get<engine::ecs::world_transform>(camera_);
        const engine::ecs::camera* cam = world_.get<engine::ecs::camera>(camera_);
        if (cam_placement == nullptr || cam == nullptr) { return; }

        const engine::mat4 view = engine::ecs::view_from_camera(*cam_placement);
        const engine::vec3 eye = engine::ecs::eye_of(*cam_placement);
        const engine::projector proj{
            engine::ecs::projection_of(*cam, static_cast<float>(k_width)
                                                 / static_cast<float>(k_height)),
            engine::viewport{0.0f, 0.0f, static_cast<float>(k_width),
                             static_cast<float>(k_height), 0.0f, 1.0f},
            engine::near_mode::clip};

        // ---- ONE CALL, AND IT USED TO BE TWENTY LINES IN EVERY DEMO --------
        //
        // Lesson 5.12 hoisted this into the engine. What it replaced is still
        // visible in `ecs_swarm`'s `render_system`, and the diff is not about
        // length: the matrix-to-transform trick it hides is a genuine seam, and a
        // seam repeated in three files is three places to fix it.
        collect_ = engine::collect_renderables(world_, assets_.meshes(), objects_);

        const engine::render_options opts{.cull = engine::cull_choice::back,
                                          .normals = engine::normal_source::vertex,
                                          .shading = engine::shade_eval::gouraud};
        engine::collect_triangles(triangles_, scratch_, objects_, assets_.meshes(),
                                  {view, eye}, proj, lights_, opts);

        const engine::fill_style style{.shade = engine::shading::vertex_colour,
                                       .cull = engine::cull_mode::back,
                                       .eye = eye};
        engine::draw_triangles(fb(), &depth_, triangles_, false, style);

        // ---- The debug layer, after the world and before the HUD ------------
        debug_.clear();
        if (links_) { queue_hierarchy_links(); }
        if (gizmos_) { gizmo_system(world_, debug_, last_pick_); }
        debug_drawn_ = engine::draw_debug_lines(fb(), view, proj, debug_);

        // AGE THE QUEUE AFTER THE FLUSH. Doing it before drops every line that
        // was queued this frame with the default lifetime (5.11).
        debug_.advance(time().dt());

        if (shot_path_ != nullptr)
        {
            // `printf`, NOT a log call, and the distinction is worth one line.
            // A log is a DIAGNOSTIC: off by default, filtered by category, aimed
            // at whoever is debugging. This is the program's OUTPUT — the whole
            // reason the run exists — and a tool whose result only appears when
            // you pass the right `--log` spec is a tool with a trapdoor.
            //
            // It is also the shot's receipt. Two runs with the same
            // `--shot-steps` must print identical numbers, which is what makes
            // the picture reproducible rather than merely repeatable-looking.
            std::printf("collector: %d/%d orbs, %zu objects, %zu triangles, "
                        "%d debug lines, %d steps\n",
                        score_, k_orbs_static + k_orbs_riding, collect_.drawn,
                        triangles_.size(), debug_drawn_, shot_steps_);
            request_quit(engine::save_ppm(fb(), shot_path_));
        }
    }

    void on_overlay() override
    {
        draw_hud();
        if (panels_) { build_panels(); }
        ui_.render();
    }

    void on_event(const SDL_Event& event) override { (void)ui_.handle_event(event); }

    void on_stop() override
    {
        ui_.stop();

        // NOTHING IN THIS COURSE HAD EVER FREED ANYTHING until Lesson 5.5, and a
        // program that exits does not need to — the OS reclaims it. It is here
        // because the COUNT is the check: `unload_all` returns how many assets it
        // released, and a game that leaks one per round would show up as a number
        // that grows. That is the cheapest possible leak detector and it costs
        // one line.
        const int freed = assets_.unload_all();
        ENGINE_LOG_INFO(log_game, "collector: released %d asset(s)", freed);
    }

private:
    // ---- Building the world ------------------------------------------------

    void declare_actions()
    {
        // Declared by NAME, bound in one place, and nothing below this function
        // mentions a scancode. That is the whole of 5.10, and the payoff here is
        // concrete: `drive` is bound to four keys at once and the action map sums
        // them, so holding Up and Down together gives exactly zero without any
        // rule having been written for that case.
        a_drive_   = actions_.declare("drive");
        a_steer_   = actions_.declare("steer");
        a_boost_   = actions_.declare("boost");
        a_restart_ = actions_.declare("restart");
        a_quit_    = actions_.declare("quit");
        a_boom_    = actions_.declare("camera_boom");
        a_gizmos_  = actions_.declare("gizmos");
        a_links_   = actions_.declare("hierarchy_links");
        a_panels_  = actions_.declare("panels");

        (void)actions_.bind_key(a_drive_, SDL_SCANCODE_W, +1.0f);
        (void)actions_.bind_key(a_drive_, SDL_SCANCODE_UP, +1.0f);
        (void)actions_.bind_key(a_drive_, SDL_SCANCODE_S, -1.0f);
        (void)actions_.bind_key(a_drive_, SDL_SCANCODE_DOWN, -1.0f);
        (void)actions_.bind_key(a_steer_, SDL_SCANCODE_D, +1.0f);
        (void)actions_.bind_key(a_steer_, SDL_SCANCODE_RIGHT, +1.0f);
        (void)actions_.bind_key(a_steer_, SDL_SCANCODE_A, -1.0f);
        (void)actions_.bind_key(a_steer_, SDL_SCANCODE_LEFT, -1.0f);
        (void)actions_.bind_key(a_boost_, SDL_SCANCODE_LSHIFT);
        (void)actions_.bind_key(a_boost_, SDL_SCANCODE_RSHIFT);
        (void)actions_.bind_key(a_restart_, SDL_SCANCODE_R);
        (void)actions_.bind_key(a_quit_, SDL_SCANCODE_ESCAPE);
        (void)actions_.bind_key(a_boom_, SDL_SCANCODE_B);
        (void)actions_.bind_key(a_gizmos_, SDL_SCANCODE_G);
        (void)actions_.bind_key(a_links_, SDL_SCANCODE_L);
        (void)actions_.bind_key(a_panels_, SDL_SCANCODE_P);
    }

    /// Spawn everything. Called at start and by [R], and it must be safe to call
    /// twice — which is why it clears first and why every id below is re-read.
    void build_world()
    {
        world_.clear();
        tree_.mark_topology_changed();
        score_ = 0;
        time_left_ = k_round_time;
        won_ = false;
        elapsed_ = 0.0f;

        spawn_floor();
        spawn_walls();
        spawn_pillars();
        spawn_rover();
        spawn_orbs();
        spawn_camera();

        // Resolve once before the first frame, so the first pickup test and the
        // first render both see composed matrices rather than identity.
        (void)tree_.rebuild_and_resolve(world_);
    }

    void spawn_floor()
    {
        const entity e = world_.create();
        engine::ecs::add_hierarchy_components(
            world_, e, box_at({0.0f, -0.25f, 0.0f}, {k_arena_half, 0.25f, k_arena_half}));
        // `closed = true` — it is a slab, not a sheet, so back-face culling is
        // valid on it. 3.4's precondition, and the reason the floor is a flattened
        // cube rather than a quad.
        world_.add<engine::renderable>(
            e, engine::renderable{.mesh = mesh_box_,
                                  .mat = {.tint = engine::pack_argb(58, 62, 74),
                                          // MATTE, AND LESSON 3.8 PREDICTED WHY. The
                                          // floor is ONE QUAD — two triangles, 14 m
                                          // across — and Gouraud shading evaluates the
                                          // surface at the four corners and interpolates
                                          // between them. The diffuse term is constant
                                          // (one normal, one directional light), but a
                                          // specular term is VIEW-dependent, so it
                                          // differs at each corner and the interpolation
                                          // reveals the diagonal the quad is split
                                          // along: a hard seam across the middle of the
                                          // arena that moves as you drive.
                                          //
                                          // Three fixes exist and only one is available
                                          // today. Subdivide the floor (more vertices,
                                          // same artifact, smaller); shade per pixel
                                          // (3.8's `shade_eval::phong`, which costs the
                                          // whole scene); or give the floor no view-
                                          // dependent term at all. A concrete floor is
                                          // matte in the first place, so the third is
                                          // not a workaround — it is the right answer
                                          // that happens to also be free.
                                          .surface = {.roughness = 0.90f}},
                                  .closed = true});
    }

    /// Four kerbs at the arena edge.
    ///
    /// They exist so that `drive_system`'s position clamp has something to BE.
    /// A game where the player stops at an invisible line is a game with a bug;
    /// a game where the player stops at a wall is a game with a wall — and the
    /// interesting part is that the engine cannot tell the difference, because
    /// the clamp is six lines of arithmetic in a gameplay system and these boxes
    /// are four `renderable`s that nothing tests against. The wall is a picture
    /// of a rule enforced somewhere else. Lesson 5.12 §8.1.
    void spawn_walls()
    {
        constexpr float t = 0.25f;                 // kerb half-thickness
        constexpr float y = 0.30f;                 // kerb half-height
        const float o = k_arena_half + t;          // just outside the floor
        const engine::vec3 halves[4] = {{k_arena_half + 2.0f * t, y, t},
                                        {k_arena_half + 2.0f * t, y, t},
                                        {t, y, k_arena_half + 2.0f * t},
                                        {t, y, k_arena_half + 2.0f * t}};
        const engine::vec3 centres[4] = {
            {0.0f, y, +o}, {0.0f, y, -o}, {+o, y, 0.0f}, {-o, y, 0.0f}};

        for (int i = 0; i < 4; ++i)
        {
            const entity e = world_.create();
            engine::ecs::add_hierarchy_components(world_, e,
                                                  box_at(centres[i], halves[i]));
            world_.add<engine::renderable>(
                e, engine::renderable{.mesh = mesh_box_,
                                      .mat = {.tint = engine::pack_argb(86, 90, 104),
                                              .surface = {.roughness = 0.55f}}});
        }
    }

    void spawn_pillars()
    {
        for (int i = 0; i < k_pillars; ++i)
        {
            const float a = 6.2831853f * static_cast<float>(i)
                            / static_cast<float>(k_pillars);
            const engine::vec3 half{0.35f, 1.15f, 0.35f};
            const entity e = world_.create();
            // `half.y` as the centre height is what puts a box ON the ground
            // rather than half-buried in it — the other half of the convention
            // `box_at` exists to make explicit.
            engine::ecs::add_hierarchy_components(
                world_, e,
                box_at({5.0f * std::cos(a), half.y, 5.0f * std::sin(a)}, half,
                       engine::quat_y(a)));
            world_.add<engine::renderable>(
                e, engine::renderable{.mesh = mesh_box_,
                                      .mat = {.tint = engine::pack_argb(158, 126, 92),
                                              .surface = {.roughness = 0.53f}}});
            world_.add<pillar>(e, pillar{});
        }
    }

    void spawn_rover()
    {
        rover_ = world_.create();
        engine::ecs::add_hierarchy_components(
            world_, rover_, box_at({0.0f, k_rover_half.y, -3.0f}, k_rover_half));
        world_.add<engine::renderable>(
            rover_, engine::renderable{.mesh = mesh_box_,
                                       .mat = {.tint = engine::pack_argb(232, 168, 60),
                                               .surface = {.roughness = 0.46f}}});
        world_.add<rover>(rover_, rover{});

        // The nose, so the rover's facing is readable from any angle.
        //
        // ---- A PITFALL WORTH MEETING NOW: SCALE COMPOSES -------------------
        // The parent is scaled (0.45, 0.34, 0.62), so this child's local units
        // are NOT metres — a local offset of z = 1.0 lands 0.62 m forward, and a
        // uniform local scale of 0.5 comes out anisotropic in world space,
        // because the parent's non-uniform scale is still in the chain. That is
        // not a bug in `hierarchy`; it is what composing a scaled basis MEANS
        // (2.8). The numbers below are chosen with the parent's scale in mind.
        nose_ = world_.create();
        engine::ecs::add_hierarchy_components(
            world_, nose_,
            engine::transform{.position = {0.0f, 0.25f, 0.55f},
                              .scale = {0.5f, 0.5f, 0.35f}});
        world_.add<engine::renderable>(
            nose_, engine::renderable{.mesh = mesh_box_,
                                      .mat = {.tint = engine::pack_argb(248, 236, 210),
                                              .surface = {.roughness = 0.42f}}});
        (void)engine::ecs::set_parent(world_, nose_, rover_);
    }

    void spawn_orbs()
    {
        // Six that sit still.
        constexpr engine::vec3 spots[k_orbs_static] = {
            {-4.3f, 0.8f, 3.4f}, {4.1f, 0.8f, 3.8f},  {-5.0f, 0.8f, -1.9f},
            {4.7f, 0.8f, -3.1f}, {-1.3f, 0.8f, 5.3f}, {1.8f, 0.8f, -5.4f}};
        for (int i = 0; i < k_orbs_static; ++i)
        {
            const entity e = make_orb(spots[i], 0.85f * static_cast<float>(i));
            world_.add<bobber>(e, bobber{.base_y = spots[i].y,
                                         .phase = 1.1f * static_cast<float>(i)});
        }

        // …and six that ride. The carousel is an entity with a `transform` and a
        // `carousel`, and NO `renderable` — you cannot see it, and there is no
        // flag saying so.
        carousel_ = world_.create();
        engine::ecs::add_hierarchy_components(
            world_, carousel_, engine::transform{.position = {0.0f, 0.0f, 0.0f}});
        world_.add<carousel>(carousel_, carousel{});

        for (int i = 0; i < k_orbs_riding; ++i)
        {
            const float a = 6.2831853f * static_cast<float>(i)
                            / static_cast<float>(k_orbs_riding);
            const entity e = make_orb({k_carousel_r * std::cos(a), 1.05f,
                                       k_carousel_r * std::sin(a)},
                                      0.6f * static_cast<float>(i));
            (void)engine::ecs::set_parent(world_, e, carousel_);
        }
    }

    [[nodiscard]] entity make_orb(engine::vec3 where, float spin_phase)
    {
        const entity e = world_.create();
        engine::ecs::add_hierarchy_components(
            world_, e,
            // NO FACTOR OF TWO HERE, and that is the asymmetry `box_at`'s
            // comment warns about: an icosahedron's vertices are at distance 1,
            // so its `scale` is its RADIUS. The orb drawn and the sphere the
            // pickup test uses are therefore the same number, which is the only
            // reason the gizmo in §8.1 can be trusted.
            engine::transform{.position = where,
                              .rotation = engine::quat_y(spin_phase),
                              .scale = {k_orb_radius, k_orb_radius, k_orb_radius}});
        world_.add<engine::renderable>(
            e, engine::renderable{.mesh = mesh_orb_,
                                  .mat = {.tint = engine::pack_argb(120, 214, 255),
                                          .surface = {.roughness = 0.38f}}});
        world_.add<collectible>(e, collectible{});
        world_.add<spinner>(e, spinner{.rate = 1.6f});
        return e;
    }

    /// The camera, the boom, and §6's whole argument in six lines.
    void spawn_camera()
    {
        // THE BOOM. An entity with a transform and nothing else — not even a
        // component of its own — whose only job is to be a place in the chain
        // where the rover's ROLL can be cancelled. See `step_world`.
        // **LESSON 7.5 SPLIT THIS INTO TWO ENTITIES, AND THE COMPILER IS WHY.**
        // `update_boom` used to write `S⁻¹ · Rz(−bank)` into a field called
        // `rotation`, which a `mat3` accepted without comment. A `quat` does not,
        // and it is right not to: that product is not a rotation. Its columns are
        // not even perpendicular when the scale is non-uniform, which the rover's
        // is — it is a SHEARED basis, and it had been living in this field since
        // Lesson 5.12.
        //
        // The fix is not a wider type, it is one more link. A `transform` is
        // `T · R · S` — scale innermost — so a single node can express
        // "rotate then scale" and cannot express "scale then rotate". A CHAIN
        // can: put the unscale in one node and the unroll in its child, and the
        // hierarchy multiplies them in the order the algebra asked for.
        //
        //     rover  world      = H · Rz(bank) · S
        //     unscale node local =                   S⁻¹      -> H · Rz(bank)
        //     pivot   node local =                        Rz(−bank) -> H
        //
        // Two entities, no shear anywhere, and the same camera basis to the bit.
        boom_scale_ = world_.create();
        engine::ecs::add_hierarchy_components(world_, boom_scale_, engine::transform{});
        (void)engine::ecs::set_parent(world_, boom_scale_, rover_);

        boom_pivot_ = world_.create();
        engine::ecs::add_hierarchy_components(world_, boom_pivot_, engine::transform{});
        (void)engine::ecs::set_parent(world_, boom_pivot_, boom_scale_);

        camera_ = world_.create();

        // The camera's LOCAL placement, expressed in boom space with `look_along`
        // — which is 2.9's `look_at` inverted: `look_at` asks "what matrix takes
        // the world into this camera's view", and this asks "where does the
        // camera have to BE". The transform it returns is what a `transform`
        // component holds.
        //
        // Note the units. The boom is a child of the rover, and the rover is
        // SCALED, so these numbers are in rover-local units and not metres —
        // the same trap the nose comment names. `unscale_boom` below is what
        // makes them metres again, and it is the tidier half of §6.
        engine::ecs::add_hierarchy_components(
            world_, camera_,
            engine::ecs::look_along({0.0f, 4.4f, -6.8f}, {0.0f, 0.2f, 4.5f},
                                    {0.0f, 1.0f, 0.0f}));
        world_.add<engine::ecs::camera>(
            camera_, engine::ecs::camera{.fovy = 55.0f * 3.14159265f / 180.0f,
                                         .near_plane = 0.1f,
                                         .far_plane = 120.0f});
        (void)engine::ecs::set_parent(world_, camera_, boom_pivot_);
        (void)engine::ecs::set_active_camera(world_, camera_);
    }

    // ---- Running the world -------------------------------------------------

    void step_world(float drive, float steer, bool boost, float h)
    {
        elapsed_ += h;

        drive_system(world_, drive, steer, boost, h);
        carousel_system(world_, h);
        spin_system(world_, h);
        bob_system(world_, h);

        update_boom();

        // RESOLVE BEFORE ANYTHING READS A WORLD MATRIX. The pickup test wants
        // world positions, and half the orbs are on a moving parent — so a
        // resolve after the pickup would test against last step's carousel, and
        // the bug would look like "the orbs are slightly hard to catch".
        tree_.rebuild_and_resolve(world_);

        last_pick_ = pickup_system(world_, tree_, dead_);
        score_ += last_pick_.collected;
        if (score_ >= k_orbs_static + k_orbs_riding) { won_ = true; }
    }

    /// Cancel the rover's cosmetic roll, in the boom, every step.
    ///
    /// ---- §6: WHY A PARENTED CAMERA IS NOT A FOLLOW CAMERA -----------------
    ///
    /// Parent a camera to the player and you get a rigid follow for free, which
    /// is the first thing everybody does and is WRONG in a specific way: the
    /// camera inherits the whole basis, including the 22 degrees of roll the
    /// rover leans into a turn. The horizon then see-saws, and it reads as the
    /// WORLD tilting rather than the car leaning, because the camera is the
    /// viewer's inner ear.
    ///
    /// The fix is not a special case in the renderer and not a "camera follow"
    /// system. It is one more link in the chain: a boom that applies the inverse
    /// roll, so the product is a basis with the rover's yaw and none of its roll.
    /// Press [B] to turn it off and watch the horizon tip.
    ///
    /// It also undoes the rover's non-uniform scale, for the reason the nose
    /// comment gives: without it, "6.4 units back" is 6.4 x 0.62 = 3.97 m and
    /// the camera sits inside the rover on the z axis while being too far away
    /// on x. Inheriting scale is right for a nose and wrong for a camera.
    void update_boom()
    {
        engine::transform* unscale = world_.get<engine::transform>(boom_scale_);
        engine::transform* unroll = world_.get<engine::transform>(boom_pivot_);
        const rover* r = world_.get<rover>(rover_);
        if (unscale == nullptr || unroll == nullptr || r == nullptr) { return; }

        if (!boom_)
        {
            unscale->scale = {1.0f, 1.0f, 1.0f};
            unroll->rotation = engine::quat::identity();
            return;
        }

        // ---- THE ORDER OF THESE TWO IS NOT A DETAIL --------------------------
        //
        // We want the boom's WORLD basis to be the rover's heading and nothing
        // else. The rover's world basis is `H * Rz(bank) * S`, so
        //
        //     local = (H * Rz(bank) * S)^-1 * H
        //           = S^-1 * Rz(-bank) * H^-1 * H
        //           = S^-1 * Rz(-bank)
        //
        // — the inverse of a product reverses its order, so the UNSCALE comes
        // FIRST. Writing `unroll * unscale` instead (which reads more naturally
        // in English: "undo the roll, then undo the scale") produces
        // `Rz(-bank) * S^-1`, and since a rotation and a non-uniform scale do
        // not commute, the product is a basis that is neither rigid nor visibly
        // wrong. It cost this file its first crash; see Lesson 5.12 §6.1.
        //
        // **LESSON 7.5 IS WHY THAT IS NOW TWO ASSIGNMENTS TO TWO ENTITIES.** The
        // product above is `diagonal × rotation`, and a `transform` builds
        // `rotation × diagonal`. They are not the same matrix and the difference
        // is a SHEAR. One node could only pretend to hold it, which is exactly
        // what it did while the field was a `mat3`. Two nodes hold it honestly,
        // in the order the derivation above wrote it down — parent first.
        unscale->scale = {1.0f / (2.0f * k_rover_half.x),
                          1.0f / (2.0f * k_rover_half.y),
                          1.0f / (2.0f * k_rover_half.z)};
        unroll->rotation = engine::quat_z(-r->bank);
    }

    /// The `--shot` opening: a fixed number of steps with two constant inputs.
    void run_scripted_opening()
    {
        constexpr float h = 1.0f / 60.0f;
        for (int i = 0; i < shot_steps_; ++i)
        {
            time_left_ = std::fmax(0.0f, time_left_ - h);
            step_world(k_shot_drive, k_shot_steer, false, h);
        }
    }

    void queue_hierarchy_links()
    {
        constexpr Uint32 k_link = engine::pack_argb(120, 190, 255);
        world_.view<engine::ecs::world_transform, engine::ecs::parent>().each(
            [&](const engine::ecs::world_transform& w, const engine::ecs::parent& p) {
                const engine::ecs::world_transform* pw =
                    world_.get<engine::ecs::world_transform>(p.value);
                if (pw == nullptr) { return; }
                debug_.line(engine::translation_of(w.matrix),
                            engine::translation_of(pw->matrix), k_link);
            });
    }

    // ---- Telling the player anything at all --------------------------------

    /// ---- FINDING 3: THE ENGINE CANNOT PUT A CHARACTER ON THE SCREEN --------
    ///
    /// A game has to say "8 of 12" and "0:43" and "you won". This engine has no
    /// way to. There is no font, no glyph, no text call anywhere under
    /// `engine/` — so the HUD below is drawn by **SDL**, with
    /// `SDL_RenderDebugTextFormat` and its built-in 8x8 bitmap font, which is a
    /// debugging facility that happens to be in the window.
    ///
    /// Three things follow, and they are why this is a finding and not a shrug:
    ///
    ///   1 It only exists on `surface::renderer`. A `--shot` run is headless,
    ///     has no `SDL_Renderer`, and therefore produces a picture of the game
    ///     with NO SCORE ON IT. Look at the expected result in §11 — the number
    ///     you most want is the one thing missing.
    ///   2 It is unavailable on the GPU path for the same reason, so the engine's
    ///     REAL renderer is the one that cannot show a number.
    ///   3 It is not composited with the world. It is drawn on top by a different
    ///     renderer at a different scale, so it cannot be tinted, faded, placed
    ///     in the scene, or measured.
    ///
    /// Module 6 fixes this properly — a baked font atlas and an overlay batch
    /// both renderers consume. Until then, this function is the honest shape of
    /// the gap: nine lines that work, in the one configuration that has a
    /// `SDL_Renderer`, by reaching past the engine to the library underneath it.
    void draw_hud()
    {
        SDL_Renderer* const r = renderer();
        if (r == nullptr) { return; }   // headless: no HUD, and that is the point

        SDL_SetRenderScale(r, 2.0f, 2.0f);
        SDL_SetRenderDrawColor(r, 226, 230, 240, 255);

        const int total = k_orbs_static + k_orbs_riding;
        const int minutes = static_cast<int>(time_left_) / 60;
        const int seconds = static_cast<int>(time_left_) % 60;
        SDL_RenderDebugTextFormat(r, 6.0f, 6.0f, "ORBS %2d / %-2d    %d:%02d    fps %5.1f",
                                  score_, total, minutes, seconds,
                                  static_cast<double>(time().fps()));

        if (won_)
        {
            SDL_SetRenderDrawColor(r, 150, 255, 170, 255);
            SDL_RenderDebugTextFormat(r, 6.0f, 20.0f, "ALL ORBS COLLECTED in %.1f s  —  [R] again",
                                      static_cast<double>(elapsed_));
        }
        else if (time_left_ <= 0.0f)
        {
            SDL_SetRenderDrawColor(r, 255, 140, 140, 255);
            SDL_RenderDebugText(r, 6.0f, 20.0f, "OUT OF TIME  —  [R] to try again");
        }

        SDL_SetRenderDrawColor(r, 150, 155, 170, 255);
        SDL_RenderDebugText(r, 6.0f, 122.0f,
                            "[WASD] drive  [Shift] boost  [B] boom  [G] gizmos  "
                            "[L] links  [P] panels  [R] restart");
        SDL_SetRenderScale(r, 1.0f, 1.0f);
    }

    void build_panels()
    {
        if (!ui_.running()) { return; }

        ImGui::SetNextWindowPos(ImVec2(16.0f, 180.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(330.0f, 250.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("collector"))
        {
            ImGui::Text("score %d / %d", score_, k_orbs_static + k_orbs_riding);
            ImGui::Text("elapsed %.1f s   remaining %.1f s",
                        static_cast<double>(elapsed_), static_cast<double>(time_left_));
            ImGui::Separator();

            // THE REPORT `collect_renderables` RETURNS, ON SCREEN. `unresolved`
            // and `missing_mesh` are both zero in a healthy frame, and both are
            // here anyway, because a counter you only look at when you suspect
            // something is a counter you will not think to look at.
            ImGui::Text("drawn %zu   unresolved %zu   dead mesh %zu",
                        collect_.drawn, collect_.unresolved, collect_.missing_mesh);
            ImGui::Text("triangles %zu   debug lines %d",
                        triangles_.size(), debug_drawn_);

            const engine::ecs::hierarchy_report& h = tree_.last_report();
            ImGui::Text("entities %zu   roots %zu   levels %zu",
                        h.entities, h.roots, tree_.levels());
            ImGui::Separator();

            ImGui::Checkbox("camera boom (unroll + unscale)", &boom_);
            ImGui::Checkbox("gameplay gizmos", &gizmos_);
            ImGui::Checkbox("hierarchy links", &links_);

            if (ImGui::Button("restart")) { build_world(); }
            ImGui::SameLine();
            if (ImGui::Button("collect all"))
            {
                // A cheat, and it exists to make the win state reachable in one
                // click while testing. Tooling, not gameplay.
                dead_.clear();
                world_.view<collectible>().each(
                    [this](entity e, const collectible&) { dead_.push_back(e); });
                for (const entity e : dead_) { (void)world_.destroy(e); }
                score_ = k_orbs_static + k_orbs_riding;
                won_ = true;
                tree_.mark_topology_changed();
            }

            ImGui::Separator();
            if (ImGui::BeginTable("actions", 2, ImGuiTableFlags_RowBg))
            {
                for (std::size_t i = 0; i < actions_.action_count(); ++i)
                {
                    const engine::action_id id{static_cast<std::uint16_t>(i)};
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    const std::string_view n = actions_.name_of(id);
                    ImGui::Text("%.*s", static_cast<int>(n.size()), n.data());
                    ImGui::TableNextColumn();
                    ImGui::Text("%+.2f", static_cast<double>(actions_.value(id)));
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    // ---- State -------------------------------------------------------------

    const char* shot_path_ = nullptr;
    int shot_steps_ = k_shot_steps;

    engine::asset_store assets_;
    engine::mesh_handle mesh_box_;
    engine::mesh_handle mesh_orb_;
    engine::lighting lights_;

    engine::ecs::registry world_;
    engine::ecs::hierarchy tree_;
    entity rover_{};
    entity nose_{};
    entity carousel_{};
    entity boom_scale_{};
    entity boom_pivot_{};
    entity camera_{};

    engine::action_map actions_;
    engine::masked_input<engine::input> gate_;
    engine::action_id a_drive_{}, a_steer_{}, a_boost_{}, a_restart_{}, a_quit_{};
    engine::action_id a_boom_{}, a_gizmos_{}, a_links_{}, a_panels_{};

    engine::debug_ui ui_;
    engine::debug_lines debug_;
    int debug_drawn_ = 0;

    int score_ = 0;
    float time_left_ = k_round_time;
    float elapsed_ = 0.0f;
    bool won_ = false;
    bool boom_ = true;
    bool gizmos_ = false;
    bool links_ = false;
    bool panels_ = true;
    pickup_result last_pick_{};
    engine::renderable_report collect_{};

    // Working storage owned across frames, so a steady-state frame allocates
    // nothing. Four vectors, four reasons, all the same reason.
    std::vector<engine::scene_object> objects_;
    std::vector<engine::raster_triangle> triangles_;
    std::vector<entity> dead_;
    engine::projection_scratch scratch_;

    engine::depth_buffer depth_{k_width, k_height};
};

}   // namespace

ENGINE_MAIN(collector_app)
