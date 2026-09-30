#!/usr/bin/env python3
"""Assemble docs/lessons/07-07-sampling-blending.html.

Same pipeline as build_71.py through build_76.py. No STATE block: STATE.md is
the sole resume key and lesson pages end at Further Reading (CLAUDE.md §9).

Prose from scratch/l77_body_{a,b,c,d,e}.html, figures from scratch/figs_77.py,
and every code listing PINNED at writing time — see LISTING_SOURCE.

WHY THE PINS MATTER PARTICULARLY HERE. `demos/rig/main.cpp` is listed WHOLE and
this lesson is the second to edit it; `math/transform.hpp` is the most-edited file
in the repository; and `anim/clip.hpp` is nine days old and Module 8 will want a
clip sampler for ragdolls. A builder that opened a repository path would render
this page's listings as they stand TODAY rather than as they stood when the prose
was written about them.

WHEN COPYING THIS FILE, EMPTY LISTING_SOURCE AS WELL AS FIGURES — build_66.py
inherited 6.5's three pins verbatim and they sat inert for a whole lesson,
making the discipline LOOK satisfied. Here the pins are generated from
LISTING_META by `_pin`, so the failure mode is the opposite one: a path added to
LISTING_META without a pin file beside it fails loudly at build time, which is
the right direction for it to fail in.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/07-07-sampling-blending.html"

FIGURES = {
    "FIG_TRACKS": ("l77_fig1.svg", "1",
        "<strong>A clip is not a sequence of poses; it is a bundle of independent channels, and "
        "the reason is that most of them are empty.</strong> On the left, the obvious layout: every "
        "joint sampled at every frame, 713 poses and 28,520 bytes, which cannot express "
        "<em>this joint never moves</em> and cannot sample a fast limb more densely than a still "
        "one. On the right, the same second of motion as per-channel tracks after reduction. "
        "<strong>One position channel in the entire rig</strong> — the root, which is what carries "
        "the character across the ground — and <strong>no scale channels at all</strong>. Six "
        "joints (head, clavicles, hands, toes) are empty on all three, and an empty channel is not "
        "an approximation: the sampler falls back to the bind value, so deleting it reproduces the "
        "pose bit for bit. 16 of 69 channels hold 100% of the motion, which is a fact about how "
        "limbs work rather than about this fixture — a limb pivots, it does not slide."),

    "FIG_BRACKET": ("l77_fig2.svg", "2",
        "<strong>The whole of sampling, and the worked numbers.</strong> Six keys of a 30 Hz track "
        "and a sample at <code>t = 0.1800</code>, which falls in the third interval; the normalised "
        "parameter is <code>(0.1800 − 0.1333) / (0.2000 − 0.1333) = 0.7000</code>, seven tenths of "
        "the way across. Below, the same lookup done two ways — and <strong>at six keys they do the "
        "same work</strong>, which is the honest version of this comparison and is exactly the "
        "point: both touch <code>k2</code> and <code>k3</code>, but the search does "
        "<code>log₂(n)</code> of that and the cursor does two for any <code>n</code>. The table is "
        "what that costs, with the driver advancing exactly one key interval per lookup so the two "
        "rows differ in nothing but length. <strong>The cursor wins 2.74× at 31 keys and 10.34× at "
        "301</strong>, and the middle column is this lesson's change: a backward jump falls into "
        "the search rather than walking, which is worth 14% and 43% and bounds the wrap at "
        "<code>log₂(n)</code>."),

    "FIG_SCALE": ("l77_fig4.svg", "4",
        "<strong>The decision Lesson 7.5 left open, settled with a picture.</strong> On the left, a "
        "growth from 1 to 8 done both ways: the lerp passes through <strong>4.500</strong> at the "
        "midpoint and the geometric rule through <strong>2.828</strong>, and only the second of "
        "those looks like steady growth — scale composes by multiplication and perceived size is "
        "closer to logarithmic than linear. On the right, the gap between them, which is exactly "
        "the arithmetic-minus-geometric mean of the ratio. <strong>Over the range content actually "
        "uses it is at most 0.62%</strong>; a full doubling is 6.07%; the 59.10% of the 1-to-8 "
        "example needs a ratio no rig animates. Three things settle it for the lerp anyway, and the "
        "last two are not close: a scale of <em>zero</em> is how an animator hides a thing and "
        "<code>ln(0)</code> is <code>−∞</code>, so the geometric rule teleports rather than shrinks; "
        "a mirrored rig carries a negative scale and <code>ln(−1)</code> is NaN; and glTF defines "
        "<code>LINEAR</code> as componentwise lerp, so an importer that log-lerps plays the file "
        "differently from every other viewer. The answer to somebody who genuinely wants geometric "
        "growth is a keyframe, not a runtime rule: six of them cut the 1-to-8 gap to 1.51%."),

    "FIG_ARCS": ("l77_fig3.svg", "3",
        "<strong>Why nlerp is free inside a clip and a judgement call between two.</strong> Log "
        "axes, and every angle is a <em>rotation</em> angle — the sphere arc a quaternion walks is "
        "half of each number, which is the one thing that must be said every time. The teal points "
        "are two adjacent keyframes of a 30 Hz clip at four limb speeds: even at "
        "<strong>720°/s</strong>, two full turns a second and about as fast as a limb moves, "
        "adjacent keys are <strong>24° apart and nlerp lags slerp by 0.0169°</strong> — three "
        "orders inside Lesson 7.5's half-degree threshold, marked. The rose points are arcs a "
        "cross-fade can reach, where the same function is not fine at all: 4.514° at 150°. Same "
        "code, same measurement, opposite verdicts, and the only thing separating them is how far "
        "apart the two orientations are. That is what <code>pose_blend_report::worst_arc</code> "
        "exists to tell you."),

    "FIG_WRAP": ("l77_fig5.svg", "5",
        "<strong>Two ways a playback clock goes wrong, and neither is subtle once measured.</strong> "
        "On the left, the frame that is missing: a baker that writes <code>frames</code> keys "
        "instead of <code>frames + 1</code> puts its last key one frame short of the stated "
        "duration, and the cycle restarts early — <strong>5.8215° of seam</strong>, once per loop, "
        "forever, on an animation that is correct at every instant except one. Note what is "
        "<em>not</em> a seam: the root travels <strong>1.2000 units</strong> over the cycle, which "
        "is the walk working rather than failing, and the first version of this instrument reported "
        "it as a defect on every correct clip. On the right, a negative time — and the moral is the "
        "opposite of the obvious one. Clamped, the pose freezes on the first key: loud, and bounded "
        "by the clip's own range. Unclamped, it extrapolates: <strong>0.015° at a thirtieth of a "
        "second</strong>, invisible, and <strong>100° at six tenths</strong>, with no bound at all. "
        "The quiet failure is the worse one."),

    "FIG_BLEND_RENDER": ("l77_fig6.svg", "6",
        "<strong>The same cross-fade, one step apart in the pipeline.</strong> Two real renders, "
        "identical inputs, identical weight of 0.5 between the same two clips. On the left the two "
        "<em>poses</em> are blended and then composed; on the right they are composed and then the "
        "<em>matrices</em> are blended — sixteen independent lerps, which is what you reach for if "
        "the thing in your hand is a pair of <code>model_from_joint</code> arrays. The right-hand "
        "limb is <strong>0.3498 units shorter</strong> and visibly thinner toward its tip, and both "
        "are the same cause: a matrix lerp averages the joint <em>positions</em>, so each bone "
        "becomes the average of two unit vectors an angle apart and shortens by the cosine of half "
        "it. That is Lesson 7.6's candy wrapper exactly, with the <em>accumulated</em> whole-chain "
        "angle in place of the per-joint one — which is why the root is untouched and the tip is "
        "ruined, and why this bug is worst at hands and feet."),

    "FIG_BLEND_CURVE": ("l77_fig7.svg", "7",
        "<strong>A prediction with no free parameters, and what it is worth.</strong> Bone "
        "<code>k</code> runs from joint <code>k−1</code> to joint <code>k</code>, so its direction "
        "is joint <code>k−1</code>'s own axis; two poses differing by <code>δ</code> at every joint "
        "differ by <code>(k−1)·δ</code> there, and a matrix blend averages the two joint positions "
        "— so the chain becomes <code>Σ cos((k−1)δ/2)</code>, the dashed grey curve. The measured "
        "matrix blend lands on it to <strong>4.768e−07</strong>. The pose blend, meanwhile, holds "
        "at <strong>exactly 5.00000</strong> at every angle, with the worst individual bone out by "
        "2.980e−07 — because <code>quat_nlerp</code> normalises, so a blended rotation is still a "
        "rotation and a blended <code>transform</code> is still a placement. The gap between the "
        "two curves is 0.4434 units at 20° per joint, which is an entirely ordinary difference "
        "between a walk pose and a turn."),

    "FIG_SIGNS": ("l77_fig8.svg", "8",
        "<strong>The double cover, arriving from outside for the first time.</strong> An exporter "
        "computes each keyframe's orientation independently and has no obligation to keep the signs "
        "consistent, so two keys one degree apart are perfectly likely to be stored as <code>q</code> "
        "and <code>−q′</code>. Both are correct; both name the same orientation; they are not the "
        "same <em>point</em>, and an interpolation walks between points. The plot is the cost, "
        "measured across one flipped interval on the fixture's left thigh: <strong>180° inside a "
        "thirtieth of a second</strong>, which reads as a corrupt file rather than as a missing dot "
        "product. <strong>This engine is immune</strong> — <code>quat_nlerp</code> calls "
        "<code>nearest</code>, and the flipped and clean clips sample bit-identically — so the "
        "value of the load-time pass is not cycles. It is the count (690 flips is a fact about your "
        "content pipeline), and it is every consumer that is not this sampler: a vertex shader, a "
        "re-exporter, a quantiser, a colleague's reimplementation."),

    "FIG_REDUCE": ("l77_fig10.svg", "10",
        "<strong>Greedy fit-and-split, on one real channel.</strong> Thirty-one keys as baked, "
        "nineteen surviving a half-degree tolerance and eight surviving two degrees — and the "
        "survivors <em>bunch where the curve turns</em>, because that is where a straight line "
        "between neighbours misses it, which is the algorithm visible in its output. The table's "
        "first row is the floor and it is not zero: the error column is measured against the "
        "closed form the keys were baked from, so <strong>0.1534° is what sampling at 30 Hz costs "
        "before a single key is thrown away</strong>, which is why the 0.05° row cannot do better "
        "than the 0.00° one. The last column is the warning: <code>std::vector</code> headers are a "
        "fixed 72 bytes per joint whether the channels hold ten keys or none, so the better the "
        "reduction works the more of the clip is container — <strong>0.04× at the top and 1.46× at "
        "the bottom</strong>. That is an argument for a flat arena, which is Module 9 arriving "
        "early and uninvited."),

    "FIG_COST": ("l77_fig9.svg", "9",
        "<strong>The number the storage argument did not predict.</strong> On the left, four "
        "layouts of the same second of motion — and a naive per-channel bake is <strong>30% BIGGER "
        "than the pose array</strong> it was supposed to beat, because a pose spends 40 bytes per "
        "joint-frame and three channels spend 52, every key carrying its own four-byte time. The "
        "layout is not the saving; the layout is what makes the saving possible, because "
        "independent times are what let a channel be reduced or deleted without dragging its "
        "neighbours. Reduced, it is <strong>7.7× smaller</strong>, and at production density 3.60 MB "
        "becomes 0.48 MB. On the right, what reading it costs: sampling 23 joints is 0.159 µs, two "
        "thirds of composing the pose and building the palette and a rounding error beside 7.6's "
        "44.72 µs of skinning. The cursor's margin over a binary search is 2.15× at 31 keys and "
        "5.63× at 301, which is <code>log₂(n)</code> against 1 showing up where it should."),
}

LISTING_META = {
    "engine/include/engine/anim/clip.hpp":      ("new", "new"),
    "engine/src/anim/clip.cpp":                 ("new", "new"),
    "engine/include/engine/math/transform.hpp": ("modified", "modified"),
    "engine/include/engine/math/quat.hpp":      ("modified", "modified"),
    "engine/include/engine/engine.hpp":         ("modified", "modified"),
    "engine/CMakeLists.txt":                    ("modified", "modified"),
    "demos/rig/main.cpp":                       ("modified", "modified"),
    "scratch/verify_77.cpp":                    ("new", "new"),
    "scratch/build_verify_77.sh":               ("new", "new"),
    "scratch/golden_77.cpp":                    ("new", "new"),
}

LISTING_LANG = {
    "scratch/build_verify_77.sh": ("bash", "shell"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
}


def _pin(path):
    return "scratch/l77_" + path.replace("/", "_")


LISTING_SOURCE = {path: _pin(path) for path in LISTING_META}


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
<title>7.7 — Sampling and Blending Animations · Build a Professional 3D Game Engine</title>
<meta name="description" content="Lesson 7.6 ended with a character that can be posed and a demo that posed it from two sliders. A clip replaces the sliders, and the whole of the replacement is a function from time to pose - which sounds like an afternoon's work and is not, because a clip does not store poses. Storing them costs 3.6 MB for thirty seconds of one character. This lesson builds the per-channel keyframe format every engine and every interchange format uses, and finds the number the storage argument does not predict: a naive per-channel bake is 30 percent BIGGER than a pose array, because every key carries its own time - the layout is not the saving, it is what makes the saving possible, and after elision and reduction the same second of motion is 7.7 times smaller with 53 of its 69 channels gone entirely. Sampling is a bracket search that playback's coherence lets you skip, with a cursor whose worst case is the loop wrap; three interpolation rules, one per field, including the scale decision Lesson 7.5 left open, settled by the arithmetic-minus-geometric mean gap, by the fact that ln(0) is minus infinity and a scale of zero is how an animator hides a thing, and by glTF's LINEAR mode; correct looping, where the duration is authored rather than derived and a clip one frame short ticks by 5.82 degrees once per cycle while a walk's root legitimately ends 1.2 units downrange; and the cross-fade, which is NOT the weighted sum Lesson 7.6 built - blend the poses and every bone keeps its length exactly, blend the composed matrices instead and the limb shortens by the sum of cos(k delta over 2), which is the candy wrapper with the accumulated whole-chain angle in place of the per-joint one, worst at hands and feet.">

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
    <a class="prev-l" href="07-06-skeletal-animation.html">
      <span class="dir">← Previous</span>
      <span class="ttl">7.6 — Skeletal Animation: The Skinning Math</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="07-07b-gltf-characters.html">
      <span class="dir">Next →</span>
      <span class="ttl">7.7b — Animated Characters from glTF: Skins and Clips</span>
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
    for name in ("a", "b", "c", "d", "e"):
        with open(f"scratch/l77_body_{name}.html") as fh:
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
    # BYTES, not characters — build_74.py's note applies. The page is UTF-8 and
    # full of × − ° ₁, so `len(page)` is several thousand short of `wc -c`.
    print(f"wrote {OUT}  ({len(page.encode('utf-8')):,} bytes, "
          f"{page.count(chr(10)):,} lines)")


if __name__ == "__main__":
    main()
