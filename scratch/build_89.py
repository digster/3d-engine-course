#!/usr/bin/env python3
"""Assemble docs/lessons/08-09-impulse-response.html.

Same pipeline as build_71.py through build_88.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l89_body_{a..f}.html, figures from scratch/figs_89.py, and
every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. `solver.hpp` and `solver.cpp` are the
files 8.10 exists to extend: the sequential-impulse solver iterates
`solve_contacts`, adds a position correction that will widen `contact_constraint`
with a bias term, and turns `solver_config::warm_start` on by default. Every one
of those edits would rewrite this page's listings under prose that describes what
they said today — and §11's claim that the normal solve is "three lines and a
max" is a claim about a specific function.

THREE LISTINGS ARE FILES EARLIER PAGES ALSO PRINTED, and they disagree with those
pages on purpose: `engine.hpp` and both CMakeLists are each longer by one entry
than 8.8's copies. Each page is an archive of its own era and the disagreement is
the edit being recorded.

NOTHING IS UNLISTED THIS LESSON. 8.8 had to carry a callout about three files
whose doc comments changed by one word; 8.9 touches exactly the eight files in
LISTING_META and no others, which is worth stating because it is the first time
in Module 8 that has been true.

EVERY NUMBER IN THE PROSE IS A MEASUREMENT FROM THIS MACHINE, and
scratch/verify_89.log is the canonical run for all of them. Each <pre> block is
internally from a single run; none of them mixes. Re-running the harness gives
the same conclusions and different timings in §13 — which is why §13 is the only
section whose numbers are timings at all, and why every timing there is a MINIMUM
over sixty repetitions with five warm-ups discarded. The first repetition after
process start measures 173 ns where every later one measures 139.5; without the
warm-up the page's numbers would depend on which sections the reader asked for.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson. Here
the pins are generated from LISTING_META by `_pin`, so a path added to
LISTING_META without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/08-09-impulse-response.html"

FIGURES = {
    "FIG_PENALTY": ("l89_fig1.svg", "1",
        "<strong>The penalty spring is not unstable. It is TUNED, and the tuning belongs to the "
        "scene rather than to the material.</strong> Left: the stiffness a spring needs is "
        "<code>m&middot;g/x</code> for whatever sink you will tolerate, and the rate that stiffness "
        "oscillates at is <code>&radic;(g/x)</code> &mdash; which <em>does not contain the "
        "mass</em>. Feed that through 8.1's stability limit <code>h&middot;&omega;&nbsp;&lt;&nbsp;2</code> "
        "and the penetration you will accept decides the frame rate of the entire simulation: 1 mm "
        "needs at least <strong>49.5&nbsp;Hz</strong>, and at 60&nbsp;Hz the finest a stable spring "
        "can manage is <strong>0.681&nbsp;mm</strong>. The amber arrow is the number that really "
        "bites &mdash; a millimetre under a 1.96&nbsp;m/s <em>impact</em> rather than under a "
        "resting weight, which needs <strong>990&nbsp;Hz</strong>, because the dynamic penetration "
        "is <code>v&radic;(m/k)</code> and has nothing to do with the weight. Right: the best "
        "result from <strong>164 tuned settings</strong> &mdash; four stiffnesses by forty-one "
        "dampings, three seconds each. Read it as a trade: a crate that settles costs 108&nbsp;mm "
        "of penetration, one that stays shallow bounces 367&nbsp;mm forever, and there is no "
        "setting that gives both. The last row is the impulse solver, which gives both and has no "
        "knob on it."),

    "FIG_CONTACT": ("l89_fig2.svg", "2",
        "<strong>Everything a contact wants is a statement about one vector, and each statement "
        "needs one number.</strong> Left: the geometry. The contact point <em>p</em> is one of "
        "8.7's four; <code>r_b</code> is the lever arm from the body's centre of mass to it; "
        "<strong>n</strong> is the manifold normal, pointing from <em>a</em> toward <em>b</em> as "
        "<code>collide.hpp</code> has since 8.4; and <strong>t&#8321;</strong>, "
        "<strong>t&#8322;</strong> span the contact plane. The velocity that matters is not either "
        "body's &mdash; it is <code>u</code>, the velocity of <em>b</em>'s material point relative "
        "to <em>a</em>'s, which is <code>v&nbsp;+&nbsp;&omega;&nbsp;&times;&nbsp;r</code> for each "
        "and is <em>negative</em> along <strong>n</strong> when the two are approaching. Right: the "
        "three demands. Non-penetration and restitution are the same equation with different "
        "right-hand sides, which is why <em>e</em> appears nowhere in the solver except in a "
        "target; friction is two more of the same equation with a clip whose radius is the "
        "<em>answer</em> to the first. That last dependency is why the two cannot be solved "
        "independently, and &sect;10 measures what happens when they are."),

    "FIG_MASS": ("l89_fig3.svg", "3",
        "<strong>A contact does not feel the body's mass; it feels how hard the body is to move "
        "along the normal, at that point.</strong> The same 10&nbsp;kg plank, pushed in two places. "
        "Through the centre of mass the contact sees the full <strong>10.000&nbsp;kg</strong>; "
        "1.9&nbsp;m out, part of the impulse turns the plank instead of lifting it and the contact "
        "sees <strong>2.702&nbsp;kg</strong> &mdash; <strong>3.70&times; lighter</strong>. That "
        "difference is the entire content of the two angular terms in <code>k</code>, and it "
        "vanishes exactly when <code>r&nbsp;&times;&nbsp;n&nbsp;=&nbsp;0</code>: a contact straight "
        "through both centres has <code>k&nbsp;=&nbsp;1/m_a&nbsp;+&nbsp;1/m_b</code>, whose "
        "reciprocal is the <em>reduced mass</em> of elementary mechanics, measured at "
        "<strong>2.100000&nbsp;kg</strong> for 3&nbsp;kg against 7. Written as "
        "<code>w&middot;(I&#8315;&sup1;w)</code> the angular terms are manifestly non-negative, "
        "which <em>proves</em> the division is safe &mdash; and over 200,000 random contacts the "
        "gap never once went negative, a check the agreement between the two algebraic forms could "
        "not have made, because both forms would have carried the same sign error."),

    "FIG_RESTITUTION": ("l89_fig4.svg", "4",
        "<strong>A ball that bounces forever, and the closed form that predicts exactly how high.</strong> "
        "Left: ten bounce heights on a log axis, with <code>e&sup2;&#8319;&middot;h&#8320;</code> "
        "drawn as the dashed line &mdash; arithmetic, not a simulation. With "
        "<code>restitution_bias</code> set the ball tracks it down to a millimetre. Without it the "
        "ball climbs <em>above</em> its own prediction and flattens out, reaching "
        "<strong>3.918&times;</strong> the closed form by the tenth bounce. The cause is an "
        "ordering artifact rather than a physics one: a semi-implicit step applies gravity before "
        "the solver looks, so the approach speed is already too large by "
        "<code>g&middot;h</code>&nbsp;=&nbsp;16.35&nbsp;cm/s, and restitution faithfully returns "
        "<em>e</em> times a speed that includes it. The error is additive while the bounce is "
        "multiplicative, so it does not shrink &mdash; the bounce has a FIXED POINT at "
        "<code>e&middot;g&middot;h/(1&nbsp;&minus;&nbsp;e)</code>, and the right-hand table "
        "measures it at <strong>1.000&times;</strong> the prediction at both e&nbsp;=&nbsp;0.5 and "
        "e&nbsp;=&nbsp;0.8. At e&nbsp;=&nbsp;0.95 that is a half-metre hop that no restitution "
        "threshold will hide."),

    "FIG_CONE": ("l89_fig5.svg", "5",
        "<strong>Coulomb's condition is a disc, and the square that circumscribes it is a physical "
        "difference with no physical cause.</strong> Left: the admissible set for the tangential "
        "impulse, in units of <code>&mu;&middot;j_n</code>. Clipping the two components "
        "independently &mdash; one <code>clamp</code> per axis instead of a <code>sqrt</code>, "
        "which is why it is common &mdash; permits up to <strong>&radic;2</strong> times as much "
        "friction along the diagonals. And the axes came out of <code>tangent_basis</code>, which "
        "chose them from the normal's smallest component, so under the box model <em>a floor "
        "decides which way is slippery</em>. Right: the measurement. A 5&nbsp;kg crate pushed at "
        "4&nbsp;m/s in 361 directions, &mu;&nbsp;=&nbsp;0.50; the radius is how far it slid. The "
        "cone is a circle to <strong>0.00%</strong> and the box is a four-lobed figure spreading "
        "<strong>42.68%</strong>, whose extreme ratio comes out <strong>0.70085</strong> against a "
        "predicted <code>1/&radic;2&nbsp;=&nbsp;0.70711</code>. Sliding at an angle to a fixed "
        "basis is the same experiment as sliding along a fixed direction with the basis rotated, so "
        "this sweep <em>is</em> the measurement of basis dependence."),

    "FIG_SLOPE": ("l89_fig6.svg", "6",
        "<strong>A measurement that disagreed with the formula by four degrees, and the formula was "
        "right.</strong> The angle at which a block starts to move, bisected against the engine's "
        "own behaviour, plotted against <code>&mu;</code>. The dashed line is "
        "<code>atan(&mu;)</code>. The red column &mdash; a <em>cube</em>, which was the section's "
        "first fixture &mdash; is 0.004&deg; out at &mu;&nbsp;=&nbsp;0.2 and "
        "<strong>3.75&deg;</strong> out at &mu;&nbsp;=&nbsp;1.0, and the shape of that error is the "
        "clue: a wrong constant would have been wrong at both ends. A block has TWO critical "
        "angles and does whichever comes first &mdash; it slides when "
        "<code>tan&theta;&nbsp;&gt;&nbsp;&mu;</code> and TIPS when "
        "<code>tan&theta;&nbsp;&gt;&nbsp;w/h</code> &mdash; and a cube's two limits collide at "
        "45&deg;. A 10:3 slab tips at 73&deg;, stays out of the way, and tracks "
        "<code>atan(&mu;)</code> to <strong>0.43&deg;</strong>. The control is the same instrument "
        "pointed at the other formula: at &mu;&nbsp;=&nbsp;5, where sliding is impossible, the cube "
        "lets go at <strong>45.0032&deg;</strong>."),

    "FIG_ROLL": ("l89_fig7.svg", "7",
        "<strong>Friction acts below the centre of mass, so it slows a ball down and spins it up at "
        "the same time &mdash; and where those two meet does not depend on &mu; at all.</strong> "
        "The centre's speed falls at <code>&mu;g</code> while the surface speed "
        "<code>&omega;&middot;r</code> climbs at <code>&mu;g/c</code>; when they meet the contact "
        "point is instantaneously stationary, which is what rolling without slipping means, and "
        "friction has nothing left to act on. Cancel <code>&mu;</code> from the crossing and the "
        "answer is <code>v&#8320;/(1&nbsp;+&nbsp;c)</code> &mdash; <strong>5/7</strong> for a solid "
        "sphere, 3/5 for a shell &mdash; so friction decides only HOW LONG the transition takes. "
        "Measured: quadrupling &mu; moves the end speed by <strong>0.0000%</strong> and the "
        "transition time by <strong>3.79&times;</strong>. Both lines here are algebra; only the "
        "green marker is a simulation. The 0.06% it sits below 5/7 is not noise either &mdash; the "
        "contact point is <code>depth/2</code> inside the ball, so the lever arm is "
        "<code>R&nbsp;&minus;&nbsp;depth/2</code>, and the corrected closed form tracks the "
        "measurement to <strong>8&nbsp;&times;&nbsp;10&#8315;&#8311;</strong> across a 150&times; "
        "sweep of penetration."),

    "FIG_LEFTOVER": ("l89_fig8.svg", "8",
        "<strong>One pass over a four-point manifold does not hold a crate up.</strong> The "
        "penetration of a resting crate after fifteen seconds, against the number of solver passes, "
        "on a log axis. At one pass it is <strong>275&nbsp;mm</strong> into the floor &mdash; more "
        "than a quarter of its own height &mdash; and still sinking at 15&nbsp;mm/s; the residual "
        "approach velocity the solve leaves behind is 2.6&nbsp;&times;&nbsp;10&#8315;&sup2; m/s "
        "rather than zero, because each point's impulse disturbs the velocity the previous point's "
        "solve just set. By sixteen passes the residual is 1.6&nbsp;&times;&nbsp;10&#8315;&#8310; "
        "and the depth has FROZEN. But look at where it freezes: <strong>7.23&nbsp;mm</strong>, "
        "which is the penetration the crate ARRIVED with on the frame it was first detected, and no "
        "number of velocity iterations will ever repair it. Those are two different problems "
        "needing two different pieces of machinery, and they are Lesson 8.10's two jobs &mdash; "
        "arriving here early enough to be measured rather than promised."),
}

LISTING_META = {
    "engine/include/engine/phys/solver.hpp": ("new", "new"),
    "engine/src/phys/solver.cpp":            ("new", "new"),
    "engine/include/engine/engine.hpp":      ("modified", "modified"),
    "engine/CMakeLists.txt":                 ("modified", "modified"),
    "demos/CMakeLists.txt":                  ("modified", "modified"),
    "demos/impulse/main.cpp":                ("new", "new"),
    "scratch/verify_89.cpp":                 ("new", "new"),
    "scratch/build_verify_89.sh":            ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_89.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l89_" + path.replace("/", "_")


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
<title>8.9 — Impulse Response: Restitution and Friction · Build a Professional 3D Game Engine</title>
<meta name="description" content="Five lessons of collision detection have built a machine that answers questions - do these two overlap, how far apart are they, how deep, at which points, which pairs are worth asking about - and every one of those stages could have been declared const. Run the whole of it on a crate falling onto a floor and the crate falls through the floor, because nothing in the physics module has ever written a velocity. This lesson writes velocities. The obvious response to an overlap is a spring proportional to it, and section 1 gives that idea every chance before rejecting it: the stiffness is not free, because a crate resting at a given sink fixes it, and the rate that stiffness oscillates at does not contain the mass at all, so the penetration you are willing to see decides the frame rate of the entire simulation. One millimetre needs 49.5 Hz to stay stable and at 60 Hz the finest achievable is 0.68 mm; and the static number is the optimistic one, because a moving crate's penetration is set by its impact energy instead, so a floor tuned for a millimetre swallows 44 of them. Both knobs are then swept - four stiffnesses by forty-one dampings, 164 settings - and the result refuses the section's first draft: a penalty contact is not broken, it is tuned, and the best setting there is lands the crate perfectly at 108 mm of penetration or keeps it at 12.6 mm while it bounces 367 mm forever, and not both. What kills it is that the tuning belongs to the scene rather than the material: the same spring under a 200 kg crate lets it 962 mm into a half-metre floor. The impulse formulation has no stiffness to be stable about because it does not model a contact as a thing that lasts. A collision is an event, and the question is what single instantaneous change in velocity leaves the surfaces moving the way a contact requires - which is one linear equation per contact in one scalar unknown, because every step from impulse to velocity is linear. The constant in that equation is the effective mass, derived here and then rearranged into a form that proves the division is always safe, with both forms checked against each other over 200,000 random contacts and a positive-definiteness check that a sign error could not survive. A ten-kilogram plank pushed at its centre weighs ten kilograms and pushed 1.9 m out weighs 2.702, which is what the angular terms mean. Restitution needs no second equation, only a different target - and it brings the strangest artifact in the module with it. Because semi-implicit Euler applies gravity before the solver looks, the approach speed already contains this step's g times h, so restitution returns e times a speed that is too large, every bounce, by an amount that does not shrink as the bounce does. The result is a fixed point: a ball settles into a hop that never dies, 2.2 cm at e of 0.8 and 58 cm at 0.95, and the closed form for it is measured at exactly 1.000 times the prediction. The fix is exact rather than approximate, and the section then shows that the restitution threshold every engine ships is the same fix made approximately - and why you should keep both anyway. Friction is the same solve twice with a clip, and the square that many engines use instead of Coulomb's disc is measured at 42.7% direction-dependence in a basis chosen from the floor's own normal, against 0.00% for the cone, for 6.8% more time. Then a slope test that disagreed with the arctangent of mu by nearly four degrees and turned out to be measuring the fixture rather than the solver, because a block tips as well as slides and a cube's two limits collide at 45 degrees; a combination rule for two materials that this course ships differently from Box2D, Bullet and PhysX because the published coefficients say so under a perturbation sweep; a sphere that stops sliding at exactly five sevenths of the speed it landed at whatever mu is, with the 0.06% it misses by predicted from the manifold's own contact-point convention across a 150-fold sweep; and a solve order that, at the one pass this lesson ships, has no friction at all rather than less of it. It ends honestly: one pass over four contact points does not hold a crate up, it sinks 275 mm in fifteen seconds, and the converged solver freezes it at the depth it arrived with, which no velocity solve can repair. Two different problems, two different pieces of machinery, both of them Lesson 8.10's. Ships solver.hpp with materials, a friction cone, a prepared constraint and six closed forms; a demo with four scenes and the friction clip drawn as itself; eleven measured sections and 82 checks, every one with a control - including one that found a four-degree error in 8.6's EPA that five lessons had not seen.">

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
    <a class="prev-l" href="08-08-broadphase.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.8 — Broadphase: A Uniform Grid</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-10-sequential-impulses.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.10 — Sequential Impulses: Warm Starting, Islands, and Sleeping</span>
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
        with open(f"scratch/l89_body_{name}.html") as fh:
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
    # full of × − ° §, so `len(page)` is several thousand short of `wc -c`.
    print(f"wrote {OUT}  ({len(page.encode('utf-8')):,} bytes, "
          f"{page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
