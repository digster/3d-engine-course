#!/usr/bin/env python3
"""Assemble docs/lessons/04-08-porting-the-scene.html.

The prose lives in scratch/l48_body_{a,b}.html, the figures are computed by
scratch/figs_48.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.

Figure placeholders are named by CONTENT, not by file number, because the figures
were authored in one order and appear in another. The number in each tuple is the
one the reader sees, and it follows PAGE order — check it against where the
@@FIG_*@@ markers actually land in the body.
"""
import os
import re

OUT = "docs/lessons/04-08-porting-the-scene.html"

FIGURES = {
    "FIG_PIPELINE": ("l48_fig1.svg", "1",
        "Every stage of the software rasterizer, and what became of it. Only five fates are "
        "possible and naming them is most of what a port is. <strong>Two stages are literally "
        "the same C++ function called from both sides</strong> &#8212; "
        "<code>parent_from_local</code> and <code>normal_matrix</code>, neither of which knows a "
        "GPU exists. Eight became fixed-function hardware, which is the work of Modules 2 and 3 "
        "turning from code you run into knowledge you have. And two could not cross at all: a "
        "cull mode and a material are pipeline state, and everything structural about this "
        "lesson follows from those two rows."),

    "FIG_RATES": ("l48_fig2.svg", "2",
        "Group data by <strong>how often it changes</strong>, not by what it is about. The "
        "camera and the lamp have nothing to do with each other and share a push because both "
        "change once a frame; the model matrix and the albedo share one because both change once "
        "a draw. This frame moves <strong>560 bytes</strong> of uniform traffic for a "
        "three-object scene &#8212; and would move the same 560 if each object had a million "
        "triangles, because per-draw overhead scales with the number of objects and not with "
        "their size."),

    "FIG_NORMALS": ("l48_fig3.svg", "3",
        "Three faces meet at a cube&#8217;s corner and each wants its own normal; a vertex "
        "carries one. So <strong>flat shading requires unshared vertices</strong> &#8212; a "
        "cube&#8217;s 8 positions become 36, an icosahedron&#8217;s 12 become 60 &#8212; and the "
        "index buffer that comes out is 0,&nbsp;1,&nbsp;2,&nbsp;3,&nbsp;… with no sharing left "
        "to express. None of the built-in meshes carries an authored normal, and a vertex shader "
        "&#8212; which sees one vertex and cannot see the triangle it belongs to &#8212; cannot "
        "compute the face normal the software rasterizer&#8217;s per-triangle loop did. The "
        "fallback moves into the importer, and Lesson 3.8&#8217;s runtime toggle becomes a "
        "build-time decision."),

    "FIG_NORMAL_MATRIX": ("l48_fig6.svg", "4",
        "The same scene under the correct inverse transpose and under the model matrix used as a "
        "normal matrix. On the two non-uniformly scaled <em>boxes</em>, "
        "<strong>zero pixels change</strong> &#8212; not &#8220;almost none&#8221;, but exactly "
        "zero, because a box&#8217;s model-space normals are its own axes and a diagonal scale "
        "sends an axis to a multiple of itself, leaving only a length that "
        "<code>normalize</code> throws away. Squash the icosahedron, whose twenty face normals "
        "are not axes, and <strong>2,160 pixels change by up to 143 codes</strong>. A test scene "
        "made of crates proves nothing about your normal handling."),

    "FIG_COMPARE": ("l48_fig4.svg", "5",
        "Same scene, same camera, same light, same geometry &#8212; literally the same "
        "<code>mesh_data</code>, imported once and handed to both renderers. <strong>87% of the "
        "pixels both drew are byte-identical</strong>; 8% differ by one code. The 2.5% that "
        "differ badly are all on silhouettes, where the two rasterizers made opposite coverage "
        "decisions, and a large disagreement at a boundary is one pixel of coverage difference "
        "wearing a large number. The hundred-odd coverage pixels form a one-pixel sliver around "
        "each object: same fill rule, different sub-pixel resolution."),

    "FIG_SRGB": ("l48_fig5.svg", "6",
        "Why a flat, untextured floor reports <em>100% of pixels differ</em> and it means "
        "nothing. Everything above linear 0.006 agrees exactly; every disagreement is in the "
        "darks, where the sRGB curve&#8217;s slope is more than twelve times its slope near "
        "white and a linear value crosses a code boundary that much faster. Both sides "
        "approximate the same curve and neither is wrong &#8212; sRGB is specified as a "
        "function, not as a table of 256 bytes. So a per-pixel comparison across this boundary "
        "has a <strong>floor of one code</strong>, and the first hypothesis about it (that the "
        "encoders differed everywhere) was refuted by the sweep that found where they actually "
        "do."),
}

LISTING_META = {
    "src/gfx/gpu_scene.hpp": ("new", "new"),
    "src/gfx/gpu_scene.cpp": ("new", "new"),
    "shaders/scene.vert.hlsl": ("new", "new"),
    "shaders/scene.frag.hlsl": ("new", "new"),
    "shaders/matrix_probe.frag.hlsl": ("new", "new"),
    "src/gfx/mesh.hpp": ("modified", "modified"),
    "src/gfx/mesh.cpp": ("modified", "modified"),
    "src/gfx/gpu_uniform.hpp": ("modified", "modified"),
    "src/gfx/gpu_present.hpp": ("modified", "modified"),
    "src/gfx/gpu_present.cpp": ("modified", "modified"),
    "src/main.cpp": ("modified", "modified"),
    "CMakeLists.txt": ("modified", "modified"),
}

# LISTING_SOURCE — added 2026-09-12, and without it THIS SCRIPT CANNOT RUN.
# ---------------------------------------------------------------------------
#
# The paths this page lists live under `src/`, and Module 5's refactor RETIRED
# THAT WHOLE DIRECTORY — the engine's sources moved to engine/src and its
# headers to engine/include/engine. So re-running this builder died at the
# first listing with a FileNotFoundError, which means the page had been
# unreproducible since Lesson 5.1 and nobody had noticed, because nobody had
# needed to rebuild it.
#
# Paths that DO still exist (shaders, CMakeLists.txt) are pinned too, and for
# the opposite reason: reading them live is silent rather than fatal, so every
# later lesson's edits leaked backwards into this page.
#
# The contents are pinned from commit ae353c3 — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "src/gfx/gpu_scene.hpp":          "scratch/l48_src_gfx_gpu_scene.hpp",
    "src/gfx/gpu_scene.cpp":          "scratch/l48_src_gfx_gpu_scene.cpp",
    "shaders/scene.vert.hlsl":        "scratch/l48_shaders_scene.vert.hlsl",
    "shaders/scene.frag.hlsl":        "scratch/l48_shaders_scene.frag.hlsl",
    "shaders/matrix_probe.frag.hlsl": "scratch/l48_shaders_matrix_probe.frag.hlsl",
    "src/gfx/mesh.hpp":               "scratch/l48_src_gfx_mesh.hpp",
    "src/gfx/mesh.cpp":               "scratch/l48_src_gfx_mesh.cpp",
    "src/gfx/gpu_uniform.hpp":        "scratch/l48_src_gfx_gpu_uniform.hpp",
    "src/gfx/gpu_present.hpp":        "scratch/l48_src_gfx_gpu_present.hpp",
    "src/gfx/gpu_present.cpp":        "scratch/l48_src_gfx_gpu_present.cpp",
    "src/main.cpp":                   "scratch/l48_src_main.cpp",
    "CMakeLists.txt":                 "scratch/l48_CMakeLists.txt",
}

LISTING_LANG = {
    "CMakeLists.txt": ("cmake", "CMake"),
    "shaders/scene.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/scene.frag.hlsl": ("hlsl", "HLSL"),
    "shaders/matrix_probe.frag.hlsl": ("hlsl", "HLSL"),
}


def esc(text):
    """Escape for a <pre><code> block, matching the rest of the course's pages."""
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
<title>4.8 — Porting the Module-3 Scene · Build a Professional 3D Game Engine</title>
<meta name="description" content="Module 3's scene, moved to the GPU — and then audited. Fifteen pipeline stages and what became of each. Why a material cannot ride on a triangle. Normals as data. The normal-matrix bug that is invisible on boxes by construction. And a per-pixel comparison of two renderers that agree to one float ULP.">

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
    <a class="prev-l" href="04-07-textures-and-depth.html">
      <span class="dir">← Previous</span>
      <span class="ttl">4.7 — Textures, Samplers, and Depth</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="04-09-renderdoc.html">
      <span class="dir">Next →</span>
      <span class="ttl">4.9 — Debugging a Frame with RenderDoc</span>
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
    with open("scratch/l48_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l48_body_b.html") as fh:
        body_b = fh.read()

    page = HEAD + body_a + "\n" + body_b + TAIL

    for key in FIGURES:
        page = page.replace(f"@@{key}@@", figure(key))

    def sub_listing(m):
        return listing(m.group(1))

    page = re.sub(r"@@LISTING:([^@]+)@@", sub_listing, page)

    left = re.findall(r"@@[A-Z0-9_:./-]+@@", page)
    if left:
        raise SystemExit(f"unsubstituted placeholders: {sorted(set(left))}")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as fh:
        fh.write(page)
    print(f"wrote {OUT}  ({len(page):,} bytes, {page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
