#!/usr/bin/env python3
"""Assemble docs/lessons/05-03-logging-and-errors.html.

Same pipeline as build_52.py: prose from scratch/l53_body_{a,b}.html, figures
from scratch/figs_53.py, and code listings read straight out of the repository so
they cannot drift from what actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-03-logging-and-errors.html"

FIGURES = {
    "FIG_ONE_VOICE": ("l53_fig1.svg", "1",
        "197 logging calls, and before this lesson every one of them was the same statement: "
        "<code>SDL_Log</code>, which is <code>SDL_LOG_CATEGORY_APPLICATION</code> at "
        "<code>SDL_LOG_PRIORITY_INFO</code> and nothing else. <strong>A fatal error and a "
        "curiosity, printed identically</strong> &#8212; so the only two settings were &#8220;all "
        "of it&#8221; and &#8220;none of it&#8221;, and since the second loses the fatal error, "
        "everybody picks the first and then stops reading. After: two independent axes, and the "
        "engine defaults to silence."),

    "FIG_LEVELS": ("l53_fig2.svg", "2",
        "The six levels, their meanings as written promises, and how many of the engine&#8217;s "
        "92 messages landed at each. Two of the meanings do real work: <strong><code>warn</code> "
        "means we recovered</strong> &#8212; that is the whole distinction from "
        "<code>error</code>, and it is a mechanical test rather than a judgement call &#8212; and "
        "<code>debug</code> means &#8220;what I did&#8221; against <code>info</code>&#8217;s "
        "&#8220;what I am&#8221;. Below the fold: the rule for <em>who</em> writes the line."),

    "FIG_DEFAULTS": ("l53_fig3.svg", "3",
        "Where the default filtering comes from, and it is not from us. SDL&#8217;s documented "
        "default table is <code>app=info, assert=warn, test=verbose, *=error</code>, and "
        "<code>SDL_LOG_CATEGORY_CUSTOM</code> is exactly where SDL stops and applications begin "
        "&#8212; so <strong>every category we invent lands in the <code>*</code> arm and defaults "
        "to error</strong>. Moving the engine off <code>APPLICATION</code> makes it quiet with no "
        "configuration and no filtering code of our own. A wrapper would have had to reimplement "
        "this, and would have got a different answer."),

    "FIG_ERROR_OR_ASSERT": ("l53_fig4.svg", "4",
        "One question separates the two things that go wrong in a program, and it settles almost "
        "every case: <strong>could a correct program, on a working machine, encounter this?</strong> "
        "Yes &#8594; the world did it to you, so return an error that ships. No &#8594; your own "
        "code is wrong, so stop &#8212; and the check may be compiled out, because by the time you "
        "ship you have either fixed the bug or you have not. The three macros beneath differ only "
        "in which builds they survive."),

    "FIG_REPORT": ("l53_fig5.svg", "5",
        "The evidence that this lesson did not need to invent an error type. "
        "<code>obj_report</code> (3.5) and <code>gpu_report</code> (4.2) are the same shape "
        "&#8212; a status enum that names the failure, the facts you ask for next, and an "
        "<code>ok()</code> &#8212; and they were written <strong>a module apart, by nobody trying "
        "to match the other</strong>. A design a codebase keeps rediscovering is one worth naming "
        "rather than replacing, so 5.3 names it and converts the odd one out."),

    "FIG_BUILD_MATRIX": ("l53_fig6.svg", "6",
        "What each macro actually emits, per build configuration, read out of the object file. "
        "Green cells are identical to an empty function &#8212; nothing was generated at all. "
        "<strong>Read column 3.</strong> Plain <code>-O2</code> kills <code>ENGINE_ASSERT</code>, "
        "because SDL decides its level from <code>__OPTIMIZE__</code> rather than "
        "<code>NDEBUG</code>, while our trace logging is still fully compiled in; column 2 is the "
        "exact inverse. Two gates, two switches, and neither configuration is the one you would "
        "guess &#8212; which is how this lesson found a real bug in its own "
        "<code>ENGINE_VERIFY</code>."),
}

LISTING_META = {
    "engine/include/engine/core/log.hpp": ("new", "new"),
    "engine/src/core/log.cpp": ("new", "new"),
    "engine/include/engine/core/assert.hpp": ("new", "new"),
    "engine/include/engine/gfx/image.hpp": ("modified", "modified"),
    "engine/src/gfx/image.cpp": ("modified", "modified"),
    "scratch/convert_logs_53.py": ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/platform/platform.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_buffer.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_debug.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_device.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_mesh.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_pipeline.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_present.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_scene.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_shader.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_texture.cpp": ("modified", "modified"),
    "engine/src/platform/app.cpp": ("modified", "modified"),
    "engine/src/platform/platform.cpp": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "demos/sandbox/main.cpp": ("modified", "modified"),
}

# LISTING_SOURCE — added 2026-09-12, and without it THIS SCRIPT CANNOT RUN.
# ---------------------------------------------------------------------------
#
# The paths this page lists live under `src/`, and Module 5's refactor RETIRED
# THAT WHOLE DIRECTORY — the engine's sources moved to engine/src and its
# headers to engine/include/engine. So re-running this builder died at the
# first listing with a FileNotFoundError, which means the page had been
# unreproducible since Lesson 5.1 and nobody had noticed, because nobody had
# needed to rebuild it.
#
# Paths that DO still exist (shaders, CMakeLists.txt) are pinned too, and for
# the opposite reason: reading them live is silent rather than fatal, so every
# later lesson's edits leaked backwards into this page.
#
# The contents are pinned from commit ea7a05f — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "engine/include/engine/core/log.hpp":    "scratch/l53_engine_include_engine_core_log.hpp",
    "engine/src/core/log.cpp":               "scratch/l53_engine_src_core_log.cpp",
    "engine/include/engine/core/assert.hpp": "scratch/l53_engine_include_engine_core_assert.hpp",
    "engine/include/engine/gfx/image.hpp":   "scratch/l53_engine_include_engine_gfx_image.hpp",
    "engine/src/gfx/image.cpp":              "scratch/l53_engine_src_gfx_image.cpp",
    "scratch/convert_logs_53.py":            "scratch/l53_scratch_convert_logs_53.py",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/platform/platform.hpp": "scratch/l53_engine_include_engine_platform_platform.hpp",
    "engine/src/gfx/gpu_buffer.cpp": "scratch/l53_engine_src_gfx_gpu_buffer.cpp",
    "engine/src/gfx/gpu_debug.cpp": "scratch/l53_engine_src_gfx_gpu_debug.cpp",
    "engine/src/gfx/gpu_device.cpp": "scratch/l53_engine_src_gfx_gpu_device.cpp",
    "engine/src/gfx/gpu_mesh.cpp": "scratch/l53_engine_src_gfx_gpu_mesh.cpp",
    "engine/src/gfx/gpu_pipeline.cpp": "scratch/l53_engine_src_gfx_gpu_pipeline.cpp",
    "engine/src/gfx/gpu_present.cpp": "scratch/l53_engine_src_gfx_gpu_present.cpp",
    "engine/src/gfx/gpu_scene.cpp": "scratch/l53_engine_src_gfx_gpu_scene.cpp",
    "engine/src/gfx/gpu_shader.cpp": "scratch/l53_engine_src_gfx_gpu_shader.cpp",
    "engine/src/gfx/gpu_texture.cpp": "scratch/l53_engine_src_gfx_gpu_texture.cpp",
    "engine/src/platform/app.cpp": "scratch/l53_engine_src_platform_app.cpp",
    "engine/src/platform/platform.cpp": "scratch/l53_engine_src_platform_platform.cpp",
    "engine/CMakeLists.txt": "scratch/l53_engine_CMakeLists.txt",
    "demos/sandbox/main.cpp": "scratch/l53_demos_sandbox_main.cpp",
}

# LISTING_SOURCE — added 2026-09-12, and without it THIS SCRIPT CANNOT RUN.
# ---------------------------------------------------------------------------
#
# The paths this page lists live under `src/`, and Module 5's refactor RETIRED
# THAT WHOLE DIRECTORY — the engine's sources moved to engine/src and its
# headers to engine/include/engine. So re-running this builder died at the
# first listing with a FileNotFoundError, which means the page had been
# unreproducible since Lesson 5.1 and nobody had noticed, because nobody had
# needed to rebuild it.
#
# Paths that DO still exist (shaders, CMakeLists.txt) are pinned too, and for
# the opposite reason: reading them live is silent rather than fatal, so every
# later lesson's edits leaked backwards into this page.
#
# The contents are pinned from commit ea7a05f — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_LANG = {
    "scratch/convert_logs_53.py": ("py", "Python"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/CMakeLists.txt": ("cmake", "CMake"),
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
<title>5.3 — Logging, Assertions, and Errors Without Exceptions · Build a Professional 3D Game Engine</title>
<meta name="description" content="197 log calls, one category, one level — and five different answers to how a function reports failure. Two axes instead of one, SDL3 log categories based at SDL_LOG_CATEGORY_CUSTOM, a compile-time floor with if constexpr, a rule for who logs a failure, SDL assertions and the loop inside them, and what a release build actually removes — measured in bytes across four build configurations.">

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
    <a class="prev-l" href="05-02-platform-layer.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.2 — The Platform and Application Layer</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-04-handles.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.4 — Handles: Generational Indices</span>
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
    with open("scratch/l53_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l53_body_b.html") as fh:
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
