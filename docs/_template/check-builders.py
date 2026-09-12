#!/usr/bin/env python3
"""Assert that every lesson page generator still reproduces its published page.

WHY THIS EXISTS
---------------
Each lesson page under `docs/lessons/` is assembled by a generator in `scratch/`
(`build_NN.py`) out of prose fragments, computed SVGs and code listings read from
the repository.  Two failure modes creep in silently, and both were found live on
2026-09-12 across 27 of 37 builders:

  CAUSE A - unpinned listings.  A builder that reads a repository file LIVE shows
    whatever that file says TODAY, so every later lesson's edits leak backwards
    into an earlier lesson's page.  The cure is `LISTING_SOURCE`: pin each listed
    path to a frozen copy taken from the commit that shipped that lesson.

  CAUSE B - stale body fragments.  A correction applied to the shipped HTML and
    not to the `scratch/lNN_body_*.html` it came from means a rebuild REVERTS the
    correction.  The 2026-09-08 "Module 8 -> Module 9" renumber did exactly this.

Neither is visible from reading the page: the page on disk is right, and only a
rebuild reveals that its source no longer produces it.

THE MEASUREMENT, AND THE MISTAKE IT IS DESIGNED AGAINST
-------------------------------------------------------
An earlier audit asked one question - "did the page file change?" - and read
"unchanged" as "reproduces".  But a builder that dies on its first listing writes
NOTHING, so eleven crashes scored as eleven passes.  Exit status, output
existence and output equality are three independent axes and this script reports
them separately.  It never infers one from another.

Each builder runs in a throwaway clone of the tree (APFS `clonefile`, so the copy
is near-free) and writes into that clone.  The working tree is never touched, and
a builder that scribbles on files other than its own page cannot corrupt anything.

USAGE
-----
    python3 docs/_template/check-builders.py              # all builders
    python3 docs/_template/check-builders.py 51 52 66     # just these
    python3 docs/_template/check-builders.py --diff 51    # show the first diff hunks

Exit status is 0 only when every builder reproduces its page byte-identically.
"""
from __future__ import annotations

import argparse
import difflib
import os
import re
import shutil
import subprocess
import sys
import tempfile

# Directories that are pure build output or history: excluded from the clone so a
# run stays fast.  Nothing a BUILDER reads lives in either.  --figures keeps
# `build/`, because figs_511.py reads a render capture from the build tree.
EXCLUDE = {"build", ".git", "out", "install"}
EXCLUDE_FIGURES = {".git"}

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BUILDERS_DIR = os.path.join(REPO, "scratch")

# `OUT = "docs/lessons/..."` is the one line every builder agrees on.  Parsing it
# rather than guessing from the filename keeps the harness honest about which
# page a given generator actually claims.
OUT_RE = re.compile(r'^OUT\s*=\s*[\'"]([^\'"]+)[\'"]', re.MULTILINE)


def builder_target(path: str) -> str | None:
    """Return the repo-relative page a builder writes, or None if it declares none."""
    with open(path, encoding="utf-8") as fh:
        match = OUT_RE.search(fh.read())
    return match.group(1) if match else None


def clone_tree(dest: str, exclude: set[str] = EXCLUDE) -> None:
    """Copy-on-write clone of the repository into `dest`.

    `cp -Rc` asks APFS for clonefile(2), which shares the blocks instead of
    copying them - a ~100 MB tree lands in about a tenth of a second.  The `-c`
    flag is macOS-only, so fall back to a real copy elsewhere.
    """
    os.makedirs(dest, exist_ok=True)
    for entry in os.listdir(REPO):
        if entry in exclude:
            continue
        src = os.path.join(REPO, entry)
        dst = os.path.join(dest, entry)
        if sys.platform == "darwin":
            subprocess.run(["cp", "-Rc", src, dst], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        elif os.path.isdir(src):
            shutil.copytree(src, dst, symlinks=True)
        else:
            shutil.copy2(src, dst)


def run_one(builder: str, show_diff: bool, figures: bool = False) -> tuple[str, str]:
    """Run one builder in a clone and classify the result.

    Returns (status, detail) where status is one of:
      ok      - exited 0 and reproduced the shipped page byte for byte
      crash   - exited non-zero (the page it would have written is irrelevant)
      missing - exited 0 but wrote no page at all
      diff    - exited 0, wrote a page, and it differs from the shipped one
      nodecl  - the script declares no OUT target, so there is nothing to check
    """
    name = os.path.basename(builder)
    target = builder_target(builder)
    if target is None:
        return "nodecl", "declares no OUT target"

    shipped = os.path.join(REPO, target)
    if not os.path.exists(shipped):
        return "crash", f"shipped page {target} does not exist"

    with tempfile.TemporaryDirectory(prefix="builder-check-") as tmp:
        tree = os.path.join(tmp, "tree")
        clone_tree(tree, EXCLUDE_FIGURES if figures else EXCLUDE)

        # --figures goes one layer deeper: a builder can reproduce its page
        # perfectly while the GENERATOR behind its diagrams is stale, which is
        # the same drift class one level down. Regenerate the SVGs first and let
        # the page comparison speak for both.
        if figures:
            figs = os.path.join("scratch", f"figs_{name[6:-3]}.py")
            if os.path.exists(os.path.join(tree, figs)):
                fig_proc = subprocess.run([sys.executable, figs], cwd=tree,
                                          capture_output=True, text=True)
                if fig_proc.returncode != 0:
                    tail = (fig_proc.stderr.strip().splitlines() or ["(no stderr)"])[-1]
                    return "figcrash", f"figs_{name[6:-3]}.py failed: {tail}"

        # Remove the page inside the clone first.  If the builder crashes we then
        # see a MISSING output rather than the page the clone inherited - which is
        # precisely the confusion that let eleven crashes pass as successes.
        produced = os.path.join(tree, target)
        if os.path.exists(produced):
            os.remove(produced)

        proc = subprocess.run(
            [sys.executable, os.path.join("scratch", name)],
            cwd=tree, capture_output=True, text=True,
        )

        if proc.returncode != 0:
            tail = (proc.stderr.strip().splitlines() or ["(no stderr)"])[-1]
            return "crash", f"exit {proc.returncode}: {tail}"

        if not os.path.exists(produced):
            return "missing", "exited 0 but wrote no page"

        with open(produced, encoding="utf-8") as fh:
            got = fh.read()
        with open(shipped, encoding="utf-8") as fh:
            want = fh.read()
        if got == want:
            return "ok", f"{len(want):,} bytes"

        got_lines, want_lines = got.splitlines(True), want.splitlines(True)
        changed = sum(1 for ln in difflib.unified_diff(want_lines, got_lines, n=0)
                      if ln[:1] in "+-" and ln[:3] not in ("+++", "---"))
        detail = f"{changed} line(s) differ from the shipped page"
        if show_diff:
            hunks = list(difflib.unified_diff(
                want_lines, got_lines, fromfile=f"shipped/{target}",
                tofile=f"rebuilt/{target}", n=1))
            detail += "\n" + "".join(hunks[:80]).rstrip()
        return "diff", detail


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("only", nargs="*", metavar="NN",
                        help="builder suffixes to check, e.g. 51 66 (default: all)")
    parser.add_argument("--diff", action="store_true",
                        help="print the first diff hunks for every differing page")
    parser.add_argument("--figures", action="store_true",
                        help="also regenerate each lesson's SVGs from figs_NN.py first "
                             "(slower; catches a stale figure generator behind a page "
                             "that otherwise rebuilds correctly)")
    args = parser.parse_args()

    builders = sorted(
        (os.path.join(BUILDERS_DIR, f) for f in os.listdir(BUILDERS_DIR)
         if re.fullmatch(r"build_\d+\.py", f)),
        # Sort by (module, lesson) so the report reads in course order: build_39
        # comes before build_310, which a plain string sort gets backwards.
        key=lambda p: (lambda d: (int(d[0]), int(d[1:])))(
            re.fullmatch(r"build_(\d+)\.py", os.path.basename(p)).group(1)),
    )
    if args.only:
        wanted = {f"build_{n}.py" for n in args.only}
        builders = [b for b in builders if os.path.basename(b) in wanted]
        if not builders:
            print("no builders matched", file=sys.stderr)
            return 2

    tally: dict[str, list[str]] = {}
    for builder in builders:
        status, detail = run_one(builder, args.diff, args.figures)
        name = os.path.basename(builder)
        tally.setdefault(status, []).append(name)
        mark = {"ok": "ok   ", "crash": "CRASH", "missing": "EMPTY",
                "diff": "DIFF ", "nodecl": "-    ", "figcrash": "FIGX "}[status]
        print(f"{mark} {name:<16} {detail}")

    print()
    total = len(builders)
    good = len(tally.get("ok", []))
    print(f"{good}/{total} builders reproduce their shipped page byte-identically")
    for status, label in (("crash", "CRASH  (wrote nothing - NOT a pass)"),
                          ("figcrash", "FIGX   (figure generator failed)"),
                          ("missing", "EMPTY  (exited 0, wrote nothing)"),
                          ("diff", "DIFF   (rebuild would change the page)"),
                          ("nodecl", "SKIP   (no OUT declared)")):
        names = tally.get(status)
        if names:
            print(f"  {label}: {' '.join(n[6:-3] for n in names)}")
    if args.figures:
        print("\n  note: figs_45/46/48 read .ppm render captures that later sessions\n"
              "        overwrote, so their SVGs no longer regenerate. The published SVGs\n"
              "        are correct and the pages rebuild from them; it is the SVGs'\n"
              "        own inputs that are lost. See LEARNINGS.md.")
    return 0 if good + len(tally.get("nodecl", [])) == total else 1


if __name__ == "__main__":
    sys.exit(main())
