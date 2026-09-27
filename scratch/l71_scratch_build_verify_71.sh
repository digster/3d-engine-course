#!/bin/sh
# scratch/build_verify_71.sh — compile and run Lesson 7.1's harness.
#
# Run from the repository root:
#
#   sh scratch/build_verify_71.sh
#
# IT LINKS NOTHING. Every function this lesson adds is `inline` in a header, so
# the harness needs the include path and no library at all — not `libengine.a`,
# not SDL. That is worth one line of comment because it is the clearest possible
# statement of what `math/euler.hpp` costs a program that uses it: a header, and
# three calls to `sin` and `cos`.
#
# -O2 by default, unlike 5.12's harness, because section C brute-forces the
# Jacobian's gain over a million unit vectors per pose. At -O0 that is about
# forty seconds; at -O2 it is under two. Nothing here is a timing measurement, so
# the optimiser cannot change an answer — only how long you wait for it.
set -e
cd "$(dirname "$0")/.."

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O2} \
    -I engine/include \
    scratch/verify_71.cpp \
    -o build/demos/verify_71

./build/demos/verify_71 "$@"
