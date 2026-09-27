#!/usr/bin/env python3
"""scratch/figs_71.py — Lesson 7.1's diagrams.

Same rules as 5.1-6.18's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR (6.10's trap)
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed (5.12's figure 8 printed a caption
    straight across a render panel because they were not)

Every number below comes from verify_71's output, except where a figure's own
geometry is recomputed here in Python — which two of them do, deliberately. The
rotation code in this file is a THIRD independent transcription of the same
convention (the C++ product, the C++ closed form, and this), so a figure that
disagrees with the page is a real disagreement and not a plotting artifact.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_45 import read_ppm, rle_rects, hexrgb                     # noqa: E402
from figs_511 import peak_sample                                    # noqa: E402

OUT = "scratch"


# ===========================================================================
# The convention, in Python. Row-major 3x3 as nested tuples.
# ===========================================================================
def rx(a):
    c, s = math.cos(a), math.sin(a)
    return ((1, 0, 0), (0, c, -s), (0, s, c))


def ry(a):
    c, s = math.cos(a), math.sin(a)
    return ((c, 0, s), (0, 1, 0), (-s, 0, c))


def rz(a):
    c, s = math.cos(a), math.sin(a)
    return ((c, -s, 0), (s, c, 0), (0, 0, 1))


def mul(a, b):
    return tuple(tuple(sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3))
                 for i in range(3))


def apply(m, v):
    return tuple(sum(m[i][k] * v[k] for k in range(3)) for i in range(3))


def euler(yaw, pitch, roll):
    """Intrinsic y-x-z, the engine's convention, spelled the same way the code is."""
    return mul(mul(ry(yaw), rx(pitch)), rz(roll))


def rot_angle(a, b):
    """The atan2 form, matching engine::angle_between_rotations."""
    at = tuple(tuple(a[j][i] for j in range(3)) for i in range(3))
    r = mul(at, b)
    sk = (r[2][1] - r[1][2], r[0][2] - r[2][0], r[1][0] - r[0][1])
    return math.atan2(math.hypot(*sk) / 2.0, (r[0][0] + r[1][1] + r[2][2] - 1.0) / 2.0)


D = math.pi / 180.0


# ---- measured (verify_71) --------------------------------------------------
CHECKS = 34

# §B — the same triple (30, 40, 50) read twenty-four ways, degrees from ours
CONVENTIONS = [
    ("xyz", 24.77, 52.54), ("xzy", 46.53, 46.94), ("yxz", 0.00, 44.90),
    ("yzx", 36.16, 16.03), ("zxy", 51.39, 36.22), ("zyx", 46.53, 18.51),
    ("xyx", 58.80, 49.74), ("xzx", 28.60, 32.76), ("yxy", 69.55, 76.49),
    ("yzy", 83.48, 74.88), ("zxz", 42.18, 39.01), ("zyz", 62.82, 72.65),
]

# §C — conditioning. (pitch, det J, sigma naive, sigma stable, brute, 1/sigma)
COND = [
    (0.00, -1.00000, 1.0000000, 1.0000000, 0.9999998, 1.0),
    (45.00, -0.70711, 0.5411961, 0.5411960, 0.5411961, 1.8),
    (60.00, -0.50000, 0.3660254, 0.3660254, 0.3660254, 2.7),
    (80.00, -0.17365, 0.1232567, 0.1232568, 0.1232568, 8.1),
    (89.00, -0.01745, 0.0123406, 0.0123412, 0.0123412, 81.0),
    (89.90, -0.00175, 0.0012449, 0.0012340, 0.0012340, 810.4),
    (89.99, -0.00017, 0.0000000, 0.0001235, 0.0001235, 8099.8),
]

# §E — the same three angle deltas at four distances from lock
BANDS = [
    (87.0, 30.0, 282.89, 91.40, 209.5, 1.14),
    (47.0, -10.0, 243.57, 149.87, 62.5, 1.37),
    (20.0, -37.0, 203.47, 166.09, 22.5, 1.60),
    (0.0, -57.0, 169.70, 133.98, 26.7, 1.81),
]

# §F — the two metrics against a known answer. (true, atan2 rel err, trace rel err)
METRIC = [
    (170.0, 8.98e-08, 8.98e-08), (90.0, 3.0e-09, 3.0e-09), (10.0, 3.0e-09, 6.68e-07),
    (1.0, 1.55e-06, 1.47e-04), (0.23, 3.69e-06, 3.04e-03),
    (0.05, 8.05e-06, 3.09e-02), (0.004, 1.24e-04, 1.00e+00),
]


# ===========================================================================
# A tiny orthographic projector, shared by the geometric figures
# ===========================================================================
class View:
    """Isometric-ish orthographic projection, so every figure shares one camera."""

    def __init__(self, cx, cy, scale, az=38.0, el=26.0):
        self.cx, self.cy, self.scale = cx, cy, scale
        self.ca, self.sa = math.cos(az * D), math.sin(az * D)
        self.ce, self.se = math.cos(el * D), math.sin(el * D)

    def __call__(self, v):
        x, y, z = v
        # Yaw about +Y, then tip about the screen-horizontal. Right-handed, so
        # +x runs right, +y runs up the page (hence the negated screen y), and
        # +z comes toward the reader.
        sx = x * self.ca - z * self.sa
        depth = x * self.sa + z * self.ca
        sy = y * self.ce - depth * self.se
        return (self.cx + sx * self.scale, self.cy - sy * self.scale)

    def depth(self, v):
        x, y, z = v
        return (x * self.sa + z * self.ca) * self.se + y * self.ce


def axis_arrow(view, uid, origin, direction, length, kind, width=1.6):
    a = view(origin)
    b = view(tuple(origin[i] + direction[i] * length for i in range(3)))
    return arrow(round(a[0], 2), round(a[1], 2), round(b[0], 2), round(b[1], 2),
                 uid, kind=kind, width=width)


def cmarker(uid, name, colour):
    """An arrowhead in an arbitrary COLOUR.

    figs_510's markers() defines five: ink, ink-soft, hi, RED and GREEN. There is
    no blue one, and an arrow asking for a marker that does not exist renders as
    a headless line — which is exactly how figure 4's roll axis shipped in its
    first draft, indistinguishable from a leader.
    """
    return (f'<defs><marker id="e-{name}-{uid}" viewBox="0 0 10 10" refX="9" refY="5"'
            f' markerWidth="6" markerHeight="6" orient="auto-start-reverse">'
            f'<path d="M0,0 L10,5 L0,10 z" fill="{colour}"/></marker></defs>')


def carrow(x1, y1, x2, y2, uid, name, colour, width=1.6, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" '
            f'stroke="{colour}" stroke-width="{width}"{d} '
            f'marker-end="url(#e-{name}-{uid})"/>')


def frame(x, y, w, h):
    """A plot frame: STROKE ONLY.

    `box()` from figs_510 fills with `--dia-fill`, which is a dark panel colour —
    correct for a memory-layout block, catastrophic for a plot, because every
    `ink` stroke and every `muted` label drawn inside then disappears. Three of
    this lesson's figures shipped a black rectangle with invisible annotations
    inside it before this helper existed.
    """
    return hollow(x, y, w, h, GREY, width=1.1)


def poly(points, colour, width=1.4, dash=None, close=True):
    d = "M " + " L ".join(f"{x:.2f},{y:.2f}" for x, y in points) + (" Z" if close else "")
    da = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<path d="{d}" fill="none" stroke="{colour}" stroke-width="{width}"{da}/>')


def circle_pts(view, frame, u, v, radius, n=96):
    """A ring: the circle spanned by in-plane directions u and v, carried by frame."""
    pts = []
    for i in range(n + 1):
        a = 2 * math.pi * i / n
        local = tuple((u[k] * math.cos(a) + v[k] * math.sin(a)) * radius for k in range(3))
        pts.append(view(apply(frame, local)))
    return pts


# ===========================================================================
# Figure 1 — the three knobs and the three axes they actually turn about
# ===========================================================================
# ONE CLAIM: "intrinsic" means each knob turns about an axis the previous knobs
# have already moved. Three panels, and the axis that is about to be used is the
# only one drawn solid.
def fig_chain():
    W, H = 760, 356
    uid = "f71a"
    b = [cmarker(uid, "bl", BLUE)]
    PANEL_W = W / 3.0

    # ONE view for all three panels. An earlier draft chose a different azimuth
    # per panel so that each live axis projected well, and the panels stopped
    # being comparable — which is the only thing a three-panel figure is for.
    panels = [
        (0.0, 0.0, 0, "1 · yaw", "about world +Y, which never moves",
         (0.0, 1.0, 0.0), GREEN, "g"),
        (40.0, 0.0, 1, "2 · pitch", "about the +X the yaw carried (yaw 40°)",
         (1.0, 0.0, 0.0), RED, "b"),
        (40.0, 55.0, 2, "3 · roll", "about the +Z both carried (pitch 55°)",
         (0.0, 0.0, 1.0), BLUE, "bl"),
    ]

    for yaw, pitch, stage, title, sub, home, colour, kind in panels:
        cx = PANEL_W * (stage + 0.5)
        view = View(cx, 172, 46)
        b.append(hollow(PANEL_W * stage + 10, 50, PANEL_W - 20, 214, GREY, dash="3 4"))
        b.append(label(cx, 24, title, cls="sm"))
        b.append(label(cx, 40, sub, cls="xs muted"))

        f1 = ry(yaw * D)
        f2 = mul(f1, rx(pitch * D))
        carrier = [((1, 0, 0), (0, 1, 0), (0, 0, 1)), f1, f2][stage]
        live = apply(carrier, home)

        # RADIUS 1.05, and the number is load-bearing. Every label in this figure
        # sits at radius 1.35 plus 16-17 screen units, so a square at 1.5 put its
        # own edge underneath two of them — which check-page.js reported as
        # text-on-shape at 1280 and, because the labels move with the viewport,
        # NOT at 390. One of the two defects was invisible at the width the
        # figure was authored at.
        ground = [view((x, 0, z)) for x, z in ((-1.05, -1.05), (1.05, -1.05),
                                               (1.05, 1.05), (-1.05, 1.05))]
        b.append(poly(ground, GREY, width=0.9, dash="2 4"))

        # The two axes that are not this knob's, faint, at wherever they are now.
        others = [((1, 0, 0), RED), ((0, 1, 0), GREEN), ((0, 0, 1), BLUE)]
        for h, col in others:
            if h == home:
                continue
            d = apply(carrier, h)
            p0 = view((0, 0, 0))
            p1 = view(tuple(d[m] * 0.80 for m in range(3)))
            b.append(cline(p0[0], p0[1], p1[0], p1[1], col, width=1.0, dash="3 3"))

        # WHERE THIS AXIS STARTED, and the arc that carried it there. Panel 1 has
        # no arc because yaw's axis is the world's own and has not been carried
        # by anything — which is the contrast the whole figure is built on.
        p0 = view((0, 0, 0))
        if stage > 0:
            hp = view(tuple(home[m] * 1.35 for m in range(3)))
            b.append(f'<line x1="{p0[0]:.2f}" y1="{p0[1]:.2f}" x2="{hp[0]:.2f}" '
                     f'y2="{hp[1]:.2f}" stroke="{colour}" stroke-width="1.6" '
                     f'stroke-opacity="0.42" stroke-dasharray="4 3"/>')
            # Pushed radially OUTWARD along the stub, away from the arc, which
            # curls inward toward the live axis. check-page.js reported this
            # label sitting on that arc at both 1280 and 390.
            sdx, sdy = hp[0] - p0[0], hp[1] - p0[1]
            sn = math.hypot(sdx, sdy) or 1.0
            b.append(label(round(hp[0] + sdx / sn * 16.0, 1),
                           round(hp[1] + sdy / sn * 16.0 + 4, 1), "was here",
                           cls="xs muted"))
            arc = []
            for j in range(41):
                t = j / 40.0
                inter = [(1 - t) * (1 if m == n else 0) for m in range(3) for n in range(3)]
                mid = ry(yaw * D * t) if stage == 1 else mul(f1, rx(pitch * D * t))
                d = apply(mid, home)
                arc.append(view(tuple(d[m] * 1.18 for m in range(3))))
            b.append(poly(arc[:-1], colour, width=1.8, dash="3 3", close=False))
            b.append(carrow(arc[-2][0], arc[-2][1], arc[-1][0], arc[-1][1], uid, kind,
                            colour, width=1.8))

        p1 = view(tuple(live[m] * 1.35 for m in range(3)))
        b.append(carrow(p0[0], p0[1], p1[0], p1[1], uid, kind, colour, width=2.6))

        # Perpendicular label offset, so it can never land on its own arrowhead.
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]
        n = math.hypot(dx, dy) or 1.0
        # Radially outward only. A perpendicular component put the +X label on
        # the carry arc; the arrowhead is the only thing directly outward and it
        # stops at the tip, so 17 units past it is clear of everything.
        lx = p1[0] + (dx / n) * 17.0
        ly = p1[1] + (dy / n) * 17.0
        name = {0: "+Y", 1: "+X", 2: "+Z"}[stage]
        b.append(label(round(lx, 1), round(ly + 3, 1), name, cls="mono xs t-hi"))

    b.append(label(W / 2, 296, "R = rotation_y(yaw) · rotation_x(pitch) · rotation_z(roll)",
                   cls="mono sm"))
    b.append(label(W / 2, 320, "read LEFT to right and it is the story above: the knobs in "
                               "the order a human turns them.", cls="xs muted"))
    b.append(label(W / 2, 336, "read RIGHT to left and it is the order the matrices act — "
                               "roll first, in the object's own frame.", cls="xs muted"))
    return svg(uid, W, H, "The intrinsic chain",
               "Three panels sharing one viewpoint. In the first, the world +Y axis is solid "
               "green and the other two are dashed; there is no arc, because yaw's axis is "
               "the world's own. In the second, a 40 degree yaw has carried the +X axis away "
               "from its old position, which is shown as a grey dashed stub labelled 'was "
               "here', with a red arc joining the two. In the third, the yaw and a 55 degree "
               "pitch have together carried +Z, with a blue arc from its old position.", b)


# ===========================================================================
# Figure 2 — the same three numbers, twenty-four ways
# ===========================================================================
# ONE CLAIM: a triple without its convention is not an orientation. Measured
# disagreement against our reading, in degrees; ours is the only zero.
def fig_orders():
    W, H = 760, 452
    uid = "f71c"
    b = []
    X0, X1 = 92.0, 700.0
    Y0 = 78.0
    ROW = 26.0
    MAX = 90.0

    b.append(label(W / 2, 24, "The triple (30°, 40°, 50°), read under each of the "
                              "24 conventions", cls="sm"))
    b.append(label(W / 2, 42, "bar length = how far that reading lands from ours, "
                              "in degrees of rotation", cls="xs muted"))

    for g in range(0, 91, 15):
        x = X0 + (X1 - X0) * g / MAX
        b.append(rule(x, Y0 - 8, x, Y0 + ROW * 12 + 4, cls="grid", width=0.8))
        b.append(label(round(x, 1), Y0 - 12, f"{g}°", cls="xs muted"))

    for i, (name, intr, extr) in enumerate(CONVENTIONS):
        y = Y0 + ROW * i
        b.append(label(X0 - 12, y + 12, name, cls="mono xs", anchor="end"))
        for j, (value, colour) in enumerate(((intr, BLUE), (extr, PURPLE))):
            yy = y + 3 + j * 8
            w = (X1 - X0) * value / MAX
            ours = (name == "yxz" and j == 0)
            c = AMBER if ours else colour
            if w < 1.5:
                b.append(f'<circle cx="{X0:.1f}" cy="{yy + 2.5:.1f}" r="3.2" fill="{c}"/>')
            else:
                b.append(f'<rect x="{X0:.1f}" y="{yy:.1f}" width="{w:.1f}" height="5" '
                         f'fill="{c}" rx="1"/>')
            if ours:
                b.append(label(X0 + 12, yy + 7, "ours — the only zero", cls="xs t-hi",
                               anchor="start"))

    # Legend OUTSIDE the plot (6.11's trap).
    ly = Y0 + ROW * 12 + 22
    b.append(f'<rect x="{X0:.1f}" y="{ly:.1f}" width="10" height="5" fill="{BLUE}" rx="1"/>')
    b.append(label(X0 + 18, ly + 6, "intrinsic — each turn about an axis the last one moved",
                   cls="xs", anchor="start"))
    b.append(f'<rect x="{X0:.1f}" y="{ly + 16:.1f}" width="10" height="5" '
             f'fill="{PURPLE}" rx="1"/>')
    b.append(label(X0 + 18, ly + 22, "extrinsic — every turn about a fixed world axis",
                   cls="xs", anchor="start"))
    b.append(label(X1, ly + 6, "worst: yzy intrinsic, 83.48° away", cls="xs muted",
                   anchor="end"))
    b.append(label(X1, ly + 22, "12 axis orders × 2 frames = 24", cls="xs muted",
                   anchor="end"))
    return svg(uid, W, H, "Twenty-four readings of one triple",
               "A horizontal bar chart with twelve rows, one per axis order, each carrying "
               "two bars: intrinsic and extrinsic. Bar length is the angular distance from "
               "this engine's reading. Only the intrinsic y-x-z bar is zero; the rest run "
               "from 16 to 83 degrees, with intrinsic y-z-y worst at 83.48.", b)


# ===========================================================================
# Figure 3 — the rig, open and locked
# ===========================================================================
# ONE CLAIM: at pitch 90 the blue axle lies along the green one. Both panels are
# the real render, run-length encoded from the PPM the demo wrote.
def fig_rig():
    uid = "f71b"
    PALETTE = [hexrgb(c) for c in (RED, GREEN, BLUE, AMBER, PURPLE,
                                   "#ced2dc", "#7896e2", "#e2846e")]
    CELL = 3
    # 1.5 SVG units per sampled cell, not 1. The rect COUNT is unchanged — only
    # their size is — so the panel grows from 220 units wide to 330 without
    # costing a byte, and a title long enough to say what the panel shows now
    # fits above it.
    PX = 1.5
    CROP = (150, 10, 810, 530)
    gw = (CROP[2] - CROP[0]) // CELL
    gh = (CROP[3] - CROP[1]) // CELL

    PAD = 16.0
    CAP = 44.0
    PANEL_W = gw * PX
    PANEL_H = gh * PX
    W = PAD * 3 + PANEL_W * 2
    # COMPUTED, not guessed: 5.12's figure 8 printed its closing note straight
    # across a render panel because this number was estimated.
    H = CAP + PANEL_H + 40

    b = []
    for i, (path, title, sub, note) in enumerate((
            ("l71_a.ppm", "pitch 0°", "three axles, three directions",
             "|det J| = 1.000   the knobs are independent"),
            ("l71_c.ppm", "pitch 90°", "the blue axle now lies along the green",
             "|det J| = 0.000   two knobs, one axis"))):
        x = PAD + i * (PANEL_W + PAD)
        w, h, data = read_ppm(os.path.join(OUT, path))
        grid_w, grid_h, grid = peak_sample(data, w, CROP, CELL)
        b.append(label(round(x + PANEL_W / 2, 1), 20, title, cls="sm"))
        b.append(label(round(x + PANEL_W / 2, 1), 36, sub, cls="xs muted"))
        b.extend(rle_rects(grid, grid_w, grid_h, x, CAP, PX, PALETTE, levels=3,
                           bg_class="fill-soft"))
        b.append(hollow(x, CAP, PANEL_W, PANEL_H, GREY, width=1.0))
        b.append(label(round(x + PANEL_W / 2, 1), CAP + PANEL_H + 22, note,
                       cls="mono xs muted"))
    return svg(uid, round(W, 1), round(H, 1), "The rig, open and locked",
               "Two renders side by side from the gimbal demo. On the left, at pitch zero, "
               "three hoops sit at right angles: a green one pivoting about the vertical, a "
               "red one and a blue one. A magenta arrow shows the weakest knob combination "
               "still producing real motion. On the right, at pitch 90 degrees, the blue "
               "hoop's axle has swung until it lies exactly along the green one, and the "
               "magenta arrow has vanished because that combination now produces nothing.",
               b)


# ===========================================================================
# Figure 4 — the Jacobian's three columns, collapsing
# ===========================================================================
# ONE CLAIM: det(J) is the volume of the box the three knob-axes span, and it is
# cos(pitch). Three panels at 0, 60 and 90 degrees.
def fig_jacobian():
    W, H = 760, 330
    uid = "f71d"
    b = [cmarker(uid, "bl", BLUE)]
    PANEL_W = W / 3.0

    for stage, pitch in enumerate((0.0, 60.0, 90.0)):
        cx = PANEL_W * (stage + 0.5)
        view = View(cx, 168, 52, az=34.0, el=24.0)
        # YAW = 0, and not because yaw is unimportant. det(J) does not depend on
        # it at all, so the figure is free to choose the yaw that DRAWS best —
        # and at yaw 34° two of the three columns projected onto almost the same
        # screen direction, which made a perfectly fat box look flat at pitch 0
        # and destroyed the whole comparison.
        yaw = 0.0
        b.append(hollow(PANEL_W * stage + 8, 36, PANEL_W - 16, 234, GREY, dash="3 4"))

        f1 = ry(yaw * D)
        f2 = mul(f1, rx(pitch * D))
        cols = [(0.0, 1.0, 0.0), apply(f1, (1, 0, 0)), apply(f2, (0, 0, 1))]

        # The parallelepiped the three columns span. Its VOLUME is |det J|, so
        # shading it is not decoration — it is the determinant, drawn.
        corners = {}
        for m in range(8):
            v = [0.0, 0.0, 0.0]
            for k in range(3):
                if m & (1 << k):
                    v = [v[i] + cols[k][i] for i in range(3)]
            corners[m] = view(tuple(x * 1.20 for x in v))
        faces = ((0, 1, 3, 2), (0, 1, 5, 4), (0, 2, 6, 4),
                 (7, 6, 4, 5), (7, 5, 1, 3), (7, 3, 2, 6))
        for face in faces:
            pts = [corners[m] for m in face]
            d = "M " + " L ".join(f"{x:.2f},{y:.2f}" for x, y in pts) + " Z"
            b.append(f'<path d="{d}" fill="{AMBER}" fill-opacity="0.10" '
                     f'stroke="{AMBER}" stroke-width="0.7"/>')

        for direction, name, colour in zip(cols, ("g", "b", "bl"), (GREEN, RED, BLUE)):
            p0 = view((0, 0, 0))
            p1 = view(tuple(direction[m] * 1.20 for m in range(3)))
            b.append(carrow(p0[0], p0[1], p1[0], p1[1], uid, name, colour, width=2.4))

        det = abs(math.cos(pitch * D))
        b.append(label(cx, 26, f"pitch {pitch:.0f}°", cls="sm"))
        b.append(label(cx, 288, f"|det J| = {det:.3f}", cls="mono sm"))
        note = ("a box: three directions" if stage == 0 else
                "flatter: the same knobs buy less" if stage == 1 else
                "FLAT: yaw and roll are one axis")
        b.append(label(cx, 306, note, cls="xs" + ("" if stage < 2 else " t-bad")))

    b.append(label(W / 2, 322,
                   "green = yaw's axis · red = pitch's axis · blue = roll's axis",
                   cls="xs muted"))
    return svg(uid, W, H, "The three knob axes, losing a dimension",
               "Three panels at pitch zero, sixty and ninety degrees. Each shows the three "
               "axes the three knobs turn about, as arrows from a common origin, with the "
               "parallelepiped they span shaded. At zero the box is fat and the determinant "
               "is one. At sixty it is visibly flattened, determinant one half. At ninety "
               "the green and blue arrows are parallel and the box has collapsed to a flat "
               "sheet with zero volume.", b)


# ===========================================================================
# Figure 5 — what survives at lock: the difference, and nothing else
# ===========================================================================
# ONE CLAIM: at pitch +90 the matrix depends on yaw and roll only through
# yaw - roll, so the whole diagonal of the (yaw, roll) square is ONE orientation.
def fig_collapse():
    W, H = 700, 380
    uid = "f71e"
    b = [cmarker(uid, "bad", RED), cmarker(uid, "ok", GREEN)]
    uid5_unused = None
    X0, Y0, S = 150.0, 60.0, 254.0

    def px(yaw, roll):
        return (X0 + (yaw + 180.0) / 360.0 * S, Y0 + S - (roll + 180.0) / 360.0 * S)

    b.append(label(W / 2, 24, "At pitch = +90°, every pose on a diagonal is the SAME "
                              "orientation", cls="sm"))
    b.append(label(W / 2, 42, "the matrix contains yaw and roll only as (yaw − roll)",
                   cls="xs muted"))
    b.append(frame(X0, Y0, S, S))

    # Iso-lines of yaw - roll. Each is one orientation; the whole square is a
    # one-parameter family wearing a two-parameter costume.
    for k in range(-6, 7):
        diff = k * 60.0
        ends = []
        for yaw in (-180.0, 180.0):
            roll = yaw - diff
            if -180.0 <= roll <= 180.0:
                ends.append((yaw, roll))
        for roll in (-180.0, 180.0):
            yaw = roll + diff
            if -180.0 < yaw < 180.0:
                ends.append((yaw, roll))
        if len(ends) >= 2:
            a, c = px(*ends[0]), px(*ends[1])
            b.append(cline(a[0], a[1], c[0], c[1], GREY, width=0.8, dash="2 5"))

    # One live line, one point on it, and the move that does nothing.
    live = 55.0
    a, c = px(-125.0, -180.0), px(180.0, 125.0)
    b.append(cline(a[0], a[1], c[0], c[1], AMBER, width=2.4))

    # The move ALONG the diagonal is drawn offset from it, because an arrow lying
    # exactly on a line it is meant to be compared with is an arrow nobody sees.
    p = px(20.0, -35.0)
    q = px(67.0, 12.0)
    off = 9.0
    b.append(f'<circle cx="{p[0]:.1f}" cy="{p[1]:.1f}" r="4.5" fill="{AMBER}"/>')
    b.append(f'<circle cx="{q[0]:.1f}" cy="{q[1]:.1f}" r="4.5" fill="none" '
             f'stroke="{AMBER}" stroke-width="1.8"/>')
    b.append(carrow(p[0] + off, p[1] + off, q[0] + off, q[1] + off, uid, "bad", RED,
                    width=2.0))

    r = px(20.0, 35.0)
    b.append(carrow(p[0], p[1], r[0], r[1], uid, "ok", GREEN, width=2.0))

    for g in (-180, -90, 0, 90, 180):
        x, _ = px(g, 0)
        _, y = px(0, g)
        b.append(label(round(x, 1), Y0 + S + 16, f"{g}", cls="mono xs muted"))
        b.append(label(X0 - 10, round(y + 4, 1), f"{g}", cls="mono xs muted", anchor="end"))
    b.append(label(X0 + S / 2, Y0 + S + 34, "yaw (degrees)", cls="xs muted"))
    b.append(f'<text x="{X0 - 42:.1f}" y="{Y0 + S / 2:.1f}" class="xs muted" '
             f'text-anchor="middle" transform="rotate(-90 {X0 - 42:.1f} '
             f'{Y0 + S / 2:.1f})">roll (degrees)</text>')

    # Annotations OUTSIDE the square.
    ax = X0 + S + 20
    b.append(cline(ax, Y0 + 26, ax + 16, Y0 + 26, RED, width=2.2))
    b.append(label(ax + 22, Y0 + 30, "along the line:", cls="xs t-hi", anchor="start"))
    b.append(label(ax, Y0 + 46, "yaw +47°, roll +47°", cls="mono xs", anchor="start"))
    b.append(label(ax, Y0 + 61, "0.0000° of motion", cls="mono xs t-bad", anchor="start"))
    b.append(cline(ax, Y0 + 88, ax + 16, Y0 + 88, GREEN, width=2.2))
    b.append(label(ax + 22, Y0 + 92, "across it:", cls="xs t-hi", anchor="start"))
    b.append(label(ax, Y0 + 108, "yaw 0°, roll +70°", cls="mono xs", anchor="start"))
    b.append(label(ax, Y0 + 123, "real rotation", cls="mono xs t-ok", anchor="start"))
    b.append(label(ax, Y0 + 158, "at pitch 0° the", cls="xs muted", anchor="start"))
    b.append(label(ax, Y0 + 173, "same joint move", cls="xs muted", anchor="start"))
    b.append(label(ax, Y0 + 188, "gives 65.51°", cls="mono xs", anchor="start"))
    b.append(label(ax, Y0 + 214, "at pitch −90° it is", cls="xs muted", anchor="start"))
    b.append(label(ax, Y0 + 229, "the SUM that dies", cls="xs muted", anchor="start"))
    b.append(label(ax, Y0 + 244, "(the other diagonal)", cls="xs muted", anchor="start"))
    return svg(uid, W, H, "The collapse at lock",
               "A square of yaw against roll, both from minus 180 to 180 degrees, crossed by "
               "dashed diagonal lines of constant yaw minus roll. One diagonal is highlighted "
               "in amber with two points on it joined by an arrow: moving along it changes "
               "both knobs by 47 degrees and produces zero rotation. A second arrow leaves "
               "the diagonal and is labelled as real rotation.", b)


# ===========================================================================
# Figure 6 — the conditioning, and the spelling that throws a hundredth away
# ===========================================================================
# ONE CLAIM: the weakest gain falls off as cos(pitch), and the obvious way to
# compute it reaches exactly zero while the pose is still 0.01 degrees out.
def fig_conditioning():
    W, H = 780, 400
    uid = "f71f"
    b = []
    X0, X1 = 92.0, 560.0
    Y0, Y1 = 62.0, 296.0

    # x: log10 of degrees away from lock, from 90 (i.e. pitch 0) down to 0.01.
    def xp(away):
        t = (math.log10(away) - math.log10(0.01)) / (math.log10(90.0) - math.log10(0.01))
        return X1 - t * (X1 - X0)

    # y: log10 of sigma_min, from 1 down to 1e-5.
    def yp(sigma):
        t = (math.log10(max(sigma, 1e-9)) + 5.0) / 5.0
        return Y1 - t * (Y1 - Y0)

    b.append(label(W / 2, 24, "How much turn a unit of knob buys, as the pose approaches "
                              "lock", cls="sm"))
    b.append(label(W / 2, 42, "both axes logarithmic; the curve is σ_min = |cos p| / "
                              "√(1 + |sin p|)", cls="xs muted"))
    b.append(frame(X0, Y0, X1 - X0, Y1 - Y0))

    for away in (90.0, 10.0, 1.0, 0.1, 0.01):
        x = xp(away)
        b.append(rule(x, Y0, x, Y1, cls="grid", width=0.8))
        txt = f"{away:g}°" if away >= 1 else f"{away}°"
        b.append(label(round(x, 1), Y1 + 16, txt, cls="mono xs muted"))
    for e in range(0, -6, -1):
        y = yp(10.0 ** e)
        b.append(rule(X0, y, X1, y, cls="grid", width=0.8))
        b.append(label(X0 - 8, round(y + 4, 1), f"1e{e}" if e else "1",
                       cls="mono xs muted", anchor="end"))

    # The stable curve, sampled densely in Python (a fourth transcription).
    pts = []
    n = 240
    for i in range(n + 1):
        away = 10.0 ** (math.log10(90.0) + (math.log10(0.01) - math.log10(90.0)) * i / n)
        p = (90.0 - away) * D
        sp, cp = abs(math.sin(p)), abs(math.cos(p))
        pts.append((xp(away), yp(cp / math.sqrt(1.0 + sp))))
    b.append(poly(pts, BLUE, width=2.0, close=False))

    # The measured points, from verify_71, on top of the curve they confirm.
    for pitch, _det, naive, stable, brute, _cost in COND:
        away = 90.0 - pitch
        if away < 0.005:
            continue
        x, y = xp(away), yp(brute)
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.4" fill="{AMBER}"/>')

    # The naive spelling, which tracks the curve and then falls off a cliff.
    naive_pts = []
    for pitch, _det, naive, _stable, _brute, _cost in COND:
        away = 90.0 - pitch
        if away < 0.005 or naive <= 0.0:
            continue
        naive_pts.append((xp(away), yp(naive)))
    b.append(poly(naive_pts, RED, width=1.4, dash="4 3", close=False))
    cliff_x, cliff_y = xp(0.01), Y1
    b.append(cline(naive_pts[-1][0], naive_pts[-1][1], cliff_x, cliff_y, RED,
                   width=1.4, dash="4 3"))
    b.append(f'<circle cx="{cliff_x:.1f}" cy="{cliff_y:.1f}" r="4.2" fill="none" '
             f'stroke="{RED}" stroke-width="1.8"/>')

    lx = X1 + 18
    b.append(cline(lx, Y0 + 12, lx + 22, Y0 + 12, BLUE, width=2.0))
    b.append(label(lx, Y0 + 30, "|cos p| / √(1+|sin p|)", cls="mono xs", anchor="start"))
    b.append(label(lx, Y0 + 42, "stable", cls="xs t-ok", anchor="start"))
    b.append(cline(lx, Y0 + 66, lx + 22, Y0 + 66, RED, width=1.4, dash="4 3"))
    b.append(label(lx, Y0 + 84, "√(1 − |sin p|)", cls="mono xs", anchor="start"))
    b.append(label(lx, Y0 + 96, "exactly 0 at 0.01°", cls="xs t-bad", anchor="start"))
    b.append(f'<circle cx="{lx + 11:.1f}" cy="{Y0 + 120:.1f}" r="3.4" fill="{AMBER}"/>')
    b.append(label(lx, Y0 + 136, "measured, by sweeping", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 150, "a million unit vectors", cls="xs muted", anchor="start"))

    b.append(label(W / 2, Y1 + 38, "degrees of pitch remaining before lock",
                   cls="xs muted"))
    b.append(label(W / 2, 358, "at 0.01° from lock a knob turning at 1 rad/s moves the body "
                               "at 0.00012 rad/s — you would need 8,100 rad/s",
                   cls="xs"))
    b.append(label(W / 2, 376, "which is 464,000 degrees per second, for one "
                               "degree-per-second of aircraft", cls="xs muted"))
    return svg(uid, W, H, "Conditioning approaching lock",
               "A log-log plot. The horizontal axis is degrees of pitch remaining before "
               "lock, running from 90 on the left down to 0.01 on the right; the vertical "
               "axis is the weakest gain, from 1 down to 1e-5. A blue curve falls steadily "
               "and straight. Amber dots from a brute-force sweep sit on it. A dashed red "
               "line follows the blue one and then plunges to the floor at 0.01 degrees, "
               "where the naive formula returns exactly zero.", b)


# ===========================================================================
# Figure 7 — interpolation: the detour, and where it gets expensive
# ===========================================================================
# ONE CLAIM: lerping three angles does not lerp the rotation. Left: the nose
# trace of the Euler lerp against the geodesic. Right: the cost at four heights.
def fig_path():
    W, H = 760, 396
    uid = "f71g"
    b = []

    # ---- left: the nose trace, computed here -----------------------------
    view = View(196, 208, 118, az=26.0, el=20.0)
    a = (-70.0 * D, -35.0 * D, 20.0 * D)
    c = (85.0 * D, 55.0 * D, -60.0 * D)
    ra, rc = euler(*a), euler(*c)

    b.append(label(196, 24, "where the nose goes", cls="sm"))
    b.append(label(196, 42, "generic pair, nowhere near lock", cls="xs muted"))

    # The unit sphere, as three faint great circles.
    ident = ((1, 0, 0), (0, 1, 0), (0, 0, 1))
    for u, v in (((1, 0, 0), (0, 1, 0)), ((0, 1, 0), (0, 0, 1)), ((1, 0, 0), (0, 0, 1))):
        b.append(poly(circle_pts(view, ident, u, v, 1.0), GREY, width=0.7, dash="2 4"))

    nose = (0.0, 0.0, -1.0)
    lerp_pts = []
    for i in range(129):
        t = i / 128.0
        e = tuple(a[k] + (c[k] - a[k]) * t for k in range(3))
        lerp_pts.append(view(apply(euler(*e), nose)))
    b.append(poly(lerp_pts, AMBER, width=2.2, close=False))

    # The geodesic: rotate about the fixed axis of rc * ra^T, by t of its angle.
    at = tuple(tuple(ra[j][i] for j in range(3)) for i in range(3))
    rel = mul(rc, at)
    total = rot_angle(ident, rel)
    sk = (rel[2][1] - rel[1][2], rel[0][2] - rel[2][0], rel[1][0] - rel[0][1])
    norm = math.hypot(*sk)
    axis = tuple(s / norm for s in sk)
    geo_pts = []
    for i in range(129):
        ang = total * i / 128.0
        # Rodrigues, used here only to DRAW the reference path. Lesson 7.2
        # derives it; this figure needs the curve, not the derivation.
        ct, st = math.cos(ang), math.sin(ang)
        k = axis
        m = tuple(tuple(ct * (1 if i2 == j else 0)
                        + (1 - ct) * k[i2] * k[j]
                        + st * ((0, -k[2], k[1]), (k[2], 0, -k[0]), (-k[1], k[0], 0))[i2][j]
                        for j in range(3)) for i2 in range(3))
        geo_pts.append(view(apply(mul(m, ra), nose)))
    b.append(poly(geo_pts, BLUE, width=2.0, dash="5 4", close=False))

    for pts, col in ((lerp_pts, AMBER), (geo_pts, BLUE)):
        b.append(f'<circle cx="{pts[0][0]:.1f}" cy="{pts[0][1]:.1f}" r="4" fill="{col}"/>')
        b.append(f'<circle cx="{pts[-1][0]:.1f}" cy="{pts[-1][1]:.1f}" r="4" fill="none" '
                 f'stroke="{col}" stroke-width="1.8"/>')

    ly = 340
    b.append(cline(84, ly, 106, ly, AMBER, width=2.2))
    b.append(label(112, ly + 4, "lerp the three angles: 204.29°", cls="mono xs",
                   anchor="start"))
    b.append(cline(84, ly + 18, 106, ly + 18, BLUE, width=2.0, dash="5 4"))
    b.append(label(112, ly + 22, "the geodesic: 179.05°", cls="mono xs", anchor="start"))
    b.append(label(84, ly + 40, "25.24° of detour, and the speed varies by 1.56×",
                   cls="xs", anchor="start"))

    # ---- right: the same deltas at four heights --------------------------
    PX0, PX1 = 466.0, 676.0
    PY0 = 76.0
    ROW = 52.0
    b.append(label((PX0 + PX1) / 2, 24, "the same three deltas, four heights", cls="sm"))
    b.append(label((PX0 + PX1) / 2, 42, "extra turning performed, per cent", cls="xs muted"))
    MAXPC = 240.0
    for g in (0, 50, 100, 150, 200):
        x = PX0 + (PX1 - PX0) * g / MAXPC
        b.append(rule(x, PY0 - 4, x, PY0 + ROW * 4 - 18, cls="grid", width=0.8))
        b.append(label(round(x, 1), PY0 - 10, f"{g}", cls="xs muted"))
    for i, (top, bottom, _path, _geo, excess, speed) in enumerate(BANDS):
        y = PY0 + ROW * i
        near = (i == 0)
        b.append(label(PX0, y + 10, f"pitch {top:.0f}° → {bottom:.0f}°",
                       cls="mono xs" + (" t-bad" if near else ""), anchor="start"))
        w = (PX1 - PX0) * excess / MAXPC
        b.append(f'<rect x="{PX0:.1f}" y="{y + 16:.1f}" width="{w:.1f}" height="8" '
                 f'fill="{RED if near else BLUE}" rx="1"/>')
        # Inside the bar when the bar is long enough to hold it, outside when it
        # is not. The first draft always put it outside and the 209.5% label ran
        # off the viewBox — which check-page.js catches, and which it should not
        # have had to.
        if w > 60.0:
            b.append(label(PX0 + w - 6, y + 24, f"+{excess:.1f}%", cls="mono xs",
                           anchor="end"))
        else:
            b.append(label(PX0 + w + 6, y + 24, f"+{excess:.1f}%", cls="mono xs",
                           anchor="start"))
    b.append(label(PX0, PY0 + ROW * 4 - 4, "nothing but the pitch band changes;",
                   cls="xs muted", anchor="start"))
    b.append(label(PX0, PY0 + ROW * 4 + 10, "the three deltas are identical.",
                   cls="xs muted", anchor="start"))
    b.append(label(PX0, PY0 + ROW * 4 + 32, "CONTROL: a ONE-knob lerp is already",
                   cls="xs", anchor="start"))
    b.append(label(PX0, PY0 + ROW * 4 + 46, "a geodesic — 0.000% excess.",
                   cls="xs t-ok", anchor="start"))
    return svg(uid, W, H, "Interpolating Euler angles",
               "On the left, a unit sphere with two paths traced on it by the craft's nose "
               "between the same two orientations: an amber curve produced by lerping the "
               "three angles, and a dashed blue geodesic. The amber path bulges away from "
               "the blue one. On the right, a bar chart of extra turning for the same three "
               "angle deltas run at four pitch bands: 209.5 per cent near lock, then 62.5, "
               "22.5 and 26.7 per cent further away.", b)


# ===========================================================================
# Figure 8 — the instrument, and the formula that cannot measure a small angle
# ===========================================================================
# ONE CLAIM: the textbook acos-of-the-trace form returns EXACTLY ZERO for a turn
# of 0.004 degrees, which is the step size every path measurement uses.
def fig_metric():
    W, H = 780, 360
    uid = "f71h"
    b = []
    X0, X1 = 92.0, 556.0
    Y0, Y1 = 66.0, 272.0
    FLOOR = 1e-7

    def xp(a):
        t = (math.log10(a) - math.log10(0.002)) / (math.log10(300.0) - math.log10(0.002))
        return X0 + t * (X1 - X0)

    def yp(err):
        t = (math.log10(max(err, FLOOR)) - math.log10(FLOOR)) / (0.0 - math.log10(FLOOR))
        return Y1 - t * (Y1 - Y0)

    b.append(label(W / 2, 24, "Relative error of two ways to measure the angle between "
                              "two rotations", cls="sm"))
    b.append(label(W / 2, 42, "against a known turn; both axes logarithmic", cls="xs muted"))
    b.append(frame(X0, Y0, X1 - X0, Y1 - Y0))

    for a in (0.01, 0.1, 1.0, 10.0, 100.0):
        x = xp(a)
        b.append(rule(x, Y0, x, Y1, cls="grid", width=0.8))
        b.append(label(round(x, 1), Y1 + 16, f"{a:g}°", cls="mono xs muted"))
    for e in range(0, -8, -1):
        y = yp(10.0 ** e)
        b.append(rule(X0, y, X1, y, cls="grid", width=0.8))
        b.append(label(X0 - 8, round(y + 4, 1), "1" if e == 0 else f"1e{e}",
                       cls="mono xs muted", anchor="end"))

    # THE FLOOR IS DRAWN AND LABELLED rather than silently clamped to. Four of
    # the fourteen measurements came back as EXACTLY zero, which has no place on
    # a log axis; plotting them at an invented 3e-9 would have been a fabricated
    # number in a figure whose whole subject is fabricated numbers.
    b.append(cline(X0, Y1, X1, Y1, GREY, width=1.6))
    b.append(label(X0 + 6, Y1 - 8, "at or below float resolution", cls="xs muted",
                   anchor="start"))

    for idx, colour, name in ((1, BLUE, "atan2"), (2, RED, "acos")):
        pts = sorted(((xp(m[0]), yp(m[idx]), m[idx]) for m in METRIC),
                     key=lambda q: q[0])
        b.append(poly([(x, y) for x, y, _ in pts], colour, width=1.9, close=False))
        for x, y, value in pts:
            if value <= FLOOR:
                b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.4" fill="none" '
                         f'stroke="{colour}" stroke-width="1.6"/>')
            else:
                b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.2" fill="{colour}"/>')

    bad = (xp(0.004), yp(1.0))
    b.append(f'<circle cx="{bad[0]:.1f}" cy="{bad[1]:.1f}" r="7" fill="none" '
             f'stroke="{RED}" stroke-width="1.8"/>')
    # 34 below rather than 16: at 16 the label sat on the red curve leaving that
    # very point, which is the one stroke it must not touch.
    b.append(label(bad[0] + 14, bad[1] + 34, "returns 0", cls="xs t-bad", anchor="start"))

    lx = X1 + 16
    b.append(cline(lx, Y0 + 12, lx + 20, Y0 + 12, BLUE, width=1.9))
    b.append(label(lx, Y0 + 28, "atan2(|R−Rᵀ|/2,", cls="mono xs", anchor="start"))
    b.append(label(lx, Y0 + 41, "  (tr R − 1)/2)", cls="mono xs", anchor="start"))
    b.append(label(lx, Y0 + 57, "what we ship", cls="xs t-ok", anchor="start"))
    b.append(cline(lx, Y0 + 86, lx + 20, Y0 + 86, RED, width=1.9))
    b.append(label(lx, Y0 + 102, "acos((tr R − 1)/2)", cls="mono xs", anchor="start"))
    b.append(label(lx, Y0 + 116, "every textbook", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 132, "100% wrong at", cls="xs t-bad", anchor="start"))
    b.append(label(lx, Y0 + 146, "0.004°", cls="mono xs t-bad", anchor="start"))
    b.append(f'<circle cx="{lx + 10:.1f}" cy="{Y0 + 172:.1f}" r="3.4" fill="none" '
             f'stroke="{GREY}" stroke-width="1.6"/>')
    b.append(label(lx, Y0 + 190, "hollow: measured", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 204, "as exactly zero", cls="xs muted", anchor="start"))

    b.append(label(W / 2, Y1 + 38, "the angle actually turned", cls="xs muted"))
    b.append(label(W / 2, 326, "a 2,048-step path across 120° takes 0.059° steps, so the "
                               "red curve is measuring nothing there", cls="xs"))
    b.append(label(W / 2, 344, "the two formulas are algebraically identical, and one of "
                               "them cannot do the job", cls="xs muted"))
    return svg(uid, W, H, "Two spellings of one formula",
               "A log-log plot of relative error against the true angle, with a floor line "
               "marked at float resolution. The blue atan2 form sits on or just above the "
               "floor across the whole range. The red acos-of-the-trace form leaves the "
               "floor at ten degrees and climbs steeply, reaching a relative error of one — "
               "meaning it returned zero — at a true angle of 0.004 degrees.", b)


FIGURES = [
    ("l71_fig1.svg", fig_chain),
    # PAGE ORDER, not writing order. The rig is what the reader meets second —
    # the artifact before the theory (CLAUDE.md pedagogy §5) — so it is fig 2,
    # even though the conventions chart was written first. 6.18 shipped a first
    # draft numbered by writing order and check-page.js reported all seven.
    ("l71_fig2.svg", fig_rig),
    ("l71_fig3.svg", fig_orders),
    ("l71_fig4.svg", fig_jacobian),
    ("l71_fig5.svg", fig_collapse),
    ("l71_fig6.svg", fig_conditioning),
    ("l71_fig7.svg", fig_path),
    ("l71_fig8.svg", fig_metric),
]

if __name__ == "__main__":
    for name, fn in FIGURES:
        path = os.path.join(OUT, name)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(fn())
        print(f"wrote {path}")
