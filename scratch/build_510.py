#!/usr/bin/env python3
"""Assemble docs/lessons/05-10-input-mapping.html.

Same pipeline as build_58.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l510_body_{a,b,c}.html, figures from scratch/figs_510.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-10-input-mapping.html"

FIGURES = {
    "FIG_INDIRECTION": ("l510_fig1.svg", "1",
        "The whole lesson in one picture, and note that <strong>none of the four losses on the "
        "right is a performance problem</strong>. A hard-coded scancode is not slow; it is unable "
        "to say the thing the program means, and the four consequences follow from that one "
        "failure. Putting a NAME in the middle &#8212; an action the game means, a binding that "
        "maps a signal onto it, a value published once per frame &#8212; dissolves all four, and "
        "the indirection earns its keep because it is <em>naming</em> the layer below rather than "
        "hiding it. A wrapper that renamed <code>key_pressed</code> to <code>button_pressed</code> "
        "would be pure cost."),

    "FIG_MECHANISM": ("l510_fig2.svg", "2",
        "One rule covering both jobs an input system has. A button, an axis built from two keys, "
        "and an axis from a continuous source are all the same mechanism &#8212; every binding "
        "contributes a signed value and the action sums them. <strong>The middle row is what pays "
        "for the design:</strong> a system with separate button and axis kinds would have needed a "
        "documented rule for &#8220;what does holding both halves mean&#8221;, and would have had "
        "to write it, test it and explain it. Here there is no rule, because &#8722;1 + 1 = 0 and "
        "arithmetic already knew that. The threshold at the bottom exists for sources that are not "
        "keys: an analog stick should have to be pushed, not brushed."),

    "FIG_EDGES": ("l510_fig3.svg", "3",
        "The decision that is invisible until an action has TWO bindings &#8212; which is to say, "
        "invisible in a first implementation and in the first test written for it. Follow the "
        "level row: it goes true in frame 2 and stays true until frame 5, so there is exactly one "
        "rising edge and one falling edge. Derive the edges from the BINDINGS instead and frames 3 "
        "and 4 each fire one too, giving <strong>two presses for one intention and a double "
        "jump</strong>. The correct rule is one sentence: an edge is a change in the ACTION, not a "
        "change in a signal. It ships as &#8220;sometimes it jumps twice&#8221; on somebody "
        "else&#8217;s machine, the week they bind a controller alongside their keyboard."),

    "FIG_FRAME": ("l510_fig4.svg", "4",
        "Why Lesson 5.10 had to add a hook, which is not a thing to do lightly. An action map must "
        "be updated once per frame, after input is published and before anything reads it &#8212; "
        "and the simulation reads it. <code>on_event</code> runs several times a frame or none; "
        "<code>on_fixed_step</code> runs zero or more times and is also where the answers get "
        "read; and <code>on_frame</code> is the near miss, right in frequency and wrong in "
        "<em>place</em>, because it runs AFTER the steps and would leave every step of the frame "
        "reading last frame&#8217;s actions. Invisible in a demo; a real 16 ms of input delay in a "
        "game."),

    "FIG_QUEUE": ("l510_fig5.svg", "5",
        "Lesson 1.4&#8217;s trap in its most expensive form, and it fails in BOTH directions, "
        "which is why neither is fixable by the caller. A frame-scoped edge read inside the step "
        "is true for the whole frame, so a two-step frame acts on it twice; and it is gone by the "
        "next frame, so a zero-step frame loses it entirely. Queueing the press fixes both with "
        "the same mechanism &#8212; the first step takes it, the second finds the queue empty, and "
        "a frame that ran no steps leaves it waiting. <strong>The four-deep cap is a decision "
        "rather than an overflow bug:</strong> an unbounded queue replays a burst of jumps after "
        "the player has stopped asking, which feels worse than losing them."),
}

LISTING_META = {
    "engine/include/engine/core/actions.hpp": ("new", "new"),
    "engine/src/core/actions.cpp": ("new", "new"),
    "demos/ecs_swarm/main.cpp": ("modified", "modified"),
    "scratch/verify_510.cpp": ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/platform/app.hpp": ("modified", "modified"),
    "engine/src/platform/app.cpp": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
}

LISTING_LANG = {
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/CMakeLists.txt": ("cmake", "CMake"),
}

# PINNED, as this file's own warning said it would have to be.
#
# Listings are spliced from the live repository so they cannot drift from what
# compiles. That has one failure mode: a file a LATER lesson changes. Three times
# now (build_57.py's retired STATE block, build_58.py's demo, and this) re-running
# an old builder would have quietly rewritten a shipped page with a newer lesson's
# code.
#
# Lesson 5.11 reworked demos/ecs_swarm/main.cpp — an ImGui panel, a debug-line
# queue and the masked input gate — so 5.10's page must read the snapshot below
# and not the live file. It was taken BEFORE the edit, from 5.10's own commit,
# and verified:
#     git show c697e14:demos/ecs_swarm/main.cpp > scratch/l510_ecs_swarm.cpp
#     git show c697e14:demos/ecs_swarm/main.cpp | diff - scratch/l510_ecs_swarm.cpp
#
# A BUILDER IS NOT FROZEN JUST BECAUSE ITS PAGE IS SHIPPED.
#
# NOTE THE SECOND ENTRY, because it is the same trap arriving from a direction
# the warning did not name. actions.hpp was 5.10's own new header, so it read as
# "finished"; 5.11 added `masked_input` to it and the rebuild spliced a class
# that mentions Lesson 5.11 into Lesson 5.10's listings. The rule is not "pin the
# demo" — it is PIN EVERY FILE THIS PAGE LISTS THAT A LATER LESSON TOUCHES, and
# the cheap way to find out is to rebuild and diff before shipping.
# EXTENDED 2026-09-12 to cover EVERY listing, not just the files this
# lesson wrote. The partial dict below pinned what its author knew would
# move; everything else stayed live, so later lessons' edits leaked into
# this page anyway — which is why it no longer rebuilt to what it shipped.
# Pinned from c697e14, verified per file against the shipped page
# with scratch/which_commit.py.
#
# The original dict, kept for its provenance:
#   LISTING_SOURCE = {
#       "demos/ecs_swarm/main.cpp": "scratch/l510_ecs_swarm.cpp",
#       "engine/include/engine/core/actions.hpp": "scratch/l510_actions.hpp",
#   }
LISTING_SOURCE = {
    "engine/include/engine/core/actions.hpp": "scratch/l510_engine_include_engine_core_actions.hpp",
    "engine/src/core/actions.cpp":            "scratch/l510_engine_src_core_actions.cpp",
    "demos/ecs_swarm/main.cpp":               "scratch/l510_demos_ecs_swarm_main.cpp",
    "scratch/verify_510.cpp":                 "scratch/l510_scratch_verify_510.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/platform/app.hpp": "scratch/l510_engine_include_engine_platform_app.hpp",
    "engine/src/platform/app.cpp": "scratch/l510_engine_src_platform_app.cpp",
    "engine/CMakeLists.txt": "scratch/l510_engine_CMakeLists.txt",
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
<title>5.10 — Input Mapping: Actions, Not Keycodes · Build a Professional 3D Game Engine</title>
<meta name="description" content="An input mapping layer for a fixed-timestep engine: actions, bindings and values, where every binding contributes a signed float so buttons and axes are one mechanism. Covers the two decisions that are invisible until they bite — why an edge must be derived from an action's level rather than from any binding's, which fails as a double jump the week somebody binds a second device, and why a fixed step needs a queued edge that survives both a two-step frame and a zero-step one. Measures nothing, and says so.">

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
    <a class="prev-l" href="05-09-transform-hierarchy.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.9 — Transform Hierarchy and the Camera System</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-11-imgui-debug-draw.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.11 — Dear ImGui and the Debug Draw System</span>
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
    for name in ("a", "b", "c"):
        with open(f"scratch/l510_body_{name}.html") as fh:
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
