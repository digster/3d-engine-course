#!/usr/bin/env python3
"""Assemble docs/lessons/07-07b-gltf-characters.html.

Same pipeline as build_618b.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l77b_body_{a,b,c,d}.html, figures from scratch/figs_77b.py, and
EVERY code listing read from a pin — because this is an INSERTED lesson (the
authoring guide's §17). The working tree already holds 7.8 and Module 8, so its
build lists are not what a student has at 7.7b. `scratch/make_t77b.py tree
scratch/_t77 scratch/_t77b --pins` writes each pin as the file after 7.7
(`scratch/replay_tree.py 7.7`) plus 7.7b's own edits, and proves every one by
delta; new files are the working tree's text. The harness, the build script, the
authoring script and the two Blender-side instruments are copied from the working
tree.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/07-07b-gltf-characters.html"

FIGURES = {
    "FIG_ORDERS": ("l77b_fig1.svg", "1",
        "Three orders of one skeleton. Blender writes its node array children first, so every joint's "
        "parent has a larger index (red); the skin's own joint list is already parent-first (green); "
        "the imported skeleton puts the <code>Armature</code> node, which no skin names and no vertex "
        "is weighted to, at joint 0 and keeps the skin's order after it, so every joint's parent comes "
        "before it."),

    "FIG_SPEC": ("l77b_fig2.svg", "2",
        "The specification's skin example, completed. <code>node_0</code> is not a joint and its "
        "translation is applied; <code>node_3</code> and <code>node_4</code>, which carry the mesh, are "
        "ignored. At rest the triangle is where the file put it; as <code>node_1</code> turns 90&#176; "
        "about <em>z</em>, v1 rides joint B round to (0,&nbsp;1.5). In red, the two wrong answers: "
        "(1,&nbsp;1) if the mesh node's transform were applied, (0,&nbsp;0) if the non-joint ancestor "
        "were dropped."),

    "FIG_ORDER_RENDER": ("l77b_fig3.svg", "3",
        "<code>mannequin --time 0.25</code>, imported correctly (left) and with its joints built in the "
        "node array's order (right). Nineteen of twenty joints come before their parents, so "
        "<code>compose_pose</code> composes each as a root at its own offset from the origin: not a "
        "lag, a pile of parts at the character's feet."),

    "FIG_INFLUENCES": ("l77b_fig4.svg", "4",
        "Top: one vertex of Blender's mannequin followed through three index spaces &#8212; "
        "<code>JOINTS_0</code> holds skin slots, a slot names a node, and the importer maps the node to "
        "its joint. Bottom: how many non-zero influences each of the 2,681 vertices kept. Nearly half "
        "follow one bone, and none needed more than four."),

    "FIG_XYZW": ("l77b_fig5.svg", "5",
        "Every rotation read w-first, which is what a blind copy of the file's x, y, z, w does. The root's "
        "identity becomes a half turn about <em>z</em>, so the character hangs upside down under the "
        "floor (<code>--xyzw --wide</code>); every other joint is folded by its own error. Right: the "
        "error of each of the file's 748 rotation keys &#8212; never less than 110.9&#176;, and not one "
        "fixed rotation, because the shift is an odd permutation."),

    "FIG_LAG": ("l77b_fig6.svg", "6",
        "Left: how far nlerp lags slerp across one interval, in degrees of rotation, for arcs of 60 to "
        "179&#176;. Every curve is zero at the midpoint and peaks near a fifth and four fifths of the way, "
        "which is why the importer probes at seven places and the midpoint is a control. Right: the "
        "pieces an interval needs to stay within 0.05&#176;, computed (line) and imported (dots)."),

    "FIG_STEP": ("l77b_fig7.svg", "7",
        "A STEP channel imported as linear keys. Each value is held flat by a copy of it one "
        "representable float before the next key (amber), and the sampler's linear interpolation across "
        "that one float is the step. Copying the keys as they are turns it into a ramp."),

    "FIG_CUBIC": ("l77b_fig8.svg", "8",
        "One cubic channel from Blender, the left thigh's rotation in the walk. The imported keys (ticks) "
        "follow the file's Hermite curve so closely the two lines coincide. The same curve with its "
        "tangents not scaled by the interval's length, <em>t</em><sub>d</sub>&nbsp;=&nbsp;0.133&nbsp;s, "
        "is 7.5 times too steep at every key and overshoots by up to 40&#176;."),

    "FIG_TWO": ("l77b_fig9.svg", "9",
        "The walk imported from Blender's sampled export and from its cubic export, compared joint by "
        "joint. They agree at the keys, differ by up to 0.916&#176; between them, and by at most "
        "0.218&#176; on the frames &#8212; the amount the two files differ by with no importer involved. "
        "The spikes are the knees, which turn fastest: the exporter normalises each tangent's control "
        "point as if it were a rotation (&#167;8.4)."),

    "FIG_NEUTRAL": ("l77b_fig10.svg", "10",
        "The head from above, 0.6&nbsp;s into the wave, when it has turned furthest. The 25 nose-tip "
        "vertices that bone heat left unweighted, in the first export (red) and after the weighting was "
        "fixed (green). In the first, Blender's exporter attached them to a <code>neutral_bone</code> "
        "nothing animates, so they stay pointing straight ahead while the head turns &#8212; up to "
        "16.9&nbsp;mm adrift."),

    "FIG_FACING": ("l77b_fig11.svg", "11",
        "glTF puts an asset's front at +<em>z</em>, and this engine's camera looks down "
        "&#8722;<em>z</em> &#8212; at the +<em>z</em> side of whatever it sees. So an unrotated glTF "
        "character faces the camera: the mannequin's nose is 2.879&nbsp;m from it and the centre of its "
        "head 3.000. Lesson 6.6 said the opposite. What differs is forward, not front."),

    "FIG_DEMO": ("l77b_fig12.svg", "12",
        "<code>mannequin --clip wave --time 1.0 --shot</code>: the second of Blender's two clips, one "
        "second in, the right arm raised and the forearm upright. The grey line from the floor to the "
        "hips is the <code>Armature</code> node the importer carried into the skeleton."),
}

LISTING_META = {
    "engine/include/engine/gfx/gltf.hpp": ("modified", "modified"),
    "engine/src/gfx/gltf.cpp": ("modified", "modified"),
    "engine/include/engine/anim/import.hpp": ("new", "new"),
    "engine/src/anim/import.cpp": ("new", "new"),
    "engine/include/engine/anim/skeleton.hpp": ("modified", "modified"),
    "engine/src/anim/skeleton.cpp": ("modified", "modified"),
    "engine/include/engine/anim/skin.hpp": ("modified", "modified"),
    "engine/include/engine/engine.hpp": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "demos/mannequin/main.cpp": ("new", "new"),
    "demos/CMakeLists.txt": ("modified", "modified"),
    "scratch/make_mannequin.py": ("new", "new"),
    "scratch/verify_77b_blender.py": ("new", "new"),
    "scratch/verify_77b_ex5.cpp": ("new", "new"),
    "scratch/verify_77b.cpp": ("new", "new"),
    "scratch/build_verify_77b.sh": ("new", "new"),
}

LISTING_LANG = {
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
    "scratch/make_mannequin.py": ("python", "Python"),
    "scratch/verify_77b_blender.py": ("python", "Python"),
    "scratch/build_verify_77b.sh": ("bash", "Shell"),
}

LISTING_SOURCE = {
    # PINNED, because an inserted lesson's files are not the working tree's (see
    # the module docstring). Every engine and demo pin was written by
    # `make_t77b.py tree ... --pins` and proved; the five scratch files are copies
    # of the working tree taken at the same time.
    "demos/CMakeLists.txt":                    "scratch/l77b_demos_CMakeLists.txt",
    "engine/CMakeLists.txt":                   "scratch/l77b_engine_CMakeLists.txt",
    "engine/include/engine/engine.hpp":        "scratch/l77b_engine_include_engine_engine.hpp",
    "engine/include/engine/gfx/gltf.hpp":      "scratch/l77b_engine_include_engine_gfx_gltf.hpp",
    "engine/src/gfx/gltf.cpp":                 "scratch/l77b_engine_src_gfx_gltf.cpp",
    "engine/include/engine/anim/skeleton.hpp": "scratch/l77b_engine_include_engine_anim_skeleton.hpp",
    "engine/src/anim/skeleton.cpp":            "scratch/l77b_engine_src_anim_skeleton.cpp",
    "engine/include/engine/anim/skin.hpp":     "scratch/l77b_engine_include_engine_anim_skin.hpp",
    "engine/include/engine/anim/import.hpp":   "scratch/l77b_engine_include_engine_anim_import.hpp",
    "engine/src/anim/import.cpp":              "scratch/l77b_engine_src_anim_import.cpp",
    "demos/mannequin/main.cpp":                "scratch/l77b_demos_mannequin_main.cpp",
    "scratch/make_mannequin.py":               "scratch/l77b_scratch_make_mannequin.py",
    "scratch/verify_77b_blender.py":           "scratch/l77b_scratch_verify_77b_blender.py",
    "scratch/verify_77b_ex5.cpp":              "scratch/l77b_scratch_verify_77b_ex5.cpp",
    "scratch/verify_77b.cpp":                  "scratch/l77b_scratch_verify_77b.cpp",
    "scratch/build_verify_77b.sh":             "scratch/l77b_scratch_build_verify_77b.sh",
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
<title>7.7b — Animated Characters from glTF: Skins and Clips · Build a Professional 3D Game Engine</title>
<meta name="description" content="A character authored in Blender and exported by Blender's glTF exporter, imported into the engine's skeleton, skinned meshes and clips. glTF's skin model read from the specification: joint matrices from global transforms and inverse binds, the skinned mesh node ignored, and the non-joint nodes above the joints carried into the skeleton; a stable parent-first sort; the file's inverse binds as data and the identity when absent; influences through three index spaces, four kept; rotations read x, y, z, w, and what a blind copy costs; LINEAR (slerp), STEP and CUBICSPLINE converted to the engine's lerp and nlerp to a stated tolerance, probed away from the midpoint where nlerp and slerp agree; a joint Blender's exporter invented for unweighted vertices; and which way a glTF character faces. The character plays within 40 micrometres of an independent double-precision evaluation of the specification, and three checks in Lesson 7.6's files are corrected.">

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
    <a class="prev-l" href="07-07-sampling-blending.html">
      <span class="dir">← Previous</span>
      <span class="ttl">7.7 — Sampling and Blending Animations</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="07-08-audio.html">
      <span class="dir">Next →</span>
      <span class="ttl">7.8 — SDL3 Audio: Streams, Mixing, and 3D Sound</span>
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
    for name in ("a", "b", "c", "d"):
        with open(f"scratch/l77b_body_{name}.html") as fh:
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
