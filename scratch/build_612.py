#!/usr/bin/env python3
"""Assemble docs/lessons/06-12-hdr-tonemapping.html.

Same pipeline as build_611.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l612_body_{a,b,c}.html, figures from scratch/figs_612.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-12-hdr-tonemapping.html"

FIGURES = {
    "FIG_LID": ("l612_fig1.svg", "1",
        "The lid, and what is above it. Every value from 1.0 upward is stored as the SAME CODE, "
        "so this is not a rounding error at the margin &#8212; above the line the clamp is TOTAL. "
        "The measured peaks are the same white dielectric under the same light with only its "
        "roughness changed, evaluated at the light&#8217;s actual mirror direction. THE DEMO&#8217;S "
        "OWN 0.49 SITS JUST UNDER, and that is not luck: Lesson 6.2 set "
        "<code>k_reference_irradiance</code> to pi precisely so a white surface would render at "
        "1.0. The engine has been avoiding this question BY CONSTRUCTION, and one polished "
        "material is all it takes to ask it."),

    "FIG_EXPOSURE": ("l612_fig2.svg", "2",
        "Exposure as a logarithmic scale, which is what lets an artist and a photographer use the "
        "same word. One EV is one STOP is a factor of two, and what the number says is <em>this "
        "much light shall be white</em> &#8212; so EV 15, full daylight, puts white at 39,322. The "
        "conversion is <code>exposure = 1/(1.2 &#215; 2^EV)</code>, of which the 2^EV is the "
        "definition of a stop and the 1.2 is a CAMERA CALIBRATION (ISO 12232, quoted rather than "
        "derived). And the engine&#8217;s previous behaviour is a point on this scale: 1.0 maps to "
        "white means an exposure of exactly 1, which is <strong>EV &#8722;0.263</strong>. 6.2&#8217;s "
        "choice WAS an exposure setting; it was simply the only one available."),

    "FIG_CURVES": ("l612_fig3.svg", "3",
        "Four operators, and the three properties that make one LEGAL rather than merely popular: "
        "monotonic (or a highlight can come out darker than its own edge), close to the identity "
        "near zero (or the dark end &#8212; where an sRGB encode spends most of its codes &#8212; is "
        "wrong), and bounded by 1 (or something downstream will clamp and you will have a curve "
        "AND a clamp). Reinhard is derived in one line: divide by something that is about 1 for "
        "small x and about x for large x, and the simplest such thing is 1 + x. Its famous defect "
        "is visible here as the curve never touching the dashed lid &#8212; reaching code 255 needs "
        "an input of <strong>224</strong>."),

    "FIG_RESOLVE": ("l612_fig5.svg", "5",
        "The resolve, and why its three steps cannot be reordered. Exposure multiplies in linear "
        "light because light accumulates linearly in time; the curve compresses in linear light "
        "because it is a statement about quantities of light rather than about codes; and the "
        "encode is last and happens exactly once, which is Lesson 6.1&#8217;s rule arriving for the "
        "sixth time in this module. BOTH WRONG ORDERINGS ARE MEASURED rather than asserted: "
        "exposure after the curve turns code 203 into code 128 <em>and</em> destroys the bright "
        "end&#8217;s variation, because everything above about 4 was already squeezed into one "
        "place; a curve after the encode compresses exactly what the encode had spread out, and "
        "crushes the shadows first."),

    "FIG_HUE": ("l612_fig4.svg", "4",
        "Two defensible ways to apply a one-number curve to a three-number colour, and they give "
        "visibly different pictures &#8212; a real choice, like <code>blend_space</code> in 2.4, "
        "not a tuning knob. PER-CHANNEL compresses the big channel more than the small one, so the "
        "colour moves toward white as it brightens, which is what film does. LUMINANCE-ONLY "
        "preserves hue and saturation exactly, because scaling by a scalar is a move along the ray "
        "from black &#8212; and then leaves a channel at 1.92 for the encode to clip. Blue is the "
        "worst case: its luminance weight is only 0.0722, so a pure blue of (0, 0, 8) has a "
        "comfortable mid-range luminance, sails through the curve, and arrives at 5.07. It "
        "preserves the hue right up to the point where it does not."),

    "FIG_PASSES": ("l612_fig6.svg", "6",
        "The first pass in this engine that draws no geometry, and the float target between the "
        "two. THE TEMPTING SHORTCUT IS TO TONEMAP AT THE END OF THE SCENE SHADER: it costs nothing "
        "extra and it is wrong three ways, each of which becomes a real limitation within two "
        "lessons. An exposure derived from the frame&#8217;s own log-average cannot be known while "
        "the frame is still being drawn; bloom (6.13) operates on the PRE-curve image and needs "
        "the values a per-fragment tonemap has already compressed; and blending would composite "
        "tonemapped values, where <code>f(a) over f(b)</code> is not <code>f(a over b)</code> "
        "because a curve is not linear. The resolve&#8217;s cost scales with PIXELS rather than "
        "with geometry, which is the defining property of a post pass."),
}

# THE SECOND TUPLE ELEMENT IS THE STATUS WORD, NOT THE LANGUAGE. `listing()`
# unpacks these as `tag, word` and emits `<span class="tag {tag}">{word}</span>`,
# so a language here puts "cpp" in a pill that every other lesson fills with
# "new" or "modified". The LANGUAGE belongs in LISTING_LANG, which already
# carries it and which drives the separate `.lang` span. Ran from 6.9 to 6.12
# before anybody looked at a pill.
LISTING_META = {
    "engine/include/engine/gfx/hdr.hpp": ("new", "new"),
    "engine/src/gfx/hdr.cpp": ("new", "new"),
    "engine/include/engine/gfx/gpu_post.hpp": ("new", "new"),
    "engine/src/gfx/gpu_post.cpp": ("new", "new"),
    "shaders/fullscreen.vert.hlsl": ("new", "new"),
    "shaders/tonemap.frag.hlsl": ("new", "new"),
    "scratch/verify_612.cpp": ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gpu_texture.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_uniform.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/raster.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_texture.cpp": ("modified", "modified"),
    "engine/src/gfx/raster.cpp": ("modified", "modified"),
    "shaders/scene.frag.hlsl": ("modified", "modified"),
    "CMakeLists.txt": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "demos/gltf_view/main.cpp": ("modified", "modified"),
}

LISTING_LANG = {
    "shaders/fullscreen.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/tonemap.frag.hlsl": ("hlsl", "HLSL"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "shaders/scene.frag.hlsl": ("hlsl", "HLSL"),
    "CMakeLists.txt": ("cmake", "CMake"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
}

LISTING_SOURCE = {
    # PINNED 2026-09-11, at the start of 6.13's session, exactly as the note below
    # instructed. Every path here is read from a FROZEN COPY of what commit 42f91ac
    # shipped, not from the live tree — so 6.13 may edit these files freely and a
    # rebuild of THIS page still reproduces what the reader was shown.
    "engine/include/engine/gfx/hdr.hpp": "scratch/l612_hdr.hpp",
    "engine/src/gfx/hdr.cpp": "scratch/l612_hdr.cpp",
    "engine/include/engine/gfx/gpu_post.hpp": "scratch/l612_gpu_post.hpp",
    "engine/src/gfx/gpu_post.cpp": "scratch/l612_gpu_post.cpp",
    "shaders/fullscreen.vert.hlsl": "scratch/l612_fullscreen.vert.hlsl",
    "shaders/tonemap.frag.hlsl": "scratch/l612_tonemap.frag.hlsl",
    # gitignored, so its only provenance is a working-tree copy taken before edits.
    "scratch/verify_612.cpp": "scratch/l612_verify_612.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gpu_texture.hpp": "scratch/l612_engine_include_engine_gfx_gpu_texture.hpp",
    "engine/include/engine/gfx/gpu_uniform.hpp": "scratch/l612_engine_include_engine_gfx_gpu_uniform.hpp",
    "engine/include/engine/gfx/raster.hpp": "scratch/l612_engine_include_engine_gfx_raster.hpp",
    "engine/src/gfx/gpu_texture.cpp": "scratch/l612_engine_src_gfx_gpu_texture.cpp",
    "engine/src/gfx/raster.cpp": "scratch/l612_engine_src_gfx_raster.cpp",
    "shaders/scene.frag.hlsl": "scratch/l612_shaders_scene.frag.hlsl",
    "CMakeLists.txt": "scratch/l612_CMakeLists.txt",
    "engine/CMakeLists.txt": "scratch/l612_engine_CMakeLists.txt",
    "demos/gltf_view/main.cpp": "scratch/l612_demos_gltf_view_main.cpp",
}

# PINNED. (Historical note, kept because the prediction is worth grading.)
#
# Lesson 6.13 is bloom and the post-processing STACK, which is the second user
# of everything in `gpu_post.{hpp,cpp}` — and `gpu_post.hpp` says outright that
# it is "deliberately not a post-processing stack" and that 6.13 is where the
# ownership question gets asked. So those two are near certain to move, and
# `hdr.hpp`/`hdr.cpp` are likely: a bloom needs a bright-pass threshold and a
# downsample chain, and both are HDR operations that will want a home.
# `fullscreen.vert.hlsl` is the one file that should NOT move — every post pass
# uses the same three corners — which makes it the useful control.
#
# So pin all six before writing a line of 6.13:
#
#     git show <6.12 commit>:<path> > scratch/l612_<name>
#     git show <6.12 commit>:<path> | diff - scratch/l612_<name>     # must be empty
#
# And take `scratch/verify_612.cpp` EARLY — gitignored, so its only provenance
# is a working-tree copy, which cannot be recovered afterwards.


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
<title>6.12 — HDR and Tonemapping · Build a Professional 3D Game Engine</title>
<meta name="description" content="This engine does not have an HDR bug - it has been avoiding the question by construction, and the lesson opens by measuring that. k_reference_irradiance is pi so a white surface renders at exactly 1.0, and the demo roughness of 0.49 peaks at 0.8676, just under; change that one number and the same shading equation returns 11.59 at roughness 0.20 and 55,917 for a polished metal, all stored as code 255. Exposure derived as a photographic EV scale, with the engine's previous behaviour located at EV -0.263 rather than treated as a special case. Reinhard derived in one line and its white-point variant from requiring f(W)=1; ACES named honestly as a five-constant fit. The three properties that make an operator legal. Per-channel against luminance-only, and why the second does not solve the problem it appears to. A full-screen resolve pass with no vertex buffer - one triangle, not two, and 4.1 explains why. What an HDR target does to 6.11 rule: compositing gets 8.1x cheaper because the round trip was a cost of storing something other than light. 45 checks, CPU and GPU agreeing to the code; golden byte-identical for the twenty-first lesson, structurally.">

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
    <a class="prev-l" href="06-11-transparency.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.11 — Transparency: Alpha Modes, Blending, and Draw Order</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-13-bloom-post-stack.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.13 — Bloom and the Post-Processing Stack</span>
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
        with open(f"scratch/l612_body_{name}.html") as fh:
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
