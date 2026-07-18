// src/core/input.cpp — implementation of the frame-coherent input snapshot.

#include "core/input.hpp"

#include <algorithm>

namespace engine {

void input::feed_event(const SDL_Event& event)
{
    // Only the wheel is handled here, and it is worth being precise about why.
    //
    // Keys and mouse buttons have a *current state* SDL will hand us on demand,
    // so update() just asks. The wheel has no current state: there is no such
    // thing as "how scrolled is the wheel". It only ever reports movement, and
    // movement that is not caught as it goes past is gone forever. Anything with
    // that shape — the wheel, typed text, relative mouse motion — has to be
    // accumulated from events. Everything else should be polled.
    if (event.type == SDL_EVENT_MOUSE_WHEEL)
    {
        // A wheel event carries a direction flag. On "natural scrolling" systems
        // (the macOS default, and an option elsewhere) SDL reports the raw values
        // with direction == SDL_MOUSEWHEEL_FLIPPED rather than pre-negating them,
        // so it is our job to normalise. Skip this and your scroll direction
        // inverts on some of your users' machines and not others — a bug report
        // you will never reproduce locally.
        const float sign = (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) ? -1.0f : 1.0f;
        wheel_accum_x_ += event.wheel.x * sign;
        wheel_accum_y_ += event.wheel.y * sign;
    }
}

void input::update()
{
    // ---- 1. This frame's state becomes next frame's "previous" ---------------
    // Do this first, before overwriting the current state. Everything the edge
    // queries know about the past lives in these two lines.
    keys_prev_ = keys_;
    buttons_prev_ = buttons_;

    // ---- 2. Sample SDL's live state -----------------------------------------
    // SDL_GetKeyboardState returns a pointer to SDL's own internal array — the
    // one SDL keeps mutating as events are pumped. That makes the next line the
    // most important line in this file:
    //
    //   We COPY the contents. We do not keep the pointer.
    //
    // Storing the pointer as "current" and last frame's pointer as "previous"
    // would store the same address twice. Both would name the same live array,
    // every comparison would compare the array to itself, and key_pressed()
    // would return false forever — with no crash, no warning, and no obvious
    // place to look. A pointer names a thing; a copy is a thing.
    int key_count = 0;
    const bool* sdl_keys = SDL_GetKeyboardState(&key_count);

    // Clamp defensively rather than trusting the reported length to match our
    // compile-time SDL_SCANCODE_COUNT. If the engine is ever linked against an
    // SDL built from different headers, this is the difference between a wrong
    // number and a buffer overrun.
    const int n = std::min(key_count, static_cast<int>(keys_.size()));
    std::copy_n(sdl_keys, n, keys_.begin());

    // Mouse buttons and position arrive together, in one call. The coordinates
    // are floats relative to the window's top-left, matching SDL3's float-based
    // render geometry.
    buttons_ = SDL_GetMouseState(&mouse_x_, &mouse_y_);

    // ---- 3. Publish the accumulated event-only deltas -----------------------
    // The drain has finished, so the accumulator holds this frame's complete
    // total. Publish it and reset, so the next frame starts from zero.
    wheel_x_ = wheel_accum_x_;
    wheel_y_ = wheel_accum_y_;
    wheel_accum_x_ = 0.0f;
    wheel_accum_y_ = 0.0f;

    // A note on a bug we deliberately do NOT fix here: keys held while the window
    // loses focus. SDL handles it — when keyboard focus moves away from every SDL
    // window, SDL_SetKeyboardFocus() calls SDL_ResetKeyboard(), which *sends key-up
    // events* for each held key (verified in SDL's src/events/SDL_keyboard.c).
    // Because they are events, our drain sees them and this snapshot picks the
    // change up — but only because we drain before we sample. Reverse the order
    // and the classic "alt-tab and your character keeps running" bug reappears.
}

bool input::key_down(SDL_Scancode key) const
{
    return valid(key) && keys_[static_cast<std::size_t>(key)];
}

bool input::key_pressed(SDL_Scancode key) const
{
    // The rising edge: down now, up last frame. Note that this is the whole of
    // edge detection — there is no separate bookkeeping, no event handler, and
    // therefore nothing that can drift out of sync with key_down().
    if (!valid(key)) { return false; }
    const std::size_t i = static_cast<std::size_t>(key);
    return keys_[i] && !keys_prev_[i];
}

bool input::key_released(SDL_Scancode key) const
{
    // The falling edge: up now, down last frame.
    if (!valid(key)) { return false; }
    const std::size_t i = static_cast<std::size_t>(key);
    return !keys_[i] && keys_prev_[i];
}

bool input::mouse_down(int button) const
{
    const SDL_MouseButtonFlags m = mask(button);
    return m != 0 && (buttons_ & m) != 0;
}

bool input::mouse_pressed(int button) const
{
    const SDL_MouseButtonFlags m = mask(button);
    return m != 0 && (buttons_ & m) != 0 && (buttons_prev_ & m) == 0;
}

bool input::mouse_released(int button) const
{
    const SDL_MouseButtonFlags m = mask(button);
    return m != 0 && (buttons_ & m) == 0 && (buttons_prev_ & m) != 0;
}

} // namespace engine
