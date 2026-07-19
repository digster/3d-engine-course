// src/game/pong.hpp — the Module 1 checkpoint: a complete, playable game.
//
// This is the first file in the project that is not part of the engine, and the
// directory it sits in is the point. Everything under src/core, src/gfx and
// src/math answers questions any game would ask — what time is it, what keys are
// down, how do I set a pixel. Nothing under those directories knows that paddles
// exist. This file knows nothing but paddles.
//
// That line is currently just a directory name and a discipline. In Module 5 it
// becomes physical: the engine turns into a static library with public headers,
// and game code is only allowed to include those. Practising the separation now,
// while it is still cheap to get wrong, is what makes that refactor feel like a
// formality rather than a rewrite.

#pragma once

#include "math/vec2.hpp"

#include <SDL3/SDL.h>

// A forward declaration, not an include. This header needs to *name* the
// framebuffer type in draw()'s signature, but it never dereferences one, so the
// compiler needs no knowledge of the class beyond "it exists". pong.cpp includes
// the real header because it actually draws.
//
// The habit is worth forming: every #include in a header is pasted into every
// file that includes it, and into every file that includes *those*. Headers that
// include what they merely mention are the reason large C++ projects take twenty
// minutes to build. Module 5 revisits this as "physical design".
namespace engine { class framebuffer; }

namespace game {

/// The court's fixed geometry, in framebuffer pixels — one place, so the drawing
/// code and the collision code can never disagree about where a paddle is.
///
/// `inline constexpr` rather than plain `constexpr`: at namespace scope `const`
/// (and therefore `constexpr`) implies internal linkage, so each translation unit
/// including this header would get its own private copy of every constant. That
/// is harmless for numbers, but `inline` says what we actually mean — one entity,
/// shared — and it is the same keyword that will matter a great deal when these
/// become something bigger than a float.
namespace court {

inline constexpr float width  = 320.0f;
inline constexpr float height = 180.0f;

inline constexpr float paddle_w = 4.0f;
inline constexpr float paddle_h = 30.0f;
inline constexpr float paddle_inset = 10.0f;   ///< gap between a paddle and its wall
inline constexpr float ball_size = 4.0f;

inline constexpr float paddle_speed = 132.0f;  ///< pixels per second, both players
inline constexpr float ai_skill = 0.82f;       ///< the AI's fraction of full paddle speed

inline constexpr float serve_speed = 105.0f;   ///< ball speed at the start of a rally
inline constexpr float speed_gain = 7.0f;      ///< added to ball speed per paddle hit
inline constexpr float max_speed = 260.0f;     ///< and the ceiling it climbs toward

/// The steepest angle a paddle can impart, in radians. 1.0472 is 60°, measured
/// from the horizontal — see §3.4 for why the classic game chooses the hit
/// position rather than physics to decide this.
inline constexpr float max_bounce_angle = 1.0472f;

inline constexpr float serve_delay = 0.7f;     ///< seconds between a point and the next serve
inline constexpr int   winning_score = 11;

/// Where each paddle's near face sits. Derived, never typed twice.
inline constexpr float left_x  = paddle_inset;
inline constexpr float right_x = width - paddle_inset - paddle_w;

} // namespace court

/// What the two players are asking for during one simulation step.
///
/// The simulation never touches the keyboard. It is handed this, and that is the
/// whole of its knowledge of the outside world — which is what makes it
/// replayable, testable, and (Module 8) recordable. Lesson 1.4's determinism
/// argument only holds if the sim's inputs are explicit, and this struct is what
/// "explicit" looks like.
struct intent
{
    float left = 0.0f;          ///< −1 up, +1 down, 0 still. Not a key — a direction.
    float right = 0.0f;
    bool right_is_ai = true;    ///< when true, `right` is ignored and the AI drives
};

/// Which part of the game we are in. `Uint8` underlying type because there are
/// three of them and this struct will one day be serialised.
enum class phase : Uint8
{
    serving,   ///< point just scored; ball parked at centre, counting down
    rally,     ///< ball in play
    over       ///< someone reached winning_score; frozen until reset
};

/// Everything the game *is*, and nothing else.
///
/// One plain struct, copyable by memcpy, with no pointers and no owned
/// resources. That is not tidiness for its own sake — it is what makes
/// `previous = current` in the fixed-step loop a single cheap assignment, and
/// what will let Module 8 write the whole thing to disk with one call. Anything
/// that cannot be copied like this does not belong in simulation state.
struct state
{
    engine::vec2 ball{};
    engine::vec2 ball_vel{};

    float left_y = 0.0f;        ///< top edge of each paddle
    float right_y = 0.0f;

    int left_score = 0;
    int right_score = 0;

    phase mode = phase::serving;
    float serve_timer = court::serve_delay;
    float serve_toward = 1.0f;  ///< +1 serves to the right player, −1 to the left
    int rally_hits = 0;         ///< paddle hits in the current rally, for the HUD

    /// True for exactly the step in which the ball jumped discontinuously — after
    /// a point is scored. Lesson 1.4's rule: **never interpolate across a
    /// teleport.** The loop reads this and snaps `previous = current` instead of
    /// rendering a smear across the whole court.
    bool teleported = false;

    /// The collision rule in force. `false` selects the naive "do the boxes
    /// overlap where the ball ended up?" test, which is what almost everyone
    /// writes first and which lets a fast ball pass straight through a paddle.
    /// Kept switchable so the bug can be seen rather than described — §3.3.
    bool swept_collision = true;

    /// The simulation's own random number generator, carried *in the state*.
    ///
    /// Not rand(), and not SDL_rand() — both keep their state in a hidden global.
    /// A simulation whose output depends on a global is not a function of its
    /// inputs, so it cannot be replayed, and two copies of it cannot be compared.
    /// Keeping the seed here means `state` still contains the entire game.
    Uint32 rng = 0x9E3779B9u;
};

/// A fresh game: centred paddles, 0–0, first serve pending.
[[nodiscard]] state make_state(Uint32 seed);

/// Advance the simulation by exactly `h` seconds.
///
/// `h` is the fixed step from engine::fixed_step, never a frame's real duration —
/// that separation is Lesson 1.4's whole argument, and it is enforced here by the
/// signature: there is nowhere to pass a frame time in.
void step(state& s, intent wanted, float h);

/// Draw the game into `fb`, interpolating between the two most recent simulation
/// states by `alpha`.
///
/// Taking both states is what makes the renderer's job unambiguous: it draws a
/// moment that never existed in the simulation, `alpha` of the way from the
/// previous step to the current one. The game module owns this rather than the
/// caller because it is the only code that knows *which* quantities may be
/// interpolated — positions yes, scores absolutely not.
void draw(engine::framebuffer& fb, const state& previous, const state& current, float alpha);

} // namespace game
