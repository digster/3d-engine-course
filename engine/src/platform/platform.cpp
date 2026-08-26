// engine/src/platform/platform.cpp — the forty lines every demo used to open with.
//
// Read start() and stop() together. They are mirror images, and the mirror is the
// point: everything is created in dependency order and destroyed in exactly the
// reverse. That symmetry is the only thing standing between us and the class of
// bug where a texture outlives the renderer that made it — which does not crash
// on the machine you wrote it on, and does on somebody else's.

#include <engine/platform/platform.hpp>

#include <cstddef>
#include <cstring>

namespace engine {

const char* name_of(surface s)
{
    switch (s)
    {
    case surface::renderer: return "renderer";
    case surface::gpu:      return "gpu";
    case surface::headless: return "headless";
    }
    return "?";
}

platform::~platform()
{
    stop();
}

bool platform::start(const app_config& cfg)
{
    // Starting twice is not a recoverable situation, it is a mistake in the
    // caller: the second start would leak the first window and re-enter SDL_Init
    // with a live one already up. Refuse loudly rather than half-doing it.
    if (started_)
    {
        SDL_Log("platform::start called twice — ignoring the second");
        return false;
    }

    cfg_ = cfg;

    // Metadata before SDL_Init, because that is when SDL reads it. It is what the
    // window manager, the taskbar, and (on some platforms) the audio mixer call
    // us. Costs one line and removes "SDL Application" from the alt-tab list.
    if (!SDL_SetAppMetadata(cfg.title, "0.5.2", "org.engine-course.engine"))
    {
        SDL_Log("SDL_SetAppMetadata failed: %s — continuing", SDL_GetError());
    }

    // Video is implied by the surface rather than requested by the caller, and a
    // headless run deliberately does not ask for it. That is not a micro-
    // optimisation: on a build server there is no display, and
    // SDL_Init(SDL_INIT_VIDEO) FAILS there. A `--shot` that needed video would be
    // a `--shot` that could not run in CI, which is most of the reason it exists.
    SDL_InitFlags flags = cfg.extra_subsystems;
    if (cfg.draw_to != surface::headless) { flags |= SDL_INIT_VIDEO; }

    if (!SDL_Init(flags))
    {
        SDL_Log("SDL_Init(0x%08x) failed: %s", static_cast<unsigned>(flags), SDL_GetError());
        return false;
    }
    started_ = true;

    const int version = SDL_GetVersion();
    SDL_Log("platform: SDL %d.%d.%d, surface=%s",
            SDL_VERSIONNUM_MAJOR(version),
            SDL_VERSIONNUM_MINOR(version),
            SDL_VERSIONNUM_MICRO(version),
            name_of(cfg.draw_to));

    // ---- The window --------------------------------------------------------
    if (cfg.draw_to != surface::headless)
    {
        const SDL_WindowFlags window_flags = cfg.resizable ? SDL_WINDOW_RESIZABLE : 0;
        window_ = SDL_CreateWindow(cfg.title, cfg.window_width, cfg.window_height, window_flags);
        if (window_ == nullptr)
        {
            SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
            stop();
            return false;
        }
    }

    // ---- The renderer, and the fork it represents --------------------------
    //
    // Under surface::gpu we stop here, with a window and nothing else, and the
    // omission is the feature. Lesson 4.2: creating an SDL_Renderer claims the
    // window, and SDL_CreateGPUDevice on a claimed window fails with a message
    // that does not obviously say so. The one line NOT executed below is what
    // makes `sandbox` able to run a GPU device at all.
    if (cfg.draw_to == surface::renderer)
    {
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr)
        {
            SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
            stop();
            return false;
        }

        if (!set_vsync(cfg.vsync))
        {
            SDL_Log("vsync %s refused: %s — continuing unsynchronised",
                    cfg.vsync ? "on" : "off", SDL_GetError());
        }
    }

    // ---- The framebuffer, and the texture it is shown through --------------
    if (cfg.fb_width > 0 && cfg.fb_height > 0)
    {
        fb_.emplace(cfg.fb_width, cfg.fb_height);

        if (renderer_ != nullptr)
        {
            // ARGB8888 to match `engine::framebuffer`'s packing exactly (Lesson
            // 1.6). Matching means the upload is a memcpy; not matching would
            // mean a per-pixel swizzle nobody asked for, at 320x180x60 = 3.5
            // million pixels a second.
            screen_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        cfg.fb_width, cfg.fb_height);
            if (screen_ == nullptr)
            {
                SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
                stop();
                return false;
            }

            if (!SDL_SetTextureScaleMode(screen_,
                                         cfg.fb_nearest ? SDL_SCALEMODE_NEAREST
                                                        : SDL_SCALEMODE_LINEAR))
            {
                SDL_Log("SDL_SetTextureScaleMode failed: %s — continuing", SDL_GetError());
            }
        }
    }

    // ---- Time and input ----------------------------------------------------
    //
    // Assigned rather than constructed in the initialiser list, because their
    // parameters live in a config that does not exist until start() is called.
    // Both are plain value types with no owned resources, so assignment is a
    // copy of a handful of numbers.
    clock_ = clock(cfg.max_frame_seconds);
    steps_ = fixed_step(cfg.fixed_hz, cfg.max_steps_per_frame);

    // The clock's first tick() would otherwise measure the time from
    // construction — which includes SDL_Init and window creation, tens of
    // milliseconds of it — and hand that to the simulation as frame one.
    clock_.tick();

    running_ = true;
    failed_ = false;
    return true;
}

void platform::stop()
{
    // Reverse order, and each guard matters: stop() is called from start()'s
    // error paths, where only some of this exists.
    if (screen_ != nullptr)   { SDL_DestroyTexture(screen_);   screen_ = nullptr; }
    if (renderer_ != nullptr) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_ != nullptr)   { SDL_DestroyWindow(window_);     window_ = nullptr; }

    fb_.reset();

    if (started_)
    {
        // Under SDL_MAIN_USE_CALLBACKS, SDL calls SDL_Quit() itself after
        // SDL_AppQuit returns, so this is the first of two. That is safe —
        // SDL_Quit forces the init count to zero and the second call finds
        // nothing to do — and calling it here is what makes the same class
        // correct in a hand-written main(), where nobody else will.
        SDL_Quit();
        started_ = false;
    }

    running_ = false;
}

void platform::request_quit(bool success)
{
    running_ = false;
    if (!success) { failed_ = true; }
}

void platform::handle(const SDL_Event& event)
{
    input_.feed_event(event);

    switch (event.type)
    {
    case SDL_EVENT_QUIT:
        SDL_Log("quit requested");
        running_ = false;
        break;

    // SDL posts SDL_EVENT_QUIT when the last window closes, so for a
    // single-window program this case is redundant — and it is here anyway,
    // because a program that grows a second window (a tools window, Module 8)
    // would otherwise silently start ignoring its own close button.
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (window_ != nullptr && event.window.windowID == SDL_GetWindowID(window_))
        {
            SDL_Log("window %u close requested", static_cast<unsigned>(event.window.windowID));
            running_ = false;
        }
        break;

    default:
        break;
    }
}

void platform::pump()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        handle(event);
    }
}

void platform::begin_frame()
{
    // This order is a contract, not a preference, and both halves of it were
    // argued for a lesson each:
    //
    //   tick()   — Lesson 1.3: sample time once, so everything downstream agrees
    //              about how long this frame is.
    //   update() — Lesson 1.2: publish the input snapshot ONCE, after the drain,
    //              so key_pressed() is an edge rather than a race.
    //
    // Putting them here means a demo cannot get them in the wrong order, and
    // cannot forget the second one — which was a real bug in Lesson 1.2's first
    // draft, and presents as "no key is ever pressed".
    clock_.tick();
    input_.update();
    steps_.begin_frame(clock_.dt());
}

bool platform::next_step()
{
    return steps_.next_step();
}

void platform::blit_framebuffer()
{
    if (renderer_ == nullptr || screen_ == nullptr || !fb_.has_value()) { return; }

    void* dst_pixels = nullptr;
    int dst_pitch = 0;
    if (!SDL_LockTexture(screen_, nullptr, &dst_pixels, &dst_pitch))
    {
        SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
        return;
    }

    // Row by row rather than one memcpy, because the pitch SDL hands back may
    // exceed width*4 — some drivers pad each row to an alignment — and copying
    // as one block would shear the image diagonally on exactly the machines you
    // do not own. Lesson 1.5 §4.3.
    Uint8* const dst = static_cast<Uint8*>(dst_pixels);
    const framebuffer& source = *fb_;
    const std::size_t row_bytes = static_cast<std::size_t>(source.pitch());

    for (int y = 0; y < source.height(); ++y)
    {
        std::memcpy(dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(dst_pitch),
                    source.row(y), row_bytes);
    }

    SDL_UnlockTexture(screen_);

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderTexture(renderer_, screen_, nullptr, nullptr);
}

void platform::present()
{
    if (renderer_ == nullptr) { return; }
    SDL_RenderPresent(renderer_);
}

bool platform::set_vsync(bool on)
{
    if (renderer_ == nullptr) { return false; }

    const int mode = on ? 1 : SDL_RENDERER_VSYNC_DISABLED;
    if (!SDL_SetRenderVSync(renderer_, mode)) { return false; }

    vsync_ = on;
    return true;
}

} // namespace engine
