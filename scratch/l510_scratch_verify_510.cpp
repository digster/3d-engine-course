// scratch/verify_510.cpp — Lesson 5.10's harness: actions, bindings, and the two edges.
//
//   §A  declaring and naming
//   §B  the one mechanism: every binding contributes a signed value
//   §C  EDGES COME FROM THE ACTION'S LEVEL, NOT FROM ANY BINDING'S
//   §D  the fixed-step trap: an edge that survives zero steps and two steps
//   §E  rebinding at runtime, and reset
//   §F  continuous sources: deltas, and the frame that has none
//   §G  the golden is still byte-identical
//
// §C and §D are the sections that would catch a plausible wrong implementation.
// Everything else checks behaviour that is hard to get wrong; those two check
// decisions, and both decisions are invisible in a single-binding, one-step-per-
// frame world — which is every world a test writes by accident.
//
// THE FAKE IS THE POINT OF THE CONCEPT. `input::update()` samples SDL's live
// keyboard state, so a harness that wanted a key held would have to persuade SDL
// that a key is held. `action_map::update` is templated on `input_snapshot`
// instead, so six functions in a struct are a complete input device and the
// mapping logic is exercised exactly as it ships.
//
// Build and run:  sh scratch/build_verify_510.sh

#include <engine/core/actions.hpp>
#include <engine/core/assert.hpp>

#include "../demos/common/demo_scene.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>

namespace {

using engine::action_id;
using engine::action_map;
using engine::mouse_axis;

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok) { ++g_failures; }
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
}

void checkf(bool ok, const char* fmt, ...)
{
    char line[512];
    va_list args;
    va_start(args, fmt);
    SDL_vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    check(ok, line);
}

[[nodiscard]] std::string read_file(const char* path)
{
    std::size_t size = 0;
    void* data = SDL_LoadFile(path, &size);
    if (data == nullptr) { return {}; }
    std::string out(static_cast<const char*>(data), size);
    SDL_free(data);
    return out;
}

/// A complete input device in six functions.
struct fake_input
{
    std::set<int> keys;
    std::set<int> buttons;
    float mx = 0.0f;
    float my = 0.0f;
    float wx = 0.0f;
    float wy = 0.0f;

    [[nodiscard]] bool key_down(SDL_Scancode k) const { return keys.count(static_cast<int>(k)) != 0; }
    [[nodiscard]] bool mouse_down(int b) const { return buttons.count(b) != 0; }
    [[nodiscard]] float mouse_x() const { return mx; }
    [[nodiscard]] float mouse_y() const { return my; }
    [[nodiscard]] float wheel_x() const { return wx; }
    [[nodiscard]] float wheel_y() const { return wy; }

    void hold(SDL_Scancode k) { keys.insert(static_cast<int>(k)); }
    void release(SDL_Scancode k) { keys.erase(static_cast<int>(k)); }
};

static_assert(engine::input_snapshot<fake_input>,
              "the harness's fake must satisfy the same concept engine::input does");
static_assert(engine::input_snapshot<engine::input>,
              "…and so must the real thing, without knowing the concept exists");

// ===========================================================================
//  §A — DECLARING AND NAMING
// ===========================================================================

void section_a_declaring()
{
    std::printf("\n=== A. Declaring and naming ===\n");

    action_map m;

    const action_id jump = m.declare("jump");
    check(jump.valid(), "declare returns a valid id");
    check(m.declare("jump") == jump,
          "declaring the same name again returns the SAME id — two subsystems may both "
          "declare an action without coordinating");
    checkf(m.action_count() == 1, "…and did not create a second action (%zu)", m.action_count());

    const action_id fire = m.declare("fire");
    check(fire.valid() && !(fire == jump), "a different name is a different id");

    check(m.find("jump") == jump, "find returns the id");
    check(!m.find("nonexistent").valid(),
          "find of an unknown name is INVALID and does not declare — a typo in a config file "
          "must not become a silent new action with no bindings");
    checkf(m.action_count() == 2, "…and the count is still %zu", m.action_count());

    check(m.name_of(jump) == "jump", "name_of round-trips");
    check(m.name_of(action_id{}).empty(), "name_of an invalid id is empty, not a crash");

    check(!m.declare("").valid(), "an empty name is refused");

    // Every query on an invalid id must be safe: an id that failed to declare
    // will be passed to these functions by code that did not check.
    const action_id bad{};
    check(m.value(bad) == 0.0f && !m.held(bad) && !m.pressed(bad) && !m.released(bad),
          "every query on an invalid id answers harmlessly rather than reading out of bounds");
    check(!m.consume_pressed(bad) && m.pending_presses(bad) == 0,
          "…including the queue");
}

// ===========================================================================
//  §B — THE ONE MECHANISM
// ===========================================================================

void section_b_values()
{
    std::printf("\n=== B. Every binding contributes a signed value ===\n");

    action_map m;
    fake_input in;

    const action_id jump = m.declare("jump");
    const action_id steer = m.declare("steer");

    check(m.bind_key(jump, SDL_SCANCODE_SPACE), "bind a key");
    check(m.bind_key(steer, SDL_SCANCODE_A, -1.0f) && m.bind_key(steer, SDL_SCANCODE_D, +1.0f),
          "bind two keys with opposite scales — an axis");
    checkf(m.binding_count(steer) == 2, "steer has %zu bindings", m.binding_count(steer));

    check(!m.bind_key(jump, static_cast<SDL_Scancode>(-1)), "an out-of-range scancode is refused");
    check(!m.bind_mouse_button(jump, 0) && !m.bind_mouse_button(jump, 9),
          "a mouse button outside 1..5 is refused");
    check(!m.bind_key(action_id{}, SDL_SCANCODE_Q), "binding an invalid action is refused");

    m.update(in);
    check(m.value(jump) == 0.0f && !m.held(jump), "nothing held, nothing active");

    in.hold(SDL_SCANCODE_SPACE);
    m.update(in);
    check(m.value(jump) == 1.0f && m.held(jump), "a held key contributes its scale");

    // THE AXIS, and the property that makes one mechanism enough for two jobs.
    in.release(SDL_SCANCODE_SPACE);
    in.hold(SDL_SCANCODE_A);
    m.update(in);
    check(m.value(steer) == -1.0f, "A alone gives -1");

    in.hold(SDL_SCANCODE_D);
    m.update(in);
    check(m.value(steer) == 0.0f,
          "A and D together give EXACTLY ZERO — they cancel, and no code anywhere says so. "
          "A separate 'axis' concept would have needed a rule for this case");
    check(!m.held(steer), "…and the action is not held, because held() tests |value|");

    in.release(SDL_SCANCODE_A);
    m.update(in);
    check(m.value(steer) == 1.0f, "D alone gives +1");

    // A scale that is not ±1.
    const action_id boost = m.declare("boost");
    m.bind_key(boost, SDL_SCANCODE_LSHIFT, 0.25f);
    in.hold(SDL_SCANCODE_LSHIFT);
    m.update(in);
    checkf(std::fabs(m.value(boost) - 0.25f) < 1e-6f, "a scale of 0.25 gives %.2f",
           static_cast<double>(m.value(boost)));
    check(!m.held(boost),
          "…and it is NOT held, because 0.25 is below the 0.5 threshold — which is what the "
          "threshold is for: an analog source must be pushed, not brushed");
}

// ===========================================================================
//  §C — EDGES COME FROM THE LEVEL
// ===========================================================================
//
// The section that would catch the most plausible wrong implementation, and it
// is invisible unless an action has more than one binding.

void section_c_edges()
{
    std::printf("\n=== C. Edges come from the action's level ===\n");

    action_map m;
    fake_input in;

    const action_id jump = m.declare("jump");
    m.bind_key(jump, SDL_SCANCODE_SPACE);
    m.bind_mouse_button(jump, SDL_BUTTON_LEFT);

    int rising = 0;
    int falling = 0;
    auto tick = [&] {
        m.update(in);
        if (m.pressed(jump)) { ++rising; }
        if (m.released(jump)) { ++falling; }
    };

    tick();
    check(rising == 0 && falling == 0, "idle: no edges");

    in.hold(SDL_SCANCODE_SPACE);
    tick();
    checkf(rising == 1 && m.held(jump), "pressing the key gives ONE rising edge (%d)", rising);

    in.buttons.insert(SDL_BUTTON_LEFT);
    tick();
    checkf(rising == 1,
           "pressing the MOUSE while the key is still held gives NO second edge (%d) — the "
           "action was already active, and an edge is a change in the ACTION, not in a binding",
           rising);

    in.release(SDL_SCANCODE_SPACE);
    tick();
    checkf(falling == 0 && m.held(jump),
           "releasing the key while the mouse is still down gives NO falling edge (%d), and the "
           "action is still active", falling);

    in.buttons.erase(SDL_BUTTON_LEFT);
    tick();
    checkf(falling == 1 && !m.held(jump), "releasing the last one gives ONE falling edge (%d)",
           falling);

    check(rising == 1 && falling == 1,
          "ONE rising and ONE falling edge for the whole sequence. Deriving edges from bindings "
          "would have given two of each — and a double jump");

    // An edge lasts exactly one frame.
    in.hold(SDL_SCANCODE_SPACE);
    m.update(in);
    check(m.pressed(jump), "the edge is true on the frame it happened");
    m.update(in);
    check(!m.pressed(jump) && m.held(jump),
          "…and false on the next frame, while the level stays true. That is Lesson 1.2's "
          "distinction, unchanged one layer up");
}

// ===========================================================================
//  §D — THE FIXED-STEP TRAP
// ===========================================================================

void section_d_step()
{
    std::printf("\n=== D. The edge that survives the step ===\n");

    action_map m;
    fake_input in;
    const action_id jump = m.declare("jump");
    m.bind_key(jump, SDL_SCANCODE_SPACE);

    // ---- a TWO-step frame must not double-fire ---------------------------
    m.update(in);
    in.hold(SDL_SCANCODE_SPACE);
    m.update(in);                       // one frame, one press

    checkf(m.pending_presses(jump) == 1, "one press queued (%u)", m.pending_presses(jump));
    int jumps = 0;
    for (int step = 0; step < 2; ++step)
    {
        if (m.consume_pressed(jump)) { ++jumps; }
    }
    checkf(jumps == 1,
           "a TWO-step frame jumps %d time — `pressed()` read here would have given 2, which is "
           "the single most-often-got-wrong thing in a fixed-timestep engine", jumps);

    // ---- a ZERO-step frame must not lose it ------------------------------
    m.reset();
    in.keys.clear();
    m.update(in);
    in.hold(SDL_SCANCODE_SPACE);
    m.update(in);                       // pressed…
    in.release(SDL_SCANCODE_SPACE);
    m.update(in);                       // …and released, with NO step in between

    check(!m.pressed(jump) && !m.held(jump),
          "by the time a step runs, the frame-scoped edge is long gone and the key is up");
    checkf(m.consume_pressed(jump) == true,
           "…but the QUEUED press survives, so a frame that ran no steps does not swallow the "
           "input");
    check(!m.consume_pressed(jump), "…and it is consumed exactly once");

    // ---- several presses queue, up to the cap ----------------------------
    m.reset();
    in.keys.clear();
    for (int i = 0; i < 6; ++i)
    {
        m.update(in);
        in.hold(SDL_SCANCODE_SPACE);
        m.update(in);
        in.release(SDL_SCANCODE_SPACE);
    }
    checkf(m.pending_presses(jump) == engine::k_action_press_queue,
           "six presses with no step queued %u, capped at %u — an UNBOUNDED queue would replay "
           "a burst of jumps after the player stopped asking, which feels worse than losing them",
           m.pending_presses(jump), engine::k_action_press_queue);

    int drained = 0;
    while (m.consume_pressed(jump)) { ++drained; }
    checkf(drained == engine::k_action_press_queue, "…and drains to exactly %d", drained);

    // ---- clear_pending, for a deliberately skipped step -------------------
    m.reset();
    in.keys.clear();
    m.update(in);
    in.hold(SDL_SCANCODE_SPACE);
    m.update(in);
    check(m.pending_presses(jump) == 1, "one queued");
    m.clear_pending();
    check(m.pending_presses(jump) == 0 && !m.consume_pressed(jump),
          "clear_pending drops it — what a pause menu calls, so that resuming does not replay "
          "what was queued while nothing was listening");
    check(m.held(jump), "…and the LEVEL is untouched, because the key really is still down");
}

// ===========================================================================
//  §E — REBINDING, AND RESET
// ===========================================================================

void section_e_rebinding()
{
    std::printf("\n=== E. Rebinding at runtime ===\n");

    action_map m;
    fake_input in;
    const action_id jump = m.declare("jump");
    m.bind_key(jump, SDL_SCANCODE_SPACE);
    m.bind_mouse_button(jump, SDL_BUTTON_LEFT);

    in.hold(SDL_SCANCODE_SPACE);
    m.update(in);
    check(m.held(jump), "Space works before the rebind");

    checkf(m.clear_bindings(jump) == 2, "clear_bindings removed both");
    check(m.binding_count(jump) == 0 && m.bindings().empty(), "…and the table is empty");

    m.update(in);
    check(!m.held(jump),
          "with no bindings the action is dead even though the key is still physically down — "
          "which is the whole point: the action is not the key");

    m.bind_key(jump, SDL_SCANCODE_J);
    m.update(in);
    check(!m.held(jump), "the OLD key no longer works");

    in.release(SDL_SCANCODE_SPACE);
    in.hold(SDL_SCANCODE_J);
    m.update(in);
    check(m.held(jump), "…and the new one does. A recompile was not required");

    // The binding table is what a settings screen enumerates and a serializer writes.
    checkf(m.bindings().size() == 1 && m.bindings()[0].action == jump
               && m.bindings()[0].code == static_cast<std::int32_t>(SDL_SCANCODE_J),
           "the binding table is inspectable: %zu entry naming the action and the code",
           m.bindings().size());

    // reset(), for a focus loss or a scene change.
    m.update(in);
    check(m.held(jump), "held before reset");
    m.reset();
    check(!m.held(jump) && !m.pressed(jump) && m.pending_presses(jump) == 0,
          "reset clears levels, edges and the queue — what a window losing focus needs, so a "
          "key held while the player alt-tabs away is not still held when they return");
    m.update(in);
    check(m.pressed(jump),
          "…and the next update sees a fresh rising edge, because reset made the level false");
}

// ===========================================================================
//  §F — CONTINUOUS SOURCES
// ===========================================================================

void section_f_axes()
{
    std::printf("\n=== F. Continuous sources ===\n");

    action_map m;
    fake_input in;

    const action_id look = m.declare("look_x");
    const action_id zoom = m.declare("zoom");
    m.bind_mouse_axis(look, mouse_axis::x, 0.5f);
    m.bind_mouse_axis(zoom, mouse_axis::wheel_y, -4.0f);

    in.mx = 100.0f;
    m.update(in);
    checkf(m.value(look) == 0.0f,
           "THE FIRST FRAME HAS NO DELTA (%.1f) — there is no previous position to subtract, and "
           "reporting 100 would have flung the camera on the first frame the program ran",
           static_cast<double>(m.value(look)));

    in.mx = 110.0f;
    m.update(in);
    checkf(std::fabs(m.value(look) - 5.0f) < 1e-6f,
           "a 10-pixel move at 0.5 sensitivity gives %.1f",
           static_cast<double>(m.value(look)));

    m.update(in);
    check(m.value(look) == 0.0f, "…and a frame with no movement gives zero, not the last delta");

    in.wy = 2.0f;
    m.update(in);
    checkf(std::fabs(m.value(zoom) + 8.0f) < 1e-6f,
           "the wheel is already a per-frame delta: 2 notches at -4 gives %.1f",
           static_cast<double>(m.value(zoom)));

    // reset() forgets the mouse origin, so the frame after it has no delta either.
    in.wy = 0.0f;
    m.reset();
    in.mx = 500.0f;
    m.update(in);
    check(m.value(look) == 0.0f,
          "after reset the next frame has no delta either — otherwise a scene change would "
          "deliver the whole distance the cursor moved while the level was loading");
}

// ===========================================================================
//  §G — THE GOLDEN
// ===========================================================================

void section_g_golden()
{
    std::printf("\n=== G. The golden ===\n");

    const char* path = "build/demos/verify_510.ppm";
    check(demo::write_reference_shot(path) == 0, "write_reference_shot() succeeded");

    const std::string ours = read_file(path);
    const std::string golden = read_file("scratch/shot_52.ppm");
    checkf(!ours.empty() && ours.size() == golden.size(), "same size: %zu bytes", ours.size());
    check(ours == golden,
          "BYTE-IDENTICAL to the golden written in Lesson 5.1 — TEN lessons. 5.10 added one "
          "header, one source file and one hook, and touched no line the reference scene runs");
}

}   // namespace

int main()
{
    std::printf("verify_510 — Lesson 5.10: input mapping\n");
    std::printf("(debug assertions %s)\n",
                engine::debug_assertions_enabled() ? "ON" : "OFF");

    section_a_declaring();
    section_b_values();
    section_c_edges();
    section_d_step();
    section_e_rebinding();
    section_f_axes();
    section_g_golden();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
