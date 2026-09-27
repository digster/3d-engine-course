#!/usr/bin/env python3
"""Freeze a builder's code listings, and VERIFY each pin against the shipped page.

A lesson's listings are supposed to show the code as it stood when the lesson was
written.  A builder that opens the live repository path shows the code as it
stands TODAY, so every later edit leaks backwards into an earlier page - and once
Module 5's refactor deleted `src/`, those builders stopped running at all.

This writes each listed path into `scratch/lNN_<flattened-path>` and prints the
LISTING_SOURCE dict to paste in.  Two details are not incidental:

  * WHICH COMMIT.  By default the commit that SHIPPED the lesson.  But several
    pages were silently rewritten by the NEXT lesson's commit - the builder read
    live, and re-running it to retrofit a nav link pulled newer code in - so for
    those the shipping commit does not reproduce the page.  Pass --commit, after
    deciding the question with which_commit.py.

  * GITIGNORED PATHS.  `scratch/` is not tracked, so a listing that points into
    it has no history to pin from and its only provenance is a working-tree copy.
    Those are marked in the output rather than passed over quietly.

Every pin is then checked by substring against the shipped page, because a pin
taken from the wrong commit fails as a puzzling diff three steps later instead of
here, where the cause is still obvious.

    python3 scratch/pin_listings.py 53 --out scratch/_dict53.txt
    python3 scratch/pin_listings.py 52 --commit ea7a05f --dry
"""
import argparse
import os
import re
import shutil
import subprocess
import sys


def esc(text):
    """The escaping every builder applies on the way into a <pre><code> block."""
    return (text.replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace('"', "&quot;")
                .replace("'", "&#x27;"))


def flatten(path):
    """src/gfx/light.hpp -> src_gfx_light.hpp, matching build_310/build_41's pins."""
    return path.replace("/", "_")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("n")
    ap.add_argument("--commit", help="pin from this commit instead of the shipping one")
    ap.add_argument("--out", help="write the LISTING_SOURCE dict here (stdout otherwise)")
    ap.add_argument("--dry", action="store_true", help="print only; write no pins")
    args = ap.parse_args()

    n = args.n
    src = open(f"scratch/build_{n}.py", encoding="utf-8").read()
    out = re.search(r'^OUT\s*=\s*["\']([^"\']+)["\']', src, re.M).group(1)
    meta = re.search(r"LISTING_META\s*=\s*\{(.*?)\n\}", src, re.S).group(1)
    paths = re.findall(r'["\']([^"\']+)["\']\s*:', meta)
    page = open(out, encoding="utf-8").read()

    commit = args.commit or subprocess.run(
        ["git", "log", "--diff-filter=A", "-1", "--format=%H", "--", out],
        capture_output=True, text=True, check=True).stdout.strip()
    if not commit:
        sys.exit(f"no shipping commit found for {out}")
    subject = subprocess.run(["git", "log", "-1", "--format=%h %s", commit],
                             capture_output=True, text=True, check=True).stdout.strip()
    print(f"# page:   {out}", file=sys.stderr)
    print(f"# commit: {subject}", file=sys.stderr)

    width = max(len(p) for p in paths) + 3
    lines, untracked, mismatched = [], [], []
    for path in paths:
        pin = f"scratch/l{n}_{flatten(path)}"
        blob = subprocess.run(["git", "show", f"{commit}:{path}"],
                              capture_output=True, check=False)
        if blob.returncode == 0:
            body = blob.stdout
        elif os.path.exists(path):
            # Not in git at that commit - gitignored, or created after it. The
            # working tree is the only provenance there is; say so out loud.
            body = open(path, "rb").read()
            untracked.append(path)
        else:
            sys.exit(f"!! {path}: not at {commit[:7]} and not in the working tree")

        if not args.dry:
            with open(pin, "wb") as fh:
                fh.write(body)
        if esc(body.decode("utf-8")) not in page:
            mismatched.append(path)
        lines.append(f'    {(chr(34) + path + chr(34) + ":"):<{width}} "{pin}",')

    dict_text = "LISTING_SOURCE = {\n" + "\n".join(lines) + "\n}\n"
    if args.out and not args.dry:
        open(args.out, "w", encoding="utf-8").write(dict_text)
        print(f"# dict written to {args.out}", file=sys.stderr)
    else:
        print(dict_text)

    if untracked:
        print(f"# NOTE {len(untracked)} listing(s) are not tracked at that commit; "
              f"pinned from the working tree:", file=sys.stderr)
        for p in untracked:
            print(f"#      {p}", file=sys.stderr)
    if mismatched:
        print(f"# !! {len(mismatched)} pin(s) do NOT appear verbatim in the shipped "
              f"page - wrong commit, or a page-only correction on top:", file=sys.stderr)
        for p in mismatched:
            print(f"#      {p}", file=sys.stderr)
    else:
        print(f"# all {len(paths)} pin(s) appear verbatim in the shipped page",
              file=sys.stderr)


if __name__ == "__main__":
    main()
