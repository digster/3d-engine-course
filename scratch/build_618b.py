#!/usr/bin/env python3
"""Assemble docs/lessons/06-18b-compute-particles.html.

Same pipeline as build_617b.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l618b_body_{a,b,c,d}.html, figures from scratch/figs_618b.py, and
EVERY code listing read from a pin — because this is an INSERTED lesson (the
authoring guide's §17). The working tree already holds Module 7 and Module 8, so its
build lists are not what a student has at 6.18b. `scratch/make_t618b.py tree
scratch/_t618 scratch/_t618b --pins` writes each pin as the file after 6.18
(`scratch/replay_tree.py 6.18`) plus 6.18b's own edits, and proves every one by
delta; new files are the working tree's text; engine.hpp, which has no context at
6.18 for its three lines, is proved by exact line accounting. The harness is copied
from the working tree.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-18b-compute-particles.html"

FIGURES = {
    "FIG_BUS": ("l618b_fig1.svg", "1",
        "Who owns the particles decides what crosses the bus. Before: the CPU steps the pool and "
        "ships all 48 bytes of every particle to the GPU every frame, whether or not it moved. "
        "After: the CPU sends one 144-byte uniform block per fixed step and the GPU steps, counts "
        "and draws. The table is <code>verify_618b</code> &#167;L against a Release library: the CPU "
        "step is cheap and flat at about 3.5&nbsp;ns a particle; the upload grows with the pool and "
        "overtakes it everywhere, and at a million particles the two together are 10.6&nbsp;ms of a "
        "16.7&nbsp;ms frame, against 0.39&nbsp;ms for the GPU to step the same pool."),

    "FIG_GRID": ("l618b_fig2.svg", "2",
        "A dispatch is sized in whole groups. 1,000 particles at 64 threads a group need sixteen "
        "groups, 1,024 threads, and the last 24 have no particle; without the kernel&#8217;s first "
        "line they write past the end of the data, which the probe measures as exactly 24 writes. "
        "Inside a group, <code>SV_DispatchThreadID</code> is the group index times 64 plus the "
        "thread&#8217;s place in it &#8212; the particle&#8217;s slot. And the textbook ceiling "
        "<code>(n + 63) / 64</code> wraps to zero groups at the top of the range, where "
        "<code>groups_for</code> does not."),

    "FIG_LAYOUT": ("l618b_fig3.svg", "3",
        "One struct, two layouts. The particle&#8217;s three 16-byte rows sit at the same offsets "
        "in C++ and on the GPU, read back to prove it. The tempting struct of two "
        "<code>float3</code>s and a <code>float</code> is 28 bytes under C++&#8217;s and HLSL&#8217;s "
        "packing and 32 under the SPIR-V rules this toolchain applies, which pad "
        "<code>velocity</code> to byte 16 &#8212; so C++ reading the GPU&#8217;s bytes sees a "
        "velocity of (0, 4, 5) and an age of 6 where the shader wrote (4, 5, 6) and 7."),

    "FIG_PAIRS": ("l618b_fig4.svg", "4",
        "Three ways to give 2,048 consecutive sparks two random numbers each, plotted as pairs. One "
        "LCG step seeded by the index is linear in the index, so every pair lies on one line and "
        "neighbours are correlated at +0.9977. The folklore sine hash fills the square but, past "
        "2<sup>24</sup>, gives neighbours the same input half the time (+0.4963), has only about "
        "four thousand distinct values from 65,536, and differs between the CPU and the GPU in "
        "480,168 of a million outputs. PCG&#8217;s hash fills the square with no structure and is "
        "bit-identical on both processors."),

    "FIG_CAP": ("l618b_fig5.svg", "5",
        "Looking down a cone&#8217;s axis at the cap of directions it contains. Rings at equal "
        "steps of angle have unequal areas, so drawing the angle uniformly crowds directions at the "
        "axis (left); drawing its cosine uniformly spreads them evenly by area (right). Drawn at "
        "40&#176; for legibility; the demo&#8217;s cone is 20&#176;, and its median direction is "
        "0.2469&nbsp;rad from the axis, not the 0.175 uniform-in-angle would give."),

    "FIG_RING": ("l618b_fig6.svg", "6",
        "Emission as a ring. Serial <em>s</em> lives in slot <em>s</em>&nbsp;&amp;&nbsp;mask, so a "
        "step&#8217;s births are a contiguous arc that overwrites the oldest slots &#8212; here "
        "serials 13 to 17 in a pool of sixteen, wrapping into slots 0 and 1. Every thread decides "
        "for itself whether its slot is in the arc, with no communication at all. On the right, "
        "<code>verify_618b</code> &#167;D: a power-of-two pool is continuous across the 32-bit "
        "counter&#8217;s wrap and a pool of 1,000 is not, and a pool smaller than rate &#215; "
        "longest life cuts particles short."),

    "FIG_SHELLS": ("l618b_fig7.svg", "7",
        "The first 0.4&nbsp;s of flight, side on, for an emitter whose speeds span only 6.9 to "
        "7&nbsp;m/s. Born all at the step&#8217;s end (left), each step&#8217;s particles travel "
        "together and the fountain comes out in shells one step&#8217;s travel apart; born at their "
        "own instants within the step (right), they do not. With the demo&#8217;s 4 to "
        "7&nbsp;m/s the two are equally smooth &#8212; the spread of speeds erases the shells by "
        "itself, which the measurement found and the first prediction did not."),

    "FIG_BOUNCE": ("l618b_fig8.svg", "8",
        "A spark dropped from 1&nbsp;m, restitution 0.45, stepped at the engine&#8217;s "
        "1/60&nbsp;s (blue) and at 1/240&nbsp;s (green). Each bounce should keep "
        "<em>e</em>&#178;&nbsp;=&nbsp;0.2025 of the height; at 1/60 the ratios are 0.185, 0.172 and "
        "0.103, because a semi-implicit step reaches half a step&#8217;s travel short of the true "
        "peak, and that shortfall is a larger fraction of a smaller bounce. At a quarter of the "
        "step the ratios move toward <em>e</em>&#178;: the step&#8217;s error, not a bug."),

    "FIG_BILLBOARD": ("l618b_fig9.svg", "9",
        "A billboard needs no vertex buffer. The instance ID indexes the list of living slots, the "
        "slot indexes the pool, and the vertex ID picks one of six corners of two "
        "counter-clockwise triangles laid along the camera&#8217;s right and up &#8212; rows 0 and 1 "
        "of the view matrix, because a rotation&#8217;s inverse is its transpose. The worked numbers "
        "are the demo&#8217;s shot camera, computed by the engine&#8217;s own <code>look_at</code>."),

    "FIG_RACE": ("l618b_fig10.svg", "10",
        "Why counting needs an atomic. A plain <code>words[0] = words[0] + 1</code> is a read, an add "
        "and a write, and two lanes that read the same value write the same result: one increment "
        "is lost. With a million threads, 296 to 509 increments survived across fifteen trials. "
        "<code>InterlockedAdd</code> makes the three steps one and hands each lane the value it "
        "replaced &#8212; which is that lane&#8217;s place in a list, and the whole of how the "
        "compaction works."),

    "FIG_COUNT": ("l618b_fig11.svg", "11",
        "The count of the living never reaches the CPU. A one-thread pass writes the draw&#8217;s "
        "indirect arguments as {6, 0, 0, 0}; the compaction adds one per living particle to the "
        "second word with an atomic and writes each slot into the list at the place it received; "
        "the draw reads all four words when the GPU reaches it. The list&#8217;s order is the "
        "atomics&#8217; and changes every frame, and the histogram shows what that does to the "
        "picture: the last bits of a half-float, never more than a few units, with the total light "
        "unchanged to a part in ten thousand."),

    "FIG_CYCLE": ("l618b_fig12.svg", "12",
        "What <code>cycle&nbsp;=&nbsp;true</code> does to a buffer the next pass reads, measured "
        "with a fresh pool and a readback after every step. Submitted back to back, steps 1 to 3 "
        "each began from an empty buffer and steps 4 to 6 carried on from the buffers of steps 1 to 3 "
        "&#8212; the pool became three pools. With a fence wait after every step, every step began "
        "from an empty buffer. Over 120 steps only <code>cycle&nbsp;=&nbsp;false</code>, or "
        "<code>SDL_WaitForGPUIdle</code> after every step, keeps the CPU&#8217;s 38,284 particles."),

    "FIG_GRAPH": ("l618b_fig13.svg", "13",
        "The demo&#8217;s frame as the graph sees it: seven passes against five resources, each "
        "cell a declared access with its version. Everything in amber was derived rather than "
        "written: <code>cycle</code> is <em>no</em> for every access that keeps what was there "
        "(the pool, the accumulated arguments) and <em>yes</em> for every one that overwrites (the "
        "cleared arguments, the list); the order follows the versions; and the HDR target, being "
        "imported, is stored for the bloom that reads it after the graph."),

    "FIG_AGREE": ("l618b_fig14.svg", "14",
        "The GPU held to the CPU&#8217;s specification for 600 steps. The integers agree exactly at "
        "every checkpoint &#8212; every serial in every slot, every alive/dead decision, the count, "
        "a million hashes. The floats agree to a tolerance: the worst living particle is "
        "1.67&nbsp;&#181;m from its twin at two seconds and 1.38&nbsp;&#181;m at ten, and only 12% "
        "are bit-identical. The CPU reference is no more exact against itself: its debug and "
        "Release builds differ on 897 of 44,915 particles."),

    "FIG_BUDGET": ("l618b_fig15.svg", "15",
        "The budget against pool size, Release library, log-log. The CPU step (blue) grows "
        "linearly at about 3.5&nbsp;ns a slot; the upload the CPU path needs (purple) is larger at "
        "every size; together (dashed) they reach 10.6&nbsp;ms at a million. The GPU step (amber), "
        "launch cost included, stays under 0.4&nbsp;ms. SDL_GPU has no timestamp queries, so the "
        "GPU column is wall-clock time around a fence: an upper bound."),

    "FIG_DEMO": ("l618b_fig16.svg", "16",
        "<code>particles --shot</code>: 65,536 slots, 2.5&nbsp;s into the fountain, simulated, "
        "counted and drawn on the GPU, with the frame declared through the frame graph. Warm "
        "sparks rise white from their core, cool to orange as they fall, bloom where they are "
        "densest and spray across the floor; the metal torus on the right hides the sparks that "
        "fall behind it. The CPU path draws the same picture, to within one code."),
}

LISTING_META = {
    "engine/include/engine/gfx/particles.hpp": ("new", "new"),
    "engine/src/gfx/particles.cpp": ("new", "new"),
    "engine/include/engine/gfx/gpu_compute.hpp": ("new", "new"),
    "engine/src/gfx/gpu_compute.cpp": ("new", "new"),
    "engine/include/engine/gfx/gpu_shader.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_shader.cpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_particles.hpp": ("new", "new"),
    "engine/src/gfx/gpu_particles.cpp": ("new", "new"),
    "engine/include/engine/gfx/gpu_pipeline.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/frame_graph.hpp": ("modified", "modified"),
    "engine/src/gfx/frame_graph.cpp": ("modified", "modified"),
    "shaders/particles_step.comp.hlsl": ("new", "new"),
    "shaders/particles_clear.comp.hlsl": ("new", "new"),
    "shaders/particles_compact.comp.hlsl": ("new", "new"),
    "shaders/particles_probe.comp.hlsl": ("new", "new"),
    "shaders/particle.vert.hlsl": ("new", "new"),
    "shaders/particle.frag.hlsl": ("new", "new"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "engine/include/engine/engine.hpp": ("modified", "modified"),
    "CMakeLists.txt": ("modified", "modified"),
    "demos/CMakeLists.txt": ("modified", "modified"),
    "demos/particles/main.cpp": ("new", "new"),
    "scratch/verify_618b.cpp": ("new", "new"),
}

LISTING_LANG = {
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
    "shaders/particles_step.comp.hlsl": ("hlsl", "HLSL"),
    "shaders/particles_clear.comp.hlsl": ("hlsl", "HLSL"),
    "shaders/particles_compact.comp.hlsl": ("hlsl", "HLSL"),
    "shaders/particles_probe.comp.hlsl": ("hlsl", "HLSL"),
    "shaders/particle.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/particle.frag.hlsl": ("hlsl", "HLSL"),
}

LISTING_SOURCE = {
    # PINNED from the first build, because an inserted lesson's files are not the
    # working tree's (see the module docstring). Every pin but the harness was
    # written by `make_t618b.py tree ... --pins` and proved; the harness is
    # gitignored-by-directory and was copied from the working tree.
    "CMakeLists.txt":                              "scratch/l618b_CMakeLists.txt",
    "demos/CMakeLists.txt":                        "scratch/l618b_demos_CMakeLists.txt",
    "engine/CMakeLists.txt":                       "scratch/l618b_engine_CMakeLists.txt",
    "engine/include/engine/engine.hpp":            "scratch/l618b_engine_include_engine_engine.hpp",
    "engine/include/engine/gfx/frame_graph.hpp":   "scratch/l618b_engine_include_engine_gfx_frame_graph.hpp",
    "engine/src/gfx/frame_graph.cpp":              "scratch/l618b_engine_src_gfx_frame_graph.cpp",
    "engine/include/engine/gfx/gpu_shader.hpp":    "scratch/l618b_engine_include_engine_gfx_gpu_shader.hpp",
    "engine/src/gfx/gpu_shader.cpp":               "scratch/l618b_engine_src_gfx_gpu_shader.cpp",
    "engine/include/engine/gfx/gpu_pipeline.hpp":  "scratch/l618b_engine_include_engine_gfx_gpu_pipeline.hpp",
    "engine/include/engine/gfx/particles.hpp":     "scratch/l618b_engine_include_engine_gfx_particles.hpp",
    "engine/src/gfx/particles.cpp":                "scratch/l618b_engine_src_gfx_particles.cpp",
    "engine/include/engine/gfx/gpu_compute.hpp":   "scratch/l618b_engine_include_engine_gfx_gpu_compute.hpp",
    "engine/src/gfx/gpu_compute.cpp":              "scratch/l618b_engine_src_gfx_gpu_compute.cpp",
    "engine/include/engine/gfx/gpu_particles.hpp": "scratch/l618b_engine_include_engine_gfx_gpu_particles.hpp",
    "engine/src/gfx/gpu_particles.cpp":            "scratch/l618b_engine_src_gfx_gpu_particles.cpp",
    "shaders/particles_step.comp.hlsl":            "scratch/l618b_shaders_particles_step.comp.hlsl",
    "shaders/particles_clear.comp.hlsl":           "scratch/l618b_shaders_particles_clear.comp.hlsl",
    "shaders/particles_compact.comp.hlsl":         "scratch/l618b_shaders_particles_compact.comp.hlsl",
    "shaders/particles_probe.comp.hlsl":           "scratch/l618b_shaders_particles_probe.comp.hlsl",
    "shaders/particle.vert.hlsl":                  "scratch/l618b_shaders_particle.vert.hlsl",
    "shaders/particle.frag.hlsl":                  "scratch/l618b_shaders_particle.frag.hlsl",
    "demos/particles/main.cpp":                    "scratch/l618b_demos_particles_main.cpp",
    "scratch/verify_618b.cpp":                     "scratch/l618b_scratch_verify_618b.cpp",
}

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
<title>6.18b — Compute Shaders: GPU Particles · Build a Professional 3D Game Engine</title>
<meta name="description" content="The engine's first compute shaders, taught through a particle system of 65,536 sparks that the GPU creates, steps, counts and draws without a byte crossing back to the CPU. A dispatch sized in thread groups and the tail every kernel guards; a particle record in three 16-byte rows, because two float3s in a row are 28 bytes to C++ and HLSL and 32 to the SPIR-V path - read back from the GPU to prove it; randomness as a PCG hash of a particle's name, bit-identical on both processors, against three popular generators that are correlated, short of bits or not one function; emission as a clock that keeps its fraction and a ring that needs no atomics, with births placed within their step; semi-implicit Euler with implicit drag and a floor; billboards drawn with no vertex buffer; the count of the living produced by an atomic and consumed by an indirect draw; what SDL_GPU synchronises, and cycle as a buffer's load op, measured turning one pool into three; and the frame graph extended with buffers and compute passes, deriving that flag. A CPU specification holds the GPU to exact integers and to floats within 1.67 micrometres over ten seconds; a million-particle step costs 0.39 ms against 10.6 ms for the CPU to step it and ship it.">

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
    <a class="prev-l" href="06-18-text-overlay.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.18 — Text and 2D Overlay Rendering</span>
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
    for name in ("a", "b", "c", "d"):
        with open(f"scratch/l618b_body_{name}.html") as fh:
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
