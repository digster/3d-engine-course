// demos/collide/main.cpp — fifteen bars, and the one that crosses zero.
//
// Lesson 8.4. The Separating Axis Theorem is not hard to state and it is very
// hard to *believe* from a statement, because the claim it makes is negative:
// these fifteen directions found no gap, therefore no direction has one. This
// demo exists to make that claim watchable.
//
//   LEFT PANEL   two boxes, wireframe, orthographic. The second one drifts
//                through the first and back out. When they overlap it turns red
//                and the minimum translation vector is drawn from its centre —
//                the shortest push that would end the overlap.
//   RIGHT PANEL  all fifteen candidate axes, as bars. Each bar is that axis's
//                gap: to the right of the centre line means "this direction
//                separates them", to the left means "it does not". The bar in
//                amber is the one the algorithm returned.
//
// WHAT TO WATCH FOR. Drive the boxes apart slowly and every bar creeps to the
// right together; the answer flips the instant the FIRST one crosses the line,
// and which one that is changes with the angle. Then press [F], which throws
// away the nine edge-edge candidates and keeps the six face normals — the
// mistake this lesson is built around. In most positions nothing changes. In the
// crossed-planks preset the boxes are visibly apart and the face-only test says
// they are touching, because the only direction that sees the gap is the cross
// product of an edge from each, and that direction is not a face normal of
// anything.
//
//   [1] [2] [3]   presets: crates, crossed planks, a crate on a slab
//   [Left/Right]  move the second box along x     [Up/Down]  along y
//   [,] [.]       along z
//   [Q] [E]       yaw it                          [Z] [C]    tilt it
//   [F]           face normals only — the wrong test, on purpose
//   [Space]       pause the drift                 [0] reset      [Esc] quit
//
//     cmake --build build --target collide
//     ./build/demos/collide
//     ./build/demos/collide --preset 1 --t 2.5 --shot out.ppm     headless
//
// THE RULES ARE 5.1'S. Nothing here includes an engine internal, and it does not
// link `demo_common`: a program written to exercise one subsystem has no
// business reaching for a shared scene.

#include <engine/core/fixed_step.hpp>
#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/collide.hpp>
#include <engine/phys/shape.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>
#include <engine/ui/debug_ui.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {

using engine::quat;
using engine::vec3;
using engine::phys::axis_source;
using engine::phys::box_shape;
using engine::phys::gap_on_axis;
using engine::phys::k_parallel_sin2;
using engine::phys::obb;
using engine::phys::separation;
using engine::phys::source_of;
using engine::phys::world_obb;

constexpr int k_width = 960;
constexpr int k_height = 540;
constexpr float k_pi = std::numbers::pi_v<float>;

/// This demo's own log category, in the range Lesson 5.3 reserved for programs
/// outside the library.
constexpr engine::log_category log_demo =
    static_cast<engine::log_category>(engine::log_category_count);

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_grid       = engine::pack_argb(38, 42, 52);
constexpr Uint32 k_axis       = engine::pack_argb(84, 92, 108);

/// Box A is always this colour. Box B is green while it is clear and red while
/// it is overlapping, which is the one thing in the left panel a reader can
/// check without reading a number.
constexpr Uint32 k_box_a      = engine::pack_argb(150, 160, 255);
constexpr Uint32 k_box_b_free = engine::pack_argb(120, 220, 160);
constexpr Uint32 k_box_b_hit  = engine::pack_argb(235, 96, 96);

/// The winning axis, and the minimum translation drawn along it. Amber, and
/// deliberately not one of the box colours: it is a direction rather than an
/// object.
constexpr Uint32 k_winner     = engine::pack_argb(235, 200, 96);

/// A candidate that separates, and one that does not.
constexpr Uint32 k_bar_apart  = engine::pack_argb(120, 220, 160);
constexpr Uint32 k_bar_over   = engine::pack_argb(96, 104, 124);

/// A candidate the degeneracy guard skipped. Two crates on the same floor have
/// a parallel edge pair at every yaw, so this is not a rare colour to see.
constexpr Uint32 k_bar_skip   = engine::pack_argb(60, 64, 78);

constexpr int k_left_x0 = 24,  k_left_y0 = 64,  k_left_x1 = 560, k_left_y1 = 500;
constexpr int k_bars_x0 = 596, k_bars_y0 = 64,  k_bars_x1 = 936, k_bars_y1 = 500;

/// Metres across the left panel.
constexpr float k_left_span = 9.0f;

/// Metres across the full width of the bar panel: a gap of +/- this reaches the
/// edge. The bars are CLAMPED rather than scaled to fit, because a fixed scale
/// is what makes "it crossed the line" mean the same thing from frame to frame.
constexpr float k_bar_span = 2.0f;

// ---- Clipping, inherited verbatim from demos/integrate, bodies and spin ----
//
// `engine::draw_line` deliberately does not clip — Lesson 2.1 says so in a
// comment — and this demo has two panels side by side, so a line leaving one of
// them would be drawn straight across the other.

int outcode(int x, int y, int bx0, int by0, int bx1, int by1)
{
    int c = 0;
    if (x < bx0) { c |= 1; } else if (x > bx1) { c |= 2; }
    if (y < by0) { c |= 4; } else if (y > by1) { c |= 8; }
    return c;
}

bool clip_to(int& x0, int& y0, int& x1, int& y1, int bx0, int by0, int bx1, int by1)
{
    int c0 = outcode(x0, y0, bx0, by0, bx1, by1);
    int c1 = outcode(x1, y1, bx0, by0, bx1, by1);

    for (int guard = 0; guard < 8; ++guard)
    {
        if ((c0 | c1) == 0) { return true; }
        if ((c0 & c1) != 0) { return false; }

        const int c = (c0 != 0) ? c0 : c1;
        int x = 0;
        int y = 0;
        const int dx = x1 - x0;
        const int dy = y1 - y0;

        if ((c & 8) != 0)      { x = x0 + dx * (by1 - y0) / ((dy != 0) ? dy : 1); y = by1; }
        else if ((c & 4) != 0) { x = x0 + dx * (by0 - y0) / ((dy != 0) ? dy : 1); y = by0; }
        else if ((c & 2) != 0) { y = y0 + dy * (bx1 - x0) / ((dx != 0) ? dx : 1); x = bx1; }
        else                   { y = y0 + dy * (bx0 - x0) / ((dx != 0) ? dx : 1); x = bx0; }

        if (c == c0) { x0 = x; y0 = y; c0 = outcode(x0, y0, bx0, by0, bx1, by1); }
        else         { x1 = x; y1 = y; c1 = outcode(x1, y1, bx0, by0, bx1, by1); }
    }
    return false;
}

void line_in(engine::framebuffer& f, int x0, int y0, int x1, int y1,
             int bx0, int by0, int bx1, int by1, Uint32 colour, bool thick = false)
{
    int a0 = x0;
    int b0 = y0;
    int a1 = x1;
    int b1 = y1;
    if (clip_to(a0, b0, a1, b1, bx0, by0, bx1, by1))
    {
        engine::draw_line(f, a0, b0, a1, b1, colour);
        if (thick) { engine::draw_line(f, a0, b0 + 1, a1, b1 + 1, colour); }
    }
}

void frame_box(engine::framebuffer& f, int x0, int y0, int x1, int y1)
{
    engine::draw_line(f, x0, y0, x1, y0, k_axis);
    engine::draw_line(f, x0, y1, x1, y1, k_axis);
    engine::draw_line(f, x0, y0, x0, y1, k_axis);
    engine::draw_line(f, x1, y0, x1, y1, k_axis);
}

void fill_rect(engine::framebuffer& f, int x0, int y0, int x1, int y1, Uint32 colour)
{
    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x) { f.put_pixel(x, y, colour); }
    }
}

/// The twelve edges of a box, by corner index.
///
/// `obb::corners` and `aabb::corners` agree on the ordering — bit 0 is the
/// first axis, bit 1 the second, bit 2 the third, and a set bit means the `+`
/// side — so an edge joins two corners whose indices differ in exactly one bit.
constexpr int k_edges[12][2] = {
    {0, 1}, {2, 3}, {4, 5}, {6, 7},     // along axis 0
    {0, 2}, {1, 3}, {4, 6}, {5, 7},     // along axis 1
    {0, 4}, {1, 5}, {2, 6}, {3, 7},     // along axis 2
};

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

class collide_app final : public engine::app
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
                // One named local per argument — Lesson 7.3 §14. `SDL_clamp`
                // expands its first argument three times and would eat the next
                // two flags if `++i` were written inline.
                const double wanted = SDL_atof(argv[++i]);
                shot_at_ = static_cast<float>(std::max(0.0, wanted));
            }
            else if (SDL_strcmp(argv[i], "--preset") == 0 && i + 1 < argc)
            {
                const int wanted = SDL_atoi(argv[++i]);
                preset_ = std::clamp(wanted, 0, 2);
            }
            else if (SDL_strcmp(argv[i], "--faces-only") == 0)
            {
                faces_only_ = true;
            }
            else if (SDL_strcmp(argv[i], "--still") == 0)
            {
                drifting_ = false;
            }
        }

        return {.title = "collide — fifteen candidate axes, live",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        SDL_SetLogPriority(log_demo, SDL_LOG_PRIORITY_INFO);
        reset();
        ENGINE_LOG_INFO(log_demo, "[1][2][3] preset  [F] face normals only  [Space] pause");
        ENGINE_LOG_INFO(log_demo, "arrows/,/. move  [Q][E] yaw  [Z][C] tilt  [0] reset");
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
        case SDL_SCANCODE_F: faces_only_ = !faces_only_; break;
        case SDL_SCANCODE_SPACE: drifting_ = !drifting_; break;
        case SDL_SCANCODE_0: reset(); break;

        case SDL_SCANCODE_LEFT:   nudge(vec3{-0.05f, 0.0f, 0.0f}); break;
        case SDL_SCANCODE_RIGHT:  nudge(vec3{+0.05f, 0.0f, 0.0f}); break;
        case SDL_SCANCODE_DOWN:   nudge(vec3{0.0f, -0.05f, 0.0f}); break;
        case SDL_SCANCODE_UP:     nudge(vec3{0.0f, +0.05f, 0.0f}); break;
        case SDL_SCANCODE_COMMA:  nudge(vec3{0.0f, 0.0f, -0.05f}); break;
        case SDL_SCANCODE_PERIOD: nudge(vec3{0.0f, 0.0f, +0.05f}); break;

        case SDL_SCANCODE_Q: spin(vec3{0.0f, 1.0f, 0.0f}, +0.05f); break;
        case SDL_SCANCODE_E: spin(vec3{0.0f, 1.0f, 0.0f}, -0.05f); break;
        case SDL_SCANCODE_Z: spin(vec3{1.0f, 0.0f, 0.0f}, +0.05f); break;
        case SDL_SCANCODE_C: spin(vec3{1.0f, 0.0f, 0.0f}, -0.05f); break;
        default: break;
        }
    }

    void on_input() override { ui_.begin_frame(); }

    void on_fixed_step(float h) override
    {
        if (!drifting_) { return; }
        t_ += h;
        // A slow sweep back and forth through contact, so that a reader who
        // touches nothing still sees the bars cross the line and come back.
        offset_ = drift_amplitude_ * std::sin(t_ * 0.55f);
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
        // GUARDED: a `--shot` run has no window and therefore no ImGui context.
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
        yaw_ = engine::quat_from_axis_angle(normalised(axis), radians) * yaw_;
    }

    void reset()
    {
        t_ = 0.0f;
        offset_ = 0.0f;
        manual_ = vec3{};
        drifting_ = true;

        switch (preset_)
        {
        case 0:   // two crates, one sliding into the other along its own x
            half_a_ = vec3{1.0f, 1.0f, 1.0f};
            half_b_ = vec3{0.8f, 0.8f, 0.8f};
            centre_a_ = vec3{-0.6f, 0.0f, 0.0f};
            base_b_ = vec3{2.2f, 0.35f, 0.2f};
            drift_ = normalised(vec3{-1.0f, 0.0f, 0.0f});
            drift_amplitude_ = 1.9f;
            yaw_ = engine::quat_from_axis_angle(normalised(vec3{0.2f, 1.0f, 0.15f}), 0.55f);
            break;

        case 1:
            // THE CROSSED PLANKS, and the placement is the argument rather than
            // an arrangement that happened to work. Pick the separating
            // direction FIRST — the cross product of one long edge from each —
            // then put the second plank along it, just far enough to clear both
            // shadows. Nothing about that placement involves a face normal, so
            // the only candidate that can see the gap is the edge-edge one.
            half_a_ = vec3{2.6f, 0.2f, 0.2f};
            half_b_ = vec3{2.6f, 0.2f, 0.2f};
            centre_a_ = vec3{};
            yaw_ = crossed_orientation();
            {
                const obb a = world_obb(box_shape(half_a_), centre_a_, quat::identity());
                const obb b = world_obb(box_shape(half_b_), vec3{}, yaw_);
                drift_ = normalised(cross(a.axis(0), b.axis(0)));
                const float reach = engine::phys::projected_radius(a, drift_) +
                                    engine::phys::projected_radius(b, drift_);
                base_b_ = drift_ * reach;
            }
            drift_amplitude_ = 0.22f;
            break;

        default:  // a crate settling onto a slab: the shared-axis case
            half_a_ = vec3{3.2f, 0.25f, 2.2f};
            half_b_ = vec3{0.7f, 0.7f, 0.7f};
            centre_a_ = vec3{0.0f, -1.4f, 0.0f};
            base_b_ = vec3{0.4f, 0.1f, 0.1f};
            drift_ = normalised(vec3{0.0f, -1.0f, 0.0f});
            drift_amplitude_ = 0.9f;
            yaw_ = engine::quat_from_axis_angle(vec3{0.0f, 1.0f, 0.0f}, 0.65f);
            break;
        }
    }

    /// The rotation that makes preset 1's second plank cross the first at a
    /// generic angle — generic so that no candidate axis coincides with another
    /// by accident, which is the trap 8.3 §6 fell into with 90° test data.
    [[nodiscard]] static quat crossed_orientation()
    {
        return engine::quat_from_axis_angle(normalised(vec3{1.0f, 2.0f, 3.0f}), 0.7f * k_pi);
    }

    [[nodiscard]] obb box_a() const
    {
        return world_obb(box_shape(half_a_), centre_a_, quat::identity());
    }

    [[nodiscard]] obb box_b() const
    {
        return world_obb(box_shape(half_b_), base_b_ + drift_ * offset_ + manual_, yaw_);
    }

    /// The fifteen candidate gaps, in the order `separation::axis_index` uses.
    /// A skipped degenerate axis is reported as a NaN so that the bar can be
    /// drawn differently rather than being drawn as a zero, which would read as
    /// "exactly touching".
    void candidate_gaps(const obb& a, const obb& b, float out[15]) const
    {
        for (int i = 0; i < 3; ++i) { out[i] = gap_on_axis(a, b, a.axis(i)); }
        for (int j = 0; j < 3; ++j) { out[3 + j] = gap_on_axis(a, b, b.axis(j)); }
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                const vec3 c = cross(a.axis(i), b.axis(j));
                const float len2 = length_squared(c);
                out[6 + 3 * i + j] = (len2 < k_parallel_sin2)
                                         ? std::numeric_limits<float>::quiet_NaN()
                                         : gap_on_axis(a, b, c / std::sqrt(len2));
            }
        }
    }

    /// Orthographic projection onto the left panel, looking down -z with a tilt
    /// so that all three axes are distinguishable.
    static void project(vec3 p, int& sx, int& sy)
    {
        constexpr float k_tilt = 0.42f;
        const float px = p.x + p.z * 0.55f * std::cos(k_tilt);
        const float py = p.y + p.z * 0.55f * std::sin(k_tilt);

        const int cx = (k_left_x0 + k_left_x1) / 2;
        const int cy = (k_left_y0 + k_left_y1) / 2;
        const float scale = static_cast<float>(k_left_x1 - k_left_x0) / k_left_span;

        sx = cx + static_cast<int>(px * scale);
        sy = cy - static_cast<int>(py * scale);
    }

    void draw_obb(engine::framebuffer& f, const obb& box, Uint32 colour) const
    {
        vec3 c[8];
        box.corners(c);
        for (const auto& e : k_edges)
        {
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            project(c[e[0]], x0, y0);
            project(c[e[1]], x1, y1);
            line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1, colour);
        }
    }

    void draw()
    {
        engine::framebuffer& f = fb();
        f.clear(k_background);

        // A one-metre grid on the left panel, so that a depth in metres has
        // something to be read against.
        const float scale = static_cast<float>(k_left_x1 - k_left_x0) / k_left_span;
        const int cx = (k_left_x0 + k_left_x1) / 2;
        const int cy = (k_left_y0 + k_left_y1) / 2;
        for (int m = -6; m <= 6; ++m)
        {
            const int gx = cx + static_cast<int>(static_cast<float>(m) * scale);
            const int gy = cy - static_cast<int>(static_cast<float>(m) * scale);
            line_in(f, gx, k_left_y0, gx, k_left_y1, k_left_x0, k_left_y0, k_left_x1,
                    k_left_y1, (m == 0) ? k_axis : k_grid);
            line_in(f, k_left_x0, gy, k_left_x1, gy, k_left_x0, k_left_y0, k_left_x1,
                    k_left_y1, (m == 0) ? k_axis : k_grid);
        }
        frame_box(f, k_left_x0, k_left_y0, k_left_x1, k_left_y1);
        frame_box(f, k_bars_x0, k_bars_y0, k_bars_x1, k_bars_y1);

        const obb a = box_a();
        const obb b = box_b();
        const separation s = engine::phys::collide(a, b);

        float gaps[15];
        candidate_gaps(a, b, gaps);

        // The face-only answer, which is the mistake this demo exists to make
        // visible: the six face normals, and nothing else.
        bool faces_say_apart = false;
        for (int i = 0; i < 6; ++i)
        {
            if (gaps[i] > 0.0f) { faces_say_apart = true; }
        }
        reported_hit_ = faces_only_ ? !faces_say_apart : s.hit();
        last_ = s;

        draw_obb(f, a, k_box_a);
        draw_obb(f, b, reported_hit_ ? k_box_b_hit : k_box_b_free);

        // The minimum translation, drawn from B's centre. When they are apart it
        // is the gap instead, drawn the other way, so the arrow always points
        // along the axis the algorithm chose.
        if (s.axis_index != -2)
        {
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            project(b.centre, x0, y0);
            project(b.centre + s.axis * std::fabs(s.depth), x1, y1);
            line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1, k_winner,
                    true);
        }

        draw_bars(f, gaps, s.axis_index);
    }

    void draw_bars(engine::framebuffer& f, const float gaps[15], int winner) const
    {
        const int zero_x = (k_bars_x0 + k_bars_x1) / 2;
        const int rows = 15;
        const int row_h = (k_bars_y1 - k_bars_y0) / rows;
        const float px_per_m =
            static_cast<float>(k_bars_x1 - k_bars_x0) * 0.5f / k_bar_span;

        // The zero line: the whole panel is a question about which side of this
        // a bar is on.
        line_in(f, zero_x, k_bars_y0, zero_x, k_bars_y1, k_bars_x0, k_bars_y0, k_bars_x1,
                k_bars_y1, k_axis);

        for (int i = 0; i < rows; ++i)
        {
            const int y0 = k_bars_y0 + i * row_h + 4;
            const int y1 = y0 + row_h - 8;

            // A separator above each of the three groups — A's faces, B's
            // faces, the nine edge pairs — because the grouping is the argument
            // and an undifferentiated stack of fifteen bars hides it.
            if (i == 3 || i == 6)
            {
                line_in(f, k_bars_x0, y0 - 3, k_bars_x1, y0 - 3, k_bars_x0, k_bars_y0,
                        k_bars_x1, k_bars_y1, k_grid);
            }

            const float g = gaps[i];
            if (std::isnan(g))
            {
                // A skipped candidate: drawn as a stub at the line rather than
                // as a zero-length bar, which would claim "exactly touching".
                fill_rect(f, zero_x - 3, y0 + (y1 - y0) / 2 - 1, zero_x + 3,
                          y0 + (y1 - y0) / 2 + 1, k_bar_skip);
                continue;
            }

            const float clamped = std::clamp(g, -k_bar_span, k_bar_span);
            const int end_x = zero_x + static_cast<int>(clamped * px_per_m);
            const int bx0 = std::min(zero_x, end_x);
            const int bx1 = std::max(zero_x, end_x);
            const Uint32 colour = (i == winner) ? k_winner
                                                : (g > 0.0f ? k_bar_apart : k_bar_over);
            fill_rect(f, std::max(bx0, k_bars_x0 + 1), y0, std::min(bx1, k_bars_x1 - 1), y1,
                      colour);
        }
    }

    void build_panel()
    {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420.0f, 300.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("collide");

        static const char* const k_preset_name[3] = {"two crates", "crossed planks",
                                                     "crate on a slab"};
        ImGui::Text("preset [1..3]  %s", k_preset_name[preset_]);
        ImGui::Text("test           %s", faces_only_ ? "SIX FACE NORMALS (wrong)"
                                                     : "all fifteen candidates");
        ImGui::Separator();

        ImGui::Text("verdict        %s", reported_hit_ ? "OVERLAPPING" : "apart");
        if (last_.hit())
        {
            ImGui::Text("depth          %.4f m", static_cast<double>(last_.depth));
        }
        else
        {
            ImGui::Text("gap            %.4f m", static_cast<double>(-last_.depth));
        }
        ImGui::Text("axis           %d (%s)", last_.axis_index,
                    engine::phys::name_of(source_of(last_)));
        ImGui::Text("               (%.3f, %.3f, %.3f)", static_cast<double>(last_.axis.x),
                    static_cast<double>(last_.axis.y), static_cast<double>(last_.axis.z));
        ImGui::Text("axes examined  %d of 15", last_.axes_tested);

        if (faces_only_ && reported_hit_ != last_.hit())
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f),
                               "The six face normals disagree with the answer.");
            ImGui::TextWrapped("They are apart, and the only direction that can see the "
                               "gap is the cross product of an edge from each box. That "
                               "direction is not a face normal of anything.");
        }

        ImGui::Separator();
        ImGui::TextWrapped("Right panel: one bar per candidate axis, its gap in metres. "
                           "Right of the line separates. Amber is the axis returned; a "
                           "stub on the line is a degenerate candidate that was skipped.");
        ImGui::End();
    }

    engine::debug_ui ui_{};

    int preset_ = 0;
    bool faces_only_ = false;
    bool drifting_ = true;
    bool reported_hit_ = false;

    float t_ = 0.0f;
    float offset_ = 0.0f;
    float drift_amplitude_ = 1.0f;

    vec3 half_a_{1.0f, 1.0f, 1.0f};
    vec3 half_b_{0.8f, 0.8f, 0.8f};
    vec3 centre_a_{};
    vec3 base_b_{};
    vec3 drift_{1.0f, 0.0f, 0.0f};
    vec3 manual_{};
    quat yaw_{};

    separation last_{};

    const char* shot_path_ = nullptr;
    float shot_at_ = 1.0f;
};

}   // namespace

ENGINE_MAIN(collide_app)
