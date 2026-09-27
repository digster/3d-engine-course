// engine/include/engine/core/assert.hpp — statements about what must be true.
//
// Lesson 5.3. An assertion is not error handling and the two are constantly
// confused, so the distinction goes first, before any macro:
//
//   AN ERROR is a thing the world does to you. A file is missing; a GPU refuses
//   a format; a user typed nonsense. Errors are EXPECTED — not welcome, but
//   anticipated — and the correct response is to return a failure the caller can
//   act on. Errors survive into release builds because the world does not stop
//   being hostile when you ship.
//
//   AN ASSERTION is a claim about YOUR OWN code. This index is in range; this
//   pointer is not null; this function was called after start(). If one is false,
//   the program is not in a state its author anticipated — nothing sensible can
//   be done, because the code that would do it was written by the same person who
//   was already wrong.
//
// The test, and it settles almost every case: **could a correct program, running
// on a working machine, encounter this?** Yes → error. No → assertion.
//
// `load_image("missing.png")` is an error; the file might not be there.
// `framebuffer::row(-1)` is an assertion; no correct caller does that.
//
// ---------------------------------------------------------------------------
// SDL'S ASSERTIONS ARE UNUSUAL, AND USEFULLY SO
// ---------------------------------------------------------------------------
//
// C's `assert()` prints and calls `abort()`. SDL's asks a handler what to do and
// offers five answers (SDL3/SDL_assert.h, `SDL_AssertState`):
//
//     SDL_ASSERTION_RETRY          re-evaluate the condition, right now
//     SDL_ASSERTION_BREAK          trip the debugger's breakpoint
//     SDL_ASSERTION_ABORT          terminate
//     SDL_ASSERTION_IGNORE         carry on this once
//     SDL_ASSERTION_ALWAYS_IGNORE  carry on, and never ask about this one again
//
// RETRY is the surprising one, and it is real: the macro's expansion is a
// `while (!(condition))` loop, so returning RETRY genuinely re-tests. Fix the
// state in your debugger, continue, and the program proceeds as though the bug
// had not happened. That is worth a great deal on an assertion that fires on
// frame 40,000 of a session you cannot easily reproduce.
//
// ALWAYS_IGNORE is the one to be careful with. It is per-assertion, stored in a
// static inside the macro's own expansion, and it is why an SDL assertion can
// fire once and then go quiet for the rest of the run.
//
// ---------------------------------------------------------------------------
// WHICH SURVIVE A RELEASE BUILD
// ---------------------------------------------------------------------------
//
// SDL picks a level unless you set one (SDL3/SDL_assert.h):
//
//     SDL_ASSERT_LEVEL 2  when _DEBUG or DEBUG is defined, or under GCC/Clang
//                         without __OPTIMIZE__            → assert + assert_release
//     SDL_ASSERT_LEVEL 1  otherwise (i.e. an optimised build) → assert_release only
//
// So `-O2` alone demotes you to level 1, which is exactly the behaviour you want
// and exactly the behaviour that surprises people the first time an assertion
// they were relying on stops firing.

#pragma once

#include <engine/core/log.hpp>

#include <SDL3/SDL.h>

// ---------------------------------------------------------------------------
// The three macros, and the rule for choosing between them
// ---------------------------------------------------------------------------

/// A claim that must hold, checked only in a debug build.
///
/// The default. Use it for anything on a path that runs often enough for the
/// check to cost something: bounds, invariants, "this was initialised".
///
///     ENGINE_ASSERT(x >= 0 && x < width_);
///
/// **The condition must have no side effects.** In a release build it is not
/// evaluated — but it IS still compiled, because SDL's disabled form wraps it in
/// `sizeof`. That is a genuinely nice property: the condition cannot rot, and
/// `ENGINE_ASSERT(file = fopen(path, "r"))` — the classic mistake, an `=` where
/// `==` was meant — still compiles in release and still does not open the file.
/// Use ENGINE_VERIFY when you need the expression to run.
#define ENGINE_ASSERT(condition) SDL_assert(condition)

/// A claim that must hold, checked in **every** build, release included.
///
/// Reserve it for the small set of conditions where continuing would be worse
/// than stopping: a null device about to be dereferenced by the driver, an index
/// about to address memory that is not ours, a state machine in a state that has
/// no transitions out. The cost is a branch that ships; the benefit is a
/// diagnosable stop instead of an undiagnosable corruption.
///
/// If you find yourself reaching for this on a hot path, the honest answer is
/// usually that the condition is not an assertion at all — it is an error, and
/// the function should return one.
#define ENGINE_CHECK(condition) SDL_assert_release(condition)

/// Evaluate `expression` in every build, and assert that it was true in debug.
///
/// For the case the other two cannot express: an operation whose *result* is an
/// invariant but whose *execution* is required.
///
///     ENGINE_VERIFY(pool.release(handle));   // must run; must succeed
///
/// Without this you get the bug every codebase has had at least once — an
/// `assert(do_the_thing())` that stops doing the thing the moment somebody
/// builds with optimisation, producing a release-only failure in code that
/// "obviously" works.
/// **This macro must not delegate to SDL_assert, and the first version did.**
///
/// SDL decides its assertion level from `__OPTIMIZE__`, not from `NDEBUG`
/// (SDL3/SDL_assert.h). So a build with `-O2` and no `-DNDEBUG` — which is what
/// `RelWithDebInfo` is, and what a lot of people's "just make it fast" build is —
/// takes SDL to level 1, where `SDL_assert` expands to `(void)sizeof(condition)`
/// and **does not evaluate it**. An `ENGINE_VERIFY` guarded on `NDEBUG` therefore
/// stops doing the thing in exactly the configuration nobody tests, which is the
/// bug this macro exists to prevent, reintroduced by the fix for it.
///
/// Measured: the first version compiled to 4 bytes at `-O2` — a bare `ret` —
/// with the assignment gone. The version below is 12 bytes there, and the
/// assignment happens.
///
/// So: evaluate ALWAYS, into a named value, and assert on the value only where
/// debug assertions are live. Gate on `SDL_ASSERT_LEVEL`, because that is the
/// thing that actually decides.
#define ENGINE_VERIFY(expression)                                              \
    do {                                                                       \
        const bool engine_verify_result_ = static_cast<bool>(expression);      \
        if constexpr (SDL_ASSERT_LEVEL >= 2)                                   \
        {                                                                      \
            SDL_assert(engine_verify_result_);                                 \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            (void)engine_verify_result_;                                       \
        }                                                                      \
    } while (false)

/// An assertion that says something before it fires.
///
/// `ENGINE_ASSERT(i < count_)` tells you the condition. This tells you the
/// numbers, which is usually the question you were about to ask anyway:
///
///     ENGINE_ASSERT_MSG(i < count_, engine::log_core,
///                       "index %d is past the end (%d items)", i, count_);
///
/// The log line goes out at CRITICAL and is emitted BEFORE the assertion, so it
/// reaches a log file even if the handler chooses to abort. In a release build
/// the whole thing vanishes, message included.
#define ENGINE_ASSERT_MSG(condition, category, ...)                            \
    do {                                                                       \
        if constexpr (2 <= SDL_ASSERT_LEVEL)                                   \
        {                                                                      \
            if (!(condition))                                                  \
            {                                                                  \
                ENGINE_LOG_CRITICAL(category, __VA_ARGS__);                    \
            }                                                                  \
            SDL_assert(condition);                                             \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            (void)sizeof((condition));                                         \
        }                                                                      \
    } while (false)

namespace engine {

/// True when `ENGINE_ASSERT` is compiled in — i.e. `SDL_ASSERT_LEVEL >= 2`.
/// Exposed so a harness can say which build it is testing rather than guess.
[[nodiscard]] constexpr bool debug_assertions_enabled()
{
    return SDL_ASSERT_LEVEL >= 2;
}

/// True when `ENGINE_CHECK` is compiled in — `SDL_ASSERT_LEVEL >= 1`, which is
/// every build except one that explicitly asked for level 0.
[[nodiscard]] constexpr bool release_assertions_enabled()
{
    return SDL_ASSERT_LEVEL >= 1;
}

/// The level SDL settled on for this translation unit. For logging at start-up,
/// so a bug report says which assertions were live.
[[nodiscard]] constexpr int assert_level()
{
    return SDL_ASSERT_LEVEL;
}

} // namespace engine
