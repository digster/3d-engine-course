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

OUT = "docs/lessons/06-08-shadow-mapping.html"

FIGURES = {
    "FIG_PROBLEM": ("l68_fig1.svg", "1",
        "Why nothing in this engine has ever cast a shadow, and it is one line of arithmetic. "
        "<code>lambert(n, l)</code> asks whether a surface FACES the light &#8212; a fact about "
        "one surface&#8217;s orientation &#8212; where the question we want is whether it can SEE "
        "the light, which is a fact about that surface and every other object in the scene. "
        "Points A and B have the same normal and the same light, so the shading equation cannot "
        "tell them apart, and <code>shade()</code>&#8217;s five arguments contain no way to "
        "discover that a caster is in the way. That is why every object in every picture so far "
        "floats: the dark patch where a thing touches another thing is the strongest spatial cue "
        "the eye has, and we have never drawn one."),

    "FIG_IDEA": ("l68_fig2.svg", "2",
        "The whole technique, and the engine already owns both halves. Render the scene from the "
        "light and keep only the depth: what you get is the distance to the nearest surface along "
        "every ray the light emits, which IS the question &#8220;what can the light see?&#8221; "
        "&#8212; and &#8220;nearest surface along a ray from a viewpoint&#8221; is not a new "
        "algorithm, it is a <strong>z-buffer</strong>, which Lesson 3.1 built and 4.7 ported. On "
        "the CPU the first pass is <code>collect_triangles</code> followed by "
        "<code>draw_triangles</code>, unmodified, with a different camera. <strong>A shadow map is "
        "not a new renderer; it is the renderer you already have, aimed somewhere else.</strong> "
        "Everything difficult is in the second pass&#8217;s comparison."),

    "FIG_ORTHO": ("l68_fig3.svg", "3",
        "A directional light has no position, so it gets a box rather than a pyramid. A "
        "perspective frustum is a pyramid because rays converge on an eye; parallel rays sweep out "
        "a box, and a box maps onto the clip cube by scale-and-offset alone. Read the difference "
        "in the bottom row: <code>perspective</code>&#8217;s is (0,&nbsp;0,&nbsp;&minus;1,&nbsp;0), "
        "which copies &minus;z into w and makes the divide happen, while "
        "<code>orthographic</code>&#8217;s is (0,&nbsp;0,&nbsp;0,&nbsp;1), so <strong>w comes out "
        "exactly 1</strong> and the divide is the identity. Three things follow, and the lesson "
        "spends all three: depth becomes affine so precision is uniform (&#167;3.3), the near "
        "plane may be NEGATIVE so the light&#8217;s eye can sit inside the scene (&#167;3.4), and "
        "a fragment&#8217;s light-space position can be recovered from its interpolated world "
        "position for free (&#167;8.5)."),

    "FIG_ARTEFACT": ("l68_fig4.svg", "4",
        "What a <em>correct</em> first implementation puts on screen, and this is Lesson "
        "3.5&#8217;s rule earning its place: the shadow under the torus is right, and everything "
        "else is <strong>shadow acne</strong> &#8212; 36,786 pixels of a 480&#215;270 frame. Not "
        "one line of code misbehaved. The fringes curve and bunch toward the horizon because they "
        "are the level sets of the light-space texel coordinate, which on a ground plane is a "
        "PROJECTIVE function of screen position; that is why real acne swirls rather than striping "
        "evenly, and the curves here are drawn from that formula rather than sketched. An SVG "
        "mock; the real frame is a PPM from "
        "<code>gltf_view --shadow 1024 --bias none</code>."),

    "FIG_ACNE": ("l68_fig5.svg", "5",
        "The derivation, and it turns a mystery into two multiplications. The map stores ONE depth "
        "per texel, sampled at one point; the fragment being tested is somewhere else inside that "
        "texel, and on a tilted surface two different points have two different depths. "
        "<strong>Half of every texel&#8217;s footprint is downhill of its own sample</strong>, "
        "downhill means further from the light, and further means &#8220;something is in front of "
        "me&#8221; &#8212; so with no bias, half of every lit surface shadows itself. Measured on "
        "an unoccluded plane: <strong>50.1%</strong>. The magnitude follows from the same picture: "
        "lateral travel is at most the reach times <code>world_per_texel</code>, and walking that "
        "far across a surface of slope tan&thinsp;&theta; changes its depth by the product. "
        "Measured worst error 1.931e&minus;03 against a derived bound of 2.101e&minus;03 &#8212; "
        "<strong>92% of the bound</strong>, which is what makes it a derivation rather than a "
        "safe over-estimate."),

    "FIG_CURES": ("l68_fig6.svg", "6",
        "Three ways to make the comparison pass, and three prices. A <strong>constant</strong> "
        "cancels an error proportional to tan&thinsp;&theta; with a number that is not, so it must "
        "be sized for the steepest surface present and every flatter one is then lifted too far "
        "&#8212; measured at 3.1&#215; too small at 75&deg; when sized for 50&deg;. That surplus "
        "is <strong>peter-panning</strong>, and it is exact: a bias of b unshadows everything "
        "whose caster is within <code>b &#215; depth_range</code> world units, which bites hardest "
        "exactly where a caster TOUCHES its receiver. <strong>Slope-scaled</strong> gives each "
        "surface what its own geometry requires and nothing more &#8212; 50.1% acne to 0.0% on the "
        "same plane. <strong>Normal-offset</strong> moves the sample POINT instead of its depth, "
        "and needs the GEOMETRIC normal: acne is a disagreement about where the triangles are, and "
        "Lesson 6.7 has just given every surface a shading normal that disagrees with them. It "
        "converts the error rather than removing it &#8212; unbounded depth error for lateral "
        "error bounded by a texel &#8212; and the lesson says so."),

    "FIG_PCF": ("l68_fig7.svg", "7",
        "Why the comparison must come before the filter, in four numbers. Two stored occluders at "
        "0.3 and 0.9 and a receiver at 0.5: average the DEPTHS and you get 0.6, which is greater "
        "than 0.5, so the receiver is reported <strong>fully lit</strong> with half its taps "
        "occluded &#8212; not a blurrier answer, a wrong one, and it stays wrong however many taps "
        "you add. Compare first and the two taps give 0 and 1, whose mean is <strong>0.5</strong>. "
        "The rule underneath generalises far past shadows: <strong>the values you average must be "
        "the values you want the average of</strong>, and a depth is not a visibility. That is the "
        "entire reason <code>SamplerComparisonState</code> exists, and why the comparison operator "
        "lives in the sampler where it is free. One caveat the lesson found by rendering: a "
        "3&#215;3 kernel&#8217;s furthest tap is 2.12 texel-diagonals away rather than 0.71, so a "
        "bias sized for one tap leaves two thirds of the error uncovered and the acne returns at "
        "the exact moment PCF is switched on."),
}

LISTING_META = {
    "engine/include/engine/gfx/bounds.hpp":     ("new", "new"),
    "engine/include/engine/gfx/shadow.hpp":     ("new", "new"),
    "engine/src/gfx/shadow.cpp":                ("new", "new"),
    "engine/include/engine/gfx/gpu_shadow.hpp": ("new", "new"),
    "engine/src/gfx/gpu_shadow.cpp":            ("new", "new"),
    "shaders/shadow.vert.hlsl":                 ("new", "new"),
    "shaders/scene.frag.hlsl":                  ("modified", "modified"),
    "scratch/verify_68.cpp":                    ("new", "new"),
}

LISTING_LANG = {
    "shaders/shadow.vert.hlsl": ("hlsl", "HLSL"),
    "shaders/scene.frag.hlsl":  ("hlsl", "HLSL"),
}

LISTING_SOURCE = {
    # PINNED at the start of the 6.9 session, before a line of cascades was
    # written, and verified byte-identical against the 6.8 commit (23f5a99).
    # 6.9 edits SEVEN of these eight files, so without the pins a rebuild of
    # this page — even just to repoint a nav link — would splice 6.9's code
    # into 6.8's listings. verify_68.cpp is gitignored, so its pin is a
    # working-tree copy taken before any edit.
    "engine/include/engine/gfx/bounds.hpp":      "scratch/l68_bounds.hpp",
    "engine/include/engine/gfx/shadow.hpp":      "scratch/l68_shadow.hpp",
    "engine/src/gfx/shadow.cpp":                 "scratch/l68_shadow.cpp",
    "engine/include/engine/gfx/gpu_shadow.hpp":  "scratch/l68_gpu_shadow.hpp",
    "engine/src/gfx/gpu_shadow.cpp":             "scratch/l68_gpu_shadow.cpp",
    "shaders/shadow.vert.hlsl":                  "scratch/l68_shadow.vert.hlsl",
    "shaders/scene.frag.hlsl":                   "scratch/l68_scene.frag.hlsl",
    "scratch/verify_68.cpp":                     "scratch/l68_verify_68.cpp",
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
<title>6.8 — Shadow Mapping: Bias, Acne, and PCF · Build a Professional 3D Game Engine</title>
<meta name="description" content="Every light in this engine is unoccluded: n dot l asks whether a surface faces the light, not whether it can see it, so nothing has ever cast a shadow. The fix is a z-buffer rendered from the light, which the engine has owned since Lesson 3.1. What is hard is the comparison, and this lesson derives it rather than tuning it: the map stores one depth per texel and the fragment is somewhere else inside that texel, so half of every lit surface shadows itself - measured at 50.1 per cent - and the error is bounded by the lateral travel times the surface slope, measured to within 8 per cent of that bound. Derives orthographic projection from an interval remap and shows what its w of exactly 1 buys; derives slope-scaled bias and quantifies why a constant one must peter-pan; shows in four numbers why a shadow comparison must precede filtering, and therefore why comparison samplers exist. Ports to both renderers, including a GPU render pass with no colour attachment at all; the reference render stays byte-identical.">

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
    <a class="prev-l" href="06-07-normal-mapping.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.7 — Normal Mapping and the TBN Derivation</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-09-cascaded-shadows.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.9 — Cascaded Shadow Maps</span>
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
        with open(f"scratch/l68_body_{name}.html") as fh:
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
