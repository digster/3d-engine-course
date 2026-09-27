#!/usr/bin/env python3
"""Assemble docs/lessons/03-10-profiling-capstone.html.

The prose lives in scratch/l310_body_{a,b,c}.html, the figures are computed by
scratch/figs_310.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.

Figure placeholders are named by CONTENT, not by file number, because the figures
were authored in one order and appear in another.
"""
import os
import re

OUT = "docs/lessons/03-10-profiling-capstone.html"

# RENUMBERED 2026-09-11 so that the displayed numbers, the filenames and the
# PAGE ORDER all agree. They did not: this page showed its figures in one order
# and numbered them in another, so a reader following "Figure 4" in the prose
# landed on a different picture. Found by `figOrder` in check-page.js, added in
# Lesson 6.12 after the same drift shipped for the third time.
#
# THE FIGURES WERE NOT MOVED. Every one of them already sat in the section that
# discusses it; only the numbering was out of step, so renumbering is the fix
# that leaves the lesson's argument where its author put it.
FIGURES = {
    "FIG_CLOCK": ("l310_fig1.svg", "1",
        "The counter on this machine ticks once every 41.667 ns. One sRGB encode takes "
        "9.3 ns, drawn to the same scale: four and a half of them fit inside a single "
        "tick. So bracketing exactly one encode with two clock reads returns 0.00 ns or "
        "41.67 ns and never 9.3 &#8212; the timer is not imprecise, it is categorically "
        "unable to represent the answer. The table is the way out. Read the TICKS "
        "columns rather than the nanoseconds: at N = 1 the whole batch spans zero to "
        "sixteen ticks, and by N = 1000 it spans about 230 and twelve repeated trials "
        "agree to 0.12 ns. Nothing about the code changed &#8212; only the form of the "
        "question."),

    "FIG_BUDGET": ("l310_fig2.svg", "2",
        "The capstone frame at two resolutions, phase by phase. Read the "
        "<code>fill</code> segment first &#8212; 96.1% of the frame, then 99.2% &#8212; "
        "and then read the row above it, which is what the figure exists for. "
        "<code>collect</code> measures 53.08&nbsp;us at 320&times;180 and 55.04&nbsp;us "
        "at 1280&times;720: sixteen times the pixels moved it by 3.7%, which is noise. "
        "Over the same change <code>fill</code> went from 1.55&nbsp;ms to 23.24&nbsp;ms, "
        "a factor of fifteen &#8212; which is the ratio of the pixel counts. Two phases, "
        "two entirely different axes, and &#167;3.3 is what that means."),

    "FIG_AXIS": ("l310_fig3.svg", "3",
        "The same experiment run in perpendicular directions, on log-log axes. Left: "
        "the same torus at increasing resolution &#8212; <code>fill</code> is a straight "
        "line of slope 1 and <code>collect</code> is flat, 61.75 to 63.75&nbsp;us across "
        "a 36&times; change in pixel count. Right: the same torus at the same size on "
        "screen, tessellated from 36 to 36,864 triangles &#8212; now <code>collect</code> "
        "climbs by a factor of 840 and the two lines CROSS. Everything about performance "
        "advice depends on which side of that crossing you are standing, and nothing "
        "about the renderer changed to move between them: only how finely the same "
        "object was cut up."),

    "FIG_LADDER": ("l310_fig5.svg", "5",
        "A differential ladder: one fixed geometry, rendered repeatedly with fill styles "
        "differing by exactly one thing, so the DELTA between adjacent rungs is the cost "
        "of what changed (&#167;3.5). Three results are worth measuring off the picture. "
        "The perspective divide &#8212; the thing Lesson 3.2 warned would cost you "
        "&#8212; has a delta of &#8722;0.018 ns/px, which is to say it is free. The "
        "depth test costs 0.27. And the sRGB encode costs 6.09, which the two brackets "
        "at the bottom compare against everything above it combined: 2.94. Lay one "
        "against the other. The largest single item in the fragment loop is the "
        "one-line call nobody has thought about since Lesson 2.4."),

    "FIG_ENCODE": ("l310_fig6.svg", "6",
        "The exact sRGB transfer function with the fitted four-term approximation "
        "dashed over it. At any honest scale they are the same line. The vertical marker "
        "shows where the linear toe is kept EXACTLY rather than fitted &#8212; every "
        "basis function here has an infinite slope at zero where the truth has 12.92, so "
        "a fit forced to cover the toe spends all its freedom there and is three codes "
        "worse everywhere else. Below, the difference between the two curves magnified "
        "770,000&times;. Two things to read off it: the worst excursion is 0.0115 of an "
        "8-bit code, and the curve CROSSES ZERO repeatedly, which is what a minimax fit "
        "looks like and is why the error is a wobble rather than a bias."),

    "FIG_AMDAHL": ("l310_fig4.svg", "4",
        "Amdahl's law, with this lesson's own optimisation plotted on it. Every curve "
        "flattens against a ceiling of 1/(1&nbsp;&#8722;&nbsp;p), because the part you "
        "did not speed up does not go away: a phase that is half your frame can never "
        "buy more than 2&times; however heroic you are. Our fill is 96% of the frame, so "
        "its ceiling is 25&times; &#8212; which is what makes it worth working on at "
        "all. Run a finger up the orange curve to the right edge and read that number "
        "off; then use the formula in the other direction, as an arithmetic check that "
        "your measured whole-frame speedup is one the parts could actually have "
        "produced."),
}

LISTING_META = {
    "src/core/profile.hpp": ("new", "new"),
    "src/core/profile.cpp": ("new", "new"),
    "src/gfx/colour.hpp": ("modified", "modified"),
    "src/gfx/colour.cpp": ("modified", "modified"),
    "src/gfx/raster.hpp": ("modified", "modified"),
    "src/gfx/raster.cpp": ("modified", "modified"),
    "src/main.cpp": ("modified", "modified"),
    "CMakeLists.txt": ("modified", "modified"),
}

LISTING_LANG = {
    "CMakeLists.txt": ("cmake", "CMake"),
}


# ---------------------------------------------------------------------------
# LISTING_SOURCE — added 2026-09-11, and without it THIS SCRIPT CANNOT RUN.
# ---------------------------------------------------------------------------
#
# Every path this page lists lives under `src/`, and Module 5's refactor RETIRED
# THAT WHOLE DIRECTORY — the engine's sources moved to engine/src and its headers
# to engine/include/engine. So re-running this builder failed at the first
# listing with a FileNotFoundError, which means the page had been unreproducible
# since Lesson 5.1 and nobody had noticed, because nobody had needed to rebuild
# it.
#
# The contents are pinned from commit 26cd723 — the commit that SHIPPED this
# lesson — so the listings show the code as it stood when the lesson was written,
# which is what a lesson's listings are supposed to show. This is the same
# discipline every builder from 5.8 onward uses; it simply arrived too late for
# this one.
LISTING_SOURCE = {
    'CMakeLists.txt': 'scratch/l310_CMakeLists.txt',
    'src/core/profile.cpp': 'scratch/l310_src_core_profile.cpp',
    'src/core/profile.hpp': 'scratch/l310_src_core_profile.hpp',
    'src/gfx/colour.cpp': 'scratch/l310_src_gfx_colour.cpp',
    'src/gfx/colour.hpp': 'scratch/l310_src_gfx_colour.hpp',
    'src/gfx/raster.cpp': 'scratch/l310_src_gfx_raster.cpp',
    'src/gfx/raster.hpp': 'scratch/l310_src_gfx_raster.hpp',
    'src/main.cpp': 'scratch/l310_src_main.cpp',
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
<title>3.10 — Profiling, and the Module 3 Capstone · Build a Professional 3D Game Engine</title>
<meta name="description" content="Module 3 finishes by measuring what it built: a self-calibrating frame budget, why the two-triangle floor costs eight times the two-thousand-triangle torus, a differential ladder that finds std::pow is the largest item in the fragment loop, and the first optimisation in the course with a measured before.">

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
     PAGE-SPECIFIC STYLES — Lesson 3.10 only.
     Deliberately OUTSIDE the SHARED-CSS markers: apply-shared.py owns what is
     between them, and page-local rules must survive a re-stamp.
     ========================================================================== -->
<style>
/* A fixed-dark text fill, for numerals sitting on a PALE swatch. The shared sheet
   provides `.t-inv` (fixed white) for the opposite case and gives the reason: the
   shape underneath is the same colour in both themes, so its label must not follow
   the theme's ink. Chosen per swatch by relative luminance in figs_310.py. */
figure.dia svg .t-onlight { fill: #22242a; }

/* One class per profiler zone, for SVG <text> that has to match the colour of a
   bar. These MUST be classes: `figure.dia svg text { fill: var(--dia-ink) }` in
   course.css is CSS, and CSS always beats an SVG presentation attribute, so an
   inline `fill="#eb786e"` on a <text> is silently ignored while the same attribute
   on the <rect> beside it works perfectly. The bar comes out right and its label
   does not, which is a difficult bug to see and an easy one to lint for —
   apply-shared.py does. Fixed hues in both themes, like `.t-inv`, because the
   shape being labelled is the same colour in both. */
figure.dia svg .z-build   { fill: #78c88c; }
figure.dia svg .z-collect { fill: #ebc864; }
figure.dia svg .z-sort    { fill: #dc8cc8; }
figure.dia svg .z-fill    { fill: #eb786e; }
figure.dia svg .z-overlay { fill: #78b4eb; }
figure.dia svg .z-present { fill: #9696dc; }
figure.dia svg .z-other   { fill: #6e7080; }

/* The Amdahl explorer. Bars are plain DOM elements rather than a canvas — they are
   two flex rows, so they inherit the theme's colours for free and stay crisp at any
   zoom, which a canvas would not. */
#w310 .widget-controls {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.35rem 1.1rem;
  font-family: var(--font-ui);
  font-size: 0.85rem;
  margin-bottom: 0.4rem;
}
#w310 .widget-controls label {
  display: inline-flex;
  align-items: center;
  gap: 0.4rem;
}
#w310 .widget-controls input[type="range"] { width: 9rem; }
#w310 .widget-controls output {
  font-family: var(--font-mono);
  min-width: 2.6rem;
  display: inline-block;
  text-align: right;
}
#w310 .widget-controls button {
  font-family: var(--font-ui);
  font-size: 0.78rem;
  padding: 0.2rem 0.55rem;
  border: 1px solid var(--rule);
  border-radius: 3px;
  background: transparent;
  color: var(--ink-soft);
  cursor: pointer;
}
#w310 .widget-controls button:hover {
  color: var(--accent-text);
  border-color: var(--accent);
}
#w310 .widget-controls .sep {
  display: inline-block;
  width: 1px;
  height: 1em;
  background: var(--rule);
}
#w310 .widget-readout {
  font-family: var(--font-mono);
  font-size: 0.78rem;
  color: var(--ink-soft);
  margin: 0.5rem 0 0;
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

# AMENDED 2026-09-11: the STATE block is gone, and this script no longer stamps
# one. `STATE.md` became the sole resume key at 0.6 and CLAUDE.md §9 was amended
# at 5.7 to retire the per-lesson block — but this builder predates that and was
# never updated, so re-running it would have RE-ADDED a 60%-of-the-file STATE
# block to a page it was stripped from. Found while renumbering this lesson's
# figures; the same amendment build_57.py received in 5.8.
TAIL = """

  <nav class="lesson-nav" aria-label="Lesson navigation (bottom)">
    <a class="prev-l" href="03-09-textures.html">
      <span class="dir">← Previous</span>
      <span class="ttl">3.9 — Texture Mapping and Bilinear Filtering</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="04-01-how-gpus-work.html">
      <span class="dir">Next →</span>
      <span class="ttl">4.1 — How GPUs Actually Work</span>
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
    with open("scratch/l310_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l310_body_b.html") as fh:
        body_b = fh.read()
    with open("scratch/l310_body_c.html") as fh:
        body_c = fh.read()

    page = HEAD + body_a + "\n" + body_b + "\n" + body_c + TAIL

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
