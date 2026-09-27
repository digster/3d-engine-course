#!/usr/bin/env python3
"""scratch/figs_76.py — Lesson 7.6's diagrams.

Same rules as 5.1-7.5's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - the figure palette must contain the DEMO's own colours (rle_rects SNAPS)

TWO SAMPLERS, AND CHOOSING THE WRONG ONE COSTS THE WHOLE FIGURE. The solid
renders here are a SHADED SURFACE, so they average (`box_sample`); the weight
view is 504 one-pixel debug lines on black, so it peaks (`peak_sample`). 5.11
learned the second half and this lesson is the first to need both in one file.

Every number below comes from verify_76's output (scratch/verify_76.log) or from
the rig demo's own receipt, except where a figure's geometry is recomputed here.
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

OUT = "scratch"

# ---- the rig demo's palette, transcribed from demos/rig/main.cpp -----------
TUBE = "#c6cad4"          # the skinned surface's tint
J_AMBER = "#f2ba5c"
J_ROSE = "#ec7a8c"
J_VIOLET = "#9e84e8"
J_TEAL = "#60c4ce"
J_CITRON = "#d6d660"
J_SAGE = "#92c894"
AX_X = "#ec5c5c"          # debug_lines::k_axis_x_colour
AX_Y = "#7ac498"
AX_Z = "#7ea2ec"
GHOST = "#606676"

JOINTS = [J_AMBER, J_ROSE, J_VIOLET, J_TEAL, J_CITRON, J_SAGE]

DEMO_PALETTE = [hexrgb(c) for c in (TUBE, J_AMBER, J_ROSE, J_VIOLET, J_TEAL,
                                    J_CITRON, J_SAGE, AX_X, AX_Y, AX_Z, GHOST)]


def f(v):
    return f"{v:.2f}"


def fmt_e(v):
    s = f"{v:.3e}"
    m, e = s.split("e")
    return f"{m}e{int(e):+03d}"


# ===========================================================================
# Measured — every figure's data, with the line of verify_76.log it came from
# ===========================================================================

# §B — depth against error. (joints, vs general inverse, bind residual)
DEPTH = [(2, 2.384e-07, 1.192e-07), (4, 2.384e-07, 2.384e-07),
         (8, 4.768e-07, 6.557e-07), (16, 1.907e-06, 2.027e-06),
         (32, 1.550e-06, 6.676e-06)]

# §F — the twist sweep. (degrees, measured radius, cos(theta/2))
TWIST = [(0.0, 1.000000, 1.000000), (30.0, 0.965926, 0.965926),
         (60.0, 0.866025, 0.866025), (90.0, 0.707107, 0.707107),
         (120.0, 0.500000, 0.500000), (150.0, 0.258819, 0.258819),
         (179.0, 0.008727, 0.008727), (180.0, 0.000000, 0.000000)]

# §F — 180 degrees over n segments. (n, measured, cos(90/n))
SEGMENTS = [(1, 0.000000), (2, 0.707107), (3, 0.866025),
            (4, 0.923879), (6, 0.965926), (8, 0.980785)]

# §F — the bend. (degrees, min radius, r cos(d/2), max radius)
BEND = [(0.0, 1.000000, 1.000000, 1.000000), (30.0, 0.965926, 0.965926, 1.000000),
        (60.0, 0.866025, 0.866025, 1.000000), (90.0, 0.707107, 0.707107, 1.000000),
        (120.0, 0.500000, 0.500000, 1.000000)]

# §G — non-uniform joint scale. (scale, normal error deg, delta N.L)
NORMALS = [(1.00, 0.0000, 0.0000), (1.10, 5.4526, 0.0202), (1.25, 12.6804, 0.0469),
           (1.50, 22.6199, 0.0833), (2.00, 36.8699, 0.1342)]

# §I — worst departure from slerp, in degrees of pose. (twist, LBS, nlerp)
BLENDS = [(30.0, 0.1337, 0.0331), (60.0, 1.1169, 0.2675), (90.0, 4.0744, 0.9187),
          (120.0, 10.9478, 2.2337), (150.0, 26.3396, 4.5140), (179.0, 76.6286, 8.0006)]

# §H — cost, against build-rel/engine/libengine.a (Release)
PALETTE_US = 0.99
SKIN_US = 41.20
NS_PER_VERTEX = 8.51
PALETTE_PCT = 2.34
PALETTE_BYTES = 4096
VERTEX_BYTES = 116160
DEBUG_NS = 153.90
DEBUG_PALETTE_US = 26.32


# ===========================================================================
# Shared helpers
# ===========================================================================

def render_panel(ppm, crop, x, y, px, cell=4, levels=3, peak=False):
    """A real render, downsampled and run-length encoded to rectangles.

    `peak` for a line drawing, averaging for a shaded surface — see the module
    docstring. Returns the elements and the panel's size in page units, because
    a figure's height must be COMPUTED from both columns.
    """
    _w, _h, data = read_ppm(os.path.join(OUT, ppm))
    sampler = peak_sample if peak else box_sample
    grid_w, grid_h, grid = sampler(data, _w, crop, cell)
    body = rle_rects(grid, grid_w, grid_h, x, y, px, DEMO_PALETTE, levels=levels,
                     bg_class="fill-shot")
    # WRAPPED IN shape-rendering="crispEdges", AND THE FIRST DRAFT WAS NOT.
    # `rle_rects` emits one rect per run, each exactly `px` tall and butted
    # against the row above it. At the browser's scale that shared edge lands
    # between device pixels and is ANTIALIASED from both sides, so a solid
    # surface picks up a hairline of the panel behind it every row or two — which
    # reads as horizontal banding across a shaded tube and looks like a
    # quantisation artifact rather than a rasterisation one. crispEdges snaps the
    # edges to the pixel grid and the seams disappear. The fix belongs here and
    # not in `rle_rects`, which every render figure since Lesson 4.5 depends on.
    return (['<g shape-rendering="crispEdges">'] + body + ['</g>'],
            grid_w * px, grid_h * px)


def table(x, y, cols, rows, widths, title=None, row_h=17.0, head_h=20.0):
    """A plain measured table. Columns are right-aligned except the first."""
    out = []
    if title:
        out.append(label(x, y - 8, title, cls="xs muted", anchor="start"))
    cx = [x]
    for w in widths[:-1]:
        cx.append(cx[-1] + w)
    for i, name in enumerate(cols):
        anchor = "start" if i == 0 else "end"
        tx = cx[i] if i == 0 else cx[i] + widths[i] - 6
        out.append(label(tx, y + 12, name, cls="xs muted", anchor=anchor))
    out.append(rule(x, y + head_h, cx[-1] + widths[-1], y + head_h, cls="grid"))
    for r, row in enumerate(rows):
        ry = y + head_h + (r + 1) * row_h
        for i, cell in enumerate(row):
            anchor = "start" if i == 0 else "end"
            tx = cx[i] if i == 0 else cx[i] + widths[i] - 6
            cls = "xs mono" if i > 0 else "xs"
            if isinstance(cell, tuple):
                cell, cls = cell
            out.append(label(tx, ry, str(cell), cls=cls, anchor=anchor))
    return out, head_h + (len(rows) + 0.5) * row_h


def bars(x, top, pw, ph, rows, peak, unit, title, sub, colours):
    out = [label(x + pw / 2, top - 27, title, cls="sm"),
           label(x + pw / 2, top - 14, sub, cls="xs muted"),
           frame(x, top, pw, ph)]
    slot = ph / len(rows)
    for i, (name, value) in enumerate(rows):
        y = top + i * slot + 20.0
        hgt = slot * 0.34
        width = (pw - 108) * (value / peak)
        out.append(box(x + 8, y, max(width, 1.5), hgt, colours[i], opacity=0.85,
                       width=1.0, rx=2))
        out.append(label(x + 8, y - 7, name, cls="xs muted", anchor="start"))
        out.append(label(x + 12 + max(width, 1.5), y + hgt - 3, unit(value),
                         cls="xs mono", anchor="start"))
    return out


# ===========================================================================
# Figure 1 — what a hierarchy cannot do  (§2)
# ===========================================================================
# ONE CLAIM: the problem is not the maths, it is the geometry. Two rigid parts
# are two matrices and a hierarchy handles them; one continuous surface is not
# two of anything.
def fig_rigid_fails():
    uid = "f76a"
    W, H = 760, 330
    b = [cmarker(uid, "amber", J_AMBER), cmarker(uid, "rose", J_ROSE)]

    PW = 352.0
    TOP = 56.0
    PH = 196.0

    def elbow(ox, rigid):
        out = [frame(ox, TOP, PW, PH)]
        # The two bones, in the two joint colours, hinged 55 degrees apart.
        hinge = (ox + PW * 0.44, TOP + PH * 0.70)
        upper = (hinge[0] - 104.0, hinge[1] - 4.0)
        ang = -55 * D
        lower = (hinge[0] + 104.0 * math.cos(ang), hinge[1] + 104.0 * math.sin(ang))

        R = 26.0
        if rigid:
            # Two capsules that do not know about each other: the inside of the
            # elbow interpenetrates and the outside opens a wedge.
            for (a, bpt, col) in ((upper, hinge, J_AMBER), (hinge, lower, J_ROSE)):
                dx, dy = bpt[0] - a[0], bpt[1] - a[1]
                L = math.hypot(dx, dy)
                nx, ny = -dy / L * R, dx / L * R
                out.append(poly([(a[0] + nx, a[1] + ny), (bpt[0] + nx, bpt[1] + ny),
                                 (bpt[0] - nx, bpt[1] - ny), (a[0] - nx, a[1] - ny)],
                                col, width=1.6))
            out.append(label(ox + PW / 2, TOP + PH + 20, "two rigid parts, two matrices",
                             cls="xs muted"))
        else:
            # One surface whose outline bends: sampled along the bone chain with
            # the radius carried round the corner.
            top_side, bot_side = [], []
            for i in range(41):
                t = i / 40.0
                if t <= 0.5:
                    u = t / 0.5
                    px = upper[0] + (hinge[0] - upper[0]) * u
                    py = upper[1] + (hinge[1] - upper[1]) * u
                    dx, dy = hinge[0] - upper[0], hinge[1] - upper[1]
                else:
                    u = (t - 0.5) / 0.5
                    px = hinge[0] + (lower[0] - hinge[0]) * u
                    py = hinge[1] + (lower[1] - hinge[1]) * u
                    dx, dy = lower[0] - hinge[0], lower[1] - hinge[1]
                # Near the hinge the direction is the BLEND of the two, which is
                # the whole picture in one line.
                w = max(0.0, 1.0 - abs(t - 0.5) / 0.22)
                if w > 0.0:
                    d0 = (hinge[0] - upper[0], hinge[1] - upper[1])
                    d1 = (lower[0] - hinge[0], lower[1] - hinge[1])
                    dx = d0[0] * (1 - t) + d1[0] * t
                    dy = d0[1] * (1 - t) + d1[1] * t
                L = math.hypot(dx, dy)
                nx, ny = -dy / L * R, dx / L * R
                shrink = 1.0 - 0.16 * w          # the blend pulls the surface in
                top_side.append((px + nx * shrink, py + ny * shrink))
                bot_side.append((px - nx * shrink, py - ny * shrink))
            out.append(poly(top_side + bot_side[::-1], TUBE, width=1.6))
            out.append(label(ox + PW / 2, TOP + PH + 20,
                             "one surface, every vertex weighted", cls="xs muted"))

        out.append(carrow(upper[0], upper[1], hinge[0], hinge[1], uid, "amber",
                          J_AMBER, width=2.0))
        out.append(carrow(hinge[0], hinge[1], lower[0], lower[1], uid, "rose",
                          J_ROSE, width=2.0))
        out.append(f'<circle cx="{f(hinge[0])}" cy="{f(hinge[1])}" r="3.4" '
                   f'fill="var(--dia-bg)" stroke="var(--dia-ink)" stroke-width="1.2"/>')
        return out

    b += elbow(16.0, True)
    b += elbow(16.0 + PW + 24.0, False)
    b.append(label(16.0 + PW / 2, 34, "what Lesson 5.9 can draw", cls="sm"))
    b.append(label(16.0 + PW + 24.0 + PW / 2, 34, "what a character is", cls="sm"))

    b.append(label(W / 2, H - 22,
                   "the vertices at the hinge belong to both bones and to neither",
                   cls="xs muted"))

    return svg(uid, W, H, "A rigid hierarchy against a skinned surface",
               "Left: two rigid capsules hinged at an elbow, interpenetrating on the "
               "inside and opening a wedge on the outside. Right: one continuous "
               "surface whose hinge vertices are moved by both bones at once.", b)


# ===========================================================================
# Figure 2 — the two factors, and the labels that cancel  (§4)
# ===========================================================================
# ONE CLAIM: a skinning matrix is model_from_model. The inner labels agree
# because both name the SAME JOINT; the outer ones agree because both name model
# space — which is what makes the weighted sum well-typed.
def fig_spaces():
    uid = "f76b"
    W, H = 760, 392
    b = [cmarker(uid, "amber", J_AMBER), cmarker(uid, "teal", J_TEAL)]

    BW, BH = 176.0, 62.0
    Y = 74.0
    xs = [24.0, 24.0 + BW + 92.0, 24.0 + 2 * (BW + 92.0)]

    names = [("MODEL space", "the mesh, as modelled"),
             ("JOINT space", "joint j, at BIND time"),
             ("MODEL space", "the mesh, deformed")]
    for i, (name, sub) in enumerate(names):
        b.append(box(xs[i], Y, BW, BH))
        b.append(label(xs[i] + BW / 2, Y + 26, name, cls="sm"))
        b.append(label(xs[i] + BW / 2, Y + 44, sub, cls="xs muted"))

    b.append(carrow(xs[0] + BW + 6, Y + BH / 2, xs[1] - 8, Y + BH / 2, uid, "amber",
                    J_AMBER, width=1.8))
    b.append(carrow(xs[1] + BW + 6, Y + BH / 2, xs[2] - 8, Y + BH / 2, uid, "teal",
                    J_TEAL, width=1.8))
    b.append(label((xs[0] + BW + xs[1]) / 2, Y - 10, "joint_from_model", cls="xs mono"))
    b.append(label((xs[0] + BW + xs[1]) / 2, Y + BH + 20,
                   "the inverse bind matrix", cls="xs muted"))
    b.append(label((xs[0] + BW + xs[1]) / 2, Y + BH + 34, "baked once", cls="xs muted"))
    b.append(label((xs[1] + BW + xs[2]) / 2, Y - 10, "model_from_joint", cls="xs mono"))
    b.append(label((xs[1] + BW + xs[2]) / 2, Y + BH + 20,
                   "the joint, POSED", cls="xs muted"))
    b.append(label((xs[1] + BW + xs[2]) / 2, Y + BH + 34, "rebuilt every frame",
                   cls="xs muted"))

    # The cancellation, written out.
    CY = 232.0
    b.append(rule(24, CY - 16, W - 24, CY - 16, cls="grid"))
    b.append(label(W / 2, CY + 8,
                   "skin  =  model_from_joint  ·  joint_from_model",
                   cls="sm mono"))
    b.append(label(W / 2, CY + 30,
                   "the inner labels agree — same joint, two different TIMES",
                   cls="xs muted"))
    b.append(label(W / 2, CY + 56, "skin  =  model_from_model", cls="sm mono"))
    b.append(label(W / 2, CY + 78,
                   "…and so do the outer ones, which is why four of them may be added",
                   cls="xs muted"))

    b.append(label(W / 2, H - 22,
                   "at the bind pose the two factors are inverses, so every skin is I "
                   "— measured 4.768e-07",
                   cls="xs muted"))

    return svg(uid, W, H, "The two factors of a skinning matrix",
               "Model space to joint space at bind time, then joint space to model "
               "space at pose time. The inner labels cancel and the outer labels are "
               "the same space, so a skinning matrix maps model space to itself.", b)


# ===========================================================================
# Figure 3 — one loop, two chains  (§5)
# ===========================================================================
# ONE CLAIM: the inverse bind array needs no matrix inversion. Inverting a
# product reverses it, so the inverse chain composes on the OTHER SIDE with the
# same parent-before-child loop.
def fig_two_chains():
    uid = "f76c"
    W, H = 760, 420
    b = []

    N = 4
    BW, BH = 104.0, 40.0
    GAP = 44.0
    X0 = 44.0
    ROWS = [(78.0, "model_from_joint[j]  =  model_from_joint[p] \u00b7 parent_from_local(j)",
             "forward \u2014 the parent's matrix multiplies on the LEFT", True),
            (216.0, "joint_from_model[j]  =  local_from_parent(j) \u00b7 joint_from_model[p]",
             "inverse \u2014 the same parent multiplies on the RIGHT", False)]

    for (y, formula, sub, forward) in ROWS:
        b.append(label(X0, y - 26, formula, cls="xs mono", anchor="start"))
        b.append(label(X0, y - 11, sub, cls="xs muted", anchor="start"))
        for j in range(N):
            x = X0 + j * (BW + GAP)
            col = JOINTS[j]
            b.append(hollow(x, y, BW, BH, col, width=1.3))
            b.append(label(x + BW / 2, y + 25, f"joint {j}", cls="xs"))
            if j + 1 < N:
                # BOTH ROWS READ LEFT TO RIGHT, and the first draft drew the
                # second row's arrows backwards to show "multiplies on the
                # right". That is a statement about the ALGEBRA and the arrows
                # are a statement about the WALK — so the picture contradicted
                # the caption beside it, which said both walks visit the array in
                # the same order. They do.
                key = f"c{j}{'f' if forward else 'i'}"
                b.append(cmarker(uid, key, col))
                b.append(carrow(x + BW + 4, y + BH / 2, x + BW + GAP - 6,
                                y + BH / 2, uid, key, col, width=1.5))
    b.append(label(W / 2, 166, "both walks visit the array in the SAME order, "
                   "index 0 upward \u2014 one flat loop, no sorting", cls="xs muted"))

    rows = [(str(n), fmt_e(a), fmt_e(bd)) for (n, a, bd) in DEPTH]
    tb, th = table(X0, 294.0, ("joints", "vs a general inverse", "bind residual"),
                   rows, (96.0, 200.0, 150.0))
    b += tb

    NX = X0 + 470.0
    b.append(label(NX, 312.0, "the control is a Gauss-Jordan", cls="xs muted", anchor="start"))
    b.append(label(NX, 328.0, "inverse written in the harness,", cls="xs muted", anchor="start"))
    b.append(label(NX, 344.0, "sharing no code with the engine.", cls="xs muted", anchor="start"))
    b.append(label(NX, 368.0, "Error grows with depth and stays", cls="xs muted", anchor="start"))
    b.append(label(NX, 384.0, "five orders below anything a rig's", cls="xs muted", anchor="start"))
    b.append(label(NX, 400.0, "own dimensions would notice.", cls="xs muted", anchor="start"))
    _ = th

    return svg(uid, W, H, "The forward chain and the inverse chain",
               "Two rows of four joints, both walked from index zero upward. The "
               "forward composition multiplies the parent on the left and the inverse "
               "composition on the right. A table of depth against error follows.", b)


# ===========================================================================
# Figure 4 — the bug, rendered  (§6)
# ===========================================================================
def fig_no_bind():
    uid = "f76d"
    # THE WHOLE FRAME, not a crop. The first version cropped to the middle 408
    # columns, which is right for the twist figures and exactly wrong here: the
    # failure IS that the mesh leaves the frame, and a crop is a way of not
    # showing it.
    CROP = (0, 0, 960, 540)
    PX, CELL = 2.00, 6
    W = 760.0
    CAP = 66.0
    GAP = 20.0

    probe, PW, PH = render_panel("l76_bend45.ppm", CROP, 0, 0, PX, cell=CELL)
    _ = probe
    X0 = (W - (2 * PW + GAP)) / 2.0

    left, _, _ = render_panel("l76_bend45.ppm", CROP, X0, CAP, PX, cell=CELL)
    right, _, _ = render_panel("l76_nobind.ppm", CROP, X0 + PW + GAP, CAP, PX, cell=CELL)

    H = CAP + PH + 96.0
    b = left + right

    b.append(label(X0 + PW / 2, 32, "skin = model_from_joint \u00b7 joint_from_model",
                   cls="xs mono"))
    b.append(label(X0 + PW / 2, 50, "the rig, bent 45\u00b0", cls="xs muted"))
    b.append(label(X0 + PW + GAP + PW / 2, 32, "skin = model_from_joint",
                   cls="xs mono"))
    b.append(label(X0 + PW + GAP + PW / 2, 50,
                   "the same rig, the same pose, no inverse binds", cls="xs muted"))

    y = CAP + PH + 30
    b.append(label(W / 2, y,
                   "worst vertex displacement 4.4366 units, on a tube 5.00 units long",
                   cls="sm"))
    b.append(label(W / 2, y + 22,
                   "the mesh\u2019s coordinates were never in any joint\u2019s space, so "
                   "multiplying them", cls="xs muted"))
    b.append(label(W / 2, y + 38,
                   "by a joint\u2019s matrix asks a question that has no answer.",
                   cls="xs muted"))

    return svg(uid, W, H, "Skinning with and without the inverse bind matrices",
               "Two real renders of the same rig in the same pose, both showing the "
               "whole frame. With the inverse bind matrices the tube bends smoothly; "
               "without them it is displaced and stretched out of the frame.", b)


# ===========================================================================
# Figure 5 — the weights, drawn  (§7)
# ===========================================================================
def fig_weights():
    uid = "f76e"
    CROP = (396, 4, 568, 500)
    PX, CELL = 2.30, 4
    PAD, CAP = 20.0, 52.0

    shot, SW, SH = render_panel("l76_wrest.ppm", CROP, PAD, CAP, PX, cell=CELL,
                                peak=True, levels=3)

    PLOT_X = PAD + SW + 44.0
    PLOT_W = 430.0
    PLOT_H = 232.0
    W = PLOT_X + PLOT_W + 24.0
    H = CAP + SH + 40.0
    b = list(shot)

    b.append(label(PAD + SW / 2, 30, "the weighting, drawn", cls="sm"))
    b.append(label(PAD + SW / 2, 44, "one ring, one blend", cls="xs muted"))

    # The weight ramp: five hat functions, one per joint.
    b.append(frame(PLOT_X, CAP, PLOT_W, PLOT_H))
    b.append(label(PLOT_X + PLOT_W / 2, CAP - 22, "weight against distance along the chain",
                   cls="sm"))
    b.append(label(PLOT_X + PLOT_W / 2, CAP - 8,
                   "two influences per vertex, linear between the joints either side",
                   cls="xs muted"))

    def px_(u):     # u in [0, 5]
        return PLOT_X + 34.0 + (PLOT_W - 58.0) * (u / 5.0)

    def py_(w):     # w in [0, 1]
        return CAP + PLOT_H - 34.0 - (PLOT_H - 58.0) * w

    for j in range(6):
        pts = []
        for i in range(0, 201):
            u = 5.0 * i / 200.0
            w = max(0.0, 1.0 - abs(u - j))
            pts.append((px_(u), py_(w)))
        b.append(poly(pts, JOINTS[j], width=1.6, close=False))
        b.append(label(px_(j), CAP + PLOT_H - 18.0, str(j), cls="xs mono"))
    b.append(rule(px_(0), py_(0), px_(5), py_(0), cls="grid"))
    b.append(rule(px_(0), py_(1), px_(5), py_(1), cls="grid", dash="3 4"))
    b.append(label(PLOT_X + 26.0, py_(1) + 4, "1", cls="xs mono", anchor="end"))
    b.append(label(PLOT_X + 26.0, py_(0) + 4, "0", cls="xs mono", anchor="end"))
    b.append(label(PLOT_X + PLOT_W / 2, CAP + PLOT_H + 18,
                   "at every point the two live weights sum to exactly 1",
                   cls="xs muted"))

    y = CAP + PLOT_H + 48
    b.append(label(PLOT_X, y, "scale every weight by 0.9 and:", cls="xs", anchor="start"))
    b.append(label(PLOT_X, y + 18,
                   "every vertex lands at 0.9 × its distance from the MODEL ORIGIN",
                   cls="xs muted", anchor="start"))
    b.append(label(PLOT_X, y + 34,
                   "measured gap from that prediction 5.218e-07; worst move 0.5012 units",
                   cls="xs mono", anchor="start"))
    b.append(label(PLOT_X, y + 52,
                   "it reads as a scale bug and it is a weighting bug.",
                   cls="xs muted", anchor="start"))

    H = max(H, y + 74)
    return svg(uid, W, H, "The weighting, and the sum-to-one law",
               "A render of the tube's rings coloured by their weight blend, beside "
               "six hat functions showing weight against distance along the chain.", b)


# ===========================================================================
# Figure 6 — the candy wrapper, derived  (§9)
# ===========================================================================
# ONE CLAIM: the average of two points theta apart on a circle sits at
# cos(theta/2) of the radius. There is no skinning in the derivation at all.
def fig_candy():
    uid = "f76f"
    W, H = 760, 452
    b = [cmarker(uid, "amber", J_AMBER), cmarker(uid, "rose", J_ROSE),
         cmarker(uid, "teal", J_TEAL)]

    CX, CY, R = 186.0, 196.0, 128.0
    THETA = 120.0

    b.append(f'<circle cx="{f(CX)}" cy="{f(CY)}" r="{f(R)}" fill="none" '
             f'class="grid" stroke-width="1.1" stroke-dasharray="3 5"/>')
    b.append(f'<circle cx="{f(CX)}" cy="{f(CY)}" r="2.6" fill="var(--dia-ink)"/>')

    a0 = -90.0 + THETA / 2
    a1 = -90.0 - THETA / 2
    p0 = (CX + R * math.cos(a0 * D), CY + R * math.sin(a0 * D))
    p1 = (CX + R * math.cos(a1 * D), CY + R * math.sin(a1 * D))
    mid = ((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2)

    b.append(carrow(CX, CY, p0[0], p0[1], uid, "amber", J_AMBER, width=1.8))
    b.append(carrow(CX, CY, p1[0], p1[1], uid, "rose", J_ROSE, width=1.8))
    b.append(cline(p0[0], p0[1], p1[0], p1[1], GOLD_CHORD, width=1.5, dash="4 4"))
    b.append(carrow(CX, CY, mid[0], mid[1], uid, "teal", J_TEAL, width=2.2))

    b.append(label(p0[0] + 26, p0[1] + 4, "R₀ v", cls="xs mono", anchor="start"))
    b.append(label(p1[0] - 26, p1[1] + 4, "R₁ v", cls="xs mono", anchor="end"))
    b.append(label(mid[0], mid[1] - 12, "½(R₀v + R₁v)", cls="xs mono"))
    b.append(label(CX, CY + R + 26, "the blend leaves the circle INWARD", cls="xs muted"))

    # The arc marking theta.
    ar = 33.0
    b.append(f'<path d="M {f(CX + ar * math.cos(a1 * D))},{f(CY + ar * math.sin(a1 * D))} '
             f'A {f(ar)},{f(ar)} 0 0 1 '
             f'{f(CX + ar * math.cos(a0 * D))},{f(CY + ar * math.sin(a0 * D))}" '
             f'fill="none" class="ink-soft" stroke-width="1.1"/>')
    # OFF THE BISECTOR, and check-page.js is why. The teal arrow runs straight
    # up from the centre through the apex, so a label centred on CX sits ON a
    # line — which the text-on-shape check reports and a reader experiences as
    # a glyph with a stroke through it.
    b.append(label(CX - 26, CY - 16, "θ", cls="sm mono"))

    # The right-angle argument, in words.
    TX = 372.0
    b.append(label(TX, 58, "the isoceles triangle does the whole job",
                   cls="sm", anchor="start"))
    for i, line in enumerate((
            "R₀v and R₁v are both length |v| and θ apart.",
            "Their midpoint lies on the BISECTOR, and the",
            "bisector of an isoceles triangle meets the base",
            "at a right angle — so the midpoint is at",
            "|v| · cos(θ/2).",
            "",
            "Nothing in that sentence mentions skinning,",
            "a joint, or a matrix. It is the same half-angle",
            "Lesson 7.3 found between two mirrors and",
            "Lesson 7.4 found inside a quaternion.")):
        b.append(label(TX, 84 + i * 18, line, cls="xs muted", anchor="start"))

    rows = [(f"{t:.0f}°", f"{m:.6f}", f"{p:.6f}") for (t, m, p) in TWIST[:7]]
    rows.append(("180°", "0.000000", "0.000000"))
    tb, _ = table(TX, 262.0, ("twist", "measured", "cos(θ/2)"),
                  rows, (74.0, 128.0, 128.0), row_h=15.0, head_h=17.0)
    b += tb

    b.append(label(186.0, 386, "worst gap over the sweep 8.742e-08", cls="xs mono"))

    return svg(uid, W, H, "Why the blend collapses to cos(theta/2)",
               "A circle with two rotated copies of a vertex theta apart and their "
               "midpoint on the bisector, inside the circle, at cos(theta/2) of the "
               "radius. A measured table of the sweep is beside it.", b)


GOLD_CHORD = "#f8d678"


# ===========================================================================
# Figure 7 — the same 180 degrees, over more joints  (§9)
# ===========================================================================
def fig_twist_render():
    uid = "f76g"
    CROP = (396, 4, 568, 500)
    PX, CELL = 1.05, 4
    PAD, CAP = 16.0, 56.0
    GAP = 34.0

    panels = []
    x = PAD
    sizes = []
    for ppm in ("l76_tw1.ppm", "l76_tw2.ppm", "l76_tw4.ppm"):
        body, pw, ph = render_panel(ppm, CROP, x, CAP, PX, cell=CELL)
        panels += body
        sizes.append((x, pw, ph))
        x += pw + GAP

    PW = sizes[0][1]
    PH = sizes[0][2]
    PLOT_X = x + 26.0
    PLOT_W = 300.0
    W = PLOT_X + PLOT_W + 24.0
    H = CAP + PH + 78.0
    b = list(panels)

    caps = [("n = 1", "0.000000"), ("n = 2", "0.707107"),
            ("n = 4", "0.923879")]
    for (px_, pw, _), (name, value) in zip(sizes, caps):
        b.append(label(px_ + pw / 2, 30, name, cls="sm"))
        b.append(label(px_ + pw / 2, 46, f"r × {value}", cls="xs mono"))

    # cos(90/n), with the measured dots on it.
    b.append(frame(PLOT_X, CAP, PLOT_W, PH))
    b.append(label(PLOT_X + PLOT_W / 2, CAP - 22, "180° of twist, over n segments",
                   cls="sm"))
    b.append(label(PLOT_X + PLOT_W / 2, CAP - 8, "radius / r  =  cos(90° / n)",
                   cls="xs mono"))

    def qx(n):
        return PLOT_X + 40.0 + (PLOT_W - 66.0) * (n - 1) / 7.0

    def qy(v):
        return CAP + PH - 34.0 - (PH - 62.0) * v

    pts = []
    for i in range(0, 141):
        n = 1.0 + 7.0 * i / 140.0
        pts.append((qx(n), qy(math.cos(math.pi / (2.0 * n)))))
    b.append(poly(pts, J_TEAL, width=1.6, close=False))
    for (n, v) in SEGMENTS:
        b.append(f'<circle cx="{f(qx(n))}" cy="{f(qy(v))}" r="3.0" fill="{J_AMBER}"/>')
        b.append(label(qx(n), CAP + PH - 16.0, str(n), cls="xs mono"))
    b.append(rule(qx(1), qy(0), qx(8), qy(0), cls="grid"))
    b.append(rule(qx(1), qy(1), qx(8), qy(1), cls="grid", dash="3 4"))
    b.append(label(PLOT_X + 32.0, qy(1) + 4, "1", cls="xs mono", anchor="end"))
    b.append(label(PLOT_X + 32.0, qy(0) + 4, "0", cls="xs mono", anchor="end"))

    b.append(label(W / 2, CAP + PH + 30,
                   "this is what a forearm twist bone IS — a direct attack on a "
                   "half-angle,", cls="xs muted"))
    b.append(label(W / 2, CAP + PH + 48,
                   "and it is why a rig carries more joints than a skeleton does.",
                   cls="xs muted"))

    return svg(uid, W, H, "The candy wrapper, and the twist-bone fix",
               "Three real renders of the same 180-degree twist spread over one, two "
               "and four joints, beside the curve cos(90/n) with the measured radii "
               "sitting on it.", b)


# ===========================================================================
# Figure 8 — a bend flattens where a twist collapses  (§9)
# ===========================================================================
def fig_bend_ellipse():
    uid = "f76h"
    W, H = 760, 366
    b = []

    PW, PH = 214.0, 196.0
    TOP = 84.0

    def ring(ox, minor, title, sub):
        out = [frame(ox, TOP, PW, PH)]
        cx, cy = ox + PW / 2, TOP + PH / 2 + 6
        R = 74.0
        out.append(f'<ellipse cx="{f(cx)}" cy="{f(cy)}" rx="{f(R)}" ry="{f(R)}" '
                   f'fill="none" class="grid" stroke-width="1.1" stroke-dasharray="3 5"/>')
        out.append(f'<ellipse cx="{f(cx)}" cy="{f(cy)}" rx="{f(R * minor[0])}" '
                   f'ry="{f(R * minor[1])}" fill="none" stroke="{J_TEAL}" '
                   f'stroke-width="1.8"/>')
        out.append(label(ox + PW / 2, TOP - 26, title, cls="sm"))
        out.append(label(ox + PW / 2, TOP - 10, sub, cls="xs mono"))
        return out

    c = math.cos(60.0 * D)      # 120 degrees of relative rotation
    b += ring(24.0, (1.0, 1.0), "at rest", "r, r")
    b += ring(24.0 + PW + 22.0, (c, c), "a TWIST of 120°", "r cos(θ/2) both ways")
    b += ring(24.0 + 2 * (PW + 22.0), (c, 1.0), "a BEND of 120°",
              "r cos(θ/2) and r")

    b.append(label(W / 2, TOP + PH + 30,
                   "a twist rotates about the ring's OWN axis, so the whole ring is in "
                   "the collapsing plane;", cls="xs muted"))
    b.append(label(W / 2, TOP + PH + 48,
                   "a bend rotates about an axis ACROSS it, so the vertices on that "
                   "axis do not move at all.", cls="xs muted"))
    b.append(label(W / 2, TOP + PH + 70,
                   "measured minor axis at 120°: 0.500000 against a prediction of "
                   "0.500000, major axis 1.000000", cls="xs mono"))

    b.append(label(W / 2, 32,
                   "the same cosine, spent two different ways", cls="sm"))

    return svg(uid, W, H, "A twist collapses a ring; a bend flattens it",
               "Three rings: undeformed, collapsed uniformly by a twist, and squashed "
               "into an ellipse by a bend whose minor axis is the same cosine.", b)


# ===========================================================================
# Figure 9 — LBS against nlerp and slerp  (§10)
# ===========================================================================
def fig_lbs_vs():
    uid = "f76i"
    W, H = 760, 378
    b = []

    PX, PY = 56.0, 62.0
    PW, PH = 396.0, 214.0
    b.append(frame(PX, PY, PW, PH))
    b.append(label(PX + PW / 2, PY - 24, "worst departure from the geodesic", cls="sm"))
    b.append(label(PX + PW / 2, PY - 10, "degrees of pose, over the whole blend",
                   cls="xs muted"))

    peak = 80.0

    def bx(t):
        return PX + 34.0 + (PW - 56.0) * (t - 30.0) / 149.0

    def by(v):
        return PY + PH - 30.0 - (PH - 54.0) * (v / peak)

    for (col, idx, name) in ((J_ROSE, 1, "LBS"), (J_TEAL, 2, "nlerp")):
        pts = [(bx(t), by(row[idx])) for row in BLENDS for t in (row[0],)]
        b.append(poly(pts, col, width=1.8, close=False))
        for (x, y) in pts:
            b.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="2.6" fill="{col}"/>')
        b.append(label(pts[-1][0] - 6, pts[-1][1] - 10, name, cls="xs", anchor="end"))

    for t in (30.0, 60.0, 90.0, 120.0, 150.0, 179.0):
        b.append(label(bx(t), PY + PH - 12, f"{t:.0f}", cls="xs mono"))
    b.append(label(PX + PW / 2, PY + PH + 22, "twist between the two joints, degrees",
                   cls="xs muted"))
    for v in (0.0, 20.0, 40.0, 60.0, 80.0):
        b.append(rule(PX + 34.0, by(v), PX + PW - 12, by(v), cls="grid", dash="2 5"))
        b.append(label(PX + 30.0, by(v) + 4, f"{v:.0f}", cls="xs mono", anchor="end"))

    TX = PX + PW + 32.0
    b.append(label(TX, 58, "two errors, and one it dodges", cls="sm", anchor="start"))
    for i, line in enumerate((
            "LBS gets the SCHEDULE wrong too, by",
            "nlerp's formula at the WHOLE angle",
            "rather than the half angle — 10.95°",
            "against 2.23° at a 120° twist.",
            "",
            "At t = 0.5 both are exact in angle and",
            "only the radius is left: 0.500000.",
            "",
            "And the one thing matrices win. A",
            "rotation matrix is UNIQUE, so there is",
            "no double cover, no long way round,",
            "and no `nearest` to forget. 7.5's",
            "359° bug cannot be written here.")):
        b.append(label(TX, 82 + i * 17, line, cls="xs muted", anchor="start"))

    b.append(label(W / 2, H - 20,
                   "so “LBS is nlerp without the normalise” is the wrong "
                   "one-liner — it is worse than that, and better",
                   cls="xs muted"))

    return svg(uid, W, H, "Linear blend skinning against nlerp and slerp",
               "Two curves of worst angular departure from the geodesic against the "
               "twist between two joints. Linear blend skinning is above normalised "
               "linear interpolation at every arc.", b)


# ===========================================================================
# Figure 10 — cost, and a build that had no flags  (§11)
# ===========================================================================
def fig_cost():
    uid = "f76j"
    W, H = 760, 388
    b = []

    b += bars(24.0, 62.0, 340.0, 118.0,
              [("palette, 64 joints", PALETTE_US), ("skin, 4,840 vertices", SKIN_US)],
              SKIN_US, lambda v: f"{v:.2f} us",
              "where a frame's skinning time goes",
              f"the palette is {PALETTE_PCT:.2f}% of the two",
              [J_TEAL, J_ROSE])

    b += bars(396.0, 62.0, 340.0, 118.0,
              [("palette", float(PALETTE_BYTES)),
               ("deformed positions + normals", float(VERTEX_BYTES))],
              float(VERTEX_BYTES), lambda v: f"{v/1024.0:.1f} KB",
              "what a frame has to SEND",
              "28.4x, which is the whole GPU-skinning argument",
              [J_TEAL, J_ROSE])

    y = 218.0
    b.append(rule(24, y - 18, W - 24, y - 18, cls="grid"))
    b.append(label(24, y, "and the measurement that nearly did not mean anything",
                   cls="sm", anchor="start"))
    rows = [("libengine.a, no build type", f"{DEBUG_NS:.2f} ns", f"{DEBUG_PALETTE_US:.2f} us"),
            ("libengine.a, Release", f"{NS_PER_VERTEX:.2f} ns", f"{PALETTE_US:.2f} us"),
            ("ratio", f"{DEBUG_NS / NS_PER_VERTEX:.1f}×",
             f"{DEBUG_PALETTE_US / PALETTE_US:.1f}×")]
    tb, _ = table(24.0, y + 14, ("", "per vertex", "per palette"),
                  rows, (300.0, 130.0, 130.0))
    b += tb

    b.append(label(24.0, y + 116,
                   "`cmake -S . -B build` leaves CMAKE_BUILD_TYPE EMPTY, so the library "
                   "carries no -O flag at all \u2014 and", cls="xs muted", anchor="start"))
    b.append(label(24.0, y + 132,
                   "-O2 on the harness's own command line does not reach what the "
                   "harness LINKS.", cls="xs muted", anchor="start"))

    return svg(uid, W, H, "What skinning costs, and what it has to send",
               "Two bar panels: per-frame time split between the palette and the "
               "vertices, and per-frame bytes for the same split. Below, a table "
               "comparing an unoptimised library build against a Release one.", b)


# ===========================================================================

# FILENAMES FOLLOW PAGE ORDER, and these last two were drafted in the opposite
# one: §10 spends the cost figure before §10.2 reaches the blend comparison.
# check-page.js's figOrder check caught it — nothing in the pipeline can, because
# every figure still renders, just under the wrong number.
FIGURES = [
    ("l76_fig1.svg", fig_rigid_fails),
    ("l76_fig2.svg", fig_spaces),
    ("l76_fig3.svg", fig_two_chains),
    ("l76_fig4.svg", fig_no_bind),
    ("l76_fig5.svg", fig_weights),
    ("l76_fig6.svg", fig_candy),
    ("l76_fig7.svg", fig_twist_render),
    ("l76_fig8.svg", fig_bend_ellipse),
    ("l76_fig9.svg", fig_cost),
    ("l76_fig10.svg", fig_lbs_vs),
]


def main():
    for name, fn in FIGURES:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}  ({os.path.getsize(path):,} bytes)")


if __name__ == "__main__":
    main()
