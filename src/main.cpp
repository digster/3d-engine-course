// src/main.cpp — the engine's entry point.
//
// The loop here is still deliberately naive — Lessons 1.3 and 1.4 replace it
// with a proper fixed-timestep loop — but it now owns an input subsystem, and
// the frame has acquired the shape it will keep for the rest of the course:
//
//     drain events  ->  update input  ->  simulate  ->  render
//
// The order is not stylistic. Input is sampled after the drain because SDL's
// internal state is only refreshed by pumping the queue, and simulation runs
// after input because it reads a snapshot that must not change underneath it.

#include "core/input.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

#include <algorithm>

namespace {

/// The four colours the demo box cycles through when Space is pressed. Grouping
/// them in an anonymous namespace gives them internal linkage — they exist only
/// in this translation unit and cannot collide with a name in another.
struct rgb { Uint8 r, g, b; };

constexpr rgb k_box_colours[] = {
    {236, 122,  92},   // coral
    {122, 196, 152},   // sage
    {126, 162, 236},   // cornflower
    {226, 196, 110},   // amber
};

/// Pixels moved per frame while a direction key is held.
///
/// Per *frame*, not per second — which means the box moves faster on a 240 Hz
/// monitor than on a 60 Hz one. That is a real bug, and it is the exact bug
/// Lesson 1.3 exists to fix. It is left visible here rather than papered over.
constexpr float k_box_speed = 6.0f;

constexpr float k_box_min_size = 12.0f;
constexpr float k_box_max_size = 400.0f;

} // namespace

int main(int argc, char* argv[])
{
    // We do not use command-line arguments yet. Marking them silences the
    // -Wall -Wextra "unused parameter" warning without weakening the signature
    // SDL's entry point requires.
    (void)argc;
    (void)argv;

    // ---- 1. Initialise the SDL subsystems we need --------------------------
    // SDL3's SDL_Init returns a bool: true on success, false on failure. (This
    // changed from SDL2, where it returned an int — one of many SDL2 habits to
    // unlearn.) SDL_INIT_VIDEO also brings up the event subsystem we rely on.
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Log what we actually linked against. This one line is the first thing you
    // want in a bug report — "it doesn't work" is far more tractable once you
    // know which SDL the machine is running.
    const int sdl_version = SDL_GetVersion();
    SDL_Log("Engine starting — SDL %d.%d.%d",
            SDL_VERSIONNUM_MAJOR(sdl_version),
            SDL_VERSIONNUM_MINOR(sdl_version),
            SDL_VERSIONNUM_MICRO(sdl_version));

    // ---- 2. Create the window ----------------------------------------------
    // Note the SDL3 signature: title, width, height, flags — and NO x/y
    // position (SDL places the window). A null return means failure, and the
    // reason is always in SDL_GetError().
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
    // Still scaffolding: it exists so we have something to draw with before we
    // own our own pixel buffer in Lesson 1.5. Passing nullptr for the name lets
    // SDL pick the best backend.
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // ---- 4. The things the demo simulates -----------------------------------
    engine::input in;

    SDL_FRect box{
        static_cast<float>(window_w) * 0.5f - 40.0f,
        static_cast<float>(window_h) * 0.5f - 40.0f,
        80.0f,
        80.0f,
    };
    int box_colour = 0;

    SDL_Log("Controls: WASD/arrows move · Space cycles colour · "
            "left-click teleports · wheel resizes · Esc quits");

    // ---- 5. The main loop ---------------------------------------------------
    bool running = true;
    while (running)
    {
        // --- 5a. Drain the event queue COMPLETELY --------------------------
        // `while`, never `if` (Lesson 1.1): draining one event per frame lets the
        // queue grow faster than it empties, and the backlog becomes input lag
        // that compounds until the app appears frozen.
        //
        // Draining also *pumps*: SDL_PollEvent refreshes the internal keyboard
        // and mouse state that in.update() is about to sample. That is why the
        // drain comes first.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            // Hand every event to the input system first. It ignores almost all
            // of them — it only needs the ones that cannot be polled — but the
            // call site stays simple: everything goes in, input decides what
            // matters.
            in.feed_event(event);

            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                // Application-scoped: "this program should terminate."
                SDL_Log("Quit requested");
                running = false;
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                // Window-scoped: THIS window's close was requested. With a single
                // window it amounts to the same thing, but an app with several
                // windows must close just the one.
                SDL_Log("Window %u close requested", static_cast<unsigned>(event.window.windowID));
                running = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                // Remember the new size: the demo clamps the box to the window,
                // so it needs to know how big the window currently is.
                window_w = event.window.data1;
                window_h = event.window.data2;
                break;

            default:
                // Everything else is drained and ignored. Draining is mandatory;
                // reacting is optional.
                break;
            }
        }

        // --- 5b. Publish this frame's input snapshot -----------------------
        // Exactly once, after the drain, before anything reads input. From here
        // to the end of the frame, every query returns the same answer.
        in.update();

        // --- 5c. Simulate ---------------------------------------------------
        // Esc quits. key_pressed, not key_down, because the intent is "the act of
        // pressing Esc quits" — an edge, not a level. (Here they behave the same;
        // saying what you mean is still worth the character count.)
        if (in.key_pressed(SDL_SCANCODE_ESCAPE))
        {
            SDL_Log("Esc pressed — quitting");
            running = false;
        }

        // Movement reads LEVELS: true for every frame the key is held, so the box
        // glides. Scancodes, not keycodes — SDL_SCANCODE_W is the physical key
        // above S wherever you are, while SDLK_W is a letter that moves house on
        // an AZERTY keyboard.
        float dx = 0.0f;
        float dy = 0.0f;
        if (in.key_down(SDL_SCANCODE_A) || in.key_down(SDL_SCANCODE_LEFT))  { dx -= 1.0f; }
        if (in.key_down(SDL_SCANCODE_D) || in.key_down(SDL_SCANCODE_RIGHT)) { dx += 1.0f; }
        if (in.key_down(SDL_SCANCODE_W) || in.key_down(SDL_SCANCODE_UP))    { dy -= 1.0f; }
        if (in.key_down(SDL_SCANCODE_S) || in.key_down(SDL_SCANCODE_DOWN))  { dy += 1.0f; }

        // +Y is DOWN in window coordinates, which is why "up" subtracts. This is
        // the first appearance of a sign that will confuse everyone at least once
        // more, in Lesson 2.11 when NDC meets the viewport transform.
        box.x += dx * k_box_speed;
        box.y += dy * k_box_speed;

        // Colour cycling reads an EDGE: exactly one advance per press, no matter
        // how long Space is held. Swap this for key_down and the box strobes
        // through the palette at frame rate — the difference the lesson is about.
        if (in.key_pressed(SDL_SCANCODE_SPACE))
        {
            constexpr int colour_count = static_cast<int>(std::size(k_box_colours));
            box_colour = (box_colour + 1) % colour_count;
        }

        // A press-edge combined with a polled position: teleport on click, once
        // per click, to wherever the cursor was when the frame was sampled.
        if (in.mouse_pressed(SDL_BUTTON_LEFT))
        {
            box.x = in.mouse_x() - box.w * 0.5f;
            box.y = in.mouse_y() - box.h * 0.5f;
        }

        // The wheel is the event-only quantity: no state to poll, just this
        // frame's accumulated movement. Resize around the centre so the box does
        // not appear to crawl away from the cursor as it grows.
        if (in.wheel_y() != 0.0f)
        {
            const float target = box.w + in.wheel_y() * 8.0f;
            const float clamped = std::clamp(target, k_box_min_size, k_box_max_size);
            const float delta = clamped - box.w;
            box.x -= delta * 0.5f;
            box.y -= delta * 0.5f;
            box.w = clamped;
            box.h = clamped;
        }

        // Keep the box inside the window, whatever the window's current size.
        box.x = std::clamp(box.x, 0.0f, static_cast<float>(window_w) - box.w);
        box.y = std::clamp(box.y, 0.0f, static_cast<float>(window_h) - box.h);

        // --- 5d. Render ------------------------------------------------------
        SDL_SetRenderDrawColor(renderer, 30, 30, 46, 255);
        SDL_RenderClear(renderer);

        const rgb c = k_box_colours[box_colour];
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
        SDL_RenderFillRect(renderer, &box);

        SDL_RenderPresent(renderer);
    }

    // ---- 6. Shut down, in reverse order of creation -------------------------
    // We destroy what we made, newest first, then quit SDL. Doing this by hand is
    // error-prone and easy to forget on an early-return path (notice we had to
    // repeat parts of it above). Module 5 wraps each resource in an RAII type so
    // cleanup becomes automatic — this manual version is here so you feel the
    // problem RAII solves.
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
