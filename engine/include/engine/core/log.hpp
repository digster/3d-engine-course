// engine/include/engine/core/log.hpp — saying things, at a volume somebody chose.
//
// Lesson 5.3. Until today this engine had 197 logging calls and every single one
// of them was `SDL_Log(...)`. Not one used a priority; not one used a category.
// That is not sloppiness that accumulated — it is what happens when the first
// lesson to print anything (0.5) reaches for the obvious function and forty
// lessons follow the precedent.
//
// The cost is that the engine cannot be turned down. `platform::start` alone
// emits four messages that are four different KINDS of statement:
//
//     "SDL_Init failed: %s"                       a fatal verdict
//     "vsync on refused: %s — continuing"         a warning; we recovered
//     "platform: SDL 3.4.12, surface=headless"    information
//     "SDL_SetAppMetadata failed — continuing"    a curiosity
//
// All four went to the same place at the same volume, so the only choices a user
// had were "all of it" and "none of it". A logging layer that cannot be turned
// down is a print statement with extra ceremony.
//
// ---------------------------------------------------------------------------
// WHY THIS FILE IS SO SHORT
// ---------------------------------------------------------------------------
//
// Because SDL3 already has a logging system, and it is a good one: priorities,
// categories, a per-category priority table, an env-var override, and a hook for
// redirecting output. Lesson 5.2 settled the policy — *a wrapper that hides a
// library you have no intention of replacing is cost with no benefit* — and the
// same reasoning applies here, so this header does not wrap SDL's logger. It
// adds the two things SDL cannot supply on our behalf:
//
//   1. NAMES FOR OUR OWN SUBSYSTEMS. SDL's categories are SDL's. Applications
//      get everything from SDL_LOG_CATEGORY_CUSTOM upward, and `log_category`
//      below is our slice of that.
//
//   2. A COMPILE-TIME FLOOR. SDL's filtering is a runtime comparison, so a
//      trace call in a hot loop still costs a call and a branch in a shipping
//      build. The macros here discard trace and debug at compile time.
//
// ---------------------------------------------------------------------------
// THE PAYOFF IS FREE, AND IT IS WORTH KNOWING WHY
// ---------------------------------------------------------------------------
//
// SDL's default priorities — read out of src/SDL_log.c, not guessed — are:
//
//     app=info, assert=warn, test=verbose, *=error
//
// Every category SDL does not know about, which is every category we invent,
// defaults to ERROR. So simply moving the engine's messages off
// SDL_LOG_CATEGORY_APPLICATION and onto our own categories makes the engine
// quiet by default, while `SDL_Log` in a demo — which IS the application —
// keeps printing. That behaviour costs no configuration and no filtering code.
// It is the thing a wrapper would have had to reimplement.

#pragma once

#include <SDL3/SDL.h>

namespace engine {

/// The engine's log categories.
///
/// `SDL_LOG_CATEGORY_CUSTOM` is where SDL stops and applications begin — the
/// header says so in a comment, and everything below it is reserved. Basing the
/// first value there and letting the rest follow means we never have to know
/// what number SDL_LOG_CATEGORY_CUSTOM currently is, and a future SDL that adds
/// a category cannot collide with us.
///
/// **These are subsystems, not severities.** A category answers "who is
/// speaking"; a priority answers "how much does it matter". Conflating them is
/// the classic mistake — a `log_error` *category* means you can never turn
/// errors off for one subsystem without turning them off everywhere.
///
/// Deliberately few. Five categories a person can hold in their head beats
/// twenty nobody can, and the cost of merging two is one grep.
enum log_category : int
{
    log_core = SDL_LOG_CATEGORY_CUSTOM,   ///< clock, input, fixed_step, profiler
    log_platform,                         ///< window, renderer, the app lifecycle
    log_gfx,                              ///< the CPU rasterizer, meshes, textures
    log_gpu,                              ///< SDL_GPU: devices, pipelines, uploads
    log_asset,                            ///< files: images, OBJ, and Module 5's asset system

    log_category_count                    ///< not a category; the count, for iteration
};

/// A category's short name, as it appears in a `--log` spec. Returns "?" for a
/// value that is not one of ours (including SDL's own categories, which we do
/// not presume to name).
[[nodiscard]] const char* name_of(log_category c);

/// A priority's short name: "trace", "debug", "info", "warn", "error",
/// "critical", or "quiet" for SDL_LOG_PRIORITY_INVALID.
[[nodiscard]] const char* name_of(SDL_LogPriority p);

/// Apply a filter spec like `"gfx=debug,platform=warn,*=error"`.
///
/// Grammar, deliberately the same shape as SDL's own `SDL_LOGGING` environment
/// variable so that two things a user might type do not need two mental models:
///
///     spec     := entry ("," entry)*
///     entry    := (category "=")? priority
///     category := "core" | "platform" | "gfx" | "gpu" | "asset" | "*"
///     priority := "trace" | "debug" | "info" | "warn" | "error"
///              |  "critical" | "quiet"
///
/// A bare priority with no category means `*`. `quiet` silences a category
/// entirely. `*` applies to our categories only — it will not silence SDL's own
/// diagnostics, which are not ours to hide.
///
/// Returns false and logs the offending token if the spec does not parse.
/// **A rejected spec changes nothing**: it is applied in two passes, so a typo
/// in the last entry cannot leave you with the first three applied and no idea
/// which took effect.
[[nodiscard]] bool set_log_levels(const char* spec);

/// Scan `argv` for `--log SPEC` and apply it. Returns false only if a spec was
/// present and failed to parse; a command line with no `--log` is a success.
///
/// Called for you by `platform::start` before anything else it does, so every
/// program built on the engine has the flag whether or not it thought about it.
[[nodiscard]] bool configure_logging(int argc, char* argv[]);

/// Also write every message to `path`, alongside wherever it was already going.
/// Pass `nullptr` to stop and close the file.
///
/// Implemented with `SDL_SetLogOutputFunction`, chaining to the default handler
/// rather than replacing it — so this ADDS a destination instead of moving one,
/// which is what "also" has to mean if a redirect is not to lose your console.
///
/// SDL holds a mutex across the output callback, so the file is written from one
/// thread at a time even when Module 8's job system is pushing messages from
/// several.
[[nodiscard]] bool set_log_file(const char* path);

/// The lowest priority the macros below will emit, as an `SDL_LogPriority`
/// value. Anything below it is discarded at compile time and costs nothing.
[[nodiscard]] SDL_LogPriority compiled_log_floor();

} // namespace engine

// ---------------------------------------------------------------------------
// The compile-time floor
// ---------------------------------------------------------------------------
//
// SDL_LogPriority's values are TRACE=1, VERBOSE=2, DEBUG=3, INFO=4, WARN=5,
// ERROR=6, CRITICAL=7 (SDL3/SDL_log.h). We compare against those numbers rather
// than the enum because the preprocessor cannot see an enum.
//
// Define ENGINE_LOG_MIN_PRIORITY on the compiler command line to override. The
// default keeps everything in a debug build and drops trace/verbose/debug in a
// release one — which is the only part of logging that has a measurable cost,
// because it is the only part that appears inside loops.
#ifndef ENGINE_LOG_MIN_PRIORITY
#  ifdef NDEBUG
#    define ENGINE_LOG_MIN_PRIORITY 4   /* INFO */
#  else
#    define ENGINE_LOG_MIN_PRIORITY 1   /* TRACE */
#  endif
#endif

// WHY `if constexpr` AND NOT `#if`.
//
// The obvious implementation is to `#define ENGINE_LOG_TRACE(...) ((void)0)` in
// a release build. Do that and the arguments are never compiled, so a trace call
// that names a variable somebody later renamed keeps compiling in release and
// breaks the day a colleague builds in debug. Logging that only compiles in one
// configuration rots, silently, and the rot is discovered by the person least
// able to fix it.
//
// `if constexpr` with a condition that is a literal comparison discards the
// statement at compile time — no call, no branch, nothing in the object file —
// while still requiring it to be valid, name-resolved C++. You get the deletion
// and keep the type-checking. Verified in §7: the disabled form emits the same
// number of instructions as an empty function.
//
// The do/while(0) is the standard idiom that makes a multi-statement macro
// behave like one statement, so `if (x) ENGINE_LOG_INFO(...); else ...` parses.

#define ENGINE_LOG_AT(priority_value, sdl_fn, category, ...)                   \
    do {                                                                       \
        if constexpr ((priority_value) >= ENGINE_LOG_MIN_PRIORITY)             \
        {                                                                      \
            sdl_fn(static_cast<int>(category), __VA_ARGS__);                   \
        }                                                                      \
    } while (false)

/// Per-frame or per-item detail. Off in release, and off at runtime by default.
#define ENGINE_LOG_TRACE(category, ...) ENGINE_LOG_AT(1, SDL_LogTrace, category, __VA_ARGS__)

/// What a subsystem did, once per operation. Off in release.
#define ENGINE_LOG_DEBUG(category, ...) ENGINE_LOG_AT(3, SDL_LogDebug, category, __VA_ARGS__)

/// A fact somebody might want on a bug report: what device, what format, what file.
#define ENGINE_LOG_INFO(category, ...)  ENGINE_LOG_AT(4, SDL_LogInfo, category, __VA_ARGS__)

/// Something is wrong and we carried on anyway. **If we recovered, it is a warning.**
#define ENGINE_LOG_WARN(category, ...)  ENGINE_LOG_AT(5, SDL_LogWarn, category, __VA_ARGS__)

/// The operation did not happen. **If the caller gets a failure back, it is an error.**
#define ENGINE_LOG_ERROR(category, ...) ENGINE_LOG_AT(6, SDL_LogError, category, __VA_ARGS__)

/// The program cannot continue. Rare, and it should stay rare.
#define ENGINE_LOG_CRITICAL(category, ...) ENGINE_LOG_AT(7, SDL_LogCritical, category, __VA_ARGS__)
