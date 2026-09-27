#!/usr/bin/env python3
"""Assert that a student who copies every listing, in course order, ends up with the engine.

WHY THIS EXISTS
---------------
The course's contract with the student is master prompt §8: "if a file changed,
it appears whole in the lesson's Code Listings section".  Nothing checked it.
On 2026-09-26 a replay of the pages found 25 lessons (5.1-6.16, 8.4, 8.8) whose
commits changed files the page never lists whole - 13 changes never mentioned at
all, 195 named only in the manifest table, 32 shown only as a snippet.  6.12's
manifest marks `shaders/scene.frag.hlsl` modified and lists only the two new
shaders; 6.15 adds 156 lines of image-based lighting to that shader and lists
only the skybox.  Every page looked complete, and `check-builders.py` was green,
because a builder reproduces what a page SAYS - not whether what it says is enough.

THE MODEL
---------
A student starts with nothing, and at each lesson - in course order, inserted
`b` lessons included - copies every listing captioned `new`, `modified` or
`unchanged` over their tree.  At 5.1 they first run that lesson's move script
("the move, and the include rewrite"), which this file replays exactly.  Only
course-owned text sources are compared: `engine/`, `demos/`, `shaders/`,
`cmake/` and the root `CMakeLists.txt`.

THE CHECKS
----------
  R1  FINAL STATE.  After the last published lesson, every course-owned source
      in HEAD must exist in the replayed tree with identical content.  This is
      the invariant a student feels: follow the pages, get the engine.
  R2  PER LESSON.  Every course-owned source a lesson's commit added, modified
      or renamed must be listed whole on that lesson's page - or, at 5.1, be
      produced by the move script with the committed content.  R2 names the
      lesson where an omission HAPPENED; R1 alone would only name the file.
  R3  (--replay, informational) after each lesson, how far the replayed tree is
      from that lesson's own commit.  Errata ported into a page legitimately
      differ from the original commit, so R3 reports and never fails.

KNOWN DEFECTS - A RATCHET, NOT AN AMNESTY
-----------------------------------------
`continuity-known.txt` lists defects that exist and are scheduled for repair.
The check fails on any defect NOT in that file (a regression) and on any entry
that no longer fails (a repair nobody recorded - delete the line).  So the list
can only shrink.  A green run with known defects still prints how many remain:
the course is continuous only when that number is zero.

USAGE
-----
    python3 docs/_template/check-continuity.py                  # R1 + R2
    python3 docs/_template/check-continuity.py --replay         # + R3 per lesson
    python3 docs/_template/check-continuity.py --write-known    # rewrite the baseline
"""
from __future__ import annotations

import argparse
import difflib
import importlib.util
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
INDEX = os.path.join(REPO, "docs", "index.html")
KNOWN_FILE = os.path.join(HERE, "continuity-known.txt")

# Reuse check-curriculum's index parser rather than a second, drifting copy of
# the row grammar.  The file name has a hyphen, so it is loaded by path.
_spec = importlib.util.spec_from_file_location(
    "check_curriculum", os.path.join(HERE, "check-curriculum.py"))
curriculum = importlib.util.module_from_spec(_spec)
assert _spec.loader is not None
# Registered BEFORE it runs: its @dataclasses look their module up in
# sys.modules while being defined, and an unregistered module is None there.
sys.modules[_spec.name] = curriculum
_spec.loader.exec_module(curriculum)

ROOT_DIRS = ("engine/", "demos/", "shaders/", "cmake/")
SOURCE_EXT = (".hpp", ".cpp", ".h", ".c", ".inl", ".hlsl", ".cmake")

LISTING_RE = re.compile(
    r'<figure class="listing[^"]*">\s*<figcaption>(?P<cap>.*?)</figcaption>\s*'
    r'<pre[^>]*><code[^>]*>(?P<code>.*?)</code></pre>', re.S)
PATH_RE = re.compile(r'<span class="path">(?P<path>[^<]+)</span>')
TAG_RE = re.compile(r'<span class="tag (?P<tag>[a-z-]+)"')
# A listing with one of these tags is a WHOLE file; untagged ones are snippets.
WHOLE_TAGS = {"new", "modified", "unchanged"}
# SEARCHED, not anchored: 5.10's commit subject was mangled by a heredoc into
# "git add -A && git commit -F- <<'EOF' Add Lesson 5.10 — ...", and an anchored
# pattern silently skipped that lesson and misread its commit as an erratum.
COMMIT_RE = re.compile(r"\bAdd Lesson (?P<id>[0-9]+\.[0-9]+[a-z]?)\b")

# Lesson 5.1's move script, as published.  Keep in step with that page: if the
# script ever changes, this replay must change with it (the unit tests pin it).
REFACTOR_LESSON = "5.1"
_MOVES = (
    (re.compile(r"src/(core|gfx|math)/([^/]+\.hpp)"), r"engine/include/engine/\1/\2"),
    (re.compile(r"src/(core|gfx)/([^/]+\.cpp)"), r"engine/src/\1/\2"),
    (re.compile(r"src/game/(pong\.(?:hpp|cpp))"), r"demos/common/\1"),
    (re.compile(r"src/main\.cpp"), r"demos/sandbox/main.cpp"),
)
# The script's `sed -E` expressions, in its order: two include rewrites on every
# line, three path-comment rewrites on line 1 only.  Until 2026-09-26 the
# published script had only the first include rule, so a student's moved files
# kept `// src/...` path comments and `pong.cpp` kept `#include "game/pong.hpp"`
# - a header the same script had just moved away, i.e. a broken build at 5.1.
_EVERY_LINE = (
    (re.compile(r'#include "(core|gfx|math)/([a-z0-9_]+\.hpp)"'), r"#include <engine/\1/\2>"),
    (re.compile(r'#include "game/pong\.hpp"'), r'#include "pong.hpp"'),
)
_LINE_ONE = (
    (re.compile(r"^// src/(core|gfx|math)/([a-z0-9_]+\.hpp)"), r"// engine/include/engine/\1/\2"),
    (re.compile(r"^// src/(core|gfx)/([a-z0-9_]+\.cpp)"), r"// engine/src/\1/\2"),
    (re.compile(r"^// src/game/(pong\.[hc]pp)"), r"// demos/common/\1"),
)


def _rewrite(text: str) -> str:
    """One file through the script's sed: per line, in expression order."""
    lines = text.split("\n")
    for i, line in enumerate(lines):
        for pattern, replacement in _EVERY_LINE:
            line = pattern.sub(replacement, line)
        if i == 0:
            for pattern, replacement in _LINE_ONE:
                line = pattern.sub(replacement, line)
        lines[i] = line
    return "\n".join(lines)


def course_owned(path: str) -> bool:
    """A file the student is expected to have, byte for byte, as a source."""
    if path == "CMakeLists.txt" or path.endswith("/CMakeLists.txt"):
        return path == "CMakeLists.txt" or path.startswith(ROOT_DIRS)
    return path.startswith(ROOT_DIRS) and path.endswith(SOURCE_EXT)


def unesc(text: str) -> str:
    """The exact inverse of the builders' `esc()`, plus numeric references.

    Not `html.unescape()`: that decodes the whole HTML5 entity table, including
    legacy entities that need no semicolon, so a listing containing `&copy` or
    `&lt` in a comment would be silently rewritten.  `&amp;` goes last so that a
    literal `&amp;lt;` in source round-trips to `&lt;`, not `<`.
    """
    text = re.sub(r"<[^>]+>", "", text)          # pages carry no markup in code; be safe
    text = (text.replace("&lt;", "<").replace("&gt;", ">")
                .replace("&quot;", '"').replace("&#x27;", "'").replace("&#39;", "'"))
    text = re.sub(r"&#x([0-9a-fA-F]+);", lambda m: chr(int(m.group(1), 16)), text)
    text = re.sub(r"&#([0-9]+);", lambda m: chr(int(m.group(1))), text)
    return text.replace("&amp;", "&")


def normalise(text: str) -> str:
    """Compare files the way a student's copy would be judged: trailing newlines
    are an artefact of how a listing is embedded, not a difference in the code."""
    return text.rstrip("\n")


@dataclass
class Listing:
    path: str
    tag: str | None
    text: str

    @property
    def whole(self) -> bool:
        return self.tag in WHOLE_TAGS


def page_listings(page_html: str) -> list[Listing]:
    """Every captioned listing on a page, in document order."""
    out = []
    for m in LISTING_RE.finditer(page_html):
        path = PATH_RE.search(m.group("cap"))
        if not path:
            continue
        tag = TAG_RE.search(m.group("cap"))
        # Hand-written pages spell the caption separator `&mdash;`, builders
        # write the literal; either way, what follows it names a snippet.
        raw = path.group("path").replace("&mdash;", "—").replace("&nbsp;", " ")
        out.append(Listing(path=unesc(raw).split(" — ")[0].strip(),
                           tag=tag.group("tag") if tag else None,
                           text=unesc(m.group("code"))))
    return out


_SECTION_RE = re.compile(r"(?=<h2\b)")
_LISTINGS_H2 = re.compile(r"<h2\b[^>]*>(?:(?!</h2>).)*complete code listings", re.S | re.I)


def listings_section(page_html: str) -> str:
    """The page's Complete Code Listings section - master prompt §6.7, "every
    file created or modified this lesson, in full".  Only listings in it count
    as whole: 6.9 captioned a nine-line excerpt of `shadow.cpp` with the
    `modified` tag in its Implementation section, and a tag-only rule took that
    excerpt for the file and hid the omission of the real one.  A page without
    the section (none today) falls back to the whole page."""
    for part in _SECTION_RE.split(page_html):
        if _LISTINGS_H2.match(part):
            return part
    return page_html


def whole_listings(page_html: str) -> dict[str, str]:
    """Path -> text of the LAST whole listing of each path in the Code Listings
    section.  Early pages list a file several times as it grows; the last one
    is the state the lesson ends in."""
    result: dict[str, str] = {}
    for listing in page_listings(listings_section(page_html)):
        if listing.whole:
            result[listing.path] = listing.text
    return result


def apply_refactor_script(tree: dict[str, str]) -> tuple[dict[str, str], set[str]]:
    """Replay Lesson 5.1's `git mv` loops and its include rewrite.

    Returns the new tree and the set of paths the script produced.  Anything
    left under `src/` afterwards is dropped: 5.1 retires the directory, and a
    file the script did not move is one the student no longer builds.
    """
    result: dict[str, str] = {}
    produced: set[str] = set()
    for path, text in tree.items():
        target = None
        for pattern, replacement in _MOVES:
            if pattern.fullmatch(path):
                target = pattern.sub(replacement, path)
                break
        if target is not None:
            result[target] = text
            produced.add(target)
        elif not path.startswith("src/"):
            result[path] = text
    # `find engine demos -name '*.[hc]pp'`: every .hpp/.cpp under the two roots.
    for path in result:
        if path.startswith(("engine/", "demos/")) and path.endswith((".hpp", ".cpp")):
            result[path] = _rewrite(result[path])
    return result, produced


# What a comment looks like depends on the language: `#` opens a comment in
# CMake but a preprocessor DIRECTIVE in C++ and HLSL.  Treating `#include` as a
# comment once filed a build-breaking include as cosmetic.
_C_COMMENT = re.compile(r"^\s*(//|$)")
_CMAKE_COMMENT = re.compile(r"^\s*(#|$)")


def describe_difference(ours: str, theirs: str, path: str = "") -> str:
    """How two versions of a file differ, in the terms a repair needs: how many
    lines, and whether any of them is code (vs. only comments or blank lines)."""
    comment = _CMAKE_COMMENT if path.endswith(("CMakeLists.txt", ".cmake")) else _C_COMMENT
    a, b = normalise(ours).split("\n"), normalise(theirs).split("\n")
    changed = [line[1:] for line in difflib.unified_diff(a, b, lineterm="", n=0)
               if line[:1] in "+-" and not line.startswith(("+++", "---"))]
    kind = "comments only" if all(comment.match(line) for line in changed) else "code"
    return f"{len(changed)} line(s), {kind}"


class Git:
    """A single long-lived `git cat-file --batch`: thousands of blob reads in
    the time a handful of `git show` subprocesses would take."""

    def __init__(self) -> None:
        self.proc = subprocess.Popen(["git", "cat-file", "--batch"], cwd=REPO,
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    def read(self, rev: str, path: str) -> str | None:
        assert self.proc.stdin and self.proc.stdout
        self.proc.stdin.write(f"{rev}:{path}\n".encode())
        self.proc.stdin.flush()
        header = self.proc.stdout.readline().decode()
        if header.endswith("missing\n"):
            return None
        size = int(header.split()[2])
        data = self.proc.stdout.read(size)
        self.proc.stdout.read(1)  # the trailing LF cat-file adds
        return data.decode("utf-8", errors="replace")

    @staticmethod
    def lesson_commits() -> dict[str, str]:
        out = subprocess.run(["git", "log", "--format=%H%x09%s"], cwd=REPO,
                             capture_output=True, text=True, check=True).stdout
        commits: dict[str, str] = {}
        for line in out.splitlines():
            sha, _, subject = line.partition("\t")
            m = COMMIT_RE.search(subject)
            if m:
                commits.setdefault(m.group("id"), sha)   # newest wins, if ever repeated
        return commits

    @staticmethod
    def changed(sha: str) -> list[tuple[str, str]]:
        """(status letter, path) for every file a commit added, modified,
        renamed or deleted.  Renames report their NEW path."""
        out = subprocess.run(["git", "diff-tree", "-r", "--no-commit-id", "--name-status",
                              "-M", "--root", sha], cwd=REPO,
                             capture_output=True, text=True, check=True).stdout
        rows = []
        for line in out.splitlines():
            parts = line.split("\t")
            rows.append((parts[0][0], parts[-1]))
        return rows

    @staticmethod
    def head_sources() -> list[str]:
        out = subprocess.run(["git", "ls-files"], cwd=REPO, capture_output=True,
                             text=True, check=True).stdout
        return [p for p in out.splitlines() if course_owned(p)]


@dataclass
class Findings:
    r1: set[str] = field(default_factory=set)       # "R1 <path>"
    r2: set[str] = field(default_factory=set)       # "R2 <lesson> <path>"
    notes: list[str] = field(default_factory=list)


def ordered_lessons() -> list:
    with open(INDEX, encoding="utf-8") as fh:
        modules, _, _ = curriculum.parse_index(fh.read())
    lessons = [row for module in modules for row in module.rows if row.published and row.href]
    return sorted(lessons, key=lambda lesson: lesson.sort_key())


def run(replay: bool) -> Findings:
    git = Git()
    commits = Git.lesson_commits()
    found = Findings()
    tree: dict[str, str] = {}
    last_listed: dict[str, str] = {}

    for lesson in ordered_lessons():
        with open(os.path.join(REPO, "docs", lesson.href), encoding="utf-8") as fh:
            listings = whole_listings(fh.read())
        scripted: set[str] = set()
        if lesson.lesson_id == REFACTOR_LESSON:
            tree, scripted = apply_refactor_script(tree)
            for path in scripted:
                last_listed[path] = f"{REFACTOR_LESSON} (moved by its script)"
        tree.update(listings)
        for path in listings:
            last_listed[path] = lesson.lesson_id

        sha = commits.get(lesson.lesson_id)
        if sha is None:
            found.notes.append(f"{lesson.lesson_id}: no 'Add Lesson' commit - R2 skipped")
            continue
        for status, path in Git.changed(sha):
            if status == "D" or not course_owned(path):
                continue
            # Presence is R2's question; whether the CONTENT is right is R1's,
            # asked once at the end, where errata ported into earlier pages
            # cannot be mistaken for omissions.
            if path in listings or path in scripted:
                continue
            found.r2.add(f"R2 {lesson.lesson_id} {path}")

        if replay:
            drift = sum(1 for path, text in tree.items() if course_owned(path)
                        and normalise(text) != normalise(git.read(sha, path) or ""))
            found.notes.append(f"{lesson.lesson_id}: replay differs from its commit in {drift} file(s)")

    for path in Git.head_sources():
        with open(os.path.join(REPO, path), encoding="utf-8", errors="replace") as fh:
            head = fh.read()
        if path not in tree:
            found.r1.add(f"R1 {path}")
            found.notes.append(f"R1 {path}: never listed whole in any lesson")
        elif normalise(tree[path]) != normalise(head):
            found.r1.add(f"R1 {path}")
            found.notes.append(f"R1 {path}: last listed whole in {last_listed.get(path, '?')}; "
                               f"HEAD differs by {describe_difference(tree[path], head, path)}")
    return found


def read_known() -> set[str]:
    if not os.path.exists(KNOWN_FILE):
        return set()
    with open(KNOWN_FILE, encoding="utf-8") as fh:
        return {line.split("#")[0].strip() for line in fh
                if line.split("#")[0].strip()}


def write_known(defects: set[str]) -> None:
    header = (
        "# Known continuity defects - a RATCHET (see check-continuity.py).\n"
        "# Each line is a defect that exists and is scheduled for repair.\n"
        "# Delete a line when it is repaired; the check fails if you forget.\n"
        "# R1 <path>             : the replayed tree's final file differs from HEAD\n"
        "# R2 <lesson> <path>    : the lesson's commit changed it; the page never lists it whole\n")
    order = lambda d: (d.split()[0], [int(x) if x.isdigit() else x
                                      for x in re.split(r"[.\s]", d.split(" ", 1)[1])])
    with open(KNOWN_FILE, "w", encoding="utf-8") as fh:
        fh.write(header + "".join(line + "\n" for line in sorted(defects, key=order)))


def ratchet(defects: set[str], known: set[str]) -> tuple[list[str], list[str], int]:
    """Split findings against the known list: (new regressions, stale entries,
    number of known defects still present).  Either list non-empty is a failure."""
    return sorted(defects - known), sorted(known - defects), len(defects & known)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--replay", action="store_true",
                        help="also report, per lesson, how far the replay is from its commit")
    parser.add_argument("--write-known", action="store_true",
                        help="record every current defect as known (baseline); use sparingly")
    parser.add_argument("--prune", action="store_true",
                        help="delete known entries that no longer fail (the ratchet's "
                             "only allowed direction); never adds one")
    parser.add_argument("-v", "--verbose", action="store_true", help="print every finding")
    args = parser.parse_args()

    found = run(args.replay)
    defects = found.r1 | found.r2
    if args.write_known:
        write_known(defects)
        print(f"wrote {len(defects)} known defect(s) to {os.path.relpath(KNOWN_FILE, REPO)}")
        return 0
    if args.prune:
        known = read_known()
        repaired = known - defects
        write_known(known & defects)
        print(f"pruned {len(repaired)} repaired entr{'y' if len(repaired) == 1 else 'ies'}; "
              f"{len(known & defects)} known defect(s) remain")
        for item in sorted(repaired):
            print(f"  repaired  {item}")
        return 0

    new, stale, remaining = ratchet(defects, read_known())
    if args.verbose or args.replay:
        for note in found.notes:
            print(f"  note   {note}")
    for item in new:
        print(f"  FAIL   new defect: {item}")
    for item in stale:
        print(f"  FAIL   repaired but still listed as known - delete the line: {item}")

    print(f"\nR1 final-state defects: {len(found.r1)}   R2 per-lesson omissions: {len(found.r2)}")
    if remaining:
        print(f"{remaining} KNOWN defect(s) remain in {os.path.relpath(KNOWN_FILE, REPO)}; "
              "the course is continuous only when this is 0.")
    if new or stale:
        return 1
    print("continuity: no regressions" + ("" if remaining else " - and no known defects"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
