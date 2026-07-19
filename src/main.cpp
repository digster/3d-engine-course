// src/main.cpp — the engine's entry point.
//
// Notice how little is left here. Everything this file does is host work: open a
// window, own a framebuffer, run the loop, hand the game its inputs, put the
// resulting pixels on screen. The rules of Pong are not in this file and could
// not be — they live in src/game/pong.cpp, and the only things crossing the
// boundary are a state struct, an intent struct, and two function calls.
//
// That shape is the point of the checkpoint. Module 5 turns this arrangement
// into a static library and a demo executable; the work of that refactor is
// mostly deleting the word "src" from some include paths, precisely because the
// separation was maintained from here on.
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
#include "math/vec2.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

#include <cmath>
#include <cstring>

namespace {

// The framebuffer is exactly the court. Two constants that must agree would
// eventually stop agreeing, so there is only one of each — game::court owns them
// and this file derives its buffer size from there.
constexpr int k_fb_width = static_cast<int>(game::court::width);
constexpr int k_fb_height = static_cast<int>(game::court::height);

constexpr Uint32 k_throttle_ms = 50;

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

/// Turn the keyboard into the two numbers the simulation actually wants.
///
/// This is the whole of the translation layer between hardware and gameplay, and
/// it is worth seeing how thin it is. The game is never told about scancodes; it
/// is told "the left player wants to go up". Module 5 generalises this into an
/// input-mapping system with named actions, and the reason that refactor is easy
/// is that the seam already exists here.
[[nodiscard]] game::intent read_intent(const engine::input& in, bool right_is_ai)
{
    game::intent wanted;
    wanted.right_is_ai = right_is_ai;

    // Levels, not edges: movement continues for as long as the key is held.
    // Lesson 1.2's distinction, and the reason both queries exist.
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

    SDL_Window* window = SDL_CreateWindow("Pong — Module 1 Checkpoint", 1280, 720,
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

    // The seed is an INPUT to the simulation, not a hidden detail of it. Taking
    // it from the clock gives a different game each run; logging it means any
    // particular game can be replayed exactly by passing the same number back.
    // That is the practical form of Lesson 1.4's determinism claim.
    const Uint32 seed = static_cast<Uint32>(SDL_GetTicksNS() & 0xFFFFFFFFu);
    SDL_Log("Match seed: %u  (same seed + same inputs = the same match)", seed);

    game::state current = game::make_state(seed);
    game::state previous = current;

    bool right_is_ai = true;

    SDL_Log("Left paddle: [W]/[S].  Right paddle: [Up]/[Down] (press [C] for a human).");
    SDL_Log("[K] collision rule · [1-4] sim rate · [V] vsync · [T] throttle · [R] reset · [Esc] quit");

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

        // --- Commands: edges, because each should happen once per press --------
        if (in.key_pressed(SDL_SCANCODE_ESCAPE)) { running = false; }
        if (in.key_pressed(SDL_SCANCODE_1)) { stepper.set_rate(10.0f); }
        if (in.key_pressed(SDL_SCANCODE_2)) { stepper.set_rate(30.0f); }
        if (in.key_pressed(SDL_SCANCODE_3)) { stepper.set_rate(60.0f); }
        if (in.key_pressed(SDL_SCANCODE_4)) { stepper.set_rate(120.0f); }

        if (in.key_pressed(SDL_SCANCODE_C)) { right_is_ai = !right_is_ai; }

        if (in.key_pressed(SDL_SCANCODE_K))
        {
            current.swept_collision = !current.swept_collision;
            previous.swept_collision = current.swept_collision;
            SDL_Log("Collision rule: %s",
                    current.swept_collision ? "swept (asks about the path)"
                                            : "naive (asks only where the ball ended up)");
        }

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
            const bool keep_swept = current.swept_collision;
            current = game::make_state(seed);
            current.swept_collision = keep_swept;
            previous = current;
        }

        const game::intent wanted = read_intent(in, right_is_ai);

        // --- Simulate: zero or more fixed steps, never a partial one -----------
        stepper.begin_frame(clk.dt());
        while (stepper.next_step())
        {
            previous = current;
            game::step(current, wanted, stepper.h());

            // The one exception to interpolation. A ball that was teleported to
            // the centre after a point has no meaningful "in between", and
            // lerping to it would draw a streak across the court at a speed the
            // ball never had. Snapping the history forward makes this frame's
            // lerp a no-op. Lesson 1.4 §5.4.
            if (current.teleported) { previous = current; }
        }

        // --- Draw -------------------------------------------------------------
        game::draw(fb, previous, current, stepper.alpha());

        if (!upload(screen_texture, fb))
        {
            SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, screen_texture, nullptr, nullptr);

        // --- The debug overlay -------------------------------------------------
        // Still SDL's built-in font rather than ours: this is developer
        // instrumentation, not part of the game, and it is the only thing on
        // screen we did not rasterise ourselves. A text coordinate here is twice
        // a framebuffer coordinate — 2x text scale over a 4x framebuffer scale.
        const float ball_speed = engine::length(current.ball_vel);

        SDL_SetRenderScale(renderer, 2.0f, 2.0f);
        SDL_SetRenderDrawColor(renderer, 210, 212, 220, 255);
        SDL_RenderDebugTextFormat(renderer, 6.0f, 6.0f,
                                  "sim %5.1f Hz   fps %6.1f   alpha %4.2f   ball %5.1f px/s   rally %d",
                                  static_cast<double>(stepper.rate_hz()),
                                  static_cast<double>(clk.fps()),
                                  static_cast<double>(stepper.alpha()),
                                  static_cast<double>(ball_speed),
                                  current.rally_hits);

        // The step budget the naive test has to fit inside: a ball whose
        // HORIZONTAL motion in one step exceeds the width of the overlap window
        // can step clean over it. Note it is |vx| and not the speed — a ball
        // moving steeply is crossing the window slowly whatever its speed says.
        // Printing it beside the actual per-step motion turns §3.3's arithmetic
        // into something you can watch cross over.
        const float per_step = std::fabs(current.ball_vel.x) * stepper.h();
        const float budget = game::court::paddle_w + game::court::ball_size;

        SDL_SetRenderDrawColor(renderer,
                               (per_step > budget && !current.swept_collision) ? 236 : 150,
                               (per_step > budget && !current.swept_collision) ? 122 : 152,
                               (per_step > budget && !current.swept_collision) ? 92 : 170,
                               255);
        SDL_RenderDebugTextFormat(renderer, 6.0f, 20.0f,
                                  "[K] collision: %-6s   ball moves %5.2f px/step, naive test needs < %.0f",
                                  current.swept_collision ? "swept" : "naive",
                                  static_cast<double>(per_step),
                                  static_cast<double>(budget));

        SDL_SetRenderDrawColor(renderer, 150, 152, 170, 255);
        SDL_RenderDebugTextFormat(renderer, 6.0f, 328.0f,
                                  "left [W]/[S]   right %s   [C] swap   [1-4] sim rate   [V] vsync   [T] throttle   [R] reset",
                                  right_is_ai ? "= AI" : "[Up]/[Down]");
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
