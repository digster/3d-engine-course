#!/usr/bin/env python3
"""Assemble docs/lessons/04-04-first-triangle.html.

The prose lives in scratch/l44_body_{a,b}.html, the figures are computed by
scratch/figs_44.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.

Figure placeholders are named by CONTENT, not by file number, because the figures
were authored in one order and appear in another.
"""
import os
import re

OUT = "docs/lessons/04-04-first-triangle.html"

FIGURES = {
    "FIG_FIELDS": ("l44_fig2.svg", "1",
        "Every field of <code>SDL_GPUGraphicsPipelineCreateInfo</code>, what it decides, and the "
        "value this course gives it &#8212; nine at the top level, <strong>fifty-three</strong> "
        "once the nested state structs are expanded, which is the count Lesson 4.1 predicted from "
        "<code>fill_style</code>. Below, four creation attempts: only the last is refused. A "
        "mismatched colour format and an attribute the shader never declared are both accepted "
        "without complaint, which is why a pipeline that creates successfully tells you almost "
        "nothing."),

    "FIG_COMPILE": ("l44_fig1.svg", "2",
        "The answer to the prediction Lesson 4.3 wrote down, on a logarithmic scale because these "
        "numbers span four orders of magnitude. Creating a shader costs <strong>0.031&nbsp;ms</strong> "
        "&#8212; in every run, in every configuration. Creating a pipeline never costs that little, "
        "and that is where the compile is. How much more depends entirely on what the driver has "
        "already compiled: about <strong>32&nbsp;ms</strong> for the first pipeline in a process, "
        "<strong>2.4&nbsp;ms</strong> for each state permutation it has not seen, and under a "
        "millisecond for one it has &#8212; including one compiled in a <em>previous run</em>, "
        "because the cache is on disk. Enabling the validation layer changes none of this "
        "materially, which is worth stating because an earlier draft of this lesson claimed the "
        "opposite at some length."),

    "FIG_TRIANGLE": ("l44_fig3.svg", "3",
        "The first triangle the GPU drew for us, rendered into an offscreen target and downloaded "
        "so that it could be <em>counted</em> rather than admired. It covered <strong>20,808</strong> "
        "pixels against the 20,972 its geometry predicts &#8212; a ratio of 0.9922, the shortfall "
        "being the boundary. At the centroid the three channels read <strong>85, 86, 85</strong>, "
        "where one third of 255 is 85: Lesson 2.4&#8217;s barycentric interpolation, performed by "
        "silicon, agreeing with your derivation to one code out of 255."),

    "FIG_WINDING": ("l44_fig4.svg", "4",
        "Winding, and why a first triangle is so often invisible. The same three points in "
        "counter-clockwise order cover 20,808 pixels; with two of them swapped they cover "
        "<strong>zero</strong> &#8212; not flipped, not dark, <em>absent</em>. The third panel is "
        "what makes this diagnosable rather than mysterious: turning culling off brings the "
        "&#8220;wrong&#8221; order back at exactly 20,808 pixels, identical to the first panel, "
        "which proves the geometry was never the problem. That two-step test will save you an "
        "evening at least once."),

    "FIG_EDGE": ("l44_fig5.svg", "5",
        "Where your rasterizer and the hardware disagree, one square per pixel, drawn from the "
        "actual comparison rather than from an impression of one. Pale blue is agreement; green is "
        "a pixel <em>ours</em> covered and the GPU did not. They appear one at a time along a "
        "single edge &#8212; 102 in the whole triangle, 0.49% &#8212; and there are no squares of "
        "the third colour at all, because the hardware covered nothing we missed. Disagreements "
        "strictly inside the triangle: <strong>zero</strong>. What separates the two is a fill "
        "rule, which is a convention about who owns a boundary pixel, not a difference of opinion "
        "about the shape."),

    "FIG_FRAME": ("l44_fig6.svg", "6",
        "One frame of <code>engine --gpu</code>, rebuilt offscreen and counted, because a window "
        "cannot be asked what it contains. A render pass clears, a blit puts the software "
        "rasterizer&#8217;s picture on top, and a second pass <em>loads</em> that rather than "
        "clearing it and draws the triangle over it. The three contributions are 22,364 + 22,364 + "
        "20,808 = <strong>65,536</strong> pixels, which is 256&#178; exactly: every pixel in the "
        "result attributable to exactly one of the three operations that made it."),
}

LISTING_META = {
    "src/gfx/gpu_pipeline.hpp": ("new", "new"),
    "src/gfx/gpu_pipeline.cpp": ("new", "new"),
    "src/gfx/gpu_buffer.hpp": ("new", "new"),
    "src/gfx/gpu_buffer.cpp": ("new", "new"),
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
# The contents are pinned from commit 413e015 — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "src/gfx/gpu_pipeline.hpp": "scratch/l44_src_gfx_gpu_pipeline.hpp",
    "src/gfx/gpu_pipeline.cpp": "scratch/l44_src_gfx_gpu_pipeline.cpp",
    "src/gfx/gpu_buffer.hpp":   "scratch/l44_src_gfx_gpu_buffer.hpp",
    "src/gfx/gpu_buffer.cpp":   "scratch/l44_src_gfx_gpu_buffer.cpp",
    "src/main.cpp":             "scratch/l44_src_main.cpp",
    "CMakeLists.txt":           "scratch/l44_CMakeLists.txt",
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
<title>4.4 — The First Triangle · Build a Professional 3D Game Engine</title>
<meta name="description" content="A pipeline object, three vertices and one draw call — the first pixel computed by hardware in this course. Where the shader compile actually happens, measured; why a first triangle is usually invisible; and a pixel-by-pixel comparison against the software rasterizer you wrote in Module 2.">

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
    <a class="prev-l" href="04-03-shader-toolchain.html">
      <span class="dir">← Previous</span>
      <span class="ttl">4.3 — The Shader Toolchain</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="04-05-vertex-buffers.html">
      <span class="dir">Next →</span>
      <span class="ttl">4.5 — Vertex Buffers and Layouts</span>
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
    with open("scratch/l44_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l44_body_b.html") as fh:
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
