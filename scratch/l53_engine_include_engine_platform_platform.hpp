// engine/include/engine/platform/platform.hpp — how a program starts, and stops.
//
// Lesson 5.2. Every demo this course has written has opened with the same forty
// lines: SDL_Init, create a window, create a renderer, set vsync, create a
// streaming texture, set its scale mode, check each of those for failure, and
// then unwind all of it in reverse at the bottom of main(). None of that is what
// any demo is ABOUT. It is what a program on a desktop operating system has to
// say before it is allowed to draw anything at all.
//
// This file owns those lines, once.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS NOT
// ---------------------------------------------------------------------------
//
// It is not an abstraction over SDL. `window()` hands you an `SDL_Window*`;
// `renderer()` hands you an `SDL_Renderer*`; `handle()` takes an `SDL_Event`.
// Every SDL type this class touches is visible in its interface, on purpose.
//
// A wrapper that HIDES a library buys you exactly one thing: the ability to
// replace that library without touching the code above it. We have no intention
// of replacing SDL — it is the course's platform layer, chosen in Lesson 0.3 and
// load-bearing in every module since — so the ability to swap it out is a benefit
// we would never collect, paid for with a parallel vocabulary (`engine::key`,
// `engine::window_flags`, `engine::event`) that must be invented, documented,
// kept in sync, and translated at the boundary forever. That trade is a bad one
// and we decline it out loud rather than by omission.
//
// What this class owns instead is LIFECYCLE and ORDER: the fact that a window
// must exist before a renderer, that a texture must die before the renderer that
// made it, that SDL_Quit comes last, and that a window claimed by an SDL_GPU
// device may never be given an SDL_Renderer (Lesson 4.2). Those are not SDL
// details you might want to swap out. They are the rules, and getting them wrong
// is the failure this file removes.
//
// ---------------------------------------------------------------------------
// THE LOOP STAYS YOURS
// ---------------------------------------------------------------------------
//
// This is the LIBRARY half of Lesson 5.2's fork. You construct a `platform`, you
// write `while (plat.running())`, and you call into it. The FRAMEWORK half —
// where the engine calls you — is `engine::app` in <engine/platform/app.hpp>,
// and it is built on top of this class rather than beside it. Nothing `app` can
// do is unreachable from here, which is the property that makes the framework
// safe to offer: it is a convenience, never a second engine.

#pragma once

#include <engine/core/clock.hpp>
#include <engine/core/fixed_step.hpp>
#include <engine/core/input.hpp>
#include <engine/gfx/framebuffer.hpp>

#include <SDL3/SDL.h>

#include <optional>

namespace engine {

/// Where a program's pixels end up, decided before any window exists.
///
/// This is the one configuration choice that cannot be changed later, and Lesson
/// 4.2 is why: **a window is claimed by an SDL_GPU device or driven by an
/// SDL_Renderer, and there is no order of operations in which it is both.** By
/// the time you could regret the answer, the window already belongs to somebody.
enum class surface
{
    /// An `SDL_Renderer` plus a streaming texture — Module 1's presentation path.
    /// The platform creates a `framebuffer` for you, uploads it every frame, and
    /// `SDL_RenderDebugText` still works, so a HUD is available.
    renderer,

    /// A bare window, claimed by nobody. You create an `engine::gpu_device` on it
    /// yourself in your own start-up code. The platform will not create a
    /// renderer, because doing so would poison the window for SDL_GPU.
    gpu,

    /// No window, no renderer, no video subsystem — and therefore no display
    /// required. A framebuffer is still created if you ask for one, so the
    /// software renderer works exactly as it does on screen.
    ///
    /// This is what makes a build server a valid place to run the engine. Every
    /// `--shot` flag in this repository is a headless run.
    headless
};

/// A human-readable name, for logs and HUDs.
[[nodiscard]] const char* name_of(surface s);

/// Everything a program decides before SDL is initialised.
///
/// Designated initialisers (C++20) make a call site read as a list of the choices
/// this program actually has an opinion about, and every default here is the
/// answer a demo would have written anyway:
///
///     return {.title = "pong", .fb_width = 320, .fb_height = 180};
///
/// That is Lesson 5.1 §3.1's rule applied to a struct instead of a parameter
/// list — gather what travels together, and make the defaults correct.
struct app_config
{
    /// Window title. Also the SDL application metadata name.
    const char* title = "engine";

    /// Window size in *window* coordinates. Note this is not the framebuffer
    /// size: Module 1 has drawn into a small buffer scaled up to fill the window
    /// since Lesson 1.5, and the two numbers have been different ever since.
    int window_width = 1280;
    int window_height = 720;

    bool resizable = true;

    surface draw_to = surface::renderer;

    /// The CPU framebuffer's size in pixels, or 0 for "this program does not want
    /// one". Under `surface::renderer` a streaming texture of exactly this size
    /// is created alongside it.
    int fb_width = 0;
    int fb_height = 0;

    /// Nearest-neighbour scaling when the framebuffer is stretched to the window.
    /// True is right for this course: a 320x180 buffer blown up 4x should look
    /// like big square pixels, not like a photograph of big square pixels.
    bool fb_nearest = true;

    /// Synchronise presentation with the display's refresh. Can be changed later
    /// with set_vsync(); this is only the starting value.
    bool vsync = true;

    /// The simulation rate handed to `fixed_step` (Lesson 1.4).
    float fixed_hz = 60.0f;
    int max_steps_per_frame = 8;

    /// The longest frame the clock is allowed to report (Lesson 1.3's clamp).
    float max_frame_seconds = 0.25f;

    /// Subsystems beyond what `draw_to` already implies. Video is initialised
    /// automatically for `renderer` and `gpu` and deliberately NOT for
    /// `headless`; audio arrives in Module 7 and goes here.
    SDL_InitFlags extra_subsystems = 0;

    /// Lesson 5.3. Scan `argv` for `--log SPEC` and apply it before anything
    /// else happens, so that start-up itself can be traced. Set to 0 / nullptr
    /// if your program parses its own arguments and calls
    /// `engine::configure_logging` when it chooses.
    ///
    /// Passing argv through the config rather than making every program
    /// remember a call is the same reasoning as every other default here: the
    /// flag exists whether or not the program thought about it.
    int argc = 0;
    char** argv = nullptr;
};

/// The program's window, renderer, framebuffer, clock, input and step
/// accumulator — created in the right order, destroyed in the reverse one.
///
/// Two-phase construction, the same shape as `gpu_device` (Lesson 4.2) and for
/// the same reason: a constructor has no way to say "no", and every single call
/// in start() can fail. The default constructor cannot fail; start() reports.
///
/// **Usage, library-style — you own the loop:**
///
///     engine::platform plat;
///     if (!plat.start({.title = "demo", .fb_width = 320, .fb_height = 180}))
///     {
///         return 1;
///     }
///
///     while (plat.running())
///     {
///         SDL_Event e;
///         while (SDL_PollEvent(&e)) { plat.handle(e); /* and your own cases */ }
///
///         plat.begin_frame();
///         while (plat.next_step()) { simulate(plat.steps().h()); }
///         render(plat.fb(), plat.steps().alpha());
///
///         plat.blit_framebuffer();
///         // …your HUD here, drawn with SDL_RenderDebugText…
///         plat.present();
///     }
///
/// The destructor calls stop(), so an early `return` on any error path unwinds
/// correctly without a `goto cleanup` and without repeating the teardown at four
/// exits — which is what the demos did until this lesson.
class platform
{
public:
    platform() = default;

    /// Tears everything down, in reverse order of creation. Safe to run on a
    /// platform that was never started, or was already stopped.
    ~platform();

    // Non-copyable AND non-movable, which is stronger than the rule the rest of
    // the engine follows. `gpu_device` and `framebuffer` are movable because a
    // program can reasonably own several. This class owns PROCESS-WIDE state —
    // there is one SDL library, initialised once — so a second one is a bug and
    // a moved-from one is a bug waiting to be found later. Deleting the move
    // makes both diagnosable at compile time.
    platform(const platform&) = delete;
    platform& operator=(const platform&) = delete;
    platform(platform&&) = delete;
    platform& operator=(platform&&) = delete;

    /// Initialise SDL and create everything `cfg` asks for.
    ///
    /// On failure, logs what went wrong, unwinds whatever it had already made,
    /// and returns false — so a caller never has to clean up after a failed
    /// start. Returns false immediately if called twice.
    [[nodiscard]] bool start(const app_config& cfg);

    /// Destroy everything and shut SDL down. Idempotent.
    void stop();

    [[nodiscard]] bool started() const { return started_; }

    /// False once a quit has been requested — by the window's close button, by
    /// SDL_EVENT_QUIT, or by request_quit(). This is the loop condition.
    [[nodiscard]] bool running() const { return running_; }

    /// True when the reason we stopped was a failure rather than a normal exit.
    /// `app_runner` turns this into the process's exit code.
    [[nodiscard]] bool failed() const { return failed_; }

    /// Ask the loop to stop at the end of this frame.
    ///
    /// It does not stop it *now*, and that is deliberate: a frame that is halfway
    /// through drawing should finish drawing. Every `--shot` in this repository
    /// relies on it — the frame is written to disk and only then does the program
    /// end.
    void request_quit(bool success = true);

    // ---- Events ------------------------------------------------------------

    /// Give the platform one event.
    ///
    /// Feeds `input` and notices the two events that end a program. It does NOT
    /// consume the event: call it from inside your own drain loop and then
    /// switch on the same event yourself.
    ///
    /// **This is the seam that makes both halves of Lesson 5.2 possible.** In the
    /// library path your loop calls SDL_PollEvent and hands each event here. In
    /// the framework path SDL calls `SDL_AppEvent` and *that* hands it here. One
    /// implementation, two ways in.
    void handle(const SDL_Event& event);

    /// Drain the whole queue through handle(). For a program that has no per-event
    /// business of its own.
    ///
    /// **Never call this from an SDL_AppEvent callback.** Under
    /// SDL_MAIN_USE_CALLBACKS, SDL pumps and dispatches the queue itself; polling
    /// it again from inside a callback would race SDL for the same events.
    void pump();

    // ---- The frame ---------------------------------------------------------

    /// Tick the clock, publish one frame of input, and hand the elapsed time to
    /// the step accumulator. Call once per frame, AFTER the events are drained.
    ///
    /// The order inside is Lesson 1.2's contract and Lesson 1.3's, and it is now
    /// in one place rather than at the top of every demo's loop.
    void begin_frame();

    /// Take one fixed simulation step, or return false when this frame's real
    /// time is spent. `while (plat.next_step())`.
    [[nodiscard]] bool next_step();

    /// Copy the framebuffer into the streaming texture and draw it over the
    /// window. Everything about presenting that is OUR work.
    ///
    /// Separate from present() because the split is measurable: on a vsynced
    /// frame SDL_RenderPresent blocks until the display is ready, and a profiler
    /// bar that is 90% "waiting for the monitor" tells you nothing about your
    /// renderer (Lesson 3.10). It is also the gap a HUD is drawn into.
    ///
    /// A no-op unless the surface is `renderer` and a framebuffer exists.
    void blit_framebuffer();

    /// Show the frame. The blocking half. A no-op without a renderer.
    void present();

    // ---- What we own -------------------------------------------------------

    /// The window, or null under `surface::headless`. Hand it to
    /// `gpu_device::create` under `surface::gpu`.
    [[nodiscard]] SDL_Window* window() const { return window_; }

    /// The renderer, or null under `surface::gpu` and `surface::headless`. Null
    /// is not an error there — it is the whole point of asking for those.
    [[nodiscard]] SDL_Renderer* renderer() const { return renderer_; }

    /// The streaming texture the framebuffer is uploaded through, or null.
    [[nodiscard]] SDL_Texture* screen_texture() const { return screen_; }

    [[nodiscard]] bool has_framebuffer() const { return fb_.has_value(); }

    /// The CPU framebuffer. **Precondition: `fb_width`/`fb_height` were non-zero
    /// in the config.** Asking for a buffer you did not request is a programming
    /// error, and dereferencing an empty optional says so immediately in a debug
    /// build rather than three frames later.
    [[nodiscard]] framebuffer& fb() { return *fb_; }
    [[nodiscard]] const framebuffer& fb() const { return *fb_; }

    [[nodiscard]] input& in() { return input_; }
    [[nodiscard]] const input& in() const { return input_; }

    [[nodiscard]] clock& time() { return clock_; }
    [[nodiscard]] const clock& time() const { return clock_; }

    [[nodiscard]] fixed_step& steps() { return steps_; }
    [[nodiscard]] const fixed_step& steps() const { return steps_; }

    [[nodiscard]] const app_config& config() const { return cfg_; }

    /// Turn presentation sync on or off. Returns false if the driver refused,
    /// in which case the previous setting stands.
    [[nodiscard]] bool set_vsync(bool on);
    [[nodiscard]] bool vsync() const { return vsync_; }

private:
    app_config cfg_{};

    bool started_ = false;
    bool running_ = false;
    bool failed_ = false;
    bool vsync_ = false;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* screen_ = nullptr;

    // `std::optional` rather than a member, because `framebuffer` has no default
    // constructor — it allocates, so it must be told a size — and the size is not
    // known until start(). The alternative, a `std::unique_ptr<framebuffer>`,
    // would put the pixels behind a second indirection and a separate allocation
    // for no benefit: the optional stores the framebuffer object inline, and the
    // framebuffer's own vector already owns the pixels.
    std::optional<framebuffer> fb_;

    clock clock_;
    input input_;
    fixed_step steps_;
};

} // namespace engine
