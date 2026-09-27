#!/bin/sh
# scratch/build_verify_76.sh — compile and run Lesson 7.6's harness.
#
# Run from the repository root:
#
#   sh scratch/build_verify_76.sh
#
# THE FIRST MODULE 7 HARNESS THAT HAS TO LINK SOMETHING, and that turns out to
# matter far more than the extra words on the command line. 7.1 through 7.5 link
# nothing, because everything they test is `inline` in a header, so the `-O2`
# below covered every instruction they timed. Skinning is not header-only:
# `anim/skeleton.cpp` and `anim/skin.cpp` are real loops over arrays and live in
# `libengine.a`, and a flag on THIS command line does not reach them.
#
# *** THE REPOSITORY'S DEFAULT CONFIGURE HAS NO BUILD TYPE. *** `cmake -S . -B
# build` leaves `CMAKE_BUILD_TYPE` empty, which means no `-O` flag at all for the
# library — and ARCHITECTURE.md §6's measuring rule ("-O2, never a debug build")
# was written about harnesses and has never reached the thing they link. Measured
# here, on the same machine in the same minute:
#
#     libengine.a, no build type    153.90 ns/vertex     26.25 us per palette
#     libengine.a, Release           9.11 ns/vertex       1.06 us per palette
#                                   -------------        -------------
#                                        16.9x                24.8x
#
# A ranking taken from the first row is a ranking of a different program. So this
# script REFUSES to measure against an unoptimised library rather than quietly
# producing numbers, which is the whole of what it learned.
#
#   cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-rel --target engine
#   sh scratch/build_verify_76.sh
#
# Set ENGINE_BUILD to choose the tree; it defaults to build-rel when that exists.
# Set ENGINE_ALLOW_UNOPTIMISED=1 to measure the slow one on purpose, which is
# exactly what produced the table above.
set -e
cd "$(dirname "$0")/.."

: "${ENGINE_BUILD:=}"
if [ -z "$ENGINE_BUILD" ]; then
    if [ -f build-rel/engine/libengine.a ]; then ENGINE_BUILD=build-rel; else ENGINE_BUILD=build; fi
fi

build_type=$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$ENGINE_BUILD/CMakeCache.txt")
if [ -z "$build_type" ] && [ -z "$ENGINE_ALLOW_UNOPTIMISED" ]; then
    echo "verify_76: $ENGINE_BUILD has an EMPTY CMAKE_BUILD_TYPE, so libengine.a"
    echo "  carries no -O flag and section H would measure a debug build."
    echo "  cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release"
    echo "  cmake --build build-rel --target engine"
    echo "  (or set ENGINE_ALLOW_UNOPTIMISED=1 to measure it anyway)"
    exit 2
fi

echo "verify_76: $ENGINE_BUILD/engine/libengine.a (${build_type:-no type})"

mkdir -p build/demos

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O2} \
    -I engine/include -I build/_deps/sdl3-src/include \
    scratch/verify_76.cpp \
    "$ENGINE_BUILD/engine/libengine.a" \
    -L "$ENGINE_BUILD/_deps/sdl3-build" -lSDL3 \
    -Wl,-rpath,"$PWD/$ENGINE_BUILD/_deps/sdl3-build" \
    -o build/demos/verify_76

./build/demos/verify_76 "$@"
