#!/bin/sh
# scratch/build_verify_72.sh — compile and run Lesson 7.2's harness.
#
# Run from the repository root:
#
#   sh scratch/build_verify_72.sh
#
# IT LINKS NOTHING, for the same reason 7.1's harness links nothing: everything
# `math/axis_angle.hpp` and `math/rotation.hpp` add is `inline` in a header, so
# the harness needs the include path and no library at all. That is the clearest
# possible statement of what axis-angle costs a program that uses it — a header,
# and rather more trig than you would like.
#
# -O2 by default. Sections B and F are TIMING measurements, which is new for this
# course's harnesses and is the one place where the optimisation level changes
# the answer rather than the wait: at -O0 the "closed form is 3x faster" claim
# measures the compiler's inlining and not the arithmetic. Both timed loops
# consume their results into a printed sink so neither can be deleted.
set -e
cd "$(dirname "$0")/.."

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O2} \
    -I engine/include \
    scratch/verify_72.cpp \
    -o build/demos/verify_72

./build/demos/verify_72 "$@"
