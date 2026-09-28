#!/bin/sh
# scratch/build_verify_69.sh — compile and run Lesson 6.9's harness.
#
# Run from the repository root, after `cmake --build build`.
#
#   sh scratch/build_verify_69.sh                              # as configured
#   ENGINE_CFLAGS="-O2 -DNDEBUG" sh scratch/build_verify_69.sh # release
#
# NO GPU DEVICE THIS TIME. 6.8's harness needed one to prove that a render pass
# with no colour attachment is a thing SDL_GPU will do. Every claim here is
# arithmetic — splits, corners, a sphere, a snapped grid, a bias — so the whole
# file is pure engine code and links without a window or a device.
set -e
cd "$(dirname "$0")/.."

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O0} \
    -I engine/include -I scratch -I build/_deps/sdl3-src/include \
    scratch/verify_69.cpp \
    build/demos/libdemo_common.a build/engine/libengine.a \
    -L build/_deps/sdl3-build -lSDL3 -Wl,-rpath,"$PWD/build/_deps/sdl3-build" \
    -o build/demos/verify_69

./build/demos/verify_69 "$@"
