#!/usr/bin/env python3
"""Assemble docs/lessons/08-03-angular-dynamics.html.

Same pipeline as build_71.py through build_82.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l83_body_{a,b,c,d,e}.html, figures from scratch/figs_83.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE, for the third lesson running, and more
than in either of the previous two. THIRTEEN listings, and five of them are files
that every remaining lesson in Modules 8 and 9 will touch: `mat3.hpp`,
`integrate.hpp`, `rigid_body.hpp`, `engine.hpp` and both CMakeLists. 8.4 adds
shapes to `phys/`, 8.5 adds GJK, 8.9 adds a solver — and every one of them edits
a file this page prints in full. A builder that opened a repository path would
render this page's listings as they stand TODAY rather than as they stood when
the prose was written about them, and the prose here refers to specific line
groups by what they say.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson. Here
the pins are generated from LISTING_META by `_pin`, so a path added to
LISTING_META without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order. Here the two happen to agree — figs_83.py was
written after the section order was settled — which is a convenience rather than
a rule, and the names stay so that moving a figure stays a one-line edit.
"""
import os
import re

OUT = "docs/lessons/08-03-angular-dynamics.html"

FIGURES = {
    "FIG_TORQUE": ("l83_fig1.svg", "1",
        "<strong>Three ways to push a box, and two of them are special cases that are not special-"
        "cased.</strong> The lever arm is drawn from the <em>centre of mass</em>, which is a choice "
        "rather than a fact and is the one that decouples the two halves of a step: about any other "
        "point, a force through the centre of mass would produce a torque, and pushing a crate "
        "squarely in the middle would set it spinning. Left: a 10 N push a metre above the centre, "
        "giving <code>r \u00d7 F = (0, 0, \u221210)</code> \u2014 about <strong>\u2212z</strong>, "
        "which by the right-hand rule tips the box toward <code>+x</code>, which is the direction "
        "we pushed. Middle: the same force applied at a corner but <em>aimed</em> at the centre, "
        "where <code>r</code> and <code>F</code> are parallel and the torque is "
        "<strong>exactly 0.0000e+00</strong> \u2014 not small, zero, because every term of the "
        "cross product has a factor of the lever arm. This is why gravity does not make things "
        "tumble. Right: a <em>couple</em>, two equal and opposite forces offset from each other, "
        "with a net force of exactly zero and a net torque of \u221210 N\u00b7m. That is the "
        "physical thing <code>add_torque</code> models."),

    "FIG_OUTER": ("l83_fig2.svg", "2",
        "<strong>What <code>|r|\u00b2\u00b71 \u2212 r \u2297 r</code> actually does, read as "
        "geometry rather than arithmetic.</strong> A 3 kg point mass two metres up the "
        "<code>y</code> axis. An axis running <em>through</em> it meets no resistance at all \u2014 "
        "the moment is exactly <strong>0.000000</strong>, which is right, because a point spun "
        "about an axis through itself does not move \u2014 while an axis <em>across</em> it meets "
        "<code>m r\u00b2 = 3 \u00d7 4 = 12</code>. The two terms say exactly that: the identity "
        "term resists every axis by <code>m r\u00b2</code>, and the outer product takes all of it "
        "back along <code>r</code>. Note what the outer product <em>is</em> \u2014 the other thing "
        "you can do with a pair of vectors, where the dot product gives a number and this gives a "
        "matrix. Its rank is one, so it flattens all of space onto a line: never a transformation "
        "you would want, always a term in something else. The derivation is checked against the raw "
        "cross products over 20,000 random pairs at <strong>1.2075e\u221206</strong>."),

    "FIG_CANCELLATION": ("l83_fig3.svg", "3",
        "<strong>The expression the derivation hands you is the one you must not ship.</strong> For "
        "a sample at <code>r = (1000, 0.001, 0)</code> \u2014 a plank, a rail, a sword, a lamp post "
        "\u2014 the true <code>Ixx/m</code> is <code>y\u00b2 + z\u00b2 = 1e\u221206</code>, and "
        "both expressions say so on paper. The derived form computes it as "
        "<code>|r|\u00b2 \u2212 x\u00b2</code>, which is <code>1000000.000001 \u2212 "
        "1000000</code>; a <code>float</code> has 24 bits of significand, so at a magnitude of "
        "10\u2076 the gap between representable values is 0.0625 and the answer was never "
        "<em>in</em> the sum. It returns <strong>exactly zero \u2014 100% wrong</strong>. The "
        "cross-product-matrix form builds the diagonal from the two terms that belong in it and "
        "never forms the cancelling sum. The consequence is behavioural rather than numerical: a "
        "zero principal moment is a singular tensor, which <code>inverse_inertia</code> refuses, "
        "which is a body that silently will not spin about its own length."),

    "FIG_GRID": ("l83_fig4.svg", "4",
        "<strong>A test that passes only if the parallel-axis theorem is exactly true.</strong> Fill "
        "a box with an <code>N\u00b3</code> grid of point masses and sum equation (1) directly, and "
        "the answer comes out <em>low</em> \u2014 by exactly <code>1 \u2212 1/N\u00b2</code>, at "
        "every resolution, to six decimal places. That is predictable before it is measured: a point "
        "at a cell's centre carries the cell's mass but none of the cell's own spread, and summed "
        "over all the cells the deficit is precisely <code>I/N\u00b2</code>. Add each cell's own "
        "tensor back \u2014 which is the theorem, applied 64 times \u2014 and the sum is "
        "<strong>exact at N = 4</strong>, against a closed form of 4.333333. Sixty-four samples "
        "reproducing an integral to float's noise floor is not an approximation converging; it is an "
        "identity."),

    "FIG_PARALLEL": ("l83_fig5.svg", "5",
        "<strong>A dumbbell, and a wrong answer that passes every test the engine has.</strong> Two "
        "2 kg balls on a 0.5 kg bar: <strong>37.5\u00d7</strong> harder to turn across the bar than "
        "along it, and almost all of the 9.63 comes from the parallel-axis term "
        "(<code>2 \u00d7 2 \u00d7 1.5\u00b2 = 9.0</code>) rather than from the balls themselves "
        "(0.256). Add the parts' tensors <em>without</em> shifting them to the common centre and you "
        "get <strong>15.3\u00d7 too small</strong> \u2014 and the result is symmetric, positive-"
        "definite, satisfies the triangle inequality, and is reported <code>usable</code>, because "
        "it is a perfectly good tensor: the tensor of a body whose parts are all piled on top of "
        "each other at the balance point. A validity check tells you a tensor <em>could</em> exist. "
        "It cannot tell you it is the tensor of the thing you meant."),

    "FIG_SANDWICH": ("l83_fig6.svg", "6",
        "<strong>Why a tensor needs a sandwich where a vector needs one matrix.</strong> Read the "
        "chain right to left, which is the order the matrices apply in: <code>R\u1d40</code> "
        "carries the angular velocity <em>into</em> body axes, <code>I</code> acts there \u2014 "
        "where it is constant and where its meaning was defined \u2014 and <code>R</code> carries "
        "the result back out. A quantity that eats a vector and produces one has to be converted on "
        "both sides. Below: the reversed sandwich <code>R\u1d40 I R</code>, which is what you write "
        "when you get it backwards, and the four things that fail to catch it. Same trace, same "
        "determinant, still symmetric, same principal moments \u2014 and out by "
        "<strong>1.569413</strong> on a body whose moments run from 1.67 to 4.33, because it is the "
        "basis change in the other direction and describes a real body that is not this one. Worse, "
        "a test built on a 90\u00b0 turn passes with the transpose in either place: on a diagonal "
        "tensor a right angle is its own inverse up to a sign. <strong>Convention bugs hide behind "
        "symmetric test data.</strong>"),

    "FIG_SPIN": ("l83_fig7.svg", "7",
        "<strong>Why an orientation cannot be integrated by addition, and the two errors of the "
        "update that replaces it.</strong> Left: the same two 90\u00b0 turns in both orders send "
        "the probe vector to results <strong>120\u00b0 apart</strong>, while the sum of the two "
        "rotation vectors is identical either way \u2014 addition commutes and rotations do not, so "
        "no vector can be a rotation's running total. There is nothing here a smaller step fixes. "
        "Right, three measurements of the update that follows from <code>q\u0307 = "
        "\u00bd\u03c9q</code>: its length inflates by exactly <code>\u221a(1 + "
        "(\u03c9h/2)\u00b2)</code> every step \u2014 0.35% at 10 rad/s and 60 Hz, compounding to "
        "<strong>1.2307</strong> in a second, and a non-unit quaternion is a matrix with a scale in "
        "it; and the angle it achieves is short by <code>\u2212(\u03c9h)\u00b2/12</code> "
        "<em>even after renormalising</em>, which is a lag rather than a drift and therefore "
        "accumulates linearly to <strong>79.2459\u00b0</strong> in a minute. The exponential map "
        "has neither problem and costs a sine and a cosine."),

    "FIG_TUMBLE": ("l83_fig8.svg", "8",
        "<strong>A torque-free body conserves two things, and its angular velocity is confined to "
        "where they meet.</strong> <code>|L|\u00b2</code> is a sphere; twice the kinetic energy, "
        "<code>\u03a3L\u1d62\u00b2/I\u1d62</code>, is an ellipsoid; and <code>\u03c9</code> "
        "lives on the curve where the two surfaces intersect. That picture is also a calculation: "
        "write <code>u\u1d62 = L\u1d62\u00b2</code> and <em>both</em> conserved quantities are "
        "linear in <code>u</code>, so the reachable set is a segment and "
        "<code>|\u03c9|\u00b2</code> is linear on it \u2014 and a linear function on a segment "
        "takes its extremes at the ends, where one <code>u\u1d62</code> reaches zero. Three "
        "candidates, one of them infeasible, and the answer is "
        "<strong>4.0524 \u2026 4.4880</strong> against a measured "
        "<strong>4.0527 \u2026 4.4924</strong>. A 10.85% swing in angular speed with nothing acting "
        "on the body, predicted by an argument that knows nothing about the implementation."),

    "FIG_MODES": ("l83_fig9.svg", "9",
        "<strong>Four answers to one term, and the column that goes the wrong way.</strong> Worst "
        "drift in <code>|L|</code> over sixty torque-free seconds \u2014 a body with nothing acting "
        "on it must not change its angular momentum at all, so every point above the dashed line is "
        "an error. The explicit treatment of the gyroscopic term is <strong>8.1's determinant</strong> "
        "arriving in the one place this engine still integrates something explicitly: it "
        "<em>grows</em>, by 3.3 \u00d7 10\u00b9\u00b9 at 30 Hz. The implicit one is its exact "
        "reciprocal and <em>damps</em>, losing 35% at 60 Hz \u2014 stable, shippable, and "
        "conserving nothing. The momentum formulation is three orders of magnitude better than "
        "either, because it does not treat the term as a term at all. <strong>And its line slopes "
        "the other way.</strong> It is the only table in this course where a smaller step is a worse "
        "answer, and the reason is that its error is not truncation: nothing in it approximates "
        "<code>L</code>, so what is left is the rounding of a round trip the engine performs once "
        "per step \u2014 which grows with the <em>number</em> of steps rather than their size."),

    "FIG_RACKET": ("l83_fig10.svg", "10",
        "<strong>The flip, as the demo draws it, and what everything in the lesson cost.</strong> "
        "Both panels are twelve seconds of the same box spun at 10 rad/s about its intermediate "
        "axis with a 1e\u22123 nudge, plotted in <em>body</em> axes \u2014 which is the frame "
        "Euler's equations are written in, and measuring in world space instead is what made the "
        "first version of this measurement miss by 20%. With the gyroscopic term <strong>off</strong> "
        "the spin component is a flat line and the perturbation never moves: the box spins about a "
        "fixed axis forever, which is what most engines ship. With the <strong>momentum</strong> "
        "formulation it drops through zero to its negative and back, twice, while the other two "
        "components pulse at each crossing \u2014 the box is turning end over end with nothing "
        "acting on it. The measured growth rate walks in on the rate Euler's equations predict as "
        "the step shrinks (2.1%, 0.56%, 0.19%, <strong>0.096%</strong>), and the flips arrive every "
        "<strong>2.6651 s</strong>, forever. Below, the budget \u2014 including the last row, which "
        "is not an algorithm but the discovery that a rotation matrix was being built twice from the "
        "same quaternion, twenty lines apart."),
}

LISTING_META = {
    "engine/include/engine/math/mat3.hpp":        ("modified", "modified"),
    "engine/include/engine/phys/integrate.hpp":   ("modified", "modified"),
    "engine/src/phys/integrate.cpp":              ("modified", "modified"),
    "engine/include/engine/phys/inertia.hpp":     ("new", "new"),
    "engine/src/phys/inertia.cpp":                ("new", "new"),
    "engine/include/engine/phys/rigid_body.hpp":  ("modified", "modified"),
    "engine/src/phys/rigid_body.cpp":             ("modified", "modified"),
    "engine/include/engine/engine.hpp":           ("modified", "modified"),
    "engine/CMakeLists.txt":                      ("modified", "modified"),
    "demos/CMakeLists.txt":                       ("modified", "modified"),
    "demos/spin/main.cpp":                        ("new", "new"),
    "scratch/verify_83.cpp":                      ("new", "new"),
    "scratch/build_verify_83.sh":                 ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_83.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l83_" + path.replace("/", "_")


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
<title>8.3 — Angular Dynamics: Torque and the Inertia Tensor · Build a Professional 3D Game Engine</title>
<meta name="description" content="Eighty-five lessons in, nothing in this engine can tumble. 8.2 gave a body a mass, which answers one question — how hard is it to push? — and this lesson answers the other one, which turns out to need nine numbers rather than one. Spin a book about its three axes and you get three different resistances; spin it about an axis between two of them and the angular momentum does not point along the spin axis at all, and a thing that eats a vector and hands back one pointing somewhere else is a matrix. We derive the inertia tensor in four lines from the vector triple product, and then find that the expression the derivation produces returns exactly zero for a plank, because |r| squared minus x squared is a sum of three squares with one subtracted straight off again and a float at a million cannot hold a millionth. Then the parallel-axis theorem, checked by a test that passes only if the theorem is exactly true; the basis change R I R transpose, and the plausible wrong one that has the same trace, the same determinant, the same symmetry and the same principal moments; and the integration of an orientation, which is not an addition at all — two 90-degree turns in the other order land 120 degrees away while the sum of the rotation vectors is identical, so no vector can be a rotation's running total. The update that replaces it inflates a unit quaternion by 0.35 percent every step and still turns 0.23 percent short after you renormalise, both predictable in advance. Then the payoff: a torque-free body conserves L and does not conserve omega, swinging its angular speed over 10.85 percent with nothing acting on it, between two bounds derived by hand and then measured to four digits. Spin it about its middle axis and it flips end over end forever, at a rate Euler's equations predict to a tenth of a percent. And the term that makes all of it happen is the one almost every engine drops, because integrating it explicitly gains 162 percent of a body's angular momentum in a minute and diverges by 3.3e11 at 30 Hz. There are four answers to that, the last of which is that the term is not physics at all — it is the price of choosing omega as the state variable, and storing the momentum instead makes it vanish from the equations, at the cost of the one table in this course where a smaller step size is a worse answer. Also: a compound body assembled without the theorem is 15 times too small and passes every validity test, and a step that was 20 times slower than 8.2's turned out to be building the same rotation matrix twice.">

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
    <a class="prev-l" href="08-02-forces-and-bodies.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.2 — Forces, Gravity, and Linear Rigid Bodies</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-04-collision-primitives.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.4 — Collision Primitives: Spheres, AABBs, OBBs, and the SAT</span>
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
        with open(f"scratch/l83_body_{name}.html") as fh:
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
