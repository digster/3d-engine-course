#!/usr/bin/env python3
"""Classify before/after harness output differences for 6.17b's additivity check.

A differing line pair is TIMING when the two lines are identical once every
number is masked AND the line carries a unit of time (ms, us, ns, s) or is a
timing-derived ratio line printed by a benchmark section. Everything else is
printed in full for a human to read. Usage: classify.py <before_dir> <after_dir>
"""
import difflib, os, re, sys

STAMP = re.compile(r"^\d{4}-\d{2}-\d{2} [\d:.]+ [a-z_0-9]+\[[\d:]+\] ")
NUM = re.compile(r"[-+]?\d+(?:\.\d+)?(?:e[-+]?\d+)?")
TIMEY = re.compile(r"(\bms\b|\bus\b|\bns\b|µs|TIMING|\bspread\b|\bmean\b|x the |x\)|\d+\.\d+x\b|% of the closed form|WASH|predict|faster|slower|per quad|per run|compile)", re.I)

def lines(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return [STAMP.sub("", l.rstrip("\n")) for l in f]

before, after = sys.argv[1], sys.argv[2]
suspicious = 0
timing = 0
for name in sorted(os.listdir(before)):
    if not name.startswith("verify_"):
        continue
    a, b = lines(os.path.join(before, name)), lines(os.path.join(after, name))
    sm = difflib.SequenceMatcher(a=a, b=b, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            continue
        old, new = a[i1:i2], b[j1:j2]
        if len(old) == len(new):
            for x, y in zip(old, new):
                if NUM.sub("#", x) == NUM.sub("#", y) and (TIMEY.search(x) or STAMP.search(x) or "ERROR" in x):
                    timing += 1
                    continue
                suspicious += 1
                print(f"{name}:\n   before: {x}\n   after:  {y}")
        else:
            suspicious += max(len(old), len(new))
            print(f"{name}: unbalanced hunk\n   before: {old}\n   after:  {new}")
print(f"\n{timing} timing line pair(s); {suspicious} other differing line(s)")
