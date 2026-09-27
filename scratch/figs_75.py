#!/usr/bin/env python3
"""scratch/figs_75.py — Lesson 7.5's diagrams.

Same rules as 5.1-7.4's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER (fig 4 is the SIGN, fig 5 the CAP:
    §6.3 comes before §6.4, and the drafting order was the other way round)
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - the figure palette must contain the DEMO's own colours (rle_rects SNAPS)

THIS LESSON IS MOSTLY FLAT, and that is a decision rather than a shortcut. Its
subject is a one-dimensional curve — an arc, walked at two different speeds —
and the honest picture of a great circle on the unit 4-sphere is a CIRCLE with
the dimension it lives in written beside it. Drawing a 3-D sphere here would add
two coordinates the argument does not use and one perspective foreshortening
that would corrupt the tick spacing, which is the whole measurement. The two
places where space genuinely matters (§8's transform hierarchy) get boxes and
matrices, not axes.

Every number below comes from verify_75's output (scratch/verify_75.log) or from
the gimbal demo's own receipt, except where a figure's geometry is recomputed
here in Python.
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
from figs_71 import cmarker, carrow, poly, frame, D                 # noqa: E402

OUT = "scratch"

# The gimbal demo's palette, transcribed from demos/gimbal/main.cpp so that page
# and render agree. `rle_rects` SNAPS each sampled pixel to the nearest entry.
TEAL = "#60ded0"          # k_axis   — the rotation axis (a dot, in [S])
GOLD = "#f8d678"          # k_nose   — the gap between the two schedules
GHOST_A = "#eca856"       # k_ghost_euler — nlerp's row of ticks
GHOST_B = "#7ebcf8"       # k_ghost_slerp — slerp's row of ticks
MAGENTA = "#d676e2"       # k_dead   — the long way round
DEMO_GREY = "#7884a8"     # k_trail  — the arc itself

DEMO_PALETTE = [hexrgb(c) for c in (TEAL, GOLD, GHOST_A, GHOST_B, MAGENTA,
                                    DEMO_GREY, GREEN, RED, "#464e60")]


# ===========================================================================
# Measured — every figure's data, with the line of verify_75.log it came from
# ===========================================================================

# §A — three notations, one formula.
AGREE_MAT3 = 4.172e-07        # worst matrix entry, 7.2's rotation_slerp vs ours
AGREE_COMPLEX = 2.384e-07     # worst angle in radians, 7.3's complex_slerp vs ours
AVERAGE_DET = 0.00007         # determinant of the entrywise average of two rotations

# §C — the path, on the 7.1 blend pair, with the atan2 metric.
GEODESIC = 179.0527
PATHS = [("slerp", 179.0524, -0.00), ("nlerp", 179.0524, -0.00),
         ("Euler lerp", 204.2903, +14.10)]
OFF_PLANE = 1.605e-07

# §D — the schedule. (rotation arc, sphere arc, worst gap deg, at t, sec^2)
SCHEDULE = [(30.0, 15.0, 0.0331, 0.789, 1.0173),
            (60.0, 30.0, 0.2675, 0.785, 1.0718),
            (90.0, 45.0, 0.9188, 0.783, 1.1716),
            (120.0, 60.0, 2.2340, 0.222, 1.3333),
            (150.0, 75.0, 4.5140, 0.229, 1.5888),
            (180.0, 90.0, 8.1491, 0.762, 2.0000)]
MAX_SPHERE_ARC = 89.9969
MAX_GAP = 8.1345
RAW_MAX_ARC = 176.3020

# §E — the sign.
FLIPS = 9923
FLIP_PCT = 49.6
LONGEST_WITH = 179.9938
LONGEST_WITHOUT = 352.6042
KEYFRAME = (1.0000, 1.0000, 359.0000)   # separation, slerp travel, without nearest
RAW_CHORD_MIN = 0.00436

# §F — the instrument.
MID_CHORD = 2.107e-08
PATH_ACOS = 88.3437
PATH_ATAN = 150.0000
ACOS_FLOOR = 0.0560           # degrees, 2*sqrt(2*eps)
GAP_AT_HALF = 0.0000
GAP_AT_QUARTER = 4.5140

# §G — the textbook form.
NAN_CLIFF = 0.0279
NAN_PREDICTED = 0.0280

# §H — nanoseconds per call, best of three after a warm-up.
COST = [("quat_nlerp", 4.763), ("quat_slerp", 16.288), ("7.2's mat3 slerp", 45.752)]
BUDGET_ARC = 73.50

# §I — the swap.
ROUND_TRIP = 5.960e-07
ROUND_TRIP_PX = 0.00014
SHEAR_SQUARE = 0.5941
SHEAR_ENTRY = 0.3922
BOOM_CHAIN_ERR = 0.0
BOOM_ONE_NODE_ERR = 0.5863
GOLDEN_HASH = "E917C06C"
CALL_SITES = 25


def fmt_e(v):
    """3.99e-07 the way the page writes it, with a real minus sign."""
    return f"{v:.3e}".replace("e-0", "e−0").replace("e+0", "e+")


def bars(x, top, pw, ph, rows, peak, unit, title, sub, colours):
    """A horizontal bar panel. 7.3's helper, with the fixed inset it earned."""
    out = [label(x + pw / 2, top - 30, title, cls="sm"),
           label(x + pw / 2, top - 14, sub, cls="xs muted"),
           frame(x, top, pw, ph)]
    slot = ph / len(rows)
    for i, (name, value) in enumerate(rows):
        y = top + i * slot + 20.0
        hgt = slot * 0.34
        width = (pw - 96) * (value / peak)
        out.append(box(x + 8, y, max(width, 1.5), hgt, colours[i], opacity=0.85,
                       width=1.0, rx=2))
        out.append(label(x + 8, y - 7, name, cls="xs muted", anchor="start"))
        out.append(label(x + 12 + max(width, 1.5), y + hgt - 3, unit(value),
                         cls="xs mono", anchor="start"))
    return out


def render_panel(ppm, crop, x, y, px, cell=3, levels=3):
    """A real render, downsampled by PEAK and run-length encoded to rectangles.

    `peak_sample` and not `box_sample`: this mode's picture is almost entirely
    one-pixel debug lines, and averaging one inside a 3x3 block costs it eight
    ninths of its brightness. Returns the elements and the panel's size in page
    units, because a figure's height must be COMPUTED from both columns.
    """
    _w, _h, data = read_ppm(os.path.join(OUT, ppm))
    grid_w, grid_h, grid = peak_sample(data, _w, crop, cell)
    body = rle_rects(grid, grid_w, grid_h, x, y, px, DEMO_PALETTE, levels=levels,
                     bg_class="fill-shot")
    return body, grid_w * px, grid_h * px


# ===========================================================================
# Figure 1 — three notations, one formula  (§2)
# ===========================================================================
# ONE CLAIM: nothing in this lesson's slerp is new. 7.2 wrote these three steps
# for mat3 and 7.3 wrote them for complex; substituting `quat` is the whole of
# the implementation, and the two older versions are kept as instruments that
# measure the new one.
def fig_one_road():
    uid = "f75a"
    W, H = 760, 396
    b = [cmarker(uid, "gold", GOLD), cmarker(uid, "blue", GHOST_B)]

    b.append(label(W / 2, 22, "slerp(a, b, t)  =  a · (a⁻¹ b)ᵗ", cls="sm mono t-hi"))
    b.append(label(W / 2, 40, "undo a  —  take t of the difference  —  redo a",
                   cls="xs muted"))

    # The three steps as a chain, once, above the three columns.
    step_w, gap = 150.0, 36.0
    total = step_w * 3 + gap * 2
    x0 = (W - total) / 2
    steps = [("a⁻¹ b", "the single turn from a to b"),
             ("( · )ᵗ", "a fraction of that turn"),
             ("a · ( · )", "start where a is")]
    for i, (op, why) in enumerate(steps):
        x = x0 + i * (step_w + gap)
        b.append(box(x, 56, step_w, 44, None, rx=4))
        b.append(label(x + step_w / 2, 78, op, cls="sm mono t-hi"))
        b.append(label(x + step_w / 2, 93, why, cls="xs muted"))
        if i < 2:
            b.append(carrow(x + step_w + 4, 78, x + step_w + gap - 4, 78,
                            uid, "gold", GOLD, width=1.5))

    # Three columns: the same three steps, in three algebras.
    col_w = 226.0
    cx0 = (W - (col_w * 3 + 18 * 2)) / 2
    cols = [
        ("Lesson 7.2  —  mat3", [
            ("a⁻¹", "transpose(A)"),
            ("the turn", "axis_angle_from_rotation"),
            ("the power", "scale the angle, Rodrigues"),
        ], "rotation_slerp", "9 floats, 3 trig calls"),
        ("Lesson 7.3  —  complex", [
            ("a⁻¹", "conjugate(z)"),
            ("the turn", "conj(a) * b"),
            ("the power", "complex_from_angle(t · arg)"),
        ], "complex_slerp", "2 floats, 1 atan2"),
        ("Lesson 7.5  —  quat", [
            ("a⁻¹", "conjugate(q)"),
            ("the turn", "conj(a) * nearest(a, b)"),
            ("the power", "quat_pow_unit"),
        ], "quat_slerp", "4 floats, 1 atan2"),
    ]
    top = 126.0
    row_h = 30.0
    body_h = 26 + row_h * 3 + 30
    for i, (title, rows, fn, note) in enumerate(cols):
        x = cx0 + i * (col_w + 18)
        live = (i == 2)
        b.append(hollow(x, top, col_w, body_h, GHOST_B if live else GREY,
                        width=1.4 if live else 1.0))
        b.append(label(x + col_w / 2, top + 17, title,
                       cls="xs t-hi" if live else "xs muted"))
        for j, (k, v) in enumerate(rows):
            y = top + 26 + row_h * j + 18
            b.append(label(x + 10, y, k, cls="xs muted", anchor="start"))
            b.append(label(x + col_w - 10, y, v, cls="xs mono", anchor="end"))
        b.append(rule(x + 8, top + 26 + row_h * 3 + 4, x + col_w - 8,
                      top + 26 + row_h * 3 + 4))
        b.append(label(x + 10, top + body_h - 9, fn, cls="xs mono t-hi", anchor="start"))
        b.append(label(x + col_w - 10, top + body_h - 9, note, cls="xs muted", anchor="end"))

    # The measurement, outside the columns.
    my = top + body_h + 30
    b.append(label(W / 2, my - 8, "and the three are the same function, measured",
                   cls="sm"))
    facts = [(f"7.2's mat3 slerp vs ours, worst matrix entry", fmt_e(AGREE_MAT3)),
             (f"7.3's complex slerp vs ours, worst angle", fmt_e(AGREE_COMPLEX) + " rad")]
    for j, (k, v) in enumerate(facts):
        y = my + 16 + j * 17
        b.append(label(cx0 + 10, y, k, cls="xs muted", anchor="start"))
        b.append(label(cx0 + col_w * 3 + 36 - 10, y, v, cls="xs mono t-ok", anchor="end"))

    y = my + 16 + 2 * 17 + 18
    b.append(label(W / 2, y,
                   "CONTROL  —  average the two matrices entrywise instead: "
                   f"det = {AVERAGE_DET:.5f}, not 1",
                   cls="xs t-bad"))
    b.append(label(W / 2, y + 15,
                   "a matrix cannot be interpolated at all, which is why the storage had to change",
                   cls="xs muted"))
    return svg(uid, W, H, "One formula in three algebras",
               "The three steps of slerp shown once, then the same three steps "
               "written for mat3, for complex numbers and for quaternions, with "
               "the measured agreement between them.", b)


# ===========================================================================
# Figure 2 — one road, two rulers  (§5)
# ===========================================================================
def fig_arc_chord():
    uid = "f75b"
    W, H = 760, 444
    b = [cmarker(uid, "gold", GOLD)]

    cx, cy, R = 250.0, 236.0, 150.0
    omega = 110.0 * D          # the sphere arc drawn, in radians
    start = -125.0 * D         # where `a` sits, measured from +x, screen-down y

    def on_circle(ang, r=1.0):
        return (cx + R * r * math.cos(ang), cy + R * r * math.sin(ang))

    b.append(label(cx, 26, "the great circle a and b span", cls="sm"))
    b.append(label(cx, 42, "drawn flat: it is a circle whatever it is embedded in",
                   cls="xs muted"))

    # The circle, the centre, and the two endpoints.
    b.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="none" class="grid" '
             f'stroke-width="1.1"/>')
    pa = on_circle(start)
    pb = on_circle(start + omega)
    b.append(rule(cx, cy, pa[0], pa[1], dash="3 3"))
    b.append(rule(cx, cy, pb[0], pb[1], dash="3 3"))
    b.append(cline(pa[0], pa[1], pb[0], pb[1], GOLD, width=1.6))

    for p, name in ((pa, "a"), (pb, "b")):
        b.append(f'<circle cx="{p[0]:.2f}" cy="{p[1]:.2f}" r="4" fill="{GHOST_B}"/>')
    b.append(label(pa[0] - 16, pa[1] + 6, "a", cls="sm mono t-hi"))
    b.append(label(pb[0] + 16, pb[1] + 2, "b", cls="sm mono t-hi"))

    # The angle mark.
    b.append(f'<path d="M {cx + 42 * math.cos(start):.2f},{cy + 42 * math.sin(start):.2f} '
             f'A 42,42 0 0 1 {cx + 42 * math.cos(start + omega):.2f},'
             f'{cy + 42 * math.sin(start + omega):.2f}" fill="none" class="ink-soft" '
             f'stroke-width="1.2"/>')
    b.append(label(cx + 62 * math.cos(start + omega / 2),
                   cy + 62 * math.sin(start + omega / 2) + 4,
                   "Ω = θ/2", cls="xs mono"))

    # Eleven ticks on each schedule.
    for i in range(11):
        t = i / 10.0
        # slerp: equal angles.
        a_s = start + omega * t
        o0 = on_circle(a_s, 1.0)
        o1 = on_circle(a_s, 1.10)
        b.append(cline(o0[0], o0[1], o1[0], o1[1], GHOST_B, width=1.6))
        # nlerp: equal steps ALONG THE CHORD, projected back out.
        px = pa[0] + (pb[0] - pa[0]) * t
        py = pa[1] + (pb[1] - pa[1]) * t
        a_n = math.atan2(py - cy, px - cx)
        n0 = on_circle(a_n, 0.90)
        n1 = on_circle(a_n, 1.0)
        b.append(cline(n0[0], n0[1], n1[0], n1[1], GHOST_A, width=1.6))
        # the chord mark itself, small, so the construction is visible
        b.append(f'<circle cx="{px:.2f}" cy="{py:.2f}" r="1.7" fill="{GOLD}"/>')

    b.append(label(cx, cy + R + 32, "outward ticks: slerp   ·   inward ticks: nlerp",
                   cls="xs muted"))
    b.append(label(cx, cy + R + 46, "gold: the chord, and eleven equal steps along it",
                   cls="xs muted"))

    # The right-hand column: the three sentences the picture makes.
    px0 = 508.0
    pw = 224.0
    notes = [
        ("The path is identical.",
         "Every chord point is a positive multiple of a "
         "point of the a–b plane, and normalising moves a "
         "point along its own radius. It cannot leave."),
        ("The schedule is not.",
         "Equal steps along a chord subtend unequal angles "
         "at the centre — small at the ends, large in the "
         "middle. That is the entire difference."),
        ("The midpoint is exact.",
         "The chord's midpoint is equidistant from both "
         "ends, and so is the arc's, and on a circle there "
         "is only one such point on the short side."),
    ]
    y = 78.0
    for title, text in notes:
        lines = []
        words = text.split()
        line = ""
        for w in words:
            if len(line) + len(w) + 1 > 38:
                lines.append(line)
                line = w
            else:
                line = (line + " " + w).strip()
        lines.append(line)
        h = 22 + 13 * len(lines) + 8
        b.append(box(px0, y, pw, h, None, rx=4))
        b.append(label(px0 + 10, y + 17, title, cls="xs t-hi", anchor="start"))
        for j, ln in enumerate(lines):
            b.append(label(px0 + 10, y + 32 + j * 13, ln, cls="xs muted", anchor="start"))
        y += h + 12

    b.append(label(px0 + pw / 2, y + 6,
                   f"measured departure from the plane: {fmt_e(OFF_PLANE)}",
                   cls="xs mono t-ok"))
    return svg(uid, W, H, "One road, two rulers",
               "A great circle with two endpoints, the chord between them, and "
               "eleven equally spaced values of t marked twice: once by slerp, "
               "which spaces them equally, and once by nlerp, which bunches them "
               "at the ends.", b)


# ===========================================================================
# Figure 3 — the schedule error, measured  (§5)
# ===========================================================================
def fig_schedule():
    uid = "f75c"
    W, H = 760, 380
    b = [cmarker(uid, "amber", GHOST_A), cmarker(uid, "blue", GHOST_B)]

    # ---- left: the gap against the arc ------------------------------------
    px, py, pw, ph = 56.0, 76.0, 316.0, 216.0
    b.append(label(px + pw / 2, 30, "how far nlerp is from slerp, at the worst t",
                   cls="sm"))
    b.append(label(px + pw / 2, 46, "degrees of pose, against the rotation arc",
                   cls="xs muted"))
    b.append(frame(px, py, pw, ph))

    def gx(arc):
        return px + pw * arc / 180.0

    def gy(gap):
        return py + ph - ph * gap / 9.0

    for arc in (0, 45, 90, 135, 180):
        b.append(rule(gx(arc), py, gx(arc), py + ph, dash="2 4"))
        b.append(label(gx(arc), py + ph + 14, str(arc), cls="xs mono muted"))
    for g in (0, 2, 4, 6, 8):
        b.append(rule(px, gy(g), px + pw, gy(g), dash="2 4"))
        b.append(label(px - 8, gy(g) + 4, str(g), cls="xs mono muted", anchor="end"))

    pts = [(gx(r[0]), gy(r[2])) for r in SCHEDULE]
    # The curve is smooth between the measured points; sample it in Python so the
    # line through them is the function and not six straight segments.
    curve = []
    for k in range(0, 181):
        arc = float(k)
        best = 0.0
        O = math.radians(arc) * 0.5
        for j in range(0, 501):
            t = j / 500.0
            phi = math.atan2(t * math.sin(O), (1 - t) + t * math.cos(O))
            best = max(best, abs(phi - t * O))
        curve.append((gx(arc), gy(math.degrees(best) * 2.0)))
    b.append(poly(curve, GHOST_A, width=1.6, close=False))
    for x, y in pts:
        b.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="3.2" fill="{GHOST_B}"/>')

    b.append(label(px + pw - 6, gy(8.1491) - 10, "8.1491° at 180°",
                   cls="xs mono t-hi", anchor="end"))
    b.append(label(px + pw / 2, py + ph + 30, "rotation arc, degrees", cls="xs muted"))

    # ---- right: the table -------------------------------------------------
    tx = 418.0
    tw = 292.0
    b.append(label(tx + tw / 2, 30, "and the speed ratio it comes from", cls="sm"))
    b.append(label(tx + tw / 2, 46, "sec²(Ω/2), derived in §5.3", cls="xs muted"))
    rows = [("rot arc", "Ω", "gap", "sec²(Ω/2)")]
    rows += [(f"{r[0]:.0f}°", f"{r[1]:.0f}°", f"{r[2]:.4f}°", f"{r[4]:.4f}")
             for r in SCHEDULE]
    rh = 22.0
    b.append(hollow(tx, 62, tw, rh * len(rows) + 8, GREY, width=1.0))
    colx = [tx + 14, tx + 88, tx + 176, tx + tw - 14]
    for i, r in enumerate(rows):
        y = 62 + 8 + rh * i + 10
        cls = "xs mono" if i else "xs muted"
        if i:
            b.append(rule(tx + 8, y - 15, tx + tw - 8, y - 15))
        for j, cell in enumerate(r):
            anchor = "end" if j == 3 else "start"
            b.append(label(colx[j], y, cell, cls=cls, anchor=anchor))

    ty = 62 + rh * len(rows) + 8 + 26
    b.append(label(tx, ty, "The bottom row is the worst case that exists.",
                   cls="xs t-hi", anchor="start"))
    # THE FIGURE NUMBER IS THE PAGE'S, NOT THE FILE'S. Figures 4 and 5 were
    # drafted in the opposite order to the page and their filenames swapped; this
    # cross-reference was written before the swap and pointed at the wrong one.
    # Nothing in the pipeline can catch a wrong figure NUMBER inside a figure.
    for j, ln in enumerate([
            "A rotation arc cannot exceed 180° once nearest has",
            "chosen the hemisphere, so Ω cannot exceed 90° and",
            "sec²(Ω/2) cannot exceed sec²(45°) = 2, exactly.",
            "Figure 5 measures that bound over 20,000 pairs."]):
        b.append(label(tx, ty + 16 + j * 13, ln, cls="xs muted", anchor="start"))
    return svg(uid, W, H, "The schedule error against the arc",
               "The worst gap between nlerp and slerp plotted against the "
               "rotation arc, with the six measured points on the derived curve "
               "and the table of speed ratios beside it.", b)


# ===========================================================================
# Figure 5 — the double cover pays a dividend  (§6.4)   [file l75_fig5.svg]
# ===========================================================================
def fig_cap():
    uid = "f75d"
    W, H = 760, 400
    b = [cmarker(uid, "mag", MAGENTA), cmarker(uid, "blue", GHOST_B)]

    b.append(label(W / 2, 24, "the thing that looked like a nuisance is what bounds the error",
                   cls="sm"))

    # ---- left: the hemisphere argument ------------------------------------
    cx, cy, R = 190.0, 216.0, 122.0
    b.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="none" class="grid" '
             f'stroke-width="1.1"/>')
    b.append(rule(cx - R - 14, cy, cx + R + 14, cy, dash="4 4"))
    b.append(label(cx + R + 20, cy + 4, "a·q = 0", cls="xs mono muted", anchor="start"))

    ang_a = -90.0 * D
    pa = (cx + R * math.cos(ang_a), cy + R * math.sin(ang_a))
    b.append(f'<circle cx="{pa[0]:.2f}" cy="{pa[1]:.2f}" r="4.5" fill="{GHOST_B}"/>')
    b.append(label(pa[0], pa[1] - 12, "a", cls="sm mono t-hi"))

    ang_b = 34.0 * D
    pb = (cx + R * math.cos(ang_b), cy + R * math.sin(ang_b))
    pnb = (cx - R * math.cos(ang_b), cy - R * math.sin(ang_b))
    b.append(f'<circle cx="{pb[0]:.2f}" cy="{pb[1]:.2f}" r="4.5" fill="{MAGENTA}"/>')
    b.append(f'<circle cx="{pnb[0]:.2f}" cy="{pnb[1]:.2f}" r="4.5" fill="{GHOST_B}"/>')
    b.append(label(pb[0] + 14, pb[1] + 5, "b", cls="sm mono", anchor="start"))
    b.append(label(pnb[0] - 14, pnb[1] - 6, "−b", cls="sm mono", anchor="end"))
    b.append(rule(pb[0], pb[1], pnb[0], pnb[1], dash="2 4"))

    # The two arcs.
    def arc_path(a0, a1, r, sweep):
        return (f'<path d="M {cx + r * math.cos(a0):.2f},{cy + r * math.sin(a0):.2f} '
                f'A {r},{r} 0 0 {sweep} {cx + r * math.cos(a1):.2f},'
                f'{cy + r * math.sin(a1):.2f}" fill="none" ')

    b.append(arc_path(ang_a, pnb and math.atan2(pnb[1] - cy, pnb[0] - cx), R * 0.86, 1)
             + f'stroke="{GHOST_B}" stroke-width="2.2"/>')
    b.append(arc_path(ang_a, ang_b, R * 0.72, 1)
             + f'stroke="{MAGENTA}" stroke-width="2.2" stroke-dasharray="4 3"/>')

    b.append(label(cx, cy + R + 26,
                   "b and −b are the SAME orientation. nearest takes the near one.",
                   cls="xs muted"))
    b.append(label(cx, cy + R + 41,
                   "so the arc actually walked never leaves one hemisphere: Ω ≤ 90°.",
                   cls="xs t-hi"))

    # ---- right: the two columns -------------------------------------------
    tx = 392.0
    tw = 340.0
    rows = [("", "the plane (7.3)", "quaternions (7.5)"),
            ("largest Ω", "180°", "90°"),
            ("worst sec²(Ω/2)", "13131.56", "2.0000"),
            ("worst gap at one t", "26.34°", "8.1491°"),
            ("why", "no double cover", "b and −b agree")]
    rh = 27.0
    b.append(hollow(tx, 58, tw, rh * len(rows) + 10, GREY, width=1.0))
    for i, r in enumerate(rows):
        y = 58 + 10 + rh * i + 12
        if i:
            b.append(rule(tx + 8, y - 17, tx + tw - 8, y - 17))
        b.append(label(tx + 12, y, r[0], cls="xs muted" if i else "xs", anchor="start"))
        b.append(label(tx + tw - 132, y, r[1],
                       cls="xs mono muted" if i else "xs muted", anchor="end"))
        b.append(label(tx + tw - 12, y, r[2],
                       cls="xs mono t-hi" if i else "xs t-hi", anchor="end"))

    my = 58 + rh * len(rows) + 10 + 28
    b.append(label(tx, my, "measured over 20,000 uniformly sampled pairs",
                   cls="xs", anchor="start"))
    facts = [("largest sphere arc seen", f"{MAX_SPHERE_ARC:.4f}°", "bound 90"),
             ("largest gap seen", f"{MAX_GAP:.4f}°", "bound 8.1491"),
             ("CONTROL: same pairs, no nearest", f"{RAW_MAX_ARC:.4f}°", "unbounded")]
    for j, (k, v, note) in enumerate(facts):
        y = my + 18 + j * 16
        cls = "xs mono t-bad" if j == 2 else "xs mono t-ok"
        b.append(label(tx, y, k, cls="xs muted", anchor="start"))
        b.append(label(tx + tw - 74, y, v, cls=cls, anchor="end"))
        b.append(label(tx + tw, y, note, cls="xs muted", anchor="end"))
    return svg(uid, W, H, "The double cover as a bound",
               "The hemisphere argument on the left and, on the right, the "
               "resulting caps on the sphere arc, the speed ratio and the "
               "positional gap, measured over twenty thousand random pairs.", b)


# ===========================================================================
# Figure 4 — one missing comparison  (§6.3)   [file l75_fig4.svg]
# ===========================================================================
def fig_sign():
    uid = "f75e"
    W, H = 760, 348
    b = [cmarker(uid, "mag", MAGENTA), cmarker(uid, "blue", GHOST_B)]

    b.append(label(W / 2, 24, "two keyframes one degree apart, signed the other way",
                   cls="sm"))
    b.append(label(W / 2, 40,
                   "an exporter has no reason to make the signs consistent, and half of them are not",
                   cls="xs muted"))

    cx, cy, R = 200.0, 196.0, 108.0
    b.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="none" class="grid" '
             f'stroke-width="1.1"/>')
    a0 = -96.0 * D
    a1 = -96.0 * D + 14 * D           # drawn wide; the real gap is 1 degree
    pa = (cx + R * math.cos(a0), cy + R * math.sin(a0))
    pb = (cx + R * math.cos(a1), cy + R * math.sin(a1))
    b.append(f'<circle cx="{pa[0]:.2f}" cy="{pa[1]:.2f}" r="4.5" fill="{GHOST_B}"/>')
    b.append(f'<circle cx="{pb[0]:.2f}" cy="{pb[1]:.2f}" r="4.5" fill="{GHOST_B}"/>')
    b.append(label(pa[0] - 12, pa[1] - 10, "k₀", cls="xs mono t-hi"))
    b.append(label(pb[0] + 14, pb[1] - 10, "k₁", cls="xs mono t-hi"))

    # THE LONG WAY, SAMPLED RATHER THAN DRAWN AS AN SVG ARC. The first version
    # used an `A` command whose two endpoints sat at different radii; when the
    # chord is longer than 2r the renderer SCALES THE RADII UP, so it swept a
    # much larger circle than intended and ran under the figure's heading.
    # check-page.js caught it as text-on-shape. A polyline has no implicit
    # geometry and nothing to get wrong.
    long_pts = []
    for k in range(0, 97):
        ang = a1 + (a0 + 2 * math.pi - a1) * k / 96.0
        long_pts.append((cx + R * 0.88 * math.cos(ang), cy + R * 0.88 * math.sin(ang)))
    b.append(poly(long_pts, MAGENTA, width=2.2, close=False))
    b.append(cline(pa[0], pa[1], pb[0], pb[1], GHOST_B, width=2.4))
    b.append(label(cx, cy + 4, "359°", cls="sm mono t-bad"))
    b.append(label(cx, cy + 20, "the long way", cls="xs muted"))
    b.append(label(cx, cy + R + 34,
                   "drawn at 14° so the two points are distinguishable; the real gap is 1°",
                   cls="xs muted"))

    # right: the numbers
    tx = 376.0
    tw = 356.0
    rows = [("separation, angle_between", f"{KEYFRAME[0]:.4f}°", "t-ok"),
            ("quat_slerp travels", f"{KEYFRAME[1]:.4f}°", "t-ok"),
            ("CONTROL: the same, without nearest", f"{KEYFRAME[2]:.4f}°", "t-bad"),
            ("pairs needing the flip, of 20,000", f"{FLIPS} ({FLIP_PCT}%)", "mono"),
            ("longest journey with nearest", f"{LONGEST_WITH:.4f}°", "t-ok"),
            ("CONTROL: longest without it", f"{LONGEST_WITHOUT:.4f}°", "t-bad")]
    rh = 26.0
    b.append(hollow(tx, 62, tw, rh * len(rows) + 10, GREY, width=1.0))
    for i, (k, v, cls) in enumerate(rows):
        y = 62 + 10 + rh * i + 12
        if i:
            b.append(rule(tx + 8, y - 16, tx + tw - 8, y - 16))
        b.append(label(tx + 12, y, k, cls="xs muted", anchor="start"))
        b.append(label(tx + tw - 12, y, v, cls=f"xs mono {cls}", anchor="end"))

    ny = 62 + rh * len(rows) + 10 + 26
    b.append(label(tx, ny, "nlerp’s version of the same failure is worse:",
                   cls="xs t-hi", anchor="start"))
    b.append(label(tx, ny + 15,
                   f"the raw chord passes within {RAW_CHORD_MIN:.5f} of the ORIGIN,",
                   cls="xs muted", anchor="start"))
    b.append(label(tx, ny + 28,
                   "where there is no direction left to normalise.",
                   cls="xs muted", anchor="start"))
    return svg(uid, W, H, "One missing comparison",
               "Two nearly identical keyframes stored with opposite signs, the "
               "359-degree journey that results, and the measured cost of "
               "omitting the shortest-arc comparison.", b)


# ===========================================================================
# Figure 6 — the demo, twice  (§7)
# ===========================================================================
def fig_render():
    uid = "f75f"
    W = 760
    PAD, CAP, PX = 14.0, 70.0, 2.03
    CROP = (216, 26, 744, 514)
    left, LW, LH = render_panel("l75_sched23.ppm", CROP, PAD, CAP, PX)
    right, RW, _ = render_panel("l75_sched50.ppm", CROP, PAD + LW + 14, CAP, PX)
    H = CAP + LH + 78
    b = list(left) + list(right)

    b.insert(0, label(W / 2, 24, "the same mode at two values of t, from a real render",
                      cls="sm"))
    b.insert(1, label(W / 2, 40,
                      "outward ticks slerp, inward ticks nlerp, magenta the long way round",
                      cls="xs muted"))
    b.append(label(PAD + LW / 2, CAP + LH + 22, "t = 0.229", cls="xs mono t-hi"))
    b.append(label(PAD + LW / 2, CAP + LH + 37,
                   f"{GAP_AT_QUARTER:.4f}° apart — the worst this arc gets",
                   cls="xs muted"))
    b.append(label(PAD + LW + 14 + RW / 2, CAP + LH + 22, "t = 0.500", cls="xs mono t-hi"))
    b.append(label(PAD + LW + 14 + RW / 2, CAP + LH + 37,
                   f"{GAP_AT_HALF:.4f}° — one glyph: they agree EXACTLY",
                   cls="xs muted"))
    b.append(label(W / 2, CAP + LH + 62,
                   "a test sampling t = 0, 0.5 and 1 certifies nlerp AS slerp — "
                   "those are the three values where it is one",
                   cls="xs t-bad"))
    return svg(uid, W, int(H), "The schedule mode at two values of t",
               "Two real renders of the gimbal demo's schedule mode, one at "
               "t = 0.229 where the two interpolations are furthest apart and "
               "one at t = 0.5 where they coincide exactly.", b)


# ===========================================================================
# Figure 7 — the instrument  (§7.3)
# ===========================================================================
def fig_metric():
    uid = "f75g"
    W, H = 760, 388
    b = [cmarker(uid, "amber", GHOST_A), cmarker(uid, "blue", GHOST_B)]

    b.append(label(W / 2, 24, "why the metric had to be rewritten before anything could be measured",
                   cls="sm"))

    # ---- left: flat cosine, linear sine -----------------------------------
    px, py, pw, ph = 52.0, 66.0, 300.0, 196.0
    b.append(frame(px, py, pw, ph))
    b.append(label(px + pw / 2, py - 10, "near zero, one of these is readable", cls="xs muted"))

    def cx_(u):     # u in [0, 1] maps to omega in [0, 0.25 rad]
        return px + pw * u

    def cy_(v):     # v in [0, 1]
        return py + ph - ph * v

    cos_pts, sin_pts = [], []
    for k in range(0, 201):
        u = k / 200.0
        om = u * 0.25
        cos_pts.append((cx_(u), cy_((1.0 - math.cos(om)) / (1.0 - math.cos(0.25)))))
        sin_pts.append((cx_(u), cy_(math.sin(om) / math.sin(0.25))))
    b.append(poly(sin_pts, GHOST_B, width=1.8, close=False))
    b.append(poly(cos_pts, GHOST_A, width=1.8, close=False))

    # the float resolution band
    # The band is where 1 - cos(Omega) sits under half an ulp, drawn at the
    # plot's own vertical scale and CLAMPED: the honest height here is a fraction
    # of a pixel, and an invisible band explains nothing. The label goes OUTSIDE
    # the plot — the first version sat it on the cosine curve, which
    # check-page.js reports as text-on-shape and a reader reads as a smudge.
    band = min(ph * 0.11, max(ph * (1.1920929e-7 / (1.0 - math.cos(0.25))) * 4e4, 9.0))
    b.append(box(px, py + ph - band, pw, band, GHOST_A,
                 opacity=0.13, width=0.8, rx=0))
    b.append(label(px + pw / 2, py + ph + 17, "Ω, radians, 0 to 0.25", cls="xs muted"))
    b.append(label(px + pw / 2, py + ph + 32,
                   "shaded (height exaggerated): everything here rounds to a dot of exactly 1",
                   cls="xs muted"))
    # THE LEGEND GOES OUTSIDE THE PLOT. Both curve labels started inside it and
    # both had to dodge something: one collided with the sub-title, the other sat
    # on the cosine it was naming. A label that cannot find a clear spot beside
    # its own curve is a legend entry.
    for k, (colour, text) in enumerate(((GHOST_B, "|v| = sin Ω   (linear in the angle)"),
                                        (GHOST_A, "1 − cos Ω   (quadratic)"))):
        ly = py + ph + 50 + k * 15
        b.append(cline(px + 6, ly - 4, px + 26, ly - 4, colour, width=2.0))
        b.append(label(px + 32, ly, text, cls="xs mono muted", anchor="start"))

    # ---- right: the path integral -----------------------------------------
    tx = 398.0
    tw = 334.0
    b.append(label(tx + tw / 2, py - 10,
                   "slerp's own geodesic, summed 4,096 times", cls="xs muted"))
    rows = [("endpoints, straight", PATH_ATAN),
            ("atan2 form (7.5)", PATH_ATAN),
            ("acos form (7.4)", PATH_ACOS)]
    b.extend(bars(tx, py + 12, tw, 132, rows, 170.0,
                  lambda v: f"{v:.4f}°", "", "",
                  [GREY, GHOST_B, GHOST_A]))
    ny = py + 12 + 132 + 26
    b.append(label(tx, ny,
                   f"the acos form scores slerp at −41.10% of its own arc",
                   cls="xs t-bad", anchor="start"))
    for j, ln in enumerate([
            "Each step is 0.037°, and the acos form's floor is",
            f"2·√(2·eps) = {ACOS_FLOOR:.4f}°. Every step is below it, so every",
            "step is quantised DOWN, and 4,096 of them accumulate.",
            "The error is biased, not noisy, which is why it does",
            "not average out."]):
        b.append(label(tx, ny + 16 + j * 13, ln, cls="xs muted", anchor="start"))
    return svg(uid, W, H, "The metric, and why it was rewritten",
               "The cosine is flat near zero where the sine is linear, so a "
               "dot-product metric loses small angles; the path integral on the "
               "right shows the consequence.", b)


# ===========================================================================
# Figure 8 — what it costs  (§7.6)
# ===========================================================================
def fig_cost():
    uid = "f75h"
    W, H = 760, 300
    b = []
    b.append(label(W / 2, 24, "what an interpolation costs, and the arc below which the cheap one is free",
                   cls="sm"))

    px, pw = 46.0, 330.0
    b.extend(bars(px, 76, pw, 132, [(n, v) for n, v in COST], 50.0,
                  lambda v: f"{v:.3f} ns", "nanoseconds per call",
                  "best of three, -O2, inputs varied per repetition",
                  [GHOST_A, GHOST_B, GREY]))

    tx = 420.0
    tw = 300.0
    b.append(label(tx + tw / 2, 46, "and the number to design with", cls="sm"))
    b.append(box(tx, 64, tw, 74, None, rx=4))
    b.append(label(tx + tw / 2, 92, f"{BUDGET_ARC:.2f}°", cls="mono t-hi"))
    b.append(label(tx + tw / 2, 112, "the arc below which nlerp stays", cls="xs muted"))
    b.append(label(tx + tw / 2, 126, "within half a degree of slerp", cls="xs muted"))
    for j, ln in enumerate([
            "A 30 Hz clip's adjacent keyframes are far below that,",
            "which is why a great deal of shipped animation code",
            "uses nlerp and is right to. A blend between two poses",
            "chosen by gameplay is not, and 8.15° of lag through",
            "the middle of a turn is visible."]):
        b.append(label(tx, 160 + j * 14, ln, cls="xs muted", anchor="start"))
    b.append(label(px + pw / 2, 240,
                   "slerp is 3.42× nlerp; 7.2's matrix route is 2.81× slerp",
                   cls="xs muted"))
    b.append(label(px + pw / 2, 256,
                   "the representation is cheaper than the one it replaced, at the same job",
                   cls="xs t-ok"))
    return svg(uid, W, H, "The cost of an interpolation",
               "Nanoseconds per call for the three interpolations this course "
               "has written, and the arc below which nlerp is indistinguishable "
               "from slerp.", b)


# ===========================================================================
# Figure 9 — the swap  (§8)
# ===========================================================================
def fig_swap():
    uid = "f75i"
    W, H = 760, 400
    b = [cmarker(uid, "gold", GOLD)]
    b.append(label(W / 2, 24, "the storage swap this header has promised since Module 2",
                   cls="sm"))

    # ---- the two structs --------------------------------------------------
    cw, ch = 300.0, 128.0
    lx, rx_ = 46.0, 414.0
    for x, title, fields, live in ((lx, "before — Modules 2 to 7.4",
                                    [("position", 3, GREY), ("rotation  mat3", 9, GHOST_A),
                                     ("scale", 3, GREY)], False),
                                   (rx_, "after — Lesson 7.5",
                                    [("position", 3, GREY), ("rotation  quat", 4, GHOST_B),
                                     ("scale", 3, GREY)], True)):
        b.append(hollow(x, 50, cw, ch, GHOST_B if live else GREY,
                        width=1.4 if live else 1.0))
        b.append(label(x + cw / 2, 68, title, cls="xs t-hi" if live else "xs muted"))
        y = 82
        for name, n, colour in fields:
            cell = (cw - 118) / 9.0
            for k in range(n):
                b.append(box(x + 104 + k * cell, y, cell - 2, 15, colour,
                             opacity=0.8, width=0.8, rx=1))
            b.append(label(x + 96, y + 12, name, cls="xs mono muted", anchor="end"))
            y += 22
        total = sum(n for _, n, _ in fields)
        b.append(label(x + cw / 2, y + 14, f"{total} floats", cls="xs mono t-hi"))

    # ---- what it cost -----------------------------------------------------
    y0 = 196.0
    b.append(label(W / 2, y0, "what the compiler found when the type got narrower",
                   cls="sm"))
    rows = [(f"call sites the swap touched", f"{CALL_SITES}", "muted"),
            ("of them assigning something that was not a rotation", "3", "t-bad"),
            ("of them in the ENGINE", "1, shipping since 5.11", "t-bad"),
            ("mat3 → quat → mat3, worst entry over 20,000 poses",
             fmt_e(ROUND_TRIP), "t-ok"),
            ("which at 3 units out, 960 px wide, is", f"{ROUND_TRIP_PX:.5f} px", "t-ok"),
            ("golden render after the swap", f"identical, {GOLDEN_HASH}", "t-ok")]
    tw = 664.0
    tx = (W - tw) / 2
    rh = 22.0
    b.append(hollow(tx, y0 + 12, tw, rh * len(rows) + 10, GREY, width=1.0))
    for i, (k, v, cls) in enumerate(rows):
        y = y0 + 12 + 10 + rh * i + 11
        if i:
            b.append(rule(tx + 8, y - 14, tx + tw - 8, y - 14))
        b.append(label(tx + 12, y, k, cls="xs muted", anchor="start"))
        b.append(label(tx + tw - 12, y, v, cls=f"xs mono {cls}", anchor="end"))
    b.append(label(W / 2, y0 + 12 + rh * len(rows) + 34,
                   "a narrower type does not only prevent future mistakes —",
                   cls="xs t-hi"))
    b.append(label(W / 2, y0 + 12 + rh * len(rows) + 48, "it finds existing ones",
                   cls="xs t-hi"))
    return svg(uid, W, H, "The storage swap",
               "The transform struct before and after the swap, and a table of "
               "what the narrower type cost and found.", b)


# ===========================================================================
# Figure 10 — the node that could not hold a rotation  (§8)
# ===========================================================================
def fig_boom():
    uid = "f75j"
    W, H = 760, 392
    b = [cmarker(uid, "gold", GOLD), cmarker(uid, "blue", GHOST_B)]
    b.append(label(W / 2, 24, "one node could only pretend; two hold it exactly",
                   cls="sm"))
    b.append(label(W / 2, 40,
                   "a transform is T · R · S, so it can say “rotate then scale” "
                   "and cannot say “scale then rotate”",
                   cls="xs muted"))

    # ---- the algebra ------------------------------------------------------
    b.append(label(W / 2, 72, "rover world  =  H · Rz(bank) · S", cls="xs mono"))
    b.append(label(W / 2, 90,
                   "boom local  =  (H · Rz(bank) · S)⁻¹ · H  =  S⁻¹ · Rz(−bank)",
                   cls="xs mono t-hi"))

    # ---- one node vs two --------------------------------------------------
    top = 112.0
    cw, ch = 330.0, 122.0
    lx, rx_ = 46.0, 384.0

    b.append(hollow(lx, top, cw, ch, RED, width=1.3))
    b.append(label(lx + cw / 2, top + 18, "one node, until Lesson 7.5", cls="xs t-bad"))
    b.append(label(lx + cw / 2, top + 42, "rotation = S⁻¹ · Rz(−bank)", cls="xs mono"))
    for j, ln in enumerate([
            "A mat3 accepted it without comment. The columns are",
            "not perpendicular — it is a SHEARED basis living in a",
            "field called rotation, and it had been there since 5.12.",
            f"Through a quat: {BOOM_ONE_NODE_ERR:.4f} of entry error."]):
        b.append(label(lx + 12, top + 62 + j * 14, ln,
                       cls="xs muted" if j < 3 else "xs mono t-bad", anchor="start"))

    b.append(hollow(rx_, top, cw, ch, GHOST_B, width=1.4))
    b.append(label(rx_ + cw / 2, top + 18, "two nodes, Lesson 7.5", cls="xs t-hi"))
    b.append(label(rx_ + cw / 2, top + 40, "child A:  scale = S⁻¹", cls="xs mono"))
    b.append(label(rx_ + cw / 2, top + 56, "child B:  rotation = quat_z(−bank)",
                   cls="xs mono"))
    for j, ln in enumerate([
            "The hierarchy multiplies them parent-first, which is the",
            "order the derivation asked for. No shear anywhere, and",
            f"the same camera basis: {fmt_e(BOOM_CHAIN_ERR) if BOOM_CHAIN_ERR else '0.000e+00'} of difference."]):
        b.append(label(rx_ + 12, top + 76 + j * 14, ln,
                       cls="xs muted" if j < 2 else "xs mono t-ok", anchor="start"))

    # ---- and what stays outside -------------------------------------------
    y = top + ch + 34
    b.append(label(W / 2, y, "the case that is genuinely outside the struct", cls="sm"))
    rows = [("a non-uniformly scaled parent with a rotated child", "", "muted"),
            ("out of square, measured", f"{SHEAR_SQUARE:.4f}", "t-bad"),
            ("worst entry after the best repair either route can make",
             f"{SHEAR_ENTRY:.4f}", "t-bad"),
            ("what collect_renderables does about it",
             "counts report.skewed", "t-hi")]
    tw = 664.0
    tx = (W - tw) / 2
    rh = 20.0
    b.append(hollow(tx, y + 10, tw, rh * len(rows) + 8, GREY, width=1.0))
    for i, (k, v, cls) in enumerate(rows):
        yy = y + 10 + 8 + rh * i + 10
        if i:
            b.append(rule(tx + 8, yy - 13, tx + tw - 8, yy - 13))
        b.append(label(tx + 12, yy, k, cls="xs muted", anchor="start"))
        b.append(label(tx + tw - 12, yy, v, cls=f"xs mono {cls}", anchor="end"))
    return svg(uid, W, H, "The node that could not hold a rotation",
               "The collector demo's camera boom, which stored a sheared basis "
               "in a field called rotation, and the two-node chain that replaces "
               "it exactly.", b)


# ===========================================================================
def main():
    figures = [("l75_fig1.svg", fig_one_road),
               ("l75_fig2.svg", fig_arc_chord),
               ("l75_fig3.svg", fig_schedule),
               ("l75_fig4.svg", fig_sign),
               ("l75_fig5.svg", fig_cap),
               ("l75_fig6.svg", fig_render),
               ("l75_fig7.svg", fig_metric),
               ("l75_fig8.svg", fig_cost),
               ("l75_fig9.svg", fig_swap),
               ("l75_fig10.svg", fig_boom)]
    for name, fn in figures:
        text = fn()
        with open(os.path.join(OUT, name), "w") as fh:
            fh.write(text)
        print(f"wrote {OUT}/{name}  ({len(text.encode('utf-8')):,} bytes)")


if __name__ == "__main__":
    main()
