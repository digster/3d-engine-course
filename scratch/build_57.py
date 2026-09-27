#!/usr/bin/env python3
"""Assemble docs/lessons/05-07-ecs-storage.html.

Same pipeline as build_56.py: prose from scratch/l57_body_{a,b}.html, figures
from scratch/figs_57.py, and code listings read straight out of the repository so
they cannot drift from what actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-07-ecs-storage.html"

FIGURES = {
    "FIG_SHAPES": ("l57_fig1.svg", "1",
        "The two candidate designs, drawn as memory, holding the same four components on the "
        "same entities. The archetype&#8217;s parallel columns are a promise about "
        "<em>identity</em>: row <em>i</em> is the same entity in every one of them, because the "
        "chunk was built so that it would be. The sparse set makes no such promise &#8212; each "
        "pool&#8217;s order is its own business &#8212; so it pays two dependent loads per extra "
        "component to find out where the entity&#8217;s data sits. <strong>That middle column is "
        "the entire structural difference between the designs</strong>, and everything measured "
        "below is its price or its refund."),

    "FIG_REDIRECT": ("l57_fig2.svg", "2",
        "Why a sparse-set lookup is not a linked list, which is the first thing Lesson 5.6 makes "
        "you worry about. The chain is two deep &#8212; the second load&#8217;s address arrives "
        "from the first &#8212; but the chains of different entities are <strong>independent</strong>, "
        "and the entity ids that start them come off a dense array sixteen to a cache line. So a "
        "dozen can be outstanding at once and their misses OVERLAP, which is 5.6&#8217;s "
        "memory-level parallelism and the difference between its 2.4&#215; arm and its 6.5&#215; "
        "one. The useful consequence: the redirect is <em>latency</em>, and latency is what a "
        "machine is good at hiding."),

    "FIG_MOVE": ("l57_fig3.svg", "3",
        "Adding one component, in each design &#8212; the operation where the two stop "
        "resembling each other. The archetype must MOVE the entity, so every column it owns "
        "travels and the source chunk is re-packed behind it; the sparse set appends to one pool "
        "and writes one index. Note what is <em>absent</em> from the lower picture: the "
        "transform, velocity and bounds pools are not read, not written and not known about. "
        "That absence is why one design&#8217;s cost contains the entity&#8217;s width and the "
        "other&#8217;s contains nothing at all."),

    "FIG_QUERY": ("l57_fig4.svg", "4",
        "The query result, and the reason a single number for &#8220;sparse set overhead&#8221; "
        "is not a number. Four series, identical source code, differing only in how much work "
        "the loop body does and whether the pools happen to share an order &#8212; and they span "
        "0.96&#215; to 2.40&#215; at the right-hand edge. <strong>Read the left half first:</strong> "
        "below a thousand entities every arm is within 16% of every other, so at this "
        "engine&#8217;s scale the whole argument is worth zero nanoseconds. The control is the "
        "K = 1 row, not plotted because it is 1.00&#215; at every size &#8212; both designs "
        "perform the same walk, so anything else would mean the harness was measuring itself."),

    "FIG_CHURN": ("l57_fig5.svg", "5",
        "The measurement that decides the lesson, and it is not the ratio. Widening the entity "
        "from four components to twelve &#8212; adding eight that this operation neither reads "
        "nor writes &#8212; takes the archetype from 13.1 ns to 58.5 ns and leaves the sparse "
        "set at 4.2. An archetype move costs about two memory touches per column the entity "
        "owns, whatever the change was about; a sparse insert is three writes into one pool and "
        "has no way to discover what else the entity has. <strong>A cost that scales with the "
        "codebase&#8217;s future is worse than a larger cost that does not.</strong>"),

    "FIG_GROUP": ("l57_fig6.svg", "6",
        "The archetype&#8217;s best case, and the move that answers it. When a query matches one "
        "entity in four, the sparse set reaches into a transform pool where three of every four "
        "elements are fetched and discarded &#8212; measured at 1.46&#215; and, once the "
        "pools&#8217; orders have diverged, 1.94&#215;. But a pool&#8217;s dense order is nobody "
        "else&#8217;s business, so two pools can be sorted to agree on index, at which point the "
        "query reads no sparse entry at all and is walking something byte-identical to an "
        "archetype chunk: <strong>0.99&#215;</strong>. This is the asymmetry the decision rests "
        "on &#8212; a sparse set can be given an archetype&#8217;s query later, one group at a "
        "time; an archetype cannot be given O(1) structural change at any price."),
}

LISTING_META = {
    "scratch/ecs_probe.hpp": ("new", "new"),
    "scratch/bench_57.cpp": ("new", "new"),
    "scratch/verify_57.cpp": ("new", "new"),
    "scratch/measure_57.py": ("new", "new"),
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
# The contents are pinned from commit d91c61a — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "scratch/ecs_probe.hpp": "scratch/l57_scratch_ecs_probe.hpp",
    "scratch/bench_57.cpp":  "scratch/l57_scratch_bench_57.cpp",
    "scratch/verify_57.cpp": "scratch/l57_scratch_verify_57.cpp",
    "scratch/measure_57.py": "scratch/l57_scratch_measure_57.py",
}

LISTING_LANG = {
    "scratch/measure_57.py": ("python", "Python"),
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
<title>5.7 — An ECS from Scratch: Storage Design · Build a Professional 3D Game Engine</title>
<meta name="description" content="Archetype or sparse set, decided by measurement before either is built. Five experiments on the two access patterns that differ: a K-component query at five world sizes with two body costs and two pool alignments, the cost of adding a component at two entity widths, archetype fragmentation across up to 4,096 chunks, and a selective query answered by a maintained group. The sparse-set redirect costs nothing at all until the working set leaves cache AND the pools' orders diverge; an archetype's structural change carries the entity's width in it, 13 ns at four components and 58 at twelve; and a group recovers the archetype's query at 0.99x, which makes the migration one-directional and decides the lesson.">

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

TAIL = """

  <nav class="lesson-nav" aria-label="Lesson navigation (bottom)">
    <a class="prev-l" href="05-06-data-oriented-design.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.6 — Data-Oriented Design: Why Scene Trees Creak</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-08-ecs-runtime.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.8 — The ECS Runtime</span>
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
    with open("scratch/l57_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l57_body_b.html") as fh:
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
