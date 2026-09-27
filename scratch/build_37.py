#!/usr/bin/env python3
"""Assemble docs/lessons/03-07-specular-blinn-phong.html.

The prose lives in scratch/l37_body_{a,b}.html, the figures are computed by
scratch/figs_37.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.
"""
import html
import os
import re

OUT = "docs/lessons/03-07-specular-blinn-phong.html"

FIGURES = {
    "FIG1": ("l37_fig1.svg", "1",
             "A perfect mirror sends light out along exactly one direction, "
             "<strong>R</strong>. Roughening it spreads the outgoing light into a lobe around "
             "R — the shaded outline, whose radius in each direction is the fraction returned "
             "that way. Three eye positions are marked: on R the answer is 1.000; 22&#176; off "
             "it is 0.404; 44&#176; off it is 0.019. The lobe drawn here is "
             "cos<sup>12</sup>, a fairly polished surface, and it is clipped at the surface "
             "because directions below that are not places an eye can be."),
    "FIG2": ("l37_fig2.svg", "2",
             "The halfway vector <strong>h</strong> is not an approximation of anything. It is "
             "the <em>exact</em> orientation a microfacet would need in order to reflect the "
             "light straight into the eye — so it bisects <strong>l</strong> and "
             "<strong>v</strong>, 34&#176; from each. Asking how much of the surface is tilted "
             "that way becomes asking how far <strong>h</strong> is from the true normal "
             "<strong>n</strong>, and here <code>dot(n, h) = 0.9613</code>."),
    "FIG3": ("l37_fig3.svg", "3",
             "Why Blinn's exponent must be larger. <strong>R</strong> is fixed by the light, so "
             "moving the eye by one degree changes &#945; by one degree — but <strong>h</strong> "
             "bisects, so the same movement changes &#946; by only half a degree. Here "
             "&#945; = 40&#176; and &#946; = 20&#176;, exactly. Verified over a 33&#215;33 sweep: "
             "the worst |&#946; &#8722; &#945;/2| is 0.000018&#176;."),
    "FIG4": ("l37_fig4.svg", "4",
             "Phong's cut-off, drawn — and note what it is <em>not</em>. <strong>R</strong> is "
             "62&#176; from the normal, the same as <strong>l</strong>, so it is well above the "
             "surface; the folklore that the mirror ray dips into the ground is simply false. "
             "What happens instead is that <code>cos<sup>p</sup></code> only answers within "
             "90&#176; of <strong>R</strong>, and that half-space is not the visible one. The "
             "shaded wedge is the mismatch — 62&#176; wide, exactly the light's own angle from "
             "the normal — and <strong>v</strong> is inside it, 108&#176; from <strong>R</strong>. "
             "<code>dot(R, v)</code> is &#8722;0.309 and Phong returns nothing, while "
             "<strong>h</strong>, lying between two vectors that are both above the surface, "
             "gives Blinn 0.0142 — code 32/255 on screen."),
    "FIG5": ("l37_fig5.svg", "5",
             "The same two models at matched exponents (Phong p = 2, Blinn q = 8), plotted "
             "against the angle from the mirror ray rather than as lobes — because a lobe on a "
             "linear radial scale buries the tail, which is the whole point here. They are "
             "almost indistinguishable near the peak, which is what the 4&#215; rule buys; they "
             "come apart through the middle; and at 90&#176; Phong falls to exactly zero and "
             "stays there while Blinn is still returning 0.063, which is code 71/255 on screen. "
             "Figure 1 has the lobe-shaped view."),
}

LISTING_META = {
    "src/math/vec3.hpp": ("modified", "modified"),
    "src/gfx/light.hpp": ("modified", "modified"),
    "src/main.cpp": ("modified", "modified"),
}

# LISTING_SOURCE — added 2026-09-12, and without it THIS SCRIPT CANNOT RUN.
# ---------------------------------------------------------------------------
#
# Every path this page lists lives under `src/`, and Module 5's refactor RETIRED
# THAT WHOLE DIRECTORY — the engine's sources moved to engine/src and its headers
# to engine/include/engine. So re-running this builder failed at the first
# listing with a FileNotFoundError, which means the page had been unreproducible
# since Lesson 5.1.
#
# The contents are pinned from commit 656ea98 — the commit that SHIPPED this
# lesson — so the listings show the code as it stood when the lesson was written,
# which is what a lesson's listings are supposed to show. Same discipline as
# build_310.py and build_41.py (fixed 2026-09-11) and every builder from 5.8 on.
LISTING_SOURCE = {
    "src/math/vec3.hpp": "scratch/l37_src_math_vec3.hpp",
    "src/gfx/light.hpp": "scratch/l37_src_gfx_light.hpp",
    "src/main.cpp":      "scratch/l37_src_main.cpp",
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
<title>3.7 — Specular and Blinn-Phong · Build a Professional 3D Game Engine</title>
<meta name="description" content="The mirror direction and the halfway vector derived from scratch, why Blinn's exponent is four times Phong's, the exact condition under which Phong's highlight is cut off, and a measured account of why per-vertex evaluation is the wrong home for a highlight.">

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
     PAGE-SPECIFIC STYLES — Lesson 3.7 only.
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

# AMENDED 2026-09-12: the STATE block is gone, and this script no longer stamps
# one. `STATE.md` became the sole resume key at 0.6 and CLAUDE.md §9 was amended
# at 5.7 to retire the per-lesson block — but this builder predates that and was
# never updated, so re-running it would have RE-ADDED a STATE block to a page it
# was stripped from. The same amendment build_57.py received in 5.8.
TAIL = """

  <nav class="lesson-nav" aria-label="Lesson navigation (bottom)">
    <a class="prev-l" href="03-06-normals-and-lambert.html">
      <span class="dir">← Previous</span>
      <span class="ttl">3.6 — Normals and Lambert's Cosine Law</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="03-08-shading-models.html">
      <span class="dir">Next →</span>
      <span class="ttl">3.8 — Flat, Gouraud and Per-Pixel Shading</span>
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
    with open("scratch/l37_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l37_body_b.html") as fh:
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
