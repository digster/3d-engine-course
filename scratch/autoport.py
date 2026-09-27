#!/usr/bin/env python3
"""Derive a builder's outstanding corrections from its own rebuild diff.

Workflow for repairing one builder:

    python3 scratch/autoport.py 38           # SHOW the corrections, change nothing
    python3 scratch/autoport.py 38 --apply   # port them back into the sources
    python3 docs/_template/check-builders.py 38

The rebuild is the oracle (see port_line.py).  This runs the builder in a clone,
diffs the result against the shipped page, and pairs each `+` line (what the
rebuild produces) with the `-` line it replaced (what the page actually says).
The pair is then a correction to port: old = what the source produces today,
new = what the page has published all along.

UNBALANCED HUNKS ARE REPORTED, NEVER GUESSED.  A hunk with three `-` lines and
one `+` line is a structural change - a block added to or removed from the page -
and pairing those positionally would corrupt the source.  Those are printed and
left for hand repair, which is the only honest thing to do with them.
"""
import argparse
import difflib
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXCLUDE = {"build", ".git", "out", "install"}


def rebuild(n):
    """Run builder n in a throwaway clone; return (shipped_text, rebuilt_text)."""
    builder = os.path.join(REPO, "scratch", f"build_{n}.py")
    target = re.search(r'^OUT\s*=\s*["\']([^"\']+)["\']',
                       open(builder, encoding="utf-8").read(), re.M).group(1)
    with tempfile.TemporaryDirectory(prefix="autoport-") as tmp:
        tree = os.path.join(tmp, "tree")
        os.makedirs(tree)
        for entry in os.listdir(REPO):
            if entry not in EXCLUDE:
                subprocess.run(["cp", "-Rc", os.path.join(REPO, entry),
                                os.path.join(tree, entry)], check=True)
        produced = os.path.join(tree, target)
        if os.path.exists(produced):
            os.remove(produced)
        proc = subprocess.run([sys.executable, os.path.join("scratch", f"build_{n}.py")],
                              cwd=tree, capture_output=True, text=True)
        if proc.returncode != 0:
            sys.exit(f"builder crashed:\n{proc.stderr}")
        if not os.path.exists(produced):
            sys.exit("builder exited 0 but wrote no page")
        return (open(os.path.join(REPO, target), encoding="utf-8").read(),
                open(produced, encoding="utf-8").read(), target)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("n")
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    shipped, rebuilt, target = rebuild(args.n)
    if shipped == rebuilt:
        print(f"build_{args.n}.py already reproduces {target}")
        return 0

    ship_lines, built_lines = shipped.splitlines(), rebuilt.splitlines()
    sm = difflib.SequenceMatcher(None, ship_lines, built_lines, autojunk=False)

    pairs, unbalanced = [], []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            continue
        want, got = ship_lines[i1:i2], built_lines[j1:j2]
        if tag == "replace" and len(want) == len(got):
            # Balanced: line k of the rebuild should read as line k of the page.
            for w, g in zip(want, got):
                if w != g:
                    pairs.append((g, w))
        else:
            unbalanced.append((tag, want, got))

    # Collapse duplicates; a single correction (a badge class, say) usually
    # appears many times and needs porting only once.
    seen, uniq = set(), []
    for old, new in pairs:
        if (old, new) not in seen:
            seen.add((old, new))
            uniq.append((old, new))

    print(f"{target}: {len(pairs)} changed line(s), {len(uniq)} distinct correction(s)")
    for old, new in uniq:
        print(f"  - rebuild: {old.strip()[:120]}")
        print(f"  + shipped: {new.strip()[:120]}")
    if unbalanced:
        print(f"\n!! {len(unbalanced)} UNBALANCED hunk(s) - port these by hand:")
        for tag, want, got in unbalanced:
            print(f"  [{tag}] page has {len(want)} line(s), rebuild has {len(got)}")
            for w in want[:6]:
                print(f"      page:    {w[:110]}")
            for g in got[:6]:
                print(f"      rebuild: {g[:110]}")

    if not args.apply:
        print("\n(dry run - pass --apply to port these back)")
        return 0

    tsv = os.path.join(REPO, "scratch", f"_port_{args.n}.tsv")
    with open(tsv, "w", encoding="utf-8") as fh:
        for old, new in uniq:
            fh.write(f"{old}\t{new}\n")
    return subprocess.run([sys.executable, "scratch/port_line.py", args.n, "--pairs", tsv],
                          cwd=REPO).returncode


if __name__ == "__main__":
    sys.exit(main())
