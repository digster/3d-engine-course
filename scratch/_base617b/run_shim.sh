#!/bin/sh
# 6.8, 6.9 and 6.16's harnesses include <engine/gfx/bounds.hpp>, which Lesson 8.4
# moved to engine/math/. A forwarding shim on the include path lets them build at
# HEAD so they can serve as additivity controls for 6.17b. usage: run_shim.sh <outdir>
cd "$(dirname "$0")/../.."
out="$1"; mkdir -p "$out"
for n in 68 69 616; do
  c++ -std=c++20 -O0 -I scratch/_base617b/shim -I engine/include -I demos/common -I scratch \
      -I build/_deps/sdl3-src/include scratch/verify_$n.cpp \
      build/demos/libdemo_common.a build/engine/libengine.a \
      -L build/_deps/sdl3-build -lSDL3 -Wl,-rpath,"$PWD/build/_deps/sdl3-build" \
      -o build/demos/verify_$n > "$out/verify_$n.txt" 2>&1 \
    && ./build/demos/verify_$n >> "$out/verify_$n.txt" 2>&1
  echo "verify_$n exit=$?" >> "$out/_summary.txt"
done
