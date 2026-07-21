// src/main.cpp — the engine's entry point.
//
// This file hosts the current lesson's demo. As of Lesson 2.2 that is triangles:
// a rotating filled triangle you can view as three intersecting half-planes,
// beside a coverage counter that shows exactly what the top-left fill rule is
// for.
//
// [Tab] now CYCLES three demos — triangles (2.2), lines (2.1), and Pong (1.8) —
// because deleting a working demo to make room would be a regression. Three is
// already too many for one executable, and the next one will be worse. That is
// not an oversight: Module 5 opens by splitting the tree into a static library
// and a demos/ directory, and this file is where the argument for doing so
// accumulates until it is impossible to ignore.
//
// The loop is the one settled in Lesson 1.4 and does not change again:
//
//     drain events -> tick clock -> update input -> N fixed steps -> render

#include "core/clock.hpp"
#include "core/fixed_step.hpp"
#include "core/input.hpp"
#include "game/pong.hpp"
#include "gfx/colour.hpp"
#include "gfx/framebuffer.hpp"
#include "gfx/raster.hpp"
#include "math/vec2.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr int k_fb_width = 320;
constexpr int k_fb_height = 180;

constexpr Uint32 k_throttle_ms = 50;

/// Which lesson's demo is on screen. [Tab] cycles.
enum class demo
{
    triangles,   ///< Lesson 2.2 — this lesson
    lines,       ///< Lesson 2.1
    pong         ///< Lesson 1.8
};

[[nodiscard]] demo next_demo(demo d)
{
    switch (d)
    {
    case demo::triangles: return demo::lines;
    case demo::lines:     return demo::pong;
    case demo::pong:      return demo::triangles;
    }
    return demo::triangles;
}

// ===========================================================================
// Lesson 2.2 — triangles
// ===========================================================================

/// How the left panel draws its triangle.
enum class tri_mode
{
    filled,      ///< fill_triangle: the finished article
    wireframe,   ///< draw_triangle: three lines, for comparison
    halfplanes   ///< colour every pixel by WHICH edge tests it passes
};

[[nodiscard]] const char* name_of(tri_mode m)
{
    switch (m)
    {
    case tri_mode::filled:     return "filled";
    case tri_mode::wireframe:  return "wireframe";
    case tri_mode::halfplanes: return "half-planes";
    }
    return "?";
}

/// Paint the three edge functions' signs across the triangle's bounding box.
///
/// This is the lesson's central claim made visible: a triangle is the region
/// where three half-planes overlap. A pixel passing all three tests is inside;
/// one passing two is in the "wrong" part of two half-planes, and so on. Written
/// with direct edge_function calls rather than the incremental form, because
/// here the point is what is being computed, not how cheaply.
void draw_halfplanes(engine::framebuffer& fb,
                     int x0, int y0, int x1, int y1, int x2, int y2)
{
    int area = engine::edge_function(x0, y0, x1, y1, x2, y2);
    if (area == 0) { return; }
    if (area < 0) { std::swap(x1, x2); std::swap(y1, y2); }

    const int min_x = std::max(0, std::min({x0, x1, x2}) - 14);
    const int min_y = std::max(0, std::min({y0, y1, y2}) - 14);
    const int max_x = std::min(fb.width() - 1, std::max({x0, x1, x2}) + 14);
    const int max_y = std::min(fb.height() - 1, std::max({y0, y1, y2}) + 14);

    for (int y = min_y; y <= max_y; ++y)
    {
        for (int x = min_x; x <= max_x; ++x)
        {
            const int w0 = engine::edge_function(x1, y1, x2, y2, x, y);
            const int w1 = engine::edge_function(x2, y2, x0, y0, x, y);
            const int w2 = engine::edge_function(x0, y0, x1, y1, x, y);

            const int passes = (w0 >= 0 ? 1 : 0) + (w1 >= 0 ? 1 : 0) + (w2 >= 0 ? 1 : 0);

            // Three shades outside, one bright fill inside — so "inside" reads
            // as a region rather than as a colour among colours.
            Uint32 c = 0;
            switch (passes)
            {
            case 3: c = engine::pack_argb(226, 196, 110); break;   // inside
            case 2: c = engine::pack_argb(56, 62, 84); break;
            case 1: c = engine::pack_argb(34, 38, 54); break;
            default: c = engine::pack_argb(20, 22, 32); break;
            }
            fb.put_pixel(x, y, c);
        }
    }
}

/// The fill rule, disabled — every boundary pixel claimed by every triangle.
///
/// Kept local to the demo rather than added to the engine, because it is not a
/// thing the engine should be able to do. Written the direct way so the
/// comparison against fill_triangle is a comparison of *rules*, not of loops.
void fill_no_rule(std::vector<int>& coverage, int w, int h,
                  int x0, int y0, int x1, int y1, int x2, int y2)
{
    int area = engine::edge_function(x0, y0, x1, y1, x2, y2);
    if (area == 0) { return; }
    if (area < 0) { std::swap(x1, x2); std::swap(y1, y2); }

    const int min_x = std::max(0, std::min({x0, x1, x2}));
    const int min_y = std::max(0, std::min({y0, y1, y2}));
    const int max_x = std::min(w - 1, std::max({x0, x1, x2}));
    const int max_y = std::min(h - 1, std::max({y0, y1, y2}));

    for (int y = min_y; y <= max_y; ++y)
    {
        for (int x = min_x; x <= max_x; ++x)
        {
            const int w0 = engine::edge_function(x1, y1, x2, y2, x, y);
            const int w1 = engine::edge_function(x2, y2, x0, y0, x, y);
            const int w2 = engine::edge_function(x0, y0, x1, y1, x, y);
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                coverage[static_cast<std::size_t>(y * w + x)] += 1;
            }
        }
    }
}

constexpr int k_cov_w = 52;    ///< the coverage grid, in its own small pixels
constexpr int k_cov_h = 52;
constexpr int k_cov_zoom = 2;
constexpr int k_cov_x = 196;   ///< where it lands in the framebuffer
constexpr int k_cov_y = 44;

/// Draw two triangles that share an edge, counting how many times each pixel is
/// written, then colour by that count.
///
/// This is the only honest way to show a fill rule working. With opaque colours
/// a double-drawn pixel looks exactly like a correctly-drawn one — the defect is
/// invisible precisely because the second write lands on top of the first. Count
/// instead, and the seam lights up.
///
/// @return the number of pixels written more than once.
int draw_coverage(engine::framebuffer& fb, bool use_rule)
{
    std::vector<int> coverage(static_cast<std::size_t>(k_cov_w * k_cov_h), 0);

    // An axis-aligned square split corner to corner — the single most common
    // piece of geometry there is, and deliberately not a rotated one.
    //
    // The reason matters. A pixel is only ever at risk of being drawn twice if
    // its centre lies EXACTLY on the shared edge, and for an edge of arbitrary
    // slope almost no pixel centres do. Rotate this quad and the defect shrinks
    // to two or three stray pixels — still wrong, but easy to dismiss as noise.
    // A 45-degree diagonal passes through every pixel centre along it, so the
    // whole seam doubles at once and the failure is impossible to miss.
    //
    // That is not a rigged demo, it is the realistic one: quads split into
    // triangle pairs, terrain grids and UI rectangles are overwhelmingly
    // axis-aligned, so the catastrophic case is also the common case.
    const int lo = 6;
    const int hi = k_cov_w - 7;

    const int ax = lo, ay = lo;    // shared edge runs from here…
    const int bx = hi, by = hi;    // …to here: slope exactly 1

    const int rx = hi, ry = lo;    // upper-right corner
    const int lx = lo, ly = hi;    // lower-left corner

    if (use_rule)
    {
        // Count through the real engine routine, by drawing each triangle into
        // its own buffer and reading it back. Slow, and it means the demo is
        // measuring the shipped code rather than a copy of it.
        for (int t = 0; t < 2; ++t)
        {
            engine::framebuffer scratch(k_cov_w, k_cov_h);
            scratch.clear(0u);
            if (t == 0) { engine::fill_triangle(scratch, ax, ay, bx, by, lx, ly, 0xFFFFFFFFu); }
            else        { engine::fill_triangle(scratch, ax, ay, rx, ry, bx, by, 0xFFFFFFFFu); }

            for (int y = 0; y < k_cov_h; ++y)
            {
                for (int x = 0; x < k_cov_w; ++x)
                {
                    if (scratch.pixel_at(x, y) != 0u)
                    {
                        coverage[static_cast<std::size_t>(y * k_cov_w + x)] += 1;
                    }
                }
            }
        }
    }
    else
    {
        fill_no_rule(coverage, k_cov_w, k_cov_h, ax, ay, bx, by, lx, ly);
        fill_no_rule(coverage, k_cov_w, k_cov_h, ax, ay, rx, ry, bx, by);
    }

    const Uint32 k_once = engine::pack_argb(122, 196, 152);   // correct
    const Uint32 k_twice = engine::pack_argb(236, 92, 92);    // drawn twice
    const Uint32 k_empty = engine::pack_argb(18, 20, 28);

    int doubled = 0;
    for (int y = 0; y < k_cov_h; ++y)
    {
        for (int x = 0; x < k_cov_w; ++x)
        {
            const int n = coverage[static_cast<std::size_t>(y * k_cov_w + x)];
            if (n > 1) { doubled += 1; }
            const Uint32 c = (n == 0) ? k_empty : (n == 1 ? k_once : k_twice);
            fb.fill_rect(k_cov_x + x * k_cov_zoom, k_cov_y + y * k_cov_zoom,
                         k_cov_zoom, k_cov_zoom, c);
        }
    }
    return doubled;
}

// ===========================================================================
// Lesson 2.1 — lines
// ===========================================================================

using line_fn = void (*)(engine::framebuffer&, int, int, int, int, Uint32);

constexpr int k_spokes = 32;
constexpr engine::vec2 k_fan_centre{88.0f, 88.0f};
constexpr float k_fan_radius = 74.0f;

/// A fan of spokes crosses every one of the eight octants, so a routine that is
/// wrong in any of them is wrong on screen rather than wrong in theory.
void draw_fan(engine::framebuffer& fb, line_fn draw, float phase)
{
    for (int i = 0; i < k_spokes; ++i)
    {
        const float angle = phase + static_cast<float>(i) * 6.28318531f
                                    / static_cast<float>(k_spokes);
        const engine::vec2 tip{k_fan_centre.x + std::cos(angle) * k_fan_radius,
                               k_fan_centre.y + std::sin(angle) * k_fan_radius};

        const bool steep = std::fabs(std::sin(angle)) > std::fabs(std::cos(angle));
        const Uint32 colour = steep ? engine::pack_argb(236, 122, 92)
                                    : engine::pack_argb(122, 196, 152);

        draw(fb, static_cast<int>(k_fan_centre.x), static_cast<int>(k_fan_centre.y),
             static_cast<int>(tip.x), static_cast<int>(tip.y), colour);
    }
}

// ===========================================================================
// Presentation
// ===========================================================================

/// Copy the framebuffer into a streaming texture, row by row.
///
/// Row by row rather than one big memcpy because the pitch SDL hands back may
/// exceed width*4 — some drivers pad each row — and copying as one block would
/// shear the image diagonally on exactly the machines you do not own. Lesson 1.5
/// §4.3.
[[nodiscard]] bool upload(SDL_Texture* texture, const engine::framebuffer& fb)
{
    void* dst_pixels = nullptr;
    int dst_pitch = 0;

    if (!SDL_LockTexture(texture, nullptr, &dst_pixels, &dst_pitch))
    {
        return false;
    }

    Uint8* const dst = static_cast<Uint8*>(dst_pixels);
    const int row_bytes = fb.pitch();

    for (int y = 0; y < fb.height(); ++y)
    {
        std::memcpy(dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(dst_pitch),
                    fb.row(y),
                    static_cast<std::size_t>(row_bytes));
    }

    SDL_UnlockTexture(texture);
    return true;
}

/// Turn the keyboard into the two numbers Pong's simulation wants.
[[nodiscard]] game::intent read_intent(const engine::input& in, bool right_is_ai)
{
    game::intent wanted;
    wanted.right_is_ai = right_is_ai;

    if (in.key_down(SDL_SCANCODE_W)) { wanted.left -= 1.0f; }
    if (in.key_down(SDL_SCANCODE_S)) { wanted.left += 1.0f; }

    if (in.key_down(SDL_SCANCODE_UP))   { wanted.right -= 1.0f; }
    if (in.key_down(SDL_SCANCODE_DOWN)) { wanted.right += 1.0f; }

    return wanted;
}

} // namespace

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const int sdl_version = SDL_GetVersion();
    SDL_Log("Engine starting — SDL %d.%d.%d",
            SDL_VERSIONNUM_MAJOR(sdl_version),
            SDL_VERSIONNUM_MINOR(sdl_version),
            SDL_VERSIONNUM_MICRO(sdl_version));

    SDL_Window* window = SDL_CreateWindow("Triangles — Module 2", 1280, 720,
                                          SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int vsync = 1;
    if (!SDL_SetRenderVSync(renderer, vsync))
    {
        SDL_Log("SDL_SetRenderVSync(%d) failed: %s — continuing unsynchronised",
                vsync, SDL_GetError());
        vsync = SDL_RENDERER_VSYNC_DISABLED;
    }

    engine::framebuffer fb(k_fb_width, k_fb_height);

    SDL_Texture* screen_texture = SDL_CreateTexture(renderer,
                                                    SDL_PIXELFORMAT_ARGB8888,
                                                    SDL_TEXTUREACCESS_STREAMING,
                                                    k_fb_width, k_fb_height);
    if (screen_texture == nullptr)
    {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureScaleMode(screen_texture, SDL_SCALEMODE_NEAREST);

    engine::clock clk;
    engine::input in;
    engine::fixed_step stepper(60.0f);

    // ---- Demo state --------------------------------------------------------
    demo which = demo::triangles;
    tri_mode mode = tri_mode::filled;
    bool use_fill_rule = true;
    bool spinning = true;
    float phase = 0.6f;

    int doubled_px = 0;
    double tri_ns_avg = 0.0;

    line_fn line_algo = engine::draw_line;
    const char* line_algo_name = "Bresenham (int)";

    const Uint32 seed = static_cast<Uint32>(SDL_GetTicksNS() & 0xFFFFFFFFu);
    game::state pong_current = game::make_state(seed);
    game::state pong_previous = pong_current;
    bool pong_right_is_ai = true;

    const Uint32 k_bg = engine::pack_argb(12, 14, 20);

    SDL_Log("Triangles: [1] filled  [2] wireframe  [3] half-planes · [R] fill rule · [Space] spin");
    SDL_Log("[Tab] cycles demos: triangles (2.2) -> lines (2.1) -> Pong (1.8)");
    SDL_Log("[V] vsync · [T] throttle · [Esc] quit");

    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            in.feed_event(event);

            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                SDL_Log("Quit requested");
                running = false;
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                SDL_Log("Window %u close requested", static_cast<unsigned>(event.window.windowID));
                running = false;
                break;

            default:
                break;
            }
        }

        clk.tick();
        in.update();

        if (in.key_pressed(SDL_SCANCODE_ESCAPE)) { running = false; }

        if (in.key_pressed(SDL_SCANCODE_TAB))
        {
            which = next_demo(which);
            SDL_SetWindowTitle(window,
                which == demo::triangles ? "Triangles — Module 2"
              : which == demo::lines     ? "Lines — Module 2"
                                         : "Pong — Module 1 Checkpoint");
        }

        if (in.key_pressed(SDL_SCANCODE_V))
        {
            vsync = (vsync == SDL_RENDERER_VSYNC_DISABLED) ? 1 : SDL_RENDERER_VSYNC_DISABLED;
            if (!SDL_SetRenderVSync(renderer, vsync))
            {
                SDL_Log("SDL_SetRenderVSync(%d) failed: %s", vsync, SDL_GetError());
            }
        }

        stepper.begin_frame(clk.dt());

        if (which == demo::pong)
        {
            // ---- Lesson 1.8's game, unchanged --------------------------------
            if (in.key_pressed(SDL_SCANCODE_C)) { pong_right_is_ai = !pong_right_is_ai; }
            if (in.key_pressed(SDL_SCANCODE_K))
            {
                pong_current.swept_collision = !pong_current.swept_collision;
                pong_previous.swept_collision = pong_current.swept_collision;
            }

            const game::intent wanted = read_intent(in, pong_right_is_ai);
            while (stepper.next_step())
            {
                pong_previous = pong_current;
                game::step(pong_current, wanted, stepper.h());
                if (pong_current.teleported) { pong_previous = pong_current; }
            }
            game::draw(fb, pong_previous, pong_current, stepper.alpha());
        }
        else if (which == demo::lines)
        {
            // ---- Lesson 2.1's fan ---------------------------------------------
            if (in.key_pressed(SDL_SCANCODE_1)) { line_algo = engine::draw_line_naive; line_algo_name = "naive y=mx+b"; }
            if (in.key_pressed(SDL_SCANCODE_2)) { line_algo = engine::draw_line_dda;   line_algo_name = "DDA (float)"; }
            if (in.key_pressed(SDL_SCANCODE_3)) { line_algo = engine::draw_line;       line_algo_name = "Bresenham (int)"; }
            if (in.key_pressed(SDL_SCANCODE_SPACE)) { spinning = !spinning; }

            while (stepper.next_step())
            {
                if (spinning) { phase += 0.12f * stepper.h(); }
            }

            fb.clear(k_bg);
            draw_fan(fb, line_algo, phase);
        }
        else
        {
            // ---- Lesson 2.2's triangles ---------------------------------------
            if (in.key_pressed(SDL_SCANCODE_1)) { mode = tri_mode::filled; }
            if (in.key_pressed(SDL_SCANCODE_2)) { mode = tri_mode::wireframe; }
            if (in.key_pressed(SDL_SCANCODE_3)) { mode = tri_mode::halfplanes; }
            if (in.key_pressed(SDL_SCANCODE_R)) { use_fill_rule = !use_fill_rule; }
            if (in.key_pressed(SDL_SCANCODE_SPACE)) { spinning = !spinning; }

            while (stepper.next_step())
            {
                if (spinning) { phase += 0.35f * stepper.h(); }
            }

            fb.clear(k_bg);

            // The left panel: one triangle, spun so it passes through every
            // orientation — including the two where its signed area changes sign.
            const engine::vec2 centre{92.0f, 96.0f};
            const float radius = 62.0f;
            int vx[3];
            int vy[3];
            for (int i = 0; i < 3; ++i)
            {
                const float a = phase + static_cast<float>(i) * 2.09439510f;   // 120 degrees
                vx[i] = static_cast<int>(std::lround(centre.x + std::cos(a) * radius));
                vy[i] = static_cast<int>(std::lround(centre.y + std::sin(a) * radius * 0.82f));
            }

            const Uint64 t0 = SDL_GetTicksNS();
            switch (mode)
            {
            case tri_mode::filled:
                engine::fill_triangle(fb, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2],
                                      engine::pack_argb(226, 196, 110));
                break;
            case tri_mode::wireframe:
                engine::draw_triangle(fb, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2],
                                      engine::pack_argb(226, 196, 110));
                break;
            case tri_mode::halfplanes:
                draw_halfplanes(fb, vx[0], vy[0], vx[1], vy[1], vx[2], vy[2]);
                break;
            }
            const Uint64 t1 = SDL_GetTicksNS();

            const double ns = static_cast<double>(t1 - t0);
            tri_ns_avg = (tri_ns_avg <= 0.0) ? ns : (tri_ns_avg * 0.95 + ns * 0.05);

            doubled_px = draw_coverage(fb, use_fill_rule);
        }

        if (!upload(screen_texture, fb))
        {
            SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, screen_texture, nullptr, nullptr);

        // A text coordinate is exactly twice a framebuffer coordinate: 2x text
        // scale over a 4x framebuffer scale.
        SDL_SetRenderScale(renderer, 2.0f, 2.0f);

        if (which == demo::pong)
        {
            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "PONG (Lesson 1.8)   collision: %-6s   rally %d",
                                      pong_current.swept_collision ? "swept" : "naive",
                                      pong_current.rally_hits);
            SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                "[Tab] next demo   [W]/[S]   [C] 2P   [K] collision   [Esc] quit");
        }
        else if (which == demo::lines)
        {
            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "LINES (Lesson 2.1)   %-16s   %d spokes   fps %5.1f",
                                      line_algo_name, k_spokes,
                                      static_cast<double>(clk.fps()));
            SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                "[1] naive  [2] DDA  [3] Bresenham   [Space] spin   [Tab] next demo   [Esc] quit");
        }
        else
        {
            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "TRIANGLES   mode: %-12s   %6.1f us/triangle   fps %5.1f",
                                      name_of(mode), tri_ns_avg / 1000.0,
                                      static_cast<double>(clk.fps()));

            // The line that turns the coverage picture into a diagnosis.
            SDL_SetRenderDrawColor(renderer,
                                   doubled_px > 0 ? 236 : 150,
                                   doubled_px > 0 ? 92 : 152,
                                   doubled_px > 0 ? 92 : 170, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 20.0f,
                                      "[R] fill rule: %-3s    shared edge drawn twice on %d px",
                                      use_fill_rule ? "ON" : "OFF", doubled_px);

            SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
            SDL_RenderDebugText(renderer, 400.0f, 40.0f, "two triangles,");
            SDL_RenderDebugText(renderer, 400.0f, 52.0f, "one shared edge");
            SDL_RenderDebugText(renderer, 400.0f, 68.0f, "green = drawn once");
            SDL_RenderDebugText(renderer, 400.0f, 80.0f, "red   = drawn twice");
            SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                "[1] filled  [2] wireframe  [3] half-planes   [R] fill rule   [Space] spin   [Tab] next demo");
        }

        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
        SDL_RenderPresent(renderer);

        if (in.key_down(SDL_SCANCODE_T))
        {
            SDL_Delay(k_throttle_ms);
        }
    }

    SDL_DestroyTexture(screen_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
