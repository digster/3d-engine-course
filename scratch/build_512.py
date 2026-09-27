#!/usr/bin/env python3
"""Assemble docs/lessons/05-12-checkpoint-game.html.

Same pipeline as build_618.py. No STATE block: STATE.md is the sole resume key
and lesson pages end at Further Reading.

Prose from scratch/l512_body_{a,b,c,d}.html, figures from scratch/figs_512.py,
and every code listing PINNED — see LISTING_SOURCE, which is populated from the
first commit rather than left empty, and for a reason peculiar to this lesson.

WHY THE PINS ARE ERA COPIES AND NOT REPOSITORY PATHS.

Lesson 5.12 closes Module 5, so everything it publishes must compile against the
engine as it stood at Lesson 5.11 (CLAUDE.md §8). This lesson was AUTHORED after
Module 6, against a repository that is eleven lessons further on, and Module 6
changed two things this program touches: 6.2 renamed
`directional_light::intensity` to `::irradiance`, and 6.5 folded `tint` and
`specular surface` into one `material`.

So the six engine/demo listings below are pinned to copies taken from a 5.11
checkout (commit 9be6c96), built and run there — `scratch/port_512.py` records
the delta and applies it to produce the files that live in the repository today.
Reading those paths LIVE would publish Module 6 spellings inside a Module 5
lesson, which is exactly the defect docs/_template/README.md §15 calls Cause A,
arriving by a route that pinning-at-the-next-lesson would not have caught.

The three harness listings are ERA-NEUTRAL — `verify_512.cpp` deliberately names
no field Module 6 moves, and compiles and passes against both engines — so their
pins are working-tree copies, which is also the only way to preserve them at all
since scratch/ is gitignored.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-12-checkpoint-game.html"

FIGURES = {
    "FIG_BOUNDARY": ("l512_fig1.svg", "1",
        "Six programs on one public API, and the blindness the arrangement creates. "
        "<code>sandbox</code> predates the boundary; <code>pong</code> is Module 1&#8217;s game "
        "relocated; and the three written since Lesson 5.1 were each written by the person who had "
        "just built the subsystem they demonstrate. <strong>When you write the demo for a thing you "
        "have just built, you already know where everything is</strong> &#8212; so you never "
        "discover that a function is unexported, because you know it is and you reach past it "
        "without noticing you chose. <code>collector</code> is the first program whose requirements "
        "were not chosen to flatter the engine, and the only one that needs every row of the "
        "library at once."),

    "FIG_UMBRELLA": ("l512_fig2.svg", "2",
        "The header that calls itself &#8220;the whole public API, in one include&#8221; and &#8220;the "
        "fastest way to see whether something is public&#8221;, measured against the directory it "
        "claims to summarise. <strong>Forty of fifty-five.</strong> Exactly one of the fifteen "
        "absences is correct and documented &#8212; <code>platform/main.hpp</code> defines an entry "
        "point, and an umbrella must not be a way to acquire a <code>main</code> by accident. The "
        "other fourteen are the entire ECS, the entire asset system, handles, pools, the logger, the "
        "assertions and the action map: very nearly everything Module 5 built. The history below is "
        "the whole explanation &#8212; 5.2 and 5.11 remembered, and seven lessons did not, which is "
        "what a rule kept by memory looks like from a distance."),

    "FIG_COMPILE": ("l512_fig3.svg", "3",
        "The measurement that comes out the opposite way round from the prediction. One translation "
        "unit, <code>c++ -std=c++20 -c</code>, best of five. The obvious expectation is that the "
        "umbrella is the expensive way and an explicit include list is the cheap one; in fact "
        "<strong>the umbrella as shipped was cheaper than including exactly what the game "
        "uses</strong>, because the fourteen headers it omitted are the templated ones &#8212; "
        "<code>registry.hpp</code>, <code>view.hpp</code>, <code>pool.hpp</code>, "
        "<code>asset_store.hpp</code>, <code>actions.hpp</code>. An incomplete umbrella and a cheap "
        "umbrella are indistinguishable on a stopwatch. Completing it costs <strong>+39%</strong> "
        "against the broken version and <strong>+5.4%</strong> against including what you use &#8212; "
        "so the single-file number is not the reason to avoid an umbrella. The reason is the "
        "incremental rebuild, which no single-file benchmark can see."),

    "FIG_BRIDGE": ("l512_fig4.svg", "4",
        "Components to pixels, on both renderers, with the missing arrow marked. The software path "
        "is complete as of this lesson: <code>collect_renderables</code> turns "
        "<code>world_transform</code> plus <code>renderable</code> into the "
        "<code>scene_object</code> span both renderers were already built to consume. The GPU path "
        "has no first step at all &#8212; <code>gpu_scene_renderer</code> consumes "
        "<code>gpu_draw_item</code>s and no public function produces them from entities. "
        "<strong><code>sandbox</code> crosses that gap privately, inside its own "
        "<code>main.cpp</code></strong>, which is precisely why nobody had noticed: the one program "
        "that ever crossed it was written before the boundary existed and keeps its bridge to "
        "itself."),

    "FIG_BOOM": ("l512_fig5.svg", "5",
        "Why a camera parented to the player is not a follow camera, and why the fix is one more "
        "link rather than a special case. The rover leans into its turns and is a scaled box, so a "
        "camera parented straight to it inherits <em>both</em> &#8212; a see-sawing horizon and a "
        "non-uniform scale. The boom cancels them, and the algebra has exactly one trap in it: "
        "<strong>the inverse of a product reverses the order of its factors</strong>, so the "
        "unscale comes first. Written the way the English reads &#8212; &#8220;undo the roll, then "
        "undo the scale&#8221; &#8212; you get the other product, which is a different matrix "
        "because a rotation and a non-uniform scale do not commute. The two agree on <em>x</em> to "
        "four decimal places and differ by 36% on <em>y</em>, which is what makes it expensive: "
        "close enough to look like a tuning problem."),

    "FIG_UNIT": ("l512_fig6.svg", "6",
        "One word, two conventions, in one header. <code>cube_mesh()</code> spans &#177;0.5, so a "
        "<code>transform</code>&#8217;s <code>scale</code> is the box&#8217;s <strong>full "
        "size</strong>; <code>icosahedron_mesh()</code>&#8217;s vertices are at distance 1.0, so "
        "its <code>scale</code> is a <strong>radius</strong>. Both are documented and both are "
        "reasonable alone; together they mean <code>scale = 1</code> gives a one-metre cube and a "
        "two-metre ball. The trap is not the asymmetry &#8212; it is that a HALF-EXTENT is what the "
        "rest of the program has in its hand (a collision test wants one; "
        "<code>debug_lines::box</code> takes one), so the value you reach for is wrong by two at "
        "exactly the moment you reach for it. This one file got it wrong three times."),

    "FIG_FINDINGS": ("l512_fig7.svg", "7",
        "What a game found that five demos did not, and where each one is answered. Two were closed "
        "for the price of a header, and both turn out to be <em>promises already on record</em>: "
        "<code>engine.hpp</code>&#8217;s claim to list the public API, and <code>scene.hpp</code>&#8217;s "
        "claim that &#8220;Module 5&#8217;s ECS replaces the struct with components&#8221;. One is "
        "reported and deliberately left standing, because fixing it means changing two published "
        "headers and the signature is a better bug report than a comment. The remaining four are "
        "not defects &#8212; they are the curriculum, named and dated at hour 62 instead of "
        "discovered at hour 434."),

    "FIG_RESULT": ("l512_fig8.svg", "8",
        "The deterministic frame, run-length encoded from the real render. 240 fixed steps with "
        "<code>drive</code> held at 1.0 and <code>steer</code> at 0.55, which is why the shot is a "
        "picture of the <em>game</em> rather than of a spawn function &#8212; the rover has driven, "
        "the carousel has turned, and two orbs are gone. Press <kbd>G</kbd> for the lower panel: "
        "889 debug lines queued by a system with no framebuffer in its signature, including an "
        "oriented box round every pillar drawn from the same world matrix a collision test would "
        "use. <strong>And neither picture carries a score</strong>, because a "
        "<code>--shot</code> run is headless, so there is no <code>SDL_Renderer</code>, so there is "
        "no HUD &#8212; the engine has no text of its own (&#167;8.2)."),
}

LISTING_META = {
    "engine/include/engine/gfx/renderable.hpp": ("new", "new"),
    "engine/src/gfx/renderable.cpp":            ("new", "new"),
    "engine/include/engine/engine.hpp":         ("modified", "modified"),
    "engine/CMakeLists.txt":                    ("modified", "modified"),
    "demos/collector/main.cpp":                 ("new", "new"),
    "demos/CMakeLists.txt":                     ("modified", "modified"),
    "scratch/verify_512.cpp":                   ("new", "new"),
    "scratch/build_verify_512.sh":              ("new", "new"),
    "scratch/golden_512.cpp":                   ("new", "new"),
}

LISTING_LANG = {
    "engine/CMakeLists.txt":       ("cmake", "CMake"),
    "demos/CMakeLists.txt":        ("cmake", "CMake"),
    "scratch/build_verify_512.sh": ("bash", "shell"),
}

LISTING_SOURCE = {
    # PINNED AT WRITING TIME, not at the next lesson's. The six engine and demo
    # files are copies taken from a Lesson 5.11 checkout (9be6c96) where this
    # program was written, compiled and run; scratch/port_512.py records the two
    # Module 6 deltas that produce the versions in the repository today.
    #
    # The three harness files are era-neutral and identical in both trees, so
    # their pins are working-tree copies — which is also the only way to keep
    # them at all, since scratch/ is gitignored and has no history.
    "engine/include/engine/gfx/renderable.hpp": "scratch/l512_engine_include_engine_gfx_renderable.hpp",
    "engine/src/gfx/renderable.cpp":            "scratch/l512_engine_src_gfx_renderable.cpp",
    "engine/include/engine/engine.hpp":         "scratch/l512_engine_include_engine_engine.hpp",
    "engine/CMakeLists.txt":                    "scratch/l512_engine_CMakeLists.txt",
    "demos/collector/main.cpp":                 "scratch/l512_demos_collector_main.cpp",
    "demos/CMakeLists.txt":                     "scratch/l512_demos_CMakeLists.txt",
    "scratch/verify_512.cpp":                   "scratch/l512_scratch_verify_512.cpp",
    "scratch/build_verify_512.sh":              "scratch/l512_scratch_build_verify_512.sh",
    "scratch/golden_512.cpp":                   "scratch/l512_scratch_golden_512.cpp",
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
<title>5.12 — Checkpoint: A Small 3D Game on the Public API · Build a Professional 3D Game Engine</title>
<meta name="description" content="Eleven lessons ago we drew a line and called it law: engine/ is a library, demos/ are programs, and a program may include nothing but the public headers. Since then we have built an application layer, a logger, handles, an asset store, an ECS, a transform hierarchy, a camera, an action map, a debug drawer and a debug UI - and every program that used them was written by the person who had just built them, that afternoon, to show them off. That is not a test of an API. A demo stops at the edge of the subsystem it demonstrates; a game crosses every edge there is, and the seams between subsystems are where a public API is weakest because nobody has ever stood on them. So this lesson writes a game: drive a rover round an arena and collect twelve orbs, half of them riding a carousel. It keeps a list of everything the published headers could not do, and there are seven. Two turn out to be promises the engine had already made and not kept - the header calling itself 'the whole public API, in one include' lists 40 of 55 and is missing the entire ECS, the asset store, the action map and the logger; and scene.hpp has said since 5.1 that 'Module 5's ECS replaces the struct with components', which Module 5 never did. Both are fixed here, one against this engine's own three-caller rule and owing a reason. The compile-time measurement that justifies completing the umbrella comes out the opposite way round from the prediction: the incomplete file was CHEAPER than including what you use, because the headers it omitted are the templated ones. A camera parented to a rolling, non-uniformly scaled rover produces a matrix that is not rigid, an assertion fires, and a headless run hangs for ever - one composition order written the way it reads in English rather than the way the algebra says, with x agreeing to four decimals and y out by 36 per cent. The unit cube spans plus or minus a half and the icosahedron has radius one, which cost this file three separate factor-of-two bugs. And the remaining five findings - no collision, no text, no audio, no public path from an entity to the GPU renderer, and an ECS view that cannot be const - are named, measured and handed to the module that owns them, at hour 62 instead of hour 434.">

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
    <a class="prev-l" href="05-11-imgui-debug-draw.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.11 — Dear ImGui and the Debug Draw System</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-01-linear-and-srgb.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.1 — Linear and sRGB: The Gamma Lesson</span>
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
        with open(f"scratch/l512_body_{name}.html") as fh:
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
