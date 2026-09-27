#!/usr/bin/env python3
"""Assemble docs/lessons/04-07-textures-and-depth.html.

The prose lives in scratch/l47_body_{a,b}.html, the figures are computed by
scratch/figs_47.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.

Figure placeholders are named by CONTENT, not by file number, because the figures
were authored in one order and appear in another.
"""
import os
import re

OUT = "docs/lessons/04-07-textures-and-depth.html"

FIGURES = {
    "FIG_SAMPLER": ("l47_fig4.svg", "1",
        "On the CPU in Lesson 3.9 the filter and the address mode were arguments to a function. "
        "On the GPU they are an <strong>object</strong>, created once and bound alongside the "
        "texture &#8212; the fourth time this module has made that move, after pipelines, vertex "
        "layouts and uniform blocks, and always for the same reason. The separation into two "
        "objects is better than either OpenGL&#8217;s fusion or our own: one image can be read "
        "three ways in one frame by binding it with three samplers, and one sampler serves every "
        "texture in a material system. Below, the fields the object has that the call had no room "
        "for &#8212; each one a decision the hardware specialises for."),

    "FIG_DEPTH_CURVE": ("l47_fig1.svg", "2",
        "Depth-buffer z against distance, for our frustum of near 0.3 and far 100. The curve is a "
        "hyperbola, not a line, because the perspective divide puts 1/<em>d</em> into the stored "
        "value &#8212; so <strong>70% of the entire representable range is spent in the first "
        "metre</strong> and the remaining ninety-nine metres share the rest. Differentiating "
        "gives dz/dd proportional to n/d&#178;: double the distance and you have a quarter of the "
        "resolution. The box at the bottom is that result turned into the number an engine "
        "actually needs."),

    "FIG_DEPTH_TABLE": ("l47_fig2.svg", "3",
        "What each format can really separate, found by drawing a far surface and then a near one "
        "and bisecting on the gap. The tick marks the formula&#8217;s prediction for D16, which "
        "the measurement tracks to within a few per cent from fifty microns at one metre to "
        "forty centimetres at ninety &#8212; theory and hardware agreeing closely enough that the "
        "model is not an approximation of what the hardware does, it <em>is</em> what it does. "
        "Note the support table above: <strong>D24_UNORM, the format a desktop renderer would "
        "have hard-coded, is not available on this device.</strong>"),

    "FIG_REVERSED": ("l47_fig3.svg", "4",
        "A float keeps its precision near zero, and the ordinary depth mapping puts the far plane "
        "there &#8212; exactly where 1/d&#178; has already thrown the resolution away, so the two "
        "effects compound. Reversing the mapping makes them very nearly cancel instead. Measured: "
        "<strong>D32_FLOAT improves 180&#215;</strong> at ninety metres, from 2.2 mm to 0.012. "
        "And the control that makes it believable &#8212; <strong>D16_UNORM gains nothing at "
        "all</strong>, because evenly spaced codes do not care which end is which, which is "
        "precisely what the theory predicts."),

    "FIG_SRGB": ("l47_fig5.svg", "5",
        "The test image carries a different colour in each corner so that orientation can be read "
        "by a program rather than eyeballed. Sampled through a <code>_UNORM</code> texture the "
        "four corners match the file <strong>byte for byte</strong> &#8212; a much stronger claim "
        "than &#8220;it looked right&#8221;, since the decoder, the upload, the sampler and the "
        "readback would all have had to agree. Through an <code>_SRGB</code> texture the same "
        "texels come back darker by exactly the sRGB decode (222 &#8594; 186), performed by the "
        "sampler for free and <em>before</em> the filter &#8212; which is the ordering Lesson 3.9 "
        "had to construct by hand."),

    "FIG_SCENE": ("l47_fig6.svg", "6",
        "The same seven tori, the same camera, the same draw call &#8212; differing only in "
        "<code>enable_depth_test</code>, <code>enable_depth_write</code> and "
        "<code>compare_op</code>, plus one attachment on the render pass. Without them whichever "
        "instance was drawn later wins, so background tori punch through the foreground one and "
        "<strong>18.9% of the covered pixels are wrong</strong>. Drawn untextured on purpose: a "
        "checkerboard on both panels would make the eye hunt for the difference, and the "
        "difference is the point."),
}

LISTING_META = {
    "src/gfx/image.hpp": ("new", "new"),
    "src/gfx/image.cpp": ("new", "new"),
    "src/gfx/gpu_texture.hpp": ("new", "new"),
    "src/gfx/gpu_texture.cpp": ("new", "new"),
    "shaders/depth_probe.vert.hlsl": ("new", "new"),
    "shaders/depth_probe.frag.hlsl": ("new", "new"),
    "shaders/texture_probe.frag.hlsl": ("new", "new"),
    "shaders/mesh.frag.hlsl": ("modified", "modified"),
    "src/gfx/gpu_uniform.hpp": ("modified", "modified"),
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
# The contents are pinned from commit 3ad3d96 — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "src/gfx/image.hpp":               "scratch/l47_src_gfx_image.hpp",
    "src/gfx/image.cpp":               "scratch/l47_src_gfx_image.cpp",
    "src/gfx/gpu_texture.hpp":         "scratch/l47_src_gfx_gpu_texture.hpp",
    "src/gfx/gpu_texture.cpp":         "scratch/l47_src_gfx_gpu_texture.cpp",
    "shaders/depth_probe.vert.hlsl":   "scratch/l47_shaders_depth_probe.vert.hlsl",
    "shaders/depth_probe.frag.hlsl":   "scratch/l47_shaders_depth_probe.frag.hlsl",
    "shaders/texture_probe.frag.hlsl": "scratch/l47_shaders_texture_probe.frag.hlsl",
    "shaders/mesh.frag.hlsl":          "scratch/l47_shaders_mesh.frag.hlsl",
    "src/gfx/gpu_uniform.hpp":         "scratch/l47_src_gfx_gpu_uniform.hpp",
    "src/main.cpp":                    "scratch/l47_src_main.cpp",
    "CMakeLists.txt":                  "scratch/l47_CMakeLists.txt",
}

LISTING_LANG = {
    "CMakeLists.txt": ("cmake", "CMake"),
    "shaders/mesh.frag.hlsl": ("hlsl", "HLSL"),
    "shaders/depth_probe.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/depth_probe.frag.hlsl": ("hlsl", "HLSL"),
    "shaders/texture_probe.frag.hlsl": ("hlsl", "HLSL"),
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
<title>4.7 — Textures, Samplers, and Depth · Build a Professional 3D Game Engine</title>
<meta name="description" content="The depth test and texture mapping ported from Modules 3 — both nearly renames, because they were designed for this. Then the parts that are not: why depth precision falls off as the square of distance, what each format can really separate, reversed-Z measured at 180x, and the one enum that decides whether your lighting is correct.">

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
    <a class="prev-l" href="04-06-uniforms.html">
      <span class="dir">← Previous</span>
      <span class="ttl">4.6 — Uniform Data and the Matrix Upload</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="04-08-porting-the-scene.html">
      <span class="dir">Next →</span>
      <span class="ttl">4.8 — Porting the Module-3 Scene</span>
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
    with open("scratch/l47_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l47_body_b.html") as fh:
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
