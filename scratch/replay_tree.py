#!/usr/bin/env python3
"""Materialise the student's tree at a point in the course, from the pages.

Lesson 6.17b's session. An inserted lesson is written AFTER the lessons that
follow it in course order, so HEAD is the wrong base for its listings: HEAD
already holds 6.18's font code, Module 7's skinning and Module 8's physics. What
a student has in front of them when they open 6.17b is whatever the pages up to
and including 6.17 told them to type — and that tree is computed here with the
continuity checker's own parser (`whole_listings`, the 5.1 move script, the
index's course order), so the two can never disagree about what "the tree at a
lesson" means.

    python3 scratch/replay_tree.py 6.17 scratch/_t617          # after 6.17
    python3 scratch/replay_tree.py 6.17 scratch/_t617 --check  # + compare to HEAD

Only course-owned sources are written (engine/, demos/, shaders/, cmake/, the
root CMakeLists.txt): the same set `check-continuity.py` judges. `--check`
prints which of them differ from HEAD, which is exactly the list of files whose
6.17b listing must be built on the replayed text rather than on HEAD's.
"""
import argparse
import importlib.util
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
CHECKER = os.path.join(REPO, "docs", "_template", "check-continuity.py")

_spec = importlib.util.spec_from_file_location("check_continuity", CHECKER)
cc = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = cc
_spec.loader.exec_module(cc)


def tree_at(lesson_id):
    """The replayed tree after `lesson_id`, in course order."""
    tree = {}
    for lesson in cc.ordered_lessons():
        with open(os.path.join(REPO, "docs", lesson.href), encoding="utf-8") as fh:
            listings = cc.whole_listings(fh.read())
        if lesson.lesson_id == cc.REFACTOR_LESSON:
            tree, _ = cc.apply_refactor_script(tree)
        tree.update(listings)
        if lesson.lesson_id == lesson_id:
            return tree
    raise SystemExit(f"lesson {lesson_id} is not published in the index")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("lesson")
    ap.add_argument("out")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    tree = tree_at(args.lesson)
    written = 0
    for path, text in sorted(tree.items()):
        if not cc.course_owned(path):
            continue
        dest = os.path.join(args.out, path)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "w", encoding="utf-8") as fh:
            # A listing is embedded without its final newline; a source file
            # has one. `normalise` ignores the difference, a compiler does not
            # care, and a diff against HEAD reads cleaner with it restored.
            fh.write(text if text.endswith("\n") else text + "\n")
        written += 1
    print(f"wrote {written} course-owned files to {args.out}")

    if args.check:
        differ = []
        for path in cc.Git.head_sources():
            head_path = os.path.join(REPO, path)
            with open(head_path, encoding="utf-8", errors="replace") as fh:
                head = fh.read()
            if path not in tree:
                differ.append(f"  absent at {args.lesson}: {path}")
            elif cc.normalise(tree[path]) != cc.normalise(head):
                differ.append(f"  differs from HEAD:   {path}")
        print(f"{len(differ)} course-owned files are not HEAD's at {args.lesson}:")
        print("\n".join(differ))


if __name__ == "__main__":
    main()
