#!/usr/bin/env python3
"""Assemble docs/lessons/07-03-complex-numbers.html.

Same pipeline as build_71.py and build_72.py. No STATE block: STATE.md is the
sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l73_body_{a,b,c,d,e}.html, figures from scratch/figs_73.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. Three of the six listed files are certain
to be edited by the next lesson or two. `engine/engine.hpp` is touched by every
lesson that adds a public header (the configure-time lint guarantees it, four
times in three lessons now), `demos/CMakeLists.txt` by every lesson that adds a
demo, and `math/complex.hpp` itself is the one to watch hardest: 7.4 writes
`quat.hpp` by analogy with it, and "by analogy" is how a header acquires a
`slerp` overload or a shared helper without anybody deciding to change this file.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order. 7.2 wrote two figures in the opposite order
to the page and had to swap them, and nothing in this pipeline can catch that for
itself — every figure still renders, just under the wrong number.
"""
import os
import re

OUT = "docs/lessons/07-03-complex-numbers.html"

FIGURES = {
    "FIG_QUARTER": ("l73_fig1.svg", "1",
        "<strong>i&sup2; = &minus;1 is a measurement of the plane, not a definition.</strong> "
        "On the left, the point <code>(1, 0)</code> turned a quarter circle anticlockwise to "
        "<code>(0, 1)</code>, and turned again to <code>(&minus;1, 0)</code>. Nothing algebraic "
        "happened: a quarter turn was performed twice and the result was observed to be the "
        "point opposite the start, which is what multiplying by <code>&minus;1</code> does. On "
        "the right, the identical three arrows relabelled <code>1</code>, <code>i</code> and "
        "<code>&minus;1</code>. That is the whole content of the equation, and the reason this "
        "lesson can derive the rest of complex arithmetic without postulating anything: "
        "&sect;3.1's multiplication table, &sect;4's product, &sect;6's Euler formula and "
        "&sect;7's half-angle are all consequences of the left-hand panel."),

    "FIG_PRODUCT": ("l73_fig2.svg", "2",
        "Multiplying by <code>z</code> scales by <code>|z|</code> and turns by "
        "<code>arg z</code>, and the proof is that the two triangles are the same shape. The "
        "dashed one has vertices <code>0, 1, w</code>; the solid one is its image under "
        "&ldquo;multiply by <code>z</code>&rdquo;, with vertices <code>0, z, zw</code>. "
        "<strong>They are drawn at different scales on purpose</strong> &mdash; the right-hand "
        "panel is at one fifth &mdash; because at a common scale the image is five times the "
        "size of the original and &ldquo;same shape&rdquo; becomes something you take on trust "
        "rather than something your eye can check. The factor of five is stated between the "
        "panels instead. The two arcs are the identical angle, once carried from <code>1</code> "
        "to <code>z</code> and once from <code>w</code> to <code>zw</code>, which is what "
        "&ldquo;arguments add&rdquo; looks like. Every number is exact: "
        "<code>(3+4i)(1+2i) = &minus;5+10i</code> needs no rounding at any step."),

    "FIG_LAYOUT": ("l73_fig3.svg", "3",
        "Where the two extra floats went. A <code>mat2</code> holding a rotation stores "
        "<code>cos&thinsp;&theta;</code> and <code>sin&thinsp;&theta;</code> <strong>twice</strong>, "
        "once with a sign flipped &mdash; which is not an observation about this engine's storage "
        "but a consequence of &sect;5's identification: the second column of such a matrix is "
        "always the first turned a quarter circle, so it carries no information the first does "
        "not. The table prices the duplication on every axis the engine cares about, and every "
        "row favours the complex number except one. <strong>That one is the honest row.</strong> "
        "Applying a rotation to a point is 4 multiplies and 2 adds either way &mdash; the same "
        "four multiplies &mdash; so this representation is cheaper to keep and cheaper to "
        "combine and not one instruction cheaper to use. Lesson 7.4 measures the three-dimensional "
        "version of that same trade, where the numbers differ and the shape does not."),

    "FIG_EULER": ("l73_fig4.svg", "4",
        "Euler's formula without a power series. A point going round the unit circle at unit "
        "speed has velocity perpendicular to its position and of the same length &mdash; the "
        "blue arrow is where it is, the teal arrow is where it is going &mdash; and "
        "&ldquo;perpendicular, same length&rdquo; is precisely what multiplying by "
        "<code>i</code> does, by the definition in &sect;3. So <code>z&prime; = iz</code>, which "
        "is the differential equation whose solution is an exponential, and the initial "
        "condition <code>z(0) = 1</code> picks out <code>e^{i&theta;}</code>. The picture also "
        "already tells you the answer is <code>cos&thinsp;&theta; + i&thinsp;sin&thinsp;&theta;</code>, "
        "because that is where the point is; the identity is the two descriptions meeting. "
        "&sect;6.1 then gets the angle-addition formulas out for free, which is the first sign "
        "that this notation pays for itself rather than merely being elegant."),

    "FIG_MIRRORS": ("l73_fig5.svg", "5",
        "<strong>Where the <code>&theta;/2</code> in every quaternion comes from, three lessons "
        "before anyone says the word.</strong> Two mirrors through the origin, 30&deg; apart. "
        "The pale probe starts on the real axis; reflected in the first mirror it lands at "
        "40&deg;, reflected in the second it lands at 60&deg;. Thirty degrees of mirror "
        "separation produced sixty degrees of rotation, and the panel gives the one line of "
        "algebra that makes it inevitable: a reflection is <code>m&sup2;&thinsp;conj(v)</code>, "
        "so the mirror's own angle enters <em>doubled</em>, and composing two collects into "
        "<code>(m&#8321;&thinsp;conj(m&#8320;))&sup2;</code>. The gold arrow is that inner object, "
        "the <strong>rotor</strong>, sitting at 30&deg; &mdash; exactly halfway round the journey "
        "the teal arrow made. It is what a quaternion stores. Measured over 4,000 mirror pairs "
        "at 3.8e&minus;06 (verify_73 &sect;E.4), and the control that makes that number mean "
        "something is &sect;E.6: run the mirrors in the other order and the rotation reverses, "
        "so the measurement is not passing because both sides are secretly the identity."),

    "FIG_MIRRORS_RENDER": ("l73_fig6.svg", "6",
        "The same construction running, with the program's own receipt beside it &mdash; a real "
        "render from <code>demos/plane</code>, not a diagram of one. Violet and blue are the two "
        "mirrors, white is the probe, grey is it reflected once, teal is it reflected twice, and "
        "gold is the rotor. The printed line is computed from the same two floats that drew the "
        "picture, which is what makes this a measurement rather than an illustration. "
        "<strong>The thing to do is hold <kbd>&uarr;</kbd> and watch the gold arrow</strong>: it "
        "moves at exactly half the rate of the teal one, for as long as you hold the key, and "
        "that is not a tuning of the demo but the content of <code>R&sup2;</code>. Note also "
        "what the render does <em>not</em> show &mdash; a grid. The figure pipeline samples each "
        "3&times;3 block's brightest pixel, so graph paper at the demo's original intensity "
        "survived the downsample and buried the construction inside it; the demo now draws its "
        "grid below that threshold, where it still reads on a screen and drops out of a capture."),

    "FIG_COST": ("l73_fig7.svg", "7",
        "The four claims of &sect;5.2, measured. <strong>Composing is 1.88&times; cheaper</strong> "
        "than a <code>mat2</code> product, which for once is close to what the operation counts "
        "predict &mdash; there is no transcendental anywhere to swamp them. The third bar is the "
        "answer most people reach for first and the worst of the three: &ldquo;just store the "
        "angle&rdquo; composes for free and cannot be <em>used</em> without a "
        "<code>cos</code> and a <code>sin</code>, so the fair unit of work is &ldquo;compose and "
        "be ready to apply&rdquo; and measured that way it is 6.45&times;. <strong>The middle "
        "panel is the one worth staring at.</strong> Applying a rotation to a point is a wash at "
        "1.05&times;, because <code>z &times; v</code> and <code>M &times; v</code> are literally "
        "the same four multiplies &mdash; which is why a renderer that has been handed "
        "quaternions converts them to matrices before it draws anything. The right-hand panel "
        "asks both representations the <em>same</em> question after a million composed rotations, "
        "<code>max |M&#7511;M &minus; I|</code>, because &ldquo;how big is the modulus error&rdquo; "
        "and &ldquo;how non-orthonormal is the matrix&rdquo; are not comparable numbers."),

    "FIG_BLEND_RENDER": ("l73_fig8.svg", "8",
        "One arc, two schedules &mdash; the whole of &sect;10 in a picture with no numbers in it. "
        "Sixteen tick marks at equally spaced values of <code>t</code>: teal outside the circle "
        "for <code>complex_slerp</code>, amber inside it for <code>complex_nlerp</code>. The teal "
        "ticks are evenly spaced, because the angle slerp covers is <code>t &times; 145&deg;</code> "
        "and nothing else. The amber ticks crowd at both ends of the arc and open out through the "
        "middle &mdash; <strong>on the same arc</strong>, which is the part that surprises people. "
        "Nlerp cannot leave the geodesic: the straight chord it walks (the faint line) never "
        "leaves the plane the endpoints span, and normalising changes a modulus and never an "
        "argument, so its excess turning is <strong>0.00%</strong>. What it gets wrong is the "
        "schedule, by exactly <code>sec&sup2;(&Omega;/2)</code>. The two arrows are at "
        "<code>t = 0.24</code>, which is where the disagreement peaks: the amber craft is 21&deg; "
        "behind, and it catches up by arriving at the same place at the same moment &mdash; which "
        "is precisely why this failure mode is invisible in a code review and obvious in motion."),

    "FIG_SCHEDULE": ("l73_fig9.svg", "9",
        "The derivation of &sect;10.3, and then the number you would actually write in a design "
        "document. On the left, nlerp's fastest step divided by its slowest, against the size of "
        "the arc, on a log scale that has to span four decades because the ratio does: the blue "
        "curve is the closed form <code>sec&sup2;(&Omega;/2)</code> and the amber dots are "
        "measurements at seven arcs, agreeing to two parts in ten thousand even at a ratio of "
        "thirteen thousand. On the right, the question an animation programmer has instead: at "
        "the same <code>t</code>, how far apart do the two blends actually get? Under 60&deg; of "
        "arc the cheap blend is wrong by about a degree and nobody will ever see it; at 150&deg; "
        "it is wrong by 26&deg; and the motion visibly lurches through its middle. <strong>That "
        "turns &ldquo;slerp or nlerp?&rdquo; from a preference into a threshold on the arc</strong> "
        "&mdash; which is why a great deal of shipped animation code blends adjacent keyframes "
        "with nlerp and is entirely right to."),

    "FIG_RECURRENCE": ("l73_fig10.svg", "10",
        "A famous optimisation, measured, and then explained by the row that explains it. "
        "Generating a circle with one complex multiply per point instead of a "
        "<code>cos</code>/<code>sin</code> pair should be five or six times faster by operation "
        "count; it measures <strong>1.21&times;</strong>. The third bar is why: "
        "<code>z = z &times; step</code> is a <em>serial dependency chain</em>, so the loop "
        "measures instruction latency, while the trig loop computes every point independently "
        "from its own index and therefore measures throughput. Interleave four independent chains "
        "and the recurrence gets the same freedom &mdash; 0.526&nbsp;ns, <strong>3.52&times;</strong>. "
        "The optimisation was real and was hiding behind a dependency chain, and no amount of "
        "counting multiplies would have found that. The table on the right is the accuracy it "
        "costs, and it carries a second finding: the cheap renormalise <em>perfectly</em> "
        "controls the modulus at every length and does nothing at all for the angle &mdash; past "
        "a quarter of a million steps it makes the angle <em>worse</em>, because it is one more "
        "rounding operation per step applied to a quantity it cannot correct."),

    "FIG_FORCED": ("l73_fig11.svg", "11",
        "Why Hamilton needed thirteen years and then a fourth dimension. Assume a "
        "three-dimensional number system spanned by <code>1</code>, <code>i</code> and "
        "<code>j</code>, with <code>i&sup2; = j&sup2; = &minus;1</code> and an associative "
        "product. Then <code>ij</code> has to land somewhere in that span, so write it as "
        "<code>a + bi + cj</code> and multiply on the left by <code>i</code>: associativity gives "
        "<code>&minus;j</code> on one side, expansion gives <code>c&sup2;</code> as the "
        "<code>j</code> coefficient on the other, and matching them demands a <em>real</em> "
        "<code>c</code> with <code>c&sup2; = &minus;1</code>. There is none. <strong>The fourth "
        "basis element is forced by three lines of algebra</strong>, not chosen for elegance. And "
        "the plane could never have warned us about the other thing that has to go, because the "
        "sandwich every quaternion text uses &mdash; <code>z v conj(z)</code> &mdash; is exactly "
        "the identity here, measured over 4,000 cases at 9.6e&minus;07: it collapses precisely "
        "<em>because</em> this algebra commutes."),
}

LISTING_META = {
    "engine/include/engine/math/complex.hpp": ("new", "new"),
    "engine/include/engine/engine.hpp":       ("modified", "modified"),
    "demos/plane/main.cpp":                   ("new", "new"),
    "demos/CMakeLists.txt":                   ("modified", "modified"),
    "scratch/verify_73.cpp":                  ("new", "new"),
    "scratch/build_verify_73.sh":             ("new", "new"),
}

LISTING_LANG = {
    "demos/CMakeLists.txt":       ("cmake", "CMake"),
    "scratch/build_verify_73.sh": ("bash", "shell"),
}

LISTING_SOURCE = {
    "engine/include/engine/math/complex.hpp": "scratch/l73_engine_include_engine_math_complex.hpp",
    "engine/include/engine/engine.hpp":       "scratch/l73_engine_include_engine_engine.hpp",
    "demos/plane/main.cpp":                   "scratch/l73_demos_plane_main.cpp",
    "demos/CMakeLists.txt":                   "scratch/l73_demos_CMakeLists.txt",
    "scratch/verify_73.cpp":                  "scratch/l73_scratch_verify_73.cpp",
    "scratch/build_verify_73.sh":             "scratch/l73_scratch_build_verify_73.sh",
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
<title>7.3 — Complex Numbers Rotate the Plane · Build a Professional 3D Game Engine</title>
<meta name="description" content="Lesson 7.2 ended with the one thing axis-angle cannot do: there is no usable formula for composing two turns. This lesson goes down a dimension and finds a representation where composing IS one multiplication - four multiplies, two adds, no trigonometry. Nothing is postulated: i squared equals minus one is derived as a measurement of the plane (a quarter turn, done twice, is multiplication by minus one), and the product, the angle-addition formulas and Euler's formula all follow from that one observation. Euler's formula arrives from a differential equation you can see rather than a power series - the velocity of a point circling at unit speed is perpendicular to its position, and perpendicular IS multiplication by i. Complex numbers are then proved to BE the 2x2 rotation-and-scale matrices, which is also the proof that a mat2 holding a rotation stores cosine and sine twice. Then the section that pays for the whole module: two reflections in mirrors phi apart make a rotation of 2 phi, so the object a rotation is BUILT from carries half its angle. That is the theta/2 in every quaternion, visible in the plane with a protractor, and the double cover comes with it for the same reason. Everything is measured - 1.88x cheaper to compose, 1.05x (a wash) to apply, 6.15x less drift - including a cheap renormalise whose failure mode is quiet enough that a unit test on the modulus certifies a function that turns the object 180 degrees the wrong way. Interpolation gets the treatment 7.5 inherits: nlerp travels exactly the right path at exactly the wrong speed, and the speed ratio is sec squared of half the arc, derived and then measured at seven arcs. The lesson ends by proving the fourth dimension is forced: assume a three-dimensional number system, ask where ij goes, and three lines of algebra demand a real number whose square is minus one.">

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
    <a class="prev-l" href="07-02-axis-angle.html">
      <span class="dir">← Previous</span>
      <span class="ttl">7.2 — Axis-Angle and Rodrigues' Rotation Formula</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="07-04-quaternions.html">
      <span class="dir">Next →</span>
      <span class="ttl">7.4 — Quaternions, Derived</span>
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
        with open(f"scratch/l73_body_{name}.html") as fh:
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
