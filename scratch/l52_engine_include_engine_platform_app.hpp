// engine/include/engine/platform/app.hpp — when the engine calls you.
//
// Lesson 5.2's fork, and the half that is genuinely a different shape of program.
//
// Everything this course has written so far is a LIBRARY user: you wrote main(),
// you wrote `while (running)`, and inside it you called the engine. This file is
// the other arrangement — a FRAMEWORK. You implement a handful of functions and
// something else decides when they run. Your program has no loop in it at all.
//
// ---------------------------------------------------------------------------
// WHY BOTH, AND WHY THIS ONE IS BUILT ON THE OTHER
// ---------------------------------------------------------------------------
//
// The framework arrangement is not merely a convenience. On a desktop it is one;
// on the web and on mobile it is the ONLY arrangement that works, because there
// the operating system owns the loop and `while (true)` inside your code starves
// the browser or the OS event pump. That is why SDL3 ships the callback entry
// points at all, and it is why an engine that intends to reach those platforms
// should have a callback-shaped spine even while it runs on desktops.
//
// So we offer both. The rule that keeps that from becoming two engines is one
// line of physical design: **`app` is implemented ON `platform`, never beside
// it.** Every service `app` gives you is reachable through `sys()`, and nothing
// in this file does anything a hand-written main() could not. If that ever stops
// being true, the framework has begun to grow a private engine inside it, and
// the library path has quietly become second-class.
//
// ---------------------------------------------------------------------------
// THE HOOKS, AND WHERE THEY SIT IN LESSON 1.4's LOOP
// ---------------------------------------------------------------------------
//
//     configure()      once, BEFORE SDL exists. Read argv, return an app_config.
//     on_start()       once, after the window and framebuffer exist.
//     ─────────────── per frame ───────────────
//     on_event()       once per event, before the frame it belongs to
//     on_fixed_step()  0..N times, N decided by the accumulator (Lesson 1.4)
//     on_frame(alpha)  once, draws into fb() using the interpolation factor
//     on_overlay()     once, after the framebuffer is on screen, before it is shown
//     ─────────────────────────────────────────
//     on_stop()        once, before the window is destroyed
//
// The only hook you must write is on_frame(). Everything else has a default that
// does the right nothing.

#pragma once

#include <engine/platform/platform.hpp>

#include <SDL3/SDL.h>

#include <memory>

namespace engine {

/// The base class a program derives from to be driven by the engine.
///
/// Derive, override what you need, and hand the type to ENGINE_MAIN (see
/// <engine/platform/main.hpp>):
///
///     class my_app final : public engine::app
///     {
///     public:
///         engine::app_config configure(int, char*[]) override
///         {
///             return {.title = "my app", .fb_width = 320, .fb_height = 180};
///         }
///
///         void on_frame(float alpha) override
///         {
///             fb().clear(engine::pack_argb(12, 14, 20));
///             draw_everything(fb(), alpha);
///         }
///     };
///
///     ENGINE_MAIN(my_app)
///
/// That program has no `main`, no `SDL_Init`, no window handling, no event drain,
/// no accumulator and no teardown — and it is a correct, fixed-timestep,
/// interpolating, cross-platform SDL3 application.
class app
{
public:
    app() = default;

    /// Virtual, because `app_runner` destroys derived instances through this
    /// type. A base with virtual functions and a non-virtual destructor is the
    /// classic silent leak: the derived destructor simply never runs.
    virtual ~app() = default;

    app(const app&) = delete;
    app& operator=(const app&) = delete;
    app(app&&) = delete;
    app& operator=(app&&) = delete;

    // ---- The hooks ---------------------------------------------------------

    /// Decide how this program starts. Called before SDL is initialised, so it
    /// may look at argv and nothing else — no window exists, no timer is running,
    /// and SDL_GetError() has nothing to say yet.
    ///
    /// Parsing arguments HERE rather than in on_start() is what lets a flag
    /// change the surface: `--shot` picks `surface::headless`, and by the time
    /// on_start() runs that decision is already irreversible (Lesson 4.2).
    [[nodiscard]] virtual app_config configure(int argc, char* argv[]);

    /// Build whatever the program needs. Return false to abort start-up; the
    /// process exits with a failure code and on_stop() still runs.
    [[nodiscard]] virtual bool on_start();

    /// One event, already fed to `in()` and already checked for quit.
    ///
    /// Use this for things that are genuinely events: text input, file drops,
    /// window resizes, and discrete key presses you would rather not poll for.
    /// Anything that asks "is this key down" belongs in on_fixed_step() instead,
    /// reading `in()`.
    virtual void on_event(const SDL_Event& event);

    /// Advance the simulation by exactly `h` seconds. Called zero or more times
    /// per frame — never assume once.
    ///
    /// Reading `in()` in here is safe and correct: input is published once per
    /// frame (Lesson 1.2), so every step in one frame sees the same keyboard.
    /// That is not sloppiness, it is the frame-coherence guarantee being used.
    virtual void on_fixed_step(float h);

    /// Draw one frame. `alpha` is how far this frame sits between the previous
    /// simulation step and the current one, in [0, 1] — Lesson 1.4's
    /// interpolation factor. The only hook without a default.
    virtual void on_frame(float alpha) = 0;

    /// Draw on top of the presented framebuffer, with SDL_RenderDebugText and
    /// friends. Runs after blit_framebuffer() and before present(), which is the
    /// only moment at which "over the render, under the vsync wait" is true.
    virtual void on_overlay();

    /// Release anything on_start() built. Runs whenever on_start() RAN, whatever
    /// it returned — so a half-built program still gets to clean up — and always
    /// before the window is destroyed.
    virtual void on_stop();

    // ---- What you are given ------------------------------------------------

    /// The platform underneath. Never hidden: if a hook is not enough, reach
    /// through here and do it by hand.
    [[nodiscard]] platform& sys() { return sys_; }
    [[nodiscard]] const platform& sys() const { return sys_; }

    // Forwarders for the four things every frame touches. They exist so that a
    // demo reads `fb()` rather than `sys().fb()` forty times, and for no deeper
    // reason — each is one line and adds no behaviour.
    [[nodiscard]] framebuffer& fb() { return sys_.fb(); }
    [[nodiscard]] input& in() { return sys_.in(); }
    [[nodiscard]] const clock& time() const { return sys_.time(); }
    [[nodiscard]] const fixed_step& steps() const { return sys_.steps(); }

    [[nodiscard]] SDL_Window* window() const { return sys_.window(); }
    [[nodiscard]] SDL_Renderer* renderer() const { return sys_.renderer(); }

    /// Ask to stop at the end of this frame. `success = false` makes the process
    /// exit with a failure code.
    void request_quit(bool success = true) { sys_.request_quit(success); }

private:
    platform sys_;
};

/// The four SDL callbacks, implemented once for every program in the project.
///
/// This is a struct of static functions rather than four free functions because
/// they are one mechanism, and grouping them says so. ENGINE_MAIN's whole body is
/// four one-line forwards into here — which means the macro contains no logic,
/// and a macro with no logic is a macro that cannot hide a bug.
struct app_runner
{
    /// SDL_AppInit. Takes ownership of `instance`, publishes it as SDL's
    /// `appstate`, configures and starts the platform, then calls on_start().
    [[nodiscard]] static SDL_AppResult init(void** appstate,
                                            std::unique_ptr<app> instance,
                                            int argc, char* argv[]);

    /// SDL_AppIterate. One whole frame: begin, steps, draw, blit, overlay, present.
    [[nodiscard]] static SDL_AppResult iterate(void* appstate);

    /// SDL_AppEvent. One event, into the platform and then into the app.
    [[nodiscard]] static SDL_AppResult event(void* appstate, SDL_Event* event);

    /// SDL_AppQuit. Runs on_stop(), tears the platform down, and destroys the
    /// instance. Called by SDL in every exit path, including a failed init.
    static void quit(void* appstate, SDL_AppResult result);
};

} // namespace engine
