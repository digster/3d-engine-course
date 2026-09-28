#!/bin/sh
# scratch/build_verify_617b.sh — compile and run Lesson 6.17b's harness.
#
# Run from the repository root, after `cmake --build build`.
#
#   sh scratch/build_verify_617b.sh
#   ENGINE_CFLAGS="-O2 -DNDEBUG" sh scratch/build_verify_617b.sh   # for §J's timing
#
# §J's TIMING line is meaningless at -O0 — it measures the debug build of the
# harness and of the inline headers it instantiates. Quote it from -O2 only.
#
# The binary goes in build/demos/ because demo_common resolves `assets/` and
# the shader directory RELATIVE TO THE EXECUTABLE.
set -e
cd "$(dirname "$0")/.."

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O0} \
    -I engine/include -I demos/common -I scratch -I build/_deps/sdl3-src/include \
    scratch/verify_617b.cpp \
    build/demos/libdemo_common.a build/engine/libengine.a \
    -L build/_deps/sdl3-build -lSDL3 -Wl,-rpath,"$PWD/build/_deps/sdl3-build" \
    -o build/demos/verify_617b

./build/demos/verify_617b "$@"
