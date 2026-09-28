#!/usr/bin/env python3
"""Lesson 6.17b's insertion bookkeeping: its listings, and the later pins it must reach.

An inserted lesson is written AFTER the lessons that follow it in course order,
so two different texts exist for every file it touches:

  - ITS LISTING is the file as a student has it at 6.17b: the tree after 6.17
    (`scratch/replay_tree.py 6.17`) plus 6.17b's edits. Not HEAD's version,
    which already holds 6.18, Module 7 and Module 8.
  - EVERY LATER PIN of the file (6.18's gpu_texture, 7.5's and 8.4's gltf_view,
    8.4's shadow.hpp) must gain 6.17b's edits too, or a student who copies
    listings in course order loses them at that lesson — the authoring guide's
    §17, step 3, which `check-continuity.py` R1 enforces.

Both are the same operation: carry 6.17b's hunk (working tree against HEAD) onto
some other version of the file. `carry()` does it and PROVES it: the result must
differ from the working-tree file by exactly the lines the other version differs
from HEAD — no more, no fewer.

    python3 scratch/make_t617b.py tree scratch/_t617 scratch/_t617b [--pins]
    python3 scratch/make_t617b.py carry            # rewrite the later pins
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
    "engine/include/engine/gfx/light.hpp",
    "engine/include/engine/gfx/shadow.hpp",
    "engine/src/gfx/shadow.cpp",
    "engine/include/engine/gfx/cubemap.hpp",
    "engine/src/gfx/cubemap.cpp",
    "engine/include/engine/gfx/clip.hpp",
    "engine/src/gfx/clip.cpp",
    "engine/include/engine/gfx/soft_renderer.hpp",
    "engine/src/gfx/soft_renderer.cpp",
    "engine/include/engine/gfx/raster.hpp",
    "engine/src/gfx/raster.cpp",
    "engine/include/engine/gfx/gpu_uniform.hpp",
    "engine/include/engine/gfx/gpu_texture.hpp",
    "engine/src/gfx/gpu_texture.cpp",
    "engine/include/engine/gfx/gpu_debug.hpp",
    "engine/src/gfx/gpu_debug.cpp",
    "engine/include/engine/gfx/gpu_shadow.hpp",
    "engine/src/gfx/gpu_shadow.cpp",
    "engine/include/engine/gfx/gpu_scene.hpp",
    "engine/src/gfx/gpu_scene.cpp",
    "shaders/scene.frag.hlsl",
    "demos/gltf_view/main.cpp",
]

# The later lessons' pins of files 6.17b changed — found by listing scratch/ for
# every lNN_ pin, in course order after 6.17b, of a path in CHANGED.
LATER_PINS = {
    "scratch/l618_engine_include_engine_gfx_gpu_texture.hpp": "engine/include/engine/gfx/gpu_texture.hpp",
    "scratch/l618_engine_src_gfx_gpu_texture.cpp": "engine/src/gfx/gpu_texture.cpp",
    "scratch/l75_demos_gltf_view_main.cpp": "demos/gltf_view/main.cpp",
    "scratch/l84_demos_gltf_view_main.cpp": "demos/gltf_view/main.cpp",
    "scratch/l84_engine_include_engine_gfx_shadow.hpp": "engine/include/engine/gfx/shadow.hpp",
}


def delta(a, b):
    """The changed lines between two texts, signs and all, context-free."""
    return [l for l in difflib.unified_diff(a.splitlines(), b.splitlines(), lineterm="", n=0)
            if l[:1] in "+-" and not l.startswith(("+++", "---"))]


def git(*args):
    return subprocess.run(["git", *args], cwd=REPO, capture_output=True, check=True).stdout


def carry(path, other, head, ours):
    """6.17b's edits to `path`, carried onto `other` (another version of it).

    `head` is HEAD's text (before 6.17b), `ours` the working tree (after it).
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
            sys.exit(f"{path}: 6.17b's hunk does not apply to this version, and its own "
                     f"edits are not simple line replacements")
        result, how = "\n".join(undone), "by undoing the other version's lines in ours"

    # THE PROOF.
    if delta(result, ours) != delta(other, head):
        sys.exit(f"{path}: the carried text differs from the working tree by more than "
                 f"the other version's own lines — refusing to write it")
    return result, how


def tree(args):
    if os.path.exists(args.out):
        shutil.rmtree(args.out)
    shutil.copytree(args.base, args.out)
    pins = {}
    for path in CHANGED:
        base_file = os.path.join(args.out, path)
        with open(base_file, encoding="utf-8") as fh:
            at_617 = fh.read()
        head = git("show", f"HEAD:{path}").decode("utf-8")
        with open(os.path.join(REPO, path), encoding="utf-8") as fh:
            ours = fh.read()
        listing, how = carry(path, at_617, head, ours)
        with open(base_file, "w", encoding="utf-8") as fh:
            fh.write(listing)
        print(f"  {how:48s} {path}")
        if args.pins:
            pin = os.path.join("scratch", "l617b_" + path.replace("/", "_"))
            with open(os.path.join(REPO, pin), "w", encoding="utf-8") as fh:
                fh.write(listing)
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
        # changes by exactly 6.17b's lines.
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
