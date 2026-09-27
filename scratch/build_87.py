#!/usr/bin/env python3
"""Assemble docs/lessons/08-07-contact-manifolds.html.

Same pipeline as build_71.py through build_86.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l87_body_{a..f}.html, figures from scratch/figs_87.py, and
every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. Eleven listings, and three of them are
files that 8.8 onward will keep editing: `shape.hpp` gains a primitive whenever
one is added, `convex.hpp` gains a query, and both CMakeLists gain a line per
lesson. 8.9's solver will also widen `contact_point` — `normal_impulse` and
`tangent_impulse` are declared here and written by nobody — so a builder that
opened a repository path would render this page's listings as they stand THEN
rather than as they stood when the prose was written about them. §11's claim
that `sizeof(contact_face)` is 124 bytes is a claim about a specific struct.

THREE LISTINGS ARE FILES 8.5 AND 8.6 ALSO PRINTED, and they disagree with those
pages on purpose. `shape.hpp` there has no `contact_face` and no `support_face`;
`convex.hpp` there has one function pointer rather than two; `engine.hpp` and
both CMakeLists are a line shorter. Each page is an archive of its own era and
the disagreement is the edit being recorded, not drift.

AND ONE THING THIS LESSON CORRECTED IN ITS OWN HEADERS RATHER THAN IN A PIN.
Several doc comments in `manifold.hpp` and `shape.hpp` were written with
PLACEHOLDER numbers while the harness was still being built — "0.0 mm of depth
and 1.4% of the query", "a median of 0.0021 degrees" — and every one of them was
replaced with the measured value before the pins were taken. A doc comment that
describes what the code was meant to do is worse than none: 8.6 §1 paid for that
lesson at 200,000 zero vectors out of 200,000, and the pins here were taken AFTER
the sweep rather than before it.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson. Here
the pins are generated from LISTING_META by `_pin`, so a path added to
LISTING_META without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/08-07-contact-manifolds.html"

FIGURES = {
    "FIG_PIN": ("l87_fig1.svg", "1",
        "<strong>A contact force through one point exerts no torque about itself.</strong> Left: a "
        "crate tilted on a floor with a single contact point. The only force available acts through "
        "that point, so it and the weight form a couple and the crate turns — and as it turns, the "
        "contact moves to the other corner and the couple reverses. Right: four points, whose "
        "convex hull is the <em>support polygon</em>, and inside which the resultant of four "
        "independent non-negative impulses can act anywhere. The table is the measurement, with the "
        "same crate, the same gravity, the same solver and the same normal in both arms: only the "
        "NUMBER of contact points differs. Read the middle columns rather than the first. A single "
        "point does not fail to hold a crate up — it holds one up by <strong>chattering</strong>, "
        "sweeping the face faster than the body can respond, at <strong>27.99 °/s of permanent "
        "rocking and 21.27 mm of sink</strong> against 5.2 and 1.16. Which is exactly why "
        "single-point contact looks nearly fine in a demo and is unusable in a game."),

    "FIG_POLYGON": ("l87_fig2.svg", "2",
        "<strong>A body rests exactly when its centre of mass projects inside the support "
        "polygon.</strong> The same crate slid off the edge of a ledge, with a dashed line dropped "
        "from its centre of mass and the contact set the manifold returned drawn as a bar "
        "underneath. At 0.00 and 0.40 m of overhang the line falls inside the bar and the crate "
        "rests to within a tenth of a degree; at 0.55 it falls outside and the crate topples 156°. "
        "Statics puts the crossing at exactly half a metre, and <strong>nothing in the simulation "
        "was told there is a ledge</strong> — the support polygon is measured from the manifold. "
        "Below, the other half of the same statement: the moment such a polygon can carry is "
        "<code>m·g·(w/2)</code>, predicted at <strong>49.050 N·m</strong> and bisected at "
        "<strong>47.869</strong>. The 2.41% is not error; it is the penetration the solver allows, "
        "which puts the crate's effective lip a hair inside its geometric one."),

    "FIG_SWING": ("l87_fig3.svg", "3",
        "<strong>The depth is continuous and the direction is not, and that is a property of a "
        "minimum rather than a defect in EPA.</strong> A cube pressed 50 mm into a wall that ENDS, "
        "seen from above, slid along the wall past its end. Two escapes are available: out through "
        "the face, which always costs 50 mm, and out past the end, which costs "
        "<code>1 − (z − 0.2)</code> and shrinks as the cube advances. They are equal at "
        "<code>z = 1.15</code>, and past it the other one is shorter. Walked in ten-micrometre "
        "steps, the worst change in the normal between two adjacent steps is <strong>90.00°</strong> "
        "and the worst change in the depth is <strong>0.01001 mm</strong> — the step size times the "
        "slope. Every implementation of penetration depth has this, because <code>min</code> of two "
        "smooth functions is not smooth where the argmin changes. 8.7 does not remove it; it makes "
        "the contact SET survive it."),

    "FIG_ARGMAX": ("l87_fig4.svg", "4",
        "<strong>A support function determines a convex set and cannot name one of its faces.</strong> "
        "Left: a box seen edge on, with its top face drawn thick as the argmax set and a fan of "
        "query directions inside that face's normal cone — every one of which has the SAME "
        "four-corner argmax. <code>support</code> has to return one of them, and which one is "
        "decided by a tie-break on three dot products that are all zero in exact arithmetic: "
        "measured over 4,000 directions from a cone one thousandth of a degree wide, it returns "
        "<strong>all four corners</strong>. That is deterministic and it is arbitrary, and those "
        "are not the same thing. The control is what makes it mean something: the same jitter about "
        "a direction pointed at a CORNER returns <strong>1 of 8</strong>, so the ambiguity belongs "
        "to the query rather than to the function and appears exactly where the feature is bigger "
        "than a point — which is exactly where a manifold is needed. Note what this lesson did NOT "
        "need: EPA's winning triangle, whose three vertices are support points with the same "
        "ambiguity intact, and which is a piece of the difference set's face rather than the whole "
        "of it."),

    "FIG_FACES": ("l87_fig5.svg", "5",
        "<strong>Each primitive presents a different kind of contact feature, and the counts are "
        "the features these shapes actually have.</strong> A sphere returns one vertex, because a "
        "ball has no flat feature and fabricating a disc there is how you get a ball that will not "
        "roll. A capsule returns one for its caps and two for the line where its cylindrical side "
        "meets the supporting plane — its only flat feature, and one-dimensional. A box returns its "
        "dominant face, chosen by an argmax over three numbers with no tolerance anywhere. And a "
        "<code>hull</code> returns the vertices ON its supporting plane, which is the one that needs "
        "a tolerance, because 8.5 made a hull a point cloud with no face list on purpose. The sweep "
        "below is the honest part: <strong>there is no value of the gather tolerance that is "
        "right</strong>. Too tight and a tessellated cylinder's side contact drops to a bare edge — "
        "two points, no support polygon, and the thing rolls when it should not. Too loose and three "
        "flat faces merge into one polygon bulging 34 mm. The question is topological and a point "
        "cloud has no topology; the only real fix is to give <code>hull</code> its faces."),

    "FIG_REFINC": ("l87_fig6.svg", "6",
        "<strong>One of the two faces supplies the clipping planes, and which one changes every "
        "contact id.</strong> Left: a crate tilted 9° on a floor. The floor's face normal is exactly "
        "along the contact normal and the crate's is 9° off, so the floor becomes the REFERENCE: "
        "its face supplies the side planes and the plane every depth is measured from. Right, "
        "swept over 40,000 overlapping box pairs against 8.4's SAT — which 8.6 §8 proved is the "
        "<em>exact</em> minimum translation for two boxes, and is therefore the reference to measure "
        "against. The last column is <code>acos(face_cos)</code> <strong>exactly</strong>, which is "
        "what makes this a knob rather than a magic number: choose the largest normal error a solver "
        "can live with and take its cosine. Tightening it costs too — the fraction of contacts "
        "classified as face contacts falls from 59.3% to 52.8% and the mean contact count with it, "
        "each one a body that was resting on a polygon and is now resting on a line. Below, whose "
        "normal the manifold reports: on a ball, which is EPA's worst case, the reference face's "
        "normal is <strong>bit-exact +y on 4,000 frames out of 4,000</strong> while EPA's wobbles by "
        "a thousandth of a degree — and a normal that is piecewise constant does not shake a stack "
        "while one that is merely accurate does."),

    "FIG_CLIP": ("l87_fig7.svg", "7",
        "<strong>Sutherland–Hodgman, one side plane per panel — and every polygon here was CUT "
        "rather than drawn.</strong> The figure script runs the same clip as "
        "<code>engine/src/phys/manifold.cpp</code> reduced to the plane. Blue is the reference face; "
        "the grey dashed square is the incident face before any cutting; amber is what survives so "
        "far, with its vertices marked. The green dashed line is the plane being applied and the "
        "short green arrow is its OUTWARD normal, built as <code>cross(edge, normal)</code> — which "
        "points away from the polygon <em>only</em> if the winding is counter-clockwise about that "
        "normal. Reverse it and every side plane faces inward: §5's control does exactly that and "
        "the manifold comes back with <strong>0 of 4 incident vertices inside</strong>, on a contact "
        "that is plainly there. It is the quietest way there is to lose a floor, because nothing "
        "errors. The final polygon is the contact set — the intersection of two convex faces, which "
        "is convex and has at most <code>m + n</code> vertices."),

    "FIG_KINDS": ("l87_fig8.svg", "8",
        "<strong>Three ways a contact point is made, and every one of them is an INDEX rather than "
        "a position.</strong> Left: the incident face lies wholly inside the reference, so all four "
        "of its corners survive — the commonest contact in a game, a crate's corner on a floor, and "
        "211,615 of 481,847 measured points. Middle: it overhangs, so two corners survive and two "
        "CROSSING points appear where its edges meet a side plane — 162,966. Right: the reference "
        "face is the SMALLER of the two, so its own corners fall inside the incident face — 31,661, "
        "which is the case that is easy to miss and is not rare. Each of those is a pair of indices "
        "into geometry that does not move: a face's own identity and a vertex's own index on its "
        "shape. Nothing in an id is computed from a position, so nothing in it can change because a "
        "body moved by a micron. To the right, the census: of 200,000 random box pairs, 189,282 "
        "overlap, the clip kept nothing exactly <strong>once</strong>, and <strong>no overlapping "
        "pair produced an empty manifold</strong>."),

    "FIG_REDUCE": ("l87_fig9.svg", "9",
        "<strong>The clip produces up to eight points, a solver can use four, and which four is not "
        "a matter of taste.</strong> Left: the heuristic, drawn. Take the deepest point — which is "
        "why the reduction never loses depth — then the point farthest from it, which fixes the "
        "longest axis of the set, then the largest triangle on each side of that axis, which is what "
        "stops the four collapsing onto a line. Right: how often the question even arises. A third "
        "of face contacts clip to more than four, so this runs on one contact in three. Below, the "
        "comparison that needed thinking about before it could be made. Four points can never span "
        "an octagon — the largest quadrilateral inscribed in a regular one is 70.7% of it — so a "
        "ratio against the whole clipped polygon measures the SHAPE of the contact as much as the "
        "heuristic. Against the <em>best</em> four there are, brute-forced over all seventy "
        "four-subsets, the heuristic reaches a mean of <strong>96.3%</strong> and a median of "
        "<strong>exactly one</strong>, for four comparisons and no search — and loses "
        "<strong>no depth at all</strong>."),

    "FIG_PERSIST": ("l87_fig10.svg", "10",
        "<strong>A contact is matched to last frame's by identity, because it cannot be matched by "
        "position.</strong> Left: the six named fields of a <code>contact_id</code>. Not one is "
        "computed from a position, so not one can change because a body moved by a micron. Right, "
        "the measurement, and the second table is the one to be suspicious of. Over a crate's "
        "landing, a ten-micrometre position match finds <strong>0.00%</strong> of the contacts while "
        "an id match finds 82 — a crate falling at walking pace travels centimetres per frame, so no "
        "tolerance tight enough to tell two corners of a face apart is loose enough to follow one "
        "corner across a frame. Hold the same crate at rest and jitter it by a hundred nanometres "
        "and <em>both</em> find 100%. A position matcher works perfectly on a body that is not "
        "moving, which is exactly why it is a trap: it passes the test you would write for it, and "
        "the failure arrives with the motion. And the control at the bottom is the one that had to "
        "be rewritten, because it convicted the code of the test's own mistake."),

    "FIG_BUDGET": ("l87_fig11.svg", "11",
        "<strong>The manifold is a third of the narrow phase, and the penetration depth is still "
        "what costs.</strong> Twenty thousand overlapping box pairs on a release library. That "
        "ordering is worth internalising because the instinct is the other way round: clipping "
        "<em>feels</em> like the expensive part, and it is four passes over at most eight vertices "
        "with no square roots, while EPA is eight to sixteen expansions each rebuilding a chunk of a "
        "surface. Nothing allocates — measured by replacing the global <code>operator new</code> and "
        "counting, 6.17 §9's method for the third lesson running. And 8.6 §12's 20.5% from four "
        "deleted default member initialisers does not repeat here, for the reason 8.6 itself named: "
        "<strong>the tell was the ARRAY, not the struct</strong>. There is no array of "
        "<code>contact_face</code> anywhere."),
}

LISTING_META = {
    "engine/include/engine/phys/manifold.hpp": ("new", "new"),
    "engine/src/phys/manifold.cpp":            ("new", "new"),
    "engine/include/engine/phys/shape.hpp":    ("modified", "modified"),
    "engine/src/phys/shape.cpp":               ("modified", "modified"),
    "engine/include/engine/phys/convex.hpp":   ("modified", "modified"),
    "engine/include/engine/engine.hpp":        ("modified", "modified"),
    "engine/CMakeLists.txt":                   ("modified", "modified"),
    "demos/CMakeLists.txt":                    ("modified", "modified"),
    "demos/manifold/main.cpp":                 ("new", "new"),
    "scratch/verify_87.cpp":                   ("new", "new"),
    "scratch/build_verify_87.sh":              ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_87.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l87_" + path.replace("/", "_")


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
<title>8.7 — Contact Manifolds and Persistence · Build a Professional 3D Game Engine</title>
<meta name="description" content="8.6 finished the narrow phase's arithmetic: epa_penetration returns how deep two convex shapes overlap, which way to push them apart, and a pair of witness points on the two surfaces. Everything a solver asks for, and a crate resting on a floor still will not sit still. A contact force acts at a point, so the torque it exerts about that point is zero, and a body held up by one point is a body balanced on a pin. This lesson measures that on the engine's own rigid_body with the same gravity, the same impulses and the same normal in both arms: four contact points settle at 1.16 millimetres of penetration and 5.2 degrees per second of residual rotation, and the same contact reduced to its deepest single point sinks 21.27 millimetres and rocks at 27.99 degrees per second, permanently. A single point does not fail to hold a crate up; it holds one up by chattering, which is why single-point contact looks nearly fine in a demo and is unusable in a game. The moment a support polygon can carry is derivable before it is measured, m times g times half the width, 49.05 newton metres, and a fourteen-step bisection lands on 47.87, two per cent low because that is the penetration the solver allows. Then the part that needed a new interface, and it is the surprising one. Lesson 8.5's claim that a convex set is determined by its support function is true, and support of a direction still returns exactly one point when the argmax is a whole square: four thousand directions drawn from a cone one thousandth of a degree wide about a face normal come back with all four corners, deterministically and arbitrarily, which are not the same thing. The control is what makes that mean something, because the same jitter about a corner direction returns one corner of eight, so the ambiguity belongs to the query rather than to the function and appears exactly where a manifold is needed. The fix is one more query on the shapes rather than a cleverer reading of EPA's output, and epa.hpp is untouched by this lesson: EPA's winning triangle is built from support points, so it carries the same ambiguity, and it is a piece of the difference set's face rather than the whole of it. So each primitive learns to return its whole feature. A sphere returns one vertex, a capsule one or two depending on how perpendicular the query is to its spine, a box its dominant face, and a point cloud the vertices on its supporting plane, gathered with a tolerance that is an angle rather than a distance and for which there is no value that is right, because the question is topological and a point cloud has no topology. Then the two features are clipped against each other with Sutherland and Hodgman, the algorithm lesson 3.3 wrote for the near plane, doing a different job: the reference face supplies the side planes and the plane every depth is measured from, and reversing its winding turns every side plane inward and empties the manifold silently, which is the quietest way there is to lose a floor. The parallelism tolerance turns out to bound the normal error exactly at its own arc cosine, 44.8 degrees at 0.5 and 2.56 at the default, which makes it a knob rather than a magic number. The clipped set is reduced to four, and four is forced rather than budgeted: a fifth impulse is a linear combination of the others, which makes a solver's answer non-unique rather than better, and the reduction reaches 96.3 per cent of the best four points there are, brute forced over every four-subset, while losing exactly no depth. The second half is persistence. Warm starting needs to know which of this frame's contacts is which of last frame's, and the tempting answer is wrong: positions are floats computed by a different route through the same code, and over a crate's landing a ten-micrometre position match finds zero per cent of them while an id match finds 82, rising to 100 per cent of 1,999 consecutive frames once it settles. A position matcher works perfectly on a body that is not moving, which is exactly why it is a trap. And the control that was written for it expected zero matches after a two-metre teleport and got four of four, because an id names a feature rather than a place and the manifold was right. A control that convicts the code of the test's own mistake is the most expensive kind there is. Ships manifold.hpp, support_face for every primitive, a frame-to-frame cache with no tombstones, a demo that applies one Sutherland-Hodgman side plane per keystroke beside the support polygon it produces, nine measured sections and 92 checks, one fixture whose answer is written down on paper first, and a knob that was written for a real hazard, measured at zero flips over 208,000 pairs, and taken back out.">

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
    <a class="prev-l" href="08-06-epa.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.6 — EPA: Penetration Depth</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-08-broadphase.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.8 — Broadphase: A Uniform Grid</span>
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
        with open(f"scratch/l87_body_{name}.html") as fh:
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
    # full of × − ° §, so `len(page)` is several thousand short of `wc -c`.
    print(f"wrote {OUT}  ({len(page.encode('utf-8')):,} bytes, "
          f"{page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
