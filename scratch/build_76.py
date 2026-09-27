#!/usr/bin/env python3
"""Assemble docs/lessons/07-06-skeletal-animation.html.

Same pipeline as build_71.py through build_75.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l76_body_{a,b,c,d,e}.html, figures from scratch/figs_76.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. Two of the twelve listed files are brand
new public headers in a brand new directory (`engine/anim/`), and Lesson 7.7 is
going to add a clip sampler to both of them before this page is a week old. A
third, `math/transform.hpp`, is the most-edited file in the repository. A builder
that opened a repository path would render this page's listings as they stand
TODAY rather than as they stood when the prose was written about them.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/07-06-skeletal-animation.html"

FIGURES = {
    "FIG_RIGID_FAILS": ("l76_fig1.svg", "1",
        "<strong>The difficulty is the geometry, not the mathematics.</strong> On the left, what "
        "Lesson 5.9's transform hierarchy draws perfectly well: two rigid parts hinged at a joint, "
        "which is a turret on a hull, a wheel on an axle, a gun on a mount. Nothing about it is "
        "wrong — and nothing about it can be a character, because the two capsules interpenetrate "
        "on the inside of the bend and open a wedge on the outside. On the right, what a character "
        "actually is: <em>one continuous surface</em>, whose vertices near the hinge are moved by "
        "both bones at once and blended by weight. Ask the hierarchy where the vertex at the crease "
        "belongs and it has no answer to give, because the honest answer is <em>both</em> and a "
        "hierarchy assigns whole objects to whole matrices. The surface also pulls IN slightly at "
        "the hinge in this drawing, which is not artistic licence — it is figure 6's "
        "<code>cos(θ/2)</code>, arriving four sections early and unavoidably."),

    "FIG_SPACES": ("l76_fig2.svg", "2",
        "<strong>The whole lesson, read off the labels.</strong> Lesson 2.8's naming convention "
        "says a product is legal when the inner labels agree, and here they do — but the two "
        "<code>joint</code>s are the same joint at two different <em>times</em>, which is the one "
        "piece of bookkeeping no convention can carry for you and the piece every confused "
        "explanation of skinning has dropped. The left arrow is the matrix the vocabulary calls an "
        "<em>inverse bind matrix</em>, named for how it was made; named for what it does it is "
        "<code>joint_from_model</code>, and it is baked once. The right arrow is where the joint is "
        "now, rebuilt every frame. <strong>Then read the OUTER labels</strong>, which is the part "
        "that is rarely said out loud: they are the same space, so a skinning matrix is "
        "<code>model_from_model</code>. That is simultaneously why four of them may legally be "
        "added together — you may add linear maps only when they share a domain and a codomain — "
        "and why the sum is not a rotation, because the model-to-model maps are closed under "
        "addition and the rotations are not. And at the bind pose the two factors are exact "
        "inverses, so every skinning matrix is the identity: measured at <strong>4.768e−07</strong> "
        "over a six-joint chain, which is float noise and is the cheapest complete test of a rig "
        "that exists."),

    "FIG_TWO_CHAINS": ("l76_fig3.svg", "3",
        "<strong>No matrix is inverted anywhere in this, and that is the point.</strong> Inverting "
        "a product reverses it, <code>(AB)⁻¹ = B⁻¹A⁻¹</code>, so the inverse bind chain composes "
        "exactly as happily as the forward one — it simply multiplies the parent on the other side. "
        "Both rows walk the array in the same direction, index 0 upward, and both read an entry "
        "their own loop has already finished; the arrows are the WALK, and the formula above each "
        "row is the algebra. (The first draft drew the second row's arrows backwards to show "
        "&ldquo;multiplies on the right&rdquo;, which made the picture contradict its own caption.) "
        "The only inverse in the whole build is <code>local_from_parent</code> on one node at a "
        "time — a transpose, three reciprocals and a negated offset — which is the function Lesson "
        "2.8 wrote a comment about and declined to write. The table is the check, and its control "
        "is what makes it worth having: a general 4×4 Gauss-Jordan inverse written from scratch in "
        "the harness, sharing no line of code with the engine, so that &ldquo;the two routes "
        "agree&rdquo; is a statement about two routes."),

    "FIG_NO_BIND": ("l76_fig4.svg", "4",
        "<strong>The single most common first attempt at skinning, and what it does.</strong> Two "
        "real renders of the same rig in the same pose, both showing the whole frame — because the "
        "failure IS that the mesh leaves the frame, and cropping would be a way of not showing it. "
        "On the left the palette is <code>model_from_joint · joint_from_model</code>; on the right "
        "it is the posed joint matrices alone, which is what you get by reaching for the thing "
        "everyone calls the &ldquo;bone matrix&rdquo;. It is not a correctable error. "
        "<code>model_from_joint</code> expects a point expressed in JOINT space and the mesh's "
        "coordinates are in MODEL space, so the product asks a question with no answer — every "
        "vertex is displaced by roughly wherever its joint is standing, and the harness measures the "
        "worst at <strong>4.4366 units on a tube 5.00 units long</strong>. In the demo this is one "
        "key, <kbd>N</kbd>, which is worth pressing a few times: the bug is much easier to recognise "
        "later if you have watched it happen."),

    "FIG_WEIGHTS": ("l76_fig5.svg", "5",
        "The weighting, drawn twice. On the left, a real render of the tube with each ring outlined "
        "in the blend of its two joints' colours — so the picture of the weights and the picture of "
        "the geometry are the same picture. On the right the same information as six hat functions: "
        "two influences per vertex, linear in the distance between the joints either side, which is "
        "the simplest weighting that works and also what an automatic bind in a modelling tool "
        "starts from. <strong>The two live weights sum to exactly 1 at every point</strong>, and "
        "that is a law rather than tidiness: a palette matrix is AFFINE, so its translation is "
        "blended along with its linear part, and a weight sum of <code>s</code> scales every vertex "
        "to <code>s</code> times its distance from the MODEL ORIGIN. Measured on this fixture with "
        "every weight scaled by 0.9: the gap from that prediction is <strong>5.218e−07</strong> and "
        "the worst vertex moves <strong>0.5012 units</strong> — which is a tenth of "
        "<code>√(0.35² + 5²)</code>, the distance from the origin to the furthest vertex, and "
        "therefore arithmetic rather than agreement. It presents as a character that is 10% too "
        "small and sunk into the floor, which reads as a scale bug and is a weighting bug."),

    "FIG_CANDY": ("l76_fig6.svg", "6",
        "<strong>The candy wrapper, in one isoceles triangle.</strong> Two joints twisted "
        "<code>θ</code> apart send the same vertex to two places, both at distance <code>|v|</code> "
        "from the axis and <code>θ</code> apart; a half-and-half blend takes their midpoint, which "
        "lies on the bisector of the apex — and the bisector of an isoceles triangle meets its base "
        "at a right angle. That right triangle gives <code>|v| cos(θ/2)</code> immediately. "
        "<strong>Nothing in the derivation mentions skinning, a joint or a matrix</strong>; it is a "
        "fact about averaging two points on a circle, and it is the same half-angle Lesson 7.3 "
        "found between two mirrors and Lesson 7.4 found inside a quaternion, arrived at for a third "
        "independent reason. The table is the check, and the last row is the one to look at: at a "
        "half-turn the radius is not small, it is <strong>zero</strong>, because the average of two "
        "antipodal points is the origin. The control (not shown) is the same vertex weighted 1.0 to "
        "a single joint, which comes back at 1.000000 — so the artifact belongs to the blend and "
        "not to the twist."),

    "FIG_TWIST_RENDER": ("l76_fig7.svg", "7",
        "<strong>Why a production rig has more joints than a skeleton does.</strong> Three real "
        "renders of the same 180° of twist, spread over one, two and four joints. The collapse "
        "depends on the angle between two ADJACENT joints rather than on the total, so dividing the "
        "twist divides the half-angle: the radius goes to <code>cos(θ/2n)</code>, which is 0.000, "
        "0.707 and 0.924 for the three panels. The measured curve on the right has the harness's "
        "six values sitting on it. That is what a forearm twist bone <em>is</em> — not a modelling "
        "nicety but a direct attack on a half-angle — and it is the fix shipped content actually "
        "uses, because it costs nothing at runtime and the rigger was going to be involved anyway. "
        "Note how fast it pays: the second joint is worth 0.707 of the radius and the fourth is "
        "worth only another 0.217, so the first one or two are doing nearly all the work."),

    "FIG_BEND_ELLIPSE": ("l76_fig8.svg", "8",
        "<strong>One cosine, two geometries — which is why the two artifacts look unrelated and are "
        "not.</strong> A TWIST rotates about the limb's own axis, so the entire ring lies in the "
        "plane the collapse happens in and every vertex moves inward by the same factor: the ring "
        "shrinks. A BEND rotates about an axis <em>across</em> the limb, and a rotation fixes its "
        "own axis — so the vertices lying on it do not move at all and the ring FLATTENS into an "
        "ellipse, semi-minor <code>r cos(θ/2)</code> and semi-major <code>r</code> untouched. "
        "Measured at 120°: minor axis <strong>0.500000</strong> against a prediction of 0.500000, "
        "major axis <strong>1.000000</strong>. That is why a twisted forearm loses its whole "
        "cross-section while a bent elbow reads as a crease and a flat patch. It is also why the "
        "demo measures the SMALLEST radius on a ring rather than the mean: the minimum is the minor "
        "axis in both cases, so one number reads the same law in both modes, and the mean would be "
        "a statement about the shape of an ellipse instead."),

    "FIG_LBS_VS": ("l76_fig10.svg", "10",
        "<strong>The one-line summary everybody repeats is wrong in both directions.</strong> "
        "Linear blend skinning is said to be <code>nlerp</code> with the normalise deleted. It is "
        "worse than that: both get the schedule wrong, but nlerp's error is governed by the arc "
        "between two <em>quaternions</em> — half the rotation angle — and the matrix blend's by the "
        "whole rotation angle, and Lesson 7.3 showed that schedule error grows much faster than "
        "linearly. Measured, that is about a factor of four at every arc: <strong>10.95° of pose "
        "against 2.23°</strong> at a 120° twist, and 76.63° against 8.00° at 179°. And it is better "
        "than that, at the one thing Lesson 7.5 spent half a lesson on: a rotation matrix is "
        "<em>unique</em>, so there is no double cover, no long way round, and no <code>nearest</code> "
        "to forget — the 359° spin between two keyframes a degree apart cannot be written here at "
        "all. Checked over 20,000 random pairs: the blended point is never further from the "
        "endpoints than they are from each other, excess <strong>1.192e−07</strong>."),

    "FIG_COST": ("l76_fig9.svg", "9",
        "<strong>The asymmetry the whole design rests on, and the measurement that nearly did not "
        "mean anything.</strong> On the left, where a frame's skinning time goes: the per-joint work "
        "is <strong>2.34%</strong> of the per-vertex work, so everything that can be hoisted to a "
        "joint should be and what remains per vertex is four multiply-adds. On the right, the same "
        "split in bytes, which is the GPU-skinning argument in one number — <strong>28.4×</strong> "
        "on this fixture, and it grows with the mesh, because the palette is a property of the rig "
        "and the deformed vertices are a property of the model. Below, the row that had to be "
        "discovered. This is the first harness in Module 7 that links a library, and "
        "<code>cmake -S . -B build</code> leaves <code>CMAKE_BUILD_TYPE</code> EMPTY — so "
        "<code>libengine.a</code> carried no <code>-O</code> flag and the first numbers were "
        "<strong>18.1× too slow</strong>. The <code>-O2</code> on a harness's own command line "
        "reaches the translation units it is on and no further, and an unoptimised static library "
        "links exactly as quietly as an optimised one."),
}

LISTING_META = {
    "engine/include/engine/anim/skeleton.hpp": ("new", "new"),
    "engine/include/engine/anim/skin.hpp":     ("new", "new"),
    "engine/src/anim/skeleton.cpp":            ("new", "new"),
    "engine/src/anim/skin.cpp":                ("new", "new"),
    "engine/include/engine/math/transform.hpp": ("modified", "modified"),
    "engine/include/engine/engine.hpp":        ("modified", "modified"),
    "engine/CMakeLists.txt":                   ("modified", "modified"),
    "demos/rig/main.cpp":                      ("new", "new"),
    "demos/CMakeLists.txt":                    ("modified", "modified"),
    "scratch/verify_76.cpp":                   ("new", "new"),
    "scratch/build_verify_76.sh":              ("new", "new"),
    "scratch/golden_76.cpp":                   ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_76.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l76_" + path.replace("/", "_")


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
<title>7.6 — Skeletal Animation: The Skinning Math · Build a Professional 3D Game Engine</title>
<meta name="description" content="Skinning has more vocabulary than mathematics: bind pose, inverse bind matrix, matrix palette, skin cluster, linear blend skinning - six terms for four multiply-adds and one matrix product, and the terms are what make it sound hard. This lesson throws the vocabulary away and derives the one matrix that matters from the naming convention Lesson 2.8 established. A skinning matrix is model_from_joint times joint_from_model; the inner labels cancel because they name the same joint at two different times, and the outer labels are the same space - so a skinning matrix is model_from_model, which is both why four of them may be added together and why the sum is not a rotation. Building the inverse bind matrices needs no matrix inversion at all, because inverting a product reverses it and the inverse chain composes with the same flat loop as the forward one, which finally writes the local_from_parent that Lesson 2.8 deliberately left out. At the bind pose every skinning matrix is exactly the identity, which is the cheapest complete test of a rig that exists and needs no reference image. Then the artifact: linear blend skinning collapses a twisted limb to cos(theta/2) of its radius - exactly zero at a half turn, the same half-angle Lesson 7.3 found between two mirrors - and spreading the twist over n joints opens it back to cos(theta/2n), which is what a forearm twist bone is. A bend obeys the same cosine on the minor axis of an ellipse. And the one-line summary everybody repeats, that linear blend skinning is nlerp without the normalise, turns out to be wrong in both directions: worse, because its schedule error uses the whole angle rather than the half angle, and better, because a rotation matrix is unique so the double cover cannot bite.">

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
    <a class="prev-l" href="07-05-slerp.html">
      <span class="dir">← Previous</span>
      <span class="ttl">7.5 — Slerp, and the Storage Swap</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="07-07-sampling-blending.html">
      <span class="dir">Next →</span>
      <span class="ttl">7.7 — Sampling and Blending Animations</span>
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
        with open(f"scratch/l76_body_{name}.html") as fh:
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
