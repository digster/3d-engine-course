#!/usr/bin/env python3
"""Assemble docs/lessons/08-04-collision-primitives.html.

Same pipeline as build_71.py through build_83.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l84_body_{a,b,c,d,e}.html, figures from scratch/figs_84.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE, for the fourth lesson running. Eleven
listings, and three of them are files the rest of Module 8 will edit on every
single lesson: `shape.hpp` gains a capsule in 8.5's exercises and a convex hull
in 8.5 proper, `collide.hpp` gains GJK's entry point in 8.5 and the manifold's
in 8.7, and `engine.hpp` gains a line per lesson. A builder that opened a
repository path would render this page's listings as they stand TODAY rather
than as they stood when the prose was written about them, and §7.6's claim that
this file ships "two independent formulations" is a claim about specific
functions in a specific file.

ONE LISTING IS A MOVED FILE. `engine/include/engine/math/bounds.hpp` was
`engine/include/engine/gfx/bounds.hpp` until this lesson, and 6.8's page prints
the old path with the old content — correctly, because a page is an archive of
its own era. The pin here holds the post-move file under the post-move path, and
the two pages disagreeing is the move being recorded rather than a drift.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson. Here
the pins are generated from LISTING_META by `_pin`, so a path added to
LISTING_META without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/08-04-collision-primitives.html"

FIGURES = {
    "FIG_SHADOW": ("l84_fig1.svg", "1",
        "<strong>The whole algorithm, as a lamp and a wall — and the one shape of object it "
        "refuses to work on.</strong> Left: a direction whose two shadows do not meet. That is a "
        "complete proof that the objects are apart, and it needs no arithmetic at all: a point "
        "belonging to both objects would cast a point belonging to both shadows. Middle: the same "
        "two objects under a different lamp, where the shadows overlap and the test says "
        "<em>nothing</em> — not that they are touching, only that this particular direction "
        "has failed to prove otherwise. A gap is evidence; an overlap is the absence of evidence, "
        "and the entire lesson is about how many directions you must try before absence of "
        "evidence becomes evidence of absence. Right: why the theorem says <em>convex</em>. A ball "
        "resting in the mouth of a cup, touching nothing — and on every direction the shadows "
        "overlap, because the cup’s shadow is solid while the cup is not. The gap is not "
        "<em>between</em> the shapes in any direction; it is tucked inside the concavity of one of "
        "them, and a projection cannot see inside a shape."),

    "FIG_PROJECT": ("l84_fig2.svg", "2",
        "<strong>The one formula everything past here depends on, worked through with real "
        "numbers.</strong> A point of a box is <code>c + Σ sᵢ hᵢ uᵢ</code> with "
        "each <code>sᵢ</code> in <code>[−1, 1]</code>; dot that with a direction and the "
        "three terms are <em>independent</em>, so the largest value comes from choosing each sign "
        "to match its own term. That is what the absolute values in <code>r(L) = Σ hᵢ "
        "|uᵢ · L|</code> are: three choices of corner, made one axis at a time. For a "
        "box with half extents (2, 1, 0.5) turned 30° about <code>z</code> and projected onto "
        "the world <code>x</code> axis, that is <strong>2.232051 m</strong>, and "
        "<code>bounds_of()</code> — which is this formula run three times, once per world "
        "axis — agrees to the digit. Note the middle term: <code>u₁ · L</code> is "
        "<em>negative</em> and it contributes <code>+0.5</code>, which is the absolute value doing "
        "its job. Drop the absolute values and you get the projection of one particular corner "
        "instead: wrong on <strong>15,089 of 20,000</strong> random boxes, by up to 100%."),

    "FIG_FIFTEEN": ("l84_fig3.svg", "3",
        "<strong>Where the fifteen come from, and why the second kind is the one people "
        "forget.</strong> Slide one box around the outside of the other and there are exactly two "
        "ways to be touching. Left: a face of one lies flat against the other, so the face’s "
        "normal is the direction to test — six faces per box but only <strong>three distinct "
        "normals</strong>, because opposite faces differ only in sign and <code>L</code> and "
        "<code>−L</code> cast the same shadow. Middle: an edge of one crosses an edge of the "
        "other, like two pencils laid across each other. The contact plane contains both edge "
        "directions, so its normal is perpendicular to both — which is their cross product, "
        "and which is a direction <em>neither box has a face in</em>. Twelve edges per box, three "
        "distinct directions, <strong>3 × 3 = nine</strong> pairs. Right: six plus nine. These "
        "are the face normals of the Minkowski difference <code>A ⊖ B</code>, whose faces come "
        "either from a face of one box or from one edge of each sweeping past the other."),

    "FIG_CROSSED": ("l84_fig4.svg", "4",
        "<strong>The counterexample, built rather than found — all fifteen candidates for two "
        "planks with five centimetres of daylight between them.</strong> The construction is the "
        "argument: pick the separating direction <em>first</em>, as the cross product of one long "
        "edge from each plank, then place the second plank along it just far enough to clear both "
        "shadows. Nothing about that placement involves a face normal. The result is that the six "
        "face axes report overlaps from <strong>0.11772 m to 3.12792 m</strong> — not "
        "marginal, not “nearly apart”, deeply overlapping by any reading — and the "
        "single axis <code>a₀ × b₀</code> reports a gap of <strong>+0.05000 m</strong>, "
        "which re-proves in double precision as +0.0500000. Delete the nine cross products and this "
        "is a collision: two planks you can see between, reported as touching, by an engine that "
        "will then generate a contact and shove them apart."),

    "FIG_DEMO": ("l84_fig5.svg", "5",
        "<strong>The same geometry, the same frame, one keystroke apart.</strong> "
        "<code>demos/collide</code>, preset 2. The right panel is the fifteen candidate gaps as "
        "bars against a zero line: left of it means this direction found no gap, right of it means "
        "the direction separates them. Left: all fifteen candidates, the box drawn green because "
        "the amber bar — an edge-edge candidate — proves them apart. Right: the same "
        "instant with <strong>[F]</strong> held, which keeps the six face normals and throws the "
        "nine cross products away. The bars do not move, because the demo still computes them all "
        "for the display; the verdict does, and the box turns red. The short stubs sitting "
        "<em>on</em> the line rather than beside it are candidates the degeneracy guard skipped, "
        "drawn differently on purpose: a zero-length bar would read as “exactly touching”, "
        "which is a different claim."),

    "FIG_MTV": ("l84_fig6.svg", "6",
        "<strong>The minimum translation vector is the shallowest escape, which is the right "
        "answer only while it is small.</strong> Left: two unit cubes whose centres are "
        "<code>(1.5, 0.2, 0.1)</code> apart overlap by 0.5 m on <code>x</code> and by 1.8 and 1.9 "
        "on the other two, so the correction is <strong>0.5 m sideways</strong> — even if the "
        "cube arrived by falling. That is what a resting contact needs, and the smallest correction "
        "that fixes the problem. Right: the same two cubes nearly concentric, centres "
        "<code>(0.1, 0.05, 0)</code> apart, where the shallowest escape from a one-metre cube is "
        "still <strong>1.9 metres</strong>. The MTV has become a teleport, and past halfway it can "
        "push an object out the face it never entered. This is a <em>regime</em> rather than a bug, "
        "and it is why 8.6 creates contacts before objects touch and 8.10 removes penetration "
        "gradually rather than in one frame."),

    "FIG_SPHERE_BOX": ("l84_fig7.svg", "7",
        "<strong>Twenty-seven cases, answered by three clamps that never ask which case it "
        "is.</strong> A point can be inside a box, beyond one of six faces, beyond one of twelve "
        "edges, or beyond one of eight corners — and the closest point is correspondingly on a "
        "face, an edge, a corner, or is the point itself. The case analysis is an afternoon and a "
        "bug. It is also unnecessary: <strong>a box is a product of three intervals</strong>, so "
        "its axes do not interact and each coordinate of the closest point depends only on the same "
        "coordinate of <code>p</code>. Clamp on two axes and you land on an edge; on three and you "
        "land on a corner; on none and you get the point back. That last case is the one that "
        "matters: when the centre is inside, the clamp does nothing, the distance is zero and the "
        "direction is <code>0/0</code>. It is <strong>8,829 of 100,000</strong> random placements, "
        "and it is exactly what a sphere looks like one frame after it tunnels into a wall."),

    "FIG_PARALLEL": ("l84_fig8.svg", "8",
        "<strong>The degenerate candidate axis is the default arrangement, and the folklore about "
        "it is about a formulation rather than about the algorithm.</strong> Left: two objects "
        "standing on the same floor have parallel up axes whatever their yaw, so one of the nine "
        "cross products is degenerate in the single most common configuration a game produces — "
        "and it is never <em>exactly</em> degenerate, because both axes arrive through a "
        "quaternion-to-matrix conversion and their cross product comes out around "
        "<strong>1.22e−07</strong> rather than zero. Right, measured on 400 genuinely "
        "overlapping pairs per row against a double-precision reference: the <strong>normalised "
        "form never fails, at any angle, with its guard removed entirely</strong> — which is a "
        "theorem, not luck, because if two convex bodies overlap then no direction separates them "
        "and an axis made of rounding error is still a direction. The unnormalised form in the "
        "books fails <strong>188 times in 400</strong> without its epsilon, because there every "
        "term of the test scales with <code>|aᵢ × bⱼ|</code> while its rounding error "
        "does not."),

    "FIG_PRECISION": ("l84_fig9.svg", "9",
        "<strong>The same two boxes, the same millimetre, and an answer that decays with nothing "
        "but distance from the origin.</strong> The red curve is the error in the reported gap; the "
        "grey one is <code>ulp(d)</code>, the spacing between representable <code>float</code>s at "
        "that distance. They track each other, and they cross the quantity being measured at about "
        "<strong>ten kilometres</strong> — at which point a millimetre is no longer a "
        "representable difference and the test cannot see it. At 100 km the reported gap is "
        "<strong>−0.000e+00</strong>: two boxes a millimetre apart, reported as touching. "
        "Nothing about the algorithm changed and nothing about the boxes changed; the subtraction "
        "<code>b.centre − a.centre</code> simply takes two numbers the size of the WORLD to "
        "produce one the size of a CRATE. Recomputing in double from the same float centres still "
        "gives 9.765625e−04, which is the control: the information was gone before "
        "<code>collide</code> was called."),

    "FIG_BUDGET": ("l84_fig10.svg", "10",
        "<strong>Two ratios that decide how a physics engine is structured, and neither of them is "
        "about the arithmetic.</strong> Left, cost per call: a sphere test is <strong>1.206 ns</strong> "
        "and a worst-case box test is <strong>73.323 ns</strong>, a factor of sixty — which is "
        "the entire justification for asking a cheap conservative question first and an expensive "
        "exact one only about the survivors. Measured directly, a bounding-sphere reject in front "
        "of <code>overlaps</code> takes the spread population from 10.090 ns to "
        "<strong>2.202 ns</strong>, rejecting 17,325 of 20,000 pairs and none of the 1,265 real "
        "overlaps. Right, how many of the fifteen a query actually examines: "
        "<strong>91% of separated pairs exit within three axes</strong> and the 6.4% that run the "
        "whole list are exactly the ones that overlap. That histogram is why face normals are "
        "tested first — an ordering chosen from a measurement rather than from taste."),
}

LISTING_META = {
    "engine/include/engine/math/bounds.hpp":      ("modified", "moved"),
    "engine/include/engine/phys/shape.hpp":       ("new", "new"),
    "engine/src/phys/shape.cpp":                  ("new", "new"),
    "engine/include/engine/phys/collide.hpp":     ("new", "new"),
    "engine/src/phys/collide.cpp":                ("new", "new"),
    "engine/include/engine/engine.hpp":           ("modified", "modified"),
    "engine/CMakeLists.txt":                      ("modified", "modified"),
    "demos/CMakeLists.txt":                       ("modified", "modified"),
    "demos/collide/main.cpp":                     ("new", "new"),
    "scratch/verify_84.cpp":                      ("new", "new"),
    "scratch/build_verify_84.sh":                 ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_84.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l84_" + path.replace("/", "_")


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
<title>8.4 — Collision Primitives: Spheres, AABBs, OBBs, and the SAT · Build a Professional 3D Game Engine</title>
<meta name="description" content="Eighty-six lessons in, nothing in this engine has a size the simulation knows about: make_box takes half extents, builds an inertia tensor and throws them away, so two crates dropped on the same spot share one cubic metre of world forever. This lesson gives them shapes and then answers the question every physics engine is organised around. The answer is one genuinely beautiful idea: hold two objects up to a lamp, and if their shadows are apart then the objects are apart, which needs no arithmetic at all. The converse — apart implies some lamp shows the gap — is the Separating Axis Theorem, it is true only for convex shapes, and a ball resting in the mouth of a cup shows exactly what convexity buys. For two boxes the infinite search over directions collapses to fifteen candidates: three face normals each, and nine cross products of one edge direction from each. Six is not enough, and we build the counterexample rather than hunting for it — two planks crossed in mid air with five centimetres of daylight between them, where all six face axes report overlaps of up to 3.13 metres and only the cross product of one long edge from each sees the gap. On two hundred thousand random pairs that case is one in twenty-two of everything six axes call a collision, and when boxes genuinely overlap the shallowest way out is an edge-edge axis half the time. Then the things nobody warns you about. The minimum translation vector is the shortest escape, which stops being the right escape the moment an object is half buried: two cubes ten centimetres out of alignment are corrected by 1.9 metres. Three clamps find the closest point on a box without enumerating twenty-seven cases, and return a zero normal on the 8,829 of 100,000 spheres whose centre is inside, which is what a sphere looks like the frame after it tunnels into a wall. Two crates on the same floor have parallel up axes at every yaw, so a degenerate cross product is the default rather than an exotic case — and the folklore about guarding it turns out to be about a formulation we are not using: with its guard removed the normalised test produces zero false separations, because an axis made of pure rounding error is still a direction and no direction separates overlapping bodies, while the unnormalised version in the books fails 188 times in 400. And a millimetre gap measured a kilometre from the origin is 2.3 percent wrong, at ten kilometres is unresolvable, and at a hundred reports exactly zero, with the algorithm and the boxes unchanged. Every separated pair here is verified by re-proving its own certificate in double precision rather than by trusting a second implementation.">

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
    <a class="prev-l" href="08-03-angular-dynamics.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.3 — Angular Dynamics: Torque and the Inertia Tensor</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-05-gjk.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.5 — GJK: Convex Distance from a Support Function</span>
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
    for name in ("a", "b", "c", "d", "e"):
        with open(f"scratch/l84_body_{name}.html") as fh:
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
