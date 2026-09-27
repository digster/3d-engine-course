#!/usr/bin/env python3
"""Assemble docs/lessons/05-04-handles.html.

Same pipeline as build_53.py: prose from scratch/l54_body_{a,b}.html, figures
from scratch/figs_54.py, and code listings read straight out of the repository so
they cannot drift from what actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-04-handles.html"

FIGURES = {
    "FIG_QUESTION": ("l54_fig1.svg", "1",
        "The only question that matters, asked of the two things a program can hold. "
        "<strong>A pointer holds the same bits before and after the memory it names is "
        "freed</strong> &#8212; so there is no test to write, no assertion to add, and no flag to "
        "set: from the pointer&#8217;s side, nothing happened. A handle carries a "
        "<em>fingerprint</em> of the fact instead, a small number that changes whenever the "
        "answer changes, and a small number that changes is all a check needs."),

    "FIG_FAILURES": ("l54_fig2.svg", "2",
        "The whole derivation in one table: three ways a reference goes wrong, against three "
        "candidate designs. <strong>Read the middle cell.</strong> Replacing a pointer with a "
        "bare index fixes relocation outright and makes aliasing <em>worse</em> &#8212; it "
        "converts a probable crash into a guaranteed silent wrong answer, because a reused slot "
        "is in range and holds a live, valid, well-formed object that is not yours. The "
        "generation is the piece that closes it, and this is why it exists rather than being "
        "decreed."),

    "FIG_BITS": ("l54_fig3.svg", "3",
        "One 32-bit word split 20 / 12, and the arithmetic that chose the split rather than a "
        "shrug. Twenty index bits is a million slots; twelve generation bits is 4,095 usable, "
        "because <strong>generation 0 is reserved so that an all-bits-zero handle is null for "
        "free</strong>. The table is the uncomfortable part: a slot&#8217;s generation repeats "
        "after 4,095 frees <em>of that slot</em>, so the budget has to be checked against the "
        "churn rate rather than assumed. For assets it is enormous; for entities it is worth "
        "measuring, which is why <code>pool</code> counts its own wraps."),

    "FIG_ANATOMY": ("l54_fig4.svg", "4",
        "Resolving handle 2:3 through the pool&#8217;s three arrays. <code>slots_</code> is "
        "<strong>sparse and stable</strong> &#8212; indexed by the handle, never moved; "
        "<code>items_</code> is <strong>dense and mobile</strong> &#8212; live objects only, "
        "packed, reallocating as it grows; <code>owners_</code> is the way back, and the only "
        "reason removal can be O(1). Note the piece of design in the middle row: "
        "<code>dense = &#8212;</code> is simultaneously &#8220;where the item lives&#8221; and "
        "&#8220;this slot is empty&#8221;, so occupancy costs no extra byte and cannot disagree "
        "with itself."),

    "FIG_REMOVE": ("l54_fig5.svg", "5",
        "Removal, in two steps, and the second one is where the design proves itself. Keeping "
        "<code>items_</code> packed means the hole must be filled by the last element &#8212; "
        "so a <strong>surviving</strong> object changes address, and its slot has to be patched "
        "to say where it went. Delete that one line and the pool still compiles and still passes "
        "any test that only ever removes the last element. Because the quad&#8217;s handle names "
        "a slot rather than a position, it survives the move untouched: that is the whole proof "
        "that a handle is not a pointer with extra steps."),

    "FIG_COST": ("l54_fig6.svg", "6",
        "Measured rather than asserted, from <code>scratch/measure_54.py</code> and "
        "<code>verify_54</code>. Left: Lesson 4.8&#8217;s cache key, whose own comment called it "
        "&#8220;the shabby version of&#8221; a handle &#8212; five tests, four of them present "
        "only because an address can be reused, collapsing to two. Right: what the conversion "
        "cost and bought. <strong>A resolution is 0.13 ns dearer than a dereference</strong> "
        "&#8212; about half a cycle, because both extra loads sit in one cache line &#8212; "
        "which is negligible once per object and would not be once per vertex."),
}

LISTING_META = {
    "engine/include/engine/core/handle.hpp": ("new", "new"),
    "engine/include/engine/core/pool.hpp": ("new", "new"),
    "engine/include/engine/gfx/scene.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/soft_renderer.hpp": ("modified", "modified"),
    "engine/src/gfx/soft_renderer.cpp": ("modified", "modified"),
    "demos/common/demo_scene.hpp": ("modified", "modified"),
    "demos/common/demo_scene.cpp": ("modified", "modified"),
    "demos/hello_cube/main.cpp": ("modified", "modified"),
    "scratch/verify_54.cpp": ("new", "new"),
}

# LISTING_SOURCE — added 2026-09-12, and without it THIS SCRIPT CANNOT RUN.
# ---------------------------------------------------------------------------
#
# Every path here was read LIVE, so this page showed whatever the repository said
# on the day it was last built — which was not the day it shipped.
#
# PINNED FROM d599928 (Lesson 5.5), NOT FROM aeb4a4b (Lesson 5.4), AND THAT IS
# NOT A MISTAKE. When 5.5 landed it re-ran this builder to retrofit the `next`
# nav link, and the live reads pulled 5.5's code into 5.4's page: 298 lines in,
# 141 out. So the published 5.4 shows `demos/common/demo_scene.hpp` carrying a
# doc comment that says, in the past tense, "Lesson 5.5 replaced this struct's
# contents" — inside Lesson 5.4. Pinning to aeb4a4b would not reproduce the
# published page, it would silently rewrite it. Decided per file with
# `scratch/which_commit.py 54 aeb4a4b d599928`.
#
# THE UNDERLYING DEFECT IS REAL AND IS NOT FIXED HERE — see build_52.py's note,
# which carries the same injury from ea7a05f. Recorded in LEARNINGS.md and left
# for the course author, because correcting it changes a published lesson's
# content, which is a different decision from making its builder reproducible.
#
# scratch/verify_54.cpp IS PINNED FROM THE PAGE ITSELF, not from git and not from
# the working tree. `scratch/` is gitignored, so the file has no history; and the
# working-tree copy has since been rewritten by 6.4 (which renamed `specular` to
# `microsurface`) and 6.7 (which took `mesh` from four spans to five). Its
# 5.4-era text therefore survives in exactly one place — this lesson's own page,
# which embeds every listing whole because CLAUDE.md §8 forbids placeholders.
# Recovered with `scratch/extract_listing.py`, which round-trips the escaping to
# prove the extraction is faithful.
LISTING_SOURCE = {
    "engine/include/engine/core/handle.hpp":       "scratch/l54_engine_include_engine_core_handle.hpp",
    "engine/include/engine/core/pool.hpp":         "scratch/l54_engine_include_engine_core_pool.hpp",
    "engine/include/engine/gfx/scene.hpp":         "scratch/l54_engine_include_engine_gfx_scene.hpp",
    "engine/include/engine/gfx/soft_renderer.hpp": "scratch/l54_engine_include_engine_gfx_soft_renderer.hpp",
    "engine/src/gfx/soft_renderer.cpp":            "scratch/l54_engine_src_gfx_soft_renderer.cpp",
    "demos/common/demo_scene.hpp":                 "scratch/l54_demos_common_demo_scene.hpp",
    "demos/common/demo_scene.cpp":                 "scratch/l54_demos_common_demo_scene.cpp",
    "demos/hello_cube/main.cpp":                   "scratch/l54_demos_hello_cube_main.cpp",
    "scratch/verify_54.cpp":                       "scratch/l54_scratch_verify_54.cpp",
}

LISTING_LANG = {}


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
<title>5.4 — Handles: Generational Indices · Build a Professional 3D Game Engine</title>
<meta name="description" content="Every resource in this engine is a borrowed pointer, and that works only because nothing is ever destroyed. Three failures derived in order — dangling, aliasing (ABA), relocation — and why a bare index makes the middle one worse. A 32-bit typed handle split 20/12 with the wrap arithmetic done, a generational pool with dense storage and O(1) swap-and-patch removal, the mesh subsystem converted end to end, and the cost measured: 0.13 ns per resolution, scene_object 40% smaller, golden still byte-identical.">

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

# AMENDED 2026-09-12: the STATE block is gone, and this script no longer
# stamps one. `STATE.md` became the sole resume key at 0.6 and CLAUDE.md §9
# was amended at 5.7 to retire the per-lesson block — but this builder
# predates that and was never updated, so re-running it would have RE-ADDED a
# STATE block to a page it was stripped from. The same amendment build_57.py
# received in 5.8.
TAIL = """

  <nav class="lesson-nav" aria-label="Lesson navigation (bottom)">
    <a class="prev-l" href="05-03-logging-and-errors.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.3 — Logging, Assertions, and Errors Without Exceptions</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-05-asset-system.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.5 — The Asset System v1</span>
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
    with open("scratch/l54_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l54_body_b.html") as fh:
        body_b = fh.read()

    page = HEAD + body_a + "\n" + body_b + TAIL

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
