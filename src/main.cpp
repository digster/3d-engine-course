// src/main.cpp — the engine's entry point.
//
// This is the first file of the real engine. It replaces the throwaway
// hello.cpp: it opens a window, runs an event loop that responds to input, and
// shuts down cleanly. The loop here is deliberately naive — Module 1 replaces it
// with a proper fixed-timestep loop — but the structure (init, create, loop,
// destroy) is the skeleton every engine shares.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // Provides the cross-platform entry point. NOTE:
                             // <SDL3/SDL.h> deliberately does NOT include this,
                             // so we include it explicitly, exactly once, here.

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
    // know which SDL the machine is running. It is "instrument the program", the
    // middle of the three debugging tools from Lesson 0.6.
    const int sdl_version = SDL_GetVersion();
    SDL_Log("Engine starting — SDL %d.%d.%d",
            SDL_VERSIONNUM_MAJOR(sdl_version),
            SDL_VERSIONNUM_MINOR(sdl_version),
            SDL_VERSIONNUM_MICRO(sdl_version));

    // ---- 2. Create the window ----------------------------------------------
    // Note the SDL3 signature: title, width, height, flags — and NO x/y
    // position (SDL places the window). A null return means failure, and the
    // reason is always in SDL_GetError().
    SDL_Window* window = SDL_CreateWindow("Engine", 1280, 720, SDL_WINDOW_RESIZABLE);
    if (window == nullptr)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // ---- 3. Create a renderer ----------------------------------------------
    // For now the renderer exists only so the window shows a defined colour
    // instead of undefined garbage. Passing nullptr for the name lets SDL pick
    // the best backend. Module 1 replaces this with our own CPU pixel buffer,
    // so treat the renderer as scaffolding, not architecture.
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // ---- 4. The main loop --------------------------------------------------
    // Naive on purpose: poll every pending event, then draw one frame, forever,
    // until we are told to stop. Module 1.4 derives the real loop — with a fixed
    // simulation step and render interpolation — from this starting point.
    bool running = true;
    while (running)
    {
        // Drain the event queue COMPLETELY, every frame. SDL_PollEvent removes
        // one event from the front and returns true; it returns false when the
        // queue is empty. `while`, never `if` — see Lesson 1.1: draining one per
        // frame lets the queue grow faster than it empties, and the backlog
        // becomes input lag that compounds until the app appears frozen.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            // event.type is the tag of a tagged union: it tells you which member
            // of SDL_Event is the valid one to read. Read the wrong member and
            // you get garbage, so every case below touches only its own member.
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
                // windows must close just the one — which is why SDL keeps the
                // two events distinct.
                SDL_Log("Window %u close requested", static_cast<unsigned>(event.window.windowID));
                running = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                // For a resize, SDL_WindowEvent's data1/data2 carry the new width
                // and height (the header says so: "resized to data1xdata2").
                SDL_Log("Window resized to %dx%d", event.window.data1, event.window.data2);
                break;

            case SDL_EVENT_KEY_DOWN:
                // SDL3 exposes the virtual key as event.key.key. (SDL2 nested it
                // as event.key.keysym.sym — do not reach for that.)
                if (event.key.key == SDLK_ESCAPE)
                {
                    running = false;
                }
                break;

            default:
                // Everything else — mouse motion, text input, gamepads — is
                // drained and ignored. Draining is mandatory; reacting is
                // optional. An event you never handle still has to be removed.
                break;
            }
        }

        // Fill the window with a calm dark slate so we can see it is alive.
        SDL_SetRenderDrawColor(renderer, 30, 30, 46, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }

    // ---- 5. Shut down, in reverse order of creation ------------------------
    // We destroy what we made, newest first, then quit SDL. Doing this by hand
    // is error-prone and easy to forget on an early-return path (notice we had
    // to repeat parts of it above). Module 5 wraps each resource in an RAII type
    // so cleanup becomes automatic — this manual version is here so you feel the
    // problem RAII solves.
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
