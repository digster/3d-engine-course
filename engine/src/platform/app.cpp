// engine/src/platform/app.cpp — the frame, and the ownership handoff.
//
// Two things happen in this file and they are worth separating in your head.
//
// The first is `iterate`, which is Lesson 1.4's loop with the `while` taken off
// the front. That is all the inversion physically is: the body of a loop, called
// by somebody else.
//
// The second is ownership crossing into C. SDL's callbacks pass an app's state
// as a `void*`, because SDL is a C library and `void*` is C's word for "yours,
// not mine". Getting a C++ object safely across that boundary and back is the
// interesting part, and it is where the one raw `new`-shaped operation in this
// engine lives.

#include <engine/platform/app.hpp>

#include <engine/core/log.hpp>

namespace engine {

// ---- The defaults ---------------------------------------------------------
//
// Out of line rather than `{}` in the header, so that adding behaviour to one of
// them later does not recompile every file that includes app.hpp (Lesson 5.1
// §3.3). They are also the documentation of what "do nothing" means for each
// hook, which is easier to read here than as six empty braces in a class body.

app_config app::configure(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    return {};
}

bool app::on_start() { return true; }

void app::on_event(const SDL_Event& event) { (void)event; }

void app::on_fixed_step(float h) { (void)h; }

void app::on_overlay() {}

void app::on_stop() {}

// ---- The callbacks --------------------------------------------------------

SDL_AppResult app_runner::init(void** appstate, std::unique_ptr<app> instance,
                               int argc, char* argv[])
{
    if (appstate == nullptr) { return SDL_APP_FAILURE; }
    *appstate = nullptr;
    if (!instance) { return SDL_APP_FAILURE; }

    // OWNERSHIP CROSSES HERE, and it crosses BEFORE anything can fail.
    //
    // release() hands the raw pointer to SDL, which will hand it back to
    // app_runner::quit(), which wraps it in a unique_ptr again and lets it die.
    // The pointer is unowned for exactly the length of the program, which sounds
    // alarming until you notice the alternative: publishing it only on success
    // means an early failure below has to destroy the instance HERE, and SDL
    // then calls quit() with a null appstate and no way to know whether that
    // means "never built" or "already cleaned up".
    //
    // One owner, one teardown path, one place that deletes. The window in which
    // the raw pointer is exposed contains no branches at all.
    app* const self = instance.release();
    *appstate = self;

    // configure() runs before SDL exists, which is precisely why it takes argv:
    // a flag that chooses the surface must be read before the surface is made.
    app_config cfg = self->configure(argc, argv);

    // Lesson 5.3. Filled in for the app rather than by it: `--log` should work
    // on every program built on the engine, including the ones whose author
    // never heard of it. An app that genuinely wants to own the flag sets
    // `argv` itself and gets whatever it set.
    if (cfg.argv == nullptr)
    {
        cfg.argc = argc;
        cfg.argv = argv;
    }

    if (!self->sys().start(cfg))
    {
        // No log here — platform::start already said exactly which call failed
        // and what SDL_GetError() had to say about it. A second, vaguer line
        // above it would only make the real message harder to find.
        return SDL_APP_FAILURE;
    }

    if (!self->on_start())
    {
        ENGINE_LOG_ERROR(engine::log_platform, "app: on_start() failed");
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult app_runner::iterate(void* appstate)
{
    app* const self = static_cast<app*>(appstate);
    if (self == nullptr) { return SDL_APP_FAILURE; }

    platform& sys = self->sys();

    // Nothing to do if a quit is already pending. SDL's own loop checks this too
    // — SDL_IterateMainCallbacks reads its result atom before calling us, so in
    // production this branch is never the one that catches it — but a callback
    // should be TOTAL: safe to call at any moment, including a moment SDL would
    // not have chosen. That is what makes the four functions testable without an
    // entry point, which is how verify_52 §D drives them at all.
    if (!sys.running())
    {
        return sys.failed() ? SDL_APP_FAILURE : SDL_APP_SUCCESS;
    }

    // ---- Lesson 1.4's loop, minus the `while` ------------------------------
    //
    // Compare it, line for line, against the loop the demos have been writing
    // since Lesson 1.4:
    //
    //     drain events  ->  SDL did that, and called on_event for each one
    //     tick clock    ->  begin_frame()
    //     update input  ->  begin_frame()
    //     N fixed steps ->  the while below
    //     render        ->  on_frame(alpha)
    //     present       ->  blit_framebuffer() + on_overlay() + present()
    //
    // Nothing about the loop changed. What changed is who says `while`.
    sys.begin_frame();

    while (sys.next_step())
    {
        self->on_fixed_step(sys.steps().h());
    }

    self->on_frame(sys.steps().alpha());

    sys.blit_framebuffer();
    self->on_overlay();
    sys.present();

    // And checked again at the BOTTOM, which is the check that matters: a quit
    // requested DURING this frame still lets the frame finish. `--shot` depends
    // on it — the frame is drawn, written to disk, and only then is the program
    // allowed to end. The two checks are not redundant. The one above declines
    // to START a frame; this one refuses to ABANDON one.
    if (!sys.running())
    {
        return sys.failed() ? SDL_APP_FAILURE : SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult app_runner::event(void* appstate, SDL_Event* event)
{
    app* const self = static_cast<app*>(appstate);
    if (self == nullptr || event == nullptr) { return SDL_APP_FAILURE; }

    // Platform first, app second, and the order is a guarantee we are making: by
    // the time on_event() sees an event, `in()` has already been fed it and a
    // close request has already been noticed. So an override never has to
    // remember to call a base implementation — the classic framework trap, where
    // forgetting `Base::onEvent(e)` breaks input in a way that looks like an
    // input bug rather than an override bug.
    self->sys().handle(*event);
    self->on_event(*event);

    if (!self->sys().running())
    {
        return self->sys().failed() ? SDL_APP_FAILURE : SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

void app_runner::quit(void* appstate, SDL_AppResult result)
{
    // The other end of the handoff. Wrapping the raw pointer back into a
    // unique_ptr means the instance is destroyed when this function returns, by
    // the ordinary rules, even if on_stop() throws — which it will not, since the
    // engine core has no exceptions, but the shape is right and costs nothing.
    const std::unique_ptr<app> owned(static_cast<app*>(appstate));
    if (!owned) { return; }

    if (result == SDL_APP_FAILURE)
    {
        ENGINE_LOG_ERROR(engine::log_platform, "app: shutting down after a failure");
    }

    // on_stop() only if the platform actually came up. If start() failed there is
    // no window, no framebuffer and no clock, and a destructor that assumed
    // otherwise would fault while cleaning up after an error — the worst possible
    // moment, because it replaces a clear message with a crash.
    if (owned->sys().started())
    {
        owned->on_stop();
    }

    // Explicit rather than left to the platform's destructor, because the ORDER
    // matters and only being explicit makes it visible: on_stop() may still touch
    // the window (a GPU demo releases its device here), so the window must
    // outlive it.
    owned->sys().stop();
}

} // namespace engine
