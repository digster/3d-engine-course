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


class RefactorReplay(unittest.TestCase):
    def test_moves_rewrites_includes_and_retires_src(self):
        tree = {
            "src/core/clock.hpp": '#include "math/vec2.hpp"\n',
            "src/gfx/raster.cpp": '#include "gfx/raster.hpp"\n#include <cmath>\n',
            "src/math/extra.cpp": "not moved by any loop\n",
            "src/game/pong.cpp": '#include "core/input.hpp"\n',
            "src/main.cpp": "int main() {}\n",
            "CMakeLists.txt": '# "core/x.hpp" stays: not under engine/ or demos/\n',
        }
        result, produced = continuity.apply_refactor_script(tree)
        self.assertEqual(set(result), {
            "engine/include/engine/core/clock.hpp", "engine/src/gfx/raster.cpp",
            "demos/common/pong.cpp", "demos/sandbox/main.cpp", "CMakeLists.txt"})
        self.assertEqual(produced, set(result) - {"CMakeLists.txt"})
        self.assertEqual(result["engine/include/engine/core/clock.hpp"],
                         "#include <engine/math/vec2.hpp>\n")
        self.assertEqual(result["CMakeLists.txt"], tree["CMakeLists.txt"])

    def test_replay_matches_the_script_lesson_5_1_publishes(self):
        """If 5.1's script is ever edited, this replay must be edited with it."""
        path = os.path.join(REPO, "docs", "lessons", "05-01-the-refactor.html")
        with open(path, encoding="utf-8") as fh:
            page = fh.read()
        script = next(item.text for item in continuity.page_listings(page)
                      if item.path.startswith("the move"))
        for line in ('for f in src/$d/*.hpp; do git mv "$f" "engine/include/engine/$d/$(basename $f)"',
                     'for f in src/$d/*.cpp; do git mv "$f" "engine/src/$d/$(basename $f)"',
                     "git mv src/game/pong.hpp demos/common/pong.hpp",
                     "git mv src/main.cpp      demos/sandbox/main.cpp",
                     r"""'s|#include "(core\|gfx\|math)/([a-z0-9_]+\.hpp)"|#include <engine/\1/\2>|'"""):
            self.assertIn(line, script)
        self.assertIn("for d in core gfx math; do", script)
        self.assertIn("for d in core gfx; do", script)


class Differences(unittest.TestCase):
    def test_comment_only_changes_are_told_apart_from_code(self):
        self.assertEqual(continuity.describe_difference("// a\nint x;\n", "// b\nint x;\n"),
                         "2 line(s), comments only")
        self.assertEqual(continuity.describe_difference("int x;\n", "int y;\n"), "2 line(s), code")

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
            run = lambda *a: subprocess.run(a, cwd=repo, check=True, capture_output=True)
            run("git", "init", "-q")
            os.makedirs(os.path.join(repo, "scratch"))
            for name in ("tracked.txt", "scratch/forced.py", "scratch/untracked.ppm"):
                with open(os.path.join(repo, name), "w") as fh:
                    fh.write(name)
            with open(os.path.join(repo, ".gitignore"), "w") as fh:
                fh.write("scratch/\n")
            run("git", "add", "tracked.txt", ".gitignore")
            run("git", "add", "-f", "scratch/forced.py")
            saved = builders.REPO
            builders.REPO = repo
            try:
                builders.clone_tracked(dest)
            finally:
                builders.REPO = saved
            self.assertTrue(os.path.exists(os.path.join(dest, "tracked.txt")))
            self.assertTrue(os.path.exists(os.path.join(dest, "scratch/forced.py")))
            self.assertFalse(os.path.exists(os.path.join(dest, "scratch/untracked.ppm")))


if __name__ == "__main__":
    unittest.main()
