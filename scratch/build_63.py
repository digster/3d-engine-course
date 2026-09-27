#!/usr/bin/env python3
"""Assemble docs/lessons/06-03-microfacet-theory.html.

Same pipeline as build_62.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l63_body_{a,b,c}.html, figures from scratch/figs_63.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-03-microfacet-theory.html"

FIGURES = {
    "FIG_LANDSCAPE": ("l63_fig1.svg", "1",
        "The whole model in one picture, and it is a counting problem. Every microfacet is a "
        "PERFECT mirror &#8212; not &#8220;a bit rough&#8221; &#8212; so a facet sends light from "
        "l to v only if its own normal happens to be h. Lesson 3.7 derived h as &#8220;the normal "
        "this surface would need in order to bounce the light into the eye&#8221;; here that "
        "stops being a way of putting it and becomes the mechanism. <strong>4 of these 48 facets "
        "qualify</strong>, and the faint normals are the ones that do not. Roughness is how "
        "widely those normals are spread &#8212; which is a HISTOGRAM OF SLOPES, a thing you can "
        "measure with an instrument, unlike a shininess exponent."),

    "FIG_DENSITY": ("l63_fig2.svg", "2",
        "Why this is a model and not a curve someone drew. Three GGX distributions, cosine-"
        "weighted: the peaks differ by <strong>sixteen times</strong> and the areas are all "
        "<strong>exactly 1</strong> &#8212; measured at 1.0000004 on a 400,000-step grid. The "
        "identity is worth reading backwards, because that reading is much better than "
        "&#8220;the distribution is normalised&#8221;: <strong>the microfacets&#8217; projected "
        "areas add up to the area of the flat surface they stand on.</strong> Nothing else could "
        "be true, which is why the cosine is in the integral and is not a convention. Lesson "
        "3.7&#8217;s bare cos&#8345; lobe integrates to 0.1848 instead, which is precisely what "
        "makes it a shape rather than a model."),

    "FIG_CONSTANT": ("l63_fig3.svg", "3",
        "The finding, and it has been shipping since Lesson 3.7. Blinn-Phong&#8217;s lobe IS a "
        "microfacet distribution &#8212; it satisfies Figure 2&#8217;s identity exactly, once "
        "given the constant <code>(s+2)/2&#960;</code>. The engine uses <code>1/&#960;</code>. "
        "The ratio is <code>(s+2)/2</code>, with no &#960; in it because both carry one and it "
        "cancels: <strong>17&#215; at the default shininess of 32</strong>, and 257&#215; at 512. "
        "Nothing ever looked wrong, and that is the part worth remembering &#8212; "
        "<code>specular::colour</code> absorbed it, so every shiny material in this engine has "
        "been authored at seventeen times a physically plausible reflectance. An error a "
        "PARAMETER can absorb is invisible until someone tries to author against real values."),

    "FIG_TAIL": ("l63_fig4.svg", "4",
        "Same roughness, same peak (3.5368 for both), and the log axis is the only way to see "
        "what separates them. <strong>GGX is LOWER than Beckmann near the peak</strong> "
        "&#8212; 0.71&#215; at 20&#176; &#8212; and then crosses over to <strong>456&#215; at "
        "45&#176;</strong>. It has taken energy out of the shoulder and put it in the tail, which "
        "it must, since both integrate to 1. The mechanism is in the formulas: Beckmann&#8217;s "
        "exponential dies faster than any power and is numerically zero (1.9e&#8722;13) by "
        "60&#176;, a hard edge where a real surface has a soft one; GGX&#8217;s rational function "
        "has a power-law tail that never quite stops. That faint glow around a highlight is why "
        "every engine now ships GGX."),

    "FIG_MASKING": ("l63_fig5.svg", "5",
        "The second term, and it is a CONSEQUENCE rather than a correction: once you have said "
        "&#8220;landscape&#8221;, you have said &#8220;some of it is hidden&#8221;. On the left, "
        "a grazing light reaches only <strong>10 of 46</strong> facets; the rest are in each "
        "other&#8217;s shadow. On the right, Smith&#8217;s G&#8321; against angle &#8212; 1 "
        "looking straight down, falling toward 0 at grazing, and falling faster the rougher the "
        "surface. <strong>This is the term Lesson 6.2 was missing</strong> when it measured a "
        "5.36&#215; view-angle swing it could not explain: at 75&#176; a near-smooth surface "
        "still shows 0.9920 of itself and a fully rough one only 0.4112. The effect was always "
        "real; what it lacked was a reason."),

    "FIG_BUDGET": ("l63_fig6.svg", "6",
        "Lesson 6.2&#8217;s energy test, reused character for character, pointed at the new "
        "machinery &#8212; and the shape of the answer is completely different. <strong>It never "
        "exceeds 1</strong>, at any roughness or view angle, where Blinn-Phong was over at every "
        "shininess. That is what the machinery bought, and it is guaranteed rather than hoped "
        "for, because D is a distribution and G is a fraction. But read the other end: a fully "
        "rough surface returns only <strong>0.3069</strong> of what arrives with Fresnel set to 1 "
        "&#8212; a surface that absorbs nothing. The pink is light the MODEL loses, because "
        "single-scattering theory lets a facet bounce light once and then forgets it. The missing "
        "69% hit a second facet on the way out. That is why rough metal renders dark, in every "
        "engine that has not bought it back."),
}

LISTING_META = {
    "engine/include/engine/gfx/microfacet.hpp": ("new", "new"),
    "scratch/verify_63.cpp": ("new", "new"),
}

LISTING_LANG = {}

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
#
# PINNED AGAIN IN LESSON 6.4, before a line of it was written, for exactly the
# reason above: 6.4 REPLACES the specular BRDF, so it edits microfacet.hpp (adds
# Fresnel and the Cook-Torrance assembly beside D and G) and rewrites
# verify_63.cpp's subject matter. Both are listed WHOLE on this page. Frozen at
# commit 8735ba5, the commit that shipped this page; microfacet.hpp verified with
# `git show 8735ba5:<path> | diff - <pin>`, and verify_63.cpp copied from the
# working tree before any 6.4 edit (gitignored, so that is the best provenance
# there is - and weaker, which is why it was taken first).
LISTING_SOURCE = {
    "engine/include/engine/gfx/light.hpp":       "scratch/l62_light.hpp",
    "shaders/scene.frag.hlsl":                   "scratch/l62_scene.frag.hlsl",
    "engine/include/engine/gfx/gpu_uniform.hpp": "scratch/l62_gpu_uniform.hpp",
    "scratch/verify_62.cpp":                     "scratch/l62_verify_62.cpp",
    "engine/include/engine/gfx/microfacet.hpp":  "scratch/l63_microfacet.hpp",
    "scratch/verify_63.cpp":                     "scratch/l63_verify_63.cpp",
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
<title>6.3 — Microfacet Theory · Build a Professional 3D Game Engine</title>
<meta name="description" content="A surface as a landscape of microscopic perfect mirrors, so that roughness stops being a slider and becomes a statistical claim about slopes. Derives the normal distribution function and the identity it must satisfy, shows that Blinn-Phong was a microfacet distribution all along and that this engine ships its constant wrong by exactly 17x, measures what GGX's tail buys over Beckmann at 456x, derives shadowing and masking as a consequence rather than a correction, and measures the 69% of light that single-scattering microfacet theory loses at full roughness. Along the way the normalisation test finds catastrophic cancellation in the textbook GGX denominator.">

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
    <a class="prev-l" href="06-02-what-a-brdf-is.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.2 — Radiometry-Lite: What a BRDF Is</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-04-cook-torrance.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.4 — Cook–Torrance PBR, Derived</span>
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
        with open(f"scratch/l63_body_{name}.html") as fh:
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
