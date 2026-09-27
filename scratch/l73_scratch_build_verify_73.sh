#!/bin/sh
# scratch/build_verify_73.sh — compile and run Lesson 7.3's harness.
#
# Run from the repository root:
#
#   sh scratch/build_verify_73.sh
#
# IT LINKS NOTHING, for the same reason 7.1's and 7.2's harnesses link nothing:
# everything `math/complex.hpp` adds is `inline` in a header, so the harness
# needs the include path and no library at all. For this lesson that is not just
# convenient, it is half the argument — the entire representation is two floats
# and a multiplication rule, and a build line with no `-l` on it says so more
# plainly than a paragraph could.
#
# -O2 by default. Sections D and G are TIMING measurements and the optimisation
# level changes their answers rather than the wait: at -O0 "the complex product
# is cheaper" measures the compiler's inlining and not the arithmetic. Both
# timed loops consume their results into a printed sink so neither can be
# deleted, and both index their inputs with the repetition counter so neither
# can be hoisted (7.2 lost two measurements to exactly those two mistakes).
#
# G's accuracy sweep walks 16.7 million products. That is the slow part of the
# run — roughly a second at -O2 and rather longer at -O0 — and it is deliberate:
# the recurrence's error is the whole question, and a short walk cannot answer it.
set -e
cd "$(dirname "$0")/.."

mkdir -p build/demos

# shellcheck disable=SC2086
c++ -std=c++20 -Wall -Wextra ${ENGINE_CFLAGS:--O2} \
    -I engine/include \
    scratch/verify_73.cpp \
    -o build/demos/verify_73

./build/demos/verify_73 "$@"
