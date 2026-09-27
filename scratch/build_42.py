#!/usr/bin/env python3
"""Assemble docs/lessons/04-02-sdl-gpu-model.html.

The prose lives in scratch/l42_body_{a,b}.html, the figures are computed by
scratch/figs_42.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.

Figure placeholders are named by CONTENT, not by file number, because the figures
were authored in one order and appear in another.
"""
import os
import re

OUT = "docs/lessons/04-02-sdl-gpu-model.html"

FIGURES = {
    "FIG_TIMELINE": ("l42_fig2.svg", "1",
        "One command buffer, drawn to scale in time. The block on the CPU lane is every SDL "
        "call needed to describe 48 full-screen copies &#8212; acquire, 48 blits, submit &#8212; "
        "and it is <strong>0.1879&nbsp;ms</strong> wide. The block on the GPU lane is that work "
        "happening, at <strong>4.7815&nbsp;ms</strong>. Neither is drawn for effect; they are the "
        "same scale. Read the empty space to the right of the CPU block as the point of the "
        "figure: the processor that issued the work is free for 96% of the time the work takes, "
        "and an engine that does not spend that time is throwing away most of its frame. The "
        "dashed arrow returning is the fence &#8212; the only instant at which any of this work's "
        "results may be believed."),

    "FIG_HOPS": ("l42_fig3.svg", "2",
        "The route from a pixel you wrote to a pixel on the panel, and the dashed line is the "
        "part worth memorising. Left of it, calling a function does the thing; right of it, "
        "calling a function <em>writes the thing down</em>. The <code>memcpy</code> into the "
        "transfer buffer is real work happening now &#8212; measured at "
        "<strong>0.0028&nbsp;ms</strong> for a 320&#215;180 frame &#8212; while "
        "<code>SDL_UploadToGPUTexture</code> and <code>SDL_BlitGPUTexture</code> cost microseconds "
        "to record and run later, on the other processor. The middle box is the one with no "
        "counterpart in Modules 1&#8211;3: a texture is tiled and possibly compressed in a layout "
        "only the driver knows, so there has to be a plainly-arranged room in between."),

    "FIG_MODEL": ("l42_fig1.svg", "3",
        "SDL_GPU&#8217;s object model against the engine you have already written. Count the "
        "colours: <strong>five renames</strong> and <strong>four genuinely new objects</strong>. "
        "The five are not a coincidence &#8212; Lesson 3.9 built <code>engine::sampler</code> as "
        "<code>SDL_GPUSamplerCreateInfo</code> field for field, and 4.1 counted "
        "<code>fill_style</code>&#8217;s ten fields against the pipeline&#8217;s nine, precisely "
        "so that this page would be an observation rather than a leap. And the four with an empty "
        "box beside them are one idea wearing four names: there are two processors now. A device "
        "is the connection, a command buffer the message, a transfer buffer the shared ground, a "
        "fence the acknowledgement."),

    "FIG_CLEAR": ("l42_fig5.svg", "4",
        "What an <code>SDL_FColor</code> becomes in memory, measured by clearing a 1&#215;1 target "
        "and downloading the byte. The straight line is an <code>_UNORM</code> format, where the "
        "byte is the float times 255 and nothing else happens; the curve is "
        "<code>_UNORM_SRGB</code>, which applies the sRGB encode on write and the matching decode "
        "on read so that shaders always work in linear light. The same 0.5 lands at "
        "<strong>128</strong> or <strong>188</strong> depending only on the format of the thing "
        "you are clearing. All five marked points on each line are measurements, and the sRGB ones "
        "match <code>engine::linear_to_srgb_u8</code> &#8212; our own function from Lesson 1.6 "
        "&#8212; to the code."),

    "FIG_FRAME": ("l42_fig4.svg", "5",
        "Where one frame of the probe actually goes, averaged over 160 frames on a 60&nbsp;Hz "
        "display. Compare the first two bars, which differ only in whether the CPU waits on a "
        "fence every frame: the orange band appears, the blue acquire band shrinks by very nearly "
        "the same amount, and <strong>the bar ends in exactly the same place</strong>. A full GPU "
        "sync on a display-bound frame is absorbed by the wait that was already there. The third "
        "bar removes the display from the equation with IMMEDIATE present mode and the frame "
        "collapses to 4.816&nbsp;ms &#8212; of which the acquire is still 4.238, because something "
        "always paces you and the only question is what. The three variants were built as three "
        "binaries and run alternately in one session, twice."),

    "FIG_BANDWIDTH": ("l42_fig6.svg", "6",
        "The measurement that could not be true, and its repair. Both bars in each pair run the "
        "identical ping-ponged blits over the identical byte count; only the CONTENT differs. Flat "
        "colour reports up to <strong>797.9&nbsp;GB/s</strong> on a machine whose memory bus is "
        "<strong>273</strong> &#8212; impossible, because lossless render-target compression means "
        "the bytes were never moved. Incompressible noise converges on "
        "<strong>277.6&nbsp;GB/s</strong>, landing on the published figure, which is the "
        "strongest evidence a benchmark can offer. The gap between the pairs is not an error once "
        "you know what it is: it is the compressor, and it is why a cleared render target is "
        "cheaper to work with than a busy one."),
}

LISTING_META = {
    "src/gfx/gpu_device.hpp": ("new", "new"),
    "src/gfx/gpu_device.cpp": ("new", "new"),
    "src/gfx/gpu_present.hpp": ("new", "new"),
    "src/gfx/gpu_present.cpp": ("new", "new"),
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
# The contents are pinned from commit 11e912c — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "src/gfx/gpu_device.hpp":  "scratch/l42_src_gfx_gpu_device.hpp",
    "src/gfx/gpu_device.cpp":  "scratch/l42_src_gfx_gpu_device.cpp",
    "src/gfx/gpu_present.hpp": "scratch/l42_src_gfx_gpu_present.hpp",
    "src/gfx/gpu_present.cpp": "scratch/l42_src_gfx_gpu_present.cpp",
    "src/main.cpp":            "scratch/l42_src_main.cpp",
    "CMakeLists.txt":          "scratch/l42_CMakeLists.txt",
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
<title>4.2 — The SDL_GPU Mental Model · Build a Professional 3D Game Engine</title>
<meta name="description" content="Device, swapchain, command buffers, render passes, pipelines, buffers, textures and samplers — five of them renames of things you already built, and four new ones that all exist because a call now records work instead of performing it. Ends with your software rasterizer on screen, carried by the GPU, with no shaders.">

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
    <a class="prev-l" href="04-01-how-gpus-work.html">
      <span class="dir">← Previous</span>
      <span class="ttl">4.1 — How GPUs Actually Work</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="04-03-shader-toolchain.html">
      <span class="dir">Next →</span>
      <span class="ttl">4.3 — The Shader Toolchain</span>
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
    with open("scratch/l42_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l42_body_b.html") as fh:
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
