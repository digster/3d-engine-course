#!/usr/bin/env python3
"""Assemble docs/lessons/06-06-gltf.html.

Same pipeline as build_65.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l66_body_{a,b,c}.html, figures from scratch/figs_66.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-06-gltf.html"

FIGURES = {
    "FIG_FORMATS": ("l66_fig1.svg", "1",
        "The structural difference, and it is not that one format is harder. An OBJ file is a bag "
        "of triangles with no placement, no hierarchy and no way to say that this run of faces is "
        "the windscreen &#8212; and its one gesture towards materials, <code>usemtl</code>, names a "
        "string the format <strong>cannot itself define</strong>. A glTF file is a scene: a node "
        "tree with transforms, meshes split into primitives because one object usually needs more "
        "than one material, and a material table the file defines in physical units. Loading one "
        "does not produce a mesh; it produces several objects, in several places, wearing several "
        "materials. That is why <code>load_model</code> returns vectors and <code>load_mesh</code> "
        "returns a handle."),

    "FIG_ACCESSOR": ("l66_fig2.svg", "2",
        "Why this is the lesson where the course takes a library, with the numbers from "
        "<code>assets/cube.gltf</code>. Reading eight positions means walking accessor to buffer "
        "view to buffer &#8212; and the buffer may be an external file, a base64 <code>data:</code> "
        "URI, or a chunk glued onto the JSON. <strong>Every field along that chain is a "
        "choice.</strong> Five component types, normalized or not, tight or interleaved at a "
        "stride, dense or sparse: roughly forty legal encodings of the same eight positions, every "
        "one of which some exporter emits. A loader that handles only the cases you happened to "
        "test fails on a customer&#8217;s file rather than on yours, and not one of the forty is "
        "about graphics."),

    "FIG_CONVENTIONS": ("l66_fig3.svg", "3",
        "The part of importing a format that actually goes wrong, checked before a line of "
        "conversion code was written. Handedness, winding and texture origin all AGREE, so the "
        "loader contains no basis change, no index reversal and no <code>1 - v</code> &#8212; and "
        "that is Lesson 2.6 having argued the choice rather than picking one: FBX is Z-up, Unity "
        "and Unreal are left-handed, and any of those would have cost a mirroring basis change "
        "here, which reverses winding, which needs a second correction. The fourth row differs and "
        "is <strong>not a conversion</strong>: an asset&#8217;s front facing +Z against a camera "
        "looking down &#8722;Z is an authoring fact, fixed with a yaw. Note the third row&#8217;s "
        "trap &#8212; OBJ disagrees where glTF agrees, so <code>flip_uv_v</code> is correct for "
        "every mesh this engine has ever loaded and wrong for every one it loads next."),

    "FIG_BRDF": ("l66_fig4.svg", "4",
        "The claim Lesson 6.4 carried as &#9888; VERIFY, discharged term by term against Khronos "
        "glTF 2.0 Appendix B. <strong>Five of the six terms are identical</strong>, and none of "
        "those agreements is a coincidence &#8212; both models derive from the same microfacet "
        "theory with the same Disney remap and the same IOR of 1.5. The fifth row is the "
        "surprising one: the spec lerps two whole BRDFs by <code>metallic</code> and this engine "
        "lerps the F0, and those commute EXACTLY because Schlick is affine in "
        "<em>f</em><sub>0</sub> &#8212; measured at 1.19e&#8722;07 over 4,851 points, which is "
        "float rounding. Only the diffuse coupling differs: one Fresnel crossing against two. The "
        "consequence is invisible at normal incidence (1.036&#215;) and unmissable at a silhouette "
        "(5.62&#215;), which is exactly why it survived two lessons unverified."),

    "FIG_LAYERS": ("l66_fig5.svg", "5",
        "Where a texture reference stops being a string, and the answer decides whether this code "
        "still works in Module 9. The tempting shortcut is to hand <code>parse_gltf</code> an "
        "<code>asset_store&amp;</code> and get finished materials back &#8212; fewer types, fewer "
        "lines, and it costs three things. The parser could no longer be tested without a "
        "filesystem (3.5 split <code>parse_obj</code> from <code>load_obj</code> for exactly this "
        "and used the split to test a dozen malformed inputs from string literals). Module "
        "9&#8217;s offline asset cooker could not use it, because a handle is meaningless outside "
        "the pool that issued it and therefore cannot be serialised. And &#8220;is this the same "
        "image?&#8221; would have a second answer, in a second place, that will eventually "
        "disagree with the first."),

    "FIG_RESULT": ("l66_fig6.svg", "6",
        "What <code>gltf_view --model cube.gltf</code> should put on screen, and three assertions "
        "in one image. <strong>The arrow points up</strong>, so <code>v</code> was not flipped "
        "&#8212; and note that a checkerboard could not have told you, being symmetric under every "
        "flip, which is why <code>make_uv_grid</code> exists (3.9). <strong>There is an image at "
        "all</strong>, so the <code>.gltf</code>&#8217;s <code>&quot;uri&quot;</code> resolved "
        "against the asset store&#8217;s own search path, decoded, and survived the RGBA-to-ARGB "
        "channel shuffle. And <strong>the highlight has the file&#8217;s shape</strong>, because "
        "<code>roughnessFactor: 0.4</code> went into <code>microsurface::roughness</code> "
        "untouched. Nothing in the demo types a colour. An SVG mock; the real frame is a PPM."),
}

LISTING_META = {
    "engine/include/engine/gfx/gltf.hpp":              ("new", "new"),
    "engine/src/gfx/gltf.cpp":                         ("new", "new"),
    "engine/include/engine/asset/asset_store.hpp":     ("modified", "modified"),
    "engine/src/asset/asset_store.cpp":                ("modified", "modified"),
    "demos/gltf_view/main.cpp":                        ("new", "new"),
    "scratch/make_gltf_assets.py":                     ("new", "new"),
    "scratch/verify_66.cpp":                           ("new", "new"),
}

LISTING_LANG = {
    "scratch/make_gltf_assets.py": ("python", "Python"),
}

# NOTHING PINNED YET. Five lessons running, the following lesson has pinned the
# previous one's sources before writing a line, and five times the rebuild diff
# has come out to exactly the nav lines that were meant to move.
#
# PIN EVERY FILE THIS PAGE LISTS THAT A LATER LESSON TOUCHES. Lesson 6.7 is
# normal mapping, and it needs the thing this lesson deferred: a texture that
# knows whether it holds colour or linear data. So it will edit `texture.hpp`
# (not listed here) and, much more to the point, `gltf.hpp` and `gltf.cpp` — the
# `wants_normal_texture` flag becomes a real load — and `asset_store.{hpp,cpp}`,
# which grows the colour-space argument on `load_texture`. All four are listed
# WHOLE below.
#
#     git show <6.6 commit>:<path> > scratch/l66_<name>
#     git show <6.6 commit>:<path> | diff - scratch/l66_<name>
#
# And take `scratch/verify_66.cpp` and `scratch/make_gltf_assets.py` EARLY —
# both are gitignored, so their only provenance is a working-tree copy, which
# cannot be recovered afterwards.
# EXTENDED 2026-09-12 to cover EVERY listing, not just the files this
# lesson wrote. The partial dict below pinned what its author knew would
# move; everything else stayed live, so later lessons' edits leaked into
# this page anyway — which is why it no longer rebuilt to what it shipped.
# Pinned from 2c787a2, verified per file against the shipped page
# with scratch/which_commit.py.
#
# The original dict, kept for its provenance:
#   LISTING_SOURCE = {
#       # PINNED at the start of the 6.7 session, before a line of normal mapping was
#       # written. Verified byte-identical against the 6.6 commit (2c787a2) with
#       # `git show 2c787a2:<path> | diff - <pin>`; verify_66.cpp and
#       # make_gltf_assets.py are gitignored, so their pins are working-tree copies
#       # taken before any edit.
#       #
#       # (What was here before this line was written: 6.5's three pins, inherited
#       # verbatim when this file was copied from build_65.py and INERT, because none
#       # of those paths appears in this page's LISTING_META. Harmless — and worth
#       # catching, because a stale pin dict makes the discipline LOOK satisfied.
#       # When copying build_NN.py, empty this dict as well as FIGURES.)
#       "engine/include/engine/gfx/gltf.hpp":          "scratch/l66_gltf.hpp",
#       "engine/src/gfx/gltf.cpp":                     "scratch/l66_gltf.cpp",
#       "engine/include/engine/asset/asset_store.hpp": "scratch/l66_asset_store.hpp",
#       "engine/src/asset/asset_store.cpp":            "scratch/l66_asset_store.cpp",
#       "scratch/verify_66.cpp":                       "scratch/l66_verify_66.cpp",
#       "scratch/make_gltf_assets.py":                 "scratch/l66_make_gltf_assets.py",
#   }
LISTING_SOURCE = {
    "engine/include/engine/gfx/gltf.hpp":          "scratch/l66_engine_include_engine_gfx_gltf.hpp",
    "engine/src/gfx/gltf.cpp":                     "scratch/l66_engine_src_gfx_gltf.cpp",
    "engine/include/engine/asset/asset_store.hpp": "scratch/l66_engine_include_engine_asset_asset_store.hpp",
    "engine/src/asset/asset_store.cpp":            "scratch/l66_engine_src_asset_asset_store.cpp",
    "demos/gltf_view/main.cpp":                    "scratch/l66_demos_gltf_view_main.cpp",
    "scratch/make_gltf_assets.py":                 "scratch/l66_scratch_make_gltf_assets.py",
    "scratch/verify_66.cpp":                       "scratch/l66_scratch_verify_66.cpp",
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
<title>6.6 — glTF 2.0 Loading · Build a Professional 3D Game Engine</title>
<meta name="description" content="Lesson 3.5 wrote an OBJ parser by hand and this lesson takes a library, and the interesting part is why the two answers differ: the test is not whether a problem is hard but whether the hard part is the subject. Builds a glTF 2.0 loader over cgltf - text and binary, node hierarchies, multi-primitive meshes, external and embedded buffers - and finds that three of the four convention checks against this engine come out as nothing to do. Discharges the VERIFY carried since 6.4 by comparing the engine BRDF against Khronos Appendix B term by term: five terms identical, the metallic blend algebraically the same because Schlick is affine in F0, and exactly one factor different. Textures and materials become cached assets with a derivation edge, and the reference render stays byte-identical.">

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
    <a class="prev-l" href="06-05-material-system.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.5 — A Material System</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-07-normal-mapping.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.7 — Normal Mapping and the TBN Derivation</span>
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
        with open(f"scratch/l66_body_{name}.html") as fh:
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
