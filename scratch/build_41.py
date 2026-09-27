#!/usr/bin/env python3
"""Assemble docs/lessons/04-01-how-gpus-work.html.

The prose lives in scratch/l41_body_{a,b}.html, the figures are computed by
scratch/figs_41.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.

Figure placeholders are named by CONTENT, not by file number, because the figures
were authored in one order and appear in another.
"""
import os
import re

OUT = "docs/lessons/04-01-how-gpus-work.html"

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
    "FIG_LATENCY": ("l41_fig1.svg", "1",
        "A dependent chain runs at the LATENCY of its operation, however many execution "
        "units are idle &#8212; the short bars are work and the long dashed gaps are waiting "
        "for its own previous result. Interleave four independent chains and the gaps fill "
        "completely, with no change to the arithmetic at all. The measured bars beneath say "
        "the same thing in numbers: one chain runs at 3.124 ns per step and 32 at 0.112, a "
        "speedup of 27.88&#215; bought entirely with <em>other work to do</em>. This is the whole "
        "GPU architecture in one picture &#8212; and it is also why a shader that uses fewer "
        "registers runs faster, since registers are what limit how many lane groups can be "
        "resident at once."),

    "FIG_QUAD": ("l41_fig2.svg", "2",
        "One triangle over an 8&#215;8 pixel grid divided into 2&#215;2 quads. Blue lanes are "
        "covered; red lanes are not, and they run the fragment shader anyway &#8212; 10 of the "
        "24 lanes shaded here, for an efficiency of 58%. Count them off the picture. The "
        "reason they run is in the legend: <code>ddx</code> is lane 1 minus lane 0 and "
        "<code>ddy</code> is lane 2 minus lane 0, so a screen-space derivative is a "
        "subtraction between neighbours &#8212; and a lane cannot subtract against a neighbour "
        "that never ran. Every automatic mipmap selection ever made is paid for out of those "
        "red squares."),

    "FIG_DIVERGE": ("l41_fig3.svg", "3",
        "Above: one warp of 32 lanes meeting <code>if (c) A(); else B();</code>. Both rows "
        "execute across all 32 lanes; the pale ones are <em>masked</em> &#8212; computed, then "
        "discarded &#8212; so a warp whose lanes disagree pays for A and B together. Below: what "
        "that actually costs, measured against a warp that never diverges. The penalty is "
        "1.00&#215; when runs of like pixels are 1024 long, 1.03&#215; at 64, and steps to "
        "1.93&#215; at 8 and below. Find the dashed line: the step happens exactly where the "
        "run length crosses the warp width of 32, which is the check that says the model is "
        "right rather than merely plausible."),

    "FIG_EFFICIENCY": ("l41_fig4.svg", "4",
        "Lane efficiency against triangle size, for four block widths, averaged over 32 "
        "rotations. Read the marked radius: at 8 pixels a 2&#215;2 quad keeps <strong>75.5%</strong> "
        "of its lanes and a 16&#215;16 block keeps <strong>8.9%</strong>. Every doubling of the "
        "block costs more than the last, and the collapse is worst exactly where modern "
        "content lives. That is one half of why the hardware chose 2&#215;2; the other half is "
        "that 2&#215;2 is the smallest block containing a neighbour in x and a neighbour in y, "
        "so it is the cheapest shape that can produce a derivative at all. The table beside "
        "the chart is the price on a real scene."),

    "FIG_PIPELINE": ("l41_fig5.svg", "5",
        "The graphics pipeline as hardware implements it, against the function in this "
        "repository that already performs each stage. Follow any row across and it ends in a "
        "lesson you have finished. Module 4 is therefore not going to teach you a pipeline &#8212; "
        "it is going to hand yours to hardware, and the reason that is a rename rather than a "
        "rewrite is a decision taken back in Module 2: the software rasterizer targets "
        "SDL_GPU&#8217;s exact NDC conventions, so none of the maths moves."),

    "FIG_STATE": ("l41_fig6.svg", "6",
        "<code>engine::fill_style</code> beside <code>SDL_GPUGraphicsPipelineCreateInfo</code>. "
        "Count both columns: <strong>ten fields against nine</strong> &#8212; and 53 on the right "
        "once its nested state structs are expanded. They are the same object, which is not a "
        "coincidence: Lesson 3.2 adopted the shape deliberately and 3.4 and 3.9 kept mirroring "
        "SDL&#8217;s field names and enumerator orders, so that this moment would be an "
        "observation rather than a leap."),
}

LISTING_META = {
    "src/gfx/raster.hpp": ("modified", "modified"),
    "src/gfx/raster.cpp": ("modified", "modified"),
    "src/main.cpp": ("modified", "modified"),
}

LISTING_LANG = {}


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
# The contents are pinned from commit b9bedf0 — the commit that SHIPPED this
# lesson — so the listings show the code as it stood when the lesson was written,
# which is what a lesson's listings are supposed to show. This is the same
# discipline every builder from 5.8 onward uses; it simply arrived too late for
# this one.
LISTING_SOURCE = {
    'src/gfx/raster.cpp': 'scratch/l41_src_gfx_raster.cpp',
    'src/gfx/raster.hpp': 'scratch/l41_src_gfx_raster.hpp',
    'src/main.cpp': 'scratch/l41_src_main.cpp',
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
<title>4.1 — How GPUs Actually Work · Build a Professional 3D Game Engine</title>
<meta name="description" content="A GPU is not a fast CPU — per lane it is slower, and it wins anyway. SIMT, 2x2 quads and helper lanes, divergence, and why render state lives in an immutable pipeline object, all measured using the software rasterizer you already own.">

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
     PAGE-SPECIFIC STYLES — Lesson 4.1 only.
     Deliberately OUTSIDE the SHARED-CSS markers: apply-shared.py owns what is
     between them, and page-local rules must survive a re-stamp.
     ========================================================================== -->
<style>
/* A fixed-dark text fill, for numerals sitting on a PALE swatch. The shared sheet
   provides `.t-inv` (fixed white) for the opposite case and gives the reason: the
   shape underneath is the same colour in both themes, so its label must not follow
   the theme's ink. Chosen per swatch by relative luminance in figs_310.py. */
figure.dia svg .t-onlight { fill: #22242a; }

/* One class per block-size series in Figure 4. These MUST be classes:
   `figure.dia svg text { fill: var(--dia-ink) }` in course.css is CSS, and CSS always
   beats an SVG presentation attribute, so an inline `fill="#eb786e"` on a <text> is
   silently ignored while the same attribute on the <rect> beside it works perfectly.
   apply-shared.py lints for exactly this. */
figure.dia svg .z-quad2  { fill: #78b4eb; }
figure.dia svg .z-quad4  { fill: #ebc864; }
figure.dia svg .z-quad8  { fill: #f0a06a; }
figure.dia svg .z-quad16 { fill: #eb786e; }

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
    <a class="prev-l" href="03-10-profiling-capstone.html">
      <span class="dir">← Previous</span>
      <span class="ttl">3.10 — Profiling, and the Module 3 Capstone</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="04-02-sdl-gpu-model.html">
      <span class="dir">Next →</span>
      <span class="ttl">4.2 — The SDL_GPU Mental Model</span>
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
    with open("scratch/l41_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l41_body_b.html") as fh:
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
