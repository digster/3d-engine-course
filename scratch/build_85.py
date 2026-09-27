#!/usr/bin/env python3
"""Assemble docs/lessons/08-05-gjk.html.

Same pipeline as build_71.py through build_84.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l85_body_{a..f}.html, figures from scratch/figs_85.py, and
every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. Thirteen listings, and five of them are
files the rest of Module 8 edits on every single lesson: `shape.hpp` gains
whatever primitive the next lesson needs, `collide.hpp` gains an entry point per
algorithm, `gjk.hpp` is about to grow 8.6's terminal-simplex contract, and
`engine.hpp` and the two CMakeLists gain a line each. A builder that opened a
repository path would render this page's listings as they stand TODAY rather than
as they stood when the prose was written about them — and §8.4's claim that
`k_duplicate_rel2` is 1e-12 rather than the user's tolerance is a claim about a
specific constant in a specific file.

TWO LISTINGS ARE FILES 8.4 ALSO PRINTED, and they disagree with 8.4's page on
purpose. `shape.hpp` there has no capsule and no `support_local`; `collide.hpp`
there has four `axis_source` values rather than five. Both pages are archives of
their own era and the disagreement is the edit being recorded, not a drift.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson. Here
the pins are generated from LISTING_META by `_pin`, so a path added to
LISTING_META without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/08-05-gjk.html"

FIGURES = {
    "FIG_LOWERBOUND": ("l85_fig1.svg", "1",
        "<strong>The number 8.4 could not give, and the reason it could not.</strong> Two unit "
        "cubes whose centres are <code>(2,&nbsp;2,&nbsp;2)</code> apart are <strong>√3 = "
        "1.7320508&nbsp;m</strong> apart, corner to corner — and every one of the SAT's fifteen "
        "candidate axes reports <strong>1.0</strong>, because every one of them is a coordinate "
        "axis. A separating-axis gap is the <em>component</em> of the offset along a direction; "
        "the distance is its <em>length</em>; and a component is never longer than the length it "
        "came from. Nothing is broken — running the whole candidate list rather than stopping at "
        "the first separating axis gives the same 1.0. The closest features here are two "
        "<strong>vertices</strong>, and the direction joining two corners is in no list built from "
        "faces and edges. Over 200,000 random pairs the best-of-fifteen answer averages 0.9817 of "
        "the truth, which sounds harmless until you read the tail: <strong>28.7% of separated "
        "pairs are more than 1% low and 5.7% are more than 10% low</strong>, with a worst case of "
        "0.6901."),

    "FIG_MINKOWSKI": ("l85_fig2.svg", "2",
        "<strong>The substitution the whole algorithm rests on, and the two numbers in it are the "
        "same number.</strong> Left: a pentagon and a quadrilateral with the segment between their "
        "closest points, <strong>1.6530&nbsp;m</strong>. Middle: the same pair as ONE set — the "
        "Minkowski difference <code>A ⊖ B</code>, every point of A minus every point of B, built "
        "here by taking all twenty pairwise differences and hulling them — with the origin marked "
        "and the arrow to its nearest point measuring <strong>1.6530&nbsp;m</strong>. Two "
        "equivalences, each two lines to prove: the shapes overlap exactly when the difference "
        "contains the ORIGIN, and their distance is the distance from the origin TO the "
        "difference. Right: the same construction with the shapes overlapping, and the origin "
        "inside. A question about a relationship has become a question about a location — and a "
        "location is something you can walk toward, which is what §6 does."),

    "FIG_SUPPORT": ("l85_fig3.svg", "3",
        "<strong>One function, and it determines the set completely.</strong> Left: three "
        "directions, three farthest points, three supporting lines. Each direction gives a "
        "half-plane that contains the whole set and touches it, so the intersection of all of them "
        "contains the set — and contains nothing else, because any outside point has a separating "
        "plane whose normal excludes it. A convex set therefore IS its support function: two "
        "convex sets with the same one are the same set. That is why <code>convex.hpp</code> can "
        "be a single function pointer, and why <code>gjk.cpp</code> never learns what a box is. "
        "Right: the sum rule. A capsule is <code>segment ⊕ ball</code>, and support functions ADD "
        "over Minkowski sums because <code>x</code> and <code>y</code> are chosen independently — "
        "so the support point is <code>±h·axis</code> plus <code>r·normalise(d)</code>, two terms, "
        "neither aware of the other. Set <code>half_height</code> to zero and the first term "
        "vanishes: <strong>20,000 of 20,000 support points come back bit-identical to the "
        "sphere's</strong>."),

    "FIG_LOOP": ("l85_fig4.svg", "4",
        "<strong>Three iterations, on figure 2's difference set, with nothing drawn by hand.</strong> "
        "The figure script runs the same loop as <code>engine/src/phys/gjk.cpp</code> with the "
        "tetrahedron case removed, so every point here was searched. Each panel shows the simplex "
        "before the step in amber, <code>v</code> in purple, the dashed continuation along "
        "<code>−v</code> where the next support is taken, and the returned point <code>w</code> in "
        "green. Read the tables: the bracket starts a metre wide "
        "(<strong>1.8358 against 0.8117</strong>), narrows to "
        "<strong>1.7112 against 1.5839</strong>, and closes. The way it closes is the part worth "
        "noticing — the support function returns a point the simplex already has, which on a "
        "polytope is how the search FINISHES rather than how it fails: there is nothing further "
        "out to find."),

    "FIG_BOUNDS": ("l85_fig5.svg", "5",
        "<strong>The answer is squeezed from both sides, and both sides are provable.</strong> The "
        "purple line is <code>|v|</code>, the length of a vector between two REAL surface points, "
        "so the true distance cannot exceed it; it only falls, because each new simplex contains "
        "the last. The green line is the distance from the origin to the supporting plane through "
        "<code>w</code>, and since <code>w</code> minimises <code>dot(x, v)</code> over the whole "
        "difference set, nothing in the set is nearer than that; it only rises. They meet at "
        "<strong>1.653025&nbsp;m from both sides with a slack of exactly zero</strong> — the same "
        "number figure 2 measured by brute force between the original shapes. This is what lets "
        "§E verify GJK without a second GJK: the two bounds are recomputable from the OUTPUT "
        "alone, in two support calls, and they hold whatever produced it."),

    "FIG_SIMPLEX": ("l85_fig6.svg", "6",
        "<strong>Compute every candidate, or decide which one to compute — and the shortcut fails "
        "exactly where the algorithm lives.</strong> Left: the closest point of a triangle is in "
        "its interior or on one of three edges, and a CLAMPED edge already contains its own two "
        "endpoints, so four candidates cover all seven Voronoi regions with no case analysis at "
        "all. Right: the same failure one dimension down, where it can be drawn. The nearest point "
        "OF a segment is <strong>1.252&nbsp;m</strong> away; the foot of the perpendicular on its "
        "infinite LINE is <strong>0.350&nbsp;m</strong> away, and a region test that misfires "
        "returns the second — a point that is not on the simplex at all. The textbook version's "
        "three region tests are signed AREAS, differences of nearly equal products, and GJK's "
        "whole job is to drive the simplex onto the closest feature, so by the last two iterations "
        "its triangles are slivers. Measured: <strong>3 failures in 200,000 fat triangles and "
        "1,048 in 200,000 slivers</strong>, worst case 14.5% wrong. Computing all four costs "
        "21.7&nbsp;ns against a 115&nbsp;ns query."),

    "FIG_DEMO": ("l85_fig7.svg", "7",
        "<strong>The world beside the difference set, which is the substitution made "
        "watchable.</strong> Left frame, preset 2: a capsule against a fourteen-vertex hull — "
        "neither of which 8.4's test can be applied to at all — with the two witness points and "
        "the segment between them on the left panel, and on the right the difference set outlined "
        "by tracing its own support function, the origin as a white cross, and <code>v</code> in "
        "pink. Right frame, preset 1: two crates overlapping, the moving one drawn red, and the "
        "gold simplex now a TETRAHEDRON around the cross with no <code>v</code> to draw, because "
        "there is no nearest point when the origin is inside. <strong>[S]</strong> steps the "
        "search one iteration at a time and the <code>|v|</code> readout only ever goes down — "
        "that monotonicity is a theorem, and §7.4 leans on it twice. Note that the outline is a "
        "SHADOW: the cross being inside it proves nothing, which is 8.4 §3's point about "
        "projections and the reason this panel shows a search rather than a picture."),

    "FIG_FLAT": ("l85_fig8.svg", "8",
        "<strong>Every description of EPA begins “start from the tetrahedron GJK terminated "
        "with”, and on the commonest arrangement in a game there is none.</strong> A hundred "
        "thousand pairs of unit cubes standing on the same floor at arbitrary yaw — 8.4 §F's "
        "arrangement, and it called it the default one because two objects on a floor share an up "
        "axis whatever else they do. <strong>Zero tetrahedra</strong>, including across all 65,633 "
        "pairs overlapping by more than 10&nbsp;cm. Tilt one box by a SINGLE DEGREE and 77,326 "
        "appear. The cause is exact rather than statistical: both boxes' <code>y</code> half "
        "extents multiply the same world direction and <code>support_local</code>'s "
        "<code>&gt;= 0</code> tie-break picks the same sign for both, so every vertex of the "
        "difference comes out with <code>w.y = (+h) − (+h) − 0 = 0</code>. All 100,000 simplices "
        "lay entirely in that plane. The difference set is three-dimensional; it is the SEARCH "
        "that never leaves its equator."),

    "FIG_ITERATIONS": ("l85_fig9.svg", "9",
        "<strong>Three columns flat and one not, and the odd one out is not the one you would "
        "guess.</strong> Left: the distribution over 117,417 separated box pairs — mean "
        "<strong>3.52</strong>, maximum <strong>10</strong>, which is what makes a 64-iteration "
        "cap comfortable rather than generous. Right: mean iterations as the tolerance tightens "
        "through five decades. A box and a hull are POLYTOPES, so the simplex can only improve "
        "finitely often and GJK does not converge on them, it FINISHES — the tolerance is never "
        "what stopped it. The SPHERE is flat at ONE for a different reason: the nearest point of a "
        "ball to an exterior point lies on the line to its centre, so the very first support call, "
        "taken along <code>delta</code>, lands ON the answer. Only the CAPSULE genuinely "
        "converges, at almost exactly one extra pass per decade — because the difference of two "
        "capsules is a parallelogram rounded by a ball, and the nearest point can sit on the "
        "curved part where neither operand decides the answer alone."),

    "FIG_FLOOR": ("l85_fig10.svg", "10",
        "<strong>Two different things give out, at two different gaps, and keeping them apart is "
        "the whole section.</strong> Left: two turned 2&nbsp;m cubes walked from a metre apart to "
        "a micrometre. THE PROOF goes first. The termination test's threshold is proportional to "
        "<code>|v|²</code> — the gap SQUARED — while its rounding error is of order "
        "<code>ε·|v|·|w|</code> — the gap times the SHAPE — so below <code>ε·|w|/tol ≈ "
        "4.8e−03&nbsp;m</code> the comparison is noise against noise and the loop stops because "
        "<code>|v|</code> stopped falling. The ANSWER is still right, and still an UPPER bound; "
        "what is gone is the certificate. THE ANSWER goes second, at <code>|v| ≤ tol·|w|</code> — "
        "a deliberate contact margin. Right: measured by bisection on cubes spanning four decades, "
        "that margin is <strong>2.11 × tolerance × size</strong> throughout. Which is the finding: "
        "<code>tolerance</code> is a RELATIVE quantity, not a distance, and “set it to a "
        "millimetre” is a category error."),

    "FIG_ORIGIN": ("l85_fig11.svg", "11",
        "<strong>8.4 §11 named this wall; this is the cure, measured.</strong> One pair of boxes "
        "at a fixed diagonal offset, walked away from the world origin — the distance is the same "
        "at every stop by construction, so any change is arithmetic rather than geometry, and "
        "because the boxes are axis aligned there is an EXACT reference in double. Two arms, and "
        "they are not two implementations: <code>gjk.cpp</code> is the same code in both columns "
        "and only the <code>convex</code> view differs, which is what makes the comparison mean "
        "anything. <strong>The naive arm's error tracks ulp(position) at a fixed fraction all the "
        "way up, reaching 1.03e−02&nbsp;m at a thousand kilometres; the relative arm's tracks "
        "nothing and stays at 1.07e−07&nbsp;m — 96,270× better.</strong> A box's support point is "
        "three additions; done in world space each rounds to half an ulp of the POSITION, six "
        "times per iteration, while the relative form's single world-sized subtraction "
        "<code>b.origin − a.origin</code> is EXACT by Sterbenz's lemma, which two things about to "
        "collide always satisfy."),

    "FIG_BUDGET": ("l85_fig12.svg", "12",
        "<strong>GJK is slower on boxes, and it should be.</strong> Left, 20,000 box pairs on a "
        "release library: the SAT's boolean test at <strong>8.4&nbsp;ns</strong> and its full "
        "answer at <strong>49.5&nbsp;ns</strong>, against GJK's <strong>73.2</strong> and "
        "<strong>114.7</strong>. The SAT knows the shape; GJK searches for it, calling out through "
        "a function pointer six times and running a four-candidate simplex solver two or three "
        "times. What GJK buys is the metres and the generality, not the speed — which is exactly "
        "why an engine keeps BOTH tests rather than replacing one with the other, and why 8.8's "
        "broadphase will reach for the cheap one first. Right, where generality does cost "
        "something: a hull's support is linear in its vertex count, <strong>3.9&nbsp;ns at 8 "
        "vertices and 51.5&nbsp;ns at 128</strong>, and the whole query is that line times roughly "
        "twice the iteration count. The abstraction itself is free: 1.908&nbsp;ns direct against "
        "1.906&nbsp;ns through <code>convex::support</code>, bit-identical."),
}

LISTING_META = {
    "engine/include/engine/phys/shape.hpp":       ("modified", "modified"),
    "engine/src/phys/shape.cpp":                  ("modified", "modified"),
    "engine/include/engine/phys/convex.hpp":      ("new", "new"),
    "engine/include/engine/phys/gjk.hpp":         ("new", "new"),
    "engine/src/phys/gjk.cpp":                    ("new", "new"),
    "engine/include/engine/phys/collide.hpp":     ("modified", "modified"),
    "engine/src/phys/collide.cpp":                ("modified", "modified"),
    "engine/include/engine/engine.hpp":           ("modified", "modified"),
    "engine/CMakeLists.txt":                      ("modified", "modified"),
    "demos/CMakeLists.txt":                       ("modified", "modified"),
    "demos/gjk/main.cpp":                         ("new", "new"),
    "scratch/verify_85.cpp":                      ("new", "new"),
    "scratch/build_verify_85.sh":                 ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_85.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l85_" + path.replace("/", "_")


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
<title>8.5 — GJK: Convex Distance from a Support Function · Build a Professional 3D Game Engine</title>
<meta name="description" content="8.4's Separating Axis Theorem answered whether two things touch and was honest about the question it could not answer. Two unit cubes whose centres are (2,2,2) apart are the square root of three metres apart, corner to corner, and every one of the SAT's fifteen candidate axes reports 1.0, because every one of them is a coordinate axis and the gap along an axis is the offset's component rather than its length. The SAT is not wrong; it is 42.3 percent low, and on random box pairs it is more than ten percent low 5.7 percent of the time. Worse, those fifteen candidates were a fact about boxes: hand the theorem a cylinder and there is no list to write down. This lesson replaces it with an algorithm that has no list at all. Two shapes become one set, the Minkowski difference, and a question about a pair becomes a question about a point: does that set contain the origin? You cannot build the set and never need to, because its support function is two calls to theirs. GJK then searches it with a simplex of at most four points, carrying an upper bound that is a real vector between two real surface points and a lower bound that is a supporting plane with the origin outside it, so when they meet the distance is proven rather than believed, which is what lets the harness verify GJK without a second GJK. Then the things nobody warns you about, all measured. The textbook simplex solver everyone copies returns a point that is not on the triangle when the triangle is a sliver, 1,048 failures in 200,000 with a worst case 14.5 percent wrong, and a sliver is not an edge case in GJK but the last two iterations of every query, so we compute four candidates instead of deciding between seven regions. The length of v is monotone by construction, which makes a two-line check both a terminator and a corruption detector: it caught a search converging cleanly to 0.0233827 metres and then jumping to 0.0674522. GJK terminates exactly on polytopes and only converges on curved shapes, so box, hull and sphere are flat against tolerance while a capsule costs one more pass per decade. Two boxes standing on the same floor produce a difference set in which every vertex has y exactly zero, so the search is confined to a plane and can never build a tetrahedron, zero out of a hundred thousand against 77,326 once one box is tilted a single degree, which is a problem for the next lesson's expanding polytope algorithm and is named here. There is a resolution floor and two separate things give out at two different gaps: the certificate at epsilon times the shape size over the tolerance, and the answer at a contact margin measured to be 2.11 times tolerance times size across four decades, which is why the tolerance is a relative quantity and not a distance. And 8.4's precision wall is fixed rather than repeated: taking every support point relative to its own shape's centre makes the error 96,270 times smaller a thousand kilometres from the origin, by Sterbenz's lemma. Thirty-three checks, every separated pair certified from the shape data alone.">

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
    <a class="prev-l" href="08-04-collision-primitives.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.4 — Collision Primitives: Spheres, AABBs, OBBs, and the SAT</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-06-epa.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.6 — EPA: Penetration Depth</span>
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
        with open(f"scratch/l85_body_{name}.html") as fh:
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
