#!/usr/bin/env python3
"""Assemble docs/lessons/06-18-text-overlay.html.

Same pipeline as build_617.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l618_body_{a,b,c}.html, figures from scratch/figs_618.py, and
every code listing read LIVE from the repository — see LISTING_SOURCE, which is
deliberately EMPTY today for the reason 6.6 learned, and which Module 7's first
session must fill in before it edits any of these files.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order — which is why FIG_SPLIT is figure 1.
The first draft numbered them by the order they were WRITTEN and check-page.js's
figOrder check reported all seven.
"""
import os
import re

OUT = "docs/lessons/06-18-text-overlay.html"

FIGURES = {
    "FIG_SPLIT": ("l618_fig1.svg", "1",
        "Three files, and the line that runs between the second and the third. "
        "<code>font.hpp</code> turns outlines into coverage; <code>overlay.hpp</code> turns strings "
        "and rectangles into quads in pixel space; <strong>neither one mentions SDL_GPU</strong>. "
        "That is not tidiness, it is what makes two things possible: <code>hello_cube</code>, a "
        "two-hundred-line demo with no GPU device at all, gets text by adding twenty lines; and the "
        "same <code>overlay_batch</code> can be rendered by both consumers and compared channel for "
        "channel, which is &#167;11. A type that knew about <code>SDL_GPUTexture</code> could not "
        "have been used by the software rasterizer, and this course keeps two renderers honest by "
        "making them consume the same data."),

    "FIG_GLYPH": ("l618_fig2.svg", "2",
        "The two rectangles, and nearly every early text bug is one mistaken for the other. The "
        "amber box is the INK: what the atlas holds, and what gets blitted. The dashed blue box is "
        "the ADVANCE: how far the pen moves next. They are different widths, and for some glyphs "
        "the ink starts LEFT of the pen &#8212; <code>j</code>&#8217;s <code>offset_x</code> is "
        "&minus;2.0, so it draws outside its own advance box in both directions. The pen sits on "
        "the BASELINE, which is why <code>offset_y</code> is negative for every glyph with ink "
        "above it: the ink starts <em>up</em> from the pen, and +y points down. Note also what the "
        "line box is: ascent to descent, <strong>16.0000 px</strong> for a font asked for at 16 px "
        "&#8212; while the em square of that same font is <strong>13.6869 px</strong>, because "
        "Karla&#8217;s ascent and descent sum to 1.169 em."),

    "FIG_ATLAS": ("l618_fig4.svg", "4",
        "Ninety-five glyphs on six shelves, and the number that looks like a verdict on the packer "
        "but is really a fact about the font. Shelf packing sorts by descending height and fills "
        "rows, so its waste is the slack under short glyphs on a tall shelf &#8212; and glyph "
        "heights at one size cluster hard (every printable ASCII glyph here is 3 to 14 texels "
        "tall), which is why a fifteen-line algorithm does well. The red block at the top left is "
        "the 2&times;2 fully-covered texel that lets one pipeline draw both the panel and the words "
        "on it. Read the two boxes below carefully: <strong>43.6%</strong> has a power-of-two "
        "denominator fixed before the packer ran and a numerator that is the glyphs&#8217; own "
        "area, so a perfect packer and a hopeless one score the same. <strong>83.3%</strong> is "
        "over the rows the packer actually touched, and moves when the algorithm does."),

    "FIG_SNAP": ("l618_fig3.svg", "3",
        "The same rounding, applied to two different quantities, over one 45-character line. "
        "Snapping each glyph&#8217;s POSITION discards the fraction for display only &#8212; the "
        "pen keeps it &#8212; so the error at each glyph is independent of the last and can never "
        "leave the shaded half-pixel band; measured worst case <strong>0.4765 px</strong>. Rounding "
        "each ADVANCE discards the fraction from the accumulator, so every error is added to the "
        "next one: a random walk that reaches <strong>5.26 px</strong> and is still "
        "<strong>4.32 px</strong> out at the end of the line. The second is tempting because it is "
        "simpler &#8212; if the pen is always an integer, nothing else needs rounding &#8212; and "
        "that is exactly why it is worth plotting rather than arguing about."),

    "FIG_GAMMA": ("l618_fig5.svg", "5",
        "Why compositing coverage in the wrong space reads as <em>stem weight</em>, and how to tell "
        "two independent bugs apart. The curve is one pixel of white over black: correct "
        "compositing emits light equal to the coverage, and lerping sRGB codes emits "
        "<code>srgb_to_linear</code> of it &#8212; which is worst where coverage is LOWEST. At a "
        "tenth of coverage the pixel emits <strong>10.3%</strong> of the light it should, and an "
        "antialiased 16 px glyph is mostly low coverage, so the tapers and the flanks of every stem "
        "very nearly vanish while the fully-covered interiors come out right. The table is the "
        "part that is usually missing: an <code>_SRGB</code> coverage atlas and an sRGB-space blend "
        "are <strong>the same function applied in two different files</strong>, and agree to four "
        "significant figures on a dark background. Inverting the contrast separates them &#8212; "
        "the blend fattens dark-on-light to <strong>137.8%</strong>, the atlas thins it to "
        "<strong>62.1%</strong>."),

    "FIG_WHERE": ("l618_fig6.svg", "6",
        "The same white text, composited at two points in one frame, and only one of them is a "
        "matter of taste. Before the tonemap, the text is a quantity of light and the curve has its "
        "way with it: linear 1.0 through the ACES fit is <strong>0.803797</strong>, which is sRGB "
        "code <strong>232</strong> rather than 255 &#8212; so the white UI is grey, and grey by an "
        "amount that moves with the exposure (165 at a quarter stop, 252 at four), which means the "
        "HUD dims when the player walks into a dark room. It blooms, too, since the bloom runs "
        "between the two marks. After the tonemap, white is 255 and stays 255. The rule underneath "
        "is Lesson 6.12&#8217;s line: scene-referred values belong before the curve, "
        "display-referred values after it &#8212; and a UI colour is a CODE somebody picked in a "
        "colour picker, not light arriving from a surface. Diegetic UI is the deliberate exception, "
        "and it goes on the other side for exactly the same reason."),

    "FIG_PASS": ("l618_fig7.svg", "7",
        "The overlay declared as a frame-graph pass, which is Lesson 6.17&#8217;s API being used "
        "for the first time by somebody other than the lesson that wrote it. The pass says one "
        "word: <code>keep</code> &#8212; <em>my output depends on what was already in this "
        "target</em> &#8212; which for an alpha blend is simply true, since the destination is an "
        "operand of <code>over</code>. It is a statement about arithmetic, answerable without "
        "knowing what a load op is. Four things follow. Note where the third one LANDS: the "
        "overlay spoke, and the consequence is that the RESOLVE must <code>STORE</code> &#8212; a "
        "different pass, in a different file, which is precisely the class of fact humans get wrong "
        "across a boundary. And note what the graph cannot do: declare this pass "
        "<code>discard_write</code> instead and it compiles happily, derives "
        "<code>DONT_CARE</code>, and erases the frame under the text. The graph relocates the "
        "claim; it does not verify it."),
}

LISTING_META = {
    "engine/include/engine/gfx/font.hpp": ("new", "new"),
    "engine/src/gfx/font.cpp": ("new", "new"),
    "engine/include/engine/gfx/overlay.hpp": ("new", "new"),
    "engine/src/gfx/overlay.cpp": ("new", "new"),
    "engine/include/engine/gfx/gpu_overlay.hpp": ("new", "new"),
    "engine/src/gfx/gpu_overlay.cpp": ("new", "new"),
    "shaders/overlay.vert.hlsl": ("new", "new"),
    "shaders/overlay.frag.hlsl": ("new", "new"),
    "engine/include/engine/gfx/gpu_texture.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_texture.cpp": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "CMakeLists.txt": ("modified", "modified"),
    "demos/hello_cube/main.cpp": ("modified", "modified"),
    "scratch/verify_618.cpp": ("new", "new"),
}

LISTING_LANG = {
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "CMakeLists.txt": ("cmake", "CMake"),
    "shaders/overlay.vert.hlsl": ("cpp", "HLSL"),
    "shaders/overlay.frag.hlsl": ("cpp", "HLSL"),
}

LISTING_SOURCE = {
    # PINNED at the start of Lesson 5.12's session, from 725e62a — the commit
    # that shipped this page — by scratch/pin_listings.py, which verified every
    # one of the fourteen appears verbatim in the published HTML.
    #
    # 5.12 is authored after this page but READ before it, and it edits
    # demos/CMakeLists.txt and demos/ecs_swarm/main.cpp; two of the pins below
    # (CMakeLists.txt, demos/hello_cube/main.cpp) are files every lesson touches
    # sooner or later. Unpinned, this builder would have started showing 5.12's
    # code inside Lesson 6.18 — Cause A from docs/_template/README.md §15.

    "engine/include/engine/gfx/font.hpp":        "scratch/l618_engine_include_engine_gfx_font.hpp",
    "engine/src/gfx/font.cpp":                   "scratch/l618_engine_src_gfx_font.cpp",
    "engine/include/engine/gfx/overlay.hpp":     "scratch/l618_engine_include_engine_gfx_overlay.hpp",
    "engine/src/gfx/overlay.cpp":                "scratch/l618_engine_src_gfx_overlay.cpp",
    "engine/include/engine/gfx/gpu_overlay.hpp": "scratch/l618_engine_include_engine_gfx_gpu_overlay.hpp",
    "engine/src/gfx/gpu_overlay.cpp":            "scratch/l618_engine_src_gfx_gpu_overlay.cpp",
    "shaders/overlay.vert.hlsl":                 "scratch/l618_shaders_overlay.vert.hlsl",
    "shaders/overlay.frag.hlsl":                 "scratch/l618_shaders_overlay.frag.hlsl",
    "engine/include/engine/gfx/gpu_texture.hpp": "scratch/l618_engine_include_engine_gfx_gpu_texture.hpp",
    "engine/src/gfx/gpu_texture.cpp":            "scratch/l618_engine_src_gfx_gpu_texture.cpp",
    "engine/CMakeLists.txt":                     "scratch/l618_engine_CMakeLists.txt",
    "CMakeLists.txt":                            "scratch/l618_CMakeLists.txt",
    "demos/hello_cube/main.cpp":                 "scratch/l618_demos_hello_cube_main.cpp",
    "scratch/verify_618.cpp":                    "scratch/l618_scratch_verify_618.cpp",
}

# NOTHING PINNED YET, and this page lists FOURTEEN files whole.
#
# The next lesson is 7.1 — EULER ANGLES AND THEIR PATHOLOGIES, the opening of the
# rotation arc. On the face of it nothing could be further from a glyph atlas.
#
# PIN ANYWAY, AND FOR ONE REASON THAT IS NOT ABOUT LIKELIHOOD.
#   1 `verify_618.cpp` is GITIGNORED, so its provenance cannot be recovered from
#     history later — only from this page. A working-tree copy taken NOW is the
#     only cheap moment, and 6.17's session proved that copy is exact.
#   2 `CMakeLists.txt` and `engine/CMakeLists.txt` are on this list, and EVERY
#     lesson edits at least one of them. These two are not "unlikely to move",
#     they are certain to.
#   3 `demos/hello_cube/main.cpp` is the course's acceptance test for the public
#     API and is edited whenever the API grows. Module 7 grows it.
#
# So, before writing a line of 7.1, run:
#
#   python3 scratch/pin_listings.py 618 --out scratch/_dict618.txt
#
# It writes every pin from the commit that shipped this page (taking the
# gitignored one from the working tree and saying so), and VERIFIES each by
# substring against the shipped HTML. Paste the dict it prints into
# LISTING_SOURCE above, re-run this file, and `git diff` the page: EIGHTEEN
# lessons running, the diff has been exactly the nav lines meant to move.

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
<title>6.18 — Text and 2D Overlay Rendering · Build a Professional 3D Game Engine</title>
<meta name="description" content="This engine can evaluate a microfacet BRDF, cascade a shadow map, prefilter an environment cube, bloom, tonemap, cull and deduce its own frame order - and it cannot put a single character on the screen. Every number Module 6 measured went to a terminal, so the picture and the measurement have never been on the same surface. Fixing that is three problems and only one is about drawing. The first is arithmetic: pen, baseline, bearing, advance, and what to round - snapping a glyph's POSITION is bounded at half a pixel forever, while rounding its ADVANCE drifts 4.32 px by the end of one 45-character line, from the same rounding applied to a different quantity. The second is packing: ninety-five glyphs on six shelves, 6.10's padding argument arriving at magnification instead of minification (47.3 per cent of edge texels carry a neighbour's ink without it), and an occupancy figure of 43.6 per cent that looks like a verdict on the packer and is really a fact about the font - the honest number, over the rows it touched, is 83.3 per cent. The third is the colour space. Compositing a glyph is `over`, `over` is a lerp, and a lerp must happen in linear light: get it wrong and light text on dark carries 62.2 per cent of the correct ink while dark text on light carries 137.8 per cent, and both are blamed on the font. Then a second, independent bug - uploading a coverage atlas as _SRGB - produces the IDENTICAL number on a dark background, because it is the same function applied in a different file; one test separates them. The GPU's UNORM fallback path is shown to reproduce the wrong CPU arithmetic in silicon, on zero differing pixels. White composited before the tonemap comes out at sRGB code 232 rather than 255, and moves with the exposure. The overlay is declared as a frame-graph pass in one word - the first external use of 6.17's API, including the `keep` on an imported resource that had never run - with an honest report of what it does not catch. And the same batch rendered by the software compositor and by SDL_GPU agrees on every pixel to within one sRGB code, with a control reporting 934 differing so the agreement means something.">

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
    <a class="prev-l" href="06-17b-local-lights.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.17b — Local Lights: Point and Spot, and Their Shadows</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="07-01-euler-angles.html">
      <span class="dir">Next →</span>
      <span class="ttl">7.1 — Euler Angles and Their Pathologies</span>
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
        with open(f"scratch/l618_body_{name}.html") as fh:
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
