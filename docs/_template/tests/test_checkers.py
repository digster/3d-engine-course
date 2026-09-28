"""Unit tests for the authoring checkers in docs/_template/.

Run from the repository root:

    python3 -m unittest discover docs/_template/tests

Stdlib only, like the checkers themselves.  The checkers' file names contain
hyphens, so they are loaded by path rather than imported.
"""
from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TEMPLATE = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(TEMPLATE))


def load(filename: str, name: str):
    spec = importlib.util.spec_from_file_location(name, os.path.join(TEMPLATE, filename))
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


continuity = load("check-continuity.py", "check_continuity")
builders = load("check-builders.py", "check_builders")
hours = load("estimate-hours.py", "estimate_hours")


def clean_git_env() -> dict[str, str]:
    """The environment for a throwaway repository's git commands, minus every
    variable that would point them at SOMEONE ELSE's repository.  A developer
    who has `GIT_INDEX_FILE` exported (to stage one change set while testing
    another) otherwise has these tests `git add` straight into that index -
    which is exactly how this was found."""
    return {k: v for k, v in os.environ.items()
            if k not in ("GIT_INDEX_FILE", "GIT_DIR", "GIT_WORK_TREE", "GIT_OBJECT_DIRECTORY",
                         "GIT_ALTERNATE_OBJECT_DIRECTORIES", "GIT_COMMON_DIR")}


def builder_esc(text: str) -> str:
    """The escaper every build_NN.py uses (scratch/build_813.py `esc`)."""
    return (text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
                .replace('"', "&quot;").replace("'", "&#x27;"))


class Unescape(unittest.TestCase):
    def test_inverts_the_builder_escaper(self):
        source = '#include <vector>\nauto s = "a & b" + \'c\'; // x < y && y > z &lt; literal'
        self.assertEqual(continuity.unesc(builder_esc(source)), source)

    def test_does_not_decode_legacy_entities_without_semicolons(self):
        # html.unescape would turn "&copy" into "©" - a silent listing rewrite.
        self.assertEqual(continuity.unesc("// &amp;copy 2026"), "// &copy 2026")

    def test_numeric_references_and_hand_written_apostrophes(self):
        self.assertEqual(continuity.unesc("it&#39;s &#x3B1; &#946;"), "it's α β")


class Listings(unittest.TestCase):
    PAGE = (
        '<figure class="listing"><figcaption><span class="path">src/a.hpp &mdash; the core</span>'
        '<span class="lang">C++</span></figcaption><pre><code>snippet</code></pre></figure>'
        '<figure class="listing"><figcaption><span class="path">src/a.hpp</span>'
        '<span class="tag modified">modified</span></figcaption><pre><code>first</code></pre></figure>'
        '<figure class="listing"><figcaption><span class="path">src/a.hpp</span>'
        '<span class="tag modified">modified</span></figcaption>'
        '<pre><code class="lang-cpp">int x = 1 &lt; 2;</code></pre></figure>'
    )

    def test_snippets_are_not_whole_listings(self):
        listings = continuity.page_listings(self.PAGE)
        self.assertEqual([item.path for item in listings], ["src/a.hpp"] * 3)
        self.assertEqual([item.whole for item in listings], [False, True, True])

    def test_last_whole_listing_is_the_lessons_final_state(self):
        self.assertEqual(continuity.whole_listings(self.PAGE), {"src/a.hpp": "int x = 1 < 2;"})

    def test_a_tagged_excerpt_outside_code_listings_is_not_the_file(self):
        """6.9's trap: a nine-line excerpt captioned with the `modified` tag."""
        tagged = ('<figure class="listing"><figcaption><span class="path">{p}</span>'
                  '<span class="tag modified">modified</span></figcaption><pre><code>{c}</code></pre></figure>')
        page = ('<h2 id="impl">4 Implementation</h2>' + tagged.format(p="src/b.cpp", c="excerpt")
                + '<h2 id="listings">5 Complete Code Listings</h2>' + tagged.format(p="src/a.hpp", c="whole")
                + '<h2 id="build">6 Build &amp; Run</h2>' + tagged.format(p="src/c.cpp", c="not here"))
        self.assertEqual(continuity.whole_listings(page), {"src/a.hpp": "whole"})


MINI_TREE = {
    "src/core/clock.hpp": '// src/core/clock.hpp — time.\n#include "math/vec2.hpp"\n',
    "src/gfx/raster.cpp": '// src/gfx/raster.cpp — pixels.\n#include "gfx/raster.hpp"\n#include <cmath>\n',
    "src/math/vec2.hpp": "// src/math/vec2.hpp — maths.\n// not line 1: src/math/vec2.hpp stays\n",
    "src/game/pong.hpp": "// src/game/pong.hpp — the game.\n",
    "src/game/pong.cpp": '// src/game/pong.cpp — rules.\n#include "game/pong.hpp"\n#include "core/input.hpp"\n',
    "src/main.cpp": "int main() {}\n",
}


def published_script() -> str:
    path = os.path.join(REPO, "docs", "lessons", "05-01-the-refactor.html")
    with open(path, encoding="utf-8") as fh:
        page = fh.read()
    return next(item.text for item in continuity.page_listings(page)
                if item.path.startswith("the move"))


class RefactorReplay(unittest.TestCase):
    def test_moves_rewrites_and_retires_src(self):
        tree = dict(MINI_TREE, **{"src/math/extra.cpp": "not moved by any loop\n",
                                  "CMakeLists.txt": '# "core/x.hpp" stays: not under engine/ or demos/\n'})
        result, produced = continuity.apply_refactor_script(tree)
        self.assertEqual(set(result), {
            "engine/include/engine/core/clock.hpp", "engine/src/gfx/raster.cpp",
            "engine/include/engine/math/vec2.hpp", "demos/common/pong.hpp",
            "demos/common/pong.cpp", "demos/sandbox/main.cpp", "CMakeLists.txt"})
        self.assertEqual(produced, set(result) - {"CMakeLists.txt"})
        self.assertEqual(result["engine/include/engine/core/clock.hpp"],
                         "// engine/include/engine/core/clock.hpp — time.\n"
                         "#include <engine/math/vec2.hpp>\n")
        self.assertEqual(result["demos/common/pong.cpp"],
                         '// demos/common/pong.cpp — rules.\n#include "pong.hpp"\n'
                         "#include <engine/core/input.hpp>\n")
        # Line 1 only: a later mention of the old path is prose, left alone.
        self.assertIn("// not line 1: src/math/vec2.hpp stays", result["engine/include/engine/math/vec2.hpp"])
        self.assertEqual(result["CMakeLists.txt"], tree["CMakeLists.txt"])

    def test_the_published_script_does_what_the_replay_models(self):
        """Run 5.1's script for real - BSD sed here on macOS, GNU sed in CI - on a
        miniature pre-refactor tree, and compare with the replay."""
        with tempfile.TemporaryDirectory() as repo:
            run = lambda *a: subprocess.run(a, cwd=repo, check=True, capture_output=True, text=True,
                                           env=clean_git_env())
            run("git", "init", "-q")
            for rel, text in MINI_TREE.items():
                os.makedirs(os.path.join(repo, os.path.dirname(rel)), exist_ok=True)
                with open(os.path.join(repo, rel), "w", encoding="utf-8") as fh:
                    fh.write(text)
            run("git", "add", "-A")
            run("bash", "-c", published_script())
            expected, _ = continuity.apply_refactor_script(dict(MINI_TREE))
            actual = {}
            for root, _, files in os.walk(repo):
                if ".git" in root.split(os.sep):
                    continue
                for name in files:
                    full = os.path.join(root, name)
                    with open(full, encoding="utf-8") as fh:
                        actual[os.path.relpath(full, repo)] = fh.read()
            self.assertEqual(actual, expected)   # also proves no *.bak is left behind

    def test_the_script_is_portable_sed(self):
        script = published_script()
        self.assertNotIn("sed -i ''", script)   # BSD-only; GNU sed reads '' as the script
        self.assertIn("sed -i.bak -E", script)
        self.assertIn("-name '*.bak' -delete", script)


class Differences(unittest.TestCase):
    def test_comment_only_changes_are_told_apart_from_code(self):
        self.assertEqual(continuity.describe_difference("// a\nint x;\n", "// b\nint x;\n"),
                         "2 line(s), comments only")
        self.assertEqual(continuity.describe_difference("int x;\n", "int y;\n"), "2 line(s), code")

    def test_a_hash_is_a_directive_in_cpp_and_a_comment_in_cmake(self):
        before, after = '#include "game/pong.hpp"\n', '#include "pong.hpp"\n'
        self.assertEqual(continuity.describe_difference(before, after, "demos/common/pong.cpp"),
                         "2 line(s), code")
        self.assertEqual(continuity.describe_difference("# a\n", "# b\n", "engine/CMakeLists.txt"),
                         "2 line(s), comments only")

    def test_trailing_newlines_are_not_a_difference(self):
        self.assertEqual(continuity.normalise("a\n\n"), continuity.normalise("a"))


class LessonCommits(unittest.TestCase):
    def test_mangled_subject_still_names_its_lesson(self):
        subject = "git add -A && git commit -F- <<'EOF' Add Lesson 5.10 — Input Mapping"
        self.assertEqual(continuity.COMMIT_RE.search(subject).group("id"), "5.10")
        self.assertEqual(continuity.COMMIT_RE.search("Add Lesson 6.17b — Local Lights").group("id"),
                         "6.17b")
        self.assertIsNone(continuity.COMMIT_RE.search("Reshape roadmap: physics becomes Module 8"))

    def test_every_published_lesson_has_a_commit(self):
        commits = continuity.Git.lesson_commits()
        missing = [l.lesson_id for l in continuity.ordered_lessons() if l.lesson_id not in commits]
        self.assertEqual(missing, [])


class Ratchet(unittest.TestCase):
    def test_new_stale_and_remaining(self):
        new, stale, remaining = continuity.ratchet({"R1 a", "R1 b"}, {"R1 b", "R1 c"})
        self.assertEqual((new, stale, remaining), (["R1 a"], ["R1 c"], 1))


class CourseOwnership(unittest.TestCase):
    def test_sources_only(self):
        owned = continuity.course_owned
        self.assertTrue(owned("CMakeLists.txt"))
        self.assertTrue(owned("engine/CMakeLists.txt"))
        self.assertTrue(owned("shaders/scene.frag.hlsl"))
        self.assertTrue(owned("cmake/Shaders.cmake"))
        self.assertFalse(owned("assets/fonts/OFL.txt"))
        self.assertFalse(owned("scratch/verify_813.cpp"))
        self.assertFalse(owned("docs/CMakeLists.txt"))


class TrackedClone(unittest.TestCase):
    def test_clone_holds_tracked_files_only(self):
        with tempfile.TemporaryDirectory() as repo, tempfile.TemporaryDirectory() as dest:
            run = lambda *a: subprocess.run(a, cwd=repo, check=True, capture_output=True,
                                           env=clean_git_env())
            run("git", "init", "-q")
            os.makedirs(os.path.join(repo, "scratch"))
            for name in ("tracked.txt", "scratch/forced.py", "scratch/untracked.ppm"):
                with open(os.path.join(repo, name), "w") as fh:
                    fh.write(name)
            with open(os.path.join(repo, ".gitignore"), "w") as fh:
                fh.write("scratch/\n")
            run("git", "add", "tracked.txt", ".gitignore")
            run("git", "add", "-f", "scratch/forced.py")
            saved, saved_env = builders.REPO, dict(os.environ)
            builders.REPO = repo
            os.environ.clear()
            os.environ.update(clean_git_env())   # clone_tracked's `git ls-files` must read THIS repo
            try:
                builders.clone_tracked(dest)
            finally:
                builders.REPO = saved
                os.environ.clear()
                os.environ.update(saved_env)
            self.assertTrue(os.path.exists(os.path.join(dest, "tracked.txt")))
            self.assertTrue(os.path.exists(os.path.join(dest, "scratch/forced.py")))
            self.assertFalse(os.path.exists(os.path.join(dest, "scratch/untracked.ppm")))


class HourEstimates(unittest.TestCase):
    """estimate-hours.py's two judgement calls, pinned."""

    def section(self, body: str) -> str:
        return '<h2 id="exercises">Exercises</h2>' + body + "<h2>Recap</h2>"

    def test_a_list_inside_an_exercise_is_not_another_exercise(self):
        # Lesson 2.5's shape: an <h3> per exercise, with a list inside one of them.
        page = self.section("<h3>1</h3><ol><li>a</li><li>b</li><li>c</li></ol><h3>2</h3>")
        self.assertEqual(hours.exercise_count(page), 2)

    def test_only_top_level_items_count(self):
        page = self.section("<ol><li>one<ul><li>x</li><li>y</li></ul></li><li>two</li></ol>")
        self.assertEqual(hours.exercise_count(page), 2)

    def test_numbered_paragraphs_are_the_fallback(self):
        # Lesson 8.3's shape, and no other structure present.
        page = self.section("<p><strong>1. A.</strong></p><p><strong>2. B.</strong></p>")
        self.assertEqual(hours.exercise_count(page), 2)
        # ...but a bold number inside a list item is not a second count.
        page = self.section("<ol><li><p><strong>1. A.</strong></p></li></ol>")
        self.assertEqual(hours.exercise_count(page), 1)

    def test_the_model_is_the_documented_one(self):
        # 9,000 words = 1 h of reading at 150 wpm; 250 code lines = 1 h; 900 comment
        # lines = 1 h; 2 exercises = 1 h; four hours, plus 15%.
        raw, rounded = hours.estimate(9000, 250, 900, 2)
        self.assertAlmostEqual(raw, 4.6)
        self.assertEqual(rounded, 5)
        self.assertEqual(hours.estimate(0, 0, 0, 0)[1], 1)   # never zero hours

    def test_the_refactor_is_priced_as_a_move_not_as_typing(self):
        # Lesson 5.1 extracts ~2,000 lines from src/main.cpp into new files. With the
        # move discount its code count is roughly a third of the raw additions.
        commit = continuity.Git.lesson_commits()["5.1"]
        code, comment = hours.added_lines(commit)
        self.assertLess(code, 1500)
        self.assertGreater(code, 500)


if __name__ == "__main__":
    unittest.main()
