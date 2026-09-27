#!/usr/bin/env python3
"""Assemble docs/lessons/03-09-textures.html.

The prose lives in scratch/l39_body_{a,b}.html, the figures are computed by
scratch/figs_39.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.

Note the FIGURES table below. Placeholders are named by CONTENT, not by file
number, because the figures were authored in one order and appear in another —
and a figure numbered by its filename would leave "Figure 5" as the second
picture on the page.
"""
import html
import os
import re

OUT = "docs/lessons/03-09-textures.html"

FIGURES = {
    "FIG_SAMPLE": ("l39_fig1.svg", "1",
        "A texel is a sample, not a square. Above, the mental picture everyone starts with: "
        "four little tiles dividing the unit interval. Below, what the four numbers actually "
        "are &#8212; four measurements, at u = &#8539;, &#8540;, &#8541; and &#8542;. Read those "
        "positions off the axis: they are the odd EIGHTHS, not the quarters, because each "
        "sample sits in the middle of its own share of the interval. Every appearance of "
        "&#8722;0.5 later in this lesson is that sentence, written as arithmetic. Note also "
        "where the joined line stops: outside the first and last sample there is nothing to "
        "interpolate with, and deciding what happens there is a separate question with three "
        "answers (&#167;3.3)."),

    "FIG_ORIGIN": ("l39_fig5.svg", "2",
        "The same image and the same coordinate, under the two conventions that disagree. "
        "SDL_GPU puts (0,&nbsp;0) at the TOP left with v increasing downwards &#8212; quoted "
        "verbatim from <code>SDL_gpu.h</code> in &#167;3.2 &#8212; so (0.25, 0.25) lands in the "
        "red quadrant. Wavefront OBJ puts it at the BOTTOM left, so the identical pair of "
        "numbers lands in the blue one. Neither is wrong; they are simply different, and "
        "something has to reconcile them. That something is <code>v &#8594; 1 &#8722; v</code>, "
        "applied once, at import &#8212; not in the parser, which must not alter its input, and "
        "not in the sampler, which has to match the hardware."),

    "FIG_ADDRESS": ("l39_fig4.svg", "3",
        "What index i maps to on a four-texel image, for indices from &#8722;5 to 8. The dashed "
        "box is the image itself; everything outside it is what an address mode decides. Read "
        "the periods off the rows: <code>repeat</code> has period 4, <code>mirrored</code> has "
        "period 8 and doubles the edge texel at every fold (…2 3 3 2…), and <code>clamp</code> "
        "has no period at all. Note the negative side in particular &#8212; getting "
        "<code>repeat</code> right there is the difference between a seamless tiling and a "
        "one-texel stripe at the origin, because C++'s <code>%</code> gives &#8722;1 where 3 is "
        "wanted."),

    "FIG_HALF": ("l39_fig2.svg", "4",
        "A step image &#8212; eight texels, four white then four black &#8212; sampled "
        "bilinearly two ways, with the image itself drawn along the top. The solid curve "
        "subtracts the half texel and crosses half brightness at exactly u = 0.5000, which is "
        "where the boundary between the two blocks actually is. The dashed curve omits it and "
        "crosses at u = 0.4375. Lay a ruler between the two crossings: the gap is 0.0625 of the "
        "image, and on eight texels that is exactly HALF OF ONE TEXEL. Measured by bisection in "
        "<code>verify_39</code> &#167;C, which reports &#8722;0.5000."),

    "FIG_LERP": ("l39_fig3.svg", "5",
        "Bilinear interpolation, drawn as what it is: two interpolations along u produce a "
        "point on the top edge and a point on the bottom edge, and a third along v produces the "
        "sample. The four weights shown are for tu = 0.62, tv = 0.35 &#8212; add them up and "
        "they come to exactly 1.0000, which is what makes this an AVERAGE and therefore unable "
        "to brighten or darken a constant image. Doing v first and u second multiplies out to "
        "the identical four weights, so there is no convention to remember; "
        "<code>verify_39</code> &#167;D checks that over 200,000 random coordinates."),

    "FIG_MINIFY": ("l39_fig6.svg", "6",
        "Why bilinear filtering cannot fix minification, on a logarithmic scale. The curve is "
        "traced by ray casting, not estimated: how many texels one screen pixel covers on our "
        "floor, from 0.60 at the bottom of the frame to 62.46 two rows below the horizon. The "
        "flat dashed line is how many texels bilinear filtering actually reads &#8212; four, "
        "always, whatever the pixel covers. They cross near row 103, and past that the sampler "
        "is looking at a shrinking fraction of the truth: about 6.4% of it at the horizon. The "
        "gap is the aliasing, and mipmaps (Module 6) are what closes it."),
}

LISTING_META = {
    "src/gfx/texture.hpp": ("new", "new"),
    "src/gfx/texture.cpp": ("new", "new"),
    "src/gfx/mesh.hpp": ("modified", "modified"),
    "src/gfx/mesh.cpp": ("modified", "modified"),
    "src/gfx/raster.hpp": ("modified", "modified"),
    "src/gfx/raster.cpp": ("modified", "modified"),
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
# The contents are pinned from commit 06140d2 — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "src/gfx/texture.hpp": "scratch/l39_src_gfx_texture.hpp",
    "src/gfx/texture.cpp": "scratch/l39_src_gfx_texture.cpp",
    "src/gfx/mesh.hpp":    "scratch/l39_src_gfx_mesh.hpp",
    "src/gfx/mesh.cpp":    "scratch/l39_src_gfx_mesh.cpp",
    "src/gfx/raster.hpp":  "scratch/l39_src_gfx_raster.hpp",
    "src/gfx/raster.cpp":  "scratch/l39_src_gfx_raster.cpp",
    "src/main.cpp":        "scratch/l39_src_main.cpp",
    "CMakeLists.txt":      "scratch/l39_CMakeLists.txt",
}

LISTING_LANG = {
    "CMakeLists.txt": ("cmake", "CMake"),
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
<title>3.9 — Texture Mapping and Bilinear Filtering · Build a Professional 3D Game Engine</title>
<meta name="description" content="A texel is a sample, not a square — and the half-texel offset, the three address modes, bilinear as a double lerp, the sRGB decode order, and the uv origin all follow from that one sentence. With a 1:1 blit that is bit-identical to its source.">

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

<!-- ==========================================================================
     PAGE-SPECIFIC STYLES — Lesson 3.9 only.
     Deliberately OUTSIDE the SHARED-CSS markers: apply-shared.py owns what is
     between them, and page-local rules must survive a re-stamp.
     ========================================================================== -->
<style>
/* A fixed-dark text fill, for numerals sitting on a PALE swatch. The shared sheet
   provides `.t-inv` (fixed white) for the opposite case and gives the reason:
   the shape underneath is the same colour in both themes, so its label must not
   follow the theme's ink. This is that rule's other half. Chosen per swatch by
   relative luminance in figs_39.py, not by eye. */
figure.dia svg .t-onlight { fill: #22242a; }

/* The widget's canvas, and the controls above it. Kept minimal — everything
   inside the canvas is drawn in JS and reads its colours from the theme's own
   custom properties, so dark mode tracks without a second palette. */
#w39 canvas {
  display: block;
  width: 100%;
  max-width: 640px;
  height: auto;
  margin: 0.6rem 0;
  touch-action: none;          /* pointer drag must not scroll the page */
  cursor: crosshair;
}
#w39 .widget-controls {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.35rem 0.9rem;
  font-family: var(--font-ui);
  font-size: 0.85rem;
}
#w39 .widget-controls .sep {
  display: inline-block;
  width: 1px;
  height: 1em;
  background: var(--rule);
}
#w39 .widget-readout {
  font-family: var(--font-mono);
  font-size: 0.8rem;
  color: var(--ink-soft);
  margin: 0.2rem 0 0;
}
</style>
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
    <a class="prev-l" href="03-08-shading-models.html">
      <span class="dir">← Previous</span>
      <span class="ttl">3.8 — Flat, Gouraud and Per-Pixel Shading</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="03-10-profiling-capstone.html">
      <span class="dir">Next →</span>
      <span class="ttl">3.10 — Profiling, and the Module 3 Capstone</span>
    </a>
  </nav>

  <footer class="foot">
    <p>
      Build a Professional 3D Game Engine ·
      <a href="../index.html">Contents</a> ·
      <a href="../conventions.html">Conventions</a> ·
      <a href="../math-toolbox.html">Math Toolbox</a> ·
      <a href="../cpp-style.html">C++ Style</a>
    </p>
    <p>MIT licensed. Copyright © 2026 digster.</p>
  </footer>
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
    with open("scratch/l39_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l39_body_b.html") as fh:
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
