#!/usr/bin/env python3
"""Assemble docs/lessons/06-08-shadow-mapping.html.

Same pipeline as build_67.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l68_body_{a,b,c}.html, figures from scratch/figs_68.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-09-cascaded-shadows.html"

FIGURES = {
    "FIG_PROBLEM": ("l69_fig1.svg", "1",
        "Where one map&#8217;s texels actually go. Lesson 6.8 fits a square box around the whole "
        "SCENE, so <code>world_per_texel</code> is one number everywhere &#8212; 0.0548 m on this "
        "40-metre scene. The crate two metres away fills half the screen and gets 39-millimetre "
        "texels; the rock forty metres away covers nine pixels and gets exactly the same. The box "
        "was fitted to the thing that does not matter, which is why the near field&#8217;s shadow "
        "edges are a staircase while the far field looks fine."),

    "FIG_SPLITS": ("l69_fig2.svg", "2",
        "Three ways to cut a frustum between 0.1 m and 60 m, and why the answer is the blend. "
        "UNIFORM gives equal metres, so cascade 0 covers 15 m &#8212; most of what you can see, "
        "and barely denser than the single map. LOGARITHMIC is derived from the right argument "
        "(screen density falls as 1/d, so slice depths must rise with d) and lands its first cut "
        "at 0.55 m, spending three quarters of the budget on your own feet. The PRACTICAL scheme "
        "interpolates between them: at &#955; = 0.5 the cuts are 7.78, 16.25 and 28.57 m. "
        "&#955; is the one number here chosen by eye, and what makes that defensible is that both "
        "ends of the dial are explicable."),

    "FIG_SPHERE": ("l69_fig3.svg", "3",
        "Why the box is fitted to a sphere. The same frustum slice is shown at two camera yaws. "
        "The dashed axis-aligned box that contains its eight corners must grow to hold the "
        "diagonal and shrink again as they realign &#8212; measured at 30.2% across a full turn. "
        "Since <code>world_per_texel</code> is the box side over the resolution, that is every "
        "texel in the map changing size while the camera merely turned, which is the shimmer. The "
        "circle is identical in both panels: a sphere has no orientation, and the measured spread "
        "over 360&#176; is 9.9&#215;10<sup>&#8722;8</sup>, which is float rounding rather than "
        "geometry. It costs 29% of the resolution and that is the trade."),

    "FIG_SNAP": ("l69_fig4.svg", "4",
        "Texel snapping, over four frames of a camera walking forward in sub-texel steps. The "
        "purple bar is fixed geometry and the highlighted line is where its shadow edge lands. "
        "UNSNAPPED the grid slides continuously, so the edge is quantised to a different texel "
        "every frame and the eye reads the result as crawling. SNAPPED the box centre is rounded "
        "down to a whole texel in light space, so the grid lands on the same world positions it "
        "landed on last frame and the edge either stays put or moves exactly one texel. Measured: "
        "7.6&#215;10<sup>&#8722;6</sup> texels of drift against 0.499 &#8212; and 0.499 is the "
        "worst it can be, since half a texel is the furthest anything can get from a grid line."),

    "FIG_BIAS": ("l69_fig5.svg", "5",
        "The audit of Lesson 6.8, and the surprise inside it. Cascades change "
        "<code>world_per_texel</code> by a factor of 7.3 and NOT ONE LINE of 6.8&#8217;s bias "
        "derivation had to move, because each cascade owns a <code>light_camera</code> and that is "
        "where both terms live. The surprise is the last column: in DEVICE depth, cascades 1, 2 "
        "and 3 all want the same bias. Both the texel size and the depth range are set by the same "
        "sphere, so the radius cancels and bias &#8776; reach&#183;tan&#952;/resolution &#8212; "
        "2.0716&#215;10<sup>&#8722;3</sup> predicted against 2.031&#215;10<sup>&#8722;3</sup> "
        "measured. Cascade 0 is the exception at 64%, because its range is set by the CASTERS "
        "rather than by its own sphere, which is exactly when the derivation says cancellation "
        "should fail."),

    "FIG_SEAM": ("l69_fig6.svg", "6",
        "The seam, which is a new artefact with a new name. Two cascades meet at 16.25 m with "
        "different texel grids (fine on the near side, coarse on the far) and different biases, so "
        "they disagree along one line at a fixed distance from the camera &#8212; precisely the "
        "shape the eye is best at finding. Blending a band either side turns a step into a ramp: "
        "both answers are defensible, so anything between them is too. It costs a second lookup, "
        "but only inside the band. <code>blend_fraction</code> defaults to 0, which shows the "
        "seam, because Lesson 3.5&#8217;s rule is that the failure has to be reachable or the fix "
        "is folklore."),
}

# THE SECOND TUPLE ELEMENT IS THE STATUS WORD, NOT THE LANGUAGE. `listing()`
# unpacks these as `tag, word` and emits `<span class="tag {tag}">{word}</span>`,
# so a language here puts "cpp" in a pill that every other lesson fills with
# "new" or "modified". The LANGUAGE belongs in LISTING_LANG, which already
# carries it and which drives the separate `.lang` span. Ran from 6.9 to 6.12
# before anybody looked at a pill.
LISTING_META = {
    "engine/include/engine/gfx/cascade.hpp": ("new", "new"),
    "engine/src/gfx/cascade.cpp": ("new", "new"),
    "scratch/verify_69.cpp": ("new", "new"),
}

LISTING_LANG = {
    "shaders/shadow.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/scene.frag.hlsl":  ("hlsl", "HLSL"),
}

LISTING_SOURCE = {
    # PINNED at the start of the 6.10 session, verified byte-identical against
    # the 6.9 commit (ea3a015). Empty until now was correct — every file this
    # page lists, 6.9 created — and that stopped being true the moment another
    # lesson could edit one.
    "engine/include/engine/gfx/cascade.hpp": "scratch/l69_cascade.hpp",
    "engine/src/gfx/cascade.cpp":            "scratch/l69_cascade.cpp",
    "scratch/verify_69.cpp":                 "scratch/l69_verify_69.cpp",
}

# NOTHING PINNED YET, and this page lists EIGHT files whole.
#
# Lesson 6.9 is cascaded shadow maps, which splits one map into several. That
# touches `shadow.hpp` and `shadow.cpp` for certain (a cascade is a second fit,
# a second map and a per-cascade bias), `gpu_shadow.{hpp,cpp}` for certain (an
# array texture, and one pass per cascade) and `scene.frag.hlsl` for certain
# (choosing a cascade per fragment, and blending the seam). `bounds.hpp` is
# likely too: fitting a cascade to a slice of the CAMERA frustum needs that
# frustum's corners, which is an AABB question.
#
# So pin all of them before writing a line of 6.9:
#
#     git show <6.8 commit>:<path> > scratch/l68_<name>
#     git show <6.8 commit>:<path> | diff - scratch/l68_<name>     # must be empty
#
# And take `scratch/verify_68.cpp` EARLY — gitignored, so its only provenance is
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
<title>6.9 — Cascaded Shadow Maps · Build a Professional 3D Game Engine</title>
<meta name="description" content="Lesson 6.8 fitted one orthographic box around the whole scene, so the near field - the part you are looking at - gets whatever texels are left over. The fix is to split the camera frustum by distance and fit a box around each slice. What makes it a lesson is that a camera-fitted box moves, and 6.8's never did: this derives the practical split scheme from the two schemes it blends, shows why the box is fitted to a sphere (a corner-fitted box changes size by 30.2 per cent under yaw, which resizes every texel in the map) and what that costs (29 per cent of the resolution), derives texel snapping and measures it at 7.6e-6 texels of drift against 0.499 unsnapped, and audits 6.8's bias by changing the quantity it was derived from - finding that nothing had to move, and that the device-space bias barely moves either because the sphere radius cancels. Ends by measuring a case where cascades lose. Ported to both renderers; the reference render stays byte-identical.">

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
    <a class="prev-l" href="06-08-shadow-mapping.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.8 — Shadow Mapping: Bias, Acne, and PCF</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-10-mipmaps.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.10 — Mipmaps, LOD, and Anisotropic Filtering</span>
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
        with open(f"scratch/l69_body_{name}.html") as fh:
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
