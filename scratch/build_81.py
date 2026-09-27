#!/usr/bin/env python3
"""Assemble docs/lessons/08-01-integrators.html.

Same pipeline as build_71.py through build_78.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l81_body_{a,b,c,d,e}.html, figures from scratch/figs_81.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. Three of the eight listings —
`engine.hpp` and both CMakeLists — are the most frequently edited files in the
repository, and every remaining lesson in Modules 8 and 9 will touch at least
one of them. A builder that opened a repository path would render this page's
listings as they stand TODAY rather than as they stood when the prose was
written about them, and §13 quotes the umbrella lint's exact tally.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson. Here
the pins are generated from LISTING_META by `_pin`, so a path added to
LISTING_META without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/08-01-integrators.html"

FIGURES = {
    "FIG_TANGENT": ("l81_fig1.svg", "1",
        "<strong>One step, and the thing it cannot know.</strong> The grey curve is where the "
        "quantity actually goes; the red line is its tangent at the moment you sampled it, extended "
        "by a step of <code>h</code>; the amber gap is the error. What sets the size of that gap is "
        "neither the value nor the slope — a straight line has a slope and no gap at all — "
        "but the <strong>curvature</strong>, which is to say the acceleration. That is why the "
        "dropped Taylor term is <code>(h²/2)·a</code> and why the local error is "
        "<code>O(h²)</code>: one factor of <code>h</code> is then paid back by having to take "
        "<code>T/h</code> steps, leaving <code>O(h)</code> over a fixed simulated time. That is the "
        "whole content of the phrase “first order”, and figure 2 is the demonstration that "
        "it is not the number which decides whether a game works."),

    "FIG_ORDER": ("l81_fig2.svg", "2",
        "<strong>Two questions, and the standard one is blind.</strong> On the left, the textbook "
        "measurement: hold the simulated time at one period, shrink the step, take the log-2 ratio. "
        "Explicit Euler reads <strong>1.089</strong> and the two symplectic rules read "
        "<strong>2.00</strong> — and semi-implicit Euler reading second order is a fact about "
        "the harmonic oscillator rather than a promotion, because its amplitude error here is "
        "exactly zero and only a frequency shift is left. The amber line is float's own floor, which "
        "the bottom row has already reached. On the right, the question a game actually asks: hold "
        "<code>h</code> at 1/60 and let time run. The two symplectic columns ADD — 2.883, 5.766, "
        "14.42, 28.84 in exact proportion to <code>t</code> — while the explicit column "
        "MULTIPLIES by 1.9 every second, reaching <strong>4.8 × 10⁵</strong> on a spring "
        "whose amplitude is one metre. Nothing measured on the left can see that."),

    "FIG_PHASE": ("l81_fig3.svg", "3",
        "<strong>The change of viewpoint that makes the bug visible.</strong> Plot position against "
        "velocity and a spring's exact motion is a <em>circle</em>, traversed forever; a correct "
        "integrator produces points on it. One step of each rule is drawn here at a deliberately "
        "coarse <code>hω = 0.7</code>. Read the grey dotted horizontal line first: <strong>both "
        "endpoints are at the same height</strong>, because both rules ran the identical velocity "
        "update from the identical state, and the only thing they disagree about is which "
        "<code>x</code> the position line was told to move at. Red lands outside everything and will "
        "be further outside next step. Green lands on the dashed ellipse — not on the circle, "
        "which is the honest surprise section 6 is about — and stays on it forever. The inset "
        "runs the same step on a small square: one rule scales its area by "
        "<code>1 + h²ω²</code>, the other by exactly 1."),

    "FIG_MATRICES": ("l81_fig4.svg", "4",
        "<strong>Three methods, three determinants, three fates.</strong> For a linear restoring "
        "force every step is a 2×2 matrix, and a determinant is the factor a matrix scales area "
        "by. Explicit Euler's is <code>1 + h²ω²</code> — greater than one for "
        "every step size and every stiffness that exist, so it inflates, always. Semi-implicit "
        "Euler's is <strong>exactly 1</strong>: the <code>−h²ω²</code> that "
        "appears in the top-left because the position line used the NEW velocity cancels the "
        "<code>+h²ω²</code> from the off-diagonal product, algebraically, with no "
        "approximation. Backward Euler is the first one's mirror image, its matrix divided by "
        "<code>(1 + h²ω²)</code> and its determinant the exact reciprocal — "
        "unconditionally stable and unconditionally lossy, which is why a released pendulum under it "
        "grinds to a halt and why no engine ships it for general dynamics."),

    "FIG_ENERGY": ("l81_fig5.svg", "5",
        "<strong>The determinant is not a description of the blow-up; it IS the blow-up.</strong> "
        "The enclosed area of a phase-space orbit is <code>2πE/ω</code>, so area is energy "
        "and a step that scales area by <code>1 + h²ω²</code> scales energy by it too. "
        "The red LINE here is the closed form <code>(1 + h²ω²)ᴺ</code> and the "
        "red DOTS are the measured run: they agree across seventeen orders of magnitude. A 1 m "
        "spring reaches <strong>3.36 × 10⁸ m</strong> after a simulated minute — and "
        "if that spring is the one-centimetre overlap between a box and a floor, which is exactly "
        "what a penalty contact is, the box is 3,357 km away. The trap is the first ten seconds, "
        "where the energy is merely 695× and a playtester says the physics feels a bit bouncy. "
        "Green and violet sit on 1.0, violet drawn dashed on top of green because they agree."),

    "FIG_SHADOW": ("l81_fig6.svg", "6",
        "<strong>What is actually conserved, and why that is the same thing as stability.</strong> "
        "A determinant of 1 says the map does not scale area. It does not say the map leaves points "
        "alone — semi-implicit Euler <em>shears</em> — so the true energy is not conserved: "
        "it wobbles by exactly <code>hω</code> peak to peak, predicted 0.104720 and measured "
        "<strong>0.104723</strong>. What is conserved is the sheared cousin "
        "<code>½(v² + ω²x² − hω²xv)</code>, held to "
        "<strong>4.6 × 10⁻⁶</strong> over 3,600 float steps, and the trajectory is a "
        "level set of it. Left: below the limit that form is positive-definite, so its level set is "
        "a closed ellipse and nothing can escape. Right: past <code>hω = 2</code> it is "
        "indefinite, the level sets are hyperbolae, and there is no closed curve left to sit on. "
        "Stability and the existence of that ellipse are one statement."),

    "FIG_LIMIT": ("l81_fig7.svg", "7",
        "<strong>Bounded is not the same as right.</strong> Every point on this chart is "
        "<em>stable</em> — none of them diverges, not after ten seconds and not after ten hours "
        "— and only the left fifth is usable. The measured worst energy excursion lands on "
        "<code>1/(1 − hω/2)</code>, which is where figure 6's shadow ellipse puts its far "
        "end, to three digits at every step size tested; at <code>hω = 1.95</code> the orbit is "
        "an ellipse so eccentric that the spring's energy swings by a factor of forty on the way "
        "round, forever. Read backwards, which is how an engineer uses it: a factor of five of "
        "headroom at 60 Hz admits <code>ω ≤ 24 rad/s</code>, <strong>a spring of about "
        "3.8 Hz</strong>. A contact stiff enough not to let a box sink visibly is hundreds of hertz, "
        "and that gap is the entire argument for why Lesson 8.10 solves impulses instead."),

    "FIG_GRAVITY": ("l81_fig8.svg", "8",
        "<strong>And this is why the bug ships.</strong> Under constant acceleration the catastrophe "
        "vanishes. All three rules compute the identical velocity — there is no state for the "
        "velocity lines to disagree about — so the only difference is position, and summing the "
        "arithmetic series gives an error of exactly <code>∓½·a·h·t</code>: "
        "explicit Euler falls short by it, semi-implicit Euler overshoots by it, and velocity Verlet "
        "is exact because the <code>½a·h²</code> it keeps IS the term the others drop. "
        "At 60 Hz that is <strong>8.2 cm after one second</strong> of falling, on a 4.9 m drop. And "
        "the lines are <em>straight</em>: the error grows like <code>t</code> while the distance "
        "grows like <code>t²</code>, so the relative error falls from 1.7% to 0.21% between one "
        "second and eight. Your first falling-cube demo is fine. It stops being fine the moment a "
        "force asks where the body is."),

    "FIG_DAMPING": ("l81_fig9.svg", "9",
        "<strong>The same two characters, in two disguises.</strong> Left: <code>v *= 0.99f</code> "
        "once per step, which is the most common frame-rate dependency in gameplay code and looks "
        "like nothing. It is explicit Euler on drag with the coefficient hidden, and it retains "
        "<strong>74% of the velocity per second at 30 Hz and 24% at 144 Hz</strong> — same "
        "constant, same source, four different games. The dashed line is what "
        "<code>pow(retained, h)</code> gives, which is the same number at every rate. Right: the "
        "same explicit step written honestly as <code>v *= (1 − hk)</code>, where the failure "
        "modes are visible. At <code>hk = 1</code> it removes the entire velocity in one go; at 1.5 "
        "it removes 150% of it, so the sign flips every step; past 2 it flips AND grows — "
        "<strong>a drag force that accelerates things</strong>. None of it is necessary, because "
        "<code>dv/dt = −kv</code> has a closed form and <code>apply_drag</code> evaluates it."),

    "FIG_DEMO": ("l81_fig10.svg", "10",
        "<strong>Six simulated seconds, in the only space where this is obvious.</strong> Left, the "
        "phase plot: the red spiral leaves the panel after about four seconds and is CLIPPED rather "
        "than rescaled, because a view that follows the worst track without limit ends up showing "
        "nothing but the thing that is broken. Green and violet are indistinguishable from each "
        "other and from the pale shadow ellipse beneath them — that is the result, not a "
        "rendering fault: at <code>hω = 0.105</code> the ellipse is 2.7% off a circle and both "
        "rules are sitting on it. Right, the same run as energy on a log axis, where a straight line "
        "means exponential growth. The table is the budget, and it is the free lunch: "
        "<strong>0.994 ns against 0.963 ns</strong> per body per step, a 3% difference against a 3% "
        "run-to-run spread, for a rule that is wrong at every step size against one that is not."),
}

LISTING_META = {
    "engine/include/engine/phys/integrate.hpp": ("new", "new"),
    "engine/src/phys/integrate.cpp":            ("new", "new"),
    "engine/include/engine/engine.hpp":         ("modified", "modified"),
    "engine/CMakeLists.txt":                    ("modified", "modified"),
    "demos/CMakeLists.txt":                     ("modified", "modified"),
    "demos/integrate/main.cpp":                 ("new", "new"),
    "scratch/verify_81.cpp":                    ("new", "new"),
    "scratch/build_verify_81.sh":               ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_81.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l81_" + path.replace("/", "_")


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
<title>8.1 — Integrators: Why One Explodes · Build a Professional 3D Game Engine</title>
<meta name="description" content="Eighty-three lessons in, nothing in this engine moves on its own: every position in every demo was authored, and Module 8 is the subsystem that computes where things are from how they are moving. Before mass, before shapes, before contacts, one question is in the way - you have a position, a velocity and an acceleration, advance them by h seconds - and the answer that occurs to everybody first is unconditionally unstable. Not inaccurate, not needs-a-smaller-step: wrong at every step size there is. The lesson turns on one number. For a linear restoring force every integrator step is a 2x2 matrix, and a determinant is the factor a matrix scales area by; explicit Euler's is 1 + h squared omega squared, greater than one always, and semi-implicit Euler's is exactly 1 because moving one line above another makes two terms cancel algebraically. On a 1 Hz spring at 60 Hz that surplus is 1.0966 percent per step - 1.92x energy per second, 1.13e17 in one minute, a one-centimetre overlap becoming 3,357 km - and the fix is measured at 0.994 ns against 0.963 ns per body per step, a difference smaller than the run-to-run noise. Also: why order of accuracy is blind to all of it, because order asks about h going to zero at fixed time while a game asks about time going to infinity at fixed h; what semi-implicit Euler actually conserves, which is not energy but a sheared cousin of it held to 4.6e-6 over 3,600 float steps while the true energy wobbles by exactly h times omega; why that shadow quantity being an ellipse rather than a hyperbola IS the stability limit h omega below 2, found by bisection at 0.3182939 against 0.3183099; why gravity forgives every integrator and a contact forgives none; and why v *= 0.99f makes four different games on four different monitors.">

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
    <a class="prev-l" href="07-08-audio.html">
      <span class="dir">← Previous</span>
      <span class="ttl">7.8 — SDL3 Audio: Streams, Mixing, and 3D Sound</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-02-forces-and-bodies.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.2 — Forces, Gravity, and Linear Rigid Bodies</span>
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
        with open(f"scratch/l81_body_{name}.html") as fh:
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
