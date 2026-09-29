#!/bin/sh
# scratch/build_verify_617.sh — compile and run Lesson 6.17's harness.
#
# Run from the repository root, after `cmake --build build`.
#
#   sh scratch/build_verify_617.sh
#   ENGINE_CFLAGS="-O2 -DNDEBUG" sh scratch/build_verify_617.sh   # for §H
#
# §G AND §H ARE TIMING SECTIONS and are meaningless at -O0: the point of both is
# which of two loops is faster, and an unoptimised build measures the debug
# bookkeeping instead. Build with -O2 before quoting either.
#
# The binary goes in build/demos/ because demo_common resolves `assets/` RELATIVE
# TO THE EXECUTABLE — put it elsewhere and torus.obj silently fails to load.
set -e
cd "$(dirname "$0")/.."

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O0} \
    -I engine/include -I demos/common -I scratch -I build/_deps/sdl3-src/include \
    scratch/verify_617.cpp \
    build/demos/libdemo_common.a build/engine/libengine.a \
    -L build/_deps/sdl3-build -lSDL3 -Wl,-rpath,"$PWD/build/_deps/sdl3-build" \
    -o build/demos/verify_617

./build/demos/verify_617 "$@"
