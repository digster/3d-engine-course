#!/bin/sh
# scratch/build_verify_77b.sh — compile and run Lesson 7.7b's harness.
#
# Run from the repository root, after `cmake --build build`:
#
#   sh scratch/build_verify_77b.sh                                   # every check
#   ENGINE_BUILD=build-rel ENGINE_CFLAGS="-O2 -DNDEBUG" \
#       sh scratch/build_verify_77b.sh                               # §L's timings
#
# §L TIMES LIBRARY CODE. `particle_pool::step` lives in libengine.a, and a flag on
# THIS command line never reaches it: the repository's default configure has no
# CMAKE_BUILD_TYPE, so build/engine/libengine.a is compiled with no -O at all
# (Lesson 7.6 measured that at 17-25x). So the library comes from ENGINE_BUILD,
# its build type is passed in, and §L says in its own output whether the numbers
# it prints are worth quoting. Every other section is exact and runs at any
# optimisation.
#
#   cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-rel --target engine
#
# The binary goes in build/demos/ because the shader directory is resolved
# RELATIVE TO THE EXECUTABLE, and build/demos/shaders is where the build copies
# the compiled kernels.
set -e
cd "$(dirname "$0")/.."

: "${ENGINE_BUILD:=build}"
build_type=$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$ENGINE_BUILD/CMakeCache.txt")
echo "verify_77b: $ENGINE_BUILD/engine/libengine.a (${build_type:-no build type})"

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O0} \
    -DENGINE_LIB_BUILD_TYPE="\"${build_type:-none}\"" \
    -I engine/include -I build/_deps/sdl3-src/include \
    scratch/verify_77b.cpp \
    "$ENGINE_BUILD/engine/libengine.a" \
    -L build/_deps/sdl3-build -lSDL3 -Wl,-rpath,"$PWD/build/_deps/sdl3-build" \
    -o build/demos/verify_77b

./build/demos/verify_77b "$@"
