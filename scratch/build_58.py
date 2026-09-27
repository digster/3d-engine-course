#!/usr/bin/env python3
"""Assemble docs/lessons/05-08-ecs-runtime.html.

Same pipeline as build_57.py, minus the STATE block: STATE.md is the sole resume
key as of 2026-09-04, and lesson pages end at Further Reading.

Prose from scratch/l58_body_{a,b}.html, figures from scratch/figs_58.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/05-08-ecs-runtime.html"

FIGURES = {
    "FIG_COMPOSE": ("l58_fig1.svg", "1",
        "The same world described two ways, and the argument for this whole lesson at a scale "
        "where the performance argument does not apply. Above, one struct: the floor carries a "
        "<code>shininess</code> it never uses and a mover with no geometry is a null handle plus "
        "a branch in the renderer, because <strong>a struct is a promise that every instance has "
        "every field</strong>. Below, six component types: a waypoint has no blank material, it "
        "has no ROW in the material pool, and it is invisible because the render query names "
        "<code>geometry</code> and it has none. Same information; the difference is which "
        "absences cost something."),

    "FIG_IDSPACE": ("l58_fig2.svg", "2",
        "Lesson 5.7&#8217;s rule 2, drawn. Lesson 5.4&#8217;s <code>pool&lt;T&gt;</code> is "
        "already a sparse set in every respect but one: each pool MINTS ITS OWN KEYS off its own "
        "free list, so the mesh pool&#8217;s handle 7 and the texture pool&#8217;s handle 7 have "
        "nothing whatever to do with each other. &#8220;Does the thing in mesh slot 7 also have "
        "a texture?&#8221; is not a slow question there &#8212; it is <em>not a question</em>. "
        "Mint the id once, from one allocator, and hand it to every pool, and the question "
        "becomes well formed. Everything else in this lesson is a consequence of that single "
        "inversion."),

    "FIG_SLOTS": ("l58_fig3.svg", "3",
        "One slot through mint, retire and reuse &#8212; the mechanism that lets a destroyed "
        "entity be detected rather than merely absent. The generation is bumped ON REMOVAL, "
        "which closes to zero width the window in which a freed slot still carries a value an "
        "outstanding id holds. After panel 3 both <code>b</code> and <code>b2</code> name slot 1 "
        "and only one of them is alive; the test that separates them is a single 32-bit compare "
        "on a value already in a register. Without the generation, a reused index is a "
        "<em>guaranteed silent wrong answer</em> rather than a probable crash."),

    "FIG_ERASE": ("l58_fig4.svg", "4",
        "<code>erase</code>, and the one line people leave out. The component moves out of the "
        "middle by swap-and-pop, which is what keeps the dense arrays packed &#8212; but the "
        "entity that was swapped INTO the hole did not ask to move, and its sparse entry still "
        "points at where it used to be. Patching that entry is one line, and without it the pool "
        "is not obviously broken but <strong>silently</strong> broken: the symptom is one entity "
        "reading another&#8217;s data, an arbitrary number of frames later, in a different "
        "system. Lesson 5.7 hit exactly this bug and found it only after two hundred frames of "
        "churn."),

    "FIG_ERASURE": ("l58_fig6.svg", "6",
        "Type erasure with no RTTI. Each component type is handed a small integer on first use "
        "by a counter behind a function-local static; that integer indexes a vector of "
        "base-class pointers, and the concrete type comes back with a plain "
        "<code>static_cast</code> &#8212; <strong>safe because the id is what created the "
        "pool</strong>, so the type at that slot is established by construction rather than "
        "guessed at use. Note the cost being accepted in the lower half: five virtual calls to "
        "destroy an entity that had three components, which is the price of not keeping a "
        "per-entity component mask. It is also the ONLY place a virtual call happens &#8212; a "
        "view holds <code>pool&lt;T&gt;*</code> and never touches the base."),

    "FIG_LEAD": ("l58_fig5.svg", "5",
        "Rule 3, in candidates. Both walks return the same six entities and both are correct; "
        "one of them looks at sixty candidates and rejects fifty-four, the other looks at twelve "
        "and rejects six. The decision is <em>one comparison in the view&#8217;s "
        "constructor</em>, made once per query rather than once per entity, and it gets better "
        "the rarer the component &#8212; a query for &#8220;placement and "
        "<code>player_input</code>&#8221; in a world of ten thousand entities and one player "
        "walks ONE candidate if it leads correctly and ten thousand if it does not. Lesson 5.7 "
        "measured the same effect at 1.73&#215; against 1.46&#215;."),
}

LISTING_META = {
    "engine/include/engine/ecs/entity.hpp": ("new", "new"),
    "engine/include/engine/ecs/pool.hpp": ("new", "new"),
    "engine/include/engine/ecs/registry.hpp": ("new", "new"),
    "engine/include/engine/ecs/view.hpp": ("new", "new"),
    "demos/ecs_swarm/main.cpp": ("new", "new"),
    "demos/CMakeLists.txt": ("modified", "modified"),
    "scratch/verify_58.cpp": ("new", "new"),
}

LISTING_LANG = {
    "demos/CMakeLists.txt": ("cmake", "CMake"),
}

# A LISTING PINNED TO A SNAPSHOT, AND THE REASON IS A BUG THIS FILE CAUSED.
#
# Listings are spliced from the live repository so they cannot drift from what
# actually compiles — which is right, and which has one failure mode: a file that
# a LATER lesson changes. Lesson 5.9 rewrote demos/ecs_swarm/main.cpp to use the
# transform hierarchy, and re-running this script afterwards (to repoint one
# navigation link) spliced 5.9's demo into 5.8's page. The listing then referenced
# `engine::ecs::hierarchy`, which does not exist at 5.8's point in the course, so
# the page carried code that could not compile where it was published.
#
# So: any file a later lesson modifies gets pinned here, to a snapshot taken at
# THIS lesson's state. The snapshot is BYTE-IDENTICAL to `demos/ecs_swarm/main.cpp`
# at commit dfbdb0f, which is the commit this page shipped in — verified with
# `git show dfbdb0f:demos/ecs_swarm/main.cpp | diff - scratch/l58_ecs_swarm.cpp`.
# It also compiles and reproduces the exact numbers this page publishes: 121
# entities, 468 components, 5 pools, 97 drawn, 3,136 triangles, lead pool 1, 97
# candidates.
#
# THE GENERAL RULE, and it is the second time this shape has bitten (build_57.py
# was still stamping a retired STATE block): A BUILDER IS NOT FROZEN JUST BECAUSE
# ITS PAGE IS SHIPPED. Re-running an old builder re-reads today's repository.
# EXTENDED 2026-09-12 to cover EVERY listing, not just the files this
# lesson wrote. The partial dict below pinned what its author knew would
# move; everything else stayed live, so later lessons' edits leaked into
# this page anyway — which is why it no longer rebuilt to what it shipped.
# Pinned from dfbdb0f, verified per file against the shipped page
# with scratch/which_commit.py.
#
# The original dict, kept for its provenance:
#   LISTING_SOURCE = {
#       "demos/ecs_swarm/main.cpp": "scratch/l58_ecs_swarm.cpp",
#   }
LISTING_SOURCE = {
    "engine/include/engine/ecs/entity.hpp":   "scratch/l58_engine_include_engine_ecs_entity.hpp",
    "engine/include/engine/ecs/pool.hpp":     "scratch/l58_engine_include_engine_ecs_pool.hpp",
    "engine/include/engine/ecs/registry.hpp": "scratch/l58_engine_include_engine_ecs_registry.hpp",
    "engine/include/engine/ecs/view.hpp":     "scratch/l58_engine_include_engine_ecs_view.hpp",
    "demos/ecs_swarm/main.cpp":               "scratch/l58_demos_ecs_swarm_main.cpp",
    "demos/CMakeLists.txt":                   "scratch/l58_demos_CMakeLists.txt",
    "scratch/verify_58.cpp":                  "scratch/l58_scratch_verify_58.cpp",
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
<title>5.8 — The ECS Runtime · Build a Professional 3D Game Engine</title>
<meta name="description" content="Building the sparse-set ECS Lesson 5.7 chose: a generational entity id minted once and honoured by every pool, a component pool that is three arrays and twenty lines of logic, a registry that type-erases without RTTI, and a query that leads with the smallest pool. Includes the swap-and-pop patch that corrupts a pool silently when omitted, the one iteration rule a view imposes, and a 121-entity demo whose argument is composition rather than speed — because at 121 entities speed is not an argument.">

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
    <a class="prev-l" href="05-07-ecs-storage.html">
      <span class="dir">← Previous</span>
      <span class="ttl">5.7 — An ECS from Scratch: Storage Design</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="05-09-transform-hierarchy.html">
      <span class="dir">Next →</span>
      <span class="ttl">5.9 — Transform Hierarchy and the Camera System</span>
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
    with open("scratch/l58_body_a.html") as fh:
        body_a = fh.read()
    with open("scratch/l58_body_b.html") as fh:
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
