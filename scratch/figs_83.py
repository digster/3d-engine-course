#!/usr/bin/env python3
"""scratch/figs_83.py — Lesson 8.3's diagrams.

Same rules as 5.1-8.2's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - the figure palette must contain the DEMO's own colours (rle_rects SNAPS)

Every number below is transcribed from scratch/verify_83.log. Nothing here is
estimated, and the section of the harness each block came from is named above it.

*** THIS FILE IS WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. *** 8.1's
figs file ended up mixing the two because successive patch scripts wrote both,
and a str.replace written against one form fails SILENTLY against the other.
One form, and every patch script against this file asserts its replacement count.

ONE COLOUR RULE FOR THIS LESSON, and it is the opposite of 8.2's. There the
colours named MASSES and reusing red/green/blue would have said something false;
here almost everything coloured is a DIRECTION — a body axis, a spin axis, a
component of omega in body coordinates — so conventions.html §10's x/y/z =
red/green/blue applies and is used throughout. AMBER is reserved for angular
momentum, which is the one vector in these figures that does not move, and
PURPLE for the quantity a mode is failing to conserve.
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

# demos/spin/main.cpp's constants, transcribed. The body axes ARE the course's
# axis colours; amber is L; the greys are the wireframe and the panel chrome.
S_X = "#eb6060"
S_Y = "#78dca0"
S_Z = "#96a0ff"
S_L = "#ebc860"
S_WIRE = "#bec6d6"
S_WIRE_BACK = "#4a505e"
S_AXIS = "#545c6c"
S_GRID = "#262a34"
SPIN_PALETTE = [hexrgb(c) for c in (S_X, S_Y, S_Z, S_L, S_WIRE, S_WIRE_BACK,
                                    S_AXIS, S_GRID)]

H60 = 1.0 / 60.0


def render_panel(ppm, crop, x, y, px, cell=4, levels=3, peak=True):
    """A real render, downsampled and run-length encoded. See figs_76."""
    _w, _h, data = read_ppm(os.path.join(OUT, ppm))
    sampler = peak_sample if peak else box_sample
    grid_w, grid_h, grid = sampler(data, _w, crop, cell)
    body = rle_rects(grid, grid_w, grid_h, x, y, px, SPIN_PALETTE, levels=levels,
                     bg_class="fill-shot")
    return (['<g shape-rendering="crispEdges">'] + body + ['</g>'],
            grid_w * px, grid_h * px)


# ---- transcribed from verify_83.log ---------------------------------------

# A — torque
A_PUSH = (10.0, 0.0, 0.0)
A_AT = (0.0, 1.0, 0.0)
A_TORQUE = (0.0, 0.0, -10.0)
A_COUPLE = -10.0

# B — the tensor, and the cancellation
B_TRIPLE = 1.2075e-06
B_ASYM = 0.0
B_THIN = (1000.0, 0.001, 0.0)
B_DERIVED_IXX = 0.0
B_CONDITIONED_IXX = 1.000000e-06
B_EXACT_IXX = 1.0e-06
B_POINT = (12.0, 0.0, 12.0)          # 3 kg at (0, 2, 0)

# C — the closed forms against a brute-force sum
C_BOX = (4.333333, 3.333333, 1.666667)     # m = 4, half (0.5, 1.0, 1.5)
C_GRID = [(4, 64, 4.062500, 0.937500, 0.937500),
          (8, 512, 4.265625, 0.984375, 0.984375),
          (20, 8000, 4.322488, 0.997497, 0.997500),
          (50, 125000, 4.331634, 0.999608, 0.999600)]
C_CELLS = [(4, 4.333332, 3.3012e-07),
           (8, 4.333342, 1.8707e-06),
           (20, 4.333335, 3.3012e-07)]
C_SPHERE = 1.800000
C_SPHERE_GRID = [(20, 4224, 1.809875, 5.4861e-03),
                 (40, 33552, 1.801528, 8.4864e-04),
                 (80, 268096, 1.800423, 2.3491e-04)]
C_CAP_SPHERE = (0.76800001, 0.76800007, 5.9605e-08)
C_CAP_ROD = (1.00013328, 1.00000000, 1.3328e-04)

# D — the parallel-axis theorem
D_SHIFTED = (4.333333, 19.333334, 17.666666)
D_SAMPLED = (4.333337, 19.333712, 17.666729)
D_SAMPLE_DIFF = 3.7766e-04
D_ROUNDTRIP = 7.1526e-07
D_DUMBBELL_MASS = 4.5000
D_DUMBBELL = (9.631312, 0.256625, 9.631312)
D_DUMBBELL_RATIO = 37.5307
D_UNSHIFTED_IXX = 0.631312
D_UNSHIFTED_FACTOR = 15.3

# E — the basis change
E_BODY = (4.333333, 3.333333, 1.666667)
E_WORLD = [(2.563311, -1.013178, -0.513794),
           (-1.013178, 3.740575, -0.415708),
           (-0.513794, -0.415708, 3.029448)]
E_TRACE = 9.333334
E_DET = 24.074078
E_BACKWARDS_DIFF = 1.569413
E_INVERSE_DIFF = 5.9605e-08
E_SCALED_TRACE = 22.333334

# F — an orientation is not a vector
F_ORDER_ANGLE = 120.000
F_INFLATE = [(1.0, 0.0167, 1.000034690, 1.000034690),
             (10.0, 0.1667, 1.003466249, 1.003466249),
             (30.0, 0.5000, 1.030776381, 1.030776381)]
F_COMPOUND = 1.2307
F_ANGLE = [(0.0167, 0.016667, 0.016666, -2.313e-05, -2.315e-05),
           (0.1667, 0.166667, 0.166282, -2.305e-03, -2.315e-03),
           (0.5000, 0.500000, 0.489957, -2.009e-02, -2.083e-02)]
F_LONG = [("linearised", 0.9999999, 79.2459),
          ("exponential", 1.0000000, 0.0019)]
F_WRONG_SIDE = 57.612

# G — L is conserved, omega is not
G_MOMENTS = (1.083333, 0.833333, 0.416667)
G_OMEGA0 = (0.6, 4.0, 0.3)
G_L2 = 11.549237
G_2T = 13.760834
G_PREDICTED = (4.0524, 4.4880)
G_MEASURED = (4.0527, 4.4924)
G_SWING = 10.85
G_L_DRIFT = 8.0497e-04
G_E_DRIFT = 1.9395e-03
G_OFF_OMEGA = 4.055860
G_MODES = [(30, 3.2967e+11, 4.6676e-01, 3.8586e-04),
           (60, 1.6233e+00, 3.4814e-01, 8.0497e-04),
           (240, 1.8069e-01, 1.2738e-01, 3.0143e-03),
           (960, 3.9355e-02, 3.0173e-02, 1.1838e-02)]

# H — the tennis racket theorem
H_STABILITY = [("x  largest", 0.0015, "stable"),
               ("y  middle", 10.9863, "UNSTABLE"),
               ("z  smallest", 0.0017, "stable")]
H_GROWTH = 4.8038
H_OSC_X = 6.9282
H_OSC_Z = 5.5470
H_FIT = [(120, 4.9058, 1.02122),
         (480, 4.8310, 1.00564),
         (1920, 4.8128, 1.00186),
         (7680, 4.8084, 1.00096)]
H_FLIPS = [2.606, 5.271, 7.877, 10.414]
H_INTERVAL = 2.6651
H_OFF = 1.0000e-04

# I — the budget
I_SIZEOF = 172
I_LIN, I_EXP = 19.450, 22.736
I_OFF, I_IMPLICIT, I_EXPLICIT_C = 19.053, 33.793, 33.061
I_MOMENTUM = 54.331
I_HOIST = (14.577, 11.139, 0.764)
I_SANDWICH = (8.545, 0.621, 0.073)
I_BEFORE = 33.793      # the step before §12's two changes
I_AFTER = 18.778       # ...and after


# ===========================================================================
# Figure 1 — torque is r x F  (§3)
# ===========================================================================
def fig1():
    uid = "l83f1"
    W, H = 900, 400
    b = [cmarker(uid, "red", RED), cmarker(uid, "blue", BLUE),
         cmarker(uid, "green", GREEN), cmarker(uid, "amber", AMBER)]

    # Three panels: the push, the push through the centre, the couple.
    pw, ph = 250, 250
    gap = 34
    titles = ["a push at the top", "the same force, aimed\nat the centre", "a couple"]

    for panel in range(3):
        px = 40 + panel * (pw + gap)
        py = 66
        b.append(frame(px, py, pw, ph))
        for i, line in enumerate(titles[panel].split("\n")):
            b.append(label(px, py - 26 + i * 15, line, cls="xs muted", anchor="start"))

        cx = px + pw / 2
        cy = py + ph / 2

        # The box: 1 m wide, 2 m tall, drawn at 46 px per metre.
        s = 46.0
        b.append(hollow(cx - 0.5 * s, cy - 1.0 * s, 1.0 * s, 2.0 * s, GREY, width=1.3))
        b.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="3.4" fill="{GREY}"/>')
        if panel == 0:
            # ONCE, and to the LEFT of the box rather than across it. The first
            # draft labelled all three panels starting at cx + 8, which runs
            # straight through the outline — check-page.js's onShape test.
            b.append(label(cx - 30, cy + 4, "centre of mass", cls="xs muted",
                           anchor="end"))

        if panel == 0:
            # r from the centre to the application point, then F.
            ay = cy - 1.0 * s
            b.append(carrow(cx, cy, cx, ay, uid, "amber", S_L, width=2.0))
            b.append(label(cx - 8, cy - 0.5 * s, "r", cls="xs mono", anchor="end"))
            b.append(carrow(cx, ay, cx + 78, ay, uid, "red", RED, width=2.4))
            b.append(label(cx + 40, ay - 10, "F = 10 N", cls="xs mono"))
            # the resulting rotation, as an arc
            b.append(f'<path d="M {cx - 40:.1f},{cy + 52:.1f} '
                     f'A 40 40 0 0 1 {cx + 40:.1f},{cy + 52:.1f}" fill="none" '
                     f'stroke="{BLUE}" stroke-width="2.0" '
                     f'marker-end="url(#e-blue-{uid})"/>')
            b.append(label(cx, cy + 86, "τ = r × F = (0, 0, −10)", cls="xs mono"))
            b.append(label(cx, cy + 102, "N·m, about −z", cls="xs muted"))
        elif panel == 1:
            corner = (cx + 0.5 * s, cy - 1.0 * s)
            b.append(carrow(cx, cy, corner[0], corner[1], uid, "amber", S_L, width=2.0))
            # Ends HALFWAY to the centre, not past it: an arrow that overshoots
            # the point it is aimed at reads as a force through the far side.
            b.append(carrow(corner[0], corner[1],
                            corner[0] + 0.55 * (cx - corner[0]),
                            corner[1] + 0.55 * (cy - corner[1]),
                            uid, "red", RED, width=2.4))
            b.append(label(cx + 62, cy - 0.66 * s, "F ∥ r", cls="xs mono", anchor="start"))
            b.append(label(cx, cy + 86, "τ = 0.0000e+00 N·m", cls="xs mono"))
            b.append(label(cx, cy + 102, "exactly, not nearly", cls="xs t-hi"))
        else:
            top = cy - 1.0 * s
            bot = cy + 1.0 * s
            b.append(carrow(cx, top, cx + 62, top, uid, "red", RED, width=2.4))
            b.append(carrow(cx, bot, cx - 62, bot, uid, "red", RED, width=2.4))
            b.append(label(cx + 34, top - 10, "+5 N", cls="xs mono"))
            b.append(label(cx - 34, bot + 18, "−5 N", cls="xs mono"))
            b.append(label(cx, cy + 86, "net force 0, net τ = −10", cls="xs mono"))
            b.append(label(cx, cy + 102, "spin without push", cls="xs muted"))

    b.append(label(40, 356, "The lever arm is measured from the CENTRE OF MASS, and that "
                            "choice is what decouples the two halves of a step: about any",
                   cls="xs muted", anchor="start"))
    b.append(label(40, 372, "other point, a force through the centre of mass would produce "
                            "a torque, and pushing a crate squarely in the middle would "
                            "set it spinning.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "Three ways to push a box: off-centre, through the centre, and as a couple",
               "The first panel shows a force applied at the top of a box with the lever "
               "arm drawn from the centre of mass, producing a torque of ten newton-metres "
               "about minus z. The second shows the same force aimed at the centre, where "
               "the lever arm and the force are parallel and the torque is exactly zero. "
               "The third shows two equal and opposite forces offset from each other: no "
               "net force at all, and a torque of minus ten.", b)


# ===========================================================================
# Figure 2 — what |r|² 1 − r ⊗ r does  (§4)
# ===========================================================================
def fig2():
    uid = "l83f2"
    W, H = 900, 384
    b = [cmarker(uid, "red", RED), cmarker(uid, "blue", BLUE),
         cmarker(uid, "green", GREEN), cmarker(uid, "amber", AMBER)]

    # Left: a point mass and two axes through it.
    px, py, pw, ph = 40, 62, 340, 250
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "a 3 kg point mass at (0, 2, 0)",
                   cls="xs muted", anchor="start"))

    cx = px + pw / 2
    cy = py + ph * 0.72
    s = 62.0
    mx, my = cx, cy - 2.0 * s * 0.5

    b.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="3" fill="{GREY}"/>')
    b.append(label(cx - 6, cy + 16, "origin", cls="xs muted", anchor="end"))
    b.append(carrow(cx, cy, mx, my, uid, "amber", S_L, width=2.0))
    b.append(label(mx + 8, (cy + my) / 2, "r", cls="xs mono", anchor="start"))
    b.append(f'<circle cx="{mx:.1f}" cy="{my:.1f}" r="7" fill="{GREY}"/>')
    b.append(label(mx + 14, my - 4, "m = 3 kg", cls="xs mono", anchor="start"))

    # the y axis (through it) and the x axis (across it)
    b.append(cline(mx, my - 44, mx, my + 130, GREEN, width=2.0, dash="5 4"))
    b.append(label(mx - 8, my - 50, "y — through the mass", cls="xs", anchor="middle"))
    b.append(cline(mx - 96, my, mx + 96, my, RED, width=2.0, dash="5 4"))
    b.append(label(px + 12, my - 10, "x — across it", cls="xs", anchor="start"))

    b += legend(px, py + ph + 26,
                [(GREEN, f"spun about y:  I = {B_POINT[1]:.4f}  — no resistance at all"),
                 (RED, f"spun about x:  I = {B_POINT[0]:.4f}  = m r²")])

    # Right: the algebra, as two blocks.
    tx = 430
    b.append(label(tx, py - 16, "…and that is exactly what the expression says",
                   cls="xs muted", anchor="start"))

    terms = [("|r|² · 1", "resist EVERY axis by m r²"),
             ("− r ⊗ r", "…then take it all back along r"),
             ("= the tensor", "m r² across, exactly 0 along")]
    b.append(rule(tx, py + 6, tx + 430, py + 6, cls="grid"))
    for i, (term, what) in enumerate(terms):
        ty2 = py + 26 + i * 20
        b.append(label(tx, ty2, term, cls="xs mono", anchor="start"))
        b.append(label(tx + 120, ty2, what, cls="xs muted", anchor="start"))
    hgt = 26 + len(terms) * 20

    y = py + hgt + 30
    b.append(label(tx, y, "The outer product a ⊗ b is the OTHER thing you can do",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 16, "with a pair of vectors: the dot product gives a number,",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 32, "this gives a matrix. Read it as an action rather than",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 48, "a formula — (a ⊗ b)v measures how much of v lies along",
                   cls="xs mono", anchor="start"))
    b.append(label(tx, y + 64, "b, and hands back that much of a. Its rank is one, so it",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 80, "flattens all of space onto a line: never a transformation",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 96, "you would want, always a TERM in something else.",
                   cls="xs t-hi", anchor="start"))

    b.append(label(tx, y + 124, f"checked against r × (ω × r) over 20,000 random pairs:",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 140, f"worst relative difference {fmt_e(B_TRIPLE)}",
                   cls="xs mono", anchor="start"))

    return svg(uid, W, H,
               "A point mass and the two axes it does and does not resist",
               "A three-kilogram point mass sits two metres from the origin. An axis "
               "running through the mass meets no resistance at all — the moment is "
               "exactly zero — while an axis across it meets m r squared, twelve. The "
               "right-hand column reads the expression term by term: the identity term "
               "resists every axis, and the outer product takes all of it back along r.",
               b)


# ===========================================================================
# Figure 3 — the form that loses the answer  (§4)
# ===========================================================================
def fig3():
    uid = "l83f3"
    W, H = 900, 350
    b = [cmarker(uid, "red", RED), cmarker(uid, "amber", AMBER)]

    px, py, pw, ph = 40, 74, 400, 150
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 42, "a sample at r = (1000, 0.001, 0) — a plank, a rail, a sword",
                   cls="xs muted", anchor="start"))
    b.append(label(px, py - 24, "Ixx / m is y² + z² = 1e−06. Both expressions say so on paper.",
                   cls="xs muted", anchor="start"))

    # A number line showing the two magnitudes involved.
    bx = px + 26
    bw = pw - 52
    by = py + 52

    def lx(v):
        return logspan(v, 1e-8, 1e7, bx, bw)

    b.append(cline(bx, by, bx + bw, by, GREY, width=1.2))
    for dec in (-6, -3, 0, 3, 6):
        x = lx(10.0 ** dec)
        b.append(rule(x, by - 5, x, by + 5, cls="grid"))
        b.append(label(x, by + 20, f"1e{dec}", cls="xs muted"))

    b.append(carrow(lx(1e6), by - 12, lx(1e6), by - 2, uid, "red", RED, width=1.8))
    b.append(label(lx(1e6) - 8, by - 16, "|r|² and x² — both 1e6", cls="xs mono",
                   anchor="end"))
    b.append(carrow(lx(1e-6), by + 34, lx(1e-6), by + 42, uid, "amber", S_L, width=1.8))
    b.append(label(lx(1e-6), by + 56, "the answer — 1e−06", cls="xs mono"))

    b.append(label(px, py + ph + 26,
                   "A float has 24 bits of significand. At 1e6 the gap between "
                   "representable numbers is 0.0625, so",
                   cls="xs muted", anchor="start"))
    b.append(label(px, py + ph + 42,
                   "the 1e−06 is not merely rounded away — it was never in the sum.",
                   cls="xs t-hi", anchor="start"))

    tx = 486
    rows = [("|r|² · 1 − r ⊗ r", f"{B_DERIVED_IXX:.6e}", "100% wrong"),
            ("−[r]ₓ [r]ₓ", f"{B_CONDITIONED_IXX:.6e}", "exact"),
            ("the true value", f"{B_EXACT_IXX:.6e}", "")]
    el, hgt = table(tx, 74, ["Ixx / m, in float", "value", ""], rows, [170, 130, 88])
    b += el

    y = 74 + hgt + 26
    for i, line in enumerate([
            "The two are the same algebra. They are not the same",
            "arithmetic: the derived form builds a sum of three",
            "squares and then subtracts one of them straight off",
            "again, which is catastrophic cancellation in the",
            "textbook sense. The cross-product-matrix form never",
            "forms the sum at all — its diagonal is y² + z².",
            "",
            "The consequence is not academic. A zero principal",
            "moment is a SINGULAR tensor, which inverse_inertia",
            "refuses, which is a body that silently will not spin",
            "about its own length."]):
        cls = "xs t-hi" if i == 10 else "xs muted"
        b.append(label(tx, y + i * 16, line, cls=cls, anchor="start"))

    return svg(uid, W, H,
               "Why the textbook form of the inertia tensor fails on a long thin body",
               "A logarithmic number line shows the two quantities involved for a sample "
               "a thousand metres along x and a millimetre along y: the squared length "
               "and the squared x component are both one million, and the answer they are "
               "supposed to differ by is one millionth. In float the subtraction returns "
               "exactly zero, a hundred percent wrong, where the cross-product-matrix form "
               "returns the exact value.", b)


# ===========================================================================
# Figure 4 — the box as a grid of cells  (§5)
# ===========================================================================
def fig4():
    uid = "l83f4"
    W, H = 900, 420
    b = [cmarker(uid, "red", RED), cmarker(uid, "blue", BLUE)]

    # Left: a 4x4 grid over a box cross-section, with one cell's own inertia
    # called out.
    px, py, pw, ph = 40, 64, 300, 250
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "N = 4: sixty-four cells, and what each one is missing",
                   cls="xs muted", anchor="start"))

    bx = px + 52
    by = py + 34
    bw = 196
    bh = 182
    b.append(hollow(bx, by, bw, bh, GREY, width=1.3))
    for i in range(1, 4):
        b.append(rule(bx + i * bw / 4, by, bx + i * bw / 4, by + bh, cls="grid"))
        b.append(rule(bx, by + i * bh / 4, bx + bw, by + i * bh / 4, cls="grid"))

    for i in range(4):
        for j in range(4):
            cxx = bx + (i + 0.5) * bw / 4
            cyy = by + (j + 0.5) * bh / 4
            b.append(f'<circle cx="{cxx:.1f}" cy="{cyy:.1f}" r="2" fill="{GREY}"/>')

    # highlight one cell
    hx = bx + 2 * bw / 4
    hy = by + 1 * bh / 4
    b.append(hollow(hx, hy, bw / 4, bh / 4, RED, width=1.8))
    # The callout goes OUTSIDE the plot — figs_82's standing rule, and the
    # first draft put it inside, where it ran straight through the panel's
    # right-hand edge.
    b.append(carrow(hx + bw / 8 + 34, py + ph + 4, hx + bw / 8 + 4, hy + bh / 8 + 6,
                    uid, "red", RED, width=1.4))
    for i, line in enumerate([
            "A point at the cell's centre carries the cell's mass",
            "and NOT its own spread. Summed over all N³ cells,",
            "that deficit is exactly I / N²."]):
        cls = "xs t-hi" if i == 2 else "xs muted"
        b.append(label(px, py + ph + 24 + i * 16, line, cls=cls, anchor="start"))

    # Right: the two tables.
    tx = 470
    rows = [(str(n), f"{val:.6f}", f"{ratio:.6f}", f"{pred:.6f}")
            for n, _pts, val, ratio, pred in C_GRID]
    el, hgt = table(tx, 64, ["N", "Ixx, points", "ratio", "1 − 1/N²"],
                    rows, [60, 118, 108, 108])
    b += el

    y = 64 + hgt + 34
    rows2 = [(str(n), f"{val:.6f}", fmt_e(err)) for n, val, err in C_CELLS]
    el2, hgt2 = table(tx, y, ["N", "Ixx, cells", "relative error"],
                      rows2, [60, 158, 176], title="…and with each cell's own tensor added back")
    b += el2

    y2 = y + hgt2 + 26
    b.append(label(tx, y2, f"The closed form is {C_BOX[0]:.6f}. The second table is",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y2 + 16, "exact at N = 4, which is 64 samples — and it can only be",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y2 + 32, "exact if the parallel-axis theorem is exactly true.",
                   cls="xs t-hi", anchor="start"))

    return svg(uid, W, H,
               "A box filled with a grid of cells, and the deficit that exposes the theorem",
               "A four-by-four grid is drawn over a box with a sample point at each cell "
               "centre, and one cell is highlighted. Summing point masses gives an answer "
               "that is low by exactly one over N squared, because each cell's own spread "
               "is missing; the first table shows the measured ratio landing on that "
               "prediction to six decimals at four different resolutions. Adding each "
               "cell's own tensor back makes the sum exact even at the coarsest grid.", b)


# ===========================================================================
# Figure 5 — the dumbbell, shifted and not  (§5)
# ===========================================================================
def fig5():
    uid = "l83f5"
    W, H = 900, 400
    b = [cmarker(uid, "red", RED), cmarker(uid, "blue", BLUE),
         cmarker(uid, "green", GREEN)]

    px, py, pw, ph = 40, 72, 330, 250
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "two 2 kg balls on a 0.5 kg bar", cls="xs muted",
                   anchor="start"))

    cx = px + pw * 0.38
    cy = py + ph / 2
    s = 58.0

    b.append(f'<rect x="{cx - 5:.1f}" y="{cy - 1.5 * s:.1f}" width="10" '
             f'height="{3.0 * s:.1f}" fill="none" stroke="{GREY}" stroke-width="1.3"/>')
    for sign in (-1, 1):
        b.append(f'<circle cx="{cx:.1f}" cy="{cy + sign * 1.5 * s:.1f}" r="15" '
                 f'fill="none" stroke="{GREY}" stroke-width="1.5"/>')
        b.append(carrow(cx, cy, cx, cy + sign * 1.5 * s, uid, "blue", BLUE, width=1.6))
    b.append(label(cx + 22, cy - 0.75 * s, "d = 1.5 m", cls="xs mono", anchor="start"))
    b.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="3.4" fill="{GREY}"/>')

    b.append(cline(cx - 92, cy, cx + 92, cy, RED, width=2.0, dash="5 4"))
    b.append(label(px + 10, cy - 10, "x — hard", cls="xs", anchor="start"))
    b.append(cline(cx, cy - 1.9 * s, cx, cy + 1.9 * s, GREEN, width=2.0, dash="5 4"))
    b.append(label(cx + 8, py + 18, "y — easy", cls="xs", anchor="start"))

    b += legend(px, py + ph + 26,
                [(RED, f"Ixx {D_DUMBBELL[0]:.6f}"),
                 (GREEN, f"Iyy {D_DUMBBELL[1]:.6f}   — a ratio of {D_DUMBBELL_RATIO:.1f}")])

    tx = 430
    rows = [("shifted, correct", f"{D_DUMBBELL[0]:.6f}", "✓"),
            ("added without shifting", f"{D_UNSHIFTED_IXX:.6f}",
             (f"{D_UNSHIFTED_FACTOR:.1f}× small", "xs t-hi"))]
    el, hgt = table(tx, 72, ["Ixx of the assembly", "value", ""], rows, [200, 128, 132])
    b += el

    y = 72 + hgt + 30
    for i, line in enumerate([
            "The wrong one is the tensor of a body whose parts are",
            "all piled on top of each other at the balance point.",
            "It is always too small, it never reports an error, and",
            "it passes every test inspect_inertia can run:",
            "",
            "    symmetric ✓   positive ✓   triangle ✓   usable ✓",
            "",
            "A validity check tells you a tensor COULD exist. It",
            "cannot tell you it is the tensor of the thing you",
            "meant. That distinction is worth holding on to — the",
            "same one returns in 8.7's contact generation."]):
        cls = "xs mono" if i == 5 else ("xs t-hi" if i == 7 else "xs muted")
        b.append(label(tx, y + i * 16, line, cls=cls, anchor="start"))

    return svg(uid, W, H,
               "A dumbbell's inertia tensor, assembled correctly and incorrectly",
               "Two balls on a bar, with the parallel-axis offsets drawn from the centre. "
               "Spun across the bar the dumbbell is thirty-seven times harder to turn than "
               "along it. Adding the parts' tensors without shifting them to the common "
               "centre gives an answer fifteen times too small, and that wrong answer "
               "passes every validity test the engine has.", b)


# ===========================================================================
# Figure 6 — R I Rᵀ, and the plausible wrong one  (§6)
# ===========================================================================
def fig6():
    uid = "l83f6"
    W, H = 900, 452
    b = [cmarker(uid, "red", RED), cmarker(uid, "blue", BLUE),
         cmarker(uid, "green", GREEN), cmarker(uid, "amber", AMBER)]

    # The sandwich as a three-step pipeline.
    px, py = 40, 82
    bw, bh = 150, 56
    gap = 44
    steps = [("ω, world axes", None),
             ("Rᵀ", "carry it INTO body axes"),
             ("I", "the tensor acts, where\nit is constant"),
             ("R", "carry the result BACK"),
             ("L, world axes", None)]

    b.append(label(px, py - 26, "A tensor eats a vector and produces one, so it has to be "
                                "converted on BOTH sides", cls="xs muted", anchor="start"))

    x = px
    for i, (name, note) in enumerate(steps):
        is_op = note is not None
        w = bw if not is_op else 84
        if is_op:
            b.append(hollow(x, py, w, bh, BLUE, width=1.5))
        else:
            b.append(box(x, py, w, bh, colour=GREY))
        b.append(label(x + w / 2, py + bh / 2 + 5, name, cls="sm mono"))
        if note:
            for j, line in enumerate(note.split("\n")):
                b.append(label(x + w / 2, py + bh + 18 + j * 14, line, cls="xs muted"))
        x += w
        if i < len(steps) - 1:
            b.append(arrow(x + 6, py + bh / 2, x + gap - 6, py + bh / 2, uid, kind="s"))
            x += gap

    b.append(label(px, py + bh + 66,
                   "Read right to left, which is the order the matrices apply in. That is "
                   "the whole of R I Rᵀ, and it is why a plain",
                   cls="xs muted", anchor="start"))
    b.append(label(px, py + bh + 82,
                   "vector transforms with R alone while a tensor needs the sandwich.",
                   cls="xs muted", anchor="start"))

    # The wrong one, and the tests that do not catch it.
    ty = 250
    rows = [("trace", f"{E_TRACE:.6f}", f"{E_TRACE:.6f}", "same"),
            ("determinant", f"{E_DET:.6f}", f"{E_DET:.6f}", "same"),
            ("symmetric", "yes", "yes", "same"),
            ("principal moments", "unchanged", "unchanged", "same"),
            ("the actual tensor", "correct",
             (f"{E_BACKWARDS_DIFF:.6f} out", "xs t-hi"), "DIFFERENT")]
    el, hgt = table(px, ty, ["what you might check", "R I Rᵀ", "Rᵀ I R", ""],
                    rows, [190, 150, 150, 120])
    b += el

    tx = 660
    for i, line in enumerate([
            "Rᵀ I R is not nonsense — it is",
            "the basis change in the other",
            "direction, so it is symmetric,",
            "has the same invariants, and",
            "describes a real body.",
            "",
            "Just not this one.",
            "",
            "And a test built on a 90° turn",
            "passes with the transpose in",
            "either place: on a diagonal",
            "tensor a right angle is its own",
            "inverse up to a sign."]):
        cls = "xs t-hi" if i in (6,) else "xs muted"
        b.append(label(tx, ty + 12 + i * 15, line, cls=cls, anchor="start"))

    return svg(uid, W, H,
               "The similarity transform read as a three-step pipeline, and the wrong one",
               "A left-to-right chain shows an angular velocity in world axes being carried "
               "into body axes by R transpose, acted on by the tensor where it is constant, "
               "and carried back out by R — which is the whole of R I R transpose. Below, a "
               "table shows that the reversed sandwich has the same trace, the same "
               "determinant, the same symmetry and the same principal moments, and differs "
               "from the correct answer by more than one and a half units.", b)


# ===========================================================================
# Figure 7 — an orientation is not a vector  (§7)
# ===========================================================================
def fig7():
    uid = "l83f7"
    W, H = 900, 430
    b = [cmarker(uid, "red", RED), cmarker(uid, "blue", BLUE),
         cmarker(uid, "green", GREEN)]

    # Left: the two orders.
    px, py, pw, ph = 40, 74, 300, 180
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "the same two 90° turns, in both orders",
                   cls="xs muted", anchor="start"))

    cx = px + pw / 2
    cy = py + ph / 2
    s = 52.0
    b.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="3" fill="{GREY}"/>')
    b.append(carrow(cx, cy, cx + s, cy, uid, "red", RED, width=2.2))
    b.append(label(cx + s + 10, cy + 4, "x then y", cls="xs mono", anchor="start"))
    b.append(carrow(cx, cy, cx, cy + s, uid, "green", GREEN, width=2.2))
    b.append(label(cx + 8, cy + s + 16, "y then x", cls="xs mono", anchor="start"))
    b.append(f'<path d="M {cx + 30:.1f},{cy:.1f} A 30 30 0 0 1 {cx:.1f},{cy + 30:.1f}" '
             f'fill="none" stroke="{GREY}" stroke-width="1.2" stroke-dasharray="4 3"/>')
    b.append(label(cx + 42, cy + 30, f"{F_ORDER_ANGLE:.0f}°", cls="xs mono", anchor="start"))

    b.append(label(px, py + ph + 26, "The two rotation VECTORS summed are identical either "
                                     "way — addition", cls="xs muted", anchor="start"))
    b.append(label(px, py + ph + 42, "commutes. So no vector can be the running total of a "
                                     "rotation, and", cls="xs muted", anchor="start"))
    b.append(label(px, py + ph + 58, "q += ω·h is not merely inaccurate. It is not a "
                                     "rotation.", cls="xs t-hi", anchor="start"))

    # Right: the two failure modes of the linearised update.
    tx = 400
    rows = [(f"{wh:.4f}", f"{meas:.9f}", f"{pred:.9f}")
            for _w, wh, meas, pred in F_INFLATE]
    el, hgt = table(tx, 74, ["ω·h", "|q| measured", "√(1 + (ωh/2)²)"],
                    rows, [90, 150, 160],
                    title="one step, BEFORE renormalising — the length inflates")
    b += el

    y = 74 + hgt + 30
    rows2 = [(f"{wh:.4f}", f"{ach:.6f}", fmt_e(rel), fmt_e(pred))
             for wh, _asked, ach, rel, pred in F_ANGLE]
    el2, hgt2 = table(tx, y, ["ω·h", "angle achieved", "rel. error", "−(ωh)²/12"],
                      rows2, [90, 130, 118, 118],
                      title="…and AFTER renormalising, the angle is still short")
    b += el2

    y2 = y + hgt2 + 30
    rows3 = [(name, f"{q:.7f}", f"{err:.4f}°") for name, q, err in F_LONG]
    el3, _h3 = table(tx, y2, ["3,600 steps at 10 rad/s", "|q|", "attitude error"],
                     rows3, [190, 130, 136])
    b += el3

    b.append(label(px, 366, f"Compounded without renormalising, 10 rad/s for one second "
                            f"gives |q| = {F_COMPOUND:.4f}.", cls="xs muted", anchor="start"))
    b.append(label(px, 382, f"And 79.2459° of attitude error after a minute is just the "
                            f"per-step 2.3e−03 × 600 radians", cls="xs muted", anchor="start"))
    b.append(label(px, 398, "of turning — the lag is predictable before it is measured.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "Why an orientation cannot be integrated by addition",
               "Two ninety-degree turns applied in both orders send the same probe vector "
               "to results a hundred and twenty degrees apart, while the sum of the two "
               "rotation vectors is identical either way. Three tables then measure the "
               "linearised quaternion update: its length inflates by exactly the predicted "
               "square root each step, the angle it actually turns through is short by the "
               "predicted omega-h squared over twelve even after renormalising, and after "
               "a minute at ten radians per second that lag is seventy-nine degrees.", b)


# ===========================================================================
# Figure 8 — L is fixed, ω wanders  (§8)
# ===========================================================================
def fig8():
    uid = "l83f8"
    W, H = 900, 400
    b = [cmarker(uid, "red", RED), cmarker(uid, "blue", BLUE),
         cmarker(uid, "green", GREEN), cmarker(uid, "amber", AMBER)]

    px, py, pw, ph = 40, 74, 340, 240
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "the two conserved quantities, and the curve where they meet",
                   cls="xs muted", anchor="start"))

    cx = px + pw / 2
    cy = py + ph / 2

    # A sphere and an ellipsoid, in outline, with the intersection sketched.
    b.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="86" fill="none" '
             f'stroke="{S_L}" stroke-width="2.0"/>')
    b.append(label(cx, py + 24, "|L|² = const — a sphere", cls="xs mono"))

    b.append(f'<ellipse cx="{cx:.1f}" cy="{cy:.1f}" rx="112" ry="62" fill="none" '
             f'stroke="{PURPLE}" stroke-width="2.0" stroke-dasharray="6 4"/>')
    b.append(label(cx, py + ph - 12, "2T = ΣLᵢ²/Iᵢ — an ellipsoid", cls="xs mono"))

    # the intersection curve, sketched as two arcs
    for sign in (-1, 1):
        pts = []
        for k in range(41):
            t = -1.0 + 2.0 * k / 40.0
            ang = t * 0.9
            pts.append((cx + sign * 62 * math.cos(ang * 0.9) + sign * 16,
                        cy + 78 * math.sin(ang)))
        b.append(poly(pts, GREEN, width=2.6, close=False))
    b.append(label(cx, cy + 4, "ω lives HERE", cls="xs t-hi"))

    tx = 430
    b.append(label(tx, py - 16, "…so the reachable range of |ω| is a DERIVATION, not a guess",
                   cls="xs muted", anchor="start"))

    rows = [("Lx = 0", "infeasible", ""),
            ("Ly = 0", f"{G_PREDICTED[1]:.4f}", "the maximum"),
            ("Lz = 0", f"{G_PREDICTED[0]:.4f}", "the minimum")]
    el, hgt = table(tx, py, ["where one component vanishes", "|ω|", ""],
                    rows, [200, 110, 130])
    b += el

    y = py + hgt + 28
    rows2 = [("predicted", f"{G_PREDICTED[0]:.4f} … {G_PREDICTED[1]:.4f}"),
             ("measured, 60 s at 60 Hz", f"{G_MEASURED[0]:.4f} … {G_MEASURED[1]:.4f}")]
    el2, hgt2 = table(tx, y, ["", "range of |ω|"], rows2, [200, 240])
    b += el2

    y2 = y + hgt2 + 26
    for i, line in enumerate([
            "Write uᵢ = Lᵢ² and BOTH conserved quantities are linear",
            "in u. So the reachable set is a segment, |ω|² = Σuᵢ/Iᵢ² is",
            "linear on it, and a linear function on a segment takes its",
            "extremes at the ENDS — where one uᵢ reaches zero.",
            "",
            f"A swing of {G_SWING:.2f}% with nothing acting on the body.",
            "L is the conserved quantity. ω is not."]):
        cls = "xs t-hi" if i >= 5 else "xs muted"
        b.append(label(tx, y2 + i * 16, line, cls=cls, anchor="start"))

    return svg(uid, W, H,
               "The momentum sphere, the energy ellipsoid, and the curve where omega lives",
               "A torque-free rigid body conserves two quantities, drawn here as a sphere "
               "and an ellipsoid; the angular velocity is confined to the curve where they "
               "intersect. Because both quantities are linear in the squared components, "
               "the extremes of the angular speed occur where one component vanishes, which "
               "makes the reachable range a derivation rather than an observation. The "
               "predicted range and the measured one agree to three decimals.", b)


# ===========================================================================
# Figure 9 — four answers, and the column that gets worse  (§9)
# ===========================================================================
def fig9():
    uid = "l83f9"
    W, H = 900, 430
    b = [cmarker(uid, "red", RED), cmarker(uid, "blue", BLUE),
         cmarker(uid, "green", GREEN), cmarker(uid, "amber", AMBER)]

    px, py, pw, ph = 40, 80, 420, 250
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 42, "worst drift in |L| over 60 torque-free seconds, by step rate",
                   cls="xs muted", anchor="start"))
    b.append(label(px, py - 24, "— a body with NOTHING acting on it must not change its "
                                "angular momentum at all", cls="xs muted", anchor="start"))

    def rx_(rate):
        return px + 34 + (math.log10(rate) - math.log10(30.0)) / (
            math.log10(960.0) - math.log10(30.0)) * (pw - 76)

    def dy_(v):
        return py + ph - 20 - logspan(v, 1e-4, 1e12, 0.0, ph - 44)

    for rate in (30, 60, 240, 960):
        b.append(rule(rx_(rate), py, rx_(rate), py + ph - 20, cls="grid"))
        b.append(label(rx_(rate), py + ph - 4, f"{rate}", cls="xs mono"))
    for dec in (-4, -2, 0, 2, 4, 6, 8, 10, 12):
        b.append(rule(px, dy_(10.0 ** dec), px + pw, dy_(10.0 ** dec), cls="grid"))
        b.append(label(px - 6, dy_(10.0 ** dec) + 4, f"1e{dec}", cls="xs muted",
                       anchor="end"))
    b.append(label(px + pw / 2, py + ph + 16, "simulation rate, Hz", cls="xs muted"))

    series = [(RED, 1, "explicit"), (BLUE, 2, "implicit"), (GREEN, 3, "momentum")]
    for colour, idx, _name in series:
        pts = [(rx_(row[0]), dy_(row[idx])) for row in G_MODES]
        b.append(poly(pts, colour, width=2.6, close=False))
        for x, y in pts:
            b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.4" fill="{colour}"/>')

    # the "no drift at all" line
    b.append(cline(px, dy_(1e-4), px + pw, dy_(1e-4), GREY, width=1.2, dash="4 3"))

    b += legend(px, py + ph + 40,
                [(RED, "explicit — 8.1's determinant, growing"),
                 (BLUE, "implicit — its exact reciprocal, damping"),
                 (GREEN, "momentum — and it goes the WRONG WAY")])

    tx = 520
    rows = [(f"{rate} Hz", fmt_e(ex), fmt_e(im), fmt_e(mo))
            for rate, ex, im, mo in G_MODES]
    el, hgt = table(tx, 80, ["rate", "explicit", "implicit", "momentum"],
                    rows, [72, 104, 104, 104])
    b += el

    y = 80 + hgt + 28
    for i, line in enumerate([
            "The first two columns improve as the step",
            "shrinks, because their error is truncation.",
            "",
            "The third gets WORSE, and it is the only",
            "table in this course that does. Its error is",
            "not truncation at all: nothing in the",
            "momentum formulation approximates L. The",
            "engine simply does not STORE it, and rebuilds",
            "it from ω and the orientation every step.",
            "",
            "That round trip loses a little each time, so",
            "the error grows with the NUMBER of steps",
            "rather than their size."]):
        cls = "xs t-hi" if i in (3, 4) else "xs muted"
        b.append(label(tx, y + i * 16, line, cls=cls, anchor="start"))

    return svg(uid, W, H,
               "Angular momentum drift for three formulations, against simulation rate",
               "On a logarithmic axis, the explicit treatment of the gyroscopic term drifts "
               "by three hundred billion at thirty hertz and still by four percent at nine "
               "hundred and sixty; the implicit one is far better behaved and damps rather "
               "than growing; and the momentum formulation is three orders of magnitude "
               "better than either — but its line slopes the other way, getting worse as "
               "the step shrinks, because its error is accumulated rounding rather than "
               "truncation.", b)


# ===========================================================================
# Figure 10 — the flip, and the budget  (§10, §12)
# ===========================================================================
def fig10():
    uid = "l83f10"
    b = [cmarker(uid, "red", RED), cmarker(uid, "blue", BLUE),
         cmarker(uid, "green", GREEN)]

    # The plot half of the demo's framebuffer, at a scale that keeps both
    # panels inside the left column. PX and CELL are chosen together: CELL
    # samples the source, PX sizes the output, and their product against the
    # crop width is what has to fit.
    CROP = (596, 64, 936, 500)
    CELL = 2
    PX = 1.15

    px = 40
    py = 92
    panels = []
    for tag, ppm, note in (("gyroscopic off", "_b83_m0.ppm",
                            "ω never changes"),
                           ("momentum", "_b83_m3.ppm",
                            "two flips in the same twelve seconds")):
        panel, PW, PH = render_panel(ppm, CROP, px, py, PX, cell=CELL, levels=4,
                                     peak=True)
        panels.append((tag, note, px, PW, PH, panel))
        px += PW + 26

    panel_h = max(p[4] for p in panels)
    W = 900
    H = int(py + panel_h + 250)

    for tag, note, x, pwid, phgt, panel in panels:
        b += panel
        b.append(hollow(x, py, pwid, phgt, GREY, width=1.1))
        b.append(label(x, py - 28, tag, cls="sm", anchor="start"))
        b.append(label(x, py - 12, note, cls="xs muted", anchor="start"))

    b += legend(40, py + panel_h + 26,
                [(S_X, "ωx, body axes"), (S_Y, "ωy — the spin axis"),
                 (S_Z, "ωz")])

    # ---- right column: the growth rate, and the flips --------------------
    tx = 500
    rows = [(f"{rate} Hz", f"{meas:.4f}", f"{H_GROWTH:.4f}", f"{ratio:.5f}")
            for rate, meas, ratio in H_FIT]
    el, hgt = table(tx, py - 16, ["step rate", "measured", "predicted", "ratio"],
                    rows, [104, 96, 96, 92],
                    title="σ = ω √((I₃−I₂)(I₂−I₁)/(I₁I₃)), fitted over the first decade")
    b += el

    y = py - 16 + hgt + 24
    for i, line in enumerate([
            "ωy reverses sign at t = " + ", ".join(f"{t:.3f}" for t in H_FLIPS) + " s",
            f"— an interval of {H_INTERVAL:.4f} s, repeating forever, with",
            "no torque of any kind acting on the body.",
            "",
            f"With the term off the perturbation stays at {H_OFF:.4e}",
            "for forty seconds and never moves. No flip, ever."]):
        cls = "xs mono" if i == 0 else ("xs t-hi" if i == 5 else "xs muted")
        b.append(label(tx, y + i * 16, line, cls=cls, anchor="start"))

    # ---- the budget, along the bottom ------------------------------------
    by = py + panel_h + 104
    rows2 = [("orientation: linearised → exponential",
              f"{I_LIN:.3f} → {I_EXP:.3f}", f"{I_EXP / I_LIN:.3f}×"),
             ("gyroscopic: off → implicit",
              f"{I_OFF:.3f} → {I_IMPLICIT:.3f}", f"{I_IMPLICIT / I_OFF:.3f}×"),
             ("…off → momentum",
              f"{I_OFF:.3f} → {I_MOMENTUM:.3f}", f"{I_MOMENTUM / I_OFF:.3f}×"),
             ("mat3_from_quat hoisted out of the step",
              f"{I_HOIST[0]:.3f} → {I_HOIST[1]:.3f}", f"{I_HOIST[2]:.3f}×"),
             ("the whole step, before and after §12",
              (f"{I_BEFORE:.3f} → {I_AFTER:.3f}", "xs mono t-hi"),
              (f"{I_AFTER / I_BEFORE:.3f}×", "xs mono t-hi"))]
    el2, _h2 = table(40, by, ["ns per body per step, 4,096 bodies", "", "ratio"],
                     rows2, [272, 146, 82])
    b += el2

    cx = 570
    for i, line in enumerate([
            "The accurate mode is also the expensive one,",
            "which is not what 8.2 found about gravity.",
            "What it buys is figure 9: three orders of",
            "magnitude, for 2.9× the angular half.",
            "",
            "And the last row is TWO changes rather than",
            "one — caching the forward tensor, and building",
            "the rotation matrix once instead of twice."]):
        cls = "xs t-hi" if i == 3 else "xs muted"
        b.append(label(cx, by + 14 + i * 16, line, cls=cls, anchor="start"))

    return svg(uid, W, H,
               "The tennis racket effect as the demo draws it, and the per-body budget",
               "Two captures of the spin demo's body-frame plot. With the gyroscopic term "
               "off, the spin axis traces a flat line and the two perturbation components "
               "never move. With the momentum formulation, the spin component drops through "
               "zero to its negative and back again twice in twelve seconds while the other "
               "two pulse at each crossing — the box is flipping end over end with nothing "
               "acting on it. Beside them, the measured growth rate converging on the rate "
               "Euler's equations predict, and below, the per-body cost of every choice in "
               "the lesson.", b)


def main():
    figs = [("l83_fig1.svg", fig1), ("l83_fig2.svg", fig2), ("l83_fig3.svg", fig3),
            ("l83_fig4.svg", fig4), ("l83_fig5.svg", fig5), ("l83_fig6.svg", fig6),
            ("l83_fig7.svg", fig7), ("l83_fig8.svg", fig8), ("l83_fig9.svg", fig9),
            ("l83_fig10.svg", fig10)]
    for name, fn in figs:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}  ({os.path.getsize(path):,} bytes)")


if __name__ == "__main__":
    main()
