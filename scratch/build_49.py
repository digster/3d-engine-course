#!/usr/bin/env python3
"""Assemble docs/lessons/04-09-renderdoc.html.

The prose lives in scratch/l49_body_{a,b}.html, the figures are computed by
scratch/figs_49.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.

Figure placeholders are named by CONTENT, not by file number, because the figures
were authored in one order and appear in another. The number in each tuple is the
one the reader sees, and it follows PAGE order — check it against where the
@@FIG_*@@ markers actually land in the body.
"""
import os
import re

OUT = "docs/lessons/04-09-renderdoc.html"

FIGURES = {
    "FIG_WALL": ("l49_fig1.svg", "1",
        "Four lanes on one timeline. Your code records calls; the command buffer holds them; "
        "the GPU executes them later; the display shows the result. <strong>A breakpoint "
        "operates entirely in the top lane</strong>, where every call has already returned "
        "success &#8212; and the submit is a wall it cannot follow. That is why a GPU bug is a "
        "<em>state</em> question rather than a control-flow one, and why the instrument you "
        "need has to be able to ask &#8220;what was bound at event 47, and what was in it&#8221;."),

    "FIG_TOOLS": ("l49_fig2.svg", "2",
        "The tool is decided by the backend and the backend is decided by your platform, so this "
        "is not a preference. <strong>RenderDoc does not support Metal</strong> &#8212; the "
        "list is quoted from its own front page &#8212; and SDL_GPU gives macOS Metal, so a Mac "
        "reader uses Xcode&#8217;s Metal Debugger. Every concept in this lesson transfers "
        "exactly; the screenshots do not. <code>SDL_gpu.h</code> has a &#8220;Debugging&#8221; "
        "section saying precisely this, on your disk, and it is the first place to look."),

    "FIG_EVENTS": ("l49_fig3.svg", "3",
        "One frame of <code>engine --trace</code>, with the annotations naming what each part is "
        "called in a capture. Four debug groups, one render pass, the per-frame and per-draw "
        "uniform pushes, the binds, three draws. <strong>Every number here was already in Lesson "
        "4.8&#8217;s log</strong> &#8212; 128 bytes per frame plus 144 per draw times three is "
        "560 &#8212; which is the point of doing this before opening a tool: you are checking "
        "the tool against a frame you already understand, not the other way round."),

    "FIG_COSTS": ("l49_fig4.svg", "4",
        "Recording time against the number of debug groups per frame, and <strong>the noise "
        "floor it has to clear first</strong>. At one and eight groups the difference is inside "
        "the band and is not a result &#8212; the first version of this measurement skipped the "
        "floor and reported a <em>negative</em> cost for adding work. Scale the count and a real "
        "per-call figure appears: 183.6 and 182.0 ns from two independent estimates, and "
        "<strong>the agreement between them is the evidence</strong>. Four groups a frame is "
        "0.004% of a 16.7 ms budget, so they ship on."),

    "FIG_CACHE": ("l49_fig5.svg", "5",
        "A simulated FIFO post-transform vertex cache over <code>torus.obj</code>&#8217;s own "
        "index order &#8212; a model, because the true count needs a tool this machine cannot "
        "run (&#167;3.6 says exactly where to read it). The curve is a <strong>staircase</strong>: "
        "caches from 6 to 32 give identical results, because this torus is emitted ring by ring "
        "and reuse happens at two distances with nothing in between. And the control is the real "
        "finding &#8212; the same 2,304 triangles with the ORDER shuffled cost "
        "<strong>2.83&#215; more</strong>, with nothing about the geometry changed."),

    "FIG_COMPARE": ("l49_fig6.svg", "6",
        "The top six rows are why a hundred and fifty lines of frame log were worth writing: it "
        "runs on every backend including the one RenderDoc cannot capture, it runs in CI where a "
        "GUI cannot, and it can be <strong>cross-checked against a second counter</strong> "
        "&#8212; which a capture cannot, having no independent account of what should have "
        "happened. The bottom six are why the tools exist, and every one of them needs the frame "
        "<em>replayed</em>. The log tells you what you asked the GPU to do; the capture tells "
        "you what the GPU had."),
}

LISTING_META = {
    "src/gfx/gpu_debug.hpp": ("new", "new"),
    "src/gfx/gpu_debug.cpp": ("new", "new"),
    "src/gfx/gpu_buffer.hpp": ("modified", "modified"),
    "src/gfx/gpu_buffer.cpp": ("modified", "modified"),
    "src/gfx/gpu_texture.cpp": ("modified", "modified"),
    "src/gfx/gpu_present.cpp": ("modified", "modified"),
    "src/gfx/gpu_shader.cpp": ("modified", "modified"),
    "src/gfx/gpu_scene.hpp": ("modified", "modified"),
    "src/gfx/gpu_scene.cpp": ("modified", "modified"),
    "src/main.cpp": ("modified", "modified"),
    "CMakeLists.txt": ("modified", "modified"),
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
# The contents are pinned from commit 2ea1e65 — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "src/gfx/gpu_debug.hpp":   "scratch/l49_src_gfx_gpu_debug.hpp",
    "src/gfx/gpu_debug.cpp":   "scratch/l49_src_gfx_gpu_debug.cpp",
    "src/gfx/gpu_buffer.hpp":  "scratch/l49_src_gfx_gpu_buffer.hpp",
    "src/gfx/gpu_buffer.cpp":  "scratch/l49_src_gfx_gpu_buffer.cpp",
    "src/gfx/gpu_texture.cpp": "scratch/l49_src_gfx_gpu_texture.cpp",
    "src/gfx/gpu_present.cpp": "scratch/l49_src_gfx_gpu_present.cpp",
    "src/gfx/gpu_shader.cpp":  "scratch/l49_src_gfx_gpu_shader.cpp",
    "src/gfx/gpu_scene.hpp":   "scratch/l49_src_gfx_gpu_scene.hpp",
    "src/gfx/gpu_scene.cpp":   "scratch/l49_src_gfx_gpu_scene.cpp",
    "src/main.cpp":            "scratch/l49_src_main.cpp",
    "CMakeLists.txt":          "scratch/l49_CMakeLists.txt",
}

LISTING_LANG = {
    "CMakeLists.txt": ("cmake", "CMake"),
}


def esc(text):
    """Escape for a <pre><code> block, matching the rest of the course's pages."""
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
<title>4.9 — Debugging a Frame with RenderDoc · Build a Professional 3D Game Engine</title>
<meta name="description" content="Why a breakpoint cannot catch a GPU bug, and what can. Choosing a frame debugger — RenderDoc does not support Metal. Naming every resource the way SDL asks. Debug groups as a C++ scope. Measuring instrumentation against its own noise floor, after a first attempt reported a negative cost for adding work.">

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
    <a class="prev-l" href="04-08-porting-the-scene.html">
      <span class="dir">← Previous</span>
      <span class="ttl">4.8 — Porting the Module-3 Scene</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-01-the-refactor.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.1 — The Refactor: Engine, Demos, and the Public API</span>
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
    with open("scratch/l49_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l49_body_b.html") as fh:
        body_b = fh.read()

    page = HEAD + body_a + "\n" + body_b + TAIL

    for key in FIGURES:
        page = page.replace(f"@@{key}@@", figure(key))

    def sub_listing(m):
        return listing(m.group(1))

    page = re.sub(r"@@LISTING:([^@]+)@@", sub_listing, page)

    left = re.findall(r"@@[A-Z0-9_:./-]+@@", page)
    if left:
        raise SystemExit(f"unsubstituted placeholders: {sorted(set(left))}")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as fh:
        fh.write(page)
    print(f"wrote {OUT}  ({len(page):,} bytes, {page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
