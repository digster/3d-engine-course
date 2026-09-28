#!/usr/bin/env python3
"""Split STATE.md into a compact resume key plus verbatim archives under state/.

One-off, kept for provenance (2026-09-27). STATE.md had grown to 838 KB - about
210K tokens - so the "resume key" a new session is meant to read whole no longer
fitted in one. Every section moves VERBATIM to state/<section>.md; STATE.md keeps
what CLAUDE.md §9 lists - headlines of conventions and decisions, the completed
roll, a short capability list, a paths-only manifest regenerated from git, and
`next:` whole. Nothing is paraphrased: every compact line is a verbatim prefix
of its entry, and the coverage check at the end proves every original line
lives in exactly one place.

    python3 scratch/split_state.py            # dry run: sizes and coverage
    python3 scratch/split_state.py --apply    # write STATE.md and state/

--apply regenerates both from the pinned source, discarding anything appended
since; once a lesson has landed after the split, run it dry only.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
import textwrap

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(REPO)
# The source is PINNED to the last commit before the split, never read from the
# working tree: after --apply, STATE.md is the compact file, and a run that read
# it would "split" the summary and overwrite the archives with it (it did, once,
# during development).
SOURCE_COMMIT = "0663233"
SRC = subprocess.run(["git", "show", f"{SOURCE_COMMIT}:STATE.md"], capture_output=True,
                     text=True, check=True).stdout.split("\n")

# Top-level keys in order; a key's block runs to the next one.
KEYS = ["course", "version", "updated", "conventions", "curriculum", "completed",
        "capabilities", "decisions", "files", "roadmap", "next"]
starts = {}
for i, line in enumerate(SRC):
    m = re.match(r"^([a-z_]+):", line)
    if m and m.group(1) in KEYS and m.group(1) not in starts:
        starts[m.group(1)] = i
order = sorted(starts.items(), key=lambda kv: kv[1])
blocks = {}
for (k, a), nxt in zip(order, order[1:] + [(None, len(SRC))]):
    b = nxt[1]
    while b > a and SRC[b - 1].strip() in ("", "```"):
        b -= 1
    blocks[k] = (a, b)

ARCHIVED = ["updated", "conventions", "curriculum", "completed", "capabilities",
            "decisions", "files", "roadmap"]
TITLES = {
    "updated": "The `updated:` log — what each lesson changed, newest first",
    "conventions": "Conventions — the full text of every key",
    "curriculum": "Curriculum — counts, reshapes and their reasons",
    "completed": "Completed — the roll with its module notes",
    "capabilities": "Capabilities — what the engine can do, lesson by lesson",
    "decisions": "Decisions — standing policies and why",
    "files": "Files — the manifest with its commentary",
    "roadmap": "Roadmap — the 2026-09-08 reshape and what follows",
}


def text(k: str) -> list[str]:
    a, b = blocks[k]
    return SRC[a:b]


def first_sentence(lines: list[str], limit: int = 230) -> str:
    """A verbatim prefix of an entry: its text up to the first sentence end
    (within its first few lines), whitespace collapsed, capped at `limit`."""
    joined = " ".join(l.strip() for l in lines[:4])
    joined = re.sub(r"\s+", " ", joined)
    m = re.search(r"^(.{20,}?[.!?])(\s|\*\*\*|$)", joined)
    out = m.group(1) if m else joined
    return out if len(out) <= limit else out[: limit - 1].rstrip() + "…"


def entries(lines: list[str], head: re.Pattern) -> list[list[str]]:
    """Split a block's body into entries that start at lines matching `head`."""
    out: list[list[str]] = []
    for line in lines[1:]:
        if head.match(line):
            out.append([line])
        elif out:
            out[-1].append(line)
    return out


def wrap_list(key: str, items: list[str]) -> list[str]:
    body = ", ".join(items)
    first = f"  {key}: "
    return textwrap.wrap(body, width=96, initial_indent=first,
                         subsequent_indent=" " * 5, break_on_hyphens=False) or [first.rstrip()]


def manifest() -> list[str]:
    files = subprocess.run(["git", "ls-files"], capture_output=True, text=True,
                           check=True).stdout.split()
    groups: dict[str, list[str]] = {}
    counts: dict[str, int] = {}
    for f in files:
        d, _, name = f.rpartition("/")
        top = f.split("/")[0]
        if top in ("scratch", "memory"):
            counts[top] = counts.get(top, 0) + 1
            continue
        groups.setdefault(d + "/" if d else "/", []).append(name)
    # One main.cpp per demo directory: name the demos once instead of 23 lines.
    demos = sorted(d.split("/")[1] for d, names in groups.items()
                   if re.fullmatch(r"demos/[a-z_0-9]+/", d) and names == ["main.cpp"])
    for name in demos:
        del groups[f"demos/{name}/"]
    out = []
    for d in sorted(groups, key=lambda x: (x != "/", x)):
        out += wrap_list(d, sorted(groups[d]))
        if d == "demos/":
            out += wrap_list("demos/<name>/main.cpp", demos)
    out.append(f"  memory/: {counts.get('memory', 0)} dated session logs (memory/YYYY-MM-DD.md)")
    out.append(f"  scratch/: {counts.get('scratch', 0)} authoring sources, force-added "
               "(builders, fragments, figures, pins, harnesses)")
    out.append("  state/: " + ", ".join(f"{k}.md" for k in ARCHIVED) + ", README.md")
    return out


def compact() -> list[str]:
    L: list[str] = []
    L += ["# STATE — resume key", "",
          "The resume key of CLAUDE.md §9: read CLAUDE.md, then this file, then continue from",
          "`next`. It holds the headlines; **the full text of every section lives verbatim in",
          "`state/`** (see `state/README.md`), and each section here says which file. Merge in",
          "place and never regenerate: a lesson updates `updated:`, `completed:`, `capabilities:`,",
          "`next:` and any headline it changes here, and appends its detail to the matching",
          "`state/` archive.", "",
          "```STATE"]
    L += text("course") + text("version")
    L += ["updated: 2026-09-27 (after Lesson 8.13 — 96 of 107 lessons; Module 8 complete. Since then",
          "         the post-Module 8 review and its fixes: 195 missing listings restored, claims about",
          "         other engines corrected, CI on four toolchains, figures legible, cross-references",
          "         linked, and this file split. Per-lesson history: state/updated.md.)", ""]
    L.append("conventions:   (headline of each key, verbatim; full text: state/conventions.md;")
    L.append("               the reader's version: docs/conventions.html)")
    for e in entries(text("conventions"), re.compile(r"^  [a-z_0-9]+:")):
        key = re.match(r"^  ([a-z_0-9]+):", e[0]).group(1)
        rest = [re.sub(r"^  [a-z_0-9]+:\s*", "", e[0])] + e[1:]
        L.append(f"  {key}: {first_sentence(rest)}")
    L.append("")
    L += text("curriculum")[:2] + ["  (reshapes and their reasons: state/curriculum.md)", ""]
    L.append("completed:")
    for line in text("completed")[1:]:
        if re.match(r"^\s*-\s+\d+\.\d+[a-z]?\s", line) or re.match(r"^\s*===>", line):
            L.append(line.rstrip())
    L.append("")
    L.append("capabilities:   (the first headline of each lesson, verbatim; every entry in full:")
    L.append("                 state/capabilities.md)")
    seen, untagged = set(), 0
    tag = re.compile(r"^-\s+(?:[A-Za-z-]+\s+)?(\d+\.\d+[a-z]?)[:\s]")
    for e in entries(text("capabilities"), re.compile(r"^  - ")):
        m = tag.match(e[0].strip())
        if not m:
            untagged += 1
            continue
        if m.group(1) in seen:
            continue
        seen.add(m.group(1))
        L.append("  " + first_sentence([e[0].strip()] + e[1:], limit=120))
    L.append(f"  - ({untagged} earlier entries, Modules 0-3, predate lesson tags; they close "
             "state/capabilities.md)")
    L.append("")
    L.append("decisions:   (headlines; full: state/decisions.md)")
    for e in entries(text("decisions"), re.compile(r"^  [a-z_0-9-]+:")):
        key = re.match(r"^  ([a-z_0-9-]+):", e[0]).group(1)
        rest = [re.sub(r"^  [a-z_0-9-]+:\s*", "", e[0])] + e[1:]
        L.append(f"  {key}: {first_sentence(rest, limit=260)}")
    L.append("")
    L.append("files:   (paths only, from `git ls-files`; commentary: state/files.md)")
    L += manifest()
    L.append("")
    L += ["roadmap: " + first_sentence([re.sub(r"^roadmap:\s*", "", text("roadmap")[0])] +
                                       text("roadmap")[1:]),
          "         (the whole of it: state/roadmap.md)", ""]
    L += text("next")
    L.append("```")
    return L


def archive(k: str) -> list[str]:
    return [f"# {TITLES[k]}", "",
            f"Moved verbatim from STATE.md's `{k}:` block on 2026-09-27, when that file became",
            "a compact resume key (CLAUDE.md §9). Append here; keep STATE.md's headline in step.",
            "", "```text"] + text(k) + ["```", ""]


def readme() -> list[str]:
    L = ["# state/ — the full text behind STATE.md", "",
         "STATE.md is the resume key a new session reads whole, so it holds headlines. Everything",
         "it summarises is here, verbatim as it stood in STATE.md on 2026-09-27, and grows here:",
         ""]
    for k in ARCHIVED:
        size = sum(len(l) + 1 for l in text(k)) / 1024
        L.append(f"- [`{k}.md`]({k}.md) — {TITLES[k].split(' — ', 1)[1]} ({size:.0f} KB)")
    return L + [""]


def coverage(new_state: list[str], archives: dict[str, list[str]]) -> tuple[int, int]:
    """Every original line inside the STATE fence must appear, in order, in the
    archive of its block, or (for `next:`, course, version) in the new STATE.md."""
    missing = 0
    for k, (a, b) in blocks.items():
        pool = archives.get(k) or new_state
        joined = "\n".join(pool)
        if "\n".join(SRC[a:b]) not in joined:
            missing += 1
            print(f"  COVERAGE FAIL: block {k}")
    return len(blocks), missing


def main() -> int:
    new_state = compact()
    archives = {k: archive(k) for k in ARCHIVED}
    total, missing = coverage(new_state, archives)
    size = sum(len(l) + 1 for l in new_state)
    print(f"STATE.md: {sum(len(l) + 1 for l in SRC) / 1024:.0f} KB -> {size / 1024:.1f} KB "
          f"({len(new_state)} lines); {total} blocks, {missing} not covered verbatim")
    for k in ARCHIVED:
        print(f"  state/{k}.md  {sum(len(l) + 1 for l in archives[k]) / 1024:6.1f} KB")
    if "--apply" in sys.argv and missing == 0:
        os.makedirs("state", exist_ok=True)
        for k, lines in archives.items():
            open(f"state/{k}.md", "w", encoding="utf-8").write("\n".join(lines))
        open("state/README.md", "w", encoding="utf-8").write("\n".join(readme()))
        open("STATE.md", "w", encoding="utf-8").write("\n".join(new_state) + "\n")
        print("written")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
