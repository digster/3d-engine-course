#!/usr/bin/env python3
"""Assemble docs/lessons/06-04-cook-torrance.html.

Same pipeline as build_63.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l64_body_{a,b,c}.html, figures from scratch/figs_64.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-04-cook-torrance.html"

FIGURES = {
    "FIG_DOOR": ("l64_fig1.svg", "1",
        "The whole lesson, and it is an accounting problem, so here are the accounts. Lessons "
        "3.7 to 6.3 added the two lobes with no coupling: the diffuse returned the FULL albedo "
        "&#8212; 1.0000 for a white surface, exactly filling the light that arrived &#8212; and "
        "the specular was added on top, so the total overran to <strong>1.1386</strong>. The same "
        "photon left twice, once as having bounced off and once as having gone in and scattered "
        "back out. Lesson 6.3 normalised the distribution and this did not move, deliberately: "
        "<strong>the fault is the plus sign, not either term.</strong> With Fresnel coupling the "
        "diffuse lobe receives only <code>1 - F</code> &#8212; the light that got past the "
        "interface &#8212; and the two together come to 0.9144. Note where the second row stops: "
        "it does not reach the line either, and &#167;10 says where that light went."),

    "FIG_JACOBIAN": ("l64_fig2.svg", "2",
        "Where the 4 in <code>4(n&#183;l)(n&#183;v)</code> comes from, and it is two factors of "
        "two. Put spherical coordinates on the FIXED direction v: a microfacet tilted by "
        "&#952;&#8341; reflects v into a ray at exactly <strong>2&#952;&#8341;</strong> "
        "&#8212; the galvanometer-mirror fact, measured here to 1.2e&#8722;07. So "
        "<code>d&#952;_out = 2 d&#952;_h</code> gives one factor of two, and "
        "sin 2&#952; = 2 sin&#952; cos&#952; gives the other &#8212; handing over the "
        "<code>cos &#952;&#8341; = v&#183;h</code> as change. The whole derivation is four lines "
        "and one identity you already know, which is worth contrasting with how often the result "
        "is simply quoted."),

    "FIG_DENOMINATOR": ("l64_fig3.svg", "3",
        "Four factors, four different reasons, and not one of them a convention. The "
        "<strong>4</strong> is Figure 2&#8217;s Jacobian. The <strong>(v&#183;h)</strong> it "
        "introduces <em>cancels</em>, against the microfacets&#8217; projected area toward the "
        "light &#8212; which is (l&#183;h), and equal to it because h bisects l and v. "
        "<strong>That cancellation is exactly why the finished formula looks unmotivated:</strong> "
        "the term that would explain the 4 is not in it. The <strong>(n&#183;v)</strong> converts "
        "flux to radiance, which is defined per unit PROJECTED area; the <strong>(n&#183;l)</strong> "
        "is the BRDF&#8217;s own definition as a quantity per unit irradiance."),

    "FIG_FRESNEL": ("l64_fig4.svg", "4",
        "The shape first, the formula second. Every curve starts at its material&#8217;s F0 "
        "&#8212; 0.02 for water, 0.04 for glass, 0.17 for diamond &#8212; sits nearly flat for "
        "forty-five degrees, and then rises to <strong>exactly 1</strong> at grazing. Not close "
        "to 1: exactly, for every material, which is why a window becomes a mirror when you look "
        "along it and why it does not matter what the window is made of. Schlick&#8217;s "
        "approximation (dashed) gets both ENDS exactly right by construction and fits the middle; "
        "the panel is how well. <strong>The relative error is 23.2% and it sits at 55&#176;</strong>, "
        "in the middle of the range where surfaces are usually seen &#8212; which is not what "
        "&#8220;very accurate&#8221; normally means. It is defensible because 23% of 0.04 is "
        "0.019 of a reflectance, and that is invisible; it is not defensible because the fit is "
        "tight."),

    "FIG_METAL": ("l64_fig5.svg", "5",
        "Why the metallic workflow is physics rather than a pipeline convention. A "
        "<strong>dielectric</strong> reflects a grey 4% off its clear outer layer &#8212; no "
        "pigment there, so the highlight is the colour of the LAMP &#8212; and the rest goes in, "
        "picks up the pigment, and scatters back out. Its colour lives in its ALBEDO. A "
        "<strong>conductor</strong> absorbs everything that crosses the interface within a few "
        "atomic layers, so there is no diffuse lobe at all and the colour has nowhere to live but "
        "F0. Gold&#8217;s F0 is (1.00, 0.71, 0.29). One albedo field therefore serves both "
        "materials and <code>metallic</code> decides which question it is answering &#8212; which "
        "is not a compression of two workflows, it is the two sentences above, written down."),

    "FIG_ENERGY": ("l64_fig6.svg", "6",
        "Lesson 6.2&#8217;s hemispherical-reflectance integrator, unchanged for the third lesson "
        "running, pointed at the finished BRDF. Without coupling the surface emits light at every "
        "roughness, worst <strong>1.2353</strong> at grazing. With it, R never reaches 1. But "
        "read the panel, because the finding is there: <strong>the coupling nearly every engine "
        "ships &#8212; 1 &#8722; F(v&#183;h), including the glTF reference BRDF &#8212; still "
        "reaches 1.3395.</strong> It accounts for the light that got IN and says nothing about the "
        "light that fails to get OUT, and a diffuse ray leaving toward a grazing eye meets the "
        "interface at a grazing angle. The two-crossing form fixes that and is reciprocal, which "
        "the obvious repair was not. It also never quite reaches 1, and &#167;10 says where that "
        "goes."),
}

LISTING_META = {
    "engine/include/engine/gfx/microfacet.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/light.hpp":      ("modified", "modified"),
    "shaders/scene.frag.hlsl":                  ("modified", "modified"),
    "scratch/verify_64.cpp":                    ("new", "new"),
}

LISTING_LANG = {
    "shaders/scene.frag.hlsl": ("hlsl", "HLSL"),
}

# NOTHING PINNED, AND THE NET FOR 6.5 IS THE WIDEST IT HAS EVER BEEN.
#
# The standing warning, which has a track record: build_57.py stamped a retired
# STATE block, build_58.py spliced a newer demo, build_510.py needed TWO pins, and
# build_61.py, build_62.py and build_63.py were each pinned by the FOLLOWING
# lesson before a line of it was written — three lessons running where the rebuild
# diff came out to exactly the nav lines that were meant to move. That is the
# standard now, not a lucky streak.
#
# PIN EVERY FILE THIS PAGE LISTS THAT A LATER LESSON TOUCHES. Lesson 6.5 is THE
# MATERIAL SYSTEM, and this page lists `microfacet.hpp` (which holds
# `microsurface`, the struct 6.5 grows into a material) and `light.hpp` (whose
# `shade()` signature 6.5 changes). Both are certain. `scene.frag.hlsl` is close
# behind, since a material system that does not reach the shader is not one.
#
#     git show <6.4 commit>:<path> > scratch/l64_<name>
#     git show <6.4 commit>:<path> | diff - scratch/l64_<name>
#
# And take `scratch/verify_64.cpp`'s pin EARLY — it is gitignored, so its only
# provenance is a working-tree copy, which is weaker and cannot be recovered:
#
#     cp scratch/verify_64.cpp scratch/l64_verify_64.cpp
#
# The cheap way to find out you needed one is to re-run this builder and
# `git diff` the page BEFORE shipping anything else.
#
# PINNED IN LESSON 6.5, before a line of it was written, for the reason the
# warning above gives: 6.5 is the material system, and `microsurface` (in
# microfacet.hpp) is the struct it grows into a material while `shade()` (in
# light.hpp) is the function whose signature it changes. Frozen at commit
# e394559, the commit that shipped this page; the three tracked files verified
# with `git show e394559:<path> | diff - <pin>`, and verify_64.cpp copied from the
# working tree during 6.4 itself, since it is gitignored and that provenance
# cannot be recovered afterwards.
# EXTENDED 2026-09-12 to cover EVERY listing, not just the files this
# lesson wrote. The partial dict below pinned what its author knew would
# move; everything else stayed live, so later lessons' edits leaked into
# this page anyway — which is why it no longer rebuilt to what it shipped.
# Pinned from e394559, verified per file against the shipped page
# with scratch/which_commit.py.
#
# The original dict, kept for its provenance:
#   LISTING_SOURCE = {
#       "engine/include/engine/gfx/microfacet.hpp": "scratch/l64_microfacet.hpp",
#       "engine/include/engine/gfx/light.hpp":      "scratch/l64_light.hpp",
#       "shaders/scene.frag.hlsl":                  "scratch/l64_scene.frag.hlsl",
#       "scratch/verify_64.cpp":                    "scratch/l64_verify_64.cpp",
#   }
LISTING_SOURCE = {
    "engine/include/engine/gfx/microfacet.hpp": "scratch/l64_engine_include_engine_gfx_microfacet.hpp",
    "engine/include/engine/gfx/light.hpp":      "scratch/l64_engine_include_engine_gfx_light.hpp",
    "shaders/scene.frag.hlsl":                  "scratch/l64_shaders_scene.frag.hlsl",
    "scratch/verify_64.cpp":                    "scratch/l64_scratch_verify_64.cpp",
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
<title>6.4 — Cook–Torrance PBR, Derived · Build a Professional 3D Game Engine</title>
<meta name="description" content="Fresnel is the term that couples the diffuse and specular lobes, and coupling them is what stops a surface emitting more light than arrives. Derives the 4(n.l)(n.v) denominator from a change of variables rather than quoting it, builds Fresnel from the physics and measures Schlick's fit at 23% relative error, derives F0 from the index of refraction and uses the round trip to audit the engine's own materials at an implied IOR of 24.6, arrives at the metallic workflow as a consequence, and measures that the coupling most engines ship still reaches 1.3395. Breaks the reference render on purpose for the first time in fifteen lessons, and extends it at the one moment that is free.">

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
    <a class="prev-l" href="06-03-microfacet-theory.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.3 — Microfacet Theory</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-05-material-system.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.5 — A Material System</span>
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
        with open(f"scratch/l64_body_{name}.html") as fh:
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
