#!/usr/bin/env python3
"""Assemble docs/lessons/08-10-sequential-impulses.html.

Same pipeline as build_71.py through build_89.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l810_body_{a..f}.html, figures from scratch/figs_810.py, and
every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. This lesson ships TEN listings and six of
them are files earlier lessons already printed — `solver.hpp`/`.cpp` from 8.9,
`rigid_body.hpp`/`.cpp` from 8.2 and 8.3, `manifold.hpp`/`.cpp` from 8.7 — and
every one of them disagrees with the earlier page on purpose. 8.11 will widen
`contact_constraint` again for a Jacobian and will move `solver_config`'s
defaults once it has joints to tune them against, so without the pins this
page's prose would end up describing a file that had moved under it.

*** AND THE PINS CARRY ONE CORRECTION BACK. *** `manifold.hpp` gained
`contact_manifold::tangent` this lesson, which means 8.7's published copy is now
missing a field. That is CORRECT and deliberate: a page is an archive of its own
era (8.6's rule), and 8.7's manifold genuinely did not have one. What would NOT
be acceptable is 8.7's page describing behaviour this lesson proved wrong, and
it does not — it says the tangent impulses are "in the solver's tangent basis",
which was true and incomplete rather than false.

THIS IS THE LARGEST LISTING SET IN THE COURSE, at a little over 10,000 lines
across ten files. Two of them (`rigid_body.hpp`, `manifold.hpp`) changed by a
few dozen lines each and are printed whole anyway, because CLAUDE.md §8 says a
file that changed appears whole and because a reader diffing against 8.3's copy
is doing something the course should support.

EVERY NUMBER IN THE PROSE IS A MEASUREMENT FROM THIS MACHINE, and
scratch/verify_810.log is the canonical run for all of them. Each <pre> block is
internally from a single run; none of them mixes. Re-running the harness gives
the same conclusions and different timings in §13 — the non-timing output is
byte-identical across runs, which was checked by diffing two consecutive ones.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson. Here
the pins are generated from LISTING_META by `_pin`, so a path added to
LISTING_META without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/08-10-sequential-impulses.html"

FIGURES = {
    "FIG_GAUSS": ("l810_fig1.svg", "1",
        "<strong>A four-point manifold is already a system, and one sweep does not solve "
        "it.</strong> Left: the four impulses are applied in turn, and each one changes the "
        "crate's velocity <em>and its angular velocity</em>, so point 4's impulse moves the "
        "velocity point 1's solve had just set exactly on target. On a 10&nbsp;kg cube of "
        "half-extent 0.25&nbsp;m, three quarters of the effect of an impulse at a corner is "
        "rotation &mdash; and rotation is precisely what the four points share. That is the "
        "coupling, and it is why the answer is a <em>system</em> rather than four independent "
        "equations. Right: the measured residual after n sweeps, on a log axis, from an "
        "identical settled state with warm starting and the position correction both switched "
        "off. It falls by a constant factor of <strong>0.53 per iteration</strong> over four "
        "decades &mdash; which is what &quot;geometric convergence&quot; means, and is a stronger "
        "claim than any single number, because it says the count you need grows as the "
        "<em>logarithm</em> of the accuracy you want. The dashed line is not a fit: it is "
        "<code>r&#8321;&nbsp;&times;&nbsp;0.53&#8319;</code> drawn from the first measured point. "
        "The last two rows are excluded from the mean because at "
        "1.14&nbsp;&times;&nbsp;10&#8315;&#8312; the residual has stopped measuring the solver "
        "and started measuring <code>float</code>."),

    "FIG_WARM": ("l810_fig2.svg", "2",
        "<strong>The loop that makes warm starting possible took four lessons to close, and "
        "8.10 is the lesson that reads the value back.</strong> Left: 8.7 gave each contact a "
        "<code>contact_id</code> built from feature indices rather than positions, so that a "
        "contact can be recognised across frames even though its position has moved; "
        "<code>carry_impulses</code> matches this frame's points onto last frame's by that id; "
        "8.9's <code>write_back</code> puts the accumulated impulse into the manifold where the "
        "cache will keep it; and <code>prepare_contacts</code> now seeds each accumulator from "
        "it. On a settled tower <strong>97.62%</strong> of points inherit an impulse. Right, "
        "top: what that is worth &mdash; at the eight iterations this engine ships, with the "
        "position correction off so that only the velocity solve is holding the tower up, the "
        "cold tower does not settle, it <em>collapses</em>. Right, bottom: the control, which is "
        "the reason to be careful about what warm starting IS. Scaling the inherited impulse by "
        "a known factor makes it a guess that is wrong by a known amount, and a guess wrong by "
        "2&times; costs more than no guess at all. Read the last two columns of the "
        "&times;5 row together: fifty-two iterations looks like a good result until the fourth "
        "column says the tower is moving at 1.54&nbsp;m/s. <strong>The residual measures "
        "consistency, not correctness</strong>, and a solver that has launched the stack has "
        "nothing left to disagree with itself about."),

    "FIG_BASIS": ("l810_fig3.svg", "3",
        "<strong>A cached number is meaningless without the frame it was measured in, and this "
        "is what that costs.</strong> <code>contact_point::tangent_impulse</code> is two floats "
        "&mdash; <em>coordinates</em> in the basis the solver happened to build that frame &mdash; "
        "and until this lesson the basis was rebuilt from the normal every frame and stored "
        "nowhere. <code>tangent_basis</code> seeds its cross product from whichever axis the "
        "normal is <em>least</em> aligned with, which for the near-vertical normal of a crate on "
        "a floor means comparing <code>|n.x|</code> against <code>|n.z|</code>: two numbers that "
        "are both about 10&#8315;&#8309; and wander around each other. Left: two consecutive "
        "frames in which they cross. The normal has barely moved and the basis has rotated by a "
        "right angle, so last frame's friction &mdash; applied as though nothing had happened "
        "&mdash; acts sideways. Right: the measurement. It is not rare (113 of 11,830 "
        "manifold-frames rotate by more than thirty degrees) and it is not small (worst case "
        "134.6&deg;). The fix is two dot products and it is exact: rebuild the world-space "
        "impulse in the frame it was measured in, then re-measure it in this one. Note the size "
        "of the payoff as well as its direction &mdash; 380&nbsp;mm of sideways drift becomes "
        "155, a factor of 2.4 and not a factor of ten, and at eight iterations a ten-crate tower "
        "falls over either way for the reason &sect;14 measures."),

    "FIG_ARRIVAL": ("l810_fig4.svg", "4",
        "<strong>The depth a contact arrives with is a CEILING, not an equality &mdash; and this "
        "figure's first draft drew the equality.</strong> Left: the narrow phase is asked once a "
        "step, and <code>collide_manifold</code> reports nothing until the shapes actually "
        "overlap, so the first frame on which a contact exists is the frame <em>after</em> the "
        "body crossed the surface. But the body crossed it at some instant INSIDE a step, and how "
        "much of that step was left over depends on where it happened to be when the step began "
        "&mdash; which is arbitrary. Right: 200 finely spaced drop heights per row say so "
        "precisely. The arrival depth is <strong>uniform on [0,&nbsp;v&middot;h]</strong>: the "
        "mean is 0.49 of the ceiling on every row, the largest of 600 samples is 0.97, and the "
        "smallest is 0.002 &mdash; which is not zero only because a finite sweep never lands "
        "exactly on a step boundary. Below: the depth a converged solver settles at, against the "
        "iteration count. It moves by <strong>3%</strong> across a sixty-four-fold range of the "
        "knob people reach for, while the residual over the same range moves by six orders of "
        "magnitude. <em>Two different problems, and iteration is the answer to one of them.</em>"),

    "FIG_CORRECTION": ("l810_fig5.svg", "5",
        "<strong>The same correction, computed the same way, applied to two different "
        "velocities.</strong> Both compute a bias "
        "<code>&beta;&middot;max(0,&nbsp;d&nbsp;&minus;&nbsp;slop)/h</code> &mdash; the "
        "separating velocity that would remove a fraction &beta; of the excess overlap during "
        "this step. Baumgarte adds it to the target of the ordinary velocity solve, so the body "
        "gains a real velocity it <em>keeps</em>; split impulse drives a second, shadow velocity "
        "that exists for one step, moves the position at the end of it, and is then thrown away. "
        "That is the entire difference, and it is worth everything. Right, top: a crate spawned "
        "inside the floor, with gravity on, and the height it reaches relative to the surface. "
        "Below 200&nbsp;mm the push has to climb and never gets out, which is exactly why this "
        "bug survives in so many engines &mdash; a reader who has only tested shallow overlaps "
        "has never seen it. At 200&nbsp;mm Baumgarte throws the crate <strong>99&nbsp;mm clear "
        "of the floor</strong>; split impulse settles at the slop from every start depth tried. "
        "Right, bottom: and it is not a trade, because the two work at the same rate to within "
        "the resolution of a frame. The only cost is a second solve, priced at 46% of a velocity "
        "iteration &mdash; no friction to clip, no restitution, one target per point."),

    "FIG_ISLANDS": ("l810_fig6.svg", "6",
        "<strong>One rule, and it is the difference between a feature and nothing at all.</strong> "
        "Two bodies are in the same island when a chain of contacts joins them, and everything in "
        "an island must be solved together because an impulse anywhere in it propagates "
        "everywhere in it. Left: four towers on one floor are four islands, because an edge of "
        "the contact graph counts only when <em>both</em> ends can move &mdash; the dashed lines "
        "are the contacts each tower has with the floor, which are real contacts and are not "
        "edges. That is safe rather than merely convenient: an impulse applied to a fixed body "
        "changes nothing any other contact can read, because its velocity is never written, so it "
        "propagates nothing and joins nothing. Right: let it bridge, as every first "
        "implementation does, and the floor welds the level into a single island. On a yard of "
        "twenty towers that is <strong>20 islands against 1</strong>, on the same contacts, with "
        "the rule as the only difference &mdash; and one island is not a slow simulation, it is "
        "<em>no sleeping at all</em>, because a single rolling marble anywhere on that floor now "
        "keeps every crate in the level awake. The whole partition costs 2.48&nbsp;ns per body, "
        "labelling included, and allocates nothing: the union&ndash;find uses its own output "
        "array as the parent array, which is the trick &sect;12 nearly lost an hour to."),

    "FIG_SLEEP": ("l810_fig7.svg", "7",
        "<strong>The sleep test cannot run before the solve, and the reason is one number: "
        "<code>g&middot;h</code>.</strong> Left: a semi-implicit step applies gravity to every "
        "dynamic body before the solver looks, so a crate that has been motionless on a floor for "
        "a minute is, at that instant, travelling downward at <strong>0.1635&nbsp;m/s</strong> at "
        "60&nbsp;Hz &mdash; three and a third times the default sleep threshold. A test placed "
        "there never fires, at any setting, and the symptom is indistinguishable from thresholds "
        "that are merely too tight: <strong>0 of 100</strong> crates read as quiet before the "
        "solve and <strong>96 of 100</strong> after it, on the same frame of the same scene. It "
        "is 8.9's restitution artifact wearing a different hat &mdash; the same "
        "<code>g&middot;h</code>, in the same place, breaking a different thing. Right, top: "
        "what sleeping buys, which is the whole solve and the whole integration but "
        "<em>not</em> the collision detection; a caller can take most of the rest back by "
        "skipping pairs in which both bodies are asleep, which is safe because two sleeping "
        "bodies cannot have moved relative to each other. Right, bottom: why the decision is per "
        "ISLAND. A body asleep is an immovable body to everything else, so sleeping one on its "
        "own means it does not fall &mdash; knock the crate out from under a three-crate tower "
        "and the island rule brings it down half a metre while the per-body rule leaves it "
        "exactly, measurably, where it was."),

    "FIG_BUDGET": ("l810_fig8.svg", "8",
        "<strong>What it costs, and how tall a stack it actually holds.</strong> Left: 2,001 "
        "bodies, 2,619 manifolds and 8,806 contact points, drawn to scale against a 16.67&nbsp;ms "
        "frame. Awake it is <strong>4.35&nbsp;ms</strong> &mdash; 26% of the budget, of which the "
        "solve is 71%, which is a reversal of 8.8's frame where the narrow phase dominated "
        "because there was no solve. Settled and asleep it is <strong>1.32&nbsp;ms</strong>, and "
        "the residue is the broadphase and narrow phase, which sleeping does not touch. The cost "
        "model underneath is linear to three figures: a fixed 14.96&nbsp;&micro;s for the "
        "islands, the sleep pass, a hundred prepares, the warm start and the write-back, plus "
        "12.92 per velocity sweep and 5.90 per position sweep. Right: and this is what actually "
        "decides the iteration count, because 60&nbsp;Hz would afford a thousand sweeps on that "
        "scene. A Gauss&ndash;Seidel sweep carries information across <em>one</em> contact, so a "
        "chain of n needs of order n sweeps &mdash; and the shipped default of eight holds a "
        "five-crate tower and does <strong>not</strong> hold a ten-crate one. An under-swept "
        "tower does not sag; it leans, because the corrections arrive inconsistently and the "
        "inconsistency has a direction. That is the honest capability of this solver, and fixing "
        "it needs a different formulation rather than a bigger number."),
}

LISTING_META = {
    "engine/include/engine/phys/solver.hpp":     ("modified", "modified"),
    "engine/src/phys/solver.cpp":                ("modified", "modified"),
    "engine/include/engine/phys/rigid_body.hpp": ("modified", "modified"),
    "engine/src/phys/rigid_body.cpp":            ("modified", "modified"),
    "engine/include/engine/phys/manifold.hpp":   ("modified", "modified"),
    "engine/src/phys/manifold.cpp":              ("modified", "modified"),
    "demos/stack/main.cpp":                      ("new", "new"),
    "demos/CMakeLists.txt":                      ("modified", "modified"),
    "scratch/verify_810.cpp":                    ("new", "new"),
    "scratch/build_verify_810.sh":               ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_810.sh": ("bash", "shell"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l810_" + path.replace("/", "_")


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
    "Lesson 8.9 ended on a confession: one pass of impulses over a four-point manifold does not "
    "hold a crate up. It leaves 2.6e-02 m/s of residual approach velocity, the crate sinks 275 mm "
    "in fifteen seconds, and it never stops - and that was one of two failures, which are "
    "different problems needing different machinery. This lesson builds both, and then the two "
    "things that make the result affordable. Iteration is the first: sweep every contact again "
    "against the velocities the other contacts have since produced, and the residual falls by a "
    "measured factor of 0.53 per sweep, geometric and constant over four decades until it reaches "
    "the float floor - and a four-point manifold on a single crate is already a system, so you do "
    "not need a stack to need a solver. Warm starting is what makes a handful of sweeps enough "
    "instead of hundreds, and it was already built: 8.7 gave every contact a feature id, 8.9's "
    "write_back puts the impulse in the manifold, and this lesson closes the loop by reading it "
    "back. A settled five-crate tower reaches a residual of 1e-4 in 89 iterations warm and has "
    "not reached it in 400 cold; at the eight iterations this engine ships the cold tower "
    "collapses, 2,020 mm of sink, where the warm one holds at 40. Closing that loop also exposed "
    "a bug three lessons old, and it is the best kind: the two cached friction impulses are "
    "coordinates in a basis, the basis was never stored with them, and tangent_basis re-chooses "
    "it every frame by comparing two components of the normal that are both around 1e-05. They "
    "cross constantly - measured at 0.96% of manifold-frames rotating by more than thirty "
    "degrees, worst case 134.6 - and last frame's friction is applied sideways. Then the second "
    "failure, which no number of velocity iterations can touch, because a converged solve says "
    "the overlap stops growing and nothing at all about the overlap already there. Section 6 sets "
    "out to prove the arrival depth is exactly one step of travel and the measurement refuses it: "
    "v times h is a ceiling, and over 200 finely spaced drops the depth is uniform on that "
    "interval with a mean of 0.49 and a largest value of 0.97. Two corrections fix it and one of "
    "them adds energy - a crate spawned 200 mm inside the floor is thrown 99 mm clear of it by "
    "Baumgarte and settles quietly at the slop under split impulse, for the same time constant "
    "and 46% of a velocity iteration. The penetration slop is then measured rather than assumed, "
    "and refuses its own folklore too: a single resting contact is bit-for-bit motionless at a "
    "slop of zero, because a geometric correction never reaches zero; what needs the slop is "
    "contacts that compete, and a five-crate tower breathes 1.80 mm at zero and stops completely "
    "at two. Then islands, whose whole content is one rule: a body that cannot move is not a "
    "bridge. Break it and a yard of twenty separate towers becomes one island, which is not a "
    "slow simulation but no sleeping at all. And sleeping, which takes a settled yard's solve "
    "from 131 microseconds to 3, with two findings that are not in the textbooks: the sleep test "
    "cannot run before the solve, because every resting body in the scene is travelling at g "
    "times h then (0 of 100 quiet, against 96 of 100 after), and sleeping has to be decided per "
    "island, because a body slept on its own becomes an immovable body and hangs in the air when "
    "the crate under it is knocked away - measured at 0.000 m of fall against 0.500. Along the "
    "way body_world::step is split in two so that a contact solve has somewhere to go, checked "
    "bit-for-bit against the function it replaced over 400 bodies and 120 steps, and the closed "
    "form for solving one step late is measured at 2.72500 mm against a predicted 2.72500. Ships "
    "the sequential-impulse solver, a union-find island builder that uses its own output array as "
    "scratch, a sleeping policy, a four-scene demo that colours crates by island and dims them as "
    "they fall asleep, eleven measured sections and 64 checks - three of which refused the claim "
    "their section was written to make. It ends where it must: eight sweeps hold a five-crate "
    "tower and do not hold a ten-crate one, which is the honest capability of a Gauss-Seidel "
    "solver and the reason Lesson 8.11 writes a constraint as a Jacobian.")


HEAD = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>8.10 — Sequential Impulses: Warm Starting, Islands, and Sleeping · Build a Professional 3D Game Engine</title>
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
    <a class="prev-l" href="08-09-impulse-response.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.9 — Impulse Response: Restitution and Friction</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-11-constraints-and-joints.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.11 — Constraints and Joints: Hinge and Ball-Socket</span>
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
        with open(f"scratch/l810_body_{name}.html") as fh:
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
    # BYTES, not characters — build_74.py's note applies. The page is UTF-8 and
    # full of × − ° §, so `len(page)` is several thousand short of `wc -c`.
    print(f"wrote {OUT}  ({len(page.encode('utf-8')):,} bytes, "
          f"{page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
