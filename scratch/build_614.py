#!/usr/bin/env python3
"""Assemble docs/lessons/06-14-antialiasing.html.

Same pipeline as build_613.py. No STATE block: STATE.md is the sole resume key and
lesson pages end at Further Reading.

Prose from scratch/l614_body_{a,b,c}.html, figures from scratch/figs_614.py, and code
listings read straight out of the repository so they cannot drift from what
actually compiles.

Figure placeholders are named by CONTENT; the number in each tuple is the one the
reader sees and follows PAGE order.
"""
import os
import re

OUT = "docs/lessons/06-14-antialiasing.html"

FIGURES = {
    "FIG_TWO": ("l614_fig1.svg", "1",
        "Two problems that share a name, and they are not variations of one thing. LEFT, GEOMETRIC: "
        "coverage is a STEP FUNCTION, whose Fourier transform has energy at every frequency &#8212; "
        "so no sample rate is sufficient, and supersampling improves the answer without ever making "
        "it right. RIGHT, SHADING: the specular lobe is NARROWER than the pixel hunting for it, so "
        "the highlight lands between sample points and flashes as the camera pans. MSAA fixes the "
        "left one and cannot touch the right one, because it multisamples COVERAGE and runs the "
        "fragment shader ONCE per primitive per pixel &#8212; the asymmetry that makes 4x cost about "
        "1.3x rather than 4x. You cannot buy shading samples with a coverage feature."),

    "FIG_NYQUIST": ("l614_fig2.svg", "2",
        "What supersampling achieves, on a signal it cannot possibly resolve. A checker of period "
        "0.37 px is far above Nyquist; its true local mean is exactly 0.5 whatever the phase, which "
        "is what makes this a ground truth rather than another rendering. Per-pixel error falls from "
        "0.5000 &#8212; as wrong as it is possible to be, every pixel fully black or fully white "
        "&#8212; to 0.0231 at 64 samples. BUT THE CHECKER NEVER COMES BACK, at any factor. What "
        "changes is that the image stops claiming to be something it is not. AND NOTE THE "
        "STATISTIC: the MEAN over the whole image is already correct at 1x, because errors of "
        "opposite sign cancel across pixels whose phases differ &#8212; which is how the first draft "
        "of this measurement passed while measuring nothing at all."),

    "FIG_RESOLVE": ("l614_fig3.svg", "3",
        "Resolving a half-covered pixel, and one mistake making its FOURTH appearance in this course "
        "after Lesson 6.1's shading, 6.10's mip chains and 6.11's compositing. A "
        "<code>framebuffer</code> holds sRGB-encoded bytes, and averaging bytes averages CODES "
        "&#8212; but the transfer function exists precisely because codes are not quantities of "
        "light. Half the light is 0.5 linear, whose sRGB encode is 0.7354, which is code 188. "
        "Averaging the bytes gives 127, a linear 0.2122, delivering 21.2% of the light instead of "
        "50%. The symptom is specific and usually misdiagnosed: antialiased edges come out TOO DARK "
        "and every silhouette grows a thin dark outline, which reads as edges being composited "
        "twice."),

    "FIG_CROSSOVER": ("l614_fig4.svg", "4",
        "Where shading aliasing begins, as a number rather than a feeling. The lobe&#8217;s width "
        "comes from solving the GGX NDF for its half-maximum &#8212; three lines, no fitting, and "
        "exact at every roughness rather than to leading order &#8212; and the coefficient is "
        "sqrt(sqrt(2)-1) = 0.6436, so the familiar &#8220;the lobe is about alpha wide&#8221; "
        "overstates it by 55%. The horizontal line is how far the normal turns across ONE pixel on a "
        "sphere 40 px in radius. Where they cross, at roughness 0.2468, a highlight stops being "
        "reliably sampled and starts falling between sample points. NOTE WHERE THE MODULE&#8217;S "
        "OWN MATERIALS LAND: the demo&#8217;s 0.49 is safe, and every polished metal Lesson 6.12 "
        "introduced is not."),

    "FIG_MSAA": ("l614_fig5.svg", "5",
        "MSAA and SSAA differ in exactly one thing: how often the fragment shader runs. Both "
        "evaluate coverage and depth at four positions; MSAA then shades ONCE per primitive per "
        "pixel and replicates, which is why it costs about 1.3x where SSAA costs 4x. On a pixel "
        "straddling a silhouette that is precisely right and very cheap, because what differs "
        "between the samples is whether they are COVERED. On an interior pixel every sample belongs "
        "to the same primitive, so MSAA&#8217;s answer IS the single-sample answer &#8212; and the "
        "measured gap between the two columns is the shading aliasing, exactly. At roughness 0.50 "
        "they agree to three parts in a thousand; at 0.05 they differ by a factor of 300."),

    "FIG_STABILITY": ("l614_fig6.svg", "6",
        "The measurement this lesson exists for, and it did not come out as expected. Filtering the "
        "NDF improves per-pixel ACCURACY against brute-force ground truth by only 2.4x at roughness "
        "0.05 &#8212; and at 0.30 and 0.20 it makes the error THREE TIMES WORSE. Meanwhile the "
        "SWING, the ratio between the brightest and dimmest value a pixel takes as the camera pans "
        "by less than one pixel, falls from 3,996.5x to 2.0x: a factor of 2,034. ANTIALIASING DOES "
        "NOT MAKE A PIXEL CORRECT, IT MAKES IT STABLE &#8212; which is obvious in hindsight, because "
        "the artefact was never &#8220;this pixel has the wrong value&#8221; but &#8220;this pixel "
        "changes violently when nothing in the scene did&#8221;. Aliasing is not an error in "
        "magnitude; it is an error discontinuous in the parameters."),

    "FIG_FRAME": ("l614_fig7.svg", "7",
        "The shape of an MSAA frame, and why it needs two colour targets where a plain one needs "
        "one: A MULTISAMPLE TEXTURE CANNOT BE SAMPLED AT ALL. You render into it and read from the "
        "single-sample target it resolves into &#8212; which is why "
        "<code>create_colour_target</code> silently drops the SAMPLER usage above 1x rather than "
        "passing on a request the driver cannot honour. STOREOP_RESOLVE rather than "
        "RESOLVE_AND_STORE, because SDL3&#8217;s header says the first lets the driver DISCARD the "
        "multisample memory and is &#8220;the most performant method&#8221;. And the rule from "
        "Lesson 6.13 placed the new target without amendment: the RESOLVED one crosses between "
        "stages so the stack owns it, and the multisample one is an intermediate of the scene pass "
        "&#8212; a design tested by a case it was not written for."),
}

LISTING_META = {
    "engine/include/engine/gfx/antialias.hpp": ("new", "new"),
    "engine/src/gfx/antialias.cpp": ("new", "new"),
    "engine/include/engine/gfx/gpu_post.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_post.cpp": ("modified", "modified"),
    "scratch/verify_614.cpp": ("new", "new"),
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gpu_pipeline.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_scene.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_texture.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/gpu_uniform.hpp": ("modified", "modified"),
    "engine/include/engine/gfx/microfacet.hpp": ("modified", "modified"),
    "engine/src/gfx/gpu_pipeline.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_scene.cpp": ("modified", "modified"),
    "engine/src/gfx/gpu_texture.cpp": ("modified", "modified"),
    "shaders/scene.frag.hlsl": ("modified", "modified"),
    "engine/CMakeLists.txt": ("modified", "modified"),
    "demos/gltf_view/main.cpp": ("modified", "modified"),
}

LISTING_LANG = {
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "shaders/scene.frag.hlsl": ("hlsl", "HLSL"),
    "engine/CMakeLists.txt": ("cmake", "CMake"),
}

LISTING_SOURCE = {
    # PINNED 2026-09-12, at the start of 6.15's session, exactly as the note below
    # instructed. Every path here is read from a FROZEN COPY of what commit 9830dd3
    # shipped, not from the live tree — so 6.15 may create cube maps, prefilter mip
    # chains and edit gpu_post freely, and a rebuild of THIS page still reproduces
    # what the reader was shown.
    #
    # All five were verified by substring against the shipped page before being
    # written here (pin_listings.py's check), which matters most for
    # scratch/verify_614.cpp: it is gitignored, so its only provenance is a
    # working-tree copy, and the page is the only thing that can confirm the copy
    # is still the lesson-era text.
    "engine/include/engine/gfx/antialias.hpp": "scratch/l614_antialias.hpp",
    "engine/src/gfx/antialias.cpp": "scratch/l614_antialias.cpp",
    "engine/include/engine/gfx/gpu_post.hpp": "scratch/l614_gpu_post.hpp",
    "engine/src/gfx/gpu_post.cpp": "scratch/l614_gpu_post.cpp",
    "scratch/verify_614.cpp": "scratch/l614_verify_614.cpp",
    # Added 2026-09-26 (repair_continuity.py): changed by this lesson's commit, never listed whole.
    "engine/include/engine/gfx/gpu_pipeline.hpp": "scratch/l614_engine_include_engine_gfx_gpu_pipeline.hpp",
    "engine/include/engine/gfx/gpu_scene.hpp": "scratch/l614_engine_include_engine_gfx_gpu_scene.hpp",
    "engine/include/engine/gfx/gpu_texture.hpp": "scratch/l614_engine_include_engine_gfx_gpu_texture.hpp",
    "engine/include/engine/gfx/gpu_uniform.hpp": "scratch/l614_engine_include_engine_gfx_gpu_uniform.hpp",
    "engine/include/engine/gfx/microfacet.hpp": "scratch/l614_engine_include_engine_gfx_microfacet.hpp",
    "engine/src/gfx/gpu_pipeline.cpp": "scratch/l614_engine_src_gfx_gpu_pipeline.cpp",
    "engine/src/gfx/gpu_scene.cpp": "scratch/l614_engine_src_gfx_gpu_scene.cpp",
    "engine/src/gfx/gpu_texture.cpp": "scratch/l614_engine_src_gfx_gpu_texture.cpp",
    "shaders/scene.frag.hlsl": "scratch/l614_shaders_scene.frag.hlsl",
    "engine/CMakeLists.txt": "scratch/l614_engine_CMakeLists.txt",
    "demos/gltf_view/main.cpp": "scratch/l614_demos_gltf_view_main.cpp",
}

# NOTHING PINNED YET, and this page lists FIVE files whole.
#
# Lesson 6.15 is SKYBOX AND IMAGE-BASED LIGHTING. A prefiltered environment map is
# a mip chain indexed by roughness, so `microfacet.hpp` (which now owns
# `ggx_lobe_half_angle`, and 6.15 needs it to decide how much sky a level covers)
# and `mipmap.hpp` are both likely to move. `gpu_texture.{hpp,cpp}` is near
# certain — a cube map is a texture TYPE this engine has never created.
#
# `antialias.{hpp,cpp}` should NOT move, which makes them the useful control.
#
# So pin all four repository files before writing a line of 6.15:
#
#     git show <6.14 commit>:<path> > scratch/l614_<name>
#     git show <6.14 commit>:<path> | diff - scratch/l614_<name>     # must be empty
#
# And take `scratch/verify_614.cpp` EARLY — gitignored, so its only provenance is
# a working-tree copy, which cannot be recovered afterwards.


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
<title>6.14 — Antialiasing: Geometric and Shading · Build a Professional 3D Game Engine</title>
<meta name="description" content="Two problems share this name and the cure everyone reaches for fixes exactly one. Geometric aliasing undersamples COVERAGE - a step function whose spectrum has energy at every frequency, so no sample rate resolves it and supersampling only stops the image lying. Shading aliasing undersamples the LIGHTING: a specular lobe narrower than the pixel looking for it. The GGX lobe has a closed form - sin t = alpha sqrt((sqrt2-1)/(1-alpha^2)), exact at every roughness, checked against a bisection of the real NDF to 1.6e-8 - and its coefficient is 0.6436, so the folklore 'about alpha wide' overstates by 55 percent. Comparing that against the normal's variation per pixel gives the crossover: roughness 0.2468 on a 40-pixel sphere, which places the demo's 0.49 safely above and every polished metal from 6.12 below. MSAA cannot reach shading aliasing by construction - it multisamples coverage and shades once per primitive per pixel, and the measured gap between MSAA and SSAA on an interior pixel reaches a factor of 300. The fix is to filter the NDF, and the measurement reframes the subject: it improves accuracy by 2.4x, sometimes makes it worse, and improves STABILITY by 2034x. Antialiasing does not make a pixel correct, it makes it stable. Plus MSAA in SDL_GPU with every field checked against the header, the resolve, and the pipeline count going 13 to 22.">

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
    <a class="prev-l" href="06-13-bloom-post-stack.html">
      <span class="dir">← Previous</span>
      <span class="ttl">6.13 — Bloom and the Post-Processing Stack</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="06-15-skybox-ibl.html">
      <span class="dir">Next →</span>
      <span class="ttl">6.15 — Skybox and Image-Based Lighting</span>
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
        with open(f"scratch/l614_body_{name}.html") as fh:
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
