#!/usr/bin/env python3
"""Assemble docs/lessons/03-05-obj-loader.html from the template + authored body.

Code listings and implementation excerpts are read from the REAL source files, so
the page cannot drift from the repository. Run from the repo root.
"""
import html
import pathlib
import re
import sys

sys.path.insert(0, 'scratch')
from l35_body1 import BODY1          # noqa: E402
from l35_body2 import BODY2          # noqa: E402
from l35_body3 import BODY3          # noqa: E402

ROOT = pathlib.Path('.')
TPL = (ROOT / 'docs/_template/lesson-template.html').read_text(encoding='utf-8')


def region(name, text=TPL):
    m = re.search(r'<!-- %s:BEGIN -->(.*?)<!-- %s:END -->' % (name, name), text, re.S)
    if not m:
        raise SystemExit('missing region ' + name)
    return m.group(1)


SHARED_CSS = region('SHARED-CSS')
SHARED_SCRIPT = region('SHARED-SCRIPT')


def read(path):
    return (ROOT / path).read_text(encoding='utf-8').replace('\r\n', '\n')


def grab(path, start_sub, end_sub='};', extra=0, dedent=True):
    """Lines from the first containing start_sub through the first later one
    containing end_sub, plus `extra` more lines."""
    lines = read(path).split('\n')
    try:
        i = next(k for k, ln in enumerate(lines) if start_sub in ln)
    except StopIteration:
        raise SystemExit('excerpt start not found in %s: %r' % (path, start_sub))
    try:
        j = next(k for k in range(i, len(lines)) if end_sub in lines[k])
    except StopIteration:
        raise SystemExit('excerpt end not found in %s: %r' % (path, end_sub))
    chunk = lines[i:j + 1 + extra]
    if dedent:
        pads = [len(ln) - len(ln.lstrip()) for ln in chunk if ln.strip()]
        cut = min(pads) if pads else 0
        chunk = [ln[cut:] if len(ln) >= cut else ln for ln in chunk]
    return '\n'.join(chunk)


LANG = {'.hpp': 'cpp', '.cpp': 'cpp', '.txt': 'cmake', '.obj': 'cpp', '': 'bash'}


def listing(caption, code, tag=None, lang='cpp', shell=False):
    cls = 'listing shell' if shell else 'listing'
    tag_html = ('<span class="tag %s">%s</span>' % (tag, tag)) if tag else ''
    label = {'cpp': 'C++', 'cmake': 'CMake', 'bash': 'shell'}.get(lang, lang)
    return (
        '<figure class="%s">\n'
        '    <figcaption>\n'
        '      <span class="path">%s</span>\n'
        '      %s\n'
        '      <span class="lang" data-lang="%s">%s</span>\n'
        '    </figcaption>\n'
        '    <pre><code class="lang-%s">%s</code></pre>\n'
        '  </figure>' % (cls, html.escape(caption), tag_html, lang, label, lang,
                         html.escape(code.rstrip('\n')))
    )


def file_listing(path, tag, caption=None, lang=None):
    p = pathlib.Path(path)
    lang = lang or LANG.get(p.suffix, 'cpp')
    return listing(caption or path, read(path), tag=tag, lang=lang)


# ---- implementation excerpts ------------------------------------------------
EXC = {
    'mesh_data': listing(
        'src/gfx/mesh.hpp — the owning half of the pair', 'modified',
        code=None) if False else listing(
        'src/gfx/mesh.hpp — the owning half of the pair',
        grab('src/gfx/mesh.hpp', '/// Geometry that OWNS its arrays'), 'modified'),

    'to_float': listing(
        'src/gfx/obj.cpp — the whole token, or nothing',
        grab('src/gfx/obj.cpp', '/// Parse a float, strictly', '}'), 'new'),

    'parse_corner': listing(
        'src/gfx/obj.cpp — v, v/vt, v//vn, v/vt/vn',
        grab('src/gfx/obj.cpp', '/// Split `v`, `v/vt`', '}'), 'new'),

    'resolve_index': listing(
        'src/gfx/obj.cpp — 1-based, and negative means relative',
        grab('src/gfx/obj.cpp', '/// Turn a raw OBJ index', '}'), 'new'),

    'corner_key': listing(
        'src/gfx/obj.cpp — what a vertex actually is',
        grab('src/gfx/obj.cpp', '/// A resolved corner: three 0-based indices'), 'new'),

    'corner_hash': listing(
        'src/gfx/obj.cpp — hashing a user-defined key',
        grab('src/gfx/obj.cpp', '/// FNV-1a over the three indices'), 'new'),

    'unify': listing(
        'src/gfx/obj.cpp — the heart of the loader',
        grab('src/gfx/obj.cpp', '// ---- THE INDEX PROBLEM',
             'face_corners.push_back(index);'), 'new'),

    'obj_status': listing(
        'src/gfx/obj.hpp — how a load can end',
        grab('src/gfx/obj.hpp', 'enum class obj_status'), 'new'),

    'obj_report': listing(
        'src/gfx/obj.hpp — the counts you reach for when a model looks wrong',
        grab('src/gfx/obj.hpp', '/// Everything the loader learned'), 'new'),

    'ceiling': listing(
        'src/gfx/mesh.hpp — the index-space ceiling',
        grab('src/gfx/mesh.hpp', '/// **The index-space ceiling.**',
             'k_max_mesh_vertices = 65536;'), 'modified'),

    'load_obj': listing(
        'src/gfx/obj.cpp — bytes off a disk',
        grab('src/gfx/obj.cpp', 'obj_report load_obj(const char* path', '}'), 'new'),

    'asset_path': listing(
        'src/gfx/obj.cpp — beside the binary, not beside the shell',
        grab('src/gfx/obj.cpp', 'std::string asset_path(const char* relative)', '}'), 'new'),

    'cmake_assets': listing(
        'CMakeLists.txt — put the assets where the program will look',
        grab('CMakeLists.txt', '# ---- Assets ---', 'VERBATIM)'), 'modified', lang='cmake'),

    'gitignore': listing(
        '.gitignore — one negation, and a real bug avoided',
        grab('.gitignore', "# `*.obj` is MSVC's", '!assets/**/*.obj'), 'modified',
        lang='bash'),

    'save_precision': listing(
        'src/gfx/obj.cpp — nine digits, because nine is the number',
        grab('src/gfx/obj.cpp', '// "%.9g" — NINE significant digits',
             'static_cast<double>(p.z)));'), 'new'),

    'edge_use': listing(
        'src/gfx/mesh.cpp — the whole topology test, in two counters',
        grab('src/gfx/mesh.cpp', '/// How many times an undirected edge'), 'new'),

    'key_of': listing(
        'src/gfx/mesh.cpp — welding by exact bits, minus the sign of zero',
        grab('src/gfx/mesh.cpp', '[[nodiscard]] position_key key_of(vec3 p)', '}'), 'new'),

    'volume': listing(
        'src/gfx/mesh.cpp — the divergence theorem, in one accumulation',
        grab('src/gfx/mesh.cpp', '// The signed volume of the tetrahedron',
             'volume6 += static_cast<double>(dot(a, cross(b, c)));'), 'new'),

    'scene_model': listing(
        'src/main.cpp — one object, and nothing about it was typed here',
        grab('src/main.cpp', 'if (kind == scene_kind::model)', 'return 1;', extra=1),
        'modified'),

    'roundtrip': listing(
        'src/main.cpp — the round trip, measured every frame',
        grab('src/main.cpp', "// ---- Lesson 3.5's own comparison",
             'roundtrip_wrong = count_differences(fb, scratch_fb, vp);', extra=1),
        'modified'),
}

# ---- full listings ----------------------------------------------------------
FULL = '\n\n  '.join([
    file_listing('src/gfx/obj.hpp', 'new'),
    file_listing('src/gfx/obj.cpp', 'new'),
    file_listing('src/gfx/mesh.hpp', 'modified'),
    file_listing('src/gfx/mesh.cpp', 'new'),
    listing('assets/cube.obj', read('assets/cube.obj'), 'new', lang='cpp'),
    listing('assets/twisted.obj', read('assets/twisted.obj'), 'new', lang='cpp'),
    listing('assets/quirks.obj  (shipped with CRLF line endings, shown here without)',
            read('assets/quirks.obj'), 'new', lang='cpp'),
    file_listing('CMakeLists.txt', 'modified', lang='cmake'),
    file_listing('.gitignore', 'modified', lang='bash'),
    file_listing('src/main.cpp', 'modified'),
])

STATE = """course: Build a Professional 3D Game Engine (SDL3 + C++20)
version: 1.0

conventions:
  world: right-handed, Y-up, -Z forward
  clip: left-handed, +Y up, z in [0,1] (SDL_GPU; projection absorbs the flip)
  matrices: column vectors, v' = M*v, column-major storage
  winding: CCW = front, cull back; front-facing is edge_function &lt; 0 in screen space
  units: 1 unit = 1 metre; radians internally
  axis colours: x/y/z = red/green/blue
  assets: OBJ is 1-based, negative = relative; vt stored as written (flip decided in 3.9)

completed:
  - 0.1 What a Game Engine Is … 0.6 Headers and the Debugger
  - 1.1 Events … 1.8 Pong (Module 1 checkpoint)
  - 2.1 Lines … 2.12 A Spinning Wireframe Mesh (Module 2 complete)
  - 3.1 The Painter's Problem and the Z-Buffer
  - 3.2 Perspective-Correct Interpolation
  - 3.3 Near-Plane Clipping
  - 3.4 Back-Face Culling
  - 3.5 A Hand-Rolled OBJ Loader

capabilities:
  - fixed-timestep loop with render interpolation; input state and events
  - CPU framebuffer via an SDL3 streaming texture; sRGB-aware colour
  - hand-built vec2/3/4, mat2/3/4, transforms, look-at, perspective, viewport
  - triangle raster with edge functions, fill rule, perspective-correct attributes
  - z-buffer (f32 / unorm24 / unorm16), near-plane clipping, back-face culling
  - OBJ loading and saving; owning mesh_data; mesh validation (euler, boundary,
    winding, signed volume); procedural torus

files:
  /: CLAUDE.md, README.md, ARCHITECTURE.md, LEARNINGS.md, PROMPT.md, LICENSE,
     .gitignore, CMakeLists.txt, STATE.md
  src/: main.cpp
  src/core/: input.hpp/.cpp, clock.hpp/.cpp, fixed_step.hpp/.cpp
  src/gfx/: clip.hpp/.cpp, colour.hpp/.cpp, depth_buffer.hpp/.cpp,
            framebuffer.hpp/.cpp, mesh.hpp/.cpp, obj.hpp/.cpp,
            raster.hpp/.cpp, viewport.hpp
  src/math/: vec2.hpp, vec3.hpp, vec4.hpp, mat2.hpp, mat3.hpp, mat4.hpp, transform.hpp
  src/game/: pong.hpp/.cpp
  assets/: cube.obj, twisted.obj, quirks.obj, torus.obj
  docs/: index.html, conventions.html, math-toolbox.html, cpp-style.html
  docs/lessons/: 00-01 … 03-05 (31 lessons)

next: 3.6 — Normals and Lambert's Cosine Law"""

NAV = """  <nav class="lesson-nav" aria-label="Lesson navigation (%s)">
    <a class="prev-l" href="03-04-back-face-culling.html">
      <span class="dir">← Previous</span>
      <span class="ttl">3.4 — Back-Face Culling</span>
    </a>
    <a class="idx-l" href="../index.html">
      <span class="dir">Index</span>
      <span class="ttl">All lessons</span>
    </a>
    <a class="next-l" href="03-06-normals-and-lambert.html">
      <span class="dir">Next →</span>
      <span class="ttl">3.6 — Normals and Lambert's Cosine Law</span>
    </a>
  </nav>"""

body = BODY1 + BODY2 + BODY3
for key, value in EXC.items():
    marker = '<!--EXC:%s-->' % key
    if marker not in body:
        raise SystemExit('unused excerpt: ' + key)
    body = body.replace(marker, value)
left = re.findall(r'<!--EXC:(\w+)-->', body)
if left:
    raise SystemExit('unfilled excerpt markers: %s' % left)
body = body.replace('<!--LISTINGS-->', FULL)

PAGE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>3.5 — A Hand-Rolled OBJ Loader · Build a Professional 3D Game Engine</title>
<meta name="description" content="Reading Wavefront OBJ from scratch: 1-based and negative indices, the index problem that turns 8 positions into 24 vertices, n-gon triangulation, and a validator that measures whether loaded geometry is safe to draw.">

<!-- ==========================================================================
     SHARED COURSE STYLESHEET  —  v1.0
     ==========================================================================
     This block is IDENTICAL in every lesson file. It is duplicated rather than
     linked because each lesson must be a fully self-contained document that
     renders from a bare filesystem with no network and no build step.

     Source of truth: docs/_template/lesson-template.html
     ========================================================================== -->
<!-- SHARED-CSS:BEGIN -->%s<!-- SHARED-CSS:END -->

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

%s
%s
  <details class="state">
    <summary>STATE — resume key</summary>
<pre>%s</pre>
  </details>

%s

  <footer class="foot">
    <p>
      Build a Professional 3D Game Engine ·
      <a href="../index.html">Contents</a> ·
      <a href="../conventions.html">Conventions</a> ·
      <a href="../math-toolbox.html">Math Toolbox</a> ·
      <a href="../cpp-style.html">C++ Style</a>
    </p>
    <p>MIT licensed. Copyright © 2026 digster.</p>
  </footer>
</div>

<!-- ==========================================================================
     PAGE SCRIPTS
     ========================================================================== -->
<!-- SHARED-SCRIPT:BEGIN -->%s<!-- SHARED-SCRIPT:END -->
</body>
</html>
""" % (SHARED_CSS, NAV % 'top', body, STATE, NAV % 'bottom', SHARED_SCRIPT)

out = ROOT / 'docs/lessons/03-05-obj-loader.html'
out.write_text(PAGE, encoding='utf-8')
print('wrote %s  (%.0f KB)' % (out, len(PAGE) / 1024))

# quick sanity checks
eqs = PAGE.count('class="eq"')
plain = PAGE.count('eq-plain')
print('  equations: %d  (.eq-plain twins: %d)' % (eqs, plain))
print('  figures:   %d' % PAGE.count('<figure class="dia'))
print('  listings:  %d' % PAGE.count('<figure class="listing'))
print('  pitfalls:  %d' % PAGE.count('class="pitfall-item"'))
print('  exercises: %d' % PAGE.count('class="exercise"'))
text_only = re.sub(r'<pre>.*?</pre>', ' ', PAGE, flags=re.S)
text_only = re.sub(r'<script.*?</script>', ' ', text_only, flags=re.S)
text_only = re.sub(r'<style.*?</style>', ' ', text_only, flags=re.S)
text_only = re.sub(r'<svg.*?</svg>', ' ', text_only, flags=re.S)
text_only = re.sub(r'<[^>]+>', ' ', text_only)
print('  words (excluding code, svg, script): %d'
      % len(html.unescape(text_only).split()))
