#!/usr/bin/env python3
"""scratch/figs_811.py — Lesson 8.11's diagrams.

Same rules as 5.1-8.10's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - no hardcoded colour on an `.ink` stroke either
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - NO SUPPRESSED ZERO on any axis whose job is to show a ratio

Every number below is transcribed from scratch/verify_811.log. Nothing here is
estimated, and the harness section each block came from is named above it.

*** WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. ***

THE COLOUR RULE, inherited from 8.4 through 8.10, with this lesson's reading:
  GREEN  = the answer that works — the block, speculation, split impulse at rest
  RED    = the failure — the flipped row, the dislocated limb, the stretched chain
  AMBER  = the lever arm and the thing it sets, rho
  BLUE   = bodies, rows, and the velocity solve
  PURPLE = the closed form a measurement is checked against
  GREY   = discarded work, and axes
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402,F401
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_71 import cmarker, carrow, poly, frame                    # noqa: E402
from figs_76 import table                                           # noqa: E402
from figs_81 import legend, logspan                                 # noqa: E402

OUT = "scratch"


def para(x, y, text, cols=100, cls="xs muted", leading=15, anchor="start"):
    """Wrap `text` to `cols` characters and emit one label per line."""
    words = text.split()
    lines, cur = [], ""
    for w in words:
        if cur and len(cur) + 1 + len(w) > cols:
            lines.append(cur)
            cur = w
        else:
            cur = w if not cur else cur + " " + w
    if cur:
        lines.append(cur)
    body = [label(x, y + i * leading, ln, cls, anchor) for i, ln in enumerate(lines)]
    return body, len(lines) * leading


def tbl(*args, **kwargs):
    return table(*args, **kwargs)


def logy(v, lo, hi, y0, height):
    t = (math.log10(max(v, lo)) - math.log10(lo)) / (math.log10(hi) - math.log10(lo))
    return y0 + height - min(max(t, 0.0), 1.0) * height


def dot(x, y, colour, r=3.2):
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{colour}"/>'


def ring(x, y, colour, r=3.6, width=1.3):
    return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="none" stroke="{colour}" stroke-width="{width}"/>'


def write(name, text):
    with open(os.path.join(OUT, name), "w") as fh:
        fh.write(text)
    print(f"{name}: {len(text)} bytes")


# ===========================================================================
# MEASUREMENTS — every one from scratch/verify_811.log, section named
# ===========================================================================

# §A — a contact is a row
ROW_MASS_REL, ROW_TANGENT_REL, ROW_VEL = 1.858e-07, 1.887e-07, 3.448e-08
LINEAR_ONLY_LO, LINEAR_ONLY_HI = 3.799, 4.032
SWEEP_V, SWEEP_W = 2.608e-08, 1.484e-07
FELL_DERIVED_MM, FELL_NEGATED_MM, FREE_FALL_MM = 26.9, 784.0, 1226.3
HAND_NS, ROW_NS = 11.953, 8.750
HAND_BYTES, ROW_BYTES = 76, 108

# §B — rods and ropes
SWING = [(60, 0.4134, 0.4134, 0.5574, 89.87), (240, 0.4819, 0.4819, 0.6990, 97.32),
         (960, 0.4956, 0.4956, 0.7371, 99.32)]

# §C — the drift
DRIFT = [(1, 1.003466), (2, 1.006897), (10, 1.033151), (30, 1.091214), (60, 1.163953)]
FIRST_DRIFT = 0.003466
RECURRENCE_ERR, ELL_ERR = 1.654e-07, 2.861e-07
KE_KEPT = 0.740938
BODY_LOSS = [("socket", 0.25, 0.4261, 0.001778, 0.001779),
             ("socket", 0.50, 0.7481, 0.002986, 0.002987),
             ("socket", 1.00, 0.9224, 0.003597, 0.003598)]

# §D — correcting a joint
CORR = [("none", 163.9531, None, 0.00386, 0.00380),
        ("Baumgarte", 16.5538, 16.5268, 0.00000, 0.00651),
        ("split impulse", 12.4993, 12.2235, 0.00488, 0.00483)]
LIMB = [("none", 0.20000, 0.000000), ("Baumgarte", 0.00009, 0.349951),
        ("split impulse", 0.00000, 0.000000)]

# §E — the block
VISITS = [(1, 6.948e-01, 1.192e-07), (2, 1.533e-01, 1.490e-08), (3, 3.535e-02, 0.0),
          (4, 8.207e-03, 0.0), (5, 1.908e-03, 0.0), (6, 4.437e-04, 0.0),
          (7, 1.031e-04, 0.0), (8, 2.391e-05, 0.0), (9, 5.487e-06, 0.0), (10, 1.289e-06, 0.0)]
GS_MEASURED, GS_PREDICTED = 0.2323, 0.2325
BLOCK_WORST, ROWS_WORST, CENTRE_WORST = 5.920e-07, 9.249e-01, 9.733e-08
RADIUS_MIN, RADIUS_MED, RADIUS_MAX = 0.001, 0.443, 0.574
CHAIN_BLOCK, CHAIN_ROWS = (93.859, 8.094), (101.944, 11.393)
COST_ROWS, COST_BLOCK = (64.75, 23.09), (58.42, 10.77)

# §F — the pendulum
T_BODY, T_POINT, T_TWO = 1.646116, 1.418503, 1.646241
T_MEASURED = 1.645967
I_CM, I_PIVOT = 0.173333, 0.673333
H2 = [(30, -6.695e-04), (60, -1.664e-04), (120, -4.156e-05), (240, -1.044e-05)]
AMP = [(10, 1.649255, 1.649255, 98.07, 98.42), (30, 1.674773, 1.674763, 94.71, 97.71),
       (60, 1.766582, 1.766389, 85.53, 95.38), (90, 1.942977, 1.941790, 75.45, 92.03),
       (120, 2.259920, 2.254017, 67.33, 88.37)]

# §G — the hinge
DOOR = [("hinge", 0.088, 0.0001, 0.00), ("ball-socket only", 4.694, 0.0, 175.98)]
FLIPS_WORLD, FLIPS_BODY, FLIP_FRAMES = 127, 0, 7787
FLIP_WORST_WORLD, FLIP_WORST_BODY = 90.000, 9.767

# §H — limits
TURNSTILE = [("reactive", 0.4985, 0.9960, 1.02e-03), ("speculative", 0.0000, 0.0000, 0.0)]
RHO = [(0.00, 0.0000, [0.0, 0.0, 0.0, 0.0]),
       (0.25, 0.4261, [0.4261, 0.1816, 0.0330, 0.0011]),
       (0.50, 0.7481, [0.7481, 0.5597, 0.3133, 0.0981]),
       (1.00, 0.9224, [0.9224, 0.8508, 0.7238, 0.5239])]
REBOUND = [(0.00, 0.00000, 0.00000, 0.0000), (0.25, 0.00109, 0.00003, 0.0011),
           (0.50, 0.09813, 0.00318, 0.0753), (1.00, 0.52388, 0.01744, 0.2706)]

# §K — what joints leave behind
STRETCH = {1: [1.33, 0.60, 0.24, 0.08, 0.01], 10: [18.36, 8.86, 4.13, 1.79, 0.66],
           100: [388.14, 158.73, 70.39, 34.66, 16.72]}
SWEEPS = [8, 16, 32, 64, 128]
SUBSTEP = [("8 sweeps × 1 step", 388.142), ("4 sweeps × 2 steps", 163.119),
           ("2 sweeps × 4 steps", 49.051), ("1 sweep × 8 steps", 20.491)]
BUDGET = [("ball-socket, block", 90.46, 13.70), ("ball-socket, rows", 104.21, 35.79),
          ("hinge, block", 112.54, 23.90), ("hinge + limit + motor", 138.79, 59.98),
          ("rod", 90.46, 12.45)]
MANIFOLD_SWEEP_NS = 129


# ===========================================================================
# FIGURE 1 — one row, and the constraints it can be                       (§2)
# ===========================================================================

def fig1():
    uid = "f1"
    W = 980
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "purple", PURPLE)]

    # ---- LEFT: the anatomy of a row --------------------------------------
    lx, ly = 26, 34
    body.append(label(lx, ly, "A row is twelve numbers in four blocks", cls="sm", anchor="start"))
    bw, bh, gap = 92, 34, 8
    names = ["linear_a", "angular_a", "linear_b", "angular_b"]
    vel = ["v_a", "ω_a", "v_b", "ω_b"]
    by = ly + 22
    for i in range(4):
        x = lx + i * (bw + gap)
        body.append(hollow(x, by, bw, bh, BLUE, width=1.3))
        body.append(label(x + bw / 2, by + 21, names[i], cls="xs mono"))
        body.append(label(x + bw / 2, by + bh + 16, "· " + vel[i], cls="xs mono"))
    right_edge = lx + 4 * bw + 3 * gap
    body.append(label(right_edge + 10, by + 21, "= dC/dt", cls="xs mono", anchor="start"))
    body.append(label(lx, by + bh + 38,
                      "Four dot products give the rate at which C changes.", cls="xs",
                      anchor="start"))

    # the impulse direction and the effective mass
    y2 = by + bh + 66
    lines = [
        "The impulse is Jᵀλ: a constraint does no work on any motion it allows,",
        "and only multiples of Jᵀ do no work on every V with J·V = 0. So a whole",
        "constraint answers with ONE number, λ, along a direction the row names.",
        "",
        "Applying Jᵀλ changes J·V by (J M⁻¹ Jᵀ)·λ — a 1×1 matrix, which is 8.9's",
        "effective mass seen from above: m_eff = 1 / (J M⁻¹ Jᵀ).",
    ]
    for i, t in enumerate(lines):
        if t:
            body.append(label(lx, y2 + i * 16, t, cls="xs", anchor="start"))

    # worked example
    wy = y2 + 6 * 16 + 18
    body.append(label(lx, wy, "Worked: a contact at a corner of a 10 kg, 0.5 m crate on a floor",
                      cls="xs", anchor="start"))
    worked = [
        "n = (0, 1, 0),  r_b = (0.25, −0.25, 0.25),  r_b × n = (−0.25, 0, 0.25)",
        "J M⁻¹ Jᵀ = 1/m + (r_b×n)·I⁻¹(r_b×n) = 0.1 + 0.125 / 0.4167 = 0.4",
        "m_eff = 2.5 kg — a quarter of the crate, because 3/4 of a push",
        "at a corner goes into turning it.",
    ]
    for i, t in enumerate(worked):
        body.append(label(lx + 12, wy + 18 + i * 16, t, cls="xs mono" if i < 2 else "xs muted",
                          anchor="start"))

    # ---- RIGHT: the table of constraints ------------------------------------
    rx = 488
    t, th = tbl(rx, 34,
                ["constraint", "linear halves", "angular halves", "bounds on λ"],
                [["contact normal", "−n,  n", "−r_a×n,  r_b×n", "[0, ∞)"],
                 ["friction", "−t,  t", "−r_a×t,  r_b×t", "±μ·λ_n"],
                 ["rod", "−u,  u", "−r_a×u,  r_b×u", "(−∞, ∞)"],
                 ["rope", "u,  −u", "r_a×u,  −r_b×u", "[0, ∞)"],
                 ["socket, per axis", "−e_k,  e_k", "−r_a×e_k,  r_b×e_k", "(−∞, ∞)"],
                 ["hinge alignment", "0,  0", "−p_i,  p_i", "(−∞, ∞)"],
                 ["hinge limit", "0,  0", "±a,  ∓a", "[0, ∞)"],
                 ["motor", "0,  0", "−a,  a", "[−τh, τh]"]],
                [118, 84, 128, 78], title="Every constraint in this lesson, as one row")
    body.extend(t)
    note_y = 34 + th + 26
    txt, nh = para(rx, note_y,
                   "The top four are ONE shape — a point row, `point_row` in the code — "
                   "differing only in their direction and their bounds. A rope is a "
                   "contact turned inside out: the same row with the direction reversed. "
                   "And friction is a motor whose target speed is zero.",
                   cols=78)
    body.extend(txt)

    H = max(wy + 18 + 4 * 16, note_y + nh) + 14
    return svg(uid, W, H,
               "A Jacobian row, and the table of constraints built from it",
               "Left: a row of twelve numbers in four blocks, dotted with the two bodies' "
               "velocities to give dC/dt; the impulse Jᵀλ and the effective mass "
               "1/(J M⁻¹ Jᵀ); and a worked example at a crate's corner giving 2.5 kg. "
               "Right: a table of the eight constraints in the lesson with their linear "
               "halves, angular halves and bounds.", body)


# ===========================================================================
# FIGURE 2 — what a visit costs, both ways                                 (§3)
# ===========================================================================

def fig2():
    uid = "f2"
    W = 980
    body = []

    lx = 26
    body.append(label(lx, 34, "One normal solve, per point per sweep", cls="sm", anchor="start"))

    # THREE REAL COLUMNS. The first draft wrote each row as one monospace
    # string padded with spaces, and SVG collapses runs of whitespace, so the
    # columns came out ragged. Coordinates are the only alignment SVG keeps.
    col_w = 430
    heads = [("8.9, hand-written", BLUE,
              [("relative velocity", "2 × cross, 1 × dot", "15"),
               ("impulse vector", "n · δ", "3"),
               ("linear, both", "2 × scaled add", "6"),
               ("angular, both", "2 × cross, 2 × I⁻¹·v", "30"),
               ("", "total", "54")]),
             ("8.11, as a Jacobian row", GREEN,
              [("J · V", "4 × dot", "12"),
               ("linear, both", "2 × (m⁻¹δ) scaled add", "8"),
               ("angular, both", "2 × scaled add of I⁻¹J", "6"),
               ("", "", ""),
               ("", "total", "26")])]
    for k, (title, col, rows) in enumerate(heads):
        y = 64 + k * 118
        body.append(hollow(lx, y, col_w, 100, col, width=1.3))
        body.append(label(lx + 12, y + 20, title, cls="xs", anchor="start"))
        body.append(label(lx + col_w - 12, y + 20, "multiplies", cls="xs muted", anchor="end"))
        for i, (stage, op, mul) in enumerate(rows):
            yy = y + 40 + i * 14
            if stage:
                body.append(label(lx + 12, yy, stage, cls="xs", anchor="start"))
            if op:
                body.append(label(lx + 150, yy, op, cls="xs mono", anchor="start"))
            if mul:
                body.append(label(lx + col_w - 12, yy, mul, cls="xs mono", anchor="end"))

    # ---- RIGHT: the measurement -----------------------------------------------
    rx, ry, pw = 520, 64, 400
    body.append(label(rx, 34, "Measured: 200 points of a settled yard, 8 sweeps, min of 400",
                      cls="sm", anchor="start"))
    peak = 14.0
    bars = [("hand-written", HAND_NS, HAND_BYTES, BLUE), ("Jacobian row", ROW_NS, ROW_BYTES, GREEN)]
    for i, (name, ns, by, col) in enumerate(bars):
        y = ry + 10 + i * 58
        w = ns / peak * (pw - 150)
        body.append(box(rx + 110, y, w, 26, col, opacity=0.55))
        body.append(label(rx, y + 18, name, cls="xs", anchor="start"))
        body.append(label(rx + 110 + w + 8, y + 18, f"{ns:.2f} ns", cls="xs mono", anchor="start"))
        body.append(label(rx + 110, y + 42, f"{by} bytes per point", cls="xs muted", anchor="start"))
    body.append(rule(rx + 110, ry, rx + 110, ry + 122, cls="grid", width=1.0))
    body.append(label(rx + 110, ry + 136, "0", cls="xs mono"))

    txt, th = para(rx, ry + 164,
                   f"The general form is {100 * (1 - ROW_NS / HAND_NS):.0f}% FASTER, which "
                   "refused the prediction that it would be slower. The saving is not "
                   "generality; it is a hoist. A row stores M⁻¹Jᵀ, so applying an "
                   "impulse is four scaled adds, where 8.9 recomputes it from the lever "
                   "arms with two cross products and two matrix products on every visit. "
                   "It costs 32 more bytes a point.", cols=76)
    body.extend(txt)
    H = max(64 + 2 * 118, ry + 164 + th) + 12
    return svg(uid, W, H,
               "The work in one normal solve, hand-written and as a Jacobian row",
               "Left: the multiplications in 8.9's normal solve (54) and in the row form (26), "
               "broken down by stage. Right: measured cost per point per sweep, 11.95 ns "
               "hand-written against 8.75 ns as a row, and the bytes each stores.", body)


# ===========================================================================
# FIGURE 3 — a rope lets go where a rod starts to push                     (§4)
# ===========================================================================

def fig3():
    uid = "f3"
    W = 980
    body = [cmarker(uid, "blue", BLUE)]

    cx, cy, R = 220, 190, 120
    body.append(label(26, 34, "Swung up with v₀² = 3.5 g L from the bottom", cls="sm",
                      anchor="start"))
    # the circle, light
    body.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="none" class="grid" stroke-width="1"/>')
    body.append(dot(cx, cy, GREY, 3.5))
    body.append(label(cx - 10, cy + 4, "pin", cls="xs muted", anchor="end"))
    # horizontal marks at y = L/2 and 3L/4
    # Short tags on the lines and the words in the legend: 8.11's first draft
    # wrote the sentences on the lines and they ran into the table.
    for frac, name, col in ((0.5, "L/2", AMBER), (0.75, "3L/4", PURPLE)):
        yy = cy - frac * R
        body.append(cline(cx - R - 10, yy, cx + R + 60, yy, col, width=1.0, dash="4 4"))
        body.append(label(cx + R + 64, yy + 4, name, cls="xs mono", anchor="start"))

    # the rod's path: bottom (alpha 0) up to alpha_max where -cos(a) = 0.75
    a_rod = math.acos(-0.75)
    rod = [(cx + R * math.sin(a), cy + R * math.cos(a))
           for a in [a_rod * k / 60 for k in range(61)]]
    body.append(poly(rod, PURPLE, width=1.6, close=False))
    # the rope's path: the same circle to alpha 120 deg, then a parabola
    a_rel = math.radians(120.0)
    rope = [(cx + R * math.sin(a), cy + R * math.cos(a))
            for a in [a_rel * k / 50 for k in range(51)]]
    body.append(poly(rope, AMBER, width=2.2, close=False))
    # parabola in metres: L = 1, g = 9.81, from (0.866, 0.5) with v = sqrt(0.5 g) (-0.5, 0.866)
    g = 9.81
    v = math.sqrt(0.5 * g)
    px, py = math.sin(a_rel), -math.cos(a_rel)
    vx, vy = v * math.cos(a_rel), v * math.sin(a_rel)
    para_pts = []
    t = 0.0
    while t < 1.0:
        x = px + vx * t
        y = py + vy * t - 0.5 * g * t * t
        para_pts.append((cx + R * x, cy - R * y))
        if t > 0.05 and x * x + y * y >= 1.0:
            break
        t += 0.01
    body.append(poly(para_pts, AMBER, width=2.2, dash="6 4", close=False))
    rel_x, rel_y = cx + R * px, cy - R * py
    body.append(dot(rel_x, rel_y, AMBER, 4.2))
    body.append(label(rel_x + 10, rel_y + 18, "the rope lets go", cls="xs", anchor="start"))
    body.append(label(rel_x - 60, cy - R * 1.02 - 20, "…and the bob flies", cls="xs muted",
                      anchor="middle"))
    body.append(label(cx + 14, cy + R + 22, "…and lands exactly here", cls="xs muted",
                      anchor="start"))

    # start
    body.append(dot(cx, cy + R, BLUE, 4.2))
    body.append(carrow(cx, cy + R, cx + 46, cy + R, uid, "blue", BLUE, width=1.6))
    body.append(label(cx + 50, cy + R + 4, "v₀", cls="xs", anchor="start"))

    body.extend(legend(26, cy + R + 44, [
        (AMBER, "rope: the circle, then a parabola — lets go at L/2, where tension reaches zero"),
        (PURPLE, "rod: carries on in compression, to its peak at 3L/4"),
    ]))

    # ---- RIGHT: the table ------------------------------------------------------
    rx = 560
    t, th = tbl(rx, 64, ["rate", "rope lets go", "rod turns", "rod peaks", "energy left"],
                [[f"{hz} Hz", f"{a:.4f} m", f"{b:.4f} m", f"{c:.4f} m", f"{e:.2f}%"]
                 for hz, a, b, c, e in SWING]
                + [["exact", "0.5000 m", "0.5000 m", "0.7500 m", "100.00%"]],
                [62, 90, 80, 80, 78], title="Measured, a 1 m joint")
    body.extend(t)
    txt, nh = para(rx, 64 + th + 24,
                   "The rope lets go EXACTLY where the rod's row changes sign from tension "
                   "to compression, at every rate — two instruments, one fact. Both read "
                   "low at 60 Hz because the bob arrives with 10% less energy than it "
                   "started with, and that is not the rope; it is §5.", cols=64)
    body.extend(txt)
    H = max(cy + R + 44 + 2 * 17 + 10, 64 + th + 24 + nh + 10)
    return svg(uid, W, H,
               "A bob on a rope and on a rod, swung up past the horizontal",
               "Left: a circle of radius L about a pin. The rope's path follows the circle to "
               "the height L/2 above the pin and then leaves on a parabola; the rod's continues "
               "to 3L/4. Right: the measured release height, turning height and peak at 60, "
               "240 and 960 Hz, with the energy left.", body)


# ===========================================================================
# FIGURE 4 — a tangent is not a circle                                     (§5)
# ===========================================================================

def fig4():
    uid = "f4"
    W = 980
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "red", RED), cmarker(uid, "amber", AMBER)]

    # ---- LEFT: the geometry, exaggerated ------------------------------------------
    body.append(label(26, 34, "One step, drawn with h twenty times too big", cls="sm",
                      anchor="start"))
    px, py, R = 70, 280, 220
    # arc of the circle
    arc = [(px + R * math.cos(a), py - R * math.sin(a)) for a in [math.radians(d) for d in range(0, 61, 2)]]
    body.append(poly(arc, GREY, width=1.2, close=False))
    body.append(dot(px, py, GREY, 3.5))
    body.append(label(px - 6, py + 16, "pin", cls="xs muted"))
    # the bob at angle 20 deg
    a0 = math.radians(20)
    bx, by = px + R * math.cos(a0), py - R * math.sin(a0)
    body.append(cline(px, py, bx, by, BLUE, width=1.6))
    body.append(label((px + bx) / 2 - 4, (py + by) / 2 + 16, "r", cls="xs mono"))
    body.append(dot(bx, by, BLUE, 4.0))
    # tangent step vh
    step = 110
    tx, ty = -math.sin(a0), -math.cos(a0)
    nx, ny = bx + tx * step, by + ty * step
    body.append(carrow(bx, by, nx, ny, uid, "blue", BLUE, width=1.8))
    body.append(label(bx + 8, by - step / 2 + 4, "v·h, along the tangent", cls="xs", anchor="start"))
    body.append(dot(nx, ny, RED, 4.0))
    body.append(cline(px, py, nx, ny, RED, width=1.2, dash="5 4"))
    body.append(label((px + nx) / 2 - 14, (py + ny) / 2 - 6, "√(r² + (v·h)²)", cls="xs mono",
                      anchor="end"))
    # radial overshoot
    ang = math.atan2(py - ny, nx - px)
    cxr, cyr = px + R * math.cos(ang), py - R * math.sin(ang)
    body.append(cline(cxr, cyr, nx, ny, RED, width=2.2))
    body.append(label(nx + 10, ny + 22, "drift δ ≈ v²h²/2r", cls="xs", anchor="start"))
    y_note = py + 42
    txt, nh = para(26, y_note,
                   "The velocity solve leaves the bob moving exactly along the tangent. "
                   "The position update follows the tangent for a whole step, and a "
                   "tangent is not the circle. Next step the solve removes the radial "
                   "part of v — which keeps r × v and so shrinks v as r grows.",
                   cols=78)
    body.extend(txt)

    # ---- RIGHT TOP: r against the recurrence ------------------------------------
    rx, ry, pw, ph = 560, 64, 360, 150
    body.append(label(rx, 34, "A rod spun at 5 m/s, no correction, 60 steps", cls="sm", anchor="start"))
    body.append(frame(rx, ry, pw, ph))
    r_lo, r_hi = 1.0, 1.2
    for v in (1.0, 1.05, 1.10, 1.15, 1.20):
        yy = ry + ph - (v - r_lo) / (r_hi - r_lo) * ph
        body.append(rule(rx, yy, rx + pw, yy, cls="grid", width=0.6))
        body.append(label(rx - 6, yy + 4, f"{v:.2f}", cls="xs mono", anchor="end"))
    # the recurrence, recomputed
    pts = []
    r, v = 1.0, 5.0
    h = 1.0 / 60.0
    pts.append((rx, ry + ph))
    for n in range(1, 61):
        nr = math.sqrt(r * r + v * v * h * h)
        v = v * r / nr
        r = nr
        pts.append((rx + n / 60 * pw, ry + ph - (r - r_lo) / (r_hi - r_lo) * ph))
    body.append(poly(pts, PURPLE, width=1.5, close=False))
    for n, rm in DRIFT:
        body.append(ring(rx + n / 60 * pw, ry + ph - (rm - r_lo) / (r_hi - r_lo) * ph, BLUE))
    for n in (0, 20, 40, 60):
        body.append(label(rx + n / 60 * pw, ry + ph + 16, str(n), cls="xs mono"))
    body.append(label(rx + pw / 2, ry + ph + 32, "step", cls="xs muted"))
    body.extend(legend(rx, ry + ph + 50, [
        (PURPLE, "the recurrence r² += (v h)², v r conserved"),
        (BLUE, f"measured — agrees to {RECURRENCE_ERR:.1e}"),
    ]))

    # ---- RIGHT BOTTOM: for a body ------------------------------------------------
    ty0 = ry + ph + 50 + 2 * 17 + 18
    t, th = tbl(rx, ty0, ["pin offset", "ρ", "lost per step", "ρ (ω h)²"],
                [[f"d = {d:.2f} W", f"{rho:.4f}", f"{m:.6f}", f"{p:.6f}"]
                 for _, d, rho, m, p in BODY_LOSS],
                [92, 72, 100, 90], title="And for a body on a pin: only the orbit is projected")
    body.extend(t)
    H = max(y_note + nh, ty0 + th) + 14
    return svg(uid, W, H,
               "Why a velocity constraint drifts: the step follows a tangent",
               "Left: a bob on a circle moves along the tangent for one step and lands outside "
               "the circle by the drift. Right: the measured radius of a rod spun at 5 m/s "
               "over 60 steps against the Pythagorean recurrence, and a table showing that a "
               "rigid body on a pin loses rho times (omega h) squared of its energy per step.",
               body)


# ===========================================================================
# FIGURE 5 — Baumgarte on a turning joint, and on a dislocated one          (§6)
# ===========================================================================

def fig5():
    uid = "f5"
    W = 980
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "green", GREEN), cmarker(uid, "red", RED)]

    body.append(label(26, 34, "Why Baumgarte keeps a turning joint's energy", cls="sm",
                      anchor="start"))
    px, py, R = 70, 250, 190
    arc = [(px + R * math.cos(a), py - R * math.sin(a)) for a in [math.radians(d) for d in range(0, 71, 2)]]
    body.append(poly(arc, GREY, width=1.2, close=False))
    body.append(dot(px, py, GREY, 3.5))
    a0, a1 = math.radians(15), math.radians(45)
    for a, tag in ((a0, "step n"), (a1, "step n+1")):
        bx, by = px + R * math.cos(a), py - R * math.sin(a)
        body.append(cline(px, py, bx, by, BLUE, width=1.0, dash="3 4"))
        body.append(dot(bx, by, BLUE, 3.8))
        body.append(label(bx + 10, by + 4, tag, cls="xs", anchor="start"))
    # at step n: the bias, inward
    bx, by = px + R * math.cos(a0), py - R * math.sin(a0)
    inx, iny = -math.cos(a0), math.sin(a0)
    body.append(carrow(bx, by, bx + inx * 56, by + iny * 56, uid, "red", RED, width=1.8))
    # at step n+1 the same vector, which now has a tangential part
    bx2, by2 = px + R * math.cos(a1), py - R * math.sin(a1)
    body.append(carrow(bx2, by2, bx2 + inx * 56, by2 + iny * 56, uid, "red", RED, width=1.8))
    tx, ty = -math.sin(a1), -math.cos(a1)
    comp = (inx * tx + iny * ty) * 56
    body.append(carrow(bx2, by2, bx2 + tx * comp, by2 + ty * comp, uid, "green", GREEN, width=2.0))
    body.extend(legend(26, py + 26, [
        (RED, "the bias: a REAL inward velocity δ/h, the same vector at both steps"),
        (GREEN, "the part of it along step n+1's tangent: (δ/h)·ωh"),
    ]))
    y_note = py + 26 + 2 * 17 + 12
    txt, nh = para(26, y_note,
                   "The joint turns by ωh between steps, so ωh of the inward bias velocity "
                   "now lies along the tangent: (δ/h)(ωh) = v(ωh)²/2, which is exactly the "
                   "speed the projection removes. Baumgarte adds energy — and on a turning "
                   "joint it adds it where the projection takes it away.",
                   cols=80)
    body.extend(txt)

    # ---- RIGHT ---------------------------------------------------------------------
    rx = 520
    t1, h1 = tbl(rx, 34, ["correction", "stretch", "δ/β", "energy lost / step"],
                 [[n, f"{s:.2f} mm", ("—" if p is None else f"{p:.2f} mm"), f"{e:.5f}"]
                  for n, s, p, e, _ in CORR],
                 [110, 86, 86, 118], title="A rod spun at 5 m/s, after 1 s")
    body.extend(t1)
    y2 = 34 + h1 + 26
    t2, h2 = tbl(rx, y2, ["correction", "gap after 3 s", "energy left in it"],
                 [[n, f"{g:.5f} m", f"{e:.6f} J"] for n, g, e in LIMB],
                 [110, 110, 180], title="A cube spawned 0.2 m out of a corner socket")
    body.extend(t2)
    txt2, nh2 = para(rx, y2 + h2 + 22,
                     "Pulled back at a corner, Baumgarte's real impulse spins the cube, and a "
                     "socket has no row that resists spin: the energy lands in the joint's "
                     "FREE degrees of freedom and stays. Through the centre it leaves nothing. "
                     "Split impulse leaves nothing either way.", cols=76)
    body.extend(txt2)
    H = max(y_note + nh, y2 + h2 + 22 + nh2) + 12
    return svg(uid, W, H,
               "Baumgarte on a turning joint and on a dislocated one",
               "Left: the inward bias velocity at one step, rotated by the joint's turn, has a "
               "component along the next step's tangent equal to the speed the projection "
               "removes. Right: the stretch and energy loss of a spinning rod under three "
               "corrections, and the energy left in a cube pulled back into a corner socket.",
               body)


# ===========================================================================
# FIGURE 6 — the block and the rows                                        (§7)
# ===========================================================================

def fig6():
    uid = "f6"
    W = 980
    body = []
    px0, py0, pw, ph = 70, 64, 400, 250
    body.append(label(26, 34, "One socket, residual |dC/dt| after each visit", cls="sm", anchor="start"))
    body.append(frame(px0, py0, pw, ph))
    lo, hi = 1e-8, 1.0
    for e in range(-8, 1):
        yy = logy(10.0 ** e, lo, hi, py0, ph)
        body.append(rule(px0, yy, px0 + pw, yy, cls="grid", width=0.6))
        body.append(label(px0 - 6, yy + 4, f"1e{e}" if e else "1", cls="xs mono", anchor="end"))

    def ix(n):
        return px0 + 18 + (n - 1) / 9 * (pw - 36)

    rows = [(ix(n), logy(r, lo, hi, py0, ph)) for n, r, _ in VISITS]
    # The prediction lies EXACTLY under the measurement, which is the finding
    # and also makes it invisible; a wide pale halo underneath shows it.
    pred0 = [(ix(n), logy(VISITS[0][1] * GS_PREDICTED ** (n - 1), lo, hi, py0, ph)) for n in range(1, 11)]
    body.append(f'<path d="M ' + " L ".join(f"{x:.2f},{y:.2f}" for x, y in pred0)
                + f'" fill="none" stroke="{PURPLE}" stroke-width="7" stroke-opacity="0.35"/>')
    body.append(poly(rows, BLUE, width=1.8, close=False))
    for x, y in rows:
        body.append(dot(x, y, BLUE))
    # block: 1.19e-7 then exact zero, drawn at the first visit only
    bx, byy = ix(1), logy(VISITS[0][2], lo, hi, py0, ph)
    body.append(dot(bx, byy, GREEN, 4.2))
    bx2 = ix(2)
    body.append(dot(bx2, logy(VISITS[1][2], lo, hi, py0, ph), GREEN, 4.2))
    for n in range(1, 11):
        body.append(label(ix(n), py0 + ph + 16, str(n), cls="xs mono"))
    body.append(label(px0 + pw / 2, py0 + ph + 32, "visit", cls="xs muted"))
    body.extend(legend(px0, py0 + ph + 50, [
        (BLUE, f"three rows, one at a time — {GS_MEASURED:.4f} per visit"),
        (PURPLE, f"K's Gauss–Seidel radius, {GS_PREDICTED:.4f} — the halo under it"),
        (GREEN, "the 3×3 block — 1.2e-07, then exactly zero"),
    ]))

    rx = 560
    t1, h1 = tbl(rx, 34, ["after ONE visit", "worst of 1,000"],
                 [["block", f"{BLOCK_WORST:.1e}"], ["rows", f"{ROWS_WORST:.3f}"],
                  ["rows, socket at the centre", f"{CENTRE_WORST:.1e}"]],
                 [200, 130], title="Relative residual, random sockets")
    body.extend(t1)
    y2 = 34 + h1 + 22
    t2, h2 = tbl(rx, y2, ["", "prepare", "one visit"],
                 [["rows", f"{COST_ROWS[0]:.1f} ns", f"{COST_ROWS[1]:.1f} ns"],
                  ["block", f"{COST_BLOCK[0]:.1f} ns", f"{COST_BLOCK[1]:.1f} ns"]],
                 [110, 110, 110], title="Cost per joint")
    body.extend(t2)
    y3 = y2 + h2 + 22
    t3, h3 = tbl(rx, y3, ["12-link chain", "worst gap", "mean gap"],
                 [["block", f"{CHAIN_BLOCK[0]:.1f} mm", f"{CHAIN_BLOCK[1]:.1f} mm"],
                  ["rows", f"{CHAIN_ROWS[0]:.1f} mm", f"{CHAIN_ROWS[1]:.1f} mm"]],
                 [110, 110, 110], title="And on a chain, 8 sweeps")
    body.extend(t3)
    H = max(py0 + ph + 50 + 3 * 17, y3 + h3) + 14
    return svg(uid, W, H,
               "A ball-socket solved as three rows and as one block",
               "Left: the residual after each visit on a log axis — the rows falling by 0.2323 "
               "per visit along K's Gauss-Seidel spectral radius, the block at 1.2e-07 after "
               "one visit. Right: worst residuals over a thousand random sockets, the cost of "
               "each form, and a twelve-link chain where the block barely helps.", body)


# ===========================================================================
# FIGURE 7 — the pendulum, and the parallel axis                           (§8)
# ===========================================================================

def fig7():
    uid = "f7"
    W = 980
    body = [cmarker(uid, "amber", AMBER)]

    # ---- LEFT: the plank on its pin ------------------------------------------------
    body.append(label(26, 34, "A 1 m plank on a pin at its top", cls="sm", anchor="start"))
    pinx, piny = 150, 70
    ang = math.radians(14)
    L = 200
    ex, ey = pinx + L * math.sin(ang), piny + L * math.cos(ang)
    ux, uy = math.sin(ang), math.cos(ang)
    nx, ny = uy, -ux
    wdt = 11
    corners = [(pinx + nx * wdt, piny + ny * wdt), (ex + nx * wdt, ey + ny * wdt),
               (ex - nx * wdt, ey - ny * wdt), (pinx - nx * wdt, piny - ny * wdt)]
    body.append(poly(corners, BLUE, width=1.5, close=True))
    body.append(dot(pinx, piny, GREY, 4.0))
    cmx, cmy = pinx + L / 2 * ux, piny + L / 2 * uy
    body.append(dot(cmx, cmy, BLUE, 4.0))
    body.append(cline(pinx, piny, cmx, cmy, AMBER, width=2.0))
    body.append(label(cmx + 16, (piny + cmy) / 2, "d = 0.5 m", cls="xs", anchor="start"))
    body.append(label(cmx + 16, cmy + 4, "centre", cls="xs muted", anchor="start"))
    body.append(cline(pinx, piny, pinx, piny + L + 20, GREY, width=0.8, dash="3 4"))
    ly0 = piny + L + 44
    lines = [("I_cm about the pin's axis = m(w² + h²)/12", f"= {I_CM:.6f} kg m²"),
             ("I_pivot = I_cm + m d²  (8.3's parallel axis)", f"= {I_PIVOT:.6f} kg m²"),
             ("T = 2π √(I_pivot / m g d)", f"= {T_BODY:.6f} s"),
             ("a point mass at d: T = 2π √(d / g)", f"= {T_POINT:.6f} s")]
    for i, (a, b) in enumerate(lines):
        body.append(label(26, ly0 + i * 17, a, cls="xs mono", anchor="start"))
        body.append(label(400, ly0 + i * 17, b, cls="xs mono", anchor="end"))
    body.append(label(26, ly0 + 4 * 17 + 8,
                      f"measured at 2°: {T_MEASURED:.6f} s against {T_TWO:.6f} predicted",
                      cls="xs", anchor="start"))

    # ---- RIGHT: period against amplitude --------------------------------------------
    rx, ry, pw, ph = 510, 64, 400, 220
    body.append(label(rx, 34, "Period against amplitude, over the small-swing period",
                      cls="sm", anchor="start"))
    body.append(frame(rx, ry, pw, ph))
    y_lo, y_hi = 0.80, 1.40
    for v in (0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4):
        yy = ry + ph - (v - y_lo) / (y_hi - y_lo) * ph
        body.append(rule(rx, yy, rx + pw, yy, cls="grid", width=0.6))
        body.append(label(rx - 6, yy + 4, f"{v:.1f}", cls="xs mono", anchor="end"))
    for a in (0, 30, 60, 90, 120, 150):
        xx = rx + a / 150 * pw
        body.append(label(xx, ry + ph + 16, f"{a}°", cls="xs mono"))
    body.append(label(rx + pw / 2, ry + ph + 32, "amplitude", cls="xs muted"))

    def agm(a, g):
        for _ in range(40):
            a, g = 0.5 * (a + g), math.sqrt(a * g)
        return a

    # Stop the curve where it leaves the frame: past about 125 degrees the
    # period climbs out of the axis, and a curve drawn over the title is a
    # curve drawn over the title.
    curve = []
    for k in range(0, 146, 1):
        th = math.radians(k)
        ratio = 1.0 / agm(1.0, math.cos(th / 2))
        if ratio > y_hi:
            break
        curve.append((rx + k / 150 * pw, ry + ph - (ratio - y_lo) / (y_hi - y_lo) * ph))
    body.append(poly(curve, PURPLE, width=1.6, close=False))
    for a, pred, meas, _, _ in AMP:
        body.append(ring(rx + a / 150 * pw, ry + ph - (meas / T_BODY - y_lo) / (y_hi - y_lo) * ph, BLUE, r=4.2))
    pt = T_POINT / T_BODY
    ypt = ry + ph - (pt - y_lo) / (y_hi - y_lo) * ph
    body.append(cline(rx, ypt, rx + pw, ypt, RED, width=1.3, dash="5 4"))
    body.extend(legend(rx, ry + ph + 50, [
        (PURPLE, "T₀ / AGM(1, cos(a/2)) — the elliptic integral, in disguise"),
        (BLUE, "measured at 3840 Hz, first period"),
        (RED, f"a point mass at the same d — {100 * (1 - pt):.1f}% short at every amplitude"),
    ]))
    t, th = tbl(rx, ry + ph + 50 + 3 * 17 + 16, ["rate", "error at 2°", "÷ next"],
                [[f"{hz} Hz", f"{e:+.3e}", (f"{e / H2[i + 1][1]:.2f}" if i < 3 else "")]
                 for i, (hz, e) in enumerate(H2)],
                [80, 110, 70], title="Second order in h")
    body.extend(t)
    H = max(ly0 + 4 * 17 + 20, ry + ph + 50 + 3 * 17 + 16 + th) + 12
    return svg(uid, W, H,
               "A physical pendulum: the parallel axis sets its period",
               "Left: a plank on a pin at its top, with its centre half a metre below, and the "
               "arithmetic from I_cm through the parallel-axis theorem to the period. Right: "
               "the period against amplitude from 0 to 150 degrees on the AGM formula, the "
               "measured points at 3840 Hz, the point-mass period as a dashed line, and a "
               "table showing the period error falling by four with each halving of the step.",
               body)


# ===========================================================================
# FIGURE 8 — the hinge's two rows, and the frame they are cached in          (§9)
# ===========================================================================

def fig8():
    uid = "f8"
    W = 980
    body = [cmarker(uid, "blue", BLUE), cmarker(uid, "amber", AMBER), cmarker(uid, "red", RED)]
    body.append(label(26, 34, "Two axes that must stay one", cls="sm", anchor="start"))
    ox, oy = 180, 220
    L = 150
    # a1 straight up, b1 tilted slightly
    body.append(carrow(ox, oy, ox, oy - L, uid, "blue", BLUE, width=2.0))
    body.append(label(ox - 8, oy - L - 6, "a₁ — the axis as body a carries it", cls="xs", anchor="end"))
    tilt = math.radians(12)
    bx, by = ox + L * math.sin(tilt), oy - L * math.cos(tilt)
    body.append(carrow(ox, oy, bx, by, uid, "red", RED, width=2.0))
    body.append(label(bx + 8, by + 2, "b₁ — as body b carries it", cls="xs", anchor="start"))
    # perpendiculars p0 (right) and p1 (into page, drawn as a small circle-dot)
    body.append(carrow(ox, oy, ox + 110, oy, uid, "amber", AMBER, width=1.6))
    body.append(label(ox + 114, oy + 4, "p₀", cls="xs mono", anchor="start"))
    body.append(ring(ox, oy, AMBER, r=7))
    body.append(dot(ox, oy, AMBER, 2.2))
    body.append(label(ox - 12, oy + 20, "p₁ (out of the page)", cls="xs mono", anchor="end"))
    yl = oy + 44
    notes = [
        "C_i = (a₁ × b₁) · p_i — two angles the hinge must not allow.",
        "J_i = [ 0, −p_i, 0, p_i ]: the relative spin may lie along a₁ and nowhere else.",
        "p₀ and p₁ are chosen from the axis IN BODY a's FRAME, where it never moves,",
        "and the cached impulse is stored as a WORLD vector — 8.10 §4's bug, avoided twice.",
    ]
    for i, t in enumerate(notes):
        body.append(label(26, yl + i * 16, t, cls="xs", anchor="start"))

    rx = 540
    t1, h1 = tbl(rx, 34, ["a 10 kg door, 5 s", "anchor gap", "misalignment", "tilt"],
                 [[n, f"{g:.3f} mm", (f"{m:.4f}°" if n == "hinge" else "—"), f"{tlt:.2f}°"]
                  for n, g, m, tlt in DOOR],
                 [130, 84, 92, 70], title="A door on its hinge, and on a socket alone")
    body.extend(t1)
    y2 = 34 + h1 + 24
    t2, h2 = tbl(rx, y2, ["basis chosen from", "turned > 30°", "worst"],
                 [["the world axis (8.10's way)", f"{FLIPS_WORLD} of {FLIP_FRAMES:,}", f"{FLIP_WORST_WORLD:.1f}°"],
                  ["body a's own axis (8.11)", f"{FLIPS_BODY} of {FLIP_FRAMES:,}", f"{FLIP_WORST_BODY:.1f}°"]],
                 [180, 110, 76], title="A twelve-plank bridge under a bouncing ball, 10 s")
    body.extend(t2)
    txt, nh = para(rx, y2 + h2 + 20,
                   "A hinge axis along z, on a plank moving in the x–y plane, has x and y "
                   "components that are rounding noise either side of zero, so a basis "
                   "chosen from it in world space flips by exactly 90°. Chosen where the "
                   "axis is a constant, it turns only as the plank does.", cols=74)
    body.extend(txt)
    H = max(yl + 4 * 16, y2 + h2 + 20 + nh) + 12
    return svg(uid, W, H,
               "The hinge's alignment rows, and the frame their impulse is cached in",
               "Left: the hinge axis as each body carries it, with two perpendiculars p0 and "
               "p1; the misalignment a1 cross b1 resolved along them is the rows' error. "
               "Right: a door's anchor gap, misalignment and tilt on a hinge and on a "
               "ball-socket alone, and a count of basis flips on a sagging bridge when the "
               "basis is chosen from the world axis rather than from body a's own.", body)


# ===========================================================================
# FIGURE 9 — limits: speculation, and the rate the parallel axis sets       (§10)
# ===========================================================================

def fig9():
    uid = "f9"
    W = 980
    body = [cmarker(uid, "amber", AMBER), cmarker(uid, "blue", BLUE)]

    # ---- LEFT: why the limit converges slowly on a door -----------------------------
    body.append(label(26, 34, "The limit row turns the door about its CENTRE", cls="sm",
                      anchor="start"))
    pinx, piny = 70, 150
    L = 260
    body.append(hollow(pinx, piny - 12, L, 24, BLUE, width=1.4))
    body.append(dot(pinx, piny, GREY, 4.2))
    body.append(label(pinx - 4, piny + 30, "pin", cls="xs muted"))
    cmx = pinx + L / 2
    body.append(dot(cmx, piny, BLUE, 4.0))
    body.append(label(cmx, piny + 30, "centre", cls="xs muted"))
    # limit's rotation about the centre: arc arrow
    arc = [(cmx + 44 * math.cos(a), piny - 44 * math.sin(a)) for a in [math.radians(d) for d in range(30, 151, 6)]]
    body.append(poly(arc, AMBER, width=1.6, close=False))
    body.append(label(cmx, piny - 54, "what the limit row sees: I_cm", cls="xs", anchor="middle"))
    # the pin's reaction
    body.append(carrow(pinx, piny - 60, pinx, piny - 18, uid, "blue", BLUE, width=1.6))
    body.append(label(pinx + 8, piny - 64, "the pin pulls back", cls="xs", anchor="start"))
    yl = piny + 58
    notes = [
        "The door really turns about the pin, where its inertia is I_cm + m d².",
        "The limit changes ω by the right amount for I_cm; the pin then gives",
        "back a fraction   ρ = m d² / (I_cm + m d²)   of it. Gauss–Seidel between",
        "the two contracts by ρ per sweep: 3/4 for ANY uniform slab hinged at its",
        "edge, whatever its size — the parallel axis again, as a rate.",
    ]
    for i, t in enumerate(notes):
        body.append(label(26, yl + i * 16, t, cls="xs", anchor="start"))
    t0, h0 = tbl(26, yl + 5 * 16 + 18, ["turnstile, 200 phases", "mean", "largest"],
                 [[n, f"{m:.4f}", f"{l:.4f}"] for n, m, l, _ in TURNSTILE],
                 [180, 90, 90], title="Overshoot past the stop, in units of ω h")
    body.extend(t0)

    # ---- RIGHT: measured contraction per sweep ----------------------------------------
    rx, ry, pw, ph = 560, 64, 360, 200
    body.append(label(rx, 34, "Error left after n sweeps, one step into the stop", cls="sm",
                      anchor="start"))
    body.append(frame(rx, ry, pw, ph))
    lo, hi = 1e-3, 1.0
    for e in (-3, -2, -1, 0):
        yy = logy(10.0 ** e, lo, hi, ry, ph)
        body.append(rule(rx, yy, rx + pw, yy, cls="grid", width=0.6))
        body.append(label(rx - 6, yy + 4, f"1e{e}" if e else "1", cls="xs mono", anchor="end"))
    ns = [1, 2, 4, 8]

    def ix(n):
        return logspan(n, 1, 8, rx + 20, pw - 40)

    colours = {0.25: GREEN, 0.50: AMBER, 1.00: RED}
    for frac, rho, errs in RHO:
        if frac == 0.0:
            continue
        col = colours[frac]
        line = [(ix(n), logy(rho ** n, lo, hi, ry, ph)) for n in (1, 2, 3, 4, 5, 6, 7, 8)]
        body.append(poly(line, PURPLE, width=1.0, dash="4 4", close=False))
        for n, e in zip(ns, errs):
            body.append(dot(ix(n), logy(e, lo, hi, ry, ph), col, 3.6))
    for n in ns:
        body.append(label(ix(n), ry + ph + 16, str(n), cls="xs mono"))
    body.append(label(rx + pw / 2, ry + ph + 32, "sweeps", cls="xs muted"))
    body.extend(legend(rx, ry + ph + 50, [
        (RED, "pin a full width out:  ρ = 0.9224"),
        (AMBER, "at the edge (a door):  ρ = 0.7481"),
        (GREEN, "a quarter of the way out:  ρ = 0.4261"),
        (PURPLE, "ρⁿ, predicted — through the centre ρ = 0, exact in one"),
    ]))
    t, th = tbl(rx, ry + ph + 50 + 4 * 17 + 14, ["pin", "ρ⁸", "rebound at 8 sweeps"],
                [[f"d = {f:.2f} W", f"{r8:.5f}", f"{rb * 100:.2f}% of arrival"]
                 for f, r8, _, rb in REBOUND],
                [96, 90, 160], title="Restitution zero, and still it bounces")
    body.extend(t)
    H = max(yl + 5 * 16 + 18 + h0, ry + ph + 50 + 4 * 17 + 14 + th) + 12
    return svg(uid, W, H,
               "A hinge limit: speculation, and the convergence rate the parallel axis sets",
               "Left: a door on a pin; the limit row rotates it about its centre and the pin "
               "pulls back, so Gauss-Seidel between them contracts by rho = m d squared over "
               "I_pivot per sweep; and a table of a turnstile's overshoot, uniform when "
               "reactive and zero when speculative. Right: the error left after 1, 2, 4 and "
               "8 sweeps for three pin offsets, lying on rho to the n, and the rebound each "
               "leaves at eight sweeps.", body)


# ===========================================================================
# FIGURE 10 — what joints leave behind                                      (§14)
# ===========================================================================

def fig10():
    uid = "f10"
    W = 980
    body = []
    px0, py0, pw, ph = 70, 64, 380, 230
    body.append(label(26, 34, "Ten links and an end weight: total stretch", cls="sm", anchor="start"))
    body.append(frame(px0, py0, pw, ph))
    lo, hi = 1e-3, 1e3
    for e in range(-3, 4):
        yy = logy(10.0 ** e, lo, hi, py0, ph)
        body.append(rule(px0, yy, px0 + pw, yy, cls="grid", width=0.6))
        body.append(label(px0 - 6, yy + 4, f"{10.0 ** e:g} mm", cls="xs mono", anchor="end"))

    def ix(n):
        return logspan(n, 8, 128, px0 + 20, pw - 40)

    cols = {1: GREEN, 10: AMBER, 100: RED}
    for ratio, vals in STRETCH.items():
        pts = [(ix(n), logy(v, lo, hi, py0, ph)) for n, v in zip(SWEEPS, vals)]
        body.append(poly(pts, cols[ratio], width=1.8, close=False))
        for x, y in pts:
            body.append(dot(x, y, cols[ratio]))
    for n in SWEEPS:
        body.append(label(ix(n), py0 + ph + 16, str(n), cls="xs mono"))
    body.append(label(px0 + pw / 2, py0 + ph + 32, "sweeps per step", cls="xs muted"))
    body.extend(legend(px0, py0 + ph + 50, [
        (RED, "end weight 100 kg"), (AMBER, "end weight 10 kg"), (GREEN, "end weight 1 kg"),
    ]))

    # ---- RIGHT TOP: the same work, spent differently ----------------------------------
    rx = 540
    body.append(label(rx, 34, "The same visits per frame, 100 kg end weight", cls="sm", anchor="start"))
    peak = 400.0
    for i, (name, mm) in enumerate(SUBSTEP):
        y = 56 + i * 34
        w = mm / peak * 250
        body.append(box(rx + 140, y, max(w, 1.0), 20, RED if i == 0 else GREEN, opacity=0.55))
        body.append(label(rx, y + 14, name, cls="xs mono", anchor="start"))
        body.append(label(rx + 140 + max(w, 1.0) + 8, y + 14, f"{mm:.1f} mm", cls="xs mono", anchor="start"))
    body.append(rule(rx + 140, 50, rx + 140, 56 + 4 * 34, cls="grid", width=1.0))
    y2 = 56 + 4 * 34 + 20
    t, th = tbl(rx, y2, ["joint", "prepare", "per sweep"],
                [[n, f"{p:.1f} ns", f"{s:.1f} ns"] for n, p, s in BUDGET],
                [180, 90, 90], title="Per joint, in the engine's loop, 1,000 joints")
    body.extend(t)
    txt, nh = para(rx, y2 + th + 20,
                   f"Against {MANIFOLD_SWEEP_NS} ns per contact manifold per sweep (8.10 §13): "
                   "a hinge with a limit and a motor costs less than half of one crate "
                   "resting on another.", cols=72)
    body.extend(txt)
    H = max(py0 + ph + 50 + 3 * 17, y2 + th + 20 + nh) + 12
    return svg(uid, W, H,
               "What joints leave behind: mass ratios, sub-stepping and the budget",
               "Left: the total stretch of a ten-link chain against sweeps per step for end "
               "weights of 1, 10 and 100 kg on log axes. Right: the same number of joint "
               "visits spent as one step of eight sweeps or eight steps of one, and the cost "
               "per joint for each kind.", body)


def main():
    figs = [fig1, fig2, fig3, fig4, fig5, fig6, fig7, fig8, fig9, fig10]
    for i, f in enumerate(figs, start=1):
        write(f"l811_fig{i}.svg", f())


if __name__ == "__main__":
    main()
