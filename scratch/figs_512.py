#!/usr/bin/env python3
"""scratch/figs_512.py — Lesson 5.12's diagrams.

Same rules as 5.10/6.x's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER, not writing order
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - legends and annotation boxes go OUTSIDE the plot

Every number comes from verify_512, scratch/port_512.py, or a measurement
recorded in the lesson body. Nothing here is illustrative.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_45 import read_ppm, rle_rects                             # noqa: E402
from figs_511 import peak_sample                                    # noqa: E402

OUT = "scratch"

# ---- measured --------------------------------------------------------------
PUBLIC_511 = 55           # public headers at Lesson 5.11 (incl. platform/main.hpp)
UMBRELLA_LISTED = 40      # #include lines in engine.hpp as shipped
UMBRELLA_MISSING = 15     # of which exactly one (platform/main.hpp) is deliberate
GAME_INCLUDES = 23        # engine headers demos/collector/main.cpp includes

# compile of one translation unit, best of five, c++ -std=c++20 -c (5.11 tree)
T_NOTHING = 0.01
T_UMBRELLA_SHIPPED = 0.28
T_EXPLICIT = 0.37
T_UMBRELLA_FIXED = 0.39

CHECKS = 27
DRIFT_REAL = 26
DRIFT_TOTAL_LINES = 1485
ENGINE_ADDED = 9774

MISSING = [("ecs/registry.hpp", "5.8"), ("ecs/view.hpp", "5.8"),
           ("ecs/entity.hpp", "5.8"), ("ecs/pool.hpp", "5.8"),
           ("ecs/hierarchy.hpp", "5.9"), ("ecs/camera.hpp", "5.9"),
           ("core/actions.hpp", "5.10"), ("core/handle.hpp", "5.4"),
           ("core/pool.hpp", "5.4"), ("core/log.hpp", "5.3"),
           ("core/assert.hpp", "5.3"), ("core/bench.hpp", "5.6"),
           ("asset/asset_store.hpp", "5.5"), ("asset/search_path.hpp", "5.5")]


# ===========================================================================
# Figure 1 — who has ever stood on the boundary   (§1)
# ===========================================================================
def fig_boundary():
    uid = "l512f1"
    W, H = 720, 364
    b = [label(20, 22, "Six programs on one public API, and what each was written to prove.",
               "sm", "start")]

    # the library
    b.append(box(24, 48, 200, 244, colour=GREY, opacity=0.10))
    b.append(label(124, 68, "engine/", "sm mono"))
    b.append(label(124, 84, "a static library", "xs muted"))
    subsys = ["platform / app", "asset store", "handles + pools", "ECS + hierarchy",
              "action map", "debug lines + UI", "software renderer", "SDL_GPU renderer"]
    for i, name in enumerate(subsys):
        y = 104 + i * 23
        b.append(hollow(40, y, 168, 18, GREY, width=1.0, rx=2))
        b.append(label(124, y + 13, name, "xs"))

    # the boundary
    b.append(rule(250, 44, 250, 300, "ink", width=1.6, dash="5 4"))
    b.append(label(250, 36, "the boundary (5.1)", "xs muted"))
    b.append(label(250, 312, "include path, not style guide", "xs muted"))

    # the programs
    progs = [("sandbox", "pre-refactor; owns its own loop", False),
             ("pong", "Module 1's game, relocated", False),
             ("hello_cube", "the smallest proof: one cube", False),
             ("ecs_swarm", "the ECS, by the person who built it", False),
             ("gltf_view", "the importer, ditto", False),
             ("collector", "a GAME. nothing it needs was built for it", True)]
    for i, (name, why, star) in enumerate(progs):
        y = 52 + i * 42
        col = AMBER if star else GREY
        b.append(hollow(290, y, 404, 30, col, width=1.6 if star else 1.0, rx=3))
        b.append(label(300, y + 13, name, "xs mono t-hi" if star else "xs mono", "start"))
        b.append(label(300, y + 25, why, "xs muted", "start"))
        b.append(arrow(250, y + 15, 286, y + 15, uid, "h" if star else "s", 1.4 if star else 1.0))

    b.append(label(20, 338,
                   "A demo may stop at the edge of the subsystem it demonstrates. A game has to cross every edge there is,",
                   "xs muted", "start"))
    b.append(label(20, 352,
                   "and the seams between subsystems are where an API is weakest, because nobody has ever stood on them.",
                   "xs muted", "start"))
    return svg(uid, W, H, "Five demo programs against the engine boundary",
               "The engine library on the left with eight subsystems, the Lesson 5.1 boundary "
               "as a dashed line, and six programs on the right. Five were written to "
               "demonstrate a subsystem by the person who had just built it; collector, "
               "highlighted, is a game and uses all of them.", b)


# ===========================================================================
# Figure 2 — the table of contents that was not one   (§3)
# ===========================================================================
def fig_umbrella():
    uid = "l512f2"
    W, H = 720, 412
    b = [label(20, 22, "engine.hpp calls itself “the whole public API, in one include”. It is not.",
               "sm", "start")]

    # the bar
    bar_x, bar_y, bar_w, bar_h = 24, 44, 672, 30
    listed_w = bar_w * UMBRELLA_LISTED / PUBLIC_511
    b.append(box(bar_x, bar_y, listed_w, bar_h, colour=GREEN, opacity=0.28))
    b.append(box(bar_x + listed_w, bar_y, bar_w - listed_w, bar_h, colour=RED, opacity=0.28))
    b.append(hollow(bar_x, bar_y, bar_w, bar_h, GREY, width=1.0))
    b.append(label(bar_x + listed_w / 2, bar_y + 19,
                   f"{UMBRELLA_LISTED} listed", "xs mono"))
    b.append(label(bar_x + listed_w + (bar_w - listed_w) / 2, bar_y + 19,
                   f"{UMBRELLA_MISSING} missing", "xs mono"))
    b.append(label(bar_x, bar_y + 90 - 4, "", "xs"))
    # "FOR IT TO LIST", NOT "PUBLIC HEADERS". The engine had 56 public headers at
    # Lesson 5.11 and one of them is engine.hpp itself, which cannot include
    # itself — so the set it is measured against is the other 55. STATE.md counts
    # the 56; this figure counts what the claim is actually about.
    b.append(label(bar_x + bar_w, 38, f"{PUBLIC_511} public headers for it to list",
                   "xs muted", "end"))

    # what is missing, by the lesson that added it
    b.append(label(24, 100, "WHAT IS MISSING — and the lesson that shipped it",
                   "xs t-bad", "start"))
    for i, (name, lesson) in enumerate(MISSING):
        col = i // 7
        row = i % 7
        x = 32 + col * 340
        y = 120 + row * 20
        b.append(label(x, y, lesson, "xs mono muted", "start"))
        b.append(label(x + 34, y, name, "xs mono", "start"))
    b.append(label(32, 120 + 7 * 20, "5.2", "xs mono muted", "start"))
    b.append(label(66, 120 + 7 * 20, "platform/main.hpp", "xs mono", "start"))
    b.append(label(200, 120 + 7 * 20, "— deliberate, and documented", "xs t-ok", "start"))

    # the history
    b.append(rule(24, 292, 696, 292, "grid"))
    b.append(label(24, 312, "THE HISTORY OF THIS FILE, WHICH EXPLAINS ITSELF", "xs", "start"))
    hist = [("5.1", "created, 38 headers", True),
            ("5.2", "remembered: platform/, app/", True),
            ("5.3 – 5.10", "seven lessons, fourteen headers, none added", False),
            ("5.11", "remembered: debug_lines, debug_ui", True),
            ("Module 6", "twenty-two more headers, none added", False)]
    for i, (when, what, ok) in enumerate(hist):
        y = 332 + i * 15
        b.append(label(32, y, when, "xs mono muted", "start"))
        b.append(label(110, y, what, "xs t-ok" if ok else "xs t-bad", "start"))
    return svg(uid, W, H, "The umbrella header lists 40 of 55 public headers",
               "A bar showing 40 of 55 public headers listed in engine.hpp and 15 missing, "
               "a list of the missing headers labelled with the lesson that added each, and "
               "a history showing the file was updated in Lessons 5.1, 5.2 and 5.11 and in "
               "no other lesson.", b)


# ===========================================================================
# Figure 3 — the measurement that went the wrong way   (§3)
# ===========================================================================
def fig_compile():
    uid = "l512f3"
    W, H = 720, 300
    b = [label(20, 22, "What it costs to compile one translation unit — and the surprise.",
               "sm", "start")]

    rows = [("nothing at all (baseline)", T_NOTHING, GREY),
            (f"#include <engine/engine.hpp>  — as shipped, {UMBRELLA_LISTED} headers",
             T_UMBRELLA_SHIPPED, RED),
            (f"the game's own {GAME_INCLUDES} explicit includes", T_EXPLICIT, BLUE),
            (f"#include <engine/engine.hpp>  — completed, {PUBLIC_511 - 1} headers",
             T_UMBRELLA_FIXED, GREEN)]
    x0, w_max = 300, 340
    for i, (name, t, col) in enumerate(rows):
        y = 58 + i * 42
        b.append(label(288, y + 14, name, "xs", "end"))
        b.append(box(x0, y, w_max * t / T_UMBRELLA_FIXED, 20, colour=col, opacity=0.30))
        b.append(label(x0 + w_max * t / T_UMBRELLA_FIXED + 8, y + 14,
                       f"{t:.2f} s", "xs mono", "start"))

    b.append(rule(24, 238, 696, 238, "grid"))
    b.append(label(24, 256,
                   "THE INCOMPLETE UMBRELLA LOOKED CHEAP BECAUSE IT WAS MISSING THE EXPENSIVE HALF.",
                   "xs t-hi", "start"))
    b.append(label(24, 272,
                   "The ECS, the asset store and the action map are the templated headers a game actually needs;",
                   "xs muted", "start"))
    b.append(label(24, 286,
                   f"completing the file costs +{100*(T_UMBRELLA_FIXED-T_UMBRELLA_SHIPPED)/T_UMBRELLA_SHIPPED:.0f}%, "
                   f"and lands {100*(T_UMBRELLA_FIXED-T_EXPLICIT)/T_EXPLICIT:.0f}% above including exactly what you use.",
                   "xs muted", "start"))
    return svg(uid, W, H, "Compile cost of the umbrella before and after completing it",
               "Four horizontal bars: an empty file at 0.01 seconds, the umbrella as shipped "
               "at 0.28, the game's own 23 explicit includes at 0.37, and the completed "
               "umbrella at 0.39. The incomplete umbrella was cheaper than including what "
               "you use, because it omitted the ECS, the asset store and the action map.", b)


# ===========================================================================
# Figure 4 — the bridge, and the one that is still missing   (§5)
# ===========================================================================
def fig_bridge():
    uid = "l512f4"
    W, H = 720, 330
    b = [label(20, 22, "From components to pixels — and the arrow that does not exist.",
               "sm", "start")]

    def chain(y, items, colour, uid_kind):
        x = 34
        for i, (txt, w) in enumerate(items):
            b.append(hollow(x, y, w, 30, colour, width=1.3))
            b.append(label(x + w / 2, y + 19, txt, "xs mono"))
            if i + 1 < len(items):
                b.append(arrow(x + w + 3, y + 15, x + w + 27, y + 15, uid, uid_kind, 1.2))
            x += w + 30
        return x

    b.append(label(24, 56, "THE SOFTWARE PATH — complete as of this lesson", "xs t-ok", "start"))
    chain(64, [("world_transform + renderable", 190), ("scene_object", 105),
               ("raster_triangle", 110), ("pixels", 70)], GREEN, "g")
    b.append(label(34, 112, "collect_renderables()", "xs mono t-hi", "start"))
    b.append(label(254, 112, "collect_triangles()", "xs mono muted", "start"))
    b.append(label(399, 112, "draw_triangles()", "xs mono muted", "start"))
    b.append(label(34, 126, "NEW — engine/gfx/renderable.hpp", "xs muted", "start"))

    b.append(rule(24, 146, 696, 146, "grid"))

    b.append(label(24, 170, "THE GPU PATH — and the gap is the first box", "xs t-bad", "start"))
    x = 34
    b.append(hollow(x, 178, 190, 30, GREY, width=1.3, dash="4 3"))
    b.append(label(x + 95, 197, "world_transform + renderable", "xs mono"))
    b.append(label(x + 95, 224, "?", "sm t-bad"))
    b.append(rule(x + 195, 193, x + 219, 193, "ink-soft", dash="3 3"))
    b.append(label(x + 207, 184, "×", "sm t-bad"))
    chain2_x = x + 224
    for txt, w in [("gpu_draw_item", 110), ("SDL_GPU draw", 110), ("pixels", 70)]:
        b.append(hollow(chain2_x, 178, w, 30, BLUE, width=1.3))
        b.append(label(chain2_x + w / 2, 197, txt, "xs mono"))
        if txt != "pixels":
            b.append(arrow(chain2_x + w + 3, 193, chain2_x + w + 27, 193, uid, "s", 1.2))
        chain2_x += w + 30

    b.append(label(24, 250,
                   "No public function turns entities into gpu_draw_items. sandbox bridges it PRIVATELY, inside its own",
                   "xs muted", "start"))
    b.append(label(24, 264,
                   "main.cpp, which is why the gap was invisible: the one program that crossed it was written before the",
                   "xs muted", "start"))
    b.append(label(24, 278,
                   "boundary existed. That is why collector renders on the CPU — the engine's REAL renderer is the one",
                   "xs muted", "start"))
    b.append(label(24, 292, "a game cannot reach.", "xs muted", "start"))
    b.append(label(24, 314, "Module 9's renderer facade is where this is answered.", "xs t-hi", "start"))
    return svg(uid, W, H, "The component-to-pixel chain on both renderers",
               "The software path runs from world_transform plus renderable through "
               "scene_object and raster_triangle to pixels, with collect_renderables as the "
               "new first step. The GPU path has gpu_draw_item, an SDL_GPU draw and pixels, "
               "but no public function connects entities to gpu_draw_item.", b)


# ===========================================================================
# Figure 5 — the boom, and why the order of two matrices is not a detail (§6)
# ===========================================================================
def fig_boom():
    uid = "l512f5"
    W, H = 720, 386
    b = [label(20, 22, "Three links, and the one that has to undo the other two.", "sm", "start")]

    # the chain
    nodes = [("rover", "H · Rz(bank) · S", AMBER),
             ("boom", "the fix goes here", GREEN),
             ("camera", "look_along(...)", BLUE)]
    for i, (name, sub, col) in enumerate(nodes):
        x = 60 + i * 220
        b.append(hollow(x, 48, 160, 44, col, width=1.5))
        b.append(label(x + 80, 66, name, "sm mono"))
        b.append(label(x + 80, 82, sub, "xs muted"))
        if i + 1 < len(nodes):
            b.append(arrow(x + 163, 70, x + 217, 70, uid, "i", 1.3))
    b.append(label(390, 104, "parent of", "xs muted"))
    b.append(label(170, 104, "parent of", "xs muted"))

    # the algebra
    b.append(rule(24, 124, 696, 124, "grid"))
    b.append(label(24, 146, "WHAT THE BOOM MUST BE", "xs", "start"))
    lines = [("we want", "world(boom) = H", ""),
             ("we have", "world(rover) = H · Rz(bank) · S", ""),
             ("so", "local = (H · Rz(bank) · S)⁻¹ · H", ""),
             ("", "= S⁻¹ · Rz(−bank) · H⁻¹ · H", ""),
             ("", "= S⁻¹ · Rz(−bank)", "← the unscale comes FIRST")]
    for i, (lead, expr, note) in enumerate(lines):
        y = 166 + i * 17
        b.append(label(96, y, lead, "xs muted", "end"))
        b.append(label(108, y, expr, "xs mono", "start"))
        if note:
            b.append(label(340, y, note, "xs t-hi", "start"))

    # the two candidates
    b.append(rule(24, 262, 696, 262, "grid"))
    cands = [("S⁻¹ · Rz(−bank)", "rigid", "view_from_camera is defined", True),
             ("Rz(−bank) · S⁻¹", "NOT rigid", "assertion fires, headless run HANGS", False)]
    for i, (expr, verdict, what, ok) in enumerate(cands):
        y = 284 + i * 40
        b.append(hollow(30, y, 200, 30, GREEN if ok else RED, width=1.4))
        b.append(label(130, y + 19, expr, "xs mono"))
        b.append(label(246, y + 13, verdict, "xs t-ok" if ok else "xs t-bad", "start"))
        b.append(label(246, y + 26, what, "xs muted", "start"))

    b.append(label(24, 372,
                   "A rotation and a non-uniform scale do not commute, so the second is a different matrix — and not a visibly wrong one.",
                   "xs muted", "start"))
    return svg(uid, W, H, "The camera boom and the order of its two matrices",
               "The rover parents a boom which parents the camera. The boom's local basis "
               "must be the inverse scale times the inverse roll, in that order, because the "
               "inverse of a product reverses it. Written the other way round the camera's "
               "placement is not rigid and view_from_camera's assertion fires.", b)


# ===========================================================================
# Figure 6 — one word, two conventions   (§8)
# ===========================================================================
def fig_unit():
    uid = "l512f6"
    W, H = 720, 300
    b = [label(20, 22, "“Unit” means two different things in one header.", "sm", "start")]

    cx1, cx2, cy, s = 170, 500, 150, 58
    # cube: +/- 0.5
    b.append(rule(cx1 - 2.2 * s, cy, cx1 + 2.2 * s, cy, "grid"))
    b.append(rule(cx1, cy - 1.7 * s, cx1, cy + 1.7 * s, "grid"))
    b.append(hollow(cx1 - 0.5 * s, cy - 0.5 * s, s, s, AMBER, width=1.6))
    b.append(label(cx1, cy - 0.5 * s - 10, "cube_mesh()", "xs mono"))
    b.append(label(cx1, cy + 0.5 * s + 16, "spans ±0.5", "xs"))
    b.append(label(cx1, cy + 0.5 * s + 30, "scale = FULL SIZE", "xs t-hi"))

    # icosahedron: radius 1
    b.append(rule(cx2 - 2.2 * s, cy, cx2 + 2.2 * s, cy, "grid"))
    b.append(rule(cx2, cy - 1.7 * s, cx2, cy + 1.7 * s, "grid"))
    pts = []
    import math
    for k in range(6):
        a = math.pi / 6 + k * math.pi / 3
        pts.append(f"{cx2 + s * math.cos(a):.1f},{cy + s * math.sin(a):.1f}")
    b.append(f'<polygon points="{" ".join(pts)}" fill="none" stroke="{BLUE}" stroke-width="1.6"/>')
    b.append(f'<circle cx="{cx2}" cy="{cy}" r="{s}" class="ink-soft" fill="none" '
             f'stroke-width="1.0" stroke-dasharray="3 3"/>')
    b.append(label(cx2, cy - s - 10, "icosahedron_mesh()", "xs mono"))
    b.append(label(cx2, cy + s + 16, "circumradius 1.0", "xs"))
    b.append(label(cx2, cy + s + 30, "scale = RADIUS", "xs t-hi"))

    b.append(arrow(cx1 + 0.5 * s, cy - 0.5 * s - 24, cx2 - s, cy - s - 24, uid, "b", 1.3))
    b.append(label((cx1 + cx2) / 2, cy - s - 30, "2× the diameter at the same scale",
                   "xs t-bad"))

    b.append(rule(24, 240, 696, 240, "grid"))
    b.append(label(24, 258, "THIS FILE GOT IT WRONG THREE TIMES:", "xs t-bad", "start"))
    b.append(label(24, 274,
                   "the floor (a quarter of its area), the pillars (half-buried), and the debug boxes (exactly 2× too big,",
                   "xs muted", "start"))
    b.append(label(24, 288,
                   "which looks like a collision margin). box_at() exists so it only had to be understood once.",
                   "xs muted", "start"))
    return svg(uid, W, H, "The cube spans plus or minus a half; the icosahedron has radius one",
               "Side by side: cube_mesh spans plus or minus 0.5 so a transform's scale is the "
               "box's full size, while icosahedron_mesh has circumradius 1.0 so its scale is a "
               "radius. At the same scale the icosahedron is twice the diameter.", b)


# ===========================================================================
# Figure 7 — what a game found, and who answers it   (§11)
# ===========================================================================
def fig_findings():
    uid = "l512f7"
    W, H = 720, 350
    b = [label(20, 22, "Seven findings, and where each one is answered.", "sm", "start")]

    rows = [("the umbrella listed 40 of 55 public headers", "FIXED HERE", True),
            ("the ECS → renderer loop had no home", "FIXED HERE", True),
            ("view() has no const overload, so no system can say it only reads",
             "reported, Exercise 4", False),
            ("no public path from entities to the GPU renderer", "Module 9", False),
            ("no collision — the wall is a clamp, the pillars are scenery", "Module 8", False),
            ("the engine cannot draw a character; the HUD is SDL's", "Module 6", False),
            ("no audio: a pickup is silent", "Module 7", False)]
    for i, (what, who, fixed) in enumerate(rows):
        y = 52 + i * 34
        b.append(box(24, y, 672, 26, colour=(GREEN if fixed else GREY),
                     opacity=0.16 if fixed else 0.07))
        b.append(label(36, y + 17, what, "xs", "start"))
        b.append(label(684, y + 17, who, "xs mono t-ok" if fixed else "xs mono muted", "end"))

    b.append(rule(24, 298, 696, 298, "grid"))
    # TWO LINES, NOT ONE. The single-line version was 139 characters, which at
    # ~5.2 units per `xs` character is 723 units from x = 24 — 27 past the
    # 720-unit viewBox, and check-page.js's svgSpill check reported exactly that
    # (19 px at 1280, 8.4 px at 390). A label may not leave the viewBox; a SHAPE
    # may.
    b.append(label(24, 316,
                   "Two of seven were closed for the price of a header, and both were promises "
                   "already on record.",
                   "xs muted", "start"))
    b.append(label(24, 331,
                   "The other five are the whole point of finding them at hour 62 rather than "
                   "hour 434.",
                   "xs muted", "start"))
    return svg(uid, W, H, "Seven findings and the module that answers each",
               "A list of seven gaps the checkpoint game found. Two are fixed in this lesson: "
               "the incomplete umbrella header and the missing ECS-to-renderer bridge. One is "
               "reported and left as an exercise. The remaining four are answered by Modules "
               "6, 7, 8 and 9.", b)


# ===========================================================================
# Figure 8 — the expected result, from the real render   (§11)
# ===========================================================================
def fig_result():
    uid = "l512f8"
    # HEIGHT COMPUTED FROM THE PANELS, NOT GUESSED. Each panel is
    # (264 / CELL) * PX = 198 units tall, so the second ends at 250 + 198 = 448.
    # The first draft put the closing note at y = 440 and it printed straight
    # across the lower render — which check-page.js CANNOT see, because a label
    # over a filled <rect> is not text-over-text and not text-over-stroke. This
    # is the class of defect that needs eyes (README §13).
    W, H = 720, 516
    b = [label(20, 20, "What `collector --shot` produces, and the one thing missing from it.",
               "sm", "start")]

    # TWO REAL RENDERS, box-sampled and run-length encoded — 5.11's technique.
    # PEAK sampling and not box sampling for the lower panel, because the gizmos
    # are one-pixel lines: averaging a 2x2 block turns 889 crisp wireframe edges
    # into a uniform haze, which is precisely the thing the panel is FOR.
    CELL, PX = 2, 1.5
    PANEL_X = 24
    for k, (path, py, sampler, palette) in enumerate((
            ("scratch/l512_shot.ppm", 44, "box",
             [(150, 156, 174),    # the floor and kerbs
              (232, 168, 60),     # the rover
              (158, 126, 92),     # the pillars
              (120, 214, 255)]),  # the orbs
            ("scratch/l512_shot_giz.ppm", 250, "peak",
             [(255, 120, 120),    # the pillar boxes
              (255, 210, 120),    # the rover's pickup sphere
              (120, 210, 255),    # the orbs' pickup spheres
              (150, 255, 170)]))):  # the lead line
        w, h, data = read_ppm(path)
        gw, gh, grid = peak_sample(data, w, (0, 0, 480, 264), CELL)
        rects = rle_rects(grid, gw, gh, PANEL_X, py, PX, palette, levels=3)
        # THE PANEL IS THE PROGRAM'S OWN BACKGROUND, not a theme colour. This is
        # a screenshot of a dark program; `fill-soft` is mid-slate in light mode
        # and the thin gizmo lines vanish into it completely (5.11's finding).
        rects[0] = (f'<rect x="{PANEL_X}" y="{py}" width="{gw*PX}" '
                    f'height="{gh*PX}" fill="#141821" stroke="none"/>')
        b += [r.replace(f'height="{PX}"', f'height="{PX + 0.5}"') for r in rects]
        b.append(f'<rect x="{PANEL_X}" y="{py}" width="{gw*PX}" height="{gh*PX}" '
                 'fill="none" class="grid" stroke-width="1"/>')

    tx = PANEL_X + 370
    b.append(label(tx, 62, "the frame", "sm t-hi", "start"))
    for i, (a, c) in enumerate((("240 fixed steps", "held at drive 1.0, steer 0.55"),
                                ("25 renderables", "of 30 live entities"),
                                ("380 triangles", "printed, so this is checkable"),
                                ("2 of 12 orbs", "collected on the way round"))):
        b.append(label(tx, 82 + i * 34, a, "xs mono", "start"))
        b.append(label(tx, 95 + i * 34, c, "xs muted", "start"))

    b.append(label(tx, 272, "press [G]", "sm t-hi", "start"))
    for i, (a, c1, c2) in enumerate((
            ("889 debug lines", "queued by a system with no",
             "framebuffer in its signature"),
            ("red boxes", "the collision geometry that",
             "nothing tests against (\u00a78.1)"))):
        y = 294 + i * 60
        b.append(label(tx, y, a, "xs mono", "start"))
        b.append(label(tx, y + 14, c1, "xs muted", "start"))
        b.append(label(tx, y + 27, c2, "xs muted", "start"))

    b.append(rule(24, 462, 696, 462, "grid"))
    b.append(label(24, 482,
                   "AND THE SCORE IS NOT ON EITHER PICTURE. A --shot run is headless, so there is no",
                   "xs muted", "start"))
    b.append(label(24, 498,
                   "SDL_Renderer, so there is no HUD \u2014 the engine has no text of its own (\u00a78.2).",
                   "xs t-bad", "start"))
    return svg(uid, W, H, "The deterministic screenshot, with and without gizmos",
               "Two run-length-encoded renders of the game's headless shot. The upper panel "
               "shows the arena: a floor with kerbs, eight pillars, ten remaining orbs and the "
               "amber rover. The lower panel adds the gameplay gizmos: pickup spheres round "
               "every orb and the rover, a line to the nearest orb, and red oriented boxes "
               "round every pillar. Neither picture carries a score, because a headless run "
               "has no SDL_Renderer and the HUD is drawn by SDL.", b)


FIGS = [("l512_fig1.svg", fig_boundary), ("l512_fig2.svg", fig_umbrella),
        ("l512_fig3.svg", fig_compile), ("l512_fig4.svg", fig_bridge),
        ("l512_fig5.svg", fig_boom), ("l512_fig6.svg", fig_unit),
        ("l512_fig7.svg", fig_findings), ("l512_fig8.svg", fig_result)]


def main():
    for name, fn in FIGS:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
