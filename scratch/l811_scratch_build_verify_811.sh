#!/bin/sh
# scratch/build_verify_811.sh — compile and run Lesson 8.11's harness.
#
# Run from the repository root:
#
#   sh scratch/build_verify_811.sh
#
# INHERITED FROM 7.7 THROUGH 8.10. Sections A, E and K time code in libengine.a,
# broadphase and the narrow phase, and all three are calls into libengine.a,
# where a harness's own -O2 cannot reach. Sections A, B and H are worse still:
# each runs tens of seconds of simulated time over hundreds of bodies, several
# times, so an unoptimised library turns a measurement into a wait.
#
# *** THE REPOSITORY'S DEFAULT CONFIGURE HAS NO BUILD TYPE. *** `cmake -S . -B
# build` leaves `CMAKE_BUILD_TYPE` empty, which means no `-O` flag at all for the
# library. 7.6 measured the cost of ignoring that at 18.1x on skinning; this
# script refuses rather than quietly producing numbers.
#
#   cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-rel --target engine
#   sh scratch/build_verify_811.sh
#
# Set ENGINE_BUILD to choose the tree; it defaults to build-rel when that exists.
# Set ENGINE_ALLOW_UNOPTIMISED=1 to measure the slow one on purpose.
set -e
cd "$(dirname "$0")/.."

: "${ENGINE_BUILD:=}"
if [ -z "$ENGINE_BUILD" ]; then
    if [ -f build-rel/engine/libengine.a ]; then ENGINE_BUILD=build-rel; else ENGINE_BUILD=build; fi
fi

build_type=$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$ENGINE_BUILD/CMakeCache.txt")
if [ -z "$build_type" ] && [ -z "$ENGINE_ALLOW_UNOPTIMISED" ]; then
    echo "verify_811: $ENGINE_BUILD has an EMPTY CMAKE_BUILD_TYPE, so libengine.a"
    echo "  carries no -O flag and section I would measure a debug build."
    echo "  cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release"
    echo "  cmake --build build-rel --target engine"
    echo "  (or set ENGINE_ALLOW_UNOPTIMISED=1 to measure it anyway)"
    exit 2
fi

echo "verify_811: $ENGINE_BUILD/engine/libengine.a (${build_type:-no type})"

mkdir -p build/demos

# shellcheck disable=SC2087
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O2} \
    -I engine/include -I build/_deps/sdl3-src/include \
    scratch/verify_811.cpp \
    "$ENGINE_BUILD/engine/libengine.a" \
    -L "$ENGINE_BUILD/_deps/sdl3-build" -lSDL3 \
    -Wl,-rpath,"$PWD/$ENGINE_BUILD/_deps/sdl3-build" \
    -o build/demos/verify_811

./build/demos/verify_811 "$@"
