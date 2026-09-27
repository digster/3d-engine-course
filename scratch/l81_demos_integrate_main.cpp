// demos/integrate/main.cpp — three integrators on one spring, drawn in the
// space where the difference is visible.
//
// Lesson 8.1. The picture is a PHASE PLOT, not a trajectory: horizontal is
// position, vertical is velocity, and one lap round the loop is one oscillation.
// A spring's exact motion is a circle in that space, and the entire content of
// this lesson is what each rule does to the circle. Explicit Euler spirals
// outward and never stops. Semi-implicit Euler traces a tilted ellipse and
// stays on it forever. Velocity Verlet traces a rounder one and also stays.
//
// Watching it as a bouncing dot tells you nothing — all three look like a spring
// for the first few seconds, which is precisely why this bug ships.
//
//   [1] [2] [3]   toggle explicit / semi-implicit / verlet
//   [Up] [Down]   change the simulation rate (h)
//   [Left][Right] change the spring stiffness (w)
//   [S]           show/hide the predicted shadow ellipse
//   [R]           reset      [Space] pause      [Esc] quit
//
//     cmake --build build --target integrate
//     ./build/demos/integrate
//     ./build/demos/integrate --t 6 --shot out.ppm       headless
//
// THE DEMO RUNS ITS OWN `fixed_step`, nested inside the application's. That
// looks redundant and is the point: the app's step rate is a rendering decision
// and the simulation's step rate is the variable under study, and Lesson 1.4's
// class is reused unchanged to make exactly that separation. Pressing [Up]
// changes what physics does, not how often the screen updates.
//
// THE RULES ARE 5.1'S. Nothing here includes an engine internal, and it does not
// link `demo_common`: a program written to exercise one subsystem has no
// business reaching for a shared scene.

#include <engine/core/fixed_step.hpp>
#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/integrate.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>
#include <engine/ui/debug_ui.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace {

using engine::vec3;
using engine::phys::area_factor;
using engine::phys::integrate;
using engine::phys::integrator;
using engine::phys::max_stable_step;
using engine::phys::motion;
using engine::phys::name_of;
using engine::phys::shadow_energy;
using engine::phys::spring_energy;

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
constexpr Uint32 k_exact      = engine::pack_argb(150, 158, 175);
constexpr Uint32 k_shadow     = engine::pack_argb(96, 104, 124);

/// One colour per rule. NOT the axis colours from conventions.html §10 — the two
/// axes in this picture are position and velocity, which are not world axes, and
/// reusing red/green/blue for methods here would say something false about them.
constexpr Uint32 k_colour[3] = {
    engine::pack_argb(235, 96, 96),    // explicit Euler
    engine::pack_argb(120, 220, 160),  // semi-implicit Euler
    engine::pack_argb(170, 150, 255),  // velocity Verlet
};

const integrator k_rules[3] = {integrator::explicit_euler,
                               integrator::semi_implicit_euler,
                               integrator::velocity_verlet};

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

/// The phase plot: a square, because a circle has to look like a circle. The
/// vertical axis is v/w rather than v, which is what makes that true — position
/// is in metres and velocity in metres per second, so plotting them against each
/// other needs a length scale, and the spring supplies exactly one.
constexpr int k_plot_cx = 268;
constexpr int k_plot_cy = 276;
constexpr int k_plot_r  = 214;

/// The energy strip, on the right: log10(E/E0) against simulated time.
constexpr int k_strip_x0 = 546;
constexpr int k_strip_x1 = 936;
constexpr int k_strip_y0 = 96;
constexpr int k_strip_y1 = 456;
constexpr float k_strip_decades = 4.0f;   // log10 range, 0 at the bottom

/// How many phase-space samples to keep per rule. 1,200 is about twenty seconds
/// at 60 Hz, which is long enough for the explicit spiral to be unmistakable and
/// short enough that the tail does not bury the head.
constexpr std::size_t k_trail = 1200;

struct track
{
    motion state{};
    bool on = true;
    std::vector<float> px;     // phase-plot coordinates, already scaled
    std::vector<float> pv;
    std::vector<float> energy; // log10(E/E0), one per strip sample
};

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

void plot_to_screen(float x, float v_over_w, float metres, int& sx, int& sy)
{
    const float s = static_cast<float>(k_plot_r) / metres;
    sx = k_plot_cx + static_cast<int>(std::lround(x * s));
    sy = k_plot_cy - static_cast<int>(std::lround(v_over_w * s));
}

/// The phase plot's panel, in pixels, as (x0, y0, x1, y1) inclusive.
constexpr int k_panel[4] = {k_plot_cx - k_plot_r - 24, k_plot_cy - k_plot_r - 24,
                            k_plot_cx + k_plot_r + 24, k_plot_cy + k_plot_r + 24};

/// Cohen-Sutherland, clipping a segment to the phase panel before it is drawn.
///
/// Lesson 2.1 left this as an exercise and Module 3 made it non-optional for
/// geometry behind the camera; here it is non-optional for a different reason.
/// A diverging explicit Euler leaves the panel after about four seconds, and
/// `draw_line` does not clip — it walks every pixel and lets `put_pixel` discard
/// the ones outside the framebuffer, which is correct but does nothing to stop
/// the red spiral from being drawn straight across the energy chart next door.
/// Returns false when the segment misses the panel entirely.
bool clip_to_panel(int& x0, int& y0, int& x1, int& y1)
{
    auto code = [](int x, int y) {
        int c = 0;
        if (x < k_panel[0]) { c |= 1; } else if (x > k_panel[2]) { c |= 2; }
        if (y < k_panel[1]) { c |= 4; } else if (y > k_panel[3]) { c |= 8; }
        return c;
    };

    int c0 = code(x0, y0);
    int c1 = code(x1, y1);
    for (int guard = 0; guard < 8; ++guard)
    {
        if ((c0 | c1) == 0) { return true; }      // both inside
        if ((c0 & c1) != 0) { return false; }     // both outside the same edge

        const int c = (c0 != 0) ? c0 : c1;
        const double dx = static_cast<double>(x1 - x0);
        const double dy = static_cast<double>(y1 - y0);
        int nx = x0;
        int ny = y0;
        if ((c & 8) != 0)
        {
            nx = x0 + static_cast<int>(std::lround(dx * (k_panel[3] - y0) / dy));
            ny = k_panel[3];
        }
        else if ((c & 4) != 0)
        {
            nx = x0 + static_cast<int>(std::lround(dx * (k_panel[1] - y0) / dy));
            ny = k_panel[1];
        }
        else if ((c & 2) != 0)
        {
            ny = y0 + static_cast<int>(std::lround(dy * (k_panel[2] - x0) / dx));
            nx = k_panel[2];
        }
        else
        {
            ny = y0 + static_cast<int>(std::lround(dy * (k_panel[0] - x0) / dx));
            nx = k_panel[0];
        }

        if (c == c0) { x0 = nx; y0 = ny; c0 = code(x0, y0); }
        else         { x1 = nx; y1 = ny; c1 = code(x1, y1); }
    }
    return false;
}

/// `draw_line`, but clipped to the phase panel first.
void draw_clipped(engine::framebuffer& f, int x0, int y0, int x1, int y1, Uint32 colour)
{
    if (clip_to_panel(x0, y0, x1, y1)) { engine::draw_line(f, x0, y0, x1, y1, colour); }
}

void draw_circle(engine::framebuffer& f, int cx, int cy, int r, Uint32 colour)
{
    constexpr int k_segments = 128;
    int px = 0;
    int py = 0;
    for (int i = 0; i <= k_segments; ++i)
    {
        const float a = 2.0f * k_pi * static_cast<float>(i) / k_segments;
        const int sx = cx + static_cast<int>(std::lround(std::cos(a) * r));
        const int sy = cy + static_cast<int>(std::lround(std::sin(a) * r));
        if (i > 0) { draw_clipped(f, px, py, sx, sy, colour); }
        px = sx;
        py = sy;
    }
}

/// The level set of the quantity semi-implicit Euler conserves exactly.
///
/// In plot coordinates p = w*x and q = v the conserved form is
/// `q^2 + p^2 - (h*w)*p*q`, which is a quadratic whose principal axes lie at 45
/// degrees, with semi-axes scaled by 1/sqrt(1 -+ h*w/2). Drawing it is the whole
/// of §4.4 made visible: the green dot is not merely bounded, it is ON this
/// curve, and it never leaves.
void draw_shadow_ellipse(engine::framebuffer& f, float q0, float hw, float omega,
                         float metres)
{
    if (hw >= 2.0f) { return; }  // no bounded level set to draw
    const float a = std::sqrt(q0 / (1.0f - hw * 0.5f));   // along (1, 1)/sqrt2
    const float b = std::sqrt(q0 / (1.0f + hw * 0.5f));   // along (1,-1)/sqrt2
    constexpr int k_segments = 160;
    constexpr float k_inv_root2 = 0.70710678f;
    int px = 0;
    int py = 0;
    for (int i = 0; i <= k_segments; ++i)
    {
        const float t = 2.0f * k_pi * static_cast<float>(i) / k_segments;
        const float s = a * std::cos(t);
        const float u = b * std::sin(t);
        // Back to PLOT coordinates, which are (x, v/w) and not (w*x, v): both
        // components carry one factor of w and the plot carries none. Forgetting
        // that divide drew an ellipse six times too big, straight across the
        // energy strip, which is at least a legible way to be wrong.
        const float p = (s + u) * k_inv_root2 / omega;
        const float q = (s - u) * k_inv_root2 / omega;
        int sx = 0;
        int sy = 0;
        plot_to_screen(p, q, metres, sx, sy);
        if (i > 0) { draw_clipped(f, px, py, sx, sy, k_shadow); }
        px = sx;
        py = sy;
    }
}

void draw_blob(engine::framebuffer& f, int sx, int sy, int r, Uint32 colour)
{
    for (int dy = -r; dy <= r; ++dy)
    {
        const int half = static_cast<int>(std::lround(std::sqrt(
            std::max(0.0f, static_cast<float>(r * r - dy * dy)))));
        engine::draw_line(f, sx - half, sy + dy, sx + half, sy + dy, colour);
    }
}

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

class integrate_app final : public engine::app
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
            else if (SDL_strcmp(argv[i], "--rate") == 0 && i + 1 < argc)
            {
                const double wanted = SDL_atof(argv[++i]);
                rate_ = static_cast<float>(std::clamp(wanted, 4.0, 2000.0));
            }
        }

        return {.title = "integrate — three rules, one spring",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        SDL_SetLogPriority(log_demo, SDL_LOG_PRIORITY_INFO);
        reset();
        ENGINE_LOG_INFO(log_demo, "[1][2][3] rules  [Up][Down] rate  [<][>] stiffness");
        ENGINE_LOG_INFO(log_demo, "[S] shadow ellipse  [R] reset  [Space] pause  [Esc] quit");
        return ui_.start(window(), renderer()) || shot_path_ != nullptr;
    }

    void on_event(const SDL_Event& event) override
    {
        (void)ui_.handle_event(event);
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) { return; }

        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_1: tracks_[0].on = !tracks_[0].on; break;
        case SDL_SCANCODE_2: tracks_[1].on = !tracks_[1].on; break;
        case SDL_SCANCODE_3: tracks_[2].on = !tracks_[2].on; break;
        case SDL_SCANCODE_S: shadow_ = !shadow_; break;
        case SDL_SCANCODE_R: reset(); break;
        case SDL_SCANCODE_SPACE: paused_ = !paused_; break;
        case SDL_SCANCODE_UP:    set_rate(rate_ * 2.0f); break;
        case SDL_SCANCODE_DOWN:  set_rate(rate_ * 0.5f); break;
        case SDL_SCANCODE_RIGHT: set_omega(omega_ * 1.25f); break;
        case SDL_SCANCODE_LEFT:  set_omega(omega_ / 1.25f); break;
        default: break;
        }
    }

    void on_input() override { ui_.begin_frame(); }

    void on_fixed_step(float h) override
    {
        if (paused_) { return; }

        // The nested accumulator. `h` here is the application's step — 60 Hz,
        // fixed by the platform layer — and `sim_` dices it again at whatever
        // rate this demo is studying. Both are Lesson 1.4's class; only the
        // constructor argument differs.
        sim_.begin_frame(h);
        while (sim_.next_step()) { advance(sim_.h()); }
    }

    void on_frame(float alpha) override
    {
        (void)alpha;
        draw();
        if (shot_path_ != nullptr && sim_t_ >= shot_at_)
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
    void set_rate(float hz)
    {
        rate_ = std::clamp(hz, 4.0f, 2000.0f);
        sim_.set_rate(rate_);
        reset();
    }

    void set_omega(float w)
    {
        omega_ = std::clamp(w, 0.5f, 80.0f);
        reset();
    }

    void reset()
    {
        sim_ = engine::fixed_step{rate_, 512};
        sim_t_ = 0.0f;
        e0_ = 0.5f * omega_ * omega_ * k_amplitude * k_amplitude;
        for (track& t : tracks_)
        {
            t.state = {.position = vec3{k_amplitude, 0.0f, 0.0f}, .velocity = vec3{}};
            t.px.clear();
            t.pv.clear();
            t.energy.clear();
        }
        strip_t_ = 0.0f;
    }

    void advance(float h)
    {
        const float w2 = omega_ * omega_;
        const auto accel = [w2](vec3 x) { return x * -w2; };

        for (std::size_t i = 0; i < 3; ++i)
        {
            track& t = tracks_[i];
            if (!t.on) { continue; }
            integrate(t.state, accel, h, k_rules[i]);

            push(t.px, t.state.position.x);
            push(t.pv, t.state.velocity.x / omega_);
        }
        sim_t_ += h;

        // The strip is sampled on its own clock so that raising the simulation
        // rate does not compress the time axis — the chart is about SIMULATED
        // seconds, and at 480 Hz there are eight samples per 60 Hz frame.
        if (sim_t_ >= strip_t_)
        {
            strip_t_ += k_strip_seconds / static_cast<float>(k_strip_x1 - k_strip_x0);
            for (track& t : tracks_)
            {
                const float e = spring_energy(t.state, omega_) / e0_;
                t.energy.push_back(std::log10(std::max(e, 1.0e-9f)));
                if (t.energy.size() > static_cast<std::size_t>(k_strip_x1 - k_strip_x0))
                {
                    t.energy.erase(t.energy.begin());
                }
            }
        }
    }

    static void push(std::vector<float>& v, float value)
    {
        v.push_back(value);
        if (v.size() > k_trail) { v.erase(v.begin()); }
    }

    void draw()
    {
        engine::framebuffer& f = fb();
        f.clear(k_background);

        // ---- phase plot ---------------------------------------------------
        //
        // The view scale follows the worst track, UP TO A CEILING of three
        // amplitudes. Without the ceiling a diverging explicit Euler pulls the
        // camera back without limit and every other curve collapses to a dot
        // within about ten seconds — the picture ends up showing only the thing
        // that is broken, at the exact moment you want to compare it against the
        // things that are not. Past the ceiling the red curve simply leaves,
        // which is a more honest drawing of what it does anyway.
        float metres = k_amplitude * 1.35f;
        for (const track& t : tracks_)
        {
            if (!t.on) { continue; }
            metres = std::max(metres, std::fabs(t.state.position.x) * 1.15f);
            metres = std::max(metres, std::fabs(t.state.velocity.x / omega_) * 1.15f);
        }
        view_metres_ = std::min(metres, k_amplitude * 3.0f);
        metres = view_metres_;

        engine::draw_line(f, k_panel[0], k_plot_cy, k_panel[2], k_plot_cy, k_axis);
        engine::draw_line(f, k_plot_cx, k_panel[1], k_plot_cx, k_panel[3], k_axis);
        for (int corner = 0; corner < 4; ++corner)
        {
            // Just the corners, not a full box: a border drawn all the way round
            // reads as a frame the curves are contained BY, and the point of the
            // clip is that one of them is not contained at all.
            const int cx = (corner & 1) ? k_panel[2] : k_panel[0];
            const int cy = (corner & 2) ? k_panel[3] : k_panel[1];
            const int dx = (corner & 1) ? -16 : 16;
            const int dy = (corner & 2) ? -16 : 16;
            engine::draw_line(f, cx, cy, cx + dx, cy, k_grid);
            engine::draw_line(f, cx, cy, cx, cy + dy, k_grid);
        }

        // The exact orbit: a circle of radius `k_amplitude`. Every rule should
        // sit on it and the whole lesson is which ones do.
        const int exact_r = static_cast<int>(std::lround(
            static_cast<double>(k_plot_r) * k_amplitude / metres));
        draw_circle(f, k_plot_cx, k_plot_cy, exact_r, k_exact);

        if (shadow_ && tracks_[1].on)
        {
            draw_shadow_ellipse(f, omega_ * omega_ * k_amplitude * k_amplitude,
                                omega_ / rate_, omega_, metres);
        }

        for (std::size_t i = 0; i < 3; ++i)
        {
            const track& t = tracks_[i];
            if (!t.on || t.px.empty()) { continue; }
            int lx = 0;
            int ly = 0;
            for (std::size_t k = 0; k < t.px.size(); ++k)
            {
                int sx = 0;
                int sy = 0;
                plot_to_screen(t.px[k], t.pv[k], metres, sx, sy);
                if (k > 0) { draw_clipped(f, lx, ly, sx, sy, k_colour[i]); }
                lx = sx;
                ly = sy;
            }
            // The head dot only when the body is still on screen, so a
            // departed track does not leave a marker pinned to the border.
            if (lx >= k_panel[0] && lx <= k_panel[2] && ly >= k_panel[1] && ly <= k_panel[3])
            {
                draw_blob(f, lx, ly, 4, k_colour[i]);
            }
        }

        // ---- energy strip ---------------------------------------------------
        engine::draw_line(f, k_strip_x0, k_strip_y0, k_strip_x0, k_strip_y1, k_axis);
        engine::draw_line(f, k_strip_x0, k_strip_y1, k_strip_x1, k_strip_y1, k_axis);
        for (int d = 0; d <= static_cast<int>(k_strip_decades); ++d)
        {
            const int y = strip_y(static_cast<float>(d));
            engine::draw_line(f, k_strip_x0, y, k_strip_x1, y, k_grid);
        }
        // The line at log10 = 0, which is "the energy it started with" and is
        // the only value a correct integrator is allowed to sit on.
        engine::draw_line(f, k_strip_x0, strip_y(0.0f), k_strip_x1, strip_y(0.0f), k_exact);

        for (std::size_t i = 0; i < 3; ++i)
        {
            const track& t = tracks_[i];
            if (!t.on || t.energy.empty()) { continue; }
            int lx = 0;
            int ly = 0;
            for (std::size_t k = 0; k < t.energy.size(); ++k)
            {
                const int sx = k_strip_x0 + static_cast<int>(k);
                const int sy = strip_y(t.energy[k]);
                if (k > 0) { engine::draw_line(f, lx, ly, sx, sy, k_colour[i]); }
                lx = sx;
                ly = sy;
            }
        }
    }

    [[nodiscard]] static int strip_y(float decades)
    {
        const float t = std::clamp(decades / k_strip_decades, -0.02f, 1.0f);
        return k_strip_y1 - static_cast<int>(std::lround(
            t * static_cast<double>(k_strip_y1 - k_strip_y0)));
    }

    void build_panel()
    {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("integrate")) { ImGui::End(); return; }

        const float h = 1.0f / rate_;
        ImGui::Text("h = 1/%.0f s = %.6f   w = %.3f rad/s (%.2f Hz)",
                    static_cast<double>(rate_), static_cast<double>(h),
                    static_cast<double>(omega_),
                    static_cast<double>(omega_ / (2.0f * k_pi)));
        ImGui::Text("h*w = %.4f      limit 2.0      steps/period %.1f",
                    static_cast<double>(h * omega_),
                    static_cast<double>(2.0f * k_pi / (h * omega_)));
        ImGui::Text("max_stable_step(semi-impl) = %.5f s  (%.0f Hz)",
                    static_cast<double>(max_stable_step(integrator::semi_implicit_euler,
                                                        omega_)),
                    static_cast<double>(omega_ / 2.0f));
        ImGui::Text("simulated t = %.2f s   view +/-%.3f m", static_cast<double>(sim_t_),
                    static_cast<double>(view_metres_));
        ImGui::Separator();

        if (ImGui::BeginTable("rules", 5, ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("rule");
            ImGui::TableSetupColumn("det");
            ImGui::TableSetupColumn("E/E0");
            ImGui::TableSetupColumn("shadow");
            ImGui::TableSetupColumn("x (m)");
            ImGui::TableHeadersRow();
            for (std::size_t i = 0; i < 3; ++i)
            {
                const track& t = tracks_[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(colour_of(i), "%s", name_of(k_rules[i]));
                ImGui::TableNextColumn();
                ImGui::Text("%.7f", static_cast<double>(area_factor(k_rules[i], omega_, h)));
                ImGui::TableNextColumn();
                ImGui::Text("%.4g", static_cast<double>(spring_energy(t.state, omega_) / e0_));
                ImGui::TableNextColumn();
                ImGui::Text("%.4g", static_cast<double>(shadow_energy(t.state, omega_, h)));
                ImGui::TableNextColumn();
                ImGui::Text("%.4g", static_cast<double>(t.state.position.x));
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::TextWrapped(
            "Left: phase space (x horizontal, v/w vertical). The pale circle is "
            "the exact orbit; the pale ellipse is what semi-implicit Euler "
            "conserves exactly. Right: energy, log10, relative to the start.");
        ImGui::Text("[1][2][3] rules  [Up][Down] rate  [<][>] w  [S] shadow");
        ImGui::Text("[R] reset  [Space] %s  [Esc] quit", paused_ ? "resume" : "pause");
        ImGui::End();
    }

    [[nodiscard]] static ImVec4 colour_of(std::size_t i)
    {
        const Uint32 c = k_colour[i];
        return ImVec4(static_cast<float>((c >> 16) & 0xFFu) / 255.0f,
                      static_cast<float>((c >> 8) & 0xFFu) / 255.0f,
                      static_cast<float>(c & 0xFFu) / 255.0f, 1.0f);
    }

    static constexpr float k_amplitude = 1.0f;
    static constexpr float k_strip_seconds = 10.0f;

    engine::debug_ui ui_;
    engine::fixed_step sim_{60.0f, 512};
    track tracks_[3];

    float rate_ = 60.0f;
    float omega_ = 2.0f * k_pi;
    float sim_t_ = 0.0f;
    float strip_t_ = 0.0f;
    float e0_ = 1.0f;
    float view_metres_ = 1.0f;
    bool paused_ = false;
    bool shadow_ = true;

    const char* shot_path_ = nullptr;
    float shot_at_ = 0.0f;
};

} // namespace

ENGINE_MAIN(integrate_app)
