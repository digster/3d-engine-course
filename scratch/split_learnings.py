#!/usr/bin/env python3
"""Split LEARNINGS.md into a readable index plus verbatim archives under learnings/.

One-off, kept for provenance (2026-09-27), the companion of split_state.py.
LEARNINGS.md had reached 569 KB and 294 sections - far past what the global rule
"read this file and remember the learnings" can mean. Every section moves
VERBATIM, in its original order, to exactly one archive:

  learnings/foundations.md   everything before the first lesson-tagged section:
                             SDL3/SDL_GPU facts, the conventions, page verification
  learnings/module-N.md      sections tagged "(Lesson N.M)" in their heading, or
                             added by an "Add Lesson N.M" commit (git blame)
  learnings/tooling.md       sections added by a non-lesson commit: the docs
                             pipeline, builders, sweeps, repairs

LEARNINGS.md keeps its preamble, a hand-written list of the hazards that recur
(CURATED below), and every section heading grouped by archive.

    python3 scratch/split_learnings.py            # dry run: sizes and coverage
    python3 scratch/split_learnings.py --apply    # write LEARNINGS.md and learnings/

The source is pinned to the last commit before the split; --apply discards any
learning appended since, so after the first lesson lands, run it dry only.
"""
from __future__ import annotations

import collections
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
SOURCE_COMMIT = "0663233"


def git(*args: str) -> str:
    return subprocess.run(["git", *args], capture_output=True, text=True, check=True).stdout


SRC = git("show", f"{SOURCE_COMMIT}:LEARNINGS.md").split("\n")


def blame_subjects() -> list[str]:
    """The subject of the commit that last touched each line (0-based)."""
    out, subject, cur = [], {}, None
    for line in git("blame", "--line-porcelain", SOURCE_COMMIT, "--", "LEARNINGS.md").split("\n"):
        m = re.match(r"^([0-9a-f]{40}) \d+ \d+", line)
        if m:
            cur = m.group(1)
        elif line.startswith("summary "):
            subject[cur] = line[8:]
        elif line.startswith("\t"):
            out.append(subject.get(cur, ""))
    return out


STARTS = [i for i, l in enumerate(SRC) if l.startswith("## ")]
PREAMBLE = SRC[: STARTS[0]]
FIRST_TAGGED = next(i for i in STARTS if re.search(r"\(Lesson \d+\.\d+", SRC[i]))
LESSON_IN_HEADING = re.compile(r"\bLessons? (\d+)\.\d+")
LESSON_COMMIT = re.compile(r"\bAdd Lesson (\d+)\.\d+")

TITLES = {
    "foundations": "Foundations — SDL3, SDL_GPU, the conventions, and verifying a page",
    "tooling": "Tooling — the docs pipeline, builders, sweeps and repairs",
}
for n in range(2, 9):
    TITLES[f"module-{n}"] = f"Module {n} — learnings from its lessons"
ORDER = ["foundations"] + [f"module-{n}" for n in range(2, 9)] + ["tooling"]


def assign() -> dict[str, list[tuple[int, int]]]:
    """Each section (start, end) to its archive, preserving file order."""
    subjects = blame_subjects()
    out: dict[str, list[tuple[int, int]]] = collections.defaultdict(list)
    for a, b in zip(STARTS, STARTS[1:] + [len(SRC)]):
        if a < FIRST_TAGGED:
            key = "foundations"
        elif m := LESSON_IN_HEADING.search(SRC[a]):
            key = f"module-{m.group(1)}"
        elif m := LESSON_COMMIT.search(subjects[a]):
            key = f"module-{m.group(1)}"
        else:
            key = "tooling"
        out[key].append((a, b))
    return out


# The hazards that recur: each has bitten more than once, or cost the most when it
# did. Written by hand, one line each, naming the archive and the section heading
# so a reader can find the full entry with a search.
CURATED = """\
**SDL and the GPU**
- **Every SDL_GPU convention is verified, never assumed** — winding is per-pipeline, not
  SDL-wide, and the NDC-parity decision is the highest-leverage choice in the course.
  *foundations: "SDL_GPU conventions — verified, not assumed", "The NDC-parity decision".*
- **Internet snippets are SDL2** — `SDL_CreateWindow` has no x/y, `SDL_Init` returns `bool`,
  `keysym` is gone. Check the header at the pinned tag. *foundations: "SDL3 API signatures…".*
- **SDL's asserts key off `__OPTIMIZE__`, not `NDEBUG`** — `-O2` alone silences `SDL_assert`;
  gate your own macros on `SDL_ASSERT_LEVEL`. *tooling: "Course-infrastructure facts (docs/, 2026-08-26)".*
- **shadercross has no releases; pin a commit** — and a build without DXC cannot read HLSL.
  *foundations: "SDL_shadercross has no releases — pin a commit".*

**Numerics**
- **NaN passes every guard you did not write for it** — `std::clamp` keeps it, it never wins a
  maximum, and "finite" is not "right". *module-3: "`std::clamp` cannot remove a NaN"; module-7:
  "A NaN never wins a maximum" and the entry on "finite".*
- **Cancellation against 1.0** — `sqrt(1 - sin*sin)` and its relatives are exact on paper and
  lose every digit that mattered; `acos` of a unit-vector dot product cannot resolve an angle
  under 0.036° in float — use the chord. *module-7: "Cancellation against 1.0…"; module-8:
  "`acos` has a resolution floor, and it is 0.036 degrees".*
- **Symmetric or hand-picked test data agrees with the code by construction** — convention bugs
  hide behind it. *module-8: "Convention bugs hide behind symmetric test data",
  "Hand-picked test data agrees with the code by construction".*

**Measurement**
- **Timing loops measure the compiler** unless the whole result is consumed and the input
  varies — otherwise it deletes or hoists the work; an accumulator that is overwritten measures
  nothing; alternate the arms or you time the machine.
  *module-7: "A timing loop measures the compiler unless you stop it twice"; module-8:
  "Alternate the arms, or you are timing the machine".*
- **A control must fail when the thing under test is broken** — one that fires on the healthy
  case, or whose degenerate case is a pass, is not a control. *module-6, module-7, module-8.*
- **Write the prediction down first; the measurement may refuse it** — sections written to
  confirm a claim have disproved it again and again, most of all in Module 8. *module-3: "Predict,
  then measure — and write the prediction down where it can be wrong", "The measurement is allowed
  to prove *you* wrong".*
- **A harness cannot check a claim about somebody else's engine** — cite the header or source
  line, never "every engine does X". *tooling.*

**Bookkeeping and the pages**
- **One fact in N places will be wrong in one** — and the check you add watches the wrong list
  until it is anchored. *module-7: "The same fact in three places…"; module-8: "…and the fourth
  place is the one nobody can see"; module-7: "A check that greps its own corpus…".*
- **A page can look complete and still not reproduce the engine** — a tagged excerpt reads as
  the whole file; `check-continuity.py` exists because of it. *tooling: "A page can look complete…",
  "A tagged excerpt is read as the whole file".*
- **Generators rot silently** — a crashing builder writes nothing, and "the file did not change"
  is not evidence; a zero-byte generated file is a silent regression. *tooling: "The builder
  reproducibility repair"; module-8: "A zero-byte generated file is a silent regression".*

**Figures and the browser**
- **CSS beats SVG presentation attributes** — `svg text { fill }` overrides `fill="…"`; theme
  text and strokes with classes. *foundations: "Verifying a lesson page"; module-8: "Theme SVG
  strokes with a class, never a literal colour".*
- **`getBBox()` is local space** — use `getBoundingClientRect()` for spill and collision checks.
  *foundations: "Verifying a lesson page".*
- **Figure numbers and filenames follow page order**, and a moved label lands on the next
  obstacle. *module-6, module-7.*
- **Check at 390 px, not only 1280** — a figure defect can be invisible at the width it was
  authored; `scroll-behavior: smooth` makes fixed-delay anchor tests lie. *module-7, module-8,
  tooling.*

**Shell and git**
- **zsh does not word-split an unquoted `$var`** — `set -- $spec` gets one argument. *module-8.*
- **`GIT_INDEX_FILE` leaks into every child `git`, and `git reset` empties a staged index** —
  set it per command, never export it; `git write-tree` before anything risky. *tooling.*
- **A move script is code** — `sed -i ''` only means "no backup" to BSD sed; a published script
  gets a test that runs it with the machine's own tools. *tooling.*
"""


def heading_index(groups: dict[str, list[tuple[int, int]]]) -> list[str]:
    L = []
    for key in ORDER:
        secs = groups.get(key, [])
        if not secs:
            continue
        L += [f"### [`learnings/{key}.md`](learnings/{key}.md) — {TITLES[key].split(' — ', 1)[1]} "
              f"({len(secs)})", ""]
        L += ["- " + SRC[a][3:].strip() for a, _ in secs]
        L.append("")
    return L


def head(groups: dict[str, list[tuple[int, int]]]) -> list[str]:
    body = list(PREAMBLE)
    while body and body[-1].strip() in ("", "---"):
        body.pop()
    return body + [
        "",
        "**This file is the index; the entries live in [`learnings/`](learnings/).** It was split",
        "on 2026-09-27, when it had reached 569 KB — far past what \"read it before writing\" can",
        "mean. Every section moved there verbatim; nothing was rewritten. Read the hazards below",
        "before any lesson, then the full entries for whatever the lesson touches (search the",
        "archive for the heading).",
        "",
        "**Adding a learning:** append the section to `learnings/module-N.md` for the module being",
        "written (or `learnings/tooling.md` for the docs pipeline), and add its heading to the",
        "list at the bottom of this file. If it is a hazard that has now bitten twice, add a line",
        "to the list just below.",
        "",
        "---",
        "",
        "## The hazards that recur — read these first",
        "",
    ] + CURATED.rstrip("\n").split("\n") + [
        "",
        "---",
        "",
        "## Every entry, by archive",
        "",
        "Headings only, in their original order. Each archive keeps its sections' `###`",
        "sub-headings, which are not repeated here.",
        "",
    ] + heading_index(groups)


def archive(key: str, secs: list[tuple[int, int]]) -> list[str]:
    L = [f"# {TITLES[key]}", "",
         "Moved verbatim from LEARNINGS.md on 2026-09-27, in the original order; LEARNINGS.md",
         "is now the index. Append new sections at the end, and add each heading there.", "",
         "---", ""]
    for a, b in secs:
        L += SRC[a:b]
    while L and L[-1] == "":
        L.pop()
    return L + [""]


def main() -> int:
    groups = assign()
    archives = {k: archive(k, groups[k]) for k in ORDER if groups.get(k)}
    new_head = head(groups)
    # Coverage: every original line after the preamble is in some archive, with
    # multiplicity, and the sections in all archives together are exactly the originals.
    need = collections.Counter(l for l in SRC[STARTS[0]:] if l.strip())
    have = collections.Counter(l for lines in archives.values() for l in lines if l.strip())
    missing = sum((need - have).values())
    total_secs = sum(len(v) for v in groups.values())
    size = sum(len(l) + 1 for l in new_head)
    print(f"LEARNINGS.md: {sum(len(l) + 1 for l in SRC) / 1024:.0f} KB -> {size / 1024:.1f} KB; "
          f"{total_secs}/{len(STARTS)} sections archived, {missing} lines not covered")
    for k, lines in archives.items():
        print(f"  learnings/{k}.md  {len(groups[k]):3d} sections  "
              f"{sum(len(l) + 1 for l in lines) / 1024:6.1f} KB")
    if "--apply" in sys.argv and missing == 0 and total_secs == len(STARTS):
        os.makedirs("learnings", exist_ok=True)
        for k, lines in archives.items():
            open(f"learnings/{k}.md", "w", encoding="utf-8").write("\n".join(lines))
        open("LEARNINGS.md", "w", encoding="utf-8").write("\n".join(new_head).rstrip("\n") + "\n")
        print("written")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
