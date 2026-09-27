#!/usr/bin/env python3
"""Assemble docs/lessons/06-15-skybox-ibl.html.

Same pipeline as build_614.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l615_body_{a,b,c}.html, figures from scratch/figs_615.py, and
every code listing read from a PINNED COPY, never live — see LISTING_SOURCE.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-15-skybox-ibl.html"

FIGURES = {
    "FIG_PROBLEM": ("l615_fig1.svg", "1",
        "Two things a constant ambient cannot do, and only one of them was ever written down. "
        "LEFT, NO DIRECTION: a uniform radiance arrives equally from everywhere, so a face turned "
        "to the sky and one turned to the floor receive identical fill and nothing in the scene is "
        "grounded. RIGHT, NO SPECULAR HALF AT ALL: <code>base * ambient</code> goes through no BRDF "
        "and a metal has no <code>base</code>, so chrome in a bright room is black except where one "
        "lamp reaches it. The second has been on record since Lesson 6.4 in a comment shipped in "
        "both <code>light.hpp</code> and <code>scene.frag.hlsl</code>, naming this lesson by "
        "number as the fix."),

    "FIG_CUBE": ("l615_fig2.svg", "2",
        "A direction picks a face by its largest component &#8212; and the texels are not the same "
        "size, which is the correctness bug this file exists to avoid. A texel at face coordinates "
        "(x, y) sits at distance r = sqrt(1+x&sup2;+y&sup2;) and is tilted away by cos&#952; = 1/r, "
        "so d&#969; = dA/r&sup3;. At the centre r = 1; at a corner r&sup3; = 3&#8730;3 = "
        "<strong>5.196</strong>. Averaging texels with equal weights therefore over-counts the "
        "corners, and the measured error is <strong>1.01%</strong> &#8212; which does NOT shrink as "
        "the cube is refined (1.010005 at 16&times;16, 1.010066 at 64&times;64). A bias, not a "
        "discretisation error, which is exactly what makes it a bug."),

    "FIG_SPLIT": ("l615_fig3.svg", "3",
        "One integral, split two ways, and only the first split is exact. THE DIFFUSE HALF FACTORS "
        "because Lambert&#8217;s BRDF is a constant, so it leaves the integral and what remains "
        "depends only on the normal &#8212; no approximation, only discretisation, measured at "
        "<strong>1.1 &times; 10&#8315;&#8309;</strong> relative at 64&times;64 faces. THE SPECULAR "
        "HALF DOES NOT FACTOR and the split-sum approximation pretends it does. The price is "
        "measured rather than hedged: 0.08% at roughness 0.10, never worse than 5.8% at near-normal "
        "incidence, <strong>29.0% dark</strong> at roughness 1 seen edge-on, and <strong>73.0%</strong> "
        "once the sky contains a sun disc."),

    "FIG_LEVEL": ("l615_fig4.svg", "5",
        "Which mip level a roughness should read &#8212; derived, and shipped. Lesson 6.14&#8217;s "
        "closed-form lobe width turns into a solid angle, a texel already has one, and setting them "
        "equal gives <code>k = &#189;&#183;log&#8322;(lobe / texel)</code>. Set that against the "
        "<code>roughness &times; (levels&minus;1)</code> everybody ships and they never differ by "
        "more than <strong>0.77 of a level</strong> (at roughness 0.45). THE FOLKLORE IS NOT "
        "ARBITRARY &#8212; it is a good fit to a derived answer, and now the error has a size and a "
        "direction: a near-mirror is OVER-blurred by most of a level, the middle of the range is "
        "under-blurred. For comparison &#8730;roughness, also seen in the wild, is off by 2.21."),

    "FIG_SHAPE": ("l615_fig5.svg", "4",
        "The split sum&#8217;s error is not noise: it has a shape, and the shape names the "
        "assumption. At near-normal incidence the approximation stays within <strong>5.8%</strong> "
        "at every roughness; at a grazing view it falls to <strong>29.0% dark</strong> by roughness "
        "1 &#8212; more than four times the error on the same material. That asymmetry IS "
        "<code>n = v = r</code> made visible: a prefiltered value is indexed by ONE direction while "
        "the true integral depends on two, so the chain bakes its lobe around the reflection, and "
        "at a grazing view that points across the horizon and averages in ground the real surface "
        "never sees. What it discards is the stretched, comet-shaped highlight."),

    "FIG_BAKE": ("l615_fig6.svg", "6",
        "One sky in, three precomputed things out &#8212; and they are only correct together. They "
        "are precomputed at three different rates: the irradiance map and the prefiltered chain "
        "depend on the environment, and the BRDF table depends on <em>neither the environment nor "
        "the material</em>, which is the most surprising consequence of Schlick&#8217;s Fresnel "
        "being linear in F0. One 64&times;64 image serves gold, chrome and plastic at once. Note "
        "the chain&#8217;s 1.3333&times; overhead &#8212; the same 4/3 Lesson 6.10 derived as 33%, "
        "from 1 + &#188; + 1/16 + &#8230;"),

    "FIG_NOISE": ("l615_fig7.svg", "7",
        "The first version of this lesson&#8217;s headline measurement was wrong, and it looked "
        "completely reasonable. With a 6000:1 sun disc in the environment, 8,192 importance samples "
        "and 1,000,000 disagree by a factor of <strong>1.98</strong>; with the sun removed, the same "
        "two counts agree to <strong>0.040%</strong>. The sample count was never the problem &#8212; "
        "THE DYNAMIC RANGE WAS. Lesson 6.14&#8217;s rule was to check that a measurement CAN produce "
        "a non-null result before believing a null one; this is its mirror, and both are really the "
        "same rule: establish what your instrument can see before reading it."),
}

LISTING_META = {
    "engine/include/engine/gfx/cubemap.hpp": ("new", "new"),
    "engine/src/gfx/cubemap.cpp": ("new", "new"),
    "shaders/skybox.vert.hlsl": ("new", "new"),
    "shaders/skybox.frag.hlsl": ("new", "new"),
    "scratch/verify_615.cpp": ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gpu_scene.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_texture.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_uniform.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/hdr.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/light.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/microfacet.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/raster.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_scene.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_shadow.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_texture.cpp": ("modified", "modified"),
    "engine/src/gfx/hdr.cpp": ("modified", "modified"),
    "engine/src/gfx/raster.cpp": ("modified", "modified"),
    "shaders/scene.frag.hlsl": ("modified", "modified"),
    "CMakeLists.txt": ("modified", "modified"),
    "cmake/EngineHelpers.cmake": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "demos/gltf_view/main.cpp": ("modified", "modified"),
}

LISTING_LANG = {
    "shaders/skybox.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/skybox.frag.hlsl": ("hlsl", "HLSL"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "shaders/scene.frag.hlsl": ("hlsl", "HLSL"),
    "CMakeLists.txt": ("cmake", "CMake"),
    "cmake/EngineHelpers.cmake": ("cmake", "CMake"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
}

LISTING_SOURCE = {
    # PINNED at the START of 6.16's session, exactly as the note below instructed.
    # Four came from 5c54785 (the commit that shipped 6.15); verify_615.cpp is
    # gitignored and was copied from the working tree, which is its only provenance.
    "engine/include/engine/gfx/cubemap.hpp": "scratch/l615_engine_include_engine_gfx_cubemap.hpp",
    "engine/src/gfx/cubemap.cpp":            "scratch/l615_engine_src_gfx_cubemap.cpp",
    "shaders/skybox.vert.hlsl":              "scratch/l615_shaders_skybox.vert.hlsl",
    "shaders/skybox.frag.hlsl":              "scratch/l615_shaders_skybox.frag.hlsl",
    "scratch/verify_615.cpp":                "scratch/l615_scratch_verify_615.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gpu_scene.hpp": "scratch/l615_engine_include_engine_gfx_gpu_scene.hpp",
    "engine/include/engine/gfx/gpu_texture.hpp": "scratch/l615_engine_include_engine_gfx_gpu_texture.hpp",
    "engine/include/engine/gfx/gpu_uniform.hpp": "scratch/l615_engine_include_engine_gfx_gpu_uniform.hpp",
    "engine/include/engine/gfx/hdr.hpp": "scratch/l615_engine_include_engine_gfx_hdr.hpp",
    "engine/include/engine/gfx/light.hpp": "scratch/l615_engine_include_engine_gfx_light.hpp",
    "engine/include/engine/gfx/microfacet.hpp": "scratch/l615_engine_include_engine_gfx_microfacet.hpp",
    "engine/include/engine/gfx/raster.hpp": "scratch/l615_engine_include_engine_gfx_raster.hpp",
    "engine/src/gfx/gpu_scene.cpp": "scratch/l615_engine_src_gfx_gpu_scene.cpp",
    "engine/src/gfx/gpu_shadow.cpp": "scratch/l615_engine_src_gfx_gpu_shadow.cpp",
    "engine/src/gfx/gpu_texture.cpp": "scratch/l615_engine_src_gfx_gpu_texture.cpp",
    "engine/src/gfx/hdr.cpp": "scratch/l615_engine_src_gfx_hdr.cpp",
    "engine/src/gfx/raster.cpp": "scratch/l615_engine_src_gfx_raster.cpp",
    "shaders/scene.frag.hlsl": "scratch/l615_shaders_scene.frag.hlsl",
    "CMakeLists.txt": "scratch/l615_CMakeLists.txt",
    "cmake/EngineHelpers.cmake": "scratch/l615_cmake_EngineHelpers.cmake",
    "engine/CMakeLists.txt": "scratch/l615_engine_CMakeLists.txt",
    "demos/gltf_view/main.cpp": "scratch/l615_demos_gltf_view_main.cpp",
}

# PINNED — done 2026-09-12, at the start of 6.16's session, before a line of the
# next lesson was written. `pin_listings.py 615 --dry` confirmed all five listings
# appear verbatim in the shipped page, which is the check that a gitignored copy
# has not drifted; re-running this file changed the page by zero bytes.
#
# The two reasons the pin was taken even though 6.16 (frustum culling) has no
# obvious business in `cubemap.*` or the skybox shaders still stand:
#   1 `verify_615.cpp` is GITIGNORED, so a working-tree copy taken now is its only
#     recoverable provenance.
#   2 "Nothing here is at risk" is what was said about `gpu_post.hpp` before 6.14
#     rewrote it.


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
<title>6.15 — Skybox and Image-Based Lighting · Build a Professional 3D Game Engine</title>
<meta name="description" content="The ambient term has been an apology since Lesson 3.6: one constant radiance standing in for the sky, the floor and every bounce off them. It has no direction, so a face turned upward and one turned downward receive identical fill; and it has no specular half at all, so chrome in a bright room renders black except where one lamp reaches it - a sentence shipped in the engine since 6.4, naming this lesson as the fix. An environment map replaces it with radiance as a function of direction, stored on the six faces of a cube, and the same texture both lights the scene and is the sky behind it. The cube's own geometry comes first: a direction picks a face by its largest component, and because a face is flat a corner texel subtends 1/(3*sqrt3) = 0.1925 of a centre one - so averaging with equal weights is 1.01% too bright and REFINING THE CUBE DOES NOT HELP, which is what makes it a bias and not an error. Then the two integrals, and only one is exact: Lambert's BRDF is constant so it leaves the integral entirely, and a uniform environment reproduces 6.2's albedo*ambient to six decimals - the old term is a special case, not a casualty. The specular half does not factor and the split sum pretends it does, at a price measured rather than hedged: 0.08% at roughness 0.10, under 5.8% at near-normal incidence, 29.0% dark at roughness 1 seen edge-on, and 73.0% once the sky has a sun in it. That asymmetry is the n=v=r assumption made visible. The roughness-to-mip mapping is DERIVED from 6.14's closed-form lobe width rather than chosen, and the folklore turns out to be within 0.77 of a level. Plus cube maps in SDL_GPU, six faces uploaded as layers, half floats written from scratch because HDR data has no other way to reach the device, and a measurement that lied until it converged."">

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
    <a class="prev-l" href="06-14-antialiasing.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.14 — Antialiasing: Geometric and Shading</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-16-frustum-culling.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.16 — Frustum Culling and Instanced Submission</span>
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
        with open(f"scratch/l615_body_{name}.html") as fh:
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
