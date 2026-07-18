#!/usr/bin/env python3
"""Propagate the shared course stylesheet and page script into every page.

WHY THIS EXISTS
---------------
Every lesson must be a fully self-contained HTML file — no external stylesheet
and no external script, because a lesson has to render from a bare filesystem
with no network. That forces the shared CSS *and* the shared page script to be
*duplicated* into ~91 files rather than linked.

Duplication across 91 files drifts. This script is the answer: it keeps one
source of truth (lesson-template.html) and stamps it into every page that opts
in with a marker.

The script region had drifted badly before it was centralised here: three
different keyword lists, a CMake highlighter that existed in one lesson only,
and a Windows-batch `::` comment rule that existed in two. Every one of those
was a silent mis-render rather than a crash, which is exactly why a marker beats
vigilance.

THIS IS NOT A BUILD STEP.
Readers never run it. The published .html files are complete, static, and work
by double-clicking. This is an authoring-time tool, run only when the shared CSS
or script changes — the same way you would run a formatter.

USAGE
-----
    python3 docs/_template/apply-shared.py           # update every page
    python3 docs/_template/apply-shared.py --check   # verify, change nothing

A page opts into a region by containing that region's marker pair, or a
previously-stamped block between them:

    <!-- SHARED-CSS:BEGIN -->      <!-- SHARED-SCRIPT:BEGIN -->
    <!-- SHARED-CSS:END -->        <!-- SHARED-SCRIPT:END -->

The two regions are independent: a page may opt into either, both, or neither.
A page carrying its own page-specific script (an interactive widget, say) keeps
it *outside* the markers — only the marked region is ever rewritten.

Exit codes: 0 = ok / up to date, 1 = drift found (with --check), 2 = error.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import NoReturn

TEMPLATE = Path(__file__).parent / "lesson-template.html"
DOCS = Path(__file__).parent.parent

# A fill="..." on an SVG <text> is a *presentation attribute*, and CSS always
# beats it. Since the shared stylesheet contains `figure.dia svg text { fill: … }`,
# any inline fill on a <text> is silently ignored — the label just renders in the
# wrong colour, with no warning, and it looks fine at a glance.
#
# This has now bitten twice, so it is a lint rather than a note. Use the classes
# (.muted, .t-hi, .t-ok, .t-bad, .t-inv, .lbl-x/y/z) instead.
TEXT_FILL_RE = re.compile(r"<text[^>]*\sfill=", re.IGNORECASE)


@dataclass(frozen=True)
class Region:
    """One block of shared markup propagated from the template into every page.

    A region is identified purely by its `label`: the marker pair is derived from
    it, and the canonical copy is read from between that same pair *in the
    template itself*. The template therefore carries every marker a page does,
    which is what makes "copy the template to start a new lesson" produce a page
    that is already opted into both regions.

    Adding a third shared block later means adding one Region and one marker pair
    — nothing else changes.
    """

    label: str

    @property
    def begin(self) -> str:
        return f"<!-- SHARED-{self.label}:BEGIN -->"

    @property
    def end(self) -> str:
        return f"<!-- SHARED-{self.label}:END -->"

    def find(self, text: str) -> str | None:
        """The text between this region's markers, or None if unmarked."""
        start, stop = text.find(self.begin), text.find(self.end)
        if start == -1 or stop == -1 or stop < start:
            return None
        return text[start + len(self.begin):stop].strip("\n")


REGIONS = (Region("CSS"), Region("SCRIPT"))


def stamp(text: str, region: Region, body: str) -> str | None:
    """Replace `region`'s marked span with `body`.

    Returns None when the page does not carry the markers, or when the stamped
    result is byte-identical to what is already there.
    """
    start = text.find(region.begin)
    end = text.find(region.end)
    if start == -1 or end == -1 or end < start:
        return None
    replacement = f"{region.begin}\n{body}\n{region.end}"
    updated = text[:start] + replacement + text[end + len(region.end):]
    return None if updated == text else updated


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="report drift without writing (for CI or a pre-commit hook)")
    args = parser.parse_args()

    # A broken template is exit 2, distinct from exit 1 "drift found", so a CI
    # gate can tell "someone needs to re-stamp" from "the tool itself is broken".
    def fail(message: str) -> NoReturn:
        print(f"error: {message}", file=sys.stderr)
        raise SystemExit(2)

    if not TEMPLATE.exists():
        fail(f"template not found at {TEMPLATE}")
    template_text = TEMPLATE.read_text(encoding="utf-8")

    bodies: dict[str, str] = {}
    for region in REGIONS:
        body = region.find(template_text)
        if body is None:
            fail(f"no {region.label} region found in {TEMPLATE.name} "
                 f"(expected {region.begin} … {region.end})")
        bodies[region.label] = body

    pages = sorted(p for p in DOCS.rglob("*.html") if p != TEMPLATE)

    drifted: dict[str, list[Path]] = {r.label: [] for r in REGIONS}
    skipped: dict[str, list[Path]] = {r.label: [] for r in REGIONS}
    lint_hits: list[tuple[Path, int, str]] = []

    rel = lambda p: p.relative_to(DOCS)  # noqa: E731

    # Lint every page, including the template — the template is where the bug
    # would do the most damage, since it is copied into every lesson.
    for page in sorted(DOCS.rglob("*.html")):
        for lineno, line in enumerate(page.read_text(encoding="utf-8").splitlines(), 1):
            if TEXT_FILL_RE.search(line):
                lint_hits.append((page, lineno, line.strip()[:70]))

    # One read and at most one write per page, even though there are two regions.
    for page in pages:
        original = page.read_text(encoding="utf-8")
        text = original
        for region in REGIONS:
            if region.begin not in text:
                skipped[region.label].append(page)
                continue
            updated = stamp(text, region, bodies[region.label])
            if updated is None:
                continue
            text = updated
            drifted[region.label].append(page)
        if text != original and not args.check:
            page.write_text(text, encoding="utf-8")

    any_drift = False
    for region in REGIONS:
        label = region.label
        opted_in = len(pages) - len(skipped[label])
        for page in skipped[label]:
            print(f"  skip   [{label}] {rel(page)} (no SHARED-{label} marker)")
        if drifted[label]:
            any_drift = True
            verb = "would update" if args.check else "updated"
            for page in drifted[label]:
                print(f"  {verb} [{label}]: {rel(page)}")
            if not args.check:
                print(f"Stamped shared {label} into {len(drifted[label])} page(s).")
        else:
            print(f"{label} up to date across {opted_in} page(s).")

    if lint_hits:
        print(f"\nLINT: {len(lint_hits)} inline fill on an SVG <text> — CSS overrides these,")
        print("      so the colour is silently ignored. Use a class instead")
        print("      (.muted .t-hi .t-ok .t-bad .t-inv .lbl-x/y/z).")
        for page, lineno, snippet in lint_hits:
            print(f"  {rel(page)}:{lineno}  {snippet}")

    if args.check and any_drift:
        total = sum(len(v) for v in drifted.values())
        print(f"\ndrift: {total} region(s) out of date. Run without --check to fix.")

    return 1 if lint_hits or (args.check and any_drift) else 0


if __name__ == "__main__":
    sys.exit(main())
