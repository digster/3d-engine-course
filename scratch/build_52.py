#!/usr/bin/env python3
"""Assemble docs/lessons/05-02-platform-layer.html.

Same pipeline as build_51.py: prose from scratch/l52_body_{a,b}.html, figures
from scratch/figs_52.py, and code listings read straight out of the repository so
they cannot drift from what actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-02-platform-layer.html"

FIGURES = {
    "FIG_FORK": ("l52_fig1.svg", "1",
        "The two arrangements, with the same six phases in both. <strong>The only thing that "
        "moves is which box the word <code>while</code> is written inside.</strong> On the left "
        "your <code>main()</code> owns the loop and calls the engine; on the right SDL owns it "
        "and calls your four callbacks. Both are supported SDL3 &#8212; the header says so in "
        "as many words &#8212; but only the right-hand one also works where the operating system "
        "owns the loop and <code>while (true)</code> cannot exist: the web, and mobile."),

    "FIG_FRAME": ("l52_fig2.svg", "2",
        "One frame under <code>SDL_MAIN_USE_CALLBACKS</code>. SDL pumps the queue, dispatches "
        "<em>every</em> queued event to <code>SDL_AppEvent</code>, and only then calls "
        "<code>SDL_AppIterate</code> &#8212; so Lesson 1.2's &#8220;drain, then "
        "<code>update()</code>&#8221; contract still holds. <strong>The contract survived the "
        "inversion; its enforcer changed.</strong> That is not read off a doc comment: it is "
        "three consecutive lines of <code>src/main/SDL_main_callbacks.c</code>, quoted in "
        "&#167;3.3."),

    "FIG_LIFECYCLE": ("l52_fig3.svg", "3",
        "The ladder <code>platform</code> owns. Read downwards it is creation; read upwards it "
        "is destruction, and the two must be exact mirrors because the texture belongs to the "
        "renderer and the renderer belongs to the window. The three dashed markers are the "
        "surfaces &#8212; each is a rung you stop climbing at. <strong>The gap between "
        "<code>gpu</code> and <code>renderer</code> is Lesson 4.2's rule, turned from a comment "
        "above an <code>if</code> into a choice that must be made before anything exists.</strong>"),

    "FIG_BOILERPLATE": ("l52_fig4.svg", "6",
        "What each demo has to say for itself, counted. The definition of &#8220;lifecycle "
        "call&#8221; is written down in <code>measure_52.py</code> rather than left to a feeling "
        "about which lines look like ceremony. <strong>48 across the three demos before; 3 "
        "after</strong> &#8212; and all three survivors are <code>SDL_PollEvent</code>, in "
        "<code>sandbox</code>, which keeps its loops on purpose. Note the first run of this "
        "script counted a <em>comment</em> about <code>SDL_Init</code> as a call to it."),

    "FIG_OWNERSHIP": ("l52_fig5.svg", "4",
        "A C++ object with a destructor, handed through a C API that only understands "
        "<code>void*</code>. <code>release()</code> gives up ownership; the constructor "
        "<code>unique_ptr(raw)</code> takes it back. The red interval is the only place in this "
        "engine where a raw pointer owns something &#8212; and <strong>it contains no branches at "
        "all</strong>, because <code>init</code> publishes the pointer BEFORE anything that can "
        "fail. Publishing only on success looks safer and gives you two teardown paths and a "
        "null that means two different things."),

    "FIG_LAYERS": ("l52_fig6.svg", "5",
        "The rule that keeps two arrangements from becoming two engines: <strong><code>app</code> "
        "is built ON <code>platform</code>, never beside it.</strong> Note <code>sandbox</code>, "
        "which reaches past <code>app</code> to <code>platform</code> and keeps its own "
        "<code>main()</code> &#8212; that has to stay possible, or the library path has no "
        "independent users and drifts into being an implementation detail of the framework. The "
        "crossed arrow is <code>app</code> reaching around <code>platform</code> to SDL, which "
        "would be the first step in growing a private engine inside the convenience wrapper."),
}

LISTING_META = {
    "engine/include/engine/platform/platform.hpp": ("new", "new"),
    "engine/src/platform/platform.cpp": ("new", "new"),
    "engine/include/engine/platform/app.hpp": ("new", "new"),
    "engine/src/platform/app.cpp": ("new", "new"),
    "engine/include/engine/platform/main.hpp": ("new", "new"),
    "engine/include/engine/gfx/image.hpp": ("modified", "modified"),
    "engine/src/gfx/image.cpp": ("modified", "modified"),
    "engine/include/engine/engine.hpp": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "demos/CMakeLists.txt": ("modified", "modified"),
    "demos/hello_cube/main.cpp": ("modified", "modified"),
    "demos/pong/main.cpp": ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "demos/sandbox/main.cpp": ("modified", "modified"),
}

# LISTING_SOURCE — added 2026-09-12, and without it THIS SCRIPT CANNOT RUN.
# ---------------------------------------------------------------------------
#
# Every path here was read LIVE, so this page showed whatever the engine said on
# the day it was last built — and the day it was last built was NOT the day it
# shipped.
#
# PINNED FROM ea7a05f (Lesson 5.3), NOT FROM 5175d70 (Lesson 5.2), AND THAT IS
# NOT A MISTAKE. When 5.3 landed it re-ran this builder to retrofit the `next`
# nav link, and the live reads quietly pulled 5.3's code into 5.2's page: 156
# lines in, 38 out. So the published 5.2 shows `ENGINE_LOG_ERROR(...)` where 5.2
# itself wrote `SDL_Log(...)`, shows `image_report` where 5.2 wrote
# `image_status`, and includes <engine/core/log.hpp>, a header Lesson 5.3
# creates. Pinning to 5175d70 would therefore NOT reproduce the published page —
# it would silently rewrite it — so the pins record what the page actually
# published, which is the only thing a reproducibility pin can honestly mean.
#
# THE UNDERLYING DEFECT IS REAL AND IS NOT FIXED HERE. Lesson 5.2's listings show
# code from a lesson the student has not read yet, which violates CLAUDE.md §8's
# "every listing must compile at its point in the course". Correcting that
# changes a published lesson's content, which is a separate decision from making
# its builder reproducible; it is recorded in LEARNINGS.md and left for the
# course author. 05-04-handles.html has the same injury from d599928 (Lesson
# 5.5) — those two are the only pages where the next lesson's commit changed
# more than the nav links.
#
# Verified per file with `scratch/which_commit.py 52 5175d70 ea7a05f`, which
# decides the question by substring rather than by inference: the page embeds
# each listing escaped, so the commit it came from is directly testable.
# platform.cpp and engine/CMakeLists.txt match NEITHER commit, because the
# 2026-09-08 Module 8 -> Module 9 renumber edited the rendered page and could not
# reach a source; that correction is ported back into the pins.
LISTING_SOURCE = {
    "engine/include/engine/platform/platform.hpp": "scratch/l52_engine_include_engine_platform_platform.hpp",
    "engine/src/platform/platform.cpp":            "scratch/l52_engine_src_platform_platform.cpp",
    "engine/include/engine/platform/app.hpp":      "scratch/l52_engine_include_engine_platform_app.hpp",
    "engine/src/platform/app.cpp":                 "scratch/l52_engine_src_platform_app.cpp",
    "engine/include/engine/platform/main.hpp":     "scratch/l52_engine_include_engine_platform_main.hpp",
    "engine/include/engine/gfx/image.hpp":         "scratch/l52_engine_include_engine_gfx_image.hpp",
    "engine/src/gfx/image.cpp":                    "scratch/l52_engine_src_gfx_image.cpp",
    "engine/include/engine/engine.hpp":            "scratch/l52_engine_include_engine_engine.hpp",
    "engine/CMakeLists.txt":                       "scratch/l52_engine_CMakeLists.txt",
    "demos/CMakeLists.txt":                        "scratch/l52_demos_CMakeLists.txt",
    "demos/hello_cube/main.cpp":                   "scratch/l52_demos_hello_cube_main.cpp",
    "demos/pong/main.cpp":                         "scratch/l52_demos_pong_main.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "demos/sandbox/main.cpp": "scratch/l52_demos_sandbox_main.cpp",
}

LISTING_LANG = {
    "engine/CMakeLists.txt": ("cmake", "CMake"),
    "demos/CMakeLists.txt": ("cmake", "CMake"),
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
<title>5.2 — The Platform and Application Layer · Build a Professional 3D Game Engine</title>
<meta name="description" content="Who owns the loop? A library you call, or a framework that calls you. Building both on SDL3's main callbacks — SDL_AppInit, SDL_AppIterate, SDL_AppEvent, SDL_AppQuit — with a lifecycle layer that admits SDL rather than hiding it, three surfaces including a headless one, and ownership handed safely through a C void*. 48 lifecycle SDL calls become 3, and Pong becomes a program.">

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

# AMENDED 2026-09-12: the STATE block is gone, and this script no longer
# stamps one. `STATE.md` became the sole resume key at 0.6 and CLAUDE.md §9
# was amended at 5.7 to retire the per-lesson block — but this builder
# predates that and was never updated, so re-running it would have RE-ADDED a
# STATE block to a page it was stripped from. The same amendment build_57.py
# received in 5.8.
TAIL = """

  <nav class="lesson-nav" aria-label="Lesson navigation (bottom)">
    <a class="prev-l" href="05-01-the-refactor.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.1 — The Refactor: Engine, Demos, and the Public API</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-03-logging-and-errors.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.3 — Logging, Assertions, and Errors Without Exceptions</span>
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
    with open("scratch/l52_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l52_body_b.html") as fh:
        body_b = fh.read()

    page = HEAD + body_a + "\n" + body_b + TAIL

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
