#!/usr/bin/env python3
"""Assemble docs/lessons/07-01-euler-angles.html.

Same pipeline as build_512.py and build_618.py. No STATE block: STATE.md is the
sole resume key and lesson pages end at Further Reading.

Prose from scratch/l71_body_{a,b,c,d,e}.html, figures from scratch/figs_71.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS ARE WORKING-TREE COPIES AND NOT REPOSITORY PATHS.

Not for 5.12's reason. Lesson 7.1 was written against the head of the repository,
so the live files ARE the lesson-era files today. The pins exist because they will
not be tomorrow: `demos/CMakeLists.txt` is edited by every lesson that adds a
target, `engine/engine.hpp` by every lesson that adds a public header (the
configure-time lint now guarantees it), and `math/transform.hpp` is the file
Lesson 7.4 replaces the stored `mat3` in. Pinning at writing time costs a minute;
recovering a lesson-era listing afterwards costs a session, and eleven builders
found that out on 2026-09-12.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/07-01-euler-angles.html"

FIGURES = {
    "FIG_CHAIN": ("l71_fig1.svg", "1",
        "What &#8220;intrinsic&#8221; means, in three panels sharing one viewpoint. Each knob turns "
        "about an axis the <em>previous</em> knobs have already moved, and the grey stubs marked "
        "&#8220;was here&#8221; are where that axis used to be. <strong>The first panel has no "
        "stub</strong>, and that absence is the whole structure: yaw&#8217;s axis is the world&#8217;s own "
        "vertical and nothing carries it. Pitch&#8217;s axis has been swung 40&#176; by the yaw; "
        "roll&#8217;s has been swung by the yaw and then tipped 55&#176; by the pitch. Read the "
        "product underneath left to right and it is this story; read it right to left, the way "
        "every matrix product in this engine is read, and it is the equally true story of a vertex "
        "being rolled, then pitched, then yawed about fixed world axes. &#167;4.3 proves those are "
        "the same matrix rather than asserting it."),

    "FIG_RIG": ("l71_fig2.svg", "2",
        "The rig, open and locked &#8212; two real renders from this lesson&#8217;s demo, not a "
        "diagram of one. Each hoop&#8217;s plane <em>contains</em> its own pivot axle, which is what "
        "makes it a ring on bearings rather than a turntable and why it visibly tips when its knob "
        "turns. On the left, at pitch 0, three axles in three directions. On the right, at pitch "
        "90&#176;, <strong>the blue axle has swung until it lies exactly along the green one</strong>: "
        "two knobs, one axis. Note what is missing from the right-hand panel &#8212; the magenta "
        "arrow, which the demo draws at the TRUE length of the rotation the weakest knob combination "
        "produces. At lock that length is zero, so there is nothing to draw. The picture is the "
        "measurement, not an illustration of it."),

    "FIG_ORDERS": ("l71_fig3.svg", "3",
        "The same three numbers, <code>(30&#176;, 40&#176;, 50&#176;)</code>, read under each of the "
        "twenty-four conventions, with bar length the angular distance from our reading. "
        "<strong>Exactly one bar is zero.</strong> The worst is <code>yzy</code> intrinsic at "
        "83.48&#176;, but the dangerous one is <code>yzx</code> extrinsic at 16.03&#176;, because "
        "16&#176; reads as a tuning problem rather than as a wrong answer &#8212; you will spend an "
        "afternoon adjusting a value that is not wrong. Twelve axis orders (three choices for the "
        "first axis, two for the second, two for the third) times two frames, intrinsic and "
        "extrinsic, is twenty-four; Shoemake&#8217;s <em>Graphics Gems IV</em> paper encodes exactly "
        "these in four bits."),

    "FIG_JACOBIAN": ("l71_fig4.svg", "4",
        "The determinant, drawn. The three arrows are the axes the three knobs actually turn about, "
        "and the shaded solid is the box they span &#8212; whose volume <strong>is</strong> "
        "<code>|det J|</code>, because that is what a determinant is (Lesson 2.6). At pitch 0 the "
        "box is a unit cube and every direction of angular velocity is reachable. At 60&#176; it is "
        "visibly squashed: the same knob rates buy less turn, and they buy it unevenly. At 90&#176; "
        "the green and blue arrows are <em>anti-parallel</em> and the box has collapsed to a flat "
        "sheet with no volume at all. The yaw is 0&#176; in all three panels because "
        "<code>det J = &#8722;cos(pitch)</code> does not depend on it &#8212; which is not an "
        "accident, since turning the whole aircraft about the vertical cannot change how well its "
        "controls work."),

    "FIG_COLLAPSE": ("l71_fig5.svg", "5",
        "At pitch = +90&#176; the composite matrix contains yaw and roll <em>only</em> as "
        "<code>yaw &#8722; roll</code> (&#167;5.4), so every point on a diagonal of this square is "
        "the <strong>same orientation</strong>. Two knobs, one number, and an entire one-parameter "
        "family of poses that a solver or an animation blend will happily wander along at no cost "
        "and to no effect. The measurement beside it is the point: moving both knobs +47&#176; along "
        "the amber line produces <code>0.0000&#176;</code> of actual rotation, while the identical "
        "joint move at pitch 0 produces <strong>65.51&#176;</strong> &#8212; which is the control, "
        "and is close to the <code>47&#8730;2 = 66.47&#176;</code> that two orthogonal turns would "
        "give. At pitch &#8722;90&#176; it is the SUM that survives, so the dead direction is the "
        "other diagonal."),

    "FIG_CONDITIONING": ("l71_fig6.svg", "6",
        "What a unit of knob buys, as the pose closes on the singularity. The blue curve is "
        "<code>&#963;<sub>min</sub></code>, the weakest gain of the rate Jacobian; the amber dots "
        "are a brute-force sweep of a million unit rate vectors per pose, which assumes nothing "
        "about the algebra that produced the curve. At a hundredth of a degree from vertical you "
        "would need <strong>8,100 rad/s of knob for 1 rad/s of aircraft</strong> &#8212; which is "
        "what &#8220;the control loop saturates and lurches&#8221; means in numbers. The red dashed "
        "line is the <em>algebraically identical</em> spelling "
        "<code>&#8730;(1 &#8722; |sin p|)</code>, and it is worth studying twice: the cliff at "
        "0.01&#176;, where <code>float</code> cancellation makes it return exactly zero at a pose "
        "that is fine, and the 0.9% drift at 89.9&#176; <em>before</em> the cliff, which is the part "
        "that gets shipped."),

    "FIG_PATH": ("l71_fig7.svg", "7",
        "Interpolating three angles does not interpolate the rotation. Left: the craft&#8217;s nose, "
        "traced along an Euler lerp (amber) and along the geodesic between the same two "
        "orientations (dashed blue). Same start, same end, <strong>25.24&#176; of detour</strong>, "
        "and a rate that varies by 1.56&#215; along the way &#8212; which is what a blend that "
        "surges in the middle looks like from the outside. Right: the <em>same three angle "
        "deltas</em> run at four pitch bands, changing nothing but how close the path comes to the "
        "singularity. Near lock the move costs <strong>three times the turning it needs</strong>. "
        "The control under it is the one that made this measurement mean anything: a lerp that "
        "moves only ONE knob is already a geodesic, exactly, at any pitch &#8212; so the first "
        "draft of this check swept the yaw alone at 88&#176; and correctly reported no detour at "
        "all."),

    "FIG_METRIC": ("l71_fig8.svg", "8",
        "Two spellings of one formula, measured against turns of known size. The red curve is the "
        "expression every reference gives for the angle between two rotations, "
        "<code>acos((tr R &#8722; 1)/2)</code>; at a true angle of 0.004&#176; its relative error is "
        "<strong>1.00</strong>, meaning it returns zero. That is not a rounding concern &#8212; a "
        "2,048-step path across 120&#176; takes 0.059&#176; steps, so &#167;7&#8217;s entire "
        "measurement would have been summing noise. The blue curve is the <code>atan2</code> form, "
        "which takes the sine from the antisymmetric part of the matrix, where it is carried by "
        "<em>differences</em> of entries rather than hidden inside a 3. Hollow markers are "
        "measurements that came back as exactly zero and therefore have no place on a log axis; "
        "they are drawn on the floor rather than at an invented value."),
}

LISTING_META = {
    "engine/include/engine/math/euler.hpp":     ("new", "new"),
    "engine/include/engine/math/transform.hpp": ("modified", "modified"),
    "engine/include/engine/engine.hpp":         ("modified", "modified"),
    "demos/gimbal/main.cpp":                    ("new", "new"),
    "demos/CMakeLists.txt":                     ("modified", "modified"),
    "scratch/verify_71.cpp":                    ("new", "new"),
    "scratch/build_verify_71.sh":               ("new", "new"),
    "scratch/golden_71.cpp":                    ("new", "new"),
}

LISTING_LANG = {
    "demos/CMakeLists.txt":       ("cmake", "CMake"),
    "scratch/build_verify_71.sh": ("bash", "shell"),
}

LISTING_SOURCE = {
    "engine/include/engine/math/euler.hpp":     "scratch/l71_engine_include_engine_math_euler.hpp",
    "engine/include/engine/math/transform.hpp": "scratch/l71_engine_include_engine_math_transform.hpp",
    "engine/include/engine/engine.hpp":         "scratch/l71_engine_include_engine_engine.hpp",
    "demos/gimbal/main.cpp":                    "scratch/l71_demos_gimbal_main.cpp",
    "demos/CMakeLists.txt":                     "scratch/l71_demos_CMakeLists.txt",
    "scratch/verify_71.cpp":                    "scratch/l71_scratch_verify_71.cpp",
    "scratch/build_verify_71.sh":               "scratch/l71_scratch_build_verify_71.sh",
    "scratch/golden_71.cpp":                    "scratch/l71_scratch_golden_71.cpp",
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
<title>7.1 — Euler Angles and Their Pathologies · Build a Professional 3D Game Engine</title>
<meta name="description" content="Every rotation in this engine so far has been composed at a call site, never typed. Twenty-six calls to rotation_x, rotation_y and rotation_z across the demos, four of which multiply two together - and each of those four invented its own private Euler convention on the spot, including two on adjacent lines of demo_scene.cpp that disagree deliberately. Nobody has ever handed this engine three numbers and said 'that is the orientation'. A designer does; a file does; a debug panel with three sliders does. So: what are the three numbers? The answer is that there are twenty-four self-consistent ways to say, they are all in use, and reading (30, 40, 50) under the wrong one puts the nose up to 83 degrees away - measured, not estimated. We fix one convention, write it into the Conventions page, and derive the matrix it names. Then we meet the hole. Three numbers cannot parameterise rotation without a singularity; that is a theorem, and what a choice of axis order decides is only where the hole sits. Ours puts it at pitch plus or minus 90, nose straight up, which every first-person camera already forbids. Most treatments show you the gimbals and stop. This one prints the number: the matrix turning knob rates into angular velocity has determinant minus cosine of the pitch, its weakest gain is |cos p| over root (1 + |sin p|), and a hundredth of a degree from lock you would need 464,000 degrees per second of knob to turn the aircraft at one. Then the real indictment - interpolation. Lerping three angles takes 14 per cent more turning than it needs on a generic pair and 209 per cent near lock, with the rate varying by half again along the way. Along the way the lesson trips over one numerical trap three separate times, in the conditioning formula, in the extraction, and in the textbook expression for the angle between two rotations, which returns exactly zero for a turn of 0.004 degrees and is therefore incapable of measuring the thing every claim here is measured with.">

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
    <a class="prev-l" href="06-18b-compute-particles.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.18b — Compute Shaders: GPU Particles</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="07-02-axis-angle.html">
      <span class="dir">Next →</span>
      <span class="ttl">7.2 — Axis-Angle and Rodrigues’ Rotation Formula</span>
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
        with open(f"scratch/l71_body_{name}.html") as fh:
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
