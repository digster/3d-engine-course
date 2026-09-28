#!/usr/bin/env python3
"""scratch/figs_82.py — Lesson 8.2's diagrams.

Same rules as 5.1-8.1's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - the figure palette must contain the DEMO's own colours (rle_rects SNAPS)

Every number below is transcribed from scratch/verify_82.log. Nothing here is
estimated, and the section of the harness each block came from is named above it.

*** THIS FILE IS WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. *** 8.1's
figs file ended up mixing the two because successive patch scripts wrote both,
and a str.replace written against one form fails SILENTLY against the other —
three label fixes were lost that way and only check-page.js noticed. One form,
and every patch script against this file asserts its replacement count.

ONE COLOUR RULE FOR THIS LESSON. In the MASS figures, AMBER is the light body
(0.1 kg), GREEN is 1 kg and BLUE is 10 kg — demos/bodies/main.cpp's own three
constants. In the FRAME figures, GREEN is world space (the correct one), RED is
the uniformly scaled parent, AMBER is the sheared one and PURPLE is the spinning
one — again the demo's own. That matters for figure 10, where `rle_rects` snaps a
screenshot to a palette and the palette has to contain the colours the
screenshot actually used.
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

# demos/bodies/main.cpp's constants, transcribed. See the module docstring.
B_LIGHT = "#ebc860"      # 0.1 kg
B_MID = "#78dca0"        # 1 kg   — and world space, in the frame panel
B_HEAVY = "#96a0ff"      # 10 kg
B_SCALE2 = "#eb6060"
B_SHEAR = "#eb9650"
B_SPIN = "#be78eb"
B_GROUND = "#78604 0".replace(" ", "")
B_AXIS = "#545c6c"
B_GRID = "#262a34"
BODIES_PALETTE = [hexrgb(c) for c in (B_LIGHT, B_MID, B_HEAVY, B_SCALE2,
                                      B_SHEAR, B_SPIN, B_GROUND, B_AXIS, B_GRID)]

G = 9.81
H60 = 1.0 / 60.0

# ---- transcribed from verify_82.log ---------------------------------------

# A — one force, three masses
FMA = [(1.0, 1.000000, 50.0000, 0.833333),
       (10.0, 0.100000, 5.0000, 0.083333),
       (1000.0, 0.001000, 0.0500, 0.000833)]
FMA_RATIO = 999.9999

# B — the accumulator
B_FORCES = [("gravity", (0.0, -19.62, 0.0)),
            ("thruster", (14.0, 0.0, 0.0)),
            ("wind", (-4.5, 1.25, 3.0)),
            ("drag", (-2.25, 0.5, -1.5))]
B_SUM = (7.250, -17.870, 1.500)
B_ACCEL = (3.6250, -8.9350, 0.7500)
B_SPREAD_LO, B_SPREAD_HI = 0.125000000, 0.128700003
B_SPREAD = 3.7000e-03
B_SMALLEST = 3.7000e-03
B_SPREAD_PCT = 2.9

# C — the round trip
C_CHECKED = 1000001
C_INEXACT = 159937
C_INEXACT_PCT = 15.9937
C_WORST = 9.537e-07
C_ULP_G = 9.537e-07
C_BAD_MASS = 1.000145
# steps, v(1 kg), v(odd), apart — the FORCE route
C_FORCE = [(1, -0.163500, -0.163500, 1.490e-08),
           (60, -9.809996, -9.809995, 9.537e-07),
           (600, -98.099884, -98.099884, 0.0)]
C_ULP_V1 = 1.490e-08

# D — damping is not drag
D_K = 0.5
D_B = 0.5
D_DAMPED = [(0.10, 19.5383), (1.00, 19.5383), (100.00, 19.5383)]
D_DAMPED_CONT = 19.6200
D_DAMPED_DISCRETE = 19.5384
D_DRAGGED = [(0.10, 1.9607, 1.9620, 1.4),
             (1.00, 19.6026, 19.6200, 14.0),
             (100.00, 1960.1298, 1962.0001, 1400.0)]
D_FREEFALL_60 = 588.62

# E — the jump
E_V0 = 4.4294
E_J = 310.06
E_FORCE_1STEP = 18604
E_RATES = [30.0, 60.0, 120.0, 144.0]
E_IMPULSE = [0.9275, 0.9632, 0.9816, 0.9846]
E_FORCE = [3.8528, 0.9632, 0.2408, 0.1672]
E_IMPULSE_SPREAD_PCT = 6.16
E_FORCE_SPREAD_X = 23.0
E_PRED = [0.926176, 0.963088, 0.981544, 0.984620]
E_MEAS = [0.927527, 0.963201, 0.981596, 0.984625]
E_CONTROL = [0.9275, 0.9632, 0.9631, 0.9692]

# F — inverse mass zero
F_HEAVY_FALL = -4.986749      # 1,000,000 kg DYNAMIC body, 1 s
F_PEBBLE_FALL = -4.986749     # 10 g body, 1 s

# G — units
G_FALL_1M = 0.451524
G_SCALES = [0.25, 0.50, 1.00, 2.00, 4.00]
G_TIMES = [1.211565, 0.856706, 0.605783, 0.428353, 0.302891]
G_RATIOS = [2.0000, 1.4142, 1.0000, 0.7071, 0.5000]
G_OVERCRANK = 2.8284
G_SLOP = [1.25, 2.50, 5.00, 10.00, 20.00]

# H — frames
H_FRAMES = [
    ("translate · rotate 45°", (6.9367, -6.9367), 1.0000, 45.000, 0.0000, True, True),
    ("uniform scale 2", (0.0000, -19.6200), 2.0000, 0.000, 0.0000, True, False),
    ("scale (2,1,1)", (0.0000, -9.8100), 1.0000, 0.000, 0.0000, False, False),
    ("scale (2,1,1) after 45°", (13.8734, -6.9367), 1.5811, 63.435, 0.6000, False, False),
]
H_LOCAL_FALL = -4.986749
H_WORLD_FALL = -9.973498
H_SPIN_TRUE = (1.0000, 2.0333)
H_SPIN_GOT = (1.4024, -1.7416)
H_SPIN_APART = 3.7963

# I — the budget
I_ROWS = [
    ("weight through the accumulator", 2.136, 1.923),
    ("gravity added after the divide", 1.780, 1.617),
    ("a sqrt per body", 1.404, 1.282),
    ("squared, one sqrt at the end", 1.333, 1.200),
    ("with max_speed and momentum", 1.495, 1.414),
    ("no report at all", 1.139, 1.099),
]
I_DIVIDE_RATIO = [0.845, 0.848, 0.847]
I_SQRT_RATIO = [1.036, 0.968, 0.969]
I_STEP_PLAIN = 2.706
I_STEP_DAMPED = 4.222
I_STEP_FIXED = 0.712
I_81_STEP = 0.963


def cbox(x, y, w, h, colour, dash=None):
    return hollow(x, y, w, h, colour, dash=dash, width=1.3)


def opoly(points, colour, width=1.4, dash=None, offset=0.0):
    """`poly`, plus a stroke-dashoffset.

    THE DEMO'S OWN PROBLEM, SOLVED THE DEMO'S OWN WAY. Where three curves lie
    exactly on top of one another, three solid strokes are indistinguishable
    from one — and the picture proving they agree looks exactly like a picture
    of a bug that lost two of them. Interleaved dashes make the agreement the
    thing you see. demos/bodies/main.cpp does this in pixels; this does it in
    SVG, with the same phases.
    """
    d = "M " + " L ".join(f"{x:.2f},{y:.2f}" for x, y in points)
    da = f' stroke-dasharray="{dash}" stroke-dashoffset="{offset:.1f}"' if dash else ""
    return f'<path d="{d}" fill="none" stroke="{colour}" stroke-width="{width}"{da}/>'


# ===========================================================================
# Figure 1 — the accumulator, and the setter that loses three quarters of it
# ===========================================================================
def fig1():
    uid = "l82f1"
    W, H = 900, 430
    b = [cmarker(uid, "sum", GREEN), cmarker(uid, "one", GREY)]

    b.append(label(40, 30, "FOUR SYSTEMS, FOUR CALLS, NO COORDINATION",
                   cls="sm t-hi", anchor="start"))

    # THE X AND Y AXES SHARE ONE SCALE, which is not a detail. A vector diagram
    # drawn with two scales distorts every direction in it, and direction is the
    # only thing this figure is about — the first version of this figure used
    # 1.30 px/N across and 2.60 px/N up, and the sum arrow pointed somewhere the
    # body was never going.
    px_per_n = 8.0
    pw, ph = 330, 290
    x0, y0 = 60, 66
    cx, cy = x0 + 150, y0 + 66

    b.append(frame(x0, y0, pw, ph))
    b.append(label(x0, y0 - 12, "tip to tail, in the order they were called",
                   cls="xs muted", anchor="start"))
    b.append(label(x0 + pw, y0 - 12, f"{px_per_n:.0f} px = 1 N, both axes",
                   cls="xs mono muted", anchor="end"))
    b.append(rule(x0, cy, x0 + pw, cy, cls="grid", width=0.9))
    b.append(rule(cx, y0, cx, y0 + ph, cls="grid", width=0.9))
    b.append(f'<circle cx="{cx}" cy="{cy}" r="7" fill="{GREY}" fill-opacity="0.45"/>')
    b.append(label(cx - 14, cy - 10, "2 kg", cls="xs muted", anchor="end"))

    px_, py_ = cx, cy
    cols = [GREY, BLUE, AMBER, PURPLE]
    for (_name, vec), col in zip(B_FORCES, cols):
        nx = px_ + vec[0] * px_per_n
        ny = py_ - vec[1] * px_per_n
        b.append(carrow(px_, py_, nx, ny, uid, "one", col, width=1.7, dash="3 3"))
        px_, py_ = nx, ny

    b.append(carrow(cx, cy, px_, py_, uid, "sum", GREEN, width=2.8))

    # Legend and the arithmetic, OUTSIDE the plot.
    items = [(col, f"{name}  ({vec[0]:g}, {vec[1]:g}, {vec[2]:g}) N")
             for (name, vec), col in zip(B_FORCES, cols)]
    items.append((GREEN, f"sum  ({B_SUM[0]:.3f}, {B_SUM[1]:.3f}, {B_SUM[2]:.3f}) N"))
    b += legend(430, 96, items)

    b.append(label(430, 196, "…and ONE divide, at the end, once:",
                   cls="xs", anchor="start"))
    b.append(label(430, 216,
                   f"a = F · (1/m) = ({B_ACCEL[0]:.4f}, {B_ACCEL[1]:.4f}, "
                   f"{B_ACCEL[2]:.4f}) m/s²",
                   cls="xs mono t-ok", anchor="start"))
    b.append(label(430, 236, "The order of the four calls does not matter:",
                   cls="xs muted", anchor="start"))
    b.append(label(430, 252, "vector addition commutes. §3.4 measures the one",
                   cls="xs muted", anchor="start"))
    b.append(label(430, 268, "way in which floating point does not.",
                   cls="xs muted", anchor="start"))

    # The failure mode: a setter.
    b.append(label(430, 306, "THE SAME FOUR SYSTEMS, WITH A SETTER",
                   cls="xs t-hi", anchor="start"))
    b.append(label(430, 326, "b.force = thruster;   ← the last writer wins",
                   cls="xs mono", anchor="start"))
    b.append(label(430, 346,
                   "F = (14.000, 0.000, 0.000) N — gravity, wind and",
                   cls="xs t-bad", anchor="start"))
    b.append(label(430, 362, "drag are simply gone, silently.",
                   cls="xs t-bad", anchor="start"))
    b.append(label(430, 388, "Forces add because Newton's second law is linear",
                   cls="xs muted", anchor="start"))
    b.append(label(430, 404, "in F. That one fact is why a physics engine has an",
                   cls="xs muted", anchor="start"))
    b.append(label(430, 420, "accumulator, and why these four need no interface.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "Four forces added tip to tail at one scale, against the same four assigned",
               "Four dashed arrows run tip to tail from a body — gravity down, a thruster right, "
               "wind and drag back to the left — and a solid green arrow spans from the body to "
               "the final tip. Beside it, the same four forces written with an assignment leave "
               "only the thruster.", b)


# ===========================================================================
# Figure 2 — inverse mass, and the two questions
# ===========================================================================
def fig2():
    uid = "l82f2"
    W, H = 900, 500
    b = [cmarker(uid, "map", GREY)]

    b.append(label(40, 30, "WHY THE RECIPROCAL IS THE ONE THAT GETS STORED",
                   cls="sm t-hi", anchor="start"))

    # Two axes: mass on top, inverse mass below, with the map between them.
    ax0, ax1 = 80, 500
    ytop, ybot = 92, 190
    b.append(rule(ax0, ytop, ax1, ytop, cls="ink-soft", width=1.4))
    b.append(rule(ax0, ybot, ax1, ybot, cls="ink-soft", width=1.4))
    b.append(label(ax0 - 12, ytop + 4, "m", cls="xs mono", anchor="end"))
    b.append(label(ax0 - 12, ybot + 4, "1/m", cls="xs mono", anchor="end"))

    # Ticks at 0, 1, 10, 1000, inf — log-ish, placed by hand-free computation.
    stops = [(0.0, "0", "∞"), (0.1, "0.1", "10"), (1.0, "1", "1"),
             (10.0, "10", "0.1"), (1000.0, "1000", "0.001"), (None, "∞", "0")]
    n = len(stops)
    for i, (_m, mlab, wlab) in enumerate(stops):
        x = ax0 + (ax1 - ax0) * i / (n - 1)
        b.append(rule(x, ytop - 6, x, ytop + 6, cls="ink-soft", width=1.2))
        b.append(rule(x, ybot - 6, x, ybot + 6, cls="ink-soft", width=1.2))
        b.append(label(x, ytop - 12, mlab, cls="xs mono"))
        b.append(label(x, ybot + 20, wlab, cls="xs mono"))
        b.append(rule(x, ytop + 8, x, ybot - 8, cls="grid", width=0.9, dash="2 3"))

    # The two ends that matter, marked on both lines.
    xleft = ax0
    xright = ax1
    b.append(f'<circle cx="{xright}" cy="{ybot}" r="5.5" fill="{GREEN}"/>')
    b.append(f'<circle cx="{xleft}" cy="{ybot}" r="5.5" fill="{RED}" '
             f'fill-opacity="0.35" stroke="{RED}"/>')
    b.append(label(xright, ybot + 44, "IMMOVABLE", cls="xs t-ok"))
    b.append(label(xright, ybot + 60, "a float, exactly", cls="xs muted"))
    b.append(label(xleft, ybot + 44, "massless", cls="xs t-bad"))
    b.append(label(xleft, ybot + 60, "no divide happens", cls="xs muted"))

    b.append(label(ax1, ytop - 40,
                   "the floor, every wall and every rock live at this end →",
                   cls="xs muted", anchor="end"))
    b.append(label(ax1, ytop - 24,
                   "which is most of the bodies in a real scene",
                   cls="xs muted", anchor="end"))

    # The arithmetic, right-hand column.
    tx = 540
    b.append(label(tx, 96, "AND THE ARITHMETIC THE SOLVER ACTUALLY DOES",
                   cls="xs t-hi", anchor="start"))
    b.append(label(tx, 118, "mass_of(floor) − mass_of(wall)  =  NaN",
                   cls="xs mono t-bad", anchor="start"))
    b.append(label(tx, 138, "inv_a + inv_b                   =  0",
                   cls="xs mono t-ok", anchor="start"))
    b.append(label(tx, 158, "1 / (inv_a + inv_b)             =  ∞",
                   cls="xs mono", anchor="start"))
    b.append(label(tx, 176, "…which is the reduced mass, and is right.",
                   cls="xs muted", anchor="start"))

    # The two questions, as a 2x2.
    qy = 306
    b.append(label(40, qy - 12, "TWO QUESTIONS, NOT ONE — WHICH IS WHY body_kind IS "
                   "NOT A BOOL", cls="sm t-hi", anchor="start"))
    cw, ch = 190, 62
    col_x = [230, 430]
    row_y = [qy + 28, qy + 28 + ch + 14]
    b.append(label(col_x[0] + cw / 2, qy + 20, "moves?  no", cls="xs muted"))
    b.append(label(col_x[1] + cw / 2, qy + 20, "moves?  yes", cls="xs muted"))
    b.append(label(220, row_y[0] + ch / 2, "a force can move it: no", cls="xs muted",
                   anchor="end"))
    b.append(label(220, row_y[1] + ch / 2, "a force can move it: yes", cls="xs muted",
                   anchor="end"))

    cells = [(0, 0, "fixed", "the ground, a wall", GREY),
             (1, 0, "kinematic", "a lift, a door", BLUE),
             (0, 1, None, "nothing is this", None),
             (1, 1, "dynamic", "a crate, a ragdoll", GREEN)]
    for cx_i, cy_i, name, note, col in cells:
        x = col_x[cx_i]
        y = row_y[cy_i]
        if name is None:
            b.append(box(x, y, cw, ch, dash="4 4"))
            b.append(label(x + cw / 2, y + ch / 2 + 4, note, cls="xs muted"))
            continue
        b.append(cbox(x, y, cw, ch, col))
        b.append(label(x + cw / 2, y + 26, name, cls="xs mono"))
        b.append(label(x + cw / 2, y + 46, note, cls="xs muted"))

    b.append(label(640, row_y[0] + 26,
                   "inv_mass answers the row.", cls="xs", anchor="start"))
    b.append(label(640, row_y[0] + 44,
                   "body_kind answers the column.", cls="xs", anchor="start"))
    b.append(label(640, row_y[1] + 26,
                   "Conflate them and a lift either", cls="xs muted", anchor="start"))
    b.append(label(640, row_y[1] + 44,
                   "sags under load or leaves its", cls="xs muted", anchor="start"))
    b.append(label(640, row_y[1] + 62,
                   "passengers behind.", cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "The map from mass to inverse mass, and the two questions body kind answers",
               "Two parallel number lines show mass running from zero to infinity above and "
               "its reciprocal running from infinity to zero below, with immovable landing on "
               "an exact zero. Below them a two-by-two grid separates whether a force can move "
               "a body from whether it moves at all.", b)


# ===========================================================================
# Figure 3 — gravity is mass-blind, and the round trip that is not exact
# ===========================================================================
def fig3():
    uid = "l82f3"
    W, H = 900, 400
    b = [cmarker(uid, "g", GREEN), cmarker(uid, "r", RED)]

    b.append(label(40, 30, "THE ONE FORCE WHOSE MASS CANCELS — AND THE TWO ROUTES "
                   "TO SAYING SO", cls="sm t-hi", anchor="start"))

    # The two code paths, as boxes.
    bw, bh = 150, 46
    y0, y1 = 80, 176
    xs = [60, 240, 420]

    def path(y, title, steps, colour, note):
        out = [label(60, y - 12, title, cls="xs t-hi", anchor="start")]
        for i, (txt, sub) in enumerate(steps):
            x = xs[i]
            out.append(cbox(x, y, bw, bh, colour))
            out.append(label(x + bw / 2, y + 20, txt, cls="xs mono"))
            out.append(label(x + bw / 2, y + 37, sub, cls="xs muted"))
            if i:
                out.append(carrow(xs[i - 1] + bw + 4, y + bh / 2, x - 4, y + bh / 2,
                                  uid, "g" if colour is GREEN else "r", colour,
                                  width=1.5))
        out.append(label(xs[2] + bw + 18, y + bh / 2 + 4, note, cls="xs",
                         anchor="start"))
        return out

    b += path(y0, "THE OBVIOUS ONE: weight is a force",
              [("F += m·g", "a DIVIDE: m = 1/inv_mass"),
               ("a = F · inv_mass", "the accumulator's payoff"),
               ("integrate(a)", "8.1, unchanged")],
              RED, "15% of the update, and")
    b.append(label(xs[2] + bw + 18, y0 + bh / 2 + 20, "exact only by luck",
                   cls="xs t-bad", anchor="start"))

    b += path(y1, "THE ONE THIS ENGINE SHIPS: gravity is an acceleration",
              [("a = F · inv_mass", "no mass anywhere"),
               ("a += g · scale", "one add"),
               ("integrate(a)", "8.1, unchanged")],
              GREEN, "exact by construction,")
    b.append(label(xs[2] + bw + 18, y1 + bh / 2 + 20, "and 15% cheaper",
                   cls="xs t-ok", anchor="start"))

    # The measurement that decides it.
    ty = 262
    b.append(label(40, ty, "THE SWEEP THAT SETTLED IT — a = (g / inv_mass) · inv_mass",
                   cls="xs t-hi", anchor="start"))
    rows = [(f"{C_CHECKED:,} masses, 1 g to 1000 t", "checked", ""),
            (f"{C_INEXACT:,} of them ({C_INEXACT_PCT:.2f}%)",
             "do not recover g", ""),
            (f"worst error {C_WORST:.3e} m/s²",
             f"= {C_WORST / C_ULP_G:.2f} ulp", "")]
    for i, (a_, b_, _c) in enumerate(rows):
        b.append(label(60, ty + 24 + i * 18, a_, cls="xs mono", anchor="start"))
        b.append(label(360, ty + 24 + i * 18, b_, cls="xs muted", anchor="start"))

    b.append(label(540, ty + 24, "Seven hand-picked masses all passed.",
                   cls="xs", anchor="start"))
    b.append(label(540, ty + 42, "A million did not, and the ones that fail",
                   cls="xs muted", anchor="start"))
    b.append(label(540, ty + 60, "are not at the extremes — the first is",
                   cls="xs muted", anchor="start"))
    b.append(label(540, ty + 78, f"{C_BAD_MASS:.6f} kg.", cls="xs mono t-hi",
                   anchor="start"))
    b.append(label(540, ty + 100, "One ulp is invisible in a fall and fatal",
                   cls="xs", anchor="start"))
    b.append(label(540, ty + 118, "to a replay. §5.4 has the table.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "Two arrangements of the same physics, and the sweep that chose between them",
               "The upper path puts weight into the force accumulator and needs a divide to "
               "recover the mass; the lower adds gravity as an acceleration after the division. "
               "Below, a sweep of a million masses finds sixteen percent of them fail to "
               "recover gravity exactly through the first route.", b)


# ===========================================================================
# Figure 4 — damping is not drag
# ===========================================================================
def fig4():
    uid = "l82f4"
    W, H = 900, 430
    b = []

    b.append(label(40, 30, "TWO KNOBS THAT LOOK THE SAME AND ARE NOT",
                   cls="sm t-hi", anchor="start"))

    # Two panels of v(t), one per model, three masses each.
    pw, ph = 330, 240
    py = 70
    panels = [(60, "DAMPING   v ×= exp(−k h)", "g / k", True),
              (470, "DRAG FORCE   F = −b v", "m g / b", False)]

    T_MAX = 26.0
    V_MAX = 30.0

    def vx(t, x0):
        return x0 + (t / T_MAX) * pw

    def vy(v):
        return py + ph - (min(v, V_MAX) / V_MAX) * ph

    for x0, title, formula, is_damped in panels:
        b.append(frame(x0, py, pw, ph))
        b.append(label(x0, py - 12, title, cls="xs t-hi", anchor="start"))
        b.append(label(x0 + pw, py - 12, f"v∞ = {formula}", cls="xs mono muted",
                       anchor="end"))
        # axes
        for v in (0, 10, 20, 30):
            yy = vy(v)
            b.append(rule(x0, yy, x0 + pw, yy, cls="grid", width=0.8))
            b.append(label(x0 - 8, yy + 4, str(v), cls="xs mono muted", anchor="end"))
        b.append(label(x0 + pw / 2, py + ph + 34, "seconds", cls="xs muted"))
        b.append(label(x0 - 44, py + ph / 2, "m/s", cls="xs muted"))
        for t in (0, 5, 10, 15, 20, 25):
            xx = vx(t, x0)
            b.append(rule(xx, py, xx, py + ph, cls="grid", width=0.8))
            b.append(label(xx, py + ph + 16, str(t), cls="xs mono muted"))

        for j, ((mass, _meas), col) in enumerate(
                zip(D_DAMPED, (B_LIGHT, B_MID, B_HEAVY))):
            tau = (1.0 / D_K) if is_damped else (mass / D_B)
            vinf = (G / D_K) if is_damped else (mass * G / D_B)
            pts = []
            for i in range(0, 241):
                t = T_MAX * i / 240.0
                v = vinf * (1.0 - math.exp(-t / tau))
                pts.append((vx(t, x0), vy(v)))
                if v > V_MAX:
                    break
            # INTERLEAVED DASHES ON BOTH PANELS, not only the left one. On the
            # left the three curves coincide and the dashes are what makes that
            # visible; on the right they separate and the same dashing keeps the
            # two panels comparable, so a reader is never asked to decide
            # whether a difference in line style means something.
            b.append(opoly(pts, col, width=2.4, dash="9 18", offset=-9.0 * j))

        if is_damped:
            b.append(rule(x0, vy(D_DAMPED_CONT), x0 + pw, vy(D_DAMPED_CONT),
                          cls="grid", width=1.2, dash="4 3"))

    # Legend and the numbers, OUTSIDE both plots.
    b += legend(60, py + ph + 62,
                [(B_LIGHT, "0.10 kg"), (B_MID, "1.00 kg"), (B_HEAVY, "100.00 kg")],
                dy=16)

    b.append(label(220, py + ph + 62, "All three damped curves are ONE curve.",
                   cls="xs t-bad", anchor="start"))
    b.append(label(220, py + ph + 80,
                   f"Measured terminal speed {D_DAMPED[0][1]:.4f} m/s at every mass.",
                   cls="xs mono muted", anchor="start"))
    b.append(label(220, py + ph + 98,
                   f"g/k says {D_DAMPED_CONT:.4f}; the DISCRETE fixed point of the",
                   cls="xs muted", anchor="start"))
    b.append(label(220, py + ph + 116,
                   f"step is g·h·e^(−kh)/(1 − e^(−kh)) = {D_DAMPED_DISCRETE:.4f}.",
                   cls="xs mono muted", anchor="start"))

    b.append(label(600, py + ph + 62, "The drag curves separate by 1000×,",
                   cls="xs t-ok", anchor="start"))
    b.append(label(600, py + ph + 80, "and so do their time constants:",
                   cls="xs muted", anchor="start"))
    b.append(label(600, py + ph + 98, "τ = m/b is 0.2 s, 2 s and 200 s.",
                   cls="xs mono muted", anchor="start"))
    b.append(label(600, py + ph + 116, "The heavy one is off the top of its panel.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "Speed against time for three masses under velocity damping and under a drag force",
               "On the left the three masses trace a single indistinguishable curve to the same "
               "terminal speed. On the right they separate completely, the lightest levelling off "
               "near two metres per second and the heaviest still climbing off the top of the "
               "panel.", b)


# ===========================================================================
# Figure 5 — an impulse has no h in it
# ===========================================================================
def fig5():
    uid = "l82f5"
    W, H = 900, 480
    b = []

    b.append(label(40, 30, "THE SAME JUMP, FOUR MONITORS", cls="sm t-hi",
                   anchor="start"))
    b.append(label(40, 50, f"70 kg, aiming at 1.00 m: v₀ = {E_V0:.4f} m/s, "
                   f"J = {E_J:.2f} N·s, and the force tuned at 60 Hz is "
                   f"{E_FORCE_1STEP:,} N for one step.",
                   cls="xs muted", anchor="start"))

    # Two bar panels sharing a LOG height axis, because the force column spans 23x.
    pw, ph = 330, 230
    py = 92
    LO, HI = 0.1, 5.0

    def hy(v):
        t = (math.log10(max(v, LO)) - math.log10(LO)) / (math.log10(HI) - math.log10(LO))
        return py + ph - t * ph

    for x0, title, data, col in ((60, "add_impulse(J)", E_IMPULSE, GREEN),
                                 (470, "add_force(J/h) for one step", E_FORCE, RED)):
        b.append(frame(x0, py, pw, ph))
        b.append(label(x0, py - 12, title, cls="xs mono t-hi", anchor="start"))
        for v in (0.1, 0.25, 0.5, 1.0, 2.0, 4.0):
            yy = hy(v)
            b.append(rule(x0, yy, x0 + pw, yy, cls="grid", width=0.8))
            b.append(label(x0 - 8, yy + 4, f"{v:g}", cls="xs mono muted", anchor="end"))
        # the 1.00 m target
        b.append(rule(x0, hy(1.0), x0 + pw, hy(1.0), cls="ink-soft", width=1.2,
                      dash="4 3"))

        slot = pw / len(E_RATES)
        for i, (rate, v) in enumerate(zip(E_RATES, data)):
            bx = x0 + i * slot + slot * 0.28
            bw_ = slot * 0.44
            top = hy(v)
            b.append(f'<rect x="{bx:.1f}" y="{top:.1f}" width="{bw_:.1f}" '
                     f'height="{py + ph - top:.1f}" fill="{col}" '
                     f'fill-opacity="0.45" stroke="{col}" stroke-width="1.2"/>')
            # Above the bar, unless that would put the digits on the frame's top
            # edge (3.853 at 30 Hz nearly fills the axis) — then just inside it.
            ly = top - 8 if top - 18 > py else top + 14
            b.append(label(bx + bw_ / 2, ly, f"{v:.3f}", cls="xs mono"))
            b.append(label(bx + bw_ / 2, py + ph + 16, f"{rate:.0f} Hz",
                           cls="xs mono muted"))
        b.append(label(x0 + pw / 2, py + ph + 36, "peak height, metres, log axis",
                       cls="xs muted"))

    b.append(label(60, py + ph + 70,
                   f"spread {E_IMPULSE_SPREAD_PCT:.2f}%", cls="sm t-ok",
                   anchor="start"))
    b.append(label(60, py + ph + 90,
                   "…and the residue is 8.1's, not this lesson's:",
                   cls="xs muted", anchor="start"))
    b.append(label(60, py + ph + 108,
                   "semi-implicit Euler's peak is v₀²/2g − v₀h/2, predicted",
                   cls="xs muted", anchor="start"))
    b.append(label(60, py + ph + 126,
                   f"{E_PRED[0]:.6f} and measured {E_MEAS[0]:.6f} at 30 Hz.",
                   cls="xs mono muted", anchor="start"))

    b.append(label(470, py + ph + 70,
                   f"spread {E_FORCE_SPREAD_X:.1f}×", cls="sm t-bad", anchor="start"))
    b.append(label(470, py + ph + 90,
                   "Δv = F·h/m, so the height goes as h². Halve the",
                   cls="xs muted", anchor="start"))
    b.append(label(470, py + ph + 108,
                   "frame time and the character jumps a quarter as",
                   cls="xs muted", anchor="start"))
    b.append(label(470, py + ph + 126,
                   "high. An impulse has no h in it at all.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "Jump height at four frame rates, applied as an impulse and as a one-step force",
               "Four bars on the left sit within six percent of each other around the one metre "
               "target. Four bars on the right, on the same logarithmic axis, run from 3.85 metres "
               "at 30 Hz down to 0.17 metres at 144 Hz.", b)


# ===========================================================================
# Figure 6 — a very heavy body is not an immovable one
# ===========================================================================
def fig6():
    uid = "l82f6"
    W, H = 900, 330
    b = [cmarker(uid, "fall", RED)]

    b.append(label(40, 30, "“JUST MAKE THE FLOOR VERY HEAVY” — ONE SECOND LATER",
                   cls="sm t-hi", anchor="start"))

    lanes = [("a 10 g pebble", "0.01 kg, dynamic", F_PEBBLE_FALL, RED),
             ("a 1,000,000 kg floor", "1e6 kg, dynamic", F_HEAVY_FALL, RED),
             ("a fixed body", "inv_mass = 0", 0.0, GREEN)]

    top = 74
    lane_h = 66
    x0 = 220
    span = 420
    scale = span / 6.0          # 6 metres across the lane

    for i, (name, sub, drop, col) in enumerate(lanes):
        y = top + i * lane_h
        b.append(label(x0 - 18, y + 8, name, cls="xs", anchor="end"))
        b.append(label(x0 - 18, y + 24, sub, cls="xs mono muted", anchor="end"))
        b.append(rule(x0, y, x0, y + 34, cls="grid", width=1.0))
        b.append(f'<circle cx="{x0}" cy="{y + 8}" r="5" fill="{GREY}" '
                 f'fill-opacity="0.5"/>')
        end = x0 + abs(drop) * scale
        if drop != 0.0:
            b.append(carrow(x0 + 8, y + 8, end, y + 8, uid, "fall", col, width=2.2))
            b.append(label(end + 10, y + 12, f"fell {abs(drop):.3f} m", cls="xs mono",
                           anchor="start"))
        else:
            b.append(label(x0 + 14, y + 12, "fell 0.000 m — did not move, at all",
                           cls="xs mono t-ok", anchor="start"))

    b.append(rule(x0, top - 14, x0, top + 2 * lane_h + 40, cls="ink-soft", width=1.2))
    b.append(label(x0, top - 22, "start", cls="xs muted"))
    b.append(label(x0 + span / 2, top + 2 * lane_h + 58, "distance fallen in one second →",
                   cls="xs muted"))

    b.append(label(40, 296,
                   "The two arrows are the same length to the last decimal place, because "
                   "gravity does not ask what anything weighs.",
                   cls="xs muted", anchor="start"))
    b.append(label(40, 314,
                   "A hundred million times the mass buys exactly nothing. "
                   "inv_mass = 0 is not a large number; it is a different kind of answer.",
                   cls="xs", anchor="start"))

    return svg(uid, W, H,
               "A pebble, a million-kilogram body and a fixed body after one second of gravity",
               "The first two arrows are identical in length at 4.987 metres; the third body has "
               "not moved.", b)


# ===========================================================================
# Figure 7 — the square root under every miniature
# ===========================================================================
def fig7():
    uid = "l82f7"
    W, H = 900, 400
    b = []

    b.append(label(40, 30, "WHERE “ONE UNIT IS ONE METRE” STOPS BEING PEDANTRY",
                   cls="sm t-hi", anchor="start"))

    # The law, as a curve: t/t1 against s, with sqrt(s) drawn through the points.
    pw, ph = 330, 230
    px, py = 60, 74
    S_MAX = 4.5
    R_MAX = 2.2

    def sx(s):
        return px + (s / S_MAX) * pw

    def sy(r):
        return py + ph - (r / R_MAX) * ph

    b.append(frame(px, py, pw, ph))
    for r in (0.5, 1.0, 1.5, 2.0):
        b.append(rule(px, sy(r), px + pw, sy(r), cls="grid", width=0.8))
        b.append(label(px - 8, sy(r) + 4, f"{r:g}", cls="xs mono muted", anchor="end"))
    for s in (1, 2, 3, 4):
        b.append(rule(sx(s), py, sx(s), py + ph, cls="grid", width=0.8))
        b.append(label(sx(s), py + ph + 16, str(s), cls="xs mono muted"))
    b.append(label(px + pw / 2, py + ph + 36,
                   "s — how many metres one unit is DRAWN as", cls="xs muted"))
    b.append(label(px - 46, py + ph / 2, "×time", cls="xs muted"))

    # STARTED WHERE IT FITS, not at s = 0. 1/√s diverges, and a shape may leave
    # the viewBox where a label may not — but this one left it upwards, straight
    # through the figure's own title. The first s at which the curve is inside
    # the panel is 1/R_MAX², computed rather than guessed.
    s_min = 1.0 / (R_MAX * R_MAX)
    curve = []
    for i in range(0, 181):
        s = s_min + (S_MAX - s_min) * i / 180.0
        curve.append((sx(s), sy(1.0 / math.sqrt(s))))
    b.append(poly(curve, GREY, width=1.6, dash="4 3", close=False))

    for s, ratio in zip(G_SCALES, G_RATIOS):
        b.append(f'<circle cx="{sx(s):.1f}" cy="{sy(ratio):.1f}" r="4.5" '
                 f'fill="{GREEN}"/>')
    b.append(rule(px, sy(1.0), px + pw, sy(1.0), cls="ink-soft", width=1.1,
                  dash="2 4"))

    b += legend(px, py + ph + 62,
                [(GREY, "1/√s, the law"), (GREEN, "measured, from the table")], dy=16)

    # The right column: the derivation and the film number.
    tx = 470
    b.append(label(tx, 92, "WHY IT IS A SQUARE ROOT", cls="xs t-hi", anchor="start"))
    b.append(label(tx, 114, "d = ½ g t²   ⟹   t = √(2d/g)", cls="xs mono",
                   anchor="start"))
    b.append(label(tx, 134, "Multiply every LENGTH by s and leave g alone,",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, 152, "and every DURATION is multiplied by √s.",
                   cls="xs muted", anchor="start"))

    b.append(label(tx, 186, "WHAT THAT SOUNDS LIKE IN A GAME", cls="xs t-hi",
                   anchor="start"))
    b.append(label(tx, 208, "world modelled TOO BIG   →  floaty, moon-like",
                   cls="xs", anchor="start"))
    b.append(label(tx, 226, "world modelled TOO SMALL →  snappy, toy-like",
                   cls="xs", anchor="start"))
    b.append(label(tx, 244, "The symptom names the bug, if you know the law.",
                   cls="xs muted", anchor="start"))

    b.append(label(tx, 278, "AND WHAT FILM CREWS DO ABOUT IT", cls="xs t-hi",
                   anchor="start"))
    b.append(label(tx, 300, f"A 1/8 scale model runs √8 = {G_OVERCRANK:.4f}× too fast,",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, 318, f"so the camera is overcranked to "
                   f"{24 * G_OVERCRANK:.1f} fps and", cls="xs muted", anchor="start"))
    b.append(label(tx, 336, "played back at 24. Same equation, other trade.",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, 364, "Rescaling g by 1/s fixes the fall exactly — and",
                   cls="xs", anchor="start"))
    b.append(label(tx, 382, "carries no absolute tolerance with it (§8.4).",
                   cls="xs t-bad", anchor="start"))

    return svg(uid, W, H,
               "The one-over-root-s law that relates a world's length scale to its clock",
               "A dashed curve of one over the square root of s passes exactly through five "
               "measured points, crossing one when s is one. Beside it, the derivation from "
               "d equals half g t squared and the film-industry overcranking factor.", b)


# ===========================================================================
# Figure 8 — four frames, four different downs
# ===========================================================================
def fig8():
    uid = "l82f8"
    W, H = 900, 400
    # ONE MARKER PER COLOUR, and the first draft had four arrows in four colours
    # sharing two grey arrowheads — which reads as a different kind of arrow
    # rather than as the same arrow in a different colour.
    cols = [GREY, RED, AMBER, PURPLE]
    b = [cmarker(uid, f"c{i}", c) for i, c in enumerate(cols)]
    b.append(cmarker(uid, "ref", GREY))

    b.append(label(40, 30, "THE SAME “DOWN”, CARRIED OUT THROUGH FOUR PARENTS",
                   cls="sm t-hi", anchor="start"))

    cell_w = 205
    cy = 180
    scale = 5.2       # px per m/s²
    names = ["rotate 45°", "uniform scale 2", "scale (2,1,1)", "scale (2,1,1) after 45°"]

    for i, ((nm, vec, gain, tilt, sq, uni, inert), col) in enumerate(
            zip(H_FRAMES, cols)):
        del nm, uni
        x0 = 50 + i * cell_w
        cx = x0 + 88
        b.append(frame(x0, 70, cell_w - 22, 220))
        b.append(label(x0 + (cell_w - 22) / 2, 62, names[i], cls="xs mono t-hi"))

        # the reference: straight down, always the same length
        # OFFSET BY SIX PIXELS, because in column 3 the two arrows are
        # identical and the reference would be perfectly hidden under the
        # answer — which is exactly the column whose point is that they agree.
        b.append(carrow(cx - 6, cy - 46, cx - 6, cy - 46 + G * scale, uid, "ref",
                        GREY, width=1.3, dash="3 3"))
        # where it actually points
        ex = cx + vec[0] * scale
        ey = (cy - 46) - vec[1] * scale
        b.append(carrow(cx, cy - 46, ex, ey, uid, f"c{i}", col, width=2.4))

        b.append(label(x0 + (cell_w - 22) / 2, 252,
                       f"gain {gain:.4f}", cls="xs mono"))
        b.append(label(x0 + (cell_w - 22) / 2, 268,
                       f"tilt {tilt:.3f}°", cls="xs mono"))
        b.append(label(x0 + (cell_w - 22) / 2, 284,
                       f"square {sq:.4f}", cls="xs mono"))

        verdict = "inertial" if inert else "NOT INERTIAL"
        b.append(label(x0 + (cell_w - 22) / 2, 306, verdict,
                       cls="xs t-ok" if inert else "xs t-bad"))

    b.append(label(50, 334,
                   "Column 3 is the trap: gravity points along y and the scale is on x, so gain "
                   "and tilt both report 1.0000 and 0.000° —", cls="xs muted",
                   anchor="start"))
    b.append(label(50, 352,
                   "and the frame is still broken. It is the `uniform` flag that catches it, "
                   "because the fault is in the frame and not in this vector.",
                   cls="xs", anchor="start"))
    b.append(label(50, 376,
                   f"Column 4 is the same frame with the body merely TURNED: the fall is now "
                   f"{H_FRAMES[3][2]:.4f} g, {H_FRAMES[3][3]:.3f}° off vertical, and 0.6000 "
                   "out of square is 53.13° of shear.",
                   cls="xs t-bad", anchor="start"))

    return svg(uid, W, H,
               "Gravity carried into world space through four different parent frames",
               "Four panels each show a dashed grey reference arrow pointing straight down and a "
               "solid arrow showing where the same local gravity actually points. The second is "
               "twice as long, the third identical, and the fourth is both longer and rotated by "
               "63 degrees.", b)


# ===========================================================================
# Figure 9 — the scaled parent, as a fall
# ===========================================================================
def fig9():
    uid = "l82f9"
    W, H = 900, 400
    b = [cmarker(uid, "loc", BLUE), cmarker(uid, "wld", RED)]

    b.append(label(40, 30, "ONE SECOND OF FALLING, IN TWO SPACES",
                   cls="sm t-hi", anchor="start"))

    # A single vertical scale in metres, with two markers on it.
    x_local = 250
    x_world = 560
    top = 76
    span = 170
    scale = span / 11.0

    for x, title, sub in ((x_local, "LOCAL, under a parent scaled by 2",
                           "what the integrator sees"),
                          (x_world, "WORLD, where the player is",
                           "what the frame produced")):
        b.append(label(x, top - 30, title, cls="xs t-hi"))
        b.append(label(x, top - 14, sub, cls="xs muted"))
        b.append(rule(x - 110, top, x + 110, top, cls="grid", width=1.0))
        b.append(f'<circle cx="{x}" cy="{top}" r="5" fill="{GREY}" '
                 f'fill-opacity="0.5"/>')

    for x, drop, col, unit, mk in ((x_local, H_LOCAL_FALL, BLUE, "units", "loc"),
                                   (x_world, H_WORLD_FALL, RED, "m", "wld")):
        end = top + abs(drop) * scale
        b.append(carrow(x, top + 8, x, end, uid, mk, col, width=2.4))
        b.append(rule(x - 110, end, x + 110, end, cls="grid", width=1.0, dash="3 3"))
        b.append(label(x + 18, end + 5, f"{drop:.6f} {unit}", cls="xs mono",
                       anchor="start"))

    b.append(label(x_local, top + span + 44, "the integrator ran once,",
                   cls="xs muted"))
    b.append(label(x_local, top + span + 62, "with g = 9.81", cls="xs mono muted"))
    b.append(label(x_world, top + span + 44, "ratio 2.0000 — the body fell",
                   cls="xs t-bad"))
    b.append(label(x_world, top + span + 62, "at 2 g and nothing reported it",
                   cls="xs t-bad"))

    b.append(label(40, 352,
                   f"And a SPINNING parent passes every test a matrix can answer: a body with no "
                   f"forces on it at all, which must travel in a straight line, ends up "
                   f"{H_SPIN_APART:.4f} m away after one second.",
                   cls="xs", anchor="start"))
    b.append(label(40, 372,
                   f"Straight line ({H_SPIN_TRUE[0]:.4f}, {H_SPIN_TRUE[1]:.4f}); "
                   f"integrated locally and mapped out, ({H_SPIN_GOT[0]:.4f}, "
                   f"{H_SPIN_GOT[1]:.4f}). A matrix has no time derivative.",
                   cls="xs mono muted", anchor="start"))

    return svg(uid, W, H,
               "A one-second fall measured in a scaled parent's space and in world space",
               "The left arrow drops 4.987 local units; the right arrow, the same fall carried "
               "out through a parent scaled by two, drops 9.973 metres — twice as far.", b)


# ===========================================================================
# Figure 10 — the demo, and the budget
# ===========================================================================
def render_panel(ppm, crop, x, y, px, cell=4, levels=3, peak=True):
    """A real render, downsampled and run-length encoded to rectangles."""
    _w, _h, data = read_ppm(os.path.join(OUT, ppm))
    sampler = peak_sample if peak else box_sample
    grid_w, grid_h, grid = sampler(data, _w, crop, cell)
    body = rle_rects(grid, grid_w, grid_h, x, y, px, BODIES_PALETTE, levels=levels,
                     bg_class="fill-shot")
    return (['<g shape-rendering="crispEdges">'] + body + ['</g>'],
            grid_w * px, grid_h * px)


def fig10():
    uid = "l82f10"
    W, H = 900, 840
    b = []

    # cell 3 at 1.5 units per cell, for 8.1's reason: a one-unit rect in a
    # 900-unit viewBox is 0.7 device pixels once the figure scales to a phone,
    # and single-cell runs along a thin dashed arc simply vanish.
    CROP = (0, 40, 960, 500)
    CELL, PX = 3, 1.5

    y = 56
    for tag, ppm, note in (("damping", "_b82_m1.ppm",
                            "three masses, one curve — the knob has no mass in it"),
                           ("drag force", "_b82_m2.ppm",
                            "three masses, three curves — the force is divided by it")):
        panel, PW, PH = render_panel(ppm, CROP, 40, y, PX, cell=CELL, levels=4,
                                     peak=True)
        b.append(f'<rect x="38" y="{y - 2:.0f}" width="{PW + 4:.0f}" '
                 f'height="{PH + 4:.0f}" class="fill-shot"/>')
        b += panel
        b.append(label(40, y - 10, f"demos/bodies, [M] = {tag}   —   {note}",
                       cls="xs muted", anchor="start"))
        y += PH + 40

    b.append(label(40, y + 4,
                   "Each body draws one dash in three. A single line cycling amber, green, blue "
                   "is three trajectories in exact agreement.",
                   cls="xs muted", anchor="start"))
    b.append(label(40, y + 22,
                   "Right panel, both times: four bodies dropped from the SAME WORLD POINT in "
                   "four different parent frames. Only green is falling.",
                   cls="xs muted", anchor="start"))

    rows = [(name, f"{med:.3f}", f"{mn:.3f}") for name, med, mn in I_ROWS]
    el, hgt = table(40, y + 48, ["§11  ns / body / step", "median", "min"],
                    rows, [270, 78, 78])
    b += el

    tx = 500
    ty = y + 48
    b.append(label(tx, ty + 12, "the divide for weight, three runs:", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, ty + 30,
                   "  ".join(f"{r:.3f}" for r in I_DIVIDE_RATIO) + "   → keep",
                   cls="xs mono t-ok", anchor="start"))
    b.append(label(tx, ty + 52, "the sqrt for max_speed, three runs:", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, ty + 70,
                   "  ".join(f"{r:.3f}" for r in I_SQRT_RATIO) + "   → a tie",
                   cls="xs mono", anchor="start"))
    b.append(label(tx, ty + 98, f"body_world::step      {I_STEP_PLAIN:.3f} ns",
                   cls="xs mono", anchor="start"))
    b.append(label(tx, ty + 116, f"…with damping 0.4     {I_STEP_DAMPED:.3f} ns",
                   cls="xs mono", anchor="start"))
    b.append(label(tx, ty + 134, f"…all bodies fixed     {I_STEP_FIXED:.3f} ns",
                   cls="xs mono", anchor="start"))
    b.append(label(tx, ty + 152, f"8.1's bare step       {I_81_STEP:.3f} ns",
                   cls="xs mono muted", anchor="start"))
    b.append(label(tx, ty + 178, "One expf costs more than the whole rest of",
                   cls="xs", anchor="start"))
    b.append(label(tx, ty + 196, "the update. A fixed body costs a quarter.",
                   cls="xs", anchor="start"))

    return svg(uid, W, H,
               "Two screenshots of the bodies demo, above the measured per-body cost of a step",
               "In the damping screenshot the three masses trace one dashed curve in three "
               "colours; in the drag screenshot they trace three separate curves, the lightest "
               "dropping almost straight down. A table gives the cost of each arrangement of the "
               "step in nanoseconds per body.", b)


# ===========================================================================
def main():
    figs = [fig1, fig2, fig3, fig4, fig5, fig6, fig7, fig8, fig9, fig10]
    for i, fn in enumerate(figs, start=1):
        path = os.path.join(OUT, f"l82_fig{i}.svg")
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}  ({os.path.getsize(path):,} bytes)")


if __name__ == "__main__":
    main()
