// demos/ecs_swarm/main.cpp — composition, and now structure.
//
// Lesson 5.8 built this program to make one argument: an entity is which
// components it has, and twenty-four of the things on screen are invisible
// because they lack a `geometry` rather than because anything hid them. That
// argument still stands and the entities that made it are still here.
//
// LESSON 5.9 ADDS THE ONE THING A FLAT WORLD CANNOT DO. Every placement used to
// be in world space, which is fine for a swarm orbiting an origin and useless the
// moment one thing has to move with another. Now:
//
//     sun ── ring member ── moon          three levels, and the middle one is
//      │         │                        where the interesting sentence lives
//      │         └── (every sixth)
//      ├── waypoint (invisible mover)
//      └── camera   (when you press [C])
//
// A moon's world position is computed by NOBODY. It falls out of
//
//     world(moon) = world(planet) * local(moon)
//
// which is the composition rule and the whole of a scene graph. Press [F] and
// the sun drifts; every ring member, every moon and every waypoint follows,
// because following is not a feature anyone implemented — it is what "my
// placement is relative to my parent" MEANS.
//
// THE CAMERA IS AN ENTITY NOW, and that is the other half of the lesson. It has
// a `transform`, a `world_transform`, a `camera` and — while it is the one being
// rendered from — an `active_camera` tag. Press [C] and it is PARENTED to a ring
// member: it rides along, resolved by the same pass as everything else, and the
// renderer never learns that anything changed.
//
// LESSON 5.10 TOOK THE KEYBOARD OUT OF THE GAMEPLAY CODE. Every one of those
// bracketed keys above used to be an `SDL_SCANCODE_*` in a switch; they are now
// ACTIONS, declared by name and bound in one place. Nothing below asks about a
// key. Press [K] and the spawn action is rebound from Space to Enter at runtime,
// which is a thing the old switch could not do at any price.
//
// It also demonstrates the two ways to read an edge, and WHY there are two:
//   - the UI toggles are read in `on_input`, which runs exactly once per frame
//   - `spawn` is read with `consume_pressed` inside `on_fixed_step`, which runs
//     zero or more times per frame — so a two-step frame must not spawn twice
//     and a zero-step frame must not lose the press
//
// LESSON 5.11 GAVE IT EYES AND A CONTROL PANEL, and the two halves argue for
// each other. The tree above was, until this lesson, a claim in a comment: you
// could see 154 objects moving and had to TAKE ON FAITH that a moon's placement
// composes through a planet's. Now [L] draws a line from every entity to its
// parent and the tree is a picture. Press [H] to detach half the ring and the
// lines to the sun vanish; press [F] and the sun drifts, dragging its lines with
// it. That is a debug view doing the only job a debug view has — making a claim
// checkable.
//
// The panel is Dear ImGui, and it replaced five SDL_RenderDebugText lines with
// hand-placed y coordinates. Note what the panel can do that they could not:
// tables, a slider that changes a value while you watch it, buttons that call
// the same functions the keys call, and a TEXT FIELD — which is the whole reason
// this lesson has an input section. Type "spawn" into the filter box and the
// world does not fill with sparks, because `masked_input` is withholding the
// keyboard from the action map while ImGui has focus.
//
//     cmake --build build --target ecs_swarm
//     ./build/demos/ecs_swarm                        a window
//     ./build/demos/ecs_swarm --shot swarm.ppm       one frame, no window

#include <engine/asset/asset_store.hpp>
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
#include <engine/gfx/mesh.hpp>
#include <engine/gfx/projector.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/gfx/scene.hpp>
#include <engine/gfx/soft_renderer.hpp>
#include <engine/gfx/viewport.hpp>
#include <engine/math/mat4.hpp>
#include <engine/math/transform.hpp>
#include <engine/platform/app.hpp>
#include <engine/ui/debug_ui.hpp>

// The one third-party header a demo in this course is allowed to include, and it
// is allowed because engine/CMakeLists.txt links imgui PUBLIC — argued in
// engine/include/engine/ui/debug_ui.hpp. Everything below that says `ImGui::` is
// TOOLING; not one pixel of the world is drawn with it.
#include <imgui.h>

#include <engine/platform/main.hpp>

#include <cmath>
#include <vector>

namespace {

using engine::ecs::entity;

// ===========================================================================
//  THE COMPONENTS
// ===========================================================================
//
// Six demo components, plus four the engine now supplies: `parent` and
// `world_transform` from ecs/hierarchy.hpp, `camera` and `active_camera` from
// ecs/camera.hpp. Every one of them is a plain struct with no base class, no
// virtual anything and no registration macro — `registry` asks a component type
// for nothing but that it be movable.

/// Where an entity is, **relative to its parent**.
///
/// The same `engine::transform` as in Lesson 5.8, and the same alias — but the
/// meaning widened, and it widened without the type changing by one character.
/// That was designed for in Lesson 2.8, which is why the function that turns one
/// into a matrix is called `parent_from_local` and not `world_from_local`. An
/// entity with no `parent` component has the world as its parent, so 5.8's flat
/// interpretation is the special case rather than the rule.
using placement = engine::transform;

/// What shape an entity is, and whether it is closed enough to cull.
struct geometry
{
    engine::mesh_handle mesh;
    bool closed = true;
};

/// What an entity looks like.
///
/// **THIS STRUCT USED TO DECLARE ITS OWN FIELDS**, and its comment said why:
/// "`scene_object`'s `tint` and `surface`, which have always described the same
/// thing and never been the same thing." A demo — restricted to the engine's
/// public API — had invented an engine type because the engine did not offer
/// one. Lesson 6.5 offers one, so the fields are gone and only the component
/// wrapper remains.
///
/// It stays a distinct type rather than becoming `engine::material` outright
/// because an ECS component is an *identity*, not just a payload: `view<geometry,
/// material>` names this component, and two components that happened to share a
/// layout would be indistinguishable to the registry. Lesson 5.7 made the same
/// call about `placement`.
struct material
{
    /// **A HANDLE, not a material** — Lesson 6.5's second half.
    ///
    /// The first half moved the fields into `engine::material`; this moves the
    /// *storage* out of the component. Ninety-six ring drones cycle through six
    /// tints, so there are six materials here and ninety-six references to them.
    /// Copying the material into every entity stored the same thirty-six bytes
    /// sixteen times over — and, worse, made "change the drones' roughness" a
    /// loop instead of an assignment.
    ///
    /// This is the case the pool exists for, and the rule it demonstrates is
    /// about SHARING rather than size: a `scene_object` still holds its material
    /// by value, because it has exactly one.
    engine::material_handle mat{};
};

/// A circular path — now around the entity's PARENT rather than around the world.
///
/// Nothing in this struct changed in Lesson 5.9, and nothing in `orbit_system`
/// changed either. What changed is where the number it writes ends up meaning
/// something, which is the cheapest possible demonstration that a hierarchy is a
/// property of the composition and not of the data.
struct orbit
{
    float radius = 1.0f;
    float speed = 1.0f;
    float phase = 0.0f;
    float tilt = 0.0f;
};

/// A tumble, integrated over the fixed step.
///
/// Two Euler angles rather than an axis and an angle, because axis-angle rotation
/// does not exist in this engine yet — Module 7 derives it by way of quaternions.
/// The component does not care: when that lands, this becomes
/// `{ quat orientation; vec3 angular_velocity; }` and not one line of `registry`,
/// `pool`, `view` or `hierarchy` changes.
struct spin
{
    float rate = 1.0f;
    float wobble = 0.5f;
    float angle = 0.0f;
};

/// Seconds until this entity destroys itself. Only the sparks have one.
struct lifetime
{
    float remaining = 1.0f;
};

/// The orbit angles a free camera flies on. One entity has this; it is a
/// component like any other, and the camera is an entity like any other.
struct camera_orbit
{
    float radius = 9.0f;
    float azimuth = 0.0f;
    float elevation = 0.32f;
    engine::vec3 target{0.0f, 0.0f, 0.0f};
};

// ===========================================================================
//  THE SYSTEMS
// ===========================================================================

/// Put every orbiting entity where its orbit says — **in its parent's space**.
///
/// Byte for byte the function Lesson 5.8 shipped. It writes a `placement`, which
/// used to be a world position and is now a local one, and it has no idea which.
/// That is the point: a system that reads and writes local data does not need to
/// know whether anything is parented to anything.
void orbit_system(engine::ecs::registry& world, float t)
{
    world.view<placement, orbit>().each([t](placement& p, const orbit& o) {
        const float a = o.phase + o.speed * t;
        // A circle of radius r in the x-z plane, tilted by `tilt` about the x
        // axis: x is untouched by the tilt (it is the axis), and the y-z pair is
        // the rotation. Multiplying x by cos(tilt) as well would give an ellipse
        // that looks similar and is not a circle.
        const float ct = std::cos(o.tilt);
        const float st = std::sin(o.tilt);
        p.position = {o.radius * std::cos(a),
                      o.radius * std::sin(a) * st,
                      o.radius * std::sin(a) * ct};
    });
}

/// Advance every spinning entity by one simulation step.
///
/// Also unchanged — and worth pausing on, because the sun has one of these. A
/// rotation applied to a parent rotates *its children's whole coordinate frame*,
/// so spinning the sun sweeps the entire ring around with it. Nothing in this
/// function knows that.
void spin_system(engine::ecs::registry& world, float h)
{
    world.view<placement, spin>().each([h](placement& p, spin& s) {
        s.angle += s.rate * h;
        p.rotation = engine::rotation_y(s.angle) * engine::rotation_x(s.wobble * s.angle);
    });
}

/// Age the sparks, and destroy the ones that have run out.
///
/// **The two-phase shape is the one rule a view imposes**, and Lesson 5.9 gives
/// it a second reason to exist: destroying an entity changes the SHAPE of the
/// hierarchy, so the resolver's level order goes stale and has to be told. Both
/// facts are handled in the same three lines.
void lifetime_system(engine::ecs::registry& world, engine::ecs::hierarchy& tree, float h,
                     std::vector<entity>& scratch)
{
    scratch.clear();
    world.view<lifetime>().each([h, &scratch](entity e, lifetime& l) {
        l.remaining -= h;
        if (l.remaining <= 0.0f) { scratch.push_back(e); }
    });
    if (scratch.empty()) { return; }

    for (entity e : scratch) { world.destroy(e); }
    tree.mark_topology_changed();
}

/// Aim the free camera from its orbit angles.
///
/// `look_along` builds the transform that PUTS a camera somewhere looking at
/// something — the inverse of Lesson 2.9's question, which asked for the matrix
/// that takes the world into the camera's view. Writing the placement and letting
/// `view_from_camera` invert it is what makes a camera an ordinary object.
void camera_orbit_system(engine::ecs::registry& world)
{
    world.view<placement, camera_orbit>().each([](placement& p, const camera_orbit& c) {
        const engine::vec3 eye =
            c.target + engine::vec3{c.radius * std::cos(c.elevation) * std::sin(c.azimuth),
                                    c.radius * std::sin(c.elevation),
                                    c.radius * std::cos(c.elevation) * std::cos(c.azimuth)};
        p = engine::ecs::look_along(eye, c.target, {0.0f, 1.0f, 0.0f});
    });
}

/// Turn everything visible into the `scene_object` array the renderer wants.
///
/// **The seam moved one level deeper and is still a seam.** In Lesson 5.8 this
/// read a `placement` and copied it; now it reads a `world_transform` — a
/// composed 4x4 — and has to get that matrix into a `scene_object`, which holds a
/// `transform`.
///
/// The conversion below is EXACT and is a trick worth understanding rather than
/// copying. `parent_from_local` builds `affine(columns scaled by `scale`,
/// position)`, and `transform::rotation` is a general `mat3` with no orthonormality
/// requirement — so setting the rotation to the matrix's whole linear part and the
/// scale to 1 reproduces any affine matrix bit for bit. It abuses the fact that
/// the field is named `rotation` and typed `mat3`, which is precisely why it is a
/// bridge and not a design. Module 6 gives the renderer a matrix directly and
/// this function loses its last four lines.
void render_system(engine::ecs::registry& world, const engine::material_pool& materials,
                   std::vector<engine::scene_object>& out)
{
    out.clear();
    world.view<engine::ecs::world_transform, geometry, material>().each(
        [&out, &materials](const engine::ecs::world_transform& w, const geometry& g,
                           const material& m) {
            // The resolve, once per entity per frame — the same place
            // `bind_albedo` sits, and for the same reason. A stale handle
            // resolves to null and the object draws with the default material
            // rather than reading freed memory, which is the whole point of
            // asking a pool instead of dereferencing a pointer.
            const engine::material* found = materials.get(m.mat);
            out.push_back(engine::scene_object{
                .xform = {.position = engine::translation_of(w.matrix),
                          .rotation = engine::linear_of(w.matrix),
                          .scale = {1.0f, 1.0f, 1.0f}},
                .geometry = g.mesh,
                .name = "entity",
                .mat = (found != nullptr) ? *found : engine::material{},
                .closed = g.closed});
        });
}

/// Draw the hierarchy: one line from every entity to its parent.
///
/// **LOOK AT THE SIGNATURE.** A registry, a queue, two booleans. No framebuffer,
/// no projector, no camera, no viewport — which is exactly what Lesson 5.11's
/// split bought, and the reason this function can sit here among the simulation
/// systems instead of inside the renderer. Before the rework it could not have
/// been written at all without handing a `framebuffer&` down to it, and the
/// moment you do that every caller of every caller needs one too.
///
/// It is also the cheapest useful debug view there is, because a tree is exactly
/// the kind of structure that is obvious as a picture and invisible as numbers.
/// An orphan is a line that is not there; a re-parent is a line that moved; a
/// cycle would be a loop you can see. `hierarchy_report` counts all three and
/// this shows you WHICH.
void hierarchy_debug_system(engine::ecs::registry& world,
                            engine::debug_lines& out, bool links, bool axes)
{
    if (!links && !axes) { return; }

    // Two colours, because the two ends of a link mean different things: the
    // child is where the line starts and the parent is what it points at.
    constexpr Uint32 k_link = engine::pack_argb(120, 190, 255, 255);
    constexpr Uint32 k_root = engine::pack_argb(255, 190, 90, 255);

    world.view<engine::ecs::world_transform, engine::ecs::parent>().each(
        [&](engine::ecs::entity e, const engine::ecs::world_transform& w,
            const engine::ecs::parent& p) {
            (void)e;
            if (axes)
            {
                // A short triad, so a hundred of them do not become a hedge.
                out.axes(w.matrix, 0.35f);
            }
            if (!links) { return; }

            const engine::ecs::world_transform* pw =
                world.get<engine::ecs::world_transform>(p.value);

            // NO PARENT TRANSFORM MEANS NO PARENT: either the field is
            // `null_entity` (a root) or it names something that has been
            // destroyed (an orphan, which the resolver has already promoted to a
            // root). Both draw a stub straight up, so a root is visible AS a root
            // rather than as an entity that simply drew nothing.
            if (pw == nullptr)
            {
                const engine::vec3 at = engine::translation_of(w.matrix);
                out.line(at, at + engine::vec3{0.0f, 0.35f, 0.0f}, k_root);
                return;
            }

            out.line(engine::translation_of(w.matrix), engine::translation_of(pw->matrix),
                     k_link);
        });
}

// ===========================================================================
//  THE PROGRAM
// ===========================================================================

constexpr int k_width = 480;
constexpr int k_height = 270;
constexpr Uint32 k_background = 0xFF0C0E14u;   // pack_argb(12, 14, 20)

constexpr int k_ring_count = 96;
constexpr int k_waypoint_count = 24;
constexpr int k_spark_burst = 32;
constexpr int k_moons_every = 6;     ///< every sixth ring member gets moons
constexpr int k_moons_each = 2;

/// A tiny linear congruential generator, seeded by hand.
///
/// Deterministic on purpose: `--shot` has to produce the same picture on every
/// machine and every run, and `std::rand` is neither seeded the same way
/// everywhere nor required to produce the same sequence. Numerical Recipes'
/// constants; good enough to scatter a swarm and not good enough for anything
/// else, which is stated here so nobody borrows it for a simulation.
class lcg
{
public:
    explicit constexpr lcg(std::uint32_t seed) : state_(seed) {}

    [[nodiscard]] std::uint32_t next()
    {
        state_ = state_ * 1664525u + 1013904223u;
        return state_;
    }

    /// Uniform-ish in [lo, hi).
    [[nodiscard]] float range(float lo, float hi)
    {
        const float u = static_cast<float>(next() >> 8) / static_cast<float>(1u << 24);
        return lo + u * (hi - lo);
    }

private:
    std::uint32_t state_;
};

class swarm_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char* argv[]) override
    {
        for (int i = 1; i < argc; ++i)
        {
            if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc) { shot_path_ = argv[++i]; }
        }

        return {.title = "ecs_swarm — one world, ten components, three levels",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        cube_ = assets_.insert_mesh("cube",
                                    engine::with_normals(engine::cube_mesh(),
                                                         engine::normal_style::flat));
        ico_ = assets_.insert_mesh("icosahedron",
                                   engine::with_normals(engine::icosahedron_mesh(),
                                                        engine::normal_style::smooth));
        torus_ = assets_.insert_mesh("torus", engine::make_torus(40, 20, 1.0f, 0.34f));

        declare_actions();

        lights_.key.direction = engine::normalised(engine::vec3{-0.4f, -0.7f, -0.55f});
        lights_.key.colour = {1.0f, 0.97f, 0.90f};
        // Lesson 6.2: `intensity` became `irradiance`, and the value became pi.
        // A light of "intensity 1" was always a light of irradiance pi; the
        // pi has moved into the BRDF where it belongs, so it must now be
        // written down here. Same picture, stated honestly.
        lights_.key.irradiance = engine::k_reference_irradiance;

        build_world();

        // The UI, on the window and renderer the platform already made.
        //
        // THE RETURN VALUE IS DELIBERATELY DISCARDED and that is not sloppiness:
        // false here means `--shot` (no window) or a GPU surface (no
        // SDL_Renderer), and neither is a reason for this program to fail to
        // start. `debug_ui` logs which it was, at info, and every subsequent call
        // is a safe no-op — so the code below never asks again except where a
        // panel would actually be built.
        (void)ui_.start(window(), renderer());

        // A pose that does not depend on the clock, so `--shot` is reproducible.
        if (shot_path_ != nullptr) { t_ = 1.234f; step_world(1.234f); }
        return true;
    }

    /// Give ImGui every event — and give it to `in()` as well, which the engine
    /// already did before this hook ran (Lesson 5.2's ordering guarantee).
    ///
    /// **Both consumers see everything.** It is tempting to "route" events —
    /// hand them to the UI and stop if it takes one — and it is wrong: steal a
    /// key-UP from `engine::input` and `input` goes on believing the key is
    /// held, forever, because a level is only ever corrected by the event that
    /// contradicts it. The arbitration happens one layer later and on LEVELS
    /// rather than events, in `on_input` below.
    void on_event(const SDL_Event& event) override
    {
        (void)ui_.handle_event(event);
    }

    /// Map this frame's input onto actions, then act on the frame-scoped edges.
    ///
    /// **This hook exists because of this file** (Lesson 5.10). It runs exactly
    /// once per frame, after `in()` is published and before any simulation step,
    /// which is the only moment at which updating an action map is correct: a map
    /// updated in `on_frame` would leave every step of the frame reading last
    /// frame's actions.
    ///
    /// The toggles below are read with `pressed()`, the frame-scoped edge, and
    /// that is safe HERE and nowhere else — Lesson 1.4's rule, unchanged. The one
    /// action that a step needs, `spawn`, is read differently; see
    /// `on_fixed_step`.
    void on_input() override
    {
        // FIRST, BEFORE ANYTHING READS A CAPTURE FLAG. ImGui computes
        // WantCaptureKeyboard and WantCaptureMouse inside NewFrame, from the
        // events processed since the last one. Open the frame later — down in
        // on_overlay, next to the panels, where it looks like it belongs — and
        // every read below answers about the PREVIOUS frame, so the first
        // keystroke after clicking into a text field also reaches the game. One
        // frame of leakage, every time, and invisible unless you look for it.
        ui_.begin_frame();

        // THE SEAM, IN TWO LINES. `gate_` is an `engine::input` with the
        // keyboard and/or mouse withheld — a different TYPE that satisfies the
        // same concept — so `action_map::update` takes it without knowing, and
        // not one character of Lesson 5.10's map had to change.
        //
        // Note that the map is still updated. Skipping the update instead would
        // freeze every level: hold [Left], click into the filter box, and the
        // camera would yaw forever. Updating through the mask reports the key as
        // UP, which produces the release edge — which is the behaviour you want,
        // because giving focus to a text field genuinely should let go of the
        // movement keys.
        gate_.update(in(), ui_.wants_keyboard(), ui_.wants_mouse());
        actions_.update(gate_);

        if (actions_.pressed(a_quit_))     { request_quit(); }
        if (actions_.pressed(a_materials_)) { toggle_materials(); }
        if (actions_.pressed(a_orbits_))   { toggle_orbits(); }
        if (actions_.pressed(a_cull_))     { cull_swarm(); }
        if (actions_.pressed(a_rebuild_))  { build_world(); }
        if (actions_.pressed(a_ride_))     { toggle_camera_ride(); }
        if (actions_.pressed(a_detach_))   { toggle_detach(); }
        if (actions_.pressed(a_drift_))    { drifting_ = !drifting_; }
        if (actions_.pressed(a_rebind_))   { rebind_spawn(); }
        if (actions_.pressed(a_panels_))   { show_panels_ = !show_panels_; }
        if (actions_.pressed(a_links_))    { show_links_ = !show_links_; }
        if (actions_.pressed(a_axes_))     { show_axes_ = !show_axes_; }
    }

    void on_fixed_step(float h) override
    {
        if (shot_path_ != nullptr) { return; }

        // THE ONE EDGE THAT IS READ IN A STEP, and it is read differently for a
        // reason worth pausing on. `on_fixed_step` runs zero or more times per
        // frame, so `pressed()` here would spawn twice on a two-step frame and
        // lose the press entirely on a zero-step one. `consume_pressed` takes one
        // queued press and returns true at most once per physical press, however
        // the steps fall.
        if (actions_.consume_pressed(a_spawn_)) { spawn_sparks(); }

        t_ += h;

        // LEVELS, read in the step — which is the other half of Lesson 1.4's rule
        // and is exactly as correct as the edges above are not. `value()` is the
        // summed contribution of every binding, so Left and Right cancel and the
        // camera holds still, with no `if` anywhere saying so.
        if (!riding_)
        {
            camera_.azimuth += (0.15f + 2.0f * actions_.value(a_yaw_)) * h;
            camera_.elevation += 1.5f * actions_.value(a_pitch_) * h;
            camera_.elevation = std::fmax(-1.4f, std::fmin(1.4f, camera_.elevation));
            camera_.radius += 6.0f * actions_.value(a_zoom_) * h;
            camera_.radius = std::fmax(2.5f, std::fmin(24.0f, camera_.radius));
        }

        step_world(h);
    }

    void on_frame(float alpha) override
    {
        (void)alpha;

        render_system(world_, materials_, objects_);

        // QUEUE, ONCE PER FRAME, AFTER THE RESOLVE. The only ordering this has
        // is "after `world_transform` is current", which the fixed steps have
        // just made true. It is called here rather than from `step_world`
        // because a step runs zero or more times per frame (Lesson 1.4) and a
        // two-step frame would queue every line twice.
        hierarchy_debug_system(world_, debug_, show_links_, show_axes_);

        fb().clear(k_background);
        depth_.clear();

        // The camera is looked up rather than held. If the entity tagged
        // `active_camera` were destroyed, this would return null and the frame
        // would draw nothing — which is a case a scene can legitimately be in,
        // and is why it is a branch rather than an assertion.
        const entity cam = engine::ecs::find_active_camera(world_);
        const engine::ecs::world_transform* cam_world =
            cam.valid() ? world_.get<engine::ecs::world_transform>(cam) : nullptr;
        const engine::ecs::camera* lens =
            cam.valid() ? world_.get<engine::ecs::camera>(cam) : nullptr;

        if (cam_world != nullptr && lens != nullptr)
        {
            const engine::mat4 view = engine::ecs::view_from_camera(*cam_world);
            const engine::vec3 eye = engine::ecs::eye_of(*cam_world);
            const engine::projector proj{
                engine::ecs::projection_of(*lens, static_cast<float>(k_width)
                                                      / static_cast<float>(k_height)),
                engine::viewport{0.0f, 0.0f, static_cast<float>(k_width),
                                 static_cast<float>(k_height), 0.0f, 1.0f},
                engine::near_mode::clip};

            const engine::render_options opts{.cull = engine::cull_choice::back};
            engine::collect_triangles(triangles_, scratch_, objects_, assets_.meshes(),
                                      {view, eye}, proj, lights_, opts, &stats_);

            const engine::fill_style style{.shade = engine::shading::vertex_colour,
                                           .cull = engine::cull_mode::back,
                                           .eye = eye};
            engine::draw_triangles(fb(), &depth_, triangles_, false, style);

            // THE FLUSH, AFTER THE SCENE, so debug geometry draws on top of it.
            // This is the only line in the program that knows both what was
            // queued and where the camera is, and it is four words long because
            // the queue did the decoupling.
            debug_drawn_ = engine::draw_debug_lines(fb(), view, proj, debug_);
        }

        // AGE THE QUEUE AT THE END OF THE FRAME, AFTER THE FLUSH — and this is
        // the single most likely thing to get wrong in the whole system. Run it
        // first and every line queued this frame with the default lifetime is
        // deleted before it is ever drawn, which presents as a debug view that
        // shows nothing, which reads as "my code did not run".
        //
        // It is outside the camera branch on purpose: a frame with no active
        // camera draws nothing, and a queue that is not aged because nothing was
        // drawn is a queue that grows without limit while you are looking at a
        // black screen.
        //
        // The count is taken BEFORE the ageing, because that is the number that
        // describes this frame: afterwards the single-frame lines are gone, which
        // is the system working correctly and a useless thing to report.
        debug_queued_ = debug_.size();
        debug_.advance(time().dt());

        if (shot_path_ != nullptr)
        {
            const engine::ecs::hierarchy_report& r = tree_.last_report();
            SDL_Log("ecs_swarm: %zu entities, %zu components, %zu pools, %zu drawn, "
                    "%d triangles",
                    world_.size(), world_.component_count(), world_.pool_count(),
                    objects_.size(), static_cast<int>(triangles_.size()));
            SDL_Log("ecs_swarm: hierarchy %zu rows, %zu roots, %zu levels, %zu orphans, "
                    "%zu cycles",
                    r.entities, r.roots, r.levels, r.orphans, r.cycles);

            // Lesson 5.11. Printed on the headless path because a picture cannot
            // be asserted on and three integers can: `drawn` below `queued` means
            // the near-plane clipper rejected something, and `dropped` above zero
            // means the capacity is too small for this world.
            SDL_Log("ecs_swarm: debug lines queued %zu, drawn %d, dropped %zu, ui %s",
                    debug_queued_, debug_drawn_, debug_.dropped(),
                    ui_.running() ? "on" : "off");
            request_quit(engine::save_ppm(fb(), shot_path_));
        }
    }

    /// The panels, and then the UI's own draw.
    ///
    /// **This hook is where a debug UI belongs and there is exactly one reason
    /// why**: it runs after `blit_framebuffer()` and before `present()`, which
    /// is the only moment at which "over the rendered frame, under the vsync
    /// wait" is true. Draw earlier and the framebuffer blit paints over the
    /// panel; draw later and there is nothing left to draw into.
    ///
    /// WHAT THIS REPLACED is worth a moment. Until Lesson 5.11 this function was
    /// five `SDL_RenderDebugTextFormat` calls with hand-placed y coordinates —
    /// 6, 20, 34, 48, 62 — a layout maintained by arithmetic in a comment. It
    /// could not be scrolled, collapsed, sorted, or edited, and adding a line in
    /// the middle meant renumbering the ones below it. That is the "why we do
    /// not hand-roll this" argument in miniature: none of what ImGui gives back
    /// is graphics, and all of it is work.
    void on_overlay() override
    {
        if (!ui_.running()) { return; }

        build_panels();
        ui_.render();
    }

private:
    // ---- The panels ---------------------------------------------------------

    /// Everything the old text HUD said, plus everything it could not.
    ///
    /// IMMEDIATE MODE IS THE WHOLE POINT. There is no widget tree here, no
    /// `Panel` object, nothing to keep in sync with the world and nothing to
    /// invalidate when an entity is destroyed. This function runs from scratch
    /// every frame and reads the live world, so the panel CANNOT show a stale
    /// number — which is the failure mode of every retained-mode debug UI ever
    /// built, and the reason a debug tool in particular wants this model.
    ///
    /// The checkboxes below write the SAME booleans the keys in `on_input`
    /// toggle. That is deliberate: a control panel must not be a second source
    /// of truth about the program's state, or it becomes another thing to debug.
    void build_panels()
    {
        if (!show_panels_) { return; }

        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340.0f, 470.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("ecs_swarm"))
        {
            const engine::ecs::hierarchy_report& h = tree_.last_report();
            const auto v = world_.view<engine::ecs::world_transform, geometry, material>();

            ImGui::Text("%.1f fps  (%.2f ms)", static_cast<double>(time().fps()),
                        1000.0 * static_cast<double>(time().dt()));

            if (ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // A TABLE, which is the first thing five lines of debug text
                // wanted to be and could never become. It aligns, it resizes with
                // the window, and adding a row does not renumber anything.
                if (ImGui::BeginTable("world", 2, ImGuiTableFlags_SizingStretchProp))
                {
                    row_u("entities", world_.size());
                    row_u("components", world_.component_count());
                    row_u("pools", world_.pool_count());
                    row_u("hierarchy rows", h.entities);
                    row_u("roots", h.roots);
                    row_u("levels", h.levels);
                    row_u("orphans", h.orphans);
                    row_u("cycles", h.cycles);
                    row_u("view candidates", v.size_hint());
                    row_u("drawn", objects_.size());
                    row_u("triangles", triangles_.size());
                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader("Debug draw", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("hierarchy links  [L]", &show_links_);
                ImGui::Checkbox("axis triads  [G]", &show_axes_);
                ImGui::SliderFloat("marker seconds", &marker_seconds_, 0.0f, 8.0f, "%.1f s");
                ImGui::SetItemTooltip("How long the markers left by [H] and [Space] survive. "
                                      "Zero means one frame — which is to say, invisible.");

                // QUEUED, DRAWN AND DROPPED, TOGETHER. The three numbers only
                // mean anything as a set: drawn less than queued means something
                // clipped, and dropped above zero means the picture you are
                // looking at is not the whole picture — which is the failure a
                // bounded queue would otherwise hide.
                ImGui::Text("queued %zu / %zu   drawn %d   dropped %zu",
                            debug_queued_, debug_.capacity(), debug_drawn_,
                            debug_.dropped());
                ImGui::ProgressBar(static_cast<float>(debug_queued_)
                                       / static_cast<float>(debug_.capacity()),
                                   ImVec2(-1.0f, 6.0f), "");
                if (ImGui::Button("clear queue")) { debug_.clear(); }
            }

            if (ImGui::CollapsingHeader("World controls", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // BUTTONS THAT CALL THE FUNCTIONS THE KEYS CALL — not copies of
                // what the keys do. Two ways in, one implementation; the moment
                // they diverge the panel starts lying.
                if (ImGui::Button("rebuild [R]")) { build_world(); }
                ImGui::SameLine();
                if (ImGui::Button("cull [X]")) { cull_swarm(); }
                ImGui::SameLine();
                if (ImGui::Button("sparks [Space]")) { spawn_sparks(); }

                ImGui::Checkbox("sun drifts  [F]", &drifting_);
                bool detached = detached_;
                if (ImGui::Checkbox("outer half detached  [H]", &detached)) { toggle_detach(); }
                bool riding = riding_;
                if (ImGui::Checkbox("camera rides a planet  [C]", &riding))
                {
                    toggle_camera_ride();
                }
            }
        }
        ImGui::End();

        build_input_panel();
    }

    /// The input panel — and the reason this lesson has an input section at all.
    ///
    /// The filter box is a TEXT FIELD, which is the one widget that makes the
    /// two consumers of the keyboard genuinely collide. Click into it, type
    /// `spawn`, and watch: the `p` does not toggle anything, the space bar does
    /// not spawn sparks, and the two "blocked" lines below turn on. Click out
    /// and every key works again — including, on that very frame, a release edge
    /// for anything you were holding when you clicked in.
    void build_input_panel()
    {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 494.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(340.0f, 210.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("input"))
        {
            ImGui::InputTextWithHint("##filter", "filter actions — type here",
                                     filter_, sizeof(filter_));

            // THE SEAM, MADE VISIBLE. Three numbers that are normally invisible
            // machinery: what the UI is claiming, and how far the virtual cursor
            // has fallen behind the real one while it claimed the mouse.
            ImGui::TextColored(gate_.blocking_keyboard() ? ImVec4(1.0f, 0.55f, 0.3f, 1.0f)
                                                         : ImVec4(0.5f, 0.6f, 0.5f, 1.0f),
                               "keyboard: %s", gate_.blocking_keyboard() ? "UI" : "game");
            ImGui::SameLine();
            ImGui::TextColored(gate_.blocking_mouse() ? ImVec4(1.0f, 0.55f, 0.3f, 1.0f)
                                                      : ImVec4(0.5f, 0.6f, 0.5f, 1.0f),
                               "   mouse: %s", gate_.blocking_mouse() ? "UI" : "game");
            ImGui::Text("cursor offset %+.0f, %+.0f px",
                        static_cast<double>(gate_.cursor_offset_x()),
                        static_cast<double>(gate_.cursor_offset_y()));
            ImGui::SetItemTooltip("How far the game's virtual cursor has fallen behind the "
                                  "real one. It grows only while the UI owns the mouse, and "
                                  "it is what stops the camera whipping round when you let "
                                  "go of a slider.");

            ImGui::Separator();
            ImGui::Text("spawn is on %s", spawn_on_enter_ ? "[Enter]" : "[Space] / [LMB]");
            ImGui::SameLine();
            if (ImGui::SmallButton("rebind [K]")) { rebind_spawn(); }

            // THE BINDING TABLE, READ FROM THE MAP rather than written out — so
            // it cannot go stale when [K] rebinds something, and so a settings
            // screen is now a straightforward exercise rather than a feature.
            if (ImGui::BeginTable("bindings", 3,
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY
                                      | ImGuiTableFlags_SizingStretchProp,
                                  ImVec2(0.0f, 0.0f)))
            {
                ImGui::TableSetupColumn("action");
                ImGui::TableSetupColumn("source");
                ImGui::TableSetupColumn("value");
                ImGui::TableHeadersRow();

                for (const engine::binding& b : actions_.bindings())
                {
                    const std::string_view name = actions_.name_of(b.action);
                    if (filter_[0] != '\0' && name.find(filter_) == std::string_view::npos)
                    {
                        continue;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(name.data(), name.data() + name.size());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(source_name(b));
                    ImGui::TableNextColumn();
                    ImGui::Text("%+.2f", static_cast<double>(actions_.value(b.action)));
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    /// One "label | value" row. A named helper because it appears eleven times,
    /// and eleven copies of two lines is how a table starts drifting.
    static void row_u(const char* label, std::size_t value)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::Text("%zu", value);
    }

    /// A binding's source, spelled for a human.
    ///
    /// SDL_GetScancodeName is the right answer for a key and it is worth knowing
    /// it exists: a rebinding UI that prints `SDL_SCANCODE_LEFTBRACKET` is a
    /// rebinding UI nobody can use, and the layout-correct name is the platform's
    /// to know, not ours.
    [[nodiscard]] static const char* source_name(const engine::binding& b)
    {
        switch (b.source)
        {
        case engine::input_source::key:
            return SDL_GetScancodeName(static_cast<SDL_Scancode>(b.code));
        case engine::input_source::mouse_button:
            return "mouse button";
        case engine::input_source::mouse_axis:
            return "mouse / wheel";
        }
        return "?";
    }

    // ---- Actions ------------------------------------------------------------

    /// Declare every action this program has, then bind the defaults.
    ///
    /// **Note that these two things are separable, and that is the whole point.**
    /// The declarations are what the program is about; the bindings are a policy
    /// that a settings screen, a config file or Module 9's serializer could
    /// replace wholesale without any of the code below changing. Today they are
    /// adjacent because there is nowhere else to put them yet.
    void declare_actions()
    {
        a_quit_      = actions_.declare("quit");
        a_spawn_     = actions_.declare("spawn_sparks");
        a_materials_ = actions_.declare("toggle_materials");
        a_orbits_    = actions_.declare("toggle_orbits");
        a_cull_      = actions_.declare("cull_swarm");
        a_rebuild_   = actions_.declare("rebuild_world");
        a_ride_      = actions_.declare("ride_camera");
        a_detach_    = actions_.declare("detach_half");
        a_drift_     = actions_.declare("drift_sun");
        a_rebind_    = actions_.declare("rebind_spawn");
        a_yaw_       = actions_.declare("camera_yaw");
        a_pitch_     = actions_.declare("camera_pitch");
        a_zoom_      = actions_.declare("camera_zoom");
        a_panels_    = actions_.declare("toggle_panels");
        a_links_     = actions_.declare("toggle_hierarchy_links");
        a_axes_      = actions_.declare("toggle_axis_triads");

        actions_.bind_key(a_quit_, SDL_SCANCODE_ESCAPE);
        actions_.bind_key(a_spawn_, SDL_SCANCODE_SPACE);

        // TWO BINDINGS, ONE ACTION, ON TWO DEVICES — which is the property the
        // whole design exists for, demonstrated with hardware this machine has.
        // Hold Space, then also press the left mouse button, then release Space:
        // the action stays active throughout and fires exactly ONE press edge,
        // because edges come from the action's level rather than from any
        // binding's. A switch statement cannot express this at all.
        actions_.bind_mouse_button(a_spawn_, SDL_BUTTON_LEFT);

        actions_.bind_key(a_materials_, SDL_SCANCODE_M);
        actions_.bind_key(a_orbits_, SDL_SCANCODE_O);
        actions_.bind_key(a_cull_, SDL_SCANCODE_X);
        actions_.bind_key(a_rebuild_, SDL_SCANCODE_R);
        actions_.bind_key(a_ride_, SDL_SCANCODE_C);
        actions_.bind_key(a_detach_, SDL_SCANCODE_H);
        actions_.bind_key(a_drift_, SDL_SCANCODE_F);
        actions_.bind_key(a_rebind_, SDL_SCANCODE_K);

        // Lesson 5.11's three. They are actions rather than scancodes for the
        // same reason everything else here is, and there is a second reason now:
        // a key that toggles the debug UI must keep working while the debug UI
        // is on screen, which means it has to go through the same mask as
        // everything else. Bind it outside the action system and it would fire
        // while you were typing its letter into the filter box.
        actions_.bind_key(a_panels_, SDL_SCANCODE_F1);
        actions_.bind_key(a_links_, SDL_SCANCODE_L);
        actions_.bind_key(a_axes_, SDL_SCANCODE_G);

        // AXES FROM PAIRS OF KEYS. Two bindings with opposite scales, and the
        // action's value is their sum — so holding both gives exactly zero and
        // nothing anywhere had to special-case it.
        actions_.bind_key(a_yaw_, SDL_SCANCODE_LEFT, -1.0f);
        actions_.bind_key(a_yaw_, SDL_SCANCODE_RIGHT, +1.0f);
        actions_.bind_key(a_pitch_, SDL_SCANCODE_DOWN, -1.0f);
        actions_.bind_key(a_pitch_, SDL_SCANCODE_UP, +1.0f);

        // …and an axis from a continuous source, through the same mechanism.
        actions_.bind_key(a_zoom_, SDL_SCANCODE_LEFTBRACKET, +1.0f);
        actions_.bind_key(a_zoom_, SDL_SCANCODE_RIGHTBRACKET, -1.0f);
        actions_.bind_mouse_axis(a_zoom_, engine::mouse_axis::wheel_y, -4.0f);
    }

    /// Rebind `spawn_sparks` at runtime, which the old switch could not do.
    ///
    /// Clear then bind, rather than a single `rebind()`, because an action may
    /// legitimately keep several bindings and a one-call version would have to
    /// guess which one is being replaced. Here it deliberately drops the mouse
    /// binding too, so the effect is visible.
    void rebind_spawn()
    {
        spawn_on_enter_ = !spawn_on_enter_;
        actions_.clear_bindings(a_spawn_);
        actions_.bind_key(a_spawn_, spawn_on_enter_ ? SDL_SCANCODE_RETURN
                                                    : SDL_SCANCODE_SPACE);
        if (!spawn_on_enter_) { actions_.bind_mouse_button(a_spawn_, SDL_BUTTON_LEFT); }
    }

    // ---- Building the world ------------------------------------------------

    /// Wipe the world and repopulate it — now with a shape as well as a set of
    /// components. Four kinds of entity became five, and the new one (a moon) is
    /// distinguished from a ring member by nothing except who its parent is.
    void build_world()
    {
        world_.clear();
        swarm_.clear();
        moons_.clear();
        materials_hidden_ = false;
        orbits_frozen_ = false;
        detached_ = false;
        riding_ = false;

        lcg rng{0x5EEDu};

        // ---- the sun: the root of everything -----------------------------
        // ---- the scene's materials, built once ---------------------------
        //
        // LESSON 6.5. Nine materials for the whole scene, and the ring's six are
        // what ninety-six drones point at. Building them here, before any entity
        // exists, is not tidiness: it is what makes the entities' materials a
        // REFERENCE rather than a copy, and it is why changing the drones' finish
        // is now one assignment instead of a loop over the registry.
        ring_palette_.clear();
        for (int i = 0; i < 6; ++i)
        {
            ring_palette_.push_back(materials_.insert(
                engine::material{.tint = tint_for(i), .surface = {.roughness = 0.53f}}));
        }
        moon_material_ = materials_.insert(
            engine::material{.tint = 0xFFF2F4F8u, .surface = {.roughness = 0.40f}});
        spark_material_ = materials_.insert(
            engine::material{.tint = 0xFF7FE0FFu, .surface = {.roughness = 0.38f}});

        sun_ = world_.create();
        engine::ecs::add_hierarchy_components(
            world_, sun_,
            placement{.position = {0.0f, 0.0f, 0.0f},
                      .rotation = engine::mat3::identity(),
                      .scale = {0.9f, 0.9f, 0.9f}});
        world_.add<geometry>(sun_, geometry{.mesh = torus_, .closed = true});
        sun_material_ = materials_.insert(
            engine::material{.tint = 0xFFE8B84Cu,
                             .surface = {.roughness = 0.42f, .metallic = 1.0f}});
        world_.add<material>(sun_, material{.mat = sun_material_});
        world_.add<spin>(sun_, spin{.rate = 0.6f, .wobble = 0.35f});

        // ---- the ring: children of the sun -------------------------------
        //
        // Their orbits are now LOCAL. Spin the sun and the whole ring sweeps with
        // it; drift the sun and the ring goes too. Neither behaviour is
        // implemented anywhere — both are what the composition rule says.
        for (int i = 0; i < k_ring_count; ++i)
        {
            const entity e = world_.create();
            const float band = static_cast<float>(i % 3);

            engine::ecs::add_hierarchy_components(
                world_, e, placement{.scale = engine::vec3{0.16f, 0.16f, 0.16f}});
            world_.add<orbit>(e, orbit{.radius = 2.4f + 0.85f * band,
                                       .speed = 0.75f - 0.16f * band,
                                       .phase = rng.range(0.0f, 6.2831853f),
                                       .tilt = rng.range(-0.55f, 0.55f)});
            world_.add<geometry>(e, geometry{.mesh = (i % 2 == 0) ? cube_ : ico_,
                                             .closed = true});
            // SIX MATERIALS, NINETY-SIX ENTITIES. The palette is built before
            // the loop; the loop only references it.
            world_.add<material>(e, material{.mat = ring_palette_[
                static_cast<std::size_t>(i) % ring_palette_.size()]});
            if (i % 3 == 0)
            {
                world_.add<spin>(e, spin{.rate = rng.range(1.2f, 3.0f),
                                         .wobble = rng.range(-0.9f, 0.9f)});
            }
            engine::ecs::set_parent(world_, e, sun_);
            swarm_.push_back(e);

            // ---- moons: children of a ring member, so DEPTH 3 -------------
            //
            // A moon's world position is computed by nobody. Two composes deep,
            // and the only thing that makes it a moon rather than a planet is
            // which entity its `parent` names.
            if (i % k_moons_every == 0)
            {
                for (int m = 0; m < k_moons_each; ++m)
                {
                    const entity moon = world_.create();
                    // NOTE THE NUMBERS, because they are the hierarchy being
                    // honest. This scale is 0.8 in the PLANET's space, and the
                    // planet is itself 0.16 of the sun's — so a moon comes out
                    // 0.128 in world units, and its local orbit radius of 3.6
                    // becomes 0.58. Scale composes down the chain exactly as
                    // rotation and translation do, which surprises people the
                    // first time a "1-metre" prop under a scaled parent turns out
                    // to be 20 cm. It is not a bug in the composition; it is what
                    // "relative to my parent" means, applied to size.
                    engine::ecs::add_hierarchy_components(
                        world_, moon, placement{.scale = engine::vec3{0.8f, 0.8f, 0.8f}});
                    world_.add<orbit>(moon, orbit{.radius = 3.6f,
                                                  .speed = rng.range(2.5f, 4.5f),
                                                  .phase = rng.range(0.0f, 6.2831853f),
                                                  .tilt = rng.range(-1.3f, 1.3f)});
                    world_.add<geometry>(moon, geometry{.mesh = ico_, .closed = true});
                    world_.add<material>(moon, material{.mat = moon_material_});
                    engine::ecs::set_parent(world_, moon, e);
                    moons_.push_back(moon);
                }
            }
        }

        // ---- waypoints: placement + orbit, and nothing else ---------------
        //
        // Lesson 5.8's argument, still standing: they move every step and are
        // invisible, because the render query asks for a `geometry` they do not
        // have. Nobody wrote an `if`.
        for (int i = 0; i < k_waypoint_count; ++i)
        {
            const entity e = world_.create();
            engine::ecs::add_hierarchy_components(world_, e, placement{});
            world_.add<orbit>(e, orbit{.radius = 1.6f,
                                       .speed = 1.4f,
                                       .phase = rng.range(0.0f, 6.2831853f),
                                       .tilt = rng.range(-1.2f, 1.2f)});
            engine::ecs::set_parent(world_, e, sun_);
        }

        // ---- the camera: an entity, like everything else ------------------
        camera_entity_ = world_.create();
        engine::ecs::add_hierarchy_components(world_, camera_entity_, placement{});
        world_.add<engine::ecs::camera>(camera_entity_, engine::ecs::camera{
                                                            .fovy = 52.0f * 3.14159265f / 180.0f,
                                                            .near_plane = 0.1f,
                                                            .far_plane = 100.0f});
        world_.add<camera_orbit>(camera_entity_, camera_);
        engine::ecs::set_active_camera(world_, camera_entity_);

        rng_ = rng;
        tree_.mark_topology_changed();
        step_world(0.0f);
    }

    /// One simulation step: local transforms first, then one resolve.
    ///
    /// **The order of these five lines is the lesson.** Everything above the
    /// resolve writes LOCAL data and knows nothing about parents; the resolve
    /// turns local into world, once, in an order that guarantees a parent is
    /// finished before its children are started. Nothing below it has to think
    /// about hierarchy at all.
    void step_world(float h)
    {
        if (drifting_)
        {
            // Move the ROOT. Everything under it follows, and no code says so.
            if (placement* p = world_.get<placement>(sun_))
            {
                p->position = {1.6f * std::sin(t_ * 0.35f), 0.5f * std::sin(t_ * 0.5f), 0.0f};
            }
        }

        orbit_system(world_, t_);
        spin_system(world_, h);
        lifetime_system(world_, tree_, h, dead_);
        if (camera_orbit* c = world_.get<camera_orbit>(camera_entity_)) { *c = camera_; }
        camera_orbit_system(world_);

        if (tree_.topology_changed()) { tree_.rebuild(world_); }
        tree_.resolve(world_);
    }

    // ---- The demonstrations -------------------------------------------------

    /// Spawn a burst of short-lived sparks, parented to the sun.
    void spawn_sparks()
    {
        for (int i = 0; i < k_spark_burst; ++i)
        {
            const entity e = world_.create();
            if (!e) { break; }   // id space exhausted; the world's failure, not ours

            engine::ecs::add_hierarchy_components(
                world_, e, placement{.scale = engine::vec3{0.07f, 0.07f, 0.07f}});
            world_.add<orbit>(e, orbit{.radius = rng_.range(1.8f, 4.8f),
                                       .speed = rng_.range(-2.6f, 2.6f),
                                       .phase = rng_.range(0.0f, 6.2831853f),
                                       .tilt = rng_.range(-1.4f, 1.4f)});
            world_.add<geometry>(e, geometry{.mesh = ico_, .closed = true});
            world_.add<material>(e, material{.mat = spark_material_});
            world_.add<lifetime>(e, lifetime{.remaining = rng_.range(0.8f, 2.4f)});
            engine::ecs::set_parent(world_, e, sun_);
        }
        tree_.mark_topology_changed();

        // A MARKER AT THE ORIGIN OF THE BURST, and note where this line is
        // written from: a function called out of `on_fixed_step`, which has no
        // framebuffer, no projector and no camera. Before the queue existed this
        // could not have been written here at any price.
        if (const engine::ecs::world_transform* w =
                world_.get<engine::ecs::world_transform>(sun_))
        {
            debug_.sphere(engine::translation_of(w->matrix), 1.2f,
                          engine::pack_argb(120, 224, 255), marker_seconds_, 20);
        }
    }

    /// Park the camera on a ring member, or take it off again.
    ///
    /// **Two lines of consequence for the whole feature.** `set_parent` links the
    /// camera into the tree; the resolver composes it with everything else; and
    /// `view_from_camera` inverts whatever came out. The renderer is not told, the
    /// camera type does not change, and nothing anywhere holds a pointer to a
    /// "current camera object".
    void toggle_camera_ride()
    {
        riding_ = !riding_;
        if (riding_)
        {
            // A vantage point beside the planet, expressed in the PLANET's space.
            const entity host = swarm_.empty() ? sun_ : swarm_[k_moons_every];
            if (!world_.alive(host)) { riding_ = false; return; }

            world_.remove<camera_orbit>(camera_entity_);
            if (placement* p = world_.get<placement>(camera_entity_))
            {
                *p = engine::ecs::look_along({0.0f, 3.5f, 9.0f}, {0.0f, 0.0f, 0.0f},
                                             {0.0f, 1.0f, 0.0f});
            }
            engine::ecs::set_parent(world_, camera_entity_, host);
        }
        else
        {
            engine::ecs::set_parent(world_, camera_entity_, engine::ecs::null_entity);
            world_.add<camera_orbit>(camera_entity_, camera_);
        }
        tree_.mark_topology_changed();
    }

    /// Detach the outer half of the ring from the sun, or re-attach it.
    ///
    /// Detached entities keep their `orbit`, so they keep circling — but they
    /// circle the WORLD origin instead of the sun, because their placement is now
    /// relative to nothing. Drift the sun with [F] and the split is unmistakable.
    void toggle_detach()
    {
        detached_ = !detached_;
        for (std::size_t i = swarm_.size() / 2; i < swarm_.size(); ++i)
        {
            if (!world_.alive(swarm_[i])) { continue; }
            engine::ecs::set_parent(world_, swarm_[i],
                                    detached_ ? engine::ecs::null_entity : sun_);

            // THE ARGUMENT FOR LIFETIMES, IN ONE FEATURE. Re-parenting happens
            // in a single frame — 16 ms — and the hierarchy lines simply look
            // different afterwards. A box that outlives the event by two seconds
            // is what turns "the tree changed" into "THESE forty-eight changed",
            // and no amount of staring at a per-frame view would tell you that.
            if (const engine::ecs::world_transform* w =
                    world_.get<engine::ecs::world_transform>(swarm_[i]))
            {
                debug_.box(w->matrix, {0.28f, 0.28f, 0.28f},
                           detached_ ? engine::pack_argb(255, 140, 90)
                                     : engine::pack_argb(140, 255, 160),
                           marker_seconds_);
            }
        }
        tree_.mark_topology_changed();
    }

    /// Take the material component away from every third ring member, or give it
    /// back. They disappear and reappear, and no renderer branch was involved.
    void toggle_materials()
    {
        for (std::size_t i = 0; i < swarm_.size(); i += 3)
        {
            const entity e = swarm_[i];

            // `swarm_` may hold ids that [X] destroyed. Nothing needs to prune it:
            // a stale entity fails `alive()` and every pool lookup, which is
            // Lesson 5.4's argument arriving in ordinary code.
            if (!world_.alive(e)) { continue; }

            if (materials_hidden_)
            {
                world_.add<material>(e, material{.mat = ring_palette_[
                    i % ring_palette_.size()]});
            }
            else
            {
                world_.remove<material>(e);
            }
        }
        materials_hidden_ = !materials_hidden_;
    }

    /// Freeze half the ring by removing its orbits. They keep spinning, because
    /// spinning is a different component read by a different system.
    void toggle_orbits()
    {
        for (std::size_t i = swarm_.size() / 2; i < swarm_.size(); ++i)
        {
            const entity e = swarm_[i];
            if (!world_.alive(e)) { continue; }

            if (orbits_frozen_)
            {
                const float band = static_cast<float>(i % 3);
                world_.add<orbit>(e, orbit{.radius = 2.4f + 0.85f * band,
                                           .speed = 0.75f - 0.16f * band,
                                           .phase = static_cast<float>(i) * 0.37f,
                                           .tilt = 0.2f});
            }
            else
            {
                world_.remove<orbit>(e);
            }
        }
        orbits_frozen_ = !orbits_frozen_;
    }

    /// Destroy every fourth ring member — **and its moons with it**.
    ///
    /// `destroy_subtree` is the explicit alternative to the orphan policy. Use
    /// plain `destroy()` here instead and the moons survive as orphans: the
    /// resolver treats them as roots, they keep orbiting the world origin, and
    /// `hierarchy_report::orphans` on the HUD counts them. Both behaviours are
    /// defensible; only one of them should be the silent default, and it is the
    /// one that does not delete entities nobody asked it to.
    void cull_swarm()
    {
        for (std::size_t i = 0; i < swarm_.size(); i += 4)
        {
            engine::ecs::destroy_subtree(world_, swarm_[i]);
        }
        tree_.mark_topology_changed();
    }

    [[nodiscard]] static Uint32 tint_for(int i)
    {
        static constexpr Uint32 k_tints[] = {0xFFE07A3Cu, 0xFF4CB8E0u, 0xFF8FD46Au,
                                             0xFFD46AB8u, 0xFFE0D04Cu, 0xFF9A8FE0u};
        return k_tints[static_cast<std::size_t>(i) % std::size(k_tints)];
    }

    // ---- State -------------------------------------------------------------

    const char* shot_path_ = nullptr;
    float t_ = 0.0f;

    engine::ecs::registry world_;
    engine::ecs::hierarchy tree_;

    /// **Every material in the scene, once each** — Lesson 6.5.
    ///
    /// Six for the ring, one each for the sun, the moons and the sparks. The
    /// entities hold handles into this; nothing holds a copy.
    engine::material_pool materials_;

    /// The ring's six, built once and handed out by `tint_for`'s old index.
    std::vector<engine::material_handle> ring_palette_;

    engine::material_handle sun_material_;
    engine::material_handle moon_material_;
    engine::material_handle spark_material_;

    entity sun_;
    entity camera_entity_;
    std::vector<entity> swarm_;   ///< the ring members, so the keys can find them again
    std::vector<entity> moons_;
    std::vector<entity> dead_;    ///< reused scratch for lifetime_system

    engine::action_map actions_;
    engine::action_id a_quit_, a_spawn_, a_materials_, a_orbits_, a_cull_, a_rebuild_;
    engine::action_id a_ride_, a_detach_, a_drift_, a_rebind_;
    engine::action_id a_yaw_, a_pitch_, a_zoom_;
    engine::action_id a_panels_, a_links_, a_axes_;
    bool spawn_on_enter_ = false;

    // ---- Lesson 5.11 --------------------------------------------------------

    /// The debug UI. Not started under `--shot`, and harmless when it is not.
    engine::debug_ui ui_;

    /// `in()`, with whatever ImGui is currently claiming withheld. Updated once
    /// per frame in `on_input`, immediately before the action map reads it.
    engine::masked_input<engine::input> gate_;

    /// World-space debug geometry, queued by anybody and drawn by the renderer.
    engine::debug_lines debug_;
    std::size_t debug_queued_ = 0;
    int debug_drawn_ = 0;

    bool show_panels_ = true;
    bool show_links_ = true;
    bool show_axes_ = false;
    float marker_seconds_ = 2.0f;

    /// The filter box's buffer. A fixed array because that is what
    /// `ImGui::InputText` takes — ImGui is a C++ library with a C heart, and it
    /// will not allocate on your behalf.
    char filter_[64] = {};

    bool materials_hidden_ = false;
    bool orbits_frozen_ = false;
    bool detached_ = false;
    bool riding_ = false;
    bool drifting_ = false;
    camera_orbit camera_{};
    lcg rng_{0x5EEDu};

    engine::asset_store assets_;
    engine::mesh_handle cube_;
    engine::mesh_handle ico_;
    engine::mesh_handle torus_;
    engine::lighting lights_;

    /// The bridge: rebuilt every frame from the ECS, handed to the renderer.
    std::vector<engine::scene_object> objects_;

    engine::depth_buffer depth_{k_width, k_height};
    std::vector<engine::raster_triangle> triangles_;
    engine::projection_scratch scratch_;
    engine::collect_stats stats_;
};

}   // namespace

ENGINE_MAIN(swarm_app)
