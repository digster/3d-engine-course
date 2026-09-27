#!/usr/bin/env python3
"""Assemble docs/lessons/05-11-imgui-debug-draw.html.

Same pipeline as build_510.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l511_body_{a,b,c}.html, figures from scratch/figs_511.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-11-imgui-debug-draw.html"

FIGURES = {
    "FIG_REWORK": ("l511_fig1.svg", "1",
        "The whole rework in one picture, and note that <strong>nothing on the right is "
        "faster</strong>. On the left, two of <code>line3</code>&#8217;s six parameters are "
        "renderer state, so only the renderer can call it &#8212; which locks out exactly the "
        "code that knows what is worth drawing. Removing those two arguments is the entire "
        "change: a queue holds world-space segments, anybody fills it, and one flush per camera "
        "hands them to whichever backend exists. The other two wins fall out for free, because "
        "a segment that has not been drawn yet can be drawn by anything and can be kept for as "
        "long as you like."),

    "FIG_INCLUDES": ("l511_fig2.svg", "2",
        "Why the rework is two headers rather than one new function in the old one. A header is "
        "a contract about what you are forced to compile against, and C++ include dependencies "
        "are transitive &#8212; so a physics system that includes the right-hand file to draw a "
        "single box compiles the framebuffer, the depth buffer, the projector, the viewport and "
        "the handle system, in every translation unit, forever. <strong>The arrow runs one "
        "way</strong>, and it is that constraint, not tidiness, that makes "
        "<code>wire_mesh()</code> take two spans instead of the <code>mesh</code> that owns "
        "them."),

    "FIG_LIFETIME": ("l511_fig3.svg", "3",
        "Three lifetimes across five frames, and the two ways to get the expiry rule wrong. "
        "Above: a zero-second line is drawn once, which is what a per-frame visualisation wants, "
        "and a two-second line is still there when you look up. Below: subtract-then-test makes "
        "the answer depend on the comparison operator AND on <code>dt</code> &#8212; with "
        "<code>&lt;</code> a one-frame line never expires when <code>dt</code> is zero, and "
        "<code>dt</code> is zero on a paused clock, a single-frame <code>--shot</code>, and any "
        "frame you are stopped in the debugger for. <strong>Testing first removes the dependency "
        "instead of getting it right</strong>, which is the more durable kind of fix."),

    "FIG_UIFRAME": ("l511_fig4.svg", "4",
        "The six ImGui calls placed in Lesson 1.4&#8217;s loop. Five of them are where you would "
        "guess. The sixth &#8212; <code>begin_frame()</code>, boxed &#8212; is the whole ordering "
        "argument: ImGui computes its capture flags inside <code>NewFrame</code>, and the mask "
        "reads them two lines later, so <code>NewFrame</code> has to have already happened. Move "
        "it down beside the panels, where it looks like it belongs, and every read during the "
        "frame answers about the previous one. The bug is exactly one frame wide, arrives every "
        "single time, and is invisible unless you know to look for it."),

    "FIG_SEAM": ("l511_fig5.svg", "5",
        "Two consumers of one keyboard, and the layer at which they are separated. <strong>Both "
        "receive every event</strong> &#8212; the dashed box on the right is the tempting fix "
        "that is not taken, because <code>engine::input</code> tracks LEVELS and a level is only "
        "ever corrected by the event that contradicts it, so a key-up routed away leaves a key "
        "held down permanently. The arbitration instead happens one layer later, on the levels "
        "themselves, through a type that satisfies Lesson 5.10&#8217;s concept &#8212; which is "
        "why <code>action_map</code> did not change by one character."),

    "FIG_CURSOR": ("l511_fig6.svg", "6",
        "The subtlest thing in the lesson, with the numbers <code>verify_511</code> &#167;E "
        "asserts. A key is a level and can simply be reported as up; the cursor is not, because "
        "the consumer <em>derives</em> a delta by differencing two frames, and suppressing one "
        "endpoint of a difference does not suppress the difference. The middle row is the "
        "version that ships: it behaves perfectly until somebody drags a slider, at which point "
        "releasing it whips the camera round by the distance they dragged. The virtual cursor "
        "makes the reported position continuous everywhere, so both boundaries are free."),

    "FIG_RESULT": ("l511_fig7.svg", "7",
        "A real render, not a mock: the demo&#8217;s headless <code>--shot</code>, box-sampled "
        "and run-length encoded. Every line is one entity&#8217;s <code>parent</code> link, "
        "queued by a function whose signature contains no framebuffer. The count is derivable by "
        "hand &#8212; 96 ring members + 32 moons + 24 waypoints = 152 &#8212; and the hierarchy "
        "report independently says 2 roots, which is the sun and the camera. <strong>Two "
        "subsystems counting the same structure two different ways and agreeing</strong> is a "
        "much stronger statement than either number alone."),
}

LISTING_META = {
    "engine/include/engine/gfx/debug_lines.hpp": ("new", "new"),
    "engine/src/gfx/debug_lines.cpp": ("new", "new"),
    "engine/include/engine/ui/debug_ui.hpp": ("new", "new"),
    "engine/src/ui/debug_ui.cpp": ("new", "new"),
    "engine/include/engine/gfx/debug_draw.hpp": ("modified", "modified"),
    "engine/src/gfx/debug_draw.cpp": ("modified", "modified"),
    "engine/include/engine/core/actions.hpp": ("modified", "modified"),
    "engine/include/engine/engine.hpp": ("modified", "modified"),
    "CMakeLists.txt": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "demos/ecs_swarm/main.cpp": ("modified", "modified"),
    "scratch/verify_511.cpp": ("new", "new"),
}

LISTING_LANG = {
    "CMakeLists.txt": ("cmake", "CMake"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
}

# NOTHING PINNED YET — but the machinery stays, and so does the warning, which by
# now has a track record: build_57.py stamped a retired STATE block, build_58.py
# spliced 5.9's demo into 5.8's page, and build_510.py needed TWO pins rather than
# the one its own note predicted.
#
# THE RULE IS NOT "PIN THE DEMO". It is: pin every file this page lists that a
# later lesson touches — which for this page is a wide net, because 5.11 lists
# both CMakeLists.txt files, the umbrella header and actions.hpp, and Module 6
# will certainly edit at least the first three.
#
#     git show <5.11's commit>:<path> > scratch/l511_<name>
#     git show <5.11's commit>:<path> | diff - scratch/l511_<name>
#
# The cheap way to notice you needed a pin is to re-run this builder and
# `git diff` the page BEFORE shipping anything else.
#
# A BUILDER IS NOT FROZEN JUST BECAUSE ITS PAGE IS SHIPPED.
# EXTENDED 2026-09-12 to cover EVERY listing, not just the files this
# lesson wrote. The partial dict below pinned what its author knew would
# move; everything else stayed live, so later lessons' edits leaked into
# this page anyway — which is why it no longer rebuilt to what it shipped.
# Pinned from 9be6c96, verified per file against the shipped page
# with scratch/which_commit.py.
#
# The original dict, kept for its provenance:
#   LISTING_SOURCE = {}
LISTING_SOURCE = {
    "engine/include/engine/gfx/debug_lines.hpp": "scratch/l511_engine_include_engine_gfx_debug_lines.hpp",
    "engine/src/gfx/debug_lines.cpp":            "scratch/l511_engine_src_gfx_debug_lines.cpp",
    "engine/include/engine/ui/debug_ui.hpp":     "scratch/l511_engine_include_engine_ui_debug_ui.hpp",
    "engine/src/ui/debug_ui.cpp":                "scratch/l511_engine_src_ui_debug_ui.cpp",
    "engine/include/engine/gfx/debug_draw.hpp":  "scratch/l511_engine_include_engine_gfx_debug_draw.hpp",
    "engine/src/gfx/debug_draw.cpp":             "scratch/l511_engine_src_gfx_debug_draw.cpp",
    "engine/include/engine/core/actions.hpp":    "scratch/l511_engine_include_engine_core_actions.hpp",
    "engine/include/engine/engine.hpp":          "scratch/l511_engine_include_engine_engine.hpp",
    "CMakeLists.txt":                            "scratch/l511_CMakeLists.txt",
    "engine/CMakeLists.txt":                     "scratch/l511_engine_CMakeLists.txt",
    "demos/ecs_swarm/main.cpp":                  "scratch/l511_demos_ecs_swarm_main.cpp",
    "scratch/verify_511.cpp":                    "scratch/l511_scratch_verify_511.cpp",
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
<title>5.11 — Dear ImGui and the Debug Draw System · Build a Professional 3D Game Engine</title>
<meta name="description" content="Integrating Dear ImGui into an SDL3 engine, and reworking an immediate-mode debug drawer into a queue with lifetimes. Covers the test for when to take a third-party dependency and why this one goes through the public boundary when stb_image did not; why a debug queue must include nothing that can draw; an expiry rule that survives a frame with dt == 0; and the input seam — why two consumers of one keyboard must be separated on levels rather than by routing events, and why a mouse delta cannot be masked by masking one of its endpoints.">

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
    <a class="prev-l" href="05-10-input-mapping.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.10 — Input Mapping: Actions, Not Keycodes</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-12-checkpoint-game.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.12 — Checkpoint: A Small 3D Game on the Public API</span>
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
        with open(f"scratch/l511_body_{name}.html") as fh:
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
