// src/main.cpp — the engine's entry point.
//
// The frame now measures itself. Its shape:
//
//     drain events  ->  tick clock  ->  update input  ->  simulate  ->  render
//
// The clock is ticked before input rather than after only for tidiness — both
// must happen after the drain and before simulation, and nothing reads the clock
// during input's update.
//
// This lesson's demo exists to be *watched*, not admired: three moving things,
// each scaled by time differently, and two controls that change the frame rate
// out from under them. The disagreement between them is the lesson.

#include "core/clock.hpp"
#include "core/input.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

namespace {

// ---- Demo constants --------------------------------------------------------
// Named, because a number in the middle of an expression is a number nobody can
// question later.

/// The broken one: pixels per FRAME. Its real-world speed is whatever the
/// machine's frame rate happens to be, multiplied by this. Lesson 1.3 §1.
constexpr float k_pixels_per_frame = 6.0f;

/// The fixed one: pixels per SECOND. A speed expressed in units of time, so it
/// means the same thing on every machine.
constexpr float k_pixels_per_second = 360.0f;

constexpr float k_box_size = 40.0f;
constexpr float k_track_a_y = 130.0f;
constexpr float k_track_b_y = 210.0f;

/// Gravity for the bouncing ball, in pixels per second squared.
constexpr float k_gravity = 1200.0f;
constexpr float k_ball_size = 24.0f;
constexpr float k_ball_start_y = 260.0f;
constexpr float k_floor_y = 620.0f;

/// Milliseconds of artificial stall while the throttle key is held. Roughly
/// 20 fps — slow enough to break things visibly, not so slow the demo is
/// unusable.
constexpr Uint32 k_throttle_ms = 50;

struct rgb { Uint8 r, g, b; };

constexpr rgb k_bad   = {236, 122,  92};   // coral — the frame-rate-dependent box
constexpr rgb k_good  = {122, 196, 152};   // sage  — the time-scaled box
constexpr rgb k_ball  = {126, 162, 236};   // cornflower — the physics ball
constexpr rgb k_guide = { 90,  90, 110};   // the reference lines

/// Wrap a box back to the left edge once it has fully left the right edge.
[[nodiscard]] float wrap_x(float x, int window_w)
{
    const float limit = static_cast<float>(window_w);
    if (x > limit) { return -k_box_size; }
    return x;
}

} // namespace

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // ---- 1. Initialise the SDL subsystems we need --------------------------
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

    // ---- 2. Create the window ----------------------------------------------
    int window_w = 1280;
    int window_h = 720;

    SDL_Window* window = SDL_CreateWindow("Engine", window_w, window_h, SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // ---- 3. Create a renderer ----------------------------------------------
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Start with vsync OFF so the loop runs as fast as the machine allows. That
    // is not how a game should ship, but it is the honest default for measuring:
    // with vsync on, every dt is the monitor's refresh interval and the whole
    // problem is invisible because one machine's frame rate is being imposed on
    // the measurement. Press V at runtime to see both regimes.
    int vsync = SDL_RENDERER_VSYNC_DISABLED;
    SDL_SetRenderVSync(renderer, vsync);

    // ---- 4. Subsystems and demo state --------------------------------------
    engine::clock clk;
    engine::input in;

    float box_bad_x = 0.0f;      // moved per frame
    float box_good_x = 0.0f;     // moved per second

    float ball_y = k_ball_start_y;
    float ball_vy = 0.0f;

    SDL_Log("Controls: V toggles vsync · hold T to throttle to ~%d fps · R resets the ball · Esc quits",
            1000 / static_cast<int>(k_throttle_ms));

    // ---- 5. The main loop ---------------------------------------------------
    bool running = true;
    while (running)
    {
        // --- 5a. Drain the event queue COMPLETELY --------------------------
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

            case SDL_EVENT_WINDOW_RESIZED:
                window_w = event.window.data1;
                window_h = event.window.data2;
                break;

            default:
                break;
            }
        }

        // --- 5b. Measure the frame, then publish input ----------------------
        // The clock must be ticked exactly once, here. Tick it twice and the
        // second dt is near zero; tick it inside the simulation and different
        // systems integrate with different numbers.
        clk.tick();
        in.update();

        const float dt = clk.dt();

        // --- 5c. Handle controls -------------------------------------------
        if (in.key_pressed(SDL_SCANCODE_ESCAPE))
        {
            running = false;
        }

        // An edge — one toggle per press (Lesson 1.2). Holding V must not flip
        // vsync sixty times a second.
        if (in.key_pressed(SDL_SCANCODE_V))
        {
            vsync = (vsync == SDL_RENDERER_VSYNC_DISABLED) ? 1 : SDL_RENDERER_VSYNC_DISABLED;
            if (!SDL_SetRenderVSync(renderer, vsync))
            {
                // Not every backend can change this on the fly. Report and carry
                // on rather than pretending it worked.
                SDL_Log("SDL_SetRenderVSync(%d) failed: %s", vsync, SDL_GetError());
            }
            SDL_Log("vsync %s", vsync == SDL_RENDERER_VSYNC_DISABLED ? "OFF" : "ON");
        }

        if (in.key_pressed(SDL_SCANCODE_R))
        {
            ball_y = k_ball_start_y;
            ball_vy = 0.0f;
        }

        // --- 5d. Simulate ---------------------------------------------------

        // The bug, kept deliberately. Nothing here mentions time, so this box's
        // speed is "6 pixels times however many frames the machine manages".
        box_bad_x = wrap_x(box_bad_x + k_pixels_per_frame, window_w);

        // The fix. A speed in pixels per second, multiplied by the seconds that
        // actually elapsed. Frame rate cancels out — exactly, for constant
        // velocity. Lesson 1.3 §3.2.
        box_good_x = wrap_x(box_good_x + k_pixels_per_second * dt, window_w);

        // And the limit of that fix. This is dt-scaled too, by the same clock,
        // and it is still wrong: explicit Euler advances position using the
        // velocity from the *start* of the step, and the resulting error is
        // proportional to dt. With a perfectly elastic bounce the only thing
        // that can change the ball's energy is the integrator — so watch it
        // climb higher every bounce, and climb faster the slower the frame rate.
        // Lesson 1.3 §3.4; the integrators themselves are Module 7.
        ball_y += ball_vy * dt;
        ball_vy += k_gravity * dt;

        if (ball_y + k_ball_size > k_floor_y)
        {
            ball_y = k_floor_y - k_ball_size;
            ball_vy = -ball_vy;
        }

        // --- 5e. Render ------------------------------------------------------
        SDL_SetRenderDrawColor(renderer, 30, 30, 46, 255);
        SDL_RenderClear(renderer);

        // Reference lines: the floor, and the height the ball was dropped from.
        // A correct simulation would return to that line on every bounce.
        SDL_SetRenderDrawColor(renderer, k_guide.r, k_guide.g, k_guide.b, 255);
        SDL_RenderLine(renderer, 0.0f, k_floor_y, static_cast<float>(window_w), k_floor_y);
        SDL_RenderLine(renderer, 0.0f, k_ball_start_y, static_cast<float>(window_w), k_ball_start_y);

        const SDL_FRect box_bad{box_bad_x, k_track_a_y, k_box_size, k_box_size};
        SDL_SetRenderDrawColor(renderer, k_bad.r, k_bad.g, k_bad.b, 255);
        SDL_RenderFillRect(renderer, &box_bad);

        const SDL_FRect box_good{box_good_x, k_track_b_y, k_box_size, k_box_size};
        SDL_SetRenderDrawColor(renderer, k_good.r, k_good.g, k_good.b, 255);
        SDL_RenderFillRect(renderer, &box_good);

        const SDL_FRect ball{static_cast<float>(window_w) * 0.5f - k_ball_size * 0.5f,
                             ball_y, k_ball_size, k_ball_size};
        SDL_SetRenderDrawColor(renderer, k_ball.r, k_ball.g, k_ball.b, 255);
        SDL_RenderFillRect(renderer, &ball);

        // The readout. SDL_RenderDebugText is SDL's built-in 8x8 bitmap font —
        // no asset, no dependency, and exactly good enough for numbers you stare
        // at while debugging. Real text rendering arrives in Module 6. Scale 2x
        // so it is legible; note that the scale multiplies the coordinates too,
        // which is why the positions below are in half-pixels.
        SDL_SetRenderScale(renderer, 2.0f, 2.0f);
        SDL_SetRenderDrawColor(renderer, 220, 220, 210, 255);
        SDL_RenderDebugTextFormat(renderer, 8.0f, 8.0f,
                                  "fps %7.1f   dt %6.2f ms   raw %6.2f ms%s   frame %llu   up %.1f s",
                                  static_cast<double>(clk.fps()),
                                  static_cast<double>(clk.dt()) * 1000.0,
                                  static_cast<double>(clk.raw_dt()) * 1000.0,
                                  clk.was_clamped() ? "  CLAMPED" : "",
                                  static_cast<unsigned long long>(clk.frame_count()),
                                  clk.elapsed());
        SDL_RenderDebugTextFormat(renderer, 8.0f, 20.0f,
                                  "vsync %s    [V] toggle   [T] hold to throttle   [R] reset ball   [Esc] quit",
                                  vsync == SDL_RENDERER_VSYNC_DISABLED ? "OFF" : "ON ");
        SDL_RenderDebugText(renderer, 8.0f, 40.0f, "coral  6 px per FRAME     - speed follows the frame rate");
        SDL_RenderDebugText(renderer, 8.0f, 52.0f, "sage   360 px per SECOND  - speed does not");
        SDL_RenderDebugText(renderer, 8.0f, 64.0f, "blue   dt-scaled gravity  - climbs anyway; faster when slower");
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);

        SDL_RenderPresent(renderer);

        // --- 5f. The throttle ------------------------------------------------
        // A level, not an edge: it should stall every frame the key is held.
        // Placed at the very end so the stall lands *between* the present and the
        // next clock tick — which is precisely where a real hitch would land.
        if (in.key_down(SDL_SCANCODE_T))
        {
            SDL_Delay(k_throttle_ms);
        }
    }

    // ---- 6. Shut down, in reverse order of creation -------------------------
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
