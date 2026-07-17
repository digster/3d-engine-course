#!/usr/bin/env python3
"""Propagate the shared course stylesheet into every page.

WHY THIS EXISTS
---------------
Every lesson must be a fully self-contained HTML file — no external stylesheet,
because a lesson has to render from a bare filesystem with no network. That
forces the shared CSS to be *duplicated* into ~91 files rather than linked.

Duplication across 91 files drifts. This script is the answer: it keeps one
source of truth (lesson-template.html) and stamps it into every page that opts
in with a marker.

THIS IS NOT A BUILD STEP.
Readers never run it. The published .html files are complete, static, and work
by double-clicking. This is an authoring-time tool, run only when the shared CSS
changes — the same way you would run a formatter.

USAGE
-----
    python3 docs/_template/apply-shared-css.py          # update every page
    python3 docs/_template/apply-shared-css.py --check   # verify, change nothing

A page opts in by containing this exact marker pair, or a previously-stamped
block between them:

    <!-- SHARED-CSS:BEGIN -->
    <!-- SHARED-CSS:END -->

Exit codes: 0 = ok / up to date, 1 = drift found (with --check), 2 = error.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

TEMPLATE = Path(__file__).parent / "lesson-template.html"
DOCS = Path(__file__).parent.parent

BEGIN = "<!-- SHARED-CSS:BEGIN -->"
END = "<!-- SHARED-CSS:END -->"

# The template's own stylesheet is delimited by these, so we can lift it out
# without depending on it being the first <style> in the file.
STYLE_RE = re.compile(r"<style>.*?</style>", re.DOTALL)

# A fill="..." on an SVG <text> is a *presentation attribute*, and CSS always
# beats it. Since the shared stylesheet contains `figure.dia svg text { fill: … }`,
# any inline fill on a <text> is silently ignored — the label just renders in the
# wrong colour, with no warning, and it looks fine at a glance.
#
# This has now bitten twice, so it is a lint rather than a note. Use the classes
# (.muted, .t-hi, .t-ok, .t-bad, .t-inv, .lbl-x/y/z) instead.
TEXT_FILL_RE = re.compile(r"<text[^>]*\sfill=", re.IGNORECASE)


def extract_shared_css() -> str:
    """Lift the <style> block out of the canonical template."""
    if not TEMPLATE.exists():
        sys.exit(f"error: template not found at {TEMPLATE}")
    match = STYLE_RE.search(TEMPLATE.read_text(encoding="utf-8"))
    if not match:
        sys.exit(f"error: no <style> block found in {TEMPLATE.name}")
    return match.group(0)


def stamp(text: str, css: str) -> str | None:
    """Replace the marked region with `css`. Returns None if unchanged/unmarked."""
    start = text.find(BEGIN)
    end = text.find(END)
    if start == -1 or end == -1 or end < start:
        return None
    replacement = f"{BEGIN}\n{css}\n{END}"
    updated = text[:start] + replacement + text[end + len(END):]
    return None if updated == text else updated


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="report drift without writing (for CI or a pre-commit hook)")
    args = parser.parse_args()

    css = extract_shared_css()
    pages = sorted(p for p in DOCS.rglob("*.html") if p != TEMPLATE)

    drifted: list[Path] = []
    skipped: list[Path] = []
    lint_hits: list[tuple[Path, int, str]] = []

    rel = lambda p: p.relative_to(DOCS)  # noqa: E731

    # Lint every page, including the template — the template is where the bug
    # would do the most damage, since it is copied into every lesson.
    for page in sorted(DOCS.rglob("*.html")):
        for lineno, line in enumerate(page.read_text(encoding="utf-8").splitlines(), 1):
            if TEXT_FILL_RE.search(line):
                lint_hits.append((page, lineno, line.strip()[:70]))

    for page in pages:
        text = page.read_text(encoding="utf-8")
        updated = stamp(text, css)
        if updated is None:
            if BEGIN not in text:
                skipped.append(page)
            continue
        drifted.append(page)
        if not args.check:
            page.write_text(updated, encoding="utf-8")

    for page in skipped:
        print(f"  skip   {rel(page)} (no SHARED-CSS marker)")

    if drifted:
        verb = "would update" if args.check else "updated"
        for page in drifted:
            print(f"  {verb}: {rel(page)}")
        if not args.check:
            print(f"\nStamped shared CSS into {len(drifted)} page(s).")
    else:
        print(f"CSS up to date across {len(pages) - len(skipped)} page(s).")

    if lint_hits:
        print(f"\nLINT: {len(lint_hits)} inline fill on an SVG <text> — CSS overrides these,")
        print("      so the colour is silently ignored. Use a class instead")
        print("      (.muted .t-hi .t-ok .t-bad .t-inv .lbl-x/y/z).")
        for page, lineno, snippet in lint_hits:
            print(f"  {rel(page)}:{lineno}  {snippet}")

    if args.check and drifted:
        print(f"\ndrift: {len(drifted)} page(s) out of date. Run without --check to fix.")

    return 1 if lint_hits or (args.check and drifted) else 0


if __name__ == "__main__":
    sys.exit(main())
