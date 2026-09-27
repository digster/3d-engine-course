#!/usr/bin/env python3
"""Assemble docs/lessons/08-06-epa.html.

Same pipeline as build_71.py through build_85.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l86_body_{a..f}.html, figures from scratch/figs_86.py, and
every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. Ten listings, and five of them are files
the rest of Module 8 edits on every lesson: `collide.hpp` gains an entry point
per algorithm, `collide.cpp` gains a façade branch, and `engine.hpp` and the two
CMakeLists gain a line each. 8.7's manifold generation will touch `epa.hpp`
itself — it needs the winning face's vertices, not only the normal — so a builder
that opened a repository path would render this page's listings as they stand
THEN rather than as they stood when the prose was written about them. §12.2's
claim that `face` carries no default member initialisers is a claim about four
specific lines in a specific file.

TWO LISTINGS ARE FILES 8.5 ALSO PRINTED, and they disagree with 8.5's page on
purpose. `collide.cpp` there returns `+0` with a comment calling it a
placeholder; here the placeholder is gone. `collide.hpp` there says `depth` is
"exact when negative and a placeholder when not". Both pages are archives of
their own era and the disagreement is the edit being recorded, not drift.

*** AND ONE OF THOSE COMMENTS WAS WRONG WHEN IT SHIPPED, WHICH IS A DIFFERENT
THING. *** 8.5's `collide.cpp` said the axis on an overlap was "the last search
direction"; §A.2 measures that it was the zero vector, 200,000 times in 200,000,
because `gjk_result::direction` is only written on the separated path. The pin is
NOT corrected: 8.5's page is an accurate archive of what shipped, wrong comment
and all, and the correction belongs in §1 of this lesson where a reader meets it
with the measurement attached. Patching the pin would erase the mistake instead
of teaching it.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson. Here
the pins are generated from LISTING_META by `_pin`, so a path added to
LISTING_META without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/08-06-epa.html"

FIGURES = {
    "FIG_PLACEHOLDER": ("l86_fig1.svg", "1",
        "<strong>What 8.5 returned when two shapes overlapped, and what it cost.</strong> Left: "
        "two unit cubes 0.60&nbsp;m apart in x and 0.15 in y, overlapping by "
        "<strong>0.400&nbsp;m</strong> — a number GJK does not compute, because the origin is "
        "inside <code>A&nbsp;⊖&nbsp;B</code> and the depth is a distance to its boundary. What it "
        "reports is <code>intersecting</code>, a distance of zero, and a direction that its own "
        "doc comment described as “the last search direction, a reasonable guess at a contact "
        "normal”. Right, the first measurement of this lesson: <strong>the guess was the ZERO "
        "VECTOR on 200,000 overlapping pairs out of 200,000</strong>, because "
        "<code>gjk_result::direction</code> is assigned in exactly one place and that place is on "
        "the separated path. So the guess measured instead is the one a naive implementation "
        "reaches for — centre to centre — which is <strong>40.15° off on average</strong> and "
        "implies a push of <strong>1.71× the true minimum translation</strong>, up to 2,538× when "
        "two long boxes overlap slightly along their length. The control at the bottom is this "
        "lesson's answer against the same exact reference: nine thousandths of a degree."),

    "FIG_MTV": ("l86_fig2.svg", "2",
        "<strong>The two-line derivation, drawn.</strong> Left: a pentagon and a ten-sided "
        "polygon overlapping — how far, and which way, to pull them apart? Middle: the same pair as ONE "
        "set, the Minkowski difference <code>A&nbsp;⊖&nbsp;B</code>, with the origin inside it "
        "because they overlap, and the purple arrow running to the nearest point of its "
        "<em>boundary</em>. Right: the ten-sided polygon moved by exactly that vector, now "
        "touching and no longer overlapping. The identity underneath is the whole lesson: A and "
        "<code>B&nbsp;+&nbsp;t</code> overlap exactly when <code>t&nbsp;=&nbsp;a&nbsp;−&nbsp;b</code> "
        "for some pair of points, which is exactly when <code>t&nbsp;∈&nbsp;A&nbsp;⊖&nbsp;B</code> "
        "— so <strong>the translations that keep two shapes overlapping ARE the difference "
        "set</strong>, and the shortest one that separates them is the shortest vector that is "
        "not in it. GJK measured the distance to the SET, which is zero here. EPA measures the "
        "distance to the set's BOUNDARY."),

    "FIG_SANDWICH": ("l86_fig3.svg", "3",
        "<strong>Three snapshots of one expansion, on figure 2's difference set, with "
        "nothing drawn by hand.</strong> The figure script runs the same algorithm as "
        "<code>engine/src/phys/epa.cpp</code> reduced to the plane, so every polygon here was "
        "expanded rather than placed. Amber is the inner polytope: every vertex is a real point "
        "of the difference set, so the whole thing fits inside it, so its closest edge — drawn "
        "green — is a <strong>lower</strong> bound on the depth. The dashed grey line is the "
        "supporting line through the support point taken along that edge's normal, an "
        "<strong>upper</strong> bound, because nothing in the set reaches past it. Read the two "
        "numbers under each panel and notice the direction of travel: <strong>the lower bound "
        "RISES</strong>. 8.5's equivalent quantity only ever fell. Same sandwich, opposite "
        "handedness — which is why the two algorithms are usually described as one, and why "
        "<code>epa.cpp</code> can check monotonicity with the same two lines <code>gjk.cpp</code> "
        "uses, with the inequality reversed."),

    "FIG_SEED": ("l86_fig4.svg", "4",
        "<strong>Every description of EPA opens “start from the tetrahedron GJK left”. Here is "
        "what GJK actually leaves.</strong> Six arrangements, 50,000 overlapping pairs each, the "
        "terminal simplex's vertex count stacked left to right. The top row is the one that "
        "matters: two crates standing on the same floor at arbitrary yaw — not an adversarial "
        "fixture but the default arrangement of almost everything in almost every level — and the "
        "four-point column is <strong>zero</strong>. Not rare; never. A shared up axis puts "
        "<code>y&nbsp;=&nbsp;0</code> on every difference vertex the search visits, so the search "
        "is trapped in a plane, and 8.5 §9 proved it exactly rather than statistically. What "
        "arrives instead is a flat <strong>TRIANGLE</strong>, 49,951 times — not the flat "
        "tetrahedron the folklore warns about, because <code>reduce_simplex</code> discards any "
        "vertex the closest point does not use and three points in a plane already span it. Tilt "
        "one crate by a single degree and 49,802 tetrahedra appear. The right-hand column is the "
        "verdict: a textbook EPA refuses <strong>100% of the floor row</strong> and 1.57% even of "
        "free orientations."),

    "FIG_APEX": ("l86_fig5.svg", "5",
        "<strong>One way out of the plane is not enough, and six faces stitched by hand are not a "
        "polytope.</strong> Left: the triangle GJK hands over, with one support point found along "
        "its normal. The origin was in that plane to begin with — that is what “GJK reduced to "
        "this triangle” means — so it is now sitting <strong>on the tetrahedron's base</strong>, "
        "the closest face is at distance zero, and the lower bound starts and stays there. "
        "Measured: <strong>30,000 times out of 30,000</strong>, maximum as well as mean. Middle: "
        "one support call on each side of the plane, and the origin is enclosed with a starting "
        "lower bound of 9&nbsp;cm against a true depth of 49. Right, the finding that changed the "
        "implementation: a bipyramid is convex only when each apex projects INSIDE the triangle, "
        "and a support point along the plane normal has no reason to — <strong>52.85% of "
        "hand-stitched seeds are not convex</strong>. Only 2.3% of queries then produced a visibly "
        "wrong answer, which is exactly why it survived: nineteen in twenty broken seeds repaired "
        "themselves on the first expansion."),

    "FIG_HORIZON": ("l86_fig6.svg", "6",
        "<strong>The rim is found by cancellation, and then by asking a question that has an "
        "answer.</strong> Left, the operation one dimension down where it is legible: a new point "
        "outside a polygon, the edges that can see it in red, and the two rim vertices circled. "
        "Every edge INTERIOR to the deleted cap belongs to two deleted faces and appears twice in "
        "opposite directions; every rim edge appears once. So push all of them onto a list and "
        "cancel the reverses — the horizon survives, already in winding order, with no adjacency "
        "stored and nothing to keep correct. Right, why that is not enough. The theorem that the "
        "visible set is connected holds in exact arithmetic; in float, a support point that lands "
        "EXACTLY on several face planes leaves <code>dot(n,&nbsp;w)</code> and <code>d</code> as "
        "the same number computed two different ways, and the last few bits scatter the "
        "classification. Measured on two cubes face to face with 0.1&nbsp;mm of overlap: three "
        "faces called visible, sharing no edge, <strong>nine horizon edges with nothing to "
        "cancel</strong>. The repair is to flood fill from the closest face, which is the one face "
        "certainly visible — in exact arithmetic it changes nothing, and in float it undoes the "
        "scatter."),

    "FIG_TEAR": ("l86_fig7.svg", "7",
        "<strong>And a random test suite would never find it.</strong> The same two formulations "
        "run over three populations of 50,000 overlapping pairs each. Crates standing on a floor: "
        "<strong>4.17% of horizons tear</strong>, and the worst reports 0.065&nbsp;m against a "
        "true 0.171 — <strong>61.7% low</strong>. The same crates at free orientations: five in "
        "fifty thousand, four hundred times rarer. Two convex hulls built from points on a "
        "sphere, which have no shared axes and no coplanar structure at all: never once. This is "
        "the third lesson running with the same moral — 8.4 §F needed a shared up axis, 8.5 §C "
        "needed a sliver triangle, and both are produced constantly by real scenes and essentially "
        "never by uniform random fixtures. The tear needs the new support point to be EXACTLY "
        "coplanar with existing faces, which requires the axis alignment and face-on-face contact "
        "that a level is made of and that a random orientation destroys."),

    "FIG_DEMO": ("l86_fig8.svg", "8",
        "<strong>The answer as a position rather than a number.</strong> Two frames from "
        "<code>demos/epa</code> on preset 3 — two crates standing on the same floor, §5's "
        "arrangement. Left, two expansions in: the gold polytope on the right panel is still a "
        "small bipyramid inside the difference set's grey silhouette, the pink vector to its "
        "closest face is short, and on the left panel the <strong>green ghost</strong> — the "
        "second crate at the position the current answer would push it to — is still buried in "
        "the blue one. That is what an unconverged lower bound <em>is</em>. Right, converged: the "
        "polytope fills most of the set and the ghost is clear. Note that the white cross is "
        "<em>inside</em> the outline, unlike 8.5's panel, and that the pink vector only ever gets "
        "LONGER — same panel, same colour, same set, opposite direction of travel, because one "
        "algorithm walks in from outside and the other pushes out from inside. "
        "<strong>[F]</strong> drops the flood fill and figure 7's failure can be watched rather "
        "than read about."),

    "FIG_CONVERGE": ("l86_fig9.svg", "9",
        "<strong>The odd one out has swapped places, and the reason is geometric.</strong> Mean "
        "expansions against tolerance, five shape pairs. Box and hull are flat, for 8.5 §9's "
        "reason with the inequality reversed: a polytope has finitely many vertices, so once the "
        "boundary face is found there is nothing beyond it and the tolerance is never what "
        "stopped the loop — the duplicate check was. And the SPHERE, which was GJK's easiest case "
        "at <strong>exactly one iteration</strong>, is EPA's worst by a factor of six, climbing to "
        "the 60-expansion cap. Same shape, same support function, opposite behaviour: the nearest "
        "point of a ball to an EXTERIOR point lies on the line to its centre, so GJK's first "
        "support call lands on the answer — but the nearest BOUNDARY point from inside is on a "
        "sphere of directions, and EPA has to cover that surface with flat triangles, every one of "
        "which is a chord that under-reaches. GJK walks TO a curved surface; EPA has to COVER it. "
        "Which is the concrete argument for keeping 8.4's closed forms rather than replacing them."),

    "FIG_ACCURACY": ("l86_fig10.svg", "10",
        "<strong>What a tolerance buys, and what distance from the world origin costs.</strong> "
        "Left: the maximum depth error tracks <code>tolerance&nbsp;×&nbsp;size</code> to within a "
        "factor of 1.4 across four decades, which is 8.5 §11's finding inherited intact — "
        "<strong><code>tolerance</code> is a relative quantity, not a distance</strong>, and “set "
        "it to a millimetre” remains a category error. The two tightest rows flatten at about "
        "5e−08 because that is where <code>float</code> runs out on metre-sized geometry, not "
        "where the algorithm does. Right: the same overlap walked from two metres to 1.8 million, "
        "with two arms that are NOT two implementations — the same <code>epa.cpp</code> in both "
        "columns, differing only in the <code>convex</code> view they are handed. The relative "
        "form, which returns every support point relative to its own shape's centre so the one "
        "world-sized subtraction is exact by Sterbenz's lemma, holds at 1.5e−08&nbsp;m throughout; "
        "the naive world-space form reaches <strong>a full centimetre</strong>. An earlier version "
        "of this fixture used half extents of 0.5 and measured a difference of exactly zero, "
        "because 0.5 is a multiple of the float grid at every scale below 2²¹ — a null result from "
        "a fixture that cannot express the effect is not a null result."),

    "FIG_BUDGET": ("l86_fig11.svg", "11",
        "<strong>EPA is fourteen times GJK, and two struct definitions were worth 20.5% of "
        "it.</strong> Left, 20,000 box pairs on a release library: the SAT's full test at "
        "<strong>84.2&nbsp;ns</strong>, GJK's distance query at <strong>77.9</strong>, and GJK "
        "followed by EPA at <strong>1146.1</strong>. The work is genuinely there — eight to "
        "sixteen expansions, each scanning every face twice and rebuilding a chunk of the surface "
        "— and nothing allocates, measured by replacing the global <code>operator new</code> and "
        "counting. The number underneath it is the one worth keeping: <strong>1344.04&nbsp;ns → "
        "1068.14</strong>, measured back to back, from deleting four default member "
        "initialisers. A default member "
        "initialiser on any member makes the whole type non-trivially-default-constructible, so "
        "<code>polytope p;</code> wrote four kilobytes of zeroes the next line overwrote, and "
        "<code>expand</code>'s horizon array three more — <em>per pass</em>. The third array was "
        "left alone because removing its initialisers measured 1054.4 against 1053.1, inside the "
        "noise. Right, the same cost against penetration depth: a deeper overlap is a bigger "
        "difference set with the origin further from its boundary, so there is more polytope to "
        "build — one more argument for a solver that stops things getting deep."),
}

LISTING_META = {
    "engine/include/engine/phys/epa.hpp":     ("new", "new"),
    "engine/src/phys/epa.cpp":                ("new", "new"),
    "engine/include/engine/phys/collide.hpp": ("modified", "modified"),
    "engine/src/phys/collide.cpp":            ("modified", "modified"),
    "engine/include/engine/engine.hpp":       ("modified", "modified"),
    "engine/CMakeLists.txt":                  ("modified", "modified"),
    "demos/CMakeLists.txt":                   ("modified", "modified"),
    "demos/epa/main.cpp":                     ("new", "new"),
    "scratch/verify_86.cpp":                  ("new", "new"),
    "scratch/build_verify_86.sh":             ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_86.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l86_" + path.replace("/", "_")


LISTING_SOURCE = {path: _pin(path) for path in LISTING_META}


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
<title>8.6 — EPA: Penetration Depth · Build a Professional 3D Game Engine</title>
<meta name="description" content="Lesson 8.5 ended on an IOU: collide(convex, convex) returned plus zero when two shapes overlapped and its own doc comment called that a placeholder, because GJK cannot measure penetration. The origin is inside the Minkowski difference and the depth is a distance to its boundary, which the search never looks at. This lesson pays the IOU, and opens by measuring how bad the placeholder really was: the axis 8.5 shipped beside it was not a poor normal but the zero vector, on 200,000 overlapping pairs out of 200,000, because gjk_result::direction is only ever written on the separated path. The definition that fixes it needs two lines. A and B plus t overlap exactly when t is in the Minkowski difference, so the translations that keep two shapes overlapping are the difference set itself, and the shortest one that separates them is the nearest point of its boundary. GJK measured the distance from the origin to the set; EPA measures the distance from the origin to the set's boundary, with the same support function and the same sandwich of bounds run in the other direction: a polytope growing inside the set whose closest face is a lower bound and only ever rises, and a supporting plane through each new support point that is an upper bound and only ever falls. Then the part every write-up skips. Every description of EPA opens by starting from the tetrahedron GJK terminated with, and on two crates standing on the same floor there is none: a four-point simplex arrives zero times in fifty thousand, and what arrives instead is a flat triangle. Building the starting polytope is on the critical path and it is where three of this lesson's four bugs lived. One apex out of the plane is not enough, because the origin ends up on the tetrahedron's base and the lower bound starts and stays at zero, thirty thousand times out of thirty thousand. Six faces stitched by hand are not a polytope, because a bipyramid is convex only when each apex projects inside the triangle, and 52.85 percent of them do not, which is fixed by building the tetrahedron on one apex and adding the other through the same beneath-and-beyond expansion the main loop uses. And the textbook visibility test tears the horizon when a support point lands exactly on several face planes, which happens on 4.17 percent of crates on a floor with a worst case 61.7 percent low, on 0.01 percent at free orientations, and never at all on random hulls: the third lesson running in which the failure needs the structure a real scene is made of and is invisible to uniform random input. Euler's formula becomes a runtime test that the surface is still a surface. The sphere, which was GJK's easiest case at exactly one iteration, is EPA's worst, because a ball has no faces to reach and flat triangles can only approximate it. The SAT's minimum overlap axis turns out to have been the exact minimum translation for boxes all along, which makes 8.4 the reference this lesson is checked against. And deleting four default member initialisers took the query from 1344.04 nanoseconds to 1068.14, measured back to back. Nine measured sections, 2,028 checks, a demo that grows the polytope one keystroke at a time beside a ghost of the shape it is about to push free.">

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
    <a class="prev-l" href="08-05-gjk.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.5 — GJK: Convex Distance from a Support Function</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-07-contact-manifolds.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.7 — Contact Manifolds and Persistence</span>
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
    for name in ("a", "b", "c", "d", "e", "f"):
        with open(f"scratch/l86_body_{name}.html") as fh:
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
    # BYTES, not characters — build_74.py's note applies. The page is UTF-8 and
    # full of × − ° ₁, so `len(page)` is several thousand short of `wc -c`.
    print(f"wrote {OUT}  ({len(page.encode('utf-8')):,} bytes, "
          f"{page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
