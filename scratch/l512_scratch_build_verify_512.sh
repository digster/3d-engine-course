#!/bin/sh
# scratch/build_verify_512.sh — compile and run Lesson 5.12's harness.
#
# Run from the repository root, after `cmake --build build`.
#
#   sh scratch/build_verify_512.sh
#
# NO TIMING SECTIONS, so -O0 is fine and is the default: every check in this
# harness is about what a value IS, not how long it takes. Lesson 5.12 measures
# nothing it did not have to.
#
# It links `libengine.a` and NOT `libdemo_common.a`, which is the same statement
# the demo's CMake target makes: a harness that reaches for the shared demo scene
# is not testing the public API.
set -e
cd "$(dirname "$0")/.."

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O0} \
    -I engine/include -I scratch -I build/_deps/sdl3-src/include \
    scratch/verify_512.cpp \
    build/engine/libengine.a \
    -L build/_deps/sdl3-build -lSDL3 -Wl,-rpath,"$PWD/build/_deps/sdl3-build" \
    -o build/demos/verify_512

./build/demos/verify_512 "$@"
