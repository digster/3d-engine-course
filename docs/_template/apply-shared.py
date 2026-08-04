#!/usr/bin/env python3
"""Keep every page's link to the shared course stylesheet and script correct.

WHY THIS EXISTS
---------------
The shared CSS and the shared page script live in exactly one place each:

    docs/shared/course.css
    docs/shared/course.js

Every page links them from inside a marker pair. This tool stamps those link
tags into every page and verifies them.

It used to do something bigger and worse. Until the extraction, the CSS and the
script were *duplicated* into all ~36 pages, and this script copied both blocks
out of lesson-template.html into each one. That duplication had already drifted
badly before it was centralised: three different highlighter keyword lists, a
CMake highlighter that existed in one lesson only, and a Windows-batch `::`
comment rule that existed in two. Every one of those was a silent mis-render
rather than a crash. The blocks are now linked, not copied, so that whole class
of drift is gone — what remains to police is the *link*.

WHAT CAN STILL GO WRONG, AND WHY --check MATTERS
------------------------------------------------
A wrong or missing href does not raise an error. The page renders, unstyled and
inert, and looks like a page that simply has not been written yet. The relative
prefix depends on the page's depth (`shared/…` at docs/, `../shared/…` at
docs/lessons/), so moving a page between directories breaks it silently. This
tool computes the correct prefix per page, which is exactly the check a human
eye is bad at.

THIS IS NOT A BUILD STEP.
Readers never run it. The published .html files link the shared files straight
off the filesystem, so docs/index.html still opens by double-clicking, offline,
with no server — verified in Chromium, Firefox and WebKit, including the upward
`../shared/` traversal from docs/lessons/. This is an authoring-time tool, run
when pages are added or moved, the same way you would run a formatter.

USAGE
-----
    python3 docs/_template/apply-shared.py           # fix every page's links
    python3 docs/_template/apply-shared.py --check   # verify, change nothing

A page opts into a region by carrying that region's marker pair:

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
from typing import Callable, NoReturn

DOCS = Path(__file__).parent.parent
SHARED = DOCS / "shared"

# The KaTeX loader lives inside the SCRIPT region and is NOT part of the shared
# course.js: both tags carry Subresource Integrity hashes and the second carries
# an inline onload handler, neither of which survives being moved into a linked
# file. So the SCRIPT region is rebuilt as "our script tag + whatever the page
# already had after it", rather than generated whole.
#
# It has to stay *inside* the region regardless: it used to sit just below
# SHARED-SCRIPT:END, so nothing propagated it and every lesson shipped without a
# maths renderer. That was invisible for six lessons because raw TeX is also the
# documented CDN-unreachable fallback. Anything every page needs goes BETWEEN
# the markers.
SCRIPT_TAG_RE = re.compile(r'<script src="[^"]*shared/course\.js"></script>')

# A fill="..." on an SVG <text> is a *presentation attribute*, and CSS always
# beats it. Since the shared stylesheet contains `figure.dia svg text { fill: … }`,
# any inline fill on a <text> is silently ignored — the label just renders in the
# wrong colour, with no warning, and it looks fine at a glance.
#
# This has now bitten twice, so it is a lint rather than a note. Use the classes
# (.muted, .t-hi, .t-ok, .t-bad, .t-inv, .lbl-x/y/z) instead.
TEXT_FILL_RE = re.compile(r"<text[^>]*\sfill=", re.IGNORECASE)


def prefix_for(page: Path) -> str:
    """Relative path from `page`'s directory back up to docs/.

    "" for docs/index.html, "../" for docs/lessons/02-01-lines.html. This is the
    only per-page state in the whole tool, and the one thing a human reliably
    gets wrong when moving a file.
    """
    return "../" * (len(page.relative_to(DOCS).parts) - 1)


@dataclass(frozen=True)
class Region:
    """One marked block whose contents this tool owns.

    A region is identified by its `label`; the marker pair is derived from it.
    `render` builds the region's canonical body for a given page prefix, and is
    handed the region's current body so a region can preserve page content it
    does not own — the SCRIPT region does exactly that for the KaTeX tags.
    """

    label: str
    render: Callable[[str, str], str]

    @property
    def begin(self) -> str:
        return f"<!-- SHARED-{self.label}:BEGIN -->"

    @property
    def end(self) -> str:
        return f"<!-- SHARED-{self.label}:END -->"


def render_css(prefix: str, _current: str) -> str:
    return (
        "\n<!-- The shared course stylesheet, linked rather than inlined. Single source of\n"
        "     truth: docs/shared/course.css. Edit that file; this page carries no copy.\n"
        "     Still no build step - the link resolves straight off the filesystem, so this\n"
        "     page opens by double-clicking, offline. -->\n"
        f'<link rel="stylesheet" href="{prefix}shared/course.css">\n'
    )


def render_script(prefix: str, current: str) -> str:
    """Our script tag, then everything the page already had after it.

    The tail is preserved verbatim rather than generated, because it holds the
    two KaTeX CDN tags (see SCRIPT_TAG_RE above for why they cannot move).
    """
    match = SCRIPT_TAG_RE.search(current)
    tail = current[match.end():] if match else current
    return (
        "\n<!-- The shared page script (theme toggle, TOC scrollspy, syntax highlighter),\n"
        "     linked rather than inlined. Single source of truth: docs/shared/course.js.\n"
        "     A plain classic script at end of body, so it runs exactly where the inline\n"
        "     copy used to: after the DOM is parsed, before KaTeX's deferred render. -->\n"
        f'<script src="{prefix}shared/course.js"></script>'
        + tail
    )


REGIONS = (Region("CSS", render_css), Region("SCRIPT", render_script))


def stamp(text: str, region: Region, prefix: str) -> str | None:
    """Rewrite `region`'s marked span for a page at `prefix`.

    Returns None when the page does not carry the markers, or when the result is
    byte-identical to what is already there.
    """
    start, end = text.find(region.begin), text.find(region.end)
    if start == -1 or end == -1 or end < start:
        return None
    current = text[start + len(region.begin):end]
    updated = text[:start] + region.begin + region.render(prefix, current) + text[end:]
    return None if updated == text else updated


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="report drift without writing (for CI or a pre-commit hook)")
    args = parser.parse_args()

    # A missing shared file is exit 2, distinct from exit 1 "drift found", so a
    # CI gate can tell "someone needs to re-stamp" from "the thing every page
    # depends on is gone".
    def fail(message: str) -> NoReturn:
        print(f"error: {message}", file=sys.stderr)
        raise SystemExit(2)

    for name in ("course.css", "course.js"):
        target = SHARED / name
        if not target.exists():
            fail(f"shared file missing: {target.relative_to(DOCS)}")
        if not target.read_text(encoding="utf-8").strip():
            fail(f"shared file is empty: {target.relative_to(DOCS)}")

    pages = sorted(DOCS.rglob("*.html"))
    drifted: dict[str, list[Path]] = {r.label: [] for r in REGIONS}
    skipped: dict[str, list[Path]] = {r.label: [] for r in REGIONS}
    lint_hits: list[tuple[Path, int, str]] = []

    rel = lambda p: p.relative_to(DOCS)  # noqa: E731

    # Lint every page, including the template — the template is where the bug
    # would do the most damage, since it is copied to start every new lesson.
    for page in pages:
        for lineno, line in enumerate(page.read_text(encoding="utf-8").splitlines(), 1):
            if TEXT_FILL_RE.search(line):
                lint_hits.append((page, lineno, line.strip()[:70]))

    # One read and at most one write per page, even though there are two regions.
    for page in pages:
        original = text = page.read_text(encoding="utf-8")
        prefix = prefix_for(page)
        for region in REGIONS:
            if region.begin not in text:
                skipped[region.label].append(page)
                continue
            updated = stamp(text, region, prefix)
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
            verb = "would fix" if args.check else "fixed"
            for page in drifted[label]:
                print(f"  {verb} [{label}]: {rel(page)}")
            if not args.check:
                print(f"Corrected the shared {label} link in {len(drifted[label])} page(s).")
        else:
            print(f"{label} link correct across {opted_in} page(s).")

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
