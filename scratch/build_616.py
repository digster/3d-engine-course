#!/usr/bin/env python3
"""Assemble docs/lessons/06-16-frustum-culling.html.

Same pipeline as build_615.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l616_body_{a,b,c}.html, figures from scratch/figs_616.py, and
every code listing read from a PINNED COPY once 6.17's session pins them — see
LISTING_SOURCE, which is deliberately EMPTY today for the reason 6.6 learned.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-16-frustum-culling.html"

FIGURES = {
    "FIG_PROBLEM": ("l616_fig1.svg", "1",
        "Two places an off-screen object can be thrown away, and everything between them is paid "
        "for. The GPU rejects it at the CLIPPER, which is downstream of the vertex shader &#8212; "
        "so every vertex has already been transformed, had its normal matrix applied and its "
        "tangent rotated, for geometry that contributes no fragment. Frustum culling rejects it "
        "before submission, for six dot products. The bars below are measured (verify_616 &#167;H) "
        "and drawn on a LOG scale, because on a linear one the green bar would be invisible "
        "&#8212; which is the opposite of the point: the ratio is <strong>409&times;</strong>, so "
        "culling pays as soon as it rejects one object in 409."),

    "FIG_PLANES": ("l616_fig2.svg", "2",
        "The six inequalities that define the clip volume, and the six row combinations that "
        "answer them. Each inequality, rearranged to <em>something &#8805; 0</em>, IS a plane in "
        "<code>(a, b, c, d)</code> form &#8212; because <code>x</code> is row 0 dotted with the "
        "world position and <code>w</code> is row 3, so <code>x + w</code> is "
        "<code>(row0 + row3) &#183; p</code>. Note that left and right both carry <em>+row3</em>: "
        "they are not negatives of one another, which is exactly what makes them two walls meeting "
        "at the apex. THE NEAR PLANE IS ROW 2 ALONE under SDL_GPU&#8217;s "
        "<code>0 &#8804; z &#8804; w</code> range; OpenGL&#8217;s would be "
        "<code>row2 + row3</code>, and copying that line loses everything within a far distance of "
        "the camera. It is also the only plane built without a subtraction, which is why it is "
        "<strong>4557&times;</strong> more accurate than the far plane."),

    "FIG_WORKED": ("l616_fig3.svg", "3",
        "A frustum with round numbers, so the algebra can be checked by hand: 90&#176; vertical, "
        "square aspect, near 1, far 11, camera at the origin looking down &#8722;z. At 90&#176; the "
        "focal length is exactly 1, so the walls sit at 45&#176; and <code>x = &#8722;z</code> on "
        "the left one. The two marked points are the check: <code>(&#8722;5, 0, &#8722;5)</code> "
        "lies ON the plane (distance 0) and <code>(&#8722;4.5, 0, &#8722;5)</code> is "
        "<strong>0.353553</strong> inside &#8212; which is <code>0.5/&#8730;2</code>, half a unit "
        "of offset measured perpendicular to a wall tilted at 45&#176;. Every number is checked in "
        "verify_616 &#167;C."),

    "FIG_CORNER": ("l616_fig4.svg", "4",
        "One corner per plane, chosen by the sign of the normal. <code>n &#183; c</code> is a sum "
        "of three independent terms, so it is maximised by taking the extreme of each axis whose "
        "sign matches the normal&#8217;s &#8212; three comparisons, no search, no loop over eight "
        "corners. If that corner is outside, all eight are. Testing the OPPOSITE corner too gives a "
        "third answer, <code>inside</code>, which is what lets a hierarchy accept a whole subtree "
        "in one test. We have no hierarchy, and the counter is there anyway: "
        "<strong>25,859 of 45,512</strong> kept boxes were wholly inside, which is how you find out "
        "whether a hierarchy would pay before building one."),

    "FIG_FALSE": ("l616_fig5.svg", "5",
        "The corner case, and its measured size. The six tests each ask &#8220;is this box wholly "
        "outside THIS plane?&#8221; independently, so a large box near a corner of the frustum can "
        "poke past each plane with a DIFFERENT corner and be rejected by none &#8212; while never "
        "entering the volume where all six conditions hold at once. Measured over 200,000 random "
        "boxes: <strong>316 false positives, 0.69% of those kept</strong>. And the number that "
        "actually matters is the last one: <strong>zero</strong> visible boxes were rejected. "
        "Keeping something invisible costs a draw call; rejecting something visible is a hole in "
        "the picture, and only one of those is allowed."),

    "FIG_CROSSOVER": ("l616_fig6.svg", "6",
        "Where culling starts to pay &#8212; and the axis matters. A first attempt swept the "
        "OBJECT COUNT and found &#8220;cull wins&#8221; six times, because both the test and the "
        "work it skips are O(n) and their ratio cannot depend on n. The crossover is in the CULL "
        "RATE. Culling costs <code>c</code> per object always and saves <code>w</code> per object "
        "rejected, so it breaks even at <code>c/w</code> = 58.3 / 23,821 = "
        "<strong>0.2446%</strong>, one object in 409. Predicted first, then found: at 0% culled it "
        "loses by 53.7 ns an object &#8212; which is exactly what the test costs &#8212; and the "
        "crossing sits between the 0% and 0.4% samples. TWO THINGS TO READ HONESTLY: the x axis is the six SAMPLES, evenly spaced, not a linear cull-rate scale &#8212; so the green curve&#8217;s shape is not a gradient; and the red &#8220;no culling&#8221; line drifts down slightly because moving objects behind the camera genuinely gives <code>collect_triangles</code> less to do. The comparison that matters is vertical, within each sample."),

    "FIG_BATCH": ("l616_fig7.svg", "7",
        "What every instance in one draw must agree about, and what batching actually saves. The "
        "key is five things and not one, because a single draw has one pipeline, one set of bound "
        "textures and one set of pushed uniforms &#8212; so the 32-byte material is part of it, and "
        "two objects rarely match on it by accident. That is why this engine&#8217;s own demo scene "
        "makes <strong>12 batches from 12 objects</strong>: instancing is a content decision before "
        "it is a renderer one. On the right, sixteen identical cubes: the 112-byte matrix block did "
        "not shrink, it changed road, and the entire 720-byte saving is fifteen MATERIALS not "
        "pushed. <strong>Instancing saves submission, not bandwidth.</strong>"),
}

LISTING_META = {
    "engine/include/engine/gfx/frustum.hpp": ("new", "new"),
    "engine/src/gfx/frustum.cpp": ("new", "new"),
    "engine/include/engine/gfx/instancing.hpp": ("new", "new"),
    "engine/src/gfx/instancing.cpp": ("new", "new"),
    "shaders/scene_instanced.vert.hlsl": ("new", "new"),
    "scratch/verify_616.cpp": ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/bounds.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_pipeline.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_scene.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_pipeline.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_scene.cpp": ("modified", "modified"),
    "CMakeLists.txt": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
}

LISTING_LANG = {
    "shaders/scene_instanced.vert.hlsl": ("hlsl", "HLSL"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "CMakeLists.txt": ("cmake", "CMake"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
}

LISTING_SOURCE = {
    # PINNED at the start of 6.17's session, per the note below. All six verified
    # by substring against the shipped page before pasting.
    "engine/include/engine/gfx/frustum.hpp":    "scratch/l616_engine_include_engine_gfx_frustum.hpp",
    "engine/src/gfx/frustum.cpp":               "scratch/l616_engine_src_gfx_frustum.cpp",
    "engine/include/engine/gfx/instancing.hpp": "scratch/l616_engine_include_engine_gfx_instancing.hpp",
    "engine/src/gfx/instancing.cpp":            "scratch/l616_engine_src_gfx_instancing.cpp",
    "shaders/scene_instanced.vert.hlsl":        "scratch/l616_shaders_scene_instanced.vert.hlsl",
    "scratch/verify_616.cpp":                   "scratch/l616_scratch_verify_616.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/bounds.hpp": "scratch/l616_engine_include_engine_gfx_bounds.hpp",
    "engine/include/engine/gfx/gpu_pipeline.hpp": "scratch/l616_engine_include_engine_gfx_gpu_pipeline.hpp",
    "engine/include/engine/gfx/gpu_scene.hpp": "scratch/l616_engine_include_engine_gfx_gpu_scene.hpp",
    "engine/src/gfx/gpu_pipeline.cpp": "scratch/l616_engine_src_gfx_gpu_pipeline.cpp",
    "engine/src/gfx/gpu_scene.cpp": "scratch/l616_engine_src_gfx_gpu_scene.cpp",
    "CMakeLists.txt": "scratch/l616_CMakeLists.txt",
    "engine/CMakeLists.txt": "scratch/l616_engine_CMakeLists.txt",
}

# NOTHING PINNED YET, and this page lists SIX files whole.
#
# Lesson 6.17 is A FRAME GRAPH. On the face of it nothing here is at risk: a frame
# graph reorganises PASSES and their targets, and none of `frustum.*`,
# `instancing.*` or `scene_instanced.vert.hlsl` is a pass.
#
# THREE REASONS TO PIN ANYWAY.
#   1 `verify_616.cpp` is GITIGNORED, so its provenance cannot be recovered from
#     history later — only from this page. A working-tree copy taken NOW is the
#     only cheap moment.
#   2 "Nothing here is at risk" is exactly what was said about `gpu_post.hpp`
#     before 6.14 rewrote it, and about `cubemap.*` before this lesson (which,
#     correctly, did not touch them — the control held).
#   3 A frame graph OWNS the render passes, and `render_batched` is a new path
#     through the scene pass. 6.17 is more likely than usual to reach in here.
#
# So, before writing a line of 6.17:
#
#   for f in engine/include/engine/gfx/frustum.hpp \
#            engine/src/gfx/frustum.cpp \
#            engine/include/engine/gfx/instancing.hpp \
#            engine/src/gfx/instancing.cpp \
#            shaders/scene_instanced.vert.hlsl; do
#     git show <6.16 commit>:$f > scratch/l616_$(echo $f | tr / _)
#   done
#   cp scratch/verify_616.cpp scratch/l616_scratch_verify_616.cpp   # gitignored
#
# then paste the dict `scratch/pin_listings.py 616 --dry` prints, re-run this
# file, and `git diff` the page: SIXTEEN lessons running, the diff has been
# exactly the nav lines meant to move.


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
<title>6.16 — Frustum Culling and Instanced Submission · Build a Professional 3D Game Engine</title>
<meta name="description" content="Every renderer in this engine submits every object every frame, and the GPU does eventually reject what is off screen - but it rejects it in the CLIPPER, downstream of the vertex shader, so a prop two kilometres behind the camera costs a full vertex dispatch, a pipeline bind, two uniform pushes and a draw call before anything notices. This lesson stops paying for that with six dot products. The interesting part is where the planes come from: they fall out of the ROWS of the view-projection matrix by one observation - a clip-space coordinate is already a signed distance - with no trigonometry and, crucially, no second copy of the truth to drift from the matrix the renderer actually uses. The near plane is the one that is a convention: SDL_GPU clips 0 &lt;= z &lt;= w, so it is row 2 ALONE where every OpenGL-derived article writes row2 + row3. The extraction is verified against facts that need no reference implementation - the eye is the apex, so its distance to all four side planes is exactly zero - and then against the fact that four zeros are NOT enough, because a reversed normal passes all four. An honest digression traces the far plane being 1.4e-3 out of place, rules out cancellation in the extraction by redoing it in double, and finds it in perspective() itself, where forming A + 1 with A = -1.003009 amplifies one ulp by 332x. Then the box test - one corner per plane, not eight - and both directions in which it is conservative, including the false-positive corner case measured at 0.69 per cent of kept boxes against zero false rejects, which is the asymmetry that licenses the whole technique. The measurement that matters is not the cull rate: a sweep over object count found nothing because both sides are O(n), and the crossover is in the CULL RATE, predicted at c/w = 0.2446 per cent and then bracketed. Finally the survivors become an instance buffer: a batch key of five things and not one, twelve objects that make twelve batches because instancing is a content decision, 112 bytes that change road rather than shrink, sixteen draw calls collapsed into one, and a bit-identical frame across 76,800 channels.">

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
    <a class="prev-l" href="06-15-skybox-ibl.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.15 — Skybox and Image-Based Lighting</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-17-frame-graph.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.17 — A Lightweight Frame Graph</span>
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
        with open(f"scratch/l616_body_{name}.html") as fh:
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
