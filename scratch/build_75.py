#!/usr/bin/env python3
"""Assemble docs/lessons/07-05-slerp.html.

Same pipeline as build_71.py through build_74.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l75_body_{a,b,c,d,e}.html, figures from scratch/figs_75.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. This lesson lists FIFTEEN files, twelve
of them modified, and several are the most-edited files in the repository:
`math/quat.hpp` and `math/transform.hpp` are both certain to move again in 7.6
(a skeleton is a hundred of these), and `demos/gimbal/main.cpp` has gained a mode
in every Module 7 lesson so far. A page whose listings float with the repository
is a page that stops being true the moment the next lesson compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order. Figures 4 and 5 were DRAFTED in the opposite
order to the page (the cap before the sign) and their filenames were swapped to
match; nothing in this pipeline can catch that for itself — every figure still
renders, just under the wrong number.
"""
import os
import re

OUT = "docs/lessons/07-05-slerp.html"

FIGURES = {
    "FIG_ONE_ROAD": ("l75_fig1.svg", "1",
        "<strong>Almost nothing in this lesson's slerp is new, and that is the finding rather "
        "than an apology.</strong> Across the top, the three steps: undo <code>a</code>, take "
        "<code>t</code> of the difference, redo <code>a</code>. Below, the same three steps "
        "written out in three algebras — Lesson 7.2's for <code>mat3</code>, 7.3's for complex "
        "numbers in the plane, and this lesson's for <code>quat</code>. <code>a·(a⁻¹b)ᵗ</code> "
        "is not a fact about quaternions; it is <strong>the definition of a geodesic on any group "
        "with an inverse, a product and a power</strong>, which is why substituting the type is "
        "the whole of the implementation. The two numbers at the bottom are what turns that claim "
        "into a measurement: 7.2's matrix version and 7.3's plane version share no code with the "
        "new one or with each other, and all three walk the same journey to float resolution. The "
        "control is the thing a matrix cannot do at all — average two rotations entrywise and the "
        "determinant comes out at <strong>0.00007</strong> instead of 1, which is not a rotation "
        "and is not near one."),

    "FIG_ARC_CHORD": ("l75_fig2.svg", "2",
        "<strong>One road, two rulers.</strong> A great circle, drawn flat because it is a circle "
        "whatever it is embedded in, with <code>a</code> and <code>b</code> on it and the chord "
        "between them in gold. Eleven equally spaced values of <code>t</code> are marked twice. "
        "The outward ticks are slerp, and they are evenly spaced, because the angle covered is "
        "<code>t·Ω</code> and nothing else in the expression depends on <code>t</code>. The "
        "inward ticks are nlerp: equal steps along the <em>chord</em>, projected back out to the "
        "circle — and they bunch at both ends. <strong>The path is identical and only the "
        "schedule differs</strong>, which is the single most misunderstood fact about nlerp in "
        "both directions: it is not a cheap approximation that wanders off course, and it is not "
        "free. The chord from <code>a</code> to <code>b</code> lies in the plane those two points "
        "span with the origin, and normalising moves a point along its own radius, so it "
        "<em>cannot</em> leave the circle. Measured departure from that plane over 501 samples: "
        "<strong>1.605e−07</strong>. And note where the two rows meet exactly — at the middle, "
        "which §7.1 shows makes the obvious unit test useless."),

    "FIG_SCHEDULE": ("l75_fig3.svg", "3",
        "The schedule error, derived and then measured on top of the derivation. The curve is "
        "<code>φ(t) = arctan(t sin Ω / ((1−t) + t cos Ω))</code> from §5.3, maximised over "
        "<code>t</code> and converted back into degrees of <em>pose</em> — which doubles it, "
        "because the sphere arc is half the rotation arc. The six dots are the harness's "
        "measurements at 30° through 180°, and they sit on it. <strong>The bottom row of the "
        "table is the worst case that exists</strong>, not merely the worst case sampled: once "
        "<code>nearest</code> has chosen the hemisphere, a rotation arc cannot exceed 180°, so "
        "<code>Ω</code> cannot exceed 90°, so <code>sec²(Ω/2)</code> cannot exceed "
        "<code>sec²(45°) = 2</code> exactly. Read the left panel as a design rule: under about "
        "60° of arc the cheap function is indistinguishable from the expensive one, which is why "
        "a great deal of shipped animation code uses nlerp and is right to."),

    "FIG_SIGN": ("l75_fig4.svg", "4",
        "<strong>The single most common quaternion bug in shipped code, and it is one missing "
        "comparison.</strong> Two animation keyframes describing poses one degree apart, stored "
        "with opposite signs — which an exporter has no reason to avoid, because <code>q</code> "
        "and <code>−q</code> are the same pose and no file format says which to write. They are "
        "<em>opposite points</em> on the sphere, though, so the arc between them as written is "
        "<strong>359°</strong>: a whole-body spin lasting exactly one keyframe interval, in the "
        "middle of a walk cycle, which reads as “the animation is corrupt” rather than as "
        "“somebody forgot a dot product”. The two points are drawn 14° apart so they can be "
        "told apart; the real gap is 1°. The frequency is the part that makes this a design "
        "input rather than an edge case: <strong>49.6% of uniformly sampled pairs need the "
        "flip</strong>. It is a coin toss. And nlerp's version is worse — the raw chord between "
        "antipodal representatives passes within <strong>0.00436</strong> of the origin, where "
        "there is no direction left to normalise."),

    "FIG_CAP": ("l75_fig5.svg", "5",
        "<strong>The thing that looked like a nuisance is what bounds the error.</strong> On the "
        "left, the argument in one picture: <code>nearest</code> forces the four-component dot "
        "product non-negative, so the arc actually walked never leaves one hemisphere and "
        "<code>Ω ≤ 90°</code>. Substitute that into §5.3's closed form and "
        "<code>sec²(Ω/2) ≤ sec²(45°) = 2</code> — <em>exactly</em> 2, over every pair of "
        "orientations that exists. On the right, what that is worth: Lesson 7.3 measured the same "
        "function in the plane, where there is no double cover to exploit, reaching a speed ratio "
        "of <strong>13,131</strong> and a positional error of <strong>26.34°</strong>. Neither "
        "is reachable here. The measured columns are the check — 20,000 uniformly sampled pairs, "
        "largest sphere arc <strong>89.9969°</strong> against a bound of 90 and largest gap "
        "<strong>8.1345°</strong> against a bound of 8.1491 — with the control being the same "
        "pairs without the comparison, where the arc reaches 176° and no bound applies at all. "
        "<strong>The cost and the guarantee are the same fact.</strong>"),

    "FIG_RENDER": ("l75_fig6.svg", "6",
        "Two real renders from <code>demos/gimbal</code>'s new <kbd>S</kbd> mode, not diagrams of "
        "them. Both rows of ticks touch the <em>same</em> circle — outward for slerp, inward for "
        "nlerp — because drawing the second row on a smaller circle would say the two paths "
        "differ, which is exactly the misconception the mode exists to kill. On the left, "
        "<code>t = 0.229</code>, where the two poses are <strong>4.5140°</strong> apart: the "
        "worst this arc gets, and still a third of a unit at this radius, which is why the ticks "
        "carry the argument and the glyphs only say which way each pose faces. On the right, "
        "<code>t = 0.5</code> — and there is one glyph, because <strong>the two functions agree "
        "exactly at the midpoint</strong>. That is a symmetry rather than a coincidence, and it "
        "is why a test sampling <code>t = 0</code>, <code>0.5</code> and <code>1</code> certifies "
        "nlerp as slerp. Two drawing decisions are load-bearing and both were mistakes first: the "
        "rotation axis points at the camera, so the path renders as a circle rather than an "
        "ellipse whose arc lengths are not proportional to its angles; and the teal axis is "
        "therefore a <em>dot</em> at the centre rather than a line."),

    "FIG_METRIC": ("l75_fig7.svg", "7",
        "<strong>The instrument had to be rewritten before anything could be measured.</strong> "
        "On the left, why: near zero the cosine is <em>quadratic</em> in the angle and the sine is "
        "<em>linear</em>, so a small separation puts the dot product within a few <code>float</code> "
        "ulps of 1 and <code>acos</code> is reading an angle off a quantity that has already lost "
        "most of it. The shaded band is everything that rounds to a dot product of exactly 1. The "
        "floor is computable — <code>2√(2·eps) = 0.0560°</code> — and a path integral over "
        "4,096 steps of a 150° arc takes steps of 0.037°, every one of them below it. On the "
        "right, the consequence: Lesson 7.4's <code>2·acos|a·b|</code> sums slerp's own geodesic "
        "to <strong>88.3437°</strong> of a 150° arc — <strong>−41.10%</strong> — because "
        "every step is quantised downward and 4,096 biased errors do not average out. The "
        "<code>atan2</code> form reports 150.0000°. <strong>The function did not change; the "
        "question did</strong>, from a separation to a step, and nothing in the name or the type "
        "marks the difference."),

    "FIG_COST": ("l75_fig8.svg", "8",
        "Three interpolations, three lessons, one benchmark — and interpolation is the row where "
        "the fourth component pays for itself outright. Lesson 7.4 had to report quaternions "
        "1.57× cheaper to compose and 1.57× <em>dearer</em> to apply to a vector; here the "
        "quaternion route is <strong>2.81× cheaper</strong> than the <code>mat3</code> slerp it "
        "replaces, which extracts an axis and an angle, scales the angle and rebuilds with "
        "Rodrigues. <strong>The number on the right is the one to design with.</strong> A speed "
        "ratio is hard to act on; “nlerp stays within half a degree of slerp up to an arc of "
        "73.50°” is a threshold you can put in a comment beside a call. A 30 Hz clip's adjacent "
        "keyframes are far below it. A turret swinging to a new target is not."),

    "FIG_SWAP": ("l75_fig9.svg", "9",
        "<strong>The swap <code>math/transform.hpp</code> has promised since Lesson 2.8</strong>, "
        "and what it cost. The <code>rotation</code> field goes from nine floats to four and the "
        "struct from fifteen to ten, and the arithmetic in "
        "<code>parent_from_local</code> does not move a character — which is the whole argument "
        "for having a named type there rather than three loose variables. The table is the "
        "interesting half. Twenty-five call sites moved, and <strong>three of them were assigning "
        "something that was not a rotation</strong> to a field called <code>rotation</code>: a "
        "<code>mat3</code> will hold anything, including the scale that came down a hierarchy, and "
        "one of the three had been shipping in the engine since Lesson 5.11. The last three rows "
        "are the honest check on whether a round trip through four floats moves the picture. Worst "
        "matrix entry over 20,000 poses: <strong>5.960e−07</strong>, which at three units out in "
        "a 960-pixel frame is <strong>0.00014 pixels</strong> — and the golden render, which has "
        "four of this lesson's edited files in its include closure, comes back "
        "<strong>byte-identical</strong>."),

    "FIG_BOOM": ("l75_fig10.svg", "10",
        "<strong>The bug a narrower type found in a published demo.</strong> Lesson 5.12's chase "
        "camera hangs off a boom that cancels the rover's cosmetic roll <em>and</em> its "
        "non-uniform scale, and the algebra it derived is right: the inverse of a product reverses "
        "its order, so the unscale comes <em>first</em>. But a <code>transform</code> builds "
        "<code>T·R·S</code> with the scale innermost — so a single node can say “rotate then "
        "scale” and cannot say “scale then rotate”, and when the scale is non-uniform those are "
        "different matrices. The difference is a <strong>shear</strong>: columns that are no longer "
        "perpendicular, living in a field called <code>rotation</code>, since Lesson 5.12, without "
        "complaint. <strong>The repair is not a wider type — it is one more link.</strong> Put the "
        "unscale in one node and the unroll in its child and the hierarchy multiplies them "
        "parent-first, which is the order the derivation asked for; measured difference from the "
        "old basis, <strong>0.000e+00</strong>. The bottom table is the case that stays outside the "
        "struct even so, and which <code>collect_renderables</code> now counts rather than hides."),
}

LISTING_META = {
    "engine/include/engine/math/quat.hpp":        ("modified", "modified"),
    "engine/include/engine/math/transform.hpp":   ("modified", "modified"),
    "engine/include/engine/gfx/renderable.hpp":   ("modified", "modified"),
    "engine/src/gfx/renderable.cpp":              ("modified", "modified"),
    "engine/include/engine/ecs/camera.hpp":       ("modified", "modified"),
    "engine/include/engine/gfx/scene.hpp":        ("modified", "modified"),
    "demos/common/demo_scene.cpp":                ("modified", "modified"),
    "demos/hello_cube/main.cpp":                  ("modified", "modified"),
    "demos/gltf_view/main.cpp":                   ("modified", "modified"),
    "demos/ecs_swarm/main.cpp":                   ("modified", "modified"),
    "demos/collector/main.cpp":                   ("modified", "modified"),
    "demos/gimbal/main.cpp":                      ("modified", "modified"),
    "scratch/verify_75.cpp":                      ("new", "new"),
    "scratch/build_verify_75.sh":                 ("new", "new"),
    "scratch/golden_75.cpp":                      ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_75.sh": ("bash", "shell"),
}


def _pin(path):
    return "scratch/l75_" + path.replace("/", "_")


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
<title>7.5 — Slerp, and the Storage Swap · Build a Professional 3D Game Engine</title>
<meta name="description" content="Three lessons have derived the same three steps in three algebras, and this one writes them for the fourth time in three lines, because a times (a inverse b) to the t is not a fact about quaternions - it is the definition of a geodesic on any group with an inverse, a product and a power. What is genuinely new is the half with no analogue in the plane. The double cover means every pair of orientations has two arcs between it, one of them wrong, and 49.6 percent of random pairs name the wrong one; the fix is a single dot-product comparison and the bug without it is a character spinning 359 degrees between two keyframes a degree apart. Then the same fact pays a dividend nobody advertises: because that comparison forces the arc into one hemisphere, nlerp's schedule error - unbounded in the plane, where Lesson 7.3 measured a factor of 13,131 - is capped here at exactly 2, and at 8.1491 degrees of pose. The thing that looked like a nuisance is what bounds the error. Two measurements refused the claim they were written to confirm. The lesson's own metric had to be rewritten before anything could be measured: angle_between shipped in 7.4 as two acos of the dot product, which is the formula every reference gives and is blind near zero, and it scored slerp's own geodesic at minus 41.10 percent of its own arc. And a table of small-arc errors printed a clean zero for a function returning NaN, because std::max of x and NaN returns x. Then the storage swap this engine has promised since Module 2: transform rotation becomes a quat, twenty-five call sites move, and the narrower type finds two more places that had never put a rotation in a field called rotation, one of them a published demo whose camera boom had been storing a sheared basis since Lesson 5.12. The golden render is byte-identical across the swap.">

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
    <a class="prev-l" href="07-04-quaternions.html">
      <span class="dir">← Previous</span>
      <span class="ttl">7.4 — Quaternions, Derived</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="07-06-skeletal-animation.html">
      <span class="dir">Next →</span>
      <span class="ttl">7.6 — Skeletal Animation: The Skinning Math</span>
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
        with open(f"scratch/l75_body_{name}.html") as fh:
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
