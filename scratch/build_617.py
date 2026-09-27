#!/usr/bin/env python3
"""Assemble docs/lessons/06-17-frame-graph.html.

Same pipeline as build_616.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l617_body_{a,b,c}.html, figures from scratch/figs_617.py, and
every code listing read from a PINNED COPY once 6.18's session pins them — see
LISTING_SOURCE, which is deliberately EMPTY today for the reason 6.6 learned.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-17-frame-graph.html"

FIGURES = {
    "FIG_TODAY": ("l617_fig1.svg", "1",
        "Seventeen render passes, four owners, and the four facts a human keeps in step by hand. "
        "Two of the owners begin their own attachments and two expect somebody else to &#8212; both "
        "for good reasons, and the result is an API where a call site cannot tell you whether you "
        "are inside a render pass. The four boxes below are the real subject of the lesson: none of "
        "them is checked by a compiler, a validation layer or a test, and each is a "
        "<em>consequence</em> of something a pass already knows. The order lives in whichever "
        "function assembles the frame plus prose in three headers; the load ops include the one "
        "6.13 called &#8220;load-bearing&#8221;; the store ops were argued out across Lessons 4.7 "
        "and 6.8, nine lessons apart; and the fourth is <code>gpu_post.cpp</code>&#8217;s "
        "<code>if (!s.enabled)</code>, which is a dependency written down a second time."),

    "FIG_VERSIONS": ("l617_fig2.svg", "2",
        "The move the whole lesson rests on. Above, &#8220;the HDR target&#8221; is an "
        "<code>SDL_GPUTexture*</code> and the sentence &#8220;the bloom reads the HDR target&#8221; "
        "does not identify a picture &#8212; the image before the skybox and the image after it are "
        "both it. Below, the same resource is a VARIABLE with values over time, and "
        "<code>hdr@1</code> and <code>hdr@2</code> are different names for different images. That "
        "is <strong>single static assignment</strong>, the form every optimising compiler converts "
        "your code into, applied to render targets instead of registers: a value assigned once has "
        "exactly one producer, and once that is true every dataflow question becomes easy."),

    "FIG_DEDUCTION": ("l617_fig3.svg", "3",
        "One sentence, four consequences, and the reason a frame graph is worth building on an API "
        "that cannot alias memory. <code>keep</code> says <em>my output depends on what was already "
        "in this target</em> &#8212; a statement about arithmetic, which a pass author can answer "
        "without knowing what a load op is. From it: the ordering edge (this pass consumes a "
        "version, so it follows its producer), the LOAD op, the PRODUCER&#8217;s store op (somebody "
        "reads that version, so it must reach memory), and the lifetime the pool needs. Note that "
        "the third is a consequence for a <em>different pass</em> than the one that spoke, which is "
        "exactly the kind of fact humans get wrong across a file boundary."),

    "FIG_WORKED": ("l617_fig4.svg", "4",
        "The whole compiler, on three passes, with every answer checkable by reading. "
        "<code>out</code> is IMPORTED &#8212; somebody outside owns it &#8212; which makes it the "
        "only value observable after the frame ends and therefore the cull root. <code>combine</code> "
        "writes it, so it lives; <code>blur</code> produced a version <code>combine</code> reads, so "
        "it lives; <code>unused</code> produces <code>tmp@2</code> and nothing reads "
        "<code>tmp@2</code>, so it does not run. The faint edge matters: <code>unused</code> has a "
        "real dependency on <code>blur</code>, and an edge that leads nowhere is still an edge that "
        "leads nowhere. Three declared passes, two render passes issued."),

    "FIG_MEMORY": ("l617_fig5.svg", "5",
        "Every transient&#8217;s lifetime across the fourteen scheduled passes, and the measurement "
        "that reframes the subject. Read the bars first: <code>hdr</code> spans the whole schedule "
        "because the resolve samples it last, and all six pyramid levels are live at once at the "
        "turn between the down chain and the up chain &#8212; which is what an additive pyramid "
        "MEANS, since every level must survive until the upsample that adds it back. Then the three "
        "totals. Perfect memory aliasing would reach <strong>85.7%</strong> of the unshared figure; "
        "what SDL_GPU 3.4.12 can actually express reaches <strong>100%</strong>, which is to say it "
        "saves <strong>nothing at all</strong>. The prize is 174,720 bytes here and "
        "<strong>5.27 MB</strong> at 1920&times;1080 &#8212; which, worked out, is exactly the bloom "
        "pyramid, sitting in the shadow map&#8217;s memory after the shadow map dies, if only the "
        "API had a call for it."),

    "FIG_SHAPE": ("l617_fig6.svg", "6",
        "Why the saving is structurally small rather than accidentally small. Our frame is a "
        "CHAIN: every stage&#8217;s output is the next stage&#8217;s input, so almost nothing is "
        "dead while anything else is alive. A frame that pays is a TREE &#8212; branches computed "
        "and consumed one after another, each with scratch targets it finishes with. The right-hand "
        "graph is built and measured in &#167;7.3: two half-resolution effects land on ONE pooled "
        "texture and 131,072 bytes come back. This is the shape a post stack takes once SSAO, "
        "reflections and depth of field arrive, which is when the pool stops being a dozen lines "
        "that save nothing."),

    "FIG_GOLDEN": ("l617_fig7.svg", "7",
        "The only proof that matters, and the control that makes it mean something. On the left, "
        "the frame assembled the way Lesson 6.13&#8217;s harness assembled it: four explicit "
        "render-pass boundaries, eleven more hidden inside <code>bloom.render</code>, every load and "
        "store op chosen by hand. On the right, four calls. Both produce the same 262,144 channels, "
        "<strong>zero differing</strong>. And because one lesson ago a golden was found that printed "
        "<code>identical=YES</code> when BOTH of its file reads failed, the same comparison is "
        "pointed at a bloom-less frame and reports <strong>196,608 differing</strong> &#8212; three "
        "quarters of the channels, which is every pixel&#8217;s RGB. The instrument can fail, so the "
        "zero is evidence."),
}

LISTING_META = {
    "engine/include/engine/gfx/frame_graph.hpp": ("new", "new"),
    "engine/src/gfx/frame_graph.cpp": ("new", "new"),
    "engine/include/engine/gfx/gpu_shadow.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_shadow.cpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_post.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_post.cpp": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "scratch/verify_617.cpp": ("new", "new"),
}

LISTING_LANG = {
    "engine/CMakeLists.txt": ("cmake", "CMake"),
}

LISTING_SOURCE = {
    # PINNED at the start of 6.18's session, from 422d414 (the commit that shipped
    # this page) except `verify_617.cpp`, which is gitignored and was taken from the
    # working tree — its only provenance. All eight verified by substring against
    # the shipped page by pin_listings.py before being written here.
    "engine/include/engine/gfx/frame_graph.hpp": "scratch/l617_engine_include_engine_gfx_frame_graph.hpp",
    "engine/src/gfx/frame_graph.cpp":            "scratch/l617_engine_src_gfx_frame_graph.cpp",
    "engine/include/engine/gfx/gpu_shadow.hpp":  "scratch/l617_engine_include_engine_gfx_gpu_shadow.hpp",
    "engine/src/gfx/gpu_shadow.cpp":             "scratch/l617_engine_src_gfx_gpu_shadow.cpp",
    "engine/include/engine/gfx/gpu_post.hpp":    "scratch/l617_engine_include_engine_gfx_gpu_post.hpp",
    "engine/src/gfx/gpu_post.cpp":               "scratch/l617_engine_src_gfx_gpu_post.cpp",
    "engine/CMakeLists.txt":                     "scratch/l617_engine_CMakeLists.txt",
    "scratch/verify_617.cpp":                    "scratch/l617_scratch_verify_617.cpp",
}

# PINNED — 2026-09-12, at the start of Lesson 6.18's session, exactly as the note
# that stood here demanded. `pin_listings.py 617` reported all eight listings
# verbatim in the shipped page, and re-running this file reproduced
# 06-17-frame-graph.html byte for byte before the nav retarget below.
#
# 6.18 (Text and 2D Overlay Rendering) then went on to edit `frame_graph.hpp/.cpp`
# and `gpu_post.hpp/.cpp` — the first two because the overlay is the API's first
# EXTERNAL user, the second pair because the overlay composites after the resolve.
# Without these pins that work would have leaked backwards onto this page, which
# is the precise failure 05-02 and 05-04 are still carrying.

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
<title>6.17 — A Lightweight Frame Graph · Build a Professional 3D Game Engine</title>
<meta name="description" content="The engine can record seventeen render passes in a frame and there is no single place where a frame happens: three types begin their own passes, two are handed one, and the rules keeping them in order live in prose in three headers, checked by nothing. This lesson makes the frame a DECLARATION. The move it rests on is refusing to let a name mean a texture - a resource is a VARIABLE with values over time, hdr@1 is what the scene pass produced and hdr@2 is what the skybox produced from it - which is single static assignment applied to render targets. From one statement per pass, four facts are then derived rather than maintained: the ordering, the load op, the PRODUCER's store op, and the lifetime. The load op is the sharp case, because 6.13 wrote a paragraph warning what happens when you get one wrong and called the symptom a tuning problem; here it is not a decision. Every derived op matches what the engine chose by hand across eight attachments, including 4.7's DONT_CARE on the scene depth and 6.8's STORE on the shadow map, argued nine lessons apart. Culling is backward reachability from imported writes, so eleven bloom passes disappear with no flag anywhere. And then the honest part: the usual sales pitch is memory aliasing, and measured on this frame it saves ZERO - because SDL_GPU 3.4.12 has no placed resource or heap, and because a chain has almost nothing to alias. Perfect aliasing would reach 85.7 per cent, an unreachable 5.27 MB at 1920x1080 which turns out to be exactly the bloom pyramid. The pool is built anyway and shown working on a frame shaped for it. A compile costs 8.62 microseconds and zero heap allocations, measured with a replaced global operator new. Five silent mistakes become named compile errors. The compiled frame is bit-identical across 262,144 channels, with a control proving the comparison can report a difference.">

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
    <a class="prev-l" href="06-16-frustum-culling.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.16 — Frustum Culling and Instanced Submission</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-18-text-overlay.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.18 — Text and 2D Overlay Rendering</span>
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
        with open(f"scratch/l617_body_{name}.html") as fh:
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
