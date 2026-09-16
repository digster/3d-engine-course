// demos/bodies/main.cpp — three masses, three ways to fall, and four frames
// that disagree about which way is down.
//
// Lesson 8.2. The demo is built around the one claim in this lesson that a
// picture settles faster than a table: **mass is invisible until something
// resists**. Three bodies of very different mass are launched on identical
// trajectories, and the left panel draws where they go under each of three
// resistance models in turn —
//
//   NONE      the three trails lie exactly on top of one another. Galileo, and
//             the reason gravity was the last force in physics to be sorted out.
//   DAMPING   `rigid_body::damping`, which is `v *= exp(-k h)` — and the three
//             trails STILL lie on top of one another, because that knob has no
//             mass in it. It looks exactly like air resistance and is not.
//   DRAG      a real force `F = -b v`, added through the accumulator, which the
//             step then divides by the mass. Now they separate: the light one
//             stops short and the heavy one carries nearly as far as it did
//             through vacuum.
//
// Two of those three pictures are identical and only the third is the physics.
// That is the entire argument for `terminal_speed_damped` and
// `terminal_speed_dragged` sitting next to each other in `phys/rigid_body.hpp`,
// made in one keypress.
//
// The right panel is the other half of the lesson. Four bodies are dropped from
// the same height and each one is integrated in a different FRAME: world space,
// a parent scaled by 2, a parent whose non-uniform scale follows a turn, and a
// parent that is spinning. All four are drawn where they end up IN THE WORLD,
// which is where a player would see them, and only one of the four is falling.
//
//   [M]           cycle none / damping / drag
//   [1] [2] [3]   toggle the three masses
//   [F]           freeze / run the frame panel
//   [Up] [Down]   change the simulation rate
//   [R] reset     [Space] pause     [Esc] quit
//
//     cmake --build build --target bodies
//     ./build/demos/bodies
//     ./build/demos/bodies --t 4 --shot out.ppm       headless
//
// THERE IS NO FLOOR. Nothing in this engine can detect a collision until Lesson
// 8.4, so the ground line is drawn and then ignored and the bodies fall through
// it. Saying so is better than quietly framing the shot so nobody notices — the
// missing capability is the next four lessons, not a bug.
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
#include <engine/math/mat4.hpp>
#include <engine/math/vec3.hpp>
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
using engine::mat4;
using engine::vec3;
using engine::phys::add_force;
using engine::phys::body_id;
using engine::phys::body_world;
using engine::phys::frame_report;
using engine::phys::inspect_frame;
using engine::phys::k_gravity;
using engine::phys::make_dynamic;
using engine::phys::mass_of;
using engine::phys::motion;
using engine::phys::rigid_body;
using engine::phys::terminal_speed_damped;
using engine::phys::terminal_speed_dragged;

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
constexpr Uint32 k_ground     = engine::pack_argb(120, 96, 64);

/// One colour per mass. NOT the axis colours from conventions.html §10 — these
/// name bodies, not directions, and reusing red/green/blue would say something
/// false about them.
constexpr Uint32 k_mass_colour[3] = {
    engine::pack_argb(235, 200, 96),   // 0.1 kg
    engine::pack_argb(120, 220, 160),  // 1 kg
    engine::pack_argb(150, 160, 255),  // 10 kg
};

/// ...and one per FRAME, in the right-hand panel.
constexpr Uint32 k_frame_colour[4] = {
    engine::pack_argb(120, 220, 160),  // world space — the correct one
    engine::pack_argb(235, 96, 96),    // uniform scale 2
    engine::pack_argb(235, 150, 80),   // non-uniform scale after a turn
    engine::pack_argb(190, 120, 235),  // a spinning parent
};

const char* const k_frame_name[4] = {"world", "scale 2", "shear", "spinning"};

/// Where all four right-panel bodies start, IN THE WORLD, and how long the panel
/// runs before it loops.
///
/// **The same world point for all four is the whole experiment.** A body placed
/// at local (0, 10) under a parent scaled by 2 starts at world (0, 20), so a
/// panel that gave them all the same LOCAL position would be comparing four
/// different drops and the divergence would be partly the starting line. Each
/// body's local position is therefore derived from this one world position
/// through its own frame, using the engine's own `place_in_parent`.
constexpr vec3 k_drop_from{0.0f, 11.0f, 0.0f};
constexpr float k_frame_loop_seconds = 1.5f;

/// What resists the motion. The point of the demo is that two of the three
/// produce the same picture.
enum class resistance : int { none, damping, drag };

const char* name_of(resistance r)
{
    switch (r)
    {
    case resistance::none:    return "none";
    case resistance::damping: return "damping   v *= exp(-k h)";
    case resistance::drag:    return "drag force   F = -b v";
    }
    return "?";
}

constexpr float k_masses[3] = {0.1f, 1.0f, 10.0f};
constexpr float k_damping = 0.35f;     // 1/s
constexpr float k_drag = 0.35f;        // N.s/m
constexpr float k_launch_speed = 22.0f;
constexpr float k_launch_angle = 50.0f * k_pi / 180.0f;

// ---------------------------------------------------------------------------
// The two panels
// ---------------------------------------------------------------------------

constexpr int k_left_x0 = 24,   k_left_y0 = 64,  k_left_x1 = 600, k_left_y1 = 470;
constexpr int k_right_x0 = 632, k_right_y0 = 64, k_right_x1 = 936, k_right_y1 = 470;

constexpr float k_left_span_x = 52.0f;
constexpr float k_left_top_y = 16.0f;
constexpr float k_left_bottom_y = -20.0f;

/// The right panel is a square box in metres, centred on the world origin.
constexpr float k_right_half = 16.0f;

/// One trail point per body per sampled instant, kept as screen pixels because
/// nothing re-projects them — neither panel pans or zooms.
struct trail
{
    std::vector<int> x;
    std::vector<int> y;
    bool on = true;
};

void push_trail(trail& t, int x, int y)
{
    if (!t.x.empty() && t.x.back() == x && t.y.back() == y) { return; }
    if (t.x.size() > 4000) { return; }
    t.x.push_back(x);
    t.y.push_back(y);
}

void left_to_screen(vec3 p, int& sx, int& sy)
{
    const float u = p.x / k_left_span_x;
    const float v = (k_left_top_y - p.y) / (k_left_top_y - k_left_bottom_y);
    sx = k_left_x0 + static_cast<int>(u * static_cast<float>(k_left_x1 - k_left_x0));
    sy = k_left_y0 + static_cast<int>(v * static_cast<float>(k_left_y1 - k_left_y0));
}

void right_to_screen(vec3 p, int& sx, int& sy)
{
    const float u = (p.x + k_right_half) / (2.0f * k_right_half);
    const float v = (k_right_half - p.y) / (2.0f * k_right_half);
    sx = k_right_x0 + static_cast<int>(u * static_cast<float>(k_right_x1 - k_right_x0));
    sy = k_right_y0 + static_cast<int>(v * static_cast<float>(k_right_y1 - k_right_y0));
}

// ---- Clipping, inherited verbatim from demos/integrate ---------------------
//
// `engine::draw_line` deliberately does not clip — Lesson 2.1 says so in a
// comment — and this demo has TWO panels side by side, so a body leaving one of
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
             int bx0, int by0, int bx1, int by1, Uint32 colour)
{
    if (clip_to(x0, y0, x1, y1, bx0, by0, bx1, by1))
    {
        engine::draw_line(f, x0, y0, x1, y1, colour);
    }
}

/// Draw a trail, optionally as one run in `stride`, offset by `phase`.
///
/// **THE DASHING IS NOT DECORATION AND IT IS NOT AN ACCIDENT OF STYLE.** In two
/// of this demo's three modes the three masses follow the SAME curve to the
/// pixel, and three solid curves drawn on top of one another are
/// indistinguishable from one curve — the picture that proves the point would
/// look exactly like the picture of a bug that lost two bodies. So each body
/// draws one run in three: a single dashed line cycling through three colours is
/// three trajectories in perfect agreement, and three separate dashed lines are
/// three trajectories that parted company.
void draw_trail(engine::framebuffer& f, const trail& t,
                int bx0, int by0, int bx1, int by1, Uint32 colour,
                int phase = 0, int stride = 1, bool thick = false)
{
    constexpr std::size_t k_run = 9;   // pixels per dash, at 960x540
    for (std::size_t i = 1; i < t.x.size(); ++i)
    {
        if (stride > 1)
        {
            const std::size_t run = i / k_run;
            if (static_cast<int>(run % static_cast<std::size_t>(stride)) != phase)
            {
                continue;
            }
        }
        line_in(f, t.x[i - 1], t.y[i - 1], t.x[i], t.y[i],
                bx0, by0, bx1, by1, colour);
        if (thick)
        {
            // A second line one pixel down, which is the cheapest legible width
            // there is and is enough: the figure in the lesson is this
            // framebuffer scaled down to page width, and a one-pixel dash
            // survives that at about half its contrast.
            line_in(f, t.x[i - 1], t.y[i - 1] + 1, t.x[i], t.y[i] + 1,
                    bx0, by0, bx1, by1, colour);
        }
    }
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

void frame_box(engine::framebuffer& f, int x0, int y0, int x1, int y1)
{
    engine::draw_line(f, x0, y0, x1, y0, k_axis);
    engine::draw_line(f, x0, y1, x1, y1, k_axis);
    engine::draw_line(f, x0, y0, x0, y1, k_axis);
    engine::draw_line(f, x1, y0, x1, y1, k_axis);
}

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

class bodies_app final : public engine::app
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
                mode_ = static_cast<resistance>(std::clamp(wanted, 0, 2));
            }
            else if (SDL_strcmp(argv[i], "--rate") == 0 && i + 1 < argc)
            {
                const double wanted = SDL_atof(argv[++i]);
                rate_ = static_cast<float>(std::clamp(wanted, 8.0, 960.0));
            }
        }

        return {.title = "bodies — three masses, three ways to fall",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        SDL_SetLogPriority(log_demo, SDL_LOG_PRIORITY_INFO);
        build_frames();
        reset();
        ENGINE_LOG_INFO(log_demo, "[M] resistance  [1][2][3] masses  [F] frames");
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
        case SDL_SCANCODE_M:
            mode_ = static_cast<resistance>((static_cast<int>(mode_) + 1) % 3);
            reset();
            break;
        case SDL_SCANCODE_1: left_[0].on = !left_[0].on; break;
        case SDL_SCANCODE_2: left_[1].on = !left_[1].on; break;
        case SDL_SCANCODE_3: left_[2].on = !left_[2].on; break;
        case SDL_SCANCODE_F: frames_running_ = !frames_running_; break;
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

        // The nested accumulator, exactly as `demos/integrate` uses it and for
        // the same reason: `h` here is the application's step, fixed by the
        // platform layer, and `sim_` dices it again at whatever rate the physics
        // is being studied at. Pressing [Up] changes what the simulation does,
        // not how often the screen updates.
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
        rate_ = std::clamp(hz, 8.0f, 960.0f);
        reset();
    }

    /// The four parent frames of the right-hand panel. Three are fixed; the
    /// fourth is rebuilt every step because it is the one that turns.
    void build_frames()
    {
        const float turn = 45.0f * k_pi / 180.0f;
        frames_[0] = engine::affine(mat3{}, vec3{});
        frames_[1] = engine::affine(engine::scale(2.0f, 2.0f, 2.0f), vec3{});
        frames_[2] = engine::affine(engine::scale(2.0f, 1.0f, 1.0f)
                                        * engine::rotation_z(turn), vec3{});
        frames_[3] = engine::affine(mat3{}, vec3{});
    }

    void reset()
    {
        sim_ = engine::fixed_step{rate_, 512};
        sim_t_ = 0.0f;

        // ---- left panel: three masses, one launch --------------------------
        world_.clear();
        const vec3 v0{k_launch_speed * std::cos(k_launch_angle),
                      k_launch_speed * std::sin(k_launch_angle),
                      0.0f};
        for (int i = 0; i < 3; ++i)
        {
            rigid_body b = make_dynamic(vec3{}, k_masses[i]);
            b.state.velocity = v0;
            if (mode_ == resistance::damping) { b.damping = k_damping; }
            ids_[i] = world_.add(b);
            left_[i].x.clear();
            left_[i].y.clear();
        }

        // ---- right panel: one body per frame -------------------------------
        spin_ = 0.0f;
        frames_[3] = engine::affine(mat3{}, vec3{});
        reset_frames();
    }

    /// Put all four right-panel bodies back on the same world point, each
    /// expressed in its own frame's local coordinates.
    void reset_frames()
    {
        rigid_body probe;
        probe.state.position = k_drop_from;
        for (int i = 0; i < 4; ++i)
        {
            local_[i] = motion{};
            // `place_in_parent` is the engine's own bridge and the only thing in
            // this demo that converts between the two spaces. Using it here
            // rather than inverting a matrix by hand is deliberate: it is the
            // public API a game would call, and it is being exercised in the
            // direction a game calls it.
            local_[i].position =
                engine::phys::place_in_parent(probe, frames_[i], engine::transform{})
                    .position;
            right_[i].x.clear();
            right_[i].y.clear();
        }
        frame_t_ = 0.0f;
    }

    void advance(float h)
    {
        // ---- left: the three masses, through the real body table -----------
        //
        // The DRAG case is the only one that touches the accumulator, and that
        // is the whole distinction: `damping` is a field the step consumes, and
        // a drag force is something a system adds — so the step divides it by
        // the mass on the way past and a heavy body feels less of it.
        if (mode_ == resistance::drag)
        {
            for (body_id id : ids_)
            {
                rigid_body* b = world_.get(id);
                add_force(*b, b->state.velocity * -k_drag);
            }
        }
        world_.step(h);

        // ---- right: one body per frame, all integrated LOCALLY -------------
        //
        // Deliberately NOT through `body_world`, which has no way to express
        // this: a body there is in world space and there is nothing else it
        // could be. These four are stepped by hand in the local space of their
        // parent, which is the mistake the panel exists to show.
        if (frames_running_)
        {
            // ALL FOUR GET THE SAME DROP, from the same world point, under the
            // same local gravity. The only variable is the frame, which is what
            // makes the four trails comparable at all.
            const vec3 g{0.0f, -k_gravity, 0.0f};
            for (int i = 0; i < 4; ++i)
            {
                engine::phys::integrate(local_[i], g, h,
                                        engine::phys::integrator::semi_implicit_euler);
            }

            spin_ += 1.6f * h;
            frames_[3] = engine::affine(engine::rotation_z(spin_), vec3{});

            // The panel loops, because three of the four leave the box within a
            // second and a half and the interesting part is the first one.
            frame_t_ += h;
            if (frame_t_ >= k_frame_loop_seconds) { reset_frames(); }
        }

        sim_t_ += h;
        sample();
    }

    void sample()
    {
        for (int i = 0; i < 3; ++i)
        {
            int sx = 0;
            int sy = 0;
            left_to_screen(world_.get(ids_[i])->state.position, sx, sy);
            push_trail(left_[i], sx, sy);
        }
        for (int i = 0; i < 4; ++i)
        {
            int sx = 0;
            int sy = 0;
            // Where the body ACTUALLY IS, which is what a player would see: the
            // local position carried out through the parent's matrix.
            right_to_screen(world_of(i), sx, sy);
            push_trail(right_[i], sx, sy);
        }
    }

    [[nodiscard]] vec3 world_of(int i) const
    {
        return engine::xyz(frames_[i] * engine::point(local_[i].position));
    }

    void draw()
    {
        engine::framebuffer& f = fb();
        f.clear(k_background);
        draw_left(f);
        draw_right(f);
    }

    void draw_left(engine::framebuffer& f)
    {
        frame_box(f, k_left_x0, k_left_y0, k_left_x1, k_left_y1);

        for (float x = 10.0f; x < k_left_span_x; x += 10.0f)
        {
            int sx = 0;
            int sy = 0;
            left_to_screen(vec3{x, 0.0f, 0.0f}, sx, sy);
            engine::draw_line(f, sx, k_left_y0, sx, k_left_y1, k_grid);
        }
        for (float y = k_left_bottom_y; y <= k_left_top_y; y += 10.0f)
        {
            int sx = 0;
            int sy = 0;
            left_to_screen(vec3{0.0f, y, 0.0f}, sx, sy);
            // The ground line, which nothing collides with until Lesson 8.4.
            engine::draw_line(f, k_left_x0, sy, k_left_x1, sy,
                              (y == 0.0f) ? k_ground : k_grid);
        }

        for (int i = 0; i < 3; ++i)
        {
            if (!left_[i].on) { continue; }
            draw_trail(f, left_[i], k_left_x0, k_left_y0, k_left_x1, k_left_y1,
                       k_mass_colour[i], i, 3, true);
            int sx = 0;
            int sy = 0;
            left_to_screen(world_.get(ids_[i])->state.position, sx, sy);
            if (sx >= k_left_x0 && sx <= k_left_x1 && sy >= k_left_y0 && sy <= k_left_y1)
            {
                draw_blob(f, sx, sy, 3, k_mass_colour[i]);
            }
        }
    }

    void draw_right(engine::framebuffer& f)
    {
        frame_box(f, k_right_x0, k_right_y0, k_right_x1, k_right_y1);

        int ox = 0;
        int oy = 0;
        right_to_screen(vec3{}, ox, oy);
        engine::draw_line(f, k_right_x0, oy, k_right_x1, oy, k_grid);
        engine::draw_line(f, ox, k_right_y0, ox, k_right_y1, k_grid);

        // BACK TO FRONT, so that frame 0 — world space, the one that is right —
        // is drawn LAST and is never hidden under a wrong answer. For the first
        // tenth of a second all four trails lie on top of each other, and which
        // one survives that overlap is a drawing-order decision rather than a
        // physical one.
        for (int k = 3; k >= 0; --k)
        {
            const int i = k;
            draw_trail(f, right_[i], k_right_x0, k_right_y0, k_right_x1, k_right_y1,
                       k_frame_colour[i], 0, 1, true);
            int sx = 0;
            int sy = 0;
            right_to_screen(world_of(i), sx, sy);
            if (sx >= k_right_x0 && sx <= k_right_x1
                && sy >= k_right_y0 && sy <= k_right_y1)
            {
                draw_blob(f, sx, sy, 3, k_frame_colour[i]);
            }
        }
    }

    void build_panel()
    {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(380.0f, 330.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("bodies"))
        {
            ImGui::Text("resistance  %s", name_of(mode_));
            ImGui::Text("rate  %.0f Hz    t = %.2f s", static_cast<double>(rate_),
                        static_cast<double>(sim_t_));
            ImGui::Separator();

            if (ImGui::BeginTable("masses", 4, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("mass");
                ImGui::TableSetupColumn("1/m");
                ImGui::TableSetupColumn("speed");
                ImGui::TableSetupColumn("v term");
                ImGui::TableHeadersRow();
                for (int i = 0; i < 3; ++i)
                {
                    const rigid_body* b = world_.get(ids_[i]);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f kg", static_cast<double>(mass_of(*b)));
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", static_cast<double>(b->inv_mass));
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f", static_cast<double>(length(b->state.velocity)));
                    ImGui::TableNextColumn();
                    switch (mode_)
                    {
                    case resistance::none:
                        ImGui::TextUnformatted("none");
                        break;
                    case resistance::damping:
                        ImGui::Text("%.2f", static_cast<double>(
                            terminal_speed_damped(k_gravity, k_damping)));
                        break;
                    case resistance::drag:
                        ImGui::Text("%.2f", static_cast<double>(
                            terminal_speed_dragged(k_gravity, k_masses[i], k_drag)));
                        break;
                    }
                }
                ImGui::EndTable();
            }

            ImGui::Separator();
            const engine::phys::step_report& r = world_.report();
            ImGui::Text("bodies %d    max speed %.2f m/s", r.bodies,
                        static_cast<double>(r.max_speed));
            ImGui::Text("travel/step %.4f m    momentum y %.2f",
                        static_cast<double>(r.max_travel),
                        static_cast<double>(r.momentum.y));

            ImGui::Separator();
            ImGui::TextUnformatted("the four frames, right panel");
            const vec3 g{0.0f, -k_gravity, 0.0f};
            for (int i = 0; i < 4; ++i)
            {
                const frame_report fr = inspect_frame(frames_[i], g);
                ImGui::Text("%-9s gain %.2f  tilt %5.1f  %s", k_frame_name[i],
                            static_cast<double>(fr.gain),
                            static_cast<double>(fr.tilt_degrees),
                            fr.inertial ? "ok" : "NOT INERTIAL");
            }
            ImGui::TextUnformatted("(the spinning one passes and is still wrong)");
        }
        ImGui::End();
    }

    engine::debug_ui ui_;
    engine::fixed_step sim_{120.0f, 512};

    body_world world_;
    body_id ids_[3]{};
    trail left_[3];

    motion local_[4];
    mat4 frames_[4];
    trail right_[4];
    float spin_ = 0.0f;
    float frame_t_ = 0.0f;
    bool frames_running_ = true;

    resistance mode_ = resistance::none;
    float rate_ = 120.0f;
    float sim_t_ = 0.0f;
    bool paused_ = false;

    const char* shot_path_ = nullptr;
    float shot_at_ = 3.0f;
};

}   // namespace

ENGINE_MAIN(bodies_app)
