#!/usr/bin/env python3
"""Assemble docs/lessons/06-13-bloom-post-stack.html.

Same pipeline as build_612.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l613_body_{a,b,c}.html, figures from scratch/figs_613.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-13-bloom-post-stack.html"

FIGURES = {
    "FIG_LID": ("l613_fig1.svg", "1",
        "Where Lesson 6.12 actually put the clipping point. A tonemap operator maps [0, inf) into "
        "[0, 1], but the DISPLAY has 256 codes, so each curve has a smallest input that already "
        "produces the largest code &#8212; and above it the collapse is as total as the clamp&#8217;s "
        "ever was. Solved by bisection through the engine&#8217;s own <code>to_encoded</code>, and "
        "ACES&#8217;s 6.3774 checked independently against the positive root of the quadratic its "
        "five constants imply. NOTE WHICH OPERATOR HAS THE HIGHEST LID: reinhard, at 223.5, and it "
        "is the one everybody agrees looks worst &#8212; because it spends its codes creeping "
        "toward a white it never reaches. Tonemapping is not the removal of clipping; it is the "
        "choice of where to clip."),

    "FIG_TAIL": ("l613_fig2.svg", "3",
        "The kernel the pyramid actually applies, measured on a single bright pixel and radially "
        "averaged. THE USUAL CLAIM FOR THE PYRAMID IS THAT IT IS CHEAP; the more interesting claim "
        "is that it is a BETTER SHAPE. A measured log-log slope of &#8722;2.01 is an inverse-square "
        "tail, which is roughly what glare in a real eye does &#8212; and which a sum of Gaussians "
        "whose widths double produces but no single Gaussian can, because exp(&#8722;r&#178;) has "
        "no tails worth the name. The fair comparison is the octave ratio, since it is "
        "normalisation-independent: over r = 32 to 64 the pyramid falls 6.0x and the Gaussian falls "
        "403x. Note also where the power law ENDS &#8212; past the pyramid&#8217;s reach there is "
        "no level left to contribute, so the tail does not decay, it stops."),

    "FIG_CHAIN": ("l613_fig3.svg", "2",
        "The chain. The levels are drawn at a FLOOR rather than at true relative size, because "
        "level 5 is 15x8 and at any scale that fits level 0 on a page that is three pixels &#8212; "
        "so the real sizes are printed instead. Down with a box filter that costs ONE bilinear tap per "
        "texel; back up with a 1-2-1 tent, ADDED into the level below by the hardware&#8217;s "
        "additive blend. The add is what makes level 0 hold the sum of every level rather than just "
        "the widest, and therefore what produces figure 3&#8217;s tail. The arithmetic below is the "
        "same one third Lesson 6.10 met in a mip chain, for the same reason &#8212; and the whole "
        "effect costs two thirds of ONE full-screen pass, against 399 million texel fetches for a "
        "single separable Gaussian of comparable reach. Eleven render passes, though: a pass writes "
        "one target, and none may read what it writes."),

    "FIG_KNEE": ("l613_fig4.svg", "5",
        "The bright pass&#8217;s threshold, and the quadratic that replaces its corner. THE "
        "ARTEFACT A HARD THRESHOLD CAUSES IS IN THE DERIVATIVE, NOT THE VALUE: the slope jumps from "
        "0 to 1 at T, so a pixel drifting from 0.999 to 1.001 goes from contributing nothing to "
        "contributing its full excess &#8212; and the set of pixels at exactly T is a contour that "
        "moves with the camera, which reads as a crawling edge along every gradient it crosses. The "
        "knee is not chosen but DERIVED: three continuity conditions (value and slope at the lower "
        "join, slope at the upper) and three coefficients, so exactly one quadratic fits. Note "
        "f(T) = k/4 &#8212; a pixel exactly at the threshold contributes a quarter of the knee "
        "width rather than nothing, which is the whole difference between a soft cut and a hard one."),

    "FIG_IDENTITIES": ("l613_fig5.svg", "4",
        "Two identities that between them decide both of the bloom&#8217;s filters, and neither is "
        "a trick for its own sake. LEFT: a bilinear sample placed at the corner shared by four "
        "texels has both fractional weights at exactly 1/2, so all four products are 1/4 and the "
        "fetch returns their unweighted mean &#8212; the texture unit computes the 2x2 box average "
        "for free, in hardware that was going to run anyway. That is why the pyramid&#8217;s "
        "sampler is LINEAR where the 6.12 resolve&#8217;s is deliberately NEAREST: here the filter "
        "mode is doing arithmetic, not smoothing. RIGHT: a box convolved with a box is a tent, so a "
        "bilinear magnification ALREADY IS a tent filter and the 1-2-1 weights are arrived at "
        "rather than picked for looking symmetric."),

    "FIG_AREA": ("l613_fig6.svg", "7",
        "The payoff, and it was a prediction with an exponent before it was a measurement. If the "
        "tail goes as r^-2 (figure 3 says it does), then the radius at which a glow crosses any "
        "fixed visibility threshold goes as sqrt(L) and the AREA goes as L &#8212; linearly, with a "
        "constant of proportionality. Tested over three decades against a fixed bar (the linear "
        "value that ACES plus the sRGB encode turns into exactly code 128), area/L holds to within "
        "8.6% and radius/sqrt(L) to within 4%. Below: the same three values through a clamp, which "
        "is three identical white dots. THE INFORMATION WAS IN THE HDR BUFFER AFTER 6.12 AND THERE "
        "WAS NO WAY TO SHOW IT."),

    "FIG_ORDER": ("l613_fig7.svg", "6",
        "Why the composite goes before the curve. The physical argument settles it already &#8212; "
        "scattering happens in the LENS, before the sensor responds, so a bloom is light and is "
        "photographed by the same curve as the light that did not scatter. But the arithmetic says "
        "it more bluntly: the curve&#8217;s output is already in [0, 1], so anything added to it "
        "lands above 1 and the encode clips it. MEASURED ON THE SAME IMAGE, ten times as many "
        "pixels clip the wrong way around &#8212; every glow grows a flat white core that grows "
        "with the source&#8217;s brightness, which is precisely the artefact bloom was introduced "
        "to remove, reintroduced one pass later."),
}

LISTING_META = {
    "engine/include/engine/gfx/bloom.hpp": ("new", "new"),
    "engine/src/gfx/bloom.cpp": ("new", "new"),
    "shaders/bloom_bright.frag.hlsl": ("new", "new"),
    "shaders/bloom_down.frag.hlsl": ("new", "new"),
    "shaders/bloom_up.frag.hlsl": ("new", "new"),
    "engine/include/engine/gfx/gpu_post.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_post.cpp": ("modified", "modified"),
    "shaders/tonemap.frag.hlsl": ("modified", "modified"),
    "scratch/verify_613.cpp": ("new", "new"),
}

LISTING_LANG = {
    "shaders/bloom_bright.frag.hlsl": ("hlsl", "HLSL"),
    "shaders/bloom_down.frag.hlsl": ("hlsl", "HLSL"),
    "shaders/bloom_up.frag.hlsl": ("hlsl", "HLSL"),
    "shaders/tonemap.frag.hlsl": ("hlsl", "HLSL"),
}

LISTING_SOURCE = {
    # PINNED 2026-09-11, at the start of 6.14's session, exactly as the note below
    # instructed. Every path here is read from a FROZEN COPY of what commit 4637254
    # shipped, not from the live tree — so 6.14 may edit these files freely and a
    # rebuild of THIS page still reproduces what the reader was shown.
    "engine/include/engine/gfx/bloom.hpp": "scratch/l613_bloom.hpp",
    "engine/src/gfx/bloom.cpp": "scratch/l613_bloom.cpp",
    "shaders/bloom_bright.frag.hlsl": "scratch/l613_bloom_bright.frag.hlsl",
    "shaders/bloom_down.frag.hlsl": "scratch/l613_bloom_down.frag.hlsl",
    "shaders/bloom_up.frag.hlsl": "scratch/l613_bloom_up.frag.hlsl",
    "engine/include/engine/gfx/gpu_post.hpp": "scratch/l613_gpu_post.hpp",
    "engine/src/gfx/gpu_post.cpp": "scratch/l613_gpu_post.cpp",
    "shaders/tonemap.frag.hlsl": "scratch/l613_tonemap.frag.hlsl",
    # gitignored, so its only provenance is a working-tree copy taken before edits.
    "scratch/verify_613.cpp": "scratch/l613_verify_613.cpp",
}

# PINNED. (Historical note, kept because the prediction is worth grading.)
#
# Lesson 6.14 is ANTIALIASING, and the files most likely to move are the ones
# that decide what a colour target IS: MSAA changes `sample_count` on every
# pipeline and every target, so `gpu_post.hpp`'s `k_hdr_format` neighbourhood,
# `gpu_post.cpp`'s pipeline creation, and the resolve are all plausible. A
# post-process AA (FXAA, SMAA) would instead add a stage — which is the first
# real test of §6's claim that `gpu_post_stack` stops scaling at four or five,
# and would edit it directly.
#
# `bloom.hpp` / `bloom.cpp` should NOT move, which makes them the useful control.
#
# So pin all eight repository files before writing a line of 6.14:
#
#     git show <6.13 commit>:<path> > scratch/l613_<name>
#     git show <6.13 commit>:<path> | diff - scratch/l613_<name>     # must be empty
#
# And take `scratch/verify_613.cpp` EARLY — gitignored, so its only provenance is
# a working-tree copy, which cannot be recovered afterwards.


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
<title>6.13 — Bloom and the Post-Processing Stack · Build a Professional 3D Game Engine</title>
<meta name="description" content="Lesson 6.12 stopped the engine destroying bright values; it did not make them visible. Measured, the clipping point only MOVED: under ACES the smallest linear value that still resolves to code 255 is 6.3774, so 6.12's polished metal still spends 13.10 stops on one code - and an 8-bit sRGB display holds 11.69 stops in total, so no curve can fix it. What fixes it is a change of variable: a real lens scatters, so a source brighter than white lands as a spike with wide skirts, and the skirts are BELOW the lid even when the source is far above it. The display puts the value in area instead of intensity, and over three decades that conversion is linear to within 8.6 percent. The pyramid derived as a sum of Gaussians whose widths double - an inverse-square tail, where a single Gaussian falls 403x over an octave against the pyramid's 6.0x, so it is cheaper AND closer to the physics. Both filters derived rather than chosen: one bilinear tap IS a 2x2 box average, and a box convolved with a box IS the 1-2-1 tent. The soft knee from three continuity conditions. Fireflies, where one pixel in 65,536 contributes 62.8 percent of the bloom, and the trade the one-tap downsample forces. Then the two questions gpu_post.hpp deferred in its own shipped words: who owns the intermediates, and what makes that answer stop scaling. 39 checks, with GPU, CPU and a paper derivation agreeing 4 of 4.">

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
    <a class="prev-l" href="06-12-hdr-tonemapping.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.12 — HDR and Tonemapping</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-14-antialiasing.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.14 — Antialiasing: Geometric and Shading</span>
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
        with open(f"scratch/l613_body_{name}.html") as fh:
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
