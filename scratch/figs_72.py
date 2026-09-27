#!/usr/bin/env python3
"""scratch/figs_72.py — Lesson 7.2's diagrams.

Same rules as 5.1-7.1's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - ~5.2 units per character for `xs`, ~6.0 for `sm`
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR (6.10's trap)
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed

Every number below comes from verify_72's output (scratch/verify_72.log) or from
the gimbal demo's own receipt, except where a figure's geometry is recomputed
here in Python. The rotation code is imported from figs_71, which makes it the
THIRD independent transcription of this engine's conventions — so a figure that
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
from figs_71 import View, euler, poly, frame, cmarker, carrow, D    # noqa: E402

OUT = "scratch"

# The teal the demo draws a rotation axis in, so the page and the render agree.
TEAL = "#60ded0"


# ===========================================================================
# Measured — every figure's data, with the line of verify_72.log it came from
# ===========================================================================

# §C.2 — an axis error of phi produces 2 sin(theta/2) * phi of orientation error
AMPLIFY = [  # (theta deg, measured ratio, 2 sin(theta/2))
    (0.5, 0.00873, 0.00873), (5.0, 0.08724, 0.08724), (30.0, 0.51765, 0.51764),
    (90.0, 1.41420, 1.41421), (150.0, 1.93185, 1.93185), (179.0, 1.99988, 1.99992),
]

# §C.3 — the two routes' worst axis error, on matrices carrying 1e-7 of
# ABSOLUTE error, 4,000 random axes per probe.
ROUTES = [  # (theta deg, skew route deg, symmetric route deg)
    (30.0, 1.749e-05, 1.191e-04), (60.0, 1.170e-05, 3.609e-05),
    (90.0, 1.054e-05, 1.574e-05), (110.0, 1.271e-05, 1.258e-05),
    (120.0, 1.288e-05, 1.491e-05), (130.0, 1.552e-05, 1.196e-05),
    (150.0, 1.942e-05, 1.255e-05), (175.0, 1.481e-04, 1.290e-05),
    (179.9, 6.892e-03, 1.211e-05),
]

# §C.5 — the identity end. The last two rows are the PLACEHOLDER axis, not a
# measured degradation, and the figure has to say so or it is lying.
IDENTITY = [  # (theta rad, axis error deg, orientation error deg, placeholder?)
    (1e-2, 0.0005, 5.182e-06, False), (1e-3, 0.0052, 5.176e-06, False),
    (1e-4, 0.0515, 5.176e-06, False), (1e-5, 0.5150, 5.176e-06, False),
    (1e-6, 78.2988, 7.280e-05, True), (1e-8, 78.2988, 5.174e-06, True),
]

# §E.3 — 7.1's four pitch bands, now with the geodesic beside the Euler lerp.
BANDS = [  # (from deg, to deg, euler excess %, slerp excess %)
    (87.0, 30.0, 209.5, 0.0), (47.0, -10.0, 62.5, 0.0),
    (20.0, -37.0, 22.5, 0.0), (0.0, -57.0, 26.7, 0.0),
]

# §A.4 — Euler's theorem fails in 4-D, and the determinant says by how much.
DET_4D = 0.25840


# ===========================================================================
# Figure 1 — every rotation leaves a line alone  (§3, §4)
# ===========================================================================
# ONE CLAIM: a rotation of 3-space fixes a whole LINE, and everything else moves
# on a circle around it. Two panels: a generic rotation with its axis found, and
# the same picture with the vectors that prove it.
def fig_fixed_line():
    uid = "f72a"
    W, H = 760, 404
    b = []

    axis = (0.40, 0.82, 0.41)
    n = math.sqrt(sum(c * c for c in axis))
    axis = tuple(c / n for c in axis)
    theta = 58.0 * D

    def turn(v):
        c, s = math.cos(theta), math.sin(theta)
        dot = sum(axis[i] * v[i] for i in range(3))
        crs = (axis[1] * v[2] - axis[2] * v[1],
               axis[2] * v[0] - axis[0] * v[2],
               axis[0] * v[1] - axis[1] * v[0])
        return tuple(v[i] * c + crs[i] * s + axis[i] * dot * (1 - c) for i in range(3))

    b.append(cmarker(uid, "teal", TEAL))
    b.append(cmarker(uid, "amber", AMBER))
    b.append(cmarker(uid, "blue", BLUE))

    for panel, (px, title, sub) in enumerate((
            (0, "before", "three vectors, and a line"),
            (1, "after a 58° turn", "one of them has not moved"))):
        cx = 190 + panel * 380
        view = View(cx, 200, 74)
        b.append(label(cx, 26, title, cls="sm"))
        b.append(label(cx, 43, sub, cls="xs muted"))

        # The axis, drawn both ways: an axis is a LINE, not a direction.
        a0 = view(tuple(-c * 2.0 for c in axis))
        a1 = view(tuple(c * 2.0 for c in axis))
        b.append(cline(a0[0], a0[1], a1[0], a1[1], TEAL, width=2.0))

        # A circle of points around the axis, showing the orbit everything
        # off the axis is confined to.
        perp = (-axis[1], axis[0], 0.0)
        pn = math.sqrt(sum(c * c for c in perp))
        perp = tuple(c / pn for c in perp)
        perp2 = (axis[1] * perp[2] - axis[2] * perp[1],
                 axis[2] * perp[0] - axis[0] * perp[2],
                 axis[0] * perp[1] - axis[1] * perp[0])
        ring = []
        for i in range(97):
            a = 2 * math.pi * i / 96
            ring.append(view(tuple((perp[k] * math.cos(a) + perp2[k] * math.sin(a)) * 1.15
                                   + axis[k] * 0.62 for k in range(3))))
        b.append(poly(ring, GREY, width=1.0, dash="3 4"))

        origin = view((0, 0, 0))
        samples = ((axis, TEAL, "teal", "on the axis"),
                   ((1.0, 0.0, 0.0), AMBER, "amber", None),
                   ((0.0, 0.0, -1.0), BLUE, "blue", None))
        for v, colour, marker, _ in samples:
            w = v if panel == 0 else turn(v)
            tip = view(tuple(c * 1.45 for c in w))
            b.append(carrow(origin[0], origin[1], tip[0], tip[1], uid, marker, colour,
                            width=2.0))

        if panel == 1:
            # Ghosts of where the two moving vectors used to be, AND the arc each
            # travelled. The ghost alone says "it was there"; the arc says "and
            # this is how it got here", which is what the dashed orbit circle
            # claims in general.
            for v, colour, _, _ in samples[1:]:
                g = view(tuple(c * 1.45 for c in v))
                b.append(cline(origin[0], origin[1], g[0], g[1], colour,
                               width=1.0, dash="2 4"))
                sweep = []
                for k in range(25):
                    a = theta * k / 24.0
                    c2, s2 = math.cos(a), math.sin(a)
                    dot2 = sum(axis[i] * v[i] for i in range(3))
                    crs = (axis[1] * v[2] - axis[2] * v[1],
                           axis[2] * v[0] - axis[0] * v[2],
                           axis[0] * v[1] - axis[1] * v[0])
                    w = tuple((v[i] * c2 + crs[i] * s2 + axis[i] * dot2 * (1 - c2)) * 1.45
                              for i in range(3))
                    sweep.append(view(w))
                b.append(poly(sweep, colour, width=1.2, close=False))

    tip = View(190, 200, 74)(tuple(c * 1.45 for c in axis))
    b.append(label(round(tip[0]) + 8, round(tip[1]) - 10, "the axis", cls="xs", anchor="start"))
    b.append(label(W / 2, H - 42,
                   "the dashed circle is the orbit: everything off the axis travels on one",
                   cls="xs muted"))
    b.append(label(W / 2, H - 22,
                   "on the right, faint lines are where the two moving vectors were and the "
                   "curves are how they got here",
                   cls="xs muted"))
    return svg(uid, W, H, "A rotation fixes a line",
               "Two panels showing the same three vectors before and after a 58 degree "
               "rotation. A teal line runs through the origin in both. The teal vector lying "
               "along that line is in exactly the same place in both panels; the amber and "
               "blue vectors have swung round it, with faint dashed lines marking where they "
               "started. A dashed grey circle around the teal line shows the orbit that "
               "everything off the axis is confined to.", b)


# ===========================================================================
# Figure 2 — why three dimensions, and not four  (§4.3)
# ===========================================================================
# ONE CLAIM: a rotation's eigenvalues sit on the unit circle in conjugate pairs,
# so an ODD dimension always leaves one real eigenvalue over, and a real
# eigenvalue of a rotation can only be +1 or -1. Determinant +1 forces +1.
def fig_odd():
    uid = "f72b"
    W, H = 760, 396
    R = 86.0
    b = [cmarker(uid, "teal", TEAL)]

    for panel, (title, angles, verdict, vcls, note) in enumerate((
            ("3-D: three eigenvalues", [58.0], "one is left over, and it must be +1",
             "t-ok", "det(R − I) = 0  →  a fixed line exists"),
            ("4-D: four eigenvalues", [35.0, 50.0], "they pair up perfectly. none is left over",
             "t-bad", "det(R − I) = %.5f  →  nothing is fixed" % DET_4D))):
        cx = 192 + panel * 376
        cy = 168
        b.append(label(cx, 26, title, cls="sm"))
        b.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="none" stroke="{GREY}"'
                 f' stroke-width="1.1" stroke-dasharray="3 4"/>')
        b.append(rule(cx - R - 18, cy, cx + R + 18, cy))
        b.append(rule(cx, cy - R - 18, cx, cy + R + 18))
        b.append(label(cx + R + 24, cy + 4, "Re", cls="xs muted", anchor="start"))
        b.append(label(cx, cy - R - 24, "Im", cls="xs muted"))

        pts = []
        for a in angles:
            pts.append((math.cos(a * D), math.sin(a * D), BLUE, "e^{iθ}"))
            pts.append((math.cos(a * D), -math.sin(a * D), BLUE, None))
        if panel == 0:
            pts.append((1.0, 0.0, TEAL, None))

        for (x, y, colour, _) in pts:
            px, py = cx + x * R, cy - y * R
            b.append(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="5.0" fill="{colour}"/>')

        if panel == 0:
            b.append(label(cx + R + 6, cy + 20, "λ = 1", cls="xs", anchor="start"))
            b.append(cline(cx + R, cy, cx + R + 4, cy + 12, TEAL, width=1.0))

        b.append(label(cx, cy + R + 40, verdict, cls=f"xs {vcls}"))
        b.append(label(cx, cy + R + 58, note, cls="mono xs muted"))

    b.append(label(W / 2, H - 46,
                   "a real matrix's eigenvalues come in conjugate PAIRS, and a rotation's all "
                   "sit on the unit circle",
                   cls="xs muted"))
    b.append(label(W / 2, H - 24,
                   "an odd dimension cannot pair them all up — and a real one on the unit "
                   "circle is ±1, which det = +1 settles",
                   cls="xs muted"))
    return svg(uid, W, H, "Why three dimensions and not four",
               "Two complex-plane panels, each with a dashed unit circle. On the left, three "
               "eigenvalues of a 3-D rotation: a conjugate pair off the real axis and one left "
               "over, sitting at plus one on the real axis and marked in teal. On the right, "
               "four eigenvalues of a 4-D double rotation, forming two conjugate pairs with "
               "none left over and none at plus one. Underneath each, the measured determinant "
               "of R minus the identity: zero on the left, 0.25840 on the right.", b)


# ===========================================================================
# Figure 3 — Rodrigues, derived from the picture  (§5.1)
# ===========================================================================
# ONE CLAIM: split v along and across the axis; the along part does not move and
# the across part turns in a plane whose two axes are v_perp and n x v.
def fig_rodrigues():
    uid = "f72c"
    W, H = 760, 430
    view = View(300, 236, 116)
    b = [cmarker(uid, "teal", TEAL), cmarker(uid, "amber", AMBER),
         cmarker(uid, "blue", BLUE), cmarker(uid, "purple", PURPLE)]

    axis = (0.0, 1.0, 0.0)
    v = (1.16, 0.86, 0.30)
    theta = 62.0 * D

    dot = sum(axis[i] * v[i] for i in range(3))
    v_par = tuple(axis[i] * dot for i in range(3))
    v_perp = tuple(v[i] - v_par[i] for i in range(3))
    n_cross_v = (axis[1] * v[2] - axis[2] * v[1],
                 axis[2] * v[0] - axis[0] * v[2],
                 axis[0] * v[1] - axis[1] * v[0])
    c, s = math.cos(theta), math.sin(theta)
    v_perp_rot = tuple(v_perp[i] * c + n_cross_v[i] * s for i in range(3))
    v_rot = tuple(v_perp_rot[i] + v_par[i] for i in range(3))

    origin = view((0, 0, 0))

    # The disc the perpendicular part turns in, drawn at v_perp's radius.
    radius = math.sqrt(sum(x * x for x in v_perp))
    disc = []
    u1 = tuple(x / radius for x in v_perp)
    u2 = tuple(x / radius for x in n_cross_v)
    for i in range(97):
        a = 2 * math.pi * i / 96
        disc.append(view(tuple((u1[k] * math.cos(a) + u2[k] * math.sin(a)) * radius
                               + v_par[k] for k in range(3))))
    b.append(poly(disc, GREY, width=1.0, dash="3 4"))

    # The axis, and the parallel component sitting on it.
    a0 = view((0, -0.5, 0))
    a1 = view((0, 1.85, 0))
    b.append(cline(a0[0], a0[1], a1[0], a1[1], TEAL, width=2.0))
    par_tip = view(v_par)
    b.append(carrow(origin[0], origin[1], par_tip[0], par_tip[1], uid, "teal", TEAL, width=2.4))

    # v, its perpendicular part, and n x v.
    v_tip = view(v)
    b.append(carrow(origin[0], origin[1], v_tip[0], v_tip[1], uid, "amber", AMBER, width=2.2))
    perp_from = view(v_par)
    perp_to = view(tuple(v_par[i] + v_perp[i] for i in range(3)))
    b.append(carrow(perp_from[0], perp_from[1], perp_to[0], perp_to[1], uid, "amber",
                    AMBER, width=1.6, dash="4 3"))
    cross_to = view(tuple(v_par[i] + n_cross_v[i] for i in range(3)))
    b.append(carrow(perp_from[0], perp_from[1], cross_to[0], cross_to[1], uid, "purple",
                    PURPLE, width=1.8))

    # The arc, and the answer.
    arc = []
    for i in range(41):
        a = theta * i / 40
        arc.append(view(tuple(v_perp[k] * math.cos(a) + n_cross_v[k] * math.sin(a)
                              + v_par[k] for k in range(3))))
    b.append(poly(arc, BLUE, width=1.3, close=False))
    rot_tip = view(v_rot)
    b.append(carrow(origin[0], origin[1], rot_tip[0], rot_tip[1], uid, "blue", BLUE, width=2.4))

    # Labels, placed off the tips they name.
    b.append(label(round(v_tip[0]) + 8, round(v_tip[1]) - 6, "v", cls="sm", anchor="start"))
    b.append(label(round(rot_tip[0]), round(rot_tip[1]) - 12, "v′", cls="sm"))
    b.append(label(round(a1[0]), round(a1[1]) - 10, "n", cls="sm"))
    b.append(label(round(par_tip[0]) - 10, round(par_tip[1]) + 2, "v∥ = (n·v) n",
                   cls="xs", anchor="end"))
    b.append(label(round(perp_to[0]) + 8, round(perp_to[1]) + 14, "v⊥", cls="xs", anchor="start"))
    b.append(label(round(cross_to[0]) - 6, round(cross_to[1]) + 16, "n × v",
                   cls="xs", anchor="end"))

    # The formula, read off the picture, as three named pieces.
    bx, by = 560, 96
    b.append(box(bx - 14, by - 26, 190, 150))
    b.append(label(bx, by - 6, "read off the picture", cls="xs muted", anchor="start"))
    for i, (piece, gloss) in enumerate((
            ("v cos θ", "most of it leans over"),
            ("+ (n × v) sin θ", "and swings sideways"),
            ("+ n (n·v)(1 − cos θ)", "restoring the along-axis part"))):
        b.append(label(bx, by + 20 + i * 38, piece, cls="mono xs", anchor="start"))
        b.append(label(bx, by + 34 + i * 38, gloss, cls="xs muted", anchor="start"))

    b.append(label(W / 2, H - 40,
                   "v⊥ and n × v are perpendicular and the SAME LENGTH, so they are a pair of "
                   "axes for the disc",
                   cls="xs muted"))
    b.append(label(W / 2, H - 20,
                   "which turns the 3-D problem into the 2-D rotation of Lesson 1.7",
                   cls="xs muted"))
    return svg(uid, W, H, "Rodrigues' formula, from the geometry",
               "A vector v is drawn from the origin beside a teal rotation axis n. A teal "
               "arrow along the axis marks the component of v parallel to it. A dashed grey "
               "disc, centred on the tip of that parallel component and perpendicular to the "
               "axis, contains the rest. Inside the disc, a dashed amber arrow is the "
               "perpendicular component and a purple arrow is n cross v, at right angles to it "
               "and the same length. A blue arc sweeps 62 degrees from one to the rotated "
               "result, drawn as a blue arrow. A panel at the right names the three terms of "
               "the formula the picture produces.", b)


# ===========================================================================
# Figure 4 — the axis, in the demo  (§6)
# ===========================================================================
# ONE CLAIM: the teal line is not a diagram, it is the axis the extraction found
# in the pose the three knobs built, drawn through the craft that pose orients.
def fig_axis_render():
    uid = "f72d"
    PALETTE = [hexrgb(c) for c in (RED, GREEN, BLUE, AMBER, PURPLE, TEAL,
                                   "#ced2dc", "#7896e2", "#e2846e")]
    CELL = 3
    PX = 1.5
    CROP = (150, 10, 810, 530)
    gw = (CROP[2] - CROP[0]) // CELL
    gh = (CROP[3] - CROP[1]) // CELL

    PAD = 16.0
    CAP = 44.0
    PANEL_W = gw * PX
    PANEL_H = gh * PX
    W = PAD * 3 + PANEL_W * 2
    H = CAP + PANEL_H + 58

    b = []
    for i, (path, title, sub, note) in enumerate((
            ("l72_axis_rings.ppm", "three knobs", "yaw 25°, pitch 40°, roll −15°",
             "the rings are the three turns that built it"),
            ("l72_axis.ppm", "one turn", "51.7083° about (+0.698, +0.562, −0.443)",
             "Euler product vs Rodrigues: 0.000003°"))):
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
    b.append(label(W / 2, CAP + PANEL_H + 46,
                   "the teal line is the same line in both panels — hiding the rings does not "
                   "move it",
                   cls="xs muted"))
    return svg(uid, round(W, 1), round(H, 1), "The axis, in the demo",
               "Two renders from the gimbal demo at the same pose. On the left, the three "
               "coloured gimbal rings that the three Euler knobs correspond to, with a teal "
               "line running straight through the aircraft. On the right, the same pose with "
               "the rings hidden, leaving the aircraft and the teal line — the single axis the "
               "whole rotation turns about. The caption underneath gives that axis and angle, "
               "and the measured disagreement between building the pose from three angles and "
               "building it from one turn: three millionths of a degree.", b)


# ===========================================================================
# Figure 5 — what an axis error actually costs  (§8.3)
# ===========================================================================
# ONE CLAIM: orientation error = 2 sin(theta/2) x axis error. It is ZERO at the
# identity, which is why the hole there is free, and MAXIMAL at a half-turn,
# which is why the hole there is not.
def fig_amplify():
    uid = "f72e"
    W, H = 760, 428
    X0, Y0, PW, PH = 92, 56, 500, 250
    b = [cmarker(uid, "teal", TEAL)]

    def px(theta_deg):
        return X0 + PW * theta_deg / 180.0

    def py(ratio):
        return Y0 + PH - PH * ratio / 2.1

    b.append(frame(X0, Y0, PW, PH))
    for r in (0.0, 0.5, 1.0, 1.5, 2.0):
        y = py(r)
        b.append(rule(X0, y, X0 + PW, y))
        b.append(label(X0 - 10, y + 4, f"{r:.1f}", cls="xs muted", anchor="end"))
    for t in (0, 45, 90, 135, 180):
        x = px(t)
        b.append(rule(x, Y0, x, Y0 + PH))
        b.append(label(x, Y0 + PH + 18, f"{t}°", cls="xs muted"))

    curve = [(px(t), py(2.0 * math.sin(t * D / 2.0))) for t in range(0, 181)]
    b.append(poly(curve, TEAL, width=2.0, close=False))
    for (t, measured, _) in AMPLIFY:
        x, y = px(t), py(measured)
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="4.2" fill="{AMBER}"/>')

    b.append(label(X0 + PW / 2, Y0 + PH + 38, "θ — how big the turn is", cls="xs muted"))
    b.append(label(X0 - 58, Y0 + PH / 2, "cost", cls="xs muted"))
    b.append(label(X0 - 58, Y0 + PH / 2 + 16, "factor", cls="xs muted"))

    # The two ends, annotated IN THE EMPTY CORNERS. The first draft put each
    # note beside the end of the curve it names, which is exactly where the curve
    # is — check-page.js reported the left one sitting on a path at 1,280 and not
    # at 390, the same width-dependence Lesson 7.1 hit. A monotone curve from
    # bottom-left to top-right leaves the top-left and bottom-right corners
    # empty; a leader line does the pointing that proximity was doing.
    b.append(cline(X0 + 20, Y0 + 40, px(4), py(2.0 * math.sin(2.0 * D)) - 4,
                   GREY, width=1.0, dash="2 3"))
    b.append(label(X0 + 16, Y0 + 32, "0 — an axis error here costs nothing",
                   cls="xs t-ok", anchor="start"))
    b.append(cline(X0 + PW - 20, Y0 + PH - 40, px(174), py(2.0 * math.sin(87.0 * D)) + 6,
                   GREY, width=1.0, dash="2 3"))
    b.append(label(X0 + PW - 16, Y0 + PH - 30, "2 — and here it costs double",
                   cls="xs t-bad", anchor="end"))

    lx = X0 + PW + 26
    b.append(box(lx - 8, Y0 + 6, 148, 108))
    b.append(label(lx, Y0 + 26, "teal: 2 sin(θ/2)", cls="xs", anchor="start"))
    b.append(label(lx, Y0 + 44, "amber: measured,", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 60, "a 0.001 rad tilt", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 84, "worst relative", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 100, "error 2.3e-05", cls="mono xs muted", anchor="start"))

    b.append(label(W / 2, H - 46,
                   "so the extraction's two hard ends are not equally dangerous: the one at "
                   "θ = 0 is multiplied by zero",
                   cls="xs muted"))
    b.append(label(W / 2, H - 20,
                   "and the one at θ = 180° is multiplied by two, exactly where the axis is "
                   "hardest to find",
                   cls="xs muted"))
    return svg(uid, W, H, "What an axis error costs",
               "A plot of the factor by which an error in a rotation's axis is magnified into "
               "an error in the resulting orientation, against the size of the turn. The teal "
               "curve is two times the sine of half the angle: it starts at zero, passes "
               "through 1.41 at ninety degrees, and reaches two at a half turn. Amber dots "
               "are measured values from six probes, lying on the curve to within two parts in "
               "a hundred thousand.", b)


# ===========================================================================
# Figure 6 — the two routes, and where to switch  (§8.4)
# ===========================================================================
# ONE CLAIM: neither route is good everywhere, they run level across a wide band,
# and 120 degrees is in the middle of that band.
def fig_routes():
    uid = "f72f"
    W, H = 760, 440
    X0, Y0, PW, PH = 92, 58, 480, 250
    b = []

    lo, hi = -5.4, -1.9   # log10 of the axis error in degrees

    def px(theta_deg):
        return X0 + PW * (theta_deg - 20.0) / 162.0

    def py(err):
        return Y0 + PH - PH * (math.log10(err) - lo) / (hi - lo)

    b.append(frame(X0, Y0, PW, PH))
    for e in range(-5, -1):
        y = py(10.0 ** e)
        b.append(rule(X0, y, X0 + PW, y))
        b.append(label(X0 - 10, y + 4, f"1e{e}", cls="mono xs muted", anchor="end"))
    for t in (30, 60, 90, 120, 150, 180):
        x = px(t)
        b.append(rule(x, Y0, x, Y0 + PH))
        b.append(label(x, Y0 + PH + 18, f"{t}°", cls="xs muted"))

    # The band where the two run level, shaded, OUTSIDE-in: a rect under the
    # curves rather than a label over them.
    b.append(box(px(95), Y0, px(142) - px(95), PH, GREY, opacity=0.10, rx=0, width=0.0))

    skew = [(px(t), py(s)) for (t, s, _) in ROUTES]
    sym = [(px(t), py(m)) for (t, _, m) in ROUTES]
    b.append(poly(skew, AMBER, width=2.0, close=False))
    b.append(poly(sym, BLUE, width=2.0, close=False))
    for (x, y) in skew:
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.6" fill="{AMBER}"/>')
    for (x, y) in sym:
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.6" fill="{BLUE}"/>')

    xt = px(120)
    b.append(cline(xt, Y0 - 14, xt, Y0 + PH, TEAL, width=1.6, dash="5 4"))
    b.append(label(xt, Y0 - 20, "120°", cls="xs"))

    b.append(label(X0 + PW / 2, Y0 + PH + 38, "θ — the size of the turn being extracted",
                   cls="xs muted"))

    lx = X0 + PW + 22
    b.append(box(lx - 8, Y0 + 4, 158, 132))
    b.append(cline(lx, Y0 + 22, lx + 20, Y0 + 22, AMBER, width=2.4))
    b.append(label(lx + 26, Y0 + 26, "R − Rᵀ", cls="xs", anchor="start"))
    b.append(cline(lx, Y0 + 44, lx + 20, Y0 + 44, BLUE, width=2.4))
    b.append(label(lx + 26, Y0 + 48, "R + Rᵀ", cls="xs", anchor="start"))
    b.append(label(lx, Y0 + 74, "worst axis error", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 90, "over 4,000 axes,", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 106, "matrices carrying", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 122, "1e-7 absolute", cls="mono xs muted", anchor="start"))

    b.append(label(W / 2, H - 70,
                   "shaded: the band where the two are within 25% of each other and the winner "
                   "flips from probe to probe",
                   cls="xs muted"))
    b.append(label(W / 2, H - 47,
                   "outside it the choice is decisive — R − Rᵀ is 6.8× better at 30°, "
                   "R + Rᵀ is 569× better at 179.9°",
                   cls="xs muted"))
    b.append(label(W / 2, H - 20,
                   "the threshold is derived from tan(θ/2) = √3 and lands in the middle of the "
                   "band, which is the best place for one to be",
                   cls="xs muted"))
    return svg(uid, W, H, "The two extraction routes",
               "A log plot of worst axis error against the size of the turn, for two ways of "
               "recovering a rotation's axis. The amber curve, which reads the axis off the "
               "antisymmetric part, is lowest at small angles and climbs steeply past 150 "
               "degrees to seven thousandths of a degree at 179.9. The blue curve, which reads "
               "it off the symmetric part, starts high at 30 degrees and then stays flat at "
               "about a hundred-thousandth of a degree all the way to a half turn. They cross "
               "in a shaded band from about 95 to 142 degrees, and a dashed teal line marks "
               "the 120 degree threshold in the middle of it.", b)


# ===========================================================================
# Figure 7 — the identity end, where the axis dies and nothing happens (§8.5)
# ===========================================================================
# ONE CLAIM: two columns of the same table move in opposite directions. The axis
# error grows by a factor of ten per decade; the orientation error does not move.
def fig_identity():
    uid = "f72g"
    W, H = 760, 404
    X0, Y0, PW, PH = 110, 56, 460, 240
    b = []

    lo, hi = -6.4, 2.4   # log10 degrees

    def px(i):
        return X0 + PW * i / 5.0

    def py(v):
        return Y0 + PH - PH * (math.log10(v) - lo) / (hi - lo)

    b.append(frame(X0, Y0, PW, PH))
    for e in range(-6, 3, 2):
        y = py(10.0 ** e)
        b.append(rule(X0, y, X0 + PW, y))
        b.append(label(X0 - 10, y + 4, f"1e{e}°", cls="mono xs muted", anchor="end"))
    for i, (theta, _, _, _) in enumerate(IDENTITY):
        x = px(i)
        b.append(rule(x, Y0, x, Y0 + PH))
        b.append(label(x, Y0 + PH + 18, f"1e{round(math.log10(theta))}", cls="mono xs muted"))

    # The threshold sits between the 1e-5 and 1e-6 probes; the shaded half is
    # where the routine has stopped trying and returned a placeholder.
    xt = (px(3) + px(4)) / 2.0
    b.append(box(xt, Y0, X0 + PW - xt, PH, GREY, opacity=0.10, rx=0, width=0.0))
    b.append(cline(xt, Y0 - 14, xt, Y0 + PH, TEAL, width=1.6, dash="5 4"))
    b.append(label(xt, Y0 - 20, "k_axis_angle_identity_angle = 1e-5", cls="xs"))

    solid_axis = [(px(i), py(a)) for i, (_, a, _, ph) in enumerate(IDENTITY) if not ph]
    ghost_axis = [(px(i), py(a)) for i, (_, a, _, ph) in enumerate(IDENTITY) if ph]
    orient = [(px(i), py(o)) for i, (_, _, o, _) in enumerate(IDENTITY)]

    b.append(poly(solid_axis, AMBER, width=2.0, close=False))
    for (x, y) in solid_axis:
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.8" fill="{AMBER}"/>')
    for (x, y) in ghost_axis:
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.8" fill="none" stroke="{AMBER}"'
                 f' stroke-width="1.6"/>')
    b.append(poly(orient, BLUE, width=2.0, close=False))
    for (x, y) in orient:
        b.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.8" fill="{BLUE}"/>')

    b.append(label(X0 + PW / 2, Y0 + PH + 38, "θ — the size of the turn, in radians",
                   cls="xs muted"))

    lx = X0 + PW + 22
    b.append(box(lx - 8, Y0 + 4, 146, 136))
    b.append(cline(lx, Y0 + 22, lx + 18, Y0 + 22, AMBER, width=2.4))
    b.append(label(lx + 24, Y0 + 26, "axis error", cls="xs", anchor="start"))
    b.append(cline(lx, Y0 + 44, lx + 18, Y0 + 44, BLUE, width=2.4))
    b.append(label(lx + 24, Y0 + 48, "orientation", cls="xs", anchor="start"))
    b.append(label(lx, Y0 + 74, "hollow amber:", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 90, "the placeholder", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 106, "axis, not a", cls="xs muted", anchor="start"))
    b.append(label(lx, Y0 + 122, "measurement", cls="xs muted", anchor="start"))

    b.append(label(W / 2, H - 46,
                   "the amber line climbs a decade per decade and ends 78° wrong; the blue one "
                   "is flat at five millionths of a degree",
                   cls="xs muted"))
    b.append(label(W / 2, H - 20,
                   "same runs, same matrices. the axis is destroyed and the orientation it "
                   "names is not.",
                   cls="xs muted"))
    return svg(uid, W, H, "The identity end",
               "A log plot with two curves against the size of the turn, from a hundredth of a "
               "radian down to ten to the minus eight. The amber curve, the error in the "
               "recovered axis, rises steadily by a factor of ten for every factor of ten the "
               "angle shrinks, reaching half a degree at the threshold and then jumping to 78 "
               "degrees where the routine returns a placeholder instead — those two points are "
               "drawn hollow. The blue curve, the error in the orientation those same numbers "
               "rebuild, is flat across the whole range at about five millionths of a degree.",
               b)


# ===========================================================================
# Figure 8 — the blend, in the demo  (§9)
# ===========================================================================
# ONE CLAIM: same start, same end, two routes. The geodesic turns rigidly about
# one line that never moves; the Euler lerp does not.
def fig_blend_render():
    uid = "f72h"
    PALETTE = [hexrgb(c) for c in (RED, GREEN, BLUE, AMBER, PURPLE, TEAL,
                                   "#ced2dc", "#7ebcf8", "#eca856", "#e2846e")]
    CELL = 3
    PX = 1.5
    CROP = (150, 10, 810, 530)
    gw = (CROP[2] - CROP[0]) // CELL
    gh = (CROP[3] - CROP[1]) // CELL

    PAD = 16.0
    CAP = 44.0
    PANEL_W = gw * PX
    PANEL_H = gh * PX
    W = PAD * 3 + PANEL_W * 2
    H = CAP + PANEL_H + 78

    b = []
    for i, (path, title, sub, note) in enumerate((
            ("l72_blend_0.35.ppm", "a third of the way",
             "amber: three angles lerped   blue: one axis, angle scaled",
             "two craft, one pose apart"),
            ("l72_blend_1.00.ppm", "arrived",
             "same start, same end, two routes",
             "204.29° travelled, 179.05° required"))):
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
    b.append(label(W / 2, CAP + PANEL_H + 48,
                   "the teal line is the slerp's axis, and it is the SAME line in both panels — "
                   "it does not move during the blend",
                   cls="xs muted"))
    b.append(label(W / 2, CAP + PANEL_H + 68,
                   "nothing the amber path does can be described that way at any instant, let "
                   "alone throughout",
                   cls="xs muted"))
    return svg(uid, round(W, 1), round(H, 1), "Two routes between the same two poses",
               "Two renders from the gimbal demo during a blend between the same pair of "
               "orientations Lesson 7.1 measured. A teal line runs diagonally across both "
               "panels: the fixed axis of the single turn separating the endpoints. On the "
               "left, a third of the way through, a solid aircraft sits at the geodesic's "
               "position and an amber wireframe aircraft, visibly rotated differently, sits at "
               "the Euler lerp's. On the right both have arrived at the same pose, and the two "
               "nose trails are visible as separate curves: a clean pale-blue arc, and an "
               "amber path that bulges well outside it before rejoining.", b)


# ===========================================================================
# Figure 9 — the indictment, answered  (§9.2)
# ===========================================================================
# ONE CLAIM: the Euler column varies by a factor of nine across four poses and
# the slerp column does not vary at all, because it cannot.
def fig_bands():
    uid = "f72i"
    W, H = 760, 416
    X0, Y0, PW, PH = 128, 56, 440, 236
    b = []

    top = 230.0

    def py(v):
        return Y0 + PH - PH * v / top

    b.append(frame(X0, Y0, PW, PH))
    for v in (0, 50, 100, 150, 200):
        y = py(v)
        b.append(rule(X0, y, X0 + PW, y))
        b.append(label(X0 - 10, y + 4, f"{v}%", cls="xs muted", anchor="end"))

    slot = PW / len(BANDS)
    for i, (a, z, euler_pc, slerp_pc) in enumerate(BANDS):
        cx = X0 + slot * (i + 0.5)
        bw = 30.0
        # Euler's bar.
        b.append(box(cx - bw - 3, py(euler_pc), bw, Y0 + PH - py(euler_pc), AMBER, rx=2))
        b.append(label(cx - bw / 2 - 3, py(euler_pc) - 8, f"{euler_pc:.1f}%", cls="xs"))
        # Slerp's, which is zero and therefore has to be drawn as a MARK rather
        # than a bar: a rectangle of height zero is invisible and would read as a
        # missing measurement rather than as a measured zero.
        b.append(cline(cx + 3, Y0 + PH, cx + 3 + bw, Y0 + PH, BLUE, width=4.0))
        b.append(label(cx + bw / 2 + 3, Y0 + PH - 10, "0.00%", cls="xs t-ok"))
        b.append(label(cx, Y0 + PH + 20, f"{a:.0f}° → {z:.0f}°", cls="xs muted"))
        if i == 0:
            b.append(label(cx, Y0 + PH + 36, "(near lock)", cls="xs muted"))

    b.append(label(X0 + PW / 2, Y0 + PH + 60,
                   "the same three angle deltas, run at four distances from gimbal lock",
                   cls="xs muted"))
    b.append(label(X0 - 84, Y0 + PH / 2 - 8, "excess", cls="xs muted"))
    b.append(label(X0 - 84, Y0 + PH / 2 + 8, "turning", cls="xs muted"))

    lx = X0 + PW + 20
    b.append(box(lx - 8, Y0 + 6, 140, 86))
    b.append(box(lx, Y0 + 18, 16, 9, AMBER, rx=2))
    b.append(label(lx + 24, Y0 + 26, "Euler lerp", cls="xs", anchor="start"))
    b.append(cline(lx, Y0 + 44, lx + 16, Y0 + 44, BLUE, width=4.0))
    b.append(label(lx + 24, Y0 + 48, "slerp", cls="xs", anchor="start"))
    b.append(label(lx, Y0 + 72, "2,048 steps each", cls="xs muted", anchor="start"))

    b.append(label(W / 2, H - 46,
                   "the amber column varies by a factor of nine with the POSE, for moves that "
                   "are identical in the angles",
                   cls="xs muted"))
    b.append(label(W / 2, H - 20,
                   "the blue one cannot vary: a geodesic performs the turning required and "
                   "there is no other number it could report",
                   cls="xs muted"))
    return svg(uid, W, H, "Excess turning, Euler lerp against slerp",
               "A bar chart of wasted turning for four blends that use identical angle "
               "changes at four different pitches. The amber bars, for interpolating three "
               "Euler angles, read 209.5 per cent near gimbal lock, then 62.5, 22.5 and 26.7 "
               "per cent further away. Beside each, the spherical interpolation's result is "
               "drawn as a flat mark on the baseline, labelled zero point zero zero per cent, "
               "because it performs exactly the turning the move requires at every pose.", b)


# ===========================================================================
def main():
    figures = [
        ("l72_fig1.svg", fig_fixed_line),
        ("l72_fig2.svg", fig_odd),
        ("l72_fig3.svg", fig_rodrigues),
        ("l72_fig4.svg", fig_axis_render),
        ("l72_fig5.svg", fig_amplify),
        ("l72_fig6.svg", fig_routes),
        ("l72_fig7.svg", fig_identity),
        # PAGE ORDER, not the order they were written: §9.2's bar chart appears
        # before §9.3's renders. The first draft had these two the other way
        # round, which is the one numbering mistake this pipeline cannot catch
        # for itself — every figure still renders, just under the wrong number.
        ("l72_fig8.svg", fig_bands),
        ("l72_fig9.svg", fig_blend_render),
    ]
    for name, fn in figures:
        path = os.path.join(OUT, name)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(fn())
        print(f"wrote {path}  ({os.path.getsize(path):,} bytes)")


if __name__ == "__main__":
    main()
