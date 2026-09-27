#!/usr/bin/env python3
"""Assemble docs/lessons/08-11-constraints-and-joints.html.

Same pipeline as build_71.py through build_810.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l811_body_{a..f}.html, figures from scratch/figs_811.py, and
every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER HERE. Ten listings, and two of them (`solver.hpp`/`.cpp`)
are files 8.9 and 8.10 already printed and 8.12 will certainly edit again: a
ragdoll needs swing and twist limits, and the lesson that adds them will widen
`joint_batch` and very likely `constraint_solver`. Without the pins this page's
prose — which quotes line-for-line snippets of both — would end up describing
files that had moved under it.

THE CONTACT HALF OF solver.cpp IS BYTE FOR BYTE 8.10's. The page says so, and
it is checkable: diff this lesson's pin against scratch/l810_engine_src_phys_
solver.cpp and every hunk is either the joint loop, the island grouping for
joints, `union_edge`, the rename, or a comment.

EVERY NUMBER IN THE PROSE IS A MEASUREMENT FROM THIS MACHINE, and
scratch/verify_811.log is the canonical run for all of them. Each <pre> block is
internally from a single run. Two consecutive runs were diffed: everything
except §A's and §E's and §K's timings is byte-identical.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES. Here the pins
are generated from LISTING_META by `_pin`, so a path added to LISTING_META
without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/08-11-constraints-and-joints.html"

FIGURES = {
    "FIG_ROW": ("l811_fig1.svg", "1",
        "<strong>A constraint is one row of twelve numbers and two bounds.</strong> Left: the "
        "Jacobian's four blocks dotted with the two bodies' linear and angular velocities give the "
        "rate at which the constraint's error <code>C</code> is changing. An ideal constraint does "
        "no work on any motion it allows, so its impulse must be a multiple of <code>Jᵀ</code> — one "
        "number, <code>λ</code>, along the row — and applying it changes <code>J·V</code> by "
        "<code>J M⁻¹ Jᵀ</code> per unit, a 1×1 matrix that is 8.9's effective mass seen from above. "
        "The worked example is a contact at a crate's corner: 2.5&nbsp;kg, because three quarters of "
        "a push there goes into turning. Right: every constraint in this lesson, as a row. The top "
        "four are one shape — a point row — differing only in direction and bounds; a rope is a "
        "contact with its direction reversed; and friction, which 8.9 wrote as a clip, is a motor "
        "whose target speed is zero."),

    "FIG_COST": ("l811_fig2.svg", "2",
        "<strong>The general form is faster, and the reason is a hoist rather than generality.</strong> "
        "Left: the multiplications in one normal solve. Measuring <code>J·V</code> costs about what "
        "8.9's relative velocity costs; the difference is in <em>applying</em> the impulse, where "
        "8.9 recomputes <code>I⁻¹(r × J)</code> for both bodies on every visit and a prepared row "
        "has it stored. Right: the measurement over 200 contacts of a settled yard, both arms as "
        "harness-local copies behind the same <code>noinline</code> boundary — 8.75&nbsp;ns a point "
        "per sweep as a row against 11.95 by hand, for 32 more bytes a point. The prediction this "
        "lesson was handed said the opposite."),

    "FIG_ROPE": ("l811_fig3.svg", "3",
        "<strong>A rope lets go exactly where a rod starts to push.</strong> A bob swung from the "
        "bottom with <code>v₀² = 3.5·g·L</code> rises above its pin. Energy and the centripetal "
        "requirement put the joint's tension at zero at <code>y = L/2</code>: a rope's one-sided row "
        "hits its bound there and the bob leaves the circle on a parabola — which lands, by a line of "
        "algebra, exactly on the lowest point of the circle — while a rod's row changes "
        "sign from tension to compression at the same height and carries the bob on to "
        "<code>3L/4</code>. The table is the measurement, and it refuses the closed form at 60&nbsp;Hz "
        "in a particular way: the two instruments agree with each other to four decimals at every "
        "rate, and both read low by an amount that shrinks with the step — because the bob arrives "
        "with 10% less energy than it started with, which is §5."),

    "FIG_DRIFT": ("l811_fig4.svg", "4",
        "<strong>A velocity constraint drifts because a tangent is not a circle.</strong> Left, drawn "
        "with a step twenty times too large: after the solve the bob moves exactly perpendicular to "
        "the rod, the position update follows that straight line for a whole step, and it lands at "
        "<code>√(r² + (v·h)²)</code> from the pin. The next solve removes the radial part of the "
        "velocity, which conserves <code>r × v</code> exactly, so the speed falls as the rod "
        "stretches. Right: the recurrence, iterated with nothing from the engine in it, against a "
        "rod spun at 5&nbsp;m/s with no correction — sixty steps, 16% of stretch, agreement to "
        "1.7&nbsp;×&nbsp;10⁻⁷. Below: for a rigid body on a pin only the centre's orbit is projected, "
        "so the energy lost per step is <code>ρ(ωh)²</code> with <code>ρ</code> the parallel-axis "
        "ratio — measured to 0.1%."),

    "FIG_BAUMGARTE": ("l811_fig5.svg", "5",
        "<strong>Baumgarte adds energy, and on a turning joint it adds it exactly where the velocity "
        "projection takes it away.</strong> Left: at the steady state the rod sits stretched by "
        "<code>δ/β</code> and Baumgarte's bias is a real inward velocity of <code>δ/h</code>. One step "
        "later the rod has turned by <code>ω·h</code>, so a fraction <code>ω·h</code> of that inward "
        "velocity lies along the new tangent — <code>v(ωh)²/2</code>, precisely the speed the "
        "projection removes. Right, top: the rod spun at 5&nbsp;m/s loses <code>(ωh)²</code> of its "
        "energy per step with no correction and under split impulse, and nothing under Baumgarte. "
        "Right, bottom: the other side of the ledger. A cube pulled back into a corner socket from "
        "0.2&nbsp;m away keeps 0.35&nbsp;J of spin under Baumgarte — energy in the joint's free "
        "directions that nothing takes back out — and exactly none under split impulse."),

    "FIG_BLOCK": ("l811_fig6.svg", "6",
        "<strong>Three coupled rows solved one at a time converge at a rate the effective-mass matrix "
        "predicts; solved as one block they are exact.</strong> Left: the residual after each visit to "
        "a single random ball-socket, on a log axis. The rows fall by 0.2323 per visit; the "
        "spectral radius of <code>K</code>'s Gauss–Seidel iteration matrix, computed from "
        "<code>K</code> alone without running anything, is 0.2325. The 3×3 block is at "
        "1.2&nbsp;×&nbsp;10⁻⁷ after one visit and exactly zero after three. Right: over a thousand "
        "random sockets the block never leaves more than 5.9&nbsp;×&nbsp;10⁻⁷ of the violation and the "
        "rows leave up to 92%; the block is also the cheaper of the two; and on a twelve-link chain "
        "it barely helps, because a chain's problem is between its joints, not inside one."),

    "FIG_PENDULUM": ("l811_fig7.svg", "7",
        "<strong>A physical pendulum's period is the parallel-axis theorem made audible.</strong> "
        "Left: a 1&nbsp;m plank on a pin at its top swings about the pin, where its inertia is "
        "<code>I_cm + m·d²</code>; a point mass at the same distance would be 13.8% quick at every "
        "amplitude. Right: the period against amplitude, divided by the small-swing period. The curve "
        "is <code>1/AGM(1, cos(θ₀/2))</code> — the complete elliptic integral written as Gauss's "
        "arithmetic–geometric mean — and the rings are the first period from rest, measured at "
        "3840&nbsp;Hz so that the swing is not losing energy while it is timed. The table below is "
        "the small-swing error at four step sizes, falling by a factor of four with each halving: "
        "second order in <code>h</code>, and nothing added by the joint."),

    "FIG_HINGE": ("l811_fig8.svg", "8",
        "<strong>A hinge is a ball-socket plus two angular rows, and the frame their impulse is cached "
        "in was a design input rather than a bug fix.</strong> Left: the axis as each body carries it; "
        "their cross product, resolved along two perpendiculars <code>p₀</code> and <code>p₁</code>, "
        "is the two angles the hinge must not allow, and the rows say the relative spin may lie along "
        "the axis and nowhere else. Right, top: a 10&nbsp;kg door stays within 0.0001° of true on its "
        "hinge and falls over, 176°, on the ball-socket alone. Right, bottom: 8.10 §4's bug, probed on "
        "a sagging bridge — a basis chosen from the hinge axis in world space flips by exactly 90° "
        "127 times in ten seconds; chosen in the body's own frame, where the axis never moves, it "
        "never does."),

    "FIG_LIMITS": ("l811_fig9.svg", "9",
        "<strong>A limit is a contact that is always detected — and on a door it converges against "
        "the pin at a rate the parallel axis sets.</strong> Left: the limit row is angular, so it "
        "turns the door about its centre; the door really turns about its pin, so the pin pulls part "
        "of the correction back, and Gauss–Seidel between the two contracts by "
        "<code>ρ = m·d²/(I_cm + m·d²)</code> per sweep — 3/4 for any uniform door. The table is a "
        "turnstile, where nothing couples: a reactive limit overshoots by a fraction of "
        "<code>ω·h</code> uniform on [0,&nbsp;1], exactly 8.10 §6's arrival depth, and a speculative "
        "one by zero. Right: the error left after 1, 2, 4 and 8 sweeps for three pin offsets, lying on "
        "<code>ρⁿ</code> to four figures; and what eight sweeps leave behind — a door with no "
        "restitution bouncing off its stop at 7.5% of its speed."),

    "FIG_LEAVES": ("l811_fig10.svg", "10",
        "<strong>What joints leave behind, and the better answer than more sweeps.</strong> Left: the "
        "total stretch of a ten-link chain against sweeps per step, for end weights of 1, 10 and "
        "100&nbsp;kg, on log axes. Each doubling of the sweeps roughly halves it, and the stretch grows "
        "with the mass ratio — 388&nbsp;mm at 100:1 and eight sweeps, a sixth of the chain's length. "
        "Right, top: the same number of joint visits spent as one 1/60&nbsp;s step of eight sweeps or "
        "eight 1/480&nbsp;s steps of one: nineteen times less stretch, because a joint's errors are "
        "second order in the step. Right, bottom: what each kind of joint costs in the solver's own "
        "loop — a hinge with a limit and a motor is less than half of one contact manifold."),
}

LISTING_META = {
    "engine/include/engine/phys/constraint.hpp": ("new", "new"),
    "engine/src/phys/constraint.cpp":            ("new", "new"),
    "engine/include/engine/phys/solver.hpp":     ("modified", "modified"),
    "engine/src/phys/solver.cpp":                ("modified", "modified"),
    "engine/include/engine/engine.hpp":          ("modified", "modified"),
    "engine/CMakeLists.txt":                     ("modified", "modified"),
    "demos/joints/main.cpp":                     ("new", "new"),
    "demos/CMakeLists.txt":                      ("modified", "modified"),
    "scratch/verify_811.cpp":                    ("new", "new"),
    "scratch/build_verify_811.sh":               ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_811.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l811_" + path.replace("/", "_")


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
    "Lesson 8.10's solver holds a stack of crates up and cannot hold a lamp up: a contact may push "
    "and may not pull, and a lamp on a chain is nothing but pulling. This lesson writes 8.9's three "
    "lines - a relative velocity along a direction, divided by an effective mass, clamped - once, for "
    "any question of that shape, as a Jacobian row: twelve numbers saying which combination of two "
    "bodies' velocities a constraint cares about, and two bounds saying what it may do about it. A "
    "contact normal turns out to be exactly such a row, checked to 1.9e-07 on two hundred real "
    "contacts, and the general form turns out to be 27% faster than the hand-written one because it "
    "stores M^-1 J^T - which refused the prediction the lesson was handed. Then the new constraints, "
    "each a choice of rows and bounds: a rod; a rope, which is a contact turned inside out and lets "
    "go at exactly the height a rod's row changes sign; a ball-socket, solved as one 3x3 block that "
    "is exact where three rows leave up to 92% of the error and converge at the spectral radius of "
    "their effective-mass matrix (0.2323 measured, 0.2325 computed); and a hinge, with speculative "
    "limits that overshoot by exactly zero and a motor whose stall torque bisects to m g d. Along "
    "the way the measurements refused four claims. A velocity-level joint drifts outward along a "
    "tangent by a Pythagorean recurrence that tracks the simulation to 1.7e-07, and loses rho times "
    "(omega h) squared of its energy every step. Baumgarte, which 8.10 showed throwing crates out of "
    "floors, conserves a turning joint's energy and leaves 0.35 J of spin in a limb pulled back into "
    "its socket. And one number, the parallel-axis ratio rho = m d^2 / (I_cm + m d^2), set a "
    "pendulum's period (1.645967 s measured against 1.646241), a joint's dissipation, a motor's "
    "spin-up and the rate at which a hinge limit converges against its pin - 0.7481 per sweep for a "
    "door, to four figures, which is why a door with no restitution bounces off its stop at 7.5% of "
    "its speed. Joints join islands by the same rule as contacts, or a struck chain wakes one link "
    "and swings it from a frozen chain in mid-air. The lesson ends on chains: a 100:1 end weight "
    "stretches ten links by 388 mm at eight sweeps, and eight sub-steps of one sweep hold it to 20 - "
    "the argument for small steps that Module 9 will price. Ships phys/constraint.hpp and .cpp, "
    "joints in the constraint solver's loop, a four-scene demo, and eleven measured sections with "
    "58 checks.")


HEAD = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>8.11 — Constraints and Joints: Hinge and Ball-Socket · Build a Professional 3D Game Engine</title>
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
    <a class="prev-l" href="08-10-sequential-impulses.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.10 — Sequential Impulses: Warm Starting, Islands, and Sleeping</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-12-ragdolls.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.12 — Ragdolls: Joints on a Skeleton</span>
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
        with open(f"scratch/l811_body_{name}.html") as fh:
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
