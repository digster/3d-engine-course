#!/usr/bin/env python3
"""Assemble docs/lessons/08-13-character-controller.html.

Same pipeline as build_71.py through build_812.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l813_body_{a..f}.html, figures from scratch/figs_813.py, and
every code listing PINNED at writing time — see LISTING_SOURCE. §13's snippets
were cut from the sources by scratch/gen_l813_body_e.py, once, into the static
fragment l813_body_e.html, and §18's project tree by scratch/gen_l813_tree.py
into l813_tree.html, which l813_body_f.html already contains; this builder reads
the fragments, never the scripts.

WHY THE PINS MATTER HERE. Eleven listings, four of them new files that Module
9 will certainly edit (a region query in `sweep` is this lesson's exercise 5,
and the scene format will serialise `character_config`). Without the pins this
page's prose, which quotes `cast`, `sweep`, `sideways` and `move_character`
line for line, would end up describing files that moved.

EVERY NUMBER IN THE PROSE IS A MEASUREMENT FROM THIS MACHINE, and
scratch/verify_813.log is the canonical run for all of them, with two named
exceptions (§7's 175 mm tangent-plane lurch and §11's boulder carrying the
character 2.94 m), which were observed on first drafts of the engine and are
told as observations. Two consecutive runs were diffed: everything except §K's
timings is byte-identical.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES. Here the pins
are generated from LISTING_META by `_pin`, so a path added to LISTING_META
without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/08-13-character-controller.html"

FIGURES = {
    "FIG_NEWTON": ("l813_fig1.svg", "1",
        "<strong>Five things a designer asks of a character, and a rigid capsule's answers.</strong> Left: "
        "a capsule pushed into a wall in mid-air. Each step the contact cancels the stick's "
        "<code>v_push</code>, friction may then hold <code>μ·m·v_push</code> against gravity's "
        "<code>m·g·h</code>, and it hangs above <code>g·h/μ</code>, 27&nbsp;cm/s. Middle: whether it gets "
        "up a ledge depends on how hard it is driven. Right: the ledger, rigid capsule in red, controller "
        "in green. Every red cell is 8.10's solver being right."),

    "FIG_PIPELINE": ("l813_fig2.svg", "2",
        "<strong>One query, five decisions, one owner per body.</strong> Left: "
        "<code>move_character</code>'s five stages, each labelled with the section that measures it, "
        "all built on a sweep: one cast per candidate obstacle, smallest <code>t</code> kept. Right: the "
        "fixed step (steer the proxy, run the physics step, move the character) and who owns which "
        "body. A designer's 1.20&nbsp;m jump rises 1.1599&nbsp;m, the discrete sum of 8.1's steps."),

    "FIG_NEWTONSTEP": ("l813_fig3.svg", "3",
        "<strong>Conservative advancement, as geometry and as Newton's method.</strong> Left: a ball "
        "moving toward another at <code>(3,&nbsp;0.6)</code>, drawn at each iterate with the plane GJK's "
        "direction separates the pair by, dashed; each step goes to where the mover would first reach "
        "that plane. Right: the distance <code>f(t)</code> is convex, so every tangent lies below it and "
        "each Newton step lands short of the root at <code>t&nbsp;=&nbsp;0.55</code>. The errors square "
        "each time, and their ratio tends to <code>f''/2|f'|&nbsp;=&nbsp;0.9</code>."),

    "FIG_UNION": ("l813_fig4.svg", "4",
        "<strong>One cast per obstacle, divide by the closing speed, and keep a skin.</strong> Left: one "
        "Newton step on the nearer of two obstacles flies through the wall behind it, 11,194 times in "
        "20,000; one cast each, never. Middle: steps to reach a face at a grazing angle. The cast takes 2 "
        "at every angle; sphere tracing, which divides by <code>|d|</code>, takes 237 at 1°. Right: with no "
        "skin a cast often cannot finish, and a skin thinner than GJK's margin is worse than none. "
        "Stepped from GJK's upper bound, 377 casts in 6,049 finish inside the skin; from the lower bound, "
        "none."),

    "FIG_CORNER": ("l813_fig5.svg", "5",
        "<strong>Clip the original motion, not what was left.</strong> One move into a 120° corner, the "
        "stick 12° off the bisector. It slides along wall A first, toward the corner (dashed blue). The "
        "remainder, clipped against the last plane or against every plane, is sent back out along wall "
        "B, and the character walks back and forth at 572–575&nbsp;mm/s. The original motion, clipped "
        "against both planes, has only the vertical crease left: still."),

    "FIG_SLOPE": ("l813_fig6.svg", "6",
        "<strong>Slopes are a normal's height, not a friction coefficient.</strong> Left: a step laid "
        "along a 30° ramp keeps the stick's horizontal part exactly (green); the plain projection keeps "
        "<code>cos²α</code> of it (red). Middle: a character standing still creeps downhill at "
        "<code>g·h·sin&nbsp;α</code> if gravity's drop is slid along the ground, to five figures. Right: "
        "flattening a steep surface's normal matters in the air. On the ground, laying the move along "
        "the floor has already refused the climb."),

    "FIG_EDGE": ("l813_fig7.svg", "7",
        "<strong>The round bottom on an edge: one right triangle, three numbers.</strong> Left: a curb "
        "meets the capsule's skin-inflated bottom sphere with a normal from the edge to the centre, and "
        "is climbed with no step-up while that is walkable, up to "
        "<code>(r&nbsp;+&nbsp;skin)(1&nbsp;−&nbsp;cos&nbsp;45°)&nbsp;=&nbsp;90.8&nbsp;mm</code>. Middle: it "
        "stands past an edge until <code>(r&nbsp;+&nbsp;skin)&nbsp;sin&nbsp;45°&nbsp;=&nbsp;219.2&nbsp;mm</code>. "
        "Right: the tilt the cast reports, on the arcsine to 0.19°. That residue is GJK's, and §6 shows "
        "it scales with the obstacle's size, not the tolerance."),

    "FIG_SNAP": ("l813_fig8.svg", "8",
        "<strong>Three ways the ground drops away.</strong> Left: moved flat, a character leaves a 30° "
        "slope above <code>(g·h²&nbsp;+&nbsp;2·skin)/(h·tan&nbsp;α)</code>, 2.36&nbsp;m/s; laid along it, "
        "never. Middle: the round bottom rolls over a crest below 6.99&nbsp;m/s and flies like a "
        "projectile above it, unless it snaps. Right: resting on a stair's edge, the snap rolls off "
        "sideways by exactly what clears the edge, then casts down."),

    "FIG_STEP": ("l813_fig9.svg", "9",
        "<strong>A step-up must go far enough across to land.</strong> At the riser the capsule's centre "
        "is up to <code>r&nbsp;+&nbsp;skin</code> short of the edge, and it lands walkable only within "
        "<code>(r&nbsp;+&nbsp;skin)&nbsp;sin&nbsp;45°</code> of it. So the across move is at least "
        "90.8&nbsp;mm, bisected at 46, 85 and 91&nbsp;mm for three risers against the prediction. The "
        "step and the round bottom add: with <code>step_height&nbsp;=&nbsp;0.30</code> the tallest ledge "
        "climbed is 0.391&nbsp;m."),

    "FIG_TUNNEL": ("l813_fig10.svg", "10",
        "<strong>A rigid capsule tunnels on schedule; the controller never does.</strong> Left: sampled "
        "at step boundaries, the first overlap with a 10&nbsp;cm wall is <code>d</code> deep, uniform on "
        "<code>[0,&nbsp;v·h]</code>, and EPA pushes it through once its centre passes the wall's middle. "
        "Right: 200 phases per speed on the closed form "
        "<code>1&nbsp;−&nbsp;(w/2&nbsp;+&nbsp;r)/(v·h)</code>, zero below 21&nbsp;m/s. The controller, dashed "
        "into a 1&nbsp;cm wall at up to 10,000&nbsp;m/s, stops a skin short every time."),

    "FIG_CARRY": ("l813_fig11.svg", "11",
        "<strong>Carry a rider by what the platform did, not by the velocity under it.</strong> Left: at "
        "2&nbsp;rad/s, carried by transform the rider stays on its circle; carried by the point velocity it "
        "moves along a tangent every step and spirals outward, drawn here for three revolutions from the "
        "recurrence the harness measures. Right: one revolution. By velocity the radius grows by "
        "<code>(1&nbsp;+&nbsp;(ωh)²)<sup>N/2</sup>&nbsp;≈&nbsp;e<sup>πωh</sup></code> per revolution, "
        "measured to four figures; by transform, not at all."),

    "FIG_PUSH": ("l813_fig12.svg", "12",
        "<strong>Steer the proxy; never teleport it.</strong> Left: steered, the proxy pushes a "
        "20&nbsp;kg crate at 2&nbsp;m/s with 5&nbsp;mm of overlap, the solver's slop. Teleported, only the "
        "position pass moves the crate, and the character sinks "
        "<code>v·h/β&nbsp;+&nbsp;slop&nbsp;−&nbsp;v·h&nbsp;=&nbsp;138&nbsp;mm</code> into it. Right: the "
        "crate's real velocity, apparent speed, overlap and coast under four proxy modes. Teleported, a "
        "sleeping crate is never woken and is walked straight through."),
}

LISTING_META = {
    "engine/include/engine/phys/cast.hpp":       ("new", "new"),
    "engine/src/phys/cast.cpp":                  ("new", "new"),
    "engine/include/engine/phys/character.hpp":  ("new", "new"),
    "engine/src/phys/character.cpp":             ("new", "new"),
    "engine/include/engine/phys/convex.hpp":     ("modified", "modified"),
    "engine/include/engine/engine.hpp":          ("modified", "modified"),
    "engine/CMakeLists.txt":                     ("modified", "modified"),
    "demos/character/main.cpp":                  ("new", "new"),
    "demos/CMakeLists.txt":                      ("modified", "modified"),
    "scratch/verify_813.cpp":                    ("new", "new"),
    "scratch/build_verify_813.sh":               ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_813.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l813_" + path.replace("/", "_")


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


DESCRIPTION = (
    "Twelve lessons built a simulation that is correct, and the player's character is the one body for "
    "which that is the problem. A rigid capsule on 8.10's solver hangs on a wall mid-jump above a stick "
    "speed of 27 cm/s, slides 2.91 m after the stick is released, climbs a 20 cm ledge only at 8 m/s, "
    "and passes through a 10 cm wall half the time at 42 m/s. So the character leaves the simulation: a "
    "capsule with no mass that asks one question - how far can I move before I touch something? - "
    "answered by a shape cast. A cast is Newton's method on a convex distance, and it is safe because "
    "each step stops at a separating plane, which is why it must step from GJK's certified lower bound: "
    "stepped from GJK's distance, 377 of 6,049 casts finished inside the skin. The rest is policy with a "
    "number: clip the original motion against every surface or walk back and forth in a corner at 58 cm/s; "
    "lay a step along walkable ground; a round bottom climbs a 90.8 mm curb for free and stands 219 mm "
    "past an edge; a step-up must carry the capsule up to 90.8 mm across to land, and climbs 0.39 m when "
    "told 0.30; carrying a rider by velocity spirals it off a turntable at e^(pi w h) a revolution. A "
    "kinematic proxy, steered and never teleported, lets the character push a crate at its own speed; "
    "teleported, it sinks 138 mm into the crate, or walks through it once it is asleep. The controller "
    "tunnels through nothing up to 10,000 m/s. Ships phys/cast.hpp and phys/character.hpp, a three-scene "
    "demo, and eleven measured sections with 90 checks. Module 8 is complete.")

HEAD = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>8.13 — A Character Controller · Build a Professional 3D Game Engine</title>
<meta name="description" content="@@DESCRIPTION@@">

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
    <a class="prev-l" href="08-12-ragdolls.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.12 — Ragdolls: Joints on a Skeleton</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="../index.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.14 — Scene Queries and a Static Mesh Collider (not yet written)</span>
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
        with open(f"scratch/l813_body_{name}.html") as fh:
            parts.append(fh.read())

    page = HEAD.replace("@@DESCRIPTION@@", DESCRIPTION) + "\n".join(parts) + TAIL

    for key in FIGURES:
        page = page.replace(f"@@{key}@@", figure(key))

    page = re.sub(r"@@LISTING:([^@]+)@@", lambda m: listing(m.group(1)), page)

    left = re.findall(r"@@[A-Z0-9_:./-]+@@", page)
    if left:
        raise SystemExit(f"unsubstituted placeholders: {sorted(set(left))}")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as fh:
        fh.write(page)
    # BYTES, not characters — build_74.py's note applies.
    print(f"wrote {OUT}  ({len(page.encode('utf-8')):,} bytes, "
          f"{page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
