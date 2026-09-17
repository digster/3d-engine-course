// demos/gjk/main.cpp — two sets become one set, and a pair becomes a point.
//
// Lesson 8.5. GJK is short and its correctness argument is not hard, but the
// MOVE it rests on is very hard to believe from a statement: two shapes are
// replaced by their Minkowski difference, and "are these two touching?" becomes
// "does that one set contain the origin?". This demo exists to make that
// substitution watchable.
//
//   LEFT PANEL   the two shapes in the world, wireframe, orthographic. When
//                they are apart the two WITNESS POINTS are drawn — one on each
//                surface — with the segment between them, which is the distance
//                GJK reports. Nothing here is new; it is the world you already
//                believe in.
//   RIGHT PANEL  the same pair as ONE set: `A ⊖ B`, outlined by tracing its own
//                support function (see `draw_silhouette` — a convex set's shadow
//                is convex, and its support function is the original's composed
//                with the transpose of the projection). The origin is the cross.
//                The whole question is whether the cross is inside the SET, and
//                the SIMPLEX — one to four of its vertices, with the current `v`
//                drawn from the origin — is the search for an answer.
//
//                The outline is a SHADOW, so the cross being inside it proves
//                nothing: that is 8.4 §3's whole point about projections, and it
//                is why the panel shows a search rather than a picture.
//
// WHAT TO WATCH FOR. Press [S] repeatedly and watch the simplex grow and `v`
// shrink; the number in the panel is the current upper bound on the distance,
// and it only ever goes down — that monotonicity is a theorem, and `gjk.cpp`
// leans on it twice. Then press [3] — two boxes standing on the same floor at
// different yaws — and step the search. **The simplex stays in one plane.**
// Both boxes share an up axis, so every support point the search asks for comes
// back with `y` exactly zero, and a search confined to a plane can never build a
// tetrahedron. Tilt one box with [Z] and the simplex lifts out.
//
//   [1] [2] [3]   presets: two crates, a capsule and a hull, two on a floor
//   [S]           one GJK iteration                [R] restart the search
//   [A]           run to completion each frame (the default)
//   [Left/Right]  move the second shape along x    [Up/Down]  along y
//   [,] [.]       along z
//   [Q] [E]       yaw it                           [Z] [C]    tilt it
//   [Space]       pause the drift                  [0] reset      [Esc] quit
//
//     cmake --build build --target gjk
//     ./build/demos/gjk
//     ./build/demos/gjk --preset 2 --t 1.5 --shot out.ppm     headless
//
// IT DRIVES THE LOOP ITSELF rather than calling `gjk_distance`, and that is the
// one place this demo reaches past the obvious API — into `cso_support` and
// `reduce_simplex`, which `gjk.hpp` exposes for exactly this reason. A single-
// stepping visualiser needs the iteration, not the answer. It still includes
// only public headers; 5.1's boundary is intact.

#include <engine/core/fixed_step.hpp>
#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/gjk.hpp>
#include <engine/phys/shape.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>
#include <engine/ui/debug_ui.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace {

using engine::quat;
using engine::vec3;
using engine::phys::as_convex;
using engine::phys::box_shape;
using engine::phys::capsule;
using engine::phys::capsule_shape;
using engine::phys::convex;
using engine::phys::cso_support;
using engine::phys::gjk_vertex;
using engine::phys::hull;
using engine::phys::obb;
using engine::phys::reduce_simplex;
using engine::phys::simplex;
using engine::phys::world_capsule;
using engine::phys::world_hull;
using engine::phys::world_obb;

constexpr int k_width = 960;
constexpr int k_height = 540;
constexpr float k_pi = std::numbers::pi_v<float>;

/// This demo's own log category, in the range Lesson 5.3 reserved for programs
/// outside the library.
constexpr int log_demo = 64;

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_grid       = engine::pack_argb(38, 42, 52);
constexpr Uint32 k_axis       = engine::pack_argb(84, 92, 108);

constexpr Uint32 k_shape_a    = engine::pack_argb(150, 160, 255);
constexpr Uint32 k_shape_b    = engine::pack_argb(120, 220, 160);
constexpr Uint32 k_shape_hit  = engine::pack_argb(235, 96, 96);
constexpr Uint32 k_witness    = engine::pack_argb(235, 200, 96);
constexpr Uint32 k_cloud      = engine::pack_argb(70, 78, 96);
constexpr Uint32 k_simplex    = engine::pack_argb(235, 200, 96);
constexpr Uint32 k_vline      = engine::pack_argb(235, 120, 200);
constexpr Uint32 k_origin     = engine::pack_argb(230, 236, 248);

constexpr int k_left_x0  = 8;
constexpr int k_left_y0  = 8;
constexpr int k_left_x1  = 470;
constexpr int k_left_y1  = 531;
constexpr int k_right_x0 = 482;
constexpr int k_right_y0 = 8;
constexpr int k_right_x1 = 951;
constexpr int k_right_y1 = 531;

/// Metres across the left panel. The right panel spans twice as much, because a
/// Minkowski difference is as wide as BOTH shapes put together.
constexpr float k_left_span = 9.0f;
constexpr float k_right_span = 13.0f;

/// `engine::draw_line` deliberately does not clip — Lesson 2.1 argues that a
/// rasterizer's job is not to guess what the caller meant — so a demo that draws
/// outside its panel clips its own lines. Cohen-Sutherland would be the general
/// answer; two panels and straight edges need only the trivial accept case plus
/// a midpoint bisection, which is eight iterations and no case analysis.
void line_in(engine::framebuffer& f, int x0, int y0, int x1, int y1,
             int cx0, int cy0, int cx1, int cy1, Uint32 colour)
{
    auto inside = [&](int x, int y) { return x >= cx0 && x <= cx1 && y >= cy0 && y <= cy1; };
    if (inside(x0, y0) && inside(x1, y1))
    {
        engine::draw_line(f, x0, y0, x1, y1, colour);
        return;
    }
    // Both ends outside on the same side: nothing to draw.
    if ((x0 < cx0 && x1 < cx0) || (x0 > cx1 && x1 > cx1) ||
        (y0 < cy0 && y1 < cy0) || (y0 > cy1 && y1 > cy1))
    {
        return;
    }
    // Otherwise clamp the endpoints. Crude — a line that only clips a corner
    // will bend — and correct enough for a debug view whose shapes are sized to
    // fit. Saying so is the point; a silent approximation is the bad kind.
    const int ax = std::clamp(x0, cx0, cx1);
    const int ay = std::clamp(y0, cy0, cy1);
    const int bx = std::clamp(x1, cx0, cx1);
    const int by = std::clamp(y1, cy0, cy1);
    engine::draw_line(f, ax, ay, bx, by, colour);
}

void frame_box(engine::framebuffer& f, int x0, int y0, int x1, int y1)
{
    engine::draw_line(f, x0, y0, x1, y0, k_axis);
    engine::draw_line(f, x0, y1, x1, y1, k_axis);
    engine::draw_line(f, x0, y0, x0, y1, k_axis);
    engine::draw_line(f, x1, y0, x1, y1, k_axis);
}

void dot(engine::framebuffer& f, int x, int y, int r, Uint32 colour)
{
    for (int dy = -r; dy <= r; ++dy)
    {
        for (int dx = -r; dx <= r; ++dx)
        {
            if (dx * dx + dy * dy <= r * r) { f.put_pixel(x + dx, y + dy, colour); }
        }
    }
}

/// The twelve edges of a box, by corner index.
///
/// `obb::corners` orders them so that bit `i` set means the `+` side of body
/// axis `i`, so an edge joins two corners whose indices differ in exactly one
/// bit — which is what this table is.
constexpr int k_edges[12][2] = {
    {0, 1}, {2, 3}, {4, 5}, {6, 7},
    {0, 2}, {1, 3}, {4, 6}, {5, 7},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

/// Directions spread over the sphere, for building a hull's vertices.
///
/// A Fibonacci lattice rather than a latitude/longitude grid, because the latter
/// piles points at the poles and leaves the equator bare.
std::vector<vec3> sphere_directions(int n)
{
    std::vector<vec3> out;
    out.reserve(static_cast<std::size_t>(n));
    const float golden = k_pi * (3.0f - std::sqrt(5.0f));
    for (int i = 0; i < n; ++i)
    {
        const float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
        const float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float a = golden * static_cast<float>(i);
        out.push_back(vec3{std::cos(a) * r, y, std::sin(a) * r});
    }
    return out;
}

// ---- the orthographic projection, and its transpose -------------------------
//
// `P(p) = (p.x + c*p.z, p.y + s*p.z)`: a tilted look down −z, so all three axes
// are distinguishable.
constexpr float k_tilt = 0.42f;
const float k_proj_c = 0.55f * std::cos(k_tilt);
const float k_proj_s = 0.55f * std::sin(k_tilt);

/// **A convex set's SHADOW is convex, and its support function is the original's
/// composed with the transpose of the projection.**
///
/// That one line is why this demo can outline a shape it knows nothing about.
/// The support of `P(X)` in a screen direction `d` is `max over x of dot(P(x),
/// d)`, and `dot(P(x), d) == dot(x, Pᵀd)` — so asking the 3D support function
/// for `Pᵀd` and projecting the answer lands exactly on the silhouette. Sweep
/// `d` around the circle and the silhouette is traced out, for a box, a capsule,
/// a hull or a Minkowski difference, with one loop and no case analysis.
///
/// Drawing the SUPPORT POINTS instead — the obvious first attempt, and this
/// demo's — draws almost nothing for a polytope: nine hundred directions on a
/// box land on eight corners.
[[nodiscard]] vec3 screen_direction(float angle)
{
    const float dx = std::cos(angle);
    const float dy = std::sin(angle);
    return vec3{dx, dy, k_proj_c * dx + k_proj_s * dy};
}

/// Trace a convex set's outline. `sup` returns a world point, `proj` projects it.
template <typename Support, typename Project>
void draw_silhouette(engine::framebuffer& f, Support sup, Project proj,
                     int cx0, int cy0, int cx1, int cy1, Uint32 colour)
{
    constexpr int k_samples = 240;
    int px = 0;
    int py = 0;
    for (int i = 0; i <= k_samples; ++i)
    {
        const float a = 2.0f * k_pi * static_cast<float>(i) / static_cast<float>(k_samples);
        int x = 0;
        int y = 0;
        proj(sup(screen_direction(a)), x, y);
        if (i > 0) { line_in(f, px, py, x, y, cx0, cy0, cx1, cy1, colour); }
        px = x;
        py = y;
    }
}

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

class gjk_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char* argv[]) override
    {
        for (int i = 1; i < argc; ++i)
        {
            if (SDL_strcmp(argv[i], "--shot") == 0 && i + 1 < argc)
            {
                shot_path_ = argv[++i];
            }
            else if (SDL_strcmp(argv[i], "--t") == 0 && i + 1 < argc)
            {
                const double wanted = SDL_atof(argv[++i]);
                shot_at_ = static_cast<float>(std::max(0.0, wanted));
            }
            else if (SDL_strcmp(argv[i], "--preset") == 0 && i + 1 < argc)
            {
                const int wanted = SDL_atoi(argv[++i]);
                preset_ = std::clamp(wanted, 0, 2);
            }
            else if (SDL_strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
            {
                const int wanted = SDL_atoi(argv[++i]);
                manual_steps_ = std::clamp(wanted, 0, 32);
                auto_run_ = false;
            }
            else if (SDL_strcmp(argv[i], "--still") == 0)
            {
                drifting_ = false;
            }
        }

        return {.title = "gjk — the Minkowski difference, searched",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        SDL_SetLogPriority(log_demo, SDL_LOG_PRIORITY_INFO);
        reset();
        ENGINE_LOG_INFO(log_demo, "[1][2][3] preset  [S] step  [R] restart  [A] auto");
        ENGINE_LOG_INFO(log_demo, "arrows/,/. move  [Q][E] yaw  [Z][C] tilt  [Space] pause");
        return ui_.start(window(), renderer()) || shot_path_ != nullptr;
    }

    void on_event(const SDL_Event& event) override
    {
        (void)ui_.handle_event(event);
        if (event.type != SDL_EVENT_KEY_DOWN) { return; }

        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_1: preset_ = 0; reset(); break;
        case SDL_SCANCODE_2: preset_ = 1; reset(); break;
        case SDL_SCANCODE_3: preset_ = 2; reset(); break;
        case SDL_SCANCODE_S: auto_run_ = false; ++manual_steps_; break;
        case SDL_SCANCODE_R: manual_steps_ = 0; break;
        case SDL_SCANCODE_A: auto_run_ = true; break;
        case SDL_SCANCODE_SPACE: drifting_ = !drifting_; break;
        case SDL_SCANCODE_0: reset(); break;

        case SDL_SCANCODE_LEFT:   nudge(vec3{-0.08f, 0.0f, 0.0f}); break;
        case SDL_SCANCODE_RIGHT:  nudge(vec3{+0.08f, 0.0f, 0.0f}); break;
        case SDL_SCANCODE_DOWN:   nudge(vec3{0.0f, -0.08f, 0.0f}); break;
        case SDL_SCANCODE_UP:     nudge(vec3{0.0f, +0.08f, 0.0f}); break;
        case SDL_SCANCODE_COMMA:  nudge(vec3{0.0f, 0.0f, -0.08f}); break;
        case SDL_SCANCODE_PERIOD: nudge(vec3{0.0f, 0.0f, +0.08f}); break;

        case SDL_SCANCODE_Q: spin(vec3{0.0f, 1.0f, 0.0f}, +0.06f); break;
        case SDL_SCANCODE_E: spin(vec3{0.0f, 1.0f, 0.0f}, -0.06f); break;
        case SDL_SCANCODE_Z: spin(vec3{1.0f, 0.0f, 0.0f}, +0.06f); break;
        case SDL_SCANCODE_C: spin(vec3{1.0f, 0.0f, 0.0f}, -0.06f); break;
        default: break;
        }
    }

    void on_input() override { ui_.begin_frame(); }

    void on_fixed_step(float h) override
    {
        if (!drifting_) { return; }
        t_ += h;
        offset_ = amplitude_ * std::sin(t_ * 0.5f);
    }

    void on_frame(float alpha) override
    {
        (void)alpha;
        draw();
        if (shot_path_ != nullptr && t_ >= shot_at_)
        {
            request_quit(engine::save_ppm(fb(), shot_path_));
        }
    }

    void on_overlay() override
    {
        if (ui_.running()) { build_panel(); }
        ui_.render();
    }

    void on_stop() override { ui_.stop(); }

private:
    void nudge(vec3 d)
    {
        drifting_ = false;
        manual_ = manual_ + d;
    }

    void spin(vec3 axis, float radians)
    {
        drifting_ = false;
        orient_b_ = engine::quat_from_axis_angle(normalised(axis), radians) * orient_b_;
    }

    void reset()
    {
        t_ = 0.0f;
        offset_ = 0.0f;
        manual_ = vec3{};
        drifting_ = true;
        manual_steps_ = 0;

        // The hull's vertices live in the app, not in the `hull` view, which is
        // non-owning. `world_hull` takes a span and keeps it; letting this
        // vector reallocate under a live `hull` would be the dangling-view bug
        // `convex.hpp` deletes its rvalue overloads to prevent.
        hull_points_.clear();
        for (const vec3& d : sphere_directions(14)) { hull_points_.push_back(d * 0.9f); }

        switch (preset_)
        {
        case 0:   // two crates, one drifting through the other
            kind_a_ = kind::box;
            kind_b_ = kind::box;
            half_a_ = vec3{1.0f, 0.8f, 0.9f};
            half_b_ = vec3{0.7f, 1.0f, 0.6f};
            centre_a_ = vec3{-0.8f, 0.0f, 0.0f};
            base_b_ = vec3{2.4f, 0.4f, 0.3f};
            drift_ = normalised(vec3{-1.0f, -0.1f, 0.0f});
            amplitude_ = 2.0f;
            orient_a_ = quat::identity();
            orient_b_ = engine::quat_from_axis_angle(normalised(vec3{0.2f, 1.0f, 0.3f}), 0.6f);
            break;

        case 1:   // a capsule against a convex hull: neither is SAT-testable
            kind_a_ = kind::capsule;
            kind_b_ = kind::hull;
            centre_a_ = vec3{-1.2f, 0.0f, 0.0f};
            base_b_ = vec3{2.2f, 0.3f, 0.2f};
            drift_ = normalised(vec3{-1.0f, 0.0f, 0.0f});
            amplitude_ = 1.8f;
            orient_a_ = engine::quat_from_axis_angle(normalised(vec3{0.0f, 0.0f, 1.0f}), 0.5f);
            orient_b_ = engine::quat_from_axis_angle(normalised(vec3{1.0f, 1.0f, 0.0f}), 0.9f);
            break;

        default:  // THE DEGENERATE ONE: two boxes standing on the same floor
            kind_a_ = kind::box;
            kind_b_ = kind::box;
            half_a_ = vec3{1.0f, 1.0f, 1.0f};
            half_b_ = vec3{1.0f, 1.0f, 1.0f};
            centre_a_ = vec3{-1.3f, 0.0f, 0.0f};
            base_b_ = vec3{2.6f, 0.0f, 0.0f};
            drift_ = normalised(vec3{-1.0f, 0.0f, 0.0f});
            amplitude_ = 2.0f;
            // Yaw only, on BOTH, which is what "standing on a floor" means.
            orient_a_ = engine::quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f}, 0.35f);
            orient_b_ = engine::quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f}, 0.9f);
            break;
        }
    }

    enum class kind { box, capsule, hull };

    [[nodiscard]] vec3 centre_b() const { return base_b_ + drift_ * offset_ + manual_; }

    // The three placements, rebuilt each frame. They are cheap and they are
    // what `as_convex` will point at, so they must outlive the `convex` — which
    // is why they are members rather than locals in `draw()`.
    void place()
    {
        box_a_ = world_obb(box_shape(half_a_), centre_a_, orient_a_);
        box_b_ = world_obb(box_shape(half_b_), centre_b(), orient_b_);
        cap_a_ = world_capsule(capsule_shape(0.45f, 0.7f), centre_a_, orient_a_);
        hull_b_ = world_hull(hull_points_, centre_b(), orient_b_);
    }

    [[nodiscard]] convex view_a() const
    {
        return (kind_a_ == kind::capsule) ? as_convex(cap_a_) : as_convex(box_a_);
    }

    [[nodiscard]] convex view_b() const
    {
        return (kind_b_ == kind::hull) ? as_convex(hull_b_) : as_convex(box_b_);
    }

    /// One GJK search, stopped after `steps` iterations.
    ///
    /// The loop from `gjk.cpp`, written out so the demo can freeze it mid-flight.
    /// It is deliberately NOT a copy of the termination logic — it stops when it
    /// is told to — because what this panel is for is the shape of the search,
    /// not the exactness of the answer.
    void run_search(int steps)
    {
        const convex a = view_a();
        const convex b = view_b();
        const vec3 delta = b.origin - a.origin;

        simplex_.clear();
        const gjk_vertex first = cso_support(a, b, delta, delta);
        simplex_.push(first);
        v_ = first.w;
        enclosed_ = false;
        taken_ = 0;

        float weights[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        for (int i = 0; i < steps; ++i)
        {
            if (length_squared(v_) <= 0.0f) { break; }
            const gjk_vertex w = cso_support(a, b, delta, -v_);

            bool duplicate = false;
            for (int k = 0; k < simplex_.count; ++k)
            {
                if (length_squared(simplex_.v[k].w - w.w) <= 1e-12f) { duplicate = true; }
            }
            if (duplicate) { break; }

            simplex_.push(w);
            ++taken_;
            if (reduce_simplex(simplex_, v_, weights)) { enclosed_ = true; break; }
        }

        // The witness points, from the same weights applied to the two halves
        // each simplex vertex remembers.
        vec3 pa{};
        vec3 pb{};
        for (int i = 0; i < simplex_.count; ++i)
        {
            pa += simplex_.v[i].pa * weights[i];
            pb += simplex_.v[i].pb * weights[i];
        }
        witness_a_ = a.origin + pa;
        witness_b_ = b.origin + pb;
    }

    /// Orthographic projection, shared by both panels. `span` is how many metres
    /// fit across, so the right panel can show a wider world at a smaller scale.
    static void project(vec3 p, int cx, int cy, float scale, int& sx, int& sy)
    {
        const float px = p.x + p.z * k_proj_c;
        const float py = p.y + p.z * k_proj_s;
        sx = cx + static_cast<int>(px * scale);
        sy = cy - static_cast<int>(py * scale);
    }

    void project_left(vec3 p, int& sx, int& sy) const
    {
        project(p, (k_left_x0 + k_left_x1) / 2, (k_left_y0 + k_left_y1) / 2,
                static_cast<float>(k_left_x1 - k_left_x0) / k_left_span, sx, sy);
    }

    void project_right(vec3 p, int& sx, int& sy) const
    {
        project(p, (k_right_x0 + k_right_x1) / 2, (k_right_y0 + k_right_y1) / 2,
                static_cast<float>(k_right_x1 - k_right_x0) / k_right_span, sx, sy);
    }

    void draw_obb(engine::framebuffer& f, const obb& box, Uint32 colour) const
    {
        vec3 c[8];
        box.corners(c);
        for (const auto& e : k_edges)
        {
            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            project_left(c[e[0]], x0, y0);
            project_left(c[e[1]], x1, y1);
            line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1, colour);
        }
    }

    /// A shape drawn the only way this demo knows how to draw an ARBITRARY
    /// convex: by tracing its silhouette through its own support function. A box
    /// could be drawn from its corners; a capsule and a hull could not, and one
    /// code path that works for all three is the lesson's own argument in the
    /// renderer.
    void draw_outline(engine::framebuffer& f, const convex& c, Uint32 colour) const
    {
        draw_silhouette(
            f, [&](vec3 d) { return c.world_support(d); },
            [&](vec3 p, int& x, int& y) { project_left(p, x, y); },
            k_left_x0, k_left_y0, k_left_x1, k_left_y1, colour);
    }

    void draw()
    {
        engine::framebuffer& f = fb();
        f.clear(k_background);

        place();
        run_search(auto_run_ ? 24 : manual_steps_);

        // ---- left: the world ------------------------------------------
        const float lscale = static_cast<float>(k_left_x1 - k_left_x0) / k_left_span;
        const int lcx = (k_left_x0 + k_left_x1) / 2;
        const int lcy = (k_left_y0 + k_left_y1) / 2;
        for (int m = -5; m <= 5; ++m)
        {
            const int gx = lcx + static_cast<int>(static_cast<float>(m) * lscale);
            const int gy = lcy - static_cast<int>(static_cast<float>(m) * lscale);
            line_in(f, gx, k_left_y0, gx, k_left_y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1,
                    (m == 0) ? k_axis : k_grid);
            line_in(f, k_left_x0, gy, k_left_x1, gy, k_left_x0, k_left_y0, k_left_x1, k_left_y1,
                    (m == 0) ? k_axis : k_grid);
        }

        const Uint32 colour_b = enclosed_ ? k_shape_hit : k_shape_b;
        // A box gets its twelve edges, which shows its orientation; anything
        // else gets the silhouette, which is all a support function can offer.
        if (kind_a_ == kind::box) { draw_obb(f, box_a_, k_shape_a); }
        else { draw_outline(f, view_a(), k_shape_a); }
        if (kind_b_ == kind::box) { draw_obb(f, box_b_, colour_b); }
        else { draw_outline(f, view_b(), colour_b); }

        if (!enclosed_)
        {
            int ax = 0, ay = 0, bx = 0, by = 0;
            project_left(witness_a_, ax, ay);
            project_left(witness_b_, bx, by);
            line_in(f, ax, ay, bx, by, k_left_x0, k_left_y0, k_left_x1, k_left_y1, k_witness);
            dot(f, ax, ay, 3, k_witness);
            dot(f, bx, by, 3, k_witness);
        }
        frame_box(f, k_left_x0, k_left_y0, k_left_x1, k_left_y1);

        // ---- right: the Minkowski difference ---------------------------
        const convex a = view_a();
        const convex b = view_b();
        const vec3 delta = b.origin - a.origin;

        // The difference set's outline, from its support function alone — which
        // is the only description of it anyone has, since building the thing
        // would mean up to 8 x 8 vertices for two boxes and 14 x 8 here.
        draw_silhouette(
            f, [&](vec3 d) { return cso_support(a, b, delta, d).w; },
            [&](vec3 p, int& x, int& y) { project_right(p, x, y); },
            k_right_x0, k_right_y0, k_right_x1, k_right_y1, k_cloud);

        int ox = 0, oy = 0;
        project_right(vec3{}, ox, oy);
        line_in(f, ox - 7, oy, ox + 7, oy, k_right_x0, k_right_y0, k_right_x1, k_right_y1,
                k_origin);
        line_in(f, ox, oy - 7, ox, oy + 7, k_right_x0, k_right_y0, k_right_x1, k_right_y1,
                k_origin);

        // The simplex: every edge between the points it holds.
        for (int i = 0; i < simplex_.count; ++i)
        {
            int xi = 0, yi = 0;
            project_right(simplex_.v[i].w, xi, yi);
            dot(f, xi, yi, 3, k_simplex);
            for (int j = i + 1; j < simplex_.count; ++j)
            {
                int xj = 0, yj = 0;
                project_right(simplex_.v[j].w, xj, yj);
                line_in(f, xi, yi, xj, yj, k_right_x0, k_right_y0, k_right_x1, k_right_y1,
                        k_simplex);
            }
        }

        if (!enclosed_)
        {
            int vx = 0, vy = 0;
            project_right(v_, vx, vy);
            line_in(f, ox, oy, vx, vy, k_right_x0, k_right_y0, k_right_x1, k_right_y1, k_vline);
            dot(f, vx, vy, 2, k_vline);
        }
        frame_box(f, k_right_x0, k_right_y0, k_right_x1, k_right_y1);
    }

    void build_panel()
    {
        ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 250.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("GJK");

        static const char* const k_names[3] = {"two crates", "capsule vs hull",
                                               "two on the same floor"};
        ImGui::Text("preset: %s", k_names[preset_]);
        ImGui::Separator();
        ImGui::Text("iterations taken : %d", taken_);
        ImGui::Text("simplex points   : %d", simplex_.count);
        ImGui::Text("|v| (upper bound): %.6f m", static_cast<double>(length(v_)));
        ImGui::Text("origin enclosed  : %s", enclosed_ ? "YES - overlapping" : "no");
        ImGui::Separator();
        ImGui::TextWrapped("The right panel is A - B. The cross is the origin. "
                           "The gold points are the simplex; the pink line is v, "
                           "the closest point found so far. The outline is the "
                           "set's SHADOW, so inside the outline is not inside "
                           "the set - that is what the search is for.");
        if (preset_ == 2)
        {
            ImGui::Separator();
            ImGui::TextWrapped("Both boxes share an up axis, so every support "
                               "point the search asks for comes back with y "
                               "exactly 0. The set is solid; the SIMPLEX never "
                               "leaves its equator, so it can never become a "
                               "tetrahedron. Press [Z] to tilt one box and watch "
                               "the simplex lift out of the plane.");
        }
        ImGui::Separator();
        ImGui::Text("[S] step  [R] restart  [A] auto  [Space] pause");
        ImGui::End();
    }

    engine::debug_ui ui_{};

    const char* shot_path_ = nullptr;
    float shot_at_ = 0.0f;
    int preset_ = 0;

    bool drifting_ = true;
    bool auto_run_ = true;
    int manual_steps_ = 0;

    float t_ = 0.0f;
    float offset_ = 0.0f;
    float amplitude_ = 2.0f;
    vec3 manual_{};
    vec3 drift_{-1.0f, 0.0f, 0.0f};

    kind kind_a_ = kind::box;
    kind kind_b_ = kind::box;
    vec3 half_a_{1.0f, 1.0f, 1.0f};
    vec3 half_b_{1.0f, 1.0f, 1.0f};
    vec3 centre_a_{};
    vec3 base_b_{};
    quat orient_a_ = quat::identity();
    quat orient_b_ = quat::identity();

    std::vector<vec3> hull_points_;

    obb box_a_{};
    obb box_b_{};
    capsule cap_a_{};
    hull hull_b_{};

    simplex simplex_{};
    vec3 v_{};
    vec3 witness_a_{};
    vec3 witness_b_{};
    bool enclosed_ = false;
    int taken_ = 0;
};

} // namespace

ENGINE_MAIN(gjk_app)
