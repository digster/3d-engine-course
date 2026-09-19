// demos/broadphase/main.cpp — the cheap question, asked of everything.
//
// Lesson 8.8. The four demos before this one each showed ONE pair of shapes in
// close-up: the SAT's axes, GJK's simplex, EPA's growing polytope, the contact
// manifold's four points. This one zooms all the way out, because the broadphase
// is the only stage of collision detection whose subject is the whole scene, and
// the thing worth seeing is not a shape at all — it is a lattice, and which of
// its cells are busy.
//
//   THE VIEW is from directly above: world x to the right, world z down, and the
//     grid drawn at the cell size in use. Height (world y) is shown as
//     brightness, dimmest at the floor.
//   CELL SHADING is occupancy. A cell holding one proxy is barely tinted; a cell
//     holding eight is bright, and its pair loop is doing 28 comparisons rather
//     than none. Watch it while [Up] and [Down] change the cell size: the whole
//     lesson about cell size is visible as the lattice getting finer while the
//     cells get darker, and then as it gets coarser while a few cells glow.
//   CANDIDATE PAIRS are drawn as lines between proxy centres, so the number the
//     panel reports has a picture attached.
//
// WHAT TO WATCH FOR. Press [B] and let the brute-force check run: the panel's
// "agree" line is the contract, checked live, every frame, against n(n-1)/2
// AABB tests. Then press [Up] or [Down] a few times. The cell size changes the
// entry count by orders of magnitude, changes the bucket tests, changes the
// milliseconds — and the pair count does not move by one, because a cell size is
// a performance decision and the answer is not allowed to depend on it.
//
// THEN PRESS [T]. A ground plane appears under the crates, and with the guard
// off it is a catastrophe you can watch: the entry count jumps by more than the
// entire rest of the scene, because one proxy is claiming tens of thousands of
// cells it is alone in. [Y] turns the guard back on and the number falls back to
// where it was, with the pair list unchanged. That is the teapot in the stadium,
// and it is the reason a uniform grid is the beginning of the subject and not
// the end of it.
//
//   [1]..[4]     presets: scattered, lattice, crowd, mixed sizes
//   [Up] [Down]  cell size            [Left] [Right]  proxy count
//   [M]          cycle the margin     [G]  grid on/off
//   [P]          candidate pairs      [C]  cell shading
//   [B]          brute-force check    [T]  add the ground plane
//   [Y]          the oversized guard  [Space] pause     [0] reset   [Esc] quit
//
//     cmake --build build --target broadphase
//     ./build/demos/broadphase
//     ./build/demos/broadphase --preset 3 --t 1.5 --shot out.ppm    headless
//     ./build/demos/broadphase --ground --no-guard --cell 1.0        the teapot
//
// `engine::engine` directly and NOT `demo_common`, the same statement `gimbal`,
// `rig`, `plane`, `collector`, `ecs_swarm`, `audio`, `integrate`, `bodies`,
// `spin`, `collide`, `gjk`, `epa` and `manifold` make. It loads nothing and
// computes everything on screen, so a `--shot` run is byte-for-byte reproducible
// on any machine.
//
// No `engine_use_assets` and no `engine_use_shaders`.

#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/bounds.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/broadphase.hpp>
#include <engine/phys/collide.hpp>
#include <engine/phys/shape.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>
#include <engine/ui/debug_ui.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace {

using engine::aabb;
using engine::vec3;
using engine::phys::broadphase_config;
using engine::phys::broadphase_pair;
using engine::phys::brute_force_pairs;
using engine::phys::k_max_cells_per_proxy;
using engine::phys::overlaps;
using engine::phys::proxy;
using engine::phys::uniform_grid;

constexpr int k_width = 960;
constexpr int k_height = 540;

/// This demo's own log category, in the range Lesson 5.3 reserved for programs
/// outside the library.
constexpr int log_demo = 68;

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_grid_line  = engine::pack_argb(40, 45, 56);
constexpr Uint32 k_frame      = engine::pack_argb(90, 98, 114);
constexpr Uint32 k_pair       = engine::pack_argb(255, 214, 90);
constexpr Uint32 k_big        = engine::pack_argb(235, 96, 96);

constexpr int k_view_x0 = 8;
constexpr int k_view_y0 = 8;
constexpr int k_view_x1 = 631;
constexpr int k_view_y1 = 531;

/// The world half-width the view covers, in metres. Everything is drawn from
/// directly above, so one number sets the scale on both axes.
constexpr float k_view_half = 22.0f;

/// The same deterministic generator the harnesses use, so the demo's scene is
/// the same scene on every machine and a `--shot` run is reproducible.
class rng
{
public:
    explicit rng(Uint32 seed) : state_(seed | 1u) {}

    Uint32 next()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }

private:
    Uint32 state_;
};

/// One moving body. The demo owns motion; the grid never sees it.
struct mover
{
    vec3 centre{};
    vec3 velocity{};
    vec3 half{0.5f, 0.5f, 0.5f};
};

enum class preset
{
    scattered,
    lattice,
    crowd,
    mixed
};

const char* name_of(preset p)
{
    switch (p)
    {
    case preset::scattered: return "scattered";
    case preset::lattice:   return "lattice";
    case preset::crowd:     return "crowd";
    case preset::mixed:     return "mixed sizes";
    }
    return "?";
}

class broadphase_app final : public engine::app
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
            else if (arg == "--preset" && i + 1 < argc)
            {
                preset_ = static_cast<preset>(std::clamp(std::atoi(argv[++i]) - 1, 0, 3));
            }
            else if (arg == "--cell" && i + 1 < argc)
            {
                cell_ = std::clamp(static_cast<float>(std::atof(argv[++i])), 0.2f, 40.0f);
            }
            else if (arg == "--ground") { ground_ = true; }
            else if (arg == "--no-guard") { guard_ = false; }
        }
        return {.title = "broadphase — the cheap question, asked of everything",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    bool on_start() override
    {
        rebuild_scene();
        ENGINE_LOG_INFO(log_demo, "broadphase demo: %d proxies, cell %.2f m", count_,
                        static_cast<double>(cell_));
        return ui_.start(window(), renderer()) || shot_path_ != nullptr;
    }

    void on_event(const SDL_Event& event) override
    {
        (void)ui_.handle_event(event);
        if (event.type != SDL_EVENT_KEY_DOWN) { return; }

        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_1: preset_ = preset::scattered; rebuild_scene(); break;
        case SDL_SCANCODE_2: preset_ = preset::lattice;   rebuild_scene(); break;
        case SDL_SCANCODE_3: preset_ = preset::crowd;     rebuild_scene(); break;
        case SDL_SCANCODE_4: preset_ = preset::mixed;     rebuild_scene(); break;
        case SDL_SCANCODE_UP:    cell_ = std::min(cell_ * 1.25f, 40.0f); break;
        case SDL_SCANCODE_DOWN:  cell_ = std::max(cell_ / 1.25f, 0.2f); break;
        case SDL_SCANCODE_RIGHT: count_ = std::min(count_ * 2, 8192); rebuild_scene(); break;
        case SDL_SCANCODE_LEFT:  count_ = std::max(count_ / 2, 16);   rebuild_scene(); break;
        case SDL_SCANCODE_M:     margin_step_ = (margin_step_ + 1) % 4; break;
        case SDL_SCANCODE_G:     draw_grid_ = !draw_grid_; break;
        case SDL_SCANCODE_P:     draw_pairs_ = !draw_pairs_; break;
        case SDL_SCANCODE_C:     draw_cells_ = !draw_cells_; break;
        case SDL_SCANCODE_B:     check_ = !check_; break;
        case SDL_SCANCODE_T:     ground_ = !ground_; rebuild_scene(); break;
        case SDL_SCANCODE_Y:     guard_ = !guard_; break;
        case SDL_SCANCODE_SPACE: running_ = !running_; break;
        case SDL_SCANCODE_0:     reset(); break;
        default: break;
        }
    }

    void on_input() override { ui_.begin_frame(); }

    void on_fixed_step(float h) override
    {
        t_ += h;
        if (running_) { advance(h); }
    }

    void on_frame(float alpha) override
    {
        (void)alpha;

        rebuild_proxies();

        broadphase_config cfg;
        cfg.cell_size = cell_;
        cfg.margin = margin();
        cfg.max_cells_per_proxy = guard_ ? k_max_cells_per_proxy : (1 << 30);

        const Uint64 t0 = SDL_GetPerformanceCounter();
        grid_.build(proxies_, cfg);
        const Uint64 t1 = SDL_GetPerformanceCounter();
        grid_ms_ = 1000.0 * static_cast<double>(t1 - t0) /
                   static_cast<double>(SDL_GetPerformanceFrequency());

        if (check_)
        {
            const Uint64 t2 = SDL_GetPerformanceCounter();
            brute_force_pairs(proxies_, reference_);
            const Uint64 t3 = SDL_GetPerformanceCounter();
            brute_ms_ = 1000.0 * static_cast<double>(t3 - t2) /
                        static_cast<double>(SDL_GetPerformanceFrequency());
            agree_ = same_set();
        }

        fb().clear(k_background);
        draw_view();

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
    [[nodiscard]] float margin() const
    {
        constexpr float steps[4] = {0.0f, 0.05f, 0.25f, 1.0f};
        return steps[margin_step_];
    }

    void reset()
    {
        cell_ = 1.5f;
        count_ = 512;
        margin_step_ = 0;
        guard_ = true;
        ground_ = false;
        rebuild_scene();
    }

    /// Place the bodies. The grid is told nothing about any of this.
    void rebuild_scene()
    {
        rng r(0x8888u);
        movers_.clear();
        movers_.reserve(static_cast<std::size_t>(count_));

        const float span = k_view_half * 0.92f;
        for (int i = 0; i < count_; ++i)
        {
            mover m;
            switch (preset_)
            {
            case preset::scattered:
                m.centre = {r.range(-span, span), r.range(0.5f, 8.0f), r.range(-span, span)};
                break;
            case preset::lattice:
            {
                const int side = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count_))));
                const int x = i % side;
                const int z = i / side;
                const float pitch = 2.0f * span / static_cast<float>(side);
                m.centre = {-span + pitch * (static_cast<float>(x) + 0.5f), 0.6f,
                            -span + pitch * (static_cast<float>(z) + 0.5f)};
                break;
            }
            case preset::crowd:
                m.centre = {r.range(-span * 0.3f, span * 0.3f), r.range(0.5f, 4.0f),
                            r.range(-span * 0.3f, span * 0.3f)};
                break;
            case preset::mixed:
            {
                const float s = (i % 17 == 0) ? r.range(2.0f, 5.0f) : r.range(0.3f, 0.7f);
                m.half = {s, s, s};
                m.centre = {r.range(-span, span), r.range(0.5f, 8.0f), r.range(-span, span)};
                break;
            }
            }
            m.velocity = {r.range(-2.0f, 2.0f), r.range(-0.6f, 0.6f), r.range(-2.0f, 2.0f)};
            movers_.push_back(m);
        }

        if (ground_)
        {
            // The teapot. A 200 m plate, two hundred times the cell in two of
            // its three dimensions, and one proxy like every other one.
            mover g;
            g.half = {100.0f, 0.1f, 100.0f};
            g.centre = {0.0f, 0.0f, 0.0f};
            g.velocity = {};
            movers_.push_back(g);
        }
        rebuild_proxies();
    }

    void advance(float dt)
    {
        const float span = k_view_half;
        for (mover& m : movers_)
        {
            if (m.half.x > 50.0f) { continue; }   // the ground plate stays put
            m.centre = m.centre + m.velocity * dt;
            // Reflect off the walls of the box the scene lives in. Not physics —
            // this demo has no solver, it has a broadphase.
            if (m.centre.x < -span || m.centre.x > span) { m.velocity.x = -m.velocity.x; }
            if (m.centre.y < 0.5f || m.centre.y > 10.0f) { m.velocity.y = -m.velocity.y; }
            if (m.centre.z < -span || m.centre.z > span) { m.velocity.z = -m.velocity.z; }
        }
    }

    /// The one line that connects the simulation to the broadphase: a world box
    /// and an index, nothing else.
    void rebuild_proxies()
    {
        proxies_.resize(movers_.size());
        for (std::size_t i = 0; i < movers_.size(); ++i)
        {
            proxies_[i].box.min = movers_[i].centre - movers_[i].half;
            proxies_[i].box.max = movers_[i].centre + movers_[i].half;
            proxies_[i].index = static_cast<Uint32>(i);
        }
    }

    [[nodiscard]] bool same_set() const
    {
        std::vector<broadphase_pair> a(grid_.pairs().begin(), grid_.pairs().end());
        std::vector<broadphase_pair> b = reference_;
        const auto less = [](const broadphase_pair& x, const broadphase_pair& y) {
            return x.a != y.a ? x.a < y.a : x.b < y.b;
        };
        std::sort(a.begin(), a.end(), less);
        std::sort(b.begin(), b.end(), less);
        return a.size() == b.size() &&
               std::equal(a.begin(), a.end(), b.begin(),
                          [](const broadphase_pair& x, const broadphase_pair& y) {
                              return x.a == y.a && x.b == y.b;
                          });
    }

    // ---- drawing ---------------------------------------------------------

    [[nodiscard]] int sx(float x) const
    {
        const float u = (x + k_view_half) / (2.0f * k_view_half);
        return k_view_x0 + static_cast<int>(u * static_cast<float>(k_view_x1 - k_view_x0));
    }
    [[nodiscard]] int sy(float z) const
    {
        const float v = (z + k_view_half) / (2.0f * k_view_half);
        return k_view_y0 + static_cast<int>(v * static_cast<float>(k_view_y1 - k_view_y0));
    }

    void draw_view()
    {
        engine::framebuffer& f = fb();

        if (draw_cells_) { shade_cells(); }
        if (draw_grid_) { draw_lattice(); }

        // The frame around the world, so the view has an edge.
        engine::draw_line(f, k_view_x0, k_view_y0, k_view_x1, k_view_y0, k_frame);
        engine::draw_line(f, k_view_x1, k_view_y0, k_view_x1, k_view_y1, k_frame);
        engine::draw_line(f, k_view_x1, k_view_y1, k_view_x0, k_view_y1, k_frame);
        engine::draw_line(f, k_view_x0, k_view_y1, k_view_x0, k_view_y0, k_frame);

        if (draw_pairs_) { draw_pairs(); }
        draw_proxies();
    }

    /// One tinted square per occupied cell, brighter the fuller it is. The
    /// occupancy is recomputed here rather than read out of the grid, because
    /// the grid deliberately does not keep a map from cell to entries — its
    /// whole storage strategy is that it does not need one.
    void shade_cells()
    {
        engine::framebuffer& f = fb();
        const int nx = static_cast<int>(std::ceil(2.0f * k_view_half / cell_)) + 2;
        if (nx > 512) { return; }   // finer than a pixel; nothing to see

        counts_.assign(static_cast<std::size_t>(nx) * static_cast<std::size_t>(nx), 0);
        const float m = margin();
        for (const proxy& p : proxies_)
        {
            aabb b = p.box;
            b.grow(m);
            if (b.extent().x > 50.0f) { continue; }   // do not paint the plate over everything
            const int x0 = cell_index(b.min.x, nx);
            const int x1 = cell_index(b.max.x, nx);
            const int z0 = cell_index(b.min.z, nx);
            const int z1 = cell_index(b.max.z, nx);
            for (int z = z0; z <= z1; ++z)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    ++counts_[static_cast<std::size_t>(z) * static_cast<std::size_t>(nx) +
                              static_cast<std::size_t>(x)];
                }
            }
        }

        for (int z = 0; z < nx; ++z)
        {
            for (int x = 0; x < nx; ++x)
            {
                const int n = counts_[static_cast<std::size_t>(z) * static_cast<std::size_t>(nx) +
                                      static_cast<std::size_t>(x)];
                if (n == 0) { continue; }
                // A cell's pair loop runs C(n, 2) comparisons, so the shade is
                // keyed on THAT rather than on the occupancy — the picture is
                // meant to show where the work is, and the work is quadratic.
                const int tests = n * (n - 1) / 2;
                const float u = std::min(1.0f, static_cast<float>(tests) / 40.0f);
                const Uint32 c = engine::pack_argb(static_cast<Uint8>(22.0f + 40.0f * u),
                                                   static_cast<Uint8>(26.0f + 70.0f * u),
                                                   static_cast<Uint8>(40.0f + 110.0f * u));
                // Clamped to the view, because the outermost cell index is
                // clamped too and would otherwise paint a stripe off the edge.
                const int px0 = std::clamp(sx(-k_view_half + static_cast<float>(x) * cell_),
                                           k_view_x0, k_view_x1);
                const int px1 = std::clamp(sx(-k_view_half + static_cast<float>(x + 1) * cell_),
                                           k_view_x0, k_view_x1);
                const int py0 = std::clamp(sy(-k_view_half + static_cast<float>(z) * cell_),
                                           k_view_y0, k_view_y1);
                const int py1 = std::clamp(sy(-k_view_half + static_cast<float>(z + 1) * cell_),
                                           k_view_y0, k_view_y1);
                if (px1 <= px0 || py1 <= py0) { continue; }
                f.fill_rect(px0, py0, px1 - px0, py1 - py0, c);
            }
        }
    }

    [[nodiscard]] int cell_index(float v, int nx) const
    {
        const int i = static_cast<int>(std::floor((v + k_view_half) / cell_));
        return std::clamp(i, 0, nx - 1);
    }

    void draw_lattice()
    {
        engine::framebuffer& f = fb();
        const int lines = static_cast<int>(2.0f * k_view_half / cell_);
        if (lines > 400) { return; }   // denser than the pixels; drawing it is a lie
        for (int i = 0; i <= lines; ++i)
        {
            const float w = -k_view_half + static_cast<float>(i) * cell_;
            engine::draw_line(f, sx(w), k_view_y0, sx(w), k_view_y1, k_grid_line);
            engine::draw_line(f, k_view_x0, sy(w), k_view_x1, sy(w), k_grid_line);
        }
    }

    void draw_pairs()
    {
        engine::framebuffer& f = fb();
        for (const broadphase_pair& p : grid_.pairs())
        {
            const vec3 a = movers_[p.a].centre;
            const vec3 b = movers_[p.b].centre;
            engine::draw_line(f, sx(a.x), sy(a.z), sx(b.x), sy(b.z), k_pair);
        }
    }

    void draw_proxies()
    {
        engine::framebuffer& f = fb();
        for (std::size_t i = 0; i < proxies_.size(); ++i)
        {
            const aabb& b = proxies_[i].box;
            const bool big = b.extent().x > 50.0f;
            const int x0 = sx(b.min.x);
            const int x1 = sx(b.max.x);
            const int y0 = sy(b.min.z);
            const int y1 = sy(b.max.z);

            // Height as brightness: a crate near the floor is dim, one at the
            // top of the box is bright. The grid is three-dimensional and the
            // picture is not, so the third axis has to go somewhere.
            const float h = std::clamp(movers_[i].centre.y / 10.0f, 0.0f, 1.0f);
            const Uint8 v = static_cast<Uint8>(80.0f + 150.0f * h);
            const Uint32 c =
                big ? k_big
                    : engine::pack_argb(static_cast<Uint8>(v * 0.55f),
                                        static_cast<Uint8>(v * 0.80f), static_cast<Uint8>(v));

            if (x1 - x0 <= 1 || y1 - y0 <= 1)
            {
                f.put_pixel(x0, y0, c);
                continue;
            }
            engine::draw_line(f, x0, y0, x1, y0, c);
            engine::draw_line(f, x1, y0, x1, y1, c);
            engine::draw_line(f, x1, y1, x0, y1, c);
            engine::draw_line(f, x0, y1, x0, y0, c);
        }
    }

    void panel()
    {
        const engine::phys::broadphase_stats& s = grid_.stats();

        ImGui::SetNextWindowPos(ImVec2(646.0f, 8.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(306.0f, 524.0f), ImGuiCond_Always);
        ImGui::Begin("broadphase", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse);

        ImGui::Text("%s, %d proxies", name_of(preset_), s.proxies);
        ImGui::Text("cell %.3f m   margin %.2f m", static_cast<double>(cell_),
                    static_cast<double>(margin()));
        ImGui::Separator();

        ImGui::Text("entries          %10d", s.entries);
        ImGui::Text("buckets          %10d", s.buckets);
        ImGui::Text("occupied         %10d", s.occupied_buckets);
        ImGui::Text("largest bucket   %10d", s.largest_bucket);
        ImGui::Text("2+ cells sharing %10d", s.colliding_buckets);
        ImGui::Separator();

        ImGui::Text("pair tests       %10lld", s.bucket_tests);
        ImGui::Text("  wrong cell     %10lld", s.cell_rejects);
        ImGui::Text("  not the owner  %10lld", s.owner_rejects);
        ImGui::Text("  boxes apart    %10lld", s.box_rejects);
        ImGui::Text("PAIRS            %10d", s.pairs);
        ImGui::Separator();

        ImGui::Text("oversized        %10d %s", s.oversized, guard_ ? "" : "(guard off)");
        ImGui::Text("from that list   %10d", s.oversized_pairs);
        ImGui::Separator();

        ImGui::Text("grid             %8.3f ms", grid_ms_);
        if (check_)
        {
            ImGui::Text("brute force      %8.3f ms", brute_ms_);
            ImGui::Text("speedup          %8.1fx", brute_ms_ / std::max(grid_ms_, 1e-6));
            ImGui::TextColored(agree_ ? ImVec4(0.4f, 0.9f, 0.5f, 1.0f)
                                      : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               "agree: %s  (%zu pairs)", agree_ ? "yes" : "NO", reference_.size());
        }
        else
        {
            ImGui::TextDisabled("[B] to check against n(n-1)/2");
        }

        ImGui::Separator();
        ImGui::TextWrapped("The cell size changes entries, pair tests and "
                           "milliseconds. It does not change PAIRS.");
        ImGui::Separator();
        ImGui::Text("[1-4] preset  [Up/Dn] cell  [Lt/Rt] count");
        ImGui::Text("[M] margin [G] grid [C] cells [P] pairs");
        ImGui::Text("[B] check  [T] ground plate  [Y] guard");
        ImGui::End();
    }

    engine::debug_ui ui_{};

    const char* shot_path_ = nullptr;
    float shot_at_ = 0.0f;
    preset preset_ = preset::scattered;

    bool running_ = true;
    bool draw_grid_ = true;
    bool draw_cells_ = true;
    bool draw_pairs_ = true;
    bool check_ = true;
    bool guard_ = true;
    bool ground_ = false;

    float t_ = 0.0f;
    float cell_ = 1.5f;
    int count_ = 256;
    int margin_step_ = 0;

    std::vector<mover> movers_;
    std::vector<proxy> proxies_;
    std::vector<broadphase_pair> reference_;
    std::vector<int> counts_;
    uniform_grid grid_;

    double grid_ms_ = 0.0;
    double brute_ms_ = 0.0;
    bool agree_ = true;
};

} // namespace

ENGINE_MAIN(broadphase_app)
