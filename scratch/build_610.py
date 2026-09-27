#!/usr/bin/env python3
"""Assemble docs/lessons/06-08-shadow-mapping.html.

Same pipeline as build_67.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l68_body_{a,b,c}.html, figures from scratch/figs_68.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-10-mipmaps.html"

FIGURES = {
    "FIG_PROBLEM": ("l610_fig1.svg", "1",
        "Lesson 3.9&#8217;s measurement, and the gap it named. Bilinear filtering reads FOUR "
        "texels &#8212; a flat line &#8212; whatever the pixel covers. What the pixel actually "
        "covers rises from 0.60 texels at the bottom of the frame to 62.46 two rows below the "
        "horizon. They cross at four, and past the crossover the sampler sees a shrinking "
        "fraction of the truth: about <strong>6.4%</strong> of it at the horizon. That gap is "
        "the sparkle, and closing it is this lesson."),

    "FIG_CHAIN": ("l610_fig2.svg", "2",
        "The image at every size at once. Each level is the one above with every 2&#215;2 block "
        "replaced by its average, so it is a quarter of the area &#8212; which makes the total "
        "extra storage a geometric series: 1/4 + 1/16 + 1/64 + &#8230; = <strong>1/3</strong>. "
        "Measured on a 256&#215;256 checker: 65,536 texels become 87,381, which is 33.33% and the "
        "series to four figures. The number everyone quotes, derived rather than remembered."),

    "FIG_LINEAR": ("l610_fig5.svg", "5",
        "The bug that makes a surface darken as it recedes. One 2&#215;2 block of a "
        "black-and-white checker should average to <em>half the light</em> &#8212; and Lesson 6.1 "
        "established that half the light is <strong>code 188</strong>, not 128. Averaging the "
        "sRGB BYTES gives code 127, which is 0.2122 of white: the naive chain delivers "
        "<strong>42.2%</strong> of the light it should, <strong>57.8% too dark</strong>, at level "
        "1 alone. It compounds down the chain, which is why the symptom is not &#8220;textures are "
        "dark&#8221; but &#8220;textures get darker with distance&#8221; &#8212; and why it is so "
        "often mistaken for a lighting falloff problem."),

    "FIG_LEVEL": ("l610_fig3.svg", "3",
        "Choosing the level. Level L has texels 2<sup>L</sup> times as wide as level 0, because "
        "each level halved them &#8212; so matching the texel to the footprint means "
        "2<sup>L</sup> = &#961;, and therefore L = log<sub>2</sub>&#961;. The logarithm is a "
        "consequence of the pyramid halving, not a tuning choice, which is why the formula has no "
        "constants in it. Lesson 3.9&#8217;s measured 62.46 texels per pixel lands at level "
        "<strong>5.965</strong> &#8212; and the fraction is real: no level has texels exactly "
        "62.46 across, which is exactly what trilinear filtering interpolates."),

    "FIG_GRADIENT": ("l610_fig4.svg", "4",
        "Where the footprint comes from, and why the CPU has to work for it. A GPU shades "
        "fragments in 2&#215;2 quads specifically so that a neighbour is always available, which "
        "makes <code>ddx</code> a subtraction &#8212; and is also why it cannot be called from "
        "divergent control flow. A scanline rasterizer has no neighbour: the row above was "
        "discarded and the pixel to the right has not happened. So the derivative comes from the "
        "TRIANGLE, by the quotient rule, and it is exact because U and W are both affine in "
        "screen space. Everything except two multiplies and a subtract hoists out of the pixel "
        "loop. Checked against a central difference of the real interpolation: they agree to "
        "<strong>5.79&#215;10<sup>&#8722;6</sup></strong>."),

    "FIG_ANISO": ("l610_fig6.svg", "6",
        "A footprint that is not square, which is every floor you will ever look along. At 16:1, "
        "isotropic filtering must choose: take the level from the LONG axis (32 texels, level 5) "
        "and the short axis is blurred by a factor of sixteen &#8212; the smeared distant ground "
        "of a game with anisotropy switched off &#8212; or take it from the short axis and the "
        "long one aliases exactly as before. Anisotropic filtering refuses the choice: it takes "
        "the level the SHORT axis asked for and several samples ALONG the long one. On a square "
        "footprint the ratio is 1, so it costs nothing &#8212; measured identical to six decimal "
        "places, which is worth checking rather than assuming."),
}

# THE SECOND TUPLE ELEMENT IS THE STATUS WORD, NOT THE LANGUAGE. `listing()`
# unpacks these as `tag, word` and emits `<span class="tag {tag}">{word}</span>`,
# so a language here puts "cpp" in a pill that every other lesson fills with
# "new" or "modified". The LANGUAGE belongs in LISTING_LANG, which already
# carries it and which drives the separate `.lang` span. Ran from 6.9 to 6.12
# before anybody looked at a pill.
LISTING_META = {
    "engine/include/engine/gfx/mipmap.hpp": ("new", "new"),
    "engine/src/gfx/mipmap.cpp": ("new", "new"),
    "scratch/verify_610.cpp": ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gpu_texture.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/material.hpp": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "demos/gltf_view/main.cpp": ("modified", "modified"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/texture.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_texture.cpp": ("modified", "modified"),
    "engine/src/gfx/raster.cpp": ("modified", "modified"),
}

LISTING_LANG = {
    "shaders/shadow.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/scene.frag.hlsl":  ("hlsl", "HLSL"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/CMakeLists.txt": ("cmake", "CMake"),
}

LISTING_SOURCE = {
    # PINNED at the start of 6.11's session, exactly as the note below asked.
    # 6.11 adds an alpha channel to the mip chain's averaging (cutout foliage
    # dissolves with distance if the chain averages coverage), so mipmap.cpp
    # WILL move under this page's feet. The page must keep showing 6.10's code.
    "engine/include/engine/gfx/mipmap.hpp": "scratch/l610_mipmap.hpp",
    "engine/src/gfx/mipmap.cpp":            "scratch/l610_mipmap.cpp",
    "scratch/verify_610.cpp":               "scratch/l610_verify_610.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gpu_texture.hpp": "scratch/l610_engine_include_engine_gfx_gpu_texture.hpp",
    "engine/include/engine/gfx/material.hpp": "scratch/l610_engine_include_engine_gfx_material.hpp",
    "engine/CMakeLists.txt": "scratch/l610_engine_CMakeLists.txt",
    "demos/gltf_view/main.cpp": "scratch/l610_demos_gltf_view_main.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/texture.hpp": "scratch/l610_engine_include_engine_gfx_texture.hpp",
    "engine/src/gfx/gpu_texture.cpp": "scratch/l610_engine_src_gfx_gpu_texture.cpp",
    "engine/src/gfx/raster.cpp": "scratch/l610_engine_src_gfx_raster.cpp",
}

# NOTHING PINNED YET, and this page lists EIGHT files whole.
#
# Lesson 6.9 is cascaded shadow maps, which splits one map into several. That
# touches `shadow.hpp` and `shadow.cpp` for certain (a cascade is a second fit,
# a second map and a per-cascade bias), `gpu_shadow.{hpp,cpp}` for certain (an
# array texture, and one pass per cascade) and `scene.frag.hlsl` for certain
# (choosing a cascade per fragment, and blending the seam). `bounds.hpp` is
# likely too: fitting a cascade to a slice of the CAMERA frustum needs that
# frustum's corners, which is an AABB question.
#
# So pin all of them before writing a line of 6.9:
#
#     git show <6.8 commit>:<path> > scratch/l68_<name>
#     git show <6.8 commit>:<path> | diff - scratch/l68_<name>     # must be empty
#
# And take `scratch/verify_68.cpp` EARLY — gitignored, so its only provenance is
# a working-tree copy, which cannot be recovered afterwards.
#
# (When copying build_NN.py, empty this dict as well as FIGURES. build_66.py
# inherited 6.5's three pins verbatim and they sat there inert for a whole
# lesson, because none of those paths appeared in its LISTING_META — harmless,
# and it made the pinning discipline LOOK satisfied.)



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
<title>6.10 — Mipmaps, LOD, and Anisotropic Filtering · Build a Professional 3D Game Engine</title>
<meta name="description" content="A debt being paid: Lesson 3.9 measured that one screen pixel covers 0.60 texels at the bottom of the frame and 62.46 below the horizon while bilinear reads four, then deferred the fix to Module 6 - a promise repeated six times, once in a shipped public header. Derives the mip level as log2 of the footprint; computes the screen-space uv derivative analytically, because a scanline rasterizer has no neighbouring fragment to subtract, and checks the closed form against a central difference of the real perspective-correct interpolation (5.79e-6). Quantifies the famous linear-light bug: an sRGB-byte-averaged chain delivers 42.2 per cent of the light it should at level 1 and compounds. Trilinear as the same artefact 6.9 met as the cascade seam; anisotropy as a refusal to choose between blurring and aliasing. Ports to both renderers; the reference render stays byte-identical, this time by design.">

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
    <a class="prev-l" href="06-09-cascaded-shadows.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.9 — Cascaded Shadow Maps</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-11-transparency.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.11 — Transparency: Alpha Modes, Blending, and Draw Order</span>
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
        with open(f"scratch/l610_body_{name}.html") as fh:
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
