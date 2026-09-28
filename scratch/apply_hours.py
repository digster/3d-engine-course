#!/usr/bin/env python3
"""Apply docs/_template/estimate-hours.py's figures to the published lessons.

One-off, kept for provenance (2026-09-27, post-Module 8 review Phase 4, the
user chose "the plan's model"). Writes, for every published lesson:
  * its index row's <td class="hrs">,
  * its page's <dt>Time</dt> figure — in the page for the hand-authored lessons
    0.1-3.6, and in the `lNN_body_a.html` fragment (then rebuilt) for the rest,
    keeping each page's own spelling (≈ or &asymp;) and any parenthetical;
then recomputes every module subtotal from the rows and the headline total.
Unpublished rows keep their planned figures: the user chose to re-estimate
published lessons only. The prose bands (index, README, CLAUDE.md) are edited
by hand, because they need words, not numbers.

    python3 scratch/apply_hours.py           # dry run: what would change
    python3 scratch/apply_hours.py --apply
"""
from __future__ import annotations

import importlib.util
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)


def _load(name: str, path: str):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


hours = _load("estimate_hours", "docs/_template/estimate-hours.py")
curriculum = sys.modules["check_curriculum"]

TIME_RE = re.compile(r"(<dt>Time</dt>\s*<dd>)(?P<dd>.*?)(</dd>)", re.S)
FIGURE_RE = re.compile(r"(?P<sym>≈|&asymp;)\s*\d+(?:\s*(?:–|&ndash;|-)\s*\d+)?\s*hours?")
HAND_AUTHORED_LAST = (3, 6)


def new_dd(dd: str, h: int) -> str:
    unit = "hour" if h == 1 else "hours"
    out, n = FIGURE_RE.subn(lambda m: f"{m.group('sym')} {h} {unit}", dd, count=1)
    if n != 1:
        raise SystemExit(f"unrecognised Time field: {dd!r}")
    return out


def page_source(lesson_id: str, href: str) -> tuple[str, str | None]:
    """(file holding the Time field, builder or None)."""
    major, minor = (int(x) for x in re.match(r"(\d+)\.(\d+)", lesson_id).groups())
    if (major, minor) <= HAND_AUTHORED_LAST:
        return os.path.join("docs", href), None
    key = f"{major}{minor}"
    return f"scratch/l{key}_body_a.html", f"scratch/build_{key}.py"


def main() -> int:
    apply = "--apply" in sys.argv
    est = {r["id"]: r["estimate"] for r in hours.rows()}
    index = open("docs/index.html", encoding="utf-8").read()

    # ---- 1. rows --------------------------------------------------------------
    changed_rows = 0

    def fix_row(m: re.Match) -> str:
        nonlocal changed_rows
        lid = m.group("id")
        if lid not in est or 'class="badge done">published' not in m.group("body"):
            return m.group(0)
        if int(m.group("hrs")) != est[lid]:
            changed_rows += 1
        return m.group(0)[: m.start("hrs") - m.start()] + str(est[lid]) + m.group(0)[m.end("hrs") - m.start():]

    index = curriculum.ROW_RE.sub(fix_row, index)

    # ---- 2. module subtotals and the headline, recomputed from the rows ------
    parts = re.split(r"(?=<!-- -+ MODULE [0-9]+ -+ -->)", index)
    total = 0
    for i, part in enumerate(parts):
        if not curriculum.MODULE_MARK_RE.match(part):
            continue
        rows = list(curriculum.ROW_RE.finditer(part))
        h = sum(int(r.group("hrs")) for r in rows)
        total += h
        parts[i] = curriculum.M_META_RE.sub(f'<span class="m-meta">{len(rows)} lessons · ~{h} h</span>',
                                            part, count=1)
    index = "".join(parts)
    index = re.sub(r'(<div class="stat"><span class="n">)~[0-9]+(</span><span class="l">hours</span>)',
                   rf"\g<1>~{total}\g<2>", index)
    index = re.sub(r"([0-9]+ lessons, )~[0-9]+( hours\.)", rf"\g<1>~{total}\g<2>", index)

    # ---- 3. page headers ------------------------------------------------------
    edits = []
    for lesson in hours.continuity.ordered_lessons():
        src, builder = page_source(lesson.lesson_id, lesson.href)
        text = open(src, encoding="utf-8").read()
        m = TIME_RE.search(text)
        if m is None:
            raise SystemExit(f"{lesson.lesson_id}: no Time field in {src}")
        dd = new_dd(m.group("dd"), est[lesson.lesson_id])
        if dd != m.group("dd"):
            edits.append((lesson.lesson_id, src, builder, text[: m.start("dd")] + dd + text[m.end("dd"):]))

    print(f"rows changed: {changed_rows}; pages changed: {len(edits)}; new total ~{total} h")
    if not apply:
        return 0
    open("docs/index.html", "w", encoding="utf-8").write(index)
    for lid, src, builder, text in edits:
        open(src, "w", encoding="utf-8").write(text)
        if builder:
            subprocess.run([sys.executable, builder], check=True, stdout=subprocess.DEVNULL)
    print("applied")
    return 0


if __name__ == "__main__":
    sys.exit(main())
