#!/usr/bin/env python3
"""Assemble docs/lessons/05-09-transform-hierarchy.html.

Same pipeline as build_58.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l59_body_{a,b,c}.html, figures from scratch/figs_59.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-09-transform-hierarchy.html"

FIGURES = {
    "FIG_COMPOSE": ("l59_fig1.svg", "1",
        "The rule, and the constraint it puts on the order you may compute it in. Above, a moon "
        "whose world position <em>nobody computed</em>: (10,&nbsp;2,&nbsp;0) is not stored "
        "anywhere and no system produced it, it is what two multiplications happen to say. Move "
        "the sun and every number in the lower row changes with no code that says so &#8212; "
        "&#8220;following&#8221; is not a feature anyone implemented. Below, the catch: the rule "
        "reads the PARENT&#8217;S WORLD MATRIX, so the parent must be finished first, and a "
        "component pool hands entities back in insertion order. verify_59 &#167;C churns a "
        "48-entity tree and finds <strong>twelve</strong> sitting ahead of their own parent."),

    "FIG_ORDERS": ("l59_fig2.svg", "2",
        "Three visit orders over one tree, all correct, all computing bit-identical matrices. A "
        "recurses depth-first and has the best temporal locality available &#8212; the "
        "parent&#8217;s matrix was written one call ago. B and C visit breadth-first, in DEPTH "
        "order, which is a topological order because a parent&#8217;s depth is always exactly one "
        "less than its child&#8217;s. <strong>B and C differ only in where the rows live</strong>, "
        "not in the order they are visited, which is why &#167;4.2 can separate the cost of the "
        "order from the cost of the layout. Note what the level orders have that the recursion "
        "does not: within a level, nothing depends on anything else."),

    "FIG_DEPTH": ("l59_fig3.svg", "3",
        "The measurement that decides the lesson, and the entity count never moves &#8212; every "
        "column is 100,000 entities, so anything that changes is the SHAPE. <strong>Read the "
        "first column first:</strong> at depth 1 there is no hierarchy, both arms do identical "
        "work, and the ratio is 1.01&#215;. That is the control that says the harness is measuring "
        "the tree rather than itself. Everything after it is the tree: recursion runs 3.34 &#8594; "
        "13.55&nbsp;ns/entity while level order runs 3.39 &#8594; 4.94. <strong>Recursion is "
        "depth-dependent and level order is very nearly not</strong> &#8212; and it saturates, so "
        "a chain twice as long costs the recursive walk almost nothing more."),

    "FIG_LEVELS": ("l59_fig4.svg", "4",
        "What <code>rebuild()</code> produces: one array of entities and four integers saying "
        "where each level starts. Then the loop, and the useful thing about it is the list of "
        "what is ABSENT &#8212; no recursion, no stack, no visited set, and no &#8220;has my "
        "parent been resolved yet&#8221; test, because the order already guarantees what those "
        "would check. Each of them is machinery that would otherwise have to be written, tested "
        "and paid for per entity. And the property at the bottom arrived free: everything in a "
        "level depends only on the level above, so a level is a <code>parallel_for</code> that "
        "Module 9 will not have to design."),

    "FIG_DIRTY": ("l59_fig5.svg", "5",
        "The optimisation this lesson measured and refused. The second row of labels is the part "
        "nobody quotes: <strong>moving 10% of a depth-8 tree dirties 36% of it</strong>, because "
        "every descendant of a moved entity has to move too, and at 25% moved the &#8220;partial"
        "&#8221; pass is reaching two thirds of the world. So the crossover sits at about a "
        "quarter, and past it the dirty pass is <em>slower</em> &#8212; 1.29&#215; when everything "
        "moves, which is exactly what an animated scene does every frame. A measurement lesson has "
        "to be willing to conclude no; this one concludes no twice."),

    "FIG_CAMERA": ("l59_fig6.svg", "6",
        "The camera, before and after. A struct owned by one program cannot be parented, cannot be "
        "swapped without a pointer somebody has to invalidate, and cannot be found by a query; an "
        "entity with four components can do all three and none of them needed designing. "
        "<code>active_camera</code> is an EMPTY struct and that is the whole feature &#8212; a "
        "component with no data is a tag, and its presence is the information. The identity at the "
        "bottom is checked bit for bit in verify_59 &#167;D: Lesson 2.9 said the view matrix is the "
        "inverse of the camera&#8217;s placement, and now that the camera HAS a placement, that "
        "sentence became executable."),
}

LISTING_META = {
    "engine/include/engine/ecs/hierarchy.hpp": ("new", "new"),
    "engine/include/engine/ecs/camera.hpp": ("new", "new"),
    "demos/ecs_swarm/main.cpp": ("modified", "modified"),
    "scratch/hier_probe.hpp": ("new", "new"),
    "scratch/bench_59.cpp": ("new", "new"),
    "scratch/verify_59.cpp": ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/math/mat4.hpp": ("modified", "modified"),
}

LISTING_LANG = {}

# A LISTING PINNED TO A SNAPSHOT — the same trap build_58.py fell into.
#
# Listings are spliced from the live repository so they cannot drift from what
# compiles, which is right and has one failure mode: a file that a LATER lesson
# changes. Lesson 5.10 rewrites demos/ecs_swarm/main.cpp to use the action map, so
# re-running this script afterwards (to repoint a navigation link, say) would
# splice 5.10's demo into 5.9's page — code referencing `engine::action_map`, which
# does not exist at 5.9's point in the course.
#
# Pinned BEFORE 5.10 touched the file, and verified byte-identical to it at commit
# c1c5ea7:  git show c1c5ea7:demos/ecs_swarm/main.cpp | diff - scratch/l59_ecs_swarm.cpp
#
# A BUILDER IS NOT FROZEN JUST BECAUSE ITS PAGE IS SHIPPED.
# EXTENDED 2026-09-12 to cover EVERY listing, not just the files this
# lesson wrote. The partial dict below pinned what its author knew would
# move; everything else stayed live, so later lessons' edits leaked into
# this page anyway — which is why it no longer rebuilt to what it shipped.
# Pinned from c1c5ea7, verified per file against the shipped page
# with scratch/which_commit.py.
#
# The original dict, kept for its provenance:
#   LISTING_SOURCE = {
#       "demos/ecs_swarm/main.cpp": "scratch/l59_ecs_swarm.cpp",
#   }
LISTING_SOURCE = {
    "engine/include/engine/ecs/hierarchy.hpp": "scratch/l59_engine_include_engine_ecs_hierarchy.hpp",
    "engine/include/engine/ecs/camera.hpp":    "scratch/l59_engine_include_engine_ecs_camera.hpp",
    "demos/ecs_swarm/main.cpp":                "scratch/l59_demos_ecs_swarm_main.cpp",
    "scratch/hier_probe.hpp":                  "scratch/l59_scratch_hier_probe.hpp",
    "scratch/bench_59.cpp":                    "scratch/l59_scratch_bench_59.cpp",
    "scratch/verify_59.cpp":                   "scratch/l59_scratch_verify_59.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/math/mat4.hpp": "scratch/l59_engine_include_engine_math_mat4.hpp",
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
<title>5.9 — Transform Hierarchy and the Camera System · Build a Professional 3D Game Engine</title>
<meta name="description" content="A transform hierarchy on top of a sparse-set ECS, and the ordering problem that makes it interesting: a parent must be resolved before its children, but a component pool hands entities back in insertion order. Three candidate visit orders measured before one is shipped — depth-first recursion runs 3.34 to 13.55 ns/entity from depth 1 to 32 while level order runs 3.39 to 4.94 — plus the dirty-flag optimisation this lesson measures and refuses at 1.29x, and a camera that becomes an ordinary entity whose view matrix is the rigid inverse of its own placement.">

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
    <a class="prev-l" href="05-08-ecs-runtime.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.8 — The ECS Runtime</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-10-input-mapping.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.10 — Input Mapping: Actions, Not Keycodes</span>
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
    parts = []
    for name in ("a", "b", "c"):
        with open(f"scratch/l59_body_{name}.html") as fh:
            parts.append(fh.read())

    page = HEAD + "\n".join(parts) + TAIL

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
