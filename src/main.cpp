// src/main.cpp — the engine's entry point.
//
// The loop is now the one the engine keeps. Its shape:
//
//     drain events -> tick clock -> update input -> step simulation N times -> render
//
// The simulation advances in fixed steps of a size we chose, as many as the
// elapsed real time pays for; rendering happens once, afterwards, at whatever
// rate the machine manages. The leftover fraction of a step becomes `alpha`,
// which the renderer uses to interpolate between the last two simulation states.
//
// The demo exists to make all of that visible: three boxes moving at the same
// speed by three different rules, and a ball whose bounce height no longer
// depends on your monitor.

#include "core/clock.hpp"
#include "core/fixed_step.hpp"
#include "core/input.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

namespace {

// ---- Demo constants --------------------------------------------------------

/// Every box moves at this speed. Expressed per second, per Lesson 1.3.
constexpr float k_pixels_per_second = 360.0f;

constexpr float k_box_size = 40.0f;
constexpr float k_track_var_y = 120.0f;   // variable dt
constexpr float k_track_raw_y = 190.0f;   // fixed step, no interpolation
constexpr float k_track_lerp_y = 260.0f;  // fixed step, interpolated

constexpr float k_gravity = 1200.0f;
constexpr float k_ball_size = 24.0f;
constexpr float k_ball_start_y = 380.0f;
constexpr float k_floor_y = 640.0f;

constexpr Uint32 k_throttle_ms = 50;      // ~20 fps while held

struct rgb { Uint8 r, g, b; };

constexpr rgb k_var   = {236, 122,  92};  // coral      — variable dt
constexpr rgb k_raw   = {226, 196, 110};  // amber      — fixed step, raw
constexpr rgb k_lerp  = {122, 196, 152};  // sage       — fixed step, interpolated
constexpr rgb k_ball  = {126, 162, 236};  // cornflower — the ball
constexpr rgb k_guide = { 90,  90, 110};

/// Everything the simulation owns.
///
/// Grouping it in one struct is what makes interpolation possible: the renderer
/// needs the state as it was one step ago *and* as it is now, so the loop keeps
/// two of these and copies one over the other. That is the real architectural
/// cost of interpolation — every renderable quantity has to exist twice — and it
/// is the reason this struct exists instead of a handful of loose floats.
struct sim_state
{
    float box_x = 0.0f;
    float box_dir = 1.0f;
    float ball_y = k_ball_start_y;
    float ball_vy = 0.0f;
};

/// Advance the simulation by exactly one fixed step.
///
/// Note the parameter: `h`, the step size, never the frame's real duration. This
/// function has no way to know how long the last frame took, and that is the
/// entire point — it cannot be affected by the machine's speed.
void step_simulation(sim_state& s, float h, int window_w)
{
    // A box bouncing between the window edges. Bouncing rather than wrapping is
    // deliberate: a wrap is a teleport, and interpolating across a teleport draws
    // the object sliding backwards across the whole screen. See the lesson's
    // pitfalls — position must be continuous for a lerp to mean anything.
    s.box_x += k_pixels_per_second * s.box_dir * h;
    const float max_x = static_cast<float>(window_w) - k_box_size;
    if (s.box_x > max_x) { s.box_x = max_x; s.box_dir = -1.0f; }
    if (s.box_x < 0.0f)  { s.box_x = 0.0f;  s.box_dir =  1.0f; }

    // The same explicit Euler integration as Lesson 1.3, and still first-order:
    // it will still add energy the bounce cannot account for. What has changed is
    // that `h` is now a constant we chose, so the error is identical on every
    // machine. Consistent, not correct — Module 7 makes it correct.
    s.ball_y += s.ball_vy * h;
    s.ball_vy += k_gravity * h;

    if (s.ball_y + k_ball_size > k_floor_y)
    {
        s.ball_y = k_floor_y - k_ball_size;
        s.ball_vy = -s.ball_vy;
    }
}

/// Linear interpolation. Module 2 builds the real math library; one line of it
/// early is better than reaching for the whole thing before we need it.
[[nodiscard]] float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
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
    int window_w = 1280;
    int window_h = 720;

    SDL_Window* window = SDL_CreateWindow("Engine", window_w, window_h, SDL_WINDOW_RESIZABLE);
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

    // Vsync off by default: with it on, only one frame rate is available and the
    // relationship between frame rate and simulation rate cannot be explored.
    int vsync = SDL_RENDERER_VSYNC_DISABLED;
    SDL_SetRenderVSync(renderer, vsync);

    // ---- 3. Subsystems and demo state --------------------------------------
    engine::clock clk;
    engine::input in;
    engine::fixed_step stepper(60.0f);

    // The two simulation states interpolation needs. `current` is the newest
    // completed step; `previous` is the one before it.
    sim_state previous;
    sim_state current;

    // The Lesson 1.3 comparison box, still integrated with the raw frame delta.
    float var_box_x = 0.0f;
    float var_box_dir = 1.0f;

    SDL_Log("Controls: [1-4] sim rate 10/30/60/120 Hz · [V] vsync · [T] hold to throttle · [R] reset · [Esc] quit");

    // ---- 4. The loop --------------------------------------------------------
    bool running = true;
    while (running)
    {
        // --- 4a. Drain events (this also pumps SDL's input state) -----------
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

        // --- 4b. Measure the frame, publish input ---------------------------
        clk.tick();
        in.update();

        // --- 4c. Controls ----------------------------------------------------
        if (in.key_pressed(SDL_SCANCODE_ESCAPE)) { running = false; }

        // Rate changes happen here, before begin_frame, so the accumulator is
        // never re-diced in the middle of a step loop.
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
            var_box_x = 0.0f;
            var_box_dir = 1.0f;
        }

        // --- 4d. Simulate: zero or more fixed steps -------------------------
        // The clamped dt goes in, whole steps come out. Note that `previous` is
        // updated inside the loop, not outside it: it must end up holding the
        // second-newest state, and a frame may take several steps or none.
        stepper.begin_frame(clk.dt());
        while (stepper.next_step())
        {
            previous = current;
            step_simulation(current, stepper.h(), window_w);
        }

        const float alpha = stepper.alpha();

        if (stepper.dropped_seconds() > 0.0f)
        {
            SDL_Log("Step cap hit: discarded %.1f ms of simulation time",
                    static_cast<double>(stepper.dropped_seconds()) * 1000.0);
        }

        // The comparison box, still stepped by the raw frame delta. It is smooth,
        // because it moves exactly as far as the elapsed time says — and it is
        // non-deterministic for precisely the same reason.
        var_box_x += k_pixels_per_second * var_box_dir * clk.dt();
        const float var_max_x = static_cast<float>(window_w) - k_box_size;
        if (var_box_x > var_max_x) { var_box_x = var_max_x; var_box_dir = -1.0f; }
        if (var_box_x < 0.0f)      { var_box_x = 0.0f;      var_box_dir =  1.0f; }

        // --- 4e. Render ------------------------------------------------------
        SDL_SetRenderDrawColor(renderer, 30, 30, 46, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, k_guide.r, k_guide.g, k_guide.b, 255);
        SDL_RenderLine(renderer, 0.0f, k_floor_y, static_cast<float>(window_w), k_floor_y);
        SDL_RenderLine(renderer, 0.0f, k_ball_start_y, static_cast<float>(window_w), k_ball_start_y);

        // Coral: variable dt.
        const SDL_FRect box_var{var_box_x, k_track_var_y, k_box_size, k_box_size};
        SDL_SetRenderDrawColor(renderer, k_var.r, k_var.g, k_var.b, 255);
        SDL_RenderFillRect(renderer, &box_var);

        // Amber: the fixed-step state, drawn exactly as simulated. Correct, and
        // it judders — the simulation's position is up to one step stale, by a
        // varying amount, and the eye reads that variation as stutter.
        const SDL_FRect box_raw{current.box_x, k_track_raw_y, k_box_size, k_box_size};
        SDL_SetRenderDrawColor(renderer, k_raw.r, k_raw.g, k_raw.b, 255);
        SDL_RenderFillRect(renderer, &box_raw);

        // Sage: the same state, interpolated. Drawn one whole step behind real
        // time — but *consistently* one step behind, and consistent lateness is
        // invisible while varying lateness is not.
        const SDL_FRect box_lerp{lerp(previous.box_x, current.box_x, alpha),
                                 k_track_lerp_y, k_box_size, k_box_size};
        SDL_SetRenderDrawColor(renderer, k_lerp.r, k_lerp.g, k_lerp.b, 255);
        SDL_RenderFillRect(renderer, &box_lerp);

        const SDL_FRect ball{static_cast<float>(window_w) * 0.5f - k_ball_size * 0.5f,
                             lerp(previous.ball_y, current.ball_y, alpha),
                             k_ball_size, k_ball_size};
        SDL_SetRenderDrawColor(renderer, k_ball.r, k_ball.g, k_ball.b, 255);
        SDL_RenderFillRect(renderer, &ball);

        // The readout. `behind` is real elapsed time minus simulated time: it
        // should hover around one step and stay there. A number that climbs means
        // the step cap is firing and the simulation is losing ground.
        const double sim_seconds = static_cast<double>(stepper.total_steps())
                                 * static_cast<double>(stepper.h());
        SDL_SetRenderScale(renderer, 2.0f, 2.0f);
        SDL_SetRenderDrawColor(renderer, 220, 220, 210, 255);
        SDL_RenderDebugTextFormat(renderer, 8.0f, 8.0f,
                                  "fps %7.1f   raw dt %6.2f ms%s   frame %llu",
                                  static_cast<double>(clk.fps()),
                                  static_cast<double>(clk.raw_dt()) * 1000.0,
                                  clk.was_clamped() ? " CLAMPED" : "",
                                  static_cast<unsigned long long>(clk.frame_count()));
        SDL_RenderDebugTextFormat(renderer, 8.0f, 20.0f,
                                  "sim %5.0f Hz  h %5.2f ms   steps %d   alpha %.2f   sim %.1f s   behind %5.1f ms",
                                  static_cast<double>(stepper.rate_hz()),
                                  static_cast<double>(stepper.h()) * 1000.0,
                                  stepper.steps_this_frame(),
                                  static_cast<double>(alpha),
                                  sim_seconds,
                                  (clk.elapsed() - sim_seconds) * 1000.0);
        SDL_RenderDebugText(renderer, 8.0f, 36.0f,
                            "[1-4] sim rate 10/30/60/120   [V] vsync   [T] throttle   [R] reset   [Esc] quit");
        SDL_RenderDebugText(renderer, 8.0f, 52.0f, "coral  variable dt        - smooth, but a different sim on every machine");
        SDL_RenderDebugText(renderer, 8.0f, 64.0f, "amber  fixed step, raw    - identical everywhere, and it judders");
        SDL_RenderDebugText(renderer, 8.0f, 76.0f, "sage   fixed step + lerp  - identical everywhere, and smooth");
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);

        SDL_RenderPresent(renderer);

        // --- 4f. The throttle ------------------------------------------------
        if (in.key_down(SDL_SCANCODE_T))
        {
            SDL_Delay(k_throttle_ms);
        }
    }

    // ---- 5. Shut down, in reverse order of creation -------------------------
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
