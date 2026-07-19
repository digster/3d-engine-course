// src/game/pong.cpp — the game's rules, in one place.
//
// Three ideas do most of the work here, and only the third is new:
//
//   1. The simulation is a pure function of (state, intent, h). It reads no
//      globals, touches no hardware, and returns nothing — everything it changes
//      is in the struct it was handed. That is what makes it replayable.
//   2. Direction and speed are chosen separately (Lesson 1.7). Every ball
//      velocity in this file is built as `unit_direction * speed`, never as two
//      numbers that happen to have the right ratio.
//   3. Collision is a question about the ball's *path*, not about where it ended
//      up. That distinction is §3.3 of the lesson, and it is the difference
//      between a game that works and a game that works on your machine.

#include "game/pong.hpp"

#include "gfx/colour.hpp"
#include "gfx/framebuffer.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace game {
namespace {

// ---- Palette ----------------------------------------------------------------
// Chosen, not computed. These are interface colours rather than light, so they
// are picked by eye and never mixed — which, per Lesson 1.6, is exactly the case
// where storing sRGB-encoded values and leaving them alone is entirely correct.

constexpr Uint32 k_bg       = engine::pack_argb(10, 12, 18);
constexpr Uint32 k_net      = engine::pack_argb(48, 52, 66);
constexpr Uint32 k_left     = engine::pack_argb(122, 196, 152);
constexpr Uint32 k_right    = engine::pack_argb(236, 122, 92);
constexpr Uint32 k_ball     = engine::pack_argb(232, 232, 220);
constexpr Uint32 k_score    = engine::pack_argb(84, 90, 110);
constexpr Uint32 k_winner   = engine::pack_argb(226, 196, 110);

// ---- Deterministic randomness ------------------------------------------------

/// xorshift32 (Marsaglia, 2003): three shifts and three xors, and a period of
/// 2³²−1.
///
/// Not a good generator by cryptographic or even statistical standards, and it
/// does not need to be — it picks serve angles. What it *is* is small enough to
/// read in one sitting, and stateless apart from the word we hand it, which is
/// the property that matters: the same state produces the same game, forever.
///
/// Zero is the one seed to avoid. All three operations map 0 to 0, so a zeroed
/// generator is stuck there permanently — make_state() guards against it.
[[nodiscard]] Uint32 next_bits(Uint32& rng)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

/// A float in [0, 1).
///
/// The shift is deliberate. A `float` has 24 bits of mantissa, so taking the top
/// 24 bits of the word and scaling by 2⁻²⁴ produces every representable value in
/// the range exactly once, with no rounding. Dividing the full 32 bits by 2³²
/// would quietly round, and the low bits — the worst ones in this generator —
/// would be the ones deciding the result.
[[nodiscard]] float next_unit(Uint32& rng)
{
    return static_cast<float>(next_bits(rng) >> 8) * (1.0f / 16777216.0f);
}

// ---- Paddles -----------------------------------------------------------------

/// Move one paddle, then keep it on the court.
///
/// `direction` is −1, 0 or +1 (or anything between, for the AI's gentler drift).
/// Speed is applied here, once, so no caller has to remember to.
void move_paddle(float& y, float direction, float speed, float h)
{
    y += direction * speed * h;
    y = std::clamp(y, 0.0f, court::height - court::paddle_h);
}

/// The opponent: track the ball, but not perfectly.
///
/// Three deliberate handicaps, because a paddle that simply matches the ball's y
/// is unbeatable and therefore not a game:
///
///   - it moves at `ai_skill` of full speed, so a steep return outruns it;
///   - it only chases while the ball is heading its way, and drifts home
///     otherwise, which is what gives the player an opening;
///   - it has a deadzone, without which it would jitter one pixel either side of
///     the ball forever.
///
/// This is the entirety of the AI in this course until Module 7's character
/// controller, and it is worth noticing how far three lines of "move toward the
/// thing, slowly" get you.
[[nodiscard]] float ai_direction(const state& s)
{
    const float paddle_centre = s.right_y + court::paddle_h * 0.5f;

    // Not coming this way — return toward the middle, unhurried.
    if (s.ball_vel.x <= 0.0f)
    {
        const float home = (court::height - court::paddle_h) * 0.5f;
        const float delta = home - s.right_y;
        if (std::fabs(delta) < 2.0f) { return 0.0f; }
        return (delta > 0.0f) ? 0.45f : -0.45f;
    }

    const float ball_centre = s.ball.y + court::ball_size * 0.5f;
    const float delta = ball_centre - paddle_centre;
    if (std::fabs(delta) < 3.0f) { return 0.0f; }   // deadzone: stop the jitter
    return (delta > 0.0f) ? 1.0f : -1.0f;
}

// ---- Collision ---------------------------------------------------------------

/// Do two axis-aligned boxes overlap?
///
/// Read it as the *separating axis* question turned inside out. Two boxes are
/// apart exactly when you can find an axis on which their shadows do not touch;
/// they overlap only when the shadows overlap on **every** axis. In 2-D with
/// axis-aligned boxes there are just two axes to check, and each check is the
/// pair of comparisons below.
///
/// That is the whole idea behind the Separating Axis Theorem, which Module 7
/// generalises to boxes at arbitrary angles — where the axes to test stop being
/// obvious but the question does not change.
[[nodiscard]] bool boxes_overlap(engine::vec2 a_min, engine::vec2 a_size,
                                 engine::vec2 b_min, engine::vec2 b_size)
{
    return a_min.x < b_min.x + b_size.x
        && a_min.x + a_size.x > b_min.x
        && a_min.y < b_min.y + b_size.y
        && a_min.y + a_size.y > b_min.y;
}

/// Does the ball overlap a paddle vertically, given the ball's top edge?
[[nodiscard]] bool overlaps_vertically(float ball_y, float paddle_y)
{
    return ball_y < paddle_y + court::paddle_h
        && ball_y + court::ball_size > paddle_y;
}

/// When during this step does the ball's leading edge reach the paddle's face?
///
/// Returns the fraction of the step at which contact happens, in [0, 1], or a
/// negative number for no contact. This is the swept test — the one that asks
/// about the ball's whole path rather than its endpoint.
///
/// @param from         ball's top-left corner at the start of the step
/// @param to           where it would be at the end, if nothing were in the way
/// @param lead_offset  distance from the ball's left edge to its leading edge:
///                     0 when moving left, ball_size when moving right
/// @param face_x       the x the leading edge must cross to be a hit
/// @param paddle_y     the paddle's top edge
/// @param moving_right which way the ball is going, which flips both comparisons
[[nodiscard]] float time_of_impact(engine::vec2 from, engine::vec2 to,
                                   float lead_offset, float face_x,
                                   float paddle_y, bool moving_right)
{
    const float lead_from = from.x + lead_offset;
    const float lead_to   = to.x + lead_offset;

    // Both halves of this condition are load-bearing. "Ended past the face" alone
    // would also fire for a ball that *started* past it — one that got behind the
    // paddle last step and is now on its way to a point. Requiring that it began
    // on the near side is what makes this a crossing test rather than a
    // side-of-the-plane test.
    const bool crossed = moving_right ? (lead_from <= face_x && lead_to > face_x)
                                      : (lead_from >= face_x && lead_to < face_x);
    if (!crossed) { return -1.0f; }

    // Crossing implies motion, so this denominator cannot be zero: if lead_from
    // and lead_to were equal, one of the two strict comparisons above would have
    // failed. The invariant is worth stating precisely because the alternative is
    // a division by zero that only shows up on a stationary ball.
    const float travelled = lead_to - lead_from;
    const float t = (face_x - lead_from) / travelled;

    // Where the ball was *at that instant* — not where it ended up. Using the
    // endpoint's y here would be the same mistake one dimension over.
    const float y_at_contact = from.y + (to.y - from.y) * t;
    if (!overlaps_vertically(y_at_contact, paddle_y)) { return -1.0f; }

    return t;
}

/// The direction the ball leaves a paddle, decided by *where* it struck.
///
/// A physically honest paddle would mirror the ball: reflect() with a normal of
/// (±1, 0), which for a flat vertical surface just negates the x component. Try
/// it (Exercise 1.8.1) and the game dies — the ball settles into a fixed shallow
/// path and the rally becomes a metronome, because nothing in the system can ever
/// change the ball's y speed.
///
/// So the 1972 original cheats, and every version since has kept the cheat: the
/// paddle is treated as though it were curved, with the hit position choosing an
/// angle. Hit the centre and the ball comes back flat; hit the top edge and it
/// leaves steeply upward. That single decision is what turns the paddle from a
/// wall into a control.
///
/// It is worth being clear about what happened here: we chose playability over
/// physics, deliberately, and wrote it down. Engines are full of these; the ones
/// that cause trouble are the undocumented ones.
[[nodiscard]] engine::vec2 bounce_direction(float ball_y, float paddle_y, bool to_the_right)
{
    const float ball_centre = ball_y + court::ball_size * 0.5f;
    const float paddle_centre = paddle_y + court::paddle_h * 0.5f;

    // −1 at the paddle's top edge, 0 at its centre, +1 at its bottom edge. The
    // clamp catches a glancing hit at the very tip, where the ball's centre can
    // sit slightly outside the paddle's span.
    const float offset = std::clamp(
        (ball_centre - paddle_centre) / (court::paddle_h * 0.5f), -1.0f, 1.0f);

    const float angle = offset * court::max_bounce_angle;

    // (cos θ, sin θ) has length 1 for every θ — that is the Pythagorean identity,
    // not a coincidence — so there is nothing to normalise. Calling normalised()
    // here would buy a square root and no correctness.
    return {std::cos(angle) * (to_the_right ? 1.0f : -1.0f), std::sin(angle)};
}

// ---- Scoring -----------------------------------------------------------------

/// Award a point and reset for the next serve.
/// @param to_left true when the left player scored.
void award_point(state& s, bool to_left)
{
    if (to_left) { s.left_score += 1; } else { s.right_score += 1; }

    s.ball = {court::width * 0.5f - court::ball_size * 0.5f,
              court::height * 0.5f - court::ball_size * 0.5f};
    s.ball_vel = {0.0f, 0.0f};

    s.serve_timer = court::serve_delay;
    s.serve_toward = to_left ? -1.0f : 1.0f;   // serve toward whoever conceded
    s.rally_hits = 0;
    s.mode = phase::serving;

    // The ball did not travel to the centre — it *appeared* there. Interpolating
    // between last step's position (off the edge of the court) and this one would
    // draw a ball streaking across the whole screen at a speed it never had.
    // Lesson 1.4 §5.4; the loop reads this flag and snaps instead.
    s.teleported = true;

    if (s.left_score >= court::winning_score || s.right_score >= court::winning_score)
    {
        s.mode = phase::over;
    }
}

/// Launch the ball at a random-ish angle toward whoever just conceded.
void serve(state& s)
{
    // A spread narrower than a paddle can produce, so a serve is always
    // returnable — the randomness is there for variety, not to decide points.
    const float spread = court::max_bounce_angle * 0.55f;
    const float angle = (next_unit(s.rng) * 2.0f - 1.0f) * spread;

    const engine::vec2 direction{std::cos(angle) * s.serve_toward, std::sin(angle)};
    s.ball_vel = direction * court::serve_speed;
    s.mode = phase::rally;
}

// ---- The ball ----------------------------------------------------------------

/// Move the ball one step, resolving whatever it runs into on the way.
void advance_ball(state& s, float h)
{
    const engine::vec2 from = s.ball;
    engine::vec2 to = s.ball + s.ball_vel * h;

    // --- Top and bottom walls -------------------------------------------------
    // Two things happen at a bounce, and forgetting either is a classic bug:
    // the velocity turns around, AND the position is mirrored back inside. Fix
    // only the velocity and the ball spends a step outside the court; fix only
    // the position and it sticks to the wall, re-colliding every step.
    //
    // The reflection is written in its general form even though these walls are
    // axis-aligned and a sign flip would do. Two reasons: it is the same cost
    // (reflect() is constexpr and folds to a negation here), and it is the form
    // that keeps working when Exercise 1.8.4 tilts a wall.
    if (to.y < 0.0f)
    {
        to.y = -to.y;                                              // mirror in y = 0
        s.ball_vel = engine::reflect(s.ball_vel, engine::vec2{0.0f, 1.0f});
    }
    else if (to.y + court::ball_size > court::height)
    {
        const float overshoot = (to.y + court::ball_size) - court::height;
        to.y -= 2.0f * overshoot;                                  // mirror in the floor
        s.ball_vel = engine::reflect(s.ball_vel, engine::vec2{0.0f, -1.0f});
    }

    // --- Paddles --------------------------------------------------------------
    // Only the paddle the ball is heading toward can be hit, which halves the
    // work and — more usefully — makes it impossible to "collide" with the paddle
    // the ball is moving away from after it has already passed it.
    const bool moving_right = s.ball_vel.x > 0.0f;
    const float lead_offset = moving_right ? court::ball_size : 0.0f;
    const float face_x = moving_right ? court::right_x : court::left_x + court::paddle_w;
    const float paddle_y = moving_right ? s.right_y : s.left_y;

    bool hit = false;
    float impact_t = 0.0f;
    float impact_y = 0.0f;

    if (s.swept_collision)
    {
        const float t = time_of_impact(from, to, lead_offset, face_x, paddle_y, moving_right);
        if (t >= 0.0f)
        {
            hit = true;
            impact_t = t;
            impact_y = from.y + (to.y - from.y) * t;
        }
    }
    else
    {
        // The naive test, kept so it can be switched on and watched failing: does
        // the ball overlap the paddle *where it ended up*? It says nothing about
        // the ball's path, so a ball that stepped clean over the paddle reports
        // no collision at all. §3.3 works out exactly how fast that has to be.
        const float paddle_x = moving_right ? court::right_x : court::left_x;
        if (boxes_overlap(to, {court::ball_size, court::ball_size},
                          {paddle_x, paddle_y}, {court::paddle_w, court::paddle_h}))
        {
            hit = true;
            impact_t = 1.0f;
            impact_y = to.y;
        }
    }

    if (hit)
    {
        // Speed and direction, chosen independently — Lesson 1.7's rule, and the
        // reason a rally can get faster without also getting flatter.
        const float speed = std::min(engine::length(s.ball_vel) + court::speed_gain,
                                     court::max_speed);
        s.ball_vel = bounce_direction(impact_y, paddle_y, !moving_right) * speed;

        // Sit the ball exactly on the face it touched, then spend the *rest* of
        // the step travelling the new way. Without that remainder the ball loses
        // a fraction of a step's motion at every single bounce — invisible per
        // hit, and a measurable drag on a long rally.
        s.ball.x = face_x - lead_offset;
        s.ball.y = impact_y;
        s.ball += s.ball_vel * (h * (1.0f - impact_t));

        s.rally_hits += 1;

        // One impact per step, deliberately. A ball that would hit a paddle and
        // then a wall within the same step gets the wall next step instead —
        // half a millisecond late at 60 Hz, and never noticeable. Module 7 does
        // the honest thing and iterates until the step is used up.
        s.ball.y = std::clamp(s.ball.y, 0.0f, court::height - court::ball_size);
    }
    else
    {
        s.ball = to;
    }

    // --- Points ---------------------------------------------------------------
    // Fully past the edge, not merely touching it, so the ball is seen to leave.
    if (s.ball.x + court::ball_size < 0.0f)
    {
        award_point(s, false);
    }
    else if (s.ball.x > court::width)
    {
        award_point(s, true);
    }
}

// ---- Digits ------------------------------------------------------------------

/// A 3×5 bitmap font, digits only.
///
/// Fifteen bits per glyph, written top row first, left to right, so the literal
/// below is a picture of the digit it draws. This is the entire idea behind a
/// bitmap font — a glyph is a rectangle of on/off pixels and a rule for reading
/// it — and it is all we need for two scores. Module 6 does real text properly,
/// with stb_truetype, kerning, and an atlas; this is the cheapest thing that
/// works, chosen knowingly.
constexpr std::array<Uint16, 10> k_digit_bits{{
    0b111'101'101'101'111u,   // 0
    0b010'110'010'010'111u,   // 1
    0b111'001'111'100'111u,   // 2
    0b111'001'111'001'111u,   // 3
    0b101'101'111'001'001u,   // 4
    0b111'100'111'001'111u,   // 5
    0b111'100'111'101'111u,   // 6
    0b111'001'001'001'001u,   // 7
    0b111'101'111'101'111u,   // 8
    0b111'101'111'001'111u,   // 9
}};

/// Draw one digit with `scale`-sized square pixels.
void draw_digit(engine::framebuffer& fb, int x, int y, int digit, int scale, Uint32 colour)
{
    if (digit < 0 || digit > 9) { return; }

    const Uint16 bits = k_digit_bits[static_cast<std::size_t>(digit)];

    for (int row = 0; row < 5; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            // The literal's leftmost bit is bit 14, so reading it in picture
            // order means counting down from there.
            const int shift = 14 - (row * 3 + col);
            if (((bits >> shift) & 1u) != 0u)
            {
                fb.fill_rect(x + col * scale, y + row * scale, scale, scale, colour);
            }
        }
    }
}

/// Draw a non-negative number, left-aligned at (x, y). Two digits is plenty for
/// a score that stops at 11.
void draw_number(engine::framebuffer& fb, int x, int y, int value, int scale, Uint32 colour)
{
    if (value >= 10)
    {
        draw_digit(fb, x, y, (value / 10) % 10, scale, colour);
        x += 4 * scale;   // 3 columns of glyph plus 1 of gap
    }
    draw_digit(fb, x, y, value % 10, scale, colour);
}

} // namespace

// ---- Public interface --------------------------------------------------------

state make_state(Uint32 seed)
{
    state s;
    s.rng = (seed == 0u) ? 0x9E3779B9u : seed;   // zero is xorshift's dead seed

    const float centre_y = (court::height - court::paddle_h) * 0.5f;
    s.left_y = centre_y;
    s.right_y = centre_y;

    s.ball = {court::width * 0.5f - court::ball_size * 0.5f,
              court::height * 0.5f - court::ball_size * 0.5f};
    s.ball_vel = {0.0f, 0.0f};

    s.mode = phase::serving;
    s.serve_timer = court::serve_delay;

    // Which way the first serve goes is the first thing the generator decides, so
    // two games with the same seed diverge only if the players do.
    s.serve_toward = (next_unit(s.rng) < 0.5f) ? -1.0f : 1.0f;

    return s;
}

void step(state& s, intent wanted, float h)
{
    // Cleared at the top of every step: the flag describes *this* step, and a
    // stale one would make the renderer refuse to interpolate forever.
    s.teleported = false;

    if (s.mode == phase::over) { return; }

    // Paddles move during the serve countdown too — being able to reposition
    // between points is most of what makes the countdown feel like a pause rather
    // than a lockout.
    move_paddle(s.left_y, wanted.left, court::paddle_speed, h);

    const float right_dir = wanted.right_is_ai ? ai_direction(s) : wanted.right;
    const float right_speed = wanted.right_is_ai
        ? court::paddle_speed * court::ai_skill
        : court::paddle_speed;
    move_paddle(s.right_y, right_dir, right_speed, h);

    if (s.mode == phase::serving)
    {
        s.serve_timer -= h;
        if (s.serve_timer <= 0.0f) { serve(s); }
        return;
    }

    advance_ball(s, h);
}

void draw(engine::framebuffer& fb, const state& previous, const state& current, float alpha)
{
    fb.clear(k_bg);

    // --- The net ---
    // Dashes rather than a solid line, for the same reason the original had them:
    // a solid line reads as a wall.
    for (int y = 2; y < static_cast<int>(court::height) - 2; y += 8)
    {
        fb.fill_rect(static_cast<int>(court::width) / 2 - 1, y, 2, 4, k_net);
    }

    // --- Scores ---
    // Drawn from `current` with no interpolation whatsoever. A score is a
    // discrete fact; there is no such thing as being 0.4 of the way to a point,
    // and lerping one would produce a number that was never true.
    const Uint32 left_colour = (current.mode == phase::over && current.left_score > current.right_score)
        ? k_winner : k_score;
    const Uint32 right_colour = (current.mode == phase::over && current.right_score > current.left_score)
        ? k_winner : k_score;

    draw_number(fb, static_cast<int>(court::width) / 2 - 34, 12, current.left_score, 3, left_colour);
    draw_number(fb, static_cast<int>(court::width) / 2 + 16, 12, current.right_score, 3, right_colour);

    // --- Paddles, interpolated ---
    // std::lerp is C++20's, in <cmath>. It is not simply a*(1-t)+b*t: the
    // standard requires it to return exactly `b` when t is 1 and to be monotonic,
    // which the obvious expression is not in floating point. Free correctness.
    const float left_y = std::lerp(previous.left_y, current.left_y, alpha);
    const float right_y = std::lerp(previous.right_y, current.right_y, alpha);

    fb.fill_rect(static_cast<int>(court::left_x), static_cast<int>(left_y),
                 static_cast<int>(court::paddle_w), static_cast<int>(court::paddle_h), k_left);
    fb.fill_rect(static_cast<int>(court::right_x), static_cast<int>(right_y),
                 static_cast<int>(court::paddle_w), static_cast<int>(court::paddle_h), k_right);

    // --- The ball, interpolated ---
    // While serving it blinks, driven by the countdown itself rather than by a
    // clock — so the animation is part of the simulation state and a replay shows
    // the identical blink.
    const bool blink_off = current.mode == phase::serving
                        && std::fmod(current.serve_timer, 0.24f) > 0.12f;

    if (!blink_off && current.mode != phase::over)
    {
        const engine::vec2 ball = engine::lerp(previous.ball, current.ball, alpha);
        fb.fill_rect(static_cast<int>(ball.x), static_cast<int>(ball.y),
                     static_cast<int>(court::ball_size), static_cast<int>(court::ball_size),
                     k_ball);
    }
}

} // namespace game
