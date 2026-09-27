#!/usr/bin/env python3
"""Assemble docs/lessons/07-02-axis-angle.html.

Same pipeline as build_71.py. No STATE block: STATE.md is the sole resume key
and lesson pages end at Further Reading.

Prose from scratch/l72_body_{a,b,c,d,e}.html, figures from scratch/figs_72.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. Three of the eight listed files are
certain to be edited by the next few lessons: `math/euler.hpp` and
`engine/engine.hpp` are touched by every lesson that adds a public header (the
configure-time lint guarantees the second), and `demos/gimbal/main.cpp` is this
module's demo and will keep growing. `math/rotation.hpp` is the one to watch
hardest: it was created as the home for representation-independent facts, which
means 7.3 and 7.4 both have a reason to add to it.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order. Figures 8 and 9 were generated in the other
order during authoring — §9.2's bar chart appears before §9.3's renders — and
that is the one numbering mistake nothing in this pipeline can catch for itself.
"""
import os
import re

OUT = "docs/lessons/07-02-axis-angle.html"

FIGURES = {
    "FIG_FIXED_LINE": ("l72_fig1.svg", "1",
        "A rotation fixes a whole <em>line</em>, and that is the entire content of Euler&#8217;s "
        "rotation theorem. The same three vectors before and after a 58&#176; turn: the teal one "
        "lies along the axis and its arrowhead has not moved a pixel, while the amber and blue "
        "ones have each swung by the same angle along the curves drawn beside them. The dashed "
        "grey circle is the orbit &#8212; it is drawn once, and <strong>every</strong> direction "
        "at that angle from the axis lives on it. Note what the picture does NOT show: any "
        "record of how the rotation was originally built. Three Euler turns or one Rodrigues "
        "turn, the fixed line is the same line, because it is a property of the rotation and not "
        "of the route taken to it."),

    "FIG_ODD": ("l72_fig2.svg", "2",
        "Why the theorem is about odd dimensions, drawn as eigenvalues on the unit circle. A "
        "rotation preserves length so every eigenvalue has magnitude 1; a real matrix has them in "
        "conjugate pairs. <strong>Three is odd, so one is always left over</strong> &#8212; and "
        "an unpaired eigenvalue is its own conjugate, hence real, hence &#177;1, and "
        "<code>det = +1</code> forces it to be <code>+1</code>. An eigenvalue of "
        "<code>+1</code> is a fixed direction: the axis. In four dimensions a rotation can turn "
        "in two independent planes at once and the four eigenvalues pair up perfectly with none "
        "spare; the measured <code>det(R &#8722; I) = 0.25840</code> under the right-hand panel "
        "is a counterexample to the whole theorem, computed by the harness with its own 4x4 "
        "determinant because <code>mat3</code> cannot hold a disproof of a claim about "
        "<code>mat3</code>."),

    "FIG_RODRIGUES": ("l72_fig3.svg", "3",
        "Rodrigues&#8217; formula, which is what you get by drawing the problem rather than "
        "looking it up. Split <code>v</code> into the part along the axis &#8212; the teal arrow, "
        "which cannot move because the axis does not &#8212; and the part across it, which lives "
        "in the dashed disc. Inside that disc, <code>v&#8869;</code> and <code>n &#215; v</code> "
        "are <strong>perpendicular and the same length</strong>, so they are a perfectly good "
        "pair of axes for it, and the 3-D problem has become the 2-D rotation of Lesson 1.7. The "
        "blue arc is the 62&#176; sweep. One detail is easy to miss and is why the code never "
        "computes <code>v&#8869;</code> at all: <code>n &#215; v</code> and "
        "<code>n &#215; v&#8869;</code> are the <em>same vector</em>, because the along-axis part "
        "of <code>v</code> contributes nothing to a cross product with <code>n</code>."),

    "FIG_AXIS_RENDER": ("l72_fig4.svg", "4",
        "The axis, in the demo &#8212; two real renders, not a diagram of one. The same pose in "
        "both panels: on the left the three gimbal rings of Lesson 7.1, which are the three turns "
        "that built it, and on the right the same orientation with the rings hidden. The teal "
        "line is the single axis the whole rotation turns about, recovered from the matrix by "
        "<code>axis_angle_from_rotation</code>, and it is the <strong>same line in both "
        "panels</strong> &#8212; hiding the rings does not move it, because it was never a "
        "property of the rings. The caption under the right panel is the number that makes this a "
        "measurement rather than a picture: building the pose from three composed Euler turns and "
        "building it from one Rodrigues turn about that axis agree to "
        "<code>0.000003&#176;</code>."),

    "FIG_AMPLIFY": ("l72_fig5.svg", "5",
        "What an error in the axis actually costs, which is the one formula that settles the "
        "design of the entire extraction. An axis wrong by <code>&#966;</code> produces "
        "<code>2 sin(&#952;/2) &#183; &#966;</code> of error in the resulting orientation &#8212; "
        "<strong>zero at the identity and two at a half-turn</strong>. So the representation&#8217;s "
        "two hard ends are not equally dangerous, and a library that gives them the same epsilon "
        "and the same fallback has misunderstood its own problem: near "
        "<code>&#952; = 0</code> the axis becomes unrecoverable and it does not matter, because "
        "whatever nonsense comes back is multiplied by nothing. The amber dots are measured by "
        "building two real matrices with tilted axes and asking "
        "<code>angle_between_rotations</code> how far apart they are &#8212; a measurement that "
        "makes no use of the curve it lands on, and lands on it to two parts in a hundred "
        "thousand."),

    "FIG_ROUTES": ("l72_fig6.svg", "6",
        "The two routes to the axis, measured at the same poses on matrices carrying 1e-7 of "
        "<em>absolute</em> error. Reading the axis off the antisymmetric part (amber) divides by "
        "<code>2 sin &#952;</code> and dies at a half-turn; reading it off the symmetric part "
        "(blue) divides by <code>1 &#8722; cos &#952;</code> and dies at the identity. "
        "<strong>They do not cross at a point.</strong> Across the shaded band they run within "
        "25% of each other and the winner flips from probe to probe, which is what a crossover "
        "looks like when you sample it finely enough; outside it the choice is decisive, 6.8x one "
        "way at 30&#176; and 569x the other at 179.9&#176;. The threshold derived from "
        "<code>tan(&#952;/2) = &#8730;3</code> lands in the middle of the band where neither "
        "route cares &#8212; which is the best possible place for one to be, because it is a "
        "point at which both are good rather than a compromise between two bad options."),

    "FIG_IDENTITY": ("l72_fig7.svg", "7",
        "The identity end, where the axis is destroyed and nothing happens. Two columns of one "
        "table, from the same runs on the same matrices, moving in opposite directions: the amber "
        "curve is the error in the recovered axis, climbing a decade for every decade the angle "
        "shrinks exactly as <code>&#949;/(2 sin &#952;)</code> predicts, and the blue curve is the "
        "error in the orientation those same numbers rebuild, flat at five millionths of a degree "
        "across the whole range. The two hollow markers are <strong>not a measurement</strong> "
        "&#8212; below the threshold the routine has stopped trying and returns a placeholder "
        "axis, so that value is a property of this test&#8217;s inputs rather than of the "
        "algorithm, and a maximum taken over the whole column would have reported it as the worst "
        "case of a degradation it is not part of. The placeholder&#8217;s own cost is predicted "
        "by Figure 5&#8217;s law to within 8%: an axis 78&#176; wrong at "
        "<code>&#952; = 1e-6</code> misplaces the object by 7.3e-05&#176;."),

    "FIG_BANDS": ("l72_fig8.svg", "8",
        "Lesson 7.1&#8217;s closing table, answered. The same three angle deltas run at four "
        "distances from gimbal lock: the Euler lerp&#8217;s wasted turning varies by a factor of "
        "nine across four moves that are <em>identical in the angles</em>, because the only thing "
        "that changed is where the pose sat. The geodesic&#8217;s is 0.00% at all four, and the "
        "important word is <strong>cannot</strong> rather than <em>does not</em> &#8212; a "
        "geodesic performs the turning a move requires and there is no other number it could "
        "report, at any pose, ever. Slerp&#8217;s zero is drawn as a mark on the baseline rather "
        "than as a bar of height zero, which would be invisible and would read as a missing "
        "measurement rather than as a measured zero."),

    "FIG_BLEND_RENDER": ("l72_fig9.svg", "9",
        "Two routes between the same two poses, from the demo&#8217;s <kbd>B</kbd> mode. The teal "
        "line is the axis of the single turn separating the endpoints and it is the "
        "<strong>same line in both panels</strong>: computed once, and it does not move during "
        "the blend, because for a geodesic there is nothing for it to do but not move. The blue "
        "craft rotates rigidly about it from start to finish; nothing the amber craft does can be "
        "described that way at any single instant, let alone throughout. On the right both have "
        "arrived and are drawn on top of each other, so only one is visible &#8212; but the two "
        "trails are not on top of each other, and the gap between them is the 25.24&#176;. "
        "Nothing solid is drawn in either panel, deliberately: an opaque aircraft between the "
        "camera and the trails hides exactly the part of each that the comparison is about."),
}

LISTING_META = {
    "engine/include/engine/math/rotation.hpp":   ("new", "new"),
    "engine/include/engine/math/axis_angle.hpp": ("new", "new"),
    "engine/include/engine/math/euler.hpp":      ("modified", "modified"),
    "engine/include/engine/engine.hpp":          ("modified", "modified"),
    "demos/gimbal/main.cpp":                     ("modified", "modified"),
    "scratch/verify_72.cpp":                     ("new", "new"),
    "scratch/build_verify_72.sh":                ("new", "new"),
    "scratch/golden_72.cpp":                     ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_72.sh": ("bash", "shell"),
}

LISTING_SOURCE = {
    "engine/include/engine/math/rotation.hpp":       "scratch/l72_engine_include_engine_math_rotation.hpp",
    "engine/include/engine/math/axis_angle.hpp":     "scratch/l72_engine_include_engine_math_axis_angle.hpp",
    "engine/include/engine/math/euler.hpp":          "scratch/l72_engine_include_engine_math_euler.hpp",
    "engine/include/engine/engine.hpp":              "scratch/l72_engine_include_engine_engine.hpp",
    "demos/gimbal/main.cpp":                         "scratch/l72_demos_gimbal_main.cpp",
    "scratch/verify_72.cpp":                         "scratch/l72_scratch_verify_72.cpp",
    "scratch/build_verify_72.sh":                    "scratch/l72_scratch_build_verify_72.sh",
    "scratch/golden_72.cpp":                         "scratch/l72_scratch_golden_72.cpp",
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
<title>7.2 — Axis-Angle and Rodrigues' Rotation Formula · Build a Professional 3D Game Engine</title>
<meta name="description" content="Lesson 7.1 asked which three turns compose to an orientation and got twenty-four answers, a singularity none of them escapes, and an interpolation that performs 14 per cent more turning than it needs on an ordinary pair of poses and 209 per cent near gimbal lock. This lesson asks what SINGLE turn a rotation is. Euler's rotation theorem says that question always has an answer, and we prove it in four lines of determinant algebra using only functions mat3 has had since Lesson 2.6 - then watch the one step that fails in four dimensions, because the theorem is false there and knowing why is what turns it from a memorised fact into an understood one. Rodrigues' rotation formula is derived from a picture rather than quoted: split the vector along and across the axis, notice that the across part turns in a plane whose two axes are already in your hand, and the three terms fall out with a reading each. Going backwards has two holes, at zero and at a half-turn, and they are not equally dangerous: an axis error of phi produces 2 sin(theta/2) times phi of orientation error, which is zero exactly where the axis becomes unrecoverable and maximal where it becomes hard to find. That one formula explains both ends, sets both thresholds, and is measured against a brute-force sweep to two parts in a hundred thousand. The threshold between the two extraction routes is derived from tan(theta/2) = root 3, giving 120 degrees, and the measurement then says something better than the derivation did - the crossover is a 45-degree-wide band rather than a point, and 120 sits in the middle of it. Then the payoff: scaling the angle of the single turn between two orientations traces the geodesic, so 7.1's plus 14 per cent and plus 209 per cent become 0.00 per cent at every pose, on the same pairs, measured with the same instrument. That is spherical linear interpolation, three lines long, and it is also where this representation runs out: 82 per cent of a call is trigonometry recovering a quantity it declined to store, and there is no usable formula at all for composing two turns. Lessons 7.3 and 7.4 fix that.">

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
    <a class="prev-l" href="07-01-euler-angles.html">
      <span class="dir">← Previous</span>
      <span class="ttl">7.1 — Euler Angles and Their Pathologies</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="07-03-complex-numbers.html">
      <span class="dir">Next →</span>
      <span class="ttl">7.3 — Complex Numbers Rotate the Plane</span>
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
        with open(f"scratch/l72_body_{name}.html") as fh:
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
