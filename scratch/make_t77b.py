#!/usr/bin/env python3
"""Lesson 7.7b's insertion bookkeeping: its listings, and the later pins it must reach.

A copy of scratch/make_t618b.py (the worked example the authoring guide's §17
points at), with 7.7b's file lists. Nothing else changed: every listing of a
changed file is the student's tree at 7.7 (`scratch/replay_tree.py 7.7`) plus
7.7b's hunk, carried and PROVED by delta; every NEW file's listing is the
working tree's text; and every later pin of the three build lists (7.8's and
Module 8's) gains 7.7b's lines the same way.

At 7.7 the glTF, skeleton and skin files are identical to HEAD (no later lesson
lists them), so their listings are simply the working tree's; the three build
lists are the ones whose 7.7 text differs, and whose later pins must carry.

    python3 scratch/make_t77b.py tree scratch/_t77 scratch/_t77b [--pins]
    python3 scratch/make_t77b.py carry            # rewrite the later pins
"""
import argparse
import difflib
import os
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CHANGED = [
    "demos/CMakeLists.txt",
    "engine/CMakeLists.txt",
    "engine/include/engine/engine.hpp",
    "engine/include/engine/gfx/gltf.hpp",
    "engine/src/gfx/gltf.cpp",
    # Three corrections to 7.6's files, each found by this lesson's first real
    # asset or its specification: the non-uniform-scale tolerance, the weight
    # tolerance, and two comments about what an out-of-order joint does.
    "engine/include/engine/anim/skeleton.hpp",
    "engine/src/anim/skeleton.cpp",
    "engine/include/engine/anim/skin.hpp",
]

# Files 7.7b CREATES. Their listing is the working tree's text, and no later pin
# can hold them.
NEW = [
    "engine/include/engine/anim/import.hpp",
    "engine/src/anim/import.cpp",
    "demos/mannequin/main.cpp",
]

# The later lessons' pins of files 7.7b changed — every lNN_ pin, in course
# order after 7.7b (7.8 and Module 8), of a path in CHANGED. Only the three
# build lists appear: nothing after 7.7 lists the glTF, skeleton or skin files.
LATER_PINS = {
    "scratch/l78_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l78_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l78_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l81_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l81_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l81_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l82_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l82_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l82_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l83_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l83_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l83_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l84_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l84_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l84_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l85_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l85_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l85_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l86_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l86_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l86_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l87_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l87_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l87_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l88_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l88_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l88_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l89_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l89_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l89_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l810_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l811_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l811_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l811_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l812_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l812_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l812_demos_CMakeLists.txt": "demos/CMakeLists.txt",
    "scratch/l813_engine_CMakeLists.txt": "engine/CMakeLists.txt",
    "scratch/l813_engine_include_engine_engine.hpp": "engine/include/engine/engine.hpp",
    "scratch/l813_demos_CMakeLists.txt": "demos/CMakeLists.txt",
}


def delta(a, b):
    """The changed lines between two texts, signs and all, context-free."""
    return [l for l in difflib.unified_diff(a.splitlines(), b.splitlines(), lineterm="", n=0)
            if l[:1] in "+-" and not l.startswith(("+++", "---"))]


def git(*args):
    return subprocess.run(["git", *args], cwd=REPO, capture_output=True, check=True).stdout


def carry(path, other, head, ours):
    """7.7b's edits to `path`, carried onto `other` (another version of it).

    `head` is HEAD's text (before 7.7b), `ours` the working tree (after it).
    Returns (text, how). Exits if the result cannot be proved.
    """
    if other.rstrip("\n") == head.rstrip("\n"):
        return ours, "copied (same text as HEAD)"

    patch = git("diff", "--no-color", "HEAD", "--", path)
    result, how = None, "patched"
    with tempfile.TemporaryDirectory() as tmp:
        dest = os.path.join(tmp, path)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", encoding="utf-8") as fh:
            fh.write(other)
        env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
        # Full context, then one line: a hunk whose outer context line is the
        # one the other version rewrote still applies with -C1.
        for flags, note in ((), ""), (("-C1",), " with -C1"):
            res = subprocess.run(["git", "apply", "--recount", *flags, "-"], cwd=tmp,
                                 input=patch, capture_output=True, env=env)
            if res.returncode == 0:
                with open(dest, encoding="utf-8") as fh:
                    result, how = fh.read(), "patched" + note
                break

    # THE OTHER DIRECTION, when the hunk will not apply: undo the other
    # version's edits in OUR file instead. Safe only when each of them is a
    # one-for-one line replacement whose HEAD text occurs exactly once in our
    # file — shadow.hpp's single `bounds.hpp` include, moved by 8.4, is exactly
    # that. (Zero-context `git apply` was tried here first and put the new
    # include OUTSIDE the header guard; the proof below refused it.)
    if result is None or delta(result, ours) != delta(other, head):
        sm = difflib.SequenceMatcher(a=head.splitlines(), b=other.splitlines(), autojunk=False)
        undone = ours.split("\n")
        ok = True
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == "equal":
                continue
            if tag != "replace" or (i2 - i1) != (j2 - j1):
                ok = False
                break
            for k in range(i2 - i1):
                line = head.splitlines()[i1 + k]
                if undone.count(line) != 1:
                    ok = False
                    break
                undone[undone.index(line)] = other.splitlines()[j1 + k]
        if not ok:
            sys.exit(f"{path}: 7.7b's hunk does not apply to this version, and its own "
                     f"edits are not simple line replacements")
        result, how = "\n".join(undone), "by undoing the other version's lines in ours"

    # THE PROOF.
    if delta(result, ours) != delta(other, head):
        sys.exit(f"{path}: the carried text differs from the working tree by more than "
                 f"the other version's own lines — refusing to write it")
    return result, how


def insert_includes(at_618, head, ours):
    """engine.hpp at 6.18 predates the Module 6 headers (5.12's umbrella lint was
    inserted after Module 6 was written and never carried into its pins, so nothing
    at 6.18 asks for them). 7.7b's hunk therefore has no context to apply to. Its
    three added lines are inserted at their ALPHABETICAL places in the 6.18 text
    instead, and the proof is exact line accounting: the result is the 6.18 text
    plus exactly the lines 7.7b added, and nothing else."""
    added = [l for l in delta(head, ours) if l.startswith("+")]
    assert all(not l.startswith("-") for l in delta(head, ours)), "7.7b only adds to engine.hpp"
    lines = at_618.split("\n")
    for a in added:
        inc = a[1:]
        prefix = inc.split("/")[0] + "/" + inc.split("/")[1]   # "#include <engine/gfx"
        block = [i for i, l in enumerate(lines) if l.startswith(prefix)]
        # the GPU-side includes form their own alphabetical run after the CPU side
        gpu = "gpu_" in inc
        block = [i for i in block if ("gpu_" in lines[i]) == gpu]
        pos = next((i for i in block if lines[i] > inc), block[-1] + 1)
        lines.insert(pos, inc)
    result = "\n".join(lines)
    got = sorted(l for l in delta(at_618, result))
    want = sorted(added)
    if got != want:
        sys.exit(f"engine.hpp: the inserted text differs from 6.18 by {got}, not {want}")
    return result, "inserted alphabetically (no context at 6.18)"


def tree(args):
    if os.path.exists(args.out):
        shutil.rmtree(args.out)
    shutil.copytree(args.base, args.out)
    pins = {}
    for path in CHANGED:
        base_file = os.path.join(args.out, path)
        with open(base_file, encoding="utf-8") as fh:
            at_77 = fh.read()
        head = git("show", f"HEAD:{path}").decode("utf-8")
        with open(os.path.join(REPO, path), encoding="utf-8") as fh:
            ours = fh.read()
        # 7.7's engine.hpp already lists the anim headers, so 7.7b's one line
        # has context and carries like any other hunk (6.18b needed
        # insert_includes, kept below for the record).
        listing, how = carry(path, at_77, head, ours)
        with open(base_file, "w", encoding="utf-8") as fh:
            fh.write(listing)
        print(f"  {how:48s} {path}")
        if args.pins:
            pin = os.path.join("scratch", "l77b_" + path.replace("/", "_"))
            with open(os.path.join(REPO, pin), "w", encoding="utf-8") as fh:
                fh.write(listing)
            pins[path] = pin
    for path in NEW:
        dest = os.path.join(args.out, path)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copyfile(os.path.join(REPO, path), dest)
        print(f"  {'new (the working tree)':48s} {path}")
        if args.pins:
            pin = os.path.join("scratch", "l77b_" + path.replace("/", "_"))
            shutil.copyfile(os.path.join(REPO, path), os.path.join(REPO, pin))
            pins[path] = pin
    if args.pins:
        print("\nLISTING_SOURCE = {")
        for path, pin in pins.items():
            print(f'    "{path}": "{pin}",')
        print("}")


def carry_later():
    for pin, path in LATER_PINS.items():
        with open(os.path.join(REPO, pin), encoding="utf-8") as fh:
            old = fh.read()
        head = git("show", f"HEAD:{path}").decode("utf-8")
        with open(os.path.join(REPO, path), encoding="utf-8") as fh:
            ours = fh.read()
        new, how = carry(path, old, head, ours)
        # Keep the pin's own final-newline convention, so the page built from it
        # changes by exactly 7.7b's lines.
        if old.endswith("\n") and not new.endswith("\n"):
            new += "\n"
        if not old.endswith("\n"):
            new = new.rstrip("\n")
        with open(os.path.join(REPO, pin), "w", encoding="utf-8") as fh:
            fh.write(new)
        print(f"  {how:48s} {pin}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    t = sub.add_parser("tree")
    t.add_argument("base")
    t.add_argument("out")
    t.add_argument("--pins", action="store_true")
    sub.add_parser("carry")
    args = ap.parse_args()
    if args.cmd == "tree":
        tree(args)
    else:
        carry_later()


if __name__ == "__main__":
    main()
