#!/usr/bin/env python3
"""Assemble docs/lessons/03-08-shading-models.html.

The prose lives in scratch/l38_body_{a,b}.html, the figures are computed by
scratch/figs_38.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.
"""
import html
import os
import re

OUT = "docs/lessons/03-08-shading-models.html"

FIGURES = {
    "FIG1": ("l38_fig1.svg", "1",
             "The two axes, and the six cells they span. The shaded row is the finding: with a "
             "FACE normal the normal is constant across the triangle, so all three evaluation "
             "points give the identical picture &#8212; 0 pixels differ, not &#8220;close&#8221;. "
             "The vertex row does not collapse: 5,285 and 4,344 pixels respectively. And the "
             "note below the grid is the condition that first draft of this lesson missed &#8212; "
             "switch on a view-dependent specular term and the top row separates too, because "
             "<code>to_eye</code> varies across a face even when the normal does not."),
    "FIG2": ("l38_fig2.svg", "2",
             "The same triangle, the same equation, three sampling densities. Each red dot is "
             "one call to <code>shade()</code>: once at the centroid, three times at the "
             "corners, or once per covered fragment. Nothing else differs &#8212; not the light, "
             "not the material, not the normal matrix. Naming the three modes after where the "
             "dots go is the entire content of &#167;2.2."),
    "FIG3": ("l38_fig3.svg", "3",
             "Why an interpolated normal has to be renormalised. Two unit normals have their "
             "tips on a circle; the straight line between those tips is a CHORD, and a chord "
             "passes inside the circle. Every interpolated value therefore lies short of unit "
             "length &#8212; exactly <code>cos(&#952;/2)</code>, which at 60&#176; apart is "
             "0.86603, or 13.4% dim. Note the shape of the error: zero at the corners, worst in "
             "the middle, which is the chord signature this course has now met four times."),
    "FIG4": ("l38_fig4.svg", "4",
             "Gouraud shading's intensity ramp across six facets. The solid curve is the truth "
             "&#8212; what per-pixel evaluates. The dashed line samples it at each facet "
             "boundary and joins the samples with straight segments; the two agree "
             "<em>exactly</em> at every knot, so nothing is discontinuous. What is "
             "discontinuous is the SLOPE, and the human visual system exaggerates a slope break "
             "into an apparent stripe &#8212; a Mach band, which is in your eye and not in the "
             "framebuffer. That is why more colour precision does not remove it."),
    "FIG5": ("l38_fig5.svg", "5",
             "The measured cost of per-pixel shading relative to Gouraud, against pixels covered "
             "per triangle, same mesh throughout and resolution swept. The folklore &#8212; "
             "&#8220;per-vertex is cheaper&#8221; &#8212; assumes a triangle covers many pixels. "
             "Below about three it does not, there are more vertices than covered pixels, and "
             "per-pixel is the CHEAPER option at 0.91&#215;. The curve asymptotes at 2.15&#215;, "
             "which is the honest steady-state price."),
}

LISTING_META = {
    "src/gfx/clip.hpp": ("modified", "modified"),
    "src/gfx/clip.cpp": ("modified", "modified"),
    "src/gfx/raster.hpp": ("modified", "modified"),
    "src/gfx/raster.cpp": ("modified", "modified"),
    "src/main.cpp": ("modified", "modified"),
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
# The contents are pinned from commit 7ec993a — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "src/gfx/clip.hpp":   "scratch/l38_src_gfx_clip.hpp",
    "src/gfx/clip.cpp":   "scratch/l38_src_gfx_clip.cpp",
    "src/gfx/raster.hpp": "scratch/l38_src_gfx_raster.hpp",
    "src/gfx/raster.cpp": "scratch/l38_src_gfx_raster.cpp",
    "src/main.cpp":       "scratch/l38_src_main.cpp",
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
    return (
        '  <figure class="listing">\n'
        '    <figcaption>\n'
        f'      <span class="path">{path}</span>\n'
        f'      <span class="tag {tag}">{word}</span>\n'
        '      <span class="lang" data-lang="cpp">C++</span>\n'
        '    </figcaption>\n'
        f'    <pre><code class="lang-cpp">{esc(body)}</code></pre>\n'
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
<title>3.8 — Flat, Gouraud and Per-Pixel Shading · Build a Professional 3D Game Engine</title>
<meta name="description" content="Where a normal comes from and where the shading equation is evaluated are two independent questions. The 2x3 grid they span, which cells coincide and why, Gouraud named, Mach bands, and a measured cost crossover that contradicts the folklore.">

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
     PAGE-SPECIFIC STYLES — Lesson 3.8 only.
     Deliberately OUTSIDE the SHARED-CSS markers: apply-shared.py owns what is
     between them, and page-local rules must survive a re-stamp.

     These name the vocabulary this lesson's figures use. They are CLASSES rather
     than fill/stroke attributes because `figure.dia svg text` in the shared sheet
     is CSS, and CSS always beats an SVG presentation attribute — an inline fill
     on a <text> is silently ignored. (Shapes are unaffected.)
     ========================================================================== -->
<style>
figure.dia svg .surf    { stroke: var(--dia-ink); }
figure.dia svg .hatch   { stroke: var(--dia-ink-soft); }
figure.dia svg .normal  { stroke: var(--dia-ink-soft); }
figure.dia svg .arc     { stroke: var(--dia-ink-soft); }
figure.dia svg .cut     { stroke: var(--verify-bd); }
figure.dia svg .facet   { stroke: var(--dia-ink); }
figure.dia svg .axis-r  { stroke: var(--note-bd); }
figure.dia svg .axis    { stroke: var(--dia-ink); }

/* The four directions this lesson keeps referring to, each with one colour used
   consistently in every figure: the light amber, the mirror ray blue, the eye
   violet, the halfway vector green. Deliberately none of them red/green/blue as
   used for the x/y/z axes — conventions.html §8 reserves those, and no figure
   here draws a coordinate axis. */
figure.dia svg .vec-l   { stroke: var(--warn-bd); }
figure.dia svg .vec-r   { stroke: var(--note-bd); }
figure.dia svg .vec-v   { stroke: var(--pitfall-bd); }
figure.dia svg .vec-h   { stroke: var(--ok-bd); }

figure.dia svg .lbl     { font-size: 15px; font-weight: 700; }
figure.dia svg .lbl-n   { fill: var(--ok-bd); }
figure.dia svg .lbl-n   { fill: var(--dia-ink-soft); }
figure.dia svg .lbl-l   { fill: var(--warn-bd); }
figure.dia svg .lbl-r   { fill: var(--note-bd); }
figure.dia svg .lbl-v   { fill: var(--pitfall-bd); }
figure.dia svg .lbl-h   { fill: var(--ok-bd); }

figure.dia svg .lobe,
figure.dia svg .lobe-b,
figure.dia svg .lobe-p  { stroke-linejoin: round; }

/* The widget's cut-off warning. `.widget svg text` in the shared sheet sets the
   fill, so this has to be a class for the same reason the diagram labels do. */
.widget svg .t-warn     { fill: var(--verify-bd); font-weight: 700; }
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
    <a class="prev-l" href="03-07-specular-blinn-phong.html">
      <span class="dir">← Previous</span>
      <span class="ttl">3.7 — Specular and Blinn-Phong</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="03-09-textures.html">
      <span class="dir">Next →</span>
      <span class="ttl">3.9 — Texture Mapping and Bilinear Filtering</span>
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
    with open("scratch/l38_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l38_body_b.html") as fh:
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
