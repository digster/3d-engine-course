#!/usr/bin/env python3
"""Assemble docs/lessons/06-05-material-system.html.

Same pipeline as build_64.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l65_body_{a,b,c}.html, figures from scratch/figs_65.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-05-material-system.html"

FIGURES = {
    "FIG_HOLE": ("l65_fig1.svg", "1",
        "The argument for this lesson, and the codebase made it without anyone acting on it. Three "
        "structs each accumulated fields describing THE SURFACE rather than the object &#8212; "
        "highlighted here &#8212; and two of them said so in their own comments: "
        "<code>scene_object</code>&#8217;s &#8220;the second kind has a name: a material&#8221;, "
        "<code>fill_style</code>&#8217;s &#8220;two structs wearing one name&#8221;. The third is "
        "different in kind and is the strongest evidence of the three: <strong>ecs_swarm is a "
        "DEMO</strong>, restricted since 5.1 to the engine&#8217;s public headers, and it declared "
        "the type itself because the engine offered none. A consumer that cannot reach inside your "
        "library and has to invent one of your types is measuring what your API does not provide. "
        "Lesson 6.4 then priced the absence at <strong>44 call sites</strong> to change one surface "
        "parameter."),

    "FIG_SPLIT": ("l65_fig2.svg", "2",
        "The whole design decision, and it is settled by the hardware rather than by taste. "
        "<strong>Can this be a number in a buffer?</strong> If yes it is per-draw data: the "
        "fragment stage reads it, the GPU pushes it as a uniform, and two objects differing only "
        "in it draw back to back with no state change between them. If no it is PIPELINE STATE, "
        "baked into a pipeline object at creation, and two objects differing in it need two "
        "pipelines and a sort between them. Lesson 4.8 already paid for this knowledge &#8212; the "
        "port to SDL_GPU produced three pipeline objects and a draw-list sort for exactly this "
        "reason, and the <code>pipeline_binds</code> against <code>ideal_pipeline_binds</code> "
        "statistic exists to price getting the sort wrong. <strong>A material that swallowed the "
        "right-hand column would be a material that cannot be a uniform</strong>, which is not a "
        "material at all."),

    "FIG_HANDLE": ("l65_fig3.svg", "3",
        "Two true statements that pull in opposite directions, and the resolution is a named step "
        "rather than a compromise. A stored reference must survive a reallocation and be "
        "CHECKABLE, which a raw pointer is not &#8212; measured in verify_65 &#167;C, where 64 "
        "insertions move the pool from 0x928c03500 to 0x928c16000 and the handle does not care, "
        "because it names a SLOT rather than an address. But a fill loop cannot afford a bounds "
        "check and a generation compare PER PIXEL, for a value that could not have changed between "
        "two pixels of one triangle. So: the handle stores, the pointer is used, and "
        "<code>bind_albedo</code> converts once per DRAW. Get the boundary wrong in one direction "
        "and the scene dangles the first time a pool grows; wrong in the other and every pixel pays "
        "for a lookup."),

    "FIG_SHARING": ("l65_fig4.svg", "4",
        "Why the pool exists, with the bytes measured &#8212; and then the better argument. "
        "Ninety-six ring drones cycle through six tints and all share one roughness, so the scene "
        "holds <strong>six materials and ninety-six references</strong>. A copy in each entity "
        "stores the same thirty-six bytes sixteen times: 3,456 bytes against 600, a "
        "<strong>5.8&#215;</strong> reduction (verify_65 &#167;E). <strong>But the bytes are the "
        "side effect.</strong> With copies, &#8220;make the drones rougher&#8221; is a loop over "
        "the registry that must find every entity carrying that appearance and hope none was "
        "missed; with handles it is one write, which &#167;E checks reaches all ninety-six. Note "
        "what this means for the rule: <code>scene_object</code> still holds its material BY VALUE, "
        "because it has exactly one. <strong>The rule is about sharing, not about size</strong> "
        "&#8212; and &#8220;use handles for big things&#8221; gets this case backwards."),

    "FIG_CLOSED": ("l65_fig5.svg", "5",
        "The correction this lesson makes, and it took building the material to see it. "
        "<code>scene_object::closed</code> carried a comment from Lesson 3.4 predicting it would "
        "move onto the material in Module 6 &#8212; &#8220;because cull mode is pipeline state and "
        "pipeline state is what a material IS&#8221;. That is wrong twice: a material is "
        "explicitly NOT pipeline state (Figure 2), and <code>closed</code> is not cull mode anyway. "
        "It is a claim about the GEOMETRY, true of a cube whatever colour you paint it, which "
        "<code>validate()</code> already counts from the mesh&#8217;s own edges. What belongs to "
        "the caller is the INTENT, because a closed mesh may still be drawn two-sided deliberately. "
        "Two questions, two owners, and <code>cull_of</code> is the single place the rule &#8212; "
        "culling is only valid on closed geometry &#8212; is written down. <strong>A comment "
        "predicting a future design cannot be tested</strong>, which is why this one repeated for "
        "three modules."),
}

LISTING_META = {
    "engine/include/engine/gfx/material.hpp": ("new", "new"),
    "engine/include/engine/gfx/cull.hpp":     ("new", "new"),
    "engine/include/engine/gfx/scene.hpp":    ("modified", "modified"),
    "scratch/verify_65.cpp":                  ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/raster.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/texture.hpp": ("modified", "modified"),
    "engine/src/gfx/soft_renderer.cpp": ("modified", "modified"),
    "demos/common/demo_scene.hpp": ("modified", "modified"),
    "demos/hello_cube/main.cpp": ("modified", "modified"),
    "demos/sandbox/main.cpp": ("modified", "modified"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gpu_uniform.hpp": ("modified", "modified"),
    "demos/common/demo_scene.cpp": ("modified", "modified"),
    "demos/ecs_swarm/main.cpp": ("modified", "modified"),
}

LISTING_LANG = {}

# NOTHING PINNED YET, and the warning has a four-lesson track record: build_57.py
# stamped a retired STATE block, build_58.py spliced a newer demo, build_510.py
# needed TWO pins, and 6.1 through 6.4 were each pinned by the FOLLOWING lesson
# before a line of it was written — the rebuild diff coming out to exactly the nav
# lines that were meant to move every time.
#
# 6.4's pinning ALSO caught something the rule was not aimed at: three stale
# "Lesson 6.7" references that had been fixed in the sources after the last build
# and never rebuilt into the page. The generalisation is wider than pinning —
# ANY GENERATED ARTIFACT NEEDS A REGENERATE-AND-DIFF AFTER THE LAST EDIT TO ITS
# INPUTS, not only at the moment you remember to pin.
#
# PIN EVERY FILE THIS PAGE LISTS THAT A LATER LESSON TOUCHES. Lesson 6.6 loads
# glTF, and this page lists `material.hpp` — the file 6.6 is most likely to grow
# (a material needs a name and a cache once it comes from a file) — and
# `scene.hpp`. `cull.hpp` is stable; nothing plausible edits a three-value enum.
#
#     git show <6.5 commit>:<path> > scratch/l65_<name>
#     git show <6.5 commit>:<path> | diff - scratch/l65_<name>
#
# And take `scratch/verify_65.cpp`'s pin EARLY — it is gitignored, so its only
# provenance is a working-tree copy, which cannot be recovered afterwards.
LISTING_SOURCE = {
    # PINNED at the start of the 6.6 session, before a line of glTF was written.
    # Verified byte-identical against the 6.5 commit (ebb3199) with
    # `git show ebb3199:<path> | diff - <pin>`; verify_65.cpp is gitignored, so
    # its pin is a working-tree copy taken before any edit.
    "engine/include/engine/gfx/material.hpp": "scratch/l65_material.hpp",
    "engine/include/engine/gfx/scene.hpp":    "scratch/l65_scene.hpp",
    "scratch/verify_65.cpp":                  "scratch/l65_verify_65.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/raster.hpp": "scratch/l65_engine_include_engine_gfx_raster.hpp",
    "engine/include/engine/gfx/texture.hpp": "scratch/l65_engine_include_engine_gfx_texture.hpp",
    "engine/src/gfx/soft_renderer.cpp": "scratch/l65_engine_src_gfx_soft_renderer.cpp",
    "demos/common/demo_scene.hpp": "scratch/l65_demos_common_demo_scene.hpp",
    "demos/hello_cube/main.cpp": "scratch/l65_demos_hello_cube_main.cpp",
    "demos/sandbox/main.cpp": "scratch/l65_demos_sandbox_main.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gpu_uniform.hpp": "scratch/l65_engine_include_engine_gfx_gpu_uniform.hpp",
    "demos/common/demo_scene.cpp": "scratch/l65_demos_common_demo_scene.cpp",
    "demos/ecs_swarm/main.cpp": "scratch/l65_demos_ecs_swarm_main.cpp",
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
<title>6.5 — A Material System · Build a Professional 3D Game Engine</title>
<meta name="description" content="Lesson 6.4 changed one surface parameter and had to edit forty-four call sites, which is what a missing type measures. Three structs had independently grown the same hole, one of them a demo that invented the type because the engine offered none. Builds the material around a rule the hardware sets rather than taste - can this be a number in a buffer? - stores texture references as handles with the resolve as a named per-draw step, derives the textured flag so it cannot contradict itself, measures sharing at 5.8x across the swarm, and corrects two comments that had been repeating for three modules, including a compile-time argument that did not survive being measured. A refactor, so the reference render is byte-identical.">

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
    <a class="prev-l" href="06-04-cook-torrance.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.4 — Cook–Torrance PBR, Derived</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-06-gltf.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.6 — glTF 2.0 Loading</span>
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
        with open(f"scratch/l65_body_{name}.html") as fh:
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
