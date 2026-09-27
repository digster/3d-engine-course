#!/usr/bin/env python3
"""Apply the mechanical half of a builder repair, and say exactly what it did.

The three edits below were identical in every pre-5.8 builder, so they are worth
automating; everything else (nav links, renumbers, badge vocabulary in prose) is
per-lesson and stays by hand.

  1. LISTING_SOURCE  - the dict plus the `.get(path, path)` lookup in listing().
                       Older builders have no pinning mechanism AT ALL, so both
                       halves have to be added, not just the entries.
  2. STATE retirement - CLAUDE.md §9 was amended at 5.7; builders older than that
                       still stamp a block the pages no longer carry.
  3. Shared markers   - apply-shared.py stamps an explanatory comment inside the
                       SHARED-CSS / SHARED-SCRIPT marker pairs. Builders written
                       before the CSS extraction emit a bare link/script tag.

Every edit is ASSERTED: if a pattern is absent the script says so rather than
silently doing nothing, because "the substitution changed nothing" is exactly how
a half-applied repair passes for a finished one.

    python3 scratch/fix_builder.py 38 --commit 7ec993a
"""
import argparse
import re
import sys

CSS_COMMENT = """<!-- The shared course stylesheet, linked rather than inlined. Single source of
     truth: docs/shared/course.css. Edit that file; this page carries no copy.
     Still no build step - the link resolves straight off the filesystem, so this
     page opens by double-clicking, offline. -->
"""

JS_COMMENT = """<!-- The shared page script (theme toggle, TOC scrollspy, syntax highlighter),
     linked rather than inlined. Single source of truth: docs/shared/course.js.
     A plain classic script at end of body, so it runs exactly where the inline
     copy used to: after the DOM is parsed, before KaTeX's deferred render. -->
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("n")
    ap.add_argument("--commit", required=True, help="the commit that shipped the lesson")
    ap.add_argument("--dict", required=True, help="file holding the LISTING_SOURCE block")
    args = ap.parse_args()

    n, path = args.n, f"scratch/build_{args.n}.py"
    src = open(path).read()
    did, skipped = [], []

    # --- 1. pinning ---------------------------------------------------------
    # Test for the ASSIGNMENT, not the name. A previous aborted run can leave
    # listing() referring to LISTING_SOURCE with no dict defined, and a check for
    # the bare name then reports "already present" for a file that cannot run.
    if re.search(r"^LISTING_SOURCE\s*=\s*\{", src, re.M):
        skipped.append("LISTING_SOURCE dict already present")
    else:
        block = open(args.dict).read().strip()
        header = (
            "\n"
            "# LISTING_SOURCE — added 2026-09-12, and without it THIS SCRIPT CANNOT RUN.\n"
            "# ---------------------------------------------------------------------------\n"
            "#\n"
            "# The paths this page lists live under `src/`, and Module 5's refactor RETIRED\n"
            "# THAT WHOLE DIRECTORY — the engine's sources moved to engine/src and its\n"
            "# headers to engine/include/engine. So re-running this builder died at the\n"
            "# first listing with a FileNotFoundError, which means the page had been\n"
            "# unreproducible since Lesson 5.1 and nobody had noticed, because nobody had\n"
            "# needed to rebuild it.\n"
            "#\n"
            "# Paths that DO still exist (shaders, CMakeLists.txt) are pinned too, and for\n"
            "# the opposite reason: reading them live is silent rather than fatal, so every\n"
            "# later lesson's edits leaked backwards into this page.\n"
            "#\n"
            f"# The contents are pinned from commit {args.commit} — the commit that SHIPPED\n"
            "# this lesson — so the listings show the code as it stood when the lesson was\n"
            "# written, which is what a lesson's listings are supposed to show. Same\n"
            "# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every\n"
            "# builder from 5.8 onward.\n"
            f"{block}\n"
        )
        meta_end = re.search(r"LISTING_META\s*=\s*\{.*?\n\}\n", src, re.S)
        if not meta_end:
            sys.exit("!! could not find the end of LISTING_META")
        src = src[:meta_end.end()] + header + src[meta_end.end():]
        did.append("inserted LISTING_SOURCE")

    old_open = "def listing(path):\n    with open(path) as fh:"
    if old_open in src:
        src = src.replace(
            old_open, "def listing(path):\n    with open(LISTING_SOURCE.get(path, path)) as fh:", 1)
        did.append("listing() now reads the pin")
    else:
        skipped.append("listing() already pinned")

    # --- 2. STATE retirement -------------------------------------------------
    state_block = re.search(
        r'TAIL = """\n  <details class="state">\n.*?</details>\n\n', src, re.S)
    if state_block:
        note = (
            "# AMENDED 2026-09-12: the STATE block is gone, and this script no longer\n"
            "# stamps one. `STATE.md` became the sole resume key at 0.6 and CLAUDE.md §9\n"
            "# was amended at 5.7 to retire the per-lesson block — but this builder\n"
            "# predates that and was never updated, so re-running it would have RE-ADDED a\n"
            "# STATE block to a page it was stripped from. The same amendment build_57.py\n"
            "# received in 5.8.\n"
            'TAIL = """\n\n'
        )
        src = src[:state_block.start()] + note + src[state_block.end():]
        did.append("retired the STATE block from TAIL")
    else:
        skipped.append("no STATE block in TAIL")

    for pat, what in ((rf'\s*with open\("scratch/l{n}_state\.txt"\) as fh:\n\s*state = fh\.read\(\)\.rstrip\(\)\n',
                       "dropped the state file read"),
                      (r'\s*page = page\.replace\("@@STATE@@", esc\(state\)\)\n',
                       "dropped the @@STATE@@ substitution")):
        new, count = re.subn(pat, "\n", src, count=1)
        if count:
            src, _ = new, did.append(what)
        else:
            skipped.append(what.replace("dropped", "nothing to drop for"))

    # --- 3. shared-asset marker comments -------------------------------------
    for begin, comment, tag, label in (
            ("<!-- SHARED-CSS:BEGIN -->", CSS_COMMENT, '<link rel="stylesheet"', "CSS"),
            ("<!-- SHARED-SCRIPT:BEGIN -->", JS_COMMENT, "<script src=", "script")):
        bare = f"{begin}\n{tag}"
        stamped = f"{begin}\n{comment}{tag}"
        if stamped in src:
            skipped.append(f"{label} marker already stamped")
        elif bare in src:
            src = src.replace(bare, stamped, 1)
            did.append(f"stamped the {label} marker comment")
        else:
            skipped.append(f"!! {label} marker not in the expected shape")

    open(path, "w").write(src)
    for line in did:
        print(f"  + {line}")
    for line in skipped:
        print(f"  . {line}")


if __name__ == "__main__":
    main()
