// demos/ecs_swarm/main.cpp — the ECS, doing the one thing a struct cannot.
//
// Lesson 5.8. This program holds 121 entities and draws 97 of them, which is a
// number Lesson 5.6 already told us is far too small for storage layout to
// matter: at this size a `std::vector<scene_object>` and a fully data-oriented
// ECS run within one percent of each other, and 5.6 published the table that says
// so. **So speed is not why this program exists, and pretending otherwise would
// be the exact folklore this course is written against.**
//
// It exists because of what it can express. Look at the components below and then
// at `engine::scene_object` in <engine/gfx/scene.hpp>, which has been accreting
// fields since Lesson 3.1 — a tint, then `closed`, then a whole `specular`
// struct — each one added because *some* object needed it and every object
// therefore got it. Six fields, of which any given object means four. Here the
// same information is six independent components, and the interesting sentences
// are the ones about entities that do NOT have all of them:
//
//   - the WAYPOINTS have a transform and an orbit and no geometry. They move
//     through the scene and are invisible, and no `if (visible)` was written
//     anywhere to arrange that: the render query asks for geometry, and they do
//     not have it. In a `scene_object` world an invisible mover is a null mesh
//     pointer and a branch in the renderer.
//   - the SUN has geometry and spin and no orbit. Same code path, different set.
//   - pressing [M] REMOVES the material component from a third of the swarm and
//     they vanish; pressing it again puts it back. Not a flag flip — the rows are
//     gone from the pool, `component_count()` on the HUD drops, and the render
//     query genuinely has fewer entities to walk.
//   - the SPARKS on [Space] carry a `lifetime` nothing else has, and the system
//     that ages them has never heard of the rest of the world.
//
// That is composition, and it is the argument at a hundred objects. The
// performance argument is Lesson 5.7's and it applies at ten thousand.
//
// WHAT THIS PROGRAM DOES NOT DO is render through the ECS. `collect_triangles`
// still takes a span of `scene_object`, so the render system below walks a view
// and FILLS one — a bridge, written in six lines and marked as temporary, because
// converting the renderer is Module 6's job and doing it here would mean changing
// the picture on the same day as changing the architecture. The reference shot
// this course has been comparing against since Lesson 5.1 is still byte-identical
// after this lesson, and that is only checkable because nothing on its path moved.
//
//     cmake --build build --target ecs_swarm
//     ./build/demos/ecs_swarm                        a window
//     ./build/demos/ecs_swarm --shot swarm.ppm       one frame, no window

#include <engine/asset/asset_store.hpp>
#include <engine/ecs/registry.hpp>
#include <engine/gfx/colour.hpp>
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

#include <engine/platform/main.hpp>

#include <cmath>
#include <vector>

namespace {

using engine::ecs::entity;

// ===========================================================================
//  THE COMPONENTS
// ===========================================================================
//
// Every one of them is a plain struct with no base class, no virtual anything,
// no registration macro and no header of its own. That is not minimalism for its
// own sake — it is Lesson 5.7's rule 1 (components stay small and single-purpose)
// meeting the fact that `registry` needs nothing from a type but that it be
// movable. `engine::transform` is used as a component below without being
// modified, or even recompiled, which is the cleanest possible demonstration of
// the point: an ECS does not ask your data to join anything.

/// Where an entity is. The engine's own `transform` (position, rotation, scale),
/// used unchanged — a component is a struct, and this one was written in Lesson
/// 2.8 by somebody who had never heard of an ECS.
using placement = engine::transform;

/// What shape an entity is, and whether that shape is closed enough to cull.
///
/// `closed` lives here rather than beside the tint because it is a fact about the
/// GEOMETRY — a quad is two-sided whatever colour you paint it — and in
/// `scene_object` it sits three fields away from the mesh it describes.
struct geometry
{
    engine::mesh_handle mesh;
    bool closed = true;
};

/// What an entity looks like: `scene_object`'s `tint` and `surface`, which have
/// always described the same thing and never been the same thing.
struct material
{
    Uint32 tint = 0xFFFFFFFFu;
    engine::specular surface{};
};

/// A circular path around the origin. Position is DERIVED from it every step, so
/// an entity with an orbit does not need its position stored anywhere else —
/// though it happens to have one, because it also has a placement.
struct orbit
{
    float radius = 1.0f;
    float speed = 1.0f;
    float phase = 0.0f;
    float tilt = 0.0f;
};

/// A tumble, integrated over the fixed step.
///
/// Two Euler angles rather than an axis and an angle, because axis-angle
/// rotation does not exist in this engine yet — Module 7 derives it, by way of
/// quaternions, and until then `rotation_y * rotation_x` is what `mat3.hpp`
/// offers. Worth noticing that the component does not care: when Module 7 lands,
/// this struct becomes `{ quat orientation; vec3 angular_velocity; }` and not one
/// line of `registry`, `pool` or `view` changes.
struct spin
{
    float rate = 1.0f;
    float wobble = 0.5f;
    float angle = 0.0f;
};

/// Seconds until this entity destroys itself. Only the sparks have one, and the
/// system that ages them is four lines that mention no other component type.
struct lifetime
{
    float remaining = 1.0f;
};

// ===========================================================================
//  THE SYSTEMS
// ===========================================================================
//
// Free functions taking a `registry&`. Not classes, not virtuals, not a base
// `System` with an `update()` — there is nothing for such a base to hold, because
// a system's entire state is the query it runs. Lesson 5.6 measured what a
// virtual call costs on an iteration (1.5–1.7× at every world size, including
// four objects in L1) and that number is the reason this file has no interfaces
// in it.
//
// Read the queries and you can see the data flow without reading the bodies:
// `orbit_system` writes placements and reads orbits; `render_system` reads
// placement, geometry and material and writes none of them. That is the property
// Module 8's job system will need in order to run two of these at once, and it
// is legible here for free.

/// Put every orbiting entity where its orbit says it should be, at time `t`.
void orbit_system(engine::ecs::registry& world, float t)
{
    world.view<placement, orbit>().each([t](placement& p, const orbit& o) {
        const float a = o.phase + o.speed * t;
        // A circle of radius r in the x–z plane, tilted by `tilt` about the x
        // axis: x is untouched by the tilt (it is the axis), and the y–z pair is
        // the rotation. Multiplying x by cos(tilt) as well would give an ellipse
        // that looks similar and is not a circle, which is the sort of thing this
        // course would rather not have in a listing.
        const float ct = std::cos(o.tilt);
        const float st = std::sin(o.tilt);
        p.position = {o.radius * std::cos(a),
                      o.radius * std::sin(a) * st,
                      o.radius * std::sin(a) * ct};
    });
}

/// Advance every spinning entity by one simulation step.
void spin_system(engine::ecs::registry& world, float h)
{
    world.view<placement, spin>().each([h](placement& p, spin& s) {
        s.angle += s.rate * h;
        p.rotation = engine::rotation_y(s.angle) * engine::rotation_x(s.wobble * s.angle);
    });
}

/// Age the sparks, and destroy the ones that have run out.
///
/// **The two-phase shape here is the one rule a view imposes**, and it is worth
/// having met once. Destroying an entity erases its rows from every pool, which
/// swap-and-pops the very array the walk is stepping through — so the loop
/// collects first and acts afterwards. In a debug build, destroying inside the
/// callback trips the assertion in `view::fetch`; in a release build it silently
/// skips whichever entity was swapped into the hole, which is exactly how erasing
/// from a `std::vector` inside a range-for behaves and is no more forgivable here.
void lifetime_system(engine::ecs::registry& world, float h, std::vector<entity>& scratch)
{
    scratch.clear();
    world.view<lifetime>().each([h, &scratch](entity e, lifetime& l) {
        l.remaining -= h;
        if (l.remaining <= 0.0f) { scratch.push_back(e); }
    });
    for (entity e : scratch) { world.destroy(e); }
}

/// Turn everything visible into the `scene_object` array the renderer still wants.
///
/// **This function is the seam**, and it is deliberately ugly enough to notice.
/// `collect_triangles` takes `std::span<const scene_object>`, so an ECS world has
/// to be flattened into one every frame — a copy of six fields per visible entity,
/// which at 140 objects is about 8 KB and roughly nothing. It stops being nothing
/// at 10,000, and Module 6 removes it by teaching the renderer to read pools
/// directly. Writing that today would mean changing the picture in the same
/// lesson as changing the architecture, and then having no way to tell which
/// change broke it.
void render_system(engine::ecs::registry& world, std::vector<engine::scene_object>& out)
{
    out.clear();
    world.view<placement, geometry, material>().each(
        [&out](const placement& p, const geometry& g, const material& m) {
            out.push_back(engine::scene_object{.xform = p,
                                               .geometry = g.mesh,
                                               .name = "entity",
                                               .tint = m.tint,
                                               .closed = g.closed,
                                               .surface = m.surface});
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

        return {.title = "ecs_swarm — one world, five components",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        // Three meshes, shared by every entity that references them. A component
        // holds a `mesh_handle` — four bytes, Lesson 5.4 — so a hundred and forty
        // entities pointing at one cube cost 140 handles and one cube, and the
        // handle stays answerable if the mesh is ever unloaded.
        cube_ = assets_.insert_mesh("cube",
                                    engine::with_normals(engine::cube_mesh(),
                                                         engine::normal_style::flat));
        ico_ = assets_.insert_mesh("icosahedron",
                                   engine::with_normals(engine::icosahedron_mesh(),
                                                        engine::normal_style::smooth));
        torus_ = assets_.insert_mesh("torus", engine::make_torus(40, 20, 1.0f, 0.34f));

        lights_.key.direction = engine::normalised(engine::vec3{-0.4f, -0.7f, -0.55f});
        lights_.key.colour = {1.0f, 0.97f, 0.90f};
        lights_.key.intensity = 1.0f;

        build_world();

        // A pose that does not depend on the clock, so `--shot` is reproducible.
        if (shot_path_ != nullptr) { t_ = 1.234f; step_world(1.234f); }
        return true;
    }

    /// Discrete presses, handled here rather than in `on_fixed_step`.
    ///
    /// Lesson 1.4's rule, which the course states as EDGES BELONG TO THE FRAME
    /// AND LEVELS BELONG TO THE STEP: `on_fixed_step` runs zero or more times per
    /// frame, so `key_pressed()` inside it would fire twice on a two-step frame
    /// and spawn sixty-four sparks instead of thirty-two. `key_down()` in a step
    /// is fine, because input is frame-coherent and every step of one frame sees
    /// the same snapshot.
    void on_event(const SDL_Event& event) override
    {
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) { return; }

        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_SPACE:  spawn_sparks(); break;
        case SDL_SCANCODE_M:      toggle_materials(); break;
        case SDL_SCANCODE_O:      toggle_orbits(); break;
        case SDL_SCANCODE_X:      cull_swarm(); break;
        case SDL_SCANCODE_R:      build_world(); break;
        default: break;
        }
    }

    void on_fixed_step(float h) override
    {
        if (shot_path_ != nullptr) { return; }
        t_ += h;
        step_world(h);
    }

    void on_frame(float alpha) override
    {
        (void)alpha;

        render_system(world_, objects_);

        fb().clear(k_background);
        depth_.clear();

        const engine::render_options opts{.cull = engine::cull_choice::back};
        engine::collect_triangles(triangles_, scratch_, objects_, assets_.meshes(),
                                  {view_, k_eye}, projector_, lights_, opts, &stats_);

        const engine::fill_style style{.shade = engine::shading::vertex_colour,
                                       .cull = engine::cull_mode::back,
                                       .eye = k_eye};
        engine::draw_triangles(fb(), &depth_, triangles_, false, style);

        if (shot_path_ != nullptr)
        {
            // The same four numbers the HUD shows, plus the two that make the
            // view's choice checkable from a headless run: which pool it decided
            // to lead with, and how many candidates that implies.
            const auto v = world_.view<placement, geometry, material>();
            SDL_Log("ecs_swarm: %zu entities, %zu components, %zu pools, %zu drawn, "
                    "%d triangles",
                    world_.size(), world_.component_count(), world_.pool_count(),
                    objects_.size(), static_cast<int>(triangles_.size()));
            SDL_Log("ecs_swarm: render view leads with pool %zu, %zu candidates",
                    v.lead(), v.size_hint());
            request_quit(engine::save_ppm(fb(), shot_path_));
        }
    }

    void on_overlay() override
    {
        SDL_Renderer* const r = renderer();
        if (r == nullptr) { return; }

        // Which pool the render query decided to lead with, by position in
        // `view<placement, geometry, material>` — 0, 1 or 2. Watch it move when
        // [M] takes the materials away: the material pool becomes the smallest,
        // so it becomes the lead, and the walk gets shorter rather than staying
        // the same length and skipping. That is Lesson 5.7's rule 3 visible on a
        // HUD, which is the only way a design decision ever becomes real.
        const auto v = world_.view<placement, geometry, material>();

        SDL_SetRenderScale(r, 2.0f, 2.0f);
        SDL_SetRenderDrawColor(r, 210, 212, 220, 255);
        SDL_RenderDebugTextFormat(r, 6.0f, 6.0f,
                                  "ECS  entities %3zu   components %4zu   pools %zu",
                                  world_.size(), world_.component_count(),
                                  world_.pool_count());
        SDL_RenderDebugTextFormat(r, 6.0f, 20.0f,
                                  "render view  lead pool %zu   candidates %3zu   drawn %3zu"
                                  "   tris %4d",
                                  v.lead(), v.size_hint(), objects_.size(),
                                  static_cast<int>(triangles_.size()));
        SDL_RenderDebugTextFormat(r, 6.0f, 34.0f,
                                  "slots %zu   free %zu   unresolved %d   fps %5.1f",
                                  world_.entities().slot_count(),
                                  world_.entities().free_count(), stats_.unresolved,
                                  static_cast<double>(time().fps()));
        SDL_RenderDebugText(r, 6.0f, 118.0f,
                            "[Space] sparks  [M] materials  [O] orbits  [X] cull  "
                            "[R] rebuild  [Esc] quit");
        SDL_SetRenderScale(r, 1.0f, 1.0f);
    }

private:
    // ---- Building the world ------------------------------------------------

    /// Wipe the world and repopulate it. Three kinds of entity, and the whole
    /// point is that "kind" is not a thing the registry knows about — it is which
    /// components got attached, and nothing records it anywhere.
    void build_world()
    {
        world_.clear();
        swarm_.clear();
        materials_hidden_ = false;
        orbits_frozen_ = false;

        lcg rng{0x5EEDu};

        // The sun: geometry, material, spin. NO orbit — it does not move, and
        // that is expressed by the absence of a component rather than by a
        // `speed = 0` that every orbit system would still have to process.
        const entity sun = world_.create();
        world_.add<placement>(sun, placement{.position = {0.0f, 0.0f, 0.0f},
                                             .rotation = engine::mat3::identity(),
                                             .scale = {0.9f, 0.9f, 0.9f}});
        world_.add<geometry>(sun, geometry{.mesh = torus_, .closed = true});
        world_.add<material>(sun, material{.tint = 0xFFE8B84Cu,
                                           .surface = {.colour = {0.7f, 0.65f, 0.5f},
                                                       .shininess = 64.0f}});
        world_.add<spin>(sun, spin{.rate = 0.6f, .wobble = 0.35f});

        // The ring: everything, and every third one also spins.
        for (int i = 0; i < k_ring_count; ++i)
        {
            const entity e = world_.create();
            const float band = static_cast<float>(i % 3);

            world_.add<placement>(e, placement{.scale = engine::vec3{0.16f, 0.16f, 0.16f}});
            world_.add<orbit>(e, orbit{.radius = 2.1f + 0.75f * band,
                                       .speed = 0.75f - 0.16f * band,
                                       .phase = rng.range(0.0f, 6.2831853f),
                                       .tilt = rng.range(-0.55f, 0.55f)});
            world_.add<geometry>(e, geometry{.mesh = (i % 2 == 0) ? cube_ : ico_,
                                             .closed = true});
            world_.add<material>(e, material{.tint = tint_for(i),
                                             .surface = {.colour = {0.45f, 0.45f, 0.5f},
                                                         .shininess = 24.0f}});
            if (i % 3 == 0)
            {
                world_.add<spin>(e, spin{.rate = rng.range(1.2f, 3.0f),
                                         .wobble = rng.range(-0.9f, 0.9f)});
            }
            swarm_.push_back(e);
        }

        // The waypoints: placement and orbit, and nothing else. They are real
        // entities, they move every step, and they are invisible — not because
        // anything hid them, but because the render query asks for a geometry
        // they do not have.
        for (int i = 0; i < k_waypoint_count; ++i)
        {
            const entity e = world_.create();
            world_.add<placement>(e, placement{});
            world_.add<orbit>(e, orbit{.radius = 1.35f,
                                       .speed = 1.4f,
                                       .phase = rng.range(0.0f, 6.2831853f),
                                       .tilt = rng.range(-1.2f, 1.2f)});
        }

        rng_ = rng;
    }

    void step_world(float h)
    {
        orbit_system(world_, t_);
        spin_system(world_, h);
        lifetime_system(world_, h, dead_);
    }

    // ---- The four demonstrations -------------------------------------------

    /// Spawn a burst of short-lived sparks near the ring.
    ///
    /// Creating an entity is a free-list pop and one push per component — Lesson
    /// 5.7's measurement was 4.2 ns for the whole structural change, and *the same
    /// 4.2 ns whether the world has four component types or twelve*, which is the
    /// property that decided the storage design. Nothing here reserves capacity,
    /// nothing rebuilds a chunk, and nothing else in the world is touched.
    void spawn_sparks()
    {
        for (int i = 0; i < k_spark_burst; ++i)
        {
            const entity e = world_.create();
            if (!e) { break; }   // id space exhausted; the world's failure, not ours

            world_.add<placement>(e, placement{.scale = engine::vec3{0.07f, 0.07f, 0.07f}});
            world_.add<orbit>(e, orbit{.radius = rng_.range(1.6f, 4.3f),
                                       .speed = rng_.range(-2.6f, 2.6f),
                                       .phase = rng_.range(0.0f, 6.2831853f),
                                       .tilt = rng_.range(-1.4f, 1.4f)});
            world_.add<geometry>(e, geometry{.mesh = ico_, .closed = true});
            world_.add<material>(e, material{.tint = 0xFF7FE0FFu,
                                             .surface = {.colour = {0.9f, 0.9f, 0.9f},
                                                         .shininess = 96.0f}});
            world_.add<lifetime>(e, lifetime{.remaining = rng_.range(0.8f, 2.4f)});
        }
    }

    /// Take the material component away from every third swarm member, or give it
    /// back. They disappear and reappear, and no renderer branch was involved.
    void toggle_materials()
    {
        for (std::size_t i = 0; i < swarm_.size(); i += 3)
        {
            const entity e = swarm_[i];

            // `swarm_` may hold ids that [X] destroyed. Nothing needs to prune it:
            // a stale entity fails `alive()` and every pool lookup, which is
            // Lesson 5.4's whole argument arriving in ordinary code. A vector of
            // pointers here would be a vector of landmines.
            if (!world_.alive(e)) { continue; }

            if (materials_hidden_)
            {
                world_.add<material>(e, material{.tint = tint_for(static_cast<int>(i)),
                                                 .surface = {.colour = {0.45f, 0.45f, 0.5f},
                                                             .shininess = 24.0f}});
            }
            else
            {
                world_.remove<material>(e);
            }
        }
        materials_hidden_ = !materials_hidden_;
    }

    /// Freeze half the swarm by removing its orbits. They keep spinning, because
    /// spinning is a different component and a different system.
    void toggle_orbits()
    {
        for (std::size_t i = swarm_.size() / 2; i < swarm_.size(); ++i)
        {
            const entity e = swarm_[i];
            if (!world_.alive(e)) { continue; }

            if (orbits_frozen_)
            {
                world_.add<orbit>(e, orbit{.radius = 2.1f + 0.75f * static_cast<float>(i % 3),
                                           .speed = 0.75f - 0.16f * static_cast<float>(i % 3),
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

    /// Destroy every fourth swarm member. Watch `slots` hold steady on the HUD
    /// while `free` climbs — and then watch [Space] reuse those slots rather than
    /// minting new ones, which is why the sparse arrays never grow past peak live.
    void cull_swarm()
    {
        for (std::size_t i = 0; i < swarm_.size(); i += 4)
        {
            world_.destroy(swarm_[i]);
        }
    }

    [[nodiscard]] static Uint32 tint_for(int i)
    {
        static constexpr Uint32 k_tints[] = {0xFFE07A3Cu, 0xFF4CB8E0u, 0xFF8FD46Au,
                                             0xFFD46AB8u, 0xFFE0D04Cu, 0xFF9A8FE0u};
        return k_tints[static_cast<std::size_t>(i) % std::size(k_tints)];
    }

    // ---- State -------------------------------------------------------------

    static constexpr engine::vec3 k_eye{4.4f, 3.0f, 5.6f};

    const char* shot_path_ = nullptr;
    float t_ = 0.0f;

    engine::ecs::registry world_;
    std::vector<entity> swarm_;   ///< the ring members, so the keys can find them again
    std::vector<entity> dead_;    ///< reused scratch for lifetime_system
    bool materials_hidden_ = false;
    bool orbits_frozen_ = false;
    lcg rng_{0x5EEDu};

    engine::asset_store assets_;
    engine::mesh_handle cube_;
    engine::mesh_handle ico_;
    engine::mesh_handle torus_;
    engine::lighting lights_;

    /// The bridge: rebuilt every frame from the ECS, handed to the renderer.
    std::vector<engine::scene_object> objects_;

    const engine::mat4 view_ = engine::look_at(k_eye, {0.0f, 0.0f, 0.0f},
                                               {0.0f, 1.0f, 0.0f});
    const engine::projector projector_{
        engine::perspective(52.0f * 3.14159265f / 180.0f,
                            static_cast<float>(k_width) / static_cast<float>(k_height),
                            0.1f, 100.0f),
        engine::viewport{0.0f, 0.0f, static_cast<float>(k_width),
                         static_cast<float>(k_height), 0.0f, 1.0f},
        engine::near_mode::clip};

    engine::depth_buffer depth_{k_width, k_height};
    std::vector<engine::raster_triangle> triangles_;
    engine::projection_scratch scratch_;
    engine::collect_stats stats_;
};

}   // namespace

ENGINE_MAIN(swarm_app)
