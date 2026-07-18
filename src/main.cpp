// src/main.cpp — the engine's entry point.
//
// The loop is the one settled in Lesson 1.4 and does not change again:
//
//     drain events -> tick clock -> update input -> N fixed steps -> render
//
// Rendering means writing pixels into memory we own (Lesson 1.5), then handing
// that block to the GPU as a texture.
//
// What changed in 1.6 is what those pixel values *mean*. The demo is now a
// comparison board: two colour ramps, each drawn twice — once by interpolating
// the stored numbers, once by interpolating the light those numbers stand for.
// The two versions touch, so the seam between them is where the difference is
// most obvious. It is not subtle, and it is why Module 6 rebuilds the renderer
// around linear light.

#include "core/clock.hpp"
#include "core/fixed_step.hpp"
#include "core/input.hpp"
#include "gfx/colour.hpp"
#include "gfx/framebuffer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

#include <array>
#include <cstring>

namespace {

// ---- The framebuffer's size ------------------------------------------------
// 320x180 scaled 4x to a 1280x720 window, so one written pixel is a visible 4x4
// block. See Lesson 1.5.
constexpr int k_fb_width = 320;
constexpr int k_fb_height = 180;

// ---- The comparison board ---------------------------------------------------
constexpr int k_strip_x = 10;
constexpr int k_strip_w = 300;
constexpr int k_strip_h = 18;

constexpr int k_pair_a_y = 22;   // black -> white
constexpr int k_pair_b_y = 78;   // red   -> green

// ---- The bouncing ball, kept from Lesson 1.4 --------------------------------
// It stays because the loop underneath is still running and should look like it,
// and because its fading trail is the same gamma question in motion.
constexpr float k_gravity = 300.0f;
constexpr float k_ball_size = 6.0f;
constexpr float k_ball_start_y = 126.0f;
constexpr float k_floor_y = 172.0f;

constexpr Uint32 k_throttle_ms = 50;

/// Trail positions, one per simulation step.
constexpr std::size_t k_trail_length = 90;

struct sim_state
{
    float ball_y = k_ball_start_y;
    float ball_vy = 0.0f;
};

void step_simulation(sim_state& s, float h)
{
    s.ball_y += s.ball_vy * h;
    s.ball_vy += k_gravity * h;

    if (s.ball_y + k_ball_size > k_floor_y)
    {
        s.ball_y = k_floor_y - k_ball_size;
        s.ball_vy = -s.ball_vy;
    }
}

[[nodiscard]] float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

/// Draw one horizontal ramp between two colours, using the given mix function.
///
/// The signature is the point: `mix` is a pointer to either engine::mix_encoded
/// or engine::mix_linear, so the two strips of a pair differ in exactly one
/// thing — which function computed each column — and in nothing else.
void draw_ramp(engine::framebuffer& fb, int y, Uint32 from, Uint32 to,
               Uint32 (*mix)(Uint32, Uint32, float))
{
    for (int i = 0; i < k_strip_w; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(k_strip_w - 1);
        fb.fill_rect(k_strip_x + i, y, 1, k_strip_h, mix(from, to, t));
    }
}

/// Copy the framebuffer into a streaming texture, one row at a time.
///
/// Row by row because the pitch SDL returns is the GPU's, not ours: drivers pad
/// rows for alignment. Lesson 1.5 §4.3.
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

    // ---- 1. Initialise ------------------------------------------------------
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

    // ---- 2. Window and renderer --------------------------------------------
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

    // ---- 3. The framebuffer, and the texture that shows it -----------------
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

    // ---- 4. Subsystems and demo state --------------------------------------
    engine::clock clk;
    engine::input in;
    engine::fixed_step stepper(60.0f);

    sim_state previous;
    sim_state current;

    std::array<int, k_trail_length> trail_y{};
    std::size_t trail_head = 0;
    std::size_t trail_count = 0;

    // Which rule the ball's trail fades with. Both paths read the same data, so
    // pressing M swaps the arithmetic and nothing else.
    bool trail_linear = true;

    const Uint32 k_bg = engine::pack_argb(14, 14, 22);
    const Uint32 k_black = engine::pack_argb(0, 0, 0);
    const Uint32 k_white = engine::pack_argb(255, 255, 255);
    const Uint32 k_red = engine::pack_argb(255, 0, 0);
    const Uint32 k_green = engine::pack_argb(0, 255, 0);
    const Uint32 k_ball_colour = engine::pack_argb(126, 162, 236);
    const Uint32 k_floor_colour = engine::pack_argb(90, 90, 110);

    SDL_Log("Controls: [M] trail mix · [1-4] sim rate · [V] vsync · [T] throttle · [R] reset · [Esc] quit");
    SDL_Log("The stored value 128 emits %.1f%% of white's light; half the light is stored as %d",
            static_cast<double>(engine::srgb_to_linear_u8(128)) * 100.0,
            static_cast<int>(engine::linear_to_srgb_u8(0.5f)));

    // ---- 5. The loop --------------------------------------------------------
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

        // --- 5a. Controls ----------------------------------------------------
        if (in.key_pressed(SDL_SCANCODE_ESCAPE)) { running = false; }
        if (in.key_pressed(SDL_SCANCODE_M)) { trail_linear = !trail_linear; }

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
            trail_head = 0;
            trail_count = 0;
        }

        // --- 5b. Simulate ----------------------------------------------------
        stepper.begin_frame(clk.dt());
        while (stepper.next_step())
        {
            previous = current;
            step_simulation(current, stepper.h());

            trail_y[trail_head] = static_cast<int>(current.ball_y);
            trail_head = (trail_head + 1) % k_trail_length;
            if (trail_count < k_trail_length) { ++trail_count; }
        }

        const float alpha = stepper.alpha();

        // --- 5c. Draw ---------------------------------------------------------
        // The background no longer covers every pixel, so the clear is back —
        // exactly the situation Lesson 1.5 warned about when it removed one.
        fb.clear(k_bg);

        // The comparison board. Each pair has the naive mix on top and the
        // correct one directly beneath it, touching, so the eye can compare them
        // without having to travel.
        draw_ramp(fb, k_pair_a_y, k_black, k_white, engine::mix_encoded);
        draw_ramp(fb, k_pair_a_y + k_strip_h, k_black, k_white, engine::mix_linear);

        draw_ramp(fb, k_pair_b_y, k_red, k_green, engine::mix_encoded);
        draw_ramp(fb, k_pair_b_y + k_strip_h, k_red, k_green, engine::mix_linear);

        fb.fill_rect(0, static_cast<int>(k_floor_y), k_fb_width, 1, k_floor_colour);

        // The trail, fading from the ball's colour back to the background. The
        // newest dot is at full strength, the oldest at none; whether that fade
        // is computed on stored numbers or on light is what M switches.
        const int trail_x = k_fb_width / 2;
        for (std::size_t k = 0; k < trail_count; ++k)
        {
            const std::size_t idx = (trail_head + k_trail_length - 1 - k) % k_trail_length;
            const float age = (trail_count > 1)
                ? static_cast<float>(k) / static_cast<float>(trail_count - 1)
                : 0.0f;
            const float strength = 1.0f - age;

            const Uint32 dot = trail_linear
                ? engine::mix_linear(k_bg, k_ball_colour, strength)
                : engine::mix_encoded(k_bg, k_ball_colour, strength);

            fb.put_pixel(trail_x - 8, trail_y[idx], dot);
            fb.put_pixel(trail_x + 8, trail_y[idx], dot);
        }

        fb.fill_rect(trail_x - static_cast<int>(k_ball_size) / 2,
                     static_cast<int>(lerp(previous.ball_y, current.ball_y, alpha)),
                     static_cast<int>(k_ball_size), static_cast<int>(k_ball_size), k_ball_colour);

        // --- 5d. Present ------------------------------------------------------
        if (!upload(screen_texture, fb))
        {
            SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, screen_texture, nullptr, nullptr);

        // Labels, drawn over our framebuffer because we still have no font of our
        // own (Module 6). At 2x text scale over a 4x framebuffer scale, a text
        // coordinate is exactly twice a framebuffer coordinate.
        SDL_SetRenderScale(renderer, 2.0f, 2.0f);
        SDL_SetRenderDrawColor(renderer, 225, 225, 215, 255);
        SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                  "fps %7.1f   sim %3.0f Hz   the value 128 emits %.1f%% of white's light",
                                  static_cast<double>(clk.fps()),
                                  static_cast<double>(stepper.rate_hz()),
                                  static_cast<double>(engine::srgb_to_linear_u8(128)) * 100.0);
        SDL_RenderDebugText(renderer, 6.0f, 30.0f,
                            "black to white   upper: mixed as stored numbers   lower: mixed as light");
        SDL_RenderDebugText(renderer, 6.0f, 126.0f,
                            "red to green     upper: mixed as stored numbers   lower: mixed as light");
        SDL_RenderDebugTextFormat(renderer, 6.0f, 236.0f,
                                  "trail fade: %-18s [M] switch   [1-4] sim rate   [Esc] quit",
                                  trail_linear ? "mixed as light" : "mixed as numbers");
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);

        SDL_RenderPresent(renderer);

        if (in.key_down(SDL_SCANCODE_T))
        {
            SDL_Delay(k_throttle_ms);
        }
    }

    // ---- 6. Shut down, in reverse order of creation -------------------------
    SDL_DestroyTexture(screen_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
