#!/bin/sh
# scratch/build_verify_75.sh — compile and run Lesson 7.5's harness.
#
# Run from the repository root:
#
#   sh scratch/build_verify_75.sh
#
# IT LINKS NOTHING, for the same reason 7.1's, 7.2's and 7.3's harnesses link
# nothing: everything `math/quat.hpp` adds is `inline` in a header, so the
# harness needs the include path and no library at all. For this lesson that is
# half the argument again — the entire representation is four floats and a
# multiplication rule, and a build line with no `-l` on it says so more plainly
# than a paragraph could.
#
# -O2 by default. Section F is TIMING and the optimisation level changes its
# answers rather than the wait: at -O0 "the quaternion product is cheaper"
# measures the compiler's inlining and not the arithmetic. Every timed loop
# consumes its result into a printed sink so none can be deleted, and every one
# indexes its inputs with the repetition counter so none can be hoisted.
#
# F.4 walks a million compositions twice. That is the slow part of the run and
# it is deliberate: drift is the whole question, and a short walk cannot answer
# it.
set -e
cd "$(dirname "$0")/.."

mkdir -p build/demos

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O2} \
    -I engine/include \
    scratch/verify_75.cpp \
    -o build/demos/verify_75

./build/demos/verify_75 "$@"
