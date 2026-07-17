// hello.cpp — the engine's toolchain smoke test.
//
// This file has exactly one job: prove that a correctly configured C++20
// compiler is installed before we try to build anything real. It is NOT engine
// code — it lives at the repo root, not under src/, and you can delete it once
// you have watched it run. We compile it BY HAND in this lesson; in Lesson 0.4
// we build this same file with CMake, so you can see what CMake automates.

#include <cstdio>
#include <version>   // C++20: the standard header of feature-test macros.

// Which language standard is the compiler ACTUALLY using?
//
// The obvious answer, __cplusplus, lies on MSVC: unless you pass
// /Zc:__cplusplus it always reports 199711L (C++98), no matter your /std flag.
// MSVC reports the truth in _MSVC_LANG instead. We prefer that where it exists,
// so this check is honest on every compiler. Pitfall #1 dissects this in full.
#if defined(_MSVC_LANG)
#  define ENG_CPP_STANDARD _MSVC_LANG
#else
#  define ENG_CPP_STANDARD __cplusplus
#endif

// This fires at COMPILE time — precisely when you want to discover a bad
// standard flag, rather than at runtime or, worse, three lessons from now when
// a C++20 feature mysteriously fails to compile.
static_assert(ENG_CPP_STANDARD >= 202002L,
              "This engine requires C++20. Check your -std / /std flag.");

// A genuine C++20 feature, used so the standard is exercised rather than merely
// asserted. `consteval` did not exist before C++20 and forces compile-time
// evaluation: if this line compiles, the standard truly took effect.
consteval int engine_cpp_edition() { return 20; }

int main()
{
    std::printf("Engine toolchain: OK\n");

    // The compiler identity lives in predefined macros. In practice exactly one
    // of these is defined, so the order only matters because Clang also defines
    // __GNUC__ for compatibility — which is why Clang must be checked first.
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

    // Print both numbers so the MSVC gotcha is visible, not just guarded against.
    std::printf("  standard   : %ld\n", static_cast<long>(ENG_CPP_STANDARD));
    std::printf("  __cplusplus: %ld\n", static_cast<long>(__cplusplus));
    std::printf("  consteval  : C++%d ok\n", engine_cpp_edition());
    return 0;
}
