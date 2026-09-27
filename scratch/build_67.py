#!/usr/bin/env python3
"""Assemble docs/lessons/06-07-normal-mapping.html.

Same pipeline as build_66.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l67_body_{a,b,c}.html, figures from scratch/figs_67.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-07-normal-mapping.html"

FIGURES = {
    "FIG_PROBLEM": ("l67_fig1.svg", "1",
        "The problem, and the reason the obvious fix is unaffordable. Shading has consulted the "
        "geometry&#8217;s normal since Lesson 3.6, so a surface can only look bumpy if it IS bumpy "
        "&#8212; and giving this torus millimetre-scale detail means millimetre-scale triangles, "
        "tens of millions of them for one prop, every one smaller than a pixel, which is the worst "
        "case for every renderer ever built. The right move is to leave the geometry alone and "
        "<strong>lie about the normal</strong>, per pixel, from an image. Note what the right-hand "
        "picture does NOT change: the outline. A normal map is invisible at the silhouette, bumps "
        "do not occlude one another, and nothing casts a shadow on anything &#8212; those are the "
        "honest limits, and parallax and displacement mapping are the techniques that address "
        "them."),

    "FIG_SPACE": ("l67_fig2.svg", "2",
        "The question Lesson 6.6 deferred, settled by one byte. <strong>128 is what a flat normal "
        "map stores</strong> for x and y, and it means 0.502 as DATA and 0.216 as a COLOUR &#8212; "
        "the gap being the sRGB transfer function, which was never meant to be applied to a "
        "direction. Read the wrong way, &#8220;no tilt&#8221; becomes a tilt of "
        "<strong>38.8&deg;</strong>, in one direction, on every surface in the scene. And it does "
        "not look like a bug: the usual diagnosis is &#8220;this map was authored too strong&#8221; "
        "and the usual fix makes the picture less wrong without making it right. The decision goes "
        "on the TEXTURE rather than the sampler because that is where the hardware puts it &#8212; "
        "an <code>_SRGB</code> format decodes, a <code>_UNORM</code> one does not, and one image "
        "cannot be both at once."),

    "FIG_DERIVATION": ("l67_fig3.svg", "3",
        "The whole derivation, and it is two equations rather than a formula to copy. A "
        "triangle&#8217;s two edges describe a walk across the surface in METRES; the same two "
        "edges&#8217; uv deltas describe that identical walk in TEXTURE UNITS. So T and B are "
        "simply whatever vectors make the two descriptions agree &#8212; two equations, two "
        "unknowns, solved by inverting the 2&#215;2 of uv deltas. The determinant is twice the "
        "signed area the triangle occupies in the chart, which is the same signed-area quantity "
        "Lesson 2.4 derived barycentric coordinates from; <strong>zero is a real case rather than "
        "a degeneracy</strong> &#8212; an untextured face or a collapsed unwrap &#8212; so the "
        "face contributes nothing instead of an infinity."),

    "FIG_BASIS": ("l67_fig4.svg", "4",
        "Four steps from three bytes to a world-space normal, and the third one explains something "
        "you have seen a hundred times. A direction has negative components and a byte does not, "
        "so the encoding is an OFFSET &#8212; which makes (0,&nbsp;0,&nbsp;1), &#8220;no "
        "change&#8221;, store as (128,&nbsp;128,&nbsp;255). <strong>That is why every normal map is "
        "lavender: it is arithmetic, not a convention somebody chose.</strong> The last step is a "
        "matrix multiply written as its own definition (Lesson 2.5: a matrix is where the basis "
        "vectors land), and reading its z term gives the round trip for free. And the round trip is "
        "not exact, which is the finding: 0.5 has no 8-bit code, so the flattest map that can be "
        "STORED still tilts its surface by 0.318&deg; &#8212; every flat normal map in existence, "
        "not just the badly authored ones."),

    "FIG_MATRIX": ("l67_fig5.svg", "5",
        "The distinction Lesson 3.6 derived, arriving on its other side. Under a non-uniform scale "
        "the normal does not move &#8212; the surface is the same plane &#8212; and the tangent "
        "does, because it lies IN the surface and is stretched along with it. So a NORMAL is "
        "defined by being perpendicular, which is the property the inverse transpose exists to "
        "preserve; and a TANGENT is a <strong>difference of positions</strong>, which transforms "
        "exactly the way positions do. Use the normal matrix for both and the frame is skewed by "
        "36.9&deg; on a scale of (2,&nbsp;1,&nbsp;1). Note why that is dangerous rather than "
        "obvious: both answers still lie in the surface, so the normal map is merely ROTATED within "
        "the plane and reads as an asset authored at the wrong angle &#8212; and under a uniform "
        "scale the two agree to 8.4e&minus;08, so the bug looks perfect on everything nobody "
        "stretched."),

    "FIG_RESULT": ("l67_fig6.svg", "6",
        "What <code>gltf_view --model torus.obj</code> should put on screen with and without "
        "<code>--bumps 0</code>. Same 2,304 triangles, same light, same material, same silhouette "
        "&#8212; and a surface that has gone from smooth to quilted, because the only thing that "
        "changed is what it claims its normal is. Three things worth checking in the real render: "
        "the tangents were <strong>derived</strong> (OBJ has no tangent attribute at all, so every "
        "frame came out of the uv chart), the <strong>outline is unchanged</strong> in both, which "
        "is the technique&#8217;s honest limit visible in its own demo, and the overall brightness "
        "is about the same &#8212; if the bumpy one were much darker, that would be &#167;8.4&#8217;s "
        "parameterisation bug rather than a shading one. An SVG mock; the real frame is a PPM."),
}

LISTING_META = {
    "engine/include/engine/gfx/texture.hpp":  ("modified", "modified"),
    "engine/src/gfx/mesh.cpp":                ("modified", "modified"),
    "engine/include/engine/gfx/material.hpp": ("modified", "modified"),
    "shaders/scene.frag.hlsl":                ("modified", "modified"),
    "scratch/verify_67.cpp":                  ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/asset/asset_store.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/clip.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gltf.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_mesh.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_scene.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_uniform.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/mesh.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/raster.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/soft_renderer.hpp": ("modified", "modified"),
    "engine/src/asset/asset_store.cpp": ("modified", "modified"),
    "engine/src/gfx/clip.cpp": ("modified", "modified"),
    "engine/src/gfx/gltf.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_mesh.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_scene.cpp": ("modified", "modified"),
    "engine/src/gfx/raster.cpp": ("modified", "modified"),
    "engine/src/gfx/soft_renderer.cpp": ("modified", "modified"),
    "engine/src/gfx/texture.cpp": ("modified", "modified"),
    "shaders/mesh.vert.hlsl": ("modified", "modified"),
    "shaders/scene.vert.hlsl": ("modified", "modified"),
    "demos/gltf_view/main.cpp": ("modified", "modified"),
    "demos/sandbox/main.cpp": ("modified", "modified"),
}

LISTING_LANG = {
    "shaders/scene.frag.hlsl": ("hlsl", "HLSL"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "shaders/mesh.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/scene.vert.hlsl": ("hlsl", "HLSL"),
}

# PINNED at commit b5cbda8 ("Add Lesson 6.7"), by Lesson 6.8, before a line of
# 6.8 was written. Every path below is one 6.8 modifies, so reading them out of
# the working tree would show the reader 6.8's code inside 6.7's page.
#
#     git show b5cbda8:<path> | diff - scratch/l67_<name>      # must be empty
#
# `scratch/verify_67.cpp` is gitignored, so its pin is a working-tree copy taken
# before 6.8 touched anything; it has no other provenance.
# EXTENDED 2026-09-12 to cover EVERY listing, not just the files this
# lesson wrote. The partial dict below pinned what its author knew would
# move; everything else stayed live, so later lessons' edits leaked into
# this page anyway — which is why it no longer rebuilt to what it shipped.
# Pinned from b5cbda8, verified per file against the shipped page
# with scratch/which_commit.py.
#
# The original dict, kept for its provenance:
#   LISTING_SOURCE = {
#       "engine/include/engine/gfx/texture.hpp":  "scratch/l67_texture.hpp",
#       "engine/src/gfx/mesh.cpp":                "scratch/l67_mesh.cpp",
#       "engine/include/engine/gfx/material.hpp": "scratch/l67_material.hpp",
#       "shaders/scene.frag.hlsl":                "scratch/l67_scene.frag.hlsl",
#       "scratch/verify_67.cpp":                  "scratch/l67_verify_67.cpp",
#   }
LISTING_SOURCE = {
    "engine/include/engine/gfx/texture.hpp":  "scratch/l67_engine_include_engine_gfx_texture.hpp",
    "engine/src/gfx/mesh.cpp":                "scratch/l67_engine_src_gfx_mesh.cpp",
    "engine/include/engine/gfx/material.hpp": "scratch/l67_engine_include_engine_gfx_material.hpp",
    "shaders/scene.frag.hlsl":                "scratch/l67_shaders_scene.frag.hlsl",
    "scratch/verify_67.cpp":                  "scratch/l67_scratch_verify_67.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/asset/asset_store.hpp": "scratch/l67_engine_include_engine_asset_asset_store.hpp",
    "engine/include/engine/gfx/clip.hpp": "scratch/l67_engine_include_engine_gfx_clip.hpp",
    "engine/include/engine/gfx/gltf.hpp": "scratch/l67_engine_include_engine_gfx_gltf.hpp",
    "engine/include/engine/gfx/gpu_mesh.hpp": "scratch/l67_engine_include_engine_gfx_gpu_mesh.hpp",
    "engine/include/engine/gfx/gpu_scene.hpp": "scratch/l67_engine_include_engine_gfx_gpu_scene.hpp",
    "engine/include/engine/gfx/gpu_uniform.hpp": "scratch/l67_engine_include_engine_gfx_gpu_uniform.hpp",
    "engine/include/engine/gfx/mesh.hpp": "scratch/l67_engine_include_engine_gfx_mesh.hpp",
    "engine/include/engine/gfx/raster.hpp": "scratch/l67_engine_include_engine_gfx_raster.hpp",
    "engine/include/engine/gfx/soft_renderer.hpp": "scratch/l67_engine_include_engine_gfx_soft_renderer.hpp",
    "engine/src/asset/asset_store.cpp": "scratch/l67_engine_src_asset_asset_store.cpp",
    "engine/src/gfx/clip.cpp": "scratch/l67_engine_src_gfx_clip.cpp",
    "engine/src/gfx/gltf.cpp": "scratch/l67_engine_src_gfx_gltf.cpp",
    "engine/src/gfx/gpu_mesh.cpp": "scratch/l67_engine_src_gfx_gpu_mesh.cpp",
    "engine/src/gfx/gpu_scene.cpp": "scratch/l67_engine_src_gfx_gpu_scene.cpp",
    "engine/src/gfx/raster.cpp": "scratch/l67_engine_src_gfx_raster.cpp",
    "engine/src/gfx/soft_renderer.cpp": "scratch/l67_engine_src_gfx_soft_renderer.cpp",
    "engine/src/gfx/texture.cpp": "scratch/l67_engine_src_gfx_texture.cpp",
    "shaders/mesh.vert.hlsl": "scratch/l67_shaders_mesh.vert.hlsl",
    "shaders/scene.vert.hlsl": "scratch/l67_shaders_scene.vert.hlsl",
    "demos/gltf_view/main.cpp": "scratch/l67_demos_gltf_view_main.cpp",
    "demos/sandbox/main.cpp": "scratch/l67_demos_sandbox_main.cpp",
}
#
# And take `scratch/verify_67.cpp` EARLY — gitignored, so its only provenance is
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
<title>6.7 — Normal Mapping and the TBN Derivation · Build a Professional 3D Game Engine</title>
<meta name="description" content="Every surface this engine draws is exactly as flat as its triangles, and the fix is to lie about the normal per pixel rather than to add geometry. Derives the tangent frame from two equations - a triangle's edges and its uv deltas describe the same walk across the surface, once in metres and once in texture units - rather than quoting the formula, with the determinant shown to be twice the signed uv area so that zero is a real case and not a degeneracy. Settles the question 6.6 deferred: a texture gains a colour space, because the byte 128 means 0.502 as data and 0.216 as a colour, and reading a normal map wrong tilts every surface by 38.8 degrees in a way that looks like an over-strong map. Explains why a tangent is a vec4, why it is carried by the model matrix where a normal needs the inverse transpose, and why every normal map is lavender. Ports to both renderers; the reference render stays byte-identical.">

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
    <a class="prev-l" href="06-06-gltf.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.6 — glTF 2.0 Loading</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-08-shadow-mapping.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.8 — Shadow Mapping: Bias, Acne, and PCF</span>
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
        with open(f"scratch/l67_body_{name}.html") as fh:
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
