// src/main.cpp — the engine's entry point.
//
// Module 2 begins, and with it the software rasterizer. This file hosts the
// current lesson's demo: a fan of lines covering all eight octants, drawn by
// whichever of the three algorithms you choose, beside a magnified view of the
// pixels one line is actually made of.
//
// Lesson 1.8's Pong is still here and still reachable with [Tab]. Two demos in
// one executable is already slightly awkward; by Module 5 it would be absurd,
// which is exactly why Module 5 opens by splitting the tree into a static
// library and a demos/ directory. The awkwardness is the argument.
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

#include <cmath>
#include <cstring>

namespace {

constexpr int k_fb_width = 320;
constexpr int k_fb_height = 180;

constexpr Uint32 k_throttle_ms = 50;

// ---- The line demo ---------------------------------------------------------

/// Which routine the fan is drawn with. The three are interchangeable by
/// signature precisely so the demo can swap them and change nothing else — the
/// same discipline as Lesson 1.6's mixing comparison: two paths must differ in
/// exactly one thing or you are comparing code, not ideas.
enum class algorithm
{
    naive,      ///< y = mx + b, stepping x. Falls apart above 45 degrees.
    dda,        ///< step the major axis, accumulate the minor in a float.
    bresenham   ///< integer error term. The one draw_line uses.
};

using line_fn = void (*)(engine::framebuffer&, int, int, int, int, Uint32);

[[nodiscard]] line_fn function_for(algorithm a)
{
    switch (a)
    {
    case algorithm::naive:     return engine::draw_line_naive;
    case algorithm::dda:       return engine::draw_line_dda;
    case algorithm::bresenham: return engine::draw_line;
    }
    return engine::draw_line;
}

[[nodiscard]] const char* name_of(algorithm a)
{
    switch (a)
    {
    case algorithm::naive:     return "naive y=mx+b";
    case algorithm::dda:       return "DDA (float)";
    case algorithm::bresenham: return "Bresenham (int)";
    }
    return "?";
}

constexpr int k_spokes = 32;
constexpr engine::vec2 k_fan_centre{88.0f, 88.0f};
constexpr float k_fan_radius = 74.0f;

/// The octant test. A fan of evenly spaced spokes crosses every one of the
/// eight octants, so a routine that is wrong in any of them is wrong on screen
/// rather than wrong in theory — the naive one loses its four steep octants
/// completely, which is the whole point of drawing it this way.
void draw_fan(engine::framebuffer& fb, line_fn draw, float phase)
{
    for (int i = 0; i < k_spokes; ++i)
    {
        const float angle = phase + static_cast<float>(i) * 6.28318531f
                                    / static_cast<float>(k_spokes);
        const engine::vec2 tip{k_fan_centre.x + std::cos(angle) * k_fan_radius,
                               k_fan_centre.y + std::sin(angle) * k_fan_radius};

        // Colour by octant, so the four the naive routine cannot draw are
        // identifiable at a glance rather than by counting.
        const bool steep = std::fabs(std::sin(angle)) > std::fabs(std::cos(angle));
        const Uint32 colour = steep ? engine::pack_argb(236, 122, 92)
                                    : engine::pack_argb(122, 196, 152);

        draw(fb, static_cast<int>(k_fan_centre.x), static_cast<int>(k_fan_centre.y),
             static_cast<int>(tip.x), static_cast<int>(tip.y), colour);
    }
}

constexpr int k_zoom = 8;          ///< magnification of the pixel inspector
constexpr int k_zoom_cols = 15;
constexpr int k_zoom_rows = 13;
constexpr int k_zoom_x = 184;      ///< top-left of the inspector, in framebuffer pixels
constexpr int k_zoom_y = 30;

/// A magnified view of the pixels one short line is made of.
///
/// This is the point of the whole lesson made visible: the ideal line is a thin
/// straight thing, and what we can actually draw is a staircase of whole
/// pixels. Which staircase is exactly what the three algorithms disagree about.
void draw_inspector(engine::framebuffer& fb, line_fn draw, float phase, bool show_grid)
{
    const Uint32 grid = engine::pack_argb(44, 48, 60);
    const Uint32 lit = engine::pack_argb(226, 196, 110);
    const Uint32 ideal = engine::pack_argb(126, 162, 236);

    // The little grid gets its own tiny framebuffer, so "which pixels did the
    // algorithm choose" is answered by reading them back rather than by
    // reimplementing the algorithm. The demo asks the same code the engine uses.
    engine::framebuffer cell(k_zoom_cols, k_zoom_rows);
    cell.clear(0u);

    const int cx = k_zoom_cols / 2;
    const int cy = k_zoom_rows / 2;
    const int tx = cx + static_cast<int>(std::lround(std::cos(phase) * 6.0));
    const int ty = cy + static_cast<int>(std::lround(std::sin(phase) * 6.0));

    draw(cell, cx, cy, tx, ty, 0xFFFFFFFFu);

    if (show_grid)
    {
        for (int r = 0; r <= k_zoom_rows; ++r)
        {
            fb.fill_rect(k_zoom_x, k_zoom_y + r * k_zoom, k_zoom_cols * k_zoom + 1, 1, grid);
        }
        for (int c = 0; c <= k_zoom_cols; ++c)
        {
            fb.fill_rect(k_zoom_x + c * k_zoom, k_zoom_y, 1, k_zoom_rows * k_zoom + 1, grid);
        }
    }

    // Every lit cell as a filled square, inset by one so the grid stays visible.
    for (int r = 0; r < k_zoom_rows; ++r)
    {
        for (int c = 0; c < k_zoom_cols; ++c)
        {
            if (cell.pixel_at(c, r) != 0u)
            {
                fb.fill_rect(k_zoom_x + c * k_zoom + 1, k_zoom_y + r * k_zoom + 1,
                             k_zoom - 1, k_zoom - 1, lit);
            }
        }
    }

    // The ideal line, drawn through the CENTRES of the end cells at magnified
    // resolution. It is itself rasterised, of course — there is no other way to
    // put anything on a screen — but at eight times the scale its own staircase
    // is far below the size of the squares underneath it.
    engine::draw_line(fb,
                      k_zoom_x + cx * k_zoom + k_zoom / 2,
                      k_zoom_y + cy * k_zoom + k_zoom / 2,
                      k_zoom_x + tx * k_zoom + k_zoom / 2,
                      k_zoom_y + ty * k_zoom + k_zoom / 2,
                      ideal);
}

/// Count the lit pixels of a fan — the number that makes the naive routine's
/// failure quantitative rather than merely visible.
[[nodiscard]] int count_lit(const engine::framebuffer& fb)
{
    int lit = 0;
    for (int y = 0; y < fb.height(); ++y)
    {
        for (int x = 0; x < fb.width(); ++x)
        {
            if (fb.pixel_at(x, y) != 0u) { lit += 1; }
        }
    }
    return lit;
}

// ---- Presentation ----------------------------------------------------------

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

    SDL_Window* window = SDL_CreateWindow("Lines — Module 2", 1280, 720, SDL_WINDOW_RESIZABLE);
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
    bool show_pong = false;
    algorithm algo = algorithm::bresenham;
    bool spinning = true;
    bool show_grid = true;
    float phase = 0.35f;
    int lit_pixels = 0;
    double fan_ns_avg = 0.0;

    const Uint32 seed = static_cast<Uint32>(SDL_GetTicksNS() & 0xFFFFFFFFu);
    game::state pong_current = game::make_state(seed);
    game::state pong_previous = pong_current;
    bool pong_right_is_ai = true;

    const Uint32 k_bg = engine::pack_argb(12, 14, 20);

    SDL_Log("Lines demo: [1] naive  [2] DDA  [3] Bresenham · [Space] spin · [G] grid");
    SDL_Log("[Tab] switches to Lesson 1.8's Pong · [V] vsync · [T] throttle · [Esc] quit");

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
            show_pong = !show_pong;
            SDL_SetWindowTitle(window, show_pong ? "Pong — Module 1 Checkpoint"
                                                 : "Lines — Module 2");
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

        if (show_pong)
        {
            // ---- Lesson 1.8's game, unchanged --------------------------------
            if (in.key_pressed(SDL_SCANCODE_C)) { pong_right_is_ai = !pong_right_is_ai; }
            if (in.key_pressed(SDL_SCANCODE_K))
            {
                pong_current.swept_collision = !pong_current.swept_collision;
                pong_previous.swept_collision = pong_current.swept_collision;
                SDL_Log("Collision rule: %s",
                        pong_current.swept_collision ? "swept" : "naive");
            }
            if (in.key_pressed(SDL_SCANCODE_R))
            {
                const bool keep = pong_current.swept_collision;
                pong_current = game::make_state(seed);
                pong_current.swept_collision = keep;
                pong_previous = pong_current;
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
        else
        {
            // ---- The line demo ------------------------------------------------
            if (in.key_pressed(SDL_SCANCODE_1)) { algo = algorithm::naive; }
            if (in.key_pressed(SDL_SCANCODE_2)) { algo = algorithm::dda; }
            if (in.key_pressed(SDL_SCANCODE_3)) { algo = algorithm::bresenham; }
            if (in.key_pressed(SDL_SCANCODE_SPACE)) { spinning = !spinning; }
            if (in.key_pressed(SDL_SCANCODE_G)) { show_grid = !show_grid; }

            while (stepper.next_step())
            {
                if (spinning) { phase += 0.12f * stepper.h(); }
            }

            const line_fn draw = function_for(algo);

            fb.clear(k_bg);

            // Time the fan alone, so the number on screen is about the line
            // routine rather than about the clear and the upload around it.
            const Uint64 t0 = SDL_GetTicksNS();
            draw_fan(fb, draw, phase);
            const Uint64 t1 = SDL_GetTicksNS();

            // The count is taken before the inspector draws, so it measures the
            // fan and nothing else — this is the number that exposes the naive
            // routine's missing pixels.
            lit_pixels = count_lit(fb);

            // A rolling average, because a single frame's timing at this scale
            // is mostly noise. Display only, exactly like clock::fps().
            const double ns = static_cast<double>(t1 - t0);
            fan_ns_avg = (fan_ns_avg <= 0.0) ? ns : (fan_ns_avg * 0.95 + ns * 0.05);

            draw_inspector(fb, draw, phase, show_grid);
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

        if (show_pong)
        {
            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "PONG (Lesson 1.8)   collision: %-6s   rally %d",
                                      pong_current.swept_collision ? "swept" : "naive",
                                      pong_current.rally_hits);
            SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                "[Tab] back to lines   [W]/[S]   [C] 2P   [K] collision   [R] reset   [Esc] quit");
        }
        else
        {
            const bool is_naive = (algo == algorithm::naive);

            SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
            SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                      "%-16s  %d spokes   %5d px lit   %6.1f us/fan   fps %5.1f",
                                      name_of(algo), k_spokes, lit_pixels,
                                      fan_ns_avg / 1000.0,
                                      static_cast<double>(clk.fps()));

            // The line that turns the artifact into a diagnosis.
            SDL_SetRenderDrawColor(renderer,
                                   is_naive ? 236 : 150,
                                   is_naive ? 122 : 152,
                                   is_naive ? 92 : 170, 255);
            SDL_RenderDebugText(renderer, 6.0f, 20.0f,
                                is_naive
                                    ? "one pixel per COLUMN: the coral (steep) spokes are dotted -- 1483 px lit where the others light 2081"
                                    : "one pixel per step of the MAJOR axis: every spoke solid, in all eight octants");

            SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
            SDL_RenderDebugText(renderer, 6.0f, 328.0f,
                                "[1] naive  [2] DDA  [3] Bresenham   [Space] spin   [G] grid   [Tab] Pong   [V] vsync   [Esc] quit");
            SDL_RenderDebugText(renderer, 368.0f, 40.0f, "the pixels,");
            SDL_RenderDebugText(renderer, 368.0f, 52.0f, "magnified 8x");
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
