// engine/include/engine/platform/main.hpp — the entry point, and the one rule.
//
// ###########################################################################
// #                                                                         #
// #   INCLUDE THIS HEADER IN EXACTLY ONE .cpp FILE PER PROGRAM.             #
// #   THAT FILE MUST NOT DEFINE main().                                     #
// #                                                                         #
// ###########################################################################
//
// Both halves of that are enforced by the compiler, and it is worth knowing what
// each violation looks like before you meet it at eleven at night.
//
// WHY ONE FILE. Including this header defines SDL_MAIN_USE_CALLBACKS and then
// includes <SDL3/SDL_main.h>, which — read it, it is worth reading — ends by
// including SDL_main_impl.h, which emits a real, non-inline function definition:
//
//     int SDL_main(int argc, char **argv)
//     {
//         return SDL_EnterAppMainCallbacks(argc, argv, SDL_AppInit,
//                                          SDL_AppIterate, SDL_AppEvent,
//                                          SDL_AppQuit);
//     }
//
// plus the platform's real entry point (`main`, or `WinMain` on Windows) that
// calls it. Two translation units including this header give you two definitions
// of both, and the linker says `duplicate symbol _main`. That is the One
// Definition Rule, not an SDL quirk.
//
// It is also exactly why this cannot live inside `libengine.a`. An entry point is
// not a library's to own: a static library that defined `main` would impose it on
// every program that linked the library, whether or not that program wanted the
// callback shape — and `sandbox`, three files away, deliberately does not.
//
// WHY NO main(). SDL_main.h ends with `#define main SDL_main`. So a `main()`
// written below this include is quietly renamed to `SDL_main` and collides with
// SDL's own — the same duplicate-symbol error, from a line that looks innocent.
// The SDL header says it plainly: the app "SHOULD NOT ALSO SUPPLY" one.
//
// ---------------------------------------------------------------------------
// VERIFIED AGAINST SDL 3.4.12
// ---------------------------------------------------------------------------
//
//   SDL_AppResult SDL_AppInit   (void **appstate, int argc, char *argv[]);
//   SDL_AppResult SDL_AppIterate(void *appstate);
//   SDL_AppResult SDL_AppEvent  (void *appstate, SDL_Event *event);
//   void          SDL_AppQuit   (void *appstate, SDL_AppResult result);
//
// declared in SDL3/SDL_main.h under `#ifdef SDL_MAIN_USE_CALLBACKS`, inside the
// header's `extern "C"` block — which is why the definitions the macro writes get
// C linkage automatically, without us saying so.
//
//   SDL_APP_CONTINUE / SDL_APP_SUCCESS / SDL_APP_FAILURE   (SDL3/SDL_init.h)

#pragma once

// Our own header first, so that <engine/...> is self-contained and this file's
// correctness does not depend on what SDL_main.h happens to drag in (cpp-style
// §3). It also pulls in <SDL3/SDL.h>, which SDL_main.h expects to coexist with.
#include <engine/platform/app.hpp>

#include <memory>

// The order of these two lines is the whole mechanism, and reversing them fails
// silently: SDL_main.h reads SDL_MAIN_USE_CALLBACKS at preprocessing time, so if
// the macro is not already defined the header takes the classic-main path,
// declares no callbacks, and your four definitions below become four unused
// functions in a program with no entry point.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

/// Define a program's entry point in terms of an `engine::app` subclass.
///
///     ENGINE_MAIN(my_app)
///
/// Write it at namespace scope, at the very bottom of the file, with no trailing
/// semicolon needed (one is harmless). `AppType` must be default-constructible
/// and derive from `engine::app`.
///
/// The macro contains no logic — four one-line forwards into `engine::app_runner`
/// and nothing else. That is on purpose. A macro is code the debugger struggles
/// to step through and the compiler reports odd line numbers for, so the only
/// thing worth putting in one is the part that genuinely cannot be a function:
/// here, the four fixed C symbol names SDL is going to look up.
#define ENGINE_MAIN(AppType)                                                    \
    SDL_AppResult SDLCALL SDL_AppInit(void** appstate, int argc, char* argv[])  \
    {                                                                           \
        return ::engine::app_runner::init(appstate,                             \
                                          std::make_unique<AppType>(),          \
                                          argc, argv);                          \
    }                                                                           \
                                                                                \
    SDL_AppResult SDLCALL SDL_AppIterate(void* appstate)                        \
    {                                                                           \
        return ::engine::app_runner::iterate(appstate);                         \
    }                                                                           \
                                                                                \
    SDL_AppResult SDLCALL SDL_AppEvent(void* appstate, SDL_Event* event)        \
    {                                                                           \
        return ::engine::app_runner::event(appstate, event);                    \
    }                                                                           \
                                                                                \
    void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult result)              \
    {                                                                           \
        ::engine::app_runner::quit(appstate, result);                           \
    }
