#!/usr/bin/env python3
"""Estimate each published lesson's hours from what the student actually has to do.

The index's hour figures were written by hand, one lesson at a time, and never
compared with one another: Module 0's lessons run ~4,600 words and Module 8's
~14,800, yet the estimates grew far less than the pages did. This script
replaces the judgement with a stated model, so the numbers can be argued with
and re-derived rather than only disagreed with.

THE MODEL (post-Module 8 review, Phase 4):

    hours = 1.15 × ( prose words / (150 words per minute × 60)
                   + new code lines / 250 per hour
                   + new comment lines / 900 per hour
                   + 0.5 × exercises )

  * PROSE is the page's reading text: everything in the page body except
    listings (<pre>), figures (<svg>), TeX source, navigation, the Complete Code
    Listings section, the Exercises section (priced separately) and Further
    Reading (links, not reading). 150 wpm is careful technical reading, about
    two thirds of a general reader's pace.
  * CODE is what the lesson's own commit ("Add Lesson N.M") added to the files a
    student types — src/, engine/, demos/, shaders/, cmake/ and the root
    CMakeLists.txt — with renames detected, so Lesson 5.1's moves count only
    what changed in them. A comment line is read rather than written, so it is
    priced at 900 per hour against 250 for code; blank lines are free.
  * EXERCISES are counted on the page, at half an hour each: most are guided
    tweaks, a few are engine challenges, and the average is what matters here.
  * The 15% is building, running and debugging — the part no count can see.

Rounded to whole hours, minimum 1, because the index's row format is an integer.

    python3 docs/_template/estimate-hours.py            # the table, estimate vs current
    python3 docs/_template/estimate-hours.py --csv      # the same, machine-readable

Standard library only, like the other checkers. Read-only: it proposes numbers
and changes nothing.
"""
from __future__ import annotations

import html
import importlib.util
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))


def _load(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, os.path.join(HERE, filename))
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module          # dataclasses need the module registered
    spec.loader.exec_module(module)
    return module


curriculum = _load("check_curriculum", "check-curriculum.py")
continuity = _load("check_continuity", "check-continuity.py")

# The coefficients, named once so the table's footer can print them.
WPM = 150.0
CODE_PER_HOUR = 250.0
COMMENT_PER_HOUR = 900.0
HOURS_PER_EXERCISE = 0.5
OVERHEAD = 1.15

TYPED_DIRS = ("src/", "engine/", "demos/", "shaders/", "cmake/")
TYPED_EXT = (".hpp", ".cpp", ".h", ".c", ".inl", ".hlsl", ".cmake", ".txt")
COMMENT_C = re.compile(r"^\s*(//|/\*|\*)")
COMMENT_CMAKE = re.compile(r"^\s*#")

SECTION_RE = re.compile(r"(?=<h2\b)")
DROP_BLOCKS = re.compile(
    r"<(script|style|svg|pre|nav|header|footer)\b.*?</\1>", re.S | re.I)
TEX_RE = re.compile(r"\\\[.*?\\\]|\\\(.*?\\\)", re.S)
TAG_RE = re.compile(r"<[^>]+>")
WORD_RE = re.compile(r"[A-Za-z0-9][A-Za-z0-9'’\-]*")


def typed(path: str) -> bool:
    if path == "CMakeLists.txt":
        return True
    if not path.startswith(TYPED_DIRS):
        return False
    return path.endswith(TYPED_EXT) and (not path.endswith(".txt")
                                         or path.endswith("CMakeLists.txt"))


def added_lines(sha: str) -> tuple[int, int]:
    """(code, comment) lines the commit added to typed files, renames detected.

    MOVED CODE IS NOT TYPED. Lesson 5.1 extracts two thousand lines from
    src/main.cpp into new files, and a student performs that with cut and paste.
    Renames alone cannot see it — the destination files are new — so an added
    line that matches (whitespace-trimmed) a line the same commit deleted from a
    typed file is counted as a move and priced at nothing. It also forgives the
    odd brace that happens to match, which is cheap to type anyway."""
    out = subprocess.run(["git", "diff", "-M", "-U0", "--no-color", f"{sha}^", sha],
                         cwd=REPO, capture_output=True, text=True, check=True).stdout
    added: list[tuple[str, str]] = []
    deleted: dict[str, int] = {}
    old_path = new_path = None
    for line in out.splitlines():
        if line.startswith("--- "):
            old_path = line[6:] if line.startswith("--- a/") else None
            continue
        if line.startswith("+++ "):
            new_path = line[6:] if line.startswith("+++ b/") else None
            continue
        if line.startswith("-") and old_path is not None and typed(old_path):
            body = line[1:].strip()
            if body:
                deleted[body] = deleted.get(body, 0) + 1
        elif line.startswith("+") and new_path is not None and typed(new_path):
            if line[1:].strip():
                added.append((new_path, line[1:]))
    code = comment = 0
    for path, body in added:
        key = body.strip()
        if deleted.get(key, 0) > 0:
            deleted[key] -= 1
            continue
        is_cmake = path.endswith((".cmake", "CMakeLists.txt"))
        if (COMMENT_CMAKE if is_cmake else COMMENT_C).match(body):
            comment += 1
        else:
            code += 1
    return code, comment


def sections(page: str) -> list[str]:
    return SECTION_RE.split(page)


def prose_words(page: str) -> int:
    kept = []
    for sec in sections(page):
        head = sec[:400].lower()
        if ("id=\"exercises\"" in head or "id=\"listings\"" in head
                or "complete code listing" in head or "further reading" in head):
            continue
        kept.append(sec)
    text = DROP_BLOCKS.sub(" ", "".join(kept))
    text = TEX_RE.sub(" ", text)
    text = html.unescape(TAG_RE.sub(" ", text))
    return len(WORD_RE.findall(text))


def exercise_count(page: str) -> int:
    """Exercises on the page, whichever of the course's four spellings it uses.

    In priority order, because the spellings nest: a <div class="exercise"> or
    an <h3> per exercise (a list inside one is not another exercise), else the
    TOP-LEVEL items of the first list, else 8.3's numbered paragraphs."""
    i = page.find('id="exercises"')
    if i < 0:
        return 0
    j = page.find("<h2", i + 10)
    block = page[i:j if j > 0 else len(page)]
    for pattern in (r'class="exercise"', r"<h3\b"):
        n = len(re.findall(pattern, block))
        if n:
            return n
    depth = items = 0
    for tag in re.finditer(r"<(/?)(ol|ul|li)\b", block):
        closing, name = tag.group(1), tag.group(2)
        if name in ("ol", "ul"):
            depth += -1 if closing else 1
        elif not closing and depth == 1:
            items += 1
    return items or len(re.findall(r"<p>\s*<strong>\d+\.", block))

def estimate(words: int, code: int, comment: int, exercises: int) -> tuple[float, int]:
    raw = OVERHEAD * (words / (WPM * 60.0) + code / CODE_PER_HOUR
                      + comment / COMMENT_PER_HOUR + HOURS_PER_EXERCISE * exercises)
    return raw, max(1, int(raw + 0.5))


def rows() -> list[dict]:
    lessons = continuity.ordered_lessons()
    commits = continuity.Git.lesson_commits()
    out = []
    for lesson in lessons:
        page = open(os.path.join(REPO, "docs", lesson.href), encoding="utf-8").read()
        sha = commits.get(lesson.lesson_id)
        code, comment = added_lines(sha) if sha else (0, 0)
        words = prose_words(page)
        ex = exercise_count(page)
        raw, hours = estimate(words, code, comment, ex)
        out.append(dict(id=lesson.lesson_id, title=lesson.title, words=words, code=code,
                        comment=comment, exercises=ex, raw=raw, estimate=hours,
                        current=lesson.hours, commit=(sha or "")[:7]))
    return out


def main() -> int:
    table = rows()
    if "--csv" in sys.argv:
        print("id,words,code,comment,exercises,raw,estimate,current,commit")
        for r in table:
            print(f"{r['id']},{r['words']},{r['code']},{r['comment']},{r['exercises']},"
                  f"{r['raw']:.2f},{r['estimate']},{r['current']},{r['commit']}")
        return 0
    print(f"{'id':>5}  {'words':>6} {'code':>5} {'cmnt':>5} {'ex':>3}  {'est':>4} {'now':>4}  title")
    for r in table:
        flag = "" if abs(r["estimate"] - r["current"]) <= 1 else "  *"
        print(f"{r['id']:>5}  {r['words']:>6} {r['code']:>5} {r['comment']:>5} {r['exercises']:>3}"
              f"  {r['estimate']:>4} {r['current']:>4}  {r['title'][:44]}{flag}")
    est = sum(r["estimate"] for r in table)
    now = sum(r["current"] for r in table)
    print(f"\n{len(table)} published lessons: estimate {est} h, index {now} h "
          f"({'+' if est >= now else ''}{est - now} h).  * = differs by more than 1 h")
    print(f"model: {OVERHEAD} × (words/{WPM:.0f}wpm + code/{CODE_PER_HOUR:.0f}/h "
          f"+ comments/{COMMENT_PER_HOUR:.0f}/h + {HOURS_PER_EXERCISE} h/exercise)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
