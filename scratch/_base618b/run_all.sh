#!/bin/sh
# Baseline (or after) run of every Module 6-8 harness, for 6.18b's additivity check.
# usage: sh scratch/_base617b/run_all.sh <outdir>
cd "$(dirname "$0")/../.."
out="$1"; mkdir -p "$out"
for n in 61 62 63 64 65 66 67 68 69 610 611 612 613 614 615 616 617 617b 618 71 72 73 74 75 76 77 78; do
  start=$(date +%s)
  sh scratch/build_verify_$n.sh > "$out/verify_$n.txt" 2>&1
  echo "verify_$n exit=$? $(( $(date +%s) - start ))s" >> "$out/_summary.txt"
done
