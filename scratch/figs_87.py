#!/usr/bin/env python3
"""scratch/figs_87.py — Lesson 8.7's diagrams.

Same rules as 5.1-8.6's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - no hardcoded colour on an `.ink` stroke either — 8.6 shipped a near-white
    origin cross that was invisible in the light theme, and only a rendered
    frame showed it
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed

Every number below is transcribed from scratch/_verify87.txt. Nothing here is
estimated, and the harness section each block came from is named above it.

*** WRITTEN IN LITERAL UNICODE THROUGHOUT, NEVER \\uXXXX. ***

THE COLOUR RULE, inherited from 8.4 through 8.6 so a reader moving between the
four relearns nothing:
  GREEN  = the answer that works / a contact that survives
  RED    = the failure, and shape B
  AMBER  = what the algorithm produced — here, the clipped polygon
  BLUE   = shape A, and the REFERENCE face, which is usually A's
  PURPLE = the contact normal
  GREY   = a candidate that found nothing, or geometry being discarded

FIGURE 8 RUNS A REAL SUTHERLAND-HODGMAN. `clip2` below is `manifold.cpp`'s inner
loop reduced to the plane, so the polygons in each of its four panels were CUT
rather than drawn. If the C++ changes and this stops agreeing, the figure is
wrong and should be regenerated rather than patched.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_71 import cmarker, carrow, poly, frame                    # noqa: E402
from figs_76 import table, f, fmt_e                                 # noqa: E402
from figs_81 import legend, logspan                                 # noqa: E402

OUT = "scratch"


# ---------------------------------------------------------------------------
# Text that fits
# ---------------------------------------------------------------------------
#
# *** 8.6's FIGURES WERE CHECKED FOR SPILLS BY A BROWSER AND THIS ONE IS TOO,
# BUT THE CHEAPEST FIX IS NOT TO SPILL. *** A label is a single <text> element
# and SVG does not wrap it; a line one character too long simply leaves the
# viewBox, and at 900 px wide with the page's `xs` face that happens at about
# 128 characters. `para` measures rather than trusting: it greedily fills lines
# to `cols` characters and emits one label per line at a fixed leading, so a
# paragraph's height is COMPUTED and the caller can put the next thing under it.

#: Characters that fit on one `xs` line spanning the full 900 px figure width.
COLS = 118


def para(x, y, text, cols=COLS, cls="xs muted", leading=17, anchor="start"):
    """Wrap `text` to `cols` characters and emit one label per line.

    Returns (body, height). The height is what it took, so the next block goes
    at `y + height` and nothing has to be guessed.
    """
    words = text.split()
    lines = []
    cur = ""
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


def table_body(*args, **kwargs):
    """`figs_76.table` returns (body, height); every table here wants the body."""
    body, _height = table(*args, **kwargs)
    return body


# ===========================================================================
# A tiny 2D kit, so figures are COMPUTED
# ===========================================================================

def sub(a, b):
    return (a[0] - b[0], a[1] - b[1])


def add(a, b):
    return (a[0] + b[0], a[1] + b[1])


def mul(a, s):
    return (a[0] * s, a[1] * s)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1]


def cross(a, b):
    """The scalar cross product: the z of the 3D one."""
    return a[0] * b[1] - a[1] * b[0]


def length(a):
    return math.hypot(a[0], a[1])


def norm(a):
    n = length(a)
    return (a[0] / n, a[1] / n) if n > 1e-12 else (0.0, 0.0)


def perp(a):
    """Rotate 90° counter-clockwise."""
    return (-a[1], a[0])


def poly_area(pts):
    """Twice the signed area, halved. 2.3's signed area, as an instrument."""
    if len(pts) < 3:
        return 0.0
    s = 0.0
    for i, p in enumerate(pts):
        q = pts[(i + 1) % len(pts)]
        s += cross(p, q)
    return abs(s) * 0.5


def rot(pts, theta, about=(0.0, 0.0)):
    c, s = math.cos(theta), math.sin(theta)
    out = []
    for p in pts:
        d = sub(p, about)
        out.append(add(about, (d[0] * c - d[1] * s, d[0] * s + d[1] * c)))
    return out


def clip2(ref, inc, planes):
    """`manifold.cpp`'s Sutherland-Hodgman, in the plane, stopped after `planes`.

    The reference polygon is wound counter-clockwise, so the outward in-plane
    normal of edge (v[i], v[i+1]) is the edge turned CLOCKWISE — which in 2D is
    `-perp(edge)`. In 3D the same statement is `cross(edge, face_normal)`.

    Returns (surviving polygon, list of per-plane (a0, a1, outward normal)).
    """
    cur = list(inc)
    sides = []
    n = len(ref)
    for e in range(n):
        a0, a1 = ref[e], ref[(e + 1) % n]
        side = mul(perp(sub(a1, a0)), -1.0)
        sides.append((a0, a1, norm(side)))
    for e in range(min(planes, n)):
        a0, _a1, side = sides[e]
        offset = dot(side, a0)
        out = []
        m = len(cur)
        if m == 0:
            break
        for i in range(m):
            c, nx = cur[i], cur[(i + 1) % m]
            dc = dot(side, c) - offset
            dn = dot(side, nx) - offset
            if dc <= 0.0:
                out.append(c)
            if (dc < 0.0) != (dn < 0.0):
                t = dc / (dc - dn)
                out.append(add(c, mul(sub(nx, c), t)))
        cur = out
    return cur, sides


def bars(x, y, w, rows, maxv, row_h=22, label_w=150, fmt=lambda v: f"{v:.2f}",
         colour=AMBER):
    """A horizontal bar chart. Values, labels and a colour per row."""
    b = []
    for i, (name, v, c) in enumerate(rows):
        yy = y + i * row_h
        b.append(label(x, yy + 4, name, "xs", "start"))
        span = w - label_w - 90
        ww = max(2.0, span * (v / maxv if maxv > 0 else 0.0))
        b.append(box(x + label_w, yy - 8, ww, 13, c if c else colour, rx=2,
                     opacity=0.85))
        b.append(label(x + label_w + ww + 8, yy + 3, fmt(v), "xs mono", "start"))
    return b


# ===========================================================================
# 1. One point is a pin
# ===========================================================================
#
# §A.1: a 10 kg crate, 2° of tilt, dropped 2 cm, same solver both arms.
#   one point : tilt 0.330°, rocking 27.989 °/s, sink 21.27 mm, area 0
#   3.33 pts  : tilt 0.041°, rocking  5.234 °/s, sink  1.16 mm, area 0.6667 m²

def fig1():
    uid = "l87f1"
    W, H = 900, 520
    b = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED),
         cmarker(uid, "purple", PURPLE)]

    b.append(label(20, 22, "A contact force through one point exerts no torque "
                           "about itself.", "sm", "start"))

    scale = 96.0
    half = 0.5

    def panel(cx, cy, tilt, pts, title, sub_):
        out = []
        crate = rot([(-half, -half), (half, -half), (half, half), (-half, half)],
                    tilt)
        crate = [add(p, (0.0, half * 1.02)) for p in crate]
        P = lambda p: (cx + p[0] * scale, cy - p[1] * scale)
        out.append(label(cx, cy - 190, title, "xs", "middle"))
        # the floor
        out.append(cline(cx - 165, cy, cx + 165, cy, GREY, width=2.0))
        out.append(poly([P(p) for p in crate], RED, width=1.7))
        # centre of mass
        com = P((0.0, half * 1.02))
        out.append(f'<circle cx="{com[0]:.1f}" cy="{com[1]:.1f}" r="3.4" '
                   f'fill="{GREY}"/>')
        out.append(label(com[0] + 9, com[1] - 5, "com", "xs muted", "start"))
        # gravity
        out.append(carrow(com[0], com[1], com[0], cy - 4, uid, "purple", PURPLE,
                          width=1.6, dash="4 3"))
        # the contact points
        for px in pts:
            q = P((px, 0.0))
            out.append(f'<circle cx="{q[0]:.1f}" cy="{q[1]:.1f}" r="4.6" '
                       f'fill="{GREEN if len(pts) > 1 else RED}"/>')
            out.append(carrow(q[0], q[1] + 2, q[0], q[1] - 34, uid,
                              "green" if len(pts) > 1 else "red",
                              GREEN if len(pts) > 1 else RED, width=1.8))
        # the support polygon, as a bar under the floor
        if len(pts) > 1:
            out.append(cline(P((min(pts), 0.0))[0], cy + 16,
                             P((max(pts), 0.0))[0], cy + 16, GREEN, width=4.0))
            out.append(label(cx, cy + 34, "support polygon", "xs", "middle"))
        else:
            out.append(f'<circle cx="{P((pts[0], 0.0))[0]:.1f}" cy="{cy + 16:.1f}" '
                       f'r="3" fill="{RED}"/>')
            out.append(label(cx, cy + 34, "support polygon: a point", "xs", "middle"))
        for i, t in enumerate(sub_):
            out.append(label(cx, cy + 56 + i * 15, t, "xs muted", "middle"))
        return out

    b += panel(210, 232, math.radians(4.0), [-0.52],
               "one contact point",
               ["the resultant must pass through it, so the",
                "weight of the crate makes a couple about it",
                "and the crate turns — every frame, for ever"])
    b += panel(660, 232, math.radians(0.6), [-0.5, -0.17, 0.17, 0.5],
               "four contact points",
               ["four independent non-negative impulses, so the",
                "resultant can act ANYWHERE in their hull, and",
                "it does: the crate rests"])

    b.append(rule(20, 366, W - 20, 366, "grid"))

    rows = [
        ["1 point", "0.330°", "27.989 °/s", "21.27 mm", "0.0000 m²"],
        ["3.33 points (mean)", "0.041°", "5.234 °/s", "1.16 mm", "0.6667 m²"],
    ]
    b += table_body(24, 382, ["contacts", "tilt, 2 s", "residual rocking", "sink",
                         "support area"],
               rows, [180, 110, 160, 110, 150],
               title="§1 — the same crate, the same solver, the same normal")

    _p, _h = para(24, 476,
                  "A single point does not TIP a crate that is already flat. It "
                  "ROCKS it, pushing whichever corner is lowest and turning the "
                  "other one down — and while it rocks, it sinks. Only one corner "
                  "of four is ever being pushed out.")
    b += _p

    return svg(uid, W, H,
               "One contact point cannot hold a crate up; four can.",
               "Two panels. Left: a crate tilted on a floor with a single contact "
               "point at one corner, its centre of mass marked and a dashed "
               "gravity arrow through it, and a red arrow at the contact showing "
               "the only force available. The support polygon below is a single "
               "point. Right: the same crate nearly level with four contact "
               "points and a green bar underneath spanning them, labelled support "
               "polygon. Below, a table of measurements: one point leaves 0.330 "
               "degrees of tilt, 27.989 degrees per second of residual rocking "
               "and 21.27 millimetres of sink with zero support area; a "
               "four-point manifold leaves 0.041 degrees, 5.234 degrees per "
               "second, 1.16 millimetres and 0.6667 square metres.",
               b)


# ===========================================================================
# 2. The support polygon, and the moment it can carry
# ===========================================================================
#
# §A.2: predicted m·g·(w/2) = 49.050 N m, bisected 47.869, −2.41%.
# §A.3: overhang 0.00/0.20/0.40/0.45 rest (0.071/0.104/0.026/0.127°),
#       0.55/0.70 tip (156.650/107.957°).

def fig2():
    uid = "l87f2"
    W, H = 900, 520
    b = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED),
         cmarker(uid, "purple", PURPLE)]

    b.append(label(20, 22, "A body rests exactly when its centre of mass projects "
                           "INSIDE the support polygon.", "sm", "start"))

    scale = 118.0
    rows = [(0.00, True, 0.071), (0.40, True, 0.026), (0.55, False, 156.650)]
    for i, (over, rests, tilt) in enumerate(rows):
        cx = 170 + i * 290
        cy = 210
        P = lambda p: (cx + p[0] * scale, cy - p[1] * scale)
        # the ledge occupies x in [-1, 0]
        b.append(poly([P((-1.0, -0.55)), P((0.0, -0.55)), P((0.0, 0.0)),
                       P((-1.0, 0.0))], GREY, width=1.6))
        cxx = -0.5 + over
        crate = [(cxx - 0.5, 0.0), (cxx + 0.5, 0.0), (cxx + 0.5, 1.0),
                 (cxx - 0.5, 1.0)]
        b.append(poly([P(p) for p in crate], RED if not rests else BLUE, width=1.7))
        com = P((cxx, 0.5))
        b.append(f'<circle cx="{com[0]:.1f}" cy="{com[1]:.1f}" r="3.6" '
                 f'fill="{GREY}"/>')
        b.append(carrow(com[0], com[1], com[0], P((cxx, -0.22))[1], uid, "purple",
                        PURPLE, width=1.5, dash="4 3"))
        # the support polygon: the contact set's x range
        lo, hi = max(cxx - 0.5, -1.0), min(cxx + 0.5, 0.0)
        if hi > lo:
            b.append(cline(P((lo, 0.0))[0], cy + 9, P((hi, 0.0))[0], cy + 9,
                           GREEN if rests else RED, width=4.5))
        b.append(label(cx, 62, f"overhang {over:.2f} m", "xs", "middle"))
        inside = "inside" if rests else "OUTSIDE"
        # BELOW the ledge, not on it. The ledge is drawn as a filled-looking
        # polygon 0.55 m deep, so a label at cy + 36 lands inside it — which
        # check-page.js reports as text-on-shape and a reader sees as a caption
        # written across a solid.
        b.append(label(cx, cy + 0.55 * scale + 26, f"com {inside} the polygon", "xs",
                       "middle"))
        b.append(label(cx, cy + 0.55 * scale + 44, f"tilt after 2 s: {tilt:.3f}°",
                       "xs mono " + ("" if rests else "t-hi"), "middle"))

    b.append(rule(20, 314, W - 20, 314, "grid"))

    b.append(label(24, 340, "§1 also asks how much MOMENT a support polygon can "
                            "carry, and the answer is derivable before it is "
                            "measured.", "xs", "start"))
    b.append(label(24, 366, "A polygon w = 1 m wide under a weight of "
                            "m·g = 98.1 N can resist at most m·g·(w/2): the "
                            "weight acting at the very lip.", "xs muted", "start"))

    b += table_body(24, 388, ["", "N·m"],
               [["predicted  m·g·(w/2)", "49.050"],
                ["measured, bisected over 14 steps", "47.869"],
                ["difference", "−2.41%"]],
               [330, 110])

    _p, _h = para(470, 406,
                  "A prediction that survives three digits is a different kind of "
                  "statement from one that survives a bracket. The 2.41% is the "
                  "penetration the solver allows: the crate's real lip is a hair "
                  "inside its geometric one.", cols=66)
    b += _p
    b.append(label(470, 406 + _h + 16,
                   "The manifold was never told where the ledge was.", "xs t-hi",
                   "start"))

    return svg(uid, W, H,
               "A body rests exactly when its centre of mass projects inside the "
               "support polygon the manifold spans.",
               "Three panels showing a one-metre crate on a ledge at overhangs of "
               "0, 0.40 and 0.55 metres. In each, a dashed vertical line drops "
               "from the crate's centre of mass, and a coloured bar under the "
               "contact marks the support polygon. At 0 and 0.40 metres the line "
               "falls inside the bar and the crate rests, tilting 0.071 and 0.026 "
               "degrees after two seconds. At 0.55 metres it falls outside and the "
               "crate topples, 156.650 degrees. Below, a table: the moment such a "
               "polygon can carry is predicted at 49.050 newton metres and "
               "measured by bisection at 47.869, 2.41 percent low.",
               b)


# ===========================================================================
# 3. The normal is discontinuous and the depth is not
# ===========================================================================
#
# §B.1: a 0.4 m cube pressed 50 mm into a wall ending at z = 1, slid in 10 µm
# steps. Worst swing between adjacent steps 90.00°, worst depth jump 0.01001 mm.

def fig3():
    uid = "l87f3"
    W, H = 900, 488
    b = [cmarker(uid, "purple", PURPLE), cmarker(uid, "green", GREEN),
         cmarker(uid, "red", RED)]

    b.append(label(20, 22, "The depth is continuous. The direction is not — and "
                           "that is a property of a minimum.", "sm", "start"))

    scale = 128.0
    for i, (z, note) in enumerate([(1.10, "out through the face, 50 mm"),
                                   (1.15, "the two are equal"),
                                   (1.20, "out past the end, 40 mm")]):
        cx = 170 + i * 290
        cy = 176
        P = lambda p: (cx + (p[0] - 0.9) * scale, cy - (p[1] - 0.2) * scale)
        # the wall: x ≤ 0, z ≤ 1. Drawn in the x-z plane, x up, z right.
        b.append(poly([P((0.55, -0.55)), P((1.0, -0.55)), P((1.0, 0.0)),
                       P((0.55, 0.0))], GREY, width=1.6))
        b.append(label(P((0.775, -0.3))[0], P((0.775, -0.3))[1] + 4, "wall", "xs muted",
                       "middle"))
        crate = [(z - 0.2, -0.05), (z + 0.2, -0.05), (z + 0.2, 0.35),
                 (z - 0.2, 0.35)]
        b.append(poly([P(p) for p in crate], RED, width=1.7))
        face_cost = 0.05
        end_cost = 1.0 - (z - 0.2)
        c = P((z, 0.15))
        # candidate escapes
        b.append(carrow(c[0], c[1], c[0], c[1] - face_cost * scale * 4.0, uid,
                        "green" if face_cost <= end_cost else "purple",
                        GREEN if face_cost <= end_cost else GREY, width=2.0))
        b.append(carrow(c[0], c[1], c[0] + end_cost * scale * 4.0, c[1], uid,
                        "green" if end_cost < face_cost else "purple",
                        GREEN if end_cost < face_cost else GREY, width=2.0))
        b.append(label(cx, 66, f"z = {z:.2f} m", "xs", "middle"))
        b.append(label(cx, cy + 118, note, "xs muted", "middle"))
        b.append(label(cx, cy + 136, f"depth {min(face_cost, end_cost) * 1000:.0f} mm",
                       "xs mono", "middle"))

    b.append(rule(20, 336, W - 20, 336, "grid"))
    b += table_body(24, 352, ["over one 10 µm step, either side of z = 1.15", "measured"],
               [["worst change in the NORMAL", "90.00°"],
                ["worst change in the DEPTH", "0.01001 mm"]],
               [420, 150])

    _p, _h = para(24, 432,
                  "The two escapes cost 0.05 and 1 − (z − 0.2). They cross at "
                  "z = 1.15, and past it the OTHER one is shorter: min of two "
                  "smooth functions is not smooth where the argmin changes.")
    b += _p
    b.append(label(24, 432 + _h + 12,
                   "Every penetration-depth implementation has this. 8.7 does not "
                   "remove it; it makes the contact SET survive it.", "xs t-hi",
                   "start"))

    return svg(uid, W, H,
               "The contact normal swings ninety degrees in one ten-micrometre "
               "step while the depth changes by ten thousandths of a millimetre.",
               "Three panels showing a small cube pressed into a wall that ends, "
               "seen from above. In each, two arrows leave the cube: one out "
               "through the wall's face costing 50 millimetres, one past the "
               "wall's end costing 1 minus z plus 0.2. At z = 1.10 the face escape "
               "is shorter and is drawn in green; at z = 1.20 the end escape is; "
               "at z = 1.15 they are equal. A table records that over a single "
               "10 micrometre step across that crossing the normal changes by "
               "90.00 degrees while the depth changes by 0.01001 millimetres.",
               b)


# ===========================================================================
# 4. A support function names no feature
# ===========================================================================
#
# §C.1: 4,000 directions in a 0.001° cone about +y. `support` returns all four
# corners; `support_face` returns 4 vertices 4000/4000 times, the same four.
# §C.2: the same jitter about a corner direction returns 1 corner of 8.

def fig4():
    uid = "l87f4"
    W, H = 900, 470
    b = [cmarker(uid, "purple", PURPLE), cmarker(uid, "green", GREEN),
         cmarker(uid, "red", RED)]

    b.append(label(20, 22, "`support(d)` returns ONE point even when the argmax "
                           "is a whole face.", "sm", "start"))

    scale = 118.0

    # ---- panel 1: the argmax set ------------------------------------------
    cx, cy = 190, 196
    P = lambda p: (cx + p[0] * scale, cy - p[1] * scale)
    sq = [(-0.6, -0.6), (0.6, -0.6), (0.6, 0.6), (-0.6, 0.6)]
    b.append(poly([P(p) for p in sq], BLUE, width=1.7))
    b.append(cline(P((-0.6, 0.6))[0], P((-0.6, 0.6))[1], P((0.6, 0.6))[0],
                   P((0.6, 0.6))[1], RED, width=4.0))
    for px in (-0.6, 0.6):
        q = P((px, 0.6))
        b.append(f'<circle cx="{q[0]:.1f}" cy="{q[1]:.1f}" r="4.4" fill="{RED}"/>')
    # the cone of directions
    for k in range(-3, 4):
        a = math.radians(90 + k * 7.0)
        t = P((0.0, 0.62))
        b.append(cline(t[0], t[1], t[0] + math.cos(a) * 44, t[1] - math.sin(a) * 44,
                       GREY, width=1.0, dash="3 3"))
    b.append(label(cx, 62, "d, jittered inside the face's normal cone", "xs",
                   "middle"))
    b.append(label(cx, cy + 116, "the argmax is the whole top EDGE", "xs muted",
                   "middle"))
    b.append(label(cx, cy + 134, "(a face, in three dimensions)", "xs muted",
                   "middle"))

    # ---- panel 2: what each query returns ---------------------------------
    b.append(rule(360, 60, 360, 350, "grid"))
    b += table_body(392, 82, ["4,000 directions, cone 0.001° wide", "returned"],
               [["distinct corners `support` gave back", "4 of 4"],
                ["`support_face` count == 4", "4000 of 4000"],
                ["distinct corners it named", "4"],
                ["its `feature` id ever changed", "never"]],
               [300, 150],
               title="§4 — the same query, two functions")

    b.append(label(392, 206, "Which corner comes back is decided by a tie-break on "
                             "three dot", "xs muted", "start"))
    b.append(label(392, 222, "products that are all zero in exact arithmetic. The "
                             "answer is", "xs muted", "start"))
    b.append(label(392, 238, "DETERMINISTIC and it is ARBITRARY, and those are "
                             "not the same", "xs muted", "start"))
    b.append(label(392, 254, "thing.", "xs muted", "start"))

    b += table_body(392, 280, ["the control: a direction pointed at a CORNER", "returned"],
               [["distinct corners `support` gave back", "1 of 8"]],
               [300, 150])

    b.append(label(392, 338, "So the ambiguity belongs to the QUERY, not to the "
                             "function: it", "xs muted", "start"))
    b.append(label(392, 354, "appears exactly where the feature is bigger than a "
                             "point —", "xs muted", "start"))
    b.append(label(392, 370, "which is exactly where a manifold is needed.", "xs t-hi",
                   "start"))

    b.append(rule(20, 396, W - 20, 396, "grid"))
    b.append(label(24, 420, "8.5's claim stands: a convex set IS determined by its "
                            "support function. But determining a set and naming "
                            "one of its faces are", "xs muted", "start"))
    b.append(label(24, 438, "different questions, and only the first one has an "
                            "answer in a single point. So the interface widens by "
                            "exactly one query.", "xs muted", "start"))
    b.append(label(24, 460, "It did NOT need EPA's winning triangle. Its three "
                            "vertices are support points, with the tie-break "
                            "problem intact.", "xs", "start"))

    return svg(uid, W, H,
               "A support function determines a convex set but cannot name one of "
               "its faces.",
               "Left: a square seen edge on, with its top edge drawn thick in red "
               "as the argmax set, its two endpoints marked, and a fan of dashed "
               "query directions inside the face's normal cone. Right: two tables. "
               "Over 4,000 directions drawn from a cone one thousandth of a degree "
               "wide, the support function returns all four corners while "
               "support_face returns the same four vertices every time and never "
               "changes its feature id. The control, a direction pointed at a "
               "corner, returns one corner of eight — so the ambiguity belongs to "
               "the query rather than to the function.",
               b)


# ===========================================================================
# 5. The five features, and the one that needs a tolerance
# ===========================================================================
#
# §D.1: box 20,000 faces, worst vertex 2.48e−06 m off the box, always the
# most-aligned of six, all CCW. Capsule 1.19e−07 m, side 1041 / cap 18959.
# §D.2: the 24-gon's gather sweep, and the octagon inset 0.03806 m predicted
# and measured.

def fig5():
    uid = "l87f5"
    W, H = 900, 552
    b = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED),
         cmarker(uid, "purple", PURPLE)]

    b.append(label(20, 22, "`support_face` returns the whole feature, and the "
                           "counts are the features these shapes actually have.",
                   "sm", "start"))

    scale = 58.0
    shapes = [
        ("sphere", 1, "a ball has no flat feature"),
        ("capsule, end on", 1, "the pole of a cap"),
        ("capsule, side on", 2, "the segment along its spine"),
        ("box", 4, "the dominant face"),
        ("hull", "n", "the vertices on the plane"),
    ]
    for i, (name, count, note) in enumerate(shapes):
        cx = 110 + i * 172
        cy = 150
        P = lambda p: (cx + p[0] * scale, cy - p[1] * scale)
        if i == 0:
            pts = [(math.cos(a), math.sin(a)) for a in
                   [math.radians(k * 8) for k in range(46)]]
            b.append(poly([P(p) for p in pts], BLUE, width=1.6))
            hit = [(0.0, -1.0)]
        elif i <= 2:
            pts = ([(math.cos(a) * 0.6, math.sin(a) * 0.6 - 0.55)
                    for a in [math.radians(180 + k * 9) for k in range(21)]]
                   + [(math.cos(a) * 0.6, math.sin(a) * 0.6 + 0.55)
                      for a in [math.radians(k * 9) for k in range(21)]])
            b.append(poly([P(p) for p in pts], BLUE, width=1.6))
            hit = [(0.0, -1.15)] if i == 1 else [(-0.6, -0.55), (-0.6, 0.55)]
        elif i == 3:
            pts = [(-0.85, -0.85), (0.85, -0.85), (0.85, 0.85), (-0.85, 0.85)]
            b.append(poly([P(p) for p in pts], BLUE, width=1.6))
            hit = [(-0.85, -0.85), (0.85, -0.85)]
        else:
            pts = [(math.cos(math.radians(k * 60 + 12)),
                    math.sin(math.radians(k * 60 + 12))) for k in range(6)]
            b.append(poly([P(p) for p in pts], BLUE, width=1.6))
            hit = [pts[3], pts[4]]
        for q in hit:
            r = P(q)
            b.append(f'<circle cx="{r[0]:.1f}" cy="{r[1]:.1f}" r="4.2" '
                     f'fill="{RED}"/>')
        if len(hit) > 1:
            b.append(cline(P(hit[0])[0], P(hit[0])[1], P(hit[-1])[0], P(hit[-1])[1],
                           RED, width=3.2))
        b.append(carrow(cx, cy + 96, cx, cy + 72, uid, "purple", PURPLE, width=1.5))
        b.append(label(cx, 62, name, "xs", "middle"))
        b.append(label(cx, cy + 122, f"{count} vertices" if count != 1
                       else "1 vertex", "xs mono t-hi", "middle"))
        b.append(label(cx, cy + 140, note, "xs muted", "middle"))

    b.append(rule(20, 320, W - 20, 320, "grid"))

    b += table_body(24, 338, ["gather_sin", "prism side face", "bulge", "end cap"],
               [["0.002", "4 vertices", "0.0000 m", "8 vertices"],
                ["0.010", "4 vertices", "0.0000 m", "8 vertices"],
                ["0.050  (default)", "8 vertices", "0.0338 m", "8 vertices"],
                ["0.200", "8 vertices", "0.0990 m", "8 vertices"]],
               [150, 140, 100, 110],
               title="§5 — a 24-sided prism, whose face has to be GATHERED")

    _p, _h = para(560, 356,
                  "A box, a sphere and a capsule answer with closed forms, because "
                  "they KNOW their own faces. A hull is a point cloud with no face "
                  "list, so its face has to be recovered from the supporting plane "
                  "— and how wide to make that plane is a guess about geometry "
                  "that nothing in the data answers.", cols=52)
    b += _p
    _p2, _h2 = para(560, 356 + _h + 14,
                    "TOO TIGHT and a cylinder's side contact is a bare edge. TOO "
                    "LOOSE and three flat faces merge into one bulged polygon. "
                    "There is no value that is right, because the question is "
                    "topological. The fix is to give hull its faces.",
                    cols=52, cls="xs")
    b += _p2

    return svg(uid, W, H,
               "Each primitive presents a different kind of contact feature, and "
               "only the hull's needs a tolerance.",
               "Five small diagrams in a row, each with an arrow showing the query "
               "direction and the returned feature marked in red: a sphere returns "
               "one vertex, a capsule queried end on returns one, a capsule "
               "queried side on returns a two-vertex segment, a box returns its "
               "four-vertex dominant face, and a hull returns the vertices on its "
               "supporting plane. Below, a table sweeping the hull's gather "
               "tolerance on a 24-sided prism: at 0.002 and 0.010 it recovers the "
               "true four-vertex quad with no bulge, and at 0.050 and 0.200 it "
               "merges neighbouring faces into an eight-vertex polygon bulging "
               "33.8 and 99.0 millimetres.",
               b)


# ===========================================================================
# 6. Reference, incident, and how parallel is parallel
# ===========================================================================
#
# §E.1 sweep: face_cos / face% / mean pts / median / worst normal error.
# §E.2: ball on a floor, EPA 0.001172° worst off +y, face normal 0.000000°,
#       4000 of 4000 frames exactly (0,1,0). Random boxes: EPA worst 0.0163°,
#       face worst 2.5600°, and where the two agree (36,113 of 39,403) 0.0000°.

def fig6():
    uid = "l87f6"
    W, H = 900, 516
    b = [cmarker(uid, "purple", PURPLE), cmarker(uid, "green", GREEN),
         cmarker(uid, "red", RED)]

    b.append(label(20, 22, "One of the two faces supplies the clipping planes. "
                           "Which one changes every contact id.", "sm", "start"))

    scale = 112.0
    cx, cy = 210, 190
    P = lambda p: (cx + p[0] * scale, cy - p[1] * scale)
    b.append(poly([P((-1.15, -0.5)), P((1.15, -0.5)), P((1.15, 0.0)),
                   P((-1.15, 0.0))], BLUE, width=1.7))
    # PIVOT ON THE CONTACT, not on a corner. Rotating about the crate's
    # bottom-LEFT corner buries the whole lower edge 8 cm into the floor and the
    # picture reads as a crate falling through it rather than resting on it.
    tilt = math.radians(9.0)
    crate = rot([(-0.5, 0.0), (0.5, 0.0), (0.5, 1.0), (-0.5, 1.0)], tilt,
                (0.0, 0.0))
    crate = [add(p, (0.0, 0.055)) for p in crate]
    b.append(poly([P(p) for p in crate], RED, width=1.7))
    # the two candidate normals
    # OUTSIDE the crate's footprint. The floor's face runs to x = ±1.15 and the
    # crate only to ±0.5, so there is clear plate to draw on — and a label inside
    # the crate is text on a shape, which check-page.js reports and a reader reads
    # as a caption written across a solid.
    b.append(carrow(P((-0.92, 0.0))[0], P((-0.92, 0.0))[1],
                    P((-0.92, 0.42))[0], P((-0.92, 0.42))[1], uid, "green", GREEN,
                    width=2.2))
    b.append(label(P((-0.92, 0.52))[0], P((-0.92, 0.52))[1], "A's face normal",
                   "xs", "middle"))
    # B's outward normal leaves the MIDDLE of its bottom edge, for the same
    # reason: an arrow from a corner reads as belonging to the corner.
    d = norm(sub(crate[1], crate[0]))
    n2 = perp(d)
    q0 = mul(add(crate[0], crate[1]), 0.5)
    b.append(carrow(P(q0)[0], P(q0)[1], P(add(q0, mul(n2, -0.46)))[0],
                    P(add(q0, mul(n2, -0.46)))[1], uid, "red", RED, width=2.2))
    b.append(label(P(add(q0, mul(n2, -0.60)))[0] + 42,
                   P(add(q0, mul(n2, -0.60)))[1] + 10, "B's face normal, 9° off",
                   "xs", "middle"))
    b.append(label(cx, 62, "cos_a = 1.00000,  cos_b = 0.98769", "xs mono", "middle"))
    b.append(label(cx, cy + 128, "A is the REFERENCE: its face supplies the side",
                   "xs muted", "middle"))
    b.append(label(cx, cy + 144, "planes and the plane every depth is measured from.",
                   "xs muted", "middle"))

    b.append(rule(430, 60, 430, 340, "grid"))
    b += table_body(460, 82, ["face_cos", "face %", "mean pts", "worst normal"],
               [["0.50000", "100.0", "3.285", "44.8024°"],
                ["0.90000", "93.9", "3.177", "25.8336°"],
                ["0.99000", "71.7", "2.689", "8.1094°"],
                ["0.99900  (default)", "59.3", "2.381", "2.5611°"],
                ["0.99999", "52.8", "2.213", "0.2576°"]],
               [150, 75, 85, 110],
               title="§6 — 40,000 box pairs, against 8.4's exact MTV")

    b.append(label(460, 232, "The worst error is acos(face_cos) EXACTLY, which is "
                             "what makes", "xs muted", "start"))
    b.append(label(460, 248, "this a knob rather than a magic number: choose the "
                             "largest", "xs muted", "start"))
    b.append(label(460, 264, "normal error a solver can live with, and take its "
                             "cosine.", "xs muted", "start"))
    b.append(label(460, 290, "Too loose calls a face 60° from the contact "
                             "direction the", "xs", "start"))
    b.append(label(460, 306, "normal. Too tight refuses a real face contact and "
                             "the mean", "xs", "start"))
    b.append(label(460, 322, "contact count falls with it.", "xs", "start"))

    b.append(rule(20, 356, W - 20, 356, "grid"))
    b += table_body(24, 372, ["the manifold's normal is the REFERENCE FACE's",
                         "EPA's", "the face's"],
               [["a 0.4 m ball on a floor, 4,000 frames, worst off +y",
                 "0.001172°", "0.000000°"],
                ["random box pairs, against the SAT's exact MTV, worst",
                 "0.0163°", "2.5600°"],
                ["…restricted to where the two agree (36,113 of 39,403)",
                 "0.0163°", "0.0000°"]],
               [400, 110, 110])

    _p, _h = para(24, 470,
                  "On a flat contact the face normal IS the answer and EPA's is an "
                  "approximation to it. Off one it is deliberately SNAPPED — which "
                  "is the trade a solver wants, because a normal that is piecewise "
                  "constant does not shake a stack and one that is merely accurate "
                  "does.")
    b += _p

    return svg(uid, W, H,
               "Choosing the reference face, and what the parallelism tolerance "
               "buys and costs.",
               "Left: a crate tilted nine degrees resting on a floor, with two "
               "arrows marking the two candidate face normals — the floor's, "
               "exactly along the contact normal, and the crate's, nine degrees "
               "off. The floor wins and becomes the reference. Right: a table "
               "sweeping face_cos over five values, showing the fraction of "
               "contacts classified as face contacts falling from 100 to 52.8 "
               "percent, the mean contact count falling from 3.285 to 2.213, and "
               "the worst normal error tracking the arc cosine of the tolerance "
               "exactly, from 44.8 degrees down to 0.26. Below, a second table "
               "comparing the manifold's normal with EPA's against the exact "
               "minimum translation.",
               b)


# ===========================================================================
# 7. Sutherland and Hodgman, one plane at a time
# ===========================================================================

REF7 = [(-1.15, -0.85), (1.15, -0.85), (1.15, 0.85), (-1.15, 0.85)]
INC7 = rot([(-0.72, -0.62), (0.72, -0.62), (0.72, 0.62), (-0.72, 0.62)],
           math.radians(24.0), (0.0, 0.0))
INC7 = [add(p, (0.78, 0.46)) for p in INC7]


def fig7():
    uid = "l87f7"
    W, H = 900, 494
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "green", GREEN),
         cmarker(uid, "red", RED), cmarker(uid, "grey", GREY)]

    b.append(label(20, 22, "The clip is 3.3's near-plane algorithm with a "
                           "different set of planes — and it is COMPUTED here, "
                           "not drawn.", "sm", "start"))

    scale = 76.0
    for panel in range(4):
        cx = 128 + panel * 216
        cy = 200
        P = lambda p: (cx + p[0] * scale, cy - p[1] * scale)
        kept, sides = clip2(REF7, INC7, panel + 1)
        b.append(poly([P(p) for p in REF7], BLUE, width=1.6))
        b.append(poly([P(p) for p in INC7], GREY, width=1.1, dash="3 3"))
        if len(kept) >= 2:
            b.append(poly([P(p) for p in kept], AMBER, width=2.2))
        for q in kept:
            r = P(q)
            b.append(f'<circle cx="{r[0]:.1f}" cy="{r[1]:.1f}" r="3.2" '
                     f'fill="{AMBER}"/>')
        a0, a1, side = sides[panel]
        e = norm(sub(a1, a0))
        # The dashed plane is EXTENDED a little past the edge it carries, so the
        # half-space it defines is legible — but only a little, or it crosses
        # into the neighbouring panel and reads as a line belonging to that one.
        far0 = add(a0, mul(e, -0.30))
        far1 = add(a1, mul(e, 0.30))
        b.append(cline(P(far0)[0], P(far0)[1], P(far1)[0], P(far1)[1], GREEN,
                       width=1.6, dash="5 4"))
        mid = mul(add(a0, a1), 0.5)
        b.append(carrow(P(mid)[0], P(mid)[1], P(add(mid, mul(side, 0.30)))[0],
                        P(add(mid, mul(side, 0.30)))[1], uid, "green", GREEN,
                        width=1.8))
        b.append(label(cx, 60, f"side plane {panel + 1} of 4", "xs", "middle"))
        b.append(label(cx, cy + 118, f"{len(kept)} vertices left", "xs mono",
                       "middle"))

    b.append(rule(20, 340, W - 20, 340, "grid"))
    _p, _h = para(24, 362,
                  "For edge v[i] → v[i+1] of a polygon wound counter-clockwise "
                  "about its own normal, the OUTWARD in-plane direction is "
                  "cross(edge, normal). Keep every vertex on the non-positive "
                  "side; where an edge crosses, insert the crossing point.",
                  cls="xs")
    b += _p
    _p2, _h2 = para(24, 362 + _h + 10,
                    "REVERSE THE WINDING AND EVERY SIDE PLANE FACES INWARD. §5's "
                    "control does exactly that, and the manifold comes back with "
                    "0 of 4 incident vertices inside — an empty contact on a pair "
                    "that is plainly touching. It is the quietest way there is to "
                    "lose a floor.")
    b += _p2
    _p3, _h3 = para(24, 362 + _h + 10 + _h2 + 10,
                    "The final polygon is the CONTACT SET: the intersection of two "
                    "convex faces, which is convex and has at most (m + n) "
                    "vertices — each plane adds at most two and removes at least "
                    "one.")
    b += _p3

    return svg(uid, W, H,
               "Sutherland-Hodgman clipping of the incident face against the "
               "reference face's four side planes, one pass per panel.",
               "Four panels. In each, a blue rectangle is the reference face and a "
               "grey dashed rotated square is the incident face. A green dashed "
               "line marks the side plane being applied, with a short green arrow "
               "for its outward normal. The amber polygon is what survives so far, "
               "with its vertices marked: the incident square is progressively cut "
               "back as each of the four planes is applied, gaining a crossing "
               "vertex where an edge leaves the half-space and losing the vertices "
               "outside it.",
               b)


# ===========================================================================
# 8. What the clip produces, and how it is named
# ===========================================================================
#
# §F.1 over 200,000 random box pairs, 189,282 overlapping:
#   face 113,677 / edge 75,604 / point 0 / clip empty 1
#   points per manifold 1:79,200 2:10,718 3:16,245 4:83,119
#   kinds: incident vertex 211,615, crossing 162,966, reference vertex 31,661,
#          edge pair 75,604, point 1
#   worst |sdf| at a claimed surface point, face contacts: 2.52e−06 m

def fig8():
    uid = "l87f8"
    W, H = 900, 468
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "green", GREEN),
         cmarker(uid, "red", RED)]

    b.append(label(20, 22, "Three ways a contact point is made, and every one of "
                           "them is an INDEX rather than a position.", "sm",
                   "start"))

    scale = 74.0
    kinds = [
        ("incident vertex", "a corner of the incident face,\ninside the reference",
         211615),
        ("crossing", "an incident EDGE cut by a\nreference side plane", 162966),
        ("reference vertex", "a corner of the REFERENCE face,\ninside the incident",
         31661),
    ]
    for i, (name, note, count) in enumerate(kinds):
        cx = 150 + i * 200
        cy = 178
        P = lambda p: (cx + p[0] * scale, cy - p[1] * scale)
        if i == 2:
            ref = [(-0.55, -0.55), (0.55, -0.55), (0.55, 0.55), (-0.55, 0.55)]
            inc = [(-1.05, -1.05), (1.05, -1.05), (1.05, 1.05), (-1.05, 1.05)]
        else:
            ref = [(-1.05, -0.85), (1.05, -0.85), (1.05, 0.85), (-1.05, 0.85)]
            inc = ([(-0.6, -0.6), (0.6, -0.6), (0.6, 0.6), (-0.6, 0.6)] if i == 0
                   else [add(p, (0.72, 0.0)) for p in
                         [(-0.6, -0.6), (0.6, -0.6), (0.6, 0.6), (-0.6, 0.6)]])
        b.append(poly([P(p) for p in ref], BLUE, width=1.6))
        b.append(poly([P(p) for p in inc], RED, width=1.4, dash="3 3"))
        kept, _ = clip2(ref, inc, len(ref))
        if len(kept) >= 2:
            b.append(poly([P(p) for p in kept], AMBER, width=2.0))
        marks = (inc if i == 0 else (kept if i == 1 else ref))
        for q in marks:
            r = P(q)
            b.append(f'<circle cx="{r[0]:.1f}" cy="{r[1]:.1f}" r="4.0" '
                     f'fill="{GREEN}"/>')
        b.append(label(cx, 60, name, "xs", "middle"))
        for j, line in enumerate(note.split("\n")):
            b.append(label(cx, cy + 106 + j * 15, line, "xs muted", "middle"))
        b.append(label(cx, cy + 142, f"{count:,} of 481,847", "xs mono t-hi",
                       "middle"))

    b.append(rule(620, 56, 620, 340, "grid"))
    b += table_body(648, 78, ["200,000 random box pairs", ""],
               [["overlapping", "189,282"],
                ["face contacts", "113,677"],
                ["edge contacts", "75,604"],
                ["clip kept nothing", "1"],
                ["manifolds with 0 points", "0"]],
               [170, 80])
    b += table_body(648, 208, ["points per manifold", ""],
               [["1", "79,200"], ["2", "10,718"], ["3", "16,245"],
                ["4", "83,119"]],
               [170, 80])

    b.append(rule(20, 356, W - 20, 356, "grid"))
    b.append(label(24, 378, "AN ID IS A SET OF INDICES INTO GEOMETRY THAT DOES "
                            "NOT MOVE — a face's own identity, a vertex's own "
                            "index on its shape. Nothing in it is", "xs", "start"))
    b.append(label(24, 396, "computed from a position, so nothing in it can change "
                            "because a body moved by a micron. It changes when "
                            "the CONTACT changes.", "xs muted", "start"))
    b.append(label(24, 422, "Checked against both boxes with a signed distance "
                            "field: the worst claimed surface point on a face "
                            "contact is 2.52e−06 m off, over 410,075 points.",
                   "xs muted", "start"))
    b.append(label(24, 446, "And a fixture whose answer is known on paper: two 1 m "
                            "cubes crossed at 45°, lowered 5 mm — contact at "
                            "(0, 0.70461, 0), depth 0.00500 m, normal +y.", "xs",
                   "start"))

    return svg(uid, W, H,
               "The three kinds of contact point the clipper produces, and their "
               "measured frequencies.",
               "Three panels. In the first, a small red dashed square sits wholly "
               "inside a blue reference rectangle and all four of its corners "
               "become contact points — the incident-vertex case, 211,615 of "
               "481,847 points. In the second it overhangs one side, so two "
               "corners survive and two crossing points appear where its edges "
               "meet the side plane — 162,966. In the third the reference "
               "rectangle is the smaller one and its own corners fall inside the "
               "incident face — 31,661. To the right, tables: of 200,000 random "
               "box pairs, 189,282 overlap, 113,677 give face contacts and 75,604 "
               "edge contacts, the clip kept nothing exactly once, and no "
               "overlapping pair produced an empty manifold.",
               b)


# ===========================================================================
# 9. Four, and why not five
# ===========================================================================
#
# §G.1 clipped-count histogram over 99,995 face contacts, and mean support area
# 0.90375 m² with the reduction against 0.74738 m² for the first four (+20.9%).
# §G.3 over 61,455 manifolds that clipped past four: area against the whole
# clipped polygon mean 0.88871; against the BEST four, mean 0.96316, median
# 1.00000, 1st percentile 0.69067, worst 0.50572. Depth lost: 0.000e+00 m.

def fig9():
    uid = "l87f9"
    W, H = 900, 496
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "green", GREEN),
         cmarker(uid, "grey", GREY)]

    b.append(label(20, 22, "The clip produces up to eight points. A solver can use "
                           "four, and which four is not a matter of taste.", "sm",
                   "start"))

    # ---- the reduction, drawn ---------------------------------------------
    scale = 82.0
    cx, cy = 172, 190
    P = lambda p: (cx + p[0] * scale, cy - p[1] * scale)
    octo = [(math.cos(math.radians(k * 45 + 12)) * 1.05,
             math.sin(math.radians(k * 45 + 12)) * 0.82) for k in range(8)]
    b.append(poly([P(p) for p in octo], GREY, width=1.4, dash="3 3"))
    for q in octo:
        r = P(q)
        b.append(f'<circle cx="{r[0]:.1f}" cy="{r[1]:.1f}" r="3.0" fill="{GREY}"/>')
    # the four the heuristic keeps: deepest, farthest, then a max-area point
    # on each side of that axis. Deepest is taken as index 5 here.
    i0 = 5
    i1 = max(range(8), key=lambda k: length(sub(octo[k], octo[i0])))
    axis = sub(octo[i1], octo[i0])
    pos = max((k for k in range(8) if k not in (i0, i1)),
              key=lambda k: cross(axis, sub(octo[k], octo[i0])))
    neg = min((k for k in range(8) if k not in (i0, i1)),
              key=lambda k: cross(axis, sub(octo[k], octo[i0])))
    keep = [octo[i0], octo[pos], octo[i1], octo[neg]]
    b.append(poly([P(p) for p in keep], AMBER, width=2.2))
    for j, q in enumerate(keep):
        r = P(q)
        b.append(f'<circle cx="{r[0]:.1f}" cy="{r[1]:.1f}" r="4.6" '
                 f'fill="{AMBER}"/>')
    b.append(label(P(octo[i0])[0], P(octo[i0])[1] + 20, "1. deepest", "xs", "middle"))
    b.append(label(P(octo[i1])[0] + 26, P(octo[i1])[1] - 20, "2. farthest from it",
                   "xs", "middle"))
    b.append(label(cx, 62, "eight clipped points, four kept", "xs", "middle"))
    b.append(label(cx, cy + 122, "3. and 4. the largest triangle on EACH", "xs muted",
                   "middle"))
    b.append(label(cx, cy + 138, "side of that axis — which is what stops", "xs muted",
                   "middle"))
    b.append(label(cx, cy + 154, "the four collapsing onto a line", "xs muted",
                   "middle"))

    # ---- the histogram -----------------------------------------------------
    b.append(rule(360, 56, 360, 330, "grid"))
    hist = [(2, 8), (3, 1573), (4, 36959), (5, 33055), (6, 20972), (7, 6349),
            (8, 1079)]
    total = sum(v for _, v in hist)
    b.append(label(392, 74, "§8 — clipped points before reduction, 99,995 face "
                            "contacts", "xs", "start"))
    b += bars(392, 100, 500,
              [(f"{k}", v, AMBER if k > 4 else GREEN) for k, v in hist],
              max(v for _, v in hist), row_h=20, label_w=30,
              fmt=lambda v: f"{v:,}  ({100.0 * v / total:5.2f}%)")

    b.append(label(392, 258, "A third of face contacts clip to more than four, so "
                             "the reduction", "xs muted", "start"))
    b.append(label(392, 274, "is not an edge case — it runs on one contact in "
                             "three.", "xs muted", "start"))

    b.append(rule(20, 346, W - 20, 346, "grid"))
    b += table_body(24, 362, ["61,455 manifolds that clipped past four", "mean",
                         "median", "worst"],
               [["area kept, as a fraction of the WHOLE clipped polygon",
                 "0.88871", "—", "0.50219"],
                ["area kept, as a fraction of the BEST four (brute forced)",
                 "0.96316", "1.00000", "0.50572"],
                ["depth lost by the reduction", "0.000 m", "—", "0.000 m"]],
               [370, 100, 100, 100])

    b.append(label(24, 458, "Four points can never span an octagon: the largest "
                            "quadrilateral inscribed in a regular one is 70.7% of "
                            "it. That is most of the gap in the first row", "xs "
                            "muted", "start"))
    b.append(label(24, 476, "and none of it in the second. The reduction is within "
                            "a few per cent of the best four there are, for four "
                            "comparisons and no search.", "xs", "start"))

    return svg(uid, W, H,
               "Reducing the clipped contact set to four points, and what the "
               "fifth would buy.",
               "Left: eight clipped points on a dashed grey octagon, with the four "
               "the reduction keeps joined into an amber quadrilateral. The "
               "deepest point is labelled first, the point farthest from it "
               "second, and the remaining two are the largest triangles on each "
               "side of that axis. Right: a bar chart of how many points the clip "
               "produces before reduction over 99,995 face contacts — 4 points "
               "36.96 percent of the time, 5 points 33.06, 6 points 20.97, and 7 "
               "or 8 the rest. Below, a table: the four kept points span 88.9 "
               "percent of the whole clipped polygon and 96.3 percent of the best "
               "four there are, and lose no depth at all.",
               b)


# ===========================================================================
# 10. Persistence: ids, not positions
# ===========================================================================
#
# §H.1: a crate settling, 591 frames, 1,969 points. ID 82.28%, position at
#       10 µm 0.00%, position at 10 mm 82.17%. At rest and jittered 100 nm a
#       frame, 1,999 frames: ID 100.00%, position at 10 µm 100.00%.
#       Slid 2 m along the same face: 4 of 4. Rolled onto its side: 0 of 4.
# §H.2: 208,000 pairs, 0 flips. §H.3: 22,807 stores, 990 hits, 21,766 drops,
#       peak 79 pairs, worst load 0.500, 749 allocations over 400 frames.

def fig10():
    uid = "l87f10"
    W, H = 900, 530
    b = [cmarker(uid, "green", GREEN), cmarker(uid, "red", RED),
         cmarker(uid, "grey", GREY)]

    b.append(label(20, 22, "A contact is matched to last frame's by IDENTITY, "
                           "because it cannot be matched by position.", "sm",
                   "start"))

    # ---- the id, anatomised -----------------------------------------------
    fields = [("reference_face", "u16", "the reference shape's face"),
              ("incident_face", "u16", "the incident shape's face"),
              ("reference_index", "u8", "its vertex, or 0xff"),
              ("incident_index", "u8", "its vertex, or 0xff"),
              ("kind", "u8", "how to read the two above"),
              ("flipped", "u8", "the reference was on b")]
    b.append(label(24, 62, "contact_id — six bytes of named fields", "xs", "start"))
    x = 24
    for i, (name, ty, note) in enumerate(fields):
        yy = 84 + i * 30
        b.append(box(x, yy - 12, 150, 22, GREEN if i < 4 else GREY, rx=3,
                     opacity=0.20))
        b.append(label(x + 8, yy + 3, name, "xs mono", "start"))
        b.append(label(x + 162, yy + 3, ty, "xs mono muted", "start"))
        b.append(label(x + 200, yy + 3, note, "xs muted", "start"))

    b.append(label(24, 282, "Not one of them is computed from a position, so not "
                            "one of them can change because a body moved by a "
                            "micron.", "xs muted", "start"))

    # ---- the measurement ---------------------------------------------------
    b.append(rule(520, 56, 520, 300, "grid"))
    b += table_body(548, 78, ["a crate settling, 591 frames, 1,969 points", "matched"],
               [["by ID", "82.28%"],
                ["by POSITION, within 10 µm", "0.00%"],
                ["by POSITION, within 10 mm", "82.17%"]],
               [200, 100])
    b += table_body(548, 170, ["the same crate at rest, jittered 100 nm a frame",
                          "matched"],
               [["by ID", "100.00%"],
                ["by POSITION, within 10 µm", "100.00%"]],
               [200, 100])

    _p, _h = para(548, 248,
                  "A POSITION MATCHER WORKS PERFECTLY ON A BODY THAT IS NOT MOVING, "
                  "which is exactly why it is a trap: the tolerance that works at "
                  "100 nm of jitter fails at the 8 cm a frame a crate travels "
                  "while it lands.", cols=54)
    b += _p

    b.append(rule(20, 316, W - 20, 316, "grid"))
    b += table_body(24, 332, ["what an id MEANS", "matched"],
               [["the crate slid 2 m along the same face", "4 of 4"],
                ["the crate rolled onto a different face", "0 of 4"]],
               [340, 100])

    _p, _h = para(490, 346,
                  "An id names a FEATURE, not a place. It cannot detect a teleport "
                  "and should not be asked to: a body that was MOVED rather than "
                  "simulated is a discontinuity, and invalidating its cached "
                  "manifolds is the CACHE's job.", cols=52)
    b += _p

    _p, _h = para(24, 424,
                  "§9's first control expected zero matches after a two-metre "
                  "teleport and got four of four — and the manifold was right. The "
                  "same corner really is on the same face. A control that convicts "
                  "the code of the test's own mistake is the most expensive kind "
                  "there is, because it looks like a finding.", cls="xs")
    b += _p
    _p2, _h2 = para(24, 424 + _h + 12,
                    "The reference choice was measured the same way: 0 flips in "
                    "208,000 pairs, because on a face contact both cosines are "
                    "exactly 1.0f and an exact tie is decided deterministically. "
                    "The knob written for it came back out.")
    b += _p2

    return svg(uid, W, H,
               "Contacts persist across frames by identity rather than by "
               "position.",
               "Left: the six named fields of a contact_id laid out as a stack of "
               "boxes — reference_face and incident_face as sixteen-bit values, "
               "reference_index and incident_index as bytes, and kind and flipped "
               "as bytes — none of them computed from a position. Right: tables of "
               "measurements. On a crate settling over 591 frames, 82.28 percent "
               "of contact points match the previous frame by id while a "
               "ten-micrometre position match finds 0.00 percent. On the same "
               "crate at rest jittered by a hundred nanometres a frame, both find "
               "100 percent — which is why position matching is a trap. Below: "
               "sliding the crate two metres along the same face keeps all four "
               "ids, and rolling it onto a different face changes all four.",
               b)


# ===========================================================================
# 11. The budget
# ===========================================================================
#
# §I.1 on 20,000 overlapping box pairs, release library:
#   GJK distance only 87.3, GJK+EPA 252.0, build_manifold alone 142.9,
#   the whole narrow phase 395.1 ns/pair, one support_face 5.2 ns.
#   0 allocations over 20,000 queries.

def fig11():
    uid = "l87f11"
    W, H = 900, 400
    b = [cmarker(uid, "amber", AMBER), cmarker(uid, "green", GREEN)]

    b.append(label(20, 22, "The manifold is a third of the narrow phase. The "
                           "penetration depth is still what costs.", "sm",
                   "start"))

    rows = [("GJK distance only", 87.3, BLUE),
            ("GJK + EPA", 252.0, GREY),
            ("build_manifold alone", 142.9, AMBER),
            ("the whole narrow phase", 395.1, GREEN)]
    b += bars(30, 80, 700, rows, 395.1, row_h=34, label_w=210,
              fmt=lambda v: f"{v:7.1f} ns/pair")

    b.append(label(30, 242, "one `support_face`", "xs", "start"))
    b.append(label(240, 242, "5.2 ns", "xs mono", "start"))
    b.append(label(30, 266, "allocations, 20,000 whole queries", "xs", "start"))
    b.append(label(240, 266, "0", "xs mono t-hi", "start"))
    b.append(label(30, 290, "sizeof(contact_id) / (contact_point)", "xs", "start"))
    b.append(label(240, 290, "8 / 40 bytes", "xs mono", "start"))

    b.append(rule(20, 312, W - 20, 312, "grid"))
    b.append(label(24, 336, "8.6 §12 deleted four default member initialisers and "
                            "bought 20.5%. The same question here buys nothing, "
                            "and the reason is the one 8.6 named:", "xs", "start"))
    b.append(label(24, 354, "the tell was the ARRAY, not the struct. `polytope` "
                            "held 128 faces and `expand` rebuilt a three-kilobyte "
                            "horizon array once per pass; a `contact_face` is",
                   "xs muted", "start"))
    b.append(label(24, 372, "124 bytes and a query builds exactly two of them, "
                            "once. There is no array of them anywhere.", "xs muted",
                   "start"))
    b.append(label(24, 396, "Nothing here allocates, measured by replacing the "
                            "global operator new and counting — 6.17 §9's method, "
                            "for the third lesson running.", "xs muted", "start"))

    return svg(uid, W, H,
               "The cost of a contact manifold against the penetration depth it is "
               "built on.",
               "A horizontal bar chart over 20,000 overlapping box pairs on a "
               "release build: GJK's distance query alone costs 87.3 nanoseconds "
               "per pair, GJK followed by EPA costs 252.0, building the manifold "
               "alone costs 142.9, and the whole narrow phase costs 395.1. One "
               "support_face query costs 5.2 nanoseconds, and twenty thousand "
               "whole queries allocate nothing at all.",
               b)


# ===========================================================================

FIGS = [fig1, fig2, fig3, fig4, fig5, fig6, fig7, fig8, fig9, fig10, fig11]


def main():
    for i, fn in enumerate(FIGS, start=1):
        path = os.path.join(OUT, f"l87_fig{i}.svg")
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}  ({os.path.getsize(path):,} bytes)")


if __name__ == "__main__":
    main()
