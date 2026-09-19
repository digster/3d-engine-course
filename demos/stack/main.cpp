// demos/stack/main.cpp — a pile of crates, and the four knobs that decide
// whether it is a pile.
//
// Lesson 8.10. The `impulse` demo resolved ONE contact and could show you the
// whole of it: a normal arrow, two friction arrows, a cone. This one cannot,
// because its subject is what a hundred contacts do to each other, and the
// interesting quantities are counts and colours rather than vectors.
//
//   THE VIEW is from the side again: world x to the right, world y up. Every
//     scene is planar so nothing is hidden.
//   THE COLOUR OF A CRATE IS ITS ISLAND. Two crates the same colour can reach
//     each other through a chain of contacts and must be solved together; two
//     of different colours cannot influence one another this step, at all, by
//     construction. Press [I] to turn the colouring off and see how little the
//     partition is visible in the geometry.
//   A CRATE THAT IS ASLEEP IS DRAWN HOLLOW AND DIM. Nothing integrates it and
//     none of its contacts are solved. Watch a pile settle and go out.
//   THE BAR ALONG THE BOTTOM is the frame's contact work: broadphase, narrow
//     phase and solve, to scale, against the 16.67 ms budget.
//
// WHAT TO WATCH FOR, IN ORDER.
//
//   [1] THE TOWER. Ten crates. At the shipped eight iterations it leans and
//     falls over about eight seconds in; hold [Right] to twenty and it stands.
//     That is not a bug being fixed, it is §13's headline being demonstrated:
//     one Gauss-Seidel sweep moves information across ONE contact, so a chain
//     of ten needs about twenty sweeps before its bottom knows what its top is
//     doing. Press [W] to turn warm starting off and watch five crates fail.
//   [2] THE YARD. Twelve towers on one floor: twelve colours, because a FIXED
//     body is not a bridge. Press [B] to bridge through fixed bodies the way a
//     first implementation does and the whole yard turns one colour — then
//     press [X] to throw a crate and watch every pile in the level wake up.
//   [3] THE DEEP CRATE. A crate spawned 200 mm inside the floor. [C] cycles the
//     correction: none leaves it there, Baumgarte throws it 99 mm into the air,
//     split impulse lifts it to the surface and stops. The dashed line is the
//     surface.
//   [4] THE WAKE-UP. Four sleeping piles and a crate you throw with [X]. The
//     island the crate reaches wakes on the frame it touches; the others never
//     do, and the counter says so.
//
//   [1]..[4]  scenes         [Space] pause   [.] single step   [R] reset
//   [Left] [Right]  velocity iterations      [C] position correction
//   [W] warm starting        [S] sleeping    [I] island colours
//   [B] bridge fixed bodies (the island bug)  [X] throw a crate
//   [Esc] quit
//
//     cmake --build build --target stack
//     ./build/demos/stack
//     ./build/demos/stack --scene 2 --iters 20 --t 4.0 --shot out.ppm   headless
//
// `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
// `rig`, `plane`, `collector`, `ecs_swarm`, `audio`, `integrate`, `bodies`,
// `spin`, `collide`, `gjk`, `epa`, `manifold`, `broadphase` and `impulse` make.
// It loads nothing and computes everything on screen, so a `--shot` run is
// byte-for-byte reproducible on any machine.
//
// No `engine_use_assets` and no `engine_use_shaders`.

#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/broadphase.hpp>
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
using engine::phys::as_convex;
using engine::phys::body_kind;
using engine::phys::body_world;
using engine::phys::bounds_of;
using engine::phys::box_shape;
using engine::phys::broadphase_config;
using engine::phys::broadphase_pair;
using engine::phys::carry_impulses;
using engine::phys::collide_manifold;
using engine::phys::contact_manifold;
using engine::phys::contact_material;
using engine::phys::contact_pair;
using engine::phys::contact_solver;
using engine::phys::island;
using engine::phys::make_box;
using engine::phys::make_fixed;
using engine::phys::manifold_cache;
using engine::phys::manifold_config;
using engine::phys::pair_key;
using engine::phys::position_correction;
using engine::phys::proxy;
using engine::phys::rigid_body;
using engine::phys::shape;
using engine::phys::sleep_config;
using engine::phys::solver_config;
using engine::phys::solver_stats;
using engine::phys::wake;
using engine::phys::world_obb;

constexpr int k_width = 960;
constexpr int k_height = 540;

/// This demo's own log category, in the range Lesson 5.3 reserved for programs
/// outside the library.
constexpr int log_demo = 70;

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_ground     = engine::pack_argb(52, 58, 72);
constexpr Uint32 k_plain      = engine::pack_argb(150, 190, 235);
constexpr Uint32 k_surface    = engine::pack_argb(120, 230, 150);
constexpr Uint32 k_impulse    = engine::pack_argb(255, 200, 80);
constexpr Uint32 k_bar_broad  = engine::pack_argb(110, 150, 210);
constexpr Uint32 k_bar_narrow = engine::pack_argb(150, 200, 120);
constexpr Uint32 k_bar_solve  = engine::pack_argb(230, 150, 90);
constexpr Uint32 k_bar_back   = engine::pack_argb(36, 40, 50);

constexpr int k_view_x0 = 8;
constexpr int k_view_y0 = 8;
constexpr int k_view_x1 = 631;
constexpr int k_view_y1 = 531;

/// Half the width of the view, in metres, PER SCENE.
///
/// One camera for four scenes wastes most of the frame on three of them: a
/// ten-crate tower is 5 m tall and 0.5 m wide, a yard of twelve is 11 m wide
/// and 1.4 m tall, and a single crate in a floor is 0.5 m of anything. The
/// aspect is fixed by the view rectangle, so what varies is the width and
/// where the ground sits.
struct view_setup
{
    float half_width;   ///< metres, half the visible width
    float ground_px;    ///< pixels from the bottom of the view to y = 0
};

/// **Twelve well-separated hues, and not a gradient.** An island index is a
/// LABEL, not a quantity: island 3 is not "between" islands 2 and 4, and a
/// colour ramp would imply that it was. The one thing this palette has to do is
/// make two adjacent islands obviously different, which a ramp does worst
/// exactly where it matters.
constexpr Uint32 k_island_colours[12] = {
    engine::pack_argb(235, 120, 110), engine::pack_argb(120, 200, 235),
    engine::pack_argb(150, 225, 130), engine::pack_argb(240, 190, 100),
    engine::pack_argb(200, 140, 235), engine::pack_argb(120, 230, 200),
    engine::pack_argb(235, 150, 190), engine::pack_argb(180, 200, 120),
    engine::pack_argb(140, 160, 240), engine::pack_argb(230, 175, 140),
    engine::pack_argb(130, 210, 165), engine::pack_argb(215, 215, 130),
};

enum class scene
{
    tower,
    yard,
    deep,
    wake_up
};

const char* name_of(scene s)
{
    switch (s)
    {
    case scene::tower:   return "tower";
    case scene::yard:    return "yard";
    case scene::deep:    return "deep crate";
    case scene::wake_up: return "wake-up";
    }
    return "?";
}

class stack_app final : public engine::app
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
                cfg_.velocity_iterations = std::clamp(std::atoi(argv[++i]), 1, 64);
            }
            else if (arg == "--no-warm")  { cfg_.warm_start = false; }
            else if (arg == "--no-sleep") { sleep_.enabled = false; }
            else if (arg == "--correction" && i + 1 < argc)
            {
                cfg_.correction =
                    static_cast<position_correction>(std::clamp(std::atoi(argv[++i]), 0, 2));
            }
        }
        return {.title = "stack — sequential impulses, islands and sleeping",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    bool on_start() override
    {
        reset_scene();
        ENGINE_LOG_INFO(log_demo, "stack demo: scene %s, %d iterations, correction %s",
                        name_of(scene_), cfg_.velocity_iterations,
                        engine::phys::name_of(cfg_.correction));
        return ui_.start(window(), renderer()) || shot_path_ != nullptr;
    }

    void on_event(const SDL_Event& event) override
    {
        (void)ui_.handle_event(event);
        if (event.type != SDL_EVENT_KEY_DOWN) { return; }

        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_1: scene_ = scene::tower;   reset_scene(); break;
        case SDL_SCANCODE_2: scene_ = scene::yard;    reset_scene(); break;
        case SDL_SCANCODE_3: scene_ = scene::deep;    reset_scene(); break;
        case SDL_SCANCODE_4: scene_ = scene::wake_up; reset_scene(); break;
        case SDL_SCANCODE_RIGHT:
            cfg_.velocity_iterations = std::min(cfg_.velocity_iterations + 2, 64);
            break;
        case SDL_SCANCODE_LEFT:
            cfg_.velocity_iterations = std::max(cfg_.velocity_iterations - 2, 1);
            break;
        case SDL_SCANCODE_C:
            cfg_.correction =
                static_cast<position_correction>((static_cast<int>(cfg_.correction) + 1) % 3);
            break;
        case SDL_SCANCODE_W: cfg_.warm_start = !cfg_.warm_start; break;
        case SDL_SCANCODE_S: sleep_.enabled = !sleep_.enabled; break;
        case SDL_SCANCODE_I: island_colours_ = !island_colours_; break;
        case SDL_SCANCODE_B: bridge_statics_ = !bridge_statics_; break;
        case SDL_SCANCODE_X: throw_crate(); break;
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
        draw_budget_bar();

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
        const std::uint32_t index = static_cast<std::uint32_t>(world_.size());
        world_.add(b);
        shapes_.push_back(s);
        return index;
    }

    void add_tower(int count, float x, float half = 0.25f, float mass = 10.0f)
    {
        for (int i = 0; i < count; ++i)
        {
            const float y = half + static_cast<float>(i) * (2.0f * half + 0.001f);
            add(make_box(vec3{x, y, 0.0f}, mass, vec3{half, half, half}),
                box_shape(vec3{half, half, half}));
        }
    }

    void reset_scene()
    {
        t_ = 0.0f;
        world_.clear();
        shapes_.clear();
        cache_.clear();
        solver_.clear();
        manifolds_.clear();
        stats_ = solver_stats{};

        // The floor, which is in every scene and in no island.
        add(make_fixed(vec3{0.0f, -0.5f, 0.0f}), box_shape(vec3{20.0f, 0.5f, 20.0f}));

        switch (scene_)
        {
        case scene::tower:
            add_tower(10, 0.0f);
            break;

        case scene::yard:
            for (int i = 0; i < 12; ++i)
            {
                add_tower(4, static_cast<float>(i) * 0.9f - 4.95f, 0.18f, 6.0f);
            }
            break;

        case scene::deep:
            // 200 mm inside the floor, which §7 measured as the shallowest
            // start from which Baumgarte throws the crate clear of it.
            add(make_box(vec3{0.0f, 0.25f - 0.200f, 0.0f}, 10.0f, vec3{0.25f, 0.25f, 0.25f}),
                box_shape(vec3{0.25f, 0.25f, 0.25f}));
            break;

        case scene::wake_up:
            for (int i = 0; i < 4; ++i) { add_tower(3, static_cast<float>(i) * 2.2f - 3.3f); }
            break;
        }
    }

    void throw_crate()
    {
        const vec3 from{-view().half_width + 0.4f, 0.3f, 0.0f};
        const std::uint32_t id = add(make_box(from, 40.0f, vec3{0.25f, 0.25f, 0.25f}),
                                     box_shape(vec3{0.25f, 0.25f, 0.25f}));
        rigid_body& b = world_.bodies()[id];
        b.state.velocity = vec3{18.0f, 1.0f, 0.0f};

        // A body created awake is still a body whose sleep clock starts
        // wherever the default put it. `wake` is one line and this is exactly
        // the case its doc comment is about.
        wake(b);
    }

    // -----------------------------------------------------------------------
    // The step, which is the lesson
    // -----------------------------------------------------------------------

    void advance(float h)
    {
        using clock = std::chrono::steady_clock;

        // 8.9's restitution bias: the velocity gravity is about to add. Set
        // every step rather than once, because the gravity and the step length
        // are both the caller's to change.
        cfg_.restitution_bias = world_.gravity() * h;

        // ---- 1. the velocity half -----------------------------------------
        world_.integrate_velocities(h);

        // ---- 2. collision detection ---------------------------------------
        clock::time_point t0 = clock::now();
        auto bodies = world_.bodies();
        proxies_.clear();
        for (std::size_t i = 0; i < bodies.size(); ++i)
        {
            proxies_.push_back(proxy{bounds_of(shapes_[i], bodies[i].state.position,
                                               bodies[i].orientation),
                                     static_cast<std::uint32_t>(i)});
        }
        grid_.build(proxies_, bp_);
        ms_broad_ = seconds_since(t0) * 1000.0;

        t0 = clock::now();
        cache_.begin_frame();
        manifolds_.clear();
        keys_.clear();
        pair_a_.clear();
        pair_b_.clear();

        // Reserved before anything is pushed: `contact_solver::add` keeps a
        // POINTER into this vector, and a reallocation half way through the
        // loop would hand the solver freed memory.
        manifolds_.reserve(grid_.pairs().size());

        for (const broadphase_pair& p : grid_.pairs())
        {
            const rigid_body& a = bodies[p.a];
            const rigid_body& b = bodies[p.b];
            if (a.kind != body_kind::dynamic && b.kind != body_kind::dynamic) { continue; }

            contact_manifold m = collide_pair(p.a, p.b);
            if (m.count == 0) { continue; }

            const std::uint64_t key = pair_key(p.a, p.b);
            if (const contact_manifold* previous = cache_.find(key)) { carry_impulses(m, *previous); }

            manifolds_.push_back(m);
            keys_.push_back(key);
            pair_a_.push_back(p.a);
            pair_b_.push_back(p.b);
        }
        ms_narrow_ = seconds_since(t0) * 1000.0;

        // ---- 3. the solve --------------------------------------------------
        t0 = clock::now();
        solver_.begin(world_.bodies());
        for (std::size_t i = 0; i < manifolds_.size(); ++i)
        {
            // [B] turns every dynamic body's partner into a bridge by lying to
            // the solver about which bodies a contact joins: both indices are
            // reported as the DYNAMIC one's, so a crate standing on the floor
            // is joined to... itself, and then the floor's other neighbours
            // through the crates that share it. It is the island bug made
            // switchable, and it exists so that [2] can show what it looks
            // like rather than describing it.
            solver_.add(pair_a_[i], pair_b_[i], manifolds_[i], material_);
        }
        stats_ = solver_.solve(h, cfg_, sleep_);
        ms_solve_ = seconds_since(t0) * 1000.0;

        for (std::size_t i = 0; i < manifolds_.size(); ++i) { cache_.store(keys_[i], manifolds_[i]); }
        cache_.end_frame();

        // ---- 4. the position half ------------------------------------------
        world_.integrate_positions(h);

        // The island labels, copied out because the solver's span is only valid
        // until the next `begin`.
        labels_.assign(solver_.island_of().begin(), solver_.island_of().end());
        if (bridge_statics_) { relabel_bridging(); }
    }

    [[nodiscard]] contact_manifold collide_pair(std::uint32_t ia, std::uint32_t ib) const
    {
        auto bodies = world_.bodies();
        const auto x = world_obb(shapes_[ia], bodies[ia].state.position, bodies[ia].orientation);
        const auto y = world_obb(shapes_[ib], bodies[ib].state.position, bodies[ib].orientation);
        return collide_manifold(as_convex(x), as_convex(y), mf_);
    }

    /// The island bug, for [B]: a union-find in which a FIXED body IS a bridge.
    ///
    /// Only the labels change — the solve above is unaffected, because letting
    /// the bug into the solver would change what the demo is showing from "the
    /// partition is wrong" to "the physics is different". What the colours then
    /// show is the partition a first implementation computes, on a scene whose
    /// physics is correct.
    void relabel_bridging()
    {
        const int n = static_cast<int>(world_.size());
        std::vector<int> parent(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) { parent[static_cast<std::size_t>(i)] = i; }
        const auto find = [&parent](int x) {
            while (parent[static_cast<std::size_t>(x)] != x)
            {
                parent[static_cast<std::size_t>(x)] =
                    parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(x)])];
                x = parent[static_cast<std::size_t>(x)];
            }
            return x;
        };
        for (std::size_t i = 0; i < manifolds_.size(); ++i)
        {
            const int ra = find(static_cast<int>(pair_a_[i]));
            const int rb = find(static_cast<int>(pair_b_[i]));
            if (ra != rb) { parent[static_cast<std::size_t>(std::max(ra, rb))] = std::min(ra, rb); }
        }
        std::vector<int> label(static_cast<std::size_t>(n), -1);
        int count = 0;
        for (int i = 0; i < n; ++i)
        {
            if (world_.bodies()[static_cast<std::size_t>(i)].kind != body_kind::dynamic)
            {
                labels_[static_cast<std::size_t>(i)] = -1;
                continue;
            }
            const int r = find(i);
            if (label[static_cast<std::size_t>(r)] < 0) { label[static_cast<std::size_t>(r)] = count++; }
            labels_[static_cast<std::size_t>(i)] = label[static_cast<std::size_t>(r)];
        }
        bridged_islands_ = count;
    }

    [[nodiscard]] static double seconds_since(std::chrono::steady_clock::time_point t0)
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }

    // -----------------------------------------------------------------------
    // Drawing
    // -----------------------------------------------------------------------

    [[nodiscard]] view_setup view() const
    {
        switch (scene_)
        {
        case scene::tower:   return {4.6f, 80.0f};
        case scene::yard:    return {6.0f, 130.0f};
        case scene::deep:    return {1.1f, 220.0f};
        case scene::wake_up: return {5.0f, 140.0f};
        }
        return {4.0f, 120.0f};
    }

    [[nodiscard]] int sx(float x) const
    {
        const float half = view().half_width;
        const float span = static_cast<float>(k_view_x1 - k_view_x0);
        return k_view_x0 + static_cast<int>((x + half) / (2.0f * half) * span);
    }

    [[nodiscard]] int sy(float y) const
    {
        const float half = view().half_width;
        const float span = static_cast<float>(k_view_x1 - k_view_x0);
        const float per_metre = span / (2.0f * half);
        return k_view_y1 - static_cast<int>(view().ground_px) - static_cast<int>(y * per_metre);
    }

    /// Liang-Barsky against the view rectangle, so that a crate half off the
    /// left edge is clipped rather than drawn across the panel.
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

    /// Darken a packed colour toward the background, keeping its hue.
    ///
    /// A sleeping crate has to say TWO things at once — which island it is in,
    /// and that it is out of the simulation — and the first draft said only
    /// the second, by painting every sleeping body the same grey. A settled
    /// scene then lost its whole partition the moment it settled, which is
    /// exactly when a reader most wants to see it.
    [[nodiscard]] static Uint32 dim(Uint32 colour, float factor)
    {
        const auto ch = [colour, factor](int shift) {
            const int v = static_cast<int>((colour >> shift) & 0xffu);
            return static_cast<Uint32>(static_cast<int>(static_cast<float>(v) * factor));
        };
        return engine::pack_argb(static_cast<int>(ch(16)), static_cast<int>(ch(8)),
                                 static_cast<int>(ch(0)));
    }

    [[nodiscard]] Uint32 colour_of(std::size_t index) const
    {
        const rigid_body& b = world_.bodies()[index];
        const int isl = index < labels_.size() ? labels_[index] : -1;
        const Uint32 base = (island_colours_ && isl >= 0)
                                ? k_island_colours[static_cast<std::size_t>(isl) % 12]
                                : k_plain;
        return b.sleeping ? dim(base, 0.32f) : base;
    }

    void draw_world()
    {
        auto bodies = world_.bodies();

        // The floor's top surface, and — in the deep-crate scene — a dashed
        // line along it, because the whole point of that scene is where the
        // crate ends up relative to this one line.
        line(vec3{-40.0f, 0.0f, 0.0f}, vec3{40.0f, 0.0f, 0.0f}, k_ground);
        line(vec3{-40.0f, -1.0f, 0.0f}, vec3{40.0f, -1.0f, 0.0f}, k_ground);
        if (scene_ == scene::deep)
        {
            const float half = view().half_width;
            for (float x = -half; x < half; x += 0.06f * half)
            {
                line(vec3{x, 0.0f, 0.0f}, vec3{x + 0.03f * half, 0.0f, 0.0f}, k_surface);
            }
        }

        for (std::size_t i = 0; i < bodies.size(); ++i)
        {
            if (bodies[i].kind == body_kind::fixed) { continue; }
            draw_box(bodies[i], shapes_[i], colour_of(i));
        }

        draw_contacts();
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

    /// One tick per contact point, its length the accumulated normal impulse.
    ///
    /// Not an arrow per point: on a settled yard there are four hundred of them
    /// and arrows would be a hedge. A tick along the normal, to one scale, is
    /// enough to see the bottom of a tower carrying more than the top — which
    /// is the one thing about the impulses that a stack makes visible.
    void draw_contacts()
    {
        constexpr float k_per_newton_second = 0.010f;
        for (const contact_manifold& m : manifolds_)
        {
            for (int i = 0; i < m.count; ++i)
            {
                const vec3 p = m.points[i].position;
                const float len = std::min(0.45f, m.points[i].normal_impulse * k_per_newton_second);
                if (len <= 0.0f) { continue; }
                line(p, p + m.normal * len, k_impulse);
            }
        }
    }

    /// Broadphase, narrow phase and solve, to scale against 16.67 ms.
    void draw_budget_bar()
    {
        engine::framebuffer& f = fb();
        constexpr int y = k_view_y1 - 34;
        constexpr int height = 14;
        constexpr int x0 = k_view_x0 + 8;
        constexpr int x1 = k_view_x1 - 8;
        constexpr double budget_ms = 1000.0 / 60.0;

        f.fill_rect(x0, y, x1 - x0, height, k_bar_back);

        const double span = static_cast<double>(x1 - x0);
        int cursor = x0;
        const struct { double ms; Uint32 colour; } parts[3] = {
            {ms_broad_, k_bar_broad}, {ms_narrow_, k_bar_narrow}, {ms_solve_, k_bar_solve}};
        for (const auto& part : parts)
        {
            const int w = static_cast<int>(part.ms / budget_ms * span);
            if (w <= 0) { continue; }
            f.fill_rect(cursor, y, std::min(w, x1 - cursor), height, part.colour);
            cursor += w;
            if (cursor >= x1) { break; }
        }
    }

    // -----------------------------------------------------------------------
    // Panel
    // -----------------------------------------------------------------------

    void panel()
    {
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(k_view_x1 + 16), 8.0f),
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.0f, 520.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("stack");

        ImGui::Text("scene: %s   t = %.2f s", name_of(scene_), static_cast<double>(t_));
        ImGui::Separator();

        ImGui::Text("velocity iterations  %d", cfg_.velocity_iterations);
        ImGui::Text("position iterations  %d", cfg_.position_iterations);
        ImGui::Text("correction           %s", engine::phys::name_of(cfg_.correction));
        ImGui::Text("warm starting        %s", cfg_.warm_start ? "on" : "OFF");
        ImGui::Text("sleeping             %s", sleep_.enabled ? "on" : "OFF");
        ImGui::Separator();

        ImGui::Text("islands              %d%s", bridge_statics_ ? bridged_islands_ : stats_.islands,
                    bridge_statics_ ? "   [B] fixed bodies bridge" : "");
        ImGui::Text("sleeping islands     %d", stats_.sleeping_islands);
        ImGui::Text("sleeping bodies      %d of %d", stats_.sleeping_bodies,
                    static_cast<int>(world_.size()) - 1);
        ImGui::Separator();

        ImGui::Text("manifolds            %d  (%d solved)", stats_.manifolds,
                    stats_.solved_manifolds);
        ImGui::Text("contact points       %d  (%d warm)", stats_.points, stats_.warm_points);
        ImGui::Text("max residual         %.3e m/s", static_cast<double>(stats_.max_residual));
        ImGui::Text("max penetration      %.3f mm",
                    static_cast<double>(stats_.max_penetration * 1000.0f));
        ImGui::Separator();

        ImGui::Text("broadphase   %6.3f ms", ms_broad_);
        ImGui::Text("narrow phase %6.3f ms", ms_narrow_);
        ImGui::Text("solve        %6.3f ms", ms_solve_);
        ImGui::Text("total        %6.3f ms of 16.67", ms_broad_ + ms_narrow_ + ms_solve_);
        ImGui::Separator();

        ImGui::TextWrapped("[1]..[4] scene   [Left]/[Right] iterations   [C] correction");
        ImGui::TextWrapped("[W] warm start   [S] sleeping   [I] island colours");
        ImGui::TextWrapped("[B] bridge fixed bodies   [X] throw a crate");
        ImGui::TextWrapped("[Space] pause   [.] step   [R] reset   [Esc] quit");

        ImGui::End();
    }

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------

    engine::debug_ui ui_{};

    body_world world_{};
    std::vector<shape> shapes_{};
    engine::phys::uniform_grid grid_{};
    manifold_cache cache_{};
    contact_solver solver_{};

    solver_config cfg_{};
    sleep_config sleep_{};
    broadphase_config bp_{};
    manifold_config mf_{};
    contact_material material_{0.0f, 0.5f};

    std::vector<proxy> proxies_{};
    std::vector<contact_manifold> manifolds_{};
    std::vector<std::uint64_t> keys_{};
    std::vector<std::uint32_t> pair_a_{};
    std::vector<std::uint32_t> pair_b_{};
    std::vector<int> labels_{};

    solver_stats stats_{};
    int bridged_islands_ = 0;
    double ms_broad_ = 0.0;
    double ms_narrow_ = 0.0;
    double ms_solve_ = 0.0;

    scene scene_ = scene::tower;
    bool island_colours_ = true;
    bool bridge_statics_ = false;
    bool running_ = true;
    bool single_step_ = false;
    float t_ = 0.0f;

    const char* shot_path_ = nullptr;
    float shot_at_ = 2.0f;
};

}  // namespace

ENGINE_MAIN(stack_app)
