#!/usr/bin/env python3
"""Replace a builder's PARTIAL LISTING_SOURCE with the complete listing set.

Builders from 5.8 onward already pin - but only the files that lesson happened to
touch. Everything else was still read live, which is why they drifted anyway:
CMakeLists.txt, soft_renderer.hpp and the scene shaders are listed by several
lessons each, so an edit in any later lesson leaked into all of them.

The old dict is kept verbatim in a comment above the new one. Its entries record
which files that lesson's author consciously froze and why, and that provenance
is worth more than the two lines it costs.

    python3 scratch/extend_pins.py 58 --dict scratch/_dict58.txt --commit dfbdb0f
"""
import argparse
import re
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("n")
    ap.add_argument("--dict", required=True)
    ap.add_argument("--commit", required=True)
    ap.add_argument("--note", default="")
    args = ap.parse_args()

    path = f"scratch/build_{args.n}.py"
    src = open(path, encoding="utf-8").read()
    # Matches both the multi-line dict and the single-line `LISTING_SOURCE = {}`
    # that a few builders carry — an empty dict is the most misleading shape of
    # all, because the mechanism LOOKS present and pins nothing.
    match = (re.search(r"^LISTING_SOURCE\s*=\s*\{\s*\}\n", src, re.M)
             or re.search(r"^LISTING_SOURCE\s*=\s*\{.*?^\}\n", src, re.S | re.M))
    if not match:
        sys.exit("!! no LISTING_SOURCE dict to extend - use fix_builder.py instead")

    old_block = match.group(0)
    kept = "\n".join("#   " + ln for ln in old_block.rstrip().splitlines())
    new_dict = open(args.dict, encoding="utf-8").read().strip()

    header = (
        f"# EXTENDED 2026-09-12 to cover EVERY listing, not just the files this\n"
        f"# lesson wrote. The partial dict below pinned what its author knew would\n"
        f"# move; everything else stayed live, so later lessons' edits leaked into\n"
        f"# this page anyway — which is why it no longer rebuilt to what it shipped.\n"
        f"# Pinned from {args.commit}, verified per file against the shipped page\n"
        f"# with scratch/which_commit.py.\n"
        f"{args.note}"
        f"#\n"
        f"# The original dict, kept for its provenance:\n"
        f"{kept}\n"
        f"{new_dict}\n"
    )
    src = src[:match.start()] + header + src[match.end():]
    open(path, "w", encoding="utf-8").write(src)
    print(f"  + extended {path} to the full listing set")


if __name__ == "__main__":
    main()
