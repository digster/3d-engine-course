// demos/pong/main.cpp — Lesson 1.8's game, as its own program at last.
//
// LESSON 5.2 GAVE THIS FILE ITS REASON TO EXIST, and the reason is not that Pong
// deserved a promotion. Until today the only way to play it was:
//
//     ./build/demos/sandbox --software      then press [Tab] four times
//
// because it was one branch of a five-way `switch` inside a 5,726-line main().
// It lived there not because anybody thought that was the right home, but
// because a demo needs a loop, and there was exactly one loop in the repository.
//
// `engine::app` is a loop you can have without writing one. So the branch came
// out, and what it turned into is this: **87 lines of code**, which own nothing
// but a game state, a flag, and five overrides. Zero SDL lifecycle calls — no
// SDL_Init, no window, no renderer, no texture, no drain, no teardown.
//
// Read it against what a Pong executable would have cost in Lesson 1.8 — SDL_Init,
// a window, a renderer, a streaming texture, an event drain, a clock, an
// accumulator, and forty lines of teardown — and the whole of that is now the
// word `engine::app` and one config struct.
//
//     cmake --build build --target pong
//     ./build/demos/pong
//
//     [W]/[S] left paddle   [Up]/[Down] right paddle (with [C])
//     [C] two-player        [K] collision rule       [Esc] quit

#include "pong.hpp"

#include <engine/gfx/colour.hpp>
#include <engine/gfx/framebuffer.hpp>
#include <engine/platform/app.hpp>
#include <engine/platform/main.hpp>

namespace {

class pong_app final : public engine::app
{
public:
    [[nodiscard]] engine::app_config configure(int argc, char* argv[]) override
    {
        (void)argc;
        (void)argv;

        // The court's dimensions ARE the framebuffer's, and saying so here rather
        // than repeating 320 and 180 means the two can never drift apart. Lesson
        // 1.8 chose those numbers; `game::court` is where they live.
        return {.title = "Pong — Lesson 1.8",
                .fb_width = static_cast<int>(game::court::width),
                .fb_height = static_cast<int>(game::court::height)};
    }

    [[nodiscard]] bool on_start() override
    {
        // Seeded from the clock, so two runs do not serve identically — and
        // carried INSIDE the state, so one run is perfectly replayable from its
        // seed. Both properties at once, which is the whole reason `state` owns
        // its own RNG instead of calling SDL_rand().
        const Uint32 seed = static_cast<Uint32>(SDL_GetTicksNS() & 0xFFFFFFFFu);
        current_ = game::make_state(seed);
        previous_ = current_;

        SDL_Log("Pong — [W]/[S] move  [C] two-player  [K] collision rule  [Esc] quit");
        return true;
    }

    /// Discrete, once-per-press decisions.
    ///
    /// These are in `on_event` rather than polled from `in()` because that is
    /// what the hook is FOR, and because `repeat` is available here: holding [K]
    /// down produces a stream of key-down events with `repeat = true`, and
    /// ignoring them is how you get one toggle per physical press without
    /// maintaining an edge table of your own.
    void on_event(const SDL_Event& event) override
    {
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) { return; }

        switch (event.key.scancode)
        {
        case SDL_SCANCODE_ESCAPE:
            request_quit();
            break;

        case SDL_SCANCODE_C:
            two_player_ = !two_player_;
            SDL_Log("[C] right paddle: %s", two_player_ ? "PLAYER" : "AI");
            break;

        case SDL_SCANCODE_K:
            // Lesson 1.8's kept-on-purpose bug. Both states carry the flag so
            // that the interpolation between them cannot straddle a rule change.
            current_.swept_collision = !current_.swept_collision;
            previous_.swept_collision = current_.swept_collision;
            SDL_Log("[K] collision: %s", current_.swept_collision ? "swept" : "naive");
            break;

        default:
            break;
        }
    }

    /// Exactly one simulation step of `h` seconds — never a frame's real
    /// duration, which is Lesson 1.4's whole argument and is enforced by the
    /// signature: there is nowhere to pass a frame time in.
    void on_fixed_step(float h) override
    {
        // Reading the keyboard inside the step loop is safe because `input`
        // publishes once per frame (Lesson 1.2): every step in one frame sees
        // the same snapshot, so two steps cannot disagree about which way a
        // paddle was moving.
        game::intent wanted;
        wanted.right_is_ai = !two_player_;
        if (in().key_down(SDL_SCANCODE_W)) { wanted.left -= 1.0f; }
        if (in().key_down(SDL_SCANCODE_S)) { wanted.left += 1.0f; }
        if (in().key_down(SDL_SCANCODE_UP))   { wanted.right -= 1.0f; }
        if (in().key_down(SDL_SCANCODE_DOWN)) { wanted.right += 1.0f; }

        previous_ = current_;
        game::step(current_, wanted, h);

        // Lesson 1.4's rule: never interpolate across a teleport. After a point
        // is scored the ball jumps to the centre, and a renderer asked to draw
        // 40% of the way between "past the paddle" and "at the centre" draws a
        // smear across the whole court.
        if (current_.teleported) { previous_ = current_; }
    }

    void on_frame(float alpha) override
    {
        fb().clear(k_background);
        game::draw(fb(), previous_, current_, alpha);
    }

    /// The HUD, drawn over the framebuffer and under the vsync wait. 2x scale
    /// because the framebuffer is blown up 4x to fill the window, and text at 1x
    /// would be unreadable while text at 4x would be enormous.
    void on_overlay() override
    {
        SDL_Renderer* const r = renderer();
        if (r == nullptr) { return; }

        SDL_SetRenderScale(r, 2.0f, 2.0f);
        SDL_SetRenderDrawColor(r, 210, 212, 220, 255);
        SDL_RenderDebugTextFormat(r, 6.0f, 6.0f,
                                  "PONG   %d - %d   collision: %-6s   rally %d   fps %5.1f",
                                  current_.left_score, current_.right_score,
                                  current_.swept_collision ? "swept" : "naive",
                                  current_.rally_hits,
                                  static_cast<double>(time().fps()));
        SDL_RenderDebugText(r, 6.0f, 328.0f,
                            "[W]/[S] move   [C] two-player   [K] collision   [Esc] quit");
        SDL_SetRenderScale(r, 1.0f, 1.0f);
    }

private:
    static constexpr Uint32 k_background = 0xFF0C0E14u;   // pack_argb(12, 14, 20)

    // The two most recent simulation states, which is the minimum a renderer
    // needs to draw a moment that never existed (Lesson 1.4).
    game::state current_{};
    game::state previous_{};
    bool two_player_ = false;
};

}   // namespace

ENGINE_MAIN(pong_app)
