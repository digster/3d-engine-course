#!/usr/bin/env python3
"""Assemble docs/lessons/08-12-ragdolls.html.

Same pipeline as build_71.py through build_811.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l812_body_{a..f}.html, figures from scratch/figs_812.py, and
every code listing PINNED at writing time — see LISTING_SOURCE. §14's snippets
were cut from the sources by scratch/gen_l812_body_e.py, once, into the static
fragment l812_body_e.html; this builder reads the fragment, never the script.

WHY THE PINS MATTER HERE. Twelve listings, and four of them (`constraint` and
`solver`, header and source) are files 8.11 printed and this lesson CORRECTED:
§7's position-pass fix and §12's kinematic wake. 8.11's page keeps its own pins
— the code as it shipped, bug and all — and this page's pins are the fixed
code. 8.13 will edit `solver` again; without these pins this page's prose, which
quotes both fixes line for line, would end up describing files that moved.

EVERY NUMBER IN THE PROSE IS A MEASUREMENT FROM THIS MACHINE, and
scratch/verify_812.log is the canonical run for all of them, with two named
exceptions (§7's 9-degree knee and §12's crate that did not move), which were
observed on the engine BEFORE the fixes and are told as observations. Two
consecutive runs were diffed: everything except §K's timings is byte-identical.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES. Here the pins
are generated from LISTING_META by `_pin`, so a path added to LISTING_META
without a pin file beside it fails loudly at build time.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/08-12-ragdolls.html"

FIGURES = {
    "FIG_PARTS": ("l812_fig1.svg", "1",
        "<strong>A ragdoll is a skeleton with mass, and each body has one owner at a time.</strong> Left: "
        "7.7's 23-joint humanoid in its bind pose, with eleven capsules drawn over it — cone-twist sockets "
        "at the waist, neck, shoulders and hips in blue, hinges at the elbows and knees in amber, the pelvis "
        "as the root. The hollow rings are the twelve passengers: joints with no body of their own, which "
        "ride on the part above them. Right: the two modes. While the clip owns the bodies they are "
        "kinematic and steered by velocity; <code>simulate()</code> gives them their mass back with the "
        "velocities they already have, and <code>animate()</code> takes it away again. The masses are "
        "Dempster's fractions of 80&nbsp;kg."),

    "FIG_SWING": ("l812_fig2.svg", "2",
        "<strong>The swing is two mirrors; Euler's singularity is in the middle of an arm's range and "
        "swing–twist's is outside every cone.</strong> Left: reflecting in the plane perpendicular to the "
        "bone at rest <code>t</code> and then in the plane perpendicular to the half-way vector "
        "<code>h</code> turns <code>t</code> onto <code>c</code> by the swing angle <code>φ</code> — the "
        "smallest rotation that does. Right: how far each split's angles move per radian of random nudge, "
        "for a hanging arm raised forward, on a log axis. Euler angles follow <code>1/cos(pitch)</code> to "
        "609.9 at 89.9°, an arm reaching forward; swing–twist reads 1.55 there and climbs only toward "
        "180°, the antipode of the axis it is measured from."),

    "FIG_CONE": ("l812_fig3.svg", "3",
        "<strong>A swing cone is the hinge's stop with <code>n̂</code> for its axis.</strong> Left: the swing "
        "<code>φ</code> is the angle between the cone's axis <code>a₁</code> and the bone <code>b₁</code>, "
        "and differentiating <code>cos φ = a₁·b₁</code> gives its rate as exactly "
        "<code>(ω_b − ω_a)·n̂</code>, with <code>n̂</code> along <code>a₁ × b₁</code>. Right: a rod socketed at "
        "its end, swung into its cone at 1 to 32 sweeps; its rebound, restitution zero, lies below and "
        "closing on <code>ρⁿ</code> as a door's did in 8.11. The table is the same rod socketed through its "
        "centre, where speculation overshoots by 1.2&nbsp;×&nbsp;10⁻⁷&nbsp;rad and a reactive cone by half a step's "
        "travel on average."),

    "FIG_TWIST": ("l812_fig4.svg", "4",
        "<strong>The twist's rate is about the half-way axis, and neither obvious axis.</strong> Left: the "
        "swing's own angular velocity is <code>2h × ḣ</code>, perpendicular to <code>a₁ + b₁</code>, so "
        "the twist rate is the relative spin along that axis divided by <code>1 + a₁·b₁</code> — the "
        "half-way vector over <code>cos(φ/2)</code>, √2 long at 90° of swing. Right: forty thousand random "
        "joints, binned by swing; the half-way row agrees with a finite difference of the twist to the "
        "difference's own resolution, and rows about the bone or the cone's axis are wrong by more than "
        "the rate itself beyond a degree of swing."),

    "FIG_CODMAN": ("l812_fig5.svg", "5",
        "<strong>Codman's paradox is a solid angle.</strong> Left: carry an arm down, forward, out to the "
        "side and down again without turning it about itself, and it comes home a quarter-turn twisted — "
        "the solid angle of the octant it enclosed. Right, top: circles at four swings, each leaving "
        "exactly <code>2π(1&nbsp;−&nbsp;cos&nbsp;φ)</code> of twist, to three decimals, from a harness that "
        "computes no solid angle. Right, bottom: a rod circling the rim of a 60° cone against a 20° twist "
        "stop. The half-way rows hold it; rows about the bone never see the twist move, and only the "
        "position pass holds it — 9.08° past the stop against 9.55° predicted — and without that it walks "
        "straight through."),

    "FIG_SELF": ("l812_fig6.svg", "6",
        "<strong>A capsule is a fair limb, and the pairs that matter are the ones that touch in falls.</strong> "
        "Left: the lever ratio <code>ρ</code> about each limb's proximal joint, capsule against Dempster's "
        "measured segment — about 0.1 high everywhere, because a capsule carries too little of its mass "
        "far from its centre. Right: every pair of parts that touched in twelve trips; the torso and a "
        "forearm touch in nine, the thighs in six, and all three pairs are two links apart, so the "
        "“within two links” rule excludes them and a forearm comes to rest inside the chest. Nothing "
        "unjointed comes within 4&nbsp;cm at rest, so the default excludes the ten jointed pairs and "
        "nothing else."),

    "FIG_BUG": ("l812_fig7.svg", "7",
        "<strong>8.11's position pass let the far stop cancel the near one.</strong> Left: a shin on a fixed "
        "thigh, pushed by gravity into its hyperextension stop. The upper stop is 150° away and not "
        "violated, so its bias is zero — and 8.11 solved it against that zero, which made it a hard "
        "“no pseudo-velocity toward me” row with exactly the lower stop's pseudo impulse, the other way. "
        "Right: the knee over ten seconds. Warm, 8.11's pass holds the violation frozen at −0.1691°; cold, "
        "the stop gives way to −70.7°. The fixed pass holds it at 0.0000° warm and a steady −1.8915° cold."),

    "FIG_SWEEPS": ("l812_fig8.svg", "8",
        "<strong>Sub-steps, not sweeps.</strong> Four of a ragdoll's failures against five budgets, on log "
        "scales: the peak joint gap in a fall, the gap left at rest after it, the gap with the whole body "
        "hung from one hand, and a chest lying on a forearm. Eight sub-steps of one sweep cost the same "
        "joint visits as the shipped eight sweeps and beat them everywhere — by four thousand times on the "
        "hanging load, which more sweeps barely move."),

    "FIG_HANDOFF": ("l812_fig9.svg", "9",
        "<strong>The chord a steered body carries is the velocity its integrator would have produced.</strong> "
        "Left: semi-implicit Euler moves a body by <code>h·v(n)</code>, so the chord between two poses is "
        "<code>v(n)</code> itself, and it is parallel to the path's tangent at the step's midpoint. Middle: "
        "handed over with the chord, a tripped character keeps its 204.40&nbsp;kg·m/s and pitches forward "
        "1.18&nbsp;m in half a second; at rest, it moves 10&nbsp;mm. Right: each joint starts violated by the "
        "centripetal term <code>½h·|ω_b×(ω_b×r_b) − ω_a×(ω_a×r_a)|</code>, predicted in purple and measured "
        "in blue, to 0.43%."),

    "FIG_RETURN": ("l812_fig10.svg", "10",
        "<strong>Handing back, and what fighting the solver costs.</strong> Left: blending from the pose "
        "read off the bodies moves nothing more than 58&nbsp;mm in the first frame, where snapping moves a "
        "body 1.65&nbsp;m; without realignment the pelvis slides 1.30&nbsp;m back toward the trip; a local "
        "blend changes no bone by more than 0.06&nbsp;mm, a model-space one by 208. Right: a character "
        "jogging into a crate, its bodies steered, teleported, or teleported with the chord as well. Only "
        "steering tells the truth about velocity and lets the solve stand."),
}

LISTING_META = {
    "engine/include/engine/phys/ragdoll.hpp":    ("new", "new"),
    "engine/src/phys/ragdoll.cpp":               ("new", "new"),
    "engine/include/engine/phys/constraint.hpp": ("modified", "modified"),
    "engine/src/phys/constraint.cpp":            ("modified", "modified"),
    "engine/include/engine/phys/solver.hpp":     ("modified", "modified"),
    "engine/src/phys/solver.cpp":                ("modified", "modified"),
    "engine/include/engine/engine.hpp":          ("modified", "modified"),
    "engine/CMakeLists.txt":                     ("modified", "modified"),
    "demos/ragdoll/main.cpp":                    ("new", "new"),
    "demos/CMakeLists.txt":                      ("modified", "modified"),
    "scratch/verify_812.cpp":                    ("new", "new"),
    "scratch/build_verify_812.sh":               ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_812.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l812_" + path.replace("/", "_")


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
    "Module 7 gave the engine a character that moves because a clip says so, and Module 8 gave it "
    "bodies that move because Newton says so; a ragdoll has to be both, one after the other. This "
    "lesson turns 7.7's 23-joint humanoid into eleven capsules with Dempster's segment masses, four "
    "hinges and six ball-sockets with a swing cone and a twist range, and hands a jogging character to "
    "the solver in mid-stride and back. The angles are a swing-twist split, not Euler angles: Euler's "
    "sensitivity reaches 609.9 at an arm raised forward where swing-twist reads 1.55, and the swing is "
    "7.4's two mirrors. The swing cone's row is exact; the twist's rate is the half-way axis over "
    "cos(phi/2), and a row about the bone is wrong by the rate the swing plane turns - which, round a "
    "loop, is the loop's solid angle: Codman's paradox, reproduced to three decimals. The handoff keeps "
    "the character's momentum exactly, 204.40 kg m/s, because a kinematic body steered by velocity "
    "carries the chord its integrator would have produced; handed over at rest it stops dead. The "
    "return blends from the pose read off the bodies, realigned over the pelvis, and moves no bone by "
    "more than 0.06 mm. The ragdoll found two bugs the module had shipped - 8.11's position pass never "
    "repaired a violated two-ended limit under split impulse, and 8.10's islands never let a moving "
    "kinematic body wake anything - and both are fixed with 8.10's and 8.11's harnesses still agreeing "
    "with their pages. And five claims refused their sections, including that a ragdoll would go to "
    "sleep: it settles to millijoules and, at 8.10's crate-tuned thresholds, mostly does not. Ships "
    "phys/ragdoll.hpp and .cpp, cone-twist joints, a three-scene demo, and eleven measured sections "
    "with 68 checks.")


HEAD = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>8.12 — Ragdolls: Joints on a Skeleton · Build a Professional 3D Game Engine</title>
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
    <a class="prev-l" href="08-11-constraints-and-joints.html">
      <span class="dir">← Previous</span>
      <span class="ttl">8.11 — Constraints and Joints: Hinge and Ball-Socket</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="08-13-character-controller.html">
      <span class="dir">Next →</span>
      <span class="ttl">8.13 — A Character Controller</span>
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
        with open(f"scratch/l812_body_{name}.html") as fh:
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
