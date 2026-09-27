#!/usr/bin/env python3
"""Assemble docs/lessons/08-02-forces-and-bodies.html.

Same pipeline as build_71.py through build_81.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l82_body_{a,b,c,d,e}.html, figures from scratch/figs_82.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE, for the second lesson running. Three of
the eight listings — `engine.hpp` and both CMakeLists — are the most frequently
edited files in the repository, and every remaining lesson in Modules 8 and 9
will touch at least one of them. `rigid_body.hpp` is now in that set too: 8.3
adds orientation, angular velocity and an inverse inertia tensor to the very
struct this page prints in full. A builder that opened a repository path would
render this page's listings as they stand TODAY rather than as they stood when
the prose was written about them.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson. Here
the pins are generated from LISTING_META by `_pin`, so a path added to
LISTING_META without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order — which is NOT the order figs_82.py generates
them in. The mapping is deliberate and is the reason the placeholders have names
rather than numbers: §4's heavy-body figure was written third and reads seventh
in the source file, and renaming the SVG to match would have broken figs_82.py's
own numbering the next time a figure moved.
"""
import os
import re

OUT = "docs/lessons/08-02-forces-and-bodies.html"

FIGURES = {
    "FIG_ACCUMULATOR": ("l82_fig1.svg", "1",
        "<strong>Four systems, four calls, and nothing between them.</strong> Gravity, a thruster, a "
        "wind volume and a crude drag model each add to the same <code>vec3</code>, in whatever "
        "order the frame runs them; the dashed arrows are those four contributions laid tip to "
        "tail and the solid green one is the sum. <strong>Both axes are at one scale</strong>, "
        "which is not a detail — the first version of this figure used 1.30 px/N across and 2.60 "
        "up, and the sum arrow pointed somewhere the body was never going. One divide then turns "
        "that vector into an acceleration, once, at the end. Note the y component of the result: "
        "<strong>−8.935</strong>, not −9.81, because the wind and the drag both had a small upward "
        "share and this body is falling slightly slower than gravity alone would have it. On the "
        "right, the same four systems written with an assignment instead of a <code>+=</code> — "
        "and gravity, wind and drag are simply gone, with no error and no warning."),

    "FIG_INVMASS": ("l82_fig2.svg", "2",
        "<strong>Why the reciprocal is the one that gets stored, and the question it does not "
        "answer.</strong> Above: mass runs from zero to infinity and its reciprocal runs the other "
        "way, which puts <em>immovable</em> — the floor, every wall, every rock, which is to say "
        "most of the bodies in a real level — on an exact representable zero, and puts the "
        "nonsense case where nothing divides by it. Storing the mass instead needs "
        "<code>HUGE_VALF</code>, and <code>inf − inf</code> is a NaN that will propagate out "
        "through the position and onto the screen. Below: <code>inv_mass</code> answers “can a "
        "force move it” and <code>body_kind</code> answers “does it move at all”, and they are "
        "different questions — a lift is immovable by forces <em>and</em> moves. Conflating them "
        "gives the two classic bugs, one per direction: a lift that sags under load, or a lift "
        "that leaves its passengers behind."),

    "FIG_HEAVY": ("l82_fig6.svg", "3",
        "<strong>“Just make the floor very heavy”, one second later.</strong> A dynamic body of one "
        "million kilograms and a ten-gram pebble, dropped together: the two arrows are the same "
        "length to the last decimal place, <strong>4.987 m</strong>, because gravity is the one "
        "force proportional to the mass it acts on and the two cancel. A hundred million times the "
        "mass buys exactly nothing. Mass resists being <em>pushed</em>, and the floor's problem was "
        "never that somebody was pushing it. The third lane is what actually works: "
        "<code>inv_mass = 0</code>, ten seconds of gravity plus a ten-kilonewton shove, and a "
        "position of exactly zero — not small, zero, because <code>x · 0</code> has nothing left to "
        "round."),

    "FIG_ROUTES": ("l82_fig3.svg", "4",
        "<strong>Two arrangements of the same physics, and the sweep that chose between them.</strong> "
        "Weight <em>is</em> a force, so the honest arrangement puts <code>m·g</code> into the "
        "accumulator with everything else — and needs a <strong>float divide per body per step</strong> "
        "to recover a mass the body deliberately does not store, which §11 measures at 15% of the "
        "whole update, more than 8.1's entire integrator step. The other adds gravity after the "
        "division, as the acceleration it already is. Both are correct; one is cheaper and one is "
        "exact. Below: seven hand-picked masses all recovered <code>g</code> to the bit, which "
        "looked like a proof and was nothing of the kind — a sweep of a million finds "
        "<strong>16%</strong> that do not, and the first of them is <strong>1.000145 kg</strong>."),

    "FIG_RESIST": ("l82_fig4.svg", "5",
        "<strong>Two knobs that look the same and are not.</strong> Speed against time for three "
        "masses a thousandfold apart. On the left, <code>rigid_body::damping</code> — "
        "<code>v ×= exp(−kh)</code>, applied to the velocity with no mass in it — and the three "
        "curves are <em>one</em> curve, which is why each body draws one dash in three and the "
        "line cycles amber, green, blue. Terminal speed <code>g/k</code>, identical for a paper cup "
        "and a piano. On the right, a drag <em>force</em> <code>F = −bv</code> through the "
        "accumulator, which the step divides by the mass: terminal speed <code>mg/b</code>, and the "
        "heaviest body is still climbing off the top of its panel. Note also the <code>g/k</code> "
        "gap: the simulation settles at 19.5383 where the continuous formula says 19.6200, because "
        "the step applies gravity and <em>then</em> damps, so its fixed point is "
        "<code>g·h·e^(−kh)/(1 − e^(−kh))</code> = <strong>19.5384</strong>."),

    "FIG_JUMP": ("l82_fig5.svg", "6",
        "<strong>The same jump, four monitors.</strong> A 70 kg character aiming at 1.00 m, tuned at "
        "60 Hz, then run at four common refresh rates — both columns on the same logarithmic axis, "
        "because that is the only way to get them into one picture. As an <strong>impulse</strong> "
        "the four peaks span 6.16%, and even that residue is 8.1's rather than this lesson's: "
        "semi-implicit Euler's peak is <code>v₀²/2g − v₀h/2</code>, predicted 0.926176 and measured "
        "0.927527 at 30 Hz. As a <strong>force held for one step</strong> the same jump spans "
        "<strong>23×</strong> — 3.85 m at 30 Hz, 16.7 cm at 144 — because <code>Δv = F·h/m</code> "
        "carries an <code>h</code> and the height goes as its square. This is why every contact "
        "response from Lesson 8.9 onward is an impulse."),

    "FIG_SCALE": ("l82_fig7.svg", "7",
        "<strong>The square root under every miniature.</strong> Multiply every length in a world by "
        "<code>s</code> and leave gravity alone, and every duration in it is multiplied by "
        "<code>√s</code> — because <code>t = √(2d/g)</code> and the <code>d</code> is the only "
        "thing that moved. Five measured fall times sit exactly on the predicted curve. Read it in "
        "the direction a developer hits it: a world modelled <em>twice as large as it depicts</em> "
        "takes <strong>1.4142× too long</strong> for everything, which is the complaint “the "
        "jumping feels floaty” with a number attached, and a world modelled half as large is the "
        "same factor too fast, which reads as “toy-like”. Film pays for the same law in the other "
        "currency: a 1/8 scale model runs <strong>2.8284×</strong> too fast, so the camera is "
        "overcranked to 67.9 fps and played back at 24."),

    "FIG_FRAMES": ("l82_fig8.svg", "8",
        "<strong>The same “down”, carried out through four parents.</strong> Each panel shows a "
        "dashed grey reference pointing straight down and, offset beside it, where the same local "
        "gravity actually lands in world space once the parent's linear part has acted on it — the "
        "<em>linear</em> part, because an acceleration is a direction and Lesson 2.7's <code>w</code> "
        "decides that. A rotation is harmless: gain exactly 1, square exactly 0. A uniform scale of "
        "2 makes the body fall at <strong>2 g</strong>. <strong>The third panel is the trap</strong> "
        "— a thoroughly broken non-uniform frame that reports gain 1.0000 and tilt 0.000°, because "
        "gravity points along y and the scale is on x, so this particular vector never touches the "
        "broken axis. The fourth is the same frame with the body merely <em>turned</em> 45°: "
        "<strong>1.5811 g at 63.435° off vertical</strong>, and <code>out_of_square</code> of 0.6000, "
        "which is a cosine and means 53.13° of shear."),

    "FIG_FALL": ("l82_fig9.svg", "9",
        "<strong>One second of falling, in two spaces.</strong> The integrator ran once, correctly, "
        "with <code>g = 9.81</code>, and produced <strong>−4.986749 units</strong> — which is the "
        "right answer to the question it was asked. Carried out through a parent scaled by 2 it is "
        "<strong>−9.973498 m</strong> in the world where the player is, a ratio of exactly 2.0000, "
        "and nothing anywhere reported an error because every line of arithmetic was right. Below: "
        "the failure mode a matrix cannot see at all. A body with <em>no forces on it</em> must "
        "travel in a straight line; integrated in a frame spinning at 2 rad/s it ends up "
        "<strong>3.7963 m</strong> away after one second. A rotating frame is non-inertial, and "
        "<code>inspect_frame</code> calls it inertial at every instant and is right at every instant."),

    "FIG_DEMO": ("l82_fig10.svg", "10",
        "<strong>Two of the demo's three modes, and the budget.</strong> Three bodies of 0.1, 1 and "
        "10 kg launched on identical trajectories. Under <em>damping</em> the three trails are one "
        "trail — each body draws one dash in three, so a single line cycling amber, green, blue is "
        "three trajectories in exact agreement rather than a bug that lost two of them. Under a "
        "<em>drag force</em> they separate completely: the light one barely clears the launch point, "
        "the heavy one carries nearly to the arc it had in vacuum. The right panel of each shot is "
        "§9 as something you can watch — four bodies dropped from the <strong>same world point</strong> "
        "in four different parent frames, of which only the green one is falling. The table is the "
        "budget, and its two rows worth reading together are the optimisations: removing the divide "
        "for weight is worth <strong>15%</strong> and repeats to the third decimal across three "
        "runs, while the far more famous one — avoiding a <code>sqrt</code> per body — measures "
        "1.036, 0.968, 0.969, which is a tie and once a loss."),
}

LISTING_META = {
    "engine/include/engine/phys/rigid_body.hpp": ("new", "new"),
    "engine/src/phys/rigid_body.cpp":            ("new", "new"),
    "engine/include/engine/engine.hpp":          ("modified", "modified"),
    "engine/CMakeLists.txt":                     ("modified", "modified"),
    "demos/CMakeLists.txt":                      ("modified", "modified"),
    "demos/bodies/main.cpp":                     ("new", "new"),
    "scratch/verify_82.cpp":                     ("new", "new"),
    "scratch/build_verify_82.sh":                ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_82.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l82_" + path.replace("/", "_")


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
<title>8.2 — Forces, Gravity, and Linear Rigid Bodies · Build a Professional 3D Game Engine</title>
<meta name="description" content="Lesson 8.1 taught this engine to advance a state through time and left it unable to tell a crate from a pebble: you hand integrate an acceleration and it believes you, which is enough to drop a cube and enough for nothing else. An acceleration cannot be added up, so four systems that each want to push the same body overwrite one another; and it does not know what anything weighs, so a thruster shoves a truck exactly as hard as a pebble. The fix is F = ma, and the interesting part is everything the engine has to decide to make it usable: an accumulator rather than a setter, because forces add and Newton's second law is linear in F; the reciprocal of the mass rather than the mass, because the common case in a real level is immovable and that is a clean exact zero where infinity is a NaN waiting to happen; and an impulse as a separate entry point, because a jump written as a one-step force is 23 times higher at 30 Hz than at 144 while the same jump written as an impulse varies by 6 percent. Then two questions that look like pedantry until they cost a week. Where one unit is one metre stops being a convention: 9.81 is in metres per second squared, a world modelled at scale s runs every duration in it by the square root of s, and that single square root is why miniatures look like miniatures and why the jumping feels floaty is a measurement rather than an opinion. And where a scaled parent quietly stops meaning it: a body integrated under a parent scaled by 2 falls at 2 g, a body merely turned under a non-uniformly scaled one falls at 1.5811 g and 63.435 degrees off vertical, and a spinning parent passes every test a matrix can answer while putting a force-free body 3.7963 m from the straight line it was obliged to travel. Also: a sweep of a million masses finds 16 percent of them fail a round trip that seven hand-picked ones all passed, and a budget in which the honest arrangement of gravity costs a float divide worth 15 percent of the whole update.">

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
    <a class="prev-l" href="08-01-integrators.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.1 — Integrators: Why One Explodes</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-03-angular-dynamics.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.3 — Angular Dynamics: Torque and the Inertia Tensor</span>
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
        with open(f"scratch/l82_body_{name}.html") as fh:
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
