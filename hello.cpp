// hello.cpp — the engine's toolchain smoke test.
//
// This file has one job: prove the build works before we build anything real.
// It is NOT engine code — it lives at the repo root, not under src/, and 0.5
// retires it in favour of the real program. In Lesson 0.3 we compiled it by
// hand; in 0.4 CMake builds it AND links SDL3, so it now also proves that
// dependency acquisition, compilation, linking, and running all work end to end.

#include <cstdio>
#include <version>              // C++20: the standard header of feature-test macros.
#include <SDL3/SDL_version.h>   // Just the version API. The umbrella <SDL3/SDL.h>
                                // and SDL's special entry-point handling arrive in
                                // 0.5, when we actually initialise SDL.

// Which language standard is the compiler ACTUALLY using?
//
// The obvious answer, __cplusplus, lies on MSVC: unless you pass
// /Zc:__cplusplus it always reports 199711L (C++98), no matter your /std flag.
// MSVC reports the truth in _MSVC_LANG instead. We prefer that where it exists,
// so this check is honest on every compiler. See Lesson 0.3, pitfall #1.
#if defined(_MSVC_LANG)
#  define ENG_CPP_STANDARD _MSVC_LANG
#else
#  define ENG_CPP_STANDARD __cplusplus
#endif

// Fires at COMPILE time — precisely when you want to catch a bad standard flag.
static_assert(ENG_CPP_STANDARD >= 202002L,
              "This engine requires C++20. Check your -std / /std flag.");

// A genuine C++20 feature, so the standard is exercised rather than merely
// asserted. `consteval` did not exist before C++20 and forces compile-time
// evaluation: if this line compiles, the standard truly took effect.
consteval int engine_cpp_edition() { return 20; }

int main()
{
    std::printf("Engine toolchain: OK\n");

    // The compiler identity lives in predefined macros. Clang also defines
    // __GNUC__ for compatibility, so it must be checked first.
#if defined(__clang__)
    std::printf("  compiler   : Clang %d.%d.%d\n",
                __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    std::printf("  compiler   : GCC %d.%d.%d\n",
                __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    std::printf("  compiler   : MSVC _MSC_VER=%d\n", _MSC_VER);
#else
    std::printf("  compiler   : unrecognised\n");
#endif

    std::printf("  standard   : %ld\n", static_cast<long>(ENG_CPP_STANDARD));
    std::printf("  __cplusplus: %ld\n", static_cast<long>(__cplusplus));
    std::printf("  consteval  : C++%d ok\n", engine_cpp_edition());

    // SDL_GetVersion() is a pure query — it needs no SDL_Init — so calling it
    // here just proves the library linked and loads. The RUNTIME (linked) and
    // COMPILE-TIME (header) versions can differ when SDL is a shared library
    // updated independently of your build; printing both makes that visible.
    const int linked = SDL_GetVersion();
    std::printf("  SDL linked : %d.%d.%d\n",
                SDL_VERSIONNUM_MAJOR(linked),
                SDL_VERSIONNUM_MINOR(linked),
                SDL_VERSIONNUM_MICRO(linked));
    std::printf("  SDL headers: %d.%d.%d\n",
                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
    return 0;
}
