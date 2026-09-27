#!/usr/bin/env python3
"""Repair one lesson's R2 continuity omissions (docs/_template/check-continuity.py).

An R2 omission is a course-owned file the lesson's commit added, modified or
renamed, which its page never lists whole - so a student following the pages
never receives the change.  The repair is the one master prompt §8 always
required: list the file, whole, as it stood at that lesson.

For each omitted file this tool:
  1. pins it from the lesson's own commit (`git show <sha>:<path>`);
  2. replays any LATER non-lesson commit that touched the file - the errata
     (2026-09-08's roadmap reshape, the glTF retrofit) - onto the pin with
     `git apply`, because those errata were applied to the published pages too;
  3. registers the pin in the builder (LISTING_META, LISTING_SOURCE, and
     LISTING_LANG for HLSL/CMake), marked with a dated comment;
  4. inserts an `@@LISTING:<path>@@` placeholder in the page's Code Listings,
     before the lesson's own harness (`scratch/...`), headers before sources;
  5. reports every file the page never MENTIONS - those also need a manifest
     row, which is prose and is written by hand.

Usage (from the repository root):
    python3 scratch/repair_continuity.py 615            # show what would change
    python3 scratch/repair_continuity.py 615 --apply    # do it, rebuild the page
Then `git add -f` the new pins and run check-continuity.py --prune.
"""
from __future__ import annotations

import argparse
import importlib.util
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)

_spec = importlib.util.spec_from_file_location(
    "check_continuity", os.path.join(REPO, "docs", "_template", "check-continuity.py"))
cc = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = cc
assert _spec.loader is not None
_spec.loader.exec_module(cc)

STAMP = "# Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole."


# Pages whose existing pins come from a commit that is not their own
# (docs/_template/README.md §15, "a page can be pinned to a commit that is not
# its own"): new pins must come from the SAME commit, or one page would show
# code from two different lessons.  The underlying defect - these two pages show
# code from the lesson after them - is recorded there and not fixed here.
PIN_COMMIT_OVERRIDE = {"5.2": "ea7a05f", "5.4": "d599928"}


def git(*args: str) -> str:
    return subprocess.run(["git", *args], capture_output=True, text=True, check=True).stdout


def lesson_id(nn: str) -> str:
    """'615' -> '6.15', '84' -> '8.4': modules are single digits."""
    return f"{nn[0]}.{int(nn[1:])}"


def errata_commits() -> list[str]:
    """Non-lesson commits that touched course-owned files, oldest first."""
    out = []
    for line in git("log", "--reverse", "--format=%H%x09%s").splitlines():
        sha, _, subject = line.partition("\t")
        if cc.COMMIT_RE.search(subject):
            continue
        files = git("diff-tree", "-r", "--no-commit-id", "--name-only", "--root", sha).split()
        if any(cc.course_owned(f) for f in files):
            out.append(sha)
    return out


def pinned_content(sha: str, path: str, errata: list[str]) -> tuple[str, list[str]]:
    """The file at the lesson's commit, with every later erratum that touched it."""
    text = git("show", f"{sha}:{path}")
    applied = []
    for e in errata:
        is_later = subprocess.run(["git", "merge-base", "--is-ancestor", sha, e]).returncode == 0
        if not is_later or path not in git("diff-tree", "-r", "--no-commit-id", "--name-only", e).split():
            continue
        patch = git("diff", f"{e}^", e, "--", path)
        with tempfile.TemporaryDirectory() as tmp:
            target = os.path.join(tmp, path)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with open(target, "w", encoding="utf-8") as fh:
                fh.write(text)
            proc = subprocess.run(["git", "apply", "-"], cwd=tmp, input=patch, text=True,
                                  capture_output=True)
            if proc.returncode == 0:
                with open(target, encoding="utf-8") as fh:
                    text = fh.read()
                applied.append(f"{e[:7]} applied")
                continue
        # The patch's CONTEXT is from the erratum's era, often many lessons after
        # the pin.  The errata here are one-line renames (Module 8 -> 9, Lesson
        # 6.10 -> 6.12), so apply them line by line: a line that exists once is
        # renamed; a line that does not exist was written later and does not apply.
        text, note = apply_line_pairs(text, patch)
        applied.append(f"{e[:7]} {note}")
    return text, applied


def apply_line_pairs(text: str, patch: str) -> tuple[str, str]:
    lines = text.split("\n")
    done = absent = 0
    for hunk in re.split(r"(?m)^@@.*$", patch)[1:]:
        minus = [l[1:] for l in hunk.split("\n") if l.startswith("-")]
        plus = [l[1:] for l in hunk.split("\n") if l.startswith("+")]
        if len(minus) != len(plus):
            raise SystemExit("erratum hunk is not line-for-line; apply it by hand")
        for old, new in zip(minus, plus):
            hits = [i for i, line in enumerate(lines) if line == old]
            if len(hits) > 1:
                raise SystemExit(f"erratum line occurs {len(hits)} times; apply by hand: {old!r}")
            if hits:
                lines[hits[0]] = new
                done += 1
            else:
                absent += 1
    return "\n".join(lines), f"line-applied {done}, {absent} not yet written at this lesson"


def order_key(path: str) -> tuple[int, str]:
    for rank, prefix in enumerate(("engine/include/", "engine/src/", "shaders/")):
        if path.startswith(prefix):
            return rank, path
    if path.endswith(("CMakeLists.txt", ".cmake")):
        return 3, path
    return (4 if path.startswith("demos/") else 5), path


def lang_of(path: str) -> tuple[str, str] | None:
    if path.endswith(".hlsl"):
        return ("hlsl", "HLSL")
    if path.endswith(("CMakeLists.txt", ".cmake")):
        return ("cmake", "CMake")
    return None


def insert_into_dict(source: str, name: str, lines: list[str]) -> str:
    """Append entries just before the closing brace of a top-level `NAME = {`."""
    empty = f"\n{name} = {{}}\n"
    if empty in source:                      # `LISTING_LANG = {}` on one line
        source = source.replace(empty, f"\n{name} = {{\n}}\n", 1)
    start = source.find(f"\n{name} = {{")
    if start < 0:
        raise SystemExit(f"builder has no top-level {name} dict")
    close = source.find("\n}", start)
    body_lines = source[start + 1:close].split("\n")[1:] if close >= 0 else []
    if close < 0 or any(re.match(r"[A-Za-z_]", line) for line in body_lines):
        # The brace found belongs to something after the dict: refuse rather
        # than write entries into the wrong place (a one-line dict did this).
        raise SystemExit(f"cannot find the end of {name}")
    block = "\n    " + STAMP + "".join(f"\n    {line}" for line in lines)
    return source[:close] + block + source[close:]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("nn", help="builder suffix, e.g. 615")
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()

    lid = lesson_id(args.nn)
    builder = f"scratch/build_{args.nn}.py"
    sha = cc.Git.lesson_commits()[lid]
    lesson = next(l for l in cc.ordered_lessons() if l.lesson_id == lid)
    page_path = os.path.join("docs", lesson.href)
    with open(page_path, encoding="utf-8") as fh:
        page = fh.read()
    listed = cc.whole_listings(page)

    missing = sorted(((s, p) for s, p in cc.Git.changed(sha)
                      if s != "D" and cc.course_owned(p) and p not in listed),
                     key=lambda sp: order_key(sp[1]))
    if not missing:
        print(f"{lid}: nothing to repair")
        return 0

    errata = errata_commits()
    with open(builder, encoding="utf-8") as fh:
        source = fh.read()
    uses_pin_fn = bool(re.search(r"LISTING_SOURCE\s*=\s*\{path:\s*_pin\(path\)", source))
    has_lang = "\nLISTING_LANG = {" in source

    meta, pins, langs, placeholders = [], [], [], []
    print(f"{lid} ({sha[:7]}): {len(missing)} file(s) to list whole")
    pin_sha = git("rev-parse", PIN_COMMIT_OVERRIDE[lid]).strip() if lid in PIN_COMMIT_OVERRIDE else sha
    if pin_sha != sha:
        print(f"  pins come from {pin_sha[:7]}, as this page's existing pins do")
    for status, path in missing:
        tag = "new" if status == "A" else "modified"
        pin = f"scratch/l{args.nn}_{path.replace('/', '_')}"
        text, applied = pinned_content(pin_sha, path, errata)
        mentioned = os.path.basename(path) in page
        print(f"  {tag:8} {path}  ({text.count(chr(10))} lines)"
              + (f"  [{', '.join(applied)}]" if applied else "")
              + ("" if mentioned else "  ** NOT MENTIONED: needs a manifest row **"))
        meta.append(f'"{path}": ("{tag}", "{tag}"),')
        pins.append(f'"{path}": "{pin}",')
        if lang_of(path):
            langs.append(f'"{path}": {lang_of(path)!r},'.replace("'", '"'))
        placeholders.append(path)
        if args.apply:
            if os.path.exists(pin):
                with open(pin, encoding="utf-8") as fh:
                    if fh.read() != text:
                        raise SystemExit(f"{pin} exists with different content - refusing")
            with open(pin, "w", encoding="utf-8") as fh:
                fh.write(text)

    if not args.apply:
        print("  (dry run: nothing written)")
        return 0

    source = insert_into_dict(source, "LISTING_META", meta)
    if not uses_pin_fn:
        source = insert_into_dict(source, "LISTING_SOURCE", pins)
    if langs:
        if not has_lang:
            raise SystemExit("builder has no LISTING_LANG dict; add the HLSL/CMake entries by hand")
        source = insert_into_dict(source, "LISTING_LANG", langs)
    with open(builder, "w", encoding="utf-8") as fh:
        fh.write(source)

    # The fragment that holds the Code Listings placeholders.
    fragments = sorted(f for f in os.listdir("scratch")
                       if re.fullmatch(rf"l{args.nn}_body_[a-z]\.html", f))
    target = next(f for f in fragments
                  if "@@LISTING:" in open(os.path.join("scratch", f), encoding="utf-8").read())
    frag_path = os.path.join("scratch", target)
    with open(frag_path, encoding="utf-8") as fh:
        lines = fh.read().split("\n")
    marks = [i for i, line in enumerate(lines) if "@@LISTING:" in line]
    indent = re.match(r"\s*", lines[marks[0]]).group(0)
    harness = [i for i in marks if "@@LISTING:scratch/" in lines[i]]
    at = harness[0] if harness else marks[-1] + 2
    new_lines = []
    for path in placeholders:
        new_lines += [f"{indent}@@LISTING:{path}@@", ""]
    lines[at:at] = new_lines
    with open(frag_path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))

    proc = subprocess.run([sys.executable, builder], capture_output=True, text=True)
    print(f"  rebuilt {page_path}: exit {proc.returncode}" +
          ("" if proc.returncode == 0 else f"\n{proc.stderr[-600:]}"))
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
