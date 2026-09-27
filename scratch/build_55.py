#!/usr/bin/env python3
"""Assemble docs/lessons/05-05-asset-system.html.

Same pipeline as build_54.py: prose from scratch/l55_body_{a,b}.html, figures
from scratch/figs_55.py, and code listings read straight out of the repository so
they cannot drift from what actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-05-asset-system.html"

FIGURES = {
    "FIG_QUESTIONS": ("l55_fig1.svg", "1",
        "Lesson 5.4 built a reference that can be told it is out of date, and answered exactly "
        "one question with it. These are the three it does not answer &#8212; and each has a "
        "receipt in the source as it stood this morning: an eight-step load in <em>demo</em> "
        "code, a comment from Lesson 3.5 promising that &#8220;Module 5 caches&#8221;, and the "
        "fact that <strong>nothing in this engine has ever freed anything</strong>. All three "
        "were unanswerable for one reason, and it is the same reason: you cannot free a thing "
        "safely until you can say what happens to the references to it."),

    "FIG_SEARCH_PATH": ("l55_fig2.svg", "2",
        "The smallest idea in the lesson and the one the rest stands on. <strong>A name is not a "
        "path.</strong> <code>&quot;torus.obj&quot;</code> is stable, is what a scene file "
        "records, and is the key the store caches on; the absolute path is where that name "
        "resolved today, on this machine, with these roots. Once they are different things, "
        "resolution becomes a place to put a policy &#8212; and an ORDERED list of roots is the "
        "one that pays immediately: mods, localisation packs, editing out of the source tree "
        "while the game runs, and a test fixture standing in for a real asset are all the same "
        "mechanism seen from four directions."),

    "FIG_NO_REFCOUNT": ("l55_fig3.svg", "3",
        "The design decision of the lesson, and it is a refusal. A refcounted handle needs a "
        "copy constructor, a destructor and a pointer to its store &#8212; which costs it every "
        "property Lesson 5.4 was written to obtain: four bytes, trivially copyable, "
        "<code>memcpy</code>-able into a component, serializable with no fixups. It would be a "
        "<code>shared_ptr</code> with extra steps. <strong>So: explicit unload</strong>, which "
        "is safe to get wrong precisely because 5.4 made staleness detectable &#8212; forgetting "
        "is a leak a total finds, unloading early is a null and a counter, and neither is a "
        "crash. One place still needs a rule, and it is on the right."),

    "FIG_CASCADE": ("l55_fig4.svg", "4",
        "The hole Lesson 5.4 deliberately left, closed. <code>with_normals</code> output is a "
        "real mesh occupying real memory, but <strong>nobody asked for it</strong>: it has no "
        "name, nothing outside the system knows it exists, and it has no reason to exist except "
        "that its source does. So a derived asset is <em>owned</em> by its source and an unload "
        "cascades &#8212; <strong>transitively</strong>, because derivations chain and a cascade "
        "that stops after one hop leaks the tail. The same shape covers a texture and its "
        "mipmaps, a shader and its variants, a mesh and its GPU upload."),

    "FIG_COST": ("l55_fig5.svg", "5",
        "What an acquire costs, to scale, and the distribution is the opposite of the "
        "intuition. <strong>The acquire IS the parse</strong> &#8212; 99.9% of it. Finding the "
        "file, reading 200 KB off disk, flipping 2,352 texture coordinates and moving four "
        "vectors into the pool are <em>together</em> 0.55%, and they are the four stages people "
        "assume the cost is in. &#8220;Asset loading is slow because disks are slow&#8221; is a "
        "sentence from a different decade. The cache hit is drawn two pixels wide because that "
        "is what 14,771&#215; looks like to scale &#8212; it is not fast loading, it is "
        "<em>not loading</em>. And the third bar is a finding of its own: the demo&#8217;s "
        "validation pass, which the store does not perform, has been quietly adding 66% to every "
        "load since Module 3."),

    "FIG_UNLOAD_LIVE": ("l55_fig6.svg", "6",
        "The lesson's whole claim in one picture, and it is <code>verify_55</code> &#167;F. Free "
        "a mesh while a scene is still holding a handle to it &#8212; and the scene <em>is</em> "
        "still holding it, because <code>build_scene</code> rewrites the same array every frame "
        "and nothing nulled it. The only consequence is one object missing and "
        "<code>unresolved = 1</code>. Before Lesson 5.4 this was a dangling <code>std::span</code> "
        "and a picture nobody could trust; before 5.5 it could not be performed at all."),
}

LISTING_META = {
    "engine/include/engine/asset/search_path.hpp": ("new", "new"),
    "engine/src/asset/search_path.cpp": ("new", "new"),
    "engine/include/engine/asset/asset_store.hpp": ("new", "new"),
    "engine/src/asset/asset_store.cpp": ("new", "new"),
    "demos/common/demo_scene.hpp": ("modified", "modified"),
    "demos/common/demo_scene.cpp": ("modified", "modified"),
    "scratch/verify_55.cpp": ("new", "new"),
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
# The contents are pinned from commit d599928 — the commit that SHIPPED
# this lesson — so the listings show the code as it stood when the lesson was
# written, which is what a lesson's listings are supposed to show. Same
# discipline as build_310.py and build_41.py (fixed 2026-09-11) and every
# builder from 5.8 onward.
LISTING_SOURCE = {
    "engine/include/engine/asset/search_path.hpp": "scratch/l55_engine_include_engine_asset_search_path.hpp",
    "engine/src/asset/search_path.cpp":            "scratch/l55_engine_src_asset_search_path.cpp",
    "engine/include/engine/asset/asset_store.hpp": "scratch/l55_engine_include_engine_asset_asset_store.hpp",
    "engine/src/asset/asset_store.cpp":            "scratch/l55_engine_src_asset_asset_store.cpp",
    "demos/common/demo_scene.hpp":                 "scratch/l55_demos_common_demo_scene.hpp",
    "demos/common/demo_scene.cpp":                 "scratch/l55_demos_common_demo_scene.cpp",
    "scratch/verify_55.cpp":                       "scratch/l55_scratch_verify_55.cpp",
}

LISTING_LANG = {}


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
<title>5.5 — The Asset System v1 · Build a Professional 3D Game Engine</title>
<meta name="description" content="Handles were the mechanism; this is the policy. An ordered search path where a name becomes a path, a store that loads once and hands back the same handle, and the word Module 5 has been building towards — unload. Why reference counting is refused (it costs a handle every property that made it worth having), why a derived asset is owned by its source and cascades transitively, why import settings are part of an asset's identity, and what an acquire actually costs: parsing is 99.9% of it, the disk is 0.4%, and the cache is 14,771x.">

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
    <a class="prev-l" href="05-04-handles.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.4 — Handles: Generational Indices</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-06-data-oriented-design.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.6 — Data-Oriented Design</span>
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
    with open("scratch/l55_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l55_body_b.html") as fh:
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
