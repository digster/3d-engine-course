#!/bin/sh
# scratch/build_verify_65.sh — compile and run Lesson 6.3's harness.
#
# Run from the repository root, after `cmake --build build`.
#
#   sh scratch/build_verify_65.sh                              # as configured
#   ENGINE_CFLAGS="-O2 -DNDEBUG" sh scratch/build_verify_65.sh # release
#
# NO GPU AND NO WINDOW, like 6.2's — and this time nothing in the lesson is
# wired into a shader at all, so there is nothing a GPU could tell us. §G still
# writes the reference shot, which is the software renderer and needs nothing.
#
# §A and §E integrate at 400k and 1200x1600 samples respectively, so the debug
# build takes a few seconds. That is the right trade: the whole claim of §A is
# that the answer is 1 to five decimals, and a cheap grid cannot say that.
#
# It is still built into build/demos/, because demo_common's reference shot loads
# torus.obj through the asset search path and SDL_GetBasePath() reports the
# EXECUTABLE's directory (Lesson 3.5). Run from the repository root, like every
# other harness, so §G's scratch/shot_52.ppm resolves.
set -e
cd "$(dirname "$0")/.."

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O0} \
    -I engine/include -I scratch -I build/_deps/sdl3-src/include \
    scratch/verify_65.cpp \
    build/demos/libdemo_common.a build/engine/libengine.a \
    -L build/_deps/sdl3-build -lSDL3 -Wl,-rpath,"$PWD/build/_deps/sdl3-build" \
    -o build/demos/verify_65

./build/demos/verify_65 "$@"
