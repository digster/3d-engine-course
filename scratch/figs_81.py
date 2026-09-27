#!/usr/bin/env python3
"""scratch/figs_81.py — Lesson 8.1's diagrams.

Same rules as 5.1-7.8's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - the figure palette must contain the DEMO's own colours (rle_rects SNAPS)

Every number below is transcribed from scratch/verify_81.log. Nothing here is
estimated, and the section of the harness each block came from is named above it.

ONE COLOUR RULE FOR THIS LESSON, held to in every figure: RED is explicit Euler,
GREEN is semi-implicit Euler, PURPLE is velocity Verlet, and GREY is the exact
answer. Those are demos/integrate/main.cpp's own constants, which matters for
figure 10 — `rle_rects` snaps a screenshot to a palette, so the palette has to
contain the colours the screenshot actually used.
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

OUT = "scratch"

# demos/integrate/main.cpp's constants, transcribed. See the module docstring.
I_EXPLICIT = "#eb6060"
I_SEMI = "#78dca0"
I_VERLET = "#aa96ff"
I_EXACT = "#969eaf"
I_AXIS = "#545c6c"
I_GRID = "#262a34"
INTEGRATE_PALETTE = [hexrgb(c) for c in (I_EXPLICIT, I_SEMI, I_VERLET,
                                         I_EXACT, I_AXIS, I_GRID)]

W_OMEGA = 2.0 * math.pi          # the 1 Hz spring used throughout
H60 = 1.0 / 60.0

# ---- transcribed from verify_81.log ---------------------------------------

# A.1 — error after one period, against h
ORDER = [
    (60,   3.880e-01, 2.883e-03, 2.871e-03),
    (120,  1.786e-01, 7.184e-04, 7.178e-04),
    (240,  8.571e-02, 1.796e-04, 1.795e-04),
    (480,  4.198e-02, 4.498e-05, 4.499e-05),
    (960,  2.077e-02, 1.176e-05, 1.168e-05),
    (1920, 1.033e-02, 3.015e-06, 1.061e-06),
]
SLOPES = (1.089, 2.002, 2.000)

# A.3 — error against t, at h = 1/60
DRIFT = [
    (1.0,  3.880e-01, 2.883e-03, 2.871e-03),
    (2.0,  9.261e-01, 5.766e-03, 5.741e-03),
    (5.0,  4.143e+00, 1.442e-02, 1.435e-02),
    (10.0, 2.539e+01, 2.884e-02, 2.870e-02),
    (20.0, 6.941e+02, 5.773e-02, 5.740e-02),
    (40.0, 4.830e+05, 1.156e-01, 1.148e-01),
]

# B.1 — the determinant, measured
DET = [("explicit Euler", 1.010966377, 1.010966182),
       ("semi-implicit", 1.000000096, 1.000000000),
       ("velocity Verlet", 1.000000035, 1.000000000)]
DET_H0 = 1.000000119          # B.4, the control
U60 = 0.010966229             # h^2 w^2 at h = 1/60

# C.1 — energy, as a multiple of the starting energy
ENERGY = [
    (1,  1.9240e+00, 9.9970e-01, 1.0000e+00),
    (5,  2.6363e+01, 9.9849e-01, 1.0000e+00),
    (10, 6.9501e+02, 9.9699e-01, 1.0000e+00),
    (30, 3.3571e+08, 9.9104e-01, 9.9998e-01),
    (60, 1.1270e+17, 9.8243e-01, 9.9992e-01),
]
AMP60 = (3.3571e+08, 9.9117e-01, 9.9996e-01)     # C.2, metres after a minute

# C.4 — what is conserved, and what only wobbles
TRUE_MIN, TRUE_MAX = 18.7570286, 20.8298473
SHAD_MIN, SHAD_MAX = 19.7391300, 19.7392216
TRUE_SPREAD, SHAD_SPREAD = 1.047e-01, 4.638e-06
HW60 = 0.104720                                   # h*w, the prediction

# D.1 — simulated seconds to double the energy
DOUBLE = [(30, 0.567, 0.538), (60, 1.067, 1.059), (120, 2.117, 2.110),
          (240, 4.217, 4.215), (1000, 17.559, 17.558), (4000, 70.230, 70.231)]
ONE_PERCENT_HZ = 2380529

# E.1/E.2 — constant acceleration
FALL = [("explicit Euler", 4.823249, -0.081751),
        ("semi-implicit", 4.986749, +0.081749),
        ("velocity Verlet", 4.904999, -0.000001)]
FALL_PRED = 0.081750
FALL_T = [(1, +0.081751, -0.081749, +0.000001),
          (2, +0.163509, -0.163491, +0.000007),
          (4, +0.327015, -0.326984, +0.000020),
          (8, +0.653276, -0.654738, -0.000807)]

# F.1/F.2 — the stability limit
LIMIT_MEASURED = (0.3182939, 0.3183099)          # semi-implicit, verlet
LIMIT_EXACT = 2.0 / W_OMEGA
EXCURSION = [(0.20, 1.111, 1.111), (0.50, 1.333, 1.333), (1.00, 2.000, 2.000),
             (1.50, 3.999, 4.000), (1.80, 9.995, 10.000), (1.95, 39.486, 40.000),
             (1.99, 199.273, 200.000)]

# G.1 — `v *= 0.99f`
DAMP = [(30, 0.739700, 0.547156), (60, 0.547157, 0.547157),
        (120, 0.299381, 0.547156), (144, 0.235217, 0.547157)]
DAMP_TARGET = 0.547157

# H.1 — drag
DRAG = [(6, 0.1000, +0.90000, 0.904837, "ok"),
        (30, 0.5000, +0.50000, 0.606531, "ok"),
        (60, 1.0000, 0.00000, 0.367879, "STOPS DEAD"),
        (90, 1.5000, -0.50000, 0.223130, "REVERSES"),
        (150, 2.5000, -1.50000, 0.082085, "DIVERGES")]
DRAG_STEPS = [(1, -0.500000, 0.223130), (2, +0.250000, 0.049787),
              (3, -0.125000, 0.011109), (4, +0.062500, 0.002479),
              (5, -0.031250, 0.000553)]

# I — the budget
BUDGET = [("explicit Euler", 0.994, 1), ("semi-implicit", 0.963, 1),
          ("velocity Verlet", 1.136, 2)]
BUDGET_FN = 1.591
BUDGET_CONST = 1.222
BUDGET_TOUCH = 0.608


# ===========================================================================
# helpers
# ===========================================================================
def logspan(v, lo, hi, x0, span):
    """Map v onto [x0, x0+span] with a log10 axis clamped to [lo, hi]."""
    t = (math.log10(max(v, lo)) - math.log10(lo)) / (math.log10(hi) - math.log10(lo))
    return x0 + min(max(t, 0.0), 1.0) * span


def legend(x, y, items, dy=17):
    """A colour key, OUTSIDE the plot. Returns a list of elements."""
    out = []
    for i, (col, text) in enumerate(items):
        if col is not None:
            out.append(cline(x, y + i * dy - 4, x + 20, y + i * dy - 4, col, width=2.4))
        out.append(label(x + 28, y + i * dy, text,
                         cls="xs" if col is not None else "xs muted", anchor="start"))
    return out


# ===========================================================================
# Figure 1 — one step, and what it misses
# ===========================================================================
def fig1():
    uid = "l81f1"
    W, H = 900, 380
    b = [cmarker(uid, "red", RED), cmarker(uid, "grey", GREY)]

    # ---- left: the tangent-line picture ----------------------------------
    px, py, pw, ph = 62, 52, 400, 250
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "a quantity and its slope, over one step",
                   cls="xs muted", anchor="start"))

    # A curve with visible curvature: x(t) = cos(t) on [0, 1.35], scaled.
    t0, t1 = 0.0, 1.35
    def gx(t):
        return px + (t - t0) / (t1 - t0) * pw
    def gy(v):
        return py + (1.10 - v) / 1.42 * ph

    curve = [(gx(t0 + (t1 - t0) * i / 120.0), gy(math.cos(t0 + (t1 - t0) * i / 120.0)))
             for i in range(121)]
    b.append(poly(curve, I_EXACT, width=2.2, close=False))

    # the tangent at t = 0.25, extended by a big step h = 0.85
    ta, hstep = 0.25, 0.85
    xa, va = ta, math.cos(ta)
    slope = -math.sin(ta)
    xb = ta + hstep
    b.append(cline(gx(xa), gy(va), gx(xb), gy(va + slope * hstep), RED, width=2.2))
    b.append(f'<circle cx="{gx(xa):.1f}" cy="{gy(va):.1f}" r="4" fill="{I_EXACT}"/>')
    b.append(f'<circle cx="{gx(xb):.1f}" cy="{gy(va + slope * hstep):.1f}" r="4" '
             f'fill="{RED}"/>')
    b.append(f'<circle cx="{gx(xb):.1f}" cy="{gy(math.cos(xb)):.1f}" r="4" '
             f'fill="{I_EXACT}"/>')

    # the gap
    b.append(cline(gx(xb), gy(va + slope * hstep), gx(xb), gy(math.cos(xb)),
                   AMBER, width=2.0, dash="4 3"))
    b.append(label(gx(xb) + 10, (gy(va + slope * hstep) + gy(math.cos(xb))) / 2 + 4,
                   "the error", cls="xs mono", anchor="start"))

    # the step bracket
    yb = py + ph + 14
    b.append(cline(gx(xa), yb, gx(xb), yb, GREY, width=1.2))
    b.append(cline(gx(xa), yb - 4, gx(xa), yb + 4, GREY, width=1.2))
    b.append(cline(gx(xb), yb - 4, gx(xb), yb + 4, GREY, width=1.2))
    b.append(label((gx(xa) + gx(xb)) / 2, yb + 16, "h", cls="sm mono"))
    b.append(label(gx(xa) - 6, gy(va) + 18, "where you are", cls="xs muted", anchor="middle"))
    b.append(label(gx(xb) + 10, gy(math.cos(xb)) + 4, "where it went",
                   cls="xs muted", anchor="start"))
    # ABOVE the red line's midpoint, not beside its endpoint. check-page.js
    # walks a path's actual stroke, so a label alongside a line it names is a
    # real collision and not a bounding-box false positive.
    b.append(label(gx(xa + 0.55 * hstep), gy(va + slope * 0.55 * hstep) - 17,
                   "where Euler put it", cls="xs mono t-bad", anchor="middle"))

    # ---- right: Taylor, term by term -------------------------------------
    tx = 540
    b.append(label(tx, 44, "The exact answer, written out:", cls="sm", anchor="start"))
    b.append(label(tx, 68, "x(t+h) = x + h·v + (h²/2)·a + (h³/6)·ȧ + …",
                   cls="sm mono t-hi", anchor="start"))
    b.append(label(tx, 96, "Euler keeps the first two terms and drops", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, 112, "the rest. What it drops is dominated by", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, 128, "(h²/2)·a — curvature — which is the amber gap.",
                   cls="xs muted", anchor="start"))

    rows = [("local, one step", "O(h²)"),
            ("steps in time T", "T/h"),
            ("global, after T", "O(h)")]
    el, hgt = table(tx, 156, ["error, by the term that is dropped", ""], rows, [220, 90])
    b += el
    y = 156 + hgt + 20
    b.append(label(tx, y, "One factor of h is paid back by taking", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 16, "more steps. That is ALL that “first order”",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 32, "means, and figure 2 is why it is not the",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 48, "number that decides whether a game works.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "One Euler step drawn as a tangent line, with the gap to the true curve",
               "A curve with visible curvature, the tangent at one point extended by a step h, "
               "and the amber gap between where the tangent lands and where the curve actually "
               "goes. Beside it the Taylor expansion with the dropped terms named.", b)


# ===========================================================================
# Figure 2 — the yardstick that cannot see it
# ===========================================================================
def fig2():
    uid = "l81f2"
    W, H = 900, 496
    b = [cmarker(uid, "red", RED)]

    # semi-implicit is drawn THICK and verlet DASHED on top of it, because on
    # this problem they agree to three digits and a plain second line would
    # simply erase the first. The overlap is the result; hiding it would be a
    # different figure.
    series = ((1, I_EXPLICIT, 2.0, None), (2, I_SEMI, 3.2, None),
              (3, I_VERLET, 1.8, "5 4"))

    # ---- left: error against h (the textbook question) --------------------
    px, py, pw, ph = 62, 56, 340, 250
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "A.1  error after ONE PERIOD, against h",
                   cls="xs muted", anchor="start"))

    def hx(rate):
        return logspan(rate, 60, 1920, px, pw)

    def ey(v):
        return py + ph - (math.log10(max(v, 1e-6)) - math.log10(1e-6)) / 6.0 * ph

    for rate in (60, 120, 240, 480, 960, 1920):
        b.append(rule(hx(rate), py, hx(rate), py + ph, cls="grid"))
        b.append(label(hx(rate), py + ph + 16, f"1/{rate}", cls="xs mono"))
    for dec in range(-6, 1):
        b.append(rule(px, ey(10.0 ** dec), px + pw, ey(10.0 ** dec), cls="grid"))
        b.append(label(px - 8, ey(10.0 ** dec) + 4, f"1e{dec}", cls="xs muted", anchor="end"))
    b.append(label(px + pw / 2, py + ph + 36, "step size", cls="xs muted"))

    for idx, col, wid, dsh in series:
        pts = [(hx(r[0]), ey(r[idx])) for r in ORDER]
        b.append(poly(pts, col, width=wid, dash=dsh, close=False))

    b.append(cline(px, ey(1e-6), px + pw, ey(1e-6), AMBER, width=1.6, dash="4 3"))
    b.append(label(px + 8, ey(1e-6) - 10, "float's own floor", cls="xs mono",
                   anchor="start"))

    # ---- right: error against t at fixed h (the game's question) ----------
    qx = 520
    b.append(frame(qx, py, pw, ph))
    b.append(label(qx, py - 16, "A.3  error against TIME, at h = 1/60",
                   cls="xs muted", anchor="start"))

    def tx_(t):
        return logspan(t, 1.0, 40.0, qx, pw)

    def ty(v):
        return py + ph - (math.log10(max(v, 1e-3)) - math.log10(1e-3)) / 9.0 * ph

    for t in (1, 2, 5, 10, 20, 40):
        b.append(rule(tx_(t), py, tx_(t), py + ph, cls="grid"))
        b.append(label(tx_(t), py + ph + 16, str(t), cls="xs mono"))
    for dec in range(-3, 7):
        b.append(rule(qx, ty(10.0 ** dec), qx + pw, ty(10.0 ** dec), cls="grid"))
        if dec % 2 == 1 or dec == -3:
            b.append(label(qx - 8, ty(10.0 ** dec) + 4, f"1e{dec}", cls="xs muted",
                           anchor="end"))
    b.append(label(qx + pw / 2, py + ph + 36, "simulated seconds", cls="xs muted"))

    for idx, col, wid, dsh in series:
        pts = [(tx_(r[0]), ty(r[idx])) for r in DRIFT]
        b.append(poly(pts, col, width=wid, dash=dsh, close=False))


    # ---- the slopes, and the point ---------------------------------------
    ly = py + ph + 54
    b += legend(px, ly, [(I_EXPLICIT, f"explicit Euler — slope {SLOPES[0]:.3f}"),
                         (I_SEMI, f"semi-implicit Euler — slope {SLOPES[1]:.3f}"),
                         (I_VERLET, f"velocity Verlet — slope {SLOPES[2]:.3f}")])
    b.append(label(qx, ly, "Order is a statement about h → 0 at fixed t.",
                   cls="xs", anchor="start"))
    b.append(label(qx, ly + 17, "A game asks the opposite: t → ∞ at fixed h.",
                   cls="xs t-hi", anchor="start"))
    b.append(label(qx, ly + 34, "Explicit MULTIPLIES by 1.9 a second. The other two",
                   cls="xs muted", anchor="start"))
    b.append(label(qx, ly + 51, "are straight on a log axis, which is ADDING.",
                   cls="xs muted", anchor="start"))
    b.append(label(qx, ly + 74, "Nothing measured on the left can see any of it.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "Two log-log plots: error against step size, and error against simulated time",
               "On the left the error after one period falls as the step shrinks, and all three "
               "rules look respectable. On the right the step is held at 1/60 and time is varied "
               "instead: explicit Euler's error multiplies while the other two only add.", b)


# ===========================================================================
# Figure 3 — phase space, where the difference is visible
# ===========================================================================
def fig3():
    uid = "l81f3"
    W, H = 900, 540
    b = [cmarker(uid, "red", RED), cmarker(uid, "green", GREEN),
         cmarker(uid, "grey", GREY)]

    # A DELIBERATELY BIG STEP. h*w = 0.7 is nine steps per oscillation, far
    # coarser than anything shippable, and it is chosen so that one step is a
    # visible arc rather than a dot. Everything said below is about the SHAPE of
    # the step, which does not depend on its size.
    hw = 0.7
    cx, cy, RD = 216, 190, 122

    b.append(rule(cx - RD - 34, cy, cx + RD + 34, cy, cls="grid"))
    b.append(rule(cx, cy - RD - 34, cx, cy + RD + 34, cls="grid"))
    b.append(label(cx + RD + 44, cy + 4, "x", cls="sm mono", anchor="start"))
    b.append(label(cx + 10, cy - RD - 40, "v / \u03c9", cls="sm mono", anchor="start"))

    def sc(x, v):
        return cx + x * RD, cy - v * RD

    b.append(f'<circle cx="{cx}" cy="{cy}" r="{RD}" fill="none" stroke="{I_EXACT}" '
             f'stroke-width="1.8" stroke-dasharray="5 4"/>')

    # Start in the lower-right quadrant, so the step travels up and right into
    # the emptiest part of the picture.
    a0 = 290.0 * D
    x0, v0 = math.cos(a0), -math.sin(a0)
    q0 = v0 * v0 + x0 * x0 - hw * x0 * v0

    aa = math.sqrt(q0 / (1.0 - hw / 2.0))
    cc = math.sqrt(q0 / (1.0 + hw / 2.0))
    pts = []
    for i in range(0, 129):
        th = 2 * math.pi * i / 128.0
        su, uu = aa * math.cos(th), cc * math.sin(th)
        pts.append(sc((su + uu) / math.sqrt(2.0), (su - uu) / math.sqrt(2.0)))
    b.append(poly(pts, I_SEMI, width=1.5, dash="3 3"))

    xe, ve = x0 + hw * v0, v0 - hw * x0
    vs = v0 - hw * x0
    xs = x0 + hw * vs

    sx, sy = sc(x0, v0)
    ex, ey_ = sc(xe, ve)
    gx_, gy_ = sc(xs, vs)

    # THE LINE THAT MAKES THE FIGURE. Both endpoints sit at the same height,
    # because both rules run the identical velocity update. Drawing it first
    # means the two arrows are read as differing in ONE coordinate.
    b.append(cline(cx - RD - 20, ey_, ex + 30, ey_, GREY, width=1.2, dash="2 4"))
    b.append(label(cx - RD - 24, ey_ + 4, "same v", cls="xs mono muted", anchor="end"))

    b.append(carrow(sx, sy, ex, ey_, uid, "red", I_EXPLICIT, width=2.2))
    b.append(carrow(sx, sy, gx_, gy_, uid, "green", I_SEMI, width=2.2))
    b.append(f'<circle cx="{sx:.1f}" cy="{sy:.1f}" r="5" fill="{I_EXACT}"/>')
    b.append(f'<circle cx="{ex:.1f}" cy="{ey_:.1f}" r="4.5" fill="{I_EXPLICIT}"/>')
    b.append(f'<circle cx="{gx_:.1f}" cy="{gy_:.1f}" r="4.5" fill="{I_SEMI}"/>')
    b.append(label(sx, sy + 20, "start", cls="xs mono"))

    # ---- the annotation column, OUTSIDE the plot -------------------------
    ax = 404
    b.append(carrow(ax - 14, 92, ex + 8, ey_ - 6, uid, "red", I_EXPLICIT, width=1.2))
    b.append(label(ax, 96, "explicit Euler lands OUTSIDE both curves.",
                   cls="xs t-bad", anchor="start"))
    b.append(carrow(ax - 14, 130, gx_ + 6, gy_ - 8, uid, "green", I_SEMI, width=1.2))
    b.append(label(ax, 134, "semi-implicit Euler lands ON the dashed",
                   cls="xs t-ok", anchor="start"))
    b.append(label(ax, 150, "ellipse, and stays on it forever.", cls="xs t-ok",
                   anchor="start"))

    b.append(label(ax, 190, "Both rules ran the SAME velocity line,",
                   cls="sm", anchor="start"))
    b.append(label(ax, 210, "which is why both dots are at the same",
                   cls="sm", anchor="start"))
    b.append(label(ax, 230, "height. They differ only in WHICH x the",
                   cls="sm", anchor="start"))
    b.append(label(ax, 250, "position line was told to move at.",
                   cls="sm", anchor="start"))
    b.append(label(ax, 278, "The pale circle is the exact orbit. The",
                   cls="xs muted", anchor="start"))
    b.append(label(ax, 294, "dashed green ellipse is the quantity",
                   cls="xs muted", anchor="start"))
    b.append(label(ax, 310, "figure 6 is about \u2014 semi-implicit Euler",
                   cls="xs muted", anchor="start"))
    b.append(label(ax, 326, "does not conserve energy, it conserves",
                   cls="xs muted", anchor="start"))
    b.append(label(ax, 342, "that.", cls="xs muted", anchor="start"))

    # ---- inset: the same step applied to an area -------------------------
    ix, iy, IR = 780, 168, 86
    b.append(frame(ix - IR, iy - IR, 2 * IR, 2 * IR))
    b.append(label(ix - IR, iy - IR - 14, "the step, applied to an AREA",
                   cls="xs muted", anchor="start"))

    e = 0.62
    base = (-0.31, -0.31)
    square = [base, (base[0] + e, base[1]), (base[0] + e, base[1] + e),
              (base[0], base[1] + e)]

    def img(fn, colour, width, dash=None):
        scr = [(ix + p[0] * IR, iy - p[1] * IR) for p in (fn(q[0], q[1]) for q in square)]
        return poly(scr, colour, width=width, dash=dash)

    b.append(img(lambda x, v: (x, v), I_EXACT, 1.6, "4 3"))
    b.append(img(lambda x, v: (x + hw * v, v - hw * x), I_EXPLICIT, 2.0))
    b.append(img(lambda x, v: (x + hw * (v - hw * x), v - hw * x), I_SEMI, 2.0))

    b.append(label(ix, iy + IR + 22, "grey dashed: what we started with",
                   cls="xs muted"))
    b.append(label(ix, iy + IR + 40,
                   f"red: \u00d7(1 + h\u00b2\u03c9\u00b2) = \u00d7{1 + hw * hw:.2f}",
                   cls="xs mono t-bad"))
    b.append(label(ix, iy + IR + 58, "green: \u00d71, exactly", cls="xs mono t-ok"))

    # ---- the measured column ---------------------------------------------
    rows = [(n, f"{got:.9f}", f"{want:.9f}") for n, got, want in DET]
    el, hgt = table(60, 388, ["B.1  measured at h = 1/60", "measured", "predicted"], rows,
                    [156, 120, 120])
    b += el
    b.append(label(60, 388 + hgt + 22,
                   f"control, h = 0: all three measure {DET_H0:.9f}, the meter's own floor.",
                   cls="xs mono muted", anchor="start"))

    b.append(label(500, 400,
                   "One step is a MATRIX.", cls="sm", anchor="start"))
    b.append(label(500, 422,
                   "A matrix has a determinant, and a", cls="sm", anchor="start"))
    b.append(label(500, 442,
                   "determinant is the factor it scales", cls="sm", anchor="start"))
    b.append(label(500, 462, "area by.", cls="sm", anchor="start"))
    b.append(label(500, 492,
                   "Explicit Euler's is bigger than one at", cls="sm t-hi", anchor="start"))
    b.append(label(500, 512,
                   "every step size there is.", cls="sm t-hi", anchor="start"))

    return svg(uid, W, H,
               "Phase space with the exact circular orbit and one step of two integrators",
               "A point on the exact orbit is stepped by explicit Euler, which lands outside "
               "every curve in the picture, and by semi-implicit Euler, which lands on the "
               "tilted ellipse it conserves. Both land at the same velocity because both ran "
               "the same velocity update. An inset shows the same step applied to a small "
               "square, which grows under one rule and is preserved exactly under the other.", b)


# ===========================================================================
# Figure 4 — three matrices, three fates
# ===========================================================================
def fig4():
    uid = "l81f4"
    W, H = 900, 400
    b = []

    cards = [
        ("explicit Euler", I_EXPLICIT,
         ["vₙ₊₁ = vₙ − hω²·xₙ",
          "xₙ₊₁ = xₙ + h·vₙ"],
         ["⎡  1      h  ⎤", "⎣ −hω²   1  ⎦"],
         "det = 1 + h²ω²",
         "> 1 always. GROWS.", "t-bad"),
        ("semi-implicit Euler", I_SEMI,
         ["vₙ₊₁ = vₙ − hω²·xₙ",
          "xₙ₊₁ = xₙ + h·vₙ₊₁"],
         ["⎡ 1−h²ω²   h ⎤", "⎣  −hω²    1 ⎦"],
         "det = 1",
         "exactly. PRESERVES.", "t-ok"),
        ("backward Euler", GREY,
         ["vₙ₊₁ = vₙ − hω²·xₙ₊₁",
          "xₙ₊₁ = xₙ + h·vₙ₊₁"],
         ["  1   ⎡  1      h  ⎤", "——— ⎣ −hω²   1  ⎦"],
         "det = 1 / (1 + h²ω²)",
         "< 1 always. SHRINKS.", "muted"),
    ]

    cw, gap = 266, 22
    for i, (name, col, upd, mat, det, verdict, cls) in enumerate(cards):
        x = 40 + i * (cw + gap)
        b.append(hollow(x, 52, cw, 250, col))
        b.append(cline(x + 14, 78, x + cw - 14, 78, col, width=2.2))
        b.append(label(x + cw / 2, 72, name, cls="sm"))
        for k, ln in enumerate(upd):
            b.append(label(x + cw / 2, 104 + k * 20, ln, cls="sm mono"))
        for k, ln in enumerate(mat):
            b.append(label(x + cw / 2, 166 + k * 20, ln, cls="sm mono"))
        b.append(label(x + cw / 2, 232, det, cls="sm mono t-hi"))
        b.append(label(x + cw / 2, 262, verdict, cls="sm " + cls))
        b.append(label(x + cw / 2, 286, "one subscript apart" if i == 1 else
                       ("the obvious order" if i == 0 else "an implicit solve"),
                       cls="xs muted"))

    b.append(label(W / 2, 336,
                   "The ONLY difference between the first two is which velocity the position "
                   "line uses — vₙ or vₙ₊₁.",
                   cls="sm"))
    b.append(label(W / 2, 358,
                   f"That one subscript moves the determinant from "
                   f"{1 + U60:.9f} to exactly 1.000000000 at h = 1/60.",
                   cls="sm mono t-hi"))
    b.append(label(W / 2, 382,
                   "Backward Euler is the mirror image of the first and is why nobody ships it: "
                   "costly to solve, and it bleeds energy.",
                   cls="xs muted"))

    return svg(uid, W, H,
               "Three integrators as update matrices with their determinants",
               "Explicit Euler, semi-implicit Euler and backward Euler written as two update "
               "lines and a 2x2 matrix each, with determinants 1+h^2w^2, exactly 1, and "
               "1/(1+h^2w^2) respectively.", b)


# ===========================================================================
# Figure 5 — a spring, run long
# ===========================================================================
def fig5():
    uid = "l81f5"
    W, H = 900, 448
    b = [cmarker(uid, "red", RED)]

    px, py, pw, ph = 62, 56, 430, 260
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "C.1  energy, as a multiple of the energy it started with",
                   cls="xs muted", anchor="start"))

    def tx_(t):
        return px + t / 60.0 * pw

    def ey(v):
        return py + ph - (math.log10(max(v, 1e-1)) + 1.0) / 19.0 * ph

    for t in (0, 10, 20, 30, 40, 50, 60):
        b.append(rule(tx_(t), py, tx_(t), py + ph, cls="grid"))
        b.append(label(tx_(t), py + ph + 16, str(t), cls="xs mono"))
    for dec in (-1, 2, 5, 8, 11, 14, 17):
        b.append(rule(px, ey(10.0 ** dec), px + pw, ey(10.0 ** dec), cls="grid"))
        b.append(label(px - 8, ey(10.0 ** dec) + 4, f"1e{dec}", cls="xs muted", anchor="end"))
    b.append(label(px + pw / 2, py + ph + 36, "simulated seconds", cls="xs muted"))

    # explicit: the closed form (1+u)^N, sampled densely, with the measured
    # points on top of it. They must coincide; that is the claim.
    pts = []
    for i in range(0, 241):
        t = 60.0 * i / 240.0
        pts.append((tx_(t), ey((1.0 + U60) ** (t * 60.0))))
    b.append(poly(pts, I_EXPLICIT, width=2.0, close=False))
    for t, ex, si, ve in ENERGY:
        b.append(f'<circle cx="{tx_(t):.1f}" cy="{ey(ex):.1f}" r="3.2" fill="{I_EXPLICIT}"/>')
        b.append(f'<circle cx="{tx_(t):.1f}" cy="{ey(si):.1f}" r="3.2" fill="{I_SEMI}"/>')
        b.append(f'<circle cx="{tx_(t):.1f}" cy="{ey(ve):.1f}" r="3.2" fill="{I_VERLET}"/>')
    b.append(poly([(tx_(t), ey(si)) for t, _e, si, _v in ENERGY], I_SEMI, width=3.2,
                  close=False))
    b.append(poly([(tx_(t), ey(ve)) for t, _e, _s, ve in ENERGY], I_VERLET, width=1.8,
                  dash="5 4", close=False))
    b.append(cline(px, ey(1.0), px + pw, ey(1.0), I_EXACT, width=1.4, dash="4 3"))


    b += legend(px, py + ph + 54,
                [(I_EXPLICIT, "explicit Euler"), (I_SEMI, "semi-implicit Euler"),
                 (I_VERLET, "velocity Verlet \u2014 dashed, on top of green")])
    b.append(label(px, py + ph + 54 + 3 * 17,
                   "red line = (1 + h\u00b2\u03c9\u00b2)\u1d3a  \u00b7  red dots = the measured run",
                   cls="xs mono", anchor="start"))

    # ---- right: the amplitude, in metres ---------------------------------
    tx = 560
    rows = [(n, f"{a:.4e} m") for (n, _g, _p), a in zip(DET, AMP60)]
    el, hgt = table(tx, 56, ["a 1 m spring, after one minute", ""], rows, [200, 118])
    b += el
    y = 56 + hgt + 22
    b.append(label(tx, y, "One centimetre of overlap between a box and", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 16, "the floor is a spring. Under explicit Euler", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 32, "that centimetre becomes 3,357 km in a", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 48, "minute — and the first ten seconds look", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 64, "completely normal, which is the trap.", cls="xs t-hi",
                   anchor="start"))

    rows2 = [("energy after 60 s", f"{ENERGY[-1][1]:.4e}"),
             ("(1 + h²ω²)³⁶⁰⁰", f"{(1 + U60) ** 3600:.4e}")]
    el2, _h2 = table(tx, y + 92, ["the determinant IS the blow-up", ""], rows2, [200, 118])
    b += el2

    return svg(uid, W, H,
               "Energy against time on a log axis for three integrators on a spring",
               "Explicit Euler's energy climbs as a straight line on a log axis, reaching 1e17 "
               "times its starting value after a minute, and the drawn line is the closed form "
               "predicted by the determinant. The other two sit flat on 1.", b)


# ===========================================================================
# Figure 6 — what semi-implicit Euler actually conserves
# ===========================================================================
def fig6():
    uid = "l81f6"
    W, H = 900, 430
    b = [cmarker(uid, "green", GREEN)]

    def panel(ox, oy, R, hw, title, ok):
        out = [rule(ox - R - 22, oy, ox + R + 22, oy, cls="grid"),
               rule(ox, oy - R - 22, ox, oy + R + 22, cls="grid"),
               label(ox, oy - R - 36, title, cls="xs mono")]
        out.append(f'<circle cx="{ox}" cy="{oy}" r="{R * 0.72:.1f}" fill="none" '
                   f'stroke="{I_EXACT}" stroke-width="1.4" stroke-dasharray="4 3"/>')
        if ok:
            # level set of q^2 + p^2 - hw*p*q = q0: principal axes at 45 degrees
            a = 0.72 / math.sqrt(1.0 - hw / 2.0)
            c = 0.72 / math.sqrt(1.0 + hw / 2.0)
            pts = []
            for i in range(0, 129):
                th = 2 * math.pi * i / 128.0
                su, uu = a * math.cos(th), c * math.sin(th)
                pts.append((ox + (su + uu) / math.sqrt(2.0) * R,
                            oy - (su - uu) / math.sqrt(2.0) * R))
            out.append(poly(pts, I_SEMI, width=2.2))
        else:
            # hw > 2: the form is indefinite, so the level sets are hyperbolae
            for sgn in (1, -1):
                pts = []
                for i in range(0, 61):
                    t = -1.05 + 2.10 * i / 60.0
                    su = 0.52 * math.sinh(t) * sgn
                    uu = 0.52 * math.cosh(t) * sgn
                    pts.append((ox + (su + uu) / math.sqrt(2.0) * R,
                                oy - (su - uu) / math.sqrt(2.0) * R))
                out.append(poly(pts, I_EXPLICIT, width=2.2, close=False))
        return out

    b += panel(146, 166, 94, 0.6, "h\u03c9 = 0.6 \u2014 an ellipse, so BOUNDED", True)
    b += panel(376, 166, 94, 2.1, "h\u03c9 = 2.1 \u2014 a hyperbola, so not", False)

    b.append(label(146, 300, "the conserved quantity is", cls="xs muted"))
    b.append(label(146, 316, "positive-definite \u2192 its level", cls="xs muted"))
    b.append(label(146, 332, "sets are closed \u2192 nothing leaves", cls="xs t-ok"))
    b.append(label(376, 300, "past h\u03c9 = 2 it is indefinite", cls="xs muted"))
    b.append(label(376, 316, "\u2192 there is no closed curve", cls="xs t-bad"))
    b.append(label(376, 332, "left to sit on", cls="xs t-bad"))

    rows = [("true energy", f"{TRUE_MIN:.4f}", f"{TRUE_MAX:.4f}", f"{TRUE_SPREAD:.3e}"),
            ("shadow energy", f"{SHAD_MIN:.4f}", f"{SHAD_MAX:.4f}", f"{SHAD_SPREAD:.3e}")]
    tx = 520
    el, hgt = table(tx, 56, ["C.4  over one minute", "min", "max", "spread"], rows,
                    [112, 78, 78, 74])
    b += el
    y = 56 + hgt + 24
    lines = [("The true energy is NOT conserved: it", "xs muted"),
             ("wobbles by exactly h\u03c9 peak to peak,", "xs muted"),
             (f"predicted {HW60:.6f}, measured {TRUE_SPREAD:.6f}.", "xs mono t-hi"),
             ("", None),
             ("What IS conserved is the sheared cousin", "xs muted"),
             ("drawn on the left \u2014 to five decimal places,", "xs muted"),
             ("over 3,600 steps.", "xs muted"),
             ("", None),
             ("Stability and the existence of that", "xs"),
             ("closed curve are the SAME statement.", "xs t-hi")]
    for i, (text, cls) in enumerate(lines):
        if cls:
            b.append(label(tx, y + i * 17, text, cls=cls, anchor="start"))

    b.append(label(W / 2, 400,
                   "\u00bd(v\u00b2 + \u03c9\u00b2x\u00b2 \u2212 h\u03c9\u00b2\u00b7xv)"
                   "  \u2014  positive-definite exactly when h\u03c9 < 2, "
                   "which is the stability limit.",
                   cls="sm mono t-hi"))

    return svg(uid, W, H,
               "The conserved quantity of semi-implicit Euler as a tilted ellipse, and as a "
               "hyperbola past the stability limit",
               "Below the limit the conserved quadratic form is positive-definite and its level "
               "set is a closed tilted ellipse the trajectory never leaves. Above it the form is "
               "indefinite and the level sets are hyperbolae that run away.", b)


# ===========================================================================
# Figure 8 — gravity, where the choice very nearly does not matter
# ===========================================================================
def fig8():
    uid = "l81f8"
    W, H = 900, 400
    b = [cmarker(uid, "grey", GREY)]

    px, py, pw, ph = 62, 56, 380, 250
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "E.2  position error against time, from rest, g = 9.81",
                   cls="xs muted", anchor="start"))

    def tx_(t):
        return px + t / 8.0 * pw

    def ey(v):
        return py + ph / 2 - v / 0.85 * (ph / 2)

    for t in (0, 2, 4, 6, 8):
        b.append(rule(tx_(t), py, tx_(t), py + ph, cls="grid"))
        b.append(label(tx_(t), py + ph + 16, str(t), cls="xs mono"))
    for v in (-0.8, -0.4, 0.0, 0.4, 0.8):
        b.append(rule(px, ey(v), px + pw, ey(v), cls="grid"))
        b.append(label(px - 8, ey(v) + 4, f"{v:+.1f}", cls="xs muted", anchor="end"))
    b.append(label(px + pw / 2, py + ph + 36, "simulated seconds", cls="xs muted"))
    b.append(label(px - 46, py - 4, "m", cls="xs muted"))

    for idx, col in ((1, I_EXPLICIT), (2, I_SEMI), (3, I_VERLET)):
        pts = [(tx_(0), ey(0.0))] + [(tx_(r[0]), ey(r[idx])) for r in FALL_T]
        b.append(poly(pts, col, width=2.2, close=False))
    b.append(cline(px, ey(0.0), px + pw, ey(0.0), I_EXACT, width=1.4, dash="4 3"))
    b.append(label(tx_(6.2), ey(0.50) - 8, "+½·g·h·t", cls="xs mono",
                   anchor="middle"))
    b.append(label(tx_(6.2), ey(-0.50) + 16, "−½·g·h·t",
                   cls="xs mono", anchor="middle"))
    b.append(label(tx_(4.4), ey(0.04) - 8, "velocity Verlet: exact", cls="xs mono t-ok",
                   anchor="middle"))

    tx = 500
    rows = [(n, f"{fallen:.6f}", f"{err:+.6f}") for n, fallen, err in FALL]
    el, hgt = table(tx, 56, ["E.1  after 1 second", "fallen (m)", "error"], rows,
                    [128, 108, 100])
    b += el
    y = 56 + hgt + 22
    b.append(label(tx, y, f"Exact: {0.5 * 9.81:.4f} m. Predicted error",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 16, f"½·g·h·t = {FALL_PRED:.6f} m — one rule short",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 32, "by it, one long by it. The SPEED is the",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 48, "same to the last bit in all three.",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 80, "LINEAR in t, not exponential. Gravity is",
                   cls="sm", anchor="start"))
    b.append(label(tx, y + 100, "forgiving, and that is why this bug ships:",
                   cls="sm", anchor="start"))
    b.append(label(tx, y + 120, "your first falling-cube demo is FINE.",
                   cls="sm t-hi", anchor="start"))
    b.append(label(tx, y + 144, "It stops being fine the moment a force",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 160, "depends on WHERE the body is.",
                   cls="xs muted", anchor="start"))

    return svg(uid, W, H,
               "Position error against time under constant gravity for three integrators",
               "Explicit Euler falls short by half g h t and semi-implicit Euler overshoots by "
               "the same amount, both growing linearly. Velocity Verlet sits on zero because the "
               "half a h squared term it keeps is exactly the one the others drop.", b)


# ===========================================================================
# Figure 7 — the limit, and the last ten per cent before it
# ===========================================================================
def fig7():
    uid = "l81f7"
    W, H = 900, 400
    b = [cmarker(uid, "red", RED)]

    px, py, pw, ph = 62, 56, 420, 250
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "F.2  worst energy excursion against hω",
                   cls="xs muted", anchor="start"))

    def gx(hw):
        return px + hw / 2.2 * pw

    def gy(v):
        return py + ph - (math.log10(max(v, 1.0))) / 2.6 * ph

    for hw in (0.0, 0.5, 1.0, 1.5, 2.0):
        b.append(rule(gx(hw), py, gx(hw), py + ph, cls="grid"))
        b.append(label(gx(hw), py + ph + 16, f"{hw:.1f}", cls="xs mono"))
    for v in (1, 10, 100):
        b.append(rule(px, gy(v), px + pw, gy(v), cls="grid"))
        b.append(label(px - 8, gy(v) + 4, f"×{v}", cls="xs muted", anchor="end"))
    b.append(label(px + pw / 2, py + ph + 36, "hω  (step size × angular frequency)",
                   cls="xs muted"))

    pts = []
    hw = 0.02
    while hw < 1.995:
        pts.append((gx(hw), gy(1.0 / (1.0 - hw / 2.0))))
        hw += 0.01
    b.append(poly(pts, I_EXACT, width=2.0, close=False))
    for hw, got, _want in EXCURSION:
        b.append(f'<circle cx="{gx(hw):.1f}" cy="{gy(got):.1f}" r="3.4" fill="{I_SEMI}"/>')

    b.append(cline(gx(2.0), py, gx(2.0), py + ph, I_EXPLICIT, width=2.0, dash="5 4"))
    b.append(label(gx(2.0) - 8, py + 18, "hω = 2", cls="xs mono t-bad", anchor="end"))
    b.append(label(gx(2.0) - 8, py + 34, "the wall", cls="xs mono t-bad", anchor="end"))
    b.append(cline(gx(0.4), py, gx(0.4), py + ph, GREEN, width=1.6, dash="3 3"))
    b.append(label(gx(0.44), py + 52, "a factor of five", cls="xs mono t-ok",
                   anchor="start"))
    b.append(label(gx(0.44), py + 68, "of headroom", cls="xs mono t-ok", anchor="start"))

    tx = 540
    rows = [("semi-implicit Euler", f"{LIMIT_MEASURED[0]:.7f}"),
            ("velocity Verlet", f"{LIMIT_MEASURED[1]:.7f}"),
            ("2 / ω", f"{LIMIT_EXACT:.7f}"),
            ("explicit Euler", "none")]
    el, hgt = table(tx, 56, ["F.1  largest stable h, ω = 2π", ""], rows, [176, 120])
    b += el
    y = 56 + hgt + 22
    b.append(label(tx, y, "Read it the other way round, which is how", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 16, "it is used. At 60 Hz with a safety factor", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 32, "of five, hω ≤ 0.4 admits ω ≤ 24 rad/s:",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, y + 48, "a spring of about 3.8 Hz.", cls="xs mono t-hi",
                   anchor="start"))
    b.append(label(tx, y + 76, "A real contact is hundreds of Hz. That is", cls="xs",
                   anchor="start"))
    b.append(label(tx, y + 92, "why contacts are not springs in this", cls="xs",
                   anchor="start"))
    b.append(label(tx, y + 108, "module — they are constraints, and 8.10", cls="xs",
                   anchor="start"))
    b.append(label(tx, y + 124, "is where that bill comes due.", cls="xs", anchor="start"))

    return svg(uid, W, H,
               "Worst energy excursion against h times omega, with the stability wall at 2",
               "The measured excursions land on the predicted curve one over one minus h omega "
               "over two, which rises without bound as h omega approaches 2. Everything left of "
               "the wall is stable and only the left fifth of it is usable.", b)


# ===========================================================================
# Figure 9 — the two ways velocity gets destroyed
# ===========================================================================
def fig9():
    uid = "l81f9"
    W, H = 900, 410
    b = [cmarker(uid, "red", RED)]

    # ---- left: v *= 0.99 at three rates ----------------------------------
    px, py, pw, ph = 62, 66, 360, 250
    b.append(frame(px, py, pw, ph))
    b.append(label(px, py - 16, "G.1  `v *= 0.99f` once per step, for one second",
                   cls="xs muted", anchor="start"))

    def tx_(t):
        return px + t * pw

    def vy(v):
        return py + ph - v * ph

    for t in (0.0, 0.25, 0.5, 0.75, 1.0):
        b.append(rule(tx_(t), py, tx_(t), py + ph, cls="grid"))
        b.append(label(tx_(t), py + ph + 16, f"{t:.2f}", cls="xs mono"))
    for v in (0.0, 0.25, 0.5, 0.75, 1.0):
        b.append(rule(px, vy(v), px + pw, vy(v), cls="grid"))
        b.append(label(px - 8, vy(v) + 4, f"{v:.2f}", cls="xs muted", anchor="end"))
    b.append(label(px + pw / 2, py + ph + 36, "seconds", cls="xs muted"))

    colours = {30: AMBER, 60: I_SEMI, 120: I_VERLET, 144: I_EXPLICIT}
    for rate, after, _fixed in DAMP:
        pts = []
        for i in range(0, int(rate) + 1):
            pts.append((tx_(i / rate), vy(0.99 ** i)))
        b.append(poly(pts, colours[rate], width=2.0, close=False))
        b.append(label(tx_(1.0) + 8, vy(after) + 4, f"{rate} Hz", cls="xs mono",
                       anchor="start"))
    b.append(cline(px, vy(DAMP_TARGET), px + pw, vy(DAMP_TARGET), I_EXACT,
                   width=1.6, dash="4 3"))
    b.append(label(px + pw, py - 34,
                   f"pow(r, h) gives {DAMP_TARGET:.6f} at EVERY rate \u2014 the dashed line",
                   cls="xs mono", anchor="end"))

    # ---- right: drag, and the step that reverses --------------------------
    tx = 512
    rows = [(f"{k:.0f}", f"{hk:.4f}", f"{eu:+.5f}", f"{ex:.6f}", v)
            for k, hk, eu, ex, v in DRAG]
    el, hgt = table(tx, 56, ["k", "h·k", "1 − h·k", "exp(−h·k)", ""],
                    rows, [48, 62, 74, 82, 96])
    b += el
    y = 56 + hgt + 22
    b.append(label(tx, y, "Drag is not a position force, so", cls="xs muted", anchor="start"))
    b.append(label(tx, y + 16, "semi-implicit Euler does not help:", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 32, "velocity is updated from velocity, and", cls="xs muted",
                   anchor="start"))
    b.append(label(tx, y + 48, "the step is plain explicit Euler.", cls="xs muted",
                   anchor="start"))
    rows2 = [(str(n), f"{eu:+.6f}", f"{ex:.6f}") for n, eu, ex in DRAG_STEPS]
    el2, _h2 = table(tx, y + 72, ["k = 90, h·k = 1.5", "explicit v", "exact"],
                     rows2, [126, 108, 100])
    b += el2
    b.append(label(tx, y + 72 + 5 * 17 + 40,
                   "A drag force that makes the sign flip every step.",
                   cls="xs t-bad", anchor="start"))

    return svg(uid, W, H,
               "Per-step damping at four step rates, and the drag step that reverses",
               "The same line v times equals 0.99 retains 74 percent of the velocity per second "
               "at 30 Hz and 24 percent at 144 Hz, while pow of the per-second factor gives the "
               "same answer at every rate. Beside it, explicit drag reverses the velocity once "
               "h times k passes 1.", b)


# ===========================================================================
# Figure 10 — the demo, and the budget
# ===========================================================================
def render_panel(ppm, crop, x, y, px, cell=4, levels=3, peak=True):
    """A real render, downsampled and run-length encoded to rectangles."""
    _w, _h, data = read_ppm(os.path.join(OUT, ppm))
    sampler = peak_sample if peak else box_sample
    grid_w, grid_h, grid = sampler(data, _w, crop, cell)
    body = rle_rects(grid, grid_w, grid_h, x, y, px, INTEGRATE_PALETTE, levels=levels,
                     bg_class="fill-shot")
    return (['<g shape-rendering="crispEdges">'] + body + ['</g>'],
            grid_w * px, grid_h * px)


def fig10():
    uid = "l81f10"
    W, H = 900, 560
    b = [cmarker(uid, "grey", GREY)]

    # THE WHOLE FRAME, not just the phase panel: the energy chart on the right
    # is half of what the demo is for, and the annotations below refer to it.
    CROP = (0, 0, 960, 540)
    # cell 3 at 1.5 units per cell, NOT cell 2 at 1.0. Same size on the page and
    # half as many rectangles — but the reason is legibility, not weight: a
    # one-unit rect in a 900-unit viewBox is 0.7 device pixels once the figure is
    # scaled to a phone, and single-cell runs along a thin arc simply vanish. The
    # first version of this figure drew the spiral as a dashed line for exactly
    # that reason, and the dashes were in the renderer, not in the demo.
    CELL, PX = 3, 1.5
    panel, PW, PH = render_panel("l81_demo.ppm", CROP, 40, 56, PX, cell=CELL,
                                 levels=4, peak=True)
    b.append(f'<rect x="{38}" y="{54}" width="{PW + 4:.0f}" height="{PH + 4:.0f}" '
             f'class="fill-shot"/>')
    b += panel
    b.append(label(40, 44, "demos/integrate, six simulated seconds, h = 1/60, ω = 2π",
                   cls="xs muted", anchor="start"))

    # annotations, OUTSIDE the panel
    ax = 40 + PW + 24
    b.append(label(ax, 76, "The red spiral leaves the panel after four", cls="xs",
                   anchor="start"))
    b.append(label(ax, 92, "seconds and is CLIPPED, not rescaled — the", cls="xs",
                   anchor="start"))
    b.append(label(ax, 108, "view follows the worst track only up to", cls="xs muted",
                   anchor="start"))
    b.append(label(ax, 124, "three amplitudes. Without that ceiling the", cls="xs muted",
                   anchor="start"))
    b.append(label(ax, 140, "picture ends up showing nothing but the", cls="xs muted",
                   anchor="start"))
    b.append(label(ax, 156, "thing that is broken.", cls="xs muted", anchor="start"))
    b.append(label(ax, 184, "Green and violet are INDISTINGUISHABLE here,", cls="xs t-ok",
                   anchor="start"))
    b.append(label(ax, 200, "and so is the pale shadow ellipse under them.", cls="xs muted",
                   anchor="start"))
    b.append(label(ax, 216, "That is the result, not a rendering fault:", cls="xs muted",
                   anchor="start"))
    b.append(label(ax, 232, "at hω = 0.105 the ellipse is 2.7% off a", cls="xs muted",
                   anchor="start"))
    b.append(label(ax, 248, "circle and both rules are sitting on it.", cls="xs muted",
                   anchor="start"))
    b.append(label(ax, 276, "Right: the same run as energy, log₁₀, against", cls="xs muted",
                   anchor="start"))
    b.append(label(ax, 292, "a flat line at 1. Straight = exponential.", cls="xs muted",
                   anchor="start"))

    rows = [(n, f"{ns:.3f}", str(ev)) for n, ns, ev in BUDGET]
    el, hgt = table(ax, 318, ["I.1  ns / body / step", "cost", "force evals"], rows,
                    [140, 78, 96])
    b += el
    y = 318 + hgt + 20
    b.append(label(ax, y, f"control, walking the array only: {BUDGET_TOUCH:.3f} ns",
                   cls="xs mono muted", anchor="start"))
    b.append(label(ax, y + 18, "The two Euler rules agree to 3%, against a",
                   cls="xs", anchor="start"))
    b.append(label(ax, y + 34, "run-to-run spread of the same size. Same",
                   cls="xs", anchor="start"))
    b.append(label(ax, y + 50, "cost. One of them is wrong at every h.",
                   cls="xs t-hi", anchor="start"))

    return svg(uid, W, H,
               "A screenshot of the integrate demo beside the measured per-step cost",
               "The phase plot shows a red spiral leaving the panel while green and violet trace "
               "a single closed curve on the exact orbit, and the energy chart shows red climbing "
               "as a straight line on a log axis while the others stay flat.", b)


# ===========================================================================
def main():
    figs = [fig1, fig2, fig3, fig4, fig5, fig6, fig7, fig8, fig9, fig10]
    for i, fn in enumerate(figs, start=1):
        path = os.path.join(OUT, f"l81_fig{i}.svg")
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}  ({os.path.getsize(path):,} bytes)")


if __name__ == "__main__":
    main()
