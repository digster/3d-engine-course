#!/usr/bin/env python3
"""Assemble docs/lessons/04-06-uniforms.html.

The prose lives in scratch/l46_body_{a,b}.html, the figures are computed by
scratch/figs_46.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.

Figure placeholders are named by CONTENT, not by file number, because the figures
were authored in one order and appear in another.
"""
import os
import re

OUT = "docs/lessons/04-06-uniforms.html"

FIGURES = {
    "FIG_RATES": ("l46_fig1.svg", "1",
        "Three kinds of data in one frame, and how often each is written. Vertex data goes down "
        "8,575 times — 1,225 vertices for each of seven instances — and instance data seven "
        "times; the camera is written <strong>once</strong>. The scale is logarithmic because "
        "the three span four orders of magnitude. Putting a view-projection matrix in the vertex "
        "buffer would mean writing the same sixteen floats 1,225 times per instance, and putting "
        "it in the instance buffer would be better arithmetic making a false claim &#8212; the "
        "camera is not a property of an instance."),

    "FIG_PUSH": ("l46_fig2.svg", "2",
        "What a push actually is. Calls recorded into one command buffer, left to right: a push "
        "sets the uniform data every draw recorded after it will read, so the second draw sees "
        "77 where the first saw 201 &#8212; measured, with nothing bound, rebound or released in "
        "between. The complete list of buffer usage flags below has no <code>UNIFORM</code> "
        "entry, because SDL_GPU has no uniform buffer object at all. Three simplifications "
        "follow: no lifetime, no synchronisation hazard, and ordering as the only rule."),

    "FIG_PACKING": ("l46_fig3.svg", "3",
        "Bytes 64 to 111 of one uniform block, with the 16-byte register boundaries drawn. C++ "
        "and the compiled shader agree about the first four fields and diverge on the last: C++ "
        "places the trailing <code>float3</code> at byte 92, and the compiler moved it to 96 "
        "because 92 through 103 would straddle the boundary. Measured, having written (241, 242, "
        "243), the naive struct delivers <strong>(242, 243, 0)</strong> &#8212; shifted by one "
        "float with a zero on the end &#8212; while every field <em>before</em> the divergence "
        "arrives intact. A uniform layout bug corrupts the tail."),

    "FIG_MATRIX": ("l46_fig4.svg", "4",
        "Sixteen floats through the uniform boundary, checked one at a time. Our <code>mat4</code> "
        "stores four columns contiguously; pushing a matrix whose element at written "
        "<em>(row, col)</em> is 16&#183;row + col + 1 and asking the shader to report "
        "<code>m[row][col]</code> returns exactly that table. <strong>No transpose anywhere</strong> "
        "&#8212; <code>memcpy</code> is the whole conversion, and the claim Lesson 2.6 made two "
        "modules before anything could test it survives contact. The compiled SPIR-V decorates "
        "the member <code>RowMajor</code>, which looks like the opposite and is an artefact of "
        "the toolchain's own vocabulary."),

    "FIG_SPACE": ("l46_fig5.svg", "5",
        "The register spaces SDL fixes per stage, and which tool notices a mistake. Putting a "
        "fragment <code>cbuffer</code> in <code>space0</code> compiles to SPIR-V without "
        "complaint and produces a <strong>byte-identical</strong> JSON reflection &#8212; so the "
        "cross-check that saved Lesson 4.5 is no help at all. The disassembler shows it, and the "
        "translation to MSL refuses it outright with an exact message. Lesson 4.3's argument for "
        "compiling offline paying off from an unexpected direction: a would-be black screen "
        "becomes a build error on your own machine."),

    "FIG_SCENE": ("l46_fig6.svg", "6",
        "The frame, rendered offscreen and downloaded so it can be counted rather than described. "
        "The geometry is Lesson 4.5's and has not moved on the device; what changed is that 64 "
        "bytes of camera and 32 of lighting now arrive each frame, so the viewpoint is something "
        "the program decides rather than something the shader was compiled with. The shadowed "
        "sides are blue because the ambient term is tinted by a sky colour &#8212; one multiply, "
        "and a one-sample approximation of the hemisphere that Module 6 replaces properly."),
}

LISTING_META = {
    "src/gfx/gpu_uniform.hpp": ("new", "new"),
    "shaders/uniform_probe.vert.hlsl": ("new", "new"),
    "shaders/uniform_probe.frag.hlsl": ("new", "new"),
    "shaders/mesh.vert.hlsl": ("modified", "modified"),
    "shaders/mesh.frag.hlsl": ("modified", "modified"),
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
# The contents are pinned from commit 264335c — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "src/gfx/gpu_uniform.hpp":         "scratch/l46_src_gfx_gpu_uniform.hpp",
    "shaders/uniform_probe.vert.hlsl": "scratch/l46_shaders_uniform_probe.vert.hlsl",
    "shaders/uniform_probe.frag.hlsl": "scratch/l46_shaders_uniform_probe.frag.hlsl",
    "shaders/mesh.vert.hlsl":          "scratch/l46_shaders_mesh.vert.hlsl",
    "shaders/mesh.frag.hlsl":          "scratch/l46_shaders_mesh.frag.hlsl",
    "src/main.cpp":                    "scratch/l46_src_main.cpp",
    "CMakeLists.txt":                  "scratch/l46_CMakeLists.txt",
}

LISTING_LANG = {
    "CMakeLists.txt": ("cmake", "CMake"),
    "shaders/mesh.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/mesh.frag.hlsl": ("hlsl", "HLSL"),
    "shaders/uniform_probe.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/uniform_probe.frag.hlsl": ("hlsl", "HLSL"),
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
<title>4.6 — Uniform Data and the Matrix Upload · Build a Professional 3D Game Engine</title>
<meta name="description" content="Data that is the same for every vertex of a draw. SDL_GPU has no uniform buffer object — you push bytes onto a command buffer. Which byte of your struct the shader actually reads, measured; whether a matrix arrives transposed, measured; and which of four tools notices a wrong register space.">

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
    <a class="prev-l" href="04-05-vertex-buffers.html">
      <span class="dir">← Previous</span>
      <span class="ttl">4.5 — Vertex Buffers and Layouts</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="04-07-textures-and-depth.html">
      <span class="dir">Next →</span>
      <span class="ttl">4.7 — Textures, Samplers, and Depth</span>
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
    with open("scratch/l46_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l46_body_b.html") as fh:
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
