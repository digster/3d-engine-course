// engine/include/engine/ui/debug_ui.hpp — Dear ImGui, owned properly.
//
// Lesson 5.11. The course's first third-party USER INTERFACE, and the first
// third-party library whose vocabulary is allowed through the engine's public
// boundary. Both of those are decisions, and both are argued below rather than
// assumed.
//
// ---------------------------------------------------------------------------
// WHY THIS DIRECTORY IS CALLED ui/ AND WHAT IS BANNED FROM IT
// ---------------------------------------------------------------------------
//
// **Tooling UI only. Never gameplay UI.** That is a binding rule of this course
// (§4) and it is an engineering line, not a matter of taste. Dear ImGui rebuilds
// its geometry from scratch every frame, knows nothing about gamepad focus
// order, localisation, safe areas, or a designer with an opinion, and hard-codes
// a look that says "developer build" from across a room — which is exactly right
// for a tool and exactly wrong for a menu somebody paid for. A game's HUD is
// built out of the renderer, in Module 6, with a material and a font atlas.
//
// So: `engine/ui/` is where the engine's *tooling* surface lives. Module 9's
// editor — hierarchy, inspector, gizmos — is built on this file, and the
// top-level `tools/` directory it will live in is a different thing (executables
// that use the engine, not part of it).
//
// ---------------------------------------------------------------------------
// WHY THE ENGINE TAKES THIS DEPENDENCY *PUBLICLY*, WHEN stb_image IS PRIVATE
// ---------------------------------------------------------------------------
//
// Lesson 4.7 acquired stb_image and hid it completely: exactly one translation
// unit includes it, `engine::image` is our own type, and engine/CMakeLists.txt
// links it PRIVATE, so no demo can see stb or accidentally start depending on
// it. That is what a good wrapper buys, and it worked.
//
// ImGui gets the opposite treatment, and the reason is not laziness. **With
// stb_image we wrapped a CONCEPT — "decode these bytes into pixels" — which is
// one function and one type. ImGui's value IS its vocabulary**: `Begin`, `Text`,
// `SliderFloat`, `BeginTable`, `Checkbox`, and four hundred more, each with
// overloads, flags, and an interaction model. A wrapper around that surface is a
// re-spelling with no content in it, and it must be re-spelled again for every
// widget, forever, by somebody who is not the person who wrote the widget.
//
// So `demos/` includes <imgui.h> directly and calls ImGui:: functions, and
// engine/CMakeLists.txt links imgui PUBLIC. The cost is real and worth stating
// out loud: the engine's public API now contains a second third-party
// vocabulary, and if ImGui were ever replaced, every panel in the project would
// be rewritten. What makes that acceptable is CONTAINMENT — the only code that
// speaks ImGui is tooling code, which is code that can be rewritten without the
// game changing behaviour by one pixel. Compare Lesson 5.1 §6 on SDL in our
// public headers: the same admission, made for the same reason, with the same
// eyes open.
//
// This header does NOT include <imgui.h>. Owning the lifecycle and speaking the
// widget language are two different jobs, and only the first is ours.
//
// ---------------------------------------------------------------------------
// THE ONE SINGLETON IN THIS ENGINE, AND WE DID NOT CHOOSE IT
// ---------------------------------------------------------------------------
//
// `asset_store` (5.5), `registry` (5.8), `action_map` (5.10) and `debug_lines`
// (5.11) are all VALUES: hold as many as you need, the engine does not decide.
// `debug_ui` cannot be, because ImGui keeps its context in a library global and
// every ImGui:: call reads it. Two live instances would silently share one
// context and one set of windows.
//
// We do not pretend otherwise. `start()` refuses a second instance, loudly, and
// says so in the log — an enforced limit you can read beats an undocumented one
// you discover. The right way to run two ImGui contexts is
// `ImGui::SetCurrentContext`, which is a real feature for a real need (multiple
// OS windows) and not one this engine has.

#pragma once

#include <SDL3/SDL.h>

namespace engine {

/// The Dear ImGui lifecycle, bound to one window and one SDL_Renderer.
///
/// ---------------------------------------------------------------------------
/// WHERE EACH CALL GOES IN THE FRAME, AND WHY IT IS NOT NEGOTIABLE
/// ---------------------------------------------------------------------------
///
///     on_start()    start(window(), renderer())
///     on_event()    handle_event(e)          ── every event, before the frame
///     on_input()    begin_frame()            ── FIRST thing, before anything
///                                               reads wants_keyboard()
///     on_overlay()  …ImGui::Begin/…/End…     ── build the panels
///                   render()                 ── after the framebuffer blit
///     on_stop()     stop()
///
/// **`begin_frame()` belongs at the top of `on_input()`, and that is the whole
/// of this lesson's ordering argument.** The capture flags — "is the UI using
/// the keyboard right now?" — are computed inside ImGui's `NewFrame`, from the
/// events processed since the last one. Call `NewFrame` late (in `on_overlay`,
/// next to the panels, where it looks like it belongs) and every read of
/// `wants_keyboard()` during the frame answers about the PREVIOUS frame. The
/// symptom is one frame of leakage: the first keystroke after clicking into a
/// text field also reaches the game. One frame, every time, and it is invisible
/// unless you are looking for it.
///
/// Lesson 5.10 added `on_input()` for a different reason — a hook that runs
/// exactly once per frame, after input is published and before the simulation
/// reads it. That is exactly the property this needs, so 5.11 adds no hook.
///
/// **On a surface with no renderer this class does nothing, safely.** `start()`
/// returns false under `surface::headless` and `surface::gpu`, `running()` then
/// answers false, and every other call is a no-op. A `--shot` run therefore
/// produces byte-identical output whether or not this file exists — which is
/// what lets Lesson 5.1's reference image survive an eleventh lesson.
class debug_ui
{
public:
    debug_ui() = default;

    /// Calls stop(). A UI that leaks its ImGui context poisons the next one,
    /// and the failure arrives as an assertion inside somebody else's library.
    ~debug_ui();

    debug_ui(const debug_ui&) = delete;
    debug_ui& operator=(const debug_ui&) = delete;
    debug_ui(debug_ui&&) = delete;
    debug_ui& operator=(debug_ui&&) = delete;

    /// Create the ImGui context and attach the SDL3 + SDL_Renderer backends.
    ///
    /// Returns false — **without logging an error** — when `window` or
    /// `renderer` is null. That is not a failure: it is `surface::headless` or
    /// `surface::gpu`, both of which are legitimate places to run a program that
    /// happens to contain a debug panel. It logs at info, because "the UI is off
    /// and here is why" is worth being able to see.
    ///
    /// Returns false WITH an error when a second instance tries to start; see
    /// the singleton note at the top of this file.
    [[nodiscard]] bool start(SDL_Window* window, SDL_Renderer* renderer);

    /// Shut the backends down and destroy the context. Idempotent.
    void stop();

    /// True between a successful start() and stop(). **Panels must be guarded on
    /// this**: with no context, an `ImGui::Begin` is a null dereference inside
    /// the library.
    [[nodiscard]] bool running() const { return running_; }

    /// Give ImGui one SDL event. Safe (and a no-op) when not running.
    ///
    /// Returns ImGui's own "I used this" answer, which is **advisory and is not
    /// what the engine arbitrates on**. The event is delivered to
    /// `engine::input` as well, always, and the two consumers are separated
    /// later by `masked_input` reading the capture flags. Withholding an event
    /// from `input` instead would leave it believing a key it never saw released
    /// is still held.
    bool handle_event(const SDL_Event& event);

    /// Open a UI frame. Call once per frame, at the top of `on_input()`, before
    /// anything reads the capture flags. No-op when not running.
    void begin_frame();

    /// Draw the panels built since `begin_frame()`. Call once, in `on_overlay()`
    /// — after the framebuffer has been blitted, before the frame is presented.
    /// No-op when not running, or when no frame is open.
    void render();

    // ---- Who owns the input right now ---------------------------------------
    //
    // All three answer false when not running, so a program written against them
    // behaves identically with the UI compiled in and switched off.

    /// The UI is using the keyboard — a text field has focus, or a window is
    /// being driven by the keyboard. Mask the keyboard while this is true.
    [[nodiscard]] bool wants_keyboard() const;

    /// The cursor is over an ImGui window, or a widget is being dragged. Mask
    /// the mouse while this is true.
    [[nodiscard]] bool wants_mouse() const;

    /// A text field is accepting characters. A strict subset of
    /// `wants_keyboard()`, and the one a game would use to decide whether to
    /// show an on-screen keyboard.
    [[nodiscard]] bool wants_text() const;

    /// The ImGui version string this engine was built against, e.g. "1.92.9b".
    /// Available whether or not a UI is running — it is a build fact, not a
    /// runtime one — and worth putting in a HUD, because "which ImGui?" is the
    /// first question when a widget behaves unexpectedly.
    [[nodiscard]] static const char* version();

private:
    bool running_ = false;
    bool frame_open_ = false;
    SDL_Renderer* renderer_ = nullptr;
};

}   // namespace engine
