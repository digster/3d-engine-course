#!/usr/bin/env python3
"""Assemble docs/lessons/05-06-data-oriented-design.html.

Same pipeline as build_55.py: prose from scratch/l56_body_{a,b}.html, figures
from scratch/figs_56.py, and code listings read straight out of the repository so
they cannot drift from what actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-06-data-oriented-design.html"

FIGURES = {
    "FIG_SCALE": ("l56_fig1.svg", "1",
        "Before any argument about layout: how much data is there, and where does it fit? A "
        "<code>scene_object</code> is 96 bytes &#8212; 1.5 cache lines &#8212; of which 60 are "
        "the <code>transform</code>, everything a model-matrix loop reads. Multiply that out "
        "against a cache hierarchy and the table below is the whole shape of the lesson in "
        "advance: <strong>every claim about layout is a claim about which row you are on</strong>, "
        "and a rule with no <em>N</em> in it is not a rule. Our scene is the first row."),

    "FIG_LAYOUTS": ("l56_fig2.svg", "2",
        "The same scene, six ways, drawn as memory. Shading marks the bytes the full-transform "
        "loop actually reads &#8212; note how much of the flat array is fetched and ignored, and "
        "note that SoA&#8217;s three arrays contain nothing else. The bottom row is the one the "
        "lesson is named after, and its caption is the whole mechanism: <strong>the address of "
        "the next node is inside the current one</strong>, so the CPU cannot begin fetching it "
        "until this one has arrived."),

    "FIG_KNEE": ("l56_fig4.svg", "4",
        "Every layout&#8217;s cost relative to a flat array, against scene size. <strong>Read the "
        "left half first, because it is the half nobody publishes:</strong> up to a thousand "
        "objects every arrangement is within 13% of every other, and scattering objects across "
        "the heap or walking a linked tree costs precisely nothing. The knee sits between 1,000 "
        "and 10,000 &#8212; which is exactly where 96 bytes an object stops fitting in this "
        "machine&#8217;s L2, making the number a property of the cache rather than of anyone&#8217;s "
        "taste. Our scene is four objects, at the far left."),

    "FIG_TWO_SOA": ("l56_fig5.svg", "5",
        "The result that changed how this course explains data-oriented design. SoA wins on both "
        "workloads and <strong>the two wins have nothing in common</strong>, which one experiment "
        "&#8212; rebuild with the vectoriser off &#8212; establishes. The full-transform win is "
        "identical at 384 bytes and at 9.6 MB and <em>vanishes completely</em> without "
        "vectorisation: it was never a cache effect. The cull win appears only once the working "
        "set leaves L2 and survives with vectorisation off: that one is the cache, in isolation. "
        "The rule this produces is not &#8220;use SoA&#8221;."),

    "FIG_VIRTUAL": ("l56_fig6.svg", "6",
        "What a <code>virtual</code> call costs, and the two explanations the measurement rules "
        "out. Monomorphic and polymorphic agree within 4% at every size, so it is <strong>not "
        "branch misprediction</strong>; the cost is flat across <em>N</em> including four objects "
        "inside L1, so it is <strong>not the vtable load</strong> either. What is left is the "
        "inlining it prevents. The footnote matters too: an earlier version of this measurement "
        "read only five of a matrix&#8217;s sixteen entries, which let every <em>inlined</em> arm "
        "skip work the virtual one had to do. It read 3.4&#215;. It was wrong."),

    "FIG_CHAIN": ("l56_fig3.svg", "3",
        "Why two things that are both called &#8220;pointer chasing&#8221; differ by a factor of "
        "three. An array of pointers has its addresses <strong>known in advance</strong>, so a "
        "core can issue a dozen dependent loads before any returns and the misses OVERLAP &#8212; "
        "memory-level parallelism, and the reason scattering objects costs 2.4&#215; rather than "
        "20&#215;. A linked list stores each address inside the previous node, so the misses are "
        "a serial dependency chain and add instead. A scene tree is the second picture, and that "
        "&#8212; not the hierarchy, not the OOP, not the virtual calls &#8212; is what creaks."),
}

LISTING_META = {
    "engine/include/engine/core/bench.hpp": ("new", "new"),
    "scratch/scene_layouts.hpp": ("new", "new"),
    "scratch/bench_56.cpp": ("new", "new"),
    "scratch/verify_56.cpp": ("new", "new"),
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
# The contents are pinned from commit f90cc2a — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "engine/include/engine/core/bench.hpp": "scratch/l56_engine_include_engine_core_bench.hpp",
    "scratch/scene_layouts.hpp":            "scratch/l56_scratch_scene_layouts.hpp",
    "scratch/bench_56.cpp":                 "scratch/l56_scratch_bench_56.cpp",
    "scratch/verify_56.cpp":                "scratch/l56_scratch_verify_56.cpp",
}

LISTING_LANG = {}


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
<title>5.6 — Data-Oriented Design: Why Scene Trees Creak · Build a Professional 3D Game Engine</title>
<meta name="description" content="The lesson that earns the ECS with numbers instead of a story. Six scene layouts, two workloads, five scene sizes, timed on the engine's own transform. Below a thousand objects layout does not matter; the knee is where the working set leaves L2; an array of pointers costs 2.4x and a linked list 6.5x, and the gap is memory-level parallelism. Three results contradicted expectation: SoA's win on a full-struct workload is vectorisation and not cache, a polymorphic virtual call costs the same as a monomorphic one, and the heap fragmentation the benchmark arranged did not happen.">

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
    <a class="prev-l" href="05-05-asset-system.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.5 — The Asset System v1</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-07-ecs-storage.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.7 — An ECS from Scratch: Storage Design</span>
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
    with open("scratch/l56_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l56_body_b.html") as fh:
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
