// src/core/input.hpp — a frame-coherent view of the keyboard and mouse.
//
// This is the engine's first real subsystem, and the first file that is not
// main.cpp. It exists because gameplay code asks two different questions about
// the same key, and answering them from raw events at every call site produces
// two different bugs.

#pragma once

#include <SDL3/SDL.h>

#include <array>

namespace engine {

/// A frame-coherent snapshot of the keyboard and mouse.
///
/// Gameplay asks two questions that sound alike and are not:
///
///   - "Is W down *right now*?"        -> key_down()     — a **level**: true for as
///                                        long as the key is held.
///   - "Did Space go down *this frame*?" -> key_pressed() — an **edge**: true for
///                                        exactly one frame per physical press.
///
/// Movement wants the level (hold to keep walking). Jumping, firing, and toggling
/// want the edge (one press, one jump, however long you hold it).
///
/// Both answers come from a single source of truth — the state snapshot — with
/// edges *derived* by comparing this frame's snapshot against last frame's. Two
/// independently maintained sources would eventually disagree; one cannot.
///
/// The per-frame contract, in this order, no exceptions:
///
///   1. Drain the SDL event queue, passing each event to feed_event().
///   2. Call update() exactly once.
///   3. Query as much as you like. The answers are frozen until the next update().
///
/// Step 1 must precede step 2 because SDL's internal state is only refreshed by
/// pumping the event queue (SDL_PollEvent pumps), so sampling before the drain
/// reads state that is one frame stale.
class input
{
public:
    /// Consume one event drained from the queue.
    ///
    /// Most input needs nothing here: SDL already tracks keys and mouse buttons
    /// internally, and update() samples that. Only quantities that exist *solely*
    /// as events — the wheel, which has no "current position" to ask for — must be
    /// caught as they go past.
    void feed_event(const SDL_Event& event);

    /// Publish one frame of input. Call once per frame, AFTER the event drain.
    void update();

    // ---- Keyboard ----------------------------------------------------------
    // Addressed by SDL_Scancode (physical key position), never SDL_Keycode (the
    // symbol the layout produces). SDL_SCANCODE_W is "the key where W sits on a
    // US QWERTY board" on every keyboard on earth; SDLK_W is 'z' on French AZERTY.
    // Movement is about positions, so movement uses scancodes.

    /// True while the key is held. The level.
    [[nodiscard]] bool key_down(SDL_Scancode key) const;

    /// True for exactly one frame, on the frame the key went down. The rising edge.
    [[nodiscard]] bool key_pressed(SDL_Scancode key) const;

    /// True for exactly one frame, on the frame the key came up. The falling edge.
    [[nodiscard]] bool key_released(SDL_Scancode key) const;

    // ---- Mouse -------------------------------------------------------------
    // `button` is an SDL button index: SDL_BUTTON_LEFT, SDL_BUTTON_MIDDLE,
    // SDL_BUTTON_RIGHT, SDL_BUTTON_X1, SDL_BUTTON_X2 (1..5, not 0-based).

    [[nodiscard]] bool mouse_down(int button) const;
    [[nodiscard]] bool mouse_pressed(int button) const;
    [[nodiscard]] bool mouse_released(int button) const;

    /// Cursor position in window coordinates: (0,0) is the top-left pixel, +Y down.
    [[nodiscard]] float mouse_x() const { return mouse_x_; }
    [[nodiscard]] float mouse_y() const { return mouse_y_; }

    /// Wheel movement accumulated during this frame, in notches. Positive Y is
    /// away from the user, positive X is to the right — already corrected for
    /// "natural"/flipped scrolling, so these directions hold on every platform.
    [[nodiscard]] float wheel_x() const { return wheel_x_; }
    [[nodiscard]] float wheel_y() const { return wheel_y_; }

private:
    /// Scancodes are 0..SDL_SCANCODE_COUNT-1 by construction, but a value cast in
    /// from an int is not checked by the type system. One comparison turns a
    /// potential out-of-bounds read into a defined `false`.
    [[nodiscard]] static bool valid(SDL_Scancode key)
    {
        return key >= 0 && static_cast<int>(key) < SDL_SCANCODE_COUNT;
    }

    /// Mouse buttons are 1-based; SDL_BUTTON_MASK(n) is 1u << (n-1).
    [[nodiscard]] static SDL_MouseButtonFlags mask(int button)
    {
        if (button < 1 || button > 5) { return 0; }
        return SDL_BUTTON_MASK(button);
    }

    // Two snapshots, one frame apart. `keys_` is this frame; `keys_prev_` is what
    // it was last frame. Every edge query is a comparison between the two, which
    // is why they must be *copies* — see input.cpp for why aliasing SDL's array
    // instead would silently produce an engine in which no key is ever pressed.
    std::array<bool, SDL_SCANCODE_COUNT> keys_{};
    std::array<bool, SDL_SCANCODE_COUNT> keys_prev_{};

    // The same two-snapshot trick for mouse buttons, but the five buttons fit in
    // one 32-bit word, so the "array" is a bitmask and the copy is a single move.
    SDL_MouseButtonFlags buttons_ = 0;
    SDL_MouseButtonFlags buttons_prev_ = 0;

    float mouse_x_ = 0.0f;
    float mouse_y_ = 0.0f;

    // The wheel is double-buffered by hand: feed_event() adds into the accumulator
    // during the drain, and update() publishes the total and clears it. Without
    // the split, a query made mid-drain would see a partial total, and the value
    // would not be stable across the frame.
    float wheel_x_ = 0.0f;
    float wheel_y_ = 0.0f;
    float wheel_accum_x_ = 0.0f;
    float wheel_accum_y_ = 0.0f;
};

} // namespace engine
