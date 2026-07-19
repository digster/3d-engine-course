// src/main.cpp — the engine's entry point.
//
// The loop is the one settled in Lesson 1.4 and does not change again:
//
//     drain events -> tick clock -> update input -> N fixed steps -> render
//
// Lesson 1.7 gives the engine vectors, and the demo is in two halves. Above: the
// dot product as a shadow — drag the mouse and watch one arrow's projection onto
// another, with the number that describes it. Below: two squares driven by the
// same keys, one moving by a raw input vector and one by a normalised one, so the
// 41% diagonal error from Exercise 1.2.3 is finally visible and finally fixed.

#include "core/clock.hpp"
#include "core/fixed_step.hpp"
#include "core/input.hpp"
#include "gfx/colour.hpp"
#include "gfx/framebuffer.hpp"
#include "math/vec2.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr int k_fb_width = 320;
constexpr int k_fb_height = 180;

// ---- Panel A: the dot product ----------------------------------------------
constexpr engine::vec2 k_dot_origin{86.0f, 46.0f};
constexpr engine::vec2 k_reference{62.0f, 0.0f};   // the arrow we project onto

// ---- Panel B: the normalisation race ---------------------------------------
constexpr float k_speed = 55.0f;          // pixels per second
constexpr float k_square = 8.0f;
constexpr float k_panel_top = 108.0f;
constexpr float k_panel_bottom = 172.0f;
constexpr float k_start_x = 24.0f;

constexpr Uint32 k_throttle_ms = 50;

struct sim_state
{
    engine::vec2 raw{k_start_x, 126.0f};      // moved by the input vector as-is
    engine::vec2 unit{k_start_x, 150.0f};     // moved by the normalised input
    float raw_travelled = 0.0f;
    float unit_travelled = 0.0f;
};

/// Advance both squares using the same input direction, one normalised and one not.
void step_simulation(sim_state& s, engine::vec2 input, float h)
{
    // The bug, kept side by side with its fix. `input` is whatever the keys add up
    // to: (1,0) for one key, (1,-1) for two. Its length is 1 in the first case and
    // sqrt(2) in the second, so scaling it directly by speed makes diagonals
    // travel 41.4% further per second.
    const engine::vec2 raw_step = input * (k_speed * h);

    // The fix: take the direction only, then choose the speed separately.
    const engine::vec2 unit_step = engine::normalised(input) * (k_speed * h);

    s.raw += raw_step;
    s.unit += unit_step;
    s.raw_travelled += engine::length(raw_step);
    s.unit_travelled += engine::length(unit_step);

    const float max_x = static_cast<float>(k_fb_width) - k_square;
    s.raw.x = std::clamp(s.raw.x, 0.0f, max_x);
    s.unit.x = std::clamp(s.unit.x, 0.0f, max_x);
    s.raw.y = std::clamp(s.raw.y, k_panel_top, k_panel_bottom - k_square);
    s.unit.y = std::clamp(s.unit.y, k_panel_top, k_panel_bottom - k_square);
}

/// The naive line from Exercise 1.5.4: step whichever axis changes more.
///
/// Lesson 2.1 derives this properly (it is called DDA), shows why Bresenham
/// replaced it, and moves the result into the engine. It lives here, local to the
/// demo, because a lesson about vectors needs to draw arrows and it would be
/// silly to wait.
void draw_line(engine::framebuffer& fb, engine::vec2 a, engine::vec2 b, Uint32 colour)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const int steps = static_cast<int>(std::max(std::fabs(dx), std::fabs(dy)));

    if (steps <= 0)
    {
        fb.put_pixel(static_cast<int>(a.x), static_cast<int>(a.y), colour);
        return;
    }

    const float step_x = dx / static_cast<float>(steps);
    const float step_y = dy / static_cast<float>(steps);
    engine::vec2 p = a;

    for (int i = 0; i <= steps; ++i)
    {
        fb.put_pixel(static_cast<int>(p.x + 0.5f), static_cast<int>(p.y + 0.5f), colour);
        p.x += step_x;
        p.y += step_y;
    }
}

/// A line with gaps, for construction lines that should not compete with the
/// arrows they annotate.
void draw_dashed(engine::framebuffer& fb, engine::vec2 a, engine::vec2 b, Uint32 colour)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const int steps = static_cast<int>(std::max(std::fabs(dx), std::fabs(dy)));
    if (steps <= 0) { return; }

    const float step_x = dx / static_cast<float>(steps);
    const float step_y = dy / static_cast<float>(steps);
    engine::vec2 p = a;

    for (int i = 0; i <= steps; ++i)
    {
        if ((i / 3) % 2 == 0)
        {
            fb.put_pixel(static_cast<int>(p.x + 0.5f), static_cast<int>(p.y + 0.5f), colour);
        }
        p.x += step_x;
        p.y += step_y;
    }
}

/// An arrow: a line with a small solid block at the tip.
void draw_arrow(engine::framebuffer& fb, engine::vec2 from, engine::vec2 to, Uint32 colour)
{
    draw_line(fb, from, to, colour);
    fb.fill_rect(static_cast<int>(to.x) - 1, static_cast<int>(to.y) - 1, 3, 3, colour);
}

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

    SDL_Window* window = SDL_CreateWindow("Engine", 1280, 720, SDL_WINDOW_RESIZABLE);
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

    int vsync = SDL_RENDERER_VSYNC_DISABLED;
    SDL_SetRenderVSync(renderer, vsync);

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

    sim_state previous;
    sim_state current;

    const Uint32 k_bg = engine::pack_argb(14, 14, 22);
    const Uint32 k_axis = engine::pack_argb(70, 70, 90);
    const Uint32 k_ref_colour = engine::pack_argb(226, 196, 110);   // b, the reference
    const Uint32 k_arrow_colour = engine::pack_argb(126, 162, 236); // a, to the mouse
    const Uint32 k_shadow_colour = engine::pack_argb(122, 196, 152);
    const Uint32 k_raw_colour = engine::pack_argb(236, 122, 92);
    const Uint32 k_unit_colour = engine::pack_argb(122, 196, 152);

    SDL_Log("Move the mouse over the top panel; drive the squares with WASD or the arrows.");
    SDL_Log("Controls: [R] reset · [1-4] sim rate · [V] vsync · [T] throttle · [Esc] quit");

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
        if (in.key_pressed(SDL_SCANCODE_1)) { stepper.set_rate(10.0f); }
        if (in.key_pressed(SDL_SCANCODE_2)) { stepper.set_rate(30.0f); }
        if (in.key_pressed(SDL_SCANCODE_3)) { stepper.set_rate(60.0f); }
        if (in.key_pressed(SDL_SCANCODE_4)) { stepper.set_rate(120.0f); }

        if (in.key_pressed(SDL_SCANCODE_V))
        {
            vsync = (vsync == SDL_RENDERER_VSYNC_DISABLED) ? 1 : SDL_RENDERER_VSYNC_DISABLED;
            if (!SDL_SetRenderVSync(renderer, vsync))
            {
                SDL_Log("SDL_SetRenderVSync(%d) failed: %s", vsync, SDL_GetError());
            }
        }

        if (in.key_pressed(SDL_SCANCODE_R))
        {
            previous = sim_state{};
            current = sim_state{};
        }

        // --- Gather the input as a VECTOR, not as two independent numbers ------
        // This is the shape the rest of the engine will use: one arrow describing
        // "which way", separate from "how fast".
        engine::vec2 input{0.0f, 0.0f};
        if (in.key_down(SDL_SCANCODE_A) || in.key_down(SDL_SCANCODE_LEFT))  { input.x -= 1.0f; }
        if (in.key_down(SDL_SCANCODE_D) || in.key_down(SDL_SCANCODE_RIGHT)) { input.x += 1.0f; }
        if (in.key_down(SDL_SCANCODE_W) || in.key_down(SDL_SCANCODE_UP))    { input.y -= 1.0f; }
        if (in.key_down(SDL_SCANCODE_S) || in.key_down(SDL_SCANCODE_DOWN))  { input.y += 1.0f; }

        stepper.begin_frame(clk.dt());
        while (stepper.next_step())
        {
            previous = current;
            step_simulation(current, input, stepper.h());
        }

        const float alpha = stepper.alpha();

        // --- Where is the mouse, in framebuffer pixels? -----------------------
        // The framebuffer is stretched over the whole window, so converting is a
        // ratio of widths. Doing it as a vector keeps the two axes together.
        int window_w = 1280;
        int window_h = 720;
        SDL_GetWindowSize(window, &window_w, &window_h);

        const engine::vec2 mouse_fb{
            in.mouse_x() * static_cast<float>(k_fb_width) / static_cast<float>(window_w),
            in.mouse_y() * static_cast<float>(k_fb_height) / static_cast<float>(window_h)};

        // --- The dot product, as geometry -------------------------------------
        const engine::vec2 a = mouse_fb - k_dot_origin;   // arrow to the cursor
        const engine::vec2 b = k_reference;               // arrow we project onto
        const float a_dot_b = engine::dot(a, b);
        const float len_a = engine::length(a);
        const float len_b = engine::length(b);
        const engine::vec2 shadow = engine::project_onto(a, b);
        const float cos_theta = (len_a > 0.0f) ? a_dot_b / (len_a * len_b) : 0.0f;

        // --- Draw --------------------------------------------------------------
        fb.clear(k_bg);

        // Panel A: a horizontal guide through the origin, so the projection has a
        // visible line to fall on even when it runs backwards.
        draw_line(fb, {4.0f, k_dot_origin.y}, {static_cast<float>(k_fb_width) - 4.0f, k_dot_origin.y}, k_axis);

        // The shadow, drawn first so the arrows sit on top of it.
        draw_line(fb, k_dot_origin, k_dot_origin + shadow, k_shadow_colour);
        draw_dashed(fb, mouse_fb, k_dot_origin + shadow, k_axis);

        draw_arrow(fb, k_dot_origin, k_dot_origin + b, k_ref_colour);
        draw_arrow(fb, k_dot_origin, mouse_fb, k_arrow_colour);

        // Panel B: the divider, then the two squares.
        fb.fill_rect(0, static_cast<int>(k_panel_top) - 6, k_fb_width, 1, k_axis);

        const engine::vec2 raw_pos = engine::lerp(previous.raw, current.raw, alpha);
        const engine::vec2 unit_pos = engine::lerp(previous.unit, current.unit, alpha);

        fb.fill_rect(static_cast<int>(raw_pos.x), static_cast<int>(raw_pos.y),
                     static_cast<int>(k_square), static_cast<int>(k_square), k_raw_colour);
        fb.fill_rect(static_cast<int>(unit_pos.x), static_cast<int>(unit_pos.y),
                     static_cast<int>(k_square), static_cast<int>(k_square), k_unit_colour);

        if (!upload(screen_texture, fb))
        {
            SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, screen_texture, nullptr, nullptr);

        // Labels. A text coordinate is exactly twice a framebuffer coordinate:
        // 2x text scale over a 4x framebuffer scale.
        const float ratio = (current.unit_travelled > 0.01f)
            ? current.raw_travelled / current.unit_travelled
            : 1.0f;

        SDL_SetRenderScale(renderer, 2.0f, 2.0f);
        SDL_SetRenderDrawColor(renderer, 225, 225, 215, 255);
        SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                  "a . b = %8.1f    |a| = %6.1f  |b| = %5.1f    cos = %+5.2f   %s",
                                  static_cast<double>(a_dot_b),
                                  static_cast<double>(len_a),
                                  static_cast<double>(len_b),
                                  static_cast<double>(cos_theta),
                                  a_dot_b > 0.0f ? "in front" : (a_dot_b < 0.0f ? "behind" : "exactly side-on"));
        SDL_RenderDebugText(renderer, 6.0f, 20.0f,
                            "blue = a (to the mouse)   amber = b   green = a's shadow on b");
        SDL_RenderDebugTextFormat(renderer, 6.0f, 210.0f,
                                  "coral: input used raw      travelled %7.1f px",
                                  static_cast<double>(current.raw_travelled));
        SDL_RenderDebugTextFormat(renderer, 6.0f, 224.0f,
                                  "green: input normalised    travelled %7.1f px",
                                  static_cast<double>(current.unit_travelled));
        SDL_RenderDebugTextFormat(renderer, 6.0f, 238.0f,
                                  "ratio %5.3f  (hold a diagonal and watch it approach 1.414)   [R] reset  [Esc] quit",
                                  static_cast<double>(ratio));
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
