// demos/manifold/main.cpp — one contact is not a contact.
//
// Lesson 8.7. 8.6's demo drew a polytope growing until it pressed against a wall
// you could not see, and ended with a single vector: how deep, and which way.
// This one draws what a solver actually needs, which is a SET of points, and the
// two things that are hard about producing it — choosing which face to clip
// against, and clipping.
//
//   LEFT PANEL   the two shapes, the contact points as filled dots, and the
//                SUPPORT POLYGON they span drawn as a closed loop. The number
//                under it is its area, and it is the number that decides whether
//                the body can rest: a single point spans zero, which is a body
//                balanced on a pin.
//   RIGHT PANEL  the same contact seen FACE ON, in the reference face's own
//                plane. The reference polygon is the outer outline; the incident
//                polygon is the one being cut; and [S] applies one side plane at
//                a time, so Sutherland and Hodgman can be watched rather than
//                read about. The plane about to be applied is drawn as a dashed
//                line with its outward normal.
//
// WHAT TO WATCH FOR. Press [1] and look at the left panel: four points, a square
// support polygon, area 1.0 m². Press [4]: a ball on the same floor gets ONE
// point and an area of zero, and that is not a shortfall — a ball has no flat
// feature, and pretending otherwise is how you get a ball that will not roll.
//
// THEN PRESS [2] AND HOLD [Z]. The crate tilts, and somewhere around a degree
// the manifold drops from four points to two: the two corners that are still
// below the floor's plane. The support polygon collapses to a line, its area
// goes to zero, and a crate resting on that line can rock about it freely. That
// is the whole of why a solver's residual rotation matters.
//
// AND PRESS [W]. The panel starts reporting, frame to frame, how many of this
// frame's contact points could be matched to last frame's BY ID. Drag the crate
// around with the arrow keys and it stays at 100%: the ids name features, and
// sliding a crate across a floor does not change which corner is on which face.
// Tip it onto its side with [Z] held and it drops to zero for exactly one frame,
// which is correct — those really are different contacts.
//
//   [1]..[5]      presets: crate, tilted crate, crossed edges, ball, prism
//   [S]           apply one clip plane           [A] run the clip to completion
//   [R]           restart the clip               [W] toggle the persistence read
//   [G]           draw the support polygon or not
//   [Left/Right]  move the upper shape along x   [Up/Down]  along y
//   [,] [.]       along z
//   [Q] [E]       yaw it                         [Z] [C]    tilt it
//   [Space]       pause the drift                [0] reset      [Esc] quit
//
//     cmake --build build --target manifold
//     ./build/demos/manifold
//     ./build/demos/manifold --preset 2 --t 1.5 --shot out.ppm     headless
//
// IT RUNS ITS OWN CLIPPER, for the same reason `gjk` and `epa` drove their own
// loops: a single-stepping visualiser needs the iteration, not the answer. It
// uses only `support_face` and `collide_manifold`, both public, so 5.1's
// boundary is intact — and the engine's own answer is printed beside the
// picture, so the two can be seen to agree.

#include <engine/core/fixed_step.hpp>
#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/quat.hpp>
#include <engine/math/vec3.hpp>
#include <engine/phys/convex.hpp>
#include <engine/phys/manifold.hpp>
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
using engine::phys::collide_manifold;
using engine::phys::contact_face;
using engine::phys::contact_manifold;
using engine::phys::carry_impulses;
using engine::phys::convex;
using engine::phys::hull;
using engine::phys::k_max_face_vertices;
using engine::phys::manifold_status;
using engine::phys::name_of;
using engine::phys::obb;
using engine::phys::sphere_shape;
using engine::phys::support_face;
using engine::phys::world_hull;
using engine::phys::world_obb;
using engine::phys::world_sphere;

constexpr int k_width = 960;
constexpr int k_height = 540;
constexpr float k_pi = std::numbers::pi_v<float>;

/// This demo's own log category, in the range Lesson 5.3 reserved for programs
/// outside the library.
constexpr int log_demo = 67;

constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
constexpr Uint32 k_grid       = engine::pack_argb(38, 42, 52);
constexpr Uint32 k_axis       = engine::pack_argb(84, 92, 108);

constexpr Uint32 k_shape_a    = engine::pack_argb(150, 160, 255);
constexpr Uint32 k_shape_b    = engine::pack_argb(235, 96, 96);
constexpr Uint32 k_ref        = engine::pack_argb(120, 170, 255);
constexpr Uint32 k_inc        = engine::pack_argb(235, 130, 110);
constexpr Uint32 k_clip       = engine::pack_argb(235, 200, 96);
constexpr Uint32 k_plane      = engine::pack_argb(150, 200, 150);
constexpr Uint32 k_support    = engine::pack_argb(120, 220, 160);
constexpr Uint32 k_point      = engine::pack_argb(245, 245, 250);
constexpr Uint32 k_normal     = engine::pack_argb(235, 120, 200);

constexpr int k_left_x0  = 8;
constexpr int k_left_y0  = 8;
constexpr int k_left_x1  = 470;
constexpr int k_left_y1  = 531;
constexpr int k_right_x0 = 482;
constexpr int k_right_y0 = 8;
constexpr int k_right_x1 = 951;
constexpr int k_right_y1 = 531;

constexpr float k_left_span = 5.0f;

/// The isometric-ish projection 8.5 and 8.6 used, unchanged, so a reader moving
/// between the three demos is looking at the same world from the same angle.
constexpr float k_proj_c = 0.55f;
constexpr float k_proj_s = 0.32f;

/// 8.5's clipper, unchanged. `engine::draw_line` does not clip, so a demo with
/// two panels clips its own lines.
void line_in(engine::framebuffer& f, int x0, int y0, int x1, int y1,
             int cx0, int cy0, int cx1, int cy1, Uint32 colour)
{
    // Liang-Barsky, four half-spaces. 2.2's line walk with a parameter range
    // clamped before it starts rather than a test inside the loop.
    float t0 = 0.0f;
    float t1 = 1.0f;
    const float dx = static_cast<float>(x1 - x0);
    const float dy = static_cast<float>(y1 - y0);
    const float p[4] = {-dx, dx, -dy, dy};
    const float q[4] = {static_cast<float>(x0 - cx0), static_cast<float>(cx1 - x0),
                        static_cast<float>(y0 - cy0), static_cast<float>(cy1 - y0)};
    for (int i = 0; i < 4; ++i)
    {
        if (p[i] == 0.0f)
        {
            if (q[i] < 0.0f) { return; }
            continue;
        }
        const float r = q[i] / p[i];
        if (p[i] < 0.0f) { t0 = std::fmax(t0, r); }
        else { t1 = std::fmin(t1, r); }
    }
    if (t0 > t1) { return; }
    engine::draw_line(f, x0 + static_cast<int>(dx * t0), y0 + static_cast<int>(dy * t0),
                      x0 + static_cast<int>(dx * t1), y0 + static_cast<int>(dy * t1), colour);
}

void frame_box(engine::framebuffer& f, int x0, int y0, int x1, int y1)
{
    engine::draw_line(f, x0, y0, x1, y0, k_axis);
    engine::draw_line(f, x1, y0, x1, y1, k_axis);
    engine::draw_line(f, x1, y1, x0, y1, k_axis);
    engine::draw_line(f, x0, y1, x0, y0, k_axis);
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

/// A dashed line, for a plane that has not been applied yet.
void dashed_in(engine::framebuffer& f, int x0, int y0, int x1, int y1,
               int cx0, int cy0, int cx1, int cy1, Uint32 colour)
{
    const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int i = 0; i < steps; i += 8)
    {
        const int j = std::min(i + 4, steps);
        const float t0 = static_cast<float>(i) / static_cast<float>(std::max(1, steps));
        const float t1 = static_cast<float>(j) / static_cast<float>(std::max(1, steps));
        line_in(f, x0 + static_cast<int>((x1 - x0) * t0), y0 + static_cast<int>((y1 - y0) * t0),
                x0 + static_cast<int>((x1 - x0) * t1), y0 + static_cast<int>((y1 - y0) * t1),
                cx0, cy0, cx1, cy1, colour);
    }
}

// ---------------------------------------------------------------------------
// The demo's own clipper: `manifold.cpp`'s, one plane at a time
// ---------------------------------------------------------------------------
//
// Identical arithmetic to `build_manifold`'s inner loop, with the pass count
// exposed so [S] can stop it. It carries no ids — the engine's answer beside it
// has those — because what this panel is for is the GEOMETRY of the cut.

struct clip_state
{
    vec3 poly[2 * k_max_face_vertices];
    int count = 0;
    int planes_applied = 0;
};

/// Clip `inc` against the first `planes` side planes of `ref`. Both polygons are
/// in world space here; the engine does this relative to `a.origin`, for 8.4
/// §11's reason, and a demo drawing metre-sized crates at the world origin does
/// not need to.
clip_state clip_steps(const contact_face& ref, const vec3* rv, const contact_face& inc,
                      const vec3* iv, int planes)
{
    clip_state s;
    s.count = inc.count;
    for (int i = 0; i < inc.count; ++i) { s.poly[i] = iv[i]; }

    const int n = std::min(planes, ref.count);
    for (int e = 0; e < n && s.count > 0; ++e)
    {
        const vec3 a0 = rv[e];
        const vec3 a1 = rv[(e + 1) % ref.count];
        const vec3 side = cross(a1 - a0, ref.normal);
        const float offset = dot(side, a0);

        vec3 out[2 * k_max_face_vertices];
        int m = 0;
        for (int i = 0; i < s.count; ++i)
        {
            const vec3 c = s.poly[i];
            const vec3 nx = s.poly[(i + 1) % s.count];
            const float dc = dot(side, c) - offset;
            const float dn = dot(side, nx) - offset;
            if (dc <= 0.0f && m < 2 * k_max_face_vertices) { out[m++] = c; }
            if ((dc < 0.0f) != (dn < 0.0f) && m < 2 * k_max_face_vertices)
            {
                out[m++] = c + (nx - c) * (dc / (dc - dn));
            }
        }
        for (int i = 0; i < m; ++i) { s.poly[i] = out[i]; }
        s.count = m;
        ++s.planes_applied;
    }
    return s;
}

enum class kind
{
    box,
    ball,
    prism,
};

class manifold_app final : public engine::app
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
                preset_ = std::clamp(std::atoi(argv[++i]) - 1, 0, 4);
                drifting_ = false;
            }
        }

        return {.title = "manifold — one contact is not a contact",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        SDL_SetLogPriority(log_demo, SDL_LOG_PRIORITY_INFO);
        reset();
        ENGINE_LOG_INFO(log_demo, "[1]..[5] preset  [S] one clip plane  [A] auto  [R] restart");
        ENGINE_LOG_INFO(log_demo, "[W] persistence  [G] support polygon  arrows move");
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
        case SDL_SCANCODE_4: preset_ = 3; reset(); break;
        case SDL_SCANCODE_5: preset_ = 4; reset(); break;
        case SDL_SCANCODE_S: auto_run_ = false; ++manual_planes_; break;
        case SDL_SCANCODE_R: manual_planes_ = 0; break;
        case SDL_SCANCODE_A: auto_run_ = true; break;
        case SDL_SCANCODE_W: track_ids_ = !track_ids_; break;
        case SDL_SCANCODE_G: show_support_ = !show_support_; break;
        case SDL_SCANCODE_SPACE: drifting_ = !drifting_; break;
        case SDL_SCANCODE_0: reset(); break;

        case SDL_SCANCODE_LEFT:   nudge(vec3{-0.04f, 0.0f, 0.0f}); break;
        case SDL_SCANCODE_RIGHT:  nudge(vec3{+0.04f, 0.0f, 0.0f}); break;
        case SDL_SCANCODE_DOWN:   nudge(vec3{0.0f, -0.01f, 0.0f}); break;
        case SDL_SCANCODE_UP:     nudge(vec3{0.0f, +0.01f, 0.0f}); break;
        case SDL_SCANCODE_COMMA:  nudge(vec3{0.0f, 0.0f, -0.04f}); break;
        case SDL_SCANCODE_PERIOD: nudge(vec3{0.0f, 0.0f, +0.04f}); break;

        case SDL_SCANCODE_Q: spin(vec3{0.0f, 1.0f, 0.0f}, +0.05f); break;
        case SDL_SCANCODE_E: spin(vec3{0.0f, 1.0f, 0.0f}, -0.05f); break;
        case SDL_SCANCODE_Z: spin(vec3{0.0f, 0.0f, 1.0f}, +0.01f); break;
        case SDL_SCANCODE_C: spin(vec3{0.0f, 0.0f, 1.0f}, -0.01f); break;
        default: break;
        }
    }

    void on_input() override { ui_.begin_frame(); }

    void on_fixed_step(float h) override
    {
        if (!drifting_) { return; }
        t_ += h;
        drift_ = 0.45f * std::sin(t_ * 0.6f);
    }

    void on_frame(float alpha) override
    {
        (void)alpha;
        solve();
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
        drift_ = 0.0f;
        manual_ = vec3{};
        drifting_ = true;
        manual_planes_ = 0;
        previous_ = contact_manifold{};
        have_previous_ = false;

        // The prism's vertices live in the app rather than in the `hull` view,
        // which is non-owning: letting this vector reallocate under a live
        // `hull` is exactly the dangling-view bug `convex.hpp` deletes its rvalue
        // overloads to prevent.
        prism_points_.clear();
        for (int i = 0; i < 12; ++i)
        {
            const float a = 2.0f * k_pi * static_cast<float>(i) / 12.0f;
            for (float y : {-0.5f, 0.5f})
            {
                prism_points_.push_back(vec3{std::cos(a) * 0.5f, y, std::sin(a) * 0.5f});
            }
        }

        kind_b_ = kind::box;
        half_b_ = vec3{0.5f, 0.5f, 0.5f};
        orient_b_ = quat::identity();
        base_b_ = vec3{0.0f, 0.99f, 0.0f};

        switch (preset_)
        {
        case 0:
            break;
        case 1:
            orient_b_ = engine::quat_from_axis_angle(vec3{0.0f, 0.0f, 1.0f}, 0.05f);
            base_b_ = vec3{0.0f, 1.01f, 0.0f};
            break;
        case 2:
            // Two crates crossed: the lower one turned 45 degrees about z, the
            // upper one 45 degrees about x. They meet edge to edge, and an edge
            // pair touches at exactly one place.
            orient_a_ = engine::quat_from_axis_angle(vec3{0.0f, 0.0f, 1.0f}, k_pi / 4.0f);
            orient_b_ = engine::quat_from_axis_angle(vec3{1.0f, 0.0f, 0.0f}, k_pi / 4.0f);
            base_b_ = vec3{0.0f, 1.40f, 0.0f};
            break;
        case 3:
            kind_b_ = kind::ball;
            base_b_ = vec3{0.0f, 0.88f, 0.0f};
            break;
        case 4:
            kind_b_ = kind::prism;
            base_b_ = vec3{0.0f, 0.99f, 0.0f};
            break;
        default: break;
        }
        if (preset_ != 2) { orient_a_ = quat::identity(); }
    }

    [[nodiscard]] vec3 centre_b() const { return base_b_ + manual_ + vec3{drift_, 0.0f, 0.0f}; }

    /// Build both shapes, ask the engine for the manifold, and re-run the clip
    /// by hand so the right panel can show it half done.
    void solve()
    {
        const engine::phys::shape floor = box_shape(half_a_);
        box_a_ = world_obb(floor, centre_a_, orient_a_);
        const convex va = as_convex(box_a_);

        convex vb{};
        switch (kind_b_)
        {
        case kind::box:
            box_b_ = world_obb(box_shape(half_b_), centre_b(), orient_b_);
            vb = as_convex(box_b_);
            break;
        case kind::ball:
            ball_b_ = world_sphere(sphere_shape(0.4f), centre_b());
            vb = as_convex(ball_b_);
            break;
        case kind::prism:
            prism_b_ = world_hull(prism_points_, centre_b(), orient_b_);
            vb = as_convex(prism_b_);
            break;
        }

        contact_manifold fresh = collide_manifold(va, vb);
        matched_ = have_previous_ ? carry_impulses(fresh, previous_) : 0;
        current_ = fresh;
        previous_ = fresh;
        have_previous_ = true;

        // The faces, for the right panel. `support_face` returns them relative to
        // each shape's own centre; the panel works in world space, so add the
        // centres back here and nowhere in the engine.
        touching_ = (current_.status != manifold_status::none);
        if (!touching_) { return; }

        const vec3 n = current_.normal;
        face_a_ = va.face_of(n);
        face_b_ = vb.face_of(-n);
        for (int i = 0; i < face_a_.count; ++i) { world_a_[i] = va.origin + face_a_.v[i]; }
        for (int i = 0; i < face_b_.count; ++i) { world_b_[i] = vb.origin + face_b_.v[i]; }

        reference_on_b_ = current_.reference_on_b;
        const contact_face& ref = reference_on_b_ ? face_b_ : face_a_;
        const contact_face& inc = reference_on_b_ ? face_a_ : face_b_;
        const vec3* rv = reference_on_b_ ? world_b_ : world_a_;
        const vec3* iv = reference_on_b_ ? world_a_ : world_b_;

        const int planes = auto_run_ ? ref.count : manual_planes_;
        clip_ = clip_steps(ref, rv, inc, iv, planes);
        clip_total_ = ref.count;
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
        project(p, (k_left_x0 + k_left_x1) / 2, (k_left_y0 + k_left_y1) / 2 + 90,
                static_cast<float>(k_left_x1 - k_left_x0) / k_left_span, sx, sy);
    }

    /// The right panel's frame: the reference face's own plane, seen face on.
    void project_face(vec3 p, int& sx, int& sy) const
    {
        const vec3 r = p - face_origin_;
        const float u = dot(r, face_u_);
        const float w = dot(r, face_w_);
        sx = (k_right_x0 + k_right_x1) / 2 + static_cast<int>(u * face_scale_);
        sy = (k_right_y0 + k_right_y1) / 2 - static_cast<int>(w * face_scale_);
    }

    void draw_obb(engine::framebuffer& f, const obb& box, Uint32 colour) const
    {
        vec3 c[8];
        box.corners(c);
        static const int e[12][2] = {{0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
                                     {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& pair : e)
        {
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            project_left(c[pair[0]], x0, y0);
            project_left(c[pair[1]], x1, y1);
            line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1, colour);
        }
    }

    void draw_shape_b(engine::framebuffer& f) const
    {
        switch (kind_b_)
        {
        case kind::box: draw_obb(f, box_b_, k_shape_b); break;
        case kind::ball:
        {
            // A ball, as three great circles. It is a wireframe demo and a
            // silhouette alone would hide which way it is turned.
            for (int axis = 0; axis < 3; ++axis)
            {
                int px = 0;
                int py = 0;
                for (int i = 0; i <= 48; ++i)
                {
                    const float a = 2.0f * k_pi * static_cast<float>(i) / 48.0f;
                    const float c = std::cos(a) * ball_b_.radius;
                    const float s = std::sin(a) * ball_b_.radius;
                    const vec3 p = (axis == 0)   ? vec3{c, s, 0.0f}
                                   : (axis == 1) ? vec3{c, 0.0f, s}
                                                 : vec3{0.0f, c, s};
                    int x = 0;
                    int y = 0;
                    project_left(ball_b_.centre + p, x, y);
                    if (i > 0)
                    {
                        line_in(f, px, py, x, y, k_left_x0, k_left_y0, k_left_x1, k_left_y1,
                                k_shape_b);
                    }
                    px = x;
                    py = y;
                }
            }
            break;
        }
        case kind::prism:
        {
            const int sides = 12;
            for (int i = 0; i < sides; ++i)
            {
                const std::size_t a0 = static_cast<std::size_t>(2 * i);
                const std::size_t a1 = static_cast<std::size_t>(2 * i + 1);
                const std::size_t b0 = static_cast<std::size_t>(2 * ((i + 1) % sides));
                const std::size_t b1 = static_cast<std::size_t>(2 * ((i + 1) % sides) + 1);
                const vec3 pa0 = prism_b_.centre + prism_b_.axes * prism_points_[a0];
                const vec3 pa1 = prism_b_.centre + prism_b_.axes * prism_points_[a1];
                const vec3 pb0 = prism_b_.centre + prism_b_.axes * prism_points_[b0];
                const vec3 pb1 = prism_b_.centre + prism_b_.axes * prism_points_[b1];
                const vec3 pairs[3][2] = {{pa0, pa1}, {pa0, pb0}, {pa1, pb1}};
                for (const auto& seg : pairs)
                {
                    int x0 = 0;
                    int y0 = 0;
                    int x1 = 0;
                    int y1 = 0;
                    project_left(seg[0], x0, y0);
                    project_left(seg[1], x1, y1);
                    line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1,
                            k_shape_b);
                }
            }
            break;
        }
        }
    }

    void draw()
    {
        engine::framebuffer& f = fb();
        f.clear(k_background);

        frame_box(f, k_left_x0, k_left_y0, k_left_x1, k_left_y1);
        frame_box(f, k_right_x0, k_right_y0, k_right_x1, k_right_y1);

        // ---- left: the world ----------------------------------------------
        for (int i = -3; i <= 3; ++i)
        {
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            const float g = static_cast<float>(i) * 0.5f;
            project_left(vec3{g, 0.5f, -1.5f}, x0, y0);
            project_left(vec3{g, 0.5f, 1.5f}, x1, y1);
            line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1, k_grid);
            project_left(vec3{-1.5f, 0.5f, g}, x0, y0);
            project_left(vec3{1.5f, 0.5f, g}, x1, y1);
            line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1, k_grid);
        }
        draw_obb(f, box_a_, k_shape_a);
        draw_shape_b(f);

        if (touching_)
        {
            // The support polygon, closed. Its AREA is the number that matters;
            // a one-point manifold draws a dot and encloses nothing.
            if (show_support_ && current_.count >= 2)
            {
                for (int i = 0; i < current_.count; ++i)
                {
                    int x0 = 0;
                    int y0 = 0;
                    int x1 = 0;
                    int y1 = 0;
                    project_left(current_.points[i].position, x0, y0);
                    project_left(current_.points[(i + 1) % current_.count].position, x1, y1);
                    line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1,
                            k_support);
                }
            }
            for (int i = 0; i < current_.count; ++i)
            {
                int x = 0;
                int y = 0;
                project_left(current_.points[i].position, x, y);
                dot(f, x, y, 4, k_point);
                dot(f, x, y, 2, current_.points[i].warm ? k_support : k_shape_b);
            }
            // The manifold normal, from the deepest point.
            if (current_.count > 0)
            {
                int x0 = 0;
                int y0 = 0;
                int x1 = 0;
                int y1 = 0;
                const vec3 p = current_.points[0].position;
                project_left(p, x0, y0);
                project_left(p + current_.normal * 0.6f, x1, y1);
                line_in(f, x0, y0, x1, y1, k_left_x0, k_left_y0, k_left_x1, k_left_y1, k_normal);
            }
        }

        // ---- right: the contact, face on ----------------------------------
        if (!touching_) { return; }

        const contact_face& ref = reference_on_b_ ? face_b_ : face_a_;
        const contact_face& inc = reference_on_b_ ? face_a_ : face_b_;
        const vec3* rv = reference_on_b_ ? world_b_ : world_a_;
        const vec3* iv = reference_on_b_ ? world_a_ : world_b_;
        if (ref.count < 2) { return; }

        // A frame in the reference plane. `u` runs along its first edge, which
        // makes the picture rotate with the shape rather than with the camera.
        face_origin_ = vec3{};
        for (int i = 0; i < ref.count; ++i) { face_origin_ += rv[i]; }
        face_origin_ = face_origin_ * (1.0f / static_cast<float>(ref.count));
        face_u_ = normalised_or(rv[1] - rv[0], vec3{1.0f, 0.0f, 0.0f});
        face_w_ = cross(ref.normal, face_u_);

        // AUTO-FIT, because the two polygons are not the same size and often not
        // within an order of magnitude: a crate's bottom is 1 m across and the
        // floor it rests on is three. A fixed scale draws one of them off the
        // edge of the panel, which is what the first version of this demo did.
        float extent = 0.1f;
        for (int i = 0; i < ref.count; ++i)
        {
            const vec3 r = rv[i] - face_origin_;
            extent = std::fmax(extent, std::fmax(std::fabs(dot(r, face_u_)),
                                                 std::fabs(dot(r, face_w_))));
        }
        for (int i = 0; i < inc.count; ++i)
        {
            const vec3 r = iv[i] - face_origin_;
            extent = std::fmax(extent, std::fmax(std::fabs(dot(r, face_u_)),
                                                 std::fabs(dot(r, face_w_))));
        }
        face_scale_ = 0.40f * static_cast<float>(k_right_x1 - k_right_x0) / extent;

        for (int i = 0; i < ref.count; ++i)
        {
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            project_face(rv[i], x0, y0);
            project_face(rv[(i + 1) % ref.count], x1, y1);
            line_in(f, x0, y0, x1, y1, k_right_x0, k_right_y0, k_right_x1, k_right_y1, k_ref);
        }

        // The reference polygon's own vertices, so the side planes can be
        // counted off against them.
        for (int i = 0; i < ref.count; ++i)
        {
            int x = 0;
            int y = 0;
            project_face(rv[i], x, y);
            dot(f, x, y, 2, k_ref);
        }

        // The incident polygon, before any cutting, DASHED. It coincides exactly
        // with the clip result whenever nothing was cut, which is the commonest
        // case and would otherwise look like a missing polygon.
        for (int i = 0; i < inc.count; ++i)
        {
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            project_face(iv[i], x0, y0);
            project_face(iv[(i + 1) % inc.count], x1, y1);
            if (inc.count == 1) { break; }
            dashed_in(f, x0, y0, x1, y1, k_right_x0, k_right_y0, k_right_x1, k_right_y1, k_inc);
        }
        if (inc.count == 1)
        {
            int x = 0;
            int y = 0;
            project_face(iv[0], x, y);
            dot(f, x, y, 4, k_inc);
        }

        // NOTHING WAS CLIPPED, SO DO NOT DRAW A CLIP. When neither feature is
        // parallel enough to the contact normal the engine takes the edge path
        // instead, and drawing the side planes anyway would show an algorithm
        // that did not run. The two supporting EDGES and the one point they
        // produce are what happened.
        if (current_.status != manifold_status::face)
        {
            for (int i = 0; i < current_.count; ++i)
            {
                int x = 0;
                int y = 0;
                project_face(current_.points[i].position, x, y);
                dot(f, x, y, 5, k_point);
            }
            return;
        }

        // The plane about to be applied, dashed, with its outward normal.
        if (clip_.planes_applied < clip_total_)
        {
            const int e = clip_.planes_applied;
            const vec3 a0 = rv[e];
            const vec3 a1 = rv[(e + 1) % ref.count];
            const vec3 side = normalised_or(cross(a1 - a0, ref.normal), ref.normal);
            const vec3 mid = (a0 + a1) * 0.5f;
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            project_face(a0 - (a1 - a0) * 2.0f, x0, y0);
            project_face(a1 + (a1 - a0) * 2.0f, x1, y1);
            dashed_in(f, x0, y0, x1, y1, k_right_x0, k_right_y0, k_right_x1, k_right_y1, k_plane);
            project_face(mid, x0, y0);
            project_face(mid + side * 0.25f, x1, y1);
            line_in(f, x0, y0, x1, y1, k_right_x0, k_right_y0, k_right_x1, k_right_y1, k_plane);
        }

        // What survives so far.
        for (int i = 0; i < clip_.count; ++i)
        {
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            project_face(clip_.poly[i], x0, y0);
            project_face(clip_.poly[(i + 1) % clip_.count], x1, y1);
            if (clip_.count > 1)
            {
                line_in(f, x0, y0, x1, y1, k_right_x0, k_right_y0, k_right_x1, k_right_y1, k_clip);
            }
            project_face(clip_.poly[i], x0, y0);
            dot(f, x0, y0, 3, k_clip);
        }

        // And the engine's kept points, on the same picture.
        for (int i = 0; i < current_.count; ++i)
        {
            int x = 0;
            int y = 0;
            project_face(current_.points[i].position, x, y);
            dot(f, x, y, 5, k_point);
        }
    }

    void build_panel()
    {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Contact manifold");

        static const char* k_names[5] = {"crate on a floor", "tilted crate", "crossed edges",
                                         "ball on a floor", "prism on a floor"};
        ImGui::Text("preset: %s", k_names[preset_]);
        ImGui::Separator();

        if (!touching_)
        {
            ImGui::TextWrapped("Apart. `collide_manifold` returns a manifold with "
                               "no points and a status of `none` - there is no "
                               "speculative margin, which is 8.9's job.");
            ImGui::Separator();
            ImGui::Text("[1]..[5] preset  arrows move  [Space] pause");
            ImGui::End();
            return;
        }

        ImGui::Text("status           : %s", name_of(current_.status));
        ImGui::Text("contact points   : %d  (clipped %d)", current_.count, current_.clipped);
        ImGui::Text("support area     : %.4f m2", static_cast<double>(current_.support_area()));
        ImGui::Text("deepest          : %.4f m", static_cast<double>(current_.deepest()));
        ImGui::Text("normal           : (%+.3f %+.3f %+.3f)",
                    static_cast<double>(current_.normal.x), static_cast<double>(current_.normal.y),
                    static_cast<double>(current_.normal.z));
        ImGui::Text("reference face   : %s (feature %u)", reference_on_b_ ? "shape B" : "shape A",
                    static_cast<unsigned>(reference_on_b_ ? face_b_.feature : face_a_.feature));
        ImGui::Text("features         : A %d vertices, B %d vertices", face_a_.count,
                    face_b_.count);

        ImGui::Separator();
        ImGui::Text("clip: %d of %d side planes applied, %d points",
                    clip_.planes_applied, clip_total_, clip_.count);
        ImGui::Text("cos to normal    : A %.5f   B %.5f",
                    static_cast<double>(dot(face_a_.normal, current_.normal)),
                    static_cast<double>(dot(face_b_.normal, -current_.normal)));
        if (current_.status != manifold_status::face)
        {
            ImGui::TextWrapped("Neither feature is within `face_cos` of the "
                               "contact normal, or one of them is a single "
                               "point, so there is nothing to clip. The right "
                               "panel shows the contact rather than a clip that "
                               "did not happen.");
        }
        if (!auto_run_ && current_.status == manifold_status::face)
        {
            ImGui::TextWrapped("Press [S] for the next side plane. The dashed "
                               "line is the plane, the short stub is its OUTWARD "
                               "normal, and everything on that side is about to "
                               "go.");
        }

        ImGui::Separator();
        for (int i = 0; i < current_.count; ++i)
        {
            const engine::phys::contact_id& id = current_.points[i].id;
            ImGui::Text("%d: %-16s ref %3u inc %3u  %6.2f mm %s", i, name_of(id.kind),
                        static_cast<unsigned>(id.reference_index),
                        static_cast<unsigned>(id.incident_index),
                        1000.0 * static_cast<double>(current_.points[i].depth),
                        current_.points[i].warm ? "warm" : "");
        }

        if (track_ids_)
        {
            ImGui::Separator();
            ImGui::Text("matched to last frame : %d of %d", matched_, current_.count);
            ImGui::TextWrapped("Move the shape with the arrow keys and this stays "
                               "at full: an id names a FEATURE, and sliding a "
                               "crate does not change which corner is on which "
                               "face. Tilt it far enough with [Z] and it drops "
                               "for one frame, which is correct - those really "
                               "are different contacts.");
        }

        ImGui::Separator();
        ImGui::Text("[S] step [A] auto [R] restart [W] ids [G] polygon");
        ImGui::End();
    }

    engine::debug_ui ui_{};

    const char* shot_path_ = nullptr;
    float shot_at_ = 0.0f;
    int preset_ = 0;

    bool drifting_ = true;
    bool auto_run_ = true;
    bool track_ids_ = false;
    bool show_support_ = true;
    int manual_planes_ = 0;

    float t_ = 0.0f;
    float drift_ = 0.0f;
    vec3 manual_{};

    kind kind_b_ = kind::box;
    vec3 half_a_{1.5f, 0.5f, 1.5f};
    vec3 half_b_{0.5f, 0.5f, 0.5f};
    vec3 centre_a_{};
    vec3 base_b_{0.0f, 0.99f, 0.0f};
    quat orient_a_ = quat::identity();
    quat orient_b_ = quat::identity();

    std::vector<vec3> prism_points_;

    obb box_a_{};
    obb box_b_{};
    engine::sphere ball_b_{};
    hull prism_b_{};

    contact_manifold current_{};
    contact_manifold previous_{};
    bool have_previous_ = false;
    int matched_ = 0;

    contact_face face_a_{};
    contact_face face_b_{};
    vec3 world_a_[k_max_face_vertices]{};
    vec3 world_b_[k_max_face_vertices]{};
    bool reference_on_b_ = false;
    bool touching_ = false;

    clip_state clip_{};
    int clip_total_ = 0;

    mutable vec3 face_origin_{};
    mutable vec3 face_u_{1.0f, 0.0f, 0.0f};
    mutable vec3 face_w_{0.0f, 0.0f, 1.0f};
    mutable float face_scale_ = 150.0f;
};

} // namespace

ENGINE_MAIN(manifold_app)
