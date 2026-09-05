// engine/include/engine/core/actions.hpp — what the player MEANT, not which key they hit.
//
// Lesson 5.10. Every program in this course so far reads input like this:
//
//     case SDL_SCANCODE_C: toggle_camera_ride(); break;
//
// which is fine for a demo and wrong for a game, in four separate ways:
//
//   1. IT CANNOT BE REBOUND. The scancode is compiled in. A left-handed player, a
//      player on a keyboard where that key is awkward, a player with a disability
//      that makes it unreachable — all of them are out of luck, and the fix is a
//      recompile.
//   2. IT IS ABOUT ONE DEVICE. A gamepad has no scancodes. Adding one means a
//      second switch that must be kept in step with the first, forever, by hand.
//   3. IT CANNOT BE RECORDED. A replay system wants to store what the player
//      MEANT — "jump" — because the binding may have changed between the
//      recording and the playback. Storing `SDL_SCANCODE_SPACE` records a fact
//      about a keyboard rather than a fact about the game.
//   4. IT SAYS THE WRONG THING. `if (key_pressed(SDL_SCANCODE_SPACE))` inside a
//      jump handler is a sentence about hardware in a file about jumping. That is
//      not a performance problem or a portability problem; it is the code failing
//      to describe what it does, which is the kind of problem that compounds.
//
// One level of indirection fixes all four:
//
//     an ACTION is a NAME          "jump", "camera_yaw"
//     a BINDING maps a signal onto it   Space -> jump, +1
//     a FRAME publishes the values      jump = 1.0, held, pressed this frame
//
// THE ONE MECHANISM. Every binding contributes a SIGNED FLOAT to its action, and
// an action's value is the sum of its bindings' contributions. That single rule
// covers both things an input system has to do:
//
//     jump    <- Space (+1)                  value 0 or 1; ask held()/pressed()
//     steer   <- A (-1) and D (+1)           value -1, 0 or +1; ask value()
//     look_x  <- mouse delta x (x sensitivity) value unbounded; ask value()
//
// There is no "button action" and "axis action" mode to choose between, because
// the second is what the first already is once you allow a scale.
//
// WHAT THIS FILE DOES NOT DO, and it is the motivation in point 2 above, so it is
// said out loud rather than buried: THERE IS NO GAMEPAD SUPPORT HERE. Not because
// the design cannot take one — `binding::source` has room and `action_map::update`
// has one arm to add — but because it could not be RUN on the machine this was
// written on, and untested device code in an engine is a liability rather than a
// feature. Lesson 5.10 §7 says exactly what would need writing and verifying, and
// Exercise 10.2 does it. What IS demonstrated here is the property the gamepad
// claim rests on: one action, several bindings, on more than one device.

#pragma once

#include <engine/core/input.hpp>

#include <SDL3/SDL.h>

#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

// ---- Naming an action ------------------------------------------------------

/// A declared action, as a small integer.
///
/// A handle, not a string, for the reason every hot path in this engine is an
/// array index: `value(jump)` is `values_[jump.index]`, and comparing two ids is
/// comparing two `uint16_t`. The NAME still exists — `action_map::name_of()` —
/// because serialization, a rebinding UI and a debug HUD all need it, and none of
/// them is on a hot path.
///
/// Stable for the lifetime of the `action_map` that issued it. Declaring the same
/// name twice returns the same id, so a subsystem may declare the actions it uses
/// without knowing whether somebody else already did.
struct action_id
{
    static constexpr std::uint16_t k_invalid = 0xFFFFu;

    std::uint16_t index = k_invalid;

    [[nodiscard]] constexpr bool valid() const { return index != k_invalid; }
    [[nodiscard]] explicit constexpr operator bool() const { return valid(); }
    [[nodiscard]] friend constexpr bool operator==(action_id, action_id) = default;
};

/// Where a binding's value comes from.
///
/// The extension point. A gamepad button and a gamepad axis are two more
/// enumerators and two more arms in `update()`; see the note at the top of this
/// file for why they are not here.
enum class input_source : std::uint8_t
{
    key,            ///< `code` is an SDL_Scancode. Contributes `scale` while held.
    mouse_button,   ///< `code` is 1..5 (SDL_BUTTON_LEFT…). Contributes `scale` while held.
    mouse_axis,     ///< `code` is a `mouse_axis`. Contributes delta x `scale`.
};

/// Which continuous quantity a `mouse_axis` binding reads.
///
/// All four are DELTAS — how much the quantity moved during this frame — because
/// an absolute cursor position is not a useful thing to bind an action to. The
/// deltas for `x` and `y` are computed by `action_map` from consecutive frames'
/// positions; the wheel is already a per-frame delta in `input`.
enum class mouse_axis : std::uint8_t
{
    x,        ///< cursor movement right, in window pixels
    y,        ///< cursor movement DOWN, in window pixels (+Y is down — 1.5's convention)
    wheel_x,
    wheel_y,
};

/// One signal, mapped onto one action.
///
/// `scale` is what makes a single mechanism cover buttons and axes: bind a key
/// with +1 and another with −1 and the action is a two-way axis; bind a mouse
/// delta with 0.2 and the action is a sensitivity-scaled look control.
///
/// Trivially copyable and free of pointers, so a binding table is
/// `std::vector<binding>` and Module 8's serializer will write it as bytes.
struct binding
{
    action_id action;
    input_source source = input_source::key;
    std::int32_t code = 0;
    float scale = 1.0f;
};

/// The threshold at which an action counts as "held".
///
/// A digital binding contributes ±1, so anything above zero would do; 0.5 is
/// chosen so that an analog source (a gamepad trigger, when one arrives) has to
/// be pushed halfway rather than brushed. `held()` compares the ABSOLUTE value,
/// so a two-way axis is "held" when pushed either way — legal, occasionally
/// useful, and rarely what you want to ask of an axis.
inline constexpr float k_action_press_threshold = 0.5f;

/// How many unconsumed presses of one action are remembered. See
/// `action_map::consume_pressed`.
inline constexpr std::uint8_t k_action_press_queue = 4;

// ---- What `update` needs -----------------------------------------------------

/// Anything that can answer the six questions a binding table asks.
///
/// `engine::input` satisfies this without being told about it — no base class, no
/// registration, and not one character changed in Lesson 1.2's header. That is
/// the property a concept has and an interface does not: it describes a shape,
/// and types that already have that shape are already members.
///
/// The reason it exists at all is testability. `input::update()` samples SDL's
/// live keyboard state, so a test that wants a key held would have to convince
/// SDL that a key is held. With this, the test writes a struct with six functions
/// and the mapping logic is exercised exactly as it ships.
template <typename T>
concept input_snapshot = requires(const T& t, SDL_Scancode key, int button) {
    { t.key_down(key) } -> std::convertible_to<bool>;
    { t.mouse_down(button) } -> std::convertible_to<bool>;
    { t.mouse_x() } -> std::convertible_to<float>;
    { t.mouse_y() } -> std::convertible_to<float>;
    { t.wheel_x() } -> std::convertible_to<float>;
    { t.wheel_y() } -> std::convertible_to<float>;
};

static_assert(input_snapshot<input>,
              "engine::input must satisfy input_snapshot — if this fires, input.hpp changed "
              "one of the six accessors the action map reads");

// ---- The map ---------------------------------------------------------------

/// Declared actions, their bindings, and this frame's values.
///
/// **Not a singleton, and not owned by the engine.** It is a value: a program
/// holds one, a two-player game holds two, and a game that wants per-entity input
/// puts one in a component and lets `view<player_input>` find it. That is the
/// same argument `asset_store` made in Lesson 5.5 and `registry` in 5.8, and it
/// is the reason none of them is reachable through a global — the engine does not
/// know how many of a thing a game needs, so it declines to decide.
///
/// **The per-frame contract is `input`'s, one layer up**, and it is the same
/// contract for the same reason (Lesson 1.2):
///
///   1. drain the SDL event queue into `input::feed_event`
///   2. `input::update()`
///   3. `action_map::update(input)`   <- exactly once, here
///   4. query as much as you like; the answers are frozen until the next update
///
/// Step 3 must follow step 2, because an action's value is derived from the
/// input snapshot and a map updated first would publish last frame's answer.
class action_map
{
public:
    // ---- Declaring ---------------------------------------------------------

    /// Declare an action, or return the id it already has.
    ///
    /// Idempotent on purpose: two subsystems may both declare `"quit"` without
    /// coordinating, and a scene file that names an action the program already
    /// knows about must not create a second one. Returns an invalid id only if
    /// the name is empty or the map is full (65,534 actions).
    [[nodiscard]] action_id declare(std::string_view name);

    /// The id for `name`, or an invalid id. Does NOT declare — a lookup that
    /// created things would make a typo in a config file into a silent new action
    /// with no bindings and no behaviour.
    [[nodiscard]] action_id find(std::string_view name) const;

    /// The name `id` was declared with, or an empty view.
    [[nodiscard]] std::string_view name_of(action_id id) const;

    [[nodiscard]] std::size_t action_count() const { return names_.size(); }

    // ---- Binding -----------------------------------------------------------

    /// Bind a key. `scale` is the value it contributes while held: +1 for a
    /// button, ±1 for the two halves of an axis.
    bool bind_key(action_id id, SDL_Scancode key, float scale = 1.0f);

    /// Bind a mouse button, 1..5 (`SDL_BUTTON_LEFT` … `SDL_BUTTON_X2`).
    bool bind_mouse_button(action_id id, int button, float scale = 1.0f);

    /// Bind a mouse or wheel delta. `scale` is the sensitivity, and it is the
    /// caller's job to pick one that suits the units — a pixel of cursor
    /// movement and a notch of wheel are not comparable quantities.
    bool bind_mouse_axis(action_id id, mouse_axis axis, float scale);

    /// Remove every binding for `id`. Returns how many went.
    ///
    /// The first half of a rebind: clear, then bind. Two calls rather than one
    /// `rebind()`, because an action may legitimately keep several bindings and a
    /// single-call rebind would have to guess which one is being replaced.
    std::size_t clear_bindings(action_id id);

    /// Every binding, in the order they were added.
    ///
    /// This is what a settings screen enumerates and what Module 8's serializer
    /// writes. It is deliberately a flat list rather than a per-action map: the
    /// natural question a rebinding UI asks is "what is bound to this action",
    /// which is a filter over a short list, and the natural question a conflict
    /// checker asks is "what else is bound to this key", which a per-action map
    /// would answer badly.
    [[nodiscard]] std::span<const binding> bindings() const { return bindings_; }

    /// How many bindings `id` has.
    [[nodiscard]] std::size_t binding_count(action_id id) const;

    // ---- The frame ---------------------------------------------------------

    /// Publish one frame of actions. Call once per frame, AFTER `input::update()`.
    ///
    /// **Templated on the SHAPE of its argument rather than its type**, and the
    /// reason is worth more than the six lines it costs. This function needs
    /// something that can answer six questions — is this key down, is this mouse
    /// button down, where is the cursor, how far has the wheel turned. `input`
    /// is one such thing. A test's hand-built fake is another, and without this
    /// the fake would be impossible: `input::update()` samples SDL's live
    /// keyboard state, so a test that wanted a key held would have to make SDL
    /// believe a key was held.
    ///
    /// The `input_snapshot` concept says exactly that requirement, in code the
    /// compiler checks, so a type that nearly satisfies it fails at the
    /// definition with a readable message rather than deep inside a template.
    /// `engine::input` satisfies it without knowing this file exists — no base
    /// class, no registration, no change to Lesson 1.2's header.
    ///
    /// It is also free. There is no virtual call and no indirection: the compiler
    /// instantiates one copy per source type and inlines the accessors, so the
    /// production path compiles to exactly what a hand-written
    /// `update(const input&)` would have.
    template <input_snapshot Source>
    void update(const Source& in)
    {
        // Last frame's levels become the comparison. Every edge below is derived
        // from this pair, and deriving them from the BINDINGS instead would be
        // wrong in a way that is easy to miss — see the note after the loop.
        held_prev_ = held_;

        const float dx = have_mouse_ ? in.mouse_x() - mouse_x_ : 0.0f;
        const float dy = have_mouse_ ? in.mouse_y() - mouse_y_ : 0.0f;
        mouse_x_ = in.mouse_x();
        mouse_y_ = in.mouse_y();
        have_mouse_ = true;

        for (float& v : values_) { v = 0.0f; }

        for (const binding& b : bindings_)
        {
            if (!in_range(b.action)) { continue; }

            float contribution = 0.0f;
            switch (b.source)
            {
            case input_source::key:
                if (in.key_down(static_cast<SDL_Scancode>(b.code))) { contribution = b.scale; }
                break;

            case input_source::mouse_button:
                if (in.mouse_down(b.code)) { contribution = b.scale; }
                break;

            case input_source::mouse_axis:
                switch (static_cast<mouse_axis>(b.code))
                {
                case mouse_axis::x:       contribution = dx * b.scale; break;
                case mouse_axis::y:       contribution = dy * b.scale; break;
                case mouse_axis::wheel_x: contribution = in.wheel_x() * b.scale; break;
                case mouse_axis::wheel_y: contribution = in.wheel_y() * b.scale; break;
                }
                break;
            }

            values_[b.action.index] += contribution;
        }

        // THE EDGES COME FROM THE ACTION'S LEVEL, NOT FROM ANY BINDING'S EDGE,
        // and the difference shows up the moment an action has two bindings. Bind
        // "jump" to both Space and the left mouse button, then: hold Space (one
        // rising edge, correct); press the mouse while still holding Space (the
        // action was ALREADY active, so there must be NO second edge); release
        // Space (still active through the mouse, so NO falling edge); release the
        // mouse (one falling edge). Deriving from bindings would give two rising
        // edges and a double jump. Deriving from the level gives one, because the
        // level is what the question was about.
        for (std::size_t i = 0; i < values_.size(); ++i)
        {
            const bool now = std::fabs(values_[i]) >= k_action_press_threshold;
            const bool was = held_prev_[i] != 0u;
            held_[i] = now ? 1u : 0u;

            if (now && !was && queued_[i] < k_action_press_queue) { ++queued_[i]; }
        }
    }

    /// Forget every value, edge and queued press.
    ///
    /// For a scene change or a window losing focus: without it, a key held while
    /// the player alt-tabs away is still "held" when they come back, and the
    /// release edge that should have fired never did.
    void reset();

    // ---- Asking ------------------------------------------------------------

    /// The summed value of every binding. Meaningful for an axis; for a button,
    /// prefer `held()` — two keys bound to one button action both held sum to 2.
    [[nodiscard]] float value(action_id id) const;

    /// Is the action active right now? **A level** (Lesson 1.2).
    [[nodiscard]] bool held(action_id id) const;

    /// Did it become active this frame? **An edge, and it belongs to the FRAME.**
    ///
    /// Read this in `on_event` or `on_frame`, never in `on_fixed_step` — the step
    /// runs zero or more times per frame (Lesson 1.4), so an edge read there fires
    /// twice on a two-step frame and not at all on a zero-step one. That is the
    /// same rule `input::key_pressed` carries, inherited unchanged because the
    /// underlying hazard is unchanged.
    ///
    /// If you need an edge inside the step — and physics genuinely does — use
    /// `consume_pressed()`, which exists precisely because this function cannot
    /// safely be used there.
    [[nodiscard]] bool pressed(action_id id) const;

    /// Did it become inactive this frame? The falling edge, same frame rule.
    [[nodiscard]] bool released(action_id id) const;

    // ---- The edge that survives the step -----------------------------------

    /// Take one queued press, if there is one. **Safe inside `on_fixed_step`.**
    ///
    /// THE PROBLEM THIS SOLVES is Lesson 1.4's, and it is worth stating in full
    /// because it is the single most-often-got-wrong thing in a fixed-timestep
    /// engine. `on_fixed_step` runs a variable number of times per frame:
    ///
    ///     a frame with TWO steps   -> `pressed()` is true in both, and the player
    ///                                 jumps twice for one press
    ///     a frame with ZERO steps  -> `pressed()` was true during a frame in which
    ///                                 no step ran, and the jump is LOST
    ///
    /// Neither is fixable by the caller without keeping state, so the map keeps it.
    /// Every rising edge pushes one press onto a small queue; this function pops
    /// one and returns true. A two-step frame consumes it once and the second step
    /// gets nothing; a zero-step frame leaves it queued until a step happens.
    ///
    /// **The queue is `k_action_press_queue` deep and presses past that are
    /// dropped**, which is a decision rather than an oversight: an unbounded queue
    /// turns a hitch into a burst of queued jumps arriving after the player has
    /// stopped asking, which feels worse than losing them. Four is enough to
    /// survive a stutter and short enough not to feel like a recording. A fighting
    /// game with a deliberate input buffer would raise it and call the number a
    /// design parameter, which is exactly what it is.
    [[nodiscard]] bool consume_pressed(action_id id);

    /// How many presses are queued and unconsumed. Diagnostics, and the way a
    /// test can prove the queue is doing what it claims.
    [[nodiscard]] std::uint8_t pending_presses(action_id id) const;

    /// Drop every queued press without acting on them. Call it when a step is
    /// deliberately skipped — a pause menu, a loading screen — so that resuming
    /// does not replay what was queued while nothing was listening.
    void clear_pending();

private:
    [[nodiscard]] bool in_range(action_id id) const { return id.index < names_.size(); }

    void grow_to_fit();

    std::vector<std::string> names_;
    std::vector<binding> bindings_;

    // Parallel arrays, indexed by action_id::index. Three states rather than one,
    // for the same reason `input` keeps two snapshots: an edge is a comparison
    // between frames, and deriving it needs both sides.
    std::vector<float> values_;
    std::vector<std::uint8_t> held_;
    std::vector<std::uint8_t> held_prev_;
    std::vector<std::uint8_t> queued_;

    // The mouse deltas are this map's business rather than `input`'s, because
    // `input` publishes a cursor POSITION and a delta needs two of them. Keeping
    // the previous position here means `input` does not grow a field that only one
    // caller wants.
    float mouse_x_ = 0.0f;
    float mouse_y_ = 0.0f;
    bool have_mouse_ = false;
};

}   // namespace engine
