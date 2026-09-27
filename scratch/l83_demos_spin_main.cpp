// demos/spin/main.cpp — a box that flips end over end with nothing touching it.
//
// Lesson 8.3. The demo is built around the one claim in this lesson that has to
// be WATCHED rather than read: an asymmetric rigid body, thrown with no torque
// on it at all, does not spin about a fixed axis. It tumbles — and if you spin
// it about its middle axis it periodically flips over, forever, with nothing
// acting on it.
//
// That is the Dzhanibekov effect, named after the cosmonaut who noticed a
// wing-nut doing it in orbit in 1985 and reportedly assumed something was wrong
// with the spacecraft. It is a consequence of the inertia tensor having three
// different eigenvalues, and it falls out of the one term in Euler's equations
// that most engines drop.
//
//   LEFT PANEL   the box itself, wireframe, orthographic, with its three body
//                axes drawn in the course's x/y/z = red/green/blue.
//   RIGHT PANEL  the angular velocity in BODY axes, plotted against time. The
//                flip is the green trace crossing zero and coming back up the
//                other side, and the two pulses either side of it are the
//                perturbation growing and collapsing.
//
// FOUR MODES, ON ONE KEY, and the demo exists to make the difference between
// them visible rather than tabular:
//
//   OFF        omega never changes. The box spins about a fixed world axis like
//              a badly animated prop. This is what most engines ship.
//   EXPLICIT   the term, added explicitly. It flips — and it also gains energy
//              until the box is spinning too fast to look at. At 30 Hz it
//              diverges outright.
//   IMPLICIT   the term, solved with one Newton iteration. Stable, and visibly
//              slowing down: it loses a third of its angular momentum a minute.
//   MOMENTUM   integrate L, derive omega. The one that is right.
//
//   [G]           cycle the four modes
//   [1] [2] [3]   spin about x (largest), y (intermediate), z (smallest)
//   [E]           linearised / exponential orientation update
//   [Up] [Down]   change the simulation rate
//   [R] reset     [Space] pause     [Esc] quit
//
//     cmake --build build --target spin
//     ./build/demos/spin
//     ./build/demos/spin --t 8 --shot out.ppm       headless
//
// THERE IS NO GRAVITY AND NO FLOOR, and that is the experiment rather than a
// missing feature: every claim in this demo is about a body with NOTHING acting
// on it. A gravity that pulled on the centre of mass would not change the
// tumble at all — its lever arm is zero — but it would carry the box out of the
// panel, and a reader would reasonably wonder whether the falling was doing it.
//
// THE RULES ARE 5.1'S. Nothing here includes an engine internal, and it does not
// link `demo_common`: a program written to exercise one subsystem has no
// business reaching for a shared scene.

#include <engine/core/fixed_step.hpp>
#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/mat3.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/inertia.hpp>
#include <engine/phys/rigid_body.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>
#include <engine/ui/debug_ui.hpp>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace {

using engine::mat3;
using engine::quat;
using engine::vec3;
using engine::phys::angular_momentum;
using engine::phys::body_id;
using engine::phys::body_world;
using engine::phys::gyroscopic_mode;
using engine::phys::inertia_of;
using engine::phys::kinetic_energy;
using engine::phys::make_box;
using engine::phys::rigid_body;
using engine::phys::spin_rule;

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
constexpr Uint32 k_wire       = engine::pack_argb(190, 198, 214);
constexpr Uint32 k_wire_back   = engine::pack_argb(74, 80, 94);

/// Angular momentum. Amber, and deliberately NOT one of the axis colours: `L`
/// is the one vector in the left panel that does not move, and a reader who
/// took it for a fourth body axis would have the picture exactly backwards.
constexpr Uint32 k_momentum   = engine::pack_argb(235, 200, 96);

/// The body axes, in the course's colours — conventions.html §10, x/y/z =
/// red/green/blue. These name DIRECTIONS, so reusing the convention is the whole
/// point; the `bodies` demo deliberately did not, because its colours named
/// masses.
constexpr Uint32 k_body_axis[3] = {
    engine::pack_argb(235, 96, 96),     // x — the largest moment
    engine::pack_argb(120, 220, 160),   // y — the intermediate one
    engine::pack_argb(150, 160, 255),   // z — the smallest
};

/// The box. 1 x 2 x 3 metres, which is the smallest shape whose three principal
/// moments are all different — and "all different" is the entire precondition
/// for everything this demo shows.
constexpr vec3 k_half{0.5f, 1.0f, 1.5f};
constexpr float k_mass = 1.0f;

/// How fast it is thrown, and how big the nudge on the other two axes is.
///
/// **THE NUDGE IS NOT OPTIONAL AND IT IS NOT CHEATING.** A body spinning exactly
/// about a principal axis is at an equilibrium and stays there — including at
/// the *unstable* one, in the same way a pencil balanced exactly on its point
/// does not fall. Real bodies are never exactly on an axis, and neither is
/// anything a player throws. 1e-3 rad/s is about a sixteenth of a degree per
/// second, which is far below what any hand can control.
constexpr float k_spin = 10.0f;
constexpr float k_nudge = 1e-3f;

/// The engine's `name_of(gyroscopic_mode)` returns one word, which is right for
/// a log line and not enough for a panel whose whole job is to say what the
/// difference IS. Named differently rather than overloaded: ADL finds the
/// engine's overload for an `engine::phys` type, and a same-named function here
/// is an ambiguity rather than a shadow.
const char* describe(gyroscopic_mode mode)
{
    switch (mode)
    {
    case gyroscopic_mode::off:           return "off — no tumble, ever";
    case gyroscopic_mode::explicit_term: return "explicit — tumbles, gains energy";
    case gyroscopic_mode::implicit_term: return "implicit — tumbles, loses it";
    case gyroscopic_mode::momentum:      return "momentum — conserves L";
    }
    return "?";
}

const char* const k_axis_name[3] = {"x  largest I", "y  INTERMEDIATE", "z  smallest I"};

// ---------------------------------------------------------------------------
// The two panels
// ---------------------------------------------------------------------------

constexpr int k_left_x0 = 24,  k_left_y0 = 64,  k_left_x1 = 560, k_left_y1 = 500;
constexpr int k_plot_x0 = 596, k_plot_y0 = 64,  k_plot_x1 = 936, k_plot_y1 = 500;

/// Metres across the left panel. The box's longest diagonal is 3.74 m, so this
/// leaves it room to turn without ever clipping.
constexpr float k_left_span = 6.5f;

/// Seconds across the plot, and rad/s up it.
constexpr float k_plot_seconds = 12.0f;
constexpr float k_plot_range = 12.0f;

// ---- Clipping, inherited verbatim from demos/integrate and demos/bodies ----
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

void draw_blob(engine::framebuffer& f, int sx, int sy, int r, Uint32 colour)
{
    for (int dy = -r; dy <= r; ++dy)
    {
        for (int dx = -r; dx <= r; ++dx)
        {
            if (dx * dx + dy * dy <= r * r) { f.put_pixel(sx + dx, sy + dy, colour); }
        }
    }
}

/// A trace of one scalar against time, kept as screen pixels.
struct trace
{
    std::vector<int> x;
    std::vector<int> y;
};

void push_trace(trace& t, int x, int y)
{
    if (!t.x.empty() && t.x.back() == x && t.y.back() == y) { return; }
    if (t.x.size() > 6000) { return; }
    t.x.push_back(x);
    t.y.push_back(y);
}

void draw_trace(engine::framebuffer& f, const trace& t, Uint32 colour, bool thick)
{
    for (std::size_t i = 1; i < t.x.size(); ++i)
    {
        // A trace that wrapped round to the left edge must not be joined across
        // the panel. One test, and without it every lap draws a horizontal line
        // through the middle of the plot.
        if (t.x[i] < t.x[i - 1]) { continue; }
        line_in(f, t.x[i - 1], t.y[i - 1], t.x[i], t.y[i],
                k_plot_x0, k_plot_y0, k_plot_x1, k_plot_y1, colour, thick);
    }
}

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

class spin_app final : public engine::app
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
            else if (SDL_strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
            {
                const int wanted = SDL_atoi(argv[++i]);
                mode_ = static_cast<gyroscopic_mode>(std::clamp(wanted, 0, 3));
            }
            else if (SDL_strcmp(argv[i], "--axis") == 0 && i + 1 < argc)
            {
                const int wanted = SDL_atoi(argv[++i]);
                axis_ = std::clamp(wanted, 0, 2);
            }
            else if (SDL_strcmp(argv[i], "--rate") == 0 && i + 1 < argc)
            {
                const double wanted = SDL_atof(argv[++i]);
                rate_ = static_cast<float>(std::clamp(wanted, 8.0, 3840.0));
            }
        }

        return {.title = "spin — a box that flips with nothing touching it",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        SDL_SetLogPriority(log_demo, SDL_LOG_PRIORITY_INFO);
        reset();
        ENGINE_LOG_INFO(log_demo, "[G] gyroscopic mode  [1][2][3] spin axis  [E] spin rule");
        ENGINE_LOG_INFO(log_demo, "[Up][Down] rate  [R] reset  [Space] pause  [Esc] quit");
        return ui_.start(window(), renderer()) || shot_path_ != nullptr;
    }

    void on_event(const SDL_Event& event) override
    {
        (void)ui_.handle_event(event);
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) { return; }

        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_G:
            mode_ = static_cast<gyroscopic_mode>((static_cast<int>(mode_) + 1) % 4);
            reset();
            break;
        case SDL_SCANCODE_1: axis_ = 0; reset(); break;
        case SDL_SCANCODE_2: axis_ = 1; reset(); break;
        case SDL_SCANCODE_3: axis_ = 2; reset(); break;
        case SDL_SCANCODE_E:
            rule_ = (rule_ == spin_rule::linearised) ? spin_rule::exponential
                                                     : spin_rule::linearised;
            reset();
            break;
        case SDL_SCANCODE_R: reset(); break;
        case SDL_SCANCODE_SPACE: paused_ = !paused_; break;
        case SDL_SCANCODE_UP:   set_rate(rate_ * 2.0f); break;
        case SDL_SCANCODE_DOWN: set_rate(rate_ * 0.5f); break;
        default: break;
        }
    }

    void on_input() override { ui_.begin_frame(); }

    void on_fixed_step(float h) override
    {
        if (paused_) { return; }

        // The nested accumulator, exactly as `demos/integrate` and
        // `demos/bodies` use it and for the same reason: `h` here is the
        // application's step, fixed by the platform layer, and `sim_` dices it
        // again at whatever rate the physics is being studied at. Pressing [Up]
        // changes what the simulation does, not how often the screen updates.
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
        rate_ = std::clamp(hz, 8.0f, 3840.0f);
        reset();
    }

    void reset()
    {
        sim_ = engine::fixed_step{rate_, 512};
        sim_t_ = 0.0f;

        world_.clear();
        world_.set_gravity(vec3{});          // nothing acts on this body. At all.
        world_.set_spin_rule(rule_);

        rigid_body b = make_box(vec3{}, k_mass, k_half);
        b.gyroscopic = mode_;

        vec3 w{k_nudge, k_nudge, k_nudge};
        (&w.x)[axis_] = k_spin;
        b.angular_velocity = w;

        id_ = world_.add(b);

        const rigid_body* p = world_.get(id_);
        l0_ = length(angular_momentum(*p));
        e0_ = kinetic_energy(*p);

        for (trace& t : plot_) { t.x.clear(); t.y.clear(); }
        sample_ = 0;
    }

    void advance(float h)
    {
        world_.step(h);
        sim_t_ += h;

        // Sample the plot at a fixed wall rate rather than every step, so that
        // changing [Up]/[Down] changes the SIMULATION and not the picture's
        // resolution — otherwise a faster rate would draw a denser curve and
        // the two effects would be impossible to tell apart.
        const float per_sample = k_plot_seconds / static_cast<float>(k_plot_x1 - k_plot_x0);
        while (static_cast<float>(sample_) * per_sample < sim_t_)
        {
            ++sample_;
            const float t = static_cast<float>(sample_) * per_sample;
            const vec3 wb = body_omega();

            const int sx = k_plot_x0
                         + static_cast<int>(std::fmod(t, k_plot_seconds) / k_plot_seconds
                                            * static_cast<float>(k_plot_x1 - k_plot_x0));
            for (int k = 0; k < 3; ++k)
            {
                const float v = (&wb.x)[k];
                const float u = 0.5f - 0.5f * std::clamp(v / k_plot_range, -1.0f, 1.0f);
                const int sy = k_plot_y0
                             + static_cast<int>(u * static_cast<float>(k_plot_y1 - k_plot_y0));
                push_trace(plot_[k], sx, sy);
            }
        }
    }

    /// The angular velocity **in body axes**, which is the only frame in which
    /// the flip is visible as a sign change.
    ///
    /// The engine stores `omega` in world space (rigid_body.hpp says why), and
    /// for a body spinning about its own y axis the two perturbation components
    /// are carried around that axis at the spin rate — so a world-space
    /// `omega.x` oscillates at 10 rad/s while its envelope grows, and a plot of
    /// it is unreadable. Euler's equations are body-frame equations.
    [[nodiscard]] vec3 body_omega() const
    {
        const rigid_body* b = world_.get(id_);
        if (b == nullptr) { return vec3{}; }
        return transpose(mat3_from_quat(b->orientation)) * b->angular_velocity;
    }

    /// Orthographic projection of a world point onto the left panel, looking
    /// down -z with a slight tilt so that all three axes are distinguishable.
    ///
    /// No perspective, no depth buffer and no camera: this demo owns exactly one
    /// object and the question it answers is "which way is it pointing", for
    /// which a parallel projection is strictly better — a perspective one would
    /// make the near face larger and invite the reader to interpret that as
    /// motion toward them.
    void to_left(vec3 p, int& sx, int& sy) const
    {
        // A fixed isometric-ish view: yaw 35 degrees, pitch 20.
        constexpr float yaw = 35.0f * k_pi / 180.0f;
        constexpr float pitch = 20.0f * k_pi / 180.0f;
        const float cy = std::cos(yaw);
        const float sy_ = std::sin(yaw);
        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);

        const float x = p.x * cy + p.z * sy_;
        const float z = -p.x * sy_ + p.z * cy;
        const float y = p.y * cp - z * sp;

        const float u = 0.5f + x / k_left_span;
        const float v = 0.5f - y / k_left_span;
        sx = k_left_x0 + static_cast<int>(u * static_cast<float>(k_left_x1 - k_left_x0));
        sy = k_left_y0 + static_cast<int>(v * static_cast<float>(k_left_y1 - k_left_y0));
    }

    void draw()
    {
        engine::framebuffer& f = fb();
        f.clear(k_background);

        draw_left(f);
        draw_plot(f);
    }

    void draw_left(engine::framebuffer& f)
    {
        frame_box(f, k_left_x0, k_left_y0, k_left_x1, k_left_y1);

        const rigid_body* b = world_.get(id_);
        if (b == nullptr) { return; }

        const mat3 r = mat3_from_quat(b->orientation);

        // The eight corners, in body coordinates, carried out to world space.
        vec3 corner[8];
        for (int i = 0; i < 8; ++i)
        {
            const vec3 local{((i & 1) != 0) ? k_half.x : -k_half.x,
                             ((i & 2) != 0) ? k_half.y : -k_half.y,
                             ((i & 4) != 0) ? k_half.z : -k_half.z};
            corner[i] = r * local;
        }

        // The twelve edges, as pairs of corner indices — each edge flips exactly
        // one bit of the index, which is what makes a cube's edge list writable
        // from the definition rather than looked up.
        static const int edge[12][2] = {
            {0, 1}, {2, 3}, {4, 5}, {6, 7},
            {0, 2}, {1, 3}, {4, 6}, {5, 7},
            {0, 4}, {1, 5}, {2, 6}, {3, 7},
        };

        // Two passes so that the far edges are drawn first and dimmer. This is
        // not a depth buffer — it is one sort key per edge, which is all a
        // convex wireframe needs and is the whole of Lesson 3.1's painter's
        // algorithm on twelve primitives.
        for (int pass = 0; pass < 2; ++pass)
        {
            for (const auto& e : edge)
            {
                const float depth = 0.5f * (corner[e[0]].z + corner[e[1]].z);
                const bool near_edge = depth >= 0.0f;
                if (near_edge != (pass == 1)) { continue; }

                int x0 = 0;
                int y0 = 0;
                int x1 = 0;
                int y1 = 0;
                to_left(corner[e[0]], x0, y0);
                to_left(corner[e[1]], x1, y1);
                line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1,
                        near_edge ? k_wire : k_wire_back, near_edge);
            }
        }

        // The three body axes, so that the flip is legible as a thing happening
        // to the BOX rather than to an abstract curve. Each is drawn from the
        // centre out to 1.6x the half-extent along it.
        int cx = 0;
        int cy = 0;
        to_left(vec3{}, cx, cy);
        for (int k = 0; k < 3; ++k)
        {
            vec3 local{};
            (&local.x)[k] = (&k_half.x)[k] * 1.6f;
            int ax = 0;
            int ay = 0;
            to_left(r * local, ax, ay);
            line_in(f, cx, cy, ax, ay, k_left_x0, k_left_y0, k_left_x1, k_left_y1,
                    k_body_axis[k], true);
            draw_blob(f, ax, ay, 3, k_body_axis[k]);
        }

        // ...and the angular momentum, which is the one vector in the picture
        // that does not move. Drawn in grey because it is not a body axis and
        // must not be read as one.
        int lx = 0;
        int ly = 0;
        const vec3 l = angular_momentum(*b);
        const float len = length(l);
        if (len > 0.0f)
        {
            to_left(l * (2.6f / len), lx, ly);
            line_in(f, cx, cy, lx, ly, k_left_x0, k_left_y0, k_left_x1, k_left_y1,
                    k_momentum, true);
            draw_blob(f, lx, ly, 4, k_momentum);
        }
    }

    void draw_plot(engine::framebuffer& f)
    {
        frame_box(f, k_plot_x0, k_plot_y0, k_plot_x1, k_plot_y1);

        // Zero, and the +/- spin level, so that "the green trace crossed zero"
        // is something the reader can see rather than infer.
        const int mid = k_plot_y0 + (k_plot_y1 - k_plot_y0) / 2;
        engine::draw_line(f, k_plot_x0, mid, k_plot_x1, mid, k_axis);

        for (int sign = -1; sign <= 1; sign += 2)
        {
            const float u = 0.5f - 0.5f * (static_cast<float>(sign) * k_spin / k_plot_range);
            const int sy = k_plot_y0
                         + static_cast<int>(u * static_cast<float>(k_plot_y1 - k_plot_y0));
            engine::draw_line(f, k_plot_x0, sy, k_plot_x1, sy, k_grid);
        }

        // The spin axis LAST, so that the trace the demo is about is never
        // hidden under one of the other two.
        for (int k = 0; k < 3; ++k)
        {
            if (k != axis_) { draw_trace(f, plot_[k], k_body_axis[k], false); }
        }
        draw_trace(f, plot_[axis_], k_body_axis[axis_], true);
    }

    void build_panel()
    {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400.0f, 300.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("spin"))
        {
            ImGui::Text("gyroscopic  %s", describe(mode_));
            ImGui::Text("spin axis   %s", k_axis_name[axis_]);
            ImGui::Text("orientation %s", engine::phys::name_of(rule_));
            ImGui::Text("rate  %.0f Hz    t = %.2f s", static_cast<double>(rate_),
                        static_cast<double>(sim_t_));
            ImGui::Separator();

            const rigid_body* b = world_.get(id_);
            if (b != nullptr)
            {
                const mat3 i_body = inertia_of(*b);
                ImGui::Text("Ix %.4f   Iy %.4f   Iz %.4f",
                            static_cast<double>(i_body.c0.x),
                            static_cast<double>(i_body.c1.y),
                            static_cast<double>(i_body.c2.z));

                const vec3 wb = body_omega();
                ImGui::Text("omega (body)  %7.4f %7.4f %7.4f",
                            static_cast<double>(wb.x), static_cast<double>(wb.y),
                            static_cast<double>(wb.z));

                const float l = length(angular_momentum(*b));
                const float e = kinetic_energy(*b);
                ImGui::Separator();
                ImGui::Text("|L|  %.6f    drift %+.3e", static_cast<double>(l),
                            static_cast<double>(l / l0_ - 1.0f));
                ImGui::Text("E    %.6f    drift %+.3e", static_cast<double>(e),
                            static_cast<double>(e / e0_ - 1.0f));
                ImGui::TextUnformatted("(both are exactly conserved by the real");
                ImGui::TextUnformatted(" motion; the drifts are this loop's)");

                ImGui::Separator();
                const engine::phys::step_report& rep = world_.report();
                ImGui::Text("max spin %.4f rad/s   |q|-1 %.2e",
                            static_cast<double>(rep.max_spin),
                            static_cast<double>(rep.max_unit_error));
            }

            ImGui::Separator();
            ImGui::TextUnformatted("amber = L, the vector that does not move");
            ImGui::TextUnformatted("[G] mode  [1][2][3] axis  [E] rule");
            ImGui::TextUnformatted("[Up][Down] rate  [R] reset  [Space] pause");
        }
        ImGui::End();
    }

    engine::debug_ui ui_;
    engine::fixed_step sim_{240.0f, 512};

    body_world world_;
    body_id id_{};
    trace plot_[3];
    int sample_ = 0;

    float l0_ = 1.0f;
    float e0_ = 1.0f;

    gyroscopic_mode mode_ = gyroscopic_mode::momentum;
    spin_rule rule_ = spin_rule::exponential;
    int axis_ = 1;                 // the intermediate one — the interesting case
    float rate_ = 240.0f;
    float sim_t_ = 0.0f;
    bool paused_ = false;

    const char* shot_path_ = nullptr;
    float shot_at_ = 6.0f;
};

}   // namespace

ENGINE_MAIN(spin_app)
