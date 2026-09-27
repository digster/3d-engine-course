#!/usr/bin/env python3
"""Assemble docs/lessons/04-05-vertex-buffers.html.

The prose lives in scratch/l45_body_{a,b}.html, the figures are computed by
scratch/figs_45.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.

Figure placeholders are named by CONTENT, not by file number, because the figures
were authored in one order and appear in another.
"""
import os
import re

OUT = "docs/lessons/04-05-vertex-buffers.html"

FIGURES = {
    "FIG_LAYOUT": ("l45_fig1.svg", "1",
        "The same 39,200 bytes, arranged two ways. <strong>Interleaved</strong>, one vertex is "
        "thirty-two contiguous bytes, so a 64-byte cache line holds exactly two whole vertices "
        "and a vertex fetch is one memory transaction. <strong>Separate</strong>, one "
        "<em>attribute</em> is contiguous, and a vertex is twelve bytes here, twelve bytes far "
        "away and eight somewhere else again. The two measurements at the bottom are why this is "
        "a trade rather than a rule: fetching a whole vertex costs 1 line interleaved and up to "
        "5 separate, while a pass that reads only positions costs 613 lines interleaved and 230 "
        "separate."),

    "FIG_CONTRACT": ("l45_fig2.svg", "2",
        "The vertex declared three times &#8212; in HLSL, in the JSON reflection derived from it, "
        "and in the C++ pipeline description &#8212; joined by nothing but a location number. The "
        "middle column is the one this lesson starts using: <code>shadercross</code> has been "
        "emitting it since Lesson 4.3 and we were reading only the resource counts. Below, six "
        "layouts handed to pipeline creation. <strong>SDL refuses exactly one</strong>, and it is "
        "the one that would have been obvious anyway; the four silent ones are the four that draw "
        "a plausible wrong picture."),

    "FIG_PITCH": ("l45_fig3.svg", "5",
        "Actual pixels, downloaded from the device. Three pipelines that differ in one integer "
        "and in nothing else &#8212; same shader, same buffer, same attributes. A pitch of 32 "
        "draws a torus covering 3,696 pixels; 28 or 36 draws a shattered cloud covering about "
        "5,000, because coverage goes <em>up</em> when geometry sprays outward. The strip below "
        "is why it shatters rather than shifts: the address of vertex <em>i</em> is the buffer "
        "plus <em>i</em> times the pitch, so a four-byte error is four bytes at vertex 1 and "
        "4,896 by the end of the mesh &#8212; and is exactly zero at vertex 0, which is how this "
        "bug survives a three-vertex test."),

    "FIG_INDEX": ("l45_fig4.svg", "3",
        "What the index buffer buys, on our own torus rather than on a textbook cube. A vertex "
        "where six triangles meet is stored once and named six times; across the mesh the average "
        "vertex is named 5.64 times. The indexed form is 53,024 bytes against 221,184 &#8212; "
        "<strong>4.17&#215;</strong> &#8212; and note that the saving of 181,984 bytes is far "
        "larger than the 13,824 the index buffer costs, because an index is two bytes and a "
        "vertex is thirty-two. Below, the second saving: an expanded draw invokes the vertex "
        "shader 6,912 times with no possibility of doing better, while an indexed draw invokes it "
        "somewhere between 1,225 and 6,912 depending on how well the post-transform cache "
        "catches the reuse."),

    "FIG_RATE": ("l45_fig5.svg", "4",
        "Instancing, in full. Slot 0 advances one element per vertex; slot 1 advances one element "
        "per <em>instance</em>, so all 1,225 vertices of instance 1 read placement record 1. "
        "Nothing else differs &#8212; same buffer type, same usage bit, same "
        "<code>SDL_BindGPUVertexBuffers</code>, same attribute machinery &#8212; and the shader "
        "cannot tell which of its six inputs came from which. Below, the number that makes the "
        "case: 53,024 bytes of torus uploaded once at startup, against 196 bytes of placement "
        "rewritten every frame."),

    "FIG_SCENE": ("l45_fig6.svg", "6",
        "The frame, rendered offscreen and downloaded so that it can be counted rather than "
        "described. Seven tori, each at its own place, its own angle and its own tint, from "
        "<strong>one vertex buffer, one index buffer and one draw call</strong> &#8212; "
        "<code>SDL_DrawGPUIndexedPrimitives(pass, 6912, 7, 0, 0, 0)</code>. They cover 32,006 of "
        "the target's 147,456 pixels, entirely inside the frame. The faint grid on each surface "
        "is the third vertex attribute, the texture coordinate, made visible: nothing samples a "
        "texture until Lesson 4.7, and an attribute you cannot see is an attribute you cannot "
        "debug."),
}

LISTING_META = {
    "src/gfx/gpu_mesh.hpp": ("new", "new"),
    "src/gfx/gpu_mesh.cpp": ("new", "new"),
    "shaders/mesh.vert.hlsl": ("new", "new"),
    "shaders/mesh.frag.hlsl": ("new", "new"),
    "src/gfx/gpu_buffer.hpp": ("modified", "modified"),
    "src/gfx/gpu_buffer.cpp": ("modified", "modified"),
    "src/gfx/gpu_pipeline.hpp": ("modified", "modified"),
    "src/gfx/gpu_pipeline.cpp": ("modified", "modified"),
    "src/gfx/gpu_shader.hpp": ("modified", "modified"),
    "src/gfx/gpu_shader.cpp": ("modified", "modified"),
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
# The contents are pinned from commit aa27e74 — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "src/gfx/gpu_mesh.hpp":     "scratch/l45_src_gfx_gpu_mesh.hpp",
    "src/gfx/gpu_mesh.cpp":     "scratch/l45_src_gfx_gpu_mesh.cpp",
    "shaders/mesh.vert.hlsl":   "scratch/l45_shaders_mesh.vert.hlsl",
    "shaders/mesh.frag.hlsl":   "scratch/l45_shaders_mesh.frag.hlsl",
    "src/gfx/gpu_buffer.hpp":   "scratch/l45_src_gfx_gpu_buffer.hpp",
    "src/gfx/gpu_buffer.cpp":   "scratch/l45_src_gfx_gpu_buffer.cpp",
    "src/gfx/gpu_pipeline.hpp": "scratch/l45_src_gfx_gpu_pipeline.hpp",
    "src/gfx/gpu_pipeline.cpp": "scratch/l45_src_gfx_gpu_pipeline.cpp",
    "src/gfx/gpu_shader.hpp":   "scratch/l45_src_gfx_gpu_shader.hpp",
    "src/gfx/gpu_shader.cpp":   "scratch/l45_src_gfx_gpu_shader.cpp",
    "src/main.cpp":             "scratch/l45_src_main.cpp",
    "CMakeLists.txt":           "scratch/l45_CMakeLists.txt",
}

LISTING_LANG = {
    "CMakeLists.txt": ("cmake", "CMake"),
    "shaders/mesh.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/mesh.frag.hlsl": ("hlsl", "HLSL"),
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
<title>4.5 — Vertex Buffers and Layouts · Build a Professional 3D Game Engine</title>
<meta name="description" content="Real geometry on the device: interleaved vertex buffers, 16-bit index buffers and per-instance data. What a wrong pitch actually draws, measured; what an index buffer saves on a real mesh, in bytes and in vertex-shader invocations; and the layout cross-check that the reflection JSON has been able to perform since Lesson 4.3.">

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
    <a class="prev-l" href="04-04-first-triangle.html">
      <span class="dir">← Previous</span>
      <span class="ttl">4.4 — The First Triangle</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="04-06-uniforms.html">
      <span class="dir">Next →</span>
      <span class="ttl">4.6 — Uniform Data and the Matrix Upload</span>
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
    with open("scratch/l45_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l45_body_b.html") as fh:
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
