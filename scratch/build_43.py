#!/usr/bin/env python3
"""Assemble docs/lessons/04-03-shader-toolchain.html.

The prose lives in scratch/l43_body_{a,b}.html, the figures are computed by
scratch/figs_43.py, and the code listings are read straight out of the repository
so they cannot drift from what actually compiles. CLAUDE.md §8: if a file changed,
it appears whole, with zero placeholders.

Figure placeholders are named by CONTENT, not by file number, because the figures
were authored in one order and appear in another.
"""
import os
import re

OUT = "docs/lessons/04-03-shader-toolchain.html"

FIGURES = {
    "FIG_TOOLCHAIN": ("l43_fig1.svg", "1",
        "The toolchain, and the reason it has a hub. One source language enters on the left; "
        "everything on the right is translated <em>from SPIR-V</em> by SPIRV-Cross. The upper "
        "front end &#8212; shadercross built with DirectXShaderCompiler &#8212; is the sanctioned "
        "path and is drawn dashed because the machine this lesson was written on does not have "
        "it; <code>glslc -x hlsl</code> does the same hop and is what our CMake fell back to. "
        "Note where the work is: <strong>12.3&nbsp;ms</strong> for the front end against "
        "<strong>1.5</strong> for each translation. And note the consequence of the shape &#8212; "
        "lose the front end and you lose every output, not one, because there is no SPIR-V for "
        "the second stage to read."),

    "FIG_SPACES": ("l43_fig2.svg", "2",
        "Where a resource must be declared, per stage, fixed by <code>SDL_gpu.h</code>. The "
        "right-hand column is the part that makes this checkable rather than memorisable: a "
        "register space becomes a SPIR-V <em>descriptor set</em>, and the numbers shown were read "
        "out of this course&#8217;s own compiled shaders with "
        "<code>spirv-dis | grep DescriptorSet</code>. Getting one wrong is not a compile error "
        "and not a load error &#8212; the shader builds, loads and runs, and reads whatever "
        "happens to be bound at the slot it named."),

    "FIG_COUNTS": ("l43_fig4.svg", "3",
        "The four integers in shadercross&#8217;s reflection output are, one for one, the four "
        "counts <code>SDL_GPUShaderCreateInfo</code> demands. Below them, what SDL does when they "
        "are wrong, measured on a fragment shader that really has one sampler and one uniform "
        "buffer: <strong>every set of counts was accepted</strong>, including ninety-nine of "
        "each. Nothing validates these at creation. That is the argument for reflection &#8212; "
        "not that it saves typing, but that it is the only check there is."),

    "FIG_ENTRY": ("l43_fig6.svg", "4",
        "The name of your entry point, through the toolchain. You wrote <code>main</code>; the "
        "SPIR-V still calls it <code>main</code>; the MSL calls it <strong>main0</strong>, "
        "because <code>main</code> is reserved in Metal Shading Language and SPIRV-Cross renames "
        "it. The three measured trials underneath are the happy half of this lesson&#8217;s "
        "temperament: a wrong entry point is <strong>refused at creation</strong>, with a "
        "message, where a wrong resource count (Figure 3) sails straight through."),

    "FIG_SIZES": ("l43_fig3.svg", "5",
        "Every file in the toolchain, on one scale, in bytes. Compiled shaders are "
        "<em>small</em> &#8212; 1,100 bytes of SPIR-V at the largest here, and the MSL "
        "translations smaller still &#8212; and the reflection file that tells SDL how to bind "
        "them is the same order of size as the code it describes. The <code>.hlsl</code> bars "
        "lead only because this course&#8217;s shaders carry more comment than code, which is a "
        "fact about our commenting rather than about shaders."),

    "FIG_WHEN": ("l43_fig5.svg", "6",
        "When each part of the toolchain runs. Build time is <strong>61.2&nbsp;ms</strong> for "
        "four shaders through three hops, paid once per edit. Program start is "
        "<strong>0.028&nbsp;ms</strong> for all four <code>SDL_CreateGPUShader</code> calls "
        "&#8212; and that is the interesting number, because eight microseconds per shader cannot "
        "include compiling anything, and the cold and repeated measurements are identical so it "
        "is not a cache either. The third bar is empty on purpose: the real compile has not "
        "happened yet, and Lesson 4.4 goes looking for it."),
}

LISTING_META = {
    "shaders/triangle.vert.hlsl": ("new", "new"),
    "shaders/triangle.frag.hlsl": ("new", "new"),
    "shaders/textured.vert.hlsl": ("new", "new"),
    "shaders/textured.frag.hlsl": ("new", "new"),
    "cmake/Shaders.cmake": ("new", "new"),
    "src/gfx/gpu_shader.hpp": ("new", "new"),
    "src/gfx/gpu_shader.cpp": ("new", "new"),
    "src/gfx/gpu_device.hpp": ("modified", "modified"),
    "src/gfx/gpu_device.cpp": ("modified", "modified"),
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
# The contents are pinned from commit 2a807d2 — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "shaders/triangle.vert.hlsl": "scratch/l43_shaders_triangle.vert.hlsl",
    "shaders/triangle.frag.hlsl": "scratch/l43_shaders_triangle.frag.hlsl",
    "shaders/textured.vert.hlsl": "scratch/l43_shaders_textured.vert.hlsl",
    "shaders/textured.frag.hlsl": "scratch/l43_shaders_textured.frag.hlsl",
    "cmake/Shaders.cmake":        "scratch/l43_cmake_Shaders.cmake",
    "src/gfx/gpu_shader.hpp":     "scratch/l43_src_gfx_gpu_shader.hpp",
    "src/gfx/gpu_shader.cpp":     "scratch/l43_src_gfx_gpu_shader.cpp",
    "src/gfx/gpu_device.hpp":     "scratch/l43_src_gfx_gpu_device.hpp",
    "src/gfx/gpu_device.cpp":     "scratch/l43_src_gfx_gpu_device.cpp",
    "src/main.cpp":               "scratch/l43_src_main.cpp",
    "CMakeLists.txt":             "scratch/l43_CMakeLists.txt",
}

LISTING_LANG = {
    "CMakeLists.txt": ("cmake", "CMake"),
    "cmake/Shaders.cmake": ("cmake", "CMake"),
    "shaders/triangle.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/triangle.frag.hlsl": ("hlsl", "HLSL"),
    "shaders/textured.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/textured.frag.hlsl": ("hlsl", "HLSL"),
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
<title>4.3 — The Shader Toolchain · Build a Professional 3D Game Engine</title>
<meta name="description" content="One HLSL source into SPIR-V, MSL and DXIL through SDL_shadercross, wired into CMake with a configure-time capability probe — plus the register spaces SDL_GPU fixes, the four resource counts it never validates, and the entry point SPIRV-Cross renames behind your back.">

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
    <a class="prev-l" href="04-02-sdl-gpu-model.html">
      <span class="dir">← Previous</span>
      <span class="ttl">4.2 — The SDL_GPU Mental Model</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="04-04-first-triangle.html">
      <span class="dir">Next →</span>
      <span class="ttl">4.4 — The First Triangle</span>
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
    with open("scratch/l43_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l43_body_b.html") as fh:
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
