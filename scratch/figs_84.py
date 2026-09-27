#!/usr/bin/env python3
"""scratch/figs_84.py — Lesson 8.4's diagrams.

Same rules as 5.1-8.3's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - the figure palette must contain the DEMO's own colours (rle_rects SNAPS)

Every number below is transcribed from scratch/verify_84.log. Nothing here is
estimated, and the section of the harness each block came from is named above it.

*** THIS FILE IS WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. *** 8.1's
figs file ended up mixing the two because successive patch scripts wrote both,
and a str.replace written against one form fails SILENTLY against the other.

THE COLOUR RULE FOR THIS LESSON. Almost nothing here is a coordinate axis, so
8.3's x/y/z = red/green/blue does not apply and using it would say something
false. Instead:
  GREEN  = separated / this direction proves them apart
  RED    = overlapping
  AMBER  = the axis the algorithm returned
  BLUE   = box A, and geometry belonging to it
  GREY   = a candidate that found nothing
which is the demo's own key, so a reader moves between the two without
relearning anything.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_45 import read_ppm, rle_rects, hexrgb, box_sample         # noqa: E402
from figs_511 import peak_sample                                    # noqa: E402
from figs_71 import cmarker, carrow, poly, frame, D                 # noqa: E402
from figs_76 import table, f, fmt_e                                 # noqa: E402
from figs_81 import legend, logspan                                 # noqa: E402

OUT = "scratch"

# demos/collide/main.cpp's constants, transcribed. rle_rects SNAPS every sampled
# cell to the nearest palette entry, so a colour the demo draws and this list
# omits comes out as whichever entry happens to be closest.
C_BG      = "#101218"
C_GRID    = "#262a34"
C_AXIS    = "#545c6c"
C_BOX_A   = "#96a0ff"
C_B_FREE  = "#78dca0"
C_B_HIT   = "#eb6060"
C_WINNER  = "#ebc860"
C_BAR_OVER = "#60687c"
C_BAR_SKIP = "#3c404e"
COLLIDE_PALETTE = [hexrgb(c) for c in (C_BG, C_GRID, C_AXIS, C_BOX_A, C_B_FREE,
                                       C_B_HIT, C_WINNER, C_BAR_OVER, C_BAR_SKIP)]


def render_panel(ppm, crop, x, y, px, cell=4, levels=3, peak=True):
    """A real render, downsampled and run-length encoded. See figs_76."""
    _w, _h, data = read_ppm(os.path.join(OUT, ppm))
    sampler = peak_sample if peak else box_sample
    grid_w, grid_h, grid = sampler(data, _w, crop, cell)
    body = rle_rects(grid, grid_w, grid_h, x, y, px, COLLIDE_PALETTE, levels=levels,
                     bg_class="fill-shot")
    return (['<g shape-rendering="crispEdges">'] + body + ['</g>'],
            grid_w * px, grid_h * px)


# ---- transcribed from verify_84.log ---------------------------------------

# A — spheres
A_DEPTH = 0.500000
A_IA = (-1.0, 1.0)
A_IB = (0.5, 2.5)
A_PAIRS = 200000
A_OVERLAP = 26671
A_APART = 173329

# B — the projected radius
B_HALF = (2.0, 1.0, 0.5)
B_DOTS = (0.866025, -0.500000, 0.000000)
B_RADIUS = 2.232051
B_WORST_ABS = 7.1526e-07
B_NOABS_WRONG = 15089
B_NOABS_OF = 20000

# C — AABBs
C_PAIRS = 200000
C_OVERLAP = 25416
C_IDENTICAL = 200000
C_ANY_AXIS_WRONG = 149779

# D — the fifteen, and the crossed boxes
D_GAPS = [-3.12792, -1.94690, -0.11772,
          -3.09390, -0.78103, -1.62944,
          +0.05000, -1.84854, -1.37867,
          -0.30609, -2.76598, -1.19660,
          -2.00934, -2.69462, -3.12592]
D_LABELS = ["A face 0", "A face 1", "A face 2",
            "B face 0", "B face 1", "B face 2",
            "a0 × b0", "a0 × b1", "a0 × b2",
            "a1 × b0", "a1 × b1", "a1 × b2",
            "a2 × b0", "a2 × b1", "a2 × b2"]
D_CERT = 0.0500000
D_PAIRS = 200000
D_OVERLAP = 117583
D_FACE_YES = 123310
D_EDGE_SAVED = 5727
D_EDGE_PCT_OF_YES = 4.6
D_EDGE_MTV = 58550
D_EDGE_MTV_PCT = 49.8
D_ABSR_AGREE = 200000
D_ALIGNED_DISAGREE = 0

# E — the MTV
E_TESTED = 84586
E_SEPARATED = 84586
E_RESIDUAL = 1.431e-06
E_HALF_STILL = 84586
E_WRONG_WAY = 84586
E_SHALLOW = 0.5000
E_DEEP = 1.9000

# F — parallel edges. (guarded-normalised, absR, absR+eps) false separations
F_CORNER = [(1e-1, 9.98e-02, 0, 0, 0, 400),
            (1e-2, 1.00e-02, 0, 0, 0, 400),
            (1e-3, 1.00e-03, 0, 0, 0, 400),
            (1e-4, 1.00e-04, 0, 0, 0, 400),
            (1e-5, 1.00e-05, 0, 0, 0, 400),
            (1e-6, 1.10e-06, 0, 0, 0, 400),
            (1e-7, 2.58e-07, 0, 188, 0, 400),
            (0.0,  1.22e-07, 0, 89, 0, 400)]
F_GUARD_FIRED = 0
F_CANDIDATES = 1800000

# G — the precision floor
G_ROWS = [(0.0, 1.000e-03, 4.668e-08, 1.401e-45),
          (1.0, 9.999e-04, 7.253e-08, 1.192e-07),
          (10.0, 1.000e-03, 4.043e-07, 9.537e-07),
          (100.0, 9.995e-04, 5.494e-07, 7.629e-06),
          (1000.0, 9.766e-04, 2.344e-05, 6.104e-05),
          (10000.0, 9.766e-04, 2.344e-05, 9.766e-04),
          (100000.0, 0.0, 1.000e-03, 7.812e-03)]
G_GAP = 1.0e-03

# H — sphere against box
H_PAIRS = 100000
H_OUTSIDE = 91171
H_INSIDE = 8829
H_CLAMP_OK = 100000
H_NAIVE_ZERO = 8829
H_WALL_DEPTH = 0.3000

# I — the budget
I_SPHERE = 1.206
I_AABB = 2.606
I_SPREAD = 16.858
I_CROWD = 73.323
I_BOUNDS = 1.331
I_HIT = 79.258
I_OVERLAPS = 10.458
I_RATIO = 7.578
I_PRE_A = 10.090
I_PRE_B = 2.202
I_PRE_RATIO = 4.582
I_AXES_SPREAD = 2.47
I_AXES_CROWD = 15.00
I_HIST = [(1, 11126, 55.6), (2, 5023, 25.1), (3, 2115, 10.6), (4, 122, 0.6),
          (5, 119, 0.6), (6, 124, 0.6), (7, 15, 0.1), (8, 14, 0.1), (9, 19, 0.1),
          (10, 15, 0.1), (11, 8, 0.0), (12, 7, 0.0), (13, 8, 0.0), (14, 9, 0.0),
          (15, 1276, 6.4)]


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


# ===========================================================================
# Figure 1 — the shadow test, and the half of it that needs convexity  (§3)
# ===========================================================================
def fig1():
    uid = "l84f1"
    W, H = 900, 372
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="fill-bg" stroke="none"/>']

    # Two convex shapes, fixed once and shown under two different lamps.
    hexa = [(-44 + 28 * math.cos(a), -20 + 28 * math.sin(a))
            for a in [i * math.pi / 3 for i in range(6)]]
    tri = [(30, -34), (74, -6), (36, 30)]

    def shadow_panel(x0, title, sub, shapes, theta, note, ok):
        """One lamp direction: the shapes, and their shadows on that direction."""
        out = []
        pw = 262
        cx, cy = x0 + pw / 2, 156
        out.append(label(cx, 46, title, cls="sm"))
        out.append(label(cx, 62, sub, cls="xs muted"))

        ux, uy = math.cos(theta), math.sin(theta)
        nx, ny = -uy, ux                      # the perpendicular, to offset the line
        ox, oy = cx + nx * 92, cy + ny * 92   # a point ON the projection line

        out.append(cline(ox - ux * 122, oy - uy * 122, ox + ux * 122, oy + uy * 122,
                         GREY, width=1.2))
        out.append(label(ox + ux * 134, oy + uy * 134 + 4, "L", cls="xs mono muted"))

        spans = []
        for pts, colour in shapes:
            out.append(poly([(cx + px, cy + py) for px, py in pts], colour, width=1.6))
            ts = [px * ux + py * uy for px, py in pts]
            lo, hi = min(ts), max(ts)
            spans.append((lo, hi))
            # The shadow, as a thick segment of the projection line.
            out.append(cline(ox + ux * lo, oy + uy * lo, ox + ux * hi, oy + uy * hi,
                             colour, width=6.0))
            # Dashed rays from the two extreme vertices to their feet.
            for t in (lo, hi):
                vx, vy = min(((cx + px, cy + py, px * ux + py * uy) for px, py in pts),
                             key=lambda q: abs(q[2] - t))[:2]
                out.append(rule(vx, vy, ox + ux * t, oy + uy * t, cls="grid", dash="3 4"))
        return out, spans

    els, spans = shadow_panel(30, "a direction with a gap",
                              "the shadows do not meet — apart, proved",
                              [(hexa, BLUE), (tri, GREEN)], 0.0, "", True)
    b += els
    gap = spans[1][0] - spans[0][1]
    b.append(label(30 + 131, 330, f"gap on this L  +{gap / 62.0:.2f} m",
                   cls="xs mono t-ok"))
    b.append(label(30 + 131, 346, "one witness is a complete proof", cls="xs muted"))

    els2, spans2 = shadow_panel(320, "a direction without one",
                                "this lamp says nothing at all",
                                [(hexa, BLUE), (tri, GREEN)], math.pi / 2.35, "", False)
    b += els2
    over = min(spans2[0][1], spans2[1][1]) - max(spans2[0][0], spans2[1][0])
    b.append(label(320 + 131, 330, f"overlap on this L  −{over / 62.0:.2f} m",
                   cls="xs mono muted"))
    b.append(label(320 + 131, 346, "no conclusion — try another direction", cls="xs muted"))

    # The non-convex counterexample, where the converse fails.
    x0 = 612
    pw = 262
    cx, cy = x0 + pw / 2, 150
    b.append(label(cx, 46, "and why CONVEX is in the theorem", cls="sm"))
    b.append(label(cx, 62, "apart, yet EVERY shadow overlaps", cls="xs muted"))
    cup = [(-58, -44), (58, -44), (58, 38), (28, 38), (28, -12), (-28, -12),
           (-28, 38), (-58, 38)]
    ball = [(18 * math.cos(a), 10 + 18 * math.sin(a))
            for a in [i * math.pi / 6 for i in range(12)]]
    b.append(poly([(cx + px, cy + py) for px, py in cup], BLUE, width=1.6))
    b.append(poly([(cx + px, cy + py) for px, py in ball], RED, width=1.6))

    for pts, colour, dy in ((cup, BLUE, 0.0), (ball, RED, 7.0)):
        lo = min(p[0] for p in pts)
        hi = max(p[0] for p in pts)
        b.append(cline(cx + lo, cy + 82 + dy, cx + hi, cy + 82 + dy, colour, width=6.0))
    b.append(cline(cx - 118, cy + 78, cx + 118, cy + 78, GREY, width=1.0))

    for pts, colour, dx in ((cup, BLUE, 0.0), (ball, RED, 7.0)):
        lo = min(p[1] for p in pts)
        hi = max(p[1] for p in pts)
        b.append(cline(cx + 106 + dx, cy + lo, cx + 106 + dx, cy + hi, colour, width=6.0))
    b.append(cline(cx + 102, cy - 60, cx + 102, cy + 60, GREY, width=1.0))

    b.append(label(cx, cy + 116, "the ball sits in the cup's mouth,", cls="xs muted"))
    b.append(label(cx, cy + 130, "touching nothing — and no direction", cls="xs muted"))
    b.append(label(cx, cy + 144, "can say so, because the gap is not", cls="xs muted"))
    b.append(label(cx, cy + 158, "between the two SHAPES, it is inside", cls="xs muted"))
    b.append(label(cx, cy + 172, "one of them", cls="xs muted"))

    return svg(uid, W, H, "The shadow test",
               "Two convex shapes projected onto two different directions, one of which "
               "shows a gap, and a non-convex pair that are apart while every shadow "
               "overlaps.", b)


# ===========================================================================
# Figure 2 — the radius of a box's shadow  (§6)
# ===========================================================================
def fig2():
    uid = "l84f2"
    W, H = 900, 402
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="fill-bg" stroke="none"/>']

    # The worked example: half extents (2, 1, 0.5), turned 30 degrees about z,
    # projected onto the world x axis. Drawn in the z = 0 plane, where the third
    # half extent contributes nothing and the picture is honest about why.
    cx, cy = 250, 170
    s = 52.0
    ang = 30.0 * D
    u0 = (math.cos(ang), -math.sin(ang))
    u1 = (math.sin(ang), math.cos(ang))
    h0, h1 = B_HALF[0], B_HALF[1]

    corners = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            corners.append((cx + (u0[0] * sx * h0 + u1[0] * sy * h1) * s,
                            cy + (u0[1] * sx * h0 + u1[1] * sy * h1) * s))
    order = [corners[0], corners[1], corners[3], corners[2]]
    b.append(poly(order, BLUE, width=1.6))
    b.append(label(cx, cy - 6, "box", cls="xs muted"))

    # The two in-plane axes, drawn from the centre.
    b += [carrow(cx, cy, cx + u0[0] * h0 * s, cy + u0[1] * h0 * s, uid, "u0", BLUE,
                 width=1.6),
          carrow(cx, cy, cx + u1[0] * h1 * s, cy + u1[1] * h1 * s, uid, "u1", PURPLE,
                 width=1.6)]
    # THE TWO AXIS LABELS ARE A KEY BELOW THE PICTURE rather than text beside
    # their arrows. Inside the box every position that is clear of one arrow is
    # on the outline or on the other arrow — which is check-page.js's textOnShape,
    # and which reads exactly as badly as the check implies.

    b += legend(cx - 96, cy + 184, [(BLUE, "h₀ u₀  — 2.0 m along the first axis"),
                                    (PURPLE, "h₁ u₁  — 1.0 m along the second")])

    # The shadow on the world x axis.
    sy_line = cy + 132
    lo = min(p[0] for p in corners)
    hi = max(p[0] for p in corners)
    b.append(cline(cx - 190, sy_line, cx + 190, sy_line, GREY, width=1.2))
    b.append(label(cx + 202, sy_line + 4, "L = x", cls="xs mono muted", anchor="start"))
    b.append(cline(lo, sy_line, hi, sy_line, GREEN, width=6.0))
    for p in corners:
        b.append(rule(p[0], p[1], p[0], sy_line, cls="grid", dash="3 4"))
    b.append(cline(cx, sy_line - 16, cx, sy_line + 16, GREY, width=1.0))
    b.append(label((cx + hi) / 2, sy_line + 24, "radius", cls="xs mono"))
    b.append(label((cx + hi) / 2, sy_line + 38, f"{B_RADIUS:.4f} m", cls="xs mono t-hi"))

    # The stacked contributions, to the right of the picture and outside it.
    tx = 540
    b.append(label(tx, 62, "r(L)  =  Σᵢ hᵢ |uᵢ · L|", cls="sm mono", anchor="start"))
    b.append(label(tx, 84, "the three terms, for this box and this L:",
                   cls="xs muted", anchor="start"))

    rows = [(f"h₀ |u₀·L|", f"2.0 × |{B_DOTS[0]:.4f}|", f"{h0 * abs(B_DOTS[0]):.6f}"),
            (f"h₁ |u₁·L|", f"1.0 × |{B_DOTS[1]:.4f}|", f"{h1 * abs(B_DOTS[1]):.6f}"),
            (f"h₂ |u₂·L|", f"0.5 × |{B_DOTS[2]:.4f}|", f"{0.5 * abs(B_DOTS[2]):.6f}")]
    els, used = table(tx, 96, ("term", "value", "metres"), rows, (110, 150, 92))
    b += els
    ty = 96 + used
    b.append(rule(tx, ty - 6, tx + 352, ty - 6, cls="grid"))
    b.append(label(tx, ty + 12, "sum", cls="xs", anchor="start"))
    b.append(label(tx + 352 - 6, ty + 12, f"{B_RADIUS:.6f}", cls="xs mono t-hi",
                   anchor="end"))

    b.append(label(tx, ty + 44, "THE THIRD TERM IS ZERO BECAUSE u₂ ⟂ L, not",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, ty + 58, "because the box is flat. Turn it out of the page",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, ty + 72, "and that 0.5 starts contributing.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H, "The radius of a box's shadow",
               "A box turned 30 degrees, its projection onto the x axis, and the three "
               "terms of the projected-radius formula worked through with numbers.", b)


# ===========================================================================
# Figure 3 — where fifteen comes from  (§7)
# ===========================================================================
def fig3():
    uid = "l84f3"
    W, H = 900, 400
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="fill-bg" stroke="none"/>']

    b.append(label(158, 44, "3 + 3 face normals", cls="sm"))
    b.append(label(158, 60, "a gap you can see from a face", cls="xs muted"))

    # Left panel: two boxes separated along a face normal.
    cx, cy = 158, 168
    b.append(hollow(cx - 92, cy - 44, 74, 88, BLUE, width=1.5))
    b.append(hollow(cx + 18, cy - 30, 74, 60, GREEN, width=1.5))
    b.append(cline(cx - 18, cy - 62, cx - 18, cy + 62, GREY, dash="4 4"))
    b.append(cline(cx + 18, cy - 62, cx + 18, cy + 62, GREY, dash="4 4"))
    b.append(carrow(cx - 18, cy + 76, cx + 18, cy + 76, uid, "gapa", GREEN, width=1.8))
    b.append(label(cx, cy + 96, "gap", cls="xs mono"))
    b.append(label(cx, cy + 112, "found by a face normal", cls="xs muted"))

    # Middle panel: the edge-edge case, in perspective.
    b.append(label(450, 44, "9 edge × edge cross products", cls="sm"))
    b.append(label(450, 60, "a gap no face can see", cls="xs muted"))
    mx, my = 450, 168
    # Two skew rods, one going one way and one the other, with the common
    # perpendicular drawn between them.
    b.append(cline(mx - 110, my + 34, mx + 30, my - 26, BLUE, width=7.0))
    b.append(cline(mx - 30, my + 44, mx + 112, my - 40, GREEN, width=7.0))
    b.append(label(mx - 118, my + 52, "an edge of A", cls="xs muted", anchor="start"))
    b.append(label(mx + 14, my - 40, "an edge of B", cls="xs muted", anchor="start"))
    # the common perpendicular
    b.append(carrow(mx - 4, my + 12, mx + 20, my + 44, uid, "perp", AMBER, width=1.8))
    b.append(label(mx + 46, my + 54, "a₀ × b₀", cls="xs mono t-hi", anchor="start"))
    b.append(label(mx, my + 96, "perpendicular to one edge from each,", cls="xs muted"))
    b.append(label(mx, my + 110, "which is a direction neither box has a face in",
                   cls="xs muted"))

    # Right panel: the arithmetic of 15.
    tx = 640
    b.append(label(tx, 44, "the candidate list", cls="sm", anchor="start"))
    rows = [("A's face normals", "3", "a₀ a₁ a₂"),
            ("B's face normals", "3", "b₀ b₁ b₂"),
            ("edge × edge", "9", "aᵢ × bⱼ"),
            ("total", "15", "")]
    els, used = table(tx, 60, ("source", "n", "axes"), rows, (150, 40, 72))
    b += els

    b.append(label(tx, 60 + used + 26, "A box has six faces and three DISTINCT",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, 60 + used + 40, "normals: opposite faces differ only in",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, 60 + used + 54, "sign, and L and −L cast the same shadow.",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, 60 + used + 76, "Twelve edges, three directions, for the",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, 60 + used + 90, "same reason.", cls="xs muted", anchor="start"))

    b.append(rule(30, 348, 872, 348, cls="grid"))
    b.append(label(30, 368,
                   "The fifteen are the face normals of the Minkowski difference A ⊖ B: its "
                   "faces come either from a face of one box, or from one edge of each "
                   "sweeping past the other.", cls="xs muted", anchor="start"))
    b.append(label(30, 386,
                   f"Measured over {D_PAIRS:,} random pairs: {D_EDGE_SAVED:,} were separated "
                   f"ONLY by an edge-edge axis — {D_EDGE_PCT_OF_YES}% of everything six axes "
                   f"called overlapping, and every one re-proved in double precision.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H, "Where fifteen candidate axes come from",
               "Face normals catch gaps a face can see; the nine cross products catch the "
               "gap between two edges sliding past each other.", b)


# ===========================================================================
# Figure 4 — the crossed boxes, all fifteen gaps  (§7.3)
# ===========================================================================
def fig4():
    uid = "l84f4"
    W = 900
    row_h = 19.0
    top = 84
    H = int(top + len(D_GAPS) * row_h + 24)
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="fill-bg" stroke="none"/>']

    b.append(label(W / 2, 44, "two crossed planks, 5 cm apart — every candidate axis",
                   cls="sm"))
    b.append(label(W / 2, 60, "gap in metres; positive means this direction proves them apart",
                   cls="xs muted"))

    x_label = 40
    x_zero = 470
    px_per_m = 118.0
    b.append(cline(x_zero, top - 8, x_zero, top + len(D_GAPS) * row_h + 4, GREY, width=1.2))

    for i, (g, name) in enumerate(zip(D_GAPS, D_LABELS)):
        y = top + i * row_h
        win = g > 0.0
        colour = AMBER if win else C_BAR_OVER
        x_end = x_zero + g * px_per_m
        b.append(f'<rect x="{min(x_zero, x_end):.2f}" y="{y:.2f}" '
                 f'width="{abs(x_end - x_zero):.2f}" height="{row_h - 6:.2f}" '
                 f'fill="{colour}" stroke="none"/>')
        b.append(label(x_label, y + row_h - 10, name, cls="xs mono", anchor="start"))
        b.append(label(x_zero + 300, y + row_h - 10, f"{g:+.5f}",
                       cls="xs mono " + ("t-hi" if win else "muted"), anchor="end"))
        if i in (3, 6):
            b.append(rule(x_label, y - 3, x_zero + 300, y - 3, cls="grid"))

    return svg(uid, W, H, "All fifteen candidate gaps for two crossed planks",
               "Every face normal reports an overlap; the single edge-edge axis reports a "
               "five-centimetre gap.", b)


# ===========================================================================
# Figure 5 — the demo: the same geometry, two answers  (§7.4)
# ===========================================================================
def fig5():
    uid = "l84f5"
    W = 900
    crop = (0, 40, 960, 520)
    left, lw, lh = render_panel("_b84_p1.ppm", crop, 22, 78, 1.75, cell=4, levels=3)
    right, rw, rh = render_panel("_b84_p1f.ppm", crop, 22 + lw + 26, 78, 1.75, cell=4,
                                 levels=3)
    H = int(78 + max(lh, rh) + 24)
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="fill-bg" stroke="none"/>']
    b.append(label(22 + lw / 2, 46, "all fifteen candidates", cls="sm"))
    b.append(label(22 + lw / 2, 62, "green box — apart, and the amber bar proves it",
                   cls="xs muted"))
    b.append(label(22 + lw + 26 + rw / 2, 46, "six face normals only  [F]", cls="sm"))
    b.append(label(22 + lw + 26 + rw / 2, 62, "red box — the same geometry, called a collision",
                   cls="xs muted"))
    b += left
    b += right

    return svg(uid, W, H, "The collide demo, with and without the edge-edge axes",
               "Two screenshots of the same two planks: with all fifteen candidates they "
               "are correctly apart, with only the six face normals they are reported as "
               "colliding.", b)


# ===========================================================================
# Figure 6 — the MTV, and where it stops being an answer  (§8)
# ===========================================================================
def fig6():
    uid = "l84f6"
    W, H = 900, 320
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="fill-bg" stroke="none"/>']

    s = 62.0

    # Left: a shallow overlap. The shallowest way out is not the way it came in.
    cx, cy = 210, 170
    b.append(label(cx, 46, "a shallow overlap", cls="sm"))
    b.append(label(cx, 62, "depth 0.5000 m, on A's x face", cls="xs muted"))
    b.append(hollow(cx - s, cy - s, 2 * s, 2 * s, BLUE, width=1.6))
    off = (1.5 * s / 2, 0.2 * s / 2)
    b.append(hollow(cx - s + off[0] * 2, cy - s - off[1] * 2, 2 * s, 2 * s, RED, width=1.6))
    b.append(carrow(cx + off[0] * 2, cy - off[1] * 2, cx + off[0] * 2 + 0.5 * s,
                    cy - off[1] * 2, uid, "mtv", AMBER, width=2.2))
    b.append(label(cx + off[0] * 2 + 0.25 * s, cy - off[1] * 2 - 14, "0.50 m",
                   cls="xs mono t-hi"))
    b.append(label(cx, cy + s + 34, "it entered travelling down and left;", cls="xs muted"))
    b.append(label(cx, cy + s + 48, "the shortest way out is straight right", cls="xs muted"))

    # Right: deep overlap, where the MTV is a teleport.
    dx, dy = 640, 170
    b.append(label(dx, 46, "a deep one", cls="sm"))
    b.append(label(dx, 62, "depth 1.9000 m, on the same face", cls="xs muted"))
    b.append(hollow(dx - s, dy - s, 2 * s, 2 * s, BLUE, width=1.6))
    b.append(hollow(dx - s + 0.1 * s, dy - s - 0.05 * s, 2 * s, 2 * s, RED, width=1.6))
    b.append(carrow(dx + 0.1 * s, dy - 0.05 * s, dx + 0.1 * s + 1.9 * s, dy - 0.05 * s,
                    uid, "deep", AMBER, width=2.2))
    b.append(label(dx + 0.1 * s + 0.95 * s, dy - s - 16, "1.90 m", cls="xs mono t-hi"))
    b.append(label(dx, dy + s + 34, "the boxes are 10 cm apart and the", cls="xs muted"))
    b.append(label(dx, dy + s + 48, "correction is nearly two metres", cls="xs muted"))

    return svg(uid, W, H, "The minimum translation vector, and its limit",
               "A shallow overlap where the MTV is a sensible correction, and a deep one "
               "where the shallowest escape is nearly two metres.", b)


# ===========================================================================
# Figure 7 — a sphere against a box: three clamps  (§9)
# ===========================================================================
def fig7():
    uid = "l84f7"
    W, H = 900, 420
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="fill-bg" stroke="none"/>']

    # Left: the nine regions in 2-D (27 in 3-D) that the clamp does not have to
    # enumerate.
    cx, cy = 200, 180
    hw, hh = 78, 58
    b.append(label(cx, 46, "the nine regions you do not enumerate", cls="sm"))
    b.append(label(cx, 62, "in 3-D there are 27 of them", cls="xs muted"))
    b.append(hollow(cx - hw, cy - hh, 2 * hw, 2 * hh, BLUE, width=1.6))
    for gx in (-1, 0, 1):
        for gy in (-1, 0, 1):
            if gx == 0 and gy == 0:
                continue
            px = cx + gx * (hw + 42)
            py = cy + gy * (hh + 40)
            b.append(hollow(px - 30, py - 16, 60, 32, GREY, dash="3 4", rx=4))
    b.append(rule(cx - hw, cy - hh - 46, cx - hw, cy + hh + 46, cls="grid", dash="3 4"))
    b.append(rule(cx + hw, cy - hh - 46, cx + hw, cy + hh + 46, cls="grid", dash="3 4"))
    b.append(rule(cx - hw - 78, cy - hh, cx + hw + 78, cy - hh, cls="grid", dash="3 4"))
    b.append(rule(cx - hw - 78, cy + hh, cx + hw + 78, cy + hh, cls="grid", dash="3 4"))
    b.append(label(cx, cy + hh + 88, "each coordinate of the closest point", cls="xs muted"))
    b.append(label(cx, cy + hh + 102, "depends only on the same coordinate of p",
                   cls="xs muted"))

    # Middle: the clamp for a sphere outside.
    mx, my = 560, 150
    b.append(label(mx + 60, 46, "outside: clamp, then measure", cls="sm"))
    b.append(hollow(mx - 70, my - 44, 140, 88, BLUE, width=1.6))
    px, py = mx + 128, my - 74
    b.append(f'<circle cx="{px}" cy="{py}" r="34" fill="none" stroke="{GREEN}" '
             f'stroke-width="1.6"/>')
    b.append(f'<circle cx="{px}" cy="{py}" r="2.6" fill="{GREEN}" stroke="none"/>')
    b.append(f'<circle cx="{mx + 70}" cy="{my - 44}" r="3.0" fill="{AMBER}" stroke="none"/>')
    b.append(cline(mx + 70, my - 44, px, py, AMBER, width=1.6, dash="4 4"))
    b.append(label(mx + 70 - 34, my - 56, "closest point", cls="xs mono", anchor="middle"))
    b.append(label(px + 40, py + 4, "centre", cls="xs muted", anchor="start"))
    b.append(label(mx + 106, my - 8, "d", cls="xs mono t-hi"))
    b.append(label(mx + 60, my + 74, "depth = r − d,  normal along d", cls="xs mono"))

    # Bottom right: inside.
    ix, iy = 560, 316
    b.append(label(ix + 60, 262, "inside: the clamp does nothing", cls="sm"))
    b.append(hollow(ix - 70, iy - 34, 140, 68, BLUE, width=1.6))
    b.append(f'<circle cx="{ix + 10}" cy="{iy + 6}" r="26" fill="none" stroke="{RED}" '
             f'stroke-width="1.6"/>')
    b.append(f'<circle cx="{ix + 10}" cy="{iy + 6}" r="2.6" fill="{RED}" stroke="none"/>')
    b.append(carrow(ix + 10, iy + 6, ix + 10, iy + 40, uid, "out", AMBER, width=1.8))
    b.append(label(ix + 96, iy + 18, "nearest face", cls="xs mono", anchor="start"))
    b.append(label(ix + 60, iy + 74, "depth = r + inset,  normal = that face",
                   cls="xs mono"))

    return svg(uid, W, H, "A sphere against a box",
               "Three independent clamps find the closest point without enumerating the 27 "
               "regions, and the case where the centre is inside needs a different answer.",
               b)


# ===========================================================================
# Figure 8 — the degenerate axis, and which formulation cares  (§10)
# ===========================================================================
def fig8():
    uid = "l84f8"
    W, H = 900, 372
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="fill-bg" stroke="none"/>']

    # Left: why the degenerate pair is the common case.
    cx, cy = 190, 160
    b.append(label(cx, 46, "the parallel pair is the DEFAULT", cls="sm"))
    b.append(label(cx, 62, "two things on one floor, at any yaw", cls="xs muted"))
    b.append(hollow(cx - 130, cy + 46, 260, 22, BLUE, width=1.5))
    b.append(hollow(cx - 74, cy - 14, 62, 60, GREEN, width=1.5))
    b.append(hollow(cx + 20, cy - 4, 52, 50, GREEN, width=1.5))
    for x in (cx - 43, cx + 46):
        b.append(carrow(x, cy + 40, x, cy - 42, uid, f"up{int(x)}", AMBER, width=1.6))
    b.append(label(cx, cy - 58, "both up axes point the same way", cls="xs mono"))
    b.append(label(cx, cy + 96, "so a₁ × b₁ ≈ 0 — and never exactly 0,", cls="xs muted"))
    b.append(label(cx, cy + 110, "because both came through a quaternion", cls="xs muted"))
    b.append(label(cx, cy + 124, "conversion. Measured at 1.22e−07.", cls="xs muted"))

    # Right: the two formulations against the sweep.
    tx = 452
    b.append(label(tx, 46, "false separations, 400 overlapping pairs each",
                   cls="sm", anchor="start"))
    rows = []
    for tilt, ln, norm, absr, abse, n in F_CORNER:
        rows.append((("exactly shared" if tilt == 0.0 else f"{tilt:.0e}"),
                     fmt_e(ln), f"{norm}", f"{absr}", f"{abse}"))
    els, used = table(tx, 62, ("tilt, rad", "min |aᵢ×bⱼ|", "normalised", "absR",
                               "absR + ε"), rows, (108, 104, 92, 66, 78))
    b += els

    ty = 62 + used + 22
    b.append(label(tx, ty, "THE NORMALISED FORM NEVER FAILS, and that is a",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, ty + 14, "theorem: if two convex bodies overlap then NO",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, ty + 28, "direction separates them, so an axis made of pure",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, ty + 42, "rounding error reports an overlap like any other.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H, "The degenerate candidate axis",
               "Two objects on the same floor always have a parallel edge pair, and the two "
               "formulations of the edge-edge test respond to it completely differently.", b)


# ===========================================================================
# Figure 9 — where the precision floor is  (§11)
# ===========================================================================
def fig9():
    uid = "l84f9"
    W, H = 900, 348
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="fill-bg" stroke="none"/>']

    b.append(label(W / 2, 46, "the same two boxes, further from the origin", cls="sm"))
    b.append(label(W / 2, 62, "a 1 mm gap, reported in float", cls="xs muted"))

    px0, py0 = 96, 96
    pw, ph = 470, 210
    b.append(frame(px0, py0, pw, ph))

    lo_y, hi_y = 1e-9, 1e-2
    lo_x, hi_x = 1.0, 1e5

    def sx(v):
        return logspan(v, lo_x, hi_x, px0, pw)

    def sy(v):
        t = (math.log10(max(v, lo_y)) - math.log10(lo_y)) / (math.log10(hi_y) - math.log10(lo_y))
        return py0 + ph - min(max(t, 0.0), 1.0) * ph

    # decade grid
    for dxp in range(0, 6):
        x = sx(10.0 ** dxp)
        b.append(rule(x, py0, x, py0 + ph, cls="grid"))
        b.append(label(x, py0 + ph + 16, f"1e{dxp}", cls="xs mono muted"))
    for dyp in range(-9, -1):
        y = sy(10.0 ** dyp)
        b.append(rule(px0, y, px0 + pw, y, cls="grid"))
        b.append(label(px0 - 8, y + 4, f"1e{dyp}", cls="xs mono muted", anchor="end"))
    b.append(label(px0 + pw / 2, py0 + ph + 34, "distance from the origin, metres",
                   cls="xs muted"))

    # the 1 mm gap being measured
    y_gap = sy(G_GAP)
    b.append(cline(px0, y_gap, px0 + pw, y_gap, GREEN, width=1.6, dash="6 5"))

    # ulp(d) and the measured error
    ulp_pts = [(d, u) for d, _g, _e, u in G_ROWS if d >= 1.0]
    err_pts = [(d, e) for d, _g, e, _u in G_ROWS if d >= 1.0]
    for pts, colour, width in ((ulp_pts, GREY, 1.6), (err_pts, RED, 2.2)):
        path = " ".join(f"{'M' if i == 0 else 'L'}{sx(d):.1f},{sy(v):.1f}"
                        for i, (d, v) in enumerate(pts))
        b.append(f'<path d="{path}" fill="none" stroke="{colour}" stroke-width="{width}"/>')
        for d, v in pts:
            b.append(f'<circle cx="{sx(d):.1f}" cy="{sy(v):.1f}" r="3" fill="{colour}" '
                     f'stroke="none"/>')

    b += legend(px0 + pw + 34, py0 + 18,
                [(RED, "error in the reported gap"),
                 (GREY, "ulp(d) — the spacing of floats there"),
                 (GREEN, "the quantity being measured")])

    b.append(label(px0 + pw + 34, py0 + 92, "The two meet at 10 km, and", cls="xs muted",
                   anchor="start"))
    b.append(label(px0 + pw + 34, py0 + 106, "that is the whole story: the", cls="xs muted",
                   anchor="start"))
    b.append(label(px0 + pw + 34, py0 + 120, "subtraction b.centre − a.centre", cls="xs muted",
                   anchor="start"))
    b.append(label(px0 + pw + 34, py0 + 134, "is performed on numbers of the", cls="xs muted",
                   anchor="start"))
    b.append(label(px0 + pw + 34, py0 + 148, "size of the WORLD to produce", cls="xs muted",
                   anchor="start"))
    b.append(label(px0 + pw + 34, py0 + 162, "one of the size of a CRATE.", cls="xs muted",
                   anchor="start"))

    return svg(uid, W, H, "Where the precision floor is",
               "The error in a reported gap tracks the spacing of floats at the boxes' "
               "distance from the origin, and crosses the gap being measured at ten "
               "kilometres.", b)


# ===========================================================================
# Figure 10 — the budget  (§12)
# ===========================================================================
def fig10():
    uid = "l84f10"
    W, H = 900, 420
    b = [f'<rect x="0" y="0" width="{W}" height="{H}" class="fill-bg" stroke="none"/>']

    # Left: cost per test.
    b.append(label(232, 46, "nanoseconds per test", cls="sm"))
    b.append(label(232, 62, "20,000 pairs, median of 21 runs", cls="xs muted"))

    rows = [("sphere vs sphere", I_SPHERE, GREEN),
            ("bounds_of(obb)", I_BOUNDS, GREY),
            ("aabb vs aabb", I_AABB, GREEN),
            ("obb vs obb, spread", I_SPREAD, BLUE),
            ("obb vs obb, crowded", I_CROWD, RED),
            ("overlaps(), crowded", I_OVERLAPS, AMBER)]
    x0, top = 40, 86
    bw = 250.0
    row_h = 30.0
    worst = max(v for _n, v, _c in rows)
    for i, (name, v, colour) in enumerate(rows):
        y = top + i * row_h
        w = bw * v / worst
        b.append(label(x0, y + 10, name, cls="xs", anchor="start"))
        b.append(f'<rect x="{x0 + 150:.1f}" y="{y:.1f}" width="{w:.1f}" height="14" '
                 f'fill="{colour}" stroke="none" rx="2"/>')
        b.append(label(x0 + 150 + w + 8, y + 11, f"{v:.2f}", cls="xs mono", anchor="start"))

    yb = top + len(rows) * row_h + 14
    b.append(rule(x0, yb, x0 + 420, yb, cls="grid"))
    b.append(label(x0, yb + 20, f"The boolean-only form is {I_RATIO:.2f}× the full one on the",
                   cls="xs muted", anchor="start"))
    b.append(label(x0, yb + 34, f"same pairs, for the same answers: {I_HIT:.1f} ns against",
                   cls="xs muted", anchor="start"))
    b.append(label(x0, yb + 48, f"{I_OVERLAPS:.1f}. Everything it saves is square roots.",
                   cls="xs muted", anchor="start"))

    # Right: how many axes a query actually asks for.
    hx = 530
    b.append(label(hx + 160, 46, "axes examined per query", cls="sm"))
    b.append(label(hx + 160, 62, f"spread population, mean {I_AXES_SPREAD} of 15",
                   cls="xs muted"))
    hb_top = 86
    hb_h = 16.0
    hw = 240.0
    worst_pct = max(p for _n, _c, p in I_HIST)
    for i, (n, _c, pct) in enumerate(I_HIST):
        y = hb_top + i * hb_h
        w = hw * pct / worst_pct
        colour = AMBER if n == 15 else (GREEN if n <= 3 else C_BAR_OVER)
        b.append(label(hx, y + 11, f"{n}", cls="xs mono", anchor="end"))
        b.append(f'<rect x="{hx + 8:.1f}" y="{y:.1f}" width="{max(w, 0.8):.1f}" '
                 f'height="{hb_h - 5:.1f}" fill="{colour}" stroke="none"/>')
        if pct >= 0.5:
            b.append(label(hx + 8 + w + 8, y + 11, f"{pct:.1f}%", cls="xs mono",
                           anchor="start"))

    yh = hb_top + len(I_HIST) * hb_h + 16
    b.append(rule(hx - 20, yh, hx + 330, yh, cls="grid"))
    b.append(label(hx - 20, yh + 20, "91% of separated pairs exit on one of the first",
                   cls="xs muted", anchor="start"))
    b.append(label(hx - 20, yh + 34, "three axes; 6.4% run the whole list, and those are",
                   cls="xs muted", anchor="start"))
    b.append(label(hx - 20, yh + 48, "exactly the pairs that overlap.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H, "The budget",
               "Cost per test for each primitive pair, and the distribution of how many of "
               "the fifteen candidate axes a query actually examines.", b)


def main():
    figs = [("l84_fig1.svg", fig1), ("l84_fig2.svg", fig2), ("l84_fig3.svg", fig3),
            ("l84_fig4.svg", fig4), ("l84_fig5.svg", fig5), ("l84_fig6.svg", fig6),
            ("l84_fig7.svg", fig7), ("l84_fig8.svg", fig8), ("l84_fig9.svg", fig9),
            ("l84_fig10.svg", fig10)]
    for name, fn in figs:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}  ({os.path.getsize(path):,} bytes)")


if __name__ == "__main__":
    main()
