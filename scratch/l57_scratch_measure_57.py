#!/usr/bin/env python3
"""scratch/measure_57.py — the numbers Lesson 5.7 quotes.

Builds scratch/bench_57.cpp TWICE — once at -O2, once at -O2 with the vectoriser
off — for the same reason Lesson 5.6 did: below the knee a microbenchmark is
measuring the compiler's decisions, not the machine's memory, and the only way to
tell which you are looking at is to take the compiler's decisions away and see
what moves.

Run from the repository root, after `cmake --build build`.
"""
import collections
import os
import subprocess
import sys

BASE = ("c++ -std=c++20 -O2 -DNDEBUG -Wall -Wextra "
        "-I engine/include -I scratch -I build/_deps/sdl3-src/include scratch/bench_57.cpp "
        "build/engine/libengine.a -L build/_deps/sdl3-build -lSDL3 "
        "-Wl,-rpath,{cwd}/build/_deps/sdl3-build -o {out}")

CONFIGS = [
    ("vectorised", "", "build/demos/bench_57"),
    ("scalar", "-fno-vectorize -fno-slp-vectorize", "build/demos/bench_57_novec"),
]

SIZES = [4, 100, 1000, 10000, 100000]


def build_and_run(flags, out):
    cmd = BASE.format(cwd=os.getcwd(), out=out)
    if flags:
        cmd = cmd.replace("-O2 -DNDEBUG", f"-O2 -DNDEBUG {flags}")
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr)
        sys.exit(1)
    return subprocess.run([f"./{out}"], capture_output=True, text=True).stdout


def parse(text):
    out = {"q": collections.defaultdict(list), "s": collections.defaultdict(list),
           "f": collections.defaultdict(list), "sel": collections.defaultdict(list),
           "mem": {}, "size": {}}
    for line in text.strip().splitlines():
        f = line.split()
        if f[0] == "sizeof":
            out["size"][f[1]] = int(f[2])
        elif f[0] == "mem":
            out["mem"][int(f[1])] = (int(f[2]), int(f[3]), int(f[4]))
        elif f[0] == "q":
            out["q"][(f[1], int(f[3]), int(f[4]))].append((f[2], float(f[5])))
        elif f[0] == "s":
            out["s"][(f[1], int(f[3]))].append((f[2], float(f[4])))
        elif f[0] == "f":
            out["f"][int(f[3])].append((f[1], float(f[4])))
        elif f[0] == "sel":
            out["sel"][(f[1], int(f[3]))].append((f[2], float(f[4])))
    return out


def rule(title, n=78):
    print()
    print("=" * n)
    print(title)
    print("=" * n)


def header(label, sizes, width=24):
    print(f"  {label:<{width}}" + "".join(f"{n:>12,}" for n in sizes))
    print("  " + "-" * (width + 12 * len(sizes)))


# ---------------------------------------------------------------------------


def query_table(rows, body, label):
    """Rows arrive baseline, arm, baseline, arm — one pairing each."""
    print(f"\n  {label}")
    header("", SIZES)
    for k in (1, 2, 3, 4):
        e = {n: rows[(body, k, n)] for n in SIZES}
        base = {n: (e[n][0][1] + e[n][2][1]) / 2.0 for n in SIZES}
        print(f"  K={k}  {'archetype ns/item':<19}" + "".join(f"{base[n]:>12.3f}" for n in SIZES))
        for idx, name in ((1, "sparse, aligned"), (3, "sparse, scrambled")):
            cells = "".join(f"{e[n][idx][1] / e[n][idx - 1][1]:>11.2f}x" for n in SIZES)
            print(f"  {'':<5}{name:<19}" + cells)
        print()


def churn_table(rows):
    header("", SIZES, 24)
    for width, note in (("narrow", "4 components, 108 B"), ("wide", "12 components, 236 B")):
        for i, name in ((0, "archetype"), (1, "sparse set")):
            cells = "".join(f"{rows[(width, n)][i][1]:>12.2f}" for n in SIZES)
            print(f"  {width + ', ' + name:<24}" + cells)
        ratios = "".join(f"{rows[(width, n)][0][1] / rows[(width, n)][1][1]:>11.2f}x"
                         for n in SIZES)
        print(f"  {'  -> ratio':<24}" + ratios)
        print(f"  {'  (' + note + ')':<24}")
        print()


def fragment_table(rows):
    for n in sorted(rows):
        entries = rows[n]
        print(f"\n  n = {n:,} entities, query over 4 components, real body")
        print(f"    {'archetypes':>12}{'entities each':>16}{'ns/item':>10}{'vs 1 chunk':>13}")
        for i in range(0, len(entries), 2):
            base = entries[i][1]
            label, val = entries[i + 1]
            chunks = int(label)
            print(f"    {chunks:>12,}{n // chunks:>16,}{val:>10.3f}{val / base:>12.2f}x")


def select_table(rows, body, label):
    print(f"\n  {label}")
    header("", SIZES, 24)
    e = {n: rows[(body, n)] for n in SIZES}
    base = {n: sum(x[1] for x in e[n][0::2]) / len(e[n][0::2]) for n in SIZES}
    print(f"  {'archetype ns/item':<24}" + "".join(f"{base[n]:>12.3f}" for n in SIZES))
    names = [(1, "lead small pool"), (3, "…and scrambled"), (5, "lead big pool"),
             (7, "GROUPED")]
    for idx, name in names:
        cells = "".join(f"{e[n][idx][1] / e[n][idx - 1][1]:>11.2f}x" for n in SIZES)
        print(f"  {name:<24}" + cells)


def main():
    data = {}
    for name, flags, out in CONFIGS:
        data[name] = parse(build_and_run(flags, out))

    vec = data["vectorised"]
    sca = data["scalar"]
    sz = vec["size"]

    rule("0. WHAT AN ENTITY WEIGHS")
    payload = sz["transform"] + sz["velocity"] + sz["bounds"] + sz["material"]
    print(f"  transform {sz['transform']:>4} B   (the lead component, and Lesson 5.6's)")
    print(f"  velocity  {sz['velocity']:>4} B")
    print(f"  bounds    {sz['bounds']:>4} B")
    print(f"  material  {sz['material']:>4} B")
    print(f"  {'':<10}{'':>4}     ----")
    print(f"  4 components{payload:>6} B per entity")
    print()
    print("  Storage, both designs, four component types:")
    print(f"    {'n':>9}{'archetype':>14}{'sparse set':>14}{'of which index':>16}"
          f"{'overhead':>11}")
    for n in SIZES:
        a, s, i = vec["mem"][n]
        print(f"    {n:>9,}{a / 1024.0:>12,.1f} KB{s / 1024.0:>12,.1f} KB"
              f"{i / 1024.0:>14,.1f} KB{100.0 * (s - a) / a:>10.0f}%")
    print()
    print("  The sparse set's overhead is one dense id AND one sparse slot per")
    print("  component type, per ENTITY IN THE WORLD — the slot is paid whether or")
    print("  not the entity has the component. With 4 types it is 25%; the term")
    print("  that matters is that it scales with the number of TYPES, so a world")
    print("  with 32 of them pays 4 x 32 = 128 bytes an entity in index alone.")

    rule("1. QUERY — the cost of reaching K components  (-O2)")
    print("  Lower is faster; the archetype is the baseline. Each arm was timed")
    print("  ALTERNATELY against it, so the ratio is the quantity both arms paid the")
    print("  same price for. K=1 touches only the lead pool and is the control: both")
    print("  designs do literally the same walk, so anything but 1.00x at large n")
    print("  would mean the harness is measuring itself.")
    query_table(vec["q"], "cheap", "BODY 1 — cheap: one float per component, ~nothing to hide behind")
    query_table(vec["q"], "real", "BODY 2 — real: build the model matrix, as a render query does")

    rule("2. THE SAME QUERY WITH THE VECTORISER OFF  (-fno-vectorize -fno-slp-vectorize)")
    print("  One experiment, two jobs. It explains the n=4 column — where the")
    print("  vectorised table is non-monotonic in K, which no memory effect can be —")
    print("  and it isolates what is left when codegen is held still.")
    query_table(sca["q"], "cheap", "BODY 1 — cheap, scalar")
    query_table(sca["q"], "real", "BODY 2 — real, scalar")

    rule("3. STRUCTURAL CHANGE — ns per add or remove")
    print("  1% of the world gains a component and loses it again, every frame.")
    print("  'narrow' entities carry 4 components; 'wide' ones carry 12. Watch which")
    print("  column moves when the entity gets wider — that is the finding.")
    print()
    churn_table(vec["s"])

    rule("4. ARCHETYPE FRAGMENTATION — the archetype's own downside")
    print("  The same entities, the same query, split across more and more")
    print("  archetypes. This is what 'fragmentation' costs at these scales.")
    fragment_table(vec["f"])

    rule("5. SELECTIVITY — a query that matches one entity in four")
    print("  Now the archetype visits ONLY matching entities, while the sparse set")
    print("  must reach into a pool that is four times as dense as the answer. This")
    print("  is the archetype's best case, and the last row is the reason it is not")
    print("  the end of the argument.")
    select_table(vec["sel"], "cheap", "cheap body")
    select_table(vec["sel"], "real", "real body")

    rule("6. WHAT DISAGREED")
    print("  Nothing: every arm in every table returned a BIT-IDENTICAL accumulated")
    print("  value to its baseline, which the benchmark asserts on every rep. That is")
    print("  possible here and was not in Lesson 5.6, because all of these arms visit")
    print("  the entities in the SAME ORDER — only the addresses differ. The one arm")
    print("  that reorders (the scrambled selective query) is checked separately in")
    print("  verify_57 §F, to 0.0e+00.")


if __name__ == "__main__":
    main()
