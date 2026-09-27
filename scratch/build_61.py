#!/usr/bin/env python3
"""Assemble docs/lessons/06-01-linear-and-srgb.html.

Same pipeline as build_511.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l61_body_{a,b,c}.html, figures from scratch/figs_61.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-01-linear-and-srgb.html"

FIGURES = {
    "FIG_TWO_THINGS": ("l61_fig1.svg", "1",
        "The whole subject in one curve. The dashed diagonal is what everybody assumes a stored "
        "value means; the curve is what it actually means. Read it in both directions, because "
        "the two readings are the two integers worth memorising: <strong>code 128 emits 0.2159 "
        "of white&#8217;s light</strong>, not half, and <strong>half the light is stored as code "
        "188</strong>. Sixty codes apart. Note also the SHAPE of the disagreement &#8212; the "
        "curve sags below the diagonal everywhere and most in the darks, so every mistake made "
        "here is small near white and enormous in shadow, which is exactly the shape that "
        "hides."),

    "FIG_BUDGET": ("l61_fig2.svg", "2",
        "Why an encoding exists at all, and it is not the CRT story. Both strips hold the same "
        "256 codes, positioned by how much light each emits. Spent evenly in light, half the "
        "budget &#8212; 128 codes &#8212; goes to the brightest half of the range, where the eye "
        "can barely tell two neighbours apart, and the darkest tenth gets 26. Spent by the sRGB "
        "curve, the darkest tenth gets <strong>90</strong>. The eye judges RATIOS, so equal "
        "steps should be equal ratios; that is what a power curve does, and it is why 8 bits of "
        "linear light needs about 12 to look as smooth."),

    "FIG_CURVE": ("l61_fig3.svg", "3",
        "The dark end, magnified, where the piecewise definition earns its keep. Below linear "
        "0.0031308 the encoding is a straight line of slope 12.92; above it, a shifted power. "
        "<strong>The straight piece is not a committee compromise</strong> &#8212; the derivative "
        "of x^(1/2.4) is infinite at zero, so a pure power curve has unbounded gain at black, "
        "amplifies sensor noise into banding, and cannot be inverted stably. The dashed line is "
        "the <code>pow(x, 1/2.2)</code> everyone reaches for: close in the midtones, and off by "
        "<strong>8 codes</strong> down here, where the eye has the most codes to notice it "
        "with."),

    "FIG_PIPELINE": ("l61_fig4.svg", "4",
        "The rule is not &#8220;convert carefully&#8221;, it is <strong>convert twice, at the "
        "edges</strong> &#8212; and the difference is structural rather than a matter of "
        "diligence. Converting per operation (which is what Lesson 1.6 did, and said so) makes "
        "every new operation a chance to forget, and quantises to 8 bits after every step. "
        "Converting at the edges makes the middle a region where ordinary arithmetic is valid, "
        "because there is nothing else in it. The audit in &#167;5 walks this picture looking "
        "for edges that do not convert, and finds one: <strong>the GPU path had an input edge "
        "and no output edge at all.</strong>"),

    "FIG_RAMP": ("l61_fig5.svg", "5",
        "The bug, on real downloaded pixels: nine light levels, rendered by the identical shader "
        "into the two target formats, with the swatches drawn from the stored codes themselves "
        "&#8212; so the top row is literally what the engine displayed for four modules. "
        "<strong>Read the ratio row from the right.</strong> Near white the error is 1.1x and "
        "looks like nothing; in shadow it is 13x. An error that is a RATIO rather than an offset "
        "hides wherever there is least contrast to spare, and reads as a deliberate moody grade "
        "rather than a defect &#8212; which is the whole answer to &#8220;how did nobody "
        "notice?&#8221;"),

    "FIG_TARGETS": ("l61_fig6.svg", "6",
        "One shader, one quantity of light, three destinations &#8212; and the middle two agree "
        "with each other and with <code>engine::linear_to_srgb_u8</code> to the code. The left "
        "one is the engine before this lesson. <strong>The two right answers are not equally "
        "right:</strong> hardware blending happens AFTER the fragment shader, so a shader that "
        "encodes hands the blend unit codes to interpolate, which is this lesson&#8217;s own "
        "mistake moved one stage later. Correct for opaque geometry, wrong the moment anything "
        "is transparent &#8212; so prefer the swapchain, keep the fallback, and read the field "
        "that says which one you are on."),
}

LISTING_META = {
    "engine/include/engine/gfx/gpu_device.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_device.cpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_uniform.hpp": ("modified", "modified"),
    "shaders/scene.frag.hlsl": ("modified", "modified"),
    "engine/include/engine/gfx/colour.hpp": ("unchanged", "unchanged"),
    "engine/src/gfx/colour.cpp": ("unchanged", "unchanged"),
    "scratch/verify_61.cpp": ("new", "new"),
}

LISTING_LANG = {
    "shaders/scene.frag.hlsl": ("hlsl", "HLSL"),
}

# PIN EVERY FILE THIS PAGE LISTS THAT A LATER LESSON TOUCHES — a rule with a
# track record: build_57.py stamped a retired STATE block, build_58.py spliced a
# newer demo, build_510.py needed TWO pins (the demo, which its own note
# predicted, and actions.hpp, which it did not).
#
# PINNED IN LESSON 6.2, BEFORE A LINE OF IT WAS WRITTEN. All six repository files
# this page reproduces are frozen at commit 373dd4b, the commit that shipped this
# page, and verified byte-identical to it:
#
#     git show 373dd4b:<path> | diff - scratch/l61_<name>
#
# Two of them 6.2 was always going to touch (`gpu_uniform.hpp`'s `key` comment and
# `scene.frag.hlsl`'s shading equation, since the pi moves into the BRDF); the other
# four are pinned anyway, because the rule earned by three separate incidents is
# that the file which bites you is the one you were sure was finished.
# EXTENDED 2026-09-12 to cover EVERY listing, not just the files this
# lesson wrote. The partial dict below pinned what its author knew would
# move; everything else stayed live, so later lessons' edits leaked into
# this page anyway — which is why it no longer rebuilt to what it shipped.
# Pinned from 373dd4b, verified per file against the shipped page
# with scratch/which_commit.py.
#
# The original dict, kept for its provenance:
#   LISTING_SOURCE = {
#       "engine/include/engine/gfx/gpu_device.hpp":   "scratch/l61_gpu_device.hpp",
#       "engine/src/gfx/gpu_device.cpp":              "scratch/l61_gpu_device.cpp",
#       "engine/include/engine/gfx/gpu_uniform.hpp":  "scratch/l61_gpu_uniform.hpp",
#       "shaders/scene.frag.hlsl":                    "scratch/l61_scene.frag.hlsl",
#       "engine/include/engine/gfx/colour.hpp":       "scratch/l61_colour.hpp",
#       "engine/src/gfx/colour.cpp":                  "scratch/l61_colour.cpp",
#   }
LISTING_SOURCE = {
    "engine/include/engine/gfx/gpu_device.hpp":  "scratch/l61_engine_include_engine_gfx_gpu_device.hpp",
    "engine/src/gfx/gpu_device.cpp":             "scratch/l61_engine_src_gfx_gpu_device.cpp",
    "engine/include/engine/gfx/gpu_uniform.hpp": "scratch/l61_engine_include_engine_gfx_gpu_uniform.hpp",
    "shaders/scene.frag.hlsl":                   "scratch/l61_shaders_scene.frag.hlsl",
    "engine/include/engine/gfx/colour.hpp":      "scratch/l61_engine_include_engine_gfx_colour.hpp",
    "engine/src/gfx/colour.cpp":                 "scratch/l61_engine_src_gfx_colour.cpp",
    "scratch/verify_61.cpp":                     "scratch/l61_scratch_verify_61.cpp",
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
<title>6.1 — Linear and sRGB: The Gamma Lesson · Build a Professional 3D Game Engine</title>
<meta name="description" content="The gamma lesson: why a stored colour channel is a code for light rather than a quantity of it, why the sRGB encoding exists as a perceptual budget rather than a CRT artefact, and why its transfer function is piecewise. Derives the linear toe from the slope of a power curve at zero, then audits every colour conversion in the engine and finds the one that was missing: SDL claims windows with a swapchain whose values are sRGB codes, and the fragment shader had been returning linear light into it since Module 4 — measured at 13x too dark in shadow.">

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
    <a class="prev-l" href="05-12-checkpoint-game.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.12 — Checkpoint: A Small 3D Game on the Public API</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-02-what-a-brdf-is.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.2 — Radiometry-Lite: What a BRDF Is</span>
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
        with open(f"scratch/l61_body_{name}.html") as fh:
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
