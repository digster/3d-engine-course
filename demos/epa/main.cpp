// demos/epa/main.cpp — the balloon in the dark room.
//
// Lesson 8.6. GJK walks toward the origin from outside and stops when it gets
// there. EPA starts with the origin already inside the Minkowski difference and
// walks OUTWARD to the boundary, one face at a time, and the reason it is worth
// watching rather than reading is that "expand the polytope toward its closest
// face" is a sentence, while the thing it describes is a shape growing until it
// presses against a wall you cannot see.
//
//   LEFT PANEL   the two shapes in the world. When they overlap, the MTV is
//                drawn from the second shape's centre — the shortest push that
//                separates them — and a GHOST outline shows exactly where that
//                push would put it. When the search has not converged the ghost
//                still overlaps, which is the whole point of stepping.
//   RIGHT PANEL  the same pair as ONE set: `A ⊖ B`, outlined by tracing its own
//                support function (8.5's `draw_silhouette`, unchanged). The
//                origin is the cross, and because the shapes overlap it is
//                INSIDE. The gold mesh is the expanding polytope; the pink line
//                is the vector to its closest face, which is the current lower
//                bound on the depth.
//
// WHAT TO WATCH FOR. Press [S] repeatedly. The gold mesh grows outward and the
// pink line LENGTHENS — the opposite of GJK, where `v` only ever shrank — until
// the polytope's closest face lies on the difference set's boundary and the two
// bounds meet. Then press [3]: two crates standing on the same floor, the
// arrangement 8.5 §9 showed GJK cannot build a tetrahedron for. The seed is a
// flat triangle plus two support points found along its normal, and the panel
// says how many extra vertices that cost.
//
// AND PRESS [F]. That drops the flood fill from the visibility test — the
// textbook formulation — and on preset 3 the depth is often visibly wrong and
// the status reads `stalled`, because the horizon came back in pieces. §7 of the
// lesson measures it at 4.17% of crates on a floor, worst case 61.7% low.
//
//   [1] [2] [3]   presets: two crates, a capsule and a hull, two on a floor
//   [S]           one expansion                    [R] restart the expansion
//   [A]           run to completion each frame (the default)
//   [F]           toggle the flood fill            [G] draw the polytope or not
//   [Left/Right]  move the second shape along x    [Up/Down]  along y
//   [,] [.]       along z
//   [Q] [E]       yaw it                           [Z] [C]    tilt it
//   [Space]       pause the drift                  [0] reset      [Esc] quit
//
//     cmake --build build --target epa
//     ./build/demos/epa
//     ./build/demos/epa --preset 2 --t 1.5 --shot out.ppm     headless
//
// IT BUILDS ITS OWN POLYTOPE rather than only calling `epa_penetration`, for the
// same reason `gjk`'s demo drove its own loop: a single-stepping visualiser needs
// the iteration, not the answer. It uses only `cso_support`, which `gjk.hpp`
// exposes for exactly this, so 5.1's boundary is intact — and the numbers in the
// panel come from the real `epa_penetration` beside it, so the picture and the
// engine can be seen to agree.

#include <engine/core/fixed_step.hpp>
#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/epa.hpp>
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
using engine::phys::epa_config;
using engine::phys::epa_penetration;
using engine::phys::epa_result;
using engine::phys::epa_status;
using engine::phys::gjk_distance;
using engine::phys::gjk_result;
using engine::phys::gjk_status;
using engine::phys::gjk_vertex;
using engine::phys::hull;
using engine::phys::name_of;
using engine::phys::obb;
using engine::phys::world_capsule;
using engine::phys::world_hull;
using engine::phys::world_obb;

constexpr int k_width = 960;
constexpr int k_height = 540;
constexpr float k_pi = std::numbers::pi_v<float>;

/// This demo's own log category, in the range Lesson 5.3 reserved for programs
/// outside the library.
constexpr int log_demo = 65;

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_grid       = engine::pack_argb(38, 42, 52);
constexpr Uint32 k_axis       = engine::pack_argb(84, 92, 108);

constexpr Uint32 k_shape_a    = engine::pack_argb(150, 160, 255);
constexpr Uint32 k_shape_b    = engine::pack_argb(235, 96, 96);
constexpr Uint32 k_ghost      = engine::pack_argb(90, 150, 110);
constexpr Uint32 k_mtv        = engine::pack_argb(235, 200, 96);
constexpr Uint32 k_cloud      = engine::pack_argb(70, 78, 96);
constexpr Uint32 k_poly       = engine::pack_argb(190, 160, 70);
constexpr Uint32 k_face       = engine::pack_argb(235, 200, 96);
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

constexpr float k_left_span = 9.0f;
constexpr float k_right_span = 13.0f;

/// 8.5's clipper, unchanged. `engine::draw_line` does not clip, so a demo with
/// two panels clips its own lines.
void line_in(engine::framebuffer& f, int x0, int y0, int x1, int y1,
             int cx0, int cy0, int cx1, int cy1, Uint32 colour)
{
    auto inside = [&](int x, int y) { return x >= cx0 && x <= cx1 && y >= cy0 && y <= cy1; };
    if (inside(x0, y0) && inside(x1, y1))
    {
        engine::draw_line(f, x0, y0, x1, y1, colour);
        return;
    }
    if ((x0 < cx0 && x1 < cx0) || (x0 > cx1 && x1 > cx1) ||
        (y0 < cy0 && y1 < cy0) || (y0 > cy1 && y1 > cy1))
    {
        return;
    }
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

constexpr int k_edges[12][2] = {
    {0, 1}, {2, 3}, {4, 5}, {6, 7},
    {0, 2}, {1, 3}, {4, 6}, {5, 7},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

/// Directions spread over the sphere, for building a hull's vertices — a
/// Fibonacci lattice, because a latitude/longitude grid piles points at the
/// poles and leaves the equator bare.
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
constexpr float k_tilt = 0.42f;
const float k_proj_c = 0.55f * std::cos(k_tilt);
const float k_proj_s = 0.55f * std::sin(k_tilt);

/// 8.5's silhouette trick: a convex set's shadow is convex, and its support
/// function is the original's composed with the transpose of the projection.
[[nodiscard]] vec3 screen_direction(float angle)
{
    const float dx = std::cos(angle);
    const float dy = std::sin(angle);
    return vec3{dx, dy, k_proj_c * dx + k_proj_s * dy};
}

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
// The polytope, rebuilt here so it can be drawn mid-flight
// ---------------------------------------------------------------------------
//
// The same algorithm as `epa.cpp`, written out because the engine's copy is an
// implementation detail behind a function that returns an answer. Everything it
// touches — `cso_support`, `gjk_vertex`, `gjk_result::terminal` — is public.

struct dface
{
    int v[3];
    vec3 n;
    float d;
};

struct dpoly
{
    std::vector<gjk_vertex> vert;
    std::vector<dface> faces;

    void add_face(int i, int j, int k)
    {
        const vec3 a = vert[static_cast<std::size_t>(i)].w;
        const vec3 nn = cross(vert[static_cast<std::size_t>(j)].w - a,
                              vert[static_cast<std::size_t>(k)].w - a);
        const float n2 = length_squared(nn);
        dface f{{i, j, k}, vec3{}, 3.4e38f};
        if (n2 > 1e-24f)
        {
            f.n = nn * (1.0f / std::sqrt(n2));
            f.d = dot(f.n, a);
        }
        faces.push_back(f);
    }

    [[nodiscard]] int closest() const
    {
        int best = -1;
        float best_d = 3.4e38f;
        for (std::size_t i = 0; i < faces.size(); ++i)
        {
            if (faces[i].d < best_d) { best_d = faces[i].d; best = static_cast<int>(i); }
        }
        return best;
    }
};

bool share_edge(const dface& f, const dface& g)
{
    int common = 0;
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            if (f.v[i] == g.v[j]) { ++common; break; }
        }
    }
    return common >= 2;
}

/// Beneath-and-beyond, with `epa.cpp`'s flood fill made optional so [F] can
/// show what the textbook formulation does.
bool grow(dpoly& p, const gjk_vertex& w, int seed_face, bool flood_fill)
{
    std::vector<int> cand;
    int seed_slot = -1;
    for (std::size_t i = 0; i < p.faces.size(); ++i)
    {
        if (dot(p.faces[i].n, w.w) > p.faces[i].d)
        {
            if (static_cast<int>(i) == seed_face) { seed_slot = static_cast<int>(cand.size()); }
            cand.push_back(static_cast<int>(i));
        }
    }
    if (cand.empty() || seed_slot < 0) { return false; }

    std::vector<int> visible;
    if (flood_fill)
    {
        std::vector<bool> taken(cand.size(), false);
        std::vector<int> queue{seed_slot};
        taken[static_cast<std::size_t>(seed_slot)] = true;
        for (std::size_t head = 0; head < queue.size(); ++head)
        {
            const int slot = queue[head];
            visible.push_back(cand[static_cast<std::size_t>(slot)]);
            for (std::size_t j = 0; j < cand.size(); ++j)
            {
                if (taken[j]) { continue; }
                if (share_edge(p.faces[static_cast<std::size_t>(cand[static_cast<std::size_t>(slot)])],
                               p.faces[static_cast<std::size_t>(cand[j])]))
                {
                    taken[j] = true;
                    queue.push_back(static_cast<int>(j));
                }
            }
        }
    }
    else
    {
        visible = cand;
    }

    std::vector<std::pair<int, int>> horizon;
    for (int fi : visible)
    {
        const dface& dead = p.faces[static_cast<std::size_t>(fi)];
        for (int e = 0; e < 3; ++e)
        {
            const int from = dead.v[e];
            const int to = dead.v[(e + 1) % 3];
            bool cancelled = false;
            for (std::size_t h = 0; h < horizon.size(); ++h)
            {
                if (horizon[h].first == to && horizon[h].second == from)
                {
                    horizon.erase(horizon.begin() + static_cast<std::ptrdiff_t>(h));
                    cancelled = true;
                    break;
                }
            }
            if (!cancelled) { horizon.emplace_back(from, to); }
        }
    }
    if (horizon.size() < 3) { return false; }

    // The loop check: on a single closed rim every vertex starts one edge and
    // ends one edge.
    for (const auto& h : horizon)
    {
        int starts = 0;
        int ends = 0;
        for (const auto& g : horizon)
        {
            if (g.first == h.first) { ++starts; }
            if (g.second == h.first) { ++ends; }
        }
        if (starts != 1 || ends != 1) { return false; }
    }

    const int index = static_cast<int>(p.vert.size());
    p.vert.push_back(w);

    std::vector<dface> kept;
    for (std::size_t i = 0; i < p.faces.size(); ++i)
    {
        if (std::find(visible.begin(), visible.end(), static_cast<int>(i)) == visible.end())
        {
            kept.push_back(p.faces[i]);
        }
    }
    p.faces = kept;
    for (const auto& h : horizon) { p.add_face(h.first, h.second, index); }
    return true;
}

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

class epa_app final : public engine::app
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
                preset_ = std::clamp(SDL_atoi(argv[++i]), 0, 2);
            }
            else if (SDL_strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
            {
                manual_steps_ = std::clamp(SDL_atoi(argv[++i]), 0, 32);
                auto_run_ = false;
            }
            else if (SDL_strcmp(argv[i], "--no-flood") == 0)
            {
                flood_fill_ = false;
            }
            else if (SDL_strcmp(argv[i], "--still") == 0)
            {
                drifting_ = false;
            }
        }

        return {.title = "epa — the balloon in the dark room",
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
        ENGINE_LOG_INFO(log_demo, "[F] flood fill  [G] polytope  arrows move  [Space] pause");
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
        case SDL_SCANCODE_F: flood_fill_ = !flood_fill_; break;
        case SDL_SCANCODE_G: show_poly_ = !show_poly_; break;
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
        // non-owning: letting this vector reallocate under a live `hull` would be
        // the dangling-view bug `convex.hpp` deletes its rvalue overloads to
        // prevent.
        hull_points_.clear();
        for (const vec3& d : sphere_directions(14)) { hull_points_.push_back(d * 0.9f); }

        switch (preset_)
        {
        case 0:   // two crates driven through each other
            kind_a_ = kind::box;
            kind_b_ = kind::box;
            half_a_ = vec3{1.0f, 0.8f, 0.9f};
            half_b_ = vec3{0.7f, 1.0f, 0.6f};
            centre_a_ = vec3{-0.4f, 0.0f, 0.0f};
            base_b_ = vec3{1.2f, 0.4f, 0.3f};
            drift_ = normalised(vec3{-1.0f, -0.1f, 0.0f});
            amplitude_ = 1.1f;
            orient_a_ = quat::identity();
            orient_b_ = engine::quat_from_axis_angle(normalised(vec3{0.2f, 1.0f, 0.3f}), 0.6f);
            break;

        case 1:   // a capsule inside a convex hull: neither is SAT-testable
            kind_a_ = kind::capsule;
            kind_b_ = kind::hull;
            centre_a_ = vec3{-0.5f, 0.0f, 0.0f};
            base_b_ = vec3{0.9f, 0.3f, 0.2f};
            drift_ = normalised(vec3{-1.0f, 0.0f, 0.0f});
            amplitude_ = 0.9f;
            orient_a_ = engine::quat_from_axis_angle(normalised(vec3{0.0f, 0.0f, 1.0f}), 0.5f);
            orient_b_ = engine::quat_from_axis_angle(normalised(vec3{1.0f, 1.0f, 0.0f}), 0.9f);
            break;

        default:  // THE DEGENERATE ONE: two crates standing on the same floor
            kind_a_ = kind::box;
            kind_b_ = kind::box;
            half_a_ = vec3{1.0f, 1.0f, 1.0f};
            half_b_ = vec3{1.0f, 1.0f, 1.0f};
            centre_a_ = vec3{-0.7f, 0.0f, 0.0f};
            base_b_ = vec3{1.4f, 0.0f, 0.0f};
            drift_ = normalised(vec3{-1.0f, 0.0f, 0.0f});
            amplitude_ = 1.0f;
            // Yaw only, on BOTH, which is what "standing on a floor" means.
            orient_a_ = engine::quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f}, 0.35f);
            orient_b_ = engine::quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f}, 0.9f);
            break;
        }
    }

    enum class kind { box, capsule, hull };

    [[nodiscard]] vec3 centre_b() const { return base_b_ + drift_ * offset_ + manual_; }

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

    /// Seed the polytope from GJK's terminal simplex, then expand `steps` times.
    ///
    /// The seed is `epa.cpp`'s: a tetrahedron when GJK left one with volume, and
    /// otherwise a triangle plus two support points found along its plane normal,
    /// added through the same `grow` the loop uses so that what comes out is the
    /// convex hull rather than six triangles that might not be one.
    void run_expansion(int steps)
    {
        const convex a = view_a();
        const convex b = view_b();
        const vec3 delta = b.origin - a.origin;

        poly_.vert.clear();
        poly_.faces.clear();
        seed_extra_ = 0;
        taken_ = 0;
        lower_ = 0.0f;
        upper_ = 0.0f;
        normal_ = vec3{};
        torn_ = false;

        const gjk_result g = gjk_distance(a, b);
        overlapping_ = g.status != gjk_status::separated;
        engine_ = epa_penetration(a, b, g.terminal, {0.0001f, 32, flood_fill_});
        if (!overlapping_) { return; }

        gjk_vertex pts[4];
        int count = std::min(4, g.terminal.count);
        for (int i = 0; i < count; ++i) { pts[i] = g.terminal.v[i]; }
        if (count == 0) { return; }

        bool have_tetra = false;
        if (count == 4)
        {
            const float vol6 = dot(cross(pts[1].w - pts[0].w, pts[2].w - pts[0].w),
                                   pts[3].w - pts[0].w);
            have_tetra = std::fabs(vol6) > 1e-7f;
        }

        if (!have_tetra)
        {
            // Grow to a triangle first, the way `seed_polytope` does.
            if (count == 1)
            {
                static const vec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                                             {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
                float best = 0.0f;
                for (const vec3& d : dirs)
                {
                    const gjk_vertex w = cso_support(a, b, delta, d);
                    const float m2 = length_squared(w.w - pts[0].w);
                    if (m2 > best) { best = m2; pts[1] = w; }
                }
                if (best <= 0.0f) { return; }
                count = 2;
                ++seed_extra_;
            }
            if (count == 2)
            {
                const vec3 e = pts[1].w - pts[0].w;
                const vec3 ax =
                    (std::fabs(e.x) <= std::fabs(e.y) && std::fabs(e.x) <= std::fabs(e.z))
                        ? vec3{1, 0, 0}
                        : (std::fabs(e.y) <= std::fabs(e.z) ? vec3{0, 1, 0} : vec3{0, 0, 1});
                const vec3 t0 = normalised_or(cross(e, ax), vec3{1, 0, 0});
                const vec3 t1 = normalised_or(cross(e, t0), vec3{0, 1, 0});
                const vec3 around[4] = {t0, -t0, t1, -t1};
                float best = 0.0f;
                for (const vec3& d : around)
                {
                    const gjk_vertex w = cso_support(a, b, delta, d);
                    const float area2 = length_squared(cross(e, w.w - pts[0].w));
                    if (area2 > best) { best = area2; pts[2] = w; }
                }
                if (best <= 0.0f) { return; }
                count = 3;
                ++seed_extra_;
            }

            const vec3 plane = normalised_or(cross(pts[1].w - pts[0].w, pts[2].w - pts[0].w),
                                             vec3{0, 1, 0});
            const gjk_vertex up = cso_support(a, b, delta, plane);
            const gjk_vertex down = cso_support(a, b, delta, -plane);
            seed_extra_ += 2;
            if (dot(up.w - pts[0].w, plane) <= 0.0f) { return; }
            if (dot(pts[0].w - down.w, plane) <= 0.0f) { return; }

            pts[3] = up;
            build_tetra(pts);
            // The base is face 0 after the winding fix, and `down` is beyond it.
            if (!grow(poly_, down, 0, true)) { return; }
        }
        else
        {
            build_tetra(pts);
        }

        for (int i = 0; i < steps; ++i)
        {
            const int f = poly_.closest();
            if (f < 0) { break; }
            const dface face = poly_.faces[static_cast<std::size_t>(f)];
            const gjk_vertex w = cso_support(a, b, delta, face.n);
            const float h = dot(w.w, face.n);
            if (h - face.d <= 1e-4f) { break; }

            bool duplicate = false;
            for (const gjk_vertex& v : poly_.vert)
            {
                if (length_squared(v.w - w.w) <= 1e-12f) { duplicate = true; }
            }
            if (duplicate) { break; }

            if (!grow(poly_, w, f, flood_fill_)) { torn_ = true; break; }
            ++taken_;
        }

        const int f = poly_.closest();
        if (f >= 0)
        {
            const dface face = poly_.faces[static_cast<std::size_t>(f)];
            lower_ = face.d;
            normal_ = face.n;
            upper_ = dot(cso_support(a, b, delta, face.n).w, face.n);
        }
    }

    void build_tetra(gjk_vertex q[4])
    {
        const float vol6 = dot(cross(q[1].w - q[0].w, q[2].w - q[0].w), q[3].w - q[0].w);
        if (vol6 > 0.0f) { std::swap(q[1], q[2]); }
        for (int i = 0; i < 4; ++i) { poly_.vert.push_back(q[i]); }
        poly_.add_face(0, 1, 2);
        poly_.add_face(0, 3, 1);
        poly_.add_face(0, 2, 3);
        poly_.add_face(1, 3, 2);
    }

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

    void draw_obb(engine::framebuffer& f, const obb& box, vec3 shift, Uint32 colour) const
    {
        vec3 c[8];
        box.corners(c);
        for (const auto& e : k_edges)
        {
            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            project_left(c[e[0]] + shift, x0, y0);
            project_left(c[e[1]] + shift, x1, y1);
            line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1, colour);
        }
    }

    void draw_outline(engine::framebuffer& f, const convex& c, vec3 shift, Uint32 colour) const
    {
        draw_silhouette(
            f, [&](vec3 d) { return c.world_support(d) + shift; },
            [&](vec3 p, int& x, int& y) { project_left(p, x, y); },
            k_left_x0, k_left_y0, k_left_x1, k_left_y1, colour);
    }

    void draw()
    {
        engine::framebuffer& f = fb();
        f.clear(k_background);

        place();
        run_expansion(auto_run_ ? 32 : manual_steps_);

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

        if (kind_a_ == kind::box) { draw_obb(f, box_a_, vec3{}, k_shape_a); }
        else { draw_outline(f, view_a(), vec3{}, k_shape_a); }
        if (kind_b_ == kind::box) { draw_obb(f, box_b_, vec3{}, k_shape_b); }
        else { draw_outline(f, view_b(), vec3{}, k_shape_b); }

        // THE GHOST: where the MTV would put the second shape. When the search
        // has not finished, the ghost still overlaps — which is what an
        // unconverged lower bound means, drawn rather than described.
        if (overlapping_ && lower_ > 0.0f)
        {
            const vec3 push = normal_ * lower_;
            if (kind_b_ == kind::box) { draw_obb(f, box_b_, push, k_ghost); }
            else { draw_outline(f, view_b(), push, k_ghost); }

            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            project_left(centre_b(), x0, y0);
            project_left(centre_b() + push, x1, y1);
            line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1, k_mtv);
            dot(f, x1, y1, 3, k_mtv);
        }
        frame_box(f, k_left_x0, k_left_y0, k_left_x1, k_left_y1);

        // ---- right: the Minkowski difference ---------------------------
        const convex a = view_a();
        const convex b = view_b();
        const vec3 delta = b.origin - a.origin;

        draw_silhouette(
            f, [&](vec3 d) { return cso_support(a, b, delta, d).w; },
            [&](vec3 p, int& x, int& y) { project_right(p, x, y); },
            k_right_x0, k_right_y0, k_right_x1, k_right_y1, k_cloud);

        int ox = 0, oy = 0;
        project_right(vec3{}, ox, oy);

        if (show_poly_)
        {
            const int cf = poly_.closest();
            for (std::size_t i = 0; i < poly_.faces.size(); ++i)
            {
                const Uint32 colour = (static_cast<int>(i) == cf) ? k_face : k_poly;
                for (int e = 0; e < 3; ++e)
                {
                    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
                    project_right(poly_.vert[static_cast<std::size_t>(
                                                 poly_.faces[i].v[e])].w,
                                  x0, y0);
                    project_right(poly_.vert[static_cast<std::size_t>(
                                                 poly_.faces[i].v[(e + 1) % 3])].w,
                                  x1, y1);
                    line_in(f, x0, y0, x1, y1, k_right_x0, k_right_y0, k_right_x1, k_right_y1,
                            colour);
                }
            }
        }

        line_in(f, ox - 7, oy, ox + 7, oy, k_right_x0, k_right_y0, k_right_x1, k_right_y1,
                k_origin);
        line_in(f, ox, oy - 7, ox, oy + 7, k_right_x0, k_right_y0, k_right_x1, k_right_y1,
                k_origin);

        // The vector to the closest face: the current lower bound, which unlike
        // GJK's `v` only ever gets LONGER.
        if (overlapping_ && lower_ > 0.0f)
        {
            int vx = 0, vy = 0;
            project_right(normal_ * lower_, vx, vy);
            line_in(f, ox, oy, vx, vy, k_right_x0, k_right_y0, k_right_x1, k_right_y1, k_vline);
            dot(f, vx, vy, 2, k_vline);
        }
        frame_box(f, k_right_x0, k_right_y0, k_right_x1, k_right_y1);
    }

    void build_panel()
    {
        ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(330.0f, 300.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("EPA");

        static const char* const k_names[3] = {"two crates", "capsule in a hull",
                                               "two on the same floor"};
        ImGui::Text("preset: %s", k_names[preset_]);
        ImGui::Separator();
        if (!overlapping_)
        {
            ImGui::TextWrapped("Apart. There is no penetration to measure - that "
                               "is GJK's question, and 8.5 answered it.");
        }
        else
        {
            ImGui::Text("expansions taken : %d", taken_);
            ImGui::Text("seed vertices    : %d extra", seed_extra_);
            ImGui::Text("polytope         : %zu verts, %zu faces", poly_.vert.size(),
                        poly_.faces.size());
            ImGui::Text("lower bound      : %.6f m", static_cast<double>(lower_));
            ImGui::Text("upper bound      : %.6f m", static_cast<double>(upper_));
            ImGui::Separator();
            ImGui::Text("engine answer    : %.6f m", static_cast<double>(engine_.depth));
            ImGui::Text("engine status    : %s", name_of(engine_.status));
        }
        ImGui::Separator();
        ImGui::Text("flood fill: %s%s", flood_fill_ ? "ON" : "OFF (textbook)",
                    torn_ ? "   HORIZON TORN" : "");
        ImGui::Separator();
        ImGui::TextWrapped("Right panel: A - B, with the origin INSIDE it because "
                           "the shapes overlap. The gold mesh is the expanding "
                           "polytope and the pink line reaches its closest face - "
                           "the current lower bound on the depth. It only ever "
                           "gets LONGER, which is GJK's monotonicity reversed.");
        if (preset_ == 2)
        {
            ImGui::Separator();
            ImGui::TextWrapped("Both crates share an up axis, so GJK's simplex "
                               "never leaves the y = 0 plane and hands over a "
                               "flat TRIANGLE. The two extra seed vertices are "
                               "the support points found along its normal. Press "
                               "[F] to drop the flood fill and watch the depth go "
                               "wrong.");
        }
        ImGui::Separator();
        ImGui::Text("[S] step [R] restart [A] auto [F] flood [G] mesh");
        ImGui::End();
    }

    engine::debug_ui ui_{};

    const char* shot_path_ = nullptr;
    float shot_at_ = 0.0f;
    int preset_ = 0;

    bool drifting_ = true;
    bool auto_run_ = true;
    bool flood_fill_ = true;
    bool show_poly_ = true;
    int manual_steps_ = 0;

    float t_ = 0.0f;
    float offset_ = 0.0f;
    float amplitude_ = 1.0f;
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

    dpoly poly_{};
    epa_result engine_{};
    vec3 normal_{};
    float lower_ = 0.0f;
    float upper_ = 0.0f;
    int taken_ = 0;
    int seed_extra_ = 0;
    bool overlapping_ = false;
    bool torn_ = false;
};

} // namespace

ENGINE_MAIN(epa_app)
