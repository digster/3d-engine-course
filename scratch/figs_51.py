#!/usr/bin/env python3
"""Compute every SVG figure for Lesson 5.1 from real data.

Every number in every caption comes from scratch/measure_50.log and
scratch/verify_50.log. Nothing here is estimated.

Writes scratch/l51_fig{1..6}.svg.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_45 import (svg, write, f, C_POS, C_NRM, C_UV, C_INST, C_BAD, C_HUB,
                     read_ppm, box_sample, rle_rects)

# ---- Measured inputs --------------------------------------------------------
MAIN_BEFORE, MAIN_AFTER = 7789, 5721
PUBLIC_HEADERS = 37
ENGINE_SOURCES = 24
DEMO_HEADERS, DEMO_SOURCES = 2, 4
LIB_KB = 1201
PARAMS_BEFORE, PARAMS_AFTER = 15, 8
CALL_SITES = 8
DUMMY_OUTS = 7

TOUCH = [                       # (label, seconds, what rebuilds)
    ("demos/sandbox/main.cpp",                    0.38, "one demo"),
    ("engine/src/gfx/raster.cpp",                 0.42, "one object file, then relink"),
    ("engine/include/engine/gfx/gpu_scene.hpp",   0.81, "every TU that includes it"),
    ("engine/include/engine/gfx/raster.hpp",      0.97, "every TU that includes it"),
]
TU_BEFORE, TU_AFTER = 1.30, 0.82

GOLDEN_HASH = "905BF27E"
FRAMES = [                      # (label, hash, painted)
    ("solids",     "0C2ABCDE",  1470),
    ("cycle",      "A60DAAC1",  1766),
    ("intersect",  "481B36C3",  1659),
    ("z-fight",    "941E7023",  1635),
    ("floor",      "C650A746", 39139),
    ("model",      "81727C17",  2258),
    ("near plane", "7A140628", 30253),
]
GOLDEN_BYTES = 1209600
HARNESS_COPIES = 4
UMBRELLA_MS, ONE_HEADER_MS = 262, 215
UMBRELLA_LINES, ONE_HEADER_LINES = 82507, 72942


# =============================================================================
# Figure 1 — one directory, then a wall
# =============================================================================
def fig1():
    W, H = 720, 400
    b = []
    b.append('<text x="20" y="20" class="sm">Before: one executable, so every file '
             'could see every other file</text>')
    b.append('<text x="384" y="20" class="sm">After: a library, and '
             '<tspan class="t-hi">an outside</tspan></text>')

    # ---- before: one box, and a note that everything reaches everything ----
    b.append('<rect x="20" y="34" width="330" height="180" rx="4" class="fill-soft ink-soft" '
             'stroke-width="1"/>')
    b.append('<text x="30" y="50" class="xs mono muted">src/</text>')
    BOXES = [("main.cpp", 34, 62, 120, 26, C_UV),
             ("core/", 34, 100, 84, 22, C_POS),
             ("gfx/", 128, 100, 84, 22, C_POS),
             ("math/", 222, 100, 84, 22, C_POS),
             ("game/pong", 34, 134, 110, 22, C_NRM)]
    for name, x, y, w, h, col in BOXES:
        b.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{col}" '
                 f'fill-opacity="0.22" stroke="{col}" stroke-width="1" rx="2"/>')
        b.append(f'<text x="{x+w/2}" y="{y+h/2+4}" class="xs mono" text-anchor="middle">'
                 f'{name}</text>')
    b.append('<text x="34" y="176" class="xs muted">#include &quot;gfx/raster.hpp&quot;</text>')
    b.append('<text x="34" y="192" class="xs muted">&#8230;from anywhere, to anything.</text>')
    b.append(f'<text x="34" y="208" class="xs t-bad">main.cpp: {MAIN_BEFORE:,} lines</text>')

    # ---- the wall, stopping short of the quoted error ---------------------
    b.append('<line x1="366" y1="30" x2="366" y2="248" class="hi" stroke-width="2" '
             'stroke-dasharray="5 4"/>')

    # ---- after ------------------------------------------------------------
    BX, BW = 384, 286
    b.append(f'<rect x="{BX}" y="34" width="{BW}" height="70" rx="4" fill="{C_NRM}" '
             f'fill-opacity="0.10" stroke="{C_NRM}" stroke-width="1"/>')
    b.append(f'<text x="{BX+10}" y="50" class="xs mono muted">demos/</text>')
    for name, x in (("sandbox", 386), ("hello_cube", 482), ("demo_common", 578)):
        b.append(f'<rect x="{x}" y="58" width="90" height="22" fill="{C_NRM}" '
                 f'fill-opacity="0.26" stroke="{C_NRM}" stroke-width="1" rx="2"/>')
        b.append(f'<text x="{x+45}" y="73" class="xs mono" text-anchor="middle">{name}</text>')
    b.append(f'<text x="{BX+10}" y="122" class="xs muted">'
             'may include only what is below the gate</text>')

    b.append(f'<rect x="{BX}" y="130" width="{BW}" height="46" rx="4" fill="{C_HUB}" '
             f'fill-opacity="0.20" stroke="{C_HUB}" stroke-width="1.4"/>')
    b.append(f'<text x="{BX+BW/2}" y="148" class="xs mono" text-anchor="middle">'
             'engine/include/engine/ &#8212; PUBLIC</text>')
    b.append(f'<text x="{BX+BW/2}" y="166" class="xs muted" text-anchor="middle">'
             f'{PUBLIC_HEADERS} headers; nothing else resolves</text>')

    b.append(f'<rect x="{BX}" y="188" width="{BW}" height="46" rx="4" fill="{C_POS}" '
             f'fill-opacity="0.14" stroke="{C_POS}" stroke-width="1"/>')
    b.append(f'<text x="{BX+BW/2}" y="206" class="xs mono" text-anchor="middle">'
             'engine/src/ &#8212; PRIVATE</text>')
    b.append(f'<text x="{BX+BW/2}" y="224" class="xs muted" text-anchor="middle">'
             f'{ENGINE_SOURCES} sources + stb, reachable only from inside</text>')

    # the two legal arrows, in the right margin so no label can sit on them
    b.append('<line x1="678" y1="106" x2="678" y2="126" class="ink-soft" stroke-width="1.2" '
             'marker-end="url(#e-s)"/>')
    b.append('<line x1="678" y1="178" x2="678" y2="185" class="ink-soft" stroke-width="1.2" '
             'marker-end="url(#e-s)"/>')
    # …and the one that cannot exist, further out still
    b.append(f'<path d="M700 106 L700 184" fill="none" stroke="{C_BAD}" stroke-width="1.4" '
             'stroke-dasharray="3 3"/>')
    b.append(f'<line x1="692" y1="136" x2="708" y2="152" stroke="{C_BAD}" stroke-width="2"/>')
    b.append(f'<line x1="708" y1="136" x2="692" y2="152" stroke="{C_BAD}" stroke-width="2"/>')

    b.append(f'<text x="{BX}" y="252" class="xs t-ok">'
             f'main.cpp: {MAIN_AFTER:,} lines &#8212; '
             f'{round(100*(MAIN_BEFORE-MAIN_AFTER)/MAIN_BEFORE)}% of it was never the demo&#8217;s'
             '</text>')

    # ---- the compiler's answer, quoted ------------------------------------
    b.append('<rect x="20" y="272" width="680" height="74" rx="4" class="fill-soft grid" '
             'stroke-width="1"/>')
    b.append('<text x="32" y="294" class="xs mono">'
             'demos/probe.cpp:1:10: fatal error: &#x27;gfx/raster.hpp&#x27; file not found'
             '</text>')
    b.append('<text x="32" y="312" class="xs mono muted">'
             '#include &quot;gfx/raster.hpp&quot;   // the spelling every file used until today'
             '</text>')
    b.append('<text x="32" y="336" class="xs t-hi">'
             'That message is the boundary. Not a review comment, not a convention '
             '&#8212; a compiler.</text>')

    b.append(f'<text x="20" y="{H-30}" class="xs muted">'
             'Measured: 37/37 public headers also compile ALONE, and a demo reaching for '
             'stb_image.h is refused too.</text>')
    return svg(W, H, "One directory, then a wall",
               "Before: src/ as one directory every file could reach. After: demos/ above a "
               "gate of public headers, engine/src private below it.",
               "\n".join(b), "l51f1")


# =============================================================================
# Figure 2 — fifteen parameters, five kinds
# =============================================================================
def fig2():
    W, H = 720, 478
    b = []
    b.append('<text x="20" y="20" class="sm">'
             '<tspan class="mono">collect_triangles</tspan> before publication: '
             f'{PARAMS_BEFORE} parameters, at {CALL_SITES} call sites</text>')

    KIND = [("out",    C_UV,   "where the answer goes"),
            ("in",     C_POS,  "what to draw"),
            ("camera", C_HUB,  "where from"),
            ("policy", C_INST, "how"),
            ("stats",  C_NRM,  "and tell me about it")]
    PARAMS = [("std::vector<raster_triangle>& out", "out"),
              ("projection_scratch& scratch",        "out"),
              ("const scene_object* objects",        "in"),
              ("int count",                          "in"),
              ("const mat4& view_from_world",        "camera"),
              ("const projector& pr",                "camera"),
              ("trs_order order",                    "policy"),
              ("clip_stats& stats",                  "stats"),
              ("cull_choice cull",                   "policy"),
              ("normal_source nsrc",                 "policy"),
              ("shade_eval eval",                    "policy"),
              ("const lighting& lights",             "in"),
              ("bool correct_normals",               "policy"),
              ("normal_stats* normals_out",          "stats"),
              ("vec3 eye_world",                     "camera"),
              ("specular_model spec_model",          "policy")]
    colour = {k: c for k, c, _ in KIND}

    def esc(t):
        return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

    y = 40
    for text, kind in PARAMS:
        text = esc(text)
        b.append(f'<rect x="20" y="{y}" width="8" height="14" fill="{colour[kind]}"/>')
        b.append(f'<text x="34" y="{y+11}" class="xs mono">{text}</text>')
        b.append(f'<text x="336" y="{y+11}" class="xs muted" text-anchor="end">{kind}</text>')
        y += 19
    b.append(f'<text x="20" y="{y+14}" class="xs muted">'
             'Sixteen rows for fifteen parameters, because <tspan class="mono">objects</tspan> '
             'and <tspan class="mono">count</tspan></text>')
    b.append(f'<text x="20" y="{y+28}" class="xs muted">'
             'are one idea spelled as two, which is its own kind of mistake.</text>')

    b.append('<line x1="360" y1="30" x2="360" y2="432" class="grid" stroke-width="1"/>')

    for i, (k, c, note) in enumerate(KIND):
        yy = 44 + i * 20
        b.append(f'<rect x="380" y="{yy-10}" width="8" height="11" fill="{c}"/>')
        b.append(f'<text x="394" y="{yy}" class="xs">{k} &#8212; '
                 f'<tspan class="muted">{note}</tspan></text>')

    b.append('<text x="380" y="164" class="xs t-hi">'
             'Seven rows are one kind: policy.</text>')
    b.append('<text x="380" y="180" class="xs muted">'
             'Three more are the camera, spelled as two values a</text>')
    b.append('<text x="380" y="194" class="xs muted">'
             'caller must remember to derive together.</text>')
    b.append('<text x="380" y="216" class="xs">'
             'A parameter list is a design nobody was</text>')
    b.append('<text x="380" y="230" class="xs">'
             'ever asked to defend.</text>')

    b.append(f'<text x="380" y="262" class="sm">After: {PARAMS_AFTER} parameters</text>')
    AFTER = [("std::vector<raster_triangle>& out", "out"),
             ("projection_scratch& scratch",        "out"),
             ("std::span<const scene_object>",      "in"),
             ("const camera_view& camera",          "camera"),
             ("const projector& pr",                "camera"),
             ("const lighting& lights",             "in"),
             ("const render_options& opts",         "policy"),
             ("collect_stats* stats = nullptr",     "stats")]
    y = 274
    for text, kind in AFTER:
        text = esc(text)
        b.append(f'<rect x="380" y="{y}" width="8" height="14" fill="{colour[kind]}"/>')
        b.append(f'<text x="394" y="{y+11}" class="xs mono">{text}</text>')
        y += 19

    b.append(f'<text x="20" y="{H-24}" class="xs t-ok">'
             f'{DUMMY_OUTS} throwaway variables deleted along the way: '
             '<tspan class="mono">clip_stats ignored;</tspan> existed only because the '
             'out-parameter was not optional.</text>')
    b.append(f'<text x="20" y="{H-8}" class="xs muted">'
             'Nothing was removed from the pipeline. The seven policy rows became one struct '
             'whose every default is the correct answer.</text>')
    return svg(W, H, "Fifteen parameters, five kinds",
               "The old parameter list colour-coded by kind, showing seven policy parameters "
               "and three camera values, and the eight-parameter signature that replaced it.",
               "\n".join(b), "l51f2")


# =============================================================================
# Figure 3 — dependency direction is a design tool
# =============================================================================
def fig3():
    W, H = 720, 348
    b = []
    b.append('<text x="20" y="20" class="sm">Every arrow points DOWN, and that is the '
             'whole rule</text>')

    # Placed by hand rather than centred, because hello_cube&#8217;s arrow has to reach
    # the engine WITHOUT crossing demo_common&#8217;s box on the way.
    NODES = [("sandbox",            40, 56, 120, C_NRM),
             ("scratch/verify_50", 180, 56, 150, C_NRM),
             ("hello_cube",        360, 56, 120, C_NRM),
             ("demo_common",        80, 124, 200, C_NRM),
             ("engine::engine",    140, 192, 240, C_POS),
             ("SDL3::SDL3",        160, 260, 200, C_HUB)]
    centres = {}
    for name, x, y, w, col in NODES:
        b.append(f'<rect x="{x}" y="{y}" width="{w}" height="34" rx="4" fill="{col}" '
                 f'fill-opacity="0.20" stroke="{col}" stroke-width="1.2"/>')
        b.append(f'<text x="{x+w/2}" y="{y+22}" class="sm mono" text-anchor="middle">'
                 f'{name}</text>')
        centres[name] = (x + w / 2, y)

    def arrow(a, bname):
        (ax, ay), (bx, by) = centres[a], centres[bname]
        b.append(f'<line x1="{f(ax)}" y1="{f(ay+34)}" x2="{f(bx)}" y2="{f(by-4)}" '
                 'class="ink-soft" stroke-width="1.2" marker-end="url(#e-s)"/>')
    arrow("sandbox", "demo_common")
    arrow("scratch/verify_50", "demo_common")
    arrow("hello_cube", "engine::engine")
    arrow("demo_common", "engine::engine")
    arrow("engine::engine", "SDL3::SDL3")

    # stb, stopping at the boundary
    b.append(f'<rect x="520" y="192" width="140" height="34" rx="4" fill="{C_UV}" '
             f'fill-opacity="0.18" stroke="{C_UV}" stroke-width="1" stroke-dasharray="4 3"/>')
    b.append('<text x="590" y="214" class="sm mono" text-anchor="middle">stb_image</text>')
    b.append('<line x1="518" y1="209" x2="384" y2="209" class="ink-soft" stroke-width="1.2" '
             'marker-end="url(#e-s)"/>')
    b.append('<text x="520" y="242" class="xs muted">PRIVATE: it stops here.</text>')
    b.append('<text x="520" y="256" class="xs muted">No demo can see it.</text>')

    # the arrow that must never exist, in the gap between the two rows
    b.append(f'<path d="M124 190 L124 162" fill="none" stroke="{C_BAD}" stroke-width="1.4" '
             'stroke-dasharray="3 3"/>')
    b.append(f'<line x1="116" y1="168" x2="132" y2="184" stroke="{C_BAD}" stroke-width="2"/>')
    b.append(f'<line x1="132" y1="168" x2="116" y2="184" stroke="{C_BAD}" stroke-width="2"/>')
    b.append('<text x="20" y="172" class="xs t-bad">the engine</text>')
    b.append('<text x="20" y="186" class="xs t-bad">needing a demo</text>')

    b.append('<line x1="20" y1="300" x2="700" y2="300" class="grid" stroke-width="1"/>')
    b.append('<text x="20" y="318" class="xs">'
             'CMake enforces it by ORDER: <tspan class="mono">add_subdirectory(engine)</tspan> '
             'comes first, so when demos/ asks for '
             '<tspan class="mono">engine::engine</tspan> the target exists.</text>')
    b.append('<text x="20" y="334" class="xs muted">'
             'Swap the two lines and the configure fails. A build system enforcing an '
             'architecture rule is the cheapest enforcement there is.</text>')
    return svg(W, H, "Dependency direction",
               "A four-level graph: demos above demo_common above engine above SDL3, with "
               "stb_image private to the engine and an upward arrow crossed out.",
               "\n".join(b), "l51f3")


# =============================================================================
# Figure 4 — what a change costs now
# =============================================================================
def fig4():
    W, H = 720, 330
    b = []
    b.append('<text x="20" y="20" class="sm">Touch one file, rebuild, and time it '
             '(best of three)</text>')

    x0, w_max = 250, 340
    top = 1.05
    for i, (label, secs, what) in enumerate(TOUCH):
        y = 44 + i * 46
        public = "include" in label
        col = C_POS if public else C_NRM
        w = w_max * secs / top
        b.append(f'<text x="{x0-10}" y="{y+15}" class="xs mono" text-anchor="end">{label}</text>')
        b.append(f'<rect x="{x0}" y="{y}" width="{f(w)}" height="20" fill="{col}" '
                 f'fill-opacity="0.30" stroke="{col}" stroke-width="1"/>')
        b.append(f'<text x="{f(x0+w+8)}" y="{y+15}" class="xs mono">{secs:.2f} s</text>')
        b.append(f'<text x="{x0}" y="{y+34}" class="xs muted">{what}</text>')

    b.append(f'<text x="20" y="240" class="xs t-hi">'
             'A public header is a promise about rebuild time as much as about behaviour.'
             '</text>')
    b.append('<text x="20" y="256" class="xs muted">'
             'Seconds, on a project this size. The RATIO is what carries: 2.3&#215; here, and '
             'the same shape at a hundred times the code.</text>')

    b.append(f'<line x1="20" y1="272" x2="{W-20}" y2="272" class="grid" stroke-width="1"/>')
    b.append('<text x="20" y="292" class="xs">'
             'And the biggest translation unit, compiled alone at -O2:</text>')
    for i, (lab, secs, col) in enumerate((("before  src/main.cpp", TU_BEFORE, C_BAD),
                                          ("after   demos/sandbox/main.cpp", TU_AFTER, C_NRM))):
        y = 302 + i * 16
        w = 200 * secs / 1.4
        b.append(f'<text x="20" y="{y+8}" class="xs mono">{lab}</text>')
        b.append(f'<rect x="230" y="{y}" width="{f(w)}" height="10" fill="{col}" '
                 f'fill-opacity="0.32" stroke="{col}" stroke-width="0.8"/>')
        b.append(f'<text x="{f(230+w+8)}" y="{y+8}" class="xs mono">{secs:.2f} s</text>')
    b.append(f'<text x="470" y="318" class="xs t-ok">'
             f'{100*(TU_BEFORE-TU_AFTER)/TU_BEFORE:.0f}% less, for '
             f'{round(100*(MAIN_BEFORE-MAIN_AFTER)/MAIN_BEFORE)}% fewer lines</text>')
    return svg(W, H, "What a change costs",
               "Bar chart of incremental rebuild times after touching four different files, "
               "and the compile time of the largest translation unit before and after.",
               "\n".join(b), "l51f4")


# =============================================================================
# Figure 5 — the test that had to exist first
# =============================================================================
def fig5():
    W, H = 720, 372
    b = []
    b.append('<text x="20" y="20" class="sm">'
             'The order of operations, and it is the whole method</text>')

    STEPS = [("1. write --shot", "before a single file moved", C_UV),
             ("2. capture", f"7 frames, {GOLDEN_BYTES:,} bytes, hash {GOLDEN_HASH}", C_UV),
             ("3. MOVE 2,239 lines", "engine/, demos/, new headers", C_POS),
             ("4. run --shot", "byte-identical", C_NRM),
             ("5. REDESIGN the API", "15 parameters &#8594; 8", C_POS),
             ("6. run --shot", "byte-identical", C_NRM),
             ("7. verify_50, which LINKS", "byte-identical, from outside the demo", C_NRM)]
    y = 40
    for title, note, col in STEPS:
        b.append(f'<rect x="20" y="{y}" width="10" height="20" fill="{col}"/>')
        b.append(f'<text x="40" y="{y+14}" class="xs mono">{title}</text>')
        b.append(f'<text x="230" y="{y+14}" class="xs muted">{note}</text>')
        y += 26

    b.append(f'<text x="20" y="{y+12}" class="xs t-hi">'
             'Two verifications, not one: a move that changes nothing, then a change that '
             'moves nothing.</text>')
    b.append(f'<text x="20" y="{y+28}" class="xs muted">'
             'Either alone would have left the other unaccounted for.</text>')

    # the filmstrip
    fy = 272
    b.append(f'<text x="20" y="{fy-10}" class="xs muted">'
             'the seven pinned frames, and what each one exists to reach:</text>')
    fw = (W - 40 - 6 * 6) / 7
    for i, (label, h, painted) in enumerate(FRAMES):
        x = 20 + i * (fw + 6)
        b.append(f'<rect x="{f(x)}" y="{fy}" width="{f(fw)}" height="42" rx="2" '
                 f'class="fill-soft grid" stroke-width="1"/>')
        b.append(f'<text x="{f(x+fw/2)}" y="{fy+16}" class="xs" text-anchor="middle">'
                 f'{label}</text>')
        b.append(f'<text x="{f(x+fw/2)}" y="{fy+30}" class="xs mono muted" '
                 f'text-anchor="middle">{h}</text>')
    b.append(f'<text x="20" y="{fy+62}" class="xs">'
             'The seventh was added because the first six all reported '
             '<tspan class="mono">straddle = 0</tspan>: '
             '<tspan class="t-hi">the near-plane clipper was never being called</tspan>.</text>')
    b.append(f'<text x="20" y="{fy+78}" class="xs muted">'
             'A characterization test that does not reach a branch cannot pin it. '
             'Frame 6 puts the camera on the floor: 8 triangles straddle, 32 in, 36 out.</text>')
    return svg(W, H, "The test that had to exist first",
               "Seven numbered steps from writing the characterization test through two "
               "verified transformations, and the seven pinned frames with their hashes.",
               "\n".join(b), "l51f5")


# =============================================================================
# Figure 6 — what is still on the wrong side of the line
# =============================================================================
def fig6():
    W, H = 720, 366
    b = []
    b.append('<text x="20" y="20" class="sm">'
             'Four things the boundary did NOT sort out, and when each is paid</text>')

    ROWS = [
        ("cull_choice is applied twice",
         "collect_triangles reads one of its four values; draw_triangles applies the rest",
         "6.5, the material system", C_UV),
        ("SDL is in the public API",
         "framebuffer says Uint32; gpu_device takes an SDL_Window*. The vocabulary is adopted",
         "5.2, the platform layer", C_INST),
        ("render_options ships the demo&#8217;s teaching switches",
         "trs_order::tsr exists so a lesson can show a bug. A shipping engine has no such enum",
         "an exercise, and honestly never", C_BAD),
        ("the sandbox is still 5,721 lines",
         "splitting it needs an application layer to split it INTO",
         "5.2, who owns the loop", C_POS),
    ]
    y = 40
    for title, why, when, col in ROWS:
        b.append(f'<rect x="20" y="{y}" width="6" height="52" fill="{col}"/>')
        b.append(f'<text x="36" y="{y+14}" class="sm">{title}</text>')
        b.append(f'<text x="36" y="{y+32}" class="xs muted">{why}</text>')
        b.append(f'<text x="36" y="{y+48}" class="xs t-hi">paid in {when}</text>')
        y += 64

    b.append(f'<line x1="20" y1="{y+4}" x2="{W-20}" y2="{y+4}" class="grid" stroke-width="1"/>')
    b.append(f'<text x="20" y="{y+26}" class="sm t-hi">'
             'A refactor lesson that ends &#8220;and now it is clean&#8221; is lying to you.'
             '</text>')
    b.append(f'<text x="20" y="{y+44}" class="xs muted">'
             'The first pass draws the line. Which side each thing belongs on is argued for '
             'the rest of the module</text>')
    b.append(f'<text x="20" y="{y+58}" class="xs muted">'
             '&#8212; and two of these four are argued by code that does not exist yet.'
             '</text>')
    return svg(W, H, "What is still on the wrong side",
               "Four leftovers of the refactor, each with why it is wrong and which later "
               "lesson resolves it.",
               "\n".join(b), "l51f6")


# =============================================================================
# Figure 7 — what hello_cube actually draws
# =============================================================================
def fig7():
    """Built from scratch/hello_cube.ppm, which `hello_cube --shot` renders.

    Run-length encoded against a two-entry palette — the cube's amber tint and a
    neutral, so a shaded face collapses into a handful of runs instead of a
    hundred near-identical colours. See figs_45.quantise for why the snapping
    happens hue-first.
    """
    W, H = 720, 312
    b = []
    b.append('<text x="20" y="20" class="sm">'
             './build/demos/hello_cube &#8212; 160 lines, public headers only</text>')

    w, h, data = read_ppm("scratch/hello_cube.ppm")
    CELL = 2
    gw, gh, grid = box_sample(data, w, (60, 6, 260, 174), CELL)
    PX = 3
    PANEL_X, PANEL_Y = 20, 32
    rects = rle_rects(grid, gw, gh, PANEL_X, PANEL_Y, PX,
                      [(224, 168, 60), (200, 200, 200)], levels=3)
    # The page scales this viewBox down, so 3-unit rows land on fractional device
    # pixels and the panel shows through between them as hairline seams. Overlapping
    # each row by a fraction of a unit costs nothing and removes them.
    b += [r.replace(f'height="{PX}"', f'height="{PX + 0.6}"') for r in rects]
    b.append(f'<rect x="{PANEL_X}" y="{PANEL_Y}" width="{gw*PX}" height="{gh*PX}" '
             'fill="none" class="grid" stroke-width="1"/>')
    b.append(f'<text x="{PANEL_X}" y="{PANEL_Y + gh*PX + 16}" class="xs muted">'
             f'{gw}&#215;{gh} cells of the 320&#215;180 framebuffer, run-length encoded'
             '</text>')

    tx = PANEL_X + gw * PX + 28
    NOTES = [
        ("12 triangles", "a cube; the fill culls the six facing away"),
        ("2,760 pixels painted", "counted by --shot, so this figure is checkable"),
        ("3 brightnesses", "one directional light, Lambert, per vertex"),
        ("0 mentions of a triangle", "in the whole source file"),
    ]
    y = 62
    for head, note in NOTES:
        b.append(f'<text x="{tx}" y="{y}" class="sm t-hi">{head}</text>')
        b.append(f'<text x="{tx}" y="{y+16}" class="xs muted">{note}</text>')
        y += 44

    b.append(f'<text x="{tx}" y="{y+8}" class="xs">'
             'The sandbox demo exists to show you the insides,</text>')
    b.append(f'<text x="{tx}" y="{y+24}" class="xs">'
             'so it is a bad advertisement for the API.</text>')
    b.append(f'<text x="{tx}" y="{y+40}" class="xs t-ok">This file is the honest one.</text>')
    return svg(W, H, "What hello_cube draws",
               "A lit amber cube on a dark background, reconstructed from the demo's own "
               "framebuffer, beside four facts about the program that drew it.",
               "\n".join(b), "l51f7")


if __name__ == "__main__":
    for i, fn in enumerate((fig1, fig2, fig3, fig4, fig5, fig6, fig7), start=1):
        write(f"l51_fig{i}.svg", fn())
