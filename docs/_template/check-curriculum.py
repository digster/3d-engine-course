#!/usr/bin/env python3
"""Verify that docs/index.html agrees with itself and with docs/lessons/.

WHY THIS EXISTS
---------------
docs/index.html is the course's shop window and its only public statement of
scope. It is hand-edited, one row at a time, once per lesson — and nothing has
ever checked it. By Lesson 6.8 it had drifted five separate ways at once:

    hero stat said 95 lessons          curriculum prose said 94
    headline said ~436 hours           the nine module subtotals summed to 438
                                       the ninety-five lesson rows summed to 444
    Modules 3, 4 and 6 declared subtotals their own rows contradicted
    Module 5 was badged "in progress" with all eleven lessons published

Two outside reviewers found every one of those from the page alone. None of them
is a hard failure: a wrong total renders perfectly and reads as fact. That is
exactly the class of bug a human eye is bad at and a script is good at.

WHAT IT CHECKS
--------------
  1. Each module's `m-meta` matches its own rows (count and hour sum).
  2. The hero stats and the curriculum prose sentence match the whole table.
  3. Every row badged `published` has a lesson file; every row without a badge
     has none. A row that links a file it does not have is a 404; a file with no
     badge is work that shipped without being announced.
  4. A module whose rows are all published is badged `complete`, not
     `in progress`.
  5. Lesson ids are unique, ordered, and grouped into the module they claim.
  6. Nav chains: each lesson page's prev/next hrefs and titles agree with the
     index's order, and its <title> and .eyebrow carry its own id and module.
  7. Every href pointing inside docs/ resolves to a file that exists.
  8. STATE.md's `docs/lessons/:` manifest lists every published page.
  9. STATE.md's `completed:` roll names every published lesson number.
 10. index.html's <meta name="description"> states the same lesson count the
     table does.
 11. Each published lesson page's own "Time: ~ N hours" matches its index row.

CHECK 8 WAS ADDED BY LESSON 7.4, AND STATE.md ASKED FOR IT BY NAME. That file's
own note beside the list reads: "check-curriculum.py verifies the INDEX against
the filesystem; nothing verifies this list, which is why it is the one that
rots... the durable fix is to have it verify these three sections too, and that
is now the highest-value piece of bookkeeping work outstanding." It had rotted
four times across three sections by 6.18 and was missing FIVE entries — 5.12,
7.1, 7.2, 7.3 — when 7.4 came to append. The list is not the problem;
hand-maintaining a second copy of a fact the filesystem already knows is.

Check 7 is why this tool caught three dead prerequisite links in 6.8 that had
been shipped and read for a week: `02-10-perspective-projection.html` when the
file is `02-10-perspective.html`, and two more of the same shape. A dead link in
a prerequisites list is invisible until a reader clicks it.

THIS IS NOT A BUILD STEP.
Readers never run it. Like apply-shared.py it is an authoring-time tool, run
after a lesson lands or a renumber happens — the moment when the index and the
lessons can disagree.

USAGE
-----
    python3 docs/_template/check-curriculum.py            # report everything
    python3 docs/_template/check-curriculum.py --quiet    # only failures

Exit codes: 0 = everything agrees, 1 = disagreement found, 2 = error.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

DOCS = Path(__file__).parent.parent
INDEX = DOCS / "index.html"
LESSONS = DOCS / "lessons"
STATE = DOCS.parent / "STATE.md"

# One lesson row. The table is hand-written on one line per row, which is what
# makes a line-oriented regex the right tool here rather than an HTML parser:
# the failure we are hunting is a typo'd number, and a parser would happily
# normalise the very whitespace that tells us a row was edited by hand.
ROW_RE = re.compile(
    r'<tr><td class="id">(?P<id>[0-9]+\.[0-9]+[a-z]?)</td>'
    r"<td>(?P<body>.*?)</td>"
    r'<td class="hrs">(?P<hrs>[0-9]+)\s*h</td></tr>'
)
ROW_LINK_RE = re.compile(r'<a href="(?P<href>lessons/[^"]+)">')
ROW_BADGE_RE = re.compile(r'<span class="badge done">published</span>')
ROW_TITLE_RE = re.compile(r'<span class="t">(?:<a [^>]*>)?(?P<title>.*?)(?:</a>)?(?:\s*<span|</span>)')

MODULE_MARK_RE = re.compile(r"<!-- -+ MODULE (?P<num>[0-9]+) -+ -->")
M_NUM_RE = re.compile(r'<span class="m-num">Module (?P<num>[0-9]+)</span>')
M_META_RE = re.compile(r'<span class="m-meta">(?P<n>[0-9]+) lessons · ~(?P<h>[0-9]+) h</span>')
M_BADGE_RE = re.compile(r'<span class="badge (?:done|new)">(?P<status>complete|in progress)</span>')

STAT_RE = re.compile(r'<div class="stat"><span class="n">~?(?P<n>[0-9]+)</span><span class="l">(?P<label>[a-z]+)</span></div>')
PROSE_RE = re.compile(r"^\s*(?P<n>[0-9]+) lessons, ~(?P<h>[0-9]+) hours\.", re.MULTILINE)

# Lesson-page structure. The number a page states about itself lives in three
# places, and they are written by hand at three different moments, so they drift
# independently.
#
# Pages are inconsistent about entities — some write `&mdash;` and `&middot;`,
# others the literal — and ·, and both render identically. Matching the raw text
# therefore reports pure noise, so every page is normalised through
# `unentity()` before these patterns are applied.
TITLE_RE = re.compile(r"<title>(?P<id>[0-9]+\.[0-9]+[a-z]?) — (?P<title>.*?) ·")
EYEBROW_RE = re.compile(r'<div class="eyebrow">Module (?P<mod>[0-9]+) — [^<·]*· Lesson (?P<id>[0-9]+\.[0-9]+[a-z]?)\b')
NAV_RE = re.compile(
    r'<a class="(?P<dir>prev|next)-l" href="(?P<href>[^"]+)">\s*'
    r'<span class="dir">[^<]*</span>\s*'
    r'<span class="ttl">(?P<ttl>[^<]*)</span>'
)
# Any href that points somewhere inside docs/ rather than off to the web.
LOCAL_HREF_RE = re.compile(r'href="(?!https?:|mailto:|#)(?P<href>[^"#]+)(?:#[^"]*)?"')


def unentity(text: str) -> str:
    """Normalise the punctuation entities pages spell inconsistently.

    Only the handful that appear in titles, eyebrows and nav labels — this is a
    comparison aid, not a general HTML unescaper, and it must not touch `&lt;`
    or `&gt;`, which appear inside code listings and would create tags.
    """
    for entity, char in (("&mdash;", "—"), ("&ndash;", "–"), ("&middot;", "·"),
                         ("&amp;", "&"), ("&#39;", "'"), ("&rsquo;", "’"),
                         ("&nbsp;", " ")):
        text = text.replace(entity, char)
    return text


@dataclass
class Lesson:
    lesson_id: str
    title: str
    hours: int
    href: str | None
    published: bool

    @property
    def module(self) -> int:
        return int(self.lesson_id.split(".")[0])

    def sort_key(self) -> tuple[int, int, str]:
        major, _, minor = self.lesson_id.partition(".")
        digits = re.match(r"([0-9]+)([a-z]?)", minor)
        assert digits is not None
        return int(major), int(digits.group(1)), digits.group(2)


@dataclass
class Module:
    num: int
    declared_lessons: int
    declared_hours: int
    status: str | None
    rows: list[Lesson] = field(default_factory=list)


class Report:
    """Collects failures so every check runs, rather than dying on the first."""

    def __init__(self, quiet: bool) -> None:
        self.failures: list[str] = []
        self.quiet = quiet

    def fail(self, check: str, message: str) -> None:
        self.failures.append(f"  {check}: {message}")

    def ok(self, message: str) -> None:
        if not self.quiet:
            print(f"  ok     {message}")


def parse_index(text: str) -> tuple[list[Module], dict[str, int], tuple[int, int] | None]:
    """Split index.html into modules, hero stats, and the prose sentence.

    Modules are delimited by an HTML comment rather than a heading or an id —
    `<!-- ---- MODULE 6 ---- -->` — so that comment is load-bearing structure,
    not decoration. Splitting on it keeps each module's rows with the m-meta
    that claims to describe them, which is the whole point of check 1.
    """
    marks = list(MODULE_MARK_RE.finditer(text))
    modules: list[Module] = []
    for i, mark in enumerate(marks):
        end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
        block = text[mark.start():end]

        num = int(mark.group("num"))
        inner = M_NUM_RE.search(block)
        if inner and int(inner.group("num")) != num:
            # The comment and the rendered heading disagree — the comment is a
            # copy-paste of the module above it, which is exactly how a whole
            # module ends up filed under the wrong number.
            print(f"  FAIL   structure: MODULE {num} comment wraps a block headed "
                  f"'Module {inner.group('num')}'", file=sys.stderr)

        meta = M_META_RE.search(block)
        badge = M_BADGE_RE.search(block)
        module = Module(
            num=num,
            declared_lessons=int(meta.group("n")) if meta else -1,
            declared_hours=int(meta.group("h")) if meta else -1,
            status=badge.group("status") if badge else None,
        )
        for row in ROW_RE.finditer(block):
            body = row.group("body")
            link = ROW_LINK_RE.search(body)
            title = ROW_TITLE_RE.search(body)
            module.rows.append(
                Lesson(
                    lesson_id=row.group("id"),
                    title=(title.group("title").strip() if title else "?"),
                    hours=int(row.group("hrs")),
                    href=link.group("href") if link else None,
                    published=bool(ROW_BADGE_RE.search(body)),
                )
            )
        modules.append(module)

    stats = {m.group("label"): int(m.group("n")) for m in STAT_RE.finditer(text)}
    prose_match = PROSE_RE.search(text)
    prose = (int(prose_match.group("n")), int(prose_match.group("h"))) if prose_match else None
    return modules, stats, prose


def check_totals(modules: list[Module], stats: dict[str, int],
                 prose: tuple[int, int] | None, report: Report) -> None:
    """Checks 1 and 2 — every number the page states about itself."""
    total_lessons = sum(len(m.rows) for m in modules)
    total_hours = sum(row.hours for m in modules for row in m.rows)
    published = sum(1 for m in modules for row in m.rows if row.published)

    for module in modules:
        actual_hours = sum(row.hours for row in module.rows)
        if module.declared_lessons != len(module.rows):
            report.fail("subtotal", f"Module {module.num} declares "
                                    f"{module.declared_lessons} lessons, has {len(module.rows)}")
        if module.declared_hours != actual_hours:
            report.fail("subtotal", f"Module {module.num} declares "
                                    f"~{module.declared_hours} h, rows sum to {actual_hours}")
    if not any(f.startswith("  subtotal") for f in report.failures):
        report.ok(f"{len(modules)} module subtotals match their rows")

    for label, expected in (("lessons", total_lessons), ("modules", len(modules)),
                            ("hours", total_hours), ("published", published)):
        if stats.get(label) != expected:
            report.fail("hero stat", f"'{label}' says {stats.get(label)}, table says {expected}")
    if prose is None:
        report.fail("prose", "could not find the 'N lessons, ~H hours.' sentence")
    elif prose != (total_lessons, total_hours):
        report.fail("prose", f"says {prose[0]} lessons / ~{prose[1]} h, "
                             f"table says {total_lessons} / {total_hours}")
    else:
        report.ok(f"prose and hero stats agree: {total_lessons} lessons, ~{total_hours} h")


META_COUNT_RE = re.compile(r'<meta name="description" content="A (?P<n>[0-9]+)-lesson course')


def check_meta_description(text: str, total_lessons: int, report: Report) -> None:
    """Check 10 — the one number on this page that is not on this page.

    CHECK 10 WAS ADDED BY LESSON 8.1, AND IT HAD BEEN WRONG SINCE THE RESHAPE.
    `<meta name="description">` said "A 94-lesson course" for eight days and nine
    published lessons after the course became 107 lessons, because checks 1 and 2
    read the HERO STATS and the CURRICULUM PROSE — the two places a human looking
    at the rendered page can see — and the description is in the <head>, where
    nobody looks and every search result and link preview does.

    The shape of the miss is worth naming because it is the same one check 9 was
    written for: a fact was recorded in N places, a rule was written to verify
    N-1 of them, and the unverified one was the one that could not be seen.
    ASSUME THERE IS ONE MORE PLACE."""
    match = META_COUNT_RE.search(text)
    if match is None:
        report.fail("meta", 'no \'<meta name="description" content="A N-lesson course...\'')
    elif int(match.group("n")) != total_lessons:
        report.fail("meta", f"description says {match.group('n')} lessons, "
                            f"table says {total_lessons}")
    else:
        report.ok(f"meta description agrees: {total_lessons} lessons")


def check_files_and_badges(modules: list[Module], report: Report) -> list[Lesson]:
    """Checks 3, 4 and 5 — badges, files, ordering, and module grouping."""
    ordered: list[Lesson] = []
    seen: set[str] = set()

    for module in modules:
        for row in module.rows:
            ordered.append(row)
            if row.lesson_id in seen:
                report.fail("duplicate", f"lesson {row.lesson_id} appears twice")
            seen.add(row.lesson_id)
            if row.module != module.num:
                report.fail("grouping", f"lesson {row.lesson_id} sits inside Module {module.num}")

            if row.published:
                if row.href is None:
                    report.fail("badge", f"{row.lesson_id} is badged published but links nothing")
                elif not (DOCS / row.href).exists():
                    report.fail("missing file", f"{row.lesson_id} links {row.href}, which does not exist")
            elif row.href is not None:
                report.fail("badge", f"{row.lesson_id} links {row.href} but carries no published badge")

        published_rows = [r for r in module.rows if r.published]
        if module.rows and len(published_rows) == len(module.rows) and module.status != "complete":
            report.fail("module badge", f"Module {module.num} has all {len(module.rows)} lessons "
                                        f"published but is badged '{module.status}'")
        if published_rows and len(published_rows) < len(module.rows) and module.status == "complete":
            report.fail("module badge", f"Module {module.num} is badged complete with "
                                        f"{len(module.rows) - len(published_rows)} lessons unwritten")

    if ordered != sorted(ordered, key=Lesson.sort_key):
        report.fail("order", "lesson ids are not in ascending order")
    else:
        report.ok(f"{len(ordered)} lesson ids unique, ordered, correctly grouped")

    # Every lesson file should be announced by exactly one row. A file nobody
    # links is work that shipped invisibly.
    linked = {row.href for row in ordered if row.href}
    for path in sorted(LESSONS.glob("*.html")):
        rel = f"lessons/{path.name}"
        if rel not in linked:
            report.fail("orphan", f"{rel} exists but no index row links it")
    return ordered


PAGE_TIME_RE = re.compile(r"<dt>Time</dt>\s*<dd>(?P<dd>.*?)</dd>", re.S)


def check_page_hours(ordered: list[Lesson], report: Report) -> None:
    """Check 11 — the estimate, which is written twice and agreed once.

    CHECK 11 WAS ADDED BY LESSON 8.1, AS THE SECOND INSTANCE OF CHECK 10'S CLASS
    IN A SINGLE SESSION. A lesson's hour estimate is written in the index row's
    <td class="hrs"> and again in the page's own <dt>Time</dt> block, and the two
    are edited at different moments: the page when the lesson is drafted, the row
    when it lands and the real cost is known. 8.1 shipped at 6 h, the index and
    every module subtotal moved with it, and the page went on saying 5. Checks 1
    and 2 could not see it — they only ever read the index, where all four numbers
    agreed with each other. The page is the second copy, and the second copy is
    what a reader looks at before deciding to start.

    IT IS A RANGE TEST, NOT AN EQUALITY, and that is a fact about the corpus
    rather than a softening. Pages spell this field at least four ways —
    "≈ 4 hours", "≈ 4–5 hours", "&asymp; 4&ndash;5 hours", and one with a
    parenthetical after it — so the honest question is whether the index's single
    number falls inside what the page claims. An equality test would have flagged
    fourteen published pages that are not wrong about anything.
    """
    problems = 0
    checked = 0
    for row in ordered:
        if not (row.published and row.href):
            continue
        path = DOCS / row.href
        if not path.exists():
            continue
        match = PAGE_TIME_RE.search(path.read_text(encoding="utf-8"))
        if match is None:
            report.fail("page hours", f"{row.lesson_id} has no '<dt>Time</dt><dd>…</dd>' block")
            problems += 1
            continue
        # Everything up to the word "hours"; a trailing parenthetical is prose.
        head = unentity(match.group("dd")).split("hour")[0]
        stated = [int(n) for n in re.findall(r"[0-9]+", head)]
        checked += 1
        if not stated:
            report.fail("page hours", f"{row.lesson_id} states no number before 'hours'")
            problems += 1
        elif not (min(stated) <= row.hours <= max(stated)):
            span = f"{min(stated)}" if len(set(stated)) == 1 else f"{min(stated)}-{max(stated)}"
            report.fail("page hours", f"{row.lesson_id} says {span} h, "
                                      f"the index row says {row.hours} h")
            problems += 1
    if problems == 0:
        report.ok(f"{checked} pages agree with their index row's hours")


def check_navigation(ordered: list[Lesson], report: Report) -> None:
    """Check 6 — the prev/next chain, and what each page says about itself.

    A lesson's own number is written in four places by hand at four different
    moments: its filename, its <title>, its .eyebrow, and the two nav blocks of
    its neighbours. Nothing has ever compared them.
    """
    published = [row for row in ordered if row.published and row.href]
    by_href = {row.href: i for i, row in enumerate(published)}
    problems = 0

    for i, row in enumerate(published):
        assert row.href is not None
        page = DOCS / row.href
        text = unentity(page.read_text(encoding="utf-8"))

        title = TITLE_RE.search(text)
        if title is None:
            report.fail("page title", f"{row.href} has no '<title>N.M — …' prefix")
            problems += 1
        elif title.group("id") != row.lesson_id:
            report.fail("page title", f"{row.href} titles itself {title.group('id')}, "
                                      f"index says {row.lesson_id}")
            problems += 1

        eyebrow = EYEBROW_RE.search(text)
        if eyebrow is None:
            report.fail("eyebrow", f"{row.href} has no '.eyebrow' lesson line")
            problems += 1
        elif eyebrow.group("id") != row.lesson_id or int(eyebrow.group("mod")) != row.module:
            report.fail("eyebrow", f"{row.href} eyebrow reads Module {eyebrow.group('mod')} / "
                                   f"Lesson {eyebrow.group('id')}, index says "
                                   f"Module {row.module} / Lesson {row.lesson_id}")
            problems += 1

        prev_row = published[i - 1] if i > 0 else None
        next_row = published[i + 1] if i + 1 < len(published) else None
        for nav in NAV_RE.finditer(text):
            want = prev_row if nav.group("dir") == "prev" else next_row
            href, ttl = nav.group("href"), nav.group("ttl").strip()

            if want is None:
                # The frontier: the last written lesson's `next` names the lesson
                # about to be written and falls back to the index for its href.
                continue
            assert want.href is not None
            want_file = want.href.split("/")[-1]
            if href != want_file and href != "../index.html":
                report.fail("nav", f"{row.href} {nav.group('dir')} → {href}, expected {want_file}")
                problems += 1
            elif href == "../index.html" and next_row is not None and nav.group("dir") == "next":
                report.fail("nav", f"{row.href} next falls back to the index, but "
                                   f"{want_file} exists")
                problems += 1
            if not ttl.startswith(want.lesson_id):
                report.fail("nav", f"{row.href} {nav.group('dir')} is labelled '{ttl}', "
                                   f"expected it to name {want.lesson_id}")
                problems += 1

        if row.href not in by_href:  # pragma: no cover - defensive
            continue

    if problems == 0:
        report.ok(f"nav chain, titles and eyebrows consistent across {len(published)} pages")


def check_links(report: Report) -> None:
    """Check 7 — every href pointing inside docs/ resolves.

    Silent by nature: a dead link renders as ordinary blue text and only fails
    when a reader clicks it.
    """
    dead = 0
    for page in sorted(DOCS.rglob("*.html")):
        # _template/lesson-template.html links PREV.html and NEXT.html on
        # purpose: they are the slots an author fills in. A skeleton is allowed
        # to be a skeleton.
        if "_template" in page.parts:
            continue
        text = page.read_text(encoding="utf-8")
        for match in LOCAL_HREF_RE.finditer(text):
            href = match.group("href")
            if not href or href.startswith("data:"):
                continue
            target = (page.parent / href).resolve()
            if not target.exists():
                report.fail("dead link", f"{page.relative_to(DOCS)} → {href}")
                dead += 1
    if dead == 0:
        report.ok("every internal href resolves")


def check_state_manifest(report: "Report") -> None:
    """STATE.md's `docs/lessons/:` list must name every published page.

    A one-way check, deliberately. It reports pages the manifest is MISSING and
    says nothing about entries the manifest has and the filesystem does not,
    because that second case is a deleted lesson — which has never happened and
    which, if it ever does, deserves a human rather than a green tick.
    """
    if not STATE.exists():
        report.fail("STATE manifest", "STATE.md not found")
        return

    text = STATE.read_text(encoding="utf-8")

    # ANCHORED ON THE INDENTED KEY, not on the bare string, and Lesson 7.8 is
    # why. `text.find("docs/lessons/:")` matched a mention of the key in a
    # `completed:` note a thousand lines above the manifest, so the block this
    # walked was a paragraph of prose and the check reported all 82 published
    # pages missing. Loud, and still the wrong failure for the wrong reason — a
    # check that can be broken by writing about it is a check nobody can write
    # about.
    marker = "\n  docs/lessons/:"
    at = text.find(marker)
    if at < 0:
        report.fail("STATE manifest", "no `  docs/lessons/:` block found")
        return
    at += 1   # past the newline, so the slice starts at the key itself
    marker = marker[1:]

    # The manifest is an indented block: take everything up to the next line
    # that starts a new top-level key at the same indentation. `docs/shared/:`
    # is what follows it today; matching on "two spaces then a word then a
    # colon" is what makes this survive the block growing.
    tail = text[at:]
    end = re.search(r"\n  [A-Za-z0-9_/.]+:", tail[len(marker):])
    block = tail[: len(marker) + end.start()] if end else tail

    listed = set(re.findall(r"\b(\d\d-\d\d[a-z]?-[a-z0-9-]+\.html)", block))
    on_disk = {p.name for p in sorted(LESSONS.glob("*.html"))}
    missing = sorted(on_disk - listed)

    if missing:
        report.fail(
            "STATE manifest",
            f"`docs/lessons/:` is missing {len(missing)}: {', '.join(missing)}",
        )
    else:
        report.ok(f"STATE.md lists all {len(on_disk)} published pages")


def check_state_completed(report: "Report") -> None:
    """STATE.md's `completed:` list must name every lesson the index marks done.

    CHECK 9 WAS ADDED BY LESSON 7.8, BECAUSE CHECK 8 WATCHED THE WRONG LIST.
    STATE.md carries the same fact in three places — the `completed:` roll, the
    `capabilities:` notes, and the `files:` manifest — and 7.7 landed in two of
    them. Its page shipped, its capability entry was written, its filename went
    into `docs/lessons/:`, and the `completed:` list simply never gained a line;
    the module marker underneath still read "6 of 8". Check 8 passed the whole
    time, because a manifest of FILENAMES cannot see a missing lesson NUMBER.

    Which is the same structural failure this file was built for and the same one
    the umbrella lint in engine/CMakeLists.txt was built for: two lists that must
    agree, and nothing making them. A new conversation resuming from STATE.md
    would have read "MODULE 7 IN PROGRESS: 6 of 8" and rewritten a published
    lesson.
    """
    if not STATE.exists():
        return   # check 8 has already reported this

    text = STATE.read_text(encoding="utf-8")
    at = text.find("\ncompleted:")
    if at < 0:
        report.fail("STATE completed", "no `completed:` block found")
        return

    tail = text[at + 1:]
    end = re.search(r"\n[a-z_]+:", tail)
    block = tail[: end.start()] if end else tail
    listed = set(re.findall(r"^\s*-\s+(\d+\.\d+[a-z]?)\s", block, re.M))

    published = set()
    for path in sorted(LESSONS.glob("*.html")):
        m = re.match(r"(\d\d)-(\d\d[a-z]?)-", path.name)
        if m:
            published.add(f"{int(m.group(1))}.{m.group(2).lstrip('0') or '0'}")

    missing = sorted(published - listed, key=lambda v: [int(x) for x in v.split(".")[:1]] + [v])
    if missing:
        report.fail(
            "STATE completed",
            f"`completed:` is missing {len(missing)}: {', '.join(missing)}",
        )
    else:
        report.ok(f"STATE.md's completed list names all {len(published)} lessons")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--quiet", action="store_true", help="print failures only")
    args = parser.parse_args()

    if not INDEX.exists():
        print(f"error: {INDEX} not found", file=sys.stderr)
        return 2

    text = INDEX.read_text(encoding="utf-8")
    modules, stats, prose = parse_index(text)
    if not modules:
        print("error: no '<!-- ---- MODULE N ---- -->' markers found in index.html", file=sys.stderr)
        return 2

    report = Report(args.quiet)
    check_totals(modules, stats, prose, report)
    check_meta_description(text, sum(len(m.rows) for m in modules), report)
    ordered = check_files_and_badges(modules, report)
    check_navigation(ordered, report)
    check_page_hours(ordered, report)
    check_links(report)
    check_state_manifest(report)
    check_state_completed(report)

    if report.failures:
        print(f"\n{len(report.failures)} problem(s):")
        for failure in report.failures:
            print(failure)
        return 1

    total_hours = sum(row.hours for m in modules for row in m.rows)
    print(f"\ncurriculum consistent: {len(modules)} modules, "
          f"{sum(len(m.rows) for m in modules)} lessons, ~{total_hours} h.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
