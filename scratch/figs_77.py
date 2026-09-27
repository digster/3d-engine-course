#!/usr/bin/env python3
"""scratch/figs_77.py — Lesson 7.7's diagrams.

Same rules as 5.1-7.6's:
  - computed coordinates, never hand-placed
  - no fill="..." on any <text>: CSS wins over presentation attributes
  - filenames numbered by PAGE ORDER
  - no HTML tags inside <text>; use <tspan class="t-hi">
  - `rule()` takes a CSS CLASS; `cline()` takes a COLOUR
  - LEGENDS AND ANNOTATION BOXES GO OUTSIDE THE PLOT
  - a SHAPE can leave the viewBox where a label cannot
  - panel heights are COMPUTED, never guessed
  - the figure palette must contain the DEMO's own colours (rle_rects SNAPS)

Every number below is transcribed from scratch/verify_77.log or from the rig
demo's own `--shot` receipt. Nothing here is estimated, and the section of the
harness each block came from is named above it.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figs_510 import (svg, box, hollow, label, arrow, rule, esc,   # noqa: E402
                      AMBER, BLUE, GREEN, RED, PURPLE, GREY)
from figs_610 import cline                                          # noqa: E402
from figs_45 import read_ppm, rle_rects, hexrgb, box_sample         # noqa: E402
from figs_71 import cmarker, carrow, poly, frame, D                 # noqa: E402
from figs_76 import (TUBE, J_AMBER, J_ROSE, J_VIOLET, J_TEAL,       # noqa: E402
                     J_CITRON, J_SAGE, AX_X, AX_Y, AX_Z, GHOST,
                     DEMO_PALETTE, render_panel, table, f, fmt_e)

OUT = "scratch"


# ===========================================================================
# Measured — every figure's data, with the line of verify_77.log it came from
# ===========================================================================

CHECKS = 39

# §I — what survived the reduction, joint by joint. (name, P, R, S, keys)
CHANNELS = [
    ("root", 1, 0, 0, 26), ("hips", 0, 1, 0, 6), ("spine1", 0, 1, 0, 4),
    ("spine2", 0, 1, 0, 4), ("chest", 0, 1, 0, 5), ("neck", 0, 1, 0, 4),
    ("head", 0, 0, 0, 0), ("clavicle.L", 0, 0, 0, 0), ("upperarm.L", 0, 1, 0, 14),
    ("forearm.L", 0, 1, 0, 11), ("hand.L", 0, 0, 0, 0), ("clavicle.R", 0, 0, 0, 0),
    ("upperarm.R", 0, 1, 0, 14), ("forearm.R", 0, 1, 0, 11), ("hand.R", 0, 0, 0, 0),
    ("thigh.L", 0, 1, 0, 19), ("shin.L", 0, 1, 0, 16), ("foot.L", 0, 1, 0, 11),
    ("toe.L", 0, 0, 0, 0), ("thigh.R", 0, 1, 0, 19), ("shin.R", 0, 1, 0, 16),
    ("foot.R", 0, 1, 0, 11), ("toe.R", 0, 0, 0, 0),
]
LIVE_CHANNELS, ALL_CHANNELS = 16, 69

# §A — three bracket strategies. (keys, two walks, walk+search, binary) in ns
STRATEGY = [(31, 1.548, 1.324, 3.626), (301, 1.729, 0.985, 10.183)]

# §C — the scale decision. (a, b, lerp mid, geom mid, gap %)
SCALE = [(1.0, 1.05, 1.02500, 1.02470, 0.030), (1.0, 1.25, 1.12500, 1.11803, 0.623),
         (1.0, 2.0, 1.50000, 1.41421, 6.066), (1.0, 4.0, 2.50000, 2.00000, 25.000),
         (1.0, 8.0, 4.50000, 2.82843, 59.099)]
# §C — 1 -> 8 split into n keys. (n, per-step ratio, gap %)
SPLITS = [(1, 8.00000, 59.099), (2, 2.82843, 13.820), (3, 2.00000, 6.066),
          (6, 1.41421, 1.505), (12, 1.18921, 0.376)]

# §D — arcs a clip contains. (deg/s, arc per key at 30 Hz, nlerp error deg)
KEY_ARCS = [(90, 3.00, 0.000033), (180, 6.00, 0.000264),
            (360, 12.00, 0.002112), (720, 24.00, 0.016921)]
# §D — arcs a BLEND can contain. (rotation arc deg, nlerp error deg)
BLEND_ARCS = [(30.0, 0.033089), (73.5, 0.495159), (90.0, 0.918789),
              (150.0, 4.513929)]
# quat.hpp's table, which is in SPHERE degrees. (sphere arc, sphere gap)
SPHERE_ARCS = [(30.0, 0.1337), (90.0, 4.0746), (150.0, 26.3423)]

# §B — wrapping. (t, wrap_time, fmod)
WRAP = [(-0.100, 0.9000, -0.1000), (-0.001, 0.9990, -0.0010), (0.000, 0.0, 0.0),
        (0.500, 0.5, 0.5), (1.000, 0.0, 0.0), (1.250, 0.25, 0.25),
        (2.750, 0.75, 0.75), (-3.400, 0.6000, -0.4000)]
# §B — what each wrongness costs. (t, u, extrapolated deg, frozen deg)
NEGATIVE = [(-0.033, -0.99, 0.015, 5.763), (-0.100, -3.00, 0.800, 16.458),
            (-0.300, -9.00, 21.995, 26.630), (-0.600, -18.00, 100.007, 16.458)]

# §E — the seam
SEAM_CLOSED_KEYS, SEAM_OPEN_KEYS = 31, 30
SEAM_CLOSED_DEG, SEAM_OPEN_DEG = 0.0000, 5.8215
ROOT_TRAVEL = 1.2000
SEAM_SHORT_DEG, SEAM_SHORT_OOR = 42.7780, 1035

# §F — the double cover. (t, error of a lerp with no `nearest`, degrees)
NAIVE = [(0.0333, 0.000), (0.0400, 61.148), (0.0500, 120.000),
         (0.0600, 155.279), (0.0666, 180.000)]
FLIPS_REPORTED, FLIPS_NEGATED, NAIVE_WORST = 690, 345, 180.000

# §G — cross-fade. (per-joint gap deg, pose chain, matrix chain, prediction)
FADE = [(0.0, 5.00000, 5.00000, 5.00000), (5.0, 5.00000, 4.97150, 4.97150),
        (10.0, 5.00000, 4.88662, 4.88662), (20.0, 5.00000, 4.55657, 4.55657),
        (30.0, 5.00000, 4.03906, 4.03906), (45.0, 5.00000, 3.01367, 3.01367)]
POSE_BONE_ERR, PREDICTION_GAP = 2.980e-07, 4.768e-07

# §H — reduction. (tolerance deg, keys, key bytes, error deg, header ratio)
REDUCE = [(0.00, 2139, 37076, 0.1534, 0.04), (0.05, 386, 7616, 0.1534, 0.22),
          (0.25, 287, 5636, 0.2553, 0.29), (0.50, 191, 3716, 0.5102, 0.45),
          (2.00, 104, 1976, 3.0000, 0.84), (8.00, 62, 1136, 7.8921, 1.46)]
KEPT_05 = [0.000, 0.100, 0.167, 0.200, 0.233, 0.267, 0.300, 0.333, 0.400, 0.500,
           0.600, 0.667, 0.700, 0.733, 0.767, 0.800, 0.833, 0.900, 1.000]
KEPT_20 = [0.000, 0.167, 0.267, 0.367, 0.633, 0.733, 0.833, 1.000]
# §H — fitting against a curve you will not play
SPIN_NLERP_KEYS, SPIN_NLERP_ERR = 3, 0.4651
SPIN_SLERP_KEYS, SPIN_SLERP_ERR = 2, 2.2340

# §I — bytes and nanoseconds
POSE_ARRAY, RAW_BYTES, LEAN_BYTES, CONTAINER = 28520, 37076, 3716, 1656
SCALE_NAIVE_MB, SCALE_LEAN_MB = 3.60, 0.48
SAMPLE_CURSOR_US, SAMPLE_BINARY_US, PALETTE_US = 0.159, 0.341, 0.240
LONG_CURSOR_US, LONG_BINARY_US = 0.172, 0.966
FMOD = [(1.0, 3.798, 7.726), (10.0, 1.603, 5.901)]

# demos/rig receipts (--clip --time 0.5 --fade 0.5 [--matrix-blend])
CHAIN_POSE, CHAIN_MATRIX = 5.00000, 4.65020
DEMO_ARC = 28.46
CURL_KEYS, CURL_BYTES, TWIST_KEYS, TWIST_BYTES = 60, 1200, 56, 1120


# ===========================================================================
# Figure 1 — the layout, and what it is for  (§3)
# ===========================================================================
# ONE CLAIM: a clip is not a sequence of poses, it is a bundle of independent
# channels — and the reason is that most of them are empty.
def fig_tracks():
    uid = "f77a"
    W = 760
    TOP = 98.0
    ROW = 13.0
    H = TOP + len(CHANNELS) * ROW + 108.0
    b = []

    b.append(label(W / 2, 30, "one clip, one second, twenty-three joints", cls="sm"))
    b.append(label(W / 2, 47,
                   "baked at 30 Hz into every channel, then reduced — the map is "
                   "what survived", cls="xs muted"))

    # ---- left: the pose-array layout ------------------------------------
    LX, LW = 34.0, 178.0
    b.append(frame(LX, TOP, LW, len(CHANNELS) * ROW))
    b.append(label(LX + LW / 2, TOP - 20, "a pose array", cls="xs"))
    b.append(label(LX + LW / 2, TOP - 6, "every joint, every frame", cls="xs muted"))
    for i in range(len(CHANNELS)):
        for k in range(31):
            cx = LX + 6.0 + (LW - 12.0) * k / 30.0
            b.append(f'<rect x="{f(cx - 1.4)}" y="{f(TOP + i * ROW + 4.0)}" '
                     f'width="2.8" height="{f(ROW - 8.0)}" fill="{J_VIOLET}" '
                     f'fill-opacity="0.55" stroke="none"/>')

    # ---- right: the per-channel layout ----------------------------------
    RX = 268.0
    NAME_W = 92.0
    CHAN_W = 356.0
    b.append(label(RX + NAME_W + CHAN_W / 2, TOP - 20, "per-channel tracks", cls="xs"))
    b.append(label(RX + NAME_W + CHAN_W / 2, TOP - 6,
                   "each channel keeps its own times", cls="xs muted"))
    b.append(frame(RX + NAME_W, TOP, CHAN_W, len(CHANNELS) * ROW))

    # The three sub-rows inside each joint row: position, rotation, scale.
    SUB = (ROW - 4.0) / 3.0
    for i, (name, p, r, s, keys) in enumerate(CHANNELS):
        y = TOP + i * ROW
        dim = "xs muted" if keys == 0 else "xs"
        b.append(label(RX + NAME_W - 8, y + ROW - 3.0, name, cls=dim, anchor="end"))
        for row, (live, colour) in enumerate(((p, J_AMBER), (r, J_TEAL), (s, J_ROSE))):
            sy = y + 2.0 + row * SUB
            if not live:
                b.append(rule(RX + NAME_W + 6, sy + SUB / 2, RX + NAME_W + CHAN_W - 6,
                              sy + SUB / 2, cls="grid", width=0.6, dash="1 4"))
                continue
            # The key COUNT is measured; the placement is even, because the
            # harness only reports the times for thigh.L (figure 9 draws those).
            n = keys if r or s else keys
            for k in range(n):
                cx = (RX + NAME_W + 8.0
                      + (CHAN_W - 16.0) * (k / max(n - 1, 1)))
                b.append(f'<rect x="{f(cx - 1.2)}" y="{f(sy)}" width="2.4" '
                         f'height="{f(SUB - 0.6)}" fill="{colour}" stroke="none"/>')

    y = TOP + len(CHANNELS) * ROW + 26
    for i, (colour, text) in enumerate(((J_AMBER, "position"), (J_TEAL, "rotation"),
                                        (J_ROSE, "scale"))):
        lx = RX + NAME_W + 8 + i * 96
        b.append(box(lx, y - 9, 10, 8, colour, width=0.8, rx=2))
        b.append(label(lx + 16, y, text, cls="xs muted", anchor="start"))

    b.append(label(LX, y, f"{31 * len(CHANNELS)} poses", cls="xs mono", anchor="start"))
    b.append(label(LX, y + 15, f"{POSE_ARRAY:,} bytes", cls="xs mono", anchor="start"))

    b.append(label(W / 2, y + 40,
                   f"{LIVE_CHANNELS} of {ALL_CHANNELS} channels carry anything at all; "
                   "every scale channel is gone, and so is",
                   cls="xs muted"))
    b.append(label(W / 2, y + 56,
                   f"every position but the root’s — {LEAN_BYTES:,} bytes "
                   f"against the pose array’s {POSE_ARRAY:,}.", cls="xs muted"))
    return svg(uid, W, H, "A clip is a bundle of independent channels",
               "On the left, a pose array: every joint sampled at every frame. On the "
               "right, the same motion as per-channel tracks after reduction, where "
               "most channels are empty and the rest keep their own key times.", b)


# ===========================================================================
# Figure 2 — the bracket, and the cursor  (§4)
# ===========================================================================
def fig_bracket():
    uid = "f77b"
    W, H = 760, 396
    b = [cmarker(uid, "teal", J_TEAL), cmarker(uid, "amber", J_AMBER)]

    # ---- the timeline ----------------------------------------------------
    TX, TW, TY = 60.0, 640.0, 118.0
    keys = [0.0, 0.0667, 0.1333, 0.2000, 0.2667, 0.3333]
    span = keys[-1]
    probe = 0.1800

    def px(t):
        return TX + TW * t / span

    b.append(label(W / 2, 32, "one channel, one lookup", cls="sm"))
    b.append(label(W / 2, 49,
                   "six keys of a 30 Hz track, and a sample that falls between two "
                   "of them", cls="xs muted"))
    b.append(rule(TX, TY, TX + TW, TY, cls="ink-soft", width=1.2))
    for i, t in enumerate(keys):
        x = px(t)
        b.append(rule(x, TY - 9, x, TY + 9, cls="ink-soft", width=1.2))
        b.append(label(x, TY - 16, f"k{i}", cls="xs muted"))
        b.append(label(x, TY + 26, f"{t:.4f}", cls="xs mono"))

    # the bracketing interval, highlighted
    x0, x1 = px(keys[2]), px(keys[3])
    b.append(box(x0, TY - 7, x1 - x0, 14, J_TEAL, opacity=0.18, width=1.2, rx=2))
    b.append(carrow(px(probe), TY - 46, px(probe), TY - 12, uid, "amber", J_AMBER))
    b.append(label(px(probe), TY - 54, f"t = {probe:.4f}", cls="xs mono"))

    b.append(label((x0 + x1) / 2, TY + 52, "cursor = 2", cls="xs mono"))
    b.append(label((x0 + x1) / 2, TY + 68,
                   "u = (0.1800 − 0.1333) / (0.2000 − 0.1333) = 0.7000",
                   cls="xs mono"))

    # ---- the two searches ------------------------------------------------
    SY = TY + 112.0
    b.append(label(W / 2, SY, "two ways to find that 2", cls="sm"))

    CW = 300.0
    for col, (title, sub, steps) in enumerate((
            ("binary search", "2 comparisons here, log2(n) in general", [2, 3]),
            ("a cursor", "2 comparisons here, and 2 for any n", [2, 3]))):
        cx = 50.0 + col * (CW + 60.0)
        b.append(label(cx + CW / 2, SY + 28, title, cls="xs"))
        b.append(label(cx + CW / 2, SY + 44, sub, cls="xs muted"))
        for i, t in enumerate(keys):
            kx = cx + 18.0 + (CW - 36.0) * i / (len(keys) - 1)
            hit = i in steps
            b.append(box(kx - 9, SY + 58, 18, 18,
                         J_TEAL if hit else None, opacity=0.5 if hit else None,
                         width=1.0, rx=3))
            b.append(label(kx, SY + 71, str(i), cls="xs mono"))
        b.append(label(cx + CW / 2, SY + 94,
                       "touches k" + ", k".join(str(s) for s in steps),
                       cls="xs mono"))

    # ---- the measurement -------------------------------------------------
    rows = []
    for keys_n, two, mixed, binary in STRATEGY:
        rows.append([f"{keys_n} keys", f"{two:.3f}", f"{mixed:.3f}", f"{binary:.3f}",
                     f"{binary / mixed:.2f}×"])
    tb, th = table(150.0, SY + 116.0,
                   ["one lookup per key interval", "two walks", "walk+search",
                    "binary", "cursor wins"],
                   rows, [186, 88, 96, 74, 86])
    b += tb
    b.append(label(W / 2, SY + 116.0 + th + 16,
                   "at six keys the two do the same work \u2014 which is the point: the "
                   "cursor is flat in n and the search is not.", cls="xs muted"))
    b.append(label(W / 2, SY + 116.0 + th + 32,
                   "nanoseconds per lookup, best of three.", cls="xs muted"))

    H = int(SY + 116.0 + th + 52)
    return svg(uid, W, H, "Finding the two keys that bracket a time",
               "A timeline of six keyframes with a sample falling inside the third "
               "interval, the normalised parameter computed from real numbers, and "
               "the same lookup done by binary search and by a remembered cursor.", b)


# ===========================================================================
# Figure 3 — the scale decision  (§5)
# ===========================================================================
def fig_scale():
    uid = "f77c"
    W, H = 760, 400
    b = [cmarker(uid, "rose", J_ROSE), cmarker(uid, "teal", J_TEAL)]

    b.append(label(W / 2, 30, "scale: equal differences, or equal ratios", cls="sm"))
    b.append(label(W / 2, 47,
                   "the same two endpoints, 1 and 8, reached two ways", cls="xs muted"))

    # ---- left: the two curves -------------------------------------------
    PX, PY, PW, PH = 54.0, 74.0, 300.0, 210.0
    b.append(frame(PX, PY, PW, PH))

    def cx(t):
        return PX + 14.0 + (PW - 28.0) * t

    def cy(v):
        return PY + PH - 16.0 - (PH - 32.0) * (v - 1.0) / 7.0

    b += [poly([(cx(i / 64.0), cy(1.0 + 7.0 * i / 64.0)) for i in range(65)],
               J_ROSE, close=False),
          poly([(cx(i / 64.0), cy(8.0 ** (i / 64.0))) for i in range(65)],
               J_TEAL, close=False)]
    for v, colour, name in ((4.5, J_ROSE, "4.500"), (2.82843, J_TEAL, "2.828")):
        b.append(f'<circle cx="{f(cx(0.5))}" cy="{f(cy(v))}" r="3.4" fill="{colour}"/>')
        b.append(label(cx(0.5) + 10, cy(v) + 4, name, cls="xs mono", anchor="start"))
    b.append(rule(cx(0.5), cy(2.82843), cx(0.5), cy(4.5), cls="grid", dash="2 3"))
    b.append(label(PX + PW / 2, PY - 10, "the value, against t", cls="xs muted"))
    b.append(label(PX + 14, PY + PH + 16, "1", cls="xs mono", anchor="start"))
    b.append(label(PX + PW - 14, PY + PH + 16, "8", cls="xs mono", anchor="end"))
    b.append(label(cx(0.62), cy(6.4), "lerp", cls="xs", anchor="start"))
    b.append(label(cx(0.72), cy(3.0), "a·(b/a)ᵗ", cls="xs", anchor="start"))

    # ---- right: the gap against the ratio -------------------------------
    QX = PX + PW + 52.0
    QW = 300.0
    b.append(frame(QX, PY, QW, PH))
    peak = 60.0

    def gx(ratio):
        return QX + 18.0 + (QW - 36.0) * math.log(ratio, 8.0)

    def gy(pct):
        return PY + PH - 18.0 - (PH - 36.0) * pct / peak

    # the band content actually uses
    b.append(box(gx(1.0001), PY + 4, gx(1.25) - gx(1.0001), PH - 8, J_TEAL,
                 opacity=0.12, width=0.0, rx=2))
    b.append(label((gx(1.0001) + gx(1.25)) / 2, PY + 22, "squash", cls="xs muted"))
    b.append(label((gx(1.0001) + gx(1.25)) / 2, PY + 36, "and", cls="xs muted"))
    b.append(label((gx(1.0001) + gx(1.25)) / 2, PY + 50, "stretch", cls="xs muted"))
    b.append(label((gx(1.0001) + gx(1.25)) / 2, PY + 66, "\u2264 0.62%", cls="xs mono"))

    pts = [(gx(r), gy(100.0 * (0.5 * (1.0 + r) - math.sqrt(r)) / math.sqrt(r)))
           for r in [1.0 + 7.0 * (i / 96.0) ** 2 for i in range(1, 97)]]
    b.append(poly(pts, J_ROSE, close=False))
    for _a, ratio, _lm, _gm, gap in SCALE:
        b.append(f'<circle cx="{f(gx(ratio))}" cy="{f(gy(gap))}" r="3.0" '
                 f'fill="{J_ROSE}"/>')
        # 1.05 SITS INSIDE THE SHADED BAND and its two labels collided outright
        # with 1.25's in the first draft. The band carries the number instead.
        if ratio >= 1.25:
            b.append(label(gx(ratio) - (14 if ratio == 1.25 else 0), gy(gap) - 10,
                           f"{gap:.2f}%", cls="xs mono"))
            b.append(label(gx(ratio), PY + PH + 16, f"{ratio:g}×", cls="xs mono"))
    b.append(label(QX + QW / 2, PY - 10,
                   "how far apart the two rules are, at the midpoint", cls="xs muted"))

    # ---- the blockers ----------------------------------------------------
    y = PY + PH + 44
    b.append(label(W / 2, y, "and two things that end the argument", cls="sm"))
    lines = [
        "a scale of ZERO is how an animator hides a thing, and ln(0) is −∞: "
        "the geometric rule",
        "does not shrink toward zero, it teleports there on the first frame after "
        "t = 0. A mirrored",
        "rig carries a NEGATIVE scale, and ln(−1) is NaN. glTF’s LINEAR mode "
        "is (1−t)·vₖ + t·vₖ₊₁.",
    ]
    for i, text in enumerate(lines):
        b.append(label(W / 2, y + 20 + i * 16, text, cls="xs muted"))

    H = int(y + 20 + len(lines) * 16 + 14)
    return svg(uid, W, H, "Interpolating scale linearly or geometrically",
               "Two curves from 1 to 8 with their midpoints marked at 4.500 and 2.828, "
               "and the percentage gap between the two rules plotted against the ratio, "
               "with the range content actually uses shaded.", b)


# ===========================================================================
# Figure 4 — the arcs a clip contains  (§5)
# ===========================================================================
def fig_arcs():
    uid = "f77d"
    W, H = 760, 372
    b = []

    b.append(label(W / 2, 30, "nlerp is fine inside a clip and not fine between two",
                   cls="sm"))
    b.append(label(W / 2, 47,
                   "rotation degrees throughout — the sphere arc a quaternion "
                   "walks is half of each number", cls="xs muted"))

    PX, PY, PW, PH = 76.0, 80.0, 608.0, 176.0
    b.append(frame(PX, PY, PW, PH))

    def ax(arc):
        return PX + 28.0 + (PW - 56.0) * math.log(arc / 2.0, 100.0)

    def ay(err):
        return PY + PH - 26.0 - (PH - 52.0) * math.log10(max(err, 1e-5) / 1e-5) / 5.0

    b.append(rule(ax(73.5), PY + 6, ax(73.5), PY + PH - 6, cls="grid", dash="3 4"))
    b.append(label(ax(73.5) - 28, PY + 18, "73.50°", cls="xs mono"))
    b.append(label(ax(73.5) + 6, PY - 10, "half a degree of lag", cls="xs muted",
                   anchor="start"))

    curve = []
    for i in range(121):
        arc = 2.0 * (100.0 ** (i / 120.0))
        best = 0.0
        for j in range(1, 128):
            t = j / 128.0
            half = math.radians(arc) / 2.0
            th = math.atan2(t * math.sin(half), (1 - t) + t * math.cos(half))
            best = max(best, abs(2.0 * math.degrees(th) - t * arc))
        curve.append((ax(arc), ay(best)))
    b.append(poly(curve, GREY, width=1.1, close=False))

    for rate, arc, err in KEY_ARCS:
        b.append(f'<circle cx="{f(ax(arc))}" cy="{f(ay(err))}" r="3.4" '
                 f'fill="{J_TEAL}"/>')
        # BELOW the dot: the 720 deg/s point and the 30 deg blend point are a
        # tenth of a decade apart and their labels touched.
        b.append(label(ax(arc), ay(err) + 17, f"{rate}°/s", cls="xs mono"))
    for n, (arc, err) in enumerate(BLEND_ARCS):
        b.append(f'<circle cx="{f(ax(arc))}" cy="{f(ay(err))}" r="3.4" '
                 f'fill="{J_ROSE}"/>')
        # ALTERNATING SIDES. 73.50 and 90 are a fifth of a decade apart on this
        # axis and their labels overlapped outright in the first draft.
        b.append(label(ax(arc), ay(err) + (-10 if n % 2 == 0 else 17),
                       f"{err:.3f}°", cls="xs mono"))

    for arc in (2, 6, 24, 90, 200):
        b.append(label(ax(arc), PY + PH + 16, f"{arc}°", cls="xs mono"))
    b.append(label(PX + PW / 2, PY + PH + 34,
                   "rotation between two keys, or between two poses", cls="xs muted"))
    b.append(label(PX - 8, PY + 14, "1e+0", cls="xs mono", anchor="end"))
    b.append(label(PX - 8, PY + PH - 24, "1e−5", cls="xs mono", anchor="end"))
    b.append(label(PX - 8, PY - 6, "nlerp lag", cls="xs muted", anchor="end"))

    y = PY + PH + 54
    b.append(box(PX, y - 10, 12, 9, J_TEAL, width=0.8, rx=2))
    b.append(label(PX + 18, y, "adjacent keys of a 30 Hz clip, at four limb speeds",
                   cls="xs muted", anchor="start"))
    b.append(box(PX, y + 8, 12, 9, J_ROSE, width=0.8, rx=2))
    b.append(label(PX + 18, y + 18, "arcs a cross-fade between two poses can reach",
                   cls="xs muted", anchor="start"))
    b.append(label(W / 2, y + 44,
                   "a limb turning at 720°/s moves 24° between keys and nlerp "
                   "lags it by 0.0169° — three orders inside the threshold.",
                   cls="xs muted"))
    return svg(uid, W, H, "Where nlerp is free and where it is not",
               "The nlerp-slerp lag plotted against the rotation between two "
               "orientations on log axes, with a clip's adjacent keyframes clustered "
               "far below the half-degree threshold and a cross-fade's arcs above it.",
               b)


# ===========================================================================
# Figure 5 — looping  (§6)
# ===========================================================================
def fig_wrap():
    uid = "f77e"
    W, H = 760, 330
    b = [cmarker(uid, "rose", J_ROSE)]

    b.append(label(W / 2, 28, "the frame that is missing, and the time that is negative",
                   cls="sm"))

    # ---- left: the seam --------------------------------------------------
    LX, LW = 44.0, 300.0
    TOP = 84.0
    b.append(label(LX + LW / 2, TOP - 22, "where the last key lands", cls="xs"))
    for row, (n, last, gap, name) in enumerate((
            (SEAM_CLOSED_KEYS, 1.000, SEAM_CLOSED_DEG, "31 keys, last at 1.000"),
            (SEAM_OPEN_KEYS, 0.967, SEAM_OPEN_DEG, "30 keys, last at 0.967"))):
        y = TOP + row * 76.0
        b.append(rule(LX, y, LX + LW, y, cls="ink-soft", width=1.1))
        for k in range(n):
            x = LX + LW * (k / 30.0)
            b.append(rule(x, y - 6, x, y + 6, cls="grid", width=1.0))
        b.append(rule(LX + LW, y - 12, LX + LW, y + 12, cls="hi", width=1.4))
        b.append(label(LX + LW, y - 20, "duration", cls="xs muted"))
        if last < 1.0:
            xl = LX + LW * last
            b.append(box(xl, y - 10, LX + LW - xl, 20, J_ROSE, opacity=0.35,
                         width=1.2, rx=2))
            b.append(label(xl + (LW * (1.0 - last)) / 2, y + 40, "one frame",
                           cls="xs muted"))
        b.append(label(LX, y + 26, name, cls="xs", anchor="start"))
        b.append(label(LX, y + 42, f"loop seam {gap:.4f}°", cls="xs mono",
                       anchor="start"))

    b.append(label(LX, TOP + 168,
                   f"root travel over the cycle {ROOT_TRAVEL:.4f} units —",
                   cls="xs muted", anchor="start"))
    b.append(label(LX, TOP + 184,
                   "which is the walk working, not the loop failing.",
                   cls="xs muted", anchor="start"))

    # ---- right: negative time -------------------------------------------
    RX = LX + LW + 44.0
    rows = [[f"{t:+.3f}", f"{u:.2f}", f"{ex:.3f}°", f"{fr:.3f}°"]
            for t, u, ex, fr in NEGATIVE]
    tb, th = table(RX, TOP - 22, ["t", "u", "extrapolates", "freezes"],
                   rows, [70, 58, 96, 82])
    b += tb
    b.append(label(RX, TOP - 40, "a time below zero, two ways to be wrong",
                   cls="xs", anchor="start"))
    b.append(label(RX, TOP - 22 + th + 20,
                   "std::fmod(−0.1, 1.0) is −0.1, not 0.9.",
                   cls="xs muted", anchor="start"))
    b.append(label(RX, TOP - 22 + th + 36,
                   "Clamped, the pose sticks on the first key — loud,",
                   cls="xs muted", anchor="start"))
    b.append(label(RX, TOP - 22 + th + 52,
                   "and bounded by the clip’s own range. Unclamped, it",
                   cls="xs muted", anchor="start"))
    b.append(label(RX, TOP - 22 + th + 68,
                   "extrapolates: 0.8° out at a tenth of a second,",
                   cls="xs muted", anchor="start"))
    b.append(label(RX, TOP - 22 + th + 84,
                   "100° out at six tenths, and still climbing.",
                   cls="xs muted", anchor="start"))

    b.append(label(W / 2, H - 22,
                   f"a duration SHORTER than the keys cuts the cycle: "
                   f"{SEAM_SHORT_DEG:.2f}° of seam and {SEAM_SHORT_OOR:,} keys "
                   "past the end.", cls="xs muted"))
    return svg(uid, W, H, "Closing a loop, and wrapping a negative time",
               "Two key timelines, one whose last key lands on the duration and one "
               "a frame short, with the resulting loop seam; and a table of what a "
               "negative playback time does with and without a clamp.", b)


# ===========================================================================
# Figure 6 — the two blends, rendered  (§7)
# ===========================================================================
def fig_blend_render():
    uid = "f77f"
    # A TIGHT CROP AND A BIGGER SCALE. The first draft used the whole right half
    # of the frame at cell=4, which put two 149-unit panels on a 760-unit page:
    # the tube was forty pixels wide and the 7% length difference the figure
    # exists to show was a judgement call. This crop is the tube and nothing else.
    CROP = (330, 30, 570, 510)
    PX, CELL = 2.55, 3
    W = 760.0
    CAP = 78.0
    GAP = 40.0

    probe, PW, PH = render_panel("l77_fade5.ppm", CROP, 0, 0, PX, cell=CELL)
    _ = probe
    X0 = (W - (2 * PW + GAP)) / 2.0

    left, _, _ = render_panel("l77_fade5.ppm", CROP, X0, CAP, PX, cell=CELL)
    right, _, _ = render_panel("l77_fade5m.ppm", CROP, X0 + PW + GAP, CAP, PX,
                               cell=CELL)
    H = CAP + PH + 112.0
    b = left + right

    b.append(label(W / 2, 30, "the same cross-fade, one step apart in the pipeline",
                   cls="sm"))
    b.append(label(X0 + PW / 2, 50, "blend the POSES", cls="xs mono"))
    b.append(label(X0 + PW / 2, 66, "then compose", cls="xs muted"))
    b.append(label(X0 + PW + GAP + PW / 2, 50, "blend the MATRICES", cls="xs mono"))
    b.append(label(X0 + PW + GAP + PW / 2, 66, "after composing", cls="xs muted"))
    b.append(label(X0 + PW / 2, CAP + PH + 24,
                   f"chain {CHAIN_POSE:.5f} of 5.000", cls="sm"))
    b.append(label(X0 + PW + GAP + PW / 2, CAP + PH + 24,
                   f"chain {CHAIN_MATRIX:.5f} of 5.000", cls="sm"))

    y = CAP + PH + 50
    b.append(label(W / 2, y,
                   "identical inputs, identical weight — and the right-hand limb "
                   "is 0.3498 units shorter", cls="xs muted"))
    b.append(label(W / 2, y + 18,
                   "and visibly thinner toward the tip, because the blended matrices "
                   "are not rotations", cls="xs muted"))
    b.append(label(W / 2, y + 36,
                   "and the error accumulates down the chain. Lesson 7.6’s candy "
                   "wrapper, one level up.", cls="xs muted"))
    return svg(uid, W, int(H), "Blending poses against blending matrices",
               "Two real renders of the same rig cross-faded halfway between two "
               "clips. The pose blend keeps the limb at full length; the matrix blend "
               "shortens it and tapers it toward the tip.", b)


# ===========================================================================
# Figure 7 — the shortening, measured  (§7)
# ===========================================================================
def fig_blend_curve():
    uid = "f77g"
    W, H = 760, 372
    b = [cmarker(uid, "teal", J_TEAL)]

    b.append(label(W / 2, 30, "what a matrix blend costs, against what it should cost",
                   cls="sm"))
    b.append(label(W / 2, 47,
                   "a six-joint chain, five bones of one unit, blended half and half",
                   cls="xs muted"))

    PX, PY, PW, PH = 78.0, 78.0, 372.0, 206.0
    b.append(frame(PX, PY, PW, PH))

    def bx(gap):
        return PX + 26.0 + (PW - 52.0) * gap / 45.0

    def by(v):
        return PY + PH - 26.0 - (PH - 52.0) * (v - 2.8) / 2.4

    # the prediction, as a continuous curve
    pred = []
    for i in range(91):
        gap = 45.0 * i / 90.0
        total = sum(math.cos(0.5 * k * math.radians(gap)) for k in range(5))
        pred.append((bx(gap), by(total)))
    # WIDE AND DASHED, DRAWN FIRST. The measured curve lands on top of it to
    # 4.768e-07, so a 1.1-wide grey line underneath a 1.4-wide rose one is
    # invisible — and a prediction the reader cannot see is not a prediction.
    b.append(poly(pred, GREY, width=3.4, dash="2 5", close=False))

    b.append(poly([(bx(g), by(p)) for g, p, _m, _q in FADE], J_TEAL, close=False))
    for g, p, m, _q in FADE:
        b.append(f'<circle cx="{f(bx(g))}" cy="{f(by(p))}" r="3.2" fill="{J_TEAL}"/>')
        b.append(f'<circle cx="{f(bx(g))}" cy="{f(by(m))}" r="3.2" fill="{J_ROSE}"/>')
    b.append(poly([(bx(g), by(m)) for g, _p, m, _q in FADE], J_ROSE, close=False))

    for v in (3.0, 3.5, 4.0, 4.5, 5.0):
        b.append(label(PX - 8, by(v) + 4, f"{v:.1f}", cls="xs mono", anchor="end"))
    for g in (0, 10, 20, 30, 45):
        b.append(label(bx(g), PY + PH + 16, f"{g}°", cls="xs mono"))
    b.append(label(PX + PW / 2, PY + PH + 34, "per-joint difference between the poses",
                   cls="xs muted"))
    b.append(label(PX - 8, PY - 6, "chain length", cls="xs muted", anchor="end"))
    b.append(label(bx(30), by(5.0) - 12, "blend the poses", cls="xs", anchor="middle"))
    # BELOW-LEFT OF THE CURVE, NOT ON IT. check-page.js's `onShape` test reports
    # a label whose box overlaps a stroked path, and the first placement sat
    # squarely on the measured curve at 34 degrees.
    b.append(label(bx(13), by(3.30), "blend the matrices", cls="xs", anchor="middle"))

    # ---- right: the derivation in one line -------------------------------
    QX = PX + PW + 44.0
    b.append(label(QX, PY + 6, "why that curve", cls="xs", anchor="start"))
    lines = [
        "Bone k runs from joint k−1 to joint k, so its",
        "direction is joint k−1’s own axis. The two poses",
        "turn that joint by (k−1)·δ, and a matrix blend",
        "averages the two joint POSITIONS — so the bone",
        "becomes the average of two unit vectors (k−1)·δ",
        "apart, which is cos of half of it.",
        "",
        "    ∑ cos((k−1)·δ / 2),  k = 1 … 5",
        "",
        f"measured against that: {PREDICTION_GAP:.3e}",
        f"pose route, worst bone error: {POSE_BONE_ERR:.3e}",
    ]
    for i, text in enumerate(lines):
        cls = "xs mono" if text.strip().startswith(("∑", "measured", "pose")) \
            else "xs muted"
        b.append(label(QX, PY + 44 + i * 16, text, cls=cls, anchor="start"))

    b.append(label(W / 2, H - 22,
                   "the error grows down the chain, so it is worst at hands and feet "
                   "— which is where props are attached.", cls="xs muted"))
    return svg(uid, W, H, "Chain length under the two blends",
               "Chain length plotted against the per-joint difference between two "
               "poses. The pose blend holds at exactly five units; the matrix blend "
               "falls along the sum of cosines of half the accumulated angle.", b)


# ===========================================================================
# Figure 8 — the double cover, inside a track  (§8)
# ===========================================================================
def fig_signs():
    uid = "f77h"
    W, H = 760, 412
    b = [cmarker(uid, "rose", J_ROSE)]

    b.append(label(W / 2, 30, "two keys, one degree apart, written with opposite signs",
                   cls="sm"))

    # ---- left: the sphere picture ---------------------------------------
    CX, CY, R = 186.0, 164.0, 88.0
    b.append(f'<circle cx="{f(CX)}" cy="{f(CY)}" r="{f(R)}" fill="none" class="grid" '
             f'stroke-width="1.1"/>')
    a_ang = -68.0 * D
    b_ang = -62.0 * D
    for ang, colour, name, anchor in ((a_ang, J_TEAL, "qₖ", "end"),
                                      (b_ang, J_TEAL, "qₖ₊₁", "start")):
        x, y = CX + R * math.cos(ang), CY + R * math.sin(ang)
        b.append(rule(CX, CY, x, y, cls="ink-soft", width=1.2))
        b.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="3.6" fill="{colour}"/>')
        b.append(label(x + (10 if anchor == "start" else -10), y - 6, name,
                       cls="xs mono", anchor=anchor))
    x, y = CX - R * math.cos(b_ang), CY - R * math.sin(b_ang)
    b.append(rule(CX, CY, x, y, cls="ink-soft", width=1.2, dash="3 4"))
    b.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="3.6" fill="{J_ROSE}"/>')
    b.append(label(x, y + 18, "−qₖ₊₁", cls="xs mono"))
    b.append(label(CX, CY + R + 28,
                   "\u2212q and q are the SAME ORIENTATION and not the", cls="xs muted"))
    b.append(label(CX, CY + R + 44,
                   "same point. A lerp between the two teal dots", cls="xs muted"))
    b.append(label(CX, CY + R + 60,
                   "travels 1\u00b0; between a teal and the rose, 359\u00b0.", cls="xs muted"))

    # ---- right: the measured spike --------------------------------------
    PX, PY, PW, PH = 396.0, 82.0, 320.0, 180.0
    b.append(frame(PX, PY, PW, PH))
    b.append(label(PX + PW / 2, PY - 22, "one flipped interval, played by a lerp with "
                                         "no nearest", cls="xs"))

    def nx(t):
        return PX + 22.0 + (PW - 44.0) * (t - NAIVE[0][0]) / (NAIVE[-1][0] - NAIVE[0][0])

    def ny(v):
        return PY + PH - 24.0 - (PH - 48.0) * v / 190.0

    b.append(poly([(nx(t), ny(v)) for t, v in NAIVE], J_ROSE, close=False))
    for t, v in NAIVE:
        b.append(f'<circle cx="{f(nx(t))}" cy="{f(ny(v))}" r="3.0" fill="{J_ROSE}"/>')
    b.append(rule(PX + 14, ny(0.0), PX + PW - 14, ny(0.0), cls="grid", dash="3 4"))
    # BOTTOM RIGHT, where the curve has already climbed away. At the left the
    # dashed zero line and the curve's first point are the same place.
    b.append(label(PX + PW - 16, ny(0.0) + 18, "nearest, and this engine",
                   cls="xs muted", anchor="end"))
    for v in (0, 90, 180):
        b.append(label(PX - 8, ny(v) + 4, f"{v}°", cls="xs mono", anchor="end"))
    b.append(label(PX + PW / 2, PY + PH + 18,
                   "one keyframe interval, 0.033 s to 0.067 s", cls="xs muted"))

    y = 336.0
    b.append(label(W / 2, y,
                   f"validate reports {FLIPS_REPORTED} sign flips; canonicalise_rotations "
                   f"negates {FLIPS_NEGATED} keys and they go to zero.", cls="xs muted"))
    b.append(label(W / 2, y + 18,
                   "This engine samples both versions IDENTICALLY (0.000e+00°), "
                   "because nearest is inside quat_nlerp.", cls="xs muted"))
    b.append(label(W / 2, y + 36,
                   "What the pass buys is the count — and every consumer that is "
                   "not this engine.", cls="xs muted"))
    return svg(uid, W, H, "A rotation track with inconsistent signs",
               "Two quaternions one degree apart on a circle, with the second's "
               "antipode marked; and the measured error of interpolating across that "
               "flipped interval without the nearest comparison, peaking at 180 "
               "degrees.", b)


# ===========================================================================
# Figure 9 — reduction  (§9)
# ===========================================================================
def fig_reduce():
    uid = "f77i"
    W, H = 760, 396
    b = []

    b.append(label(W / 2, 30, "throwing away the keys the sampler can rebuild", cls="sm"))
    b.append(label(W / 2, 47, "thigh.L's rotation channel: 31 keys, then 19, then 8",
                   cls="xs muted"))

    PX, PY, PW, PH = 152.0, 84.0, 546.0, 128.0
    b.append(frame(PX, PY, PW, PH))

    def kx(t):
        return PX + 22.0 + (PW - 44.0) * t

    def ky(v):
        return PY + PH / 2 - (PH / 2 - 18.0) * v

    # the curve the keys were baked from: 28 sin(2 pi t), normalised
    b.append(poly([(kx(i / 240.0), ky(math.sin(2 * math.pi * i / 240.0)))
                   for i in range(241)], GREY, width=1.1, close=False))

    for row, (times, colour, name) in enumerate((
            ([i / 30.0 for i in range(31)], J_VIOLET, "31 keys, as baked"),
            (KEPT_05, J_TEAL, "19 keys, 0.50°"),
            (KEPT_20, J_AMBER, "8 keys, 2.00°"))):
        ty = PY + PH + 26.0 + row * 22.0
        b.append(label(PX + 14, ty + 4, name, cls="xs", anchor="end"))
        for t in times:
            b.append(f'<circle cx="{f(kx(t))}" cy="{f(ty)}" r="2.6" fill="{colour}"/>')
            if row == 1:
                b.append(f'<circle cx="{f(kx(t))}" cy="{f(ky(math.sin(2*math.pi*t)))}" '
                         f'r="2.8" fill="{J_TEAL}"/>')

    b.append(label(PX + PW / 2, PY + PH + 100,
                   "the survivors bunch where the curve turns, which is where a "
                   "straight line between neighbours misses it", cls="xs muted"))

    rows = [[f"{tol:.2f}°" if tol > 0 else "none", f"{keys:,}", f"{byt:,}",
             f"{err:.4f}°", f"{ratio:.2f}×"] for tol, keys, byt, err, ratio
            in REDUCE]
    tb, th = table(150.0, PY + PH + 118.0,
                   ["tolerance", "keys", "key bytes", "error", "headers/keys"],
                   rows, [110, 86, 108, 96, 110])
    b += tb
    H = int(PY + PH + 118.0 + th + 44)
    b.append(label(W / 2, H - 30,
                   "the error column is against the FUNCTION the keys were baked from, "
                   "so the first row is what 30 Hz costs before", cls="xs muted"))
    b.append(label(W / 2, H - 14,
                   "anything is thrown away. The last column is the warning: the "
                   "std::vector headers do not shrink with the keys.", cls="xs muted"))
    return svg(uid, W, H, "Keyframe reduction on one channel",
               "A sine curve with its 31 baked keys, the 19 that survive a half-degree "
               "tolerance and the 8 that survive two degrees, and a table of keys, "
               "bytes and error at five tolerances.", b)


# ===========================================================================
# Figure 10 — where the bytes and the nanoseconds go  (§9)
# ===========================================================================
def fig_cost():
    uid = "f77j"
    W, H = 760, 376
    b = []

    b.append(label(W / 2, 30, "what a second of motion weighs, and what reading it costs",
                   cls="sm"))

    # ---- left: bytes -----------------------------------------------------
    PX, PY, PW, PH = 48.0, 76.0, 330.0, 178.0
    b.append(frame(PX, PY, PW, PH))
    b.append(label(PX + PW / 2, PY - 22, "23 joints, 31 frames, one second", cls="xs"))
    rows = [("pose array, 40 B/joint", POSE_ARRAY, J_VIOLET),
            ("per-channel, every key", RAW_BYTES, J_ROSE),
            ("per-channel, reduced", LEAN_BYTES, J_TEAL),
            ("  + std::vector headers", LEAN_BYTES + CONTAINER, J_SAGE)]
    slot = PH / len(rows)
    for i, (name, value, colour) in enumerate(rows):
        y = PY + i * slot + 22.0
        width = (PW - 130.0) * value / RAW_BYTES
        b.append(box(PX + 10, y, max(width, 1.5), slot * 0.34, colour, opacity=0.85,
                     width=1.0, rx=2))
        b.append(label(PX + 10, y - 7, name, cls="xs muted", anchor="start"))
        b.append(label(PX + 14 + max(width, 1.5), y + slot * 0.34 - 3, f"{value:,} B",
                       cls="xs mono", anchor="start"))

    b.append(label(PX + PW / 2, PY + PH + 22,
                   "the per-channel layout is 30% BIGGER than a pose", cls="xs muted"))
    b.append(label(PX + PW / 2, PY + PH + 38,
                   "array until something is elided — every key", cls="xs muted"))
    b.append(label(PX + PW / 2, PY + PH + 54,
                   "carries its own time. Then it is 7.7× smaller.", cls="xs muted"))

    # ---- right: nanoseconds ---------------------------------------------
    QX = PX + PW + 48.0
    QW = 330.0
    b.append(frame(QX, PY, QW, PH))
    b.append(label(QX + QW / 2, PY - 22, "microseconds per frame, 23 joints", cls="xs"))
    trows = [("sample, cursor", SAMPLE_CURSOR_US, J_TEAL),
             ("sample, binary", SAMPLE_BINARY_US, J_ROSE),
             ("compose + palette", PALETTE_US, J_VIOLET),
             ("sample, cursor, 301 keys", LONG_CURSOR_US, J_TEAL),
             ("sample, binary, 301 keys", LONG_BINARY_US, J_ROSE)]
    slot = PH / len(trows)
    for i, (name, value, colour) in enumerate(trows):
        # +22, NOT +14. The first row's label sat across the frame's TOP EDGE,
        # which check-page.js's (d) test reports as text crossing a hollow box —
        # it samples the perimeter, so "inside the frame" is fine and "straddling
        # it" is not. Five rows in 178 units still fit with three to spare.
        y = PY + i * slot + 22.0
        width = (QW - 130.0) * value / LONG_BINARY_US
        b.append(box(QX + 10, y, max(width, 1.5), slot * 0.30, colour, opacity=0.85,
                     width=1.0, rx=2))
        b.append(label(QX + 10, y - 9, name, cls="xs muted", anchor="start"))
        b.append(label(QX + 14 + max(width, 1.5), y + slot * 0.30 - 3, f"{value:.3f}",
                       cls="xs mono", anchor="start"))

    b.append(label(QX + QW / 2, PY + PH + 22,
                   "sampling is two thirds of what composing and", cls="xs muted"))
    b.append(label(QX + QW / 2, PY + PH + 38,
                   "building the palette cost — not free, and not", cls="xs muted"))
    b.append(label(QX + QW / 2, PY + PH + 54,
                   "where a frame goes. Both are noise beside skinning.",
                   cls="xs muted"))

    b.append(label(W / 2, H - 26,
                   f"at production scale — 100 joints, 30 seconds of this density "
                   f"— {SCALE_NAIVE_MB:.2f} MB of pose array against "
                   f"{SCALE_LEAN_MB:.2f} MB of clip.", cls="xs muted"))
    b.append(label(W / 2, H - 10,
                   f"And std::fmod is not constant time: {FMOD[0][1]:.3f} ns on a "
                   f"wrapped clock against {FMOD[0][2]:.3f} after it has run to 6,666 "
                   "seconds.", cls="xs muted"))
    return svg(uid, W, H, "The cost of a clip in bytes and in nanoseconds",
               "Bar charts of four storage layouts and five sampling timings, showing "
               "that a per-channel layout only pays once channels are elided and that "
               "a cursor beats a binary search by more on a longer track.", b)


FIGURES = [
    ("l77_fig1.svg", fig_tracks),
    ("l77_fig2.svg", fig_bracket),
    # PAGE ORDER, not writing order: §5.2's arcs figure comes before §5.3's
    # scale figure, and §9.1's cost table before §9.2's reduction. check-page.js
    # compares caption numbers against DOM position and caught both swaps.
    ("l77_fig3.svg", fig_arcs),
    ("l77_fig4.svg", fig_scale),
    ("l77_fig5.svg", fig_wrap),
    ("l77_fig6.svg", fig_blend_render),
    ("l77_fig7.svg", fig_blend_curve),
    ("l77_fig8.svg", fig_signs),
    ("l77_fig9.svg", fig_cost),
    ("l77_fig10.svg", fig_reduce),
]


def main():
    for name, fn in FIGURES:
        path = os.path.join(OUT, name)
        with open(path, "w") as fh:
            fh.write(fn())
        print(f"wrote {path}  ({os.path.getsize(path):,} bytes)")


if __name__ == "__main__":
    main()
