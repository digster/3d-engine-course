#!/usr/bin/env python3
"""Assemble docs/lessons/07-04-quaternions.html.

Same pipeline as build_71.py, build_72.py and build_73.py. No STATE block:
STATE.md is the sole resume key and lesson pages end at Further Reading
(CLAUDE.md §9).

Prose from scratch/l74_body_{a,b,c,d,e}.html, figures from scratch/figs_74.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. Four of the seven listed files are
certain to be edited by the next lesson: 7.5 performs the storage swap this
lesson declined, which touches `math/transform.hpp` by definition,
`math/quat.hpp` (slerp lands there), `engine/engine.hpp` (the configure-time
lint guarantees it) and `demos/gimbal/main.cpp` (every mode this rig has gained
was added by the lesson that needed it). A page whose listings float with the
repository is a page that stops being true the moment the next lesson compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one
the reader sees and follows PAGE order. 7.2 wrote two figures in the opposite
order to the page and had to swap them, and nothing in this pipeline can catch
that for itself — every figure still renders, just under the wrong number.
"""
import os
import re

OUT = "docs/lessons/07-04-quaternions.html"

FIGURES = {
    "FIG_FORCED": ("l74_fig1.svg", "1",
        "<strong>Nothing here was postulated except that a direction of space is like any other "
        "direction of space.</strong> On the left, the demand: every unit imaginary — the gold "
        "<code>u</code>, not just the three coordinate ones — must square to &minus;1, measured "
        "over 20,000 axes drawn uniformly on the sphere at 2.980e&minus;07. In the middle, the "
        "three lines that turns into anticommutativity: <code>(i+j)/&radic;2</code> is a unit "
        "imaginary, so <code>(i+j)&sup2; = &minus;2</code>, and expanding gives "
        "<code>&minus;2 + (ij + ji)</code>. On the right, the table every one of those twelve "
        "open entries collapses to, with <code>ijk = &minus;1</code> below it. <strong>The "
        "failure to commute is not a design choice somebody made and you have to live with; it "
        "is what isotropy costs</strong>, and a commuting alternative would be describing a "
        "space that is not ours."),

    "FIG_PRODUCT": ("l74_fig2.svg", "2",
        "One multiplication, and two famous halves inside it. Distributing "
        "<code>(w&#8321;+v&#8321;)(w&#8322;+v&#8322;)</code> leaves everything interesting in "
        "<code>v&#8321;v&#8322;</code>, and expanding that against &sect;3's table gives "
        "<strong><code>&minus;(v&#8321;&middot;v&#8322;) + (v&#8321;&times;v&#8322;)</code></strong> "
        "&mdash; the dot product from the three squared terms, the cross product from the six "
        "mixed ones. That is not an analogy. Gibbs and Heaviside cut the two operations out of "
        "exactly this expression in the 1880s, and every vector-algebra course since has taught "
        "the pieces without the thing they came from. <strong>Swap the operands and precisely one "
        "term changes sign.</strong> The worked pair at the bottom is pushed through by hand in "
        "&sect;4.3: the difference between the two orders is <code>(0, &minus;2, 10, 4)</code>, "
        "which is exactly twice <code>v&#8321;&times;v&#8322;</code>."),

    "FIG_MIRRORS": ("l74_fig3.svg", "3",
        "<strong>Three things fall out of one calculation, and all three are usually handed to "
        "you separately.</strong> Two mirror planes meet along a line, which is the axis. A "
        "reflection in the plane perpendicular to a unit normal <code>n</code> is "
        "<code>n v n</code> &mdash; which expands to <code>v &minus; 2(n&middot;v)n</code>, the "
        "function this engine has had since Lesson 1.8. Doing it twice and moving the brackets "
        "gives <code>(n&#8321;n&#8320;) v (n&#8320;n&#8321;)</code>, and since conjugation "
        "reverses a product and negates a pure quaternion, the right-hand factor IS the "
        "conjugate of the left. <strong>So the sandwich was not chosen &mdash; it is what "
        "&ldquo;do two reflections&rdquo; looks like.</strong> Working <code>q</code> out then "
        "gives <code>cos(&theta;/2) + sin(&theta;/2)&#8202;n&#770;</code> after a negation that "
        "<em>nothing in the derivation determines</em>, which is the double cover arriving before "
        "anyone has named it. The panel on the left is drawn with the axis toward the reader so "
        "the 30&deg; and the 60&deg; can be checked with a protractor."),

    "FIG_COMMUTE": ("l74_fig4.svg", "4",
        "A real render from <code>demos/gimbal</code>, not a diagram of one. Two journeys leave "
        "the same nose using the same two turns: amber pitches 90&deg; and then yaws 90&deg;, "
        "blue yaws first. <strong>The four arcs form a quadrilateral that fails to close</strong>, "
        "and the magenta arc is the gap &mdash; the same picture as walking a mile south, east, "
        "north and west on a sphere and not arriving home. The receipt beside it is the part that "
        "makes this a measurement: the gap is <strong>120.0000&deg;</strong> from the quaternion "
        "metric, 120.0000&deg; from the matrix metric that Lesson 7.1 wrote and that has never "
        "heard of a quaternion, and 120.0000&deg; from the closed form "
        "<code>2&#8202;acos|c&#8308; + 2c&sup2;s&sup2; &minus; s&#8308;|</code> derived in "
        "&sect;7.1. Three routes sharing no code. Hold the mode open and watch the gap grow from "
        "nothing as <code>&phi;</code> sweeps: it goes as <code>&phi;&sup2;</code>, so "
        "non-commutativity is not a threshold effect waiting at large angles &mdash; two 5&deg; "
        "turns miss by 0.44&deg;, which is small and is not zero."),

    "FIG_COVER": ("l74_fig5.svg", "5",
        "<strong>The craft is home and its quaternion is not.</strong> A real render at exactly "
        "one full revolution: the pose readout says <code>0.0000 deg</code> from the start, and "
        "the printed quaternion is <code>(&minus;1, 0, 0, 0)</code>. The dial plots "
        "<code>(w, v&middot;n&#770;)</code>, which is "
        "<code>(cos(&theta;/2), sin(&theta;/2))</code> and therefore a point going round at "
        "exactly half the craft's rate &mdash; teal hand at 0&deg;, gold hand at 180&deg;, on the "
        "mark for <code>w = &minus;1</code>. It takes a second lap to bring both home together. "
        "This is Lesson 7.3's figure 6 with one more imaginary unit and for the same reason: the "
        "object is the square root of the rotation. Two drawing decisions are load-bearing here "
        "and both were mistakes first &mdash; the dial is built facing the camera, because in a "
        "fixed world plane it renders as an ellipse whose angles cannot be read; and the panel "
        "keeps the program's own dark background, because the gold hand against the page's pale "
        "diagram fill was at 1.8:1 and that hand is the whole figure."),

    "FIG_EXTRACT": ("l74_fig6.svg", "6",
        "Why the fourth component removes a problem the third could not. On the left, which "
        "component Shepperd's pivot chose over 20,000 random rotations, and the identity that "
        "makes the choice safe: the four candidates are trace-like combinations of the diagonal "
        "whose sum is <strong>exactly 4</strong>, so the largest is always at least 1 and the "
        "divisor always at least 2 &mdash; measured smallest pivot, 1.038. <strong>There is no "
        "bad case and therefore no threshold.</strong> Compare Lesson 7.2, whose three candidates "
        "summed to 1, guaranteed only <code>1/3</code>, and needed a crossover derived at "
        "<code>tan(&theta;/2) = &radic;3</code>. On the right, the round trip on a log scale. The "
        "naive trace route is indistinguishable from Shepperd's up to about 120&deg; &mdash; which "
        "is exactly where a hand-written test suite lives &mdash; and then dies: at 179.99&deg; it "
        "is off by <strong>180 degrees</strong>, a completely different orientation rather than a "
        "precision loss, because it divides by <code>4w</code> and <code>w = cos(&theta;/2)</code>."),

    "FIG_COST": ("l74_fig7.svg", "7",
        "The three claims of &sect;10, measured, and the middle panel is the one most treatments "
        "omit. <strong>Composing is 1.57&times; cheaper and applying is 1.57&times; dearer</strong> "
        "&mdash; a quaternion is better at everything except the operation a renderer performs "
        "most. Repairing is 3.25&times; cheaper, and &sect;10.4 argues that this row rather than "
        "the drift row is the real advantage. <strong>The number to design with is at the "
        "bottom.</strong> Converting costs 4.619&nbsp;ns and each vector then saves 0.609, so the "
        "matrix pays for itself after about eight vectors &mdash; which is a small enough number "
        "that a single skinned vertex influenced by four bones is already past it. That one "
        "crossover is why every renderer handed quaternions converts them before it draws "
        "anything, and it is the honest correction to &ldquo;quaternions are faster than "
        "matrices&rdquo;, which is true of storing and composing and false of the thing a frame "
        "spends its time on."),

    "FIG_DRIFT": ("l74_fig8.svg", "8",
        "Two tables, and both say something other than what was expected. On the left, "
        "<strong>the raw quaternion walk drifts WORSE than the raw matrix walk</strong> at every "
        "step size &mdash; where the plane's complex number won by 6.15&times; in Lesson 7.3 "
        "&mdash; because an unrepaired norm error compounds as <code>|q|&#8319;</code> and the "
        "sandwich then squares it, so the answer is set by one input rather than by the "
        "representation. Three step sizes were measured for exactly that reason. One "
        "<code>renormalised_fast</code> per step ends the argument at "
        "<strong>6&times;10&#8315;&#8312;</strong>, five orders better than either raw walk and a "
        "third the cost of Gram-Schmidt. On the right, 7.3's sharpest warning <em>inverting</em>: "
        "that lesson found this function catastrophic at <code>|q| = 2</code>, and here that input "
        "is <strong>perfect</strong>, because it returns <code>&minus;q</code> and "
        "<code>&minus;q</code> is the same rotation. <strong>The failure is not monotonic, so a "
        "test sampling the two obvious points certifies a broken function.</strong>"),

    "FIG_CANNOT": ("l74_fig9.svg", "9",
        "Why the storage swap this header has promised since Module 2 is not a rename. The ECS "
        "hierarchy produces a matrix with scale in it; <code>gfx/renderable.cpp</code> assigns "
        "its upper-left 3&times;3 to a field called <code>rotation</code> and writes "
        "<code>1</code> into the field called <code>scale</code>; and the recomposition "
        "reproduces the original affine matrix <em>to the bit</em> &mdash; <strong>because a "
        "<code>mat3</code> will hold anything.</strong> A quaternion will not, which turns that "
        "line into a compile error, which is the type doing its job. The measured rows are worse "
        "than &ldquo;the scale is dropped&rdquo;: a uniform scale of 2 extracts to something of "
        "norm 1.334 whose matrix has determinant 1.65 and whose pose is 28&deg; wrong &mdash; "
        "neither answer. Taking the scale off as the column lengths first is exact; a "
        "<strong>shear</strong> defeats both routes, because it changes the angles between the "
        "columns and not their lengths. <strong>A narrower type does not only prevent future "
        "mistakes. It finds existing ones</strong>, and this one has been shipping since 5.11."),

    "FIG_SCORECARD": ("l74_fig10.svg", "10",
        "Four lessons, four representations, one table &mdash; and it is not an argument for "
        "deleting three of them. The quaternion column <strong>loses exactly one row</strong>, "
        "applying to a point, and that row has a 4.6&nbsp;ns workaround worth taking after eight "
        "vectors. Everything else is a win or a tie, and two of the wins are things the other "
        "three simply cannot do: interpolate along the geodesic, and extract with no bad case. "
        "But read across rather than down and all four are still right for something. Euler "
        "angles are an <em>interface</em>, which is why Lesson 7.1 built the header and put a "
        "<code>euler_angles</code> in no struct. Axis-angle is a <em>reading</em>, the answer to "
        "&ldquo;what single turn is this pose?&rdquo;. A matrix is what the renderer multiplies "
        "by. A quaternion is what the engine will <em>store</em> &mdash; from Lesson 7.5, for the "
        "reason &sect;11 measures."),
}

LISTING_META = {
    "engine/include/engine/math/quat.hpp":      ("new", "new"),
    "engine/include/engine/math/transform.hpp": ("modified", "modified"),
    "engine/include/engine/engine.hpp":         ("modified", "modified"),
    "demos/gimbal/main.cpp":                    ("modified", "modified"),
    "scratch/verify_74.cpp":                    ("new", "new"),
    "scratch/build_verify_74.sh":               ("new", "new"),
    "scratch/golden_74.cpp":                    ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_74.sh": ("bash", "shell"),
}

LISTING_SOURCE = {
    "engine/include/engine/math/quat.hpp":      "scratch/l74_engine_include_engine_math_quat.hpp",
    "engine/include/engine/math/transform.hpp": "scratch/l74_engine_include_engine_math_transform.hpp",
    "engine/include/engine/engine.hpp":         "scratch/l74_engine_include_engine_engine.hpp",
    "demos/gimbal/main.cpp":                    "scratch/l74_demos_gimbal_main.cpp",
    "scratch/verify_74.cpp":                    "scratch/l74_scratch_verify_74.cpp",
    "scratch/build_verify_74.sh":               "scratch/l74_scratch_build_verify_74.sh",
    "scratch/golden_74.cpp":                    "scratch/l74_scratch_golden_74.cpp",
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
<title>7.4 — Quaternions, Derived · Build a Professional 3D Game Engine</title>
<meta name="description" content="Lesson 7.3 proved that no three-dimensional number system can describe rotations, so a fourth component is forced. This lesson asks what that arithmetic has to BE, and derives every entry of the quaternion multiplication table from one geometric demand: space has no preferred direction, so every unit imaginary must be a half-turn and square to minus one. Three lines then force anticommutativity - it is what isotropy costs, not a design choice - and associativity gives the rest, with Hamilton ijk equals minus one arriving as a consequence rather than a definition. Multiplying two quaternions out reveals the dot product and the cross product inside the product, exactly where Gibbs and Heaviside cut them from in the 1880s, and the cross product turns out to be the single term that changes sign when you swap the operands: quaternions not commuting, the cross product being antisymmetric, and rotations of space not commuting are one fact said three ways. The half-angle is not postulated either. Composing two reflections in planes produces the sandwich q v conj q, the form cos half theta plus sin half theta times the axis, AND the double cover, all three out of one calculation, with the double cover appearing as a sign the derivation cannot determine. Everything is measured: 1.57 times cheaper to compose, 1.57 times dearer to apply, so there is a crossover and it is 7.6 vectors; Shepperd extraction with four candidates that sum to four, so the pivot is never below one and there is no bad case anywhere, against a naive route that is off by 180 degrees at a turn of 179.99; and two measurements that refused the claims they were written to confirm. A quaternion better conditioning turns out to be a property of holding one rather than extracting one, and the cheap renormalisation is exact at norm one AND at norm two and catastrophic between, so a test at the two obvious points certifies a broken function. Ends by finding, with the narrower type, that the engine field called rotation has a caller that does not put a rotation in it.">

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
    <a class="prev-l" href="07-03-complex-numbers.html">
      <span class="dir">← Previous</span>
      <span class="ttl">7.3 — Complex Numbers Rotate the Plane</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="07-05-slerp.html">
      <span class="dir">Next →</span>
      <span class="ttl">7.5 — Slerp, and the Storage Swap</span>
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
        with open(f"scratch/l74_body_{name}.html") as fh:
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
    # BYTES, not characters. Every builder from 3.7 to 7.3 prints `len(page)`
    # and calls it bytes; the page is UTF-8 and full of × − ° ₁, so the number
    # is 2,405 short of what `wc -c` says on this lesson. Harmless — nothing
    # consumes it — and wrong, which is the kind of thing that wastes twenty
    # minutes when somebody eventually diffs the two. Fixed here; the other
    # forty-six are recorded in STATE as a sweep for 9.10 rather than touched
    # by a lesson that has no other business in them.
    print(f"wrote {OUT}  ({len(page.encode('utf-8')):,} bytes, "
          f"{page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
