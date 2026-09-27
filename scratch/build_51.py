#!/usr/bin/env python3
"""Assemble docs/lessons/05-01-the-refactor.html.

The prose lives in scratch/l51_body_{a,b}.html, the figures are computed by
scratch/figs_51.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-01-the-refactor.html"

FIGURES = {
    "FIG_TREE": ("l51_fig1.svg", "1",
        "Before, <code>src/</code> was one directory in which every file could reach every other "
        "file, and <code>#include &quot;gfx/raster.hpp&quot;</code> worked from anywhere. After, "
        "there is a gate: <strong>the public headers are the only path that resolves from "
        "outside</strong>, and <code>engine/src/</code> is unreachable. The crossed arrow is the "
        "include a demo can no longer write &#8212; not by convention but because the file is not "
        "on its include path. The quoted error is the architecture: a compiler is never in a "
        "hurry and never makes an exception."),

    "FIG_PARAMS": ("l51_fig2.svg", "2",
        "The old parameter list, coloured by <em>kind</em> rather than read in order. Seven of "
        "the sixteen rows are policy and three are the camera spelled as separate values a caller "
        "must remember to derive together &#8212; which is the whole design, visible in one "
        "glance and invisible in the signature. <strong>A parameter list is a design nobody was "
        "ever asked to defend</strong>, because each addition was locally reasonable and nobody "
        "read the total. Gathering is not tidying: it removes a way to be wrong."),

    "FIG_COST": ("l51_fig4.svg", "3",
        "Touch one file, rebuild, best of three. A private source costs one object file and a "
        "relink; a public header costs everyone who includes it &#8212; <strong>2.3&#215; on a "
        "project of about twenty-two thousand lines</strong>. The seconds are trivial and quoting "
        "them as a hardship would be dishonest; the RATIO is what carries to a codebase fifty "
        "times the size, where the same two edits are a coffee break apart. This is why "
        "&#8220;should this be in the header?&#8221; is an engineering question."),

    "FIG_DEPS": ("l51_fig3.svg", "4",
        "Every arrow points down, and which way they point is a decision rather than a discovery. "
        "<code>stb_image</code> is PRIVATE, so the dependency <strong>stops at the "
        "boundary</strong>: no demo can see it, and replacing it would be invisible outside "
        "<code>image.cpp</code>. <code>SDL3::SDL3</code> is PUBLIC, which is an admission &#8212; "
        "our public headers name SDL's types, so we have adopted its vocabulary permanently. The "
        "crossed arrow is the engine needing a demo, which is the one shape that would make the "
        "library useless."),

    "FIG_METHOD": ("l51_fig5.svg", "5",
        "The order is the method. The test that could falsify the refactor is written "
        "<strong>before a single file moves</strong>, and then the work is done in two passes "
        "&#8212; a move that changes nothing, then a change that moves nothing &#8212; each "
        "verified separately, because relocation and redesign break in completely different ways "
        "and one edit containing both leaves you two suspects. The seventh frame was added after "
        "the log admitted that six of them never reached the near-plane clipper."),

    "FIG_RESIDUE": ("l51_fig6.svg", "6",
        "The four things this refactor did not sort out, written down rather than quietly left. "
        "One is a genuine defect that <code>verify_50</code> now <em>pins</em> in its current "
        "state, so a known bug cannot become two. One is a cost of this course's own design and "
        "will probably never be paid. Two need code that does not exist yet. "
        "<strong>The first pass draws the line; which side each thing belongs on is argued for "
        "the rest of the module.</strong>"),

    "FIG_CUBE": ("l51_fig7.svg", "7",
        "What <code>./build/demos/hello_cube</code> puts on screen: an amber cube turning about "
        "its y axis, three visible faces at three brightnesses because a directional light falls "
        "on them from up and to the left. Rendered by <code>hello_cube --shot</code> and reduced "
        "here to flat rectangles. <strong>160 lines, every symbol from &lt;engine/&#8230;&gt;, "
        "and the word &#8220;triangle&#8221; appears nowhere in it.</strong> That is the "
        "difference between a library and a directory."),
}

LISTING_META = {
    "CMakeLists.txt": ("modified", "modified"),
    "cmake/EngineHelpers.cmake": ("new", "new"),
    "cmake/Shaders.cmake": ("modified", "modified"),
    "engine/CMakeLists.txt": ("new", "new"),
    "demos/CMakeLists.txt": ("new", "new"),
    "engine/include/engine/engine.hpp": ("new", "new"),
    "engine/include/engine/gfx/projector.hpp": ("new", "new"),
    "engine/include/engine/gfx/scene.hpp": ("new", "new"),
    "engine/include/engine/gfx/soft_renderer.hpp": ("new", "new"),
    "engine/include/engine/gfx/debug_draw.hpp": ("new", "new"),
    "engine/src/gfx/soft_renderer.cpp": ("new", "new"),
    "engine/src/gfx/debug_draw.cpp": ("new", "new"),
    "demos/common/demo_scene.hpp": ("new", "new"),
    "demos/common/demo_scene.cpp": ("new", "new"),
    "demos/hello_cube/main.cpp": ("new", "new"),
    "demos/sandbox/main.cpp": ("modified", "modified"),
}

# LISTING_SOURCE — added 2026-09-12, and without it THIS SCRIPT CANNOT RUN.
# ---------------------------------------------------------------------------
#
# The paths this page lists live under `src/`, and Module 5's refactor RETIRED
# THAT WHOLE DIRECTORY — the engine's sources moved to engine/src and its
# headers to engine/include/engine. So re-running this builder died at the
# first listing with a FileNotFoundError, which means the page had been
# unreproducible since Lesson 5.1 and nobody had noticed, because nobody had
# needed to rebuild it.
#
# Paths that DO still exist (shaders, CMakeLists.txt) are pinned too, and for
# the opposite reason: reading them live is silent rather than fatal, so every
# later lesson's edits leaked backwards into this page.
#
# The contents are pinned from commit 9e7c9fd — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "CMakeLists.txt":                              "scratch/l51_CMakeLists.txt",
    "cmake/EngineHelpers.cmake":                   "scratch/l51_cmake_EngineHelpers.cmake",
    "cmake/Shaders.cmake":                         "scratch/l51_cmake_Shaders.cmake",
    "engine/CMakeLists.txt":                       "scratch/l51_engine_CMakeLists.txt",
    "demos/CMakeLists.txt":                        "scratch/l51_demos_CMakeLists.txt",
    "engine/include/engine/engine.hpp":            "scratch/l51_engine_include_engine_engine.hpp",
    "engine/include/engine/gfx/projector.hpp":     "scratch/l51_engine_include_engine_gfx_projector.hpp",
    "engine/include/engine/gfx/scene.hpp":         "scratch/l51_engine_include_engine_gfx_scene.hpp",
    "engine/include/engine/gfx/soft_renderer.hpp": "scratch/l51_engine_include_engine_gfx_soft_renderer.hpp",
    "engine/include/engine/gfx/debug_draw.hpp":    "scratch/l51_engine_include_engine_gfx_debug_draw.hpp",
    "engine/src/gfx/soft_renderer.cpp":            "scratch/l51_engine_src_gfx_soft_renderer.cpp",
    "engine/src/gfx/debug_draw.cpp":               "scratch/l51_engine_src_gfx_debug_draw.cpp",
    "demos/common/demo_scene.hpp":                 "scratch/l51_demos_common_demo_scene.hpp",
    "demos/common/demo_scene.cpp":                 "scratch/l51_demos_common_demo_scene.cpp",
    "demos/hello_cube/main.cpp":                   "scratch/l51_demos_hello_cube_main.cpp",
    "demos/sandbox/main.cpp":                      "scratch/l51_demos_sandbox_main.cpp",
}

LISTING_LANG = {
    "CMakeLists.txt": ("cmake", "CMake"),
    "cmake/EngineHelpers.cmake": ("cmake", "CMake"),
    "cmake/Shaders.cmake": ("cmake", "CMake"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def esc(text):
    return (text.replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace('"', "&quot;")
                .replace("'", "&#x27;"))


def listing(path):
    with open(LISTING_SOURCE.get(path, path)) as fh:
        body = fh.read()
    tag, word = LISTING_META[path]
    lang, label = LISTING_LANG.get(path, ("cpp", "C++"))
    return (
        '  <figure class="listing">\n'
        '    <figcaption>\n'
        f'      <span class="path">{path}</span>\n'
        f'      <span class="tag {tag}">{word}</span>\n'
        f'      <span class="lang" data-lang="{lang}">{label}</span>\n'
        '    </figcaption>\n'
        f'    <pre><code class="lang-{lang}">{esc(body)}</code></pre>\n'
        '  </figure>\n'
    )


def figure(key):
    name, num, caption = FIGURES[key]
    with open(f"scratch/{name}") as fh:
        svg = fh.read().rstrip()
    svg = "\n".join("    " + ln if ln.strip() else ln for ln in svg.split("\n"))
    return (
        '  <figure class="dia bleed">\n'
        f'{svg}\n'
        '    <figcaption>\n'
        f'      <span class="fignum">Figure {num}.</span>\n'
        f'      {caption}\n'
        '    </figcaption>\n'
        '  </figure>\n'
    )


HEAD = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>5.1 — The Refactor: Engine, Demos, and the Public API · Build a Professional 3D Game Engine</title>
<meta name="description" content="Splitting a 7,789-line main.cpp into a static library and the programs built on it. Enforcing the boundary with the include path rather than the style guide. Designing a public API: fifteen parameters become eight. Physical design and rebuild cost, measured. And refactoring with a characterization test written before anything moved.">

<!-- SHARED-CSS:BEGIN -->
<!-- The shared course stylesheet, linked rather than inlined. Single source of
     truth: docs/shared/course.css. Edit that file; this page carries no copy.
     Still no build step - the link resolves straight off the filesystem, so this
     page opens by double-clicking, offline. -->
<link rel="stylesheet" href="../shared/course.css">
<!-- SHARED-CSS:END -->

<!-- KaTeX (optional). If unreachable the raw TeX remains readable, and every
     equation is also stated in prose + .eq-plain, so nothing is lost. -->
<link rel="stylesheet"
      href="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/katex.min.css"
      integrity="sha384-nB0miv6/jRmo5UMMR1wu3Gz6NLsoTkbqJghGIsx//Rlm+ZU03BU6SQNC66uf4l5+"
      crossorigin="anonymous">
</head>
<body>

<header class="masthead">
  <div class="masthead-inner">
    <a class="course" href="../index.html">Build a Professional 3D Game Engine</a>
    <span class="spacer"></span>
    <a href="../index.html">Contents</a>
    <a href="../conventions.html">Conventions</a>
    <a href="../math-toolbox.html">Math Toolbox</a>
    <button class="theme-toggle" id="theme-toggle" type="button" aria-label="Toggle colour theme">Theme</button>
  </div>
</header>

<div class="wrap">

"""

# AMENDED 2026-09-12: the STATE block is gone, and this script no longer
# stamps one. `STATE.md` became the sole resume key at 0.6 and CLAUDE.md §9
# was amended at 5.7 to retire the per-lesson block — but this builder
# predates that and was never updated, so re-running it would have RE-ADDED a
# STATE block to a page it was stripped from. The same amendment build_57.py
# received in 5.8.
TAIL = """

  <nav class="lesson-nav" aria-label="Lesson navigation (bottom)">
    <a class="prev-l" href="04-09-renderdoc.html">
      <span class="dir">← Previous</span>
      <span class="ttl">4.9 — Debugging a Frame with RenderDoc</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-02-platform-layer.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.2 — The Platform and Application Layer</span>
    </a>
  </nav>

</div>

<!-- SHARED-SCRIPT:BEGIN -->
<!-- The shared page script (theme toggle, TOC scrollspy, syntax highlighter),
     linked rather than inlined. Single source of truth: docs/shared/course.js.
     A plain classic script at end of body, so it runs exactly where the inline
     copy used to: after the DOM is parsed, before KaTeX's deferred render. -->
<script src="../shared/course.js"></script>
<script defer
        src="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/katex.min.js"
        integrity="sha384-7zkQWkzuo3B5mTepMUcHkMB5jZaolc2xDwL6VFqjFALcbeS9Ggm/Yr2r3Dy4lfFg"
        crossorigin="anonymous"></script>
<script defer
        src="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/contrib/auto-render.min.js"
        integrity="sha384-43gviWU0YVjaDtb/GhzOouOXtZMP/7XUzwPTstBeZFe/+rCMvRwr4yROQP43s0Xk"
        crossorigin="anonymous"
        onload="renderMathInElement(document.body, {
          delimiters: [
            {left: '\\\\[', right: '\\\\]', display: true},
            {left: '\\\\(', right: '\\\\)', display: false}
          ],
          throwOnError: false
        });"></script>
<!-- SHARED-SCRIPT:END -->
</body>
</html>
"""


def main():
    with open("scratch/l51_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l51_body_b.html") as fh:
        body_b = fh.read()

    page = HEAD + body_a + "\n" + body_b + TAIL

    for key in FIGURES:
        page = page.replace(f"@@{key}@@", figure(key))

    page = re.sub(r"@@LISTING:([^@]+)@@", lambda m: listing(m.group(1)), page)

    left = re.findall(r"@@[A-Z0-9_:./-]+@@", page)
    if left:
        raise SystemExit(f"unsubstituted placeholders: {sorted(set(left))}")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as fh:
        fh.write(page)
    print(f"wrote {OUT}  ({len(page):,} bytes, {page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
