// demos/plane/main.cpp — the complex plane, drawn, so that every claim in
// Lesson 7.3 can be watched rather than believed.
//
// Lesson 7.3. The gimbal demo puts a number on screen next to a picture, and
// this one does the same job one dimension down, where the picture is honest
// about everything: there is no camera, no projection and no perspective, so a
// length on screen IS a length in the maths and an angle on screen IS the angle.
// That is the entire reason Module 7 spends a lesson in the plane before going
// to four dimensions — the plane is the only place where the eye is a valid
// instrument.
//
// Four modes, and each one is a section of the lesson made movable:
//
//   [1] PRODUCT   — z·w drawn as a spiral similarity. Two triangles, (0, 1, w)
//                   and (0, z, zw), and they are the SAME triangle scaled by
//                   |z| and turned by arg z. Multiplication by z is that.
//   [2] POWERS    — z, z², z³ … marching round the circle. Each step is four
//                   multiplies and two adds; the whole ring costs no trig at
//                   all after the first one. Press [R] to watch the drift that
//                   buys, magnified, and [N] to renormalise it away.
//   [3] MIRRORS   — two mirror lines and a point bounced off both. The net turn
//                   is TWICE the angle between the mirrors, which is where the
//                   theta/2 in every quaternion comes from, and it is visible
//                   here with a protractor.
//   [4] BLEND     — slerp against nlerp from the same pair of poses. Ticks are
//                   drawn at equal steps of t, so the two paths can be compared
//                   as SCHEDULES: same arc, same endpoints, different spacing.
//
//     cmake --build build --target plane
//     ./build/demos/plane                              play with it
//     ./build/demos/plane --mode 3 --angle 25          start in a mode, posed
//     ./build/demos/plane --mode 4 --shot out.ppm      headless, deterministic
//
// THE RULES ARE 5.1'S. Nothing here includes an engine internal; everything it
// draws it draws with `framebuffer`, `raster`'s `draw_line`, and the arithmetic
// in `math/complex.hpp`. It does not even link `demo_common` — a program written
// to show what one header does has no business reaching for a shared scene.

#include <engine/core/log.hpp>
#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/gfx/raster.hpp>
#include <engine/math/complex.hpp>
#include <engine/math/vec2.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

using engine::complex;
using engine::vec2;

constexpr int k_width = 960;
constexpr int k_height = 540;

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_rad = k_pi / 180.0f;

/// This demo's own log category, in the range Lesson 5.3 reserved for programs
/// outside the library: the engine's five stop at `log_asset`, and a demo that
/// logged under one of those would be claiming to be part of a subsystem it is
/// not. `collector` does the same thing with `log_game`.
constexpr engine::log_category log_plane =
    static_cast<engine::log_category>(engine::log_category_count);

// ---------------------------------------------------------------------------
// Colours. The course conventions (Conventions §10) fix x/y/z as red/green/blue;
// this demo is in the plane and has no z, so the palette here is about ROLES:
// the operand you set, the operand it acts on, the answer, and the machinery.
// ---------------------------------------------------------------------------
constexpr Uint32 k_background = engine::pack_argb(16, 18, 24);
// DELIBERATELY DIM, and the exact value matters. The lesson's figure pipeline
// downsamples a capture of this program 3:1 by taking each block's brightest
// pixel and then floors anything below a luminance of 30,000 to true black
// (scratch/figs_511.py). Graph paper at (38, 42, 52) clears that floor and
// survives the downsample, so the published figure came out as a strong grid
// with the construction faint inside it. At (24, 26, 32) the grid still gives
// the eye a scale on a real screen and drops out of a capture, which is what a
// backdrop should do in both places.
constexpr Uint32 k_grid       = engine::pack_argb(24, 26, 32);
constexpr Uint32 k_axes       = engine::pack_argb(92, 98, 112);
constexpr Uint32 k_circle     = engine::pack_argb(70, 78, 96);

constexpr Uint32 k_z          = engine::pack_argb(236, 168, 86);    // amber: the rotation
constexpr Uint32 k_w          = engine::pack_argb(126, 188, 248);   // blue: what it acts on
constexpr Uint32 k_result     = engine::pack_argb(96, 222, 208);    // teal: the answer
constexpr Uint32 k_mirror     = engine::pack_argb(214, 118, 226);   // violet: a mirror line
constexpr Uint32 k_rotor      = engine::pack_argb(248, 214, 120);   // pale gold: the half-angle
constexpr Uint32 k_ghost      = engine::pack_argb(120, 132, 168);   // grey: construction
constexpr Uint32 k_probe      = engine::pack_argb(226, 230, 238);   // white: the point being moved

// ---------------------------------------------------------------------------
// §VIEW — plane coordinates to pixels
// ---------------------------------------------------------------------------
//
// ONE mapping, used by everything, and it is deliberately simple enough to check
// by eye: the origin sits at the middle of the framebuffer and one unit is
// `k_scale` pixels. The only subtlety is the sign on y, and it is the one
// `mat2::rotation` warns about — the framebuffer's +y points DOWN, so drawing an
// anticlockwise turn without the flip produces a clockwise picture and an hour
// of confusion. Flipping here, once, means every angle in this file matches the
// maths and the lesson's figures.

constexpr float k_scale = 150.0f;

struct view
{
    float cx = static_cast<float>(k_width) * 0.5f;
    float cy = static_cast<float>(k_height) * 0.5f;

    [[nodiscard]] int px(float x) const { return static_cast<int>(std::lround(cx + x * k_scale)); }
    [[nodiscard]] int py(float y) const { return static_cast<int>(std::lround(cy - y * k_scale)); }
};

void line(engine::framebuffer& fb, const view& v, vec2 a, vec2 b, Uint32 colour)
{
    engine::draw_line(fb, v.px(a.x), v.py(a.y), v.px(b.x), v.py(b.y), colour);
}

/// A line three pixels thick, by drawing it three times with small offsets.
///
/// Lesson 7.2 found this out the hard way: the figure pipeline downsamples a
/// render 3:1 by taking each block's BRIGHTEST pixel, so a one-pixel line
/// survives only if it happens to be the brightest thing in its block. Width
/// survives that sampler where a dashed pattern does not.
void fat_line(engine::framebuffer& fb, const view& v, vec2 a, vec2 b, Uint32 colour)
{
    for (int dx = -1; dx <= 1; ++dx)
    {
        engine::draw_line(fb, v.px(a.x) + dx, v.py(a.y), v.px(b.x) + dx, v.py(b.y), colour);
        engine::draw_line(fb, v.px(a.x), v.py(a.y) + dx, v.px(b.x), v.py(b.y) + dx, colour);
    }
}

/// An arrow from the origin to `tip`, with a head built — of course — by
/// multiplying by two complex numbers.
///
/// The head is the demo quietly using the thing it is demonstrating: the two
/// barbs are the shaft direction turned by ±160°, scaled to a fixed length, and
/// "turned by" is one multiplication. There is no `mat2` anywhere in this file.
void arrow(engine::framebuffer& fb, const view& v, vec2 from, vec2 tip, Uint32 colour,
           bool fat = true)
{
    if (fat) { fat_line(fb, v, from, tip, colour); }
    else     { line(fb, v, from, tip, colour); }

    const vec2 along = tip - from;
    const float len = engine::length(along);
    if (len < 1e-4f) { return; }

    const complex direction{along.x / len, along.y / len};
    const float barb = 0.09f;
    for (const float sign : {1.0f, -1.0f})
    {
        const complex swept = direction * engine::complex_from_angle(sign * 160.0f * k_rad);
        const vec2 end = tip + vec2{swept.re, swept.im} * barb;
        fat_line(fb, v, tip, end, colour);
    }
}

void dot(engine::framebuffer& fb, const view& v, vec2 p, Uint32 colour, int radius = 3)
{
    const int cx = v.px(p.x);
    const int cy = v.py(p.y);
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            if (x * x + y * y <= radius * radius) { fb.put_pixel(cx + x, cy + y, colour); }
        }
    }
}

/// A circle of radius `r`, drawn by the recurrence Lesson 7.3 §11 measures:
/// one complex multiply per point, and `cos`/`sin` called exactly once for the
/// whole ring rather than once per segment.
///
/// At 180 segments the accumulated error is a hundred-thousandth of a degree
/// (verify_73 §G), which is four orders of magnitude below one pixel at this
/// scale — so the demo can use the fast form without the picture being a lie
/// about the fast form.
void circle(engine::framebuffer& fb, const view& v, float r, Uint32 colour, int segments = 180)
{
    const complex step = engine::complex_from_angle(2.0f * k_pi / static_cast<float>(segments));
    complex z{r, 0.0f};
    for (int k = 0; k < segments; ++k)
    {
        const complex next = z * step;
        line(fb, v, vec2{z.re, z.im}, vec2{next.re, next.im}, colour);
        z = next;
    }
}

/// An arc from angle `a0` to angle `a1` at radius `r` — the "how far did it
/// turn" annotation, and the only way to show an angle without a protractor.
void arc(engine::framebuffer& fb, const view& v, float r, float a0, float a1, Uint32 colour)
{
    const int segments = 64;
    for (int k = 0; k < segments; ++k)
    {
        const float t0 = a0 + (a1 - a0) * static_cast<float>(k) / static_cast<float>(segments);
        const float t1 = a0 + (a1 - a0) * static_cast<float>(k + 1) / static_cast<float>(segments);
        line(fb, v,
             vec2{r * std::cos(t0), r * std::sin(t0)},
             vec2{r * std::cos(t1), r * std::sin(t1)}, colour);
    }
}

/// A line through the origin in the direction `m`, drawn right across the view.
void through_origin(engine::framebuffer& fb, const view& v, complex m, Uint32 colour)
{
    const vec2 d{m.re, m.im};
    fat_line(fb, v, d * -4.0f, d * 4.0f, colour);
}

void grid(engine::framebuffer& fb, const view& v)
{
    for (int k = -6; k <= 6; ++k)
    {
        const float f = static_cast<float>(k) * 0.5f;
        line(fb, v, {f, -3.0f}, {f, 3.0f}, k_grid);
        line(fb, v, {-4.0f, f}, {4.0f, f}, k_grid);
    }
    line(fb, v, {-4.0f, 0.0f}, {4.0f, 0.0f}, k_axes);   // the real axis
    line(fb, v, {0.0f, -2.0f}, {0.0f, 2.0f}, k_axes);   // the imaginary axis
}

// ---------------------------------------------------------------------------
// The app
// ---------------------------------------------------------------------------

class plane_app final : public engine::app
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
            else if (SDL_strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
            {
                // THE ARGUMENT IS HOISTED INTO A LOCAL, and this is not style.
                // `SDL_clamp` is a MACRO — `(((x) < (a)) ? (a) : (((x) > (b)) ?
                // (b) : (x)))` — so it expands `x` up to three times. Written as
                // `SDL_clamp(SDL_atoi(argv[++i]), 1, 4)` it advances `i` three
                // times, eats the next two flags, and the program silently ran
                // in the wrong mode; with `--t` it also swallowed `--shot` and
                // its path, so a headless run opened a window and hung. See
                // Lesson 7.3 §14. One named local per argument, always.
                const int wanted = SDL_atoi(argv[++i]);
                mode_ = std::clamp(wanted, 1, 4);
            }
            else if (SDL_strcmp(argv[i], "--angle") == 0 && i + 1 < argc)
            {
                angle_ = static_cast<float>(SDL_atof(argv[++i])) * k_rad;
            }
            else if (SDL_strcmp(argv[i], "--angle2") == 0 && i + 1 < argc)
            {
                angle2_ = static_cast<float>(SDL_atof(argv[++i])) * k_rad;
            }
            else if (SDL_strcmp(argv[i], "--t") == 0 && i + 1 < argc)
            {
                // FROZEN at the given t rather than started, so a `--shot` of a
                // blend is reproducible. The same decision the gimbal demo made
                // for the same reason: a headless run never calls on_fixed_step.
                const float wanted = static_cast<float>(SDL_atof(argv[++i]));
                t_ = std::clamp(wanted, 0.0f, 1.0f);
                animate_ = false;
            }
            else if (SDL_strcmp(argv[i], "--drift") == 0) { drift_ = true; }
        }

        return {.title = "plane — multiplication is rotation",
                .draw_to = (shot_path_ != nullptr) ? engine::surface::headless
                                                   : engine::surface::renderer,
                .fb_width = k_width,
                .fb_height = k_height};
    }

    [[nodiscard]] bool on_start() override
    {
        // A DEMO'S OWN CATEGORY IS THE DEMO'S TO ENABLE. `--log info` is applied
        // by `configure_logging` to the engine's five categories and deliberately
        // no further — `*` "applies to our categories only" (log.hpp §115) — and
        // `log_plane` is not one of them. Without this line every readout below
        // is computed, formatted and dropped, because SDL defaults a custom
        // category to ERROR. `collector` has the same gap and has never noticed,
        // because its one INFO line reports a shutdown nobody reads.
        SDL_SetLogPriority(log_plane, SDL_LOG_PRIORITY_INFO);

        ENGINE_LOG_INFO(log_plane, "plane — [1..4] mode  [Left]/[Right] and [Up]/[Down] angles");
        ENGINE_LOG_INFO(log_plane, "        [Space] animate  [R] drift  [N] renormalise  [Esc] quit");
        report();
        return true;
    }

    void on_event(const SDL_Event& event) override
    {
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) { return; }

        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE: request_quit(); break;
        case SDL_SCANCODE_1: mode_ = 1; report(); break;
        case SDL_SCANCODE_2: mode_ = 2; report(); break;
        case SDL_SCANCODE_3: mode_ = 3; report(); break;
        case SDL_SCANCODE_4: mode_ = 4; report(); break;
        case SDL_SCANCODE_SPACE: animate_ = !animate_; break;
        case SDL_SCANCODE_R: drift_ = !drift_; report(); break;
        case SDL_SCANCODE_N: renormalise_ = !renormalise_; report(); break;
        default: break;
        }
    }

    void on_fixed_step(float h) override
    {
        if (in().key_down(SDL_SCANCODE_LEFT))  { angle_ -= 60.0f * k_rad * h; report(); }
        if (in().key_down(SDL_SCANCODE_RIGHT)) { angle_ += 60.0f * k_rad * h; report(); }
        if (in().key_down(SDL_SCANCODE_UP))    { angle2_ += 60.0f * k_rad * h; report(); }
        if (in().key_down(SDL_SCANCODE_DOWN))  { angle2_ -= 60.0f * k_rad * h; report(); }

        if (animate_)
        {
            t_ += h * 0.25f;
            if (t_ > 1.0f) { t_ -= 1.0f; }
        }
    }

    void on_frame(float alpha) override
    {
        (void)alpha;
        engine::framebuffer& f = fb();
        f.clear(k_background);

        const view v;
        grid(f, v);
        circle(f, v, 1.0f, k_circle);

        switch (mode_)
        {
        case 1: draw_product(f, v); break;
        case 2: draw_powers(f, v); break;
        case 3: draw_mirrors(f, v); break;
        default: draw_blend(f, v); break;
        }

        if (shot_path_ != nullptr) { request_quit(engine::save_ppm(f, shot_path_)); }
    }

private:
    // ---- [1] the product as a spiral similarity ----------------------------
    //
    // TWO TRIANGLES, AND THEY ARE THE SAME TRIANGLE. Draw (0, 1, w) and
    // (0, z, zw). Multiplying every vertex of the first by z gives the second,
    // and multiplying by z is "scale by |z|, turn by arg z" — so the second
    // triangle is the first, scaled and turned. That is the geometric content
    // of complex multiplication, and it is the whole of §4.
    void draw_product(engine::framebuffer& f, const view& v)
    {
        const complex z = engine::complex_from_angle(angle_) * z_modulus();
        const complex w = engine::complex_from_angle(angle2_) * 1.35f;
        const complex zw = z * w;

        // The reference triangle: 0, 1, w. Its shape is what gets carried.
        line(f, v, {0.0f, 0.0f}, {1.0f, 0.0f}, k_ghost);
        line(f, v, {1.0f, 0.0f}, {w.re, w.im}, k_ghost);
        arrow(f, v, {0.0f, 0.0f}, {w.re, w.im}, k_w);
        dot(f, v, {1.0f, 0.0f}, k_ghost);

        // Its image under "multiply by z": 0, z, zw. Same shape, |z| times as
        // big, turned by arg z — which is why the two grey edges are parallel
        // only when arg z is zero.
        line(f, v, {0.0f, 0.0f}, {z.re, z.im}, k_ghost);
        line(f, v, {z.re, z.im}, {zw.re, zw.im}, k_ghost);
        arrow(f, v, {0.0f, 0.0f}, {z.re, z.im}, k_z);
        arrow(f, v, {0.0f, 0.0f}, {zw.re, zw.im}, k_result);

        // The two arcs: arg z, carried from 1 to z and from w to zw. Equal by
        // construction, and drawn so that the eye can check it.
        arc(f, v, 0.55f, 0.0f, engine::angle_from_complex(z), k_z);
        arc(f, v, 0.9f, engine::angle_from_complex(w),
            engine::angle_from_complex(w) + engine::angle_from_complex(z), k_result);
    }

    // ---- [2] powers, and the drift they accumulate -------------------------
    //
    // z^k for k = 0..n, each one a single complex multiply off the last. With
    // [R] the per-step error is multiplied by 1000 so it is visible at all: the
    // ring opens into a spiral, which is exactly what an un-renormalised
    // incremental rotation does in slow motion. [N] then closes it again.
    void draw_powers(engine::framebuffer& f, const view& v)
    {
        constexpr int k_steps = 96;
        complex step = engine::complex_from_angle(2.0f * k_pi / static_cast<float>(k_steps));

        // The drift is INJECTED rather than waited for. A float's real drift over
        // 96 steps is a millionth of a degree and invisible; scaling the step's
        // modulus by 1.0008 per multiply produces in one ring what a real
        // recurrence takes millions of steps to produce, and it is the same
        // failure. Saying so here matters — a figure that showed real drift at
        // this magnification would be a fabrication.
        if (drift_) { step = step * 1.003f; }

        complex z = complex::identity();
        vec2 previous{z.re, z.im};
        for (int k = 1; k <= k_steps; ++k)
        {
            z = z * step;
            if (renormalise_) { z = engine::renormalised_fast(z); }
            const vec2 now{z.re, z.im};
            fat_line(f, v, previous, now, k_result);
            if ((k % 8) == 0) { dot(f, v, now, k_z, 2); }
            previous = now;
        }

        // Where it ended up, against where it started. A closed ring means the
        // recurrence came home; a gap is the drift, drawn to scale.
        dot(f, v, {1.0f, 0.0f}, k_w, 4);
        dot(f, v, {z.re, z.im}, k_z, 4);
        if (engine::distance(vec2{z.re, z.im}, vec2{1.0f, 0.0f}) > 0.01f)
        {
            arrow(f, v, {1.0f, 0.0f}, {z.re, z.im}, k_mirror, false);
        }
    }

    // ---- [3] two mirrors, and the doubled angle ----------------------------
    //
    // THE HALF-ANGLE, VISIBLE. Two mirror lines at angles `angle_` and
    // `angle2_`; a probe point is reflected in the first and then in the second.
    // The net motion is a rotation by twice the angle BETWEEN the mirrors, and
    // the gold arrow is the rotor — the object at half the turn, which is the
    // thing a quaternion stores.
    void draw_mirrors(engine::framebuffer& f, const view& v)
    {
        const complex m0 = engine::complex_from_angle(angle_);
        const complex m1 = engine::complex_from_angle(angle2_);

        through_origin(f, v, m0, k_mirror);
        through_origin(f, v, m1, k_w);

        const vec2 start{1.25f, 0.0f};
        const vec2 once = engine::reflect_in_line(start, m0);
        const vec2 twice = engine::reflect_in_line(once, m1);

        // The journey, as three points and two hops. Each hop is a reflection,
        // and reflections reverse orientation — which is why two of them are
        // needed to get back to a rotation.
        fat_line(f, v, start, once, k_ghost);
        fat_line(f, v, once, twice, k_ghost);
        arrow(f, v, {0.0f, 0.0f}, start, k_probe);
        arrow(f, v, {0.0f, 0.0f}, once, k_ghost);
        arrow(f, v, {0.0f, 0.0f}, twice, k_result);
        dot(f, v, start, k_probe);
        dot(f, v, once, k_ghost);
        dot(f, v, twice, k_result);

        // The rotor: half the turn, drawn at a distinct radius so it cannot be
        // mistaken for the answer. `apply_rotor` uses it twice; drawn once, it
        // sits exactly halfway round, which is the picture of `theta/2`.
        const complex rotor = engine::rotor_from_mirrors(m0, m1);
        arrow(f, v, {0.0f, 0.0f}, vec2{rotor.re, rotor.im} * 0.75f, k_rotor);

        // The two arcs that are the claim: the angle between the mirrors, and
        // the angle the point actually travelled.
        for (const float r : {0.44f, 0.45f, 0.46f}) { arc(f, v, r, angle_, angle2_, k_mirror); }
        for (const float r : {1.44f, 1.45f, 1.46f})
        {
            arc(f, v, r, std::atan2(start.y, start.x), std::atan2(twice.y, twice.x), k_result);
        }
    }

    // ---- [4] slerp against nlerp -------------------------------------------
    //
    // SAME ARC, DIFFERENT SCHEDULE. Both paths lie on the circle — nlerp cannot
    // leave it, because the straight chord it follows never leaves the plane
    // the two endpoints span. What differs is WHERE ALONG the arc each one is
    // at a given t, and the tick marks at equal t are the entire figure.
    void draw_blend(engine::framebuffer& f, const view& v)
    {
        const complex a = engine::complex_from_angle(angle_);
        const complex b = engine::complex_from_angle(angle2_);

        // The chord nlerp actually walks, before the normalise pushes it out.
        line(f, v, vec2{a.re, a.im}, vec2{b.re, b.im}, k_grid);

        constexpr int k_ticks = 16;
        for (int k = 0; k <= k_ticks; ++k)
        {
            const float t = static_cast<float>(k) / static_cast<float>(k_ticks);
            const complex s = engine::complex_slerp(a, b, t);
            const complex n = engine::complex_nlerp(a, b, t);

            // Ticks pointing outward for slerp and inward for nlerp, on two
            // radii, so that "the same t is at a different angle" is the one
            // thing the picture shows.
            const vec2 su{s.re, s.im};
            const vec2 nu{n.re, n.im};
            fat_line(f, v, su * 1.10f, su * 1.22f, k_result);
            fat_line(f, v, nu * 0.78f, nu * 0.90f, k_z);
        }

        // The two moving points at the current t, which is where the lurch is
        // felt rather than seen: they separate in the middle and rejoin at
        // the ends.
        const complex s_now = engine::complex_slerp(a, b, t_);
        const complex n_now = engine::complex_nlerp(a, b, t_);
        arrow(f, v, {0.0f, 0.0f}, vec2{s_now.re, s_now.im} * 1.16f, k_result);
        arrow(f, v, {0.0f, 0.0f}, vec2{n_now.re, n_now.im} * 0.84f, k_z);
        dot(f, v, vec2{a.re, a.im}, k_w, 4);
        dot(f, v, vec2{b.re, b.im}, k_w, 4);

        // The un-normalised chord point, so the normalise step is not magic:
        // this is where a plain lerp would put you, and the tick above it is
        // where pushing it back to the circle puts you.
        const vec2 chord = engine::lerp(vec2{a.re, a.im}, vec2{b.re, b.im}, t_);
        dot(f, v, chord, k_ghost, 3);
        line(f, v, chord, vec2{n_now.re, n_now.im} * 0.84f, k_ghost);
    }

    /// The modulus of `z` in the product mode, swept with the angle so that the
    /// spiral-similarity picture shows a SCALE as well as a turn. A unit `z`
    /// would make the two triangles congruent and the word "similarity" would
    /// have nothing to point at.
    [[nodiscard]] float z_modulus() const { return 0.75f + 0.35f * std::cos(angle_ * 0.5f); }

    /// One line of numbers per change, so the picture always has a receipt.
    ///
    /// Printed rather than drawn: this demo has no ImGui layer, because adding
    /// one would put a dependency on `debug_ui` into a program whose entire
    /// point is that it needs two floats and a multiplication rule.
    void report() const
    {
        switch (mode_)
        {
        case 1:
        {
            const complex z = engine::complex_from_angle(angle_) * z_modulus();
            const complex w = engine::complex_from_angle(angle2_) * 1.35f;
            const complex zw = z * w;
            // EVERY READOUT FITS IN 66 COLUMNS, and that is not tidiness. These
            // lines are quoted verbatim in Lesson 7.3, whose output blocks
            // scroll rather than wrap, so anything past the fold is invisible
            // until a reader thinks to drag sideways — and what falls off the
            // end of a readout is always the answer. It also makes the demo
            // legible in an 80-column terminal.
            ENGINE_LOG_INFO(log_plane, "[1] |z| %.4f * |w| %.4f = |zw| %.4f (%.4f)",
                            static_cast<double>(engine::length(z)),
                            static_cast<double>(engine::length(w)),
                            static_cast<double>(engine::length(zw)),
                            static_cast<double>(engine::length(z) * engine::length(w)));
            ENGINE_LOG_INFO(log_plane, "    arg %+.2f + %+.2f = %+.2f deg (%+.2f)",
                            static_cast<double>(engine::angle_from_complex(z) / k_rad),
                            static_cast<double>(engine::angle_from_complex(w) / k_rad),
                            static_cast<double>(engine::angle_from_complex(zw) / k_rad),
                            static_cast<double>((engine::angle_from_complex(z)
                                                 + engine::angle_from_complex(w)) / k_rad));
            break;
        }
        case 2:
            ENGINE_LOG_INFO(log_plane, "[2] 96 powers, one multiply each.  drift x1000: %s   "
                            "renormalise: %s",
                            drift_ ? "ON" : "off", renormalise_ ? "ON" : "off");
            break;
        case 3:
        {
            const complex m0 = engine::complex_from_angle(angle_);
            const complex m1 = engine::complex_from_angle(angle2_);
            const complex rotor = engine::rotor_from_mirrors(m0, m1);
            ENGINE_LOG_INFO(log_plane, "[3] mirrors %+.2f and %+.2f deg (%.2f apart)",
                            static_cast<double>(angle_ / k_rad),
                            static_cast<double>(angle2_ / k_rad),
                            static_cast<double>((angle2_ - angle_) / k_rad));
            ENGINE_LOG_INFO(log_plane, "    rotor %+.2f deg -> rotation %+.2f deg",
                            static_cast<double>(engine::angle_from_complex(rotor) / k_rad),
                            static_cast<double>(engine::angle_from_complex(rotor * rotor)
                                                / k_rad));
            break;
        }
        default:
        {
            const complex a = engine::complex_from_angle(angle_);
            const complex b = engine::complex_from_angle(angle2_);
            const float omega = engine::angle_between(a, b);
            const float half = std::cos(0.5f * omega);
            ENGINE_LOG_INFO(log_plane, "[4] arc %.2f deg   nlerp sec^2(omega/2) = %.3f",
                            static_cast<double>(omega / k_rad),
                            static_cast<double>(1.0f / (half * half)));
            break;
        }
        }
    }

    // ---- State -------------------------------------------------------------
    const char* shot_path_ = nullptr;
    int mode_ = 1;
    float angle_ = 35.0f * k_rad;
    float angle2_ = 70.0f * k_rad;
    float t_ = 0.35f;
    bool animate_ = false;
    bool drift_ = false;
    bool renormalise_ = false;
};

} // namespace

ENGINE_MAIN(plane_app)
