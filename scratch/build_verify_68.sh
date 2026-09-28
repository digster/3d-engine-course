#!/bin/sh
# scratch/build_verify_68.sh — compile and run Lesson 6.8's harness.
#
# Run from the repository root, after `cmake --build build`.
#
#   sh scratch/build_verify_68.sh                              # as configured
#   ENGINE_CFLAGS="-O2 -DNDEBUG" sh scratch/build_verify_68.sh # release
#
# A GPU DEVICE AND NO WINDOW. §G creates a headless `gpu_device` (the same
# `create(nullptr, ...)` verify_48 uses) to build the depth-only pipeline and run
# the pass — because "a render pass with no colour attachment" is a claim about
# SDL_GPU that only SDL_GPU can settle. Everything else is arithmetic.
#
# IT MUST RUN FROM THE REPOSITORY ROOT: §G loads shadow.vert/shadow.frag out of
# the build's shader directory and §H resolves scratch/shot_52.ppm.
#
# It is still built into build/demos/, because demo_common's reference shot loads
# torus.obj through the asset search path and SDL_GetBasePath() reports the
# EXECUTABLE's directory (Lesson 3.5). Run from the repository root, like every
# other harness, so §H's scratch/shot_52.ppm resolves.
set -e
cd "$(dirname "$0")/.."

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O0} \
    -I engine/include -I scratch -I build/_deps/sdl3-src/include \
    scratch/verify_68.cpp \
    build/demos/libdemo_common.a build/engine/libengine.a \
    -L build/_deps/sdl3-build -lSDL3 -Wl,-rpath,"$PWD/build/_deps/sdl3-build" \
    -o build/demos/verify_68

./build/demos/verify_68 "$@"
