// engine/src/ui/debug_ui.cpp — the six ImGui calls that matter, in order.
//
// This is the only file in the engine that includes <imgui.h>. Everything else
// that speaks ImGui is tooling code in demos/ and, from Module 8, tools/ — which
// is the containment that made taking the dependency publicly acceptable in the
// first place (see the header).

#include <engine/ui/debug_ui.hpp>

#include <engine/core/log.hpp>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

namespace engine {

namespace {

// ImGui's context is a library global, so "is one already running?" is a
// process-wide question and has to be answered with process-wide state. One
// bool, in an anonymous namespace, is the smallest honest way to say that.
//
// It is NOT thread-safe, and neither is ImGui: every ImGui:: call must happen on
// the thread that made the context. Module 8's job system does not change that,
// and when it arrives this comment is the reminder.
bool g_context_live = false;

}   // namespace

debug_ui::~debug_ui()
{
    stop();
}

bool debug_ui::start(SDL_Window* window, SDL_Renderer* renderer)
{
    if (running_) { return true; }

    // NOT AN ERROR. `surface::headless` has no window and `surface::gpu` has no
    // SDL_Renderer, and a program built for either is a program that legitimately
    // has no debug UI. Logging this at error level would put a red line in every
    // `--shot` run in this repository and train the reader to ignore red lines.
    if (window == nullptr || renderer == nullptr)
    {
        ENGINE_LOG_INFO(engine::log_platform,
                        "debug_ui: no %s — the UI is off for this run",
                        (window == nullptr) ? "window" : "SDL_Renderer");
        return false;
    }

    if (g_context_live)
    {
        ENGINE_LOG_ERROR(engine::log_platform,
                         "debug_ui: a second instance cannot start — ImGui keeps its context in "
                         "a library global, so two of these would silently share one UI");
        return false;
    }

    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext() == nullptr)
    {
        ENGINE_LOG_ERROR(engine::log_platform, "debug_ui: ImGui::CreateContext failed");
        return false;
    }

    // Keyboard navigation on, gamepad navigation off. The first is what makes a
    // debug panel usable without reaching for the mouse; the second would quietly
    // eat a gamepad the game wants, which is precisely the class of bug this
    // lesson's input seam exists to prevent.
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // NO imgui.ini. ImGui writes window positions to a file beside the working
    // directory by default, which means a tool that behaves differently depending
    // on where you launched it from — the exact reproducibility problem Lesson
    // 3.5 solved for assets with SDL_GetBasePath. A debug panel that always opens
    // where the code says it opens is worth more than remembered positions.
    io.IniFilename = nullptr;

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer))
    {
        ENGINE_LOG_ERROR(engine::log_platform, "debug_ui: ImGui_ImplSDL3_InitForSDLRenderer failed");
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplSDLRenderer3_Init(renderer))
    {
        ENGINE_LOG_ERROR(engine::log_platform, "debug_ui: ImGui_ImplSDLRenderer3_Init failed");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    renderer_ = renderer;
    running_ = true;
    g_context_live = true;

    ENGINE_LOG_INFO(engine::log_platform, "debug_ui: Dear ImGui %s, SDL_Renderer backend",
                    IMGUI_VERSION);
    return true;
}

void debug_ui::stop()
{
    if (!running_) { return; }

    // REVERSE ORDER OF CREATION, and the two backends before the context — both
    // shutdowns reach into the context to release what they registered in it, so
    // destroying it first turns teardown into a use-after-free. The same rule
    // platform::stop() follows for the window and the renderer (Lesson 5.2).
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    renderer_ = nullptr;
    frame_open_ = false;
    running_ = false;
    g_context_live = false;
}

bool debug_ui::handle_event(const SDL_Event& event)
{
    // The guard is load-bearing rather than defensive: ImGui_ImplSDL3_ProcessEvent
    // asserts on a null backend, and events arrive before on_start() has run and
    // after on_stop() has torn down.
    if (!running_) { return false; }
    return ImGui_ImplSDL3_ProcessEvent(&event);
}

void debug_ui::begin_frame()
{
    if (!running_ || frame_open_) { return; }

    // THE ORDER OF THESE THREE IS FIXED BY ImGui, and it is renderer, then
    // platform, then core. The renderer backend rebuilds the font texture if it
    // was invalidated; the platform backend fills in display size, delta time and
    // this frame's input; ImGui::NewFrame then computes everything derived from
    // them — including WantCaptureKeyboard and WantCaptureMouse, which is why
    // this function has to run before anything asks.
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    frame_open_ = true;
}

void debug_ui::render()
{
    if (!running_ || !frame_open_) { return; }

    // Render() turns this frame's widget calls into vertex and index buffers;
    // RenderDrawData feeds them to SDL_Renderer. They are two calls because a
    // program may legitimately want the draw data without drawing it — a
    // screenshot tool, a test, or the SDL_GPU backend, which submits the same
    // ImDrawData through a command buffer instead.
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
    frame_open_ = false;
}

bool debug_ui::wants_keyboard() const
{
    return running_ && ImGui::GetIO().WantCaptureKeyboard;
}

bool debug_ui::wants_mouse() const
{
    return running_ && ImGui::GetIO().WantCaptureMouse;
}

bool debug_ui::wants_text() const
{
    return running_ && ImGui::GetIO().WantTextInput;
}

const char* debug_ui::version()
{
    return IMGUI_VERSION;
}

}   // namespace engine
