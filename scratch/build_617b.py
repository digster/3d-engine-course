#!/usr/bin/env python3
"""Assemble docs/lessons/06-17b-local-lights.html.

Same pipeline as build_617.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l617b_body_{a,b,c,d}.html, figures from scratch/figs_617b.py, and
EVERY code listing read from a pinned copy from the first build, not from the
working tree — because this is an INSERTED lesson (the authoring guide's §17). The
working tree already holds 6.18, Module 7 and Module 8, so its files are not what a
student has at 6.17b. `scratch/make_t617b.py tree scratch/_t617 scratch/_t617b
--pins` writes each pin as the file after 6.17 (`scratch/replay_tree.py 6.17`) plus
6.17b's own edits, and proves every one by delta.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-17b-local-lights.html"

FIGURES = {
    "FIG_SPHERES": ("l617b_fig1.svg", "1",
        "The whole of the inverse-square law in one picture, and the numbers that pin it down. A narrow "
        "beam of rays from the lamp cuts a patch from a sphere at distance <em>d</em> and a patch of four "
        "times the area at 2<em>d</em>: the same rays, the same power, spread four times as thin. Nothing "
        "is absorbed on the way. On the right, <code>verify_617b</code> &#167;A: a lamp of intensity "
        "&#960; renders a white surface at exactly 1.0 from one metre &#8212; the engine&#8217;s sun "
        "&#8212; and a quarter and a sixteenth of that from two and four; three spheres from half a metre "
        "to eight collect the same 39.47852, which is the claim the derivation makes; and a lamp high "
        "enough above what it lights becomes a sun, to (<em>D</em>/<em>d</em>)&#179;."),

    "FIG_WINDOWS": ("l617b_fig2.svg", "2",
        "Three ways to force a light to zero at its range, drawn against the bare inverse square. On the "
        "left, over the whole range: the fourth power keeps both windows near 1 close to the lamp and "
        "spends their descent in the outer half, where the light is already dim &#8212; 0.879 at half the "
        "range and 0.577 at 0.7 for the engine&#8217;s. On the right, the last fifth, magnified, which is "
        "where they differ in kind. glTF&#8217;s recommended 1&nbsp;&#8722;&nbsp;<em>x</em>&#8308; arrives "
        "at the range falling twice as steeply as the inverse square, &#8722;4/<em>r</em>&#179;, and "
        "stops dead &#8212; a crease, which the eye sees as a ring on a floor. Squared, as Karis proposed "
        "and Frostbite adopted, it meets zero tangentially."),

    "FIG_CONE": ("l617b_fig3.svg", "3",
        "A spot light is a point light with a mask, and the mask is ramped in <em>cosine</em>. Left: the "
        "lamp, its axis (<code>direction</code>, the direction the light travels), full intensity inside "
        "the inner cone and none outside the outer. Right: the falloff between them for the harness&#8217;s "
        "cone, 0.30 to 0.45&nbsp;rad. The solid curve is glTF&#8217;s and the engine&#8217;s, "
        "saturate(cos&nbsp;&#183;&nbsp;scale&nbsp;+&nbsp;offset)&#178;, which needs no <code>acos</code>; "
        "the dashed one is the same ramp made linear in angle, which would. Halfway in angle they give "
        "0.2999 and 0.25: close, systematic, and the price of never calling an inverse cosine per lamp "
        "per fragment."),

    "FIG_ACNE": ("l617b_fig4.svg", "4",
        "The failure first, rendered by the GPU from straight above. A warm point light at the right and a "
        "cool spot from the left light the same ground; a box hovers over it and a thin sign stands on it. "
        "Left, with shadows on and no bias: the ground shadows <em>itself</em> across both pools &#8212; in "
        "nested squares under the bulb, one family per cube face, and in speckled arcs under the spot, "
        "where 6.8&#8217;s orthographic acne was straight stripes. 19,502 of 67,715 judged points on the spot&#8217;s side are "
        "acne. Right, with the bias this lesson derives: the same scene, and against a ray-cast ground "
        "truth that shares no code with the engine, zero acne and zero leaks."),

    "FIG_PYRAMID": ("l617b_fig5.svg", "5",
        "Why 6.8&#8217;s bias cannot be reused unchanged. Left: a spot&#8217;s map is a pyramid, and one "
        "texel of a 512 map covers 1.652&nbsp;mm at a metre, 3.3 at two and 13.2 at eight &#8212; its size "
        "is proportional to the axial distance of whatever it lands on. Right, on logarithmic axes: the "
        "device-depth bias a 45&#176; surface needs, per fragment, through the depth curve (falling like "
        "1/<em>d</em>), against 6.8&#8217;s formula reused as a constant. They agree at one distance only, "
        "<em>n</em>&#183;<em>f</em>/1&nbsp;m = 0.40&nbsp;m; beyond it the constant is too large in "
        "proportion to distance, twenty times at eight metres."),

    "FIG_AXIAL": ("l617b_fig6.svg", "6",
        "The assumption 6.8&#8217;s derivation never had to state. A shadow map stores <em>axial</em> "
        "depth &#8212; distance along the camera&#8217;s axis &#8212; and under an orthographic camera every "
        "ray is the axis, so axial depth and distance along the ray are the same number. Under perspective "
        "they are not. Two rays one texel apart meet a wall that faces the lamp squarely, 30&#176; off "
        "the axis, at almost the same distance from the lamp &#8212; but their feet on the axis differ, "
        "so the stored depth changes across the texel although 6.8&#8217;s tan&nbsp;&#952; says it is "
        "flat. Per texel, axial depth changes by texel&nbsp;&#183;&nbsp;sin&nbsp;&#945;&nbsp;"
        "cos&nbsp;&#966;&nbsp;/&nbsp;cos&nbsp;&#952; &#8212; which on the axis is tan&nbsp;&#952; again, "
        "and for the facing wall is sin&nbsp;&#966;&nbsp;cos&nbsp;&#966;."),

    "FIG_CUBE": ("l617b_fig7.svg", "7",
        "Six 90&#176; cameras tile the sphere around a point light, and each must draw its face the way the "
        "hardware will read it. Left: the cube unfolded in SDL&#8217;s face order, +X &#8722;X +Y "
        "&#8722;Y +Z &#8722;Z, so face <em>f</em> of cube <em>c</em> is array layer 6<em>c</em>&nbsp;+&nbsp;<em>f</em>. "
        "Right: the +X face as 6.15&#8217;s table defines it &#8212; <strong>u</strong> growing toward "
        "&#8722;<em>z</em>, <strong>v</strong> growing <em>down</em> the image toward &#8722;<em>y</em> "
        "&#8212; and what <code>look_at</code> draws there, whose right is +<em>z</em>: the mirror image. "
        "The correct camera has rows (<strong>u</strong>, &#8722;<strong>v</strong>, "
        "&#8722;<strong>major</strong>) and determinant &#8722;1. A rotation cannot be a reflection, so no "
        "choice of up vector rescues <code>look_at</code>."),

    "FIG_GUARD": ("l617b_fig8.svg", "8",
        "Finite is not small. Near-plane clipping keeps every vertex in front of the camera, but a corner "
        "clipped to a 5&nbsp;cm near plane over an 8&nbsp;m floor projects about 40,000 pixels to the side "
        "of a 512-pixel map. <code>to_pixel</code>&#8217;s &#177;8,000 clamp, written for Lesson 3.3&#8217;s "
        "unclipped mode, drags that corner inward; the triangle changes shape and its plane of depths tilts "
        "across the whole map, which read 0.950 where the floor was at 0.987. Guard-band clipping cuts the "
        "polygon at &#177;7,936 instead and places a new corner <em>on the true edge</em>, so every depth "
        "inside the map is untouched &#8212; and because no frame in Modules 3 to 6 ever reached the band, "
        "the reference render is byte-identical."),

    "FIG_FLOOR": ("l617b_fig9.svg", "9",
        "A bare floor under a bulb, rendered by the CPU from above, where white is lit and black is acne. "
        "Nothing here casts a shadow, so every dark mark is the map disagreeing with itself. Left, with "
        "6.8&#8217;s reach: 713 points of acne in stripes along one cube face, from Lesson 2.2&#8217;s "
        "integer vertex snapping sliding a guard-clipped plane by up to half a texel &#8212; measured at "
        "0.36 of a texel&#8217;s depth step. Right, with half a texel more reach under perspective: 0 of "
        "128,122."),

    "FIG_RECORD": ("l617b_fig10.svg", "10",
        "One lamp as the GPU reads it: <code>gpu_local_light</code>, eleven <code>float4</code>s, 176 bytes, "
        "one per lamp in a <code>StructuredBuffer</code> at <code>t8</code>. Every member is a "
        "<code>float4</code> so that DXC, glslang and SPIRV-Cross have nothing to disagree about, and the "
        "spot&#8217;s shadow matrix is stored as four ROWS so that no default matrix packing can transpose "
        "it on the way. The count is not in the record: it lives in the float of padding 6.15 left at "
        "offset 180 of the light block, and the renderer sets it &#8212; never the caller &#8212; so the "
        "length the shader loops to and the buffer it indexes always describe the same list."),

    "FIG_GRAPH": ("l617b_fig11.svg", "11",
        "Two frames through 6.17&#8217;s frame graph. Frame 1 declares seven shadow passes, each clearing "
        "one layer of an imported texture; because a layered clear keeps the other layers, the six cube "
        "faces form a chain of versions, and because the scene samples the last of them, every face&#8217;s "
        "store op is derived as STORE. Against the hand-recorded frame: 0 of 196,608 channels differ. "
        "Frame 2 declares no shadow passes at all and samples version 0 of both imports &#8212; whatever the "
        "textures held when the frame began, which is frame 1&#8217;s maps. One pass runs, and 0 channels "
        "differ; clear the maps first and 18,015 do, so the cache is doing the work."),

    "FIG_BOTH": ("l617b_fig12.svg", "12",
        "The same two lamps through <code>light.hpp</code> and <code>scene.frag.hlsl</code>, compared pixel "
        "by pixel in linear light, with every pixel a known point on the ground. Left, the CPU: "
        "<code>shade_local</code> and <code>local_shadow_set</code>. Middle, the GPU: a storage buffer and "
        "a depth cube array. Right, in red, the 259 pixels that differ, out of 61,043 lit &#8212; every one "
        "within a pixel of a shadow edge, where a nearest-texel comparison and a 2&#215;2 bilinear one may "
        "legitimately disagree. Widening the spot&#8217;s cone by 5% moves 9,992 pixels, so the instrument "
        "that reports 259 can see ten thousand."),

    "FIG_DEMO": ("l617b_fig13.svg", "13",
        "<code>gltf_view --lights</code> on the CPU renderer, with the sun at a tenth of its usual strength "
        "and both frames brightened 2.5&#215; for the page. A warm glow from the bulb behind the model, a "
        "cool, sharp-edged pool from the spot in front of it. Left: both lamps shadowed &#8212; one spot map "
        "and six cube faces &#8212; and the bulb throws the slab&#8217;s and the cube&#8217;s shadows to the "
        "left across the floor, away from itself, as no directional light could from that side. Right: "
        "<code>--lamp-shadows 0</code>, the same light with no occlusion. Look at the spot&#8217;s pool: it is "
        "bluer on the left, because the bulb&#8217;s shadow of the slab falls across it and takes the "
        "bulb&#8217;s warm light out &#8212; two lamps, and one shadowing the other&#8217;s light."),
}

LISTING_META = {
    "engine/include/engine/gfx/light.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/shadow.hpp": ("modified", "modified"),
    "engine/src/gfx/shadow.cpp": ("modified", "modified"),
    "engine/include/engine/gfx/cubemap.hpp": ("modified", "modified"),
    "engine/src/gfx/cubemap.cpp": ("modified", "modified"),
    "engine/include/engine/gfx/clip.hpp": ("modified", "modified"),
    "engine/src/gfx/clip.cpp": ("modified", "modified"),
    "engine/include/engine/gfx/soft_renderer.hpp": ("modified", "modified"),
    "engine/src/gfx/soft_renderer.cpp": ("modified", "modified"),
    "engine/include/engine/gfx/raster.hpp": ("modified", "modified"),
    "engine/src/gfx/raster.cpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_uniform.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_texture.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_texture.cpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_debug.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_debug.cpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_shadow.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_shadow.cpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_scene.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_scene.cpp": ("modified", "modified"),
    "shaders/scene.frag.hlsl": ("modified", "modified"),
    "demos/gltf_view/main.cpp": ("modified", "modified"),
    "scratch/verify_617b.cpp": ("new", "new"),
}

LISTING_LANG = {
    "shaders/scene.frag.hlsl": ("hlsl", "HLSL"),
}

LISTING_SOURCE = {
    # PINNED from the first build, because an inserted lesson's files are not the
    # working tree's (see the module docstring). Every pin but the harness was
    # written by `make_t617b.py tree ... --pins` and proved by delta; the harness
    # is gitignored-by-directory and was copied from the working tree.
    "engine/include/engine/gfx/light.hpp":         "scratch/l617b_engine_include_engine_gfx_light.hpp",
    "engine/include/engine/gfx/shadow.hpp":        "scratch/l617b_engine_include_engine_gfx_shadow.hpp",
    "engine/src/gfx/shadow.cpp":                   "scratch/l617b_engine_src_gfx_shadow.cpp",
    "engine/include/engine/gfx/cubemap.hpp":       "scratch/l617b_engine_include_engine_gfx_cubemap.hpp",
    "engine/src/gfx/cubemap.cpp":                  "scratch/l617b_engine_src_gfx_cubemap.cpp",
    "engine/include/engine/gfx/clip.hpp":          "scratch/l617b_engine_include_engine_gfx_clip.hpp",
    "engine/src/gfx/clip.cpp":                     "scratch/l617b_engine_src_gfx_clip.cpp",
    "engine/include/engine/gfx/soft_renderer.hpp": "scratch/l617b_engine_include_engine_gfx_soft_renderer.hpp",
    "engine/src/gfx/soft_renderer.cpp":            "scratch/l617b_engine_src_gfx_soft_renderer.cpp",
    "engine/include/engine/gfx/raster.hpp":        "scratch/l617b_engine_include_engine_gfx_raster.hpp",
    "engine/src/gfx/raster.cpp":                   "scratch/l617b_engine_src_gfx_raster.cpp",
    "engine/include/engine/gfx/gpu_uniform.hpp":   "scratch/l617b_engine_include_engine_gfx_gpu_uniform.hpp",
    "engine/include/engine/gfx/gpu_texture.hpp":   "scratch/l617b_engine_include_engine_gfx_gpu_texture.hpp",
    "engine/src/gfx/gpu_texture.cpp":              "scratch/l617b_engine_src_gfx_gpu_texture.cpp",
    "engine/include/engine/gfx/gpu_debug.hpp":     "scratch/l617b_engine_include_engine_gfx_gpu_debug.hpp",
    "engine/src/gfx/gpu_debug.cpp":                "scratch/l617b_engine_src_gfx_gpu_debug.cpp",
    "engine/include/engine/gfx/gpu_shadow.hpp":    "scratch/l617b_engine_include_engine_gfx_gpu_shadow.hpp",
    "engine/src/gfx/gpu_shadow.cpp":               "scratch/l617b_engine_src_gfx_gpu_shadow.cpp",
    "engine/include/engine/gfx/gpu_scene.hpp":     "scratch/l617b_engine_include_engine_gfx_gpu_scene.hpp",
    "engine/src/gfx/gpu_scene.cpp":                "scratch/l617b_engine_src_gfx_gpu_scene.cpp",
    "shaders/scene.frag.hlsl":                     "scratch/l617b_shaders_scene.frag.hlsl",
    "demos/gltf_view/main.cpp":                    "scratch/l617b_demos_gltf_view_main.cpp",
    "scratch/verify_617b.cpp":                     "scratch/l617b_scratch_verify_617b.cpp",
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
<title>6.17b — Local Lights: Point and Spot, and Their Shadows · Build a Professional 3D Game Engine</title>
<meta name="description" content="Every light the engine has drawn so far is the sun. This lesson adds point and spot lights to both renderers and gives each a shadow. The inverse-square law is derived from the area of a sphere and measured as the same power through spheres of 0.5, 2 and 8 metres to 2.6e-6, with intensity defined as the irradiance at one metre so that a lamp of intensity pi at one metre IS the engine's sun. Three published range windows are compared at the edge, where glTF's unsquared recipe leaves a crease and Karis's squared window does not; a spot is a point light masked by a cone ramped in cosine. Shadows: a perspective map for a spot, six mirrored cameras into a cube for a point - a look_at camera draws every face mirrored. Carrying 6.8's bias to a camera with a position exposes the depth curve, an axial slope sin(alpha) cos(phi) / cos(theta) of which 6.8's tan(theta) is the on-axis case, and a zero-slope knife edge that float rounding decides. A ray-cast judge that shares no code with the engine scores the derived bias at zero acne and zero leaks over 411,075 points, and was itself wrong four times first. A Module 3 clamp that moved near-clipped vertices becomes guard-band clipping without moving the reference render. The GPU reads the lamps from its first fragment storage buffer, the frame graph schedules or caches the shadow passes, and the two renderers disagree on 259 of 61,043 lit pixels, every one within a pixel of a shadow edge.">

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
    <a class="prev-l" href="06-17-frame-graph.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.17 — A Lightweight Frame Graph</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-18-text-overlay.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.18 — Text and 2D Overlay Rendering</span>
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
        with open(f"scratch/l617b_body_{name}.html") as fh:
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
