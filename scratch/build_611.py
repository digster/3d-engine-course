#!/usr/bin/env python3
"""Assemble docs/lessons/06-11-transparency.html.

Same pipeline as build_610.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l611_body_{a,b,c}.html, figures from scratch/figs_611.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-11-transparency.html"

FIGURES = {
    "FIG_MODES": ("l611_fig1.svg", "1",
        "One glTF enum, three different machines &#8212; and they land on OPPOSITE SIDES of the "
        "line Lesson 6.5 drew through <code>material</code>. <strong>MASK is a number in a "
        "buffer</strong>: the fragment compares its own coverage against a uniform and either "
        "exists or does not, nothing about the pipeline changes, and because a discarded fragment "
        "writes no depth it occludes nothing, so no draw order can be wrong. <strong>BLEND is "
        "pipeline state</strong>: blend factors, a blend op, and depth writes turned off. The "
        "clearest evidence the split is real rather than pedagogical is the bottom row &#8212; "
        "masking cost this engine <strong>zero</strong> new pipelines and blending cost "
        "<strong>six</strong>."),

    "FIG_OVER": ("l611_fig2.svg", "2",
        "<code>over</code>, derived from what alpha actually measures. A fraction <em>a</em> of "
        "the pixel&#8217;s AREA is source and <em>1&#8722;a</em> is what was already there, so the "
        "honest colour is the area-weighted average &#8212; which is a LERP, and Lesson 2.4 "
        "settled where a lerp has to happen. The worked case is the one to memorise: half-coverage "
        "white over black is half the LIGHT, which stores as code <strong>188</strong>, while "
        "lerping the stored bytes gives code <strong>128</strong> and emits 0.2159 &#8212; "
        "<strong>42.9%</strong> of the light the composite should have."),

    "FIG_SPACE": ("l611_fig3.svg", "3",
        "The same error swept over the whole range, which is why it survives review. The two "
        "curves TOUCH AT BOTH ENDS &#8212; at <em>a</em> = 0 the answer is the destination and at "
        "<em>a</em> = 1 it is the source, exact in either space, because no averaging happened "
        "&#8212; so a fade starts correctly, finishes correctly, and is wrong only in the middle "
        "where it is fastest. The worst gap is <strong>0.2898 of full white at a = 0.55</strong>, "
        "over a fifth of the display&#8217;s entire range. Note that the wrong curve IS the sRGB "
        "transfer function, arrived at by accident: lerping codes and reading the result as light "
        "is exactly applying the curve to the coverage."),

    "FIG_PREMUL": ("l611_fig4.svg", "4",
        "Why a cutout gets a dark outline. A bilinear filter averages four texels &#8212; colour "
        "and alpha independently, with the same weights &#8212; and the two &#8220;empty&#8221; "
        "texels still hold something in their colour channels, which is almost always black "
        "because that is what an image editor leaves in a cleared region. Straight alpha drags "
        "that invisible black into the visible edge at full weight, halving the leaf&#8217;s green "
        "(<strong>0.2636</strong> where the leaf is <strong>0.5271</strong>). Weighting by "
        "coverage &#8212; which is arithmetically what premultiplying, averaging and dividing back "
        "out does &#8212; recovers it exactly. <strong>Premultiplied is the representation in "
        "which averaging is already correct</strong>, which is what it is really for; the saved "
        "multiply is incidental."),

    "FIG_COVERAGE": ("l611_fig5.svg", "5",
        "Foliage that thins out as it recedes, measured down a real chain. Every downsample pulls "
        "the alpha toward its local mean, and a fixed cutoff then rejects a growing fraction: by "
        "level 6 an ordinary chain passes <strong>0.2500</strong> where the source passed "
        "<strong>0.3635</strong> &#8212; <strong>31.2%</strong> of the leaves gone, and nothing in "
        "the chain is wrong. AVERAGING AND THRESHOLDING DO NOT COMMUTE. The rescale (Casta&#241;o "
        "2010) holds the source&#8217;s coverage to within 0.0115 down to 8&#215;8, and then stops "
        "&#8212; a 4&#215;4 level has sixteen texels, so its coverage can only be a multiple of "
        "1/16 and 0.3635 is not one of them."),

    "FIG_ORDER": ("l611_fig6.svg", "6",
        "The sort, and where sorting runs out. The partition is STABLE and its predicate is "
        "<code>mode != blend</code> rather than <code>mode == opaque</code>, which is the line "
        "most likely to be written backwards: masked geometry belongs with the opaque geometry, "
        "because a discarded fragment occludes nothing. The tail is sorted by AXIAL depth &#8212; "
        "6.9&#8217;s distinction, arriving a second time, and worth 16.6% at the corner of a "
        "frame. And then the ceiling: for two INTERSECTING blended quads, red is in front on one "
        "side of the crossing and blue on the other, so the correct picture needs both orders at "
        "once. That is not a better sort waiting to be written; it is what OIT is for."),
}

# THE SECOND TUPLE ELEMENT IS THE STATUS WORD, NOT THE LANGUAGE. `listing()`
# unpacks these as `tag, word` and emits `<span class="tag {tag}">{word}</span>`,
# so a language here puts "cpp" in a pill that every other lesson fills with
# "new" or "modified". The LANGUAGE belongs in LISTING_LANG, which already
# carries it and which drives the separate `.lang` span. Ran from 6.9 to 6.12
# before anybody looked at a pill.
LISTING_META = {
    "engine/include/engine/gfx/blend.hpp": ("new", "new"),
    "engine/src/gfx/blend.cpp": ("new", "new"),
    "engine/include/engine/gfx/draw_order.hpp": ("new", "new"),
    "engine/src/gfx/draw_order.cpp": ("new", "new"),
    "scratch/verify_611.cpp": ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gltf.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_pipeline.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_scene.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_uniform.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/material.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/mipmap.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/raster.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/texture.hpp": ("modified", "modified"),
    "engine/src/asset/asset_store.cpp": ("modified", "modified"),
    "engine/src/gfx/gltf.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_pipeline.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_scene.cpp": ("modified", "modified"),
    "engine/src/gfx/mipmap.cpp": ("modified", "modified"),
    "engine/src/gfx/raster.cpp": ("modified", "modified"),
    "engine/src/gfx/texture.cpp": ("modified", "modified"),
    "shaders/scene.frag.hlsl": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "demos/gltf_view/main.cpp": ("modified", "modified"),
}

LISTING_LANG = {
    "shaders/scene.frag.hlsl": ("hlsl", "HLSL"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "shaders/scene.frag.hlsl": ("hlsl", "HLSL"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
}

LISTING_SOURCE = {
    # PINNED at the start of 6.12's session, exactly as the note below asked —
    # and the prediction in it was right: `blend_over` re-encodes through
    # `to_encoded`, which CLAMPS into [0,1], and 6.12's whole subject is that
    # values above 1 stop being an error. blend.{hpp,cpp} move under this page's
    # feet in this very lesson.
    "engine/include/engine/gfx/blend.hpp":      "scratch/l611_blend.hpp",
    "engine/src/gfx/blend.cpp":                 "scratch/l611_blend.cpp",
    "engine/include/engine/gfx/draw_order.hpp": "scratch/l611_draw_order.hpp",
    "engine/src/gfx/draw_order.cpp":            "scratch/l611_draw_order.cpp",
    "scratch/verify_611.cpp":                   "scratch/l611_verify_611.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gltf.hpp": "scratch/l611_engine_include_engine_gfx_gltf.hpp",
    "engine/include/engine/gfx/gpu_pipeline.hpp": "scratch/l611_engine_include_engine_gfx_gpu_pipeline.hpp",
    "engine/include/engine/gfx/gpu_scene.hpp": "scratch/l611_engine_include_engine_gfx_gpu_scene.hpp",
    "engine/include/engine/gfx/gpu_uniform.hpp": "scratch/l611_engine_include_engine_gfx_gpu_uniform.hpp",
    "engine/include/engine/gfx/material.hpp": "scratch/l611_engine_include_engine_gfx_material.hpp",
    "engine/include/engine/gfx/mipmap.hpp": "scratch/l611_engine_include_engine_gfx_mipmap.hpp",
    "engine/include/engine/gfx/raster.hpp": "scratch/l611_engine_include_engine_gfx_raster.hpp",
    "engine/include/engine/gfx/texture.hpp": "scratch/l611_engine_include_engine_gfx_texture.hpp",
    "engine/src/asset/asset_store.cpp": "scratch/l611_engine_src_asset_asset_store.cpp",
    "engine/src/gfx/gltf.cpp": "scratch/l611_engine_src_gfx_gltf.cpp",
    "engine/src/gfx/gpu_pipeline.cpp": "scratch/l611_engine_src_gfx_gpu_pipeline.cpp",
    "engine/src/gfx/gpu_scene.cpp": "scratch/l611_engine_src_gfx_gpu_scene.cpp",
    "engine/src/gfx/mipmap.cpp": "scratch/l611_engine_src_gfx_mipmap.cpp",
    "engine/src/gfx/raster.cpp": "scratch/l611_engine_src_gfx_raster.cpp",
    "engine/src/gfx/texture.cpp": "scratch/l611_engine_src_gfx_texture.cpp",
    "shaders/scene.frag.hlsl": "scratch/l611_shaders_scene.frag.hlsl",
    "engine/CMakeLists.txt": "scratch/l611_engine_CMakeLists.txt",
    "demos/gltf_view/main.cpp": "scratch/l611_demos_gltf_view_main.cpp",
}

# NOTHING PINNED YET, and this page lists FIVE files whole.
#
# Lesson 6.12 is HDR and tonemapping, which changes what a colour target IS —
# a float format instead of an 8-bit one — and therefore reaches straight into
# the two files this page is proudest of. `blend.hpp`/`blend.cpp` are near
# certain to move: `blend_over` clamps into [0,1] through `to_encoded`, and the
# entire point of 6.12 is that values above 1 stop being an error. `draw_order`
# is much less likely to be touched, but it is cheap to pin and expensive to
# reconstruct.
#
# So pin all four before writing a line of 6.12:
#
#     git show <6.11 commit>:<path> > scratch/l611_<name>
#     git show <6.11 commit>:<path> | diff - scratch/l611_<name>     # must be empty
#
# And take `scratch/verify_611.cpp` EARLY — gitignored, so its only provenance
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
<title>6.11 — Transparency: Alpha Modes, Blending, and Draw Order · Build a Professional 3D Game Engine</title>
<meta name="description" content="Not a missing feature but a correctness gap in shipped code: gpu_pipeline.hpp has said no blending since 4.4, and 6.6's glTF importer ignores alphaMode entirely - the one gap in that importer which fires no status, because a status says the file wants what the engine cannot do and there was no blend state to compare against. Derives the over operator from coverage rather than opacity; measures what compositing in the wrong space costs (code 188 against code 128, 42.9 per cent of the light) and then catches the HARDWARE making the same mistake on a UNORM target, discharging a prediction 6.1 left in a shader comment. Alpha masking keeps the depth buffer honest and blending does not, which is the same dependency that costs a GPU its early-Z. Premultiplied alpha derived as the representation in which averaging is correct. A coverage-preserving mip chain, because averaging and thresholding do not commute. A back-to-front sort, and the intersecting quads for which no per-object order exists. Nine pipelines where there were three. 49 checks; golden byte-identical for the twentieth lesson, through a refactor.">

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
    <a class="prev-l" href="06-10-mipmaps.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.10 — Mipmaps, LOD, and Anisotropic Filtering</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-12-hdr-tonemapping.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.12 — HDR and Tonemapping</span>
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
        with open(f"scratch/l611_body_{name}.html") as fh:
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
