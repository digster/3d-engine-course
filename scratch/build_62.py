#!/usr/bin/env python3
"""Assemble docs/lessons/06-02-what-a-brdf-is.html.

Same pipeline as build_61.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l62_body_{a,b,c}.html, figures from scratch/figs_62.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-02-what-a-brdf-is.html"

FIGURES = {
    "FIG_QUANTITIES": ("l62_fig1.svg", "1",
        "The chain, and each link is a division. Flux is everything the lamp emits &#8212; a "
        "property of the LAMP, which is why it is useless for shading. Divide by area and you "
        "have irradiance, a property of the surface&#8217;s situation. Divide again by solid "
        "angle and you have radiance, which is the only one of the three that answers "
        "&#8220;what colour is this pixel?&#8221;, because a pixel is a narrow cone of "
        "directions and the other two have already thrown direction away. <strong>Note the "
        "&#8869; on dA in the third panel</strong>: radiance is per unit area measured "
        "PERPENDICULAR to the ray, which is the same projected-area idea Figure 3 makes the "
        "cosine out of."),

    "FIG_INVARIANCE": ("l62_fig2.svg", "2",
        "Why the third quantity is the storable one, in six numbers. Double the distance and the "
        "irradiance falls by four &#8212; the inverse-square law, and nobody is surprised. What "
        "is easy to miss is that <strong>the solid angle the emitter subtends falls by four "
        "too</strong>, for the same reason and at the same rate. Radiance is their ratio, so it "
        "does not move: 7.5 at every distance, to the last bit. That cancellation is what lets a "
        "framebuffer hold one number per pixel with no distance term anywhere in the shading "
        "equation, and it is worth knowing that it fails the moment there is fog in the way."),

    "FIG_PROJECTED": ("l62_fig3.svg", "3",
        "Lambert&#8217;s cosine law is a statement about AREA, and this is the second turn of the "
        "spiral that started in Lesson 3.6. One beam, fixed width, three tilts: the patch it "
        "lands on stretches by 1/cos&#8201;&#952;, so each square metre of it receives "
        "cos&#8201;&#952; as much. At 60&#176; that is exactly twice the area and exactly half "
        "the light. <strong>Nothing in the argument mentions what the surface is made of</strong> "
        "&#8212; which is the whole point, and the reason this lesson moves the cosine onto the "
        "light&#8217;s side of the shading equation and leaves the material&#8217;s answer to a "
        "BRDF."),

    "FIG_BRDF": ("l62_fig4.svg", "4",
        "The question that confuses everyone once, settled by a unit. Both surfaces reflect "
        "<em>one hundred percent</em> of the light that arrives; the right one merely funnels it "
        "into a 20&#176; cone instead of the whole hemisphere. A BRDF is what leaves PER "
        "STERADIAN, so its value is one over the space the light is spread across &#8212; "
        "0.3183&#8201;sr&#8315;&#185; for Lambert, <strong>2.7210 for the glossy one</strong>, "
        "and unbounded for a mirror, whose cone has no width at all. The quantity that really is "
        "capped at 1 is the INTEGRAL of the BRDF over the hemisphere, which is what Figure 6 "
        "measures; confusing the function with its integral is the actual mistake behind "
        "&#8220;but it is greater than one&#8221;."),

    "FIG_THE_PI": ("l62_fig5.svg", "5",
        "The derivation this lesson exists for, as a picture. Every patch of the hemisphere, "
        "projected straight down, casts a shadow of its own size times cos&#8201;&#952; &#8212; "
        "and those shadows are the polar grid on the right, at radius sin&#8201;&#952;, tiling "
        "the unit disc <strong>exactly once</strong>, nothing left over and nothing "
        "double-covered. So the cosine-weighted hemisphere IS the disc, and it measures "
        "&#960;&#183;1&#178; = &#960;. A constant BRDF k therefore returns k&#183;&#960; of "
        "everything that arrives; demanding that this equal the albedo forces "
        "<strong>k = albedo/&#960;</strong> and nothing else. The &#960; is not a convention and "
        "it is not a fudge factor &#8212; it is the area of a circle, arriving for the same "
        "reason it always does."),

    "FIG_ENERGY": ("l62_fig6.svg", "6",
        "Energy conservation is a number, and this engine fails it in two separate ways. The top "
        "chart is the expected failure: the raw cos&#8309; lobe with no constant at all returns "
        "more light than arrives for every shininess below about 12, peaking at "
        "<strong>2.6650</strong>. The 1/&#960; this lesson applies fixes that completely. The "
        "bottom chart is the failure that survives, and it was a surprise: because diffuse and "
        "specular are simply ADDED with no coupling, a white surface with any highlight at all "
        "reflects more than it receives &#8212; <strong>1.1386 at the engine&#8217;s default "
        "shininess of 32</strong>. The same light is being counted once as having bounced off "
        "the surface and once as having gone into it. That is what Fresnel fixes in Lesson 6.4, "
        "and it fixes it with a mechanism rather than a constant."),
}

LISTING_META = {
    "engine/include/engine/gfx/light.hpp": ("modified", "modified"),
    "shaders/scene.frag.hlsl": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_uniform.hpp": ("modified", "modified"),
    "scratch/verify_62.cpp": ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "demos/common/demo_scene.cpp": ("modified", "modified"),
    "demos/ecs_swarm/main.cpp": ("modified", "modified"),
    "demos/hello_cube/main.cpp": ("modified", "modified"),
    "demos/sandbox/main.cpp": ("modified", "modified"),
}

LISTING_LANG = {
    "shaders/scene.frag.hlsl": ("hlsl", "HLSL"),
}

# NOTHING PINNED YET, and here is the standing warning, which has a track record:
# build_57.py stamped a retired STATE block, build_58.py spliced a newer demo,
# build_510.py needed TWO pins, and build_61.py was pinned by THIS lesson before a
# line of it was written.
#
# PIN EVERY FILE THIS PAGE LISTS THAT A LATER LESSON TOUCHES. For this page the net
# is wide and the timing is short: `light.hpp` and `scene.frag.hlsl` are the two
# files the whole of the rest of Module 6 edits — 6.3 adds a microfacet
# distribution, 6.4 replaces `specular_brdf` outright, 6.5 pulls the material
# parameters out. Both are reproduced whole below. `gpu_uniform.hpp` grows a field
# the moment a second light or a shadow matrix appears.
#
#     git show <6.2 commit>:<path> > scratch/l62_<name>
#     git show <6.2 commit>:<path> | diff - scratch/l62_<name>
#
# The cheap way to find out you needed one is to re-run this builder and
# `git diff` the page BEFORE shipping anything else.
#
# PINNED IN LESSON 6.3, BEFORE A LINE OF IT WAS WRITTEN. All four listings are
# frozen at commit 3507837, the commit that shipped this page. The three tracked
# files were verified with `git show 3507837:<path> | diff - <pin>`;
# scratch/verify_62.cpp is gitignored, so its pin is a copy of the working tree
# taken before any 6.3 edit — which is the best provenance available for a file
# git does not track, and worth knowing is weaker.
#
# 6.3 was ALWAYS going to reach two of these: light.hpp gains a pointer to the new
# microfacet header, and scene.frag.hlsl is the other half of the lighting model.
# EXTENDED 2026-09-12 to cover EVERY listing, not just the files this
# lesson wrote. The partial dict below pinned what its author knew would
# move; everything else stayed live, so later lessons' edits leaked into
# this page anyway — which is why it no longer rebuilt to what it shipped.
# Pinned from 3507837, verified per file against the shipped page
# with scratch/which_commit.py.
#
# The original dict, kept for its provenance:
#   LISTING_SOURCE = {
#       "engine/include/engine/gfx/light.hpp":       "scratch/l62_light.hpp",
#       "shaders/scene.frag.hlsl":                   "scratch/l62_scene.frag.hlsl",
#       "engine/include/engine/gfx/gpu_uniform.hpp": "scratch/l62_gpu_uniform.hpp",
#       "scratch/verify_62.cpp":                     "scratch/l62_verify_62.cpp",
#   }
LISTING_SOURCE = {
    "engine/include/engine/gfx/light.hpp":       "scratch/l62_engine_include_engine_gfx_light.hpp",
    "shaders/scene.frag.hlsl":                   "scratch/l62_shaders_scene.frag.hlsl",
    "engine/include/engine/gfx/gpu_uniform.hpp": "scratch/l62_engine_include_engine_gfx_gpu_uniform.hpp",
    "scratch/verify_62.cpp":                     "scratch/l62_scratch_verify_62.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "demos/common/demo_scene.cpp": "scratch/l62_demos_common_demo_scene.cpp",
    "demos/ecs_swarm/main.cpp": "scratch/l62_demos_ecs_swarm_main.cpp",
    "demos/hello_cube/main.cpp": "scratch/l62_demos_hello_cube_main.cpp",
    "demos/sandbox/main.cpp": "scratch/l62_demos_sandbox_main.cpp",
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
<title>6.2 — Radiometry-Lite: What a BRDF Is · Build a Professional 3D Game Engine</title>
<meta name="description" content="Just enough radiometry to be dangerous: flux, irradiance and radiance, why radiance is the quantity a pixel holds, and the steradian derived from the geometry of a sphere. Then a BRDF stated as a ratio with units of inverse steradian — including why a value above 1 invents no energy — and the pi in Lambert's albedo/pi derived by integrating a constant BRDF over the hemisphere rather than quoted. Ends by finding that pi hiding inside this engine's own light intensity field since Lesson 3.6, and by measuring, as a number, exactly how far Blinn-Phong is from conserving energy.">

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
    <a class="prev-l" href="06-01-linear-and-srgb.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.1 — Linear and sRGB: The Gamma Lesson</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-03-microfacet-theory.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.3 — Microfacet Theory</span>
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
        with open(f"scratch/l62_body_{name}.html") as fh:
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
